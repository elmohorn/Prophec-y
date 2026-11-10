#pragma once

#include <algorithm>
#include <cmath>
#include <optional>

namespace p6plugin::ui {

inline constexpr int kLedRingSegments = 25;

struct LedSegmentState {
    bool lit{};
    bool tip{};
};

// The ring and the needle use the Slider's normalized position. A semantic
// zero may sit away from the geometric centre (for example coarse tuning).
inline LedSegmentState ledSegmentState(int index, float position,
                                      std::optional<float> zero = {}) noexcept
{
    position = std::isfinite(position) ? std::clamp(position, 0.f, 1.f) : 0.f;
    const int selected = static_cast<int>(std::lround(position * (kLedRingSegments - 1)));
    if (zero && std::isfinite(*zero)) {
        const int centre = static_cast<int>(std::lround(
            std::clamp(*zero, 0.f, 1.f) * (kLedRingSegments - 1)));
        const bool lit = selected != centre
            && index >= std::min(selected, centre) && index <= std::max(selected, centre);
        return {lit, lit && index == selected};
    }
    const bool lit = position > 0.f && index >= 0 && index <= selected;
    return {lit, lit && index == selected};
}

} // namespace p6plugin::ui
