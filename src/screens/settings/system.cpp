#include "system.hpp"
#include "../../core/app.hpp"
#include "../../platform/sysctl.hpp"
#include "../../render/util.hpp"

BL::SystemSettings::SystemSettings(App &app): ListScreen(app)
{
    set_title("System");
    set_footer("A: select  B: back");
    add_item("Hostname", "--");
    add_item("Timezone", "--");
    add_item("Time Sync", "on");
    add_item("Check for Updates");
    add_item("Storage", "--");
    add_item("Memory", "--");
    add_item("CPU Temp", "--");
    add_item("Uptime", "--");
    add_item("About", "big-launcher v2");
}

void BL::SystemSettings::on_show()
{
    refresh_info();
}

void BL::SystemSettings::refresh_info()
{
    app_.jobs().run(ctl("sys", "info"), [this](const JobResult &res) {
        if (!res.ok) return;
        for (const auto &line : split_lines(res.output)) {
            auto eq = line.find('=');
            if (eq == std::string::npos) continue;
            std::string k = line.substr(0, eq);
            std::string v = line.substr(eq + 1);
            if (k == "hostname") set_value(item_hostname, v);
            else if (k == "timezone") set_value(item_timezone, v);
            else if (k == "storage") set_value(item_storage, v);
            else if (k == "ram") set_value(item_ram, v);
            else if (k == "temp") set_value(item_temp, v);
            else if (k == "uptime") set_value(item_uptime, v);
        }
        app_.mark_dirty();
    });
}

void BL::SystemSettings::on_select(int index)
{
    switch (index) {
        case item_hostname:
            app_.show_toast("Hostname editing needs the on-screen keyboard (Phase 3)");
            break;
        case item_timezone:
            app_.jobs().run(ctl("sys", "timezone/cycle"), [this](const JobResult &r) {
                if (r.ok) {
                    auto lines = split_lines(r.output);
                    if (!lines.empty())
                        set_value(item_timezone, lines[0]);
                }
            });
            break;
        case item_sync:
            app_.jobs().run(ctl("sys", "ntp/sync"), [this](const JobResult &r) {
                app_.show_toast(r.ok ? "Time synced" : "NTP sync failed");
            });
            break;
        case item_updates:
            app_.show_toast("Checking for updates...");
            app_.jobs().run_stream(ctl("sys", "upgrade"),
                [this](const std::string &line) {
                    set_value(item_updates, line);
                    app_.mark_dirty();
                },
                [this](const JobResult &r) {
                    app_.show_toast(r.ok ? "System up to date" : "Upgrade finished");
                    set_text(item_updates, "Check for Updates");
                    refresh_info();
                });
            break;
        default:
            break;
    }
}
