#include <algorithm>

#include "editor.hpp"
#include "../core/app.hpp"
#include "../ui/engine/focus.hpp"

namespace Foyer {
    using namespace ui;

    void rebuild_page(Layer &page, Engine &e)
    {
        std::string fid = Focus::current() && page.root->contains(Focus::current()) ? Focus::current()->id : "";
        Focus::set(nullptr, false);
        page.root = std::make_unique<Node>();
        page.initial_focus = nullptr;
        page.build(e);
        Focus::set_scope(page.root.get());
        Node *f = fid.empty() ? nullptr : page.root->find(fid);
        if (!f) f = Focus::first_in(page.root.get());
        Focus::set(f, false);
        invalidate_layout();
    }

    static const char* size_label(TileSize s)
    {
        switch (s) { case TileSize::Small: return "Small"; case TileSize::Large: return "Large"; default: return "Medium"; }
    }

    static void prompt(App &app, const std::string &title, const std::string &init, std::function<void(const std::string&)> f)
    {
        app.ui().push(std::make_unique<KeyboardSheet>(app, title, init, false, std::move(f)));
    }

    template <typename T> static void move_item(std::vector<T> &v, int i, int d)
    {
        int j = i + d;
        if (i < 0 || j < 0 || i >= (int) v.size() || j >= (int) v.size()) return;
        std::swap(v[i], v[j]);
    }

    // ---- LayoutEditor -------------------------------------------------------------
    void LayoutEditor::on_enter(Engine &e) { rebuild_page(*this, e); }

    void LayoutEditor::fill(Engine &e, Node &b)
    {
        (void) e;
        auto &L = app_.layout();
        kit::section(&b, "Tabs  \xC2\xB7  Left / Right to reorder");
        for (int i = 0; i < (int) L.tabs.size(); i++) {
            auto &t = L.tabs[i];
            std::string kind = t.kind == "library" ? "All apps" : t.kind == "settings" ? "Settings" :
                               std::to_string(t.rows.size()) + " rows";
            auto *r = row(t.title, kind, [this, i] { app_.ui().push(std::make_unique<TabEditor>(app_, i)); }, "layout");
            r->id = "tab" + std::to_string(i);
            r->on_adjust = [this, i, &e](Action a) {
                move_item(app_.layout().tabs, i, a == Action::Right ? 1 : -1);
                app_.save_layout();
                rebuild_page(*this, e);
                if (Node *n = root->find("tab" + std::to_string(std::clamp(i + (a == Action::Right ? 1 : -1), 0, (int) app_.layout().tabs.size() - 1)))) Focus::set(n);
                return true;
            };
        }
        row("Add tab", "", [this, &e] {
            prompt(app_, "New tab name", "", [this, &e](const std::string &name) {
                if (name.empty()) return;
                Tab t; t.title = name; t.id = slug(name);
                Row r; r.title = "Favourites"; t.rows.push_back(r);
                auto &tabs = app_.layout().tabs;
                // keep Settings last
                auto it = std::find_if(tabs.begin(), tabs.end(), [](auto &x) { return x.kind == "settings"; });
                tabs.insert(it, t);
                app_.save_layout();
                rebuild_page(*this, e);
            });
        }, "plus");
        kit::section(&b, "Apps");
        for (auto &[id, a] : app_.apps()) {
            std::string aid = id;
            row(a.title, "Edit", [this, aid] { app_.ui().push(std::make_unique<AppEditor>(app_, aid)); }, "app", a.command);
        }
        row("Add app", "", [this] {
            prompt(app_, "App name", "", [this](const std::string &name) {
                if (name.empty()) return;
                std::string id = slug(name);
                AppEntry a; a.id = id; a.title = name; a.command = "foyer-app " + id;
                app_.save_app(a);
                app_.ui().push(std::make_unique<AppEditor>(app_, id, true));
            });
        }, "plus");
        kit::section(&b, "Reset");
        row("Restore default layout", "", [this, &e] {
            app_.ui().push(std::make_unique<ConfirmDialog>(app_, "Restore the default layout?",
                "Your tabs and rows are replaced (a .bak copy is kept). Apps are not removed.", "Restore", "Cancel",
                [this, &e](bool yes) {
                    if (!yes) return;
                    std::string def = Paths::find_data("layout.toml");
                    for (auto &d : Paths::data_dirs()) {
                        if (d == Paths::config_dir()) continue;
                        std::string p = d + "/layout.toml";
                        if (FILE *f = fopen(p.c_str(), "r")) { fclose(f); def = p; break; }
                    }
                    Layout l = load_layout(def);
                    if (l.tabs.empty()) { app_.toast("No default layout found", true); return; }
                    app_.layout() = l;
                    app_.save_layout();
                    rebuild_page(*this, e);
                    app_.toast("Default layout restored");
                }));
        }, "restart");
    }

