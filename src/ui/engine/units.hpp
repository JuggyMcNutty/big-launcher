#pragma once

#include <SDL3/SDL.h>

namespace Foyer {
    // Size classes, chosen by the short edge of the render target in px.
    enum class Breakpoint { XS, SM, MD, LG };   // <600, <900, <1600, >=1600

    const char* breakpoint_name(Breakpoint bp);

    struct Insets { float left = 0, top = 0, right = 0, bottom = 0; };

    // Density-independent units. 1 dp == 1 px on a 1080p screen at 100% UI
    // scale. Everything in the UI engine is specified in dp and converted
    // here, so the same layout works from 480p to 4K.
    //
    //   scale = short_edge / 1080 * ui_scale * density_boost(breakpoint)
    //
    // The boost keeps small screens readable (a 480p TV is viewed from the
    // same couch distance as a 4K one, so text can't shrink linearly).
    class Metrics {
    public:
        void configure(int width_px, int height_px, int ui_scale_pct = 100, int overscan_pct = 0);

        float dp(float v) const { return v * scale_; }
        int   dpi(float v) const { return (int) (v * scale_ + 0.5f); }   // rounded px
        float to_dp(float px) const { return scale_ > 0 ? px / scale_ : px; }

        int width() const { return w_; }
        int height() const { return h_; }
        float scale() const { return scale_; }
        Breakpoint breakpoint() const { return bp_; }
        const Insets& safe_insets() const { return safe_; }   // px
        SDL_FRect safe_rect() const;                           // px
        float safe_width_dp() const;
        float safe_height_dp() const;

        static Breakpoint breakpoint_for(int short_edge_px);
        static float density_boost(Breakpoint bp);

        bool operator==(const Metrics &o) const {
            return w_ == o.w_ && h_ == o.h_ && scale_ == o.scale_ &&
                   safe_.left == o.safe_.left && safe_.top == o.safe_.top;
        }

    private:
        int w_ = 1920, h_ = 1080;
        float scale_ = 1.f;
        Breakpoint bp_ = Breakpoint::MD;
        Insets safe_{};
    };
}
