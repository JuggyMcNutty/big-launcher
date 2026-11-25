#include <sstream>
#include <cstring>

#include "sysctl.hpp"

namespace Foyer {
    static std::string shell_quote(const std::string &s)
    {
        if (s.empty()) return "''";
        bool needs = false;
        for (char c : s)
            if (!isalnum((unsigned char)c) && strchr("._/-", c) == nullptr) { needs = true; break; }
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
        oss << "htpc-ctl " << group << " " << cmd;
        for (const auto &a : args)
            oss << " " << shell_quote(a);
        return oss.str();
    }
}

Foyer::NetStatus Foyer::parse_net_status(const std::string &output)
{
    NetStatus st;
    std::istringstream in(output);
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.starts_with("iface\t")) {
            std::vector<std::string> f;
            size_t start = 0;
            while (true) {
                size_t tab = line.find('\t', start);
                f.push_back(line.substr(start, tab == std::string::npos ? std::string::npos : tab - start));
                if (tab == std::string::npos) break;
                start = tab + 1;
            }
            if (f.size() < 9) continue;
            st.ifaces.push_back({f[1], f[2], f[3], f[4], f[5], f[6], f[7], f[8]});
        }
        else if (line.starts_with("eth=")) st.eth = line.substr(4);
        else if (line.starts_with("wifi=")) st.wifi = line.substr(5);
    }
    return st;
}
