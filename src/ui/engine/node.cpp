#include <algorithm>
#include <limits>

#include "node.hpp"
#include "engine.hpp"
#include "focus.hpp"

namespace Foyer::ui {
    float Motion::speed = 1.f;
    bool Motion::reduce = false;

    static bool g_invalid = true;
    static bool g_layout_invalid = true;
    void invalidate() { g_invalid = true; }
    bool is_invalid() { return g_invalid; }
    void clear_invalid() { g_invalid = false; }
    void invalidate_layout() { g_layout_invalid = true; g_invalid = true; }
    bool layout_invalid() { return g_layout_invalid; }
    void clear_layout_invalid() { g_layout_invalid = false; }

    // ---- Animated --------------------------------------------------------------
    void Animated::to(float t)
    {
        if (Motion::reduce) { snap(t); return; }
        if (t == target_ && (active_ || value_ == t)) return;
        target_ = t;
        active_ = value_ != t || vel_ != 0.f;
        invalidate();
    }

    void Animated::snap(float t)
    {
        if (value_ == t && target_ == t && !active_) return;
        value_ = target_ = t;
        vel_ = 0.f;
        active_ = false;
        invalidate();
    }

    bool Animated::step(float dt, float stiffness)
    {
        if (!active_) return false;
        float k = stiffness * Motion::speed * Motion::speed;
        float c = 2.f * std::sqrt(k);            // critical damping
        // semi-implicit Euler in small substeps for stability
        dt = std::min(dt, 0.05f);
        int n = std::max(1, (int) std::ceil(dt / 0.004f));
        float h = dt / n;
        for (int i = 0; i < n; i++) {
            float a = -k * (value_ - target_) - c * vel_;
            vel_ += a * h;
            value_ += vel_ * h;
        }
        float span = std::max(1e-3f, std::fabs(target_) * 1e-3f);
        if (std::fabs(value_ - target_) < span && std::fabs(vel_) < span * 10.f) {
            value_ = target_;
            vel_ = 0.f;
            active_ = false;
        }
        invalidate();
        return active_;
    }

    // ---- Node ------------------------------------------------------------------
    Node::Node() = default;

    Node::~Node()
    {
        // Destroy children while we are still a complete Node so their
        // destructors can safely clear back-references in ancestors.
        children_.clear();
        for (Node *p = parent_; p; p = p->parent_)
            if (p->remembered == this) p->remembered = nullptr;
        Focus::node_destroyed(this);
    }

    Node* Node::adopt(std::unique_ptr<Node> child)
    {
        child->parent_ = this;
        children_.push_back(std::move(child));
        invalidate_layout();
        return children_.back().get();
    }

    void Node::clear_children()
    {
        children_.clear();
        invalidate_layout();
    }

    void Node::remove_child(Node *child)
    {
        auto it = std::find_if(children_.begin(), children_.end(),
                               [&](const auto &p) { return p.get() == child; });
        if (it != children_.end()) {
            children_.erase(it);
            invalidate_layout();
        }
    }

    bool Node::contains(const Node *n) const
    {
        for (; n; n = n->parent_)
            if (n == this) return true;
        return false;
    }

    bool Node::focused() const { return Focus::current() == this; }
    bool Node::focus_within() const { return contains(Focus::current()); }

    Node* Node::find(const std::string &want)
    {
        if (id == want) return this;
        for (auto &c : children_)
            if (Node *f = c->find(want)) return f;
        return nullptr;
    }

    Size Node::measure(Engine &e, float avail_w, float avail_h)
    {
        (void) e; (void) avail_w; (void) avail_h;
        return {0.f, 0.f};
    }

    bool Node::tick(Engine &e, float dt)
    {
        (void) e;
        bool a = scale.step(dt);
        a |= opacity.step(dt);
        a |= tx.step(dt);
        a |= ty.step(dt);
        return a;
    }

    // ---- layout ----------------------------------------------------------------
    static constexpr float UNSET = std::numeric_limits<float>::quiet_NaN();

    static float resolve(const Engine &e, const Dim &d, float avail)
    {
        switch (d.kind) {
            case Dim::Dp:   return e.metrics().dp(d.v);
            case Dim::Pct:  return std::isfinite(avail) ? avail * d.v / 100.f : UNSET;
            case Dim::Fill: return std::isfinite(avail) ? avail : UNSET;
            default:        return UNSET;
        }
    }

