#pragma once

#include "kit.hpp"

namespace Foyer {
    class App;

    // Quick-access panel sliding in from the right: now playing, volume,
    // close app / home, display + power shortcuts. As --overlay it is drawn
    // by a separate process that gamescope composites over the running app.
    class QuickPanel : public ui::Layer {
    public:
        QuickPanel(App &app, bool external);
        void build(ui::Engine &e) override;
        void paint_under(ui::Engine &e, ui::Canvas &c) override;
        bool on_action(ui::Engine &e, Action a) override;
        bool tick(ui::Engine &e, float dt) override;
        void on_enter(ui::Engine &e) override;
    private:
        App &app_;
        bool external_;
        kit::RowButton *vol_ = nullptr;
        ui::Text *stats_line_ = nullptr;
        int volume_ = -1;
        float poll_ = 0;   // seconds since the last stats sample
        void refresh_volume();
        void refresh_stats();
        void close_panel();
    };
}
