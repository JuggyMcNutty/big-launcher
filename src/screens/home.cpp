#include "home.hpp"
#include "../core/app.hpp"
#include "../core/layout_model.hpp"
#include "../render/renderer.hpp"
#include "../render/theme.hpp"
#include "../render/image.hpp"
#include "../render/util.hpp"
#include "../ui/text_sprite.hpp"
#include "../ui/shapes.hpp"
#include <ctime>
#include <memory>

namespace Foyer {
    HomeScreen::HomeScreen(App &app): Screen(app)
    {
        build_sidebar();
    }

    void HomeScreen::build_sidebar()
    {
        sidebar_.clear();
        int mi = 0;
        for (const auto &m : app_.layout().menus)
            sidebar_.push_back({m.title, mi++, ""});
        for (const auto &c : app_.layout().sidebar_commands)
            sidebar_.push_back({c.title, -1, c.command});
        if (sidebar_.empty())
            sidebar_.push_back({"Home", -1, ""});
        sidebar_index_ = 0;
        menu_index_ = 0;
    }

    static SDL_Surface* load_icon(const std::string &path, int w, int h)
    {
        if (path.empty()) return nullptr;
        if (path.ends_with(".svg")) {
            SVGRasterizer raster;
            return raster.rasterize_svg_from_file(path, w, h);
        }
        std::string p = path;
        return load_surface(p);
    }

    void HomeScreen::on_show()
    {
        auto &r = app_.renderer();
        auto &t = app_.theme();
        float sw = app_.screen_w(), sh = app_.screen_h();
        columns_ = app_.layout().columns;
        y_min_ = sh * 0.12f;
        y_max_ = sh * 0.96f;

        // Sidebar
        sidebar_x_ = sw * 0.08f;
        sidebar_w_ = sw * 0.22f;
        sidebar_h_ = sh * 0.06f;
        sidebar_y0_ = y_min_;
        sidebar_y_advance_ = sh * 0.07f;

        // Cards
        float left = sw * 0.42f, right = sw * 0.92f;
        card_spacing_ = sw * 0.011f;
        card_w_ = (right - left - (columns_ - 1) * card_spacing_) / columns_;
        card_h_ = card_w_ * 3.f / 4.f;
        card_x0_ = left;
        card_y0_ = y_min_;

        // Sidebar text sprites
        sidebar_sprites_.clear();
        {
            Font &font = t.font((int)(sidebar_h_ * 0.5f));
            for (auto &e : sidebar_) {
                sidebar_sprites_.emplace_back();
                sidebar_sprites_.back().render(r, font, e.title, (int)(sidebar_w_ - 20));
            }
        }

        // Sidebar highlight
        sidebar_hl_ = create_rounded_rect(r, (int) sidebar_w_, (int) sidebar_h_,
                                          (int)(sidebar_h_ * 0.2f), t.sidebar_highlight);

        // Grid highlight
        int radius = (int)(card_h_ * 0.06f);
        grid_hl_ = create_rounded_border(r, (int) card_w_ + 16, (int) card_h_ + 16,
                                         radius + 8, 4, t.menu_highlight);

        rebuild_cards();
    }

    void HomeScreen::rebuild_cards()
    {
        auto &r = app_.renderer();
        card_texts_.clear();
        const auto &menu = app_.layout().menus[menu_index_];
        int radius = (int)(card_h_ * 0.06f);
        for (const auto &e : menu.entries) {
            // Background
            SDL_Color bg = {0x18, 0x18, 0x1E, 0xFF};
            if (!e.card.background.empty())
                hex_to_color(e.card.background, bg);
            auto target = r.create_texture((int) card_w_, (int) card_h_);
            auto bgc = create_rounded_rect(r, (int) card_w_, (int) card_h_, radius, bg);
            r.composit_texture(*bgc, *target, nullptr);

            // Icon
            if (!e.card.icon.empty()) {
                float margin = card_h_ * (e.card.icon_margin / 100.f);
                int iw = (int)(card_w_ - 2 * margin);
                int ih = (int)(card_h_ - 2 * margin);
                SDL_Surface *icon = load_icon(e.card.icon, iw, ih);
                if (icon) {
                    auto itex = r.create_texture(*icon);
                    SDL_FRect dst{margin, margin, itex->get_w(), itex->get_h()};
                    r.composit_texture(*itex, *target, &dst);
                    free_surface(icon);
                }
            }
            card_texts_.push_back(std::move(target));
        }
    }

    void HomeScreen::update_clock()
    {
        if (!app_.config().clock_enabled) return;
        time_t t = time(nullptr);
        struct tm tmv;
        localtime_r(&t, &tmv);
        char buf[64];
        snprintf(buf, sizeof(buf), app_.config().clock_format.c_str(), tmv);
        std::string s = buf;
        if (s != clock_str_) {
            clock_str_ = s;
            auto &r = app_.renderer();
            auto &t2 = app_.theme();
            clock_sprite_.render(r, t2.font((int)(sidebar_h_ * 0.5f)), s);
            app_.mark_dirty();
        }
    }

    void HomeScreen::update()
    {
        update_clock();
        app_.anim().update();
    }

    void HomeScreen::enter_grid()
    {
        mode_ = Mode::GRID;
        grid_row_ = 0;
        grid_col_ = 0;
        app_.play_click();
    }

