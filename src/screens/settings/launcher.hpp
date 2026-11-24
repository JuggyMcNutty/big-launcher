#pragma once

#include "../../ui/list_screen.hpp"

namespace BL {
    class LauncherSettings : public ListScreen {
    public:
        explicit LauncherSettings(App &app);
        void on_hide() override;
    protected:
        void on_select(int index) override;
    private:
        static constexpr int item_screensaver = 0, item_clock = 1, item_clockfmt = 2, item_sounds = 3;
    };
}
