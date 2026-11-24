#pragma once

#include <string>

#include "drawable.hpp"

namespace BL {
    class Font;
    class Clock : public Drawable {
        private:
            Font *font = nullptr;
            std::string last_rendered;
            int max_width = 0;
            std::string get_time_string() const;
        public:
            Clock(int font_height, int max_width);
            ~Clock();
            void render_surface();
            void render_texture();
            void update();
            void set_text_color(const SDL_Color &color);
    };
}