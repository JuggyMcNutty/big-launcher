#pragma once

#include <string>
#include <map>
#include <memory>
#include <list>
#include <unordered_map>

#include <SDL3/SDL.h>

#include "node.hpp"

struct TTF_Font;

namespace Foyer::ui {
    // Fonts are opened per (file, px size). Glyph textures are cached per
    // (font, px, text, wrap) with an LRU bound; at a mode change everything is
    // dropped and re-rasterised at the new pixel size (always crisp).
    class FontCache {
    public:
        ~FontCache();
        TTF_Font* get(const std::string &file, int px);
        void clear();
    private:
        std::map<std::pair<std::string, int>, TTF_Font*> fonts_;
    };

    struct TextTexture {
        SDL_Texture *tex = nullptr;
        int w = 0, h = 0;
        ~TextTexture();
    };

    class TextCache {
    public:
        explicit TextCache(size_t max_entries = 600): max_(max_entries) {}
        // Returns a texture (owned by the cache) or nullptr for empty text.
        std::shared_ptr<TextTexture> get(SDL_Renderer *r, FontCache &fc, const std::string &font, int px,
                                         const std::string &text, int wrap_px, int max_lines, bool center);
        void clear() { map_.clear(); lru_.clear(); }
        size_t size() const { return map_.size(); }
    private:
        size_t max_;
        std::list<std::string> lru_;
        std::unordered_map<std::string, std::pair<std::shared_ptr<TextTexture>, std::list<std::string>::iterator>> map_;
    };

    enum class TextRole { Xs, Sm, Md, Lg, Xl, Hero };
    enum class TextAlign { Left, Center, Right };

    class Text : public Node {
    public:
        Text(std::string s = "", TextRole role = TextRole::Md);
        void set(const std::string &s);
        const std::string& str() const { return text_; }

        TextRole role;
        TextAlign align = TextAlign::Left;
        bool display_font = false;
        bool dim = false;             // use text_dim instead of text
        bool has_color = false;
        SDL_Color color{255, 255, 255, 255};
        int max_lines = 1;            // 0 = unlimited; overflow -> ellipsis
        float size_dp = 0.f;          // override role size

        Size measure(Engine &e, float avail_w, float avail_h) override;
        void paint(Canvas &c, const SDL_FRect &r, float alpha) override;
        int px_size(Engine &e) const;

    private:
        std::string text_;
        Engine *engine_ = nullptr;
        std::shared_ptr<TextTexture> cached_;
        std::string cache_key_;
        std::shared_ptr<TextTexture> texture_for(Engine &e, int wrap_px);
    };
}
