#include <algorithm>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <csignal>

#include <SDL3_image/SDL_image.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#include <lconfig.h>
#include "app.hpp"
#include "../input/action_mapper.hpp"
#include "../platform/sysctl.hpp"
#include "../render/basic_lazy_file_sink.hpp"
#include "../render/logger.hpp"
#include "../render/util.hpp"
#include "../screens/shell.hpp"
#include "../screens/quick_panel.hpp"
#include "../screens/settings.hpp"
#include "../ui/engine/focus.hpp"

namespace fs = std::filesystem;

namespace Foyer {
    std::shared_ptr<spdlog::logger> init_logging_global()
    {
        std::string dir;
        if (const char *s = getenv("XDG_STATE_HOME"); s && *s) dir = (fs::path(s) / "foyer").string();
        else if (const char *h = getenv("HOME"); h && *h && std::string(h) != "/") dir = (fs::path(h) / ".local/state/foyer").string();
        else dir = "/tmp/foyer";
        std::error_code ec;
        fs::create_directories(dir, ec);
        auto file_sink = std::make_shared<spdlog::sinks::basic_lazy_file_sink_st>((fs::path(dir) / "foyer.log").string(), true);
        file_sink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] %v");
        auto console = std::make_shared<spdlog::sinks::stdout_color_sink_st>();
        const char *lvl = getenv("FOYER_LOG_STDERR");   // e.g. info (boot tests)
        console->set_level(lvl && *lvl ? spdlog::level::from_str(lvl) : spdlog::level::warn);
        console->set_pattern("[%^%l%$] %v");
        std::vector<spdlog::sink_ptr> sinks{file_sink, console};
        auto logger = std::make_shared<spdlog::logger>("foyer", sinks.begin(), sinks.end());
        logger->set_level(spdlog::level::info);
        logger->flush_on(spdlog::level::info);
        spdlog::set_default_logger(logger);
        return logger;
    }

    // init-spawned sessions lack /usr/local/bin (app wrappers live there)
    static void ensure_path()
    {
        const char *cur = getenv("PATH");
        std::string path = cur ? cur : "/usr/sbin:/usr/bin:/sbin:/bin";
        std::string prefix;
        for (const char *dir : {"/usr/local/sbin", "/usr/local/bin"})
            if ((":" + path + ":").find(std::string(":") + dir + ":") == std::string::npos)
                prefix += std::string(dir) + ":";
        if (!prefix.empty()) setenv("PATH", (prefix + path).c_str(), 1);
    }

    static std::string app_log_dir()
    {
        if (const char *s = getenv("XDG_STATE_HOME"); s && *s) return (fs::path(s) / "foyer" / "apps").string();
        if (const char *h = getenv("HOME"); h && *h) return (fs::path(h) / ".local/state/foyer/apps").string();
        return "/tmp/foyer/apps";
    }

    static std::string last_log_line(const std::string &path)
    {
        FILE *f = fopen(path.c_str(), "rb");
        if (!f) return "";
        fseek(f, 0, SEEK_END);
        long size = ftell(f), start = size > 4096 ? size - 4096 : 0;
        fseek(f, start, SEEK_SET);
        std::string buf((size_t) (size - start), '\0');
        buf.resize(fread(buf.data(), 1, buf.size(), f));
        fclose(f);
        std::string last;
        for (auto &line : split_lines(buf)) {
            std::string t = line;
            while (!t.empty() && isspace((unsigned char) t.back())) t.pop_back();
            if (!t.empty()) last = t;
        }
        if (last.size() > 90) last = last.substr(0, 87) + "...";
        return last;
    }
}

using namespace Foyer;

uint32_t App::evt_global()
{
    static uint32_t t = SDL_RegisterEvents(1);
    return t;
}

