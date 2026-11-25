#include "kit.hpp"
#include "../core/app.hpp"
#include "../ui/engine/focus.hpp"

namespace Foyer {
    using namespace ui;

    namespace kit {
        RowButton::RowButton(App &app, const std::string &l, const std::string &v, const std::string &icon,
                             const std::string &sub_text): Button(l, v)
        {
            style.w = Dim::fill();
            style.min_h = 76;
            radius = 16;
            if (!icon.empty()) {
                auto img = std::make_unique<Image>(app.icon(icon));
                img->style.w = Dim::dp(34);
                img->style.h = Dim::dp(34);
                img->style.shrink = 0.f;
                img->has_tint = true;
                icon_img = static_cast<Image*>(insert(0, std::move(img)));
            }
            if (!sub_text.empty()) {
                // label + sub stacked in a column, in the label's slot
                auto col = std::make_unique<Node>();
                col->style.dir = Dir::Column;
                col->style.grow = 1.f;
                col->style.shrink = 1.f;
                col->style.gap = 4;
                auto *lt = col->add<Text>(l, TextRole::Md);
                sub = col->add<Text>(sub_text, TextRole::Sm);
                sub->dim = true;
                size_t at = index_of(label);
                remove_child(label);
                label = lt;
                insert(at, std::move(col));
            }
            on_action = [this](Action a) {
                if ((a == Action::Left || a == Action::Right) && on_adjust) return on_adjust(a);
                return false;
            };
        }

        bool RowButton::tick(Engine &e, float dt)
        {
            bool a = Button::tick(e, dt);
            if (icon_img) icon_img->tint = mix(e.tokens().text, e.tokens().on_accent, highlight());
            return a;
        }

        Node* info_row(Node *parent, const std::string &k, const std::string &v, Text **out)
        {
            auto *r = parent->add<Node>();
            r->style.dir = Dir::Row;
            r->style.align = Align::Center;
            r->style.pad = Edges::xy(28, 10);
            r->style.gap = 20;
            auto *kt = r->add<Text>(k, TextRole::Sm);
            kt->dim = true;
            kt->style.grow = 1.f;
            auto *vt = r->add<Text>(v.empty() ? "--" : v, TextRole::Sm);
            vt->align = TextAlign::Right;
            vt->style.shrink = 0.f;
            if (out) *out = vt;
            return r;
        }

        Text* section(Node *parent, const std::string &title)
        {
            auto *t = parent->add<Text>(title, TextRole::Sm);
            t->dim = true;
            t->style.pad = Edges{28, 26, 0, 6};
            return t;
        }

        Scroller* list(Node *parent, float max_w)
        {
            auto *sc = parent->add<Scroller>(Dir::Column);
            sc->style.grow = 1.f;
            sc->style.shrink = 1.f;
            sc->style.min_h = 0;
            sc->style.w = Dim::fill();
            sc->style.max_w = max_w;
            sc->style.has_self_align = true;
            sc->style.self_align = Align::Center;
            sc->style.gap = 8;
            sc->style.pad = Edges{0, 16, 0, 40};
            sc->style.align = Align::Stretch;
            sc->margin = 120;
            return sc;
        }

        Node* header(Node *parent, const std::string &title, const std::string &subtitle, Text **sub_out)
        {
            auto *h = parent->add<Node>();
            h->style.dir = Dir::Column;
            h->style.shrink = 0.f;
            h->style.gap = 6;
            h->style.pad = Edges{0, 8, 0, 18};
            h->style.w = Dim::fill();
            h->style.max_w = 1100;
            h->style.has_self_align = true;
            h->style.self_align = Align::Center;
            auto *t = h->add<Text>(title, TextRole::Xl);
            t->display_font = true;
            auto *s = h->add<Text>(subtitle, TextRole::Md);
            s->dim = true;
            s->visible = !subtitle.empty();
            if (sub_out) *sub_out = s;
            return h;
        }

        Node* hints(Node *parent, const std::vector<std::pair<std::string, std::string>> &items)
        {
            auto *r = parent->add<Node>();
            r->style.dir = Dir::Row;
            r->style.shrink = 0.f;
            r->style.gap = 34;
            r->style.justify = Justify::End;
            r->style.align = Align::Center;
            r->style.pad = Edges{0, 12, 0, 4};
            for (auto &[k, v] : items) {
                auto *g = r->add<Node>();
                g->style.dir = Dir::Row; g->style.gap = 10; g->style.align = Align::Center;
                auto *key = g->add<Box>();
                key->has_bg = true;
                key->radius = 8;
                key->style.pad = Edges::xy(10, 2);
                key->style.min_w = 34;
                key->style.align = Align::Center;
                key->style.dir = Dir::Column;
                auto *kt = key->add<Text>(k, TextRole::Xs);
                kt->align = TextAlign::Center;
                key->bg = {0xFF, 0xFF, 0xFF, 0x22};
                auto *vt = g->add<Text>(v, TextRole::Xs);
                vt->dim = true;
            }
            return r;
        }
    }

    // ---- Page ------------------------------------------------------------------
    Page::Page(App &app, std::string t, std::string s): app_(app), title_(std::move(t)), subtitle_(std::move(s)) {}

