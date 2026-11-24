#pragma once

namespace BL {
    // Power operations via OpenRC (the box runs OpenRC, not systemd).
    // These spawn short-lived helper processes; the caller should run them
    // through the job queue or directly (they are quick).
    namespace power {
        bool shutdown();   // poweroff
        bool reboot();     // reboot
        bool suspend();    // echo mem > /sys/power/state
        bool hibernate();  // echo disk > /sys/power/state
        bool hybrid();     // echo hybrid > /sys/power/state
    }
}