App::App(const Options &o): opt_(o)
{
    ensure_path();
    set_executable_dir(SDL_GetBasePath());
    Paths::init(opt_.config_dir);
    init_video();
    (void) evt_global();               // register user events on the main thread
    (void) JobQueue::evt_done();
    jobs_ = std::make_unique<JobQueue>();
    load_data();
    apply_appearance();
    if (settings_.sound && !opt_.screenshot.size()) {
        // Opening the audio device can block for a long time (or forever)
        // when the PipeWire graph is unhealthy; never stall the UI for it.
        int vol = settings_.sound_volume;
        sound_init_ = std::thread([this, vol] {
            try { sound_ = std::make_unique<Sound>(vol); sound_ready_ = true; }
            catch (...) { Foyer::logger::warn("UI sounds unavailable"); }
        });
    }
    engine_->on_click = [this] { play_click(); };
    engine_->on_select = [this] { play_select(); };
    engine_->overlay_paint = [](ui::Engine &e, ui::Canvas &c) { Shell::paint_overlay(e, c); };
    engine_->overlay_active = [] { return Shell::overlay_active(); };

    if (settings_.gamepad) {
        SDL_InitSubSystem(SDL_INIT_GAMEPAD);
        if (!settings_.gamepad_mappings.empty())
            SDL_AddGamepadMappingsFromFile(settings_.gamepad_mappings.c_str());
    }
#ifdef FOYER_EVDEV
    if (!opt_.overlay && opt_.screenshot.empty()) {
        evdev_ = std::make_unique<EvdevGlobal>();
        evdev_->set_callback([](Action a) {
            SDL_Event e{};
            e.type = evt_global();
            e.user.data1 = (void*) (intptr_t) (int) a;
            SDL_PushEvent(&e);
        });
        evdev_->start();
    }
#endif
#ifdef FOYER_CEC
    cec_ = std::make_unique<Cec>();
#endif
    build_root();
    last_input_ = SDL_GetTicks();
    Foyer::logger::info("Foyer {} ready ({}x{}, theme '{}'){}", PROJECT_VERSION_STR,
                        engine_->metrics().width(), engine_->metrics().height(), settings_.theme,
                        opt_.overlay ? " [overlay]" : "");
}

App::~App()
{
#ifdef FOYER_EVDEV
    evdev_.reset();
#endif
    if (jobs_) jobs_->stop();
    engine_.reset();   // layers first (they may reference the window)
    if (sound_init_.joinable()) {
        if (sound_ready_) sound_init_.join();
        else sound_init_.detach();   // stuck in the audio backend: don't hang on exit
    }
    if (sound_ready_) sound_.reset();
    else (void) sound_.release();
    if (renderer_) SDL_DestroyRenderer(renderer_);
    if (window_) SDL_DestroyWindow(window_);
    TTF_Quit();
    SDL_Quit();
}

void App::init_video()
{
    SDL_SetHint(SDL_HINT_VIDEO_X11_NET_WM_BYPASS_COMPOSITOR, "0");
    if (!opt_.screenshot.empty() && !getenv("SDL_VIDEO_DRIVER"))
        SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "offscreen,dummy");
    if (!SDL_Init(SDL_INIT_VIDEO))
        Foyer::logger::critical("Could not initialize SDL: {}", SDL_GetError());
    if (!TTF_Init())
        Foyer::logger::critical("Could not initialize SDL_ttf: {}", SDL_GetError());

    int w = opt_.width, h = opt_.height;
    SDL_WindowFlags flags = SDL_WINDOW_HIGH_PIXEL_DENSITY;
    bool fullscreen = !opt_.windowed && opt_.screenshot.empty() && !(w && h);
    if (fullscreen) {
        int n = 0;
        SDL_DisplayID *ds = SDL_GetDisplays(&n);
        const SDL_DisplayMode *dm = (ds && n) ? SDL_GetDesktopDisplayMode(ds[0]) : nullptr;
        if (dm) { w = dm->w; h = dm->h; }
        SDL_free(ds);
        flags |= SDL_WINDOW_FULLSCREEN;
    } else {
        flags |= SDL_WINDOW_RESIZABLE;
        if (!opt_.screenshot.empty()) flags |= SDL_WINDOW_HIDDEN;
    }
    if (w <= 0 || h <= 0) { w = 1920; h = 1080; }
    if (opt_.overlay) flags |= SDL_WINDOW_TRANSPARENT;
    window_ = SDL_CreateWindow("Foyer", w, h, flags);
    if (!window_) Foyer::logger::critical("Could not create window: {}", SDL_GetError());
    renderer_ = SDL_CreateRenderer(window_, opt_.screenshot.empty() ? nullptr : "software");
    if (!renderer_) renderer_ = SDL_CreateRenderer(window_, nullptr);
    if (!renderer_) Foyer::logger::critical("Could not create renderer: {}", SDL_GetError());
    SDL_SetRenderVSync(renderer_, 1);
    SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_BLEND);
    if (fullscreen) SDL_HideCursor();
    int pw = w, ph = h;
    SDL_GetRenderOutputSize(renderer_, &pw, &ph);
    engine_ = std::make_unique<ui::Engine>(renderer_, pw, ph);
    Foyer::logger::info("Video: {} renderer, output {}x{}", SDL_GetRendererName(renderer_), pw, ph);
}

