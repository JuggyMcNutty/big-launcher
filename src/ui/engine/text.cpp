#include <algorithm>
#include <cmath>

#include <SDL3_ttf/SDL_ttf.h>

#include "text.hpp"
#include "engine.hpp"
#include "canvas.hpp"
#include "../../render/util.hpp"
#include "../../render/logger.hpp"

namespace Foyer::ui {
    // ---- FontCache ---------------------------------------------------------------
    FontCache::~FontCache() { clear(); }

    void FontCache::clear()
    {
        for (auto &[k, f] : fonts_) if (f) TTF_CloseFont(f);
        fonts_.clear();
    }

    TTF_Font* FontCache::get(const std::string &file, int px)
    {
        px = std::clamp(px, 6, 400);
        auto key = std::make_pair(file, px);
        auto it = fonts_.find(key);
        if (it != fonts_.end()) return it->second;
        std::string path = file;
        if (path.find('/') == std::string::npos)
            path = Foyer::find_file<Foyer::FileType::FONT>(file.c_str());
        TTF_Font *f = path.empty() ? nullptr : TTF_OpenFont(path.c_str(), (float) px);
        if (!f && file != "Inter-Medium.ttf") {
            Foyer::logger::warn("Font '{}' unavailable ({}), falling back to Inter", file, SDL_GetError());
            f = get("Inter-Medium.ttf", px);
            fonts_[key] = f;
            return f;   // alias; closed via the Inter entry
        }
        if (!f) Foyer::logger::error("Could not open any font: {}", SDL_GetError());
        fonts_[key] = f;
        return f;
    }

    TextTexture::~TextTexture() { if (tex) SDL_DestroyTexture(tex); }

    // ---- text helpers ------------------------------------------------------------
    static std::string ellipsize(TTF_Font *f, const std::string &s, int max_w)
    {
        int w = 0;
        TTF_GetStringSize(f, s.c_str(), s.size(), &w, nullptr);
        if (w <= max_w || max_w <= 0) return s;
        const std::string dots = "\xE2\x80\xA6";
        int dw = 0;
        TTF_GetStringSize(f, dots.c_str(), dots.size(), &dw, nullptr);
        size_t fit = 0;
        int mw = 0;
        TTF_MeasureString(f, s.c_str(), s.size(), std::max(0, max_w - dw), &mw, &fit);
        // don't split a UTF-8 sequence
        while (fit > 0 && fit < s.size() && ((unsigned char) s[fit] & 0xC0) == 0x80) fit--;
        std::string out = s.substr(0, fit);
        while (!out.empty() && out.back() == ' ') out.pop_back();
        return out + dots;
    }

    // Greedy word wrap into at most max_lines (0 = unlimited) lines.
    static std::vector<std::string> wrap_lines(TTF_Font *f, const std::string &s, int wrap, int max_lines)
    {
        std::vector<std::string> lines;
        std::string cur;
        auto width = [&](const std::string &t) { int w = 0; TTF_GetStringSize(f, t.c_str(), t.size(), &w, nullptr); return w; };
        size_t i = 0;
        std::vector<std::string> paragraphs;
        {
            std::string p;
            for (char ch : s) { if (ch == '\n') { paragraphs.push_back(p); p.clear(); } else p += ch; }
            paragraphs.push_back(p);
        }
        (void) i;
        for (const auto &para : paragraphs) {
            cur.clear();
            size_t pos = 0;
            bool any = false;
            while (pos <= para.size()) {
                size_t sp = para.find(' ', pos);
                std::string word = para.substr(pos, sp == std::string::npos ? std::string::npos : sp - pos);
                std::string trial = cur.empty() ? word : cur + " " + word;
                if (!cur.empty() && width(trial) > wrap) {
                    lines.push_back(cur);
                    cur = word;
                } else {
                    cur = trial;
                }
                any = true;
                if (sp == std::string::npos) break;
                pos = sp + 1;
            }
            if (any) lines.push_back(cur);
        }
        if (max_lines > 0 && (int) lines.size() > max_lines) {
            std::string rest;
            for (size_t k = max_lines - 1; k < lines.size(); k++) rest += (rest.empty() ? "" : " ") + lines[k];
            lines.resize(max_lines);
            lines.back() = ellipsize(f, rest, wrap);
        } else {
            for (auto &l : lines) l = ellipsize(f, l, wrap);
        }
        return lines;
    }

