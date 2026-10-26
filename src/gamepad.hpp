#pragma once
#include "render.hpp"
#include <SDL3/SDL.h>
#include <functional>
#include <optional>
#include <string>
#include <vector>

// Native gamepad support. This has no original-game counterpart: the original
// is mouse and keyboard only. A virtual cursor driven by the left stick stands
// in for the mouse, and face/shoulder buttons emit the default FTL hotkeys.
// In snap mode the left stick instead jumps between the boxes and buttons the
// current screen hit-tests, which a target source reports on demand.
namespace ftl {

struct GamepadAction {
    enum class Type { Move, LeftDown, LeftUp, RightClick, KeyDown, KeyUp };
    Type type;
    float x = 0;
    float y = 0;
    std::string key;
    bool shift = false;
};

class GamepadCursor {
public:
    // Call after SDL_Init. Opens any gamepad already connected.
    void init();
    void shutdown();
    // Feed every SDL event: handles hot-plugging and keeps the virtual cursor
    // where a real mouse or a touch last left it (logical 1280x720 coordinates).
    void handleEvent(const SDL_Event& event, float logicalX, float logicalY);
    // Polls the gamepad once per frame and returns the menu actions it caused.
    std::vector<GamepadAction> update(double seconds);

    bool leftHeld() const {
        return leftHeld_ || touchHeld_;
    }

    // Reports the clickable rectangles of the current screen. Calling it may
    // move the game's pointer; update() moves it back afterwards.
    void targetSource(std::function<std::vector<Rect>()> source) {
        targets_ = std::move(source);
    }

    // Draws the snap highlight, the mode notice and, while Minus is held, the
    // controls card. Call after the frame.
    void drawOverlay(Renderer& renderer) const;

    bool connected() const {
        return pad_ != nullptr;
    }

private:
    struct Binding {
        SDL_GamepadButton button;
        const char* key;
    };

    SDL_Gamepad* pad_ = nullptr;
    float x_ = 640;
    float y_ = 360;
    bool leftHeld_ = false;
    bool touchHeld_ = false;
    bool help_ = false;
    bool previous_[SDL_GAMEPAD_BUTTON_COUNT] = {};
    // Snap mode state. Minus tapped toggles the mode, held shows the card.
    bool snap_ = true;
    double minusHeld_ = -1;
    double noticeLeft_ = 0;
    double repeatLeft_ = 0;
    bool stickHeld_ = false;
    double refreshLeft_ = -1;
    bool refreshAgain_ = false;
    double time_ = 0;
    std::optional<Rect> selected_;
    std::vector<Rect> cached_;
    std::function<std::vector<Rect>()> targets_;
    void open(SDL_JoystickID id);
    void pollTouch(std::vector<GamepadAction>& actions);
    std::vector<Rect> probe();
    void navigate(float dx, float dy, std::vector<GamepadAction>& actions);
    void refreshSelection(std::vector<GamepadAction>& actions);
};
} // namespace ftl
