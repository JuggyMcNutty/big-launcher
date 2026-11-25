#pragma once

#include <string>
#include <memory>

#include <SDL3/SDL.h>

#include "../render/renderer.hpp"

namespace Foyer {
    class Font;

    // Renders a string to a texture once; draw it at any position with color
    // modulation. Re-render when the text/font changes.
    class TextSprite {
    public:
        TextSprite() = default;

        void render(Renderer &r, Font &font, const std::string &text, int max_width = 0);
        void draw(Renderer &r, float x, float y, const SDL_Color &color = {0xFF,0xFF,0xFF,0xFF});
        bool valid() const { return tex_ != nullptr; }
        float width() const { return tex_ ? tex_->get_w() : 0.f; }
        float height() const { return tex_ ? tex_->get_h() : 0.f; }

    private:
        TexturePtr tex_;
    };
}
