#pragma once

#include <string>
#include <vector>
#include <optional>

#include <SDL3/SDL.h>

namespace BL {
    struct Hotkey {
        SDL_Keycode keycode = 0;
        std::string command;
    };

    // INI config with read AND write. Preserves comments and key order by
    // keeping the raw line structure and only rewriting managed key values.
    // Writes are atomic (temp + rename) with a .bak backup.
    class Config {
    public:
        // [Settings]
        SDL_Color sidebar_highlight_color = {0xF2, 0xF2, 0xF2, 0xFF};
        SDL_Color sidebar_text_color      = {0xF2, 0xF2, 0xF2, 0xFF};
        SDL_Color sidebar_text_color_hl   = {0x01, 0x01, 0x6E, 0xFF};
        SDL_Color menu_highlight_color    = {0xFF, 0xFF, 0xFF, 0xFF};
        std::string background_image_path;
        bool mouse_select = false;
        bool debug = false;

        // [Sound]
        bool sound_enabled = false;
        int  sound_volume  = 10;

        // [Clock]
        bool clock_enabled = false;
        std::string clock_format = "%H:%M";

        // [Screensaver]
        bool  screensaver_enabled   = false;
        Uint32 screensaver_idle_time = 900000;
        Uint8 screensaver_intensity = 170;

        // [Gamepad]
        bool gamepad_enabled = false;
        std::string gamepad_mappings_file;

        // [Hotkeys]  (keycode hex -> command)
        std::vector<Hotkey> hotkeys;

        // [Launcher]  (new in v2)
        std::string autostart_command;
        int ui_scale = 100;   // percent
        int overscan = 0;     // percent

        Config() = default;

        void parse(const std::string &file);
        void write(const std::string &file) const;

        // Low-level ordered access. Updates the raw line (or appends) and the
        // matching typed field if known.
        void set(const std::string &section, const std::string &key, const std::string &value);
        std::optional<std::string> get(const std::string &section, const std::string &key) const;

    private:
        struct Line {
            std::string section;
            std::string key;
            std::string value;
            bool kv = false;
            std::string original;  // raw text (comments / blanks / unknown keys)
        };

        std::string path;
        std::vector<Line> lines;

        // String form of a typed field for a managed key (nullopt if unmanaged)
        std::optional<std::string> typed_value(const std::string &section, const std::string &key) const;
        // Parse a string into a typed field for a managed key
        void typed_set(const std::string &section, const std::string &key, const std::string &value);
        void load_typed();
    };
}