    static bool in_flow(const Node &c) { return c.visible && !c.style.absolute; }

    static void clamp_size(const Engine &e, const Style &s, float &w, float &h)
    {
        const auto &m = e.metrics();
        w = std::clamp(w, m.dp(s.min_w), std::max(m.dp(s.min_w), m.dp(s.max_w)));
        h = std::clamp(h, m.dp(s.min_h), std::max(m.dp(s.min_h), m.dp(s.max_h)));
    }

    Size measure_node(Engine &e, Node &n, float aw, float ah)
    {
        const Style &s = n.style;
        const auto &m = e.metrics();
        float pl = m.dp(s.pad.l), pr = m.dp(s.pad.r), pt = m.dp(s.pad.t), pb = m.dp(s.pad.b);
        float w = resolve(e, s.w, aw);
        float h = resolve(e, s.h, ah);
        if (s.aspect > 0.f) {
            if (std::isfinite(w) && !std::isfinite(h)) h = w / s.aspect;
            else if (std::isfinite(h) && !std::isfinite(w)) w = h * s.aspect;
        }
        if (!std::isfinite(w) || !std::isfinite(h)) {
            float cw = (std::isfinite(w) ? w : aw) - pl - pr;
            float ch = (std::isfinite(h) ? h : ah) - pt - pb;
            Size content{0.f, 0.f};
            if (n.children().empty()) {
                content = n.measure(e, cw, ch);
            } else {
                float gap = m.dp(s.gap);
                int count = 0;
                float mw = cw, mh = ch;
                if (n.unbounded_main()) {
                    if (s.dir == Dir::Row) mw = std::numeric_limits<float>::infinity();
                    else mh = std::numeric_limits<float>::infinity();
                }
                for (auto &cp : n.children()) {
                    Node &c = *cp;
                    if (!in_flow(c)) continue;
                    Size cs = measure_node(e, c, mw, mh);
                    if (s.dir == Dir::Row) {
                        content.w += cs.w + (count ? gap : 0.f);
                        content.h = std::max(content.h, cs.h);
                    } else if (s.dir == Dir::Column) {
                        content.h += cs.h + (count ? gap : 0.f);
                        content.w = std::max(content.w, cs.w);
                    } else {
                        content.w = std::max(content.w, cs.w);
                        content.h = std::max(content.h, cs.h);
                    }
                    count++;
                }
                Size own = n.measure(e, cw, ch);   // e.g. text in a button
                content.w = std::max(content.w, own.w);
                content.h = std::max(content.h, own.h);
            }
            if (!std::isfinite(w)) w = content.w + pl + pr;
            if (!std::isfinite(h)) h = content.h + pt + pb;
            if (s.aspect > 0.f) {
                if (s.w.kind == Dim::Auto && s.h.kind != Dim::Auto) w = h * s.aspect;
                else if (s.h.kind == Dim::Auto && s.w.kind != Dim::Auto) h = w / s.aspect;
            }
        }
        clamp_size(e, s, w, h);
        return {w, h};
    }

