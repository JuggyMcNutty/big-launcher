#pragma once

#include <string>
#include <vector>

namespace BL {
    // Builds htpc-ctl command strings. htpc-ctl is the single POSIX-sh system
    // control backend on the box (supersedes htpc-net). It is always invoked
    // through the job queue, never synchronously.
    std::string ctl(const std::string &group, const std::string &cmd,
                    const std::vector<std::string> &args = {});
}
