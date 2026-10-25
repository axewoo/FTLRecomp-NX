#include "gamepad.hpp"
#include "log.hpp"
#include "render.hpp"
#include <algorithm>
#include <cmath>
#include <utility>
#ifdef __SWITCH__
#include <switch.h>
#endif

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
    pollTouch(actions);
    if (pad_ == nullptr) {
        return actions;
    }
#ifdef __SWITCH__
    // sdl3-nx reports Xbox labels for the Joy-Cons, so use Nintendo positions.
    const auto confirm = SDL_GAMEPAD_BUTTON_EAST;
    const auto cancel = SDL_GAMEPAD_BUTTON_SOUTH;
    const auto pause = SDL_GAMEPAD_BUTTON_NORTH;
    const auto jump = SDL_GAMEPAD_BUTTON_WEST;
#else
    const auto confirm = labelled(pad_, SDL_GAMEPAD_BUTTON_LABEL_A, SDL_GAMEPAD_BUTTON_LABEL_CROSS);
    const auto cancel = labelled(pad_, SDL_GAMEPAD_BUTTON_LABEL_B, SDL_GAMEPAD_BUTTON_LABEL_CIRCLE);
    const auto pause = labelled(pad_, SDL_GAMEPAD_BUTTON_LABEL_X, SDL_GAMEPAD_BUTTON_LABEL_TRIANGLE);
    const auto jump = labelled(pad_, SDL_GAMEPAD_BUTTON_LABEL_Y, SDL_GAMEPAD_BUTTON_LABEL_SQUARE);
#endif
    const Binding keys[] = {{pause, "Space"},
                            {jump, "J"},
                            {SDL_GAMEPAD_BUTTON_LEFT_SHOULDER, "Q"},
                            {SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER, "U"},
                            {SDL_GAMEPAD_BUTTON_DPAD_UP, "1"},
                            {SDL_GAMEPAD_BUTTON_DPAD_RIGHT, "2"},
                            {SDL_GAMEPAD_BUTTON_DPAD_DOWN, "3"},
                            {SDL_GAMEPAD_BUTTON_DPAD_LEFT, "4"},
                            {SDL_GAMEPAD_BUTTON_START, "Escape"},
                            {SDL_GAMEPAD_BUTTON_LEFT_STICK, "V"},
                            {SDL_GAMEPAD_BUTTON_RIGHT_STICK, "C"}};

    // Left stick moves the cursor; the right stick moves it at a quarter
    // speed for precise targeting. ZR (right trigger) speeds it up.
    const float boost =
        SDL_GetGamepadAxis(pad_, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER) > 16000 ? cursorBoost : 1.f;
    const float dx = axis(pad_, SDL_GAMEPAD_AXIS_LEFTX) + axis(pad_, SDL_GAMEPAD_AXIS_RIGHTX) / 4;
    const float dy = axis(pad_, SDL_GAMEPAD_AXIS_LEFTY) + axis(pad_, SDL_GAMEPAD_AXIS_RIGHTY) / 4;
    if (dx != 0 || dy != 0) {
        // A late frame must not throw the cursor: cap the step at 1/30 s.
        const float step = cursorSpeed * boost * float(std::min(seconds, 1.0 / 30));
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
    // Hold Minus (Back) for the controls card.
    help_ = current[SDL_GAMEPAD_BUTTON_BACK];
    std::copy(std::begin(current), std::end(current), std::begin(previous_));
    return actions;
}

void GamepadCursor::pollTouch(std::vector<GamepadAction>& actions) {
#ifdef __SWITCH__
    // sdl3-nx turns off touch-to-mouse and reports finger-down as finger-up,
    // so read the touch screen directly. Its coordinates are already 1280x720.
    HidTouchScreenState state{};
    if (hidGetTouchScreenStates(&state, 1) == 0) {
        return;
    }
    if (state.count > 0) {
        const float x = std::clamp(float(state.touches[0].x), 0.f, 1279.f);
        const float y = std::clamp(float(state.touches[0].y), 0.f, 719.f);
        if (!touchHeld_) {
            touchHeld_ = true;
            x_ = x;
            y_ = y;
            actions.push_back({GamepadAction::Type::Move, x_, y_, {}});
            actions.push_back({GamepadAction::Type::LeftDown, x_, y_, {}});
        } else if (x != x_ || y != y_) {
            x_ = x;
            y_ = y;
            actions.push_back({GamepadAction::Type::Move, x_, y_, {}});
        }
    } else if (touchHeld_) {
        touchHeld_ = false;
        actions.push_back({GamepadAction::Type::LeftUp, x_, y_, {}});
    }
#else
    (void)actions;
#endif
}

void GamepadCursor::drawHelp(Renderer& renderer) const {
    if (!help_) {
        return;
    }
    static constexpr std::pair<const char*, const char*> rows[] = {
        {"Left stick", "Move cursor"},
        {"Right stick", "Move cursor slowly"},
        {"ZR (hold)", "Faster cursor"},
        {"A", "Click / hold to drag"},
        {"B", "Deselect / cancel / power down"},
        {"ZL + A", "Shift-click"},
        {"X", "Pause"},
        {"Y", "Jump / star map"},
        {"L", "Select all crew"},
        {"R", "Ship upgrades"},
        {"D-pad up, right, down, left", "Weapons 1, 2, 3, 4"},
        {"Left stick click", "Toggle autofire"},
        {"Right stick click", "Cloak"},
        {"+", "Menu"},
        {"Touch screen", "Tap to click, drag to move"},
        {"- (hold)", "This card"},
    };
    // Undo any clip, tint or zoom the game frame left behind.
    renderer.setClip(std::nullopt);
    renderer.setTint(1);
    renderer.setTransform();
    renderer.windowFrame({290, 80, 700, 560});
    auto& title = renderer.font("fonts/JustinFont12Bold.font");
    renderer.text(title, "CONTROLS", 640, 104, 2, {255, 255, 255, 255}, true);
    auto& font = renderer.font("fonts/JustinFont12Bold.font");
    float y = 160;
    for (const auto& [button, action] : rows) {
        renderer.text(font, button, 330, y, 1, {255, 230, 94, 255});
        renderer.text(font, action, 640, y, 1, {255, 255, 255, 255});
        y += 28;
    }
}
} // namespace ftl
