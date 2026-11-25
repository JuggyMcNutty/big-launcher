#include <algorithm>
#include <cmath>
#include <ctime>
#include <deque>

#include "shell.hpp"
#include "settings.hpp"
#include "../core/app.hpp"
#include "../ui/engine/focus.hpp"

namespace Foyer {
    using namespace ui;

    // ---- toasts (global) --------------------------------------------------------
    namespace {
        struct ToastItem { std::string msg; bool error; Uint64 at; };
        std::deque<ToastItem> g_toasts;
        std::string g_launching;
        Uint64 g_launch_at = 0;
    }

    void Shell::post_toast(const std::string &msg, bool error)
    {
        g_toasts.push_back({msg, error, SDL_GetTicks()});
        while (g_toasts.size() > 3) g_toasts.pop_front();
        invalidate();
    }

    void Shell::set_launching(const std::string &title)
    {
        g_launching = title;
        g_launch_at = SDL_GetTicks();
        invalidate();
    }

    static SDL_Color tile_color(const std::string &hex)
    {
        SDL_Color c{0x1E, 0x22, 0x2E, 0xFF};
        parse_color(hex, c);
        return c;
    }

    // ---- TileCard -----------------------------------------------------------------
    TileCard::TileCard(App &app, TileInfo i, float w, float aspect): info(std::move(i))
    {
        focusable = true;
        style.w = Dim::dp(w);
        style.h = Dim::dp(w / aspect);
        style.shrink = 0.f;
        style.dir = Dir::Stack;
        radius = 18;
        if (!info.tile_art.empty()) {
            art_ = add<Image>(app.resolve_art(info.tile_art), Fit::Cover);
            art_->radius = radius;
        }
        if (!info.icon.empty() && info.tile_art.empty()) {
            auto *wrap = add<Node>();
            wrap->style.dir = Dir::Column;
            wrap->style.align = Align::Center;
            wrap->style.justify = Justify::Center;
            wrap->style.pad = Edges::all(w * 0.16f);
            logo_ = wrap->add<Image>(app.resolve_art(info.icon), Fit::Contain);
            logo_->style.w = Dim::fill();
            logo_->style.h = Dim::fill();
            logo_->style.grow = 1.f;
        }
        if (info.icon.empty() && info.tile_art.empty()) {
            auto *wrap = add<Node>();
            wrap->style.dir = Dir::Column;
            wrap->style.align = Align::Center;
            wrap->style.justify = Justify::Center;
            wrap->style.pad = Edges::all(16);
            name_ = wrap->add<Text>(info.title, TextRole::Lg);
            name_->align = TextAlign::Center;
            name_->max_lines = 2;
        }
    }

    bool TileCard::tick(Engine &e, float dt)
    {
        bool a = Box::tick(e, dt);
        bool f = focused();
        hl_.to(f ? 1.f : 0.f);
        scale.to(f ? e.tokens().focus_scale : 1.f);
        ty.to(f ? -6.f : 0.f);
        a |= hl_.step(dt, 300.f);
        return a;
    }

    void TileCard::paint(Canvas &c, const SDL_FRect &r, float alpha)
    {
        (void) alpha;
        if (!engine_) return;
        const auto &m = engine_->metrics();
        const Tokens &t = engine_->tokens();
        float rad = m.dp(radius);
        float h = hl_.get();
        // drop shadow grows with focus; accent glow under the focused tile
        c.shadow({r.x, r.y + m.dp(10 + 8 * h), r.w, r.h}, rad, m.dp(18 + 22 * h), with_alpha(t.shadow, (Uint8) (t.shadow.a * (0.6f + 0.4f * h))));
        if (h > 0.01f)
            c.shadow({r.x, r.y + m.dp(6), r.w, r.h}, rad, m.dp(36), with_alpha(t.accent, (Uint8) (0x70 * h)));
        c.rounded(r, rad, info.bg);
        // subtle top sheen (inset below the rounded corners)
        c.gradient({r.x + rad * 0.3f, r.y + 1, r.w - rad * 0.6f, r.h * 0.45f},
                   {255, 255, 255, (Uint8) (10 + 12 * h)}, {255, 255, 255, 0});
    }

