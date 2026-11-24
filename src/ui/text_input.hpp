#pragma once

#include <string>
#include <functional>
#include <vector>

#include "screen.hpp"
#include "text_sprite.hpp"
#include "../render/renderer.hpp"

namespace BL {
    // On-screen keyboard: a modal screen the user types into with the D-pad.
    // Pushed on the screen stack so it grabs all input. On submit (Done key)
    // calls on_submit(text) and pops itself. Back deletes; the final Back
    // (when text is empty and cancel is allowed) cancels.
    class TextInput : public Screen {
    public:
        // on_submit is called with the typed text when the user presses Done.
        // on_cancel is called when the user cancels (optional).
        TextInput(App &app, std::string title,
                  std::function<void(std::string)> on_submit,
                  std::function<void()> on_cancel = nullptr);

        void on_show() override;
        void on_hide() override;
        void draw() override;
        bool on_action(Action action) override;

        void set_masked(bool m) { masked_ = m; rebuild_text(); }
        void set_initial(const std::string &t) { text_ = t; rebuild_text(); }

    private:
        struct Key { std::string label; std::string value; bool special = false; };
        enum Special { None, Backspace, Shift, Space, Done };

        std::vector<std::vector<Key>> rows_;
        int cur_row_ = 0, cur_col_ = 0;
        bool shifted_ = false;
        bool masked_ = false;

        std::string title_;
        std::string text_;
        std::function<void(std::string)> on_submit_;
        std::function<void()> on_cancel_;

        TextSprite title_sprite_;
        TextSprite text_sprite_;
        TextSprite hint_sprite_;
        std::vector<TextSprite> key_sprites_;   // flattened, parallel to rows_
        TexturePtr key_hl_;

        void build_rows();
        void rebuild_text();
        void type_key(const Key &k);
        void move(int dr, int dc);
        int rows_count() const { return (int) rows_.size(); }
        int cols_in_row(int r) const { return rows_[r].size(); }
    };
}
