#pragma once

#include <string>
#include <memory>

#include "node.hpp"
#include "text.hpp"
#include "assets.hpp"

namespace Foyer::ui {
    // A container with an optional rounded background, border and shadow.
    class Box : public Node {
    public:
        Box() = default;
        explicit Box(Dir d) { style.dir = d; }
        bool has_bg = false;
        SDL_Color bg{0, 0, 0, 0};
        float radius = 0.f;              // dp
        float border_w = 0.f;            // dp
        SDL_Color border{0, 0, 0, 0};
        float shadow_blur = 0.f;         // dp; 0 = none
        float shadow_y = 0.f;            // dp
        SDL_Color shadow_color{0, 0, 0, 0x80};
        void paint(Canvas &c, const SDL_FRect &r, float alpha) override;
    protected:
        Engine *engine_ = nullptr;
        bool tick(Engine &e, float dt) override { engine_ = &e; return Node::tick(e, dt); }
    };

    enum class Fit { Contain, Cover, Fill };

    // Async-loaded image (PNG/JPG/WebP/SVG). Fades in when decoded.
    class Image : public Node {
    public:
        Image() = default;
        explicit Image(std::string path, Fit f = Fit::Contain): fit(f), path_(std::move(path)) {}
        void set_path(const std::string &p);
        const std::string& path() const { return path_; }
        Fit fit = Fit::Contain;
        float radius = 0.f;              // dp; rounded clip via mask texture
        float blur = 0.f;                // dp; >0 requests a blurred variant
        bool has_tint = false;
        SDL_Color tint{255, 255, 255, 255};
        bool loaded() const { return handle_ && handle_->ready(); }
        Size measure(Engine &e, float avail_w, float avail_h) override;
        void paint(Canvas &c, const SDL_FRect &r, float alpha) override;
        bool tick(Engine &e, float dt) override;
    private:
        std::string path_;
        AssetHandle handle_;
        AssetHandle pending_swap_;
        int req_w_ = 0, req_h_ = 0;
        float req_blur_ = 0.f;
        Animated fade_{0.f};
        bool shown_ = false;
        Engine *engine_ = nullptr;
    };

    // Focusable button with label (and optional value on the right). The
    // focus look (accent fill + scale) is driven by the focus engine.
    class Button : public Box {
    public:
        explicit Button(const std::string &label, const std::string &value = "");
        Text *label;
        Text *value;
        Text *sub = nullptr;
        bool tick(Engine &e, float dt) override;
        void paint(Canvas &c, const SDL_FRect &r, float alpha) override;
        bool enabled = true;
        bool has_bg_base_ = false;    // draw the surface colour when unfocused (cards)
        bool base_hi = false;         // ...using surface_hi (buttons on a surface)
        float highlight() const { return hl_.get(); }
    private:
        Animated hl_{0.f};
    };

    // Bar sparkline: fixed-slot rolling buffer (2 s samples), oldest on the
    // left. Values are 0..1; the page owns the scaling.
    class Graph : public Node {
    public:
        static constexpr int kSlots = 30;   // 2 s samples -> 60 s window
        Graph() { style.h = Dim::dp(44); }
        void push(float v01) {
            vals_.push_back(std::min(std::max(v01, 0.f), 1.f));
            if ((int) vals_.size() > kSlots) vals_.erase(vals_.begin());
            invalidate();
        }
        void clear() { vals_.clear(); invalidate(); }
        const std::vector<float>& values() const { return vals_; }
        void paint(Canvas &c, const SDL_FRect &r, float alpha) override;
    protected:
        bool tick(Engine &e, float dt) override { engine_ = &e; return Node::tick(e, dt); }
    private:
        std::vector<float> vals_;
        Engine *engine_ = nullptr;
    };

    // Horizontal/vertical spacer.
    class Spacer : public Node {
    public:
        explicit Spacer(float dp = 0.f) { style.w = Dim::dp(dp); style.h = Dim::dp(dp); }
        static Spacer* fill(Node *parent) {
            auto *s = parent->add<Spacer>();
            s->style.w = Dim::automatic(); s->style.h = Dim::automatic(); s->style.grow = 1.f;
            return s;
        }
    };
}
