#pragma once

#include <string>
#include <vector>

#include "screen.hpp"

namespace BL {
    class Renderer;
    class Theme;

    // Base for list-style screens (settings pages, hubs). Draws a title, a
    // scrollable list of rows (text + optional value/subtext), and a footer.
    // Subclasses provide items and handle selection. Input is D-pad/keyboard/
    // gamepad navigation + Accept + Back.
    class ListScreen : public Screen {
    public:
        explicit ListScreen(App &app);
        void draw() override;
        bool on_action(Action action) override;

        void set_title(const std::string &t) { title_ = t; }
        void set_footer(const std::string &f) { footer_ = f; }
        void clear_items();
        void add_item(const std::string &text, const std::string &value = "", const std::string &subtext = "");
        void set_enabled(int i, bool e);
        void set_value(int i, const std::string &v);
        void set_text(int i, const std::string &t);
        int  selected() const { return index_; }
        void set_selected(int i);
        size_t count() const { return items_.size(); }
        const std::string& item_text(int i) const { return items_[i].text; }
        const std::string& item_value(int i) const { return items_[i].value; }

    protected:
        struct Item { std::string text, value, subtext; bool enabled = true; };

        // Called when the selected item is accepted.
        virtual void on_select(int index) = 0;
        // Called on Back; return true to consume (not pop).
        virtual bool on_back_override() { return false; }
        // Called when the list is first shown / refreshed.
        virtual void on_refresh() {}

        std::vector<Item> items_;
        int index_ = 0;
        int view_offset_ = 0;
        std::string title_;
        std::string footer_;

        int row_height() const;
        int rows_visible() const;
        void clamp_view();
        void move(int delta);
    };
}
