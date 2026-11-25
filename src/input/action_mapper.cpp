#include "action_mapper.hpp"
#include <cmath>

std::optional<Foyer::Action> Foyer::ActionMapper::keyboard(SDL_Keycode key)
{
    switch (key) {
        case SDLK_UP:    return Action::Up;
        case SDLK_DOWN:  return Action::Down;
        case SDLK_LEFT:  return Action::Left;
        case SDLK_RIGHT: return Action::Right;
        case SDLK_RETURN:
        case SDLK_KP_ENTER:
        case SDLK_SPACE: return Action::Accept;
        case SDLK_ESCAPE: return Action::Back;
        default: return std::nullopt;
    }
}

std::optional<Foyer::Action> Foyer::ActionMapper::gamepad_button(SDL_GamepadButton button)
{
    switch (button) {
        case SDL_GAMEPAD_BUTTON_DPAD_UP:    return Action::Up;
        case SDL_GAMEPAD_BUTTON_DPAD_DOWN:  return Action::Down;
        case SDL_GAMEPAD_BUTTON_DPAD_LEFT:  return Action::Left;
        case SDL_GAMEPAD_BUTTON_DPAD_RIGHT: return Action::Right;
        case SDL_GAMEPAD_BUTTON_SOUTH:      return Action::Accept;
        case SDL_GAMEPAD_BUTTON_EAST:       return Action::Back;
        case SDL_GAMEPAD_BUTTON_GUIDE:      return Action::Home;
        case SDL_GAMEPAD_BUTTON_START:      return Action::Menu;
        case SDL_GAMEPAD_BUTTON_LEFT_SHOULDER:  return Action::VolUp;
        case SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER: return Action::VolDown;
        default: return std::nullopt;
    }
}

std::optional<Foyer::Action> Foyer::ActionMapper::gamepad_axis(SDL_GamepadAxis axis, int value, int deadzone)
{
    if (std::abs(value) < deadzone)
        return std::nullopt;
    switch (axis) {
        case SDL_GAMEPAD_AXIS_LEFTX: return value < 0 ? Action::Left : Action::Right;
        case SDL_GAMEPAD_AXIS_LEFTY: return value < 0 ? Action::Up   : Action::Down;
        default: return std::nullopt;
    }
}