std::string App::legacy_import()
{
    std::string msg;
    std::string ll = opt_.legacy_layout, lc = opt_.legacy_config;
    if (ll.empty()) ll = (fs::path(Paths::config_dir()) / "layout.xml").string();
    if (lc.empty()) lc = (fs::path(Paths::config_dir()) / "config.ini").string();
    maybe_import(Paths::config_dir(), ll, lc, &msg);
    return msg;
}

void App::load_data()
{
    std::error_code ec;
    const std::string &cfg = Paths::config_dir();
    fs::create_directories(cfg, ec);
    std::string imported = legacy_import();
    LoadReport rep;
    // first run: seed from the shipped defaults
    for (const char *f : {"settings.toml", "layout.toml"}) {
        fs::path dst = fs::path(cfg) / f;
        if (!fs::exists(dst, ec)) {
            std::string src;
            for (auto &d : Paths::data_dirs())
                if (d != cfg && fs::exists(fs::path(d) / f, ec)) { src = (fs::path(d) / f).string(); break; }
            if (!src.empty()) fs::copy_file(src, dst, ec);
        }
    }
    settings_ = load_settings((fs::path(cfg) / "settings.toml").string(), &rep);
    layout_ = load_layout((fs::path(cfg) / "layout.toml").string(), &rep);
    if (layout_.tabs.empty()) {
        Tab home; home.id = "home"; home.title = "Home";
        Tab lib; lib.id = "apps"; lib.title = "Apps"; lib.kind = "library";
        Tab st; st.id = "settings"; st.title = "Settings"; st.kind = "settings";
        layout_.tabs = {home, lib, st};
    }
    reload_apps();
    if (!imported.empty()) toast(imported);
    for (auto &w : rep.warnings) Foyer::logger::warn("{}", w);
    if (!rep.warnings.empty()) toast("Config problem: " + rep.warnings.front(), true);
    // the profile can cap the texture budget (small RAM boxes)
    engine_->assets().set_budget(settings_.asset_budget_mb << 20);
}

void App::reload_apps()
{
    std::vector<std::string> dirs;
    for (auto &d : Paths::data_dirs()) dirs.push_back((fs::path(d) / "apps.d").string());
    LoadReport rep;
    apps_ = load_apps(dirs, &rep);
    for (auto &w : rep.warnings) Foyer::logger::warn("{}", w);
}

void App::apply_appearance()
{
    update_metrics();
    std::string bp = Foyer::breakpoint_name(engine_->metrics().breakpoint());
    LoadReport rep;
    ui::Tokens t = load_theme(settings_.theme, bp, settings_, &theme_dir_, &rep);
    for (auto &w : rep.warnings) Foyer::logger::warn("{}", w);
    engine_->set_tokens(t);
}

void App::update_metrics()
{
    int w = 0, h = 0;
    SDL_GetRenderOutputSize(renderer_, &w, &h);
    engine_->configure(w, h, settings_.ui_scale, settings_.overscan);
}

