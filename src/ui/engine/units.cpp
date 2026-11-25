#include <algorithm>

#include "units.hpp"

const char* Foyer::breakpoint_name(Breakpoint bp)
{
    switch (bp) {
        case Breakpoint::XS: return "xs";
        case Breakpoint::SM: return "sm";
        case Breakpoint::MD: return "md";
        case Breakpoint::LG: return "lg";
    }
    return "md";
}

Foyer::Breakpoint Foyer::Metrics::breakpoint_for(int short_edge_px)
{
    if (short_edge_px < 600) return Breakpoint::XS;
    if (short_edge_px < 900) return Breakpoint::SM;
    if (short_edge_px < 1600) return Breakpoint::MD;
    return Breakpoint::LG;
}

float Foyer::Metrics::density_boost(Breakpoint bp)
{
    switch (bp) {
        case Breakpoint::XS: return 1.35f;
        case Breakpoint::SM: return 1.10f;
        default: return 1.0f;
    }
}

void Foyer::Metrics::configure(int width_px, int height_px, int ui_scale_pct, int overscan_pct)
{
    w_ = std::max(1, width_px);
    h_ = std::max(1, height_px);
    int short_edge = std::min(w_, h_);
    bp_ = breakpoint_for(short_edge);
    ui_scale_pct = std::clamp(ui_scale_pct, 50, 250);
    overscan_pct = std::clamp(overscan_pct, 0, 20);
    scale_ = (float) short_edge / 1080.f * (ui_scale_pct / 100.f) * density_boost(bp_);
    float ox = w_ * overscan_pct / 100.f, oy = h_ * overscan_pct / 100.f;
    safe_ = {ox, oy, ox, oy};
}

SDL_FRect Foyer::Metrics::safe_rect() const
{
    return {safe_.left, safe_.top, w_ - safe_.left - safe_.right, h_ - safe_.top - safe_.bottom};
}

float Foyer::Metrics::safe_width_dp() const { return to_dp(safe_rect().w); }
float Foyer::Metrics::safe_height_dp() const { return to_dp(safe_rect().h); }
