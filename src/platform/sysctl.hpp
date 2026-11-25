#pragma once

#include <string>
#include <vector>

namespace Foyer {
    // Builds foyer-ctl command strings. foyer-ctl is the single POSIX-sh system
    // control backend on the box (supersedes htpc-net). It is always invoked
    // through the job queue, never synchronously.
    std::string ctl(const std::string &group, const std::string &cmd,
                    const std::vector<std::string> &args = {});
}

namespace Foyer {
    // Parsed `foyer-ctl net status` output.
    struct NetIface {
        std::string name, type, state, ip, mac, gateway, dns, speed;
    };
    struct NetStatus {
        std::vector<NetIface> ifaces;
        std::string eth;    // summary: ip or "no link"
        std::string wifi;   // summary: ip, state or "no adapter"
    };
    NetStatus parse_net_status(const std::string &output);
}
