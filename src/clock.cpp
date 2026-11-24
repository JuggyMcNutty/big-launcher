#include <chrono>
#include <ctime>

#include <lconfig.h>

#include "clock.hpp"
#include "text.hpp"
#include "config.hpp"
#include "logger.hpp"

extern BL::Config config;

BL::Clock::Clock(int font_height, int max_width):
    BL::Drawable(),
    max_width(max_width)
{
    font = new Font(SIDEBAR_FONT, font_height);
}

BL::Clock::~Clock()
{
    delete font;
}

std::string BL::Clock::get_time_string() const
{
    const std::time_t t = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    std::tm tm = {};
    localtime_r(&t, &tm);
    char buffer[64];
    const size_t length = std::strftime(buffer, sizeof(buffer), config.clock_format.c_str(), &tm);
    if (!length)
        BL::logger::error("Could not format clock with format string '{}'", config.clock_format);
    return std::string(buffer, length);
}

void BL::Clock::render_surface()
{
    last_rendered = get_time_string();
    surface = font->render_text(last_rendered, nullptr, nullptr, max_width);
    pos.w = surface->w;
    pos.h = surface->h;
}

void BL::Clock::render_texture()
{
    texture = renderer->create_texture(*surface);
}

void BL::Clock::update()
{
    const std::string now = get_time_string();
    if (now == last_rendered)
        return;
    last_rendered = now;

    // Regenerate surface and texture with the new time
    BL::free_surface(surface);
    surface = font->render_text(now, nullptr, nullptr, max_width);
    pos.w = surface->w;
    pos.h = surface->h;
    delete texture;
    render_texture();
    texture->set_color_mod(config.sidebar_text_color);
}

void BL::Clock::set_text_color(const SDL_Color &color)
{
    if (texture)
        texture->set_color_mod(color);
}