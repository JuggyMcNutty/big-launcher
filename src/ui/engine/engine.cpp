#include <algorithm>
#include <cmath>

#include <SDL3_image/SDL_image.h>

#include "engine.hpp"
#include "focus.hpp"
#include "../../render/logger.hpp"

namespace Foyer::ui {
    Engine::Engine(SDL_Renderer *r, int w, int h): r_(r), canvas_(r)
    {
        metrics_.configure(w, h);
        assets_ = std::make_unique<AssetCache>(r);
    }

    Engine::~Engine()
    {
        Focus::set(nullptr, false);
        Focus::set_scope(nullptr);
        layers_.clear();
        dying_.clear();
        text_.clear();
        fonts_.clear();
        assets_.reset();
    }

    void Engine::configure(int w, int h, int ui_scale, int overscan)
    {
        Metrics m;
        m.configure(w, h, ui_scale, overscan);
        if (m == metrics_) return;
        metrics_ = m;
        Foyer::logger::info("UI metrics: {}x{} scale {:.2f} breakpoint {}", w, h, m.scale(), breakpoint_name(m.breakpoint()));
        text_.clear();
        fonts_.clear();
        rebuild_all();
    }

    void Engine::set_tokens(const Tokens &t)
    {
        tokens_ = t;
        Motion::speed = std::clamp(t.motion_speed, 0.25f, 4.f);
        Motion::reduce = t.reduce_motion;
        text_.clear();
        fonts_.clear();
        rebuild_all();
    }

    void Engine::rebuild_all()
    {
        // Rebuild every layer, preserving focus by node id where possible.
        std::string focus_id = Focus::current() ? Focus::current()->id : "";
        for (auto &l : layers_) {
            l->saved_focus = nullptr;
            l->initial_focus = nullptr;
            Focus::set(nullptr, false);
            l->root = std::make_unique<Node>();
            l->build(*this);
        }
        if (Layer *t = top()) {
            Node *f = focus_id.empty() ? nullptr : t->root->find(focus_id);
            activate_layer(t);
            if (f) Focus::set(f, false);
        }
        invalidate_layout();
    }

    void Engine::layout_layer(Layer &l)
    {
        SDL_FRect s = metrics_.safe_rect();
        l.root->style.w = Dim::dp(metrics_.to_dp(s.w));
        l.root->style.h = Dim::dp(metrics_.to_dp(s.h));
        arrange_node(*this, *l.root, s.x, s.y, s.w, s.h);
    }

    void Engine::activate_layer(Layer *l)
    {
        Focus::set_scope(l->root.get());
        Node *f = l->saved_focus;
        if (!f || !l->root->contains(f)) f = l->initial_focus;
        if (!f || !l->root->contains(f)) f = Focus::first_in(l->root.get());
        Focus::set(f, false);
        if (f && f->on_focus_changed) f->on_focus_changed(true);
    }

    Layer* Engine::push(std::unique_ptr<Layer> l, bool animate)
    {
        if (Layer *t = top()) {
            t->saved_focus = Focus::current();
            t->on_leave(*this);
        }
        l->build(*this);
        Layer *raw = l.get();
        layers_.push_back(std::move(l));
        if (animate && !Motion::reduce) raw->enter.snap(0.f), raw->enter.to(1.f);
        else raw->enter.snap(1.f);
        layout_layer(*raw);
        activate_layer(raw);
        raw->on_enter(*this);
        invalidate_layout();
        return raw;
    }

    void Engine::pop(bool animate)
    {
        if (layers_.size() <= 1) return;
        auto l = std::move(layers_.back());
        layers_.pop_back();
        l->on_leave(*this);
        Focus::set(nullptr, false);
        if (animate && !Motion::reduce) {
            l->closing = true;
            l->enter.to(0.f);
            dying_.push_back(std::move(l));
        }
        if (Layer *t = top()) {
            activate_layer(t);
            t->on_enter(*this);
        }
        invalidate_layout();
    }

    void Engine::pop_to_root()
    {
        while (layers_.size() > 1) pop(false);
    }

    void Engine::replace_root(std::unique_ptr<Layer> l)
    {
        Focus::set(nullptr, false);
        layers_.clear();
        dying_.clear();
        push(std::move(l), false);
    }

    void Engine::focus(Node *n) { Focus::set(n); }

    bool Engine::dispatch(Action a)
    {
        Layer *t = top();
        if (!t) return false;
        // 1. focused node chain (bubbling)
        for (Node *n = Focus::current(); n; n = n->parent())
            if (n->on_action && n->on_action(a)) { invalidate(); return true; }
        // 2. layer
        if (t->on_action(*this, a)) { invalidate(); return true; }
        // 3. default behaviours
        switch (a) {
            case Action::Up: case Action::Down: case Action::Left: case Action::Right:
                if (Focus::move(*this, a)) { if (on_click) on_click(); return true; }
                return false;
            case Action::Accept:
                if (Node *f = Focus::current(); f && f->on_activate) {
                    if (on_select) on_select();
                    f->on_activate();
                    invalidate();
                    return true;
                }
                return false;
            case Action::Back:
                if (layers_.size() > 1) { pop(); return true; }
                return false;
            default:
                return false;
        }
    }