    void Page::build(Engine &e)
    {
        root->style.dir = Dir::Column;
        root->style.pad = Edges::xy(e.tokens().page_margin, 36);
        root->style.align = Align::Stretch;
        kit::header(root.get(), title_, subtitle_, &subtitle_text_);
        list_ = kit::list(root.get());
        body_ = list_;
        fill(e, *list_);
        kit::hints(root.get(), {{"A", "Select"}, {"B", "Back"}});
    }

    void Page::paint_under(Engine &e, Canvas &c)
    {
        const Tokens &t = e.tokens();
        float w = (float) e.metrics().width(), h = (float) e.metrics().height();
        c.fill({0, 0, w, h}, t.bg);
        c.gradient({0, 0, w, h * 0.6f}, with_alpha(t.accent, 0x26), with_alpha(t.accent, 0));
    }

    kit::RowButton* Page::row(const std::string &label, const std::string &value, std::function<void()> act,
                              const std::string &icon, const std::string &sub)
    {
        auto *b = body_->add<kit::RowButton>(app_, label, value, icon, sub);
        b->on_activate = std::move(act);
        return b;
    }

    void Page::refocus_first()
    {
        if (Node *f = Focus::first_in(root.get())) Focus::set(f);
    }

    // ---- ChoiceSheet -----------------------------------------------------------
    ChoiceSheet::ChoiceSheet(App &app, std::string t, std::vector<std::string> o, int sel, std::function<void(int)> f)
        : app_(app), title_(std::move(t)), options_(std::move(o)), selected_(sel), on_pick_(std::move(f))
    {
        opaque = false;
        dim_below = true;
    }

    void ChoiceSheet::build(Engine &e)
    {
        (void) e;
        root->style.dir = Dir::Row;
        root->style.justify = Justify::End;
        auto *panel = root->add<Box>(Dir::Column);
        panel->has_bg = true;
        panel->bg = e.tokens().surface;
        panel->radius = e.tokens().radius_lg;
        panel->shadow_blur = 40;
        panel->shadow_color = e.tokens().shadow;
        panel->style.w = Dim::dp(620);
        panel->style.max_w = e.metrics().safe_width_dp() * 0.9f;
        panel->style.h = Dim::fill();
        panel->style.pad = Edges::all(28);
        panel->style.gap = 10;
        panel->tx.snap(80.f);
        panel->tx.to(0.f);
        auto *t = panel->add<Text>(title_, TextRole::Lg);
        t->style.pad = Edges{8, 4, 0, 14};
        auto *sc = kit::list(panel, 10000);
        for (int i = 0; i < (int) options_.size(); i++) {
            auto *b = sc->add<kit::RowButton>(app_, options_[i], i == selected_ ? "\xE2\x9C\x93" : "");
            b->on_activate = [this, i, &e] {
                auto cb = on_pick_;
                e.pop();
                if (cb) cb(i);
            };
            if (i == selected_) initial_focus = b;
        }
    }

    // ---- ConfirmDialog ---------------------------------------------------------
    ConfirmDialog::ConfirmDialog(App &app, std::string t, std::string m, std::string y, std::string n,
                                 std::function<void(bool)> d, int cd)
        : app_(app), title_(std::move(t)), message_(std::move(m)), yes_(std::move(y)), no_(std::move(n)),
          done_(std::move(d)), countdown_(cd)
    {
        opaque = false;
        dim_below = true;
        started_ = SDL_GetTicks();
    }

    void ConfirmDialog::finish(Engine &e, bool yes)
    {
        if (fired_) return;
        fired_ = true;
        auto cb = done_;
        e.pop();
        if (cb) cb(yes);
    }

    void ConfirmDialog::build(Engine &e)
    {
        root->style.dir = Dir::Column;
        root->style.align = Align::Center;
        root->style.justify = Justify::Center;
        auto *card = root->add<Box>(Dir::Column);
        card->has_bg = true;
        card->bg = e.tokens().surface;
        card->radius = e.tokens().radius_lg;
        card->shadow_blur = 50;
        card->shadow_color = e.tokens().shadow;
        card->style.w = Dim::dp(760);
        card->style.max_w = e.metrics().safe_width_dp() * 0.92f;
        card->style.pad = Edges::all(40);
        card->style.gap = 18;
        card->scale.snap(0.94f);
        card->scale.to(1.f);
        card->add<Text>(title_, TextRole::Lg);
        msg_ = card->add<Text>(message_, TextRole::Md);
        msg_->dim = true;
        msg_->max_lines = 4;
        auto *btns = card->add<Node>();
        btns->style.dir = Dir::Row;
        btns->style.gap = 16;
        btns->style.justify = Justify::End;
        btns->style.pad.t = 12;
        auto *no = btns->add<Button>(no_);
        no->label->align = TextAlign::Center;
        no->style.min_w = 180;
        no->on_activate = [this, &e] { finish(e, false); };
        auto *yes = btns->add<Button>(yes_);
        yes->label->align = TextAlign::Center;
        yes->style.min_w = 180;
        yes->on_activate = [this, &e] { finish(e, true); };
        initial_focus = countdown_ > 0 ? (Node*) yes : (Node*) no;
    }

    bool ConfirmDialog::tick(Engine &e, float dt)
    {
        (void) dt;
        if (countdown_ <= 0 || fired_) return false;
        int left = countdown_ - (int) ((SDL_GetTicks() - started_) / 1000);
        if (left <= 0) { finish(e, false); return false; }
        std::string m = message_ + "\nReverting in " + std::to_string(left) + " s.";
        if (msg_->str() != m) msg_->set(m);
        return true;
    }
}