    std::shared_ptr<TextTexture> TextCache::get(SDL_Renderer *r, FontCache &fc, const std::string &font, int px,
                                                const std::string &text, int wrap, int max_lines, bool center)
    {
        if (text.empty()) return nullptr;
        std::string key = font + '\x1f' + std::to_string(px) + '\x1f' + std::to_string(wrap) + '\x1f' +
                          std::to_string(max_lines) + (center ? "c" : "l") + '\x1f' + text;
        auto it = map_.find(key);
        if (it != map_.end()) {
            lru_.splice(lru_.begin(), lru_, it->second.second);
            return it->second.first;
        }
        TTF_Font *f = fc.get(font, px);
        if (!f) return nullptr;
        SDL_Color white{255, 255, 255, 255};
        SDL_Surface *s = nullptr;
        if (max_lines == 1 || wrap <= 0) {
            std::string t = wrap > 0 ? ellipsize(f, text, wrap) : text;
            s = TTF_RenderText_Blended(f, t.c_str(), t.size(), white);
        } else {
            auto lines = wrap_lines(f, text, wrap, max_lines);
            std::string joined;
            for (size_t k = 0; k < lines.size(); k++) joined += (k ? "\n" : "") + lines[k];
            TTF_SetFontWrapAlignment(f, center ? TTF_HORIZONTAL_ALIGN_CENTER : TTF_HORIZONTAL_ALIGN_LEFT);
            s = TTF_RenderText_Blended_Wrapped(f, joined.c_str(), joined.size(), white, wrap);
        }
        if (!s) return nullptr;
        auto tt = std::make_shared<TextTexture>();
        tt->tex = SDL_CreateTextureFromSurface(r, s);
        tt->w = s->w; tt->h = s->h;
        SDL_DestroySurface(s);
        if (tt->tex) {
            SDL_SetTextureBlendMode(tt->tex, SDL_BLENDMODE_BLEND);
            SDL_SetTextureScaleMode(tt->tex, SDL_SCALEMODE_LINEAR);
        }
        lru_.push_front(key);
        map_[key] = {tt, lru_.begin()};
        while (map_.size() > max_) {
            map_.erase(lru_.back());
            lru_.pop_back();
        }
        return tt;
    }

    // ---- Text node ---------------------------------------------------------------
    Text::Text(std::string s, TextRole r): role(r), text_(std::move(s)) {}

    void Text::set(const std::string &s)
    {
        if (s == text_) return;
        text_ = s;
        cached_.reset();
        invalidate_layout();
    }

    int Text::px_size(Engine &e) const
    {
        const Tokens &t = e.tokens();
        float dp = size_dp;
        if (dp <= 0.f) {
            switch (role) {
                case TextRole::Xs: dp = t.text_xs; break;
                case TextRole::Sm: dp = t.text_sm; break;
                case TextRole::Md: dp = t.text_md; break;
                case TextRole::Lg: dp = t.text_lg; break;
                case TextRole::Xl: dp = t.text_xl; break;
                case TextRole::Hero: dp = t.text_hero; break;
            }
        }
        return std::max(6, e.metrics().dpi(dp));
    }

    std::shared_ptr<TextTexture> Text::texture_for(Engine &e, int wrap_px)
    {
        engine_ = &e;
        const std::string &font = display_font ? e.tokens().font_display : e.tokens().font;
        return e.text_cache().get(e.sdl(), e.fonts(), font, px_size(e), text_, wrap_px, max_lines,
                                  align == TextAlign::Center);
    }

    Size Text::measure(Engine &e, float avail_w, float avail_h)
    {
        (void) avail_h;
        if (text_.empty()) {
            TTF_Font *f = e.fonts().get(e.tokens().font, px_size(e));
            return {0.f, f ? (float) TTF_GetFontHeight(f) : 0.f};
        }
        int wrap = std::isfinite(avail_w) ? std::max(1, (int) avail_w) : 0;
        if (style.w.kind == Dim::Dp) wrap = e.metrics().dpi(style.w.v) - (int) e.metrics().dp(style.pad.l + style.pad.r);
        auto t = texture_for(e, wrap);
        return t ? Size{(float) t->w, (float) t->h} : Size{0.f, 0.f};
    }

    void Text::paint(Canvas &c, const SDL_FRect &r0, float alpha)
    {
        (void) alpha;
        if (text_.empty() || !engine_) return;
        Engine &e = *engine_;
        int wrap = std::max(1, (int) (r0.w - e.metrics().dp(style.pad.l + style.pad.r)));
        auto t = texture_for(e, wrap);
        if (!t || !t->tex) return;
        SDL_Color col = has_color ? color : (dim ? e.tokens().text_dim : e.tokens().text);
        const auto &m = e.metrics();
        SDL_FRect r{r0.x + m.dp(style.pad.l), r0.y + m.dp(style.pad.t),
                    r0.w - m.dp(style.pad.l + style.pad.r), r0.h - m.dp(style.pad.t + style.pad.b)};
        float x = r.x;
        if (align == TextAlign::Center) x = r.x + (r.w - t->w) / 2;
        else if (align == TextAlign::Right) x = r.x + r.w - t->w;
        float y = r.y + (r.h - t->h) / 2;
        c.texture(t->tex, nullptr, {std::round(x), std::round(y), (float) t->w, (float) t->h}, col);
    }
}
