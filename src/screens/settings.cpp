#include <algorithm>
#include <cmath>

#include <lconfig.h>
#include "settings.hpp"
#include "editor.hpp"
#include "keyboard.hpp"
#include "../core/app.hpp"
#include "../render/util.hpp"
#include "../ui/engine/focus.hpp"

namespace Foyer {
    using namespace ui;

    namespace {
        struct Category { const char *id, *title, *sub, *icon; };
        const Category kCategories[] = {
            {"appearance", "Appearance", "Theme, accent, wallpaper", "palette"},
            {"layout", "Layout", "Tabs, rows and tiles", "layout"},
            {"display", "Display", "Resolution and scaling", "display"},
            {"audio", "Audio", "Output and volume", "volume"},
            {"network", "Network", "Ethernet and WiFi", "network"},
            {"bluetooth", "Bluetooth", "Pair devices", "bluetooth"},
            {"launcher", "Launcher", "Sounds, clock, input", "sliders"},
            {"system", "System", "About this device", "info"},
            {"power", "Power", "Sleep, restart, shut down", "power"},
        };

        // A settings category tile (icon + title + subtitle).
        class CategoryTile : public Button {
        public:
            CategoryTile(App &app, const Category &c, float w): Button("")
            {
                style.dir = Dir::Column;
                style.align = Align::Start;
                style.justify = Justify::End;
                style.w = Dim::dp(w);
                style.h = Dim::dp(w * 0.5f);
                style.pad = Edges::all(24);
                style.gap = 6;
                style.shrink = 0.f;
                radius = 20;
                has_bg_base_ = true;
                remove_child(label);
                remove_child(value);
                icon_ = add<Image>(app.icon(c.icon));
                icon_->style.w = Dim::dp(44); icon_->style.h = Dim::dp(44);
                icon_->has_tint = true;
                Spacer::fill(this);
                label = add<Text>(c.title, TextRole::Lg);
                value = add<Text>(c.sub, TextRole::Sm);
                value->max_lines = 1;
            }
            bool tick(Engine &e, float dt) override
            {
                bool a = Button::tick(e, dt);
                icon_->tint = mix(e.tokens().accent, e.tokens().on_accent, highlight());
                scale.to(focused() ? 1.03f : 1.f);
                return a;
            }
        private:
            Image *icon_;
        };
    }

    Node* build_settings_hub(App &app, Engine &e, Node &into)
    {
        const Tokens &t = e.tokens();
        auto *head = into.add<Node>();
        head->style.pad = Edges{t.page_margin, 30, t.page_margin, 18};
        auto *ht = head->add<Text>("Settings", TextRole::Xl);
        ht->display_font = true;
        auto *sc = into.add<Scroller>(Dir::Column);
        sc->style.grow = 1.f;
        sc->style.shrink = 1.f;
        sc->style.pad = Edges{t.page_margin, 10, t.page_margin, 40};
        sc->style.gap = t.tile_gap;
        sc->margin = 30;
        float avail = e.metrics().safe_width_dp() - 2 * t.page_margin;
        int cols = e.metrics().breakpoint() == Breakpoint::XS ? 2 : avail > 1500 ? 4 : 3;
        float w = std::min(520.f, (avail - (cols - 1) * t.tile_gap) / cols);
        Node *row = nullptr, *first = nullptr;
        int i = 0;
        for (auto &c : kCategories) {
            if (i % cols == 0) {
                row = sc->add<Node>();
                row->style.dir = Dir::Row;
                row->style.gap = t.tile_gap;
                row->style.shrink = 0.f;
                row->style.pad = Edges{0, 6, 0, 6};
            }
            auto *tile = row->add<CategoryTile>(app, c, w);
            std::string id = c.id;
            tile->id = std::string("settings:") + id;
            tile->on_activate = [&app, id] { open_settings_page(app, id); };
            if (!first) first = tile;
            i++;
        }
        return first;
    }

