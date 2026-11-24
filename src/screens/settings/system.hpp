#pragma once

#include "../../ui/list_screen.hpp"

namespace BL {
    class SystemSettings : public ListScreen {
    public:
        explicit SystemSettings(App &app);
        void on_show() override;
    protected:
        void on_select(int index) override;
    private:
        void refresh_info();
        static constexpr int item_hostname = 0, item_timezone = 1, item_sync = 2, item_updates = 3,
            item_storage = 4, item_ram = 5, item_temp = 6, item_uptime = 7, item_about = 8;
    };
}
