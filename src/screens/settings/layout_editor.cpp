#include "layout_editor.hpp"
#include "../../core/app.hpp"
#include "../../ui/text_input.hpp"
#include "../../render/util.hpp"

// ---- LayoutEditor (menu list) ------------------------------------------
Foyer::LayoutEditor::LayoutEditor(App &app): ListScreen(app)
{
    set_title("Layout");
    set_footer("A: open menu  B: save & back");
}

void Foyer::LayoutEditor::rebuild()
{
    clear_items();
    for (const auto &m : app_.layout().menus)
        add_item(m.title, std::to_string(m.entries.size()) + " items");
}

void Foyer::LayoutEditor::on_show() { rebuild(); }

void Foyer::LayoutEditor::on_select(int index)
{
    if (index < 0 || index >= (int) app_.layout().menus.size()) return;
    app_.play_select();
    app_.screens().push(std::make_unique<LayoutMenuEditor>(app_, index));
}

bool Foyer::LayoutEditor::on_back_override()
{
    // Save on the way out.
    app_.save_layout();
    app_.show_toast("Layout saved");
    return false; // let it pop
}

// ---- LayoutMenuEditor (entry list) -------------------------------------
Foyer::LayoutMenuEditor::LayoutMenuEditor(App &app, int menu_index): ListScreen(app), menu_index_(menu_index)
{
    set_title("Menu: " + app.layout().menus[menu_index].title);
    set_footer("A: edit entry  B: back");
}

void Foyer::LayoutMenuEditor::rebuild()
{
    clear_items();
    const auto &menu = app_.layout().menus[menu_index_];
    for (const auto &e : menu.entries)
        add_item(e.title, e.command);
    add_item("+ Add entry");
}

void Foyer::LayoutMenuEditor::on_show() { rebuild(); }

void Foyer::LayoutMenuEditor::on_select(int index)
{
    auto &menu = app_.layout().menus[menu_index_];
    if (index < (int) menu.entries.size()) {
        app_.play_select();
        app_.screens().push(std::make_unique<LayoutEntryActions>(app_, menu_index_, index));
    }
    else {
        // Add entry: prompt for title then command
        app_.screens().push(std::make_unique<TextInput>(app_, "New entry title",
            [this](std::string title) {
                if (title.empty()) return;
                app_.screens().push(std::make_unique<TextInput>(app_, "Command",
                    [this, title](std::string command) {
                        auto &m = app_.layout().menus[menu_index_];
                        Entry e;
                        e.title = title;
                        e.command = command;
                        m.entries.push_back(std::move(e));
                        app_.save_layout();
                        app_.show_toast("Added \"" + title + "\"");
                        rebuild();
                    }));
            }));
    }
}

// ---- LayoutEntryActions -------------------------------------------------
Foyer::LayoutEntryActions::LayoutEntryActions(App &app, int menu_index, int entry_index)
    : ListScreen(app), menu_index_(menu_index), entry_index_(entry_index)
{
    set_title("Entry: " + app.layout().menus[menu_index].entries[entry_index].title);
    set_footer("A: do action  B: back");
}

void Foyer::LayoutEntryActions::on_show()
{
    clear_items();
    add_item("Edit title");
    add_item("Edit command", app_.layout().menus[menu_index_].entries[entry_index_].command);
    add_item("Move up");
    add_item("Move down");
    add_item("Delete");
}

void Foyer::LayoutEntryActions::on_select(int index)
{
    auto &menu = app_.layout().menus[menu_index_];
    if (entry_index_ < 0 || entry_index_ >= (int) menu.entries.size()) return;
    Entry &e = menu.entries[entry_index_];
    switch (index) {
        case act_title:
            app_.screens().push(std::make_unique<TextInput>(app_, "Title",
                [this](std::string t) {
                    if (!t.empty()) {
                        app_.layout().menus[menu_index_].entries[entry_index_].title = t;
                        app_.save_layout();
                    }
                }));
            return;
        case act_command: {
            std::string current = e.command;
            auto ti = std::make_unique<TextInput>(app_, "Command",
                [this](std::string c) {
                    app_.layout().menus[menu_index_].entries[entry_index_].command = c;
                    app_.save_layout();
                });
            ti->set_initial(current);
            app_.screens().push(std::move(ti));
            return;
        }
        case act_up:
            if (entry_index_ > 0) {
                std::swap(menu.entries[entry_index_], menu.entries[entry_index_ - 1]);
                entry_index_--;
                app_.save_layout();
            }
            return;
        case act_down:
            if (entry_index_ + 1 < (int) menu.entries.size()) {
                std::swap(menu.entries[entry_index_], menu.entries[entry_index_ + 1]);
                entry_index_++;
                app_.save_layout();
            }
            return;
        case act_delete:
            menu.entries.erase(menu.entries.begin() + entry_index_);
            app_.save_layout();
            app_.show_toast("Deleted entry");
            app_.screens().pop(); // back to menu editor (which will rebuild on show)
            return;
        default:
            return;
    }
}