    void open_settings_page(App &app, const std::string &id)
    {
        std::unique_ptr<Layer> p;
        if (id == "audio") p = std::make_unique<AudioPage>(app);
        else if (id == "display") p = std::make_unique<DisplayPage>(app);
        else if (id == "network") p = std::make_unique<NetworkPage>(app);
        else if (id == "bluetooth") p = std::make_unique<BluetoothPage>(app);
        else if (id == "system") p = std::make_unique<SystemPage>(app);
        else if (id == "power") p = std::make_unique<PowerPage>(app);
        else if (id == "appearance") p = std::make_unique<AppearancePage>(app);
        else if (id == "launcher") p = std::make_unique<LauncherPage>(app);
        else if (id == "layout") p = std::make_unique<LayoutEditor>(app);
        if (p) app.ui().push(std::move(p));
    }

    static std::vector<std::string> tabs(const std::string &line)
    {
        std::vector<std::string> f;
        size_t s = 0;
        while (true) {
            size_t t = line.find('\t', s);
            f.push_back(line.substr(s, t == std::string::npos ? std::string::npos : t - s));
            if (t == std::string::npos) break;
            s = t + 1;
        }
        return f;
    }

    static std::string kv(const std::string &out, const std::string &key)
    {
        for (auto &l : split_lines(out))
            if (l.starts_with(key + "=")) return l.substr(key.size() + 1);
        return "";
    }

    // ---- Audio -------------------------------------------------------------------
    void AudioPage::fill(Engine &e, Node &b)
    {
        (void) e;
        kit::section(&b, "Volume");
        vol_ = row("Volume", "--", nullptr, "volume");
        vol_->on_adjust = [this](Action a) {
            if (volume_ < 0) return true;
            int v = std::clamp(volume_ + (a == Action::Right ? 5 : -5), 0, 150);
            volume_ = v;
            vol_->set_value(std::to_string(v) + "%");
            app_.play_click();
            app_.jobs().run(ctl("audio", "volume/set", {std::to_string(v)}), [this](const JobResult&) { app_.refresh_status(); });
            return true;
        };
        vol_->on_activate = [this] { vol_->on_adjust(Action::Right); };
        mute_ = row("Mute", "--", [this] {
            app_.jobs().run(ctl("audio", "mute/toggle"), [this](const JobResult&) { refresh(); app_.refresh_status(); });
        }, "mute");
        kit::section(&b, "Output");
        outputs_ = b.add<Node>();
        outputs_->style.dir = Dir::Column;
        outputs_->style.gap = 8;
        kit::info_row(outputs_, "Looking for outputs\xE2\x80\xA6", "");
        kit::section(&b, "Interface");
        ui_sounds_ = row("UI sounds", kit::onoff(app_.settings().sound), [this] {
            app_.settings().sound = !app_.settings().sound;
            ui_sounds_->set_value(kit::onoff(app_.settings().sound));
            app_.save_settings();
            app_.toast("UI sounds take effect after restarting the launcher");
        }, "volume");
    }

    void AudioPage::on_enter(Engine &e) { (void) e; refresh(); }

    void AudioPage::refresh()
    {
        app_.jobs().run(ctl("audio", "volume/get"), [this](const JobResult &r) {
            if (!r.ok) { vol_->set_value("unavailable"); return; }
            std::string v = kv(r.output, "volume");
            volume_ = v.empty() ? -1 : std::atoi(v.c_str());
            muted_ = kv(r.output, "muted") == "1";
            vol_->set_value(volume_ >= 0 ? std::to_string(volume_) + "%" : "--");
            mute_->set_value(kit::onoff(muted_));
        });
        app_.jobs().run(ctl("audio", "outputs"), [this](const JobResult &r) {
            outputs_->clear_children();
            int n = 0;
            for (auto &line : split_lines(r.output)) {
                auto f = tabs(line);
                if (f.size() < 4) continue;
                std::string card = f[0], prof = f[1], desc = f[2];
                bool active = f[3] == "1";
                auto *btn = outputs_->add<kit::RowButton>(app_, desc, active ? "Active" : "",
                                                          prof.find("hdmi") != std::string::npos ? "display" : "volume");
                btn->on_activate = [this, card, prof, desc] {
                    app_.jobs().run(ctl("audio", "output/set", {card, prof}), [this, desc](const JobResult &res) {
                        app_.toast(res.ok ? "Audio output: " + desc : "Could not switch output", !res.ok);
                        refresh();
                    });
                };
                n++;
            }
            if (!n) kit::info_row(outputs_, r.ok ? "No audio outputs found" : "Audio service unavailable", "");
            invalidate_layout();
        });
    }

