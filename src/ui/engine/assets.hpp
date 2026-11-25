#pragma once

#include <atomic>
#include <condition_variable>
#include <deque>
#include <list>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include <SDL3/SDL.h>

namespace Foyer::ui {
    // One decoded image at a specific pixel size (+ optional blur). Decoding
    // happens on worker threads into an SDL_Surface; the GPU upload happens
    // on the UI thread in AssetCache::pump().
    struct Asset {
        std::string key;
        std::string path;
        int w = 0, h = 0;         // requested box (px); 0 = natural size
        float blur = 0.f;         // px sigma
        bool cover = false;       // rasterise to cover the box (else contain)

        std::atomic<int> state{0};          // 0 queued, 1 decoded (surface), 2 ready (texture), -1 failed
        SDL_Surface *surface = nullptr;     // worker -> UI thread
        SDL_Texture *texture = nullptr;
        int tex_w = 0, tex_h = 0;
        size_t bytes = 0;
        Uint64 last_used = 0;

        bool ready() const { return state.load() == 2 && texture; }
        bool failed() const { return state.load() == -1; }
        ~Asset();
    };
    using AssetHandle = std::shared_ptr<Asset>;

    // LRU texture cache with a byte budget (VRAM-ish). Entries still
    // referenced by nodes are never evicted. Thread pool size defaults to
    // hardware concurrency - 1 (min 1, max 3).
    class AssetCache {
    public:
        AssetCache(SDL_Renderer *r, size_t budget_bytes = 96u << 20, int threads = 0);
        ~AssetCache();
        AssetCache(const AssetCache &) = delete;
        AssetCache &operator=(const AssetCache &) = delete;

        // Request an image at a px box. Returns immediately.
        AssetHandle request(const std::string &path, int w, int h, float blur_px = 0.f, bool cover = false);
        // Upload decoded surfaces (UI thread). Returns true if anything became ready.
        bool pump(int max_uploads = 4);
        void evict();
        void clear();
        void set_budget(size_t bytes) { budget_ = bytes; }

        size_t bytes() const { return bytes_; }
        size_t budget() const { return budget_; }
        size_t count() const { return map_.size(); }
        bool busy() const { return pending_.load() > 0; }

        // Synchronous decode (used by workers and tests): file -> RGBA32 surface.
        static SDL_Surface* decode(const std::string &path, int w, int h, float blur_px, bool cover);
        static void blur_surface(SDL_Surface *s, float sigma);

    private:
        SDL_Renderer *r_;
        size_t budget_;
        size_t bytes_ = 0;
        std::unordered_map<std::string, AssetHandle> map_;
        std::vector<std::thread> workers_;
        std::mutex mu_;
        std::condition_variable cv_;
        std::deque<AssetHandle> queue_;
        std::deque<AssetHandle> decoded_;
        std::atomic<int> pending_{0};
        bool stop_ = false;
        void worker();
    };
}
