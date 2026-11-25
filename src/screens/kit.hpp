#pragma once

#include <functional>
#include <string>
#include <vector>

#include "../ui/engine/engine.hpp"
#include "../ui/engine/scroller.hpp"
#include "../ui/engine/widgets.hpp"

namespace Foyer {
    class App;

    // Shared building blocks for Big-Picture style pages.
    namespace kit {
        using namespace Foyer::ui;

        // A focusable list row: icon? + label (+ sub) ... value / chevron.
        class RowButton : public Button {
        public:
            RowButton(App &app, const std::string &label, const std::string &value = "",
                      const std::string &icon = "", const std::string &sub = "");
            Image *icon_img = nullptr;
            std::function<bool(Action)> on_adjust;   // Left/Right
            void set_value(const std::string &v) { value->set(v); value->visible = !v.empty(); invalidate_layout(); }
            bool tick(Engine &e, float dt) override;
        };

        // Non-focusable key/value info line.
        Node* info_row(Node *parent, const std::string &k, const std::string &v, Text **value_out = nullptr);
        // Section heading inside a list.
        Text* section(Node *parent, const std::string &title);

        // Vertical scrolling list area. Returns the inner column to add rows to.
        Scroller* list(Node *parent, float max_width_dp = 1100);

        // Page header: big title + optional subtitle.
        Node* header(Node *parent, const std::string &title, const std::string &subtitle = "", Text **sub_out = nullptr);

        // Footer hint bar ("A Select   B Back").
        Node* hints(Node *parent, const std::vector<std::pair<std::string, std::string>> &items);

        // Rounded pill toggle text.
        inline std::string onoff(bool v) { return v ? "On" : "Off"; }
    }

    // A full-screen settings-style page with header, scrolling list and hints.
    class Page : public ui::Layer {
    public:
        Page(App &app, std::string title, std::string subtitle = "");
        void build(ui::Engine &e) override;
        void paint_under(ui::Engine &e, ui::Canvas &c) override;
    protected:
        App &app_;
        std::string title_, subtitle_;
        ui::Scroller *list_ = nullptr;
        ui::Node *body_ = nullptr;
        ui::Text *subtitle_text_ = nullptr;
        virtual void fill(ui::Engine &e, ui::Node &body) = 0;
        kit::RowButton* row(const std::string &label, const std::string &value = "",
                            std::function<void()> on_activate = nullptr, const std::string &icon = "",
                            const std::string &sub = "");
        void refocus_first();
    };

    // Modal choice list (sheet from the right).
    class ChoiceSheet : public ui::Layer {
    public:
        ChoiceSheet(App &app, std::string title, std::vector<std::string> options, int selected,
                    std::function<void(int)> on_pick);
        void build(ui::Engine &e) override;
    private:
        App &app_;
        std::string title_;
        std::vector<std::string> options_;
        int selected_;
        std::function<void(int)> on_pick_;
    };

    // Yes/No confirmation (optionally with a countdown that picks `default_no`).
    class ConfirmDialog : public ui::Layer {
    public:
        ConfirmDialog(App &app, std::string title, std::string message, std::string yes, std::string no,
                      std::function<void(bool)> done, int countdown_s = 0);
        void build(ui::Engine &e) override;
        bool tick(ui::Engine &e, float dt) override;
    private:
        App &app_;
        std::string title_, message_, yes_, no_;
        std::function<void(bool)> done_;
        int countdown_;
        Uint64 started_ = 0;
        ui::Text *msg_ = nullptr;
        bool fired_ = false;
        void finish(ui::Engine &e, bool yes);
    };
}
