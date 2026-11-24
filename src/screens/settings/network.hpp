#pragma once

#include "../../ui/list_screen.hpp"

namespace BL {
    class NetworkSettings : public ListScreen {
    public:
        explicit NetworkSettings(App &app);
        void on_show() override;
    protected:
        void on_select(int index) override;
    private:
        void refresh_status();
        void scan();
        static constexpr int item_eth = 0, item_wifi = 1, item_scan = 2;
    };
}
