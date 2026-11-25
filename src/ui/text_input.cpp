#include "text_input.hpp"
#include "../core/app.hpp"
#include "../render/theme.hpp"
#include "../render/util.hpp"
#include "shapes.hpp"
#include <algorithm>
#include <cctype>

namespace Foyer {
    TextInput::TextInput(App &app, std::string title,
                         std::function<void(std::string)> on_submit,
                         std::function<void()> on_cancel)
        : Screen(app), title_(std::move(title)),
          on_submit_(std::move(on_submit)), on_cancel_(std::move(on_cancel))
    {
        build_rows();
    }

    void TextInput::build_rows()
    {
        rows_.clear();
        auto letter_row = [](const char *s) {
            std::vector<Key> r;
            for (const char *p = s; *p; ++p)
                r.push_back({ std::string(1, *p), std::string(1, *p) });
            return r;
        };
        rows_.push_back(letter_row("qwertyuiop"));
        rows_.push_back(letter_row("asdfghjkl"));
        rows_.push_back(letter_row("zxcvbnm"));
        rows_.push_back({
            { "\u232B", "backspace", true },
            { "\u21E7", "shift", true },
            { "space", "space", true },
            { "done", "done", true },
        });
        cur_row_ = 0;
        cur_col_ = 0;
    }

    void TextInput::rebuild_text()
    {
        auto &r = app_.renderer();
        auto &t = app_.theme();
        std::string shown;
        if (masked_) {
            shown.reserve(text_.size() * 3);
            for (size_t i = 0; i < text_.size(); i++)
                shown += "\xE2\x80\xA2";  // •
        } else shown = text_;
        if (shown.empty()) shown = " ";
        text_sprite_.render(r, t.font((int)(app_.screen_h() * 0.045)), shown);
        // Re-render key sprites to reflect shift state
        int kfs = (int)(app_.screen_h() * 0.03);
        key_sprites_.clear();
        for (auto &row : rows_)
            for (auto &k : row) {
                std::string label = k.label;
                if (!k.special && label.size() == 1 && shifted_)
                    label = std::string(1, (char) std::toupper((unsigned char) label[0]));
                key_sprites_.emplace_back();
                key_sprites_.back().render(r, t.font(kfs), label);
            }
        app_.mark_dirty();
    }

    void TextInput::on_show()
    {
        auto &r = app_.renderer();
        auto &t = app_.theme();
        title_sprite_.render(r, t.font((int)(app_.screen_h() * 0.043)), title_);
        key_hl_ = create_rounded_border(r, 1, 1, 8, 3, t.menu_highlight);
        hint_sprite_.render(r, t.font((int)(app_.screen_h() * 0.022)), "D-pad: move   Select: type   Back: delete/cancel");
        rebuild_text();
    }

    void TextInput::on_hide() {}

    void TextInput::move(int dr, int dc)
    {
        int nr = cur_row_ + dr, nc = cur_col_ + dc;
        if (nr < 0) nr = 0;
        if (nr >= rows_count()) nr = rows_count() - 1;
        if (nc < 0) nc = 0;
        if (nc >= cols_in_row(nr)) nc = cols_in_row(nr) - 1;
        if (nr != cur_row_ || nc != cur_col_) {
            cur_row_ = nr;
            cur_col_ = nc;
            app_.play_click();
            app_.mark_dirty();
        }
    }

    void TextInput::type_key(const Key &k)
    {
        if (!k.special) {
            char c = k.value.empty() ? '\0' : k.value[0];
            if (shifted_) c = (char) std::toupper((unsigned char) c);
            text_.push_back(c);
            shifted_ = false;
            rebuild_text();
        }
        else if (k.value == "backspace") {
            if (!text_.empty()) { text_.pop_back(); rebuild_text(); }
        }
        else if (k.value == "shift") {
            shifted_ = !shifted_;
            rebuild_text();
        }
        else if (k.value == "space") {
            text_.push_back(' ');
            rebuild_text();
        }
        else if (k.value == "done") {
            app_.play_select();
            std::string out = text_;
            auto cb = std::move(on_submit_);
            app_.screens().pop();
            if (cb) cb(std::move(out));
            return;
        }
        app_.play_click();
        app_.mark_dirty();
    }

    bool TextInput::on_action(Action action)
    {
        switch (action) {
            case Action::Up:    move(-1, 0); return true;
            case Action::Down:  move(1, 0);  return true;
            case Action::Left:  move(0, -1); return true;
            case Action::Right: move(0, 1);  return true;
            case Action::Accept:
                type_key(rows_[cur_row_][cur_col_]);
                return true;
            case Action::Back:
                if (!text_.empty()) { text_.pop_back(); app_.play_click(); rebuild_text(); }
                else if (on_cancel_) {
                    auto cb = std::move(on_cancel_);
                    app_.play_click();
                    app_.screens().pop();
                    if (cb) cb();
                }
                return true;
            default:
                return false;
        }
    }

    void TextInput::draw()
    {
        auto &r = app_.renderer();
        auto &t = app_.theme();
        float sw = app_.screen_w(), sh = app_.screen_h();

        r.clear();
        r.fill_rect({0, 0, sw, sh}, {0x0A, 0x0A, 0x0E, 0xF0});

        // Title
        title_sprite_.draw(r, (sw - title_sprite_.width()) / 2, sh * 0.07, t.text);

        // Text display
        {
            float bw = std::max(text_sprite_.width() + 80, sw * 0.3f);
            float bx = (sw - bw) / 2, by = sh * 0.16;
            float bh = (float)(sh * 0.09);
            r.fill_rect({bx, by, bw, bh}, {0x18, 0x18, 0x1E, 0xFF});
            text_sprite_.draw(r, (sw - text_sprite_.width()) / 2, by + (bh - text_sprite_.height()) / 2, t.text);
        }

        // Keyboard
        float key_w = sw * 0.055f;
        float key_h = sh * 0.085f;
        float gap = sw * 0.006f;
        float row_gap = sh * 0.025f;
        float y0 = sh * 0.30f;
        int kidx = 0;
        for (int ri = 0; ri < rows_count(); ri++) {
            int cols = cols_in_row(ri);
            float row_w = cols * key_w + (cols - 1) * gap;
            float x0 = (sw - row_w) / 2;
            float y = y0 + ri * (key_h + row_gap);
            for (int ci = 0; ci < cols; ci++) {
                const Key &k = rows_[ri][ci];
                float x = x0 + ci * (key_w + gap);
                SDL_Color bg = k.special ? SDL_Color{0x22, 0x22, 0x2A, 0xFF}
                                         : SDL_Color{0x18, 0x18, 0x1E, 0xFF};
                if (ri == cur_row_ && ci == cur_col_) bg = {0x2A, 0x3A, 0x4A, 0xFF};
                r.fill_rect({x, y, key_w, key_h}, bg);
                if (ri == cur_row_ && ci == cur_col_) {
                    key_hl_->set_pos(x, y, key_w, key_h);
                    r.draw(*key_hl_);
                }
                SDL_Color col = t.text;
                if (k.value == "shift" && shifted_) col = t.accent;
                key_sprites_[kidx].draw(r, x + (key_w - key_sprites_[kidx].width()) / 2,
                                        y + (key_h - key_sprites_[kidx].height()) / 2, col);
                kidx++;
            }
        }

        // Footer hint
        hint_sprite_.draw(r, (sw - hint_sprite_.width()) / 2, sh * 0.93, t.text_dim);
    }
}
