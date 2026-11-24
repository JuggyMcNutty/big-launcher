#pragma once

#include <optional>

#include <SDL3/SDL.h>

#include "action.hpp"

namespace BL {
    // Maps raw input (keyboard keys, gamepad buttons/axes) to semantic Actions.
    class ActionMapper {
    public:
        // nullopt = key is not a navigation action (e.g. a letter key)
        static std::optional<Action> keyboard(SDL_Keycode key);
        static std::optional<Action> gamepad_button(SDL_GamepadButton button);
        // Returns a direction action if the axis exceeds deadzone
        static std::optional<Action> gamepad_axis(SDL_GamepadAxis axis, int value, int deadzone = 10000);
    };
}