    // ---- TabEditor ----------------------------------------------------------------
    TabEditor::TabEditor(App &app, int tab): Page(app, "Edit tab"), tab_(tab) {}
    void TabEditor::on_enter(Engine &e) { rebuild_page(*this, e); }

    void TabEditor::fill(Engine &e, Node &b)
    {
        (void) e;
        auto &L = app_.layout();
        if (tab_ >= (int) L.tabs.size()) { kit::info_row(&b, "Tab no longer exists", ""); return; }
        Tab &t = L.tabs[tab_];
        title_ = t.title;
        row("Name", t.title, [this, &e] {
            prompt(app_, "Tab name", app_.layout().tabs[tab_].title, [this, &e](const std::string &n) {
                if (n.empty()) return;
                app_.layout().tabs[tab_].title = n;
                app_.save_layout();
                rebuild_page(*this, e);
            });
        }, "layout");
        if (t.kind == "rows") {
            kit::section(&b, "Rows  \xC2\xB7  Left / Right to reorder");
            for (int i = 0; i < (int) t.rows.size(); i++) {
                auto &r = t.rows[i];
                auto *rb = row(r.title.empty() ? "(untitled row)" : r.title,
                               std::to_string(r.tiles.size()) + " tiles  \xC2\xB7  " + size_label(r.size),
                               [this, i] { app_.ui().push(std::make_unique<RowEditor>(app_, tab_, i)); }, "layout");
                rb->id = "row" + std::to_string(i);
                rb->on_adjust = [this, i, &e](Action a) {
                    move_item(app_.layout().tabs[tab_].rows, i, a == Action::Right ? 1 : -1);
                    app_.save_layout();
                    rebuild_page(*this, e);
                    return true;
                };
            }
            row("Add row", "", [this, &e] {
                prompt(app_, "Row title", "", [this, &e](const std::string &n) {
                    Row r; r.title = n;
                    app_.layout().tabs[tab_].rows.push_back(r);
                    app_.save_layout();
                    rebuild_page(*this, e);
                });
            }, "plus");
        } else {
            kit::info_row(&b, "Type", t.kind == "library" ? "All apps (automatic)" : "Settings (built in)");
        }
        if (t.kind != "settings") {
            kit::section(&b, "Danger zone");
            row("Delete tab", "", [this] {
                app_.ui().push(std::make_unique<ConfirmDialog>(app_, "Delete this tab?", app_.layout().tabs[tab_].title,
                    "Delete", "Cancel", [this](bool yes) {
                        if (!yes) return;
                        auto &tabs = app_.layout().tabs;
                        if (tab_ < (int) tabs.size()) tabs.erase(tabs.begin() + tab_);
                        app_.save_layout();
                        app_.ui().pop(false);
                    }));
            }, "close");
        }
    }

    // ---- RowEditor ----------------------------------------------------------------
    RowEditor::RowEditor(App &app, int tab, int row): Page(app, "Edit row"), tab_(tab), row_(row) {}
    void RowEditor::on_enter(Engine &e) { rebuild_page(*this, e); }

