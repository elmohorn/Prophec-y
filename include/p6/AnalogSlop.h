#pragma once

#include <cstdint>

namespace p6 {

// Slow independent oscillator drift. This is deliberately a random walk / held
// random target rather than note-on random detune, so a sustained note keeps
// moving as a real VCO does. Adapted from the MIT-licensed approach in
// joshjetson/phosphor; see THIRD_PARTY_NOTICES.md.
class AnalogSlop {
public:
    void prepare(double sampleRate, std::uint32_t seed);
    void reset();
    double process(); // normalized -1..+1

private:
    static std::uint32_t mix32(std::uint32_t x);
    double nextNoise();

    double sampleRate_{48000.0};
    std::uint32_t state_{1u};
    std::uint32_t stride_{1u};
    double target_{};
    double value_{};
    double updatePhase_{};
    double updateStep_{};
    double smoothing_{};
};

} // namespace p6