    // ---- Display -----------------------------------------------------------------
    void DisplayPage::fill(Engine &e, Node &b)
    {
        kit::section(&b, "Output");
        mode_ = row("Resolution", "--", [this] { pick_mode(); }, "display",
                    "Modes reported by your TV or monitor");
        render_ = row("Render resolution", "Native", [this] { pick_render(); }, "sliders",
                      "Render below output and upscale (faster on low-power boards)");
        kit::section(&b, "Interface");
        auto &s = app_.settings();
        scale_ = row("UI scale", std::to_string(s.ui_scale) + "%", nullptr, "layout", "Left / Right to adjust");
        scale_->on_adjust = [this](Action a) {
            auto &st = app_.settings();
            st.ui_scale = std::clamp(st.ui_scale + (a == Action::Right ? 10 : -10), 50, 200);
            scale_->set_value(std::to_string(st.ui_scale) + "%");
            app_.save_settings();
            app_.apply_appearance();
            return true;
        };
        overscan_ = row("TV safe area", std::to_string(s.overscan) + "%", nullptr, "display",
                        "Shrinks the UI if your TV crops the edges");
        overscan_->on_adjust = [this](Action a) {
            auto &st = app_.settings();
            st.overscan = std::clamp(st.overscan + (a == Action::Right ? 1 : -1), 0, 10);
            overscan_->set_value(std::to_string(st.overscan) + "%");
            app_.save_settings();
            app_.apply_appearance();
            return true;
        };
        kit::section(&b, "Current");
        const auto &m = e.metrics();
        kit::info_row(&b, "Launcher surface", std::to_string(m.width()) + " \xC3\x97 " + std::to_string(m.height()));
        kit::info_row(&b, "Layout class", std::string(breakpoint_name(m.breakpoint())) + "  (scale " +
                      std::to_string((int) std::lround(m.scale() * 100)) + "%)");
    }

    void DisplayPage::on_enter(Engine &e) { (void) e; refresh(); }

    void DisplayPage::refresh()
    {
        app_.jobs().run(ctl("display", "mode/get"), [this](const JobResult &r) {
            auto lines = split_lines(r.output);
            current_ = lines.empty() ? "auto" : lines[0];
            render_cur_ = kv(r.output, "render");
            if (render_cur_.empty()) render_cur_ = "native";
            mode_->set_value(current_ == "auto" ? "Automatic" : current_);
            render_->set_value(render_cur_ == "native" ? "Native" : render_cur_);
        });
        app_.jobs().run(ctl("display", "modes"), [this](const JobResult &r) {
            modes_.clear();
            preferred_.clear();
            for (auto &line : split_lines(r.output)) {
                auto f = tabs(line);
                if (f[0] == "connector" && f.size() >= 3 && f[2] == "connected") connector_ = f[1];
                if (f[0] == "mode" && f.size() >= 2) {
                    modes_.push_back(f[1]);
                    if (f.size() >= 3 && f[2] == "preferred") preferred_ = f[1];
                }
            }
            if (!connector_.empty() && mode_->sub) mode_->sub->set("Connected: " + connector_ +
                                                                    (preferred_.empty() ? "" : "  \xE2\x80\xA2  preferred " + preferred_));
        });
    }

    void DisplayPage::pick_mode()
    {
        if (modes_.empty()) { app_.toast("No display modes reported (is a TV connected?)", true); return; }
        std::vector<std::string> opts{"Automatic (monitor preferred" + (preferred_.empty() ? std::string(")") : ": " + preferred_ + ")")};
        std::vector<std::string> vals{"auto"};
        int sel = current_ == "auto" ? 0 : -1;
        for (auto &m : modes_) {
            for (int hz : {60, 50, 30}) {
                if (hz != 60 && m.find("2160") == std::string::npos && hz == 30) continue;
                std::string v = m + "@" + std::to_string(hz);
                opts.push_back(m + "  " + std::to_string(hz) + " Hz" + (m == preferred_ && hz == 60 ? "  (preferred)" : ""));
                vals.push_back(v);
                if (v == current_) sel = (int) vals.size() - 1;
            }
        }
        app_.ui().push(std::make_unique<ChoiceSheet>(app_, "Resolution", opts, std::max(0, sel), [this, vals](int i) {
            std::string v = vals[i];
            if (v == current_) return;
            app_.jobs().run(ctl("display", "mode/set", {v}), [this](const JobResult &res) {
                if (!res.ok) app_.toast("Could not change resolution: " + res.output, true);
                else app_.toast("Switching resolution\xE2\x80\xA6 You'll be asked to keep it.");
            });
        }));
    }

