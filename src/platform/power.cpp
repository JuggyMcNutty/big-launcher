#include "power.hpp"
#include "process.hpp"
#include "../render/logger.hpp"

namespace Foyer::power {
    // Fire-and-forget: these spawn a helper that acts on the whole system.
    static bool run(const std::string &cmd)
    {
        Foyer::logger::debug("Power: {}", cmd);
        auto p = Process::spawn(cmd);
        (void) p;  // intentionally not waited on
        return p.alive() || true;
    }

    bool shutdown() { return run("poweroff"); }
    bool reboot()   { return run("reboot"); }
    bool suspend()  { return run("echo mem > /sys/power/state"); }
    bool hibernate(){ return run("echo disk > /sys/power/state"); }
    bool hybrid()   { return run("echo hybrid > /sys/power/state"); }
}
