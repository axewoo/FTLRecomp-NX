#include "perf_overlay.hpp"
#include <algorithm>
#include <cstdio>
#include <optional>
#ifdef __SWITCH__
#include <switch.h>
#endif

namespace ftl {
namespace {
constexpr double refreshMs = 500;
constexpr double stutterMs = 25;
constexpr double stutterWindowMs = 10'000;

std::string format(const char* pattern, auto... values) {
    char buffer[128];
    std::snprintf(buffer, sizeof buffer, pattern, values...);
    return buffer;
}
} // namespace

void PerfOverlay::frame(double intervalMs, double updateMs, double drawMs) {
    clockMs_ += intervalMs;
    if (intervalMs > stutterMs) {
        stutters_.push_back(clockMs_);
    }
    while (!stutters_.empty() && clockMs_ - stutters_.front() > stutterWindowMs) {
        stutters_.pop_front();
    }
    ++frames_;
    windowMs_ += intervalMs;
    worstMs_ = std::max(worstMs_, intervalMs);
    updateMs_ += updateMs;
    drawMs_ += drawMs;
    if (windowMs_ >= refreshMs) {
        fps_ = frames_ * 1000 / windowMs_;
        averageMs_ = windowMs_ / frames_;
        shownWorstMs_ = worstMs_;
        shownUpdateMs_ = updateMs_ / frames_;
        shownDrawMs_ = drawMs_ / frames_;
        frames_ = 0;
        windowMs_ = worstMs_ = updateMs_ = drawMs_ = 0;
    }
}

void PerfOverlay::draw(Renderer& renderer) const {
    if (!visible_) {
        return;
    }
    renderer.setClip(std::nullopt);
    renderer.setTint(1);
    renderer.setTransform();
    std::string memory = format("Textures %.0f MB", renderer.textureBytes() / 1048576.0);
#ifdef __SWITCH__
    u64 used = 0;
    u64 total = 0;
    if (R_SUCCEEDED(svcGetInfo(&used, InfoType_UsedMemorySize, CUR_PROCESS_HANDLE, 0)) &&
        R_SUCCEEDED(svcGetInfo(&total, InfoType_TotalMemorySize, CUR_PROCESS_HANDLE, 0))) {
        memory += format("   RAM %.0f / %.0f MB", used / 1048576.0, total / 1048576.0);
    }
#endif
    const std::string lines[] = {
        format("%.1f FPS   frame %.1f ms   worst %.1f ms", fps_, averageMs_, shownWorstMs_),
        format("CPU: game %.2f ms   drawing %.2f ms", shownUpdateMs_, shownDrawMs_),
        format("Stutters over 25 ms (last 10 s): %d", int(stutters_.size())),
        memory,
    };
    auto& font = renderer.font("fonts/JustinFont10.font");
    renderer.fill({880, 0, 400, 82}, {0, 0, 0, 190});
    float y = 6;
    for (const auto& line : lines) {
        renderer.text(font, line, 890, y, 1, {255, 255, 255, 255});
        y += 18;
    }
}
} // namespace ftl
