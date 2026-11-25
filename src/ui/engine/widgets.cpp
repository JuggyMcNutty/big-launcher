#include <algorithm>
#include <cmath>

#include "widgets.hpp"
#include "engine.hpp"
#include "canvas.hpp"

namespace Foyer::ui {
    // ---- Box ---------------------------------------------------------------------
    void Box::paint(Canvas &c, const SDL_FRect &r, float alpha)
    {
        (void) alpha;
        if (!engine_) return;
        const auto &m = engine_->metrics();
        float rad = m.dp(radius);
        if (shadow_blur > 0.f && shadow_color.a) {
            SDL_FRect sr{r.x, r.y + m.dp(shadow_y), r.w, r.h};
            c.shadow(sr, rad, m.dp(shadow_blur), shadow_color);
        }
        if (has_bg && bg.a) c.rounded(r, rad, bg);
        if (border_w > 0.f && border.a) c.border(r, rad, m.dp(border_w), border);
    }

    // ---- Image -------------------------------------------------------------------
    void Image::set_path(const std::string &p)
    {
        if (p == path_) return;
        path_ = p;
        handle_.reset();
        shown_ = false;
        fade_.snap(0.f);
        invalidate();
    }

    Size Image::measure(Engine &e, float aw, float ah)
    {
        (void) e;
        // Images size from style; natural size only if fully auto and loaded.
        if (handle_ && handle_->ready()) {
            float w = (float) handle_->tex_w, h = (float) handle_->tex_h;
            if (std::isfinite(aw) && w > aw) { h *= aw / w; w = aw; }
            if (std::isfinite(ah) && h > ah) { w *= ah / h; h = ah; }
            return {w, h};
        }
        return {0.f, 0.f};
    }

    bool Image::tick(Engine &e, float dt)
    {
        engine_ = &e;
        bool a = Node::tick(e, dt);
        int w = (int) std::lround(frame.w), h = (int) std::lround(frame.h);
        float bpx = e.metrics().dp(blur);
        if (!path_.empty() && w > 0 && h > 0 &&
            (!handle_ || std::abs(w - req_w_) > 8 || std::abs(h - req_h_) > 8 || bpx != req_blur_)) {
            // Blurred backgrounds are decoded at reduced resolution (cheap + softer).
            float ds = bpx > 0.5f ? 0.25f : 1.f;
            int rw = std::max(8, (int) (w * ds)), rh = std::max(8, (int) (h * ds));
            auto next = e.assets().request(path_, rw, rh, bpx * ds, fit == Fit::Cover);
            if (!handle_ || !next->ready()) {
                // keep showing the old texture until the new one is ready
                if (!handle_) handle_ = next;
                else pending_swap_ = next;
            } else {
                handle_ = next;
            }
            req_w_ = w; req_h_ = h; req_blur_ = bpx;
        }
        if (pending_swap_ && pending_swap_->ready()) { handle_ = pending_swap_; pending_swap_.reset(); invalidate(); }
        if (pending_swap_ && pending_swap_->failed()) pending_swap_.reset();
        if (handle_ && handle_->ready() && !shown_) {
            shown_ = true;
            fade_.to(1.f);
        }
        if (handle_ && handle_->ready()) handle_->last_used = SDL_GetTicks();
        a |= fade_.step(dt, 180.f);
        return a;
    }

    void Image::paint(Canvas &c, const SDL_FRect &r, float alpha)
    {
        (void) alpha;
        if (!handle_ || !handle_->ready()) return;
        float tw = (float) handle_->tex_w, th = (float) handle_->tex_h;
        SDL_FRect src{0, 0, tw, th}, dst = r;
        if (fit == Fit::Contain) {
            float s = std::min(r.w / tw, r.h / th);
            dst.w = tw * s; dst.h = th * s;
            dst.x = r.x + (r.w - dst.w) / 2; dst.y = r.y + (r.h - dst.h) / 2;
        } else if (fit == Fit::Cover) {
            float s = std::max(r.w / tw, r.h / th);
            float sw = r.w / s, sh = r.h / s;
            src = {(tw - sw) / 2, (th - sh) / 2, sw, sh};
        }
        SDL_Color mod = has_tint ? tint : SDL_Color{255, 255, 255, 255};
        c.push_alpha(fade_.get());
        c.texture(handle_->texture, &src, dst, mod);
        c.pop_alpha();
    }

    // ---- Button ------------------------------------------------------------------
    Button::Button(const std::string &l, const std::string &v)
    {
        style.dir = Dir::Row;
        style.align = Align::Center;
        style.pad = Edges::xy(28, 16);
        style.gap = 20;
        style.min_h = 64;
        focusable = true;
        radius = 14;
        label = add<Text>(l, TextRole::Md);
        label->style.grow = 1.f;
        label->style.shrink = 1.f;
        value = add<Text>(v, TextRole::Md);
        value->dim = true;
        value->align = TextAlign::Right;
        value->style.shrink = 0.f;
        value->visible = !v.empty();
    }

    bool Button::tick(Engine &e, float dt)
    {
        bool a = Box::tick(e, dt);
        hl_.to(focused() ? 1.f : 0.f);
        a |= hl_.step(dt, 320.f);
        label->has_color = true;
        const Tokens &t = e.tokens();
        SDL_Color txt = enabled ? t.text : t.text_dim;
        label->color = mix(txt, t.on_accent, hl_.get());
        value->has_color = true;
        value->color = mix(t.text_dim, t.on_accent, hl_.get() * 0.85f);
        if (sub) { sub->has_color = true; sub->color = mix(t.text_dim, t.on_accent, hl_.get() * 0.8f); }
        return a;
    }

    void Button::paint(Canvas &c, const SDL_FRect &r, float alpha)
    {
        if (!engine_) return;
        const Tokens &t = engine_->tokens();
        float h = hl_.get();
        has_bg = true;
        bg = mix(with_alpha(t.surface, has_bg_base_ ? t.surface.a : 0), t.accent, h);
        if (!has_bg_base_) bg.a = (Uint8) (t.accent.a * h);
        shadow_blur = 22.f * h;
        shadow_y = 8.f * h;
        shadow_color = with_alpha(t.shadow, (Uint8) (t.shadow.a * h));
        Box::paint(c, r, alpha);
    }
}
