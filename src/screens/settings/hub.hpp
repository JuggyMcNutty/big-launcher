#pragma once

#include "../../ui/list_screen.hpp"

namespace Foyer {
    class SettingsHub : public ListScreen {
    public:
        explicit SettingsHub(App &app);
    protected:
        void on_select(int index) override;
    };
}
