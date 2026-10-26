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
// Snap mode: stick deflection that counts as a push, and its repeat delays.
constexpr float snapPush = 0.5f;
constexpr double snapFirstRepeat = 0.35;
constexpr double snapRepeat = 0.12;
// Minus released sooner than this toggles the mode instead of showing help.
constexpr double minusTap = 0.3;
// Delays after a click or hotkey before the selection is checked again, so
// the screen it opened has appeared; the second catches slower fades.
constexpr double snapSettle = 0.3;
constexpr double snapSettleAgain = 0.7;

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

float rawAxis(SDL_Gamepad* pad, SDL_GamepadAxis which) {
    return float(SDL_GetGamepadAxis(pad, which)) / 32767.f;
}

bool covers(const Rect& r, float x, float y) {
    return x >= r.x && x <= r.x + r.w && y >= r.y && y <= r.y + r.h;
}

bool same(const Rect& a, const Rect& b) {
    return std::abs(a.x - b.x) < 1 && std::abs(a.y - b.y) < 1 && std::abs(a.w - b.w) < 1 &&
           std::abs(a.h - b.h) < 1;
}

// The smallest target under the point, which is the one a click would hit.
const Rect* under(const std::vector<Rect>& targets, float x, float y) {
    const Rect* found = nullptr;
    for (const auto& r : targets) {
        if (covers(r, x, y) && (found == nullptr || r.w * r.h < found->w * found->h)) {
            found = &r;
        }
    }
    return found;
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
        selected_.reset();
    }
}

std::vector<Rect> GamepadCursor::probe() {
    std::vector<Rect> targets;
    if (!targets_) {
        return targets;
    }
    for (const auto& r : targets_()) {
        // Skip slivers, doors (the free cursor still reaches them, and they
        // would sit between every pair of rooms), whole-screen panels and
        // anything off screen.
        if (r.w < 4 || r.h < 4 || (std::min(r.w, r.h) <= 12 && std::max(r.w, r.h) <= 24) ||
            r.w * r.h > 1280 * 720 * .35f || r.x + r.w <= 0 ||
            r.y + r.h <= 0 || r.x >= 1280 || r.y >= 720) {
            continue;
        }
        if (std::none_of(targets.begin(), targets.end(), [&](const Rect& t) { return same(t, r); })) {
            targets.push_back(r);
        }
    }
    cached_ = targets;
    return targets;
}

void GamepadCursor::navigate(float dx, float dy, std::vector<GamepadAction>& actions) {
    // While A is held the probe would drag whatever is held, so reuse the
    // targets found before the press.
    const auto targets = leftHeld() ? cached_ : probe();
    const float length = std::hypot(dx, dy);
    const float ux = dx / length;
    const float uy = dy / length;
    const Rect* current = under(targets, x_, y_);
    const Rect* best = nullptr;
    float bestScore = 0;
    // Look in a cone around the push first, then anywhere on that side.
    for (const float spread : {2.f, 1000.f}) {
        for (const auto& r : targets) {
            // Leave the current box, and never stop on a box that encloses it.
            if ((current != nullptr && same(r, *current)) || covers(r, x_, y_)) {
                continue;
            }
            // Measure to the nearest point of the box, so wide boxes such as
            // event choices are as close as they look.
            // Its centre must still lie ahead.
            const float vx = std::clamp(x_, r.x, r.x + r.w) - x_;
            const float vy = std::clamp(y_, r.y, r.y + r.h) - y_;
            const float along = vx * ux + vy * uy;
            const float across = std::abs(vx * uy - vy * ux);
            const float ahead = (r.x + r.w / 2 - x_) * ux + (r.y + r.h / 2 - y_) * uy;
            if (ahead < 2 || across > std::max(along, 1.f) * spread) {
                continue;
            }
            const float score = along + across * 2.5f;
            if (best == nullptr || score < bestScore) {
                best = &r;
                bestScore = score;
            }
        }
        if (best != nullptr) {
            break;
        }
    }
    if (best == nullptr && current == nullptr) {
        // Off every box (a new screen): start from the nearest one.
        for (const auto& r : targets) {
            const float score = std::hypot(r.x + r.w / 2 - x_, r.y + r.h / 2 - y_);
            if (best == nullptr || score < bestScore) {
                best = &r;
                bestScore = score;
            }
        }
    }
    if (best != nullptr) {
        selected_ = *best;
        x_ = best->x + best->w / 2;
        y_ = best->y + best->h / 2;
    } else if (current != nullptr) {
        selected_ = *current;
    }
    // Also puts the pointer back where the probe found it.
    actions.push_back({GamepadAction::Type::Move, x_, y_, {}});
}

