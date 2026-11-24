#include "theme.hpp"
#include "../core/config.hpp"

BL::Font& BL::Theme::font(int size)
{
    std::string key = font_file + "|" + std::to_string(size);
    auto it = fonts.find(key);
    if (it != fonts.end())
        return *it->second;
    auto font = std::make_unique<Font>(font_file, size);
    auto &ref = *font;
    fonts.emplace(std::move(key), std::move(font));
    return ref;
}

BL::Font& BL::Theme::title_font() { return font(title_font_size); }
BL::Font& BL::Theme::item_font()  { return font(item_font_size); }
BL::Font& BL::Theme::info_font()  { return font(info_font_size); }
BL::Font& BL::Theme::status_font(){ return font(status_font_size); }

void BL::Theme::apply_config()
{
    // Called by the app after config is parsed; the app copies config values
    // into this theme. Kept here as a hook for future config-driven theming.
}