    void arrange_node(Engine &e, Node &n, float x, float y, float w, float h)
    {
        const Style &s = n.style;
        const auto &m = e.metrics();
        n.frame = {x, y, w, h};
        float pl = m.dp(s.pad.l), pr = m.dp(s.pad.r), pt = m.dp(s.pad.t), pb = m.dp(s.pad.b);
        float cx = pl, cy = pt;
        float cw = std::max(0.f, w - pl - pr), ch = std::max(0.f, h - pt - pb);
        float gap = m.dp(s.gap);

        // Absolutely positioned children
        for (auto &cp : n.children()) {
            Node &c = *cp;
            if (!c.visible || !c.style.absolute) continue;
            Size cs = measure_node(e, c, cw, ch);
            arrange_node(e, c, cx + m.dp(c.style.x), cy + m.dp(c.style.y), cs.w, cs.h);
        }

        auto align_of = [&](const Node &c) { return c.style.has_self_align ? c.style.self_align : s.align; };

        if (s.dir == Dir::Stack) {
            for (auto &cp : n.children()) {
                Node &c = *cp;
                if (!in_flow(c)) continue;
                Size cs = measure_node(e, c, cw, ch);
                Align a = align_of(c);
                float fw = (c.style.w.kind == Dim::Auto && a == Align::Stretch) ? cw : cs.w;
                float fh = (c.style.h.kind == Dim::Auto && a == Align::Stretch) ? ch : cs.h;
                float ox = a == Align::Center ? (cw - fw) / 2 : a == Align::End ? cw - fw : 0.f;
                float oy = a == Align::Center ? (ch - fh) / 2 : a == Align::End ? ch - fh : 0.f;
                arrange_node(e, c, cx + ox, cy + oy, fw, fh);
            }
            n.did_layout(e);
            return;
        }

        bool row = s.dir == Dir::Row;
        float main_avail = row ? cw : ch;
        float cross_avail = row ? ch : cw;

        struct Item { Node *n; float main, cross; float grow; };
        std::vector<Item> items;
        float used = 0.f, grow_total = 0.f;
        for (auto &cp : n.children()) {
            Node &c = *cp;
            if (!in_flow(c)) continue;
            const Dim &md = row ? c.style.w : c.style.h;
            float grow = c.style.grow;
            if (md.kind == Dim::Fill && grow <= 0.f) grow = 1.f;
            float mw = cw, mh = ch;
            if (n.unbounded_main()) {
                if (row) mw = std::numeric_limits<float>::infinity();
                else mh = std::numeric_limits<float>::infinity();
            }
            Size cs = measure_node(e, c, mw, mh);
            float main = md.kind == Dim::Fill ? 0.f : (row ? cs.w : cs.h);
            if (md.kind == Dim::Fill) {
                float minv = m.dp(row ? c.style.min_w : c.style.min_h);
                main = minv;
            }
            items.push_back({&c, main, row ? cs.h : cs.w, grow});
            used += main;
            grow_total += grow;
        }
        if (!items.empty()) used += gap * (float) (items.size() - 1);
        float free = main_avail - used;
        if (free > 0.f && grow_total > 0.f) {
            for (auto &it : items)
                if (it.grow > 0.f) {
                    const Style &cs = it.n->style;
                    float maxv = m.dp(row ? cs.max_w : cs.max_h);
                    it.main = std::min(maxv, it.main + free * it.grow / grow_total);
                }
            free = 0.f;
        } else if (free < 0.f && !n.unbounded_main()) {
            // Shrink items proportionally (scrollers set shrink = 0 on children)
            float shrinkable = 0.f;
            for (auto &it : items) shrinkable += it.main * it.n->style.shrink;
            if (shrinkable > 0.f) {
                float over = std::min(-free, shrinkable);
                for (auto &it : items)
                    it.main -= over * (it.main * it.n->style.shrink / shrinkable);
                free += over;
            }
        }

        float pos = 0.f, between = gap;
        if (free > 0.f) {
            switch (s.justify) {
                case Justify::Center: pos = free / 2; break;
                case Justify::End: pos = free; break;
                case Justify::SpaceBetween:
                    if (items.size() > 1) between = gap + free / (float) (items.size() - 1);
                    break;
                default: break;
            }
        }
        for (auto &it : items) {
            Node &c = *it.n;
            Align a = align_of(c);
            const Dim &cd = row ? c.style.h : c.style.w;
            float cross = it.cross;
            if (cd.kind == Dim::Fill || (cd.kind == Dim::Auto && a == Align::Stretch && c.style.aspect <= 0.f))
                cross = cross_avail;
            float off = a == Align::Center ? (cross_avail - cross) / 2 : a == Align::End ? cross_avail - cross : 0.f;
            if (row) arrange_node(e, c, cx + pos, cy + off, it.main, cross);
            else     arrange_node(e, c, cx + off, cy + pos, cross, it.main);
            pos += it.main + between;
        }
        n.did_layout(e);
    }

    SDL_FRect screen_rect(Engine &e, const Node &n)
    {
        float x = n.frame.x + e.metrics().dp(n.tx), y = n.frame.y + e.metrics().dp(n.ty);
        for (const Node *p = n.parent(); p; p = p->parent()) {
            x += p->frame.x + e.metrics().dp(p->tx) - p->scroll_x();
            y += p->frame.y + e.metrics().dp(p->ty) - p->scroll_y();
        }
        return {x, y, n.frame.w, n.frame.h};
    }
}
