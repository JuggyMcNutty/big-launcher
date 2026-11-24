#pragma once

#include <memory>

#include "renderer.hpp"

namespace BL {
    class TextureSDL: public Texture {
    private:
        std::unique_ptr<SDL_Texture, decltype(&SDL_DestroyTexture)> texture;

    public:
        explicit TextureSDL(SDL_Texture *t): Texture(t->w, t->h), texture(t, SDL_DestroyTexture) {}
        const SDL_Texture* get_texture() const { return texture.get(); }
        void set_color_mod(const SDL_Color &color) override;
    };

    class RendererSDL: public Renderer {
    public:
        explicit RendererSDL(SDL_Window &window);
        ~RendererSDL() override;

        void set_draw_color(const SDL_Color &color) override;
        void clear() override;
        void present() override;
        void draw(const Texture &texture) override;
        void fill_rect(const SDL_FRect &rect, const SDL_Color &color) override;
        void draw_line(const SDL_FRect &a, const SDL_FRect &b, const SDL_Color &color) override;

        TexturePtr create_texture(SDL_Surface &surface) override;
        TexturePtr create_texture(SDL_Surface &surface, int w, int h) override;
        TexturePtr create_texture(int w, int h) override;
        void composit_texture(const Texture &src, const Texture &dst, SDL_FRect *coords) override;
        void set_clip_rect(const SDL_Rect &rect) override;
        void disable_clip() override;
        void set_render_scale(float scale_w, float scale_h) override;
        void set_logical_representation(int w, int h) override;

    private:
        std::unique_ptr<SDL_Renderer, decltype(&SDL_DestroyRenderer)> renderer;
    };
}
