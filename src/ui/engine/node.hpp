#pragma once

#include <memory>
#include <vector>
#include <string>
#include <functional>
#include <cmath>

#include <SDL3/SDL.h>

#include "units.hpp"
#include "../../input/action.hpp"

namespace Foyer::ui {
    class Engine;
    class Canvas;

    // ---- layout primitives --------------------------------------------------
    struct Dim {
        enum Kind { Auto, Dp, Pct, Fill } kind = Auto;
        float v = 0.f;
        static Dim dp(float v) { return {Dp, v}; }
        static Dim pct(float p) { return {Pct, p}; }      // 0..100 of parent content box
        static Dim fill() { return {Fill, 0.f}; }          // take the remaining space
        static Dim automatic() { return {Auto, 0.f}; }
    };

    enum class Dir { Row, Column, Stack };
    enum class Align { Start, Center, End, Stretch };
    enum class Justify { Start, Center, End, SpaceBetween };

    struct Edges {
        float l = 0, t = 0, r = 0, b = 0;   // dp
        static Edges all(float v) { return {v, v, v, v}; }
        static Edges xy(float x, float y) { return {x, y, x, y}; }
    };

    struct Style {
        Dir dir = Dir::Column;
        Dim w, h;
        float min_w = 0, min_h = 0;            // dp
        float max_w = 1e9f, max_h = 1e9f;      // dp
        float aspect = 0.f;                    // w / h; derives the missing axis
        float grow = 0.f;                      // share of free main-axis space
        float shrink = 1.f;                    // 0 = never shrink below measured size
        Edges pad;                             // dp
        float gap = 0.f;                       // dp between children
        Align align = Align::Stretch;          // children cross-axis
        Justify justify = Justify::Start;      // children main-axis
        bool has_self_align = false;
        Align self_align = Align::Start;
        bool absolute = false;                 // positioned at (x, y) in parent content box
        float x = 0.f, y = 0.f;                // dp
    };

    struct Size { float w = 0, h = 0; };

    // ---- motion ---------------------------------------------------------------
    // Critically-damped spring. `to()` animates, `snap()` jumps. Globally slowed
    // or disabled via Motion settings (reduce motion = always snap).
    struct Motion {
        static float speed;          // 1 = default; theme `motion.speed`
        static bool reduce;          // accessibility: no animation
    };

    class Animated {
    public:
        Animated(float v = 0.f): value_(v), target_(v) {}
        float get() const { return value_; }
        float target() const { return target_; }
        operator float() const { return value_; }
        void to(float t);
        void snap(float t);
        bool active() const { return active_; }
        // Advance by dt seconds. Returns true while still moving.
        bool step(float dt, float stiffness = 260.f);
    private:
        float value_, target_, vel_ = 0.f;
        bool active_ = false;
    };

    // ---- node -----------------------------------------------------------------
    // Base of the retained scene graph. Owns its children. After layout,
    // `frame` holds the border box in px relative to the parent's origin.
    class Node {
    public:
        Node();
        virtual ~Node();
        Node(const Node &) = delete;
        Node &operator=(const Node &) = delete;

        Style style;
        std::string id;

        // visual state (animated)
        Animated scale{1.f}, opacity{1.f}, tx{0.f}, ty{0.f};   // tx/ty in dp
        bool visible = true;
        bool clip = false;

        // focus
        bool focusable = false;
        bool focus_memory = false;     // remember last focused descendant (rows)
        Node *remembered = nullptr;
        std::function<void()> on_activate;
        std::function<void(bool)> on_focus_changed;
        std::function<bool(Action)> on_action;   // bubbles focused -> root

        SDL_FRect frame{};    // px, relative to parent origin

        template <typename T, typename... Args>
        T* add(Args&&... args) {
            auto p = std::make_unique<T>(std::forward<Args>(args)...);
            T *raw = p.get();
            adopt(std::move(p));
            return raw;
        }
        Node* adopt(std::unique_ptr<Node> child);
        Node* insert(size_t index, std::unique_ptr<Node> child);
        size_t index_of(const Node *child) const;
        void clear_children();
        void remove_child(Node *child);
        Node* parent() const { return parent_; }
        const std::vector<std::unique_ptr<Node>>& children() const { return children_; }
        bool contains(const Node *n) const;
        bool focused() const;
        bool focus_within() const;
        Node* find(const std::string &id);

        // Leaf content size in px given available space (no padding).
        virtual Size measure(Engine &e, float avail_w, float avail_h);
        // Scroll offset applied to children (px).
        virtual float scroll_x() const { return 0.f; }
        virtual float scroll_y() const { return 0.f; }
        // Scrollers: children keep their natural main-axis size (never shrink).
        virtual bool unbounded_main() const { return false; }
        virtual void paint(Canvas &c, const SDL_FRect &r, float alpha) { (void) c; (void) r; (void) alpha; }
        virtual void paint_over(Canvas &c, const SDL_FRect &r, float alpha) { (void) c; (void) r; (void) alpha; }
        // Per-frame update; return true to keep animating.
        virtual bool tick(Engine &e, float dt);
        // Called after this node's children were arranged.
        virtual void did_layout(Engine &e) { (void) e; }

    protected:
        Node *parent_ = nullptr;
        std::vector<std::unique_ptr<Node>> children_;
    };

    // ---- layout ----------------------------------------------------------------
    Size measure_node(Engine &e, Node &n, float avail_w, float avail_h);
    void arrange_node(Engine &e, Node &n, float x, float y, float w, float h);

    // Absolute on-screen rect of a node, including scroll offsets and tx/ty
    // but not scale. Used by focus navigation and scrollers.
    SDL_FRect screen_rect(Engine &e, const Node &n);

    // Global "something changed, redraw" flag (UI thread only).
    void invalidate();
    bool is_invalid();
    void clear_invalid();
    void invalidate_layout();
    bool layout_invalid();
    void clear_layout_invalid();
}
