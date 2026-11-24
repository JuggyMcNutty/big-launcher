#include "app.hpp"
#include "config.hpp"
#include "layout_model.hpp"
#include "../platform/power.hpp"
#include "../platform/sysctl.hpp"
#include "../input/action_mapper.hpp"
#include "../render/util.hpp"
#include "../render/logger.hpp"
#include "../render/image.hpp"
#include "../screens/home.hpp"
#include "../screens/settings/hub.hpp"
#include "../screens/settings/audio.hpp"
#include "../screens/settings/display.hpp"
#include "../screens/settings/network.hpp"
#include "../screens/settings/bluetooth.hpp"
#include "../screens/settings/system.hpp"
#include "../screens/settings/power.hpp"
#include "../screens/settings/launcher.hpp"
#include "../render/basic_lazy_file_sink.hpp"
#include <getopt.h>
#include <filesystem>
#include <spdlog/sinks/stdout_color_sinks.h>

#include <SDL3_image/SDL_image.h>
#include <SDL3_ttf/SDL_ttf.h>

namespace BL {
    static const char *EXECUTABLE_TITLE = "big-launcher";

    std::shared_ptr<spdlog::logger> init_logging_global()
    {
        char *home_dir = getenv("HOME");
        std::filesystem::path log_path = join_paths(home_dir ? home_dir : "/tmp", ".local", "share", EXECUTABLE_TITLE, "big-launcher.log");
        std::error_code ec;
        std::filesystem::create_directories(log_path.parent_path(), ec);
        auto file_sink = std::make_shared<spdlog::sinks::basic_lazy_file_sink_st>(log_path.string(), true);
        file_sink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] %v");
        std::vector<spdlog::sink_ptr> sinks{file_sink};
        auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_st>();
        console_sink->set_level(spdlog::level::warn);
        console_sink->set_pattern("[%^%l%$] %v");
        sinks.push_back(console_sink);
        auto logger = std::make_shared<spdlog::logger>("big-launcher", sinks.begin(), sinks.end());
        logger->set_level(spdlog::level::info);
        spdlog::set_default_logger(logger);
        return logger;
    }
}

BL::App::App(std::string config_path, std::string layout_path)
{
    set_executable_dir(SDL_GetBasePath());
    config_path_ = config_path;
    layout_path_ = layout_path;
    config_.parse(config_path);
    layout_.parse(layout_path);

    init_display();
    create_window();
    renderer_ = std::make_unique<RendererSDL>(*window_);
    if (letterbox_)
        renderer_->set_logical_representation(screen_w_, screen_h_);

    apply_theme();

    if (config_.sound_enabled) {
        try { sound_ = std::make_unique<Sound>(config_.sound_volume); }
        catch (...) { sound_ = nullptr; }
    }

    jobs_ = std::make_unique<JobQueue>();

    // Gamepad subsystem
    if (config_.gamepad_enabled) {
        SDL_InitSubSystem(SDL_INIT_GAMEPAD);
        if (!config_.gamepad_mappings_file.empty())
            SDL_AddGamepadMappingsFromFile(config_.gamepad_mappings_file.c_str());
    }

#ifdef BL_EVDEV
    evdev_ = std::make_unique<EvdevGlobal>();
#endif
#ifdef BL_CEC
    cec_ = std::make_unique<Cec>();
#endif

    build_home();
    BL::logger::debug("App initialized ({}x{})", screen_w_, screen_h_);
}

BL::App::~App()
{
    if (jobs_) jobs_->stop();
    if (window_) SDL_DestroyWindow(window_);
    TTF_Quit();
    SDL_Quit();
}

