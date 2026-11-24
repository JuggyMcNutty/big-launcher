#pragma once

#include "../../ui/list_screen.hpp"

namespace BL {
    // In-UI layout editor. Three levels:
    //  LayoutEditor (menu list) -> LayoutMenuEditor (entry list) ->
    //  LayoutEntryActions (edit title/command, move, delete).
    // Edits mutate App::layout() directly; saving the editor writes layout.xml.
    class LayoutEditor : public ListScreen {
    public:
        explicit LayoutEditor(App &app);
        void on_show() override;
    protected:
        void on_select(int index) override;
        bool on_back_override() override;
    private:
        void rebuild();
    };

    class LayoutMenuEditor : public ListScreen {
    public:
        LayoutMenuEditor(App &app, int menu_index);
        void on_show() override;
    protected:
        void on_select(int index) override;
    private:
        void rebuild();
        int menu_index_;
    };

    class LayoutEntryActions : public ListScreen {
    public:
        LayoutEntryActions(App &app, int menu_index, int entry_index);
        void on_show() override;
    protected:
        void on_select(int index) override;
    private:
        int menu_index_, entry_index_;
        enum { act_title, act_command, act_up, act_down, act_delete };
    };
}
