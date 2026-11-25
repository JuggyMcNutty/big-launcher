#include "quick_menu.hpp"
#include "../core/app.hpp"
#include "../platform/sysctl.hpp"
#include "../platform/power.hpp"
#include "../render/logger.hpp"
#include <cstdlib>
#include <signal.h>

Foyer::QuickMenu::QuickMenu(App &app): ListScreen(app)
{
    set_title("Quick Menu");
    set_footer("A: select  B: dismiss");
}

void Foyer::QuickMenu::on_show()
{
    clear_items();
    add_item("Back to home");
    add_item("Volume up");
    add_item("Volume down");
    add_item("Mute");
    add_item("Reboot");
    add_item("Shutdown");
    add_item("Sleep");
}

void Foyer::QuickMenu::on_select(int index)
{
    switch (index) {
        case act_home: {
            // Kill the running app; the main launcher detects the exit.
            const char *pid = getenv("HTPC_APP_PID");
            if (pid) {
                int p = std::atoi(pid);
                if (p > 0) {
                    kill(p, SIGTERM);
                    Foyer::logger::info("Quick menu: killed app pid {}", p);
                }
            }
            app_.restart_ui(); // exit the overlay
            return;
        }
        case act_volup:    app_.jobs().run(ctl("audio", "volume/up"), nullptr);    return;
        case act_voldown:  app_.jobs().run(ctl("audio", "volume/down"), nullptr);  return;
        case act_mute:     app_.jobs().run(ctl("audio", "mute/toggle"), nullptr);  return;
        case act_reboot:   power::reboot();   return;
        case act_shutdown: power::shutdown(); return;
        case act_sleep:    power::suspend();  return;
        default: return;
    }
}
