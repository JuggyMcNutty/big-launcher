#pragma once

#include <memory>
#include <vector>
#include <string>
#include <functional>

#include <SDL3/SDL.h>

#include "units.hpp"
#include "tokens.hpp"
#include "node.hpp"
#include "canvas.hpp"
#include "text.hpp"
#include "assets.hpp"
#include "../../input/action.hpp"

namespace Foyer::ui {
    // A full-screen layer (page, modal, sheet). The top layer receives input
    // and is the focus scope. Layers below a non-opaque layer are drawn too.
    class Layer {
    public:
        virtual ~Layer() = default;
        std::unique_ptr<Node> root = std::make_unique<Node>();
        bool opaque = true;              // hide layers below (perf)
        bool dim_below = false;          // draw a scrim under this layer
        Animated enter{0.f};             // 0 -> 1 transition progress
        Node *initial_focus = nullptr;
        Node *saved_focus = nullptr;

        // Build/rebuild the node tree; called on push and on relayout (mode change).
        virtual void build(class Engine &e) { (void) e; }
        virtual void on_enter(Engine &e) { (void) e; }
        virtual void on_leave(Engine &e) { (void) e; }
        // Return true if handled. Default: nothing.
        virtual bool on_action(Engine &e, Action a) { (void) e; (void) a; return false; }
        virtual bool on_text(Engine &e, const std::string &t) { (void) e; (void) t; return false; }
        virtual bool tick(Engine &e, float dt) { (void) e; (void) dt; return false; }
        // Painted under the node tree (backgrounds) and over it (overlays).
        virtual void paint_under(Engine &e, Canvas &c) { (void) e; (void) c; }
        virtual void paint_over(Engine &e, Canvas &c) { (void) e; (void) c; }
        bool closing = false;
    };

    class Engine {
    public:
        Engine(SDL_Renderer *r, int w, int h);
        ~Engine();

        // configuration
        void configure(int w, int h, int ui_scale_pct, int overscan_pct);   // relayout + rebuild if changed
        void set_tokens(const Tokens &t);
        const Tokens& tokens() const { return tokens_; }
        const Metrics& metrics() const { return metrics_; }
        SDL_Renderer* sdl() const { return r_; }
        Canvas& canvas() { return canvas_; }
        FontCache& fonts() { return fonts_; }
        TextCache& text_cache() { return text_; }
        AssetCache& assets() { return *assets_; }

        // layers
        Layer* push(std::unique_ptr<Layer> l, bool animate = true);
        void pop(bool animate = true);
        void pop_to_root();
        void replace_root(std::unique_ptr<Layer> l);
        Layer* top() const { return layers_.empty() ? nullptr : layers_.back().get(); }
        size_t depth() const { return layers_.size(); }
        void rebuild_all();   // re-run build() (theme/layout change)

        // input
        bool dispatch(Action a);
        bool dispatch_text(const std::string &t);
        void focus(Node *n);

        // frame
        bool update(float dt);           // returns true if anything is animating
        void layout();
        void paint();
        bool needs_paint() const;
        bool screenshot(const std::string &png_path);

        // hooks
        std::function<void()> on_click;   // focus moved (sound)
        std::function<void()> on_select;  // activated (sound)
        // Drawn above every layer (toasts, launch splash). Return true from
        // overlay_active while animating.
        std::function<void(Engine&, Canvas&)> overlay_paint;
        std::function<bool()> overlay_active;

    private:
        SDL_Renderer *r_;
        Metrics metrics_;
        Tokens tokens_;
        Canvas canvas_;
        FontCache fonts_;
        TextCache text_;
        std::unique_ptr<AssetCache> assets_;
        std::vector<std::unique_ptr<Layer>> layers_;
        std::vector<std::unique_ptr<Layer>> dying_;
        bool animating_ = true;

        void paint_node(Node &n, float ox, float oy, float alpha);
        bool tick_node(Node &n, float dt);
        void activate_layer(Layer *l);
        void layout_layer(Layer &l);
    };
}
