#include "launcher.hpp"
#include "../../core/app.hpp"

Foyer::LauncherSettings::LauncherSettings(App &app): ListScreen(app)
{
    set_title("Launcher Preferences");
    set_footer("A: toggle  B: back");
    add_item("Screensaver", app_.config().screensaver_enabled ? "on" : "off");
    add_item("Clock", app_.config().clock_enabled ? "on" : "off");
    add_item("Clock Format", app_.config().clock_format);
    add_item("Sounds", app_.config().sound_enabled ? "on" : "off");
}

void Foyer::LauncherSettings::on_hide()
{
    // Persist changes
    app_.save_config();
}

void Foyer::LauncherSettings::on_select(int index)
{
    switch (index) {
        case item_screensaver:
            app_.config().screensaver_enabled = !app_.config().screensaver_enabled;
            set_value(item_screensaver, app_.config().screensaver_enabled ? "on" : "off");
            break;
        case item_clock:
            app_.config().clock_enabled = !app_.config().clock_enabled;
            set_value(item_clock, app_.config().clock_enabled ? "on" : "off");
            break;
        case item_clockfmt: {
            static const char *formats[] = {"%H:%M", "%H:%M:%S", "%I:%M %p", "%A %d %B", nullptr};
            int cur = 0;
            for (int i = 0; formats[i]; i++)
                if (app_.config().clock_format == formats[i]) { cur = i; break; }
            cur = (cur + 1) % 4;
            app_.config().clock_format = formats[cur];
            set_value(item_clockfmt, app_.config().clock_format);
            break;
        }
        case item_sounds:
            app_.config().sound_enabled = !app_.config().sound_enabled;
            set_value(item_sounds, app_.config().sound_enabled ? "on" : "off");
            break;
        default:
            break;
    }
}
