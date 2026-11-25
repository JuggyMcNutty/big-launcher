#pragma once

#include <map>
#include <optional>
#include <string>
#include <vector>

#include <SDL3/SDL.h>

#include "../ui/engine/tokens.hpp"

namespace Foyer {
    // ---- apps.d/*.toml ---------------------------------------------------------
    // Per-app metadata and artwork. Art paths are resolved relative to the
    // app file's directory (or absolute).
    struct AppEntry {
        std::string id;            // file stem unless set
        std::string title;
        std::string subtitle;
        std::string command;       // shell command, or ":special"
        std::vector<std::string> args;
        std::map<std::string, std::string> env;
        std::string icon;          // logo/glyph drawn on the tile (svg/png)
        std::string tile;          // full-bleed tile art (16:9)
        std::string hero;          // background art when focused
        std::string background;    // #RRGGBB tile colour
        std::string source;        // file it came from ("" = built-in / layout-inline)

        std::string launch_command() const;   // command + quoted args
        std::string resolve_art(const std::string &p) const;
    };

    // ---- layout.toml ----------------------------------------------------------------
    enum class TileSize { Small, Medium, Large };
    struct Tile {
        std::string app;           // id in apps.d
        // inline overrides / standalone tiles
        std::string title, command, icon, background;
        TileSize size = TileSize::Medium;
    };
    struct Row {
        std::string title;
        std::vector<Tile> tiles;
        TileSize size = TileSize::Medium;
    };
    struct Tab {
        std::string id;
        std::string title;
        std::string kind = "rows";   // "rows" | "library" | "settings"
        std::vector<Row> rows;
    };
    struct Layout {
        std::vector<Tab> tabs;
        std::vector<std::pair<std::string, std::string>> power;   // title, command (quick panel)
    };

    // ---- settings.toml ----------------------------------------------------------------
    struct KeyBinding { std::string key; std::string command; };   // key: SDL key name
    struct Settings {
        std::string theme = "default";
        std::string accent;        // "" = theme accent, else #RRGGBB
        std::string wallpaper;     // "" = theme default; "focused-art" | path | #color
        int ui_scale = 100;
        int overscan = 0;
        bool reduce_motion = false;
        float motion_speed = 1.f;
        bool sound = true;
        int sound_volume = 7;
        bool clock = true;
        std::string clock_format = "%H:%M";
        bool clock_24h = true;
        bool screensaver = true;
        int screensaver_idle_s = 300;
        int screensaver_dim = 70;  // %
        bool gamepad = true;
        std::string gamepad_mappings;
        bool mouse_select = false;
        std::string autostart;
        size_t asset_budget_mb = 96;
        std::vector<KeyBinding> hotkeys;
    };

    // ---- themes/<name>/theme.toml ---------------------------------------------------
    struct ThemeInfo {
        std::string name;          // dir name
        std::string title;
        std::string author;
        std::string dir;
    };

    // Search paths: user (/etc/foyer) first, then system (/usr/share/foyer),
    // then the build/asset dir next to the executable.
    class Paths {
    public:
        static void init(const std::string &config_dir);
        static const std::string& config_dir();           // where settings/layout/apps.d live (writable)
        static std::vector<std::string> data_dirs();       // themes/, apps.d/ defaults, assets/
        static std::string find_data(const std::string &rel);  // first existing
        static std::vector<std::string> theme_dirs();
    };

    // ---- I/O -------------------------------------------------------------------------
    struct LoadReport { std::vector<std::string> warnings; };

    Settings load_settings(const std::string &file, LoadReport *rep = nullptr);
    bool save_settings(const std::string &file, const Settings &s);

    Layout load_layout(const std::string &file, LoadReport *rep = nullptr);
    bool save_layout(const std::string &file, const Layout &l);

    std::map<std::string, AppEntry> load_apps(const std::vector<std::string> &dirs, LoadReport *rep = nullptr);
    bool save_app(const std::string &dir, const AppEntry &a);

    std::vector<ThemeInfo> list_themes();
    // Load tokens for a theme (falls back to built-in defaults for missing keys),
    // then apply per-breakpoint overrides and Settings (accent/wallpaper/motion).
    ui::Tokens load_theme(const std::string &name, const std::string &breakpoint, const Settings &s,
                          std::string *theme_dir = nullptr, LoadReport *rep = nullptr);

    // Atomic write with .bak (shared by all writers).
    bool write_file_atomic(const std::string &path, const std::string &content);

    // ---- import from big-launcher v2 (layout.xml + config.ini) -----------------------------
    struct ImportResult {
        Settings settings;
        Layout layout;
        std::map<std::string, AppEntry> apps;
        bool ok = false;
    };
    ImportResult import_legacy(const std::string &layout_xml, const std::string &config_ini,
                               const std::string &asset_dir = "");
    // Run once: if config_dir has no layout.toml but legacy files exist, convert.
    bool maybe_import(const std::string &config_dir, const std::string &legacy_layout,
                      const std::string &legacy_config, std::string *msg = nullptr);

    // colour helpers
    bool parse_color(const std::string &s, SDL_Color &out);
    std::string color_hex(SDL_Color c);
    std::string slug(const std::string &s);
}
