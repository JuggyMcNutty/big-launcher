#include <algorithm>
#include <cmath>
#include <cstdio>

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
            {"display", "Display", "Resolution and panel info", "display"},
            {"audio", "Audio", "Output and volume", "volume"},
            {"network", "Network", "Ethernet and WiFi", "network"},
            {"bluetooth", "Bluetooth", "Pair devices", "bluetooth"},
            {"launcher", "Launcher", "Sounds, clock, input", "sliders"},
            {"system", "System", "About this device", "info"},
            {"stats", "Stats", "CPU, memory, temperatures", "stats"},
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
        else if (id == "stats") p = std::make_unique<StatsPage>(app);
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
    // "3840x2160@60" -> a friendly mode label ("3840x2160 × 60 Hz"), or "".
    static std::string mode_label(const std::string &s)
    {
        if (s.empty()) return "";
        auto x = s.find('x');
        auto at = s.find('@');
        if (x == std::string::npos) return s;
        DisplayMode m;
        m.w = atoi(s.c_str());
        m.h = atoi(s.c_str() + x + 1);
        m.rate = at == std::string::npos ? 60 : std::max(atoi(s.c_str() + at + 1), 1);
        return display_mode_label(m);
    }

    void DisplayPage::fill(Engine &e, Node &b)
    {
        kit::section(&b, "Output");
        mode_ = row("Resolution", "--", [this] { pick_mode(); }, "display",
                    "Every mode your TV or monitor reports (EDID)");
        render_ = row("Render resolution", "Native", [this] { pick_render(); }, "sliders",
                      "Render below output and upscale (faster on low-power boards)");
        kit::section(&b, "Panel");
        panel_ = b.add<Node>();
        panel_->style.dir = Dir::Column;
        kit::info_row(panel_, "Reading panel info\xE2\x80\xA6", "");
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
            std::string label;
            if (current_ == "native") label = "Native" + (report_.native.empty() ? "" : ": " + mode_label(report_.native));
            else if (current_ == "auto") label = "Automatic" + (report_.preferred.empty() ? "" : ": " + mode_label(report_.preferred));
            else label = mode_label(current_);
            mode_->set_value(label.empty() ? "--" : label);
            render_->set_value(render_cur_ == "native" ? "Native" : render_cur_);
        });
        app_.jobs().run(ctl("display", "modes"), [this](const JobResult &r) {
            report_ = parse_display_report(r.output);
            std::string sub;
            if (!report_.connector.empty()) {
                sub = "Connected: " + report_.connector;
                if (!report_.connector_type.empty()) sub += " (" + report_.connector_type + ")";
                if (!report_.panel.count("name")) {
                    if (!report_.native.empty()) sub += " \xC2\xB7 native " + report_.native;
                } else {
                    sub += " \xC2\xB7 " + report_.panel.at("name");
                }
            }
            if (mode_->sub) mode_->sub->set(sub.empty() ? "Every mode your TV or monitor reports (EDID)" : sub);
            fill_panel();
        });
    }

    void DisplayPage::fill_panel()
    {
        panel_->clear_children();
        const auto &p = report_.panel;
        auto add = [this](const char *k, const std::string &v) {
            if (!v.empty()) kit::info_row(panel_, k, v);
        };
        std::string name = p.count("name") ? p.at("name") : "";
        std::string make = p.count("make") ? p.at("make") : "";
        add("Display", name + (make.empty() || name.empty() ? "" : "  (" + make + ")"));
        add("Size", p.count("size") ? p.at("size") : "");
        add("Native resolution", mode_label(report_.native));
        add("Preferred mode", mode_label(report_.preferred));
        std::string hdr = p.count("hdr") ? p.at("hdr") : "";
        if (hdr == "none" || hdr.empty()) add("HDR", "Not reported");
        else add("HDR", hdr + (p.count("hdr_max") ? "  \xC2\xB7 peak " + p.at("hdr_max") : ""));
        add("Variable refresh", p.count("vrr") ? p.at("vrr") : "");
        add("Colour gamut", p.count("colorimetry") ? p.at("colorimetry") : "");
        add("Deep colour", p.count("deepcolor") ? p.at("deepcolor") : "");
        add("Audio", p.count("audio") ? p.at("audio") : "");
        std::string link;
        if (p.count("char_rate")) link = p.at("char_rate") + (p.count("scdc") ? "  (SCDC)" : "");
        else if (p.count("tmds")) link = p.at("tmds");
        add("Max link clock", link);
        invalidate_layout();
    }

    void DisplayPage::pick_mode()
    {
        const auto &rep = report_;
        if (rep.modes.empty()) { app_.toast("No display modes reported (is a TV connected?)", true); return; }
        std::vector<std::string> opts, vals;
        if (!rep.native.empty()) {
            opts.push_back("Native \xE2\x80\x94 the panel's own resolution (" + mode_label(rep.native) + ")");
            vals.push_back("native");
        }
        opts.push_back("Automatic \xE2\x80\x94 the monitor's preferred mode" +
                       (rep.preferred.empty() ? "" : " (" + mode_label(rep.preferred) + ")"));
        vals.push_back("auto");
        int sel = current_ == "native" ? 0 : -1;
        if (sel < 0 && current_ == "auto") sel = !rep.native.empty();
        for (auto &m : rep.modes) {
            std::string v = std::to_string(m.w) + "x" + std::to_string(m.h) + "@" + std::to_string(m.rate);
            std::string tag = m.native ? "  \xC2\xB7 native" : m.preferred ? "  \xC2\xB7 preferred" : "";
            opts.push_back(display_mode_label(m) + tag);
            vals.push_back(v);
            if (v == current_) sel = (int) vals.size() - 1;
        }
        app_.ui().push(std::make_unique<ChoiceSheet>(app_, "Resolution", opts, std::max(sel, 0), [this, vals](int i) {
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

    // ---- Stats -------------------------------------------------------------------
    template <typename... A>
    static std::string sfmt(const char *fmt, A... args)
    {
        char buf[128];
        int n = snprintf(buf, sizeof buf, fmt, args...);
        return std::string(buf, n < 0 ? 0 : std::min<size_t>((size_t) n, sizeof buf - 1));
    }

    void StatsPage::fill(Engine &e, Node &b)
    {
        (void) e;
        kit::section(&b, "Processor");
        kit::info_row(&b, "CPU", "Sampling\xE2\x80\xA6", &cpu_);
        cores_ = b.add<Node>();
        cores_->style.dir = Dir::Column;
        kit::info_row(&b, "Load (1/5/15 min)", "--", &load_);
        kit::info_row(&b, "Processes", "--", &procs_);
        kit::info_row(&b, "Uptime", "--", &uptime_);
        kit::section(&b, "Memory");
        kit::info_row(&b, "RAM", "--", &mem_);
        kit::info_row(&b, "Swap", "--", &swap_);
        kit::info_row(&b, "Storage", "--", &disk_);
        kit::section(&b, "Graphics");
        kit::info_row(&b, "GPU", "--", &gpu_);
        kit::section(&b, "Temperature");
        temps_ = b.add<Node>();
        temps_->style.dir = Dir::Column;
        kit::section(&b, "Network");
        net_ = b.add<Node>();
        net_->style.dir = Dir::Column;
    }

    void StatsPage::on_enter(Engine &e) { (void) e; poll_ = 0; refresh(); }

    bool StatsPage::tick(Engine &e, float dt)
    {
        if (e.top() != this) { poll_ = 0; return false; }   // only poll while on top
        poll_ += dt;
        if (poll_ >= 2.f) {
            poll_ = 0;
            if (!app_.jobs().busy()) refresh();
            else poll_ = 1.9f;   // wait for the queue; retry soon
            return true;
        }
        return false;
    }

    void StatsPage::refresh()
    {
        app_.jobs().run(ctl("sys", "stats"), [this](const JobResult &r) {
            if (!r.ok) return;
            show(parse_sys_stats(r.output));
        });
    }

    void StatsPage::show(const StatsSnapshot &s)
    {
        if (cpu_) cpu_->set(sfmt("%d%%", s.cpu_busy));
        cores_->clear_children();
        for (auto &c : s.cores)
            kit::info_row(cores_, "Core " + std::to_string(c.index + 1), sfmt("%d%%", c.busy));
        if (load_) load_->set(sfmt("%.2f / %.2f / %.2f", s.load1, s.load5, s.load15));
        if (procs_) procs_->set(std::to_string(s.procs) + "  (" + std::to_string(s.procs_running) + " running)");
        if (uptime_) uptime_->set(fmt_uptime(s.uptime_s));
        if (mem_) {
            long long used = std::max(s.mem_total_kb - s.mem_avail_kb, 0LL);
            mem_->set(fmt_kb(used) + " used of " + fmt_kb(s.mem_total_kb));
        }
        if (swap_) swap_->set(s.swap_total_kb > 0 ? fmt_kb(s.swap_used_kb) + " of " + fmt_kb(s.swap_total_kb)
                                                  : "none");
        if (disk_) disk_->set(fmt_kb(s.disk_used_kb) + " of " + fmt_kb(s.disk_total_kb));
        if (gpu_) {
            std::string g;
            if (!s.gpu_busy.empty()) g = s.gpu_busy + "%";
            if (!s.gpu_freq.empty()) g += (g.empty() ? "" : "  \xC2\xB7  ") + s.gpu_freq + " MHz";
            gpu_->set(g.empty() ? "n/a" : g);
        }
        temps_->clear_children();
        int ntemp = 0;
        for (auto &[name, mc] : s.temps) {
            kit::info_row(temps_, name, fmt_temp(mc));
            ntemp++;
        }
        if (!ntemp) kit::info_row(temps_, "Sensors", "none");
        net_->clear_children();
        int nnet = 0;
        for (auto &i : s.net) {
            kit::info_row(net_, i.name, fmt_bps(i.rx) + "  \xC2\xB7  " + fmt_bps(i.tx) + " up");
            nnet++;
        }
        if (!nnet) kit::info_row(net_, "Interfaces", "none");
        invalidate_layout();
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
