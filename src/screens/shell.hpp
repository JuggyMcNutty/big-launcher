#pragma once

#include <string>
#include <vector>

#include "kit.hpp"
#include "../core/model.hpp"

namespace Foyer {
    class App;

    // A launchable thing shown as a tile (resolved from layout tile + apps.d).
    struct TileInfo {
        std::string title, subtitle, command;
        std::string icon, tile_art, hero_art;
        SDL_Color bg{0x1E, 0x22, 0x2E, 0xFF};
        TileSize size = TileSize::Medium;
        const AppEntry *app = nullptr;
    };

    // A Big-Picture tile: colour/art card with logo, focus zoom + glow + ring.
    class TileCard : public ui::Box {
    public:
        TileCard(App &app, TileInfo info, float width_dp, float aspect);
        TileInfo info;
        bool tick(ui::Engine &e, float dt) override;
        void paint(ui::Canvas &c, const SDL_FRect &r, float alpha) override;
        void paint_over(ui::Canvas &c, const SDL_FRect &r, float alpha) override;
    private:
        ui::Animated hl_{0.f};
        ui::Image *art_ = nullptr;
        ui::Image *logo_ = nullptr;
        ui::Text *name_ = nullptr;
    };

    // The root layer: top bar (tabs + status), the active tab's content,
    // a focused-art wallpaper and the toast stack.
    class Shell : public ui::Layer {
    public:
        explicit Shell(App &app);
        void build(ui::Engine &e) override;
        bool on_action(ui::Engine &e, Action a) override;
        bool tick(ui::Engine &e, float dt) override;
        void paint_under(ui::Engine &e, ui::Canvas &c) override;
        void on_enter(ui::Engine &e) override;

        void show_tab(const std::string &id);
        const std::string& current_tab() const { return tab_id_; }
        TileInfo resolve(const Tile &t) const;

        // global toast queue (any layer may post)
        static void post_toast(const std::string &msg, bool error);
        static void set_launching(const std::string &title);
        static void paint_overlay(ui::Engine &e, ui::Canvas &c);   // toasts, above all layers
        static bool overlay_active();

    private:
        App &app_;
        std::string tab_id_;
        ui::Node *tabs_row_ = nullptr;
        ui::Node *content_ = nullptr;
        ui::Text *clock_ = nullptr;
        ui::Image *net_icon_ = nullptr, *vol_icon_ = nullptr;
        ui::Text *net_text_ = nullptr, *vol_text_ = nullptr;
        ui::Text *hero_title_ = nullptr, *hero_sub_ = nullptr;
        std::vector<ui::Button*> tab_buttons_;
        // wallpaper cross-fade
        ui::Image wall_a_, wall_b_;
        bool wall_flip_ = false;
        std::string wall_path_;
        ui::Animated wall_mix_{1.f};
        Uint64 last_clock_ = 0;
        int built_for_tabs_ = 0;

        void build_tab(ui::Engine &e);
        void build_home(ui::Engine &e, const Tab &tab, ui::Node &into);
        void build_library(ui::Engine &e, ui::Node &into);
        void build_settings(ui::Engine &e, ui::Node &into);
        void focus_tile(const TileInfo &info);
        void update_status(ui::Engine &e);
        void set_wallpaper(const std::string &path, bool ambient = false);
    };
}
