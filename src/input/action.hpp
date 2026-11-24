#pragma once

namespace BL {
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
}
