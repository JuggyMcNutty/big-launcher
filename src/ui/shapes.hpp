#pragma once

#include <SDL3/SDL.h>

#include "../render/renderer.hpp"

namespace Foyer {
    // Create a rounded-rect texture filled with color (alpha supported).
    TexturePtr create_rounded_rect(Renderer &r, int w, int h, int radius, const SDL_Color &color);
    // Create a rounded-rect outline (border) texture.
    TexturePtr create_rounded_border(Renderer &r, int w, int h, int radius, int thickness, const SDL_Color &color);
}
