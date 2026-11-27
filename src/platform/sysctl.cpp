#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <sstream>
#include <cstring>

#include "sysctl.hpp"
#include "../render/util.hpp"

namespace Foyer {
    static std::string shell_quote(const std::string &s)
    {
        if (s.empty()) return "''";
        bool needs = false;
        for (char c : s)
            if (!isalnum((unsigned char) c) && strchr("._/-", c) == nullptr) { needs = true; break; }
        if (!needs) return s;
        std::string out = "'";
        for (char c : s) {
            if (c == '\'') out += "'\\''";
            else out += c;
        }
        out += "'";
        return out;
    }

    std::string ctl(const std::string &group, const std::string &cmd,
                    const std::vector<std::string> &args)
    {
        std::ostringstream oss;
        oss << "foyer-ctl " << group << " " << cmd;
        for (const auto &a : args)
            oss << " " << shell_quote(a);
        return oss.str();
    }

    // TAB-split a line into fields (an empty line gives one empty field).
    static std::vector<std::string> tabs(const std::string &line)
    {
        std::vector<std::string> f;
        size_t s = 0;
        while (true) {
            size_t t = line.find('\t', s);
            f.push_back(line.substr(s, t == std::string::npos ? std::string::npos : t - s));
            if (t == std::string::npos) break;
            s = t + 1;
        }
        return f;
    }

    static std::string kv(const std::string &out, const std::string &key)
    {
        for (auto &l : split_lines(out))
            if (l.starts_with(key + "=")) return l.substr(key.size() + 1);
        return "";
    }

    NetStatus parse_net_status(const std::string &output)
    {
        NetStatus st;
        std::istringstream in(output);
        std::string line;
        while (std::getline(in, line)) {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            if (line.starts_with("iface\t")) {
                auto f = tabs(line);
                if (f.size() < 9) continue;
                st.ifaces.push_back({f[1], f[2], f[3], f[4], f[5], f[6], f[7], f[8]});
            }
            else if (line.starts_with("eth=")) st.eth = line.substr(4);
            else if (line.starts_with("wifi=")) st.wifi = line.substr(5);
        }
        return st;
    }

    // ---- display ----------------------------------------------------------------

    static DisplayMode parse_mode_line(const std::string &line)
    {
        DisplayMode m;
        auto f = tabs(line);
        // mode\tWxH\trate\t[flags...]
        if (f.size() < 3 || f[0] != "mode") return m;
        auto x = f[1].find('x');
        if (x == std::string::npos) return m;
        m.w = atoi(f[1].c_str());
        m.h = atoi(f[1].c_str() + x + 1);
        m.rate = std::max(atoi(f[2].c_str()), 1);
        for (size_t i = 3; i < f.size(); i++) {
            if (f[i] == "native") m.native = true;
            if (f[i] == "preferred") m.preferred = true;
        }
        return m;
    }

    DisplayReport parse_display_report(const std::string &output)
    {
        DisplayReport r;
        std::istringstream in(output);
        std::string line;
        while (std::getline(in, line)) {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            if (line.starts_with("connector\t")) {
                auto f = tabs(line);
                if (f.size() < 3) continue;
                // the connected connector carries the report (it is emitted last)
                if (f[2] == "connected" || r.connector.empty()) {
                    r.connector = f[1];
                    r.connector_state = f[2];
                    r.connector_type = f.size() > 3 ? f[3] : "";
                }
            } else if (line.starts_with("mode\t")) {
                DisplayMode m = parse_mode_line(line);
                if (m.w > 0 && m.h > 0) r.modes.push_back(m);
            } else {
                auto eq = line.find('=');
                if (eq != std::string::npos && !line.substr(0, eq).empty())
                    r.panel[line.substr(0, eq)] = line.substr(eq + 1);
            }
        }
        r.native = kv(output, "native");
        r.preferred = kv(output, "preferred");
        std::stable_sort(r.modes.begin(), r.modes.end(), [](const DisplayMode &a, const DisplayMode &b) {
            if (a.w * a.h != b.w * b.h) return a.w * a.h > b.w * b.h;
            return a.rate > b.rate;
        });
        return r;
    }

    std::string display_mode_label(const DisplayMode &m)
    {
        return std::to_string(m.w) + "x" + std::to_string(m.h) + " \xC3\x97 " + std::to_string(m.rate) + " Hz";
    }

    // ---- stats ------------------------------------------------------------------

