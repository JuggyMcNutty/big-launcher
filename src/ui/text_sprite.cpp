#include "text_sprite.hpp"
#include "../render/text.hpp"
#include "../render/image.hpp"
#include "../render/logger.hpp"

void Foyer::TextSprite::render(Renderer &r, Font &font, const std::string &text, int max_width)
{
    if (max_width <= 0)
        max_width = 100000;
    SDL_Surface *surface = font.render_text(text, nullptr, nullptr, max_width);
    if (!surface)
        return;
    tex_ = r.create_texture(*surface);
    Foyer::free_surface(surface);
}

void Foyer::TextSprite::draw(Renderer &r, float x, float y, const SDL_Color &color)
{
    if (!tex_)
        return;
    tex_->set_pos(x, y, tex_->get_w(), tex_->get_h());
    tex_->set_color_mod(color);
    r.draw(*tex_);
}