    void TileCard::paint_over(Canvas &c, const SDL_FRect &r, float alpha)
    {
        (void) alpha;
        if (!engine_) return;
        float h = hl_.get();
        if (h < 0.01f) return;
        const auto &m = engine_->metrics();
        const Tokens &t = engine_->tokens();
        float gap = m.dp(5);
        SDL_FRect ring{r.x - gap, r.y - gap, r.w + 2 * gap, r.h + 2 * gap};
        c.border(ring, m.dp(radius) + gap, m.dp(t.focus_ring_width), with_alpha(t.focus_ring, (Uint8) (255 * h)));
    }

    // ---- Shell ------------------------------------------------------------------
    Shell::Shell(App &app): app_(app)
    {
        tab_id_ = app_.layout().tabs.empty() ? "home" : app_.layout().tabs.front().id;
        wall_a_.fit = Fit::Cover;
        wall_b_.fit = Fit::Cover;
    }

    TileInfo Shell::resolve(const Tile &t) const
    {
        TileInfo i;
        i.size = t.size;
        auto it = t.app.empty() ? app_.apps().end() : app_.apps().find(t.app);
        if (it != app_.apps().end()) {
            const AppEntry &a = it->second;
            i.app = &a;
            i.title = a.title; i.subtitle = a.subtitle; i.command = a.launch_command();
            i.icon = a.resolve_art(a.icon); i.tile_art = a.resolve_art(a.tile); i.hero_art = a.resolve_art(a.hero);
            i.bg = tile_color(a.background);
        } else if (!t.app.empty()) {
            i.title = t.app; i.subtitle = "App not installed";
        }
        if (!t.title.empty()) i.title = t.title;
        if (!t.command.empty()) i.command = t.command;
        if (!t.icon.empty()) i.icon = app_.resolve_art(t.icon);
        if (!t.background.empty()) i.bg = tile_color(t.background);
        return i;
    }

    void Shell::build(Engine &e)
    {
        const Tokens &t = e.tokens();
        root->style.dir = Dir::Column;
        root->style.align = Align::Stretch;

        // ---- top bar
        auto *bar = root->add<Node>();
        bar->style.dir = Dir::Row;
        bar->style.align = Align::Center;
        bar->style.pad = Edges{t.page_margin, 28, t.page_margin, 10};
        bar->style.gap = 10;
        tabs_row_ = bar->add<Node>();
        tabs_row_->style.dir = Dir::Row;
        tabs_row_->style.gap = 6;
        tabs_row_->style.grow = 1.f;
        tabs_row_->style.shrink = 1.f;
        tabs_row_->focus_memory = true;
        tab_buttons_.clear();
        bool compact = e.metrics().breakpoint() == Breakpoint::XS;
        for (auto &tab : app_.layout().tabs) {
            auto *b = tabs_row_->add<Button>(tab.title);
            b->style.pad = Edges::xy(compact ? 18 : 26, 10);
            b->style.min_h = 54;
            b->radius = 27;
            b->label->role = TextRole::Md;
            b->id = "tab:" + tab.id;
            std::string id = tab.id;
            b->on_focus_changed = [this, id](bool f) { if (f && id != tab_id_) show_tab(id); };
            b->on_activate = [this, id] {
                // A on a tab: drop into its content
                show_tab(id);
                if (Node *n = Focus::first_in(content_)) Focus::set(n);
            };
            tab_buttons_.push_back(b);
            if (tab.id == tab_id_) tabs_row_->remembered = b;
        }
        // status cluster
        auto *status = bar->add<Node>();
        status->style.dir = Dir::Row;
        status->style.align = Align::Center;
        status->style.gap = 14;
        status->style.shrink = 0.f;
        auto icon = [&](const std::string &name) {
            auto *img = status->add<Image>(app_.icon(name));
            img->style.w = Dim::dp(30); img->style.h = Dim::dp(30);
            img->has_tint = true; img->tint = t.text_dim;
            return img;
        };
        net_icon_ = icon("ethernet");
        net_text_ = status->add<Text>("", TextRole::Sm);
        net_text_->dim = true;
        net_text_->visible = !compact;
        vol_icon_ = icon("volume");
        vol_text_ = status->add<Text>("", TextRole::Sm);
        vol_text_->dim = true;
        auto *sp = status->add<Node>(); sp->style.w = Dim::dp(10);
        clock_ = status->add<Text>("", TextRole::Lg);
        clock_->visible = app_.settings().clock;
        last_clock_ = 0;

        // ---- content
        content_ = root->add<Node>();
        content_->style.grow = 1.f;
        content_->style.shrink = 1.f;
        content_->style.dir = Dir::Column;
        content_->style.align = Align::Stretch;
        build_tab(e);
        update_status(e);
        built_for_tabs_ = (int) app_.layout().tabs.size();
    }

