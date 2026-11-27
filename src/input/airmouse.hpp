#pragma once

#include <functional>
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <atomic>
#include <cstdint>

namespace Foyer {
    // Reads 2.4G "air mouse" USB composite devices straight from /dev/input.
    //
    // These devices report their buttons (including left/right click!) not as
    // EV_KEY/BTN_* but as EV_MSC MSC_SCAN: a 24-bit HID usage (page<<16 |
    // usage<<8), with a press and a release MSC report for one physical press.
    // The compositor never sees MSC_SCAN, so the launcher reads it itself and
    // turns it into semantic Actions via the remap table (Settings → Air
    // mouse). Pointer motion (REL_X/REL_Y) is left to the compositor/SDL.
    //
    // Device match: case-insensitive substring against the USB
    // product/manufacturer strings of the device owning an event node
    // (default "XING WEI"), falling back to the evdev name.
    class Airmouse {
    public:
        using Callback = std::function<void(int code, bool pressed)>;

        explicit Airmouse(Callback cb);
        ~Airmouse();

        void start();
        void stop();
        void set_match(const std::string &s);

        bool connected() const { return connected_.load(); }
        std::string device_name() const;

        // MSC pairing: a physical press emits the same code twice (press,
        // then release). Returns true for the press edge, false for release.
        static bool pair(int code, std::map<int, bool> &state);
        // Auto-release a code held for more than `ms` (device glitch).
        static void sweep(std::map<int, std::pair<bool, uint64_t>> &held, uint64_t now, uint64_t ms,
                          std::vector<int> *released);

    private:
        void thread_loop();
        // True if the event node belongs to a matching air mouse; fills name.
        static bool matches(const std::string &node, const std::string &match, std::string *name);
        // USB product/manufacturer of the device owning an event node.
        static bool sysfs_usb_name(const std::string &node, std::string &out);

        Callback cb_;
        std::thread thread_;
        std::atomic<bool> running_{false};
        std::atomic<bool> connected_{false};
        std::string match_ = "XING WEI";
        mutable std::mutex name_mu_;
        std::string devname_;    // guarded by name_mu_
    };
}