void App::build_root()
{
    if (opt_.overlay) {
        engine_->replace_root(std::make_unique<QuickPanel>(*this, true));
        return;
    }
    engine_->replace_root(std::make_unique<Shell>(*this));
    // A resolution change is on trial: ask to keep it (session.sh reverts
    // automatically if nobody answers).
    std::error_code ec;
    if (fs::exists(fs::path(Paths::config_dir()) / "session.conf.pending", ec)) {
        int w = engine_->metrics().width(), h = engine_->metrics().height();
        engine_->push(std::make_unique<ConfirmDialog>(*this, "Keep this resolution?",
            "The display is now " + std::to_string(w) + " \xC3\x97 " + std::to_string(h) + ".",
            "Keep", "Revert", [this](bool keep) {
                jobs_->run(ctl("display", keep ? "mode/confirm" : "mode/revert"), nullptr);
                if (keep) toast("Resolution saved");
            }, 15));
        return;
    }
    if (!opt_.open.empty()) execute(opt_.open);
    else if (!settings_.autostart.empty()) execute(settings_.autostart);
}

// ---- persistence -----------------------------------------------------------
void App::save_settings()
{
    if (!Foyer::save_settings((fs::path(Paths::config_dir()) / "settings.toml").string(), settings_))
        toast("Could not save settings", true);
}

void App::save_layout()
{
    if (!Foyer::save_layout((fs::path(Paths::config_dir()) / "layout.toml").string(), layout_))
        toast("Could not save layout", true);
}

void App::save_app(const AppEntry &a)
{
    if (!Foyer::save_app((fs::path(Paths::config_dir()) / "apps.d").string(), a))
        toast("Could not save app", true);
    reload_apps();
}

std::string App::resolve_art(const std::string &p) const
{
    if (p.empty() || p[0] == '/' || p[0] == '#') return p;
    std::string f = Paths::find_data(p);
    return f.empty() ? p : f;
}

std::string App::icon(const std::string &name) const
{
    return resolve_art("icons/ui/" + name + ".svg");
}

void App::play_click() { if (sound_ready_ && sound_) sound_->play_click(); }
void App::play_select() { if (sound_ready_ && sound_) sound_->play_select(); }

void App::toast(const std::string &msg, bool error)
{
    Foyer::logger::info("toast: {}", msg);
    Shell::post_toast(msg, error);
    ui::invalidate();
}

// ---- commands --------------------------------------------------------------
void App::execute(const std::string &command, const std::string &title)
{
    if (command.empty()) return;
    if (command[0] == ':') {
        std::string c = command.substr(1);
        std::string arg;
        if (auto sp = c.find(' '); sp != std::string::npos) { arg = c.substr(sp + 1); c = c.substr(0, sp); }
        if (c == "quit") quit_ = true;
        else if (c == "restart" || c == "reboot") jobs_->run(ctl("power", "reboot"), nullptr);
        else if (c == "shutdown" || c == "poweroff") jobs_->run(ctl("power", "shutdown"), nullptr);
        else if (c == "sleep" || c == "suspend") jobs_->run(ctl("power", "suspend"), nullptr);
        else if (c == "home") { close_app(); engine_->pop_to_root(); }
        else if (c == "settings") open_settings(arg);
        else if (c == "audio" || c == "display" || c == "network" || c == "bluetooth" || c == "system" ||
                 c == "power" || c == "appearance" || c == "layout" || c == "launcher")
            open_settings(c);
        else if (c == "wifi") open_settings("network");
        else if (c == "bt") open_settings("bluetooth");
        else if (c == "volup") jobs_->run(ctl("audio", "volume/up"), [this](const JobResult&) { refresh_status(); });
        else if (c == "voldown") jobs_->run(ctl("audio", "volume/down"), [this](const JobResult&) { refresh_status(); });
        else if (c == "mute") jobs_->run(ctl("audio", "mute/toggle"), [this](const JobResult&) { refresh_status(); });
        else if (c == "app" && apps_.count(arg)) launch(apps_[arg]);
        else Foyer::logger::warn("Unknown command '{}'", command);
        return;
    }
    if (app_proc_.alive()) {
        toast(app_title_ + " is already running");
        return;
    }
    app_title_ = title.empty() ? command : title;
    app_command_ = command;
    app_log_ = (fs::path(app_log_dir()) / (slug(app_title_) + ".log")).string();
    app_started_ = SDL_GetTicks();
    app_proc_ = Process::spawn(command, app_log_);
    if (app_proc_.alive()) {
        Foyer::logger::info("Launched '{}' (pid {}, log {})", app_title_, app_proc_.pid(), app_log_);
        Shell::set_launching(app_title_);
    } else {
        toast("Failed to launch " + app_title_, true);
    }
    ui::invalidate();
}