    void Shell::show_tab(const std::string &id)
    {
        if (id == tab_id_ && content_ && !content_->children().empty()) {
            for (auto *b : tab_buttons_) if (b->id == "tab:" + id) tabs_row_->remembered = b;
            return;
        }
        tab_id_ = id;
        for (auto *b : tab_buttons_)
            if (b->id == "tab:" + id) {
                tabs_row_->remembered = b;
                if (!b->focused() && !tabs_row_->focus_within()) {}   // keep focus where it is
            }
        if (content_) {
            content_->clear_children();
            build_tab(app_.ui());
            // content slides in
            content_->opacity.snap(0.f); content_->opacity.to(1.f);
            content_->ty.snap(24.f); content_->ty.to(0.f);
        }
        invalidate_layout();
    }

    void Shell::build_tab(Engine &e)
    {
        const Tab *tab = nullptr;
        for (auto &t : app_.layout().tabs) if (t.id == tab_id_) tab = &t;
        if (!tab && !app_.layout().tabs.empty()) { tab = &app_.layout().tabs.front(); tab_id_ = tab->id; }
        if (!tab) return;
        if (tab->kind == "library") build_library(e, *content_);
        else if (tab->kind == "settings") build_settings(e, *content_);
        else build_home(e, *tab, *content_);
        // Highlight the active tab button.
        for (auto *b : tab_buttons_) {
            bool active = b->id == "tab:" + tab_id_;
            b->has_bg_base_ = active;
        }
    }

    void Shell::focus_tile(const TileInfo &info)
    {
        if (hero_title_) hero_title_->set(info.title);
        if (hero_sub_) { hero_sub_->set(info.subtitle); hero_sub_->visible = !info.subtitle.empty(); }
        std::string art = !info.hero_art.empty() ? info.hero_art : !info.tile_art.empty() ? info.tile_art : info.icon;
        // logos are blurred far more so they read as an ambient colour glow
        set_wallpaper(art, info.hero_art.empty() && info.tile_art.empty());
    }

    void Shell::set_wallpaper(const std::string &path, bool ambient)
    {
        const Tokens &t = app_.ui().tokens();
        std::string p = path;
        if (t.wallpaper_mode == "image") p = app_.resolve_art(t.wallpaper_image);
        else if (t.wallpaper_mode == "color") p.clear();
        if (p == wall_path_) return;
        wall_path_ = p;
        wall_flip_ = !wall_flip_;
        Image &next = wall_flip_ ? wall_b_ : wall_a_;
        next.set_path(p);
        next.blur = ambient ? t.wallpaper_blur * 3.5f : t.wallpaper_blur;
        wall_mix_.snap(0.f);
        wall_mix_.to(1.f);
    }

    static float tile_width(const Tokens &t, TileSize s, Breakpoint bp)
    {
        float w = t.tile_w;
        if (bp == Breakpoint::LG) w *= 1.0f;
        switch (s) {
            case TileSize::Small: return w * 0.72f;
            case TileSize::Large: return w * 1.32f;
            default: return w;
        }
    }

