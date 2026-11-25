#include <algorithm>

#include "scroller.hpp"
#include "engine.hpp"
#include "focus.hpp"

namespace Foyer::ui {
    Scroller::Scroller(Dir d)
    {
        style.dir = d;
        clip = true;
    }

    float Scroller::max_offset() const
    {
        float view = horizontal() ? frame.w : frame.h;
        return std::max(0.f, extent_ - view);
    }

    void Scroller::scroll_to(float px, bool animate)
    {
        px = std::clamp(px, 0.f, max_offset());
        if (animate) offset_.to(px); else offset_.snap(px);
    }

    void Scroller::did_layout(Engine &e)
    {
        // Content extent = furthest child edge (+ trailing padding).
        float ext = 0.f;
        for (auto &c : children()) {
            if (!c->visible) continue;
            ext = std::max(ext, horizontal() ? c->frame.x + c->frame.w : c->frame.y + c->frame.h);
        }
        ext += e.metrics().dp(horizontal() ? style.pad.r : style.pad.b);
        extent_ = ext;
        if (offset_.target() > max_offset()) scroll_to(max_offset(), false);
        follow_focus(e, false);
    }

    void Scroller::follow_focus(Engine &e, bool animate)
    {
        Node *f = Focus::current();
        if (!f || !contains(f) || f == this) return;
        // focused rect in our content coordinates
        float pos = 0.f, size = horizontal() ? f->frame.w : f->frame.h;
        for (const Node *n = f; n && n != this; n = n->parent())
            pos += horizontal() ? n->frame.x : n->frame.y;
        float view = horizontal() ? frame.w : frame.h;
        float m = e.metrics().dp(margin);
        float target = offset_.target();
        if (anchor_start) {
            target = pos - e.metrics().dp(horizontal() ? style.pad.l : style.pad.t);
        } else {
            if (pos - m < target) target = pos - m;
            else if (pos + size + m > target + view) target = pos + size + m - view;
        }
        scroll_to(target, animate);
    }

    bool Scroller::tick(Engine &e, float dt)
    {
        Node *f = Focus::current();
        if (f != last_focus_) {
            last_focus_ = f;
            follow_focus(e, true);
        }
        bool a = Node::tick(e, dt);
        a |= offset_.step(dt, 220.f);
        return a;
    }
}
