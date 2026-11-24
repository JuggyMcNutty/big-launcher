#pragma once

#include "../../ui/list_screen.hpp"

namespace BL {
    class PowerSettings : public ListScreen {
    public:
        explicit PowerSettings(App &app);
    protected:
        void on_select(int index) override;
    private:
        static constexpr int item_reboot = 0, item_shutdown = 1, item_sleep = 2, item_timer = 3, item_restart_ui = 4;
        int sleep_timer_ = 0;  // minutes, 0 = off
    };
}