    void Shell::build_home(Engine &e, const Tab &tab, Node &into)
    {
        const Tokens &t = e.tokens();
        bool compact = e.metrics().breakpoint() == Breakpoint::XS;
        // hero text
        auto *hero = into.add<Node>();
        hero->style.dir = Dir::Column;
        hero->style.pad = Edges{t.page_margin, compact ? 10.f : 34.f, t.page_margin, compact ? 10.f : 24.f};
        hero->style.gap = 6;
        hero->style.grow = 1.f;
        hero->style.justify = Justify::End;
        hero->style.min_h = compact ? 70.f : 150.f;
        hero_title_ = hero->add<Text>("", compact ? TextRole::Xl : TextRole::Hero);
        hero_title_->display_font = true;
        hero_sub_ = hero->add<Text>("", TextRole::Lg);
        hero_sub_->dim = true;

        // rows
        auto *rows = into.add<Scroller>(Dir::Column);
        rows->style.shrink = 1.f;
        rows->style.gap = compact ? 10 : 18;
        rows->style.pad = Edges{0, 8, 0, 34};
        rows->margin = 30;
        rows->style.max_h = e.metrics().safe_height_dp() * (compact ? 0.72f : 0.62f);
        Node *first = nullptr;
        for (auto &row : tab.rows) {
            auto *group = rows->add<Node>();
            group->style.dir = Dir::Column;
            group->style.gap = 10;
            group->style.shrink = 0.f;
            if (!row.title.empty()) {
                auto *h = group->add<Text>(row.title, TextRole::Md);
                h->dim = true;
                h->style.pad = Edges{t.page_margin, 0, 0, 0};
            }
            auto *sc = group->add<Scroller>(Dir::Row);
            sc->style.gap = t.tile_gap;
            sc->style.pad = Edges{t.page_margin, 22, t.page_margin, 30};   // room for zoom + glow
            sc->style.align = Align::Center;
            sc->focus_memory = true;
            sc->margin = t.page_margin;
            for (auto &tile : row.tiles) {
                TileInfo info = resolve(tile);
                float w = tile_width(t, tile.size, e.metrics().breakpoint());
                auto *card = sc->add<TileCard>(app_, info, w, t.tile_aspect);
                card->on_focus_changed = [this, card](bool f) { if (f) focus_tile(card->info); };
                card->on_activate = [this, card] {
                    if (card->info.command.empty()) { app_.toast(card->info.title + " has no command", true); return; }
                    app_.execute(card->info.command, card->info.title);
                };
                if (!first) first = card;
            }
            if (row.tiles.empty()) {
                auto *hint = sc->add<Text>("Empty row \xE2\x80\x94 add apps in Settings \xE2\x86\x92 Layout", TextRole::Sm);
                hint->dim = true;
            }
        }
        if (tab.rows.empty()) {
            auto *hint = rows->add<Text>("This tab is empty. Add rows in Settings \xE2\x86\x92 Layout.", TextRole::Md);
            hint->dim = true;
            hint->style.pad = Edges{t.page_margin, 20, 0, 0};
        }
        initial_focus = first;
        if (first) {
            focus_tile(static_cast<TileCard*>(first)->info);
            if (!tabs_row_->focus_within() && Focus::current() == nullptr) Focus::set(first, false);
        } else {
            set_wallpaper("");
        }
    }

