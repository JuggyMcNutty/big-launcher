#include <SDL3_ttf/SDL_ttf.h>
#include "logger.hpp"
#include "text.hpp"
#include "util.hpp"

Foyer::Font::Font(const std::string &file, int height)
{
    std::string font_path = Foyer::find_file<Foyer::FileType::FONT>(file.c_str());
    if (font_path.empty())
        Foyer::logger::critical("Could not locate font '{}'", file);
    font = TTF_OpenFont(font_path.c_str(), static_cast<float>(height));
    if (!font)
        Foyer::logger::critical("Could not open font\nSDL Error: {}\n", SDL_GetError());
}

SDL_Surface* Foyer::Font::render_text(const std::string &text, SDL_Rect *src_rect, SDL_Rect *dst_rect, int max_width)
{
    if (text.empty()) {
        // SDL_ttf cannot render an empty string (returns NULL). Give callers
        // a 1x1 transparent surface so an empty sprite is just invisible.
        SDL_Surface *s = SDL_CreateSurface(1, 1, SDL_PIXELFORMAT_ARGB8888);
        if (s) SDL_FillSurfaceRect(s, nullptr, 0x00000000u); // transparent
        return s;
    }
    SDL_Surface *surface = nullptr;
    int width;
    std::string truncated_text;
    TTF_GetStringSize(font, text.c_str(), text.size(), &width, nullptr);
    if (width > max_width)
        truncated_text = utf8_truncate(text, width, max_width);
    const std::string &out_text = truncated_text.empty() ? text : truncated_text;

    surface = TTF_RenderText_Blended(font, out_text.c_str(), out_text.size(), color);
    if (!surface) {
        Foyer::logger::error("Could not render text '{}'", text);
        return surface;
    } 
    
    if (src_rect) {
        int y_asc_max = 0;
        int y_dsc_max = 0;
        int y_asc, y_dsc;
        int bytes = 0;
        Uint16 code_point;
        char *p = const_cast<char*>(out_text.data());
        while (*p != '\0') {
            code_point = get_unicode_code_point(p, bytes);
            TTF_GetGlyphMetrics(font, 
                code_point, 
                nullptr, 
                nullptr, 
                &y_dsc, 
                &y_asc,
                nullptr
            );
            if (y_asc > y_asc_max)
                y_asc_max = y_asc;
            if (y_dsc < y_dsc_max)
                y_dsc_max = y_dsc;
            p += bytes;
        }
        src_rect->x = 0;
        src_rect->y = TTF_GetFontAscent(font) - y_asc_max;
        src_rect->w = surface->w;
        src_rect->h = y_asc_max - y_dsc_max;
    }
    if (dst_rect && surface) {
        dst_rect->w = surface->w;
        dst_rect->h = (src_rect != nullptr) ? src_rect->h : surface->h;
    }
    return surface;
}
