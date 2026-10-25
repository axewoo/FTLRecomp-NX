#pragma once
#include <SDL3/SDL.h>
#include <string>
#include <vector>

// Native gamepad support. This has no original-game counterpart: the original
// is mouse and keyboard only. A virtual cursor driven by the left stick stands
// in for the mouse, and face/shoulder buttons emit the default FTL hotkeys.
namespace ftl {
class Renderer;

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

    // Draws the controls card while Minus is held. Call after the frame.
    void drawHelp(Renderer& renderer) const;

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
    void open(SDL_JoystickID id);
    void pollTouch(std::vector<GamepadAction>& actions);
};
} // namespace ftl