    void Shell::build_library(Engine &e, Node &into)
    {
        const Tokens &t = e.tokens();
        if (hero_title_) hero_title_ = nullptr;
        hero_sub_ = nullptr;
        auto *head = into.add<Node>();
        head->style.pad = Edges{t.page_margin, 30, t.page_margin, 10};
        auto *ht = head->add<Text>("All apps", TextRole::Xl);
        ht->display_font = true;
        auto *sc = into.add<Scroller>(Dir::Column);
        sc->style.grow = 1.f;
        sc->style.shrink = 1.f;
        sc->style.pad = Edges{t.page_margin, 20, t.page_margin, 40};
        sc->style.gap = t.tile_gap;
        sc->margin = 40;
        // grid: rows of N tiles that fit the width
        float avail = e.metrics().safe_width_dp() - 2 * t.page_margin;
        float w = t.tile_w * 0.86f;
        int cols = std::max(2, (int) ((avail + t.tile_gap) / (w + t.tile_gap)));
        w = (avail - (cols - 1) * t.tile_gap) / cols;
        Node *row = nullptr;
        int i = 0;
        Node *first = nullptr;
        std::vector<std::pair<std::string, const AppEntry*>> sorted;
        for (auto &[id, a] : app_.apps()) sorted.push_back({id, &a});
        std::sort(sorted.begin(), sorted.end(), [](auto &x, auto &y) { return x.second->title < y.second->title; });
        for (auto &[id, ap] : sorted) {
            (void) ap;
            if (i % cols == 0) {
                row = sc->add<Node>();
                row->style.dir = Dir::Row;
                row->style.gap = t.tile_gap;
                row->style.shrink = 0.f;
                row->style.pad = Edges{0, 8, 0, 8};
            }
            Tile tl; tl.app = id;
            TileInfo info = resolve(tl);
            auto *col = row->add<Node>();
            col->style.dir = Dir::Column;
            col->style.gap = 10;
            auto *card = col->add<TileCard>(app_, info, w, t.tile_aspect);
            auto *name = col->add<Text>(info.title, TextRole::Sm);
            name->style.w = Dim::dp(w);
            card->on_focus_changed = [this, card](bool f) { if (f) set_wallpaper(card->info.icon, true); };
            card->on_activate = [this, card] { app_.execute(card->info.command, card->info.title); };
            if (!first) first = card;
            i++;
        }
        if (app_.apps().empty()) {
            auto *hint = sc->add<Text>("No apps installed. Drop .toml files into /etc/foyer/apps.d/", TextRole::Md);
            hint->dim = true;
        }
        initial_focus = first;
    }

    void Shell::build_settings(Engine &e, Node &into)
    {
        hero_title_ = nullptr;
        hero_sub_ = nullptr;
        set_wallpaper("");
        initial_focus = build_settings_hub(app_, e, into);
    }

    void Shell::on_enter(Engine &e)
    {
        (void) e;
        // layout/apps may have changed in an editor
        if ((int) app_.layout().tabs.size() != built_for_tabs_) app_.ui().rebuild_all();
        else { content_->clear_children(); build_tab(app_.ui()); }
        app_.refresh_status();
    }

    bool Shell::on_action(Engine &e, Action a)
    {
        (void) e;
        // Up from the content's top edge goes to the tab bar; Back from the
        // content returns to the tab bar; Back on the tab bar goes Home.
        if (a == Action::Back) {
            if (!tabs_row_->focus_within()) {
                Node *b = tabs_row_->remembered ? tabs_row_->remembered : Focus::first_in(tabs_row_);
                Focus::set(b);
                return true;
            }
            if (tab_id_ != app_.layout().tabs.front().id) {
                show_tab(app_.layout().tabs.front().id);
                for (auto *b : tab_buttons_) if (b->id == "tab:" + tab_id_) Focus::set(b);
                return true;
            }
            return true;   // root: swallow
        }
        if (a == Action::Down && tabs_row_->focus_within()) {
            if (Node *n = Focus::first_in(content_)) { Focus::set(n); if (app_.ui().on_click) app_.ui().on_click(); return true; }
        }
        return false;
    }

    void Shell::update_status(Engine &e)
    {
        const Status &s = app_.status();
        std::string ni = s.net_kind.empty() ? "ethernet" : (s.net_kind == "offline" ? "offline" : s.net_kind);
        if (net_icon_) net_icon_->set_path(app_.icon(ni));
        if (net_text_) net_text_->set(s.net_text);
        if (vol_icon_) vol_icon_->set_path(app_.icon(s.muted ? "mute" : "volume"));
        if (vol_text_) vol_text_->set(s.volume >= 0 && !s.muted ? std::to_string(s.volume) : "");
        if (clock_ && app_.settings().clock) {
            time_t now = time(nullptr);
            char buf[64];
            strftime(buf, sizeof buf, app_.settings().clock_format.c_str(), localtime(&now));
            clock_->set(buf);
        }
        (void) e;
    }

