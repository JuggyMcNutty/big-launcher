#include <csignal>
#include <cstdlib>

#include "quick_panel.hpp"
#include "settings.hpp"
#include "../core/app.hpp"
#include "../platform/sysctl.hpp"
#include "../render/util.hpp"

namespace Foyer {
    using namespace ui;

    QuickPanel::QuickPanel(App &app, bool external): app_(app), external_(external)
    {
        opaque = false;
        dim_below = !external;
    }

    void QuickPanel::close_panel()
    {
        if (external_) app_.quit();
        else app_.ui().pop();
    }

    void QuickPanel::build(Engine &e)
    {
        const Tokens &t = e.tokens();
        root->style.dir = Dir::Row;
        root->style.justify = Justify::End;
        auto *panel = root->add<Box>(Dir::Column);
        panel->has_bg = true;
        panel->bg = with_alpha(t.surface, 0xF2);
        panel->radius = t.radius_lg;
        panel->shadow_blur = 50;
        panel->shadow_color = t.shadow;
        panel->style.w = Dim::dp(560);
        panel->style.max_w = e.metrics().safe_width_dp() * 0.9f;
        panel->style.h = Dim::fill();
        panel->style.pad = Edges::all(28);
        panel->style.gap = 10;
        panel->tx.snap(120.f);
        panel->tx.to(0.f);

        std::string running;
        if (external_) { if (const char *n = getenv("FOYER_APP_TITLE")) running = n; }
        else if (app_.app_running()) running = app_.running_title();
        auto *head = panel->add<Node>();
        head->style.dir = Dir::Column;
        head->style.pad = Edges{8, 4, 8, 14};
        head->style.gap = 4;
        auto *k = head->add<Text>(running.empty() ? "Quick menu" : "Now running", TextRole::Sm);
        k->dim = true;
        auto *n = head->add<Text>(running.empty() ? "Foyer" : running, TextRole::Xl);
        n->display_font = true;

        auto *list = kit::list(panel, 10000);
        auto add = [&](const std::string &label, const std::string &icon, std::function<void()> f, const std::string &val = "") {
            auto *b = list->add<kit::RowButton>(app_, label, val, icon);
            b->on_activate = std::move(f);
            return b;
        };
        if (!running.empty()) {
            add("Back to Foyer", "home", [this] {
                // Close the running app; the launcher notices and comes back.
                if (external_) {
                    if (const char *p = getenv("FOYER_APP_PID")) { int pid = atoi(p); if (pid > 0) { kill(-pid, SIGTERM); kill(pid, SIGTERM); } }
                    app_.quit();
                } else { app_.close_app(); close_panel(); }
            });
            add("Resume", "play", [this] { close_panel(); });
        }
        vol_ = add("Volume", "volume", nullptr, "--");
        vol_->on_adjust = [this](Action a) {
            app_.jobs().run(ctl("audio", a == Action::Right ? "volume/up" : "volume/down"), [this](const JobResult&) { refresh_volume(); });
            app_.play_click();
            return true;
        };
        vol_->on_activate = [this] {
            app_.jobs().run(ctl("audio", "mute/toggle"), [this](const JobResult&) { refresh_volume(); });
        };
        if (!external_) {
            add("Settings", "sliders", [this] { app_.ui().pop(false); app_.open_settings(); });
            add("Display", "display", [this] { app_.ui().pop(false); app_.open_settings("display"); });
        }
        add("Sleep", "moon", [this] { app_.execute(":sleep"); close_panel(); });
        for (auto &[title, cmd] : app_.layout().power) {
            if (cmd == ":sleep") continue;
            std::string c = cmd, ti = title;
            std::string icon = c == ":shutdown" ? "power" : c == ":quit" ? "home" : "restart";
            add(title, icon, [this, c, ti] {
                if (c == ":quit") { app_.quit(); return; }
                app_.ui().push(std::make_unique<ConfirmDialog>(app_, ti + "?", "", ti, "Cancel",
                    [this, c](bool yes) { if (yes) app_.execute(c); }));
            });
        }
        kit::hints(panel, {{"\xE2\x97\x80\xE2\x96\xB6", "Volume"}, {"B", "Close"}});
    }

    void QuickPanel::paint_under(Engine &e, Canvas &c)
    {
        if (!external_) return;
        // Over a running app: transparent window, dim the app a little.
        float W = (float) e.metrics().width(), H = (float) e.metrics().height();
        c.clear({0, 0, 0, 0});
        c.gradient({W * 0.4f, 0, W * 0.6f, H}, {0, 0, 0, 0}, {0, 0, 0, 0xA0}, false);
    }

    bool QuickPanel::on_action(Engine &e, Action a)
    {
        (void) e;
        if (a == Action::Back || a == Action::Menu || (a == Action::Home && external_)) { close_panel(); return true; }
        return false;
    }

    void QuickPanel::on_enter(Engine &e) { (void) e; refresh_volume(); }

    void QuickPanel::refresh_volume()
    {
        app_.jobs().run(ctl("audio", "volume/get"), [this](const JobResult &r) {
            int v = -1; bool m = false;
            for (auto &l : split_lines(r.output)) {
                if (l.starts_with("volume=")) v = atoi(l.c_str() + 7);
                if (l.starts_with("muted=")) m = l.substr(6) == "1";
            }
            if (vol_) vol_->set_value(m ? "Muted" : v >= 0 ? std::to_string(v) + "%" : "--");
            app_.refresh_status();
        });
    }
}
