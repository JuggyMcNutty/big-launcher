#pragma once

#include <string>
#include <unordered_map>
#include <memory>

#include <SDL3/SDL.h>

#include "text.hpp"

namespace BL {
    // Central visual configuration: colors + fonts. Geometry is computed by
    // screens from the screen size and these values.
    class Theme {
    public:
        // Colors
        SDL_Color sidebar_highlight  = {0xF2, 0xF2, 0xF2, 0xFF};
        SDL_Color sidebar_text       = {0xF2, 0xF2, 0xF2, 0xFF};
        SDL_Color sidebar_text_hl    = {0x01, 0x01, 0x6E, 0xFF};
        SDL_Color menu_highlight     = {0xFF, 0xFF, 0xFF, 0xFF};
        SDL_Color background         = {0x10, 0x10, 0x14, 0xFF};
        SDL_Color text               = {0xFF, 0xFF, 0xFF, 0xFF};
        SDL_Color text_dim           = {0xAA, 0xAA, 0xAA, 0xFF};
        SDL_Color accent             = {0x4B, 0x9C, 0xD6, 0xFF};
        SDL_Color danger             = {0xF4, 0x43, 0x36, 0xFF};

        // Font file (shared) and sizes
        std::string font_file = "NotoSans-Medium.ttf";
        int sidebar_font_size = 34;
        int title_font_size   = 44;
        int item_font_size    = 30;
        int info_font_size    = 24;
        int status_font_size  = 22;

        // Cached fonts by (file, size)
        std::unordered_map<std::string, std::unique_ptr<Font>> fonts;

        Font& font(int size);
        Font& title_font();
        Font& item_font();
        Font& info_font();
        Font& status_font();

        void apply_config();  // pulls colors/sizes from Config
    };
}
