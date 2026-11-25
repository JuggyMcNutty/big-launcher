#pragma once

#include <string>

#include "kit.hpp"
#include "keyboard.hpp"

namespace Foyer {
    class App;

    // Layout editor: tabs -> rows -> tiles. Every change is saved
    // (atomic + .bak) and the home screen rebuilds when you go back.
    class LayoutEditor : public Page {
    public:
        explicit LayoutEditor(App &app): Page(app, "Layout", "Tabs, rows and tiles") {}
        void on_enter(ui::Engine &e) override;
    protected:
        void fill(ui::Engine &e, ui::Node &body) override;
    };

    class TabEditor : public Page {
    public:
        TabEditor(App &app, int tab);
        void on_enter(ui::Engine &e) override;
    protected:
        void fill(ui::Engine &e, ui::Node &body) override;
    private:
        int tab_;
    };

    class RowEditor : public Page {
    public:
        RowEditor(App &app, int tab, int row);
        void on_enter(ui::Engine &e) override;
    protected:
        void fill(ui::Engine &e, ui::Node &body) override;
    private:
        int tab_, row_;
    };

    // Per-app metadata + artwork (writes /etc/foyer/apps.d/<id>.toml).
    class AppEditor : public Page {
    public:
        AppEditor(App &app, std::string id, bool is_new = false);
        void on_enter(ui::Engine &e) override;
    protected:
        void fill(ui::Engine &e, ui::Node &body) override;
    private:
        std::string id_;
        bool new_;
    };

    // Rebuild the current page in place (keeps focus by id where possible).
    void rebuild_page(ui::Layer &page, ui::Engine &e);
}
