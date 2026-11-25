#pragma once

#include "node.hpp"

namespace Foyer::ui {
    // Clips its children and scrolls smoothly (spring) to keep the focused
    // descendant inside the viewport with `margin` dp of breathing room.
    // Children are laid out at their natural main-axis size (no shrinking).
    class Scroller : public Node {
    public:
        explicit Scroller(Dir d);
        float margin = 48.f;            // dp
        bool anchor_start = false;      // align focused item to the start edge (Big Picture rows)

        float scroll_x() const override { return horizontal() ? offset_.get() : 0.f; }
        float scroll_y() const override { return horizontal() ? 0.f : offset_.get(); }
        bool unbounded_main() const override { return true; }
        bool tick(Engine &e, float dt) override;
        void did_layout(Engine &e) override;
        float content_extent() const { return extent_; }
        float max_offset() const;
        void scroll_to(float px, bool animate = true);
        float offset() const { return offset_.get(); }

    private:
        bool horizontal() const { return style.dir == Dir::Row; }
        Animated offset_{0.f};
        float extent_ = 0.f;
        Node *last_focus_ = nullptr;
        void follow_focus(Engine &e, bool animate);
    };
}
