#include "renderer_sdl.hpp"
#include "logger.hpp"

void Foyer::TextureSDL::set_color_mod(const SDL_Color &color)
{
    SDL_SetTextureBlendMode(texture.get(), SDL_BLENDMODE_BLEND);
    SDL_SetTextureColorMod(texture.get(), color.r, color.g, color.b);
    SDL_SetTextureAlphaMod(texture.get(), color.a);
}

Foyer::RendererSDL::RendererSDL(SDL_Window &window):
    Renderer(window),
    renderer(SDL_CreateRenderer(&window, nullptr), SDL_DestroyRenderer)
{
    if (!renderer)
        Foyer::logger::critical("Could not create renderer (SDL Error: {})", SDL_GetError());

    SDL_PropertiesID props = SDL_GetRendererProperties(renderer.get());
    const auto formats = reinterpret_cast<const SDL_PixelFormat*>(SDL_GetPointerProperty(props, SDL_PROP_RENDERER_TEXTURE_FORMATS_POINTER, nullptr));
    if (!formats)
        Foyer::logger::critical("Could not detect pixel formats supported by renderer");
    int i = 0;
    for (; formats[i] != SDL_PIXELFORMAT_UNKNOWN; i++) {
        if (formats[i] == SDL_PIXELFORMAT_ARGB8888)
            break;
    }
    if (formats[i] == SDL_PIXELFORMAT_UNKNOWN)
        Foyer::logger::critical("GPU does not support the required pixel format");

    SDL_SetRenderVSync(renderer.get(), 1);
    SDL_SetRenderDrawBlendMode(renderer.get(), SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer.get(), 0xFF, 0xFF, 0xFF, 0xFF);
}

Foyer::RendererSDL::~RendererSDL() = default;

void Foyer::RendererSDL::set_draw_color(const SDL_Color &color)
{
    SDL_SetRenderDrawColor(renderer.get(), color.r, color.g, color.b, color.a);
}

void Foyer::RendererSDL::clear()
{
    SDL_RenderClear(renderer.get());
}

void Foyer::RendererSDL::present()
{
    SDL_RenderPresent(renderer.get());
}

Foyer::TexturePtr Foyer::RendererSDL::create_texture(SDL_Surface &surface)
{
    return std::make_unique<TextureSDL>(SDL_CreateTextureFromSurface(renderer.get(), &surface));
}

Foyer::TexturePtr Foyer::RendererSDL::create_texture(SDL_Surface &surface, int w, int h)
{
    SDL_Texture *src_texture = SDL_CreateTextureFromSurface(renderer.get(), &surface);
    SDL_Texture *dst_texture = SDL_CreateTexture(renderer.get(), SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_TARGET, w, h);
    SDL_SetTextureBlendMode(dst_texture, SDL_BLENDMODE_BLEND);
    SDL_SetRenderTarget(renderer.get(), dst_texture);
    SDL_RenderTexture(renderer.get(), src_texture, nullptr, nullptr);
    SDL_DestroyTexture(src_texture);
    return std::make_unique<TextureSDL>(dst_texture);
}

Foyer::TexturePtr Foyer::RendererSDL::create_texture(int w, int h)
{
    SDL_Texture *texture = SDL_CreateTexture(renderer.get(), SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_TARGET, w, h);
    return std::make_unique<TextureSDL>(texture);
}

void Foyer::RendererSDL::draw(const Texture &texture)
{
    SDL_SetRenderTarget(renderer.get(), nullptr);
    SDL_RenderTexture(renderer.get(),
        const_cast<SDL_Texture*>(static_cast<const TextureSDL&>(texture).get_texture()),
        &texture.get_tex_coords(),
        &texture.get_pos()
    );
}

void Foyer::RendererSDL::fill_rect(const SDL_FRect &rect, const SDL_Color &color)
{
    SDL_SetRenderDrawColor(renderer.get(), color.r, color.g, color.b, color.a);
    SDL_FRect r = rect;
    SDL_RenderFillRect(renderer.get(), &r);
}

void Foyer::RendererSDL::draw_line(const SDL_FRect &a, const SDL_FRect &b, const SDL_Color &color)
{
    SDL_SetRenderDrawColor(renderer.get(), color.r, color.g, color.b, color.a);
    SDL_RenderLine(renderer.get(), a.x, a.y, b.x, b.y);
}

void Foyer::RendererSDL::composit_texture(const Texture &src, const Texture &dst, SDL_FRect *coords)
{
    SDL_SetRenderTarget(renderer.get(), const_cast<SDL_Texture*>(static_cast<const TextureSDL&>(dst).get_texture()));
    SDL_RenderTexture(renderer.get(), const_cast<SDL_Texture*>(static_cast<const TextureSDL&>(src).get_texture()), nullptr, coords);
    SDL_SetRenderTarget(renderer.get(), nullptr);
}

void Foyer::RendererSDL::set_clip_rect(const SDL_Rect &rect)
{
    SDL_SetRenderClipRect(renderer.get(), &rect);
}

void Foyer::RendererSDL::disable_clip()
{
    SDL_SetRenderClipRect(renderer.get(), nullptr);
}

void Foyer::RendererSDL::set_render_scale(float scale_w, float scale_h)
{
    SDL_SetRenderScale(renderer.get(), scale_w, scale_h);
}

void Foyer::RendererSDL::set_logical_representation(int w, int h)
{
    SDL_SetRenderLogicalPresentation(renderer.get(), w, h, SDL_LOGICAL_PRESENTATION_LETTERBOX);
}