void BL::App::init_display()
{
    SDL_SetHint(SDL_HINT_VIDEO_X11_NET_WM_BYPASS_COMPOSITOR, "0");
    if (!SDL_Init(SDL_INIT_VIDEO))
        BL::logger::critical("Could not initialize SDL (SDL Error: {})", SDL_GetError());

    int display_count;
    SDL_DisplayID *displays = SDL_GetDisplays(&display_count);
    if (!display_count)
        BL::logger::critical("Could not find any valid display");
    SDL_DisplayID display_id = displays[0];
    SDL_free(displays);

    dm_ = SDL_GetDesktopDisplayMode(display_id);
    if (!dm_)
        BL::logger::critical("Could not get desktop display mode (SDL Error: {})", SDL_GetError());
    screen_w_ = dm_->w;
    screen_h_ = dm_->h;

    // Force 16:9
    float aspect = (float) screen_w_ / screen_h_;
    if (aspect > 1.78f) {
        screen_w_ = (int) std::round(screen_h_ * 1.7777f);
        letterbox_ = true;
    } else if (aspect < 1.76f) {
        screen_h_ = (int) std::round(screen_w_ / 1.7777f);
        letterbox_ = true;
    }

    if (!TTF_Init())
        BL::logger::critical("Could not initialize SDL_ttf (SDL Error: {})", SDL_GetError());
}

void BL::App::create_window()
{
    window_ = SDL_CreateWindow("big-launcher", dm_->w, dm_->h, SDL_WINDOW_FULLSCREEN);
    if (!window_)
        BL::logger::critical("Could not create window (SDL Error: {})", SDL_GetError());
    SDL_HideCursor();
}

void BL::App::apply_theme()
{
    theme_.sidebar_highlight = config_.sidebar_highlight_color;
    theme_.sidebar_text = config_.sidebar_text_color;
    theme_.sidebar_text_hl = config_.sidebar_text_color_hl;
    theme_.menu_highlight = config_.menu_highlight_color;
}

void BL::App::build_home()
{
    screens_.replace(std::make_unique<HomeScreen>(*this));
}

void BL::App::play_click()
{
    if (sound_) sound_->play_click();
}

void BL::App::play_select()
{
    if (sound_) sound_->play_select();
}

void BL::App::show_toast(const std::string &msg)
{
    toast_msg_ = msg;
    toast_until_ = SDL_GetTicks() + 4000;
    mark_dirty();
}

void BL::App::restart_ui()
{
    // :quit — exit; the session script respawns the launcher
    BL::logger::info("Restart UI requested");
    quit_ = true;
}

void BL::App::close_app()
{
    if (app_proc_.alive()) {
        BL::logger::info("Closing app (pid {})", app_proc_.pid());
        app_proc_.kill();
    }
}

void BL::App::execute_command(const std::string &command)
{
    BL::logger::debug("Executing command '{}'", command);
    if (command.starts_with(":")) {
        if (command == ":quit") restart_ui();
        else if (command == ":restart") power::reboot();
        else if (command == ":shutdown") power::shutdown();
        else if (command == ":sleep") power::suspend();
        else if (command == ":home") { close_app(); }
        else if (command == ":settings") screens_.push(std::make_unique<SettingsHub>(*this));
        else if (command == ":audio") screens_.push(std::make_unique<AudioSettings>(*this));
        else if (command == ":display") screens_.push(std::make_unique<DisplaySettings>(*this));
        else if (command == ":network") screens_.push(std::make_unique<NetworkSettings>(*this));
        else if (command == ":wifi") screens_.push(std::make_unique<NetworkSettings>(*this));
        else if (command == ":bt") screens_.push(std::make_unique<BluetoothSettings>(*this));
        else if (command == ":bluetooth") screens_.push(std::make_unique<BluetoothSettings>(*this));
        else if (command == ":system") screens_.push(std::make_unique<SystemSettings>(*this));
        else if (command == ":power") screens_.push(std::make_unique<PowerSettings>(*this));
        else if (command == ":launcher") screens_.push(std::make_unique<LauncherSettings>(*this));
        else if (command == ":layout") { /* layout editor (Phase 3) */ }
        else if (command == ":volup") jobs_->run(ctl("audio", "volume/up"), [this](const JobResult&){ mark_dirty(); });
        else if (command == ":voldown") jobs_->run(ctl("audio", "volume/down"), [this](const JobResult&){ mark_dirty(); });
        else if (command == ":mute") jobs_->run(ctl("audio", "mute/toggle"), [this](const JobResult&){ mark_dirty(); });
        else {
            // Unknown special command: ignore
            BL::logger::warn("Unknown special command '{}'", command);
        }
        return;
    }

    // Launch an application
    app_proc_ = Process::spawn(command);
    if (app_proc_.alive()) {
        BL::logger::info("Launched app (pid {})", app_proc_.pid());
        mark_dirty();
    } else {
        BL::logger::error("Failed to launch '{}'", command);
        show_toast("Failed to launch app");
    }
}

void BL::App::on_global_action(Action action)
{
    // Global input while an app is focused (from evdev/CEC)
    switch (action) {
        case Action::Home:
        case Action::Menu:
            // Open quick menu (Phase 3). For now, just log.
            BL::logger::debug("Global {} while app running", action_name(action));
            break;
        case Action::VolUp:
            jobs_->run(ctl("audio", "volume/up"), nullptr);
            break;
        case Action::VolDown:
            jobs_->run(ctl("audio", "volume/down"), nullptr);
            break;
        case Action::Mute:
            jobs_->run(ctl("audio", "mute/toggle"), nullptr);
            break;
        default:
            break;
    }
}

void BL::App::handle_key(SDL_Event &event)
{
    if (event.key.repeat && event.key.key != SDLK_BACKSPACE) {
        // Allow repeat only for navigation
        auto a = ActionMapper::keyboard(event.key.key);
        if (a && *a != Action::Up && *a != Action::Down &&
            *a != Action::Left && *a != Action::Right)
            return;
    }
    auto action = ActionMapper::keyboard(event.key.key);
    if (action) {
        last_input_ = SDL_GetTicks();
        wake_screensaver();
        dispatch_action(*action);
        return;
    }
    // Check hotkeys
    for (const auto &hk : config_.hotkeys) {
        if (hk.keycode == event.key.key) {
            execute_command(hk.command);
            return;
        }
    }
}

void BL::App::handle_gamepad(SDL_Event &event)
{
    last_input_ = SDL_GetTicks();
    wake_screensaver();
    if (event.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN) {
        auto a = ActionMapper::gamepad_button((SDL_GamepadButton) event.gbutton.button);
        if (a) dispatch_action(*a);
    }
    else if (event.type == SDL_EVENT_GAMEPAD_AXIS_MOTION) {
        auto a = ActionMapper::gamepad_axis((SDL_GamepadAxis) event.gaxis.axis, event.gaxis.value);
        if (a) dispatch_action(*a);
    }
}

void BL::App::wake_screensaver()
{
    if (screensaver_active_) {
        screensaver_active_ = false;
        mark_dirty();
    }
}

void BL::App::dispatch_action(Action action)
{
    wake_screensaver();
    // Home/Menu are global
    if (action == Action::Home || action == Action::Menu) {
        if (app_proc_.alive()) {
            on_global_action(action);
            return;
        }
        // No app: Home returns to home screen (pop to bottom)
        if (action == Action::Home) {
            while (screens_.size() > 1) screens_.pop();
            mark_dirty();
            return;
        }
    }
    if (Screen *s = screens_.top()) {
        if (!s->on_action(action)) {
            if (action == Action::Back && screens_.size() > 1)
                screens_.pop();
        }
        mark_dirty();
    }
}

void BL::App::handle_event(SDL_Event &event)
{
    if (jobs_->handle_event(event))
        return;

    switch (event.type) {
        case SDL_EVENT_QUIT:
            quit_ = true;
            break;
        case SDL_EVENT_KEY_DOWN:
            handle_key(event);
            break;
        case SDL_EVENT_TEXT_INPUT: {
            std::string text(event.text.text);
            if (Screen *s = screens_.top())
                s->on_text(text);
            last_input_ = SDL_GetTicks();
            break;
        }
        case SDL_EVENT_GAMEPAD_ADDED:
            BL::logger::debug("Gamepad added: {}", SDL_GetGamepadNameForID(event.gdevice.which));
            break;
        case SDL_EVENT_GAMEPAD_REMOVED:
            BL::logger::debug("Gamepad removed");
            break;
        case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
        case SDL_EVENT_GAMEPAD_AXIS_MOTION:
            handle_gamepad(event);
            break;
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
            if (config_.mouse_select && event.button.button == SDL_BUTTON_LEFT) {
                last_input_ = SDL_GetTicks();
                dispatch_action(Action::Accept);
            }
            break;
        default:
            break;
    }
}

