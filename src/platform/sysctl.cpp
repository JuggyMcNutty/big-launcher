#include <sstream>
#include <cstring>

#include "sysctl.hpp"

namespace BL {
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