    void HomeScreen::enter_sidebar()
    {
        mode_ = Mode::SIDEBAR;
        for (int i = 0; i < (int) sidebar_.size(); i++)
            if (sidebar_[i].menu_index == menu_index_) { sidebar_index_ = i; break; }
        app_.play_click();
    }

    void HomeScreen::move_sidebar(int delta)
    {
        int target = sidebar_index_ + delta;
        if (target < 0) target = 0;
        if (target >= (int) sidebar_.size()) target = (int) sidebar_.size() - 1;
        if (target != sidebar_index_) {
            sidebar_index_ = target;
            app_.play_click();
        }
    }

    void HomeScreen::move_grid(int dr, int dc)
    {
        const auto &menu = app_.layout().menus[menu_index_];
        int rows = (int) menu.entries.size() / columns_ + (menu.entries.size() % columns_ ? 1 : 0);
        int target_row = grid_row_ + dr;
        int target_col = grid_col_ + dc;
        if (target_row < 0) target_row = 0;
        if (target_row >= rows) target_row = rows - 1;
        if (target_col < 0) target_col = 0;
        if (target_col >= columns_) target_col = columns_ - 1;
        if (target_row != grid_row_ || target_col != grid_col_) {
            grid_row_ = target_row;
            grid_col_ = target_col;
            app_.play_click();
        }
    }

    void HomeScreen::select_sidebar()
    {
        SidebarEntry &e = sidebar_[sidebar_index_];
        if (e.menu_index >= 0) {
            if (e.menu_index != menu_index_) {
                menu_index_ = e.menu_index;
                rebuild_cards();
            }
            grid_row_ = 0;
            grid_col_ = 0;
            app_.play_click();
        }
        else if (!e.command.empty()) {
            app_.play_select();
            app_.execute_command(e.command);
        }
    }

    void HomeScreen::select_grid()
    {
        const auto &menu = app_.layout().menus[menu_index_];
        int idx = grid_row_ * columns_ + grid_col_;
        if (idx < 0 || idx >= (int) menu.entries.size())
            return;
        app_.play_select();
        app_.execute_command(menu.entries[idx].command, menu.entries[idx].title);
    }

    bool HomeScreen::on_action(Action action)
    {
        switch (action) {
            case Action::Up:
                if (mode_ == Mode::SIDEBAR) move_sidebar(-1);
                else move_grid(-1, 0);
                return true;
            case Action::Down:
                if (mode_ == Mode::SIDEBAR) move_sidebar(1);
                else move_grid(1, 0);
                return true;
            case Action::Left:
                if (mode_ == Mode::GRID) enter_sidebar();
                else move_grid(0, -1);
                return true;
            case Action::Right:
                if (mode_ == Mode::SIDEBAR) enter_grid();
                else move_grid(0, 1);
                return true;
            case Action::Accept:
                if (mode_ == Mode::SIDEBAR) select_sidebar();
                else select_grid();
                return true;
            case Action::Back:
                return false;
            default:
                return false;
        }
    }

    void HomeScreen::draw()
    {
        auto &r = app_.renderer();
        auto &t = app_.theme();
        float sw = app_.screen_w(), sh = app_.screen_h();

        r.clear();
        r.fill_rect({0, 0, sw, sh}, t.background);

        // Sidebar entries
        for (int i = 0; i < (int) sidebar_.size(); i++) {
            float y = sidebar_y0_ + i * sidebar_y_advance_;
            if (y > y_max_) break;
            SDL_Color col = (i == sidebar_index_ && mode_ == Mode::SIDEBAR)
                ? t.sidebar_text_hl : t.sidebar_text;
            sidebar_sprites_[i].draw(r, sidebar_x_ + 10, y + (sidebar_h_ - sidebar_sprites_[i].height()) / 2, col);
        }

        // Sidebar highlight
        if (mode_ == Mode::SIDEBAR && sidebar_hl_) {
            float hy = sidebar_y0_ + sidebar_index_ * sidebar_y_advance_;
            sidebar_hl_->set_pos(sidebar_x_, hy, sidebar_w_, sidebar_h_);
            r.draw(*sidebar_hl_);
        }

        // Cards
        const auto &menu = app_.layout().menus[menu_index_];
        int n = std::min((int) card_texts_.size(), (int) menu.entries.size());
        for (int i = 0; i < n; i++) {
            int row = i / columns_, col = i % columns_;
            float x = card_x0_ + col * (card_w_ + card_spacing_);
            float y = card_y0_ + row * (card_h_ + card_spacing_);
            if (y > y_max_) break;
            card_texts_[i]->set_pos(x, y, card_w_, card_h_);
            r.draw(*card_texts_[i]);
        }

        // Grid highlight
        if (mode_ == Mode::GRID && grid_hl_) {
            float hx = card_x0_ + grid_col_ * (card_w_ + card_spacing_);
            float hy = card_y0_ + grid_row_ * (card_h_ + card_spacing_);
            grid_hl_->set_pos(hx - 8, hy - 8, card_w_ + 16, card_h_ + 16);
            r.draw(*grid_hl_);
        }

        // Clock
        if (app_.config().clock_enabled && clock_sprite_.valid()) {
            float cy = y_max_ - sidebar_y_advance_;
            clock_sprite_.draw(r, sidebar_x_ + 10, cy, t.sidebar_text);
        }
    }
}