    void DisplayPage::pick_render()
    {
        std::vector<std::string> opts{"Native (same as output)"}, vals{"native"};
        for (auto *m : {"3840x2160", "2560x1440", "1920x1080", "1600x900", "1280x720"}) { opts.push_back(m); vals.push_back(m); }
        int sel = 0;
        for (size_t i = 0; i < vals.size(); i++) if (vals[i] == render_cur_) sel = (int) i;
        app_.ui().push(std::make_unique<ChoiceSheet>(app_, "Render resolution", opts, sel, [this, vals](int i) {
            app_.jobs().run(ctl("display", "render/set", {vals[i]}), [this](const JobResult &res) {
                app_.toast(res.ok ? "Restarting the session\xE2\x80\xA6" : "Could not change render size", !res.ok);
            });
        }));
    }

    // ---- Network -----------------------------------------------------------------
    void NetworkPage::fill(Engine &e, Node &b)
    {
        (void) e;
        ifaces_ = b.add<Node>();
        ifaces_->style.dir = Dir::Column;
        ifaces_->style.gap = 4;
        kit::info_row(ifaces_, "Reading network status\xE2\x80\xA6", "");
        kit::section(&b, "WiFi");
        row("Scan for networks", "", [this] { scan(); }, "wifi");
        nets_ = b.add<Node>();
        nets_->style.dir = Dir::Column;
        nets_->style.gap = 8;
    }

    void NetworkPage::on_enter(Engine &e) { (void) e; refresh(); }

    void NetworkPage::refresh()
    {
        app_.jobs().run(ctl("net", "status"), [this](const JobResult &r) {
            NetStatus st = parse_net_status(r.output);
            ifaces_->clear_children();
            wifi_state_ = st.wifi;
            has_wifi_ = false;
            if (st.ifaces.empty())
                kit::info_row(ifaces_, r.ok ? "No network interfaces found" : "Could not read network status", "");
            for (auto &i : st.ifaces) {
                bool wifi = i.type == "wifi";
                has_wifi_ |= wifi;
                kit::section(ifaces_, std::string(wifi ? "WiFi" : "Ethernet") + "  \xC2\xB7  " + i.name);
                kit::info_row(ifaces_, "Status", i.state == "up" ? "Connected" : i.state);
                kit::info_row(ifaces_, "IP address", i.ip == "-" ? "" : i.ip);
                if (!wifi) kit::info_row(ifaces_, "Link speed", i.speed == "-" ? "" : i.speed);
                kit::info_row(ifaces_, "Gateway", i.gateway == "-" ? "" : i.gateway);
                kit::info_row(ifaces_, "DNS", i.dns == "-" ? "" : i.dns);
                kit::info_row(ifaces_, "MAC address", i.mac);
            }
            if (!has_wifi_) {
                kit::section(ifaces_, "WiFi");
                kit::info_row(ifaces_, "Adapter", wifi_state_ == "no adapter" ? "None detected" : wifi_state_);
            }
            invalidate_layout();
        });
    }

    void NetworkPage::scan()
    {
        if (wifi_state_ == "no adapter") { app_.toast("No WiFi adapter \xE2\x80\x94 plug in a USB WiFi dongle"); return; }
        app_.toast("Scanning for WiFi networks\xE2\x80\xA6");
        app_.jobs().run(ctl("wifi", "scan"), [this](const JobResult &r) {
            nets_->clear_children();
            int n = 0;
            for (auto &line : split_lines(r.output)) {
                auto f = tabs(line);
                if (f.size() < 2) continue;
                std::string ssid = f[0];
                int dbm = std::atoi(f[1].c_str());
                std::string bars = dbm > -55 ? "Excellent" : dbm > -67 ? "Good" : dbm > -78 ? "Fair" : "Weak";
                auto *btn = nets_->add<kit::RowButton>(app_, ssid, bars, "wifi");
                btn->on_activate = [this, ssid] {
                    app_.ui().push(std::make_unique<KeyboardSheet>(app_, "Password for \"" + ssid + "\"", "", true,
                        [this, ssid](const std::string &psk) {
                            app_.toast("Connecting to " + ssid + "\xE2\x80\xA6");
                            app_.jobs().run(ctl("wifi", "connect", {ssid, psk}), [this, ssid](const JobResult &res) {
                                app_.toast(res.ok ? "Connected to " + ssid : "Could not connect (check the password)", !res.ok);
                                refresh();
                                app_.refresh_status();
                            });
                        }));
                };
                n++;
            }
            if (!n) kit::info_row(nets_, "No networks found", "");
            invalidate_layout();
        });
    }