    bool Shell::tick(Engine &e, float dt)
    {
        Uint64 now = SDL_GetTicks();
        if (now - last_clock_ > 1000) { last_clock_ = now; update_status(e); }
        bool a = wall_a_.tick(e, dt);
        a |= wall_b_.tick(e, dt);
        a |= wall_mix_.step(dt, 120.f);
        // keep wallpaper images sized to the screen
        float W = (float) e.metrics().width(), H = (float) e.metrics().height();
        wall_a_.frame = wall_b_.frame = {0, 0, W, H};
        return a;
    }

    void Shell::paint_under(Engine &e, Canvas &c)
    {
        const Tokens &t = e.tokens();
        float W = (float) e.metrics().width(), H = (float) e.metrics().height();
        c.fill({0, 0, W, H}, t.bg);
        // cross-fading focused-art wallpaper
        Image &cur = wall_flip_ ? wall_b_ : wall_a_;
        Image &old = wall_flip_ ? wall_a_ : wall_b_;
        SDL_FRect full{0, 0, W, H};
        if (old.loaded()) { c.push_alpha(1.f - wall_mix_.get()); old.paint(c, full, 1.f); c.pop_alpha(); }
        if (cur.loaded()) { c.push_alpha(wall_mix_.get()); cur.paint(c, full, 1.f); c.pop_alpha(); }
        // darken + scrims for legibility
        c.fill(full, with_alpha(t.bg, (Uint8) (255 * std::clamp(t.wallpaper_dim, 0.f, 1.f))));
        c.gradient({0, 0, W, H * 0.25f}, with_alpha(t.bg, 0xC0), with_alpha(t.bg, 0));
        c.gradient({0, H * 0.45f, W, H * 0.55f}, with_alpha(t.bg, 0), with_alpha(t.bg, 0xF0));
        c.gradient({0, 0, W * 0.6f, H}, with_alpha(t.bg, 0x90), with_alpha(t.bg, 0), false);
        // accent wash
        c.gradient({0, 0, W, H * 0.5f}, with_alpha(t.accent, 0x18), with_alpha(t.accent, 0));
    }

    bool Shell::overlay_active()
    {
        Uint64 now = SDL_GetTicks();
        while (!g_toasts.empty() && now - g_toasts.front().at > 5000) { g_toasts.pop_front(); invalidate(); }
        return !g_toasts.empty() || (!g_launching.empty() && now - g_launch_at < 8000);
    }

    void Shell::paint_overlay(Engine &e, Canvas &c)
    {
        const auto &m = e.metrics();
        const Tokens &t = e.tokens();
        Uint64 now = SDL_GetTicks();
        SDL_FRect safe = m.safe_rect();
        float y = safe.y + safe.h - m.dp(40);
        std::vector<std::pair<std::string, bool>> items;
        for (auto it = g_toasts.rbegin(); it != g_toasts.rend(); ++it) items.push_back({it->msg, it->error});
        if (!g_launching.empty() && now - g_launch_at < 8000)
            items.insert(items.begin(), {"Starting " + g_launching + "\xE2\x80\xA6", false});
        for (auto &[msg, err] : items) {
            int px = m.dpi(t.text_md);
            int wrap = (int) std::min(safe.w * 0.7f, m.dp(1100));
            auto tex = e.text_cache().get(e.sdl(), e.fonts(), t.font, px, msg, wrap, 3, false);
            if (!tex) continue;
            float pw = tex->w + m.dp(64), ph = tex->h + m.dp(36);
            float x = safe.x + (safe.w - pw) / 2;
            y -= ph;
            SDL_FRect r{x, y, pw, ph};
            c.shadow(r, m.dp(ph / 2), m.dp(30), t.shadow);
            c.rounded(r, std::min(ph / 2, m.dp(28)), err ? mix(t.surface_hi, t.danger, 0.45f) : t.surface_hi);
            c.texture(tex->tex, nullptr, {x + m.dp(32), y + m.dp(18), (float) tex->w, (float) tex->h}, t.text);
            y -= m.dp(12);
        }
    }
}
