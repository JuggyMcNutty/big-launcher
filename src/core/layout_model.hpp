#pragma once

#include <string>
#include <vector>

namespace Foyer {
    struct Card {
        std::string icon;        // icon path
        std::string background;  // #RRGGBB color or image path
        std::string custom;      // custom card image path (optional)
        int icon_margin = 12;    // percent
    };

    struct Entry {
        std::string title;
        std::string command;
        Card card;
    };

    struct Menu {
        std::string title;
        std::vector<Entry> entries;
    };

    struct SidebarCommand {
        std::string title;
        std::string command;
    };

    // Read + write layout.xml (libxml2). Menus own entries by value.
    class LayoutModel {
    public:
        std::vector<Menu> menus;
        std::vector<SidebarCommand> sidebar_commands;
        int columns = 3;

        void parse(const std::string &file);
        void write(const std::string &file) const;

        Menu* find_menu(const std::string &title);
    };
}
