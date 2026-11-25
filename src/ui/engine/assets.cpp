#include <algorithm>
#include <cmath>
#include <cstring>

#include <SDL3_image/SDL_image.h>

#include "assets.hpp"
#include "../../render/logger.hpp"
#define NANOSVG_IMPLEMENTATION
#include "../../external/nanosvg.h"
#define NANOSVGRAST_IMPLEMENTATION
#include "../../external/nanosvgrast.h"
#include "../../external/fast_gaussian_blur_template.h"

namespace Foyer::ui {
    Asset::~Asset()
    {
        if (surface) SDL_DestroySurface(surface);
        if (texture) SDL_DestroyTexture(texture);
    }

    AssetCache::AssetCache(SDL_Renderer *r, size_t budget, int threads): r_(r), budget_(budget)
    {
        if (threads <= 0) {
            int hc = SDL_GetNumLogicalCPUCores();
            threads = std::clamp(hc - 1, 1, 3);
        }
        for (int i = 0; i < threads; i++)
            workers_.emplace_back(&AssetCache::worker, this);
    }

    AssetCache::~AssetCache()
    {
        {
            std::lock_guard l(mu_);
            stop_ = true;
        }
        cv_.notify_all();
        for (auto &t : workers_) t.join();
        clear();
    }

    static std::string make_key(const std::string &p, int w, int h, float blur, bool cover, float radius)
    {
        return p + '\x1f' + std::to_string(w) + 'x' + std::to_string(h) + '\x1f' +
               std::to_string((int) std::lround(blur * 10)) + (cover ? "c" : "f") + std::to_string((int) radius);
    }

    AssetHandle AssetCache::request(const std::string &path, int w, int h, float blur, bool cover, float radius)
    {
        // Quantise sizes so tiny animated size changes don't thrash the cache.
        auto q = [](int v) { return v <= 0 ? 0 : std::max(8, (v + 7) / 8 * 8); };
        w = q(w); h = q(h);
        std::string key = make_key(path, w, h, blur, cover, radius);
        auto it = map_.find(key);
        if (it != map_.end()) {
            it->second->last_used = SDL_GetTicks();
            return it->second;
        }
        auto a = std::make_shared<Asset>();
        a->key = key; a->path = path; a->w = w; a->h = h; a->blur = blur; a->cover = cover; a->radius = radius;
        a->last_used = SDL_GetTicks();
        map_[key] = a;
        pending_++;
        {
            std::lock_guard l(mu_);
            queue_.push_back(a);
        }
        cv_.notify_one();
        return a;
    }

    void AssetCache::worker()
    {
        while (true) {
            AssetHandle a;
            {
                std::unique_lock l(mu_);
                cv_.wait(l, [&] { return stop_ || !queue_.empty(); });
                if (stop_) return;
                a = queue_.front();
                queue_.pop_front();
            }
            // Skip work nobody wants any more (only the cache holds it).
            if (a.use_count() <= 2 && SDL_GetTicks() - a->last_used > 30000) {
                a->state = -1;
                pending_--;
                continue;
            }
            SDL_Surface *s = decode(a->path, a->w, a->h, a->blur, a->cover, a->radius);
            {
                std::lock_guard l(mu_);
                if (s) { a->surface = s; a->state = 1; decoded_.push_back(a); }
                else { a->state = -1; pending_--; }
            }
        }
    }

    bool AssetCache::pump(int max_uploads)
    {
        std::vector<AssetHandle> ready;
        {
            std::lock_guard l(mu_);
            while (!decoded_.empty() && (int) ready.size() < max_uploads) {
                ready.push_back(decoded_.front());
                decoded_.pop_front();
            }
        }
        for (auto &a : ready) {
            SDL_Texture *t = SDL_CreateTextureFromSurface(r_, a->surface);
            if (t) {
                SDL_SetTextureBlendMode(t, SDL_BLENDMODE_BLEND);
                SDL_SetTextureScaleMode(t, SDL_SCALEMODE_LINEAR);
                a->texture = t;
                a->tex_w = a->surface->w;
                a->tex_h = a->surface->h;
                a->bytes = (size_t) a->tex_w * a->tex_h * 4;
                bytes_ += a->bytes;
                a->state = 2;
            } else {
                a->state = -1;
            }
            SDL_DestroySurface(a->surface);
            a->surface = nullptr;
            pending_--;
        }
        if (!ready.empty()) evict();
        return !ready.empty();
    }

    void AssetCache::evict()
    {
        if (bytes_ <= budget_) return;
        std::vector<AssetHandle> cands;
        for (auto &[k, a] : map_)
            if (a.use_count() == 1 && a->state.load() == 2) cands.push_back(a);
        std::sort(cands.begin(), cands.end(), [](auto &x, auto &y) { return x->last_used < y->last_used; });
        for (auto &a : cands) {
            if (bytes_ <= budget_ * 3 / 4) break;
            bytes_ -= a->bytes;
            map_.erase(a->key);
        }
        // also drop failed / unreferenced queued entries
        for (auto it = map_.begin(); it != map_.end();) {
            if (it->second.use_count() == 1 && it->second->failed()) it = map_.erase(it);
            else ++it;
        }
    }

    void AssetCache::clear()
    {
        {
            std::lock_guard l(mu_);
            queue_.clear();
            decoded_.clear();
        }
        map_.clear();
        bytes_ = 0;
        pending_ = 0;
    }

    // ---- decode ------------------------------------------------------------------
    static SDL_Surface* to_rgba(SDL_Surface *s)
    {
        if (!s) return nullptr;
        if (s->format == SDL_PIXELFORMAT_RGBA32) return s;
        SDL_Surface *o = SDL_ConvertSurface(s, SDL_PIXELFORMAT_RGBA32);
        SDL_DestroySurface(s);
        return o;
    }