void GamepadCursor::refreshSelection(std::vector<GamepadAction>& actions) {
    const auto targets = probe();
    const Rect* current = under(targets, x_, y_);
    if (current == nullptr && !targets.empty()) {
        // The click changed the screen: select the nearest box on the new one.
        float bestScore = 0;
        for (const auto& r : targets) {
            const float score = std::hypot(r.x + r.w / 2 - x_, r.y + r.h / 2 - y_);
            if (current == nullptr || score < bestScore) {
                current = &r;
                bestScore = score;
            }
        }
        x_ = current->x + current->w / 2;
        y_ = current->y + current->h / 2;
    }
    if (current != nullptr) {
        selected_ = *current;
    } else {
        selected_.reset();
    }
    actions.push_back({GamepadAction::Type::Move, x_, y_, {}});
}

std::vector<GamepadAction> GamepadCursor::update(double seconds) {
    std::vector<GamepadAction> actions;
    time_ += seconds;
    noticeLeft_ = std::max(0.0, noticeLeft_ - seconds);
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

    // Free cursor: the left stick moves the cursor and the right stick moves
    // it at a quarter speed for precise targeting. Snap mode: the left stick
    // jumps between boxes and the right stick moves a free cursor at half
    // speed, for beam aiming. ZR (right trigger) speeds the cursor up.
    const float boost =
        SDL_GetGamepadAxis(pad_, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER) > 16000 ? cursorBoost : 1.f;
    float dx = axis(pad_, SDL_GAMEPAD_AXIS_RIGHTX) / 4;
    float dy = axis(pad_, SDL_GAMEPAD_AXIS_RIGHTY) / 4;
    if (snap_) {
        dx *= 2;
        dy *= 2;
        const float sx = rawAxis(pad_, SDL_GAMEPAD_AXIS_LEFTX);
        const float sy = rawAxis(pad_, SDL_GAMEPAD_AXIS_LEFTY);
        if (std::hypot(sx, sy) > snapPush) {
            repeatLeft_ -= seconds;
            if (!stickHeld_ || repeatLeft_ <= 0) {
                repeatLeft_ = stickHeld_ ? snapRepeat : snapFirstRepeat;
                stickHeld_ = true;
                navigate(sx, sy, actions);
            }
        } else {
            stickHeld_ = false;
        }
    } else {
        dx += axis(pad_, SDL_GAMEPAD_AXIS_LEFTX);
        dy += axis(pad_, SDL_GAMEPAD_AXIS_LEFTY);
    }
    if (dx != 0 || dy != 0) {
        // A late frame must not throw the cursor: cap the step at 1/30 s.
        const float step = cursorSpeed * boost * float(std::min(seconds, 1.0 / 30));
        x_ = std::clamp(x_ + dx * step, 0.f, 1279.f);
        y_ = std::clamp(y_ + dy * step, 0.f, 719.f);
        selected_.reset();
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
    // ZL + ZR + right stick click toggles the performance overlay instead of
    // cloaking; the click's release is swallowed too.
    if (pressed(SDL_GAMEPAD_BUTTON_RIGHT_STICK) && shift && boost > 1) {
        perfToggle_ = true;
        perfCombo_ = true;
    }
    bool acted = released(confirm);
    if (pressed(cancel)) {
        acted = true;
        actions.push_back({GamepadAction::Type::RightClick, x_, y_, {}});
    }
    for (const auto& binding : keys) {
        if (perfCombo_ && binding.button == SDL_GAMEPAD_BUTTON_RIGHT_STICK) {
            perfCombo_ = current[binding.button];
            continue;
        }
        if (pressed(binding.button)) {
            acted = true;
            actions.push_back({GamepadAction::Type::KeyDown, x_, y_, binding.key});
        } else if (released(binding.button)) {
            actions.push_back({GamepadAction::Type::KeyUp, x_, y_, binding.key});
        }
    }
    // In snap mode, re-check the selection once whatever was pressed settles.
    if (snap_ && acted) {
        refreshLeft_ = snapSettle;
        refreshAgain_ = true;
    }
    if (refreshLeft_ >= 0 && !leftHeld()) {
        refreshLeft_ -= seconds;
        if (refreshLeft_ < 0) {
            refreshSelection(actions);
            if (std::exchange(refreshAgain_, false)) {
                refreshLeft_ = snapSettleAgain;
            }
        }
    }
    // Tap Minus (Back) to switch modes; hold it for the controls card.
    if (current[SDL_GAMEPAD_BUTTON_BACK]) {
        minusHeld_ = minusHeld_ < 0 ? 0 : minusHeld_ + seconds;
    } else if (minusHeld_ >= 0) {
        if (minusHeld_ < minusTap) {
            snap_ = !snap_;
            noticeLeft_ = 1.5;
            selected_.reset();
            if (snap_) {
                refreshSelection(actions);
            }
        }
        minusHeld_ = -1;
    }
    help_ = minusHeld_ >= minusTap;
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
            selected_.reset();
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

void GamepadCursor::drawOverlay(Renderer& renderer) const {
    if (!help_ && noticeLeft_ <= 0 && !(snap_ && selected_)) {
        return;
    }
    // Undo any clip, tint or zoom the game frame left behind.
    renderer.setClip(std::nullopt);
    renderer.setTint(1);
    renderer.setTransform();
    if (snap_ && selected_ && !help_) {
        // A pulsing frame around the selected box: bright inside a dark
        // edge, so it shows on the pale buttons and the dark hull alike.
        auto frame = [&](float inset, float width, SDL_Color color) {
            const Rect r{selected_->x - inset,
                         selected_->y - inset,
                         selected_->w + inset * 2,
                         selected_->h + inset * 2};
            renderer.fill({r.x, r.y, r.w, width}, color);
            renderer.fill({r.x, r.y + r.h - width, r.w, width}, color);
            renderer.fill({r.x, r.y + width, width, r.h - width * 2}, color);
            renderer.fill({r.x + r.w - width, r.y + width, width, r.h - width * 2}, color);
        };
        const float pulse = .5f + .5f * float(std::sin(time_ * 6));
        frame(6, 2, {0, 0, 0, 200});
        frame(4, 3, {255, Uint8(150 + 80 * pulse), Uint8(40 * pulse), 255});
        frame(1, 1, {0, 0, 0, 200});
    }
    auto& font = renderer.font("fonts/JustinFont12Bold.font");
    if (noticeLeft_ > 0 && !help_) {
        renderer.windowFrame({490, 6, 300, 72});
        renderer.text(font,
                      snap_ ? "SNAP SELECTION" : "FREE CURSOR",
                      640,
                      34,
                      1,
                      {255, 255, 255, 255},
                      true);
    }
    if (!help_) {
        return;
    }
    using Row = std::pair<const char*, const char*>;
    static constexpr Row snapRows[] = {
        {"Left stick", "Jump between buttons and boxes"},
        {"Right stick", "Move a free cursor (beam aiming)"},
    };
    static constexpr Row freeRows[] = {
        {"Left stick", "Move cursor"},
        {"Right stick", "Move cursor slowly"},
    };
    const Row rows[] = {
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
        {"ZL + ZR + right stick click", "FPS / frame time display"},
        {"+", "Menu"},
        {"Touch screen", "Tap to click, drag to move"},
        {"- (tap)", snap_ ? "Switch to free cursor" : "Switch to snap selection"},
        {"- (hold)", "This card"},
    };
    renderer.windowFrame({290, 66, 700, 588});
    renderer.text(font, "CONTROLS", 640, 88, 2, {255, 255, 255, 255}, true);
    float y = 142;
    auto row = [&](const Row& value) {
        renderer.text(font, value.first, 330, y, 1, {255, 230, 94, 255});
        renderer.text(font, value.second, 640, y, 1, {255, 255, 255, 255});
        y += 28;
    };
    for (const auto& value : snap_ ? snapRows : freeRows) {
        row(value);
    }
    for (const auto& value : rows) {
        row(value);
    }
}
} // namespace ftl
