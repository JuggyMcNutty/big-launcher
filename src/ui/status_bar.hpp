#pragma once

#include <string>

#include "../render/renderer.hpp"
#include "text_sprite.hpp"

namespace Foyer {
    class Theme;

    // Top status bar: network (left) + audio (right). Drawn by App on top of
    // the home screen (hidden while an app runs). Updated by App via jobs.
    class StatusBar {
    public:
        void set_network(const std::string &s) { network_ = s; net_dirty_ = true; }
        void set_audio(const std::string &s) { audio_ = s; audio_dirty_ = true; }
        void draw(Renderer &r, Theme &t, float w, float h);

    private:
        std::string network_, audio_;
        TextSprite net_sprite_, audio_sprite_;
        bool net_dirty_ = true, audio_dirty_ = true;
    };
}