    void RowEditor::fill(Engine &e, Node &b)
    {
        (void) e;
        auto &L = app_.layout();
        if (tab_ >= (int) L.tabs.size() || row_ >= (int) L.tabs[tab_].rows.size()) { kit::info_row(&b, "Row no longer exists", ""); return; }
        Row &r = L.tabs[tab_].rows[row_];
        auto R = [this]() -> Row& { return app_.layout().tabs[tab_].rows[row_]; };
        row("Title", r.title, [this, &e, R] {
            prompt(app_, "Row title", R().title, [this, &e, R](const std::string &n) { R().title = n; app_.save_layout(); rebuild_page(*this, e); });
        }, "layout");
        auto *sz = row("Tile size", size_label(r.size), nullptr, "layout", "Left / Right to change");
        sz->on_adjust = [this, &e, R](Action a) {
            int v = std::clamp((int) R().size + (a == Action::Right ? 1 : -1), 0, 2);
            R().size = (TileSize) v;
            for (auto &t : R().tiles) t.size = R().size;
            app_.save_layout();
            rebuild_page(*this, e);
            return true;
        };
        kit::section(&b, "Tiles  \xC2\xB7  Left / Right to reorder, A for options");
        for (int i = 0; i < (int) r.tiles.size(); i++) {
            Tile &t = r.tiles[i];
            std::string name = t.title;
            if (name.empty() && app_.apps().count(t.app)) name = app_.apps()[t.app].title;
            if (name.empty()) name = t.app.empty() ? t.command : t.app;
            auto *tb = row(name, size_label(t.size), nullptr, "app");
            tb->id = "tile" + std::to_string(i);
            tb->on_adjust = [this, i, &e, R](Action a) {
                int j = i + (a == Action::Right ? 1 : -1);
                move_item(R().tiles, i, a == Action::Right ? 1 : -1);
                app_.save_layout();
                rebuild_page(*this, e);
                if (Node *n = root->find("tile" + std::to_string(std::clamp(j, 0, (int) R().tiles.size() - 1)))) Focus::set(n);
                return true;
            };
            tb->on_activate = [this, i, &e, R, name] {
                std::vector<std::string> opts{"Edit app details", "Make small", "Make medium", "Make large", "Remove from row"};
                app_.ui().push(std::make_unique<ChoiceSheet>(app_, name, opts, 0, [this, i, &e, R](int k) {
                    if (i >= (int) R().tiles.size()) return;
                    Tile &t = R().tiles[i];
                    if (k == 0) {
                        if (!t.app.empty()) app_.ui().push(std::make_unique<AppEditor>(app_, t.app));
                        else app_.toast("This tile is an inline command; edit layout.toml");
                        return;
                    }
                    if (k >= 1 && k <= 3) t.size = (TileSize) (k - 1);
                    if (k == 4) R().tiles.erase(R().tiles.begin() + i);
                    app_.save_layout();
                    rebuild_page(*this, e);
                }));
            };
        }
        row("Add tile", "", [this, &e, R] {
            std::vector<std::string> names, ids;
            for (auto &[id, a] : app_.apps()) { names.push_back(a.title); ids.push_back(id); }
            if (names.empty()) { app_.toast("No apps installed"); return; }
            app_.ui().push(std::make_unique<ChoiceSheet>(app_, "Add tile", names, 0, [this, &e, R, ids](int k) {
                Tile t; t.app = ids[k]; t.size = R().size;
                R().tiles.push_back(t);
                app_.save_layout();
                rebuild_page(*this, e);
            }));
        }, "plus");
        kit::section(&b, "Danger zone");
        row("Delete row", "", [this, R] {
            app_.ui().push(std::make_unique<ConfirmDialog>(app_, "Delete this row?", R().title, "Delete", "Cancel", [this](bool yes) {
                if (!yes) return;
                auto &rows = app_.layout().tabs[tab_].rows;
                if (row_ < (int) rows.size()) rows.erase(rows.begin() + row_);
                app_.save_layout();
                app_.ui().pop(false);
            }));
        }, "close");
    }

    // ---- AppEditor ----------------------------------------------------------------
    AppEditor::AppEditor(App &app, std::string id, bool is_new): Page(app, "Edit app"), id_(std::move(id)), new_(is_new) {}
    void AppEditor::on_enter(Engine &e) { rebuild_page(*this, e); }

