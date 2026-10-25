#include "gamepad.hpp"
#include "log.hpp"
#include <algorithm>
#include <cmath>

namespace ftl {
namespace {
// Cursor speed in logical pixels per second at full stick deflection.
constexpr float cursorSpeed = 900;
constexpr float cursorBoost = 2.2f;
constexpr float deadZone = 0.18f;

// Face buttons are found by their printed label, so A is the confirm button on
// Nintendo, Xbox and PlayStation (Cross) layouts alike.
SDL_GamepadButton labelled(SDL_Gamepad* pad, SDL_GamepadButtonLabel a, SDL_GamepadButtonLabel b) {
    for (const auto button : {SDL_GAMEPAD_BUTTON_SOUTH,
                              SDL_GAMEPAD_BUTTON_EAST,
                              SDL_GAMEPAD_BUTTON_WEST,
                              SDL_GAMEPAD_BUTTON_NORTH}) {
        const auto label = SDL_GetGamepadButtonLabel(pad, button);
        if (label == a || label == b) {
            return button;
        }
    }
    return SDL_GAMEPAD_BUTTON_INVALID;
}

float axis(SDL_Gamepad* pad, SDL_GamepadAxis which) {
    const float value = float(SDL_GetGamepadAxis(pad, which)) / 32767.f;
    if (std::abs(value) < deadZone) {
        return 0;
    }
    // Rescale past the dead zone and square for fine control near centre.
    const float scaled = (std::abs(value) - deadZone) / (1 - deadZone);
    return std::copysign(std::min(scaled * scaled, 1.f), value);
}
} // namespace

void GamepadCursor::init() {
    if (!SDL_InitSubSystem(SDL_INIT_GAMEPAD)) {
        logging::warning("gamepad", "gamepad init failed: {}", SDL_GetError());
        return;
    }
    int count = 0;
    SDL_JoystickID* ids = SDL_GetGamepads(&count);
    if (ids != nullptr) {
        if (count > 0) {
            open(ids[0]);
        }
        SDL_free(ids);
    }
}

void GamepadCursor::shutdown() {
    if (pad_ != nullptr) {
        SDL_CloseGamepad(pad_);
        pad_ = nullptr;
    }
}

void GamepadCursor::open(SDL_JoystickID id) {
    if (pad_ != nullptr) {
        return;
    }
    pad_ = SDL_OpenGamepad(id);
    if (pad_ != nullptr) {
        logging::info("gamepad", "using {}", SDL_GetGamepadName(pad_));
        std::fill(std::begin(previous_), std::end(previous_), true);
    }
}

void GamepadCursor::handleEvent(const SDL_Event& event, float logicalX, float logicalY) {
    if (event.type == SDL_EVENT_GAMEPAD_ADDED) {
        open(event.gdevice.which);
    } else if (event.type == SDL_EVENT_GAMEPAD_REMOVED && pad_ != nullptr &&
               SDL_GetGamepadID(pad_) == event.gdevice.which) {
        shutdown();
        leftHeld_ = false;
    } else if (event.type == SDL_EVENT_MOUSE_MOTION || event.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
        x_ = logicalX;
        y_ = logicalY;
    }
}

std::vector<GamepadAction> GamepadCursor::update(double seconds) {
    std::vector<GamepadAction> actions;
    if (pad_ == nullptr) {
        return actions;
    }
    const auto confirm = labelled(pad_, SDL_GAMEPAD_BUTTON_LABEL_A, SDL_GAMEPAD_BUTTON_LABEL_CROSS);
    const auto cancel = labelled(pad_, SDL_GAMEPAD_BUTTON_LABEL_B, SDL_GAMEPAD_BUTTON_LABEL_CIRCLE);
    const auto pause = labelled(pad_, SDL_GAMEPAD_BUTTON_LABEL_X, SDL_GAMEPAD_BUTTON_LABEL_TRIANGLE);
    const auto jump = labelled(pad_, SDL_GAMEPAD_BUTTON_LABEL_Y, SDL_GAMEPAD_BUTTON_LABEL_SQUARE);
    const Binding keys[] = {{pause, "Space"},
                            {jump, "J"},
                            {SDL_GAMEPAD_BUTTON_LEFT_SHOULDER, "Q"},
                            {SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER, "U"},
                            {SDL_GAMEPAD_BUTTON_DPAD_UP, "1"},
                            {SDL_GAMEPAD_BUTTON_DPAD_RIGHT, "2"},
                            {SDL_GAMEPAD_BUTTON_DPAD_DOWN, "3"},
                            {SDL_GAMEPAD_BUTTON_DPAD_LEFT, "4"},
                            {SDL_GAMEPAD_BUTTON_START, "Escape"},
                            {SDL_GAMEPAD_BUTTON_BACK, "I"},
                            {SDL_GAMEPAD_BUTTON_LEFT_STICK, "V"},
                            {SDL_GAMEPAD_BUTTON_RIGHT_STICK, "C"}};

    // Left stick moves the cursor; the right stick moves it at a quarter
    // speed for precise targeting. ZR (right trigger) speeds it up.
    const float boost =
        SDL_GetGamepadAxis(pad_, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER) > 16000 ? cursorBoost : 1.f;
    const float dx = axis(pad_, SDL_GAMEPAD_AXIS_LEFTX) + axis(pad_, SDL_GAMEPAD_AXIS_RIGHTX) / 4;
    const float dy = axis(pad_, SDL_GAMEPAD_AXIS_LEFTY) + axis(pad_, SDL_GAMEPAD_AXIS_RIGHTY) / 4;
    if (dx != 0 || dy != 0) {
        const float step = cursorSpeed * boost * float(std::min(seconds, 0.1));
        x_ = std::clamp(x_ + dx * step, 0.f, 1279.f);
        y_ = std::clamp(y_ + dy * step, 0.f, 719.f);
        actions.push_back({GamepadAction::Type::Move, x_, y_, {}});
    }

    // ZL (left trigger) acts as Shift for clicks.
    const bool shift = SDL_GetGamepadAxis(pad_, SDL_GAMEPAD_AXIS_LEFT_TRIGGER) > 16000;
    bool current[SDL_GAMEPAD_BUTTON_COUNT] = {};
    for (int button = 0; button < SDL_GAMEPAD_BUTTON_COUNT; ++button) {
        current[button] = SDL_GetGamepadButton(pad_, SDL_GamepadButton(button));
    }
    auto pressed = [&](SDL_GamepadButton button) {
        return button != SDL_GAMEPAD_BUTTON_INVALID && current[button] && !previous_[button];
    };
    auto released = [&](SDL_GamepadButton button) {
        return button != SDL_GAMEPAD_BUTTON_INVALID && !current[button] && previous_[button];
    };

    if (pressed(confirm)) {
        leftHeld_ = true;
        actions.push_back({GamepadAction::Type::LeftDown, x_, y_, {}, shift});
    } else if (released(confirm) && leftHeld_) {
        leftHeld_ = false;
        actions.push_back({GamepadAction::Type::LeftUp, x_, y_, {}});
    }
    if (pressed(cancel)) {
        actions.push_back({GamepadAction::Type::RightClick, x_, y_, {}});
    }
    for (const auto& binding : keys) {
        if (pressed(binding.button)) {
            actions.push_back({GamepadAction::Type::KeyDown, x_, y_, binding.key});
        } else if (released(binding.button)) {
            actions.push_back({GamepadAction::Type::KeyUp, x_, y_, binding.key});
        }
    }
    std::copy(std::begin(current), std::end(current), std::begin(previous_));
    return actions;
}
} // namespace ftl
