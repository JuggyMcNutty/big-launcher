#pragma once

#include <string>
#include <vector>

#include "renderer.hpp"

namespace BL {

    class Font;
    class Texture;

    enum class SettingsPage {
        WIFI,
        BLUETOOTH
    };

    // Full-screen settings UI (WiFi / Bluetooth), drawn on top of the layout
    // while active. Input is routed here from the main loop.
    class SettingsScreen {
        private:
            Renderer *renderer = nullptr;
            Font *title_font = nullptr;
            Font *item_font = nullptr;
            Font *info_font = nullptr;
            Texture *overlay = nullptr;   // dim backdrop (scaled 1x1)
            Texture *bar = nullptr;       // selection highlight (scaled 1x1)
            Texture *title_texture = nullptr;
            Texture *footer_texture = nullptr;
            Texture *message_texture = nullptr;
            Texture *password_texture = nullptr;

            int screen_w = 0;
            int screen_h = 0;
            SettingsPage page = SettingsPage::WIFI;
            bool active = false;
            bool password_mode = false;
            std::string password_ssid;
            std::string input_text;
            std::string message;

            std::vector<std::string> labels;   // display text per row
            std::vector<std::string> actions;  // "wifi:connect:<ssid>\t<flags>", "bt:pair:<mac>", "bt:scan", or ""
            std::vector<Texture*> row_textures;
            int index = 0;
            int view_offset = 0;

            Texture *make_solid_texture(const SDL_Color &color);
            void rebuild_textures();
            void rebuild_texture(Texture *&texture, const std::string &text, Font &font);
            void set_message(const std::string &text);
            void load_page();
            void run_action(const std::string &action);
            void submit_password();
            void clear_rows();

        public:
            SettingsScreen() = default;
            ~SettingsScreen();
            SettingsScreen(const SettingsScreen&) = delete;
            SettingsScreen& operator=(const SettingsScreen&) = delete;

            void open(SettingsPage page, Renderer &renderer, int screen_w, int screen_h);
            void close();
            bool is_active() const { return active; }
            void refresh() { if (active) load_page(); }

            void move_up();
            void move_down();
            void select();
            void text_input(const char *text);
            void backspace();
            void back();

            void draw();
    };
}