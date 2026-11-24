#pragma once

#include <string>
#include <vector>

namespace BL::net {

    struct WifiNetwork {
        std::string ssid;
        std::string signal;
        std::string flags;
    };

    struct BtDevice {
        std::string mac;
        std::string name;
    };

    // Run a shell command, return combined stdout (trailing whitespace trimmed)
    std::string run_command(const std::string &command);

    // Shell-escape a string for embedding in a popen() command line
    std::string shell_escape(const std::string &s);

    // WiFi (via the htpc-net script: wpa_supplicant/wpa_cli)
    std::string wifi_status_value(const std::string &key);   // "" if no adapter
    std::vector<WifiNetwork> wifi_scan();                    // strongest first, deduped
    bool wifi_connect(const std::string &ssid, const std::string &psk);

    // Bluetooth (via the htpc-net script: bluez/bluetoothctl)
    std::string bt_power_state();                            // "powered"/"off"/"none"
    std::vector<BtDevice> bt_devices();                      // known/paired devices
    std::vector<BtDevice> bt_scan();                        // scan, then list devices
    bool bt_pair(const std::string &mac);

}