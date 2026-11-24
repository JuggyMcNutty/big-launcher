#pragma once

#include "../../ui/list_screen.hpp"

namespace BL {
    class DisplaySettings : public ListScreen {
    public:
        explicit DisplaySettings(App &app);
        void on_show() override;
        bool on_action(Action action) override;
    protected:
        void on_select(int index) override;
    private:
        void refresh();
        std::string current_mode_ = "1920x1080@60";
        static constexpr int item_resolution = 0, item_scale = 1, item_overscan = 2;
    };
}