void App::launch(const AppEntry &a) { execute(a.launch_command(), a.title); }

void App::close_app()
{
    if (app_proc_.alive()) {
        Foyer::logger::info("Closing '{}' (pid {})", app_title_, app_proc_.pid());
        app_proc_.kill(SIGTERM);
    }
}

void App::open_settings(const std::string &page)
{
    if (auto *shell = dynamic_cast<Shell*>(engine_->depth() ? engine_->top() : nullptr)) {
        shell->show_tab("settings");
        if (!page.empty()) open_settings_page(*this, page);
        return;
    }
    if (!page.empty()) open_settings_page(*this, page);
}

void App::open_quick_panel()
{
    if (opt_.overlay) return;
    if (!app_proc_.alive()) {
        engine_->push(std::make_unique<QuickPanel>(*this, false));
        return;
    }
    if (overlay_proc_.alive()) return;
    // gamescope composites a window with GAMESCOPE_EXTERNAL_OVERLAY above the app.
    std::string exe = (fs::path(executable_dir()) / "foyer").string();
    std::string cmd = "GAMESCOPE_EXTERNAL_OVERLAY=1 FOYER_APP_PID=" + std::to_string(app_proc_.pid()) +
                      " FOYER_APP_TITLE='" + app_title_ + "' '" + exe + "' --overlay -c '" + Paths::config_dir() + "'";
    Foyer::logger::info("Opening quick panel overlay over '{}'", app_title_);
    overlay_proc_ = Process::spawn(cmd);
}

// ---- status ----------------------------------------------------------------
void App::refresh_status()
{
    last_status_ = SDL_GetTicks();
    jobs_->run(ctl("net", "status"), [this](const JobResult &res) {
        NetStatus st = parse_net_status(res.output);
        Status s = status_;
        s.net_kind = "offline"; s.net_text = "Offline";
        for (auto &i : st.ifaces)
            if (i.state == "up" && i.ip != "-") {
                s.net_kind = i.type == "wifi" ? "wifi" : "ethernet";
                s.net_text = i.ip.substr(0, i.ip.find('/'));
                break;
            }
        if (s.net_kind == "offline" && !st.eth.empty() && st.eth != "no link") { s.net_kind = "ethernet"; s.net_text = st.eth; }
        status_ = s;
        ui::invalidate();
    });
    jobs_->run(ctl("audio", "volume/get"), [this](const JobResult &res) {
        if (!res.ok) return;
        for (auto &line : split_lines(res.output)) {
            if (line.starts_with("volume=")) status_.volume = std::atoi(line.c_str() + 7);
            else if (line.starts_with("muted=")) status_.muted = line.substr(6) == "1";
        }
        ui::invalidate();
    });
}

// ---- app lifecycle ---------------------------------------------------------
void App::report_exit(int code)
{
    Uint64 ran = SDL_GetTicks() - app_started_;
    std::string tail = last_log_line(app_log_);
    Foyer::logger::info("'{}' exited with {} after {} ms (log {})", app_title_, code, ran, app_log_);
    if (code == 0) return;
    std::string msg;
    if (code == 127) msg = app_title_ + " failed: command not found (" + app_command_ + ")";
    else if (code == 143 || code == 128 + SIGKILL) return;   // we closed it
    else if (code > 128) msg = app_title_ + " crashed (signal " + std::to_string(code - 128) + ")";
    else msg = app_title_ + " exited with error " + std::to_string(code);
    if (!tail.empty()) msg += ": " + tail;
    toast(msg, true);
}

void App::poll_app()
{
    if (app_proc_.alive() && app_proc_.poll()) {
        int code = app_proc_.exit_status();
        app_proc_ = Process();
        if (overlay_proc_.alive()) { overlay_proc_.kill(SIGKILL); overlay_proc_ = Process(); }
        Shell::set_launching("");
        report_exit(code);
        refresh_status();
        ui::invalidate();
    }
    if (overlay_proc_.alive() && overlay_proc_.poll()) overlay_proc_ = Process();
}

// ---- input -----------------------------------------------------------------
void App::on_global(Action a)
{
    switch (a) {
        case Action::Home:
        case Action::Menu: open_quick_panel(); break;
        case Action::VolUp: execute(":volup"); break;
        case Action::VolDown: execute(":voldown"); break;
        case Action::Mute: execute(":mute"); break;
        default: break;
    }
}

void App::dispatch(Action a)
{
    last_input_ = SDL_GetTicks();
    if (screensaver_) { screensaver_ = false; ui::invalidate(); return; }
    if (app_proc_.alive() && !opt_.overlay) {
        // The app has focus; only global actions matter.
        if (a == Action::Home || a == Action::Menu) open_quick_panel();
        return;
    }
    if (engine_->dispatch(a)) return;
    switch (a) {
        case Action::Home:
            if (opt_.overlay) { quit_ = true; break; }
            engine_->pop_to_root();
            if (auto *shell = dynamic_cast<Shell*>(engine_->top())) shell->show_tab(layout_.tabs.front().id);
            break;
        case Action::Menu: open_quick_panel(); break;
        case Action::Back: if (opt_.overlay) quit_ = true; break;
        case Action::VolUp: case Action::VolDown: case Action::Mute: on_global(a); break;
        default: break;
    }
}

void App::handle(SDL_Event &e)
{
    if (jobs_->handle_event(e)) return;
    switch (e.type) {
        case SDL_EVENT_QUIT: quit_ = true; break;
        case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
        case SDL_EVENT_WINDOW_RESIZED:
        case SDL_EVENT_DISPLAY_CURRENT_MODE_CHANGED:
            apply_appearance();
            break;
        case SDL_EVENT_KEY_DOWN: {
            SDL_Keycode k = e.key.key;
            for (auto &hk : settings_.hotkeys)
                if (SDL_GetKeyFromName(hk.key.c_str()) == k && !e.key.repeat) { execute(hk.command); return; }
            if (k == SDLK_F12 && !e.key.repeat) {
                engine_->screenshot("/tmp/foyer-" + std::to_string(time(nullptr)) + ".png");
                return;
            }
            std::optional<Action> a;
            switch (k) {
                case SDLK_HOME: case SDLK_AC_HOME: a = Action::Home; break;
                case SDLK_MENU: case SDLK_APPLICATION: case SDLK_TAB: a = Action::Menu; break;
                case SDLK_BACKSPACE: a = Action::Back; break;
                case SDLK_VOLUMEUP: a = Action::VolUp; break;
                case SDLK_VOLUMEDOWN: a = Action::VolDown; break;
                case SDLK_MUTE: a = Action::Mute; break;
                default: a = ActionMapper::keyboard(k);
            }
            if (a) {
                if (e.key.repeat && *a != Action::Up && *a != Action::Down && *a != Action::Left && *a != Action::Right) return;
                // text entry gets Back/Backspace first
                if (k == SDLK_BACKSPACE && engine_->dispatch_text("\b")) return;
                dispatch(*a);
            }
            break;
        }
        case SDL_EVENT_TEXT_INPUT:
            last_input_ = SDL_GetTicks();
            engine_->dispatch_text(e.text.text);
            break;
        case SDL_EVENT_GAMEPAD_ADDED:
            SDL_OpenGamepad(e.gdevice.which);
            break;
        case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
            if (auto a = ActionMapper::gamepad_button((SDL_GamepadButton) e.gbutton.button)) dispatch(*a);
            break;
        case SDL_EVENT_GAMEPAD_AXIS_MOTION: {
            // edge-trigger the left stick
            static int last[SDL_GAMEPAD_AXIS_COUNT] = {};
            int axis = e.gaxis.axis, v = e.gaxis.value, dir = v > 16000 ? 1 : v < -16000 ? -1 : 0;
            if (axis < SDL_GAMEPAD_AXIS_COUNT && dir != last[axis]) {
                last[axis] = dir;
                if (dir) if (auto a = ActionMapper::gamepad_axis((SDL_GamepadAxis) axis, v)) dispatch(*a);
            }
            break;
        }
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
            if (settings_.mouse_select && e.button.button == SDL_BUTTON_LEFT) dispatch(Action::Accept);
            break;
        default:
            if (e.type == evt_global()) {
                // evdev: only while an app (not us) has focus
                if (app_proc_.alive() && !overlay_proc_.alive()) on_global((Action) (int) (intptr_t) e.user.data1);
            }
            break;
    }
}

int App::run()
{
    refresh_status();
    if (!opt_.screenshot.empty()) {
        // Render until assets are decoded and animations settle, then save.
        Uint64 t0 = SDL_GetTicks();
        auto settle = [this](int max_frames) {
            for (int i = 0; i < max_frames; i++) {
                SDL_Event e;
                while (SDL_PollEvent(&e)) handle(e);
                bool anim = engine_->update(1.f / 60.f);
                engine_->paint();
                if (!anim && !jobs_->busy() && i > 4) break;
                SDL_Delay(2);
            }
        };
        settle(400);
        std::string ks = opt_.keys;
        size_t p = 0;
        while (p < ks.size()) {
            size_t c = ks.find(',', p);
            std::string k = ks.substr(p, c == std::string::npos ? std::string::npos : c - p);
            p = c == std::string::npos ? ks.size() : c + 1;
            static const std::pair<const char*, Action> names[] = {
                {"up", Action::Up}, {"down", Action::Down}, {"left", Action::Left}, {"right", Action::Right},
                {"accept", Action::Accept}, {"a", Action::Accept}, {"back", Action::Back}, {"b", Action::Back},
                {"menu", Action::Menu}, {"home", Action::Home}};
            for (auto &[n, a] : names) if (k == n) dispatch(a);
            settle(60);
        }
        for (int i = 0; i < 600; i++) {
            SDL_Event e;
            while (SDL_PollEvent(&e)) handle(e);
            bool anim = engine_->update(1.f / 60.f);
            engine_->paint();
            if (!anim && !jobs_->busy() && i > 10) break;
            if (SDL_GetTicks() - t0 > 8000) break;
            SDL_Delay(5);
        }
        engine_->paint();
        bool ok = engine_->screenshot(opt_.screenshot);
        return ok ? 0 : 1;
    }

    last_frame_ = SDL_GetTicks();
    Uint64 fps_t = last_frame_;
    while (!quit_) {
        SDL_Event e;
        bool app_fg = app_proc_.alive() && !opt_.overlay;
        // Idle: block on events (with a timeout for clock/status/app polling).
        if (!engine_->needs_paint() || app_fg) {
            if (SDL_WaitEventTimeout(&e, app_fg ? 200 : 250)) handle(e);
        }
        while (SDL_PollEvent(&e)) handle(e);

        poll_app();
        Uint64 now = SDL_GetTicks();
        if (now - last_status_ > 5000 && !app_fg) refresh_status();
        if (settings_.screensaver && !app_fg && !screensaver_ &&
            now - last_input_ > (Uint64) settings_.screensaver_idle_s * 1000) { screensaver_ = true; ui::invalidate(); }

        float dt = std::min(0.1f, (now - last_frame_) / 1000.f);
        last_frame_ = now;
        bool anim = engine_->update(dt);
        if (app_fg) continue;   // the app is on screen; don't burn GPU
        if (anim || ui::is_invalid()) {
            engine_->paint();
            if (screensaver_)
                engine_->canvas().fill({0, 0, (float) engine_->metrics().width(), (float) engine_->metrics().height()},
                                       {0, 0, 0, (Uint8) (settings_.screensaver_dim * 255 / 100)});
            engine_->canvas().present();
            frames_++;
        }
        if (opt_.debug && now - fps_t >= 5000) {
            Foyer::logger::info("{} frames in {} ms, textures {} KiB / {} assets", frames_, now - fps_t,
                                engine_->assets().bytes() >> 10, engine_->assets().count());
            frames_ = 0; fps_t = now;
        }
    }
    return 0;
}
