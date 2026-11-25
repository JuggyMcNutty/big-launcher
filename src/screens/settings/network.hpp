#pragma once

#include <string>
#include <vector>

#include "../../ui/list_screen.hpp"
#include "../../platform/sysctl.hpp"

namespace Foyer {
    // Network page: per-interface details (from `foyer-ctl net status` iface
    // lines), WiFi state, scan + connect.
    class NetworkSettings : public ListScreen {
    public:
        explicit NetworkSettings(App &app);
        void on_show() override;
    protected:
        void on_select(int index) override;
    private:
        struct Network { std::string ssid, signal; };

        void refresh_status();
        void scan();
        void rebuild();

        std::vector<NetIface> ifaces_;
        std::vector<Network> networks_;
        std::string wifi_state_ = "--";
        bool loaded_ = false;
        bool failed_ = false;
        int scan_index_ = -1;      // item index of "Scan Networks"
        int first_network_ = -1;   // item index of first scanned network
    };
}