void BL::App::update_app_state()
{
    if (app_proc_.alive()) {
        if (app_proc_.poll()) {
            int code = app_proc_.exit_status();
            BL::logger::info("App exited with status {}", code);
            app_proc_ = Process();
            mark_dirty();
            if (code != 0)
                show_toast("App exited with an error");
        }
    }
}

void BL::App::refresh_status()
{
    last_status_refresh_ = SDL_GetTicks();
    jobs_->run(ctl("net", "status"), [this](const JobResult &res) {
        if (!res.ok) return;
        std::string net;
        for (const auto &line : split_lines(res.output)) {
            if (line.starts_with("eth=")) net = "eth " + line.substr(4);
            else if (line.starts_with("wifi=") && line.size() > 5 && line.substr(5) != "no adapter")
                net = "wifi " + line.substr(5);
        }
        status_bar_.set_network(net);
        mark_dirty();
    });
    jobs_->run(ctl("audio", "volume/get"), [this](const JobResult &res) {
        if (!res.ok) return;
        int vol = -1; bool mute = false;
        for (const auto &line : split_lines(res.output)) {
            if (line.starts_with("volume=")) vol = std::atoi(line.substr(7).c_str());
            else if (line.starts_with("muted=")) mute = line.substr(6) == "1";
        }
        std::string a = mute ? "muted" : (vol >= 0 ? ("vol " + std::to_string(vol)) : "");
        status_bar_.set_audio(a);
        mark_dirty();
    });
}

void BL::App::schedule_status_refresh()
{
    if (SDL_GetTicks() - last_status_refresh_ >= 5000)
        refresh_status();
}

void BL::App::draw()
{
    auto &r = *renderer_;
    r.clear();
    r.fill_rect({0.f, 0.f, (float) screen_w_, (float) screen_h_}, theme_.background);

    if (!app_proc_.alive() && !screens_.empty()) {
        screens_.top()->draw();
        status_bar_.draw(r, theme_, (float) screen_w_, (float) screen_h_);
    }

    // Screensaver dim
    if (screensaver_active_) {
        SDL_Color dim{0, 0, 0, config_.screensaver_intensity};
        r.fill_rect({0.f, 0.f, (float) screen_w_, (float) screen_h_}, dim);
    }

    // Toast
    if (!toast_msg_.empty() && SDL_GetTicks() < toast_until_) {
        TextSprite ts;
        ts.render(r, theme_.info_font(), toast_msg_);
        float tw = ts.width() + 40;
        r.fill_rect({(screen_w_ - tw) / 2.f, (float) screen_h_ - 120.f, tw, 60.f}, {0, 0, 0, 0xCC});
        ts.draw(r, (screen_w_ - ts.width()) / 2, screen_h_ - 108, {0xFF, 0xFF, 0xFF, 0xFF});
    }
    else if (!toast_msg_.empty() && SDL_GetTicks() >= toast_until_) {
        toast_msg_.clear();
    }

    r.present();
}

int BL::App::run()
{
    BL::logger::debug("Begin main loop");
    refresh_status();
    while (!quit_) {
        SDL_Event event;
        while (SDL_PollEvent(&event))
            handle_event(event);

        update_app_state();
        schedule_status_refresh();
        anim_.update();

        // Screensaver (dim on idle)
        if (config_.screensaver_enabled && !app_proc_.alive()) {
            Uint64 idle = SDL_GetTicks() - last_input_;
            if (idle >= config_.screensaver_idle_time && !screensaver_active_) {
                screensaver_active_ = true;
                mark_dirty();
            }
        }

        if (Screen *s = screens_.top())
            s->update();

        if (dirty_ || anim_.active()) {
            draw();
            dirty_ = false;
        }
        else
            SDL_Delay(10);
    }
    return 0;
}