    StatsSnapshot parse_sys_stats(const std::string &output)
    {
        StatsSnapshot s;
        std::istringstream in(output);
        std::string line;
        while (std::getline(in, line)) {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            auto f = tabs(line);
            if (f.empty()) continue;
            const std::string &k = f[0];
            if (k == "cpu" && f.size() > 1) s.cpu_busy = atoi(f[1].c_str());
            else if (k == "core" && f.size() > 2) {
                int busy = atoi(f[2].c_str());
                int idx = atoi(f[1].c_str());
                s.cores.push_back({idx, busy});
            } else if (k == "mem" && f.size() > 2) {
                s.mem_total_kb = atoll(f[1].c_str());
                s.mem_avail_kb = atoll(f[2].c_str());
            } else if (k == "swap" && f.size() > 2) {
                s.swap_total_kb = atoll(f[1].c_str());
                s.swap_used_kb = atoll(f[2].c_str());
            } else if (k == "load" && f.size() > 3) {
                s.load1 = atof(f[1].c_str());
                s.load5 = atof(f[2].c_str());
                s.load15 = atof(f[3].c_str());
            } else if (k == "proc" && f.size() > 2) {
                s.procs = atoi(f[1].c_str());
                s.procs_running = atoi(f[2].c_str());
            } else if (k == "up" && f.size() > 1) s.uptime_s = atof(f[1].c_str());
            else if (k == "temp" && f.size() > 2) s.temps.push_back({f[1], atoi(f[2].c_str())});
            else if (k == "gpu" && f.size() > 2) {
                if (f[1] != "-" && !f[1].empty()) s.gpu_busy = f[1];
                if (f[2] != "-" && !f[2].empty()) s.gpu_freq = f[2];
                if (f.size() > 4) {   // old 3-field form: min/max stay 0
                    if (f[3] != "-" && !f[3].empty()) s.gpu_freq_min = atoi(f[3].c_str());
                    if (f[4] != "-" && !f[4].empty()) s.gpu_freq_max = atoi(f[4].c_str());
                }
            } else if (k == "net" && f.size() > 3) s.net.push_back({f[1], atof(f[2].c_str()), atof(f[3].c_str())});
            else if (k == "dio" && f.size() > 2) {
                s.disk_read_kbs = atof(f[1].c_str());
                s.disk_write_kbs = atof(f[2].c_str());
            } else if (k == "disk") {
                if (f.size() > 3) s.mounts.push_back({f[1], atoll(f[2].c_str()), atoll(f[3].c_str())});
                else if (f.size() > 2) s.mounts.push_back({"/", atoll(f[1].c_str()), atoll(f[2].c_str())});
            }
        }
        return s;
    }

    std::vector<ProcStat> parse_sys_procs(const std::string &output)
    {
        std::vector<ProcStat> out;
        std::istringstream in(output);
        std::string line;
        while (std::getline(in, line)) {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            auto f = tabs(line);
            if (f.size() < 5 || f[0] != "proc") continue;
            ProcStat p;
            p.pid = atoi(f[1].c_str());
            p.cpu = atof(f[2].c_str());
            p.mem_kb = atoll(f[3].c_str());
            p.name = f[4];
            out.push_back(p);
        }
        return out;
    }

    std::string fmt_kb(long long kb)
    {
        char buf[64];
        double v = (double) kb;
        if (kb >= 1024LL * 1024 * 1024 * 1024) snprintf(buf, sizeof buf, "%.1f TB", v / 1024 / 1024 / 1024 / 1024);
        else if (kb >= 1024LL * 1024 * 1024) snprintf(buf, sizeof buf, "%.1f GB", v / 1024 / 1024 / 1024);
        else if (kb >= 1024LL * 1024) snprintf(buf, sizeof buf, "%.1f MB", v / 1024 / 1024);
        else if (kb >= 1024) snprintf(buf, sizeof buf, "%lld kB", kb / 1024);
        else snprintf(buf, sizeof buf, "%lld kB", kb);
        return buf;
    }

    std::string fmt_bps(double b)
    {
        char buf[64];
        if (b >= 1024 * 1024) snprintf(buf, sizeof buf, "%.1f MB/s", b / 1024 / 1024);
        else if (b >= 1024) snprintf(buf, sizeof buf, "%.0f kB/s", b / 1024);
        else snprintf(buf, sizeof buf, "%.0f B/s", std::max(b, 0.0));
        return buf;
    }

    std::string fmt_uptime(double secs)
    {
        char buf[80];
        long long s = (long long) secs;
        long long d = s / 86400, h = (s % 86400) / 3600, m = (s % 3600) / 60;
        if (d > 0) snprintf(buf, sizeof buf, "%lld day%s, %lld:%02lld", d, d == 1 ? "" : "s", h, m);
        else if (h > 0) snprintf(buf, sizeof buf, "%lld:%02lld", h, m);
        else if (m > 0) snprintf(buf, sizeof buf, "%lld min", m);
        else snprintf(buf, sizeof buf, "%lld s", s);
        return buf;
    }

    std::string fmt_temp(int milli_c)
    {
        int c = (milli_c + 500) / 1000;
        return std::to_string(c) + "\xC2\xB0" "C";
    }
}