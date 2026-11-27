#pragma once

#include <cctype>
#include <cstdint>
#include <cstddef>
#include <string>

namespace Foyer {
    // Semantic input actions. All input sources (keyboard, gamepad, evdev,
    // CEC) are mapped to these before reaching screens. Text input is handled
    // separately via SDL text events.
    enum class Action {
        None,
        Up, Down, Left, Right,
        Accept, Back, Home, Menu,
        VolUp, VolDown, Mute,
    };


    inline const char* action_name(Action a)
    {
        switch (a) {
            case Action::Up: return "Up";
            case Action::Down: return "Down";
            case Action::Left: return "Left";
            case Action::Right: return "Right";
            case Action::Accept: return "Accept";
            case Action::Back: return "Back";
            case Action::Home: return "Home";
            case Action::Menu: return "Menu";
            case Action::VolUp: return "VolUp";
            case Action::VolDown: return "VolDown";
            case Action::Mute: return "Mute";
            default: return "None";
        }
    }

    // Parse an action name (case-insensitive). "" or "None" -> Action::None.
    inline Action parse_action(const std::string &s)
    {
        // lowered names, matched against a lowered input
        static const std::pair<const char*, Action> table[] = {
            {"none", Action::None}, {"up", Action::Up}, {"down", Action::Down},
            {"left", Action::Left}, {"right", Action::Right},
            {"accept", Action::Accept}, {"back", Action::Back},
            {"home", Action::Home}, {"menu", Action::Menu},
            {"volup", Action::VolUp}, {"voldown", Action::VolDown}, {"mute", Action::Mute},
        };
        std::string l = s;
        for (char &c : l) c = (char) std::tolower((unsigned char) c);
        for (auto &[n, a] : table)
            if (l == n) return a;
        return Action::None;
    }

    // Actions assignable in the air mouse remap, in cycle order.
    inline const Action* remap_actions(size_t &count)
    {
        static const Action list[] = {
            Action::None, Action::Up, Action::Down, Action::Left, Action::Right,
            Action::Accept, Action::Back, Action::Home, Action::Menu,
            Action::VolUp, Action::VolDown, Action::Mute,
        };
        count = std::size(list);
        return list;
    }

    // Air mouse defaults, from the XING WEI 2.4G capture: buttons arrive as
    // MSC_SCAN HID usages (page<<16 | usage<<8).
    //   0x090001 Button 1 (left click)   0x010081 Desktop Y (wheel)
    //   0x070065 System Power            0x07002A keyboard 0x2A (back/del)
    //   0x0C0042..0x0C0045 enter/menu 1..3, 0x0C0223 mute, 0x0C00CF del,
    //   0x0C0224 return, 0x0C00EA vol down, 0x0C00E9 vol up
    inline std::string airmouse_default_label(int code)
    {
        switch (code) {
            case 0x090001: return "Left click";
            case 0x010081: return "Wheel";
            case 0x070065: return "Power";
            case 0x07002A: return "Click";
            case 0x0C00CF: return "Del";
            case 0x0C0223: return "Mute";
            case 0x0C0224: return "Return";
            case 0x0C00EA: return "Vol down";
            case 0x0C00E9: return "Vol up";
            case 0x0C0042: return "Menu 2";
            case 0x0C0043: return "Menu 3";
            case 0x0C0044: return "Enter";
            case 0x0C0045: return "Menu 1";
            default: return "Button";
        }
    }

    // Default action for an air mouse code ("None" = unassigned by default).
    inline std::string airmouse_default_action(int code)
    {
        switch (code) {
            case 0x090001: return "Accept";   // left click
            case 0x07002A: return "Accept";   // the click button
            case 0x0C0044: return "Accept";   // enter
            case 0x0C0224: return "Accept";   // return
            case 0x0C00CF: return "Back";     // del
            case 0x0C0223: return "Mute";     // mute
            case 0x0C00EA: return "VolDown";  // vol down
            case 0x0C00E9: return "VolUp";    // vol up
            default: return "None";
        }
    }
}
