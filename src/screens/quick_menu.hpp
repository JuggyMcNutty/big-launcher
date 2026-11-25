#pragma once

#include "../ui/list_screen.hpp"

namespace Foyer {
    // Quick menu: shown as a gamescope external overlay while an app runs.
    // Actions: back to home (kill app), volume, mute, power.
    class QuickMenu : public ListScreen {
    public:
        explicit QuickMenu(App &app);
        void on_show() override;
    protected:
        void on_select(int index) override;
    private:
        enum { act_home, act_volup, act_voldown, act_mute, act_reboot, act_shutdown, act_sleep };
    };
}
