#pragma once

#include <string>
#include <vector>
#include <memory>

#include "../ui/screen.hpp"
#include "../ui/text_sprite.hpp"
#include "../render/renderer.hpp"

namespace BL {
    // The main launcher screen: a sidebar (menus + commands) on the left and a
    // grid of app cards for the current menu. Navigation: Left/Right switch
    // between sidebar and grid; Up/Down move within; Accept selects.
    class HomeScreen : public Screen {
    public:
        explicit HomeScreen(App &app);
        void on_show() override;
        void update() override;
        void draw() override;
        bool on_action(Action action) override;

    private:
        struct SidebarEntry {
            std::string title;
            int menu_index = -1;     // >= 0 if this entry is a menu
            std::string command;     // set if this entry is a command
        };

        enum Mode { SIDEBAR, GRID };

        void build_sidebar();
        void rebuild_cards();
        void enter_grid();
        void enter_sidebar();
        void move_sidebar(int delta);
        void move_grid(int dr, int dc);
        void select_sidebar();
        void select_grid();

        std::vector<SidebarEntry> sidebar_;
        int sidebar_index_ = 0;
        Mode mode_ = Mode::SIDEBAR;
        int menu_index_ = 0;
        int grid_row_ = 0, grid_col_ = 0;
        int columns_ = 3;

        // Geometry
        float card_x0_ = 0, card_y0_ = 0;
        float card_w_ = 0, card_h_ = 0;
        float card_spacing_ = 0;
        float sidebar_x_ = 0, sidebar_y0_ = 0, sidebar_y_advance_ = 0;
        float sidebar_w_ = 0, sidebar_h_ = 0;
        float y_min_ = 0, y_max_ = 0;

        // Highlight tween (position)
        float hl_x_ = 0, hl_y_ = 0;      // current highlight pos (animated)
        float hl_target_x_ = 0, hl_target_y_ = 0;
        int hl_id_ = -1;

        // Clock
        std::string clock_str_;
        TextSprite clock_sprite_;
        void update_clock();

        // Cached textures
        std::vector<TextSprite> sidebar_sprites_;
        std::vector<std::unique_ptr<Texture>> card_texts_;
        std::unique_ptr<Texture> sidebar_hl_;
        std::unique_ptr<Texture> grid_hl_;
    };
}