    bool Engine::dispatch_text(const std::string &txt)
    {
        Layer *t = top();
        return t && t->on_text(*this, txt);
    }

    bool Engine::tick_node(Node &n, float dt)
    {
        bool a = n.tick(*this, dt);
        for (auto &c : n.children())
            if (c->visible) a |= tick_node(*c, dt);
        return a;
    }

    bool Engine::update(float dt)
    {
        bool a = false;
        if (assets_->pump()) { a = true; invalidate(); }
        a |= assets_->busy();
        for (auto &l : layers_) {
            a |= l->enter.step(dt, 300.f);
            a |= l->tick(*this, dt);
            a |= tick_node(*l->root, dt);
        }
        for (auto it = dying_.begin(); it != dying_.end();) {
            bool alive = (*it)->enter.step(dt, 300.f);
            tick_node(*(*it)->root, dt);
            if (!alive || (*it)->enter.get() < 0.02f) it = dying_.erase(it);
            else { a = true; ++it; }
        }
        animating_ = a;
        return a;
    }

    void Engine::layout()
    {
        if (!layout_invalid()) return;
        clear_layout_invalid();
        for (auto &l : layers_) layout_layer(*l);
        for (auto &l : dying_) layout_layer(*l);
    }

    bool Engine::needs_paint() const { return animating_ || is_invalid(); }

    void Engine::paint_node(Node &n, float ox, float oy, float alpha)
    {
        (void) alpha;
        if (!n.visible) return;
        float op = std::clamp(n.opacity.get(), 0.f, 1.f);
        if (op * canvas_.alpha() <= 0.004f) return;
        float x = ox + n.frame.x + metrics_.dp(n.tx);
        float y = oy + n.frame.y + metrics_.dp(n.ty);
        SDL_FRect r{x, y, n.frame.w, n.frame.h};
        float s = n.scale.get();
        bool xf = std::fabs(s - 1.f) > 1e-4f;
        if (xf) canvas_.push_transform(s, x + r.w / 2, y + r.h / 2);
        canvas_.push_alpha(op);
        n.paint(canvas_, r, canvas_.alpha());
        if (n.clip) canvas_.push_clip(r);
        float cx = x - n.scroll_x(), cy = y - n.scroll_y();
        float pad = metrics_.dp(60);
        for (auto &c : n.children()) {
            // cull children fully outside a clipping parent
            if (n.clip) {
                float l = cx + c->frame.x, t = cy + c->frame.y;
                if (l > r.x + r.w + pad || l + c->frame.w < r.x - pad ||
                    t > r.y + r.h + pad || t + c->frame.h < r.y - pad)
                    continue;
            }
            paint_node(*c, cx, cy, canvas_.alpha());
        }
        if (n.clip) canvas_.pop_clip();
        n.paint_over(canvas_, r, canvas_.alpha());
        canvas_.pop_alpha();
        if (xf) canvas_.pop_transform();
    }

    void Engine::paint()
    {
        layout();
        canvas_.clear(tokens_.bg);
        // find the lowest layer that must be drawn
        size_t first = 0;
        for (size_t i = layers_.size(); i-- > 0;)
            if (layers_[i]->opaque && layers_[i]->enter.get() > 0.99f) { first = i; break; }
        auto draw_layer = [&](Layer &l) {
            float t = std::clamp(l.enter.get(), 0.f, 1.f);
            if (l.dim_below)
                canvas_.fill({0, 0, (float) metrics_.width(), (float) metrics_.height()},
                             with_alpha(tokens_.scrim, (Uint8) (tokens_.scrim.a * t)));
            canvas_.push_alpha(t);
            // slight rise + zoom on enter
            float s = 0.97f + 0.03f * t;
            canvas_.push_transform(s, metrics_.width() / 2.f, metrics_.height() / 2.f);
            l.paint_under(*this, canvas_);
            paint_node(*l.root, 0.f, 0.f, canvas_.alpha());
            l.paint_over(*this, canvas_);
            canvas_.pop_transform();
            canvas_.pop_alpha();
        };
        for (size_t i = first; i < layers_.size(); i++) draw_layer(*layers_[i]);
        for (auto &l : dying_) draw_layer(*l);
        clear_invalid();
    }

    bool Engine::screenshot(const std::string &path)
    {
        SDL_Surface *s = SDL_RenderReadPixels(r_, nullptr);
        if (!s) {
            Foyer::logger::error("screenshot: read pixels failed: {}", SDL_GetError());
            return false;
        }
        bool ok = IMG_SavePNG(s, path.c_str());
        SDL_DestroySurface(s);
        if (ok) Foyer::logger::info("screenshot saved to {}", path);
        else Foyer::logger::error("screenshot: save failed: {}", SDL_GetError());
        return ok;
    }
}