    static void fit_box(int sw, int sh, int bw, int bh, bool cover, int &ow, int &oh)
    {
        if (bw <= 0 && bh <= 0) { ow = sw; oh = sh; return; }
        if (bw <= 0) bw = (int) std::lround((double) sw * bh / sh);
        if (bh <= 0) bh = (int) std::lround((double) sh * bw / sw);
        double sx = (double) bw / sw, sy = (double) bh / sh;
        double s = cover ? std::max(sx, sy) : std::min(sx, sy);
        ow = std::max(1, (int) std::lround(sw * s));
        oh = std::max(1, (int) std::lround(sh * s));
    }

    static SDL_Surface* crop_center(SDL_Surface *s, int w, int h)
    {
        if (!s || w <= 0 || h <= 0 || (s->w <= w && s->h <= h)) return s;
        w = std::min(w, s->w); h = std::min(h, s->h);
        SDL_Surface *o = SDL_CreateSurface(w, h, SDL_PIXELFORMAT_RGBA32);
        if (!o) return s;
        SDL_Rect src{(s->w - w) / 2, (s->h - h) / 2, w, h};
        SDL_SetSurfaceBlendMode(s, SDL_BLENDMODE_NONE);
        SDL_BlitSurface(s, &src, o, nullptr);
        SDL_DestroySurface(s);
        return o;
    }

    static void round_corners(SDL_Surface *s, float r)
    {
        if (!s || r < 1.f) return;
        int w = s->w, h = s->h;
        r = std::min(r, std::min(w, h) / 2.f);
        for (int y = 0; y < h; y++) {
            Uint8 *row = (Uint8*) s->pixels + y * s->pitch;
            float py = y + 0.5f;
            float dy = py < r ? r - py : (py > h - r ? py - (h - r) : 0.f);
            if (dy <= 0.f) continue;
            for (int x = 0; x < w; x++) {
                float px = x + 0.5f;
                float dx = px < r ? r - px : (px > w - r ? px - (w - r) : 0.f);
                if (dx <= 0.f) continue;
                float d = std::sqrt(dx * dx + dy * dy) - r;
                float cov = std::clamp(0.5f - d, 0.f, 1.f);
                row[x * 4 + 3] = (Uint8) (row[x * 4 + 3] * cov);
            }
        }
    }

    SDL_Surface* AssetCache::decode(const std::string &path, int bw, int bh, float blur, bool cover, float radius)
    {
        SDL_Surface *out = nullptr;
        bool svg = path.size() > 4 && strcasecmp(path.c_str() + path.size() - 4, ".svg") == 0;
        if (svg) {
            NSVGimage *img = nsvgParseFromFile(path.c_str(), "px", 96.0f);
            if (!img || img->width <= 0 || img->height <= 0) {
                if (img) nsvgDelete(img);
                Foyer::logger::warn("Could not parse SVG '{}'", path);
                return nullptr;
            }
            int ow, oh;
            int bw2 = bw, bh2 = bh;
            if (bw2 <= 0 && bh2 <= 0) { bw2 = (int) img->width; bh2 = (int) img->height; }
            fit_box((int) std::ceil(img->width), (int) std::ceil(img->height), bw2, bh2, cover, ow, oh);
            float scale = std::min((float) ow / img->width, (float) oh / img->height);
            if (cover) scale = std::max((float) ow / img->width, (float) oh / img->height);
            out = SDL_CreateSurface(ow, oh, SDL_PIXELFORMAT_RGBA32);
            NSVGrasterizer *rast = nsvgCreateRasterizer();
            if (out && rast) {
                SDL_FillSurfaceRect(out, nullptr, 0);
                nsvgRasterize(rast, img, 0, 0, scale, (unsigned char*) out->pixels, ow, oh, out->pitch);
            }
            if (rast) nsvgDeleteRasterizer(rast);
            nsvgDelete(img);
        } else {
            SDL_Surface *src = to_rgba(IMG_Load(path.c_str()));
            if (!src) {
                Foyer::logger::warn("Could not load image '{}': {}", path, SDL_GetError());
                return nullptr;
            }
            int ow, oh;
            fit_box(src->w, src->h, bw, bh, cover, ow, oh);
            if (ow == src->w && oh == src->h) out = src;
            else {
                out = SDL_CreateSurface(ow, oh, SDL_PIXELFORMAT_RGBA32);
                if (out) SDL_BlitSurfaceScaled(src, nullptr, out, nullptr, SDL_SCALEMODE_LINEAR);
                SDL_DestroySurface(src);
            }
        }
        if (out && cover && bw > 0 && bh > 0) out = crop_center(out, bw, bh);
        if (out && blur > 0.5f) blur_surface(out, blur);
        if (out && cover && radius >= 1.f) round_corners(out, radius);
        return out;
    }

    void AssetCache::blur_surface(SDL_Surface *s, float sigma)
    {
        if (!s || s->format != SDL_PIXELFORMAT_RGBA32) return;
        int w = s->w, h = s->h;
        std::vector<Uint8> a((size_t) w * h * 4), b((size_t) w * h * 4);
        for (int y = 0; y < h; y++)
            memcpy(&a[(size_t) y * w * 4], (Uint8*) s->pixels + y * s->pitch, (size_t) w * 4);
        Uint8 *in = a.data(), *out = b.data();
        fast_gaussian_blur<Uint8>(in, out, w, h, 4, sigma);
        for (int y = 0; y < h; y++)
            memcpy((Uint8*) s->pixels + y * s->pitch, &out[(size_t) y * w * 4], (size_t) w * 4);
    }
}
