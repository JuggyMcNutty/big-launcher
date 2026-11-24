#include "status_bar.hpp"
#include "../render/theme.hpp"

namespace BL {
    void StatusBar::draw(Renderer &r, Theme &t, float w, float h)
    {
        if (net_dirty_ || !net_sprite_.valid()) {
            net_sprite_.render(r, t.font(t.status_font_size), network_);
            net_dirty_ = false;
        }
        if (audio_dirty_ || !audio_sprite_.valid()) {
            audio_sprite_.render(r, t.font(t.status_font_size), audio_);
            audio_dirty_ = false;
        }
        float bar_h = h * 0.045f;
        float y = h * 0.012f;
        // Subtle background strip
        r.fill_rect({0, 0, w, bar_h + y * 2}, {0x00, 0x00, 0x00, 0x55});
        net_sprite_.draw(r, w * 0.02f, y, t.text_dim);
        audio_sprite_.draw(r, w - audio_sprite_.width() - w * 0.02f, y, t.text_dim);
    }
}
