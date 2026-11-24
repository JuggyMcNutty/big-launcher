#include "network.hpp"
#include "../../core/app.hpp"
#include "../../platform/sysctl.hpp"
#include "../../render/util.hpp"

BL::NetworkSettings::NetworkSettings(App &app): ListScreen(app)
{
    set_title("Network");
    set_footer("A: select  B: back");
    add_item("Ethernet", "--");
    add_item("WiFi", "--");
    add_item("Scan Networks");
}

void BL::NetworkSettings::on_show()
{
    refresh_status();
}

void BL::NetworkSettings::refresh_status()
{
    app_.jobs().run(ctl("net", "status"), [this](const JobResult &res) {
        if (!res.ok) return;
        for (const auto &line : split_lines(res.output)) {
            if (line.starts_with("eth="))
                set_value(item_eth, line.substr(4));
            else if (line.starts_with("wifi="))
                set_value(item_wifi, line.substr(5));
        }
        app_.mark_dirty();
    });
}

void BL::NetworkSettings::scan()
{
    app_.show_toast("Scanning...");
    app_.jobs().run(ctl("wifi", "scan"), [this](const JobResult &res) {
        if (!res.ok) return;
        // Remove old network items (keep first 3)
        while (count() > 3) {
            items_.pop_back();
        }
        int added = 0;
        for (const auto &line : split_lines(res.output)) {
            auto tab = line.find('\t');
            if (tab == std::string::npos) continue;
            std::string ssid = line.substr(0, tab);
            std::string signal = line.substr(tab + 1);
            add_item(ssid, signal + " dBm");
            added++;
        }
        if (!added)
            app_.show_toast("No networks found");
        app_.mark_dirty();
    });
}

void BL::NetworkSettings::on_select(int index)
{
    if (index == item_scan) {
        scan();
        return;
    }
    if (index >= 3) {
        // Connect to the selected network (open or remembered)
        std::string ssid = item_text(index);
        app_.show_toast("Connecting to " + ssid + "...");
        app_.jobs().run(ctl("wifi", "connect", {ssid, ""}), [this, ssid](const JobResult &res) {
            if (res.ok)
                app_.show_toast("Connected to " + ssid);
            else
                app_.show_toast("Connect failed (password needed?)");
            refresh_status();
        });
    }
}
