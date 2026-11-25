#pragma once

#include <string>

#include <SDL3/SDL.h>

namespace Foyer::ui {
    // Design tokens: the values a theme can change. Sizes are in dp. Loaded
    // from themes/<name>/theme.toml (see core/theme_loader).
    struct Tokens {
        // colors
        SDL_Color bg          {0x0B, 0x0D, 0x12, 0xFF};
        SDL_Color surface     {0x17, 0x1A, 0x22, 0xFF};
        SDL_Color surface_hi  {0x23, 0x27, 0x33, 0xFF};
        SDL_Color accent      {0x4F, 0x8C, 0xFF, 0xFF};
        SDL_Color on_accent   {0xFF, 0xFF, 0xFF, 0xFF};
        SDL_Color text        {0xF4, 0xF6, 0xFA, 0xFF};
        SDL_Color text_dim    {0x9A, 0xA3, 0xB5, 0xFF};
        SDL_Color danger      {0xF2, 0x55, 0x55, 0xFF};
        SDL_Color focus_ring  {0xFF, 0xFF, 0xFF, 0xFF};
        SDL_Color shadow      {0x00, 0x00, 0x00, 0x99};
        SDL_Color scrim       {0x00, 0x00, 0x00, 0xB0};

        // typography
        std::string font         = "Inter-Medium.ttf";
        std::string font_display = "Inter-Medium.ttf";
        float text_xs = 18, text_sm = 22, text_md = 27, text_lg = 34, text_xl = 48, text_hero = 76;

        // shape
        float radius_sm = 10, radius_md = 16, radius_lg = 24;
        float focus_ring_width = 4;
        float focus_scale = 1.07f;

        // layout
        float page_margin = 72;          // dp, horizontal content margin (inside safe area)
        float tile_w = 300;              // dp, base tile width (scaled per breakpoint by screens)
        float tile_aspect = 16.f / 9.f;
        float tile_gap = 26;

        // motion
        float motion_speed = 1.f;
        bool reduce_motion = false;

        // wallpaper
        std::string wallpaper_mode = "focused-art";   // "focused-art" | "image" | "color"
        std::string wallpaper_image;
        float wallpaper_dim = 0.55f;                  // 0..1 darkening over art
        float wallpaper_blur = 28;                    // dp
    };

    inline SDL_Color with_alpha(SDL_Color c, Uint8 a) { c.a = a; return c; }
    inline SDL_Color mix(SDL_Color a, SDL_Color b, float t)
    {
        auto l = [t](Uint8 x, Uint8 y) { return (Uint8) (x + (y - x) * t); };
        return {l(a.r, b.r), l(a.g, b.g), l(a.b, b.b), l(a.a, b.a)};
    }
}
