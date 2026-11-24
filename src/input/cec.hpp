#pragma once

#include <functional>
#include <thread>

#include "action.hpp"

namespace BL {
// HDMI-CEC backend (libcec). Maps TV remote keys to actions and issues TV
// power/standby on sleep. Untested until a CEC adapter is present.
class Cec {
public:
    using Callback = std::function<void(Action)>;

    Cec();
    ~Cec();

    bool init(const char *adapter = nullptr);
    void set_callback(Callback cb) { cb_ = std::move(cb); }
    void standby();   // TV standby (on sleep)
    void active();    // wake TV

private:
    Callback cb_;
    void *cec_ = nullptr;
};
}
