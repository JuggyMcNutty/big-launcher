#include "display.hpp"
#include "../../core/app.hpp"
#include "../../platform/sysctl.hpp"
#include "../../render/util.hpp"

Foyer::DisplaySettings::DisplaySettings(App &app): ListScreen(app)
{
    set_title("Display");
    set_footer("Left/Right: adjust  A: select  B: back");
    add_item("Resolution", current_mode_);
    add_item("UI Scale", std::to_string(app_.config().ui_scale) + "%");
    add_item("Overscan", std::to_string(app_.config().overscan) + "%");
}

void Foyer::DisplaySettings::on_show()
{
    refresh();
}

void Foyer::DisplaySettings::refresh()
{
    app_.jobs().run(ctl("display", "mode/get"), [this](const JobResult &res) {
        if (!res.ok) return;
        auto lines = split_lines(res.output);
        if (!lines.empty()) {
            current_mode_ = lines[0];
            set_value(item_resolution, current_mode_);
        }
        app_.mark_dirty();
    });
}

bool Foyer::DisplaySettings::on_action(Action action)
{
    if (action == Action::Left || action == Action::Right) {
        if (selected() == item_scale) {
            int delta = (action == Action::Right) ? 10 : -10;
            int v = app_.config().ui_scale + delta;
            if (v < 60) v = 60;
            if (v > 200) v = 200;
            app_.config().ui_scale = v;
            set_value(item_scale, std::to_string(v) + "%");
            app_.play_click();
            app_.mark_dirty();
            return true;
        }
        if (selected() == item_overscan) {
            int delta = (action == Action::Right) ? 2 : -2;
            int v = app_.config().overscan + delta;
            if (v < 0) v = 0;
            if (v > 20) v = 20;
            app_.config().overscan = v;
            set_value(item_overscan, std::to_string(v) + "%");
            app_.play_click();
            app_.mark_dirty();
            return true;
        }
    }
    return ListScreen::on_action(action);
}

void Foyer::DisplaySettings::on_select(int index)
{
    if (index == item_resolution) {
        // Cycle through common modes; foyer-ctl rewrites session.conf and
        // restarts the session.
        static const char *modes[] = {"1920x1080@60", "1280x720@60", "1920x1080@50", nullptr};
        int cur = 0;
        for (int i = 0; modes[i]; i++)
            if (current_mode_ == modes[i]) { cur = i; break; }
        int next = (cur + 1) % 4;
        std::string mode = modes[next];
        current_mode_ = mode;
        set_value(item_resolution, mode);
        app_.jobs().run(ctl("display", "mode/set", {mode}), nullptr);
    }
}