    // ---- Bluetooth ---------------------------------------------------------------
    void BluetoothPage::fill(Engine &e, Node &b)
    {
        (void) e;
        power_ = row("Bluetooth", "--", [this] {
            if (!available_) { app_.toast("No Bluetooth adapter detected"); return; }
            app_.jobs().run(ctl("bt", "power", {powered_ ? "off" : "on"}), [this](const JobResult&) { refresh(); });
        }, "bluetooth");
        row("Search for devices", "", [this] {
            if (!available_ || !powered_) { app_.toast(available_ ? "Turn Bluetooth on first" : "No Bluetooth adapter detected"); return; }
            app_.toast("Searching for 10 seconds\xE2\x80\xA6 put your device in pairing mode");
            app_.jobs().run(ctl("bt", "scan"), [this](const JobResult &r) { list(r.output); });
        }, "search");
        kit::section(&b, "Devices");
        devs_ = b.add<Node>();
        devs_->style.dir = Dir::Column;
        devs_->style.gap = 8;
    }

    void BluetoothPage::on_enter(Engine &e) { (void) e; refresh(); }

    void BluetoothPage::refresh()
    {
        app_.jobs().run(ctl("bt", "status"), [this](const JobResult &r) {
            std::string s = split_lines(r.output).empty() ? "" : split_lines(r.output)[0];
            available_ = r.ok && !s.empty() && s != "unavailable";
            powered_ = s == "powered";
            power_->set_value(!available_ ? "No adapter" : kit::onoff(powered_));
            if (available_) app_.jobs().run(ctl("bt", "devices"), [this](const JobResult &d) { list(d.output); });
            else { devs_->clear_children(); kit::info_row(devs_, "No Bluetooth adapter detected", ""); }
        });
    }

    void BluetoothPage::list(const std::string &out)
    {
        devs_->clear_children();
        int n = 0;
        for (auto &line : split_lines(out)) {
            auto f = tabs(line);
            if (f.size() < 2) continue;
            std::string mac = f[0], name = f[1];
            auto *btn = devs_->add<kit::RowButton>(app_, name, "Pair", "bluetooth", mac);
            btn->on_activate = [this, mac, name] {
                app_.toast("Pairing " + name + "\xE2\x80\xA6");
                app_.jobs().run(ctl("bt", "pair", {mac}), [this, name](const JobResult &r) {
                    app_.toast(r.ok ? "Connected " + name : "Pairing failed", !r.ok);
                });
            };
            n++;
        }
        if (!n) kit::info_row(devs_, "No devices yet", "");
        invalidate_layout();
    }

    // ---- System ------------------------------------------------------------------
    void SystemPage::fill(Engine &e, Node &b)
    {
        (void) e;
        kit::section(&b, "Foyer");
        kit::info_row(&b, "Version", PROJECT_VERSION_STR);
        kit::info_row(&b, "Config", Paths::config_dir());
        std::string theme_title = app_.settings().theme;
        for (auto &t : list_themes()) if (t.name == theme_title) theme_title = t.title;
        kit::info_row(&b, "Theme", theme_title);
        kit::section(&b, "Device");
        info_ = b.add<Node>();
        info_->style.dir = Dir::Column;
        kit::info_row(info_, "Loading\xE2\x80\xA6", "");
        kit::section(&b, "Maintenance");
        row("Sync clock (NTP)", "", [this] {
            app_.jobs().run(ctl("sys", "ntp/sync"), [this](const JobResult &r) { app_.toast(r.ok ? "Clock synced" : "Sync failed", !r.ok); refresh(); });
        }, "restart");
        row("Restart launcher", "", [this] { app_.quit(); }, "restart");
    }

    void SystemPage::on_enter(Engine &e) { (void) e; refresh(); }

