#include <cstdio>
#include <cstdlib>
#include <algorithm>

#include "network.hpp"
#include "logger.hpp"

// The htpc-net helper script (installed to /usr/local/bin on the box)
static const char *NET_SCRIPT = "htpc-net";

std::string BL::net::shell_escape(const std::string &s)
{
    std::string out = "'";
    for (const char c : s) {
        if (c == '\'')
            out += "'\\''";
        else
            out += c;
    }
    return out + "'";
}

std::string BL::net::run_command(const std::string &command)
{
    FILE *pipe = popen(command.c_str(), "r");
    if (!pipe) {
        BL::logger::error("Could not run command '{}'", command);
        return {};
    }
    std::string output;
    char buffer[512];
    while (fgets(buffer, sizeof(buffer), pipe))
        output += buffer;
    pclose(pipe);

    // Trim trailing whitespace/newlines
    while (!output.empty() &&
          (output.back() == '\n' || output.back() == '\r' ||
           output.back() == ' ' || output.back() == '\t'))
        output.pop_back();
    return output;
}

static std::vector<std::string> split_lines(const std::string &output)
{
    std::vector<std::string> lines;
    if (output.empty())
        return lines;
    size_t start = 0;
    while (true) {
        size_t end = output.find('\n', start);
        std::string line = output.substr(start,
            (end == std::string::npos) ? std::string::npos : end - start);
        // Trim CR and surrounding whitespace
        while (!line.empty() && (line.back() == '\r' || line.back() == ' ' || line.back() == '\t'))
            line.pop_back();
        while (!line.empty() && (line.front() == ' ' || line.front() == '\t'))
            line.erase(0, 1);
        if (!line.empty())
            lines.push_back(line);
        if (end == std::string::npos)
            break;
        start = end + 1;
    }
    return lines;
}

static std::vector<std::string> split_fields(const std::string &line, char delimiter)
{
    std::vector<std::string> fields;
    size_t start = 0;
    while (true) {
        size_t end = line.find(delimiter, start);
        fields.push_back(line.substr(start,
            (end == std::string::npos) ? std::string::npos : end - start));
        if (end == std::string::npos)
            break;
        start = end + 1;
    }
    return fields;
}

// ---------------------------------------------------------------- WiFi

std::string BL::net::wifi_status_value(const std::string &key)
{
    const std::string output = run_command(std::string(NET_SCRIPT) + " wifi-status");
    for (const std::string &line : split_lines(output)) {
        const size_t eq = line.find('=');
        if (eq != std::string::npos && line.substr(0, eq) == key) {
            std::string value = line.substr(eq + 1);
            if (!value.empty() && value.front() == '"' && value.back() == '"')
                value = value.substr(1, value.size() - 2);
            return value;
        }
    }
    return {};
}

std::vector<BL::net::WifiNetwork> BL::net::wifi_scan()
{
    std::vector<WifiNetwork> networks;
    const std::string output = run_command(std::string(NET_SCRIPT) + " wifi-scan");
    for (const std::string &line : split_lines(output)) {
        const std::vector<std::string> fields = split_fields(line, '\t');
        if (fields.size() < 2)
            continue;
        WifiNetwork network;
        network.ssid = fields[0];
        network.signal = fields[1];
        network.flags = (fields.size() > 2) ? fields[2] : "";
        if (network.ssid.empty())
            continue;
        // Deduplicate by SSID (keep strongest, script sorts descending)
        bool dup = false;
        for (const WifiNetwork &existing : networks) {
            if (existing.ssid == network.ssid) {
                dup = true;
                break;
            }
        }
        if (!dup)
            networks.push_back(network);
    }
    return networks;
}

bool BL::net::wifi_connect(const std::string &ssid, const std::string &psk)
{
    const std::string command = std::string(NET_SCRIPT) + " wifi-connect " +
        shell_escape(ssid) + " " + shell_escape(psk);
    return run_command(command).find("OK") == 0;
}

// ---------------------------------------------------------------- Bluetooth

std::string BL::net::bt_power_state()
{
    return run_command(std::string(NET_SCRIPT) + " bt-status");
}

std::vector<BL::net::BtDevice> BL::net::bt_devices()
{
    std::vector<BtDevice> devices;
    const std::string output = run_command(std::string(NET_SCRIPT) + " bt-devices");
    for (const std::string &line : split_lines(output)) {
        const std::vector<std::string> fields = split_fields(line, '\t');
        if (fields.size() != 2)
            continue;
        devices.push_back({fields[0], fields[1]});
    }
    return devices;
}

std::vector<BL::net::BtDevice> BL::net::bt_scan()
{
    run_command(std::string(NET_SCRIPT) + " bt-scan");
    return bt_devices();
}

bool BL::net::bt_pair(const std::string &mac)
{
    return run_command(std::string(NET_SCRIPT) + " bt-pair " + shell_escape(mac)).find("OK") == 0;
}