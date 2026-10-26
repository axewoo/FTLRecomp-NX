#pragma once
#include "render.hpp"
#include <cstdint>
#include <deque>

// Optional frame-timing display (ZL + ZR + right stick click, or F11). This has
// no original-game counterpart; it exists to check performance on hardware.
namespace ftl {

class PerfOverlay {
public:
    void toggle() {
        visible_ = !visible_;
    }
    bool visible() const {
        return visible_;
    }
    // One presented frame: wall time since the previous present, and the CPU
    // time spent updating the game and building the frame before presenting.
    void frame(double intervalMs, double updateMs, double drawMs);
    void draw(Renderer& renderer) const;

private:
    bool visible_ = false;
    // Running sums over the current half-second window.
    int frames_ = 0;
    double windowMs_ = 0;
    double worstMs_ = 0;
    double updateMs_ = 0;
    double drawMs_ = 0;
    // Values shown, refreshed every half second.
    double fps_ = 0;
    double averageMs_ = 0;
    double shownWorstMs_ = 0;
    double shownUpdateMs_ = 0;
    double shownDrawMs_ = 0;
    // Wall-clock ages (ms) of recent frames that took over 25 ms.
    double clockMs_ = 0;
    std::deque<double> stutters_;
};
} // namespace ftl
