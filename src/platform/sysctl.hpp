#pragma once

#include <map>
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

    // ---- display -----------------------------------------------------------------
    // Parsed `foyer-ctl display modes` output (connector lines + EDID report).
    struct DisplayMode {
        int w = 0, h = 0, rate = 60;
        bool native = false, preferred = false;
    };
    struct DisplayReport {
        std::string connector, connector_state, connector_type;
        std::vector<DisplayMode> modes;   // sorted: area desc, then rate desc
        std::string native, preferred;   // "WxH@R"
        std::map<std::string, std::string> panel;   // name/make/model/year/...
    };
    DisplayReport parse_display_report(const std::string &output);
    std::string display_mode_label(const DisplayMode &m);   // "3840x2160 @ 60 Hz"

    // ---- stats -------------------------------------------------------------------
    // Parsed `foyer-ctl sys stats` snapshot (one self-contained sample).
    struct CpuCore { int index = -1, busy = 0; };
    struct NetStat { std::string name; double rx = 0, tx = 0; };   // bytes/s
    struct StatsSnapshot {
        int cpu_busy = 0;
        std::vector<CpuCore> cores;
        long long mem_total_kb = 0, mem_avail_kb = 0;
        long long swap_total_kb = 0, swap_used_kb = 0;
        double load1 = 0, load5 = 0, load15 = 0;
        int procs = 0, procs_running = 0;
        double uptime_s = 0;
        std::vector<std::pair<std::string, int>> temps;   // name, milli-degC
        std::string gpu_busy, gpu_freq;                  // "" = not reported
        std::vector<NetStat> net;
        long long disk_used_kb = 0, disk_total_kb = 0;
    };
    StatsSnapshot parse_sys_stats(const std::string &output);

    // formatting helpers (stats page; unit-tested)
    std::string fmt_kb(long long kb);                    // 1234567 -> "1.2 GB"
    std::string fmt_bps(double bytes_per_s);            // 1258291 -> "1.2 MB/s"
    std::string fmt_uptime(double seconds);             // 123456 -> "1 day 6:17"
    std::string fmt_temp(int milli_c);                  // 45300 -> "45C"
}