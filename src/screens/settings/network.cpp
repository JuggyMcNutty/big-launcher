#include "network.hpp"
#include "../../core/app.hpp"
#include "../../platform/sysctl.hpp"
#include "../../render/util.hpp"
#include "../../ui/text_input.hpp"

namespace BL {
    static std::string dash(const std::string &s) { return s.empty() || s == "-" ? "--" : s; }
}

BL::NetworkSettings::NetworkSettings(App &app): ListScreen(app)
{
    set_title("Network");
    set_footer("A: select  B: back");
    rebuild();
}

void BL::NetworkSettings::on_show()
{
    refresh_status();
}

void BL::NetworkSettings::rebuild()
{
    int prev = selected();
    clear_items();

    auto info = [this](const std::string &k, const std::string &v) {
        add_item(k, dash(v));
        set_enabled((int) count() - 1, false);  // info row: not focusable
    };

    if (!loaded_) {
        info(failed_ ? "Could not read network status" : "Loading...", "");
    }
    else if (ifaces_.empty()) {
        info("No network interfaces found", "");
    }
    for (const auto &i : ifaces_) {
        std::string kind = i.type == "wifi" ? "WiFi" : "Ethernet";
        info(kind + " (" + i.name + ")", i.state == "up" ? "Connected" : i.state);
        info("    IP address", i.ip);
        if (i.type != "wifi") info("    Link speed", i.speed);
        info("    Gateway", i.gateway);
        info("    DNS", i.dns);
        info("    MAC", i.mac);
    }

    bool have_wifi_iface = false;
    for (const auto &i : ifaces_) if (i.type == "wifi") have_wifi_iface = true;
    if (!have_wifi_iface)
        info("WiFi", wifi_state_);

    add_item("Scan WiFi Networks");
    scan_index_ = (int) count() - 1;
    first_network_ = (int) count();
    for (const auto &n : networks_)
        add_item(n.ssid, n.signal + " dBm");

    // Keep focus on an actionable item
    if (prev >= scan_index_ && prev < (int) count()) set_selected(prev);
    else set_selected(scan_index_);
}

void BL::NetworkSettings::refresh_status()
{
    app_.jobs().run(ctl("net", "status"), [this](const JobResult &res) {
        // Parse whatever output we got even on a non-zero exit (partial info
        // is better than none).
        NetStatus st = parse_net_status(res.output);
        ifaces_ = std::move(st.ifaces);
        if (!st.wifi.empty()) wifi_state_ = st.wifi;
        loaded_ = res.ok || !ifaces_.empty();
        failed_ = !loaded_;
        if (failed_)
            app_.show_toast("Network status failed (exit " + std::to_string(res.exit_code) + ")");
        rebuild();
        app_.mark_dirty();
    });
}

void BL::NetworkSettings::scan()
{
    app_.show_toast("Scanning...");
    app_.jobs().run(ctl("wifi", "scan"), [this](const JobResult &res) {
        networks_.clear();
        for (const auto &line : split_lines(res.output)) {
            auto tab = line.find('\t');
            if (tab == std::string::npos) continue;
            networks_.push_back({line.substr(0, tab), line.substr(tab + 1)});
        }
        if (networks_.empty())
            app_.show_toast(wifi_state_ == "no adapter" ? "No WiFi adapter" : "No networks found");
        rebuild();
        app_.mark_dirty();
    });
}

void BL::NetworkSettings::on_select(int index)
{
    if (index == scan_index_) {
        scan();
        return;
    }
    int ni = index - first_network_;
    if (ni >= 0 && ni < (int) networks_.size()) {
        // Connect to the selected network: ask for the password via the OSK
        std::string ssid = networks_[ni].ssid;
        app_.screens().push(std::make_unique<TextInput>(
            app_, "Password for \"" + ssid + "\"",
            [this, ssid](std::string pass) {
                app_.show_toast("Connecting to " + ssid + "...");
                app_.jobs().run(ctl("wifi", "connect", {ssid, std::move(pass)}),
                    [this, ssid](const JobResult &res) {
                        if (res.ok)
                            app_.show_toast("Connected to " + ssid);
                        else
                            app_.show_toast("Connect failed (check password)");
                        refresh_status();
                    });
            },
            [this]() { /* cancelled */ }));
    }
}
