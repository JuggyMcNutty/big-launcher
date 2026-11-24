#include "power.hpp"
#include "../../core/app.hpp"
#include "../../platform/power.hpp"

BL::PowerSettings::PowerSettings(App &app): ListScreen(app)
{
    set_title("Power");
    set_footer("A: select  B: back");
    add_item("Reboot");
    add_item("Shutdown");
    add_item("Sleep");
    add_item("Sleep Timer", "off");
    add_item("Restart UI");
}

void BL::PowerSettings::on_select(int index)
{
    switch (index) {
        case item_reboot:
            app_.show_toast("Rebooting...");
            power::reboot();
            break;
        case item_shutdown:
            app_.show_toast("Shutting down...");
            power::shutdown();
            break;
        case item_sleep:
            app_.show_toast("Sleeping...");
            power::suspend();
            break;
        case item_timer: {
            static const int timers[] = {0, 15, 30, 60, 120};
            int idx = 0;
            for (int i = 0; i < 5; i++)
                if (sleep_timer_ == timers[i]) { idx = i; break; }
            idx = (idx + 1) % 5;
            sleep_timer_ = timers[idx];
            set_value(item_timer, sleep_timer_ ? (std::to_string(sleep_timer_) + " min") : "off");
            break;
        }
        case item_restart_ui:
            app_.restart_ui();
            break;
        default:
            break;
    }
}
