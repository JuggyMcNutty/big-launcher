#pragma once

#include "../../ui/list_screen.hpp"

namespace BL {
    class AudioSettings : public ListScreen {
    public:
        explicit AudioSettings(App &app);
        void on_show() override;
        bool on_action(Action action) override;
    protected:
        void on_select(int index) override;
    private:
        void refresh();
        int volume_ = -1;
        bool muted_ = false;
        std::vector<std::string> sinks_;
        int sink_index_ = 0;
        static constexpr int item_volume = 0, item_mute = 1, item_output = 2, item_sounds = 3, item_uivol = 4;
    };
}
