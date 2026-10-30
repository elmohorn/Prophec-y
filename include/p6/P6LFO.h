#pragma once

#include <cstdint>

namespace p6 {

// One global LFO, matching the Prophet-6 architecture. Triangle and Random are
// bipolar; saw, reverse saw and square are unipolar. Random at the very top of
// its rate range becomes white noise, per the hardware manual.
class P6LFO {
public:
    enum class Shape : std::uint8_t { Triangle = 0, Saw, ReverseSaw, Square, Random };

    void prepare(double sampleRate);
    void reset();
    void setRate(double hz);
    void setShape(Shape shape);
    void setNoiseMode(bool enabled);
    double process();

    // Diagnostic-only observation used by deterministic regression tests. It
    // does not participate in host state or synthesis control.
    [[nodiscard]] double phaseForDiagnostics() const noexcept { return phase_; }

private:
    static double polyBlep(double t, double dt);
    double whiteNoise();

    double sampleRate_{48000.0};
    double rateHz_{1.0};
    double phase_{};
    double sampleHold_{};
    Shape shape_{Shape::Triangle};
    bool noiseMode_{};
    std::uint32_t rng_{0x1f351f35u};
};

} // namespace p6
