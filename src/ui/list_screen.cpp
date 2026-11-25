#include "list_screen.hpp"
#include "text_sprite.hpp"
#include "../core/app.hpp"
#include "../render/renderer.hpp"
#include "../render/theme.hpp"

Foyer::ListScreen::ListScreen(App &app): Screen(app) {}

void Foyer::ListScreen::clear_items()
{
    items_.clear();
    index_ = 0;
    view_offset_ = 0;
}

void Foyer::ListScreen::add_item(const std::string &text, const std::string &value, const std::string &subtext)
{
    items_.push_back({text, value, subtext, true});
}

void Foyer::ListScreen::set_enabled(int i, bool e)
{
    if (i >= 0 && i < (int) items_.size())
        items_[i].enabled = e;
}

void Foyer::ListScreen::set_value(int i, const std::string &v)
{
    if (i >= 0 && i < (int) items_.size())
        items_[i].value = v;
}

void Foyer::ListScreen::set_text(int i, const std::string &t)
{
    if (i >= 0 && i < (int) items_.size())
        items_[i].text = t;
}

void Foyer::ListScreen::set_selected(int i)
{
    if (i < 0) i = 0;
    if (i >= (int) items_.size()) i = (int) items_.size() - 1;
    index_ = i;
    clamp_view();
}

int Foyer::ListScreen::row_height() const
{
    return app_.theme().item_font_size + 48;
}

int Foyer::ListScreen::rows_visible() const
{
    int top = 170;
    int bottom = app_.screen_h() - 120;
    return std::max(1, (bottom - top) / row_height());
}

void Foyer::ListScreen::clamp_view()
{
    int vis = rows_visible();
    if (index_ < view_offset_)
        view_offset_ = index_;
    else if (index_ >= view_offset_ + vis)
        view_offset_ = index_ - vis + 1;
    if (view_offset_ < 0) view_offset_ = 0;
}

void Foyer::ListScreen::move(int delta)
{
    int target = index_ + delta;
    if (target < 0) target = 0;
    if (target >= (int) items_.size()) target = (int) items_.size() - 1;
    // Skip disabled items
    int guard = 0;
    while (target != index_ && guard++ < (int) items_.size()) {
        if (items_[target].enabled) break;
        target += delta;
        if (target < 0) target = 0;
        if (target >= (int) items_.size()) target = (int) items_.size() - 1;
    }
    if (target != index_) {
        index_ = target;
        app_.play_click();
        clamp_view();
    }
}

bool Foyer::ListScreen::on_action(Action action)
{
    switch (action) {
        case Action::Up:    move(-1); return true;
        case Action::Down:  move(1);  return true;
        case Action::Accept:
            if (index_ >= 0 && index_ < (int) items_.size() && items_[index_].enabled) {
                app_.play_select();
                on_select(index_);
            }
            return true;
        case Action::Back:
            return on_back_override();
        default:
            return false;
    }
}

void Foyer::ListScreen::draw()
{
    auto &r = app_.renderer();
    auto &t = app_.theme();
    int w = app_.screen_w();
    int h = app_.screen_h();
    int margin = 80;
    int rh = row_height();

    r.clear();
    r.fill_rect({0.f, 0.f, (float) w, (float) h}, t.background);

    // Title
    TextSprite title;
    title.render(r, t.title_font(), title_);
    title.draw(r, margin, 60, t.text);

    int top = 170;
    for (int i = view_offset_; i < (int) items_.size() && (i - view_offset_) < rows_visible(); i++) {
        Item &it = items_[i];
        float y = top + (i - view_offset_) * rh;
        bool sel = (i == index_);
        if (sel)
            r.fill_rect({(float) margin - 20.f, y - 12.f, (float)(w - 2 * margin + 40), (float) rh},
                        {t.accent.r, t.accent.g, t.accent.b, 0x55});

        SDL_Color col = it.enabled ? (sel ? t.sidebar_text_hl : t.text) : t.text_dim;
        if (sel) col = t.text;

        TextSprite ts;
        ts.render(r, t.item_font(), it.text, w - 2 * margin - 300);
        ts.draw(r, margin, y, col);

        if (!it.subtext.empty()) {
            TextSprite ss;
            ss.render(r, t.info_font(), it.subtext, w - 2 * margin - 300);
            ss.draw(r, margin, y + t.item_font_size + 6, t.text_dim);
        }

        if (!it.value.empty()) {
            TextSprite vs;
            vs.render(r, t.item_font(), it.value);
            vs.draw(r, w - margin - vs.width(), y, sel ? t.text : t.text_dim);
        }
    }

    if (!footer_.empty()) {
        TextSprite fs;
        fs.render(r, t.status_font(), footer_);
        fs.draw(r, margin, h - 80, t.text_dim);
    }
}
