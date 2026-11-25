#pragma once

#include <map>
#include <tuple>
#include <vector>

#include <SDL3/SDL.h>

namespace Foyer::ui {
    // Immediate-mode drawing over SDL_Renderer with a transform stack
    // (uniform scale about a point + translate), a clip stack and an opacity
    // stack. Rounded rects, borders and soft shadows are drawn as 9-slices of
    // small cached textures, so they cost one textured quad each at any size.
    class Canvas {
    public:
        explicit Canvas(SDL_Renderer *r);
        ~Canvas();
        Canvas(const Canvas &) = delete;
        Canvas &operator=(const Canvas &) = delete;

        SDL_Renderer* sdl() const { return r_; }

        // transform
        void push_transform(float scale, float origin_x, float origin_y);
        void pop_transform();
        SDL_FRect map(const SDL_FRect &r) const;
        float map_len(float v) const { return v * a_; }

        // clip (rects in untransformed space; intersected with the current clip)
        void push_clip(const SDL_FRect &r);
        void pop_clip();

        // opacity multiplier
        void push_alpha(float a);
        void pop_alpha();
        float alpha() const { return alpha_.empty() ? 1.f : alpha_.back(); }

        // primitives (rects in untransformed px)
        void clear(SDL_Color c);
        void fill(const SDL_FRect &r, SDL_Color c);
        void rounded(const SDL_FRect &r, float radius, SDL_Color c);
        void border(const SDL_FRect &r, float radius, float thickness, SDL_Color c);
        void shadow(const SDL_FRect &r, float radius, float blur, SDL_Color c);
        // Vertical/horizontal gradient (2 stops) — used for hero scrims.
        void gradient(const SDL_FRect &r, SDL_Color from, SDL_Color to, bool vertical = true);
        // Draw a texture into dst; src may be null (whole texture).
        void texture(SDL_Texture *t, const SDL_FRect *src, const SDL_FRect &dst, SDL_Color mod = {255, 255, 255, 255});

        void present();

    private:
        struct Xf { float a, bx, by; };
        SDL_Renderer *r_;
        float a_ = 1.f, bx_ = 0.f, by_ = 0.f;
        std::vector<Xf> xf_;
        std::vector<SDL_Rect> clips_;
        std::vector<float> alpha_;

        std::map<int, SDL_Texture*> corner_;                       // radius px -> disc
        std::map<std::pair<int, int>, SDL_Texture*> ring_;         // (radius, thickness)
        std::map<std::pair<int, int>, SDL_Texture*> shadow_;       // (radius, blur)

        SDL_Texture* disc(int radius);
        SDL_Texture* ring(int radius, int thickness);
        SDL_Texture* shadow_tex(int radius, int blur);
        void nine(SDL_Texture *t, float corner_src, const SDL_FRect &dst, float corner_dst, SDL_Color c);
        void apply_clip();
        SDL_Color mod(SDL_Color c) const;
    };
}
