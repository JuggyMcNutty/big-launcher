#pragma once

#include <functional>
#include <string>
#include <vector>

#include "kit.hpp"

namespace Foyer {
    class App;

    // On-screen keyboard as a full-width bottom sheet. D-pad/gamepad driven;
    // physical keyboard typing (SDL text input) works at the same time.
    class KeyboardSheet : public ui::Layer {
    public:
        KeyboardSheet(App &app, std::string title, std::string initial, bool mask,
                      std::function<void(const std::string&)> on_submit);
        ~KeyboardSheet() override;
        void build(ui::Engine &e) override;
        bool on_text(ui::Engine &e, const std::string &t) override;
        bool on_action(ui::Engine &e, Action a) override;
    private:
        App &app_;
        SDL_Window *window_ = nullptr;
        std::string title_, text_;
        bool mask_, shift_ = false, symbols_ = false, reveal_ = false;
        std::function<void(const std::string&)> on_submit_;
        ui::Text *field_ = nullptr;
        ui::Node *keys_ = nullptr;
        void refresh();
        void rebuild_keys(ui::Engine &e);
        void press(ui::Engine &e, const std::string &k);
    };

    // Browse a directory for a file (art, wallpapers). Shows images as thumbnails.
    class FilePicker : public Page {
    public:
        FilePicker(App &app, std::string title, std::string dir, std::vector<std::string> exts,
                   std::function<void(const std::string&)> on_pick);
    protected:
        void fill(ui::Engine &e, ui::Node &body) override;
    private:
        std::string dir_;
        std::vector<std::string> exts_;
        std::function<void(const std::string&)> on_pick_;
    };
}
