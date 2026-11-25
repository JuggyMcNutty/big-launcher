#include <algorithm>
#include <cmath>

#include "canvas.hpp"

namespace Foyer::ui {
    static constexpr int MAX_RADIUS = 256;

    Canvas::Canvas(SDL_Renderer *r): r_(r) {}

    Canvas::~Canvas()
    {
        for (auto &[k, t] : corner_) SDL_DestroyTexture(t);
        for (auto &[k, t] : ring_) SDL_DestroyTexture(t);
        for (auto &[k, t] : shadow_) SDL_DestroyTexture(t);
    }

    // ---- state stacks ------------------------------------------------------------
    void Canvas::push_transform(float s, float ox, float oy)
    {
        xf_.push_back({a_, bx_, by_});
        // local p -> ox + (p - ox) * s, then the existing transform
        bx_ = a_ * ox * (1.f - s) + bx_;
        by_ = a_ * oy * (1.f - s) + by_;
        a_ *= s;
    }

    void Canvas::pop_transform()
    {
        if (xf_.empty()) return;
        a_ = xf_.back().a; bx_ = xf_.back().bx; by_ = xf_.back().by;
        xf_.pop_back();
    }

    SDL_FRect Canvas::map(const SDL_FRect &r) const
    {
        return {r.x * a_ + bx_, r.y * a_ + by_, r.w * a_, r.h * a_};
    }

    void Canvas::apply_clip()
    {
        if (clips_.empty()) SDL_SetRenderClipRect(r_, nullptr);
        else SDL_SetRenderClipRect(r_, &clips_.back());
    }

    void Canvas::push_clip(const SDL_FRect &rf)
    {
        SDL_FRect m = map(rf);
        SDL_Rect r{(int) std::floor(m.x), (int) std::floor(m.y),
                   (int) std::ceil(m.x + m.w) - (int) std::floor(m.x),
                   (int) std::ceil(m.y + m.h) - (int) std::floor(m.y)};
        if (!clips_.empty()) {
            SDL_Rect out;
            if (!SDL_GetRectIntersection(&r, &clips_.back(), &out)) out = {0, 0, 0, 0};
            r = out;
        }
        clips_.push_back(r);
        apply_clip();
    }

    void Canvas::pop_clip()
    {
        if (!clips_.empty()) clips_.pop_back();
        apply_clip();
    }

    void Canvas::push_alpha(float a) { alpha_.push_back(alpha() * std::clamp(a, 0.f, 1.f)); }
    void Canvas::pop_alpha() { if (!alpha_.empty()) alpha_.pop_back(); }

    SDL_Color Canvas::mod(SDL_Color c) const
    {
        c.a = (Uint8) std::lround(c.a * alpha());
        return c;
    }

    // ---- cached shape textures ---------------------------------------------------
    static SDL_Texture* make_alpha_texture(SDL_Renderer *r, int w, int h, auto &&coverage)
    {
        SDL_Surface *s = SDL_CreateSurface(w, h, SDL_PIXELFORMAT_RGBA32);
        if (!s) return nullptr;
        for (int y = 0; y < h; y++) {
            Uint8 *row = (Uint8*) s->pixels + y * s->pitch;
            for (int x = 0; x < w; x++) {
                float a = std::clamp(coverage(x + 0.5f, y + 0.5f), 0.f, 1.f);
                row[x * 4 + 0] = 255; row[x * 4 + 1] = 255; row[x * 4 + 2] = 255;
                row[x * 4 + 3] = (Uint8) std::lround(a * 255.f);
            }
        }
        SDL_Texture *t = SDL_CreateTextureFromSurface(r, s);
        SDL_DestroySurface(s);
        if (t) {
            SDL_SetTextureBlendMode(t, SDL_BLENDMODE_BLEND);
            SDL_SetTextureScaleMode(t, SDL_SCALEMODE_LINEAR);
        }
        return t;
    }

    // Signed distance from p to a rounded box centred in [0,w]x[0,h] with inset
    // `in` and corner radius `rad` (negative inside).
    static float sd_round_box(float px, float py, float w, float h, float in, float rad)
    {
        float hx = w / 2 - in, hy = h / 2 - in;
        float qx = std::fabs(px - w / 2) - (hx - rad);
        float qy = std::fabs(py - h / 2) - (hy - rad);
        float ox = std::max(qx, 0.f), oy = std::max(qy, 0.f);
        return std::sqrt(ox * ox + oy * oy) + std::min(std::max(qx, qy), 0.f) - rad;
    }

    SDL_Texture* Canvas::disc(int rad)
    {
        auto it = corner_.find(rad);
        if (it != corner_.end()) return it->second;
        int n = rad * 2 + 2;
        SDL_Texture *t = make_alpha_texture(r_, n, n, [&](float x, float y) {
            return 0.5f - sd_round_box(x, y, (float) n, (float) n, 0.f, (float) rad);
        });
        corner_[rad] = t;
        return t;
    }

    SDL_Texture* Canvas::ring(int rad, int th)
    {
        auto key = std::make_pair(rad, th);
        auto it = ring_.find(key);
        if (it != ring_.end()) return it->second;
        int n = rad * 2 + 2;
        SDL_Texture *t = make_alpha_texture(r_, n, n, [&](float x, float y) {
            float d = sd_round_box(x, y, (float) n, (float) n, 0.f, (float) rad);
            float outer = 0.5f - d;
            float inner = 0.5f - (-d - (float) th);   // coverage of the hole
            return std::min(outer, inner);
        });
        ring_[key] = t;
        return t;
    }

    SDL_Texture* Canvas::shadow_tex(int rad, int blur)
    {
        auto key = std::make_pair(rad, blur);
        auto it = shadow_.find(key);
        if (it != shadow_.end()) return it->second;
        int n = (rad + blur) * 2 + 2;
        float b = std::max(1.f, (float) blur);
        SDL_Texture *t = make_alpha_texture(r_, n, n, [&](float x, float y) {
            float d = sd_round_box(x, y, (float) n, (float) n, (float) blur, (float) rad);
            float v = std::clamp(0.5f - d / (2.f * b), 0.f, 1.f);
            return v * v * (3.f - 2.f * v);   // smoothstep falloff
        });
        shadow_[key] = t;
        return t;
    }

    void Canvas::nine(SDL_Texture *t, float corner_src, const SDL_FRect &dst, float corner_dst, SDL_Color c)
    {
        if (!t) return;
        c = mod(c);
        SDL_SetTextureColorMod(t, c.r, c.g, c.b);
        SDL_SetTextureAlphaMod(t, c.a);
        SDL_FRect d = map(dst);
        float cd = map_len(corner_dst);
        cd = std::min({cd, d.w / 2, d.h / 2});
        float scale = corner_src > 0 ? cd / corner_src : 1.f;
        SDL_RenderTexture9Grid(r_, t, nullptr, corner_src, corner_src, corner_src, corner_src, scale, &d);
    }

    // ---- primitives ------------------------------------------------------------
    void Canvas::clear(SDL_Color c)
    {
        SDL_SetRenderDrawColor(r_, c.r, c.g, c.b, c.a);
        SDL_RenderClear(r_);
    }

    void Canvas::fill(const SDL_FRect &r, SDL_Color c)
    {
        c = mod(c);
        if (!c.a) return;
        SDL_SetRenderDrawColor(r_, c.r, c.g, c.b, c.a);
        SDL_FRect m = map(r);
        SDL_RenderFillRect(r_, &m);
    }

    void Canvas::rounded(const SDL_FRect &r, float radius, SDL_Color c)
    {
        float rpx = map_len(radius);
        if (rpx < 0.75f) { fill(r, c); return; }
        int q = std::clamp((int) std::lround(rpx), 1, MAX_RADIUS);
        nine(disc(q), (float) q, r, radius, c);
    }

    void Canvas::border(const SDL_FRect &r, float radius, float thickness, SDL_Color c)
    {
        float rpx = std::max(map_len(radius), map_len(thickness));
        int th = std::max(1, (int) std::lround(map_len(thickness)));
        int q = std::clamp((int) std::lround(rpx), th, MAX_RADIUS);
        nine(ring(q, th), (float) q, r, std::max(radius, thickness), c);
    }

    void Canvas::shadow(const SDL_FRect &r, float radius, float blur, SDL_Color c)
    {
        int rq = std::clamp((int) std::lround(map_len(radius)), 0, MAX_RADIUS);
        int bq = std::clamp((int) std::lround(map_len(blur)), 1, 64);
        float corner_src = (float) (rq + bq);
        float bl = blur;   // dst inflate in local units
        SDL_FRect d{r.x - bl, r.y - bl, r.w + 2 * bl, r.h + 2 * bl};
        nine(shadow_tex(rq, bq), corner_src, d, radius + blur, c);
    }

    void Canvas::gradient(const SDL_FRect &r, SDL_Color a, SDL_Color b, bool vertical)
    {
        SDL_FRect m = map(r);
        a = mod(a); b = mod(b);
        auto fc = [](SDL_Color c) { return SDL_FColor{c.r / 255.f, c.g / 255.f, c.b / 255.f, c.a / 255.f}; };
        SDL_Vertex v[4];
        v[0] = {{m.x, m.y}, fc(a), {0, 0}};
        v[1] = {{m.x + m.w, m.y}, fc(vertical ? a : b), {0, 0}};
        v[2] = {{m.x + m.w, m.y + m.h}, fc(b), {0, 0}};
        v[3] = {{m.x, m.y + m.h}, fc(vertical ? b : a), {0, 0}};
        int idx[6] = {0, 1, 2, 0, 2, 3};
        SDL_RenderGeometry(r_, nullptr, v, 4, idx, 6);
    }

    void Canvas::texture(SDL_Texture *t, const SDL_FRect *src, const SDL_FRect &dst, SDL_Color c)
    {
        if (!t) return;
        c = mod(c);
        SDL_SetTextureColorMod(t, c.r, c.g, c.b);
        SDL_SetTextureAlphaMod(t, c.a);
        SDL_FRect d = map(dst);
        SDL_RenderTexture(r_, t, src, &d);
    }

    void Canvas::present()
    {
        SDL_RenderPresent(r_);
    }
}