    void SystemPage::refresh()
    {
        app_.jobs().run(ctl("sys", "info"), [this](const JobResult &r) {
            info_->clear_children();
            static const std::pair<const char*, const char*> keys[] = {
                {"hostname", "Name"}, {"timezone", "Time zone"}, {"storage", "Storage"},
                {"ram", "Memory"}, {"temp", "Temperature"}, {"uptime", "Uptime"}};
            for (auto &[k, label] : keys) kit::info_row(info_, label, kv(r.output, k));
            invalidate_layout();
        });
    }

    // ---- Power -------------------------------------------------------------------
    void PowerPage::fill(Engine &e, Node &b)
    {
        (void) e; (void) b;
        auto confirm = [this](const std::string &title, const std::string &cmd) {
            app_.ui().push(std::make_unique<ConfirmDialog>(app_, title + "?", "", title, "Cancel",
                [this, cmd](bool yes) { if (yes) app_.execute(cmd); }));
        };
        row("Sleep", "", [this] { app_.execute(":sleep"); }, "moon");
        row("Restart", "", [confirm] { confirm("Restart", ":restart"); }, "restart");
        row("Shut down", "", [confirm] { confirm("Shut down", ":shutdown"); }, "power");
        row("Restart launcher", "", [this] { app_.quit(); }, "home");
    }

    // ---- Appearance --------------------------------------------------------------
    void AppearancePage::fill(Engine &e, Node &b)
    {
        (void) e;
        auto &s = app_.settings();
        auto themes = list_themes();
        std::string cur_title = s.theme;
        for (auto &t : themes) if (t.name == s.theme) cur_title = t.title;
        kit::section(&b, "Theme");
        row("Theme", cur_title, [this, themes] {
            std::vector<std::string> names;
            int sel = 0;
            for (size_t i = 0; i < themes.size(); i++) {
                names.push_back(themes[i].title + (themes[i].author.empty() ? "" : "  \xC2\xB7  " + themes[i].author));
                if (themes[i].name == app_.settings().theme) sel = (int) i;
            }
            app_.ui().push(std::make_unique<ChoiceSheet>(app_, "Theme", names, sel, [this, themes](int i) {
                app_.settings().theme = themes[i].name;
                app_.save_settings();
                app_.apply_appearance();   // rebuilds every layer live
            }));
        }, "palette", "Drop your own into /etc/foyer/themes/");

        static const std::pair<const char*, const char*> accents[] = {
            {"Theme default", ""}, {"Blue", "#4F8CFF"}, {"Violet", "#8B5CF6"}, {"Pink", "#EC4899"},
            {"Red", "#E5484D"}, {"Orange", "#F97316"}, {"Amber", "#F5B73B"}, {"Green", "#22C55E"},
            {"Teal", "#14B8A6"}, {"Cyan", "#06B6D4"}, {"Steam", "#1A9FFF"}};
        std::string acc = "Theme default";
        for (auto &[n, v] : accents) if (s.accent == v) acc = n;
        if (!s.accent.empty() && acc == "Theme default") acc = s.accent;
        row("Accent colour", acc, [this] {
            std::vector<std::string> names;
            int sel = 0;
            for (size_t i = 0; i < std::size(accents); i++) {
                names.push_back(accents[i].first);
                if (app_.settings().accent == accents[i].second) sel = (int) i;
            }
            app_.ui().push(std::make_unique<ChoiceSheet>(app_, "Accent colour", names, sel, [this](int i) {
                app_.settings().accent = accents[i].second;
                app_.save_settings();
                app_.apply_appearance();
            }));
        }, "palette");

        std::string wp = s.wallpaper.empty() ? "Theme default" : s.wallpaper == "focused-art" ? "Focused app art" :
                         s.wallpaper[0] == '#' ? "Solid colour" : "Image";
        row("Wallpaper", wp, [this] {
            std::vector<std::string> opts{"Theme default", "Focused app art", "Solid colour (theme background)", "Image\xE2\x80\xA6"};
            app_.ui().push(std::make_unique<ChoiceSheet>(app_, "Wallpaper", opts, 0, [this](int i) {
                auto &st = app_.settings();
                if (i == 0) st.wallpaper = "";
                else if (i == 1) st.wallpaper = "focused-art";
                else if (i == 2) st.wallpaper = color_hex(app_.ui().tokens().bg);
                else {
                    app_.ui().push(std::make_unique<FilePicker>(app_, "Wallpaper image", "/etc/foyer/art",
                        std::vector<std::string>{".png", ".jpg", ".jpeg", ".webp", ".svg"}, [this](const std::string &p) {
                            app_.settings().wallpaper = p;
                            app_.save_settings();
                            app_.apply_appearance();
                        }));
                    return;
                }
                app_.save_settings();
                app_.apply_appearance();
            }));
        }, "display");

        kit::section(&b, "Motion");
        auto *rm = row("Reduce motion", kit::onoff(s.reduce_motion), nullptr, "sliders", "Turns off zoom and slide animations");
        rm->on_activate = [this, rm] {
            app_.settings().reduce_motion = !app_.settings().reduce_motion;
            rm->set_value(kit::onoff(app_.settings().reduce_motion));
            app_.save_settings();
            app_.apply_appearance();
        };
        auto speed_label = [](float v) {
            return v <= 0.7f ? std::string("Relaxed") : v >= 1.4f ? std::string("Snappy") : std::string("Normal");
        };
        auto *sp = row("Animation speed", speed_label(s.motion_speed), nullptr, "sliders", "Left / Right to adjust");
        sp->on_adjust = [this, sp, speed_label](Action a) {
            float &v = app_.settings().motion_speed;
            v = a == Action::Right ? (v < 0.9f ? 1.f : 1.6f) : (v > 1.1f ? 1.f : 0.6f);
            sp->set_value(speed_label(v));
            app_.save_settings();
            app_.apply_appearance();
            return true;
        };
    }

