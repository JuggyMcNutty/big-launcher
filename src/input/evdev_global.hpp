#pragma once

#include <functional>
#include <thread>
#include <atomic>

#include "action.hpp"

namespace BL {
// Non-grabbing evdev reader for global Home/Volume keys while an app is
// focused. Runs on its own thread; reports actions via callback.
class EvdevGlobal {
public:
    using Callback = std::function<void(Action)>;

    EvdevGlobal();
    ~EvdevGlobal();

    void set_callback(Callback cb) { cb_ = std::move(cb); }
    void start();
    void stop();

private:
    void thread_loop();
    Callback cb_;
    std::thread thread_;
    std::atomic<bool> running_{false};
};
}
