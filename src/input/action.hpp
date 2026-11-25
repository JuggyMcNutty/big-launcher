#pragma once

#include <cstdint>

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

    // SDL user event used to deliver global input (evdev/CEC) to the main
    // thread; payload is an Action in user.data1.
    static constexpr uint32_t EVT_GLOBAL_ACTION = 0x80000003;

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
}