    // ---- Launcher ----------------------------------------------------------------
    void LauncherPage::fill(Engine &e, Node &b)
    {
        (void) e;
        auto &s = app_.settings();
        auto toggle = [this](const std::string &label, bool &val, const std::string &icon, const std::string &sub = "") {
            auto *r = row(label, kit::onoff(val), nullptr, icon, sub);
            r->on_activate = [this, r, &val] {
                val = !val;
                r->set_value(kit::onoff(val));
                app_.save_settings();
                ui::invalidate();
            };
            return r;
        };
        kit::section(&b, "Home screen");
        toggle("Clock", s.clock, "info");
        auto *fmt = row("Clock format", s.clock_format == "%H:%M" ? "24-hour" : "12-hour", nullptr, "info");
        fmt->on_activate = [this, fmt] {
            auto &st = app_.settings();
            st.clock_format = st.clock_format == "%H:%M" ? "%I:%M %p" : "%H:%M";
            fmt->set_value(st.clock_format == "%H:%M" ? "24-hour" : "12-hour");
            app_.save_settings();
        };
        kit::section(&b, "Screensaver");
        toggle("Dim when idle", s.screensaver, "moon");
        auto *idle = row("Idle time", std::to_string(s.screensaver_idle_s / 60) + " min", nullptr, "moon", "Left / Right to adjust");
        idle->on_adjust = [this, idle](Action a) {
            int &v = app_.settings().screensaver_idle_s;
            v = std::clamp(v + (a == Action::Right ? 60 : -60), 60, 3600);
            idle->set_value(std::to_string(v / 60) + " min");
            app_.save_settings();
            return true;
        };
        kit::section(&b, "Input");
        toggle("Gamepad", s.gamepad, "sliders", "Applies after restarting the launcher");
        toggle("Mouse click selects", s.mouse_select, "sliders");
        kit::section(&b, "Startup");
        auto *as = row("Start app on boot", s.autostart.empty() ? "None" : s.autostart, nullptr, "play");
        as->on_activate = [this, as] {
            std::vector<std::string> names{"None"}, cmds{""};
            for (auto &[id, a] : app_.apps()) { names.push_back(a.title); cmds.push_back(":app " + id); }
            int sel = 0;
            for (size_t i = 0; i < cmds.size(); i++) if (cmds[i] == app_.settings().autostart) sel = (int) i;
            app_.ui().push(std::make_unique<ChoiceSheet>(app_, "Start app on boot", names, sel, [this, as, names, cmds](int i) {
                app_.settings().autostart = cmds[i];
                as->set_value(i == 0 ? "None" : names[i]);
                app_.save_settings();
            }));
        };
    }
}
