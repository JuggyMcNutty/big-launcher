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

        // MSC pairing: a physical press emits the same code twice (press
        // report, release report), and composite air mice mirror the same
        // code onto several HID input nodes (consumer / system / kbd / mouse
        // all advertise MSC_SCAN). step() turns the raw report stream into
        // press/release edges: the first report after DEDUP_US is a press,
        // the next is a release, and reports within DEDUP_US of the previous
        // one are mirrors of the same physical press and are ignored.
        // Returns +1 press edge, -1 release edge, 0 ignored mirror.
        static const uint64_t DEDUP_US = 100000;   // 100 ms
        static int step(int code, uint64_t now, std::map<int, std::pair<bool, uint64_t>> &held);
        // Auto-release a code held for more than `window_us` (lost release).
        static void sweep(std::map<int, std::pair<bool, uint64_t>> &held, uint64_t now, uint64_t window_us,
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
