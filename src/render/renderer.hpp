#pragma once

#include <memory>

#include <SDL3/SDL.h>

namespace Foyer {
    class Texture {
    protected:
        SDL_FRect pos{};
        SDL_FRect tex_coords{};

    public:
        Texture() = default;
        Texture(float w, float h): pos({0.f, 0.f, w, h}), tex_coords(0.f, 0.f, w, h) {}
        virtual ~Texture() = default;

        virtual void set_color_mod(const SDL_Color &color) = 0;
        void set_x(float x) { pos.x = x; }
        void set_y(float y) { pos.y = y; }
        void set_w(float w) { pos.w = w; }
        void set_h(float h) { pos.h = h; }
        void update_pos(const SDL_FRect &p) { this->pos = p; }
        void set_pos(float x, float y, float w, float h) { pos = {x, y, w, h}; }
        float get_x() const { return pos.x; }
        float get_y() const { return pos.y; }
        float get_w() const { return pos.w; }
        float get_h() const { return pos.h; }
        const SDL_FRect& get_pos() const { return pos; }
        const SDL_FRect& get_tex_coords() const { return tex_coords; }
    };

    using TexturePtr = std::unique_ptr<Texture>;

    class Renderer {
    private:
        SDL_Window &window;

    protected:
        explicit Renderer(SDL_Window &window): window(window) {}

    public:
        virtual ~Renderer() = default;

        virtual void set_draw_color(const SDL_Color &color) = 0;
        virtual void clear() = 0;
        virtual void present() = 0;
        virtual void set_clip_rect(const SDL_Rect &rect) = 0;
        virtual void disable_clip() = 0;
        virtual void draw(const Texture &texture) = 0;
        virtual void fill_rect(const SDL_FRect &rect, const SDL_Color &color) = 0;
        virtual void draw_line(const SDL_FRect &a, const SDL_FRect &b, const SDL_Color &color) = 0;

        virtual TexturePtr create_texture(SDL_Surface &surface) = 0;
        virtual TexturePtr create_texture(SDL_Surface &surface, int w, int h) = 0;
        virtual TexturePtr create_texture(int w, int h) = 0;
        virtual void composit_texture(const Texture &src, const Texture &dst, SDL_FRect *coords) = 0;
        virtual void set_render_scale(float scale_w, float scale_h) { (void)scale_w; (void)scale_h; }
        virtual void set_logical_representation(int w, int h) { (void)w; (void)h; }
    };
}