    void AppEditor::fill(Engine &e, Node &b)
    {
        (void) e;
        auto it = app_.apps().find(id_);
        if (it == app_.apps().end()) { kit::info_row(&b, "App not found", id_); return; }
        title_ = it->second.title;
        auto edit = [this, &e](std::function<void(AppEntry&)> f) {
            AppEntry a = app_.apps()[id_];
            f(a);
            a.source.clear();
            app_.save_app(a);
            rebuild_page(*this, e);
        };
        const AppEntry &a = it->second;
        auto text_field = [this, edit](const std::string &label, const std::string &cur, std::string AppEntry::*field, const std::string &icon) {
            row(label, cur.empty() ? "--" : cur, [this, edit, label, cur, field] {
                prompt(app_, label, cur, [edit, field](const std::string &v) { edit([&](AppEntry &x) { x.*field = v; }); });
            }, icon);
        };
        kit::section(&b, "Details");
        text_field("Title", a.title, &AppEntry::title, "app");
        text_field("Subtitle", a.subtitle, &AppEntry::subtitle, "info");
        text_field("Command", a.command, &AppEntry::command, "play");
        kit::section(&b, "Artwork");
        auto art_field = [this, edit](const std::string &label, const std::string &cur, std::string AppEntry::*field, const std::string &hint) {
            std::string shown = cur.empty() ? "None" : cur.substr(cur.rfind('/') == std::string::npos ? 0 : cur.rfind('/') + 1);
            row(label, shown, [this, edit, label, field] {
                std::vector<std::string> opts{"Choose image\xE2\x80\xA6", "Remove"};
                app_.ui().push(std::make_unique<ChoiceSheet>(app_, label, opts, 0, [this, edit, label, field](int k) {
                    if (k == 1) { edit([&](AppEntry &x) { x.*field = ""; }); return; }
                    app_.ui().push(std::make_unique<FilePicker>(app_, label, "/etc/foyer/art",
                        std::vector<std::string>{".png", ".jpg", ".jpeg", ".webp", ".svg"},
                        [edit, field](const std::string &p) { edit([&](AppEntry &x) { x.*field = p; }); }));
                }));
            }, "palette", hint);
        };
        art_field("Logo", a.icon, &AppEntry::icon, "Centred on the tile");
        art_field("Tile art", a.tile, &AppEntry::tile, "Full-bleed 16:9 image (replaces the logo)");
        art_field("Background art", a.hero, &AppEntry::hero, "Shown behind the home screen when focused");
        static const std::pair<const char*, const char*> colors[] = {
            {"Midnight", "#0B1A33"}, {"Slate", "#1E2430"}, {"Red", "#C4161C"}, {"Blue", "#0A5BD8"},
            {"Teal", "#0E7C7B"}, {"Purple", "#5B2A86"}, {"Green", "#167A3E"}, {"Orange", "#C2410C"}, {"Black", "#000000"}};
        std::string cname = a.background.empty() ? "Default" : a.background;
        for (auto &[n, v] : colors) if (a.background == v) cname = n;
        row("Tile colour", cname, [this, edit] {
            std::vector<std::string> names{"Default"};
            for (auto &[n, v] : colors) names.push_back(n);
            app_.ui().push(std::make_unique<ChoiceSheet>(app_, "Tile colour", names, 0, [edit](int k) {
                edit([&](AppEntry &x) { x.background = k == 0 ? "" : colors[k - 1].second; });
            }));
        }, "palette");
        kit::section(&b, "Placement");
        row("Pin to Home", "", [this] {
            auto &L = app_.layout();
            for (auto &t : L.tabs)
                if (t.kind == "rows" && !t.rows.empty()) {
                    Tile tile; tile.app = id_; tile.size = t.rows[0].size;
                    t.rows[0].tiles.push_back(tile);
                    app_.save_layout();
                    app_.toast("Added to " + t.title + " \xE2\x86\x92 " + t.rows[0].title);
                    return;
                }
            app_.toast("No row to pin to \xE2\x80\x94 add one in Layout", true);
        }, "home");
        row("Launch now", "", [this] { app_.launch(app_.apps()[id_]); }, "play");
        if (new_) kit::info_row(&b, "Saved to", Paths::config_dir() + "/apps.d/" + id_ + ".toml");
    }
}
