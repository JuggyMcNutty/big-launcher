#pragma once

#include "../../ui/list_screen.hpp"

namespace Foyer {
    class BluetoothSettings : public ListScreen {
    public:
        explicit BluetoothSettings(App &app);
        void on_show() override;
    protected:
        void on_select(int index) override;
    private:
        void refresh();
        void scan();
        bool powered_ = false;
        static constexpr int item_power = 0, item_scan = 1;
    };
}
