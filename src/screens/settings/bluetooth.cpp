#include "bluetooth.hpp"
#include "../../core/app.hpp"
#include "../../platform/sysctl.hpp"
#include "../../render/util.hpp"

Foyer::BluetoothSettings::BluetoothSettings(App &app): ListScreen(app)
{
    set_title("Bluetooth");
    set_footer("A: select  B: back");
    add_item("Power", "--");
    add_item("Scan");
}

void Foyer::BluetoothSettings::on_show()
{
    refresh();
}

void Foyer::BluetoothSettings::refresh()
{
    app_.jobs().run(ctl("bt", "status"), [this](const JobResult &res) {
        if (!res.ok) return;
        auto lines = split_lines(res.output);
        if (!lines.empty()) {
            powered_ = (lines[0] == "powered");
            set_value(item_power, powered_ ? "on" : "off");
        }
        app_.mark_dirty();
    });
    // List known devices
    app_.jobs().run(ctl("bt", "devices"), [this](const JobResult &res) {
        while (count() > 2)
            items_.pop_back();
        if (res.ok) {
            for (const auto &line : split_lines(res.output)) {
                auto tab = line.find('\t');
                if (tab == std::string::npos) continue;
                std::string mac = line.substr(0, tab);
                std::string name = line.substr(tab + 1);
                add_item(name.empty() ? mac : name, mac);
            }
        }
        app_.mark_dirty();
    });
}

void Foyer::BluetoothSettings::scan()
{
    app_.show_toast("Scanning for devices...");
    app_.jobs().run(ctl("bt", "scan"), [this](const JobResult &res) {
        while (count() > 2)
            items_.pop_back();
        if (res.ok) {
            for (const auto &line : split_lines(res.output)) {
                auto tab = line.find('\t');
                if (tab == std::string::npos) continue;
                std::string mac = line.substr(0, tab);
                std::string name = line.substr(tab + 1);
                add_item(name.empty() ? mac : name, mac);
            }
        }
        app_.mark_dirty();
    });
}

void Foyer::BluetoothSettings::on_select(int index)
{
    if (index == item_power) {
        app_.jobs().run(ctl("bt", "power", {powered_ ? "off" : "on"}), [this](const JobResult &r) {
            if (r.ok) {
                powered_ = !powered_;
                set_value(item_power, powered_ ? "on" : "off");
            }
        });
        return;
    }
    if (index == item_scan) {
        scan();
        return;
    }
    // Pair + connect a device
    std::string mac = item_value(index);
    app_.show_toast("Pairing " + mac + "...");
    app_.jobs().run(ctl("bt", "pair", {mac}), [this, mac](const JobResult &r) {
        if (r.ok)
            app_.show_toast("Paired " + mac);
        else
            app_.show_toast("Pair failed");
        refresh();
    });
}
