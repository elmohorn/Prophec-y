#pragma once

#include <array>
#include <optional>

namespace p6 {

enum class HardSyncCorrectionMode {
    LegacyStepOnly,
    StepAndSlope
};

struct HardSyncTrace {
    bool occurred{};
    double syncFraction{};
    double phaseBeforeReset{};
    double resetPhase{};
    double valueBeforeReset{};
    double valueAfterReset{};
    double valueJump{};
    double slopeBeforePerSample{};
    double slopeAfterPerSample{};
    double slopeJumpPerSample{};
    double stepCorrectionBefore{};
    double stepCorrectionAfter{};
    double slopeCorrectionBefore{};
    double slopeCorrectionAfter{};
    double totalCorrectionBefore{};
    double totalCorrectionAfter{};
    double endPhase{};
};

// Morphing Prophet-style oscillator. The waveform is represented as one
// continuously varying trapezoid family (triangle -> saw -> pulse), rather
// than crossfading independent oscillators. Corners are band-limited with a
// two-sided polyBLAMP residual and hard-sync is resolved at sub-sample time.
//
// The implementation is adapted from the MIT-licensed Prophet-6 oscillator in
// joshjetson/phosphor; see THIRD_PARTY_NOTICES.md.
class PolyBLEPOscillator {
public:
    void prepare(double sampleRate);
    void reset(double phase01 = 0.0);
    void setFrequency(double hz);
    void setShape(double shape01);       // 0 triangle -> 0.5 saw -> 1 pulse
    void setPulseWidth(double width01);  // centre = square
    // Amount of the legacy duty-dependent peak normalization. 1.0 reproduces
    // the Phosphor-derived candidate; 0.0 removes that extra attenuation while
    // still removing DC. Calibration only until Reference A supports a value.
    void setMeanNormalizationAmount(double amount01);
    // Hard-sync slave reset phase. Hardware reset polarity/phase is not
    // published; default 0.0 preserves the v0.8 behavior.
    void setSyncResetPhase(double phase01);
    void setHardSyncCorrectionMode(HardSyncCorrectionMode mode) { hardSyncCorrectionMode_ = mode; }
    const HardSyncTrace& lastHardSyncTrace() const noexcept { return lastHardSyncTrace_; }

    // Fraction 0..1 within the upcoming sample at which this oscillator wraps.
    // Used when this oscillator acts as the hard-sync master.
    std::optional<double> nextWrapFraction() const;
    // Fraction within the upcoming sample when an arbitrary oscillator phase
    // is crossed. phase=0 is equivalent to the ordinary wrap event.
    std::optional<double> nextPhaseCrossingFraction(double phase01) const;

    // Render one sample. If syncAt is supplied, this oscillator is reset to
    // phase zero at that sub-sample position with an anti-aliased correction.
    double process(std::optional<double> syncAt = std::nullopt);
    // Nominal phase-domain shape used by this oscillator, before anti-alias
    // correction, hard sync or modulation. UI previews use the same law.
    static double nominalWaveform(double phase01,double shape01,double pulseWidth01,
                                  double normalizationAmount=1.0);

    double phase() const noexcept { return phase_; }
    bool wrappedLastSample() const noexcept { return wrapped_; }
    double lastWrapOvershoot() const noexcept { return wrapOvershoot_; }

private:
    struct Shape {
        double rise{};
        double high{};
        double fall{};
        double mean{};
        double scale{1.0};

        static Shape at(double morph, double duty, double normalizationAmount);
        double value(double phase) const;
        double slopePerPhase(double phase) const;
        std::array<std::array<double, 3>, 2> edges() const;
    };

    struct Stretch {
        double from{};
        double span{};
        double t0{};
        double t1{1.0};
        double place(double reached) const;
    };

    struct Correction {
        double before{};
        double after{};
    };

    struct ClosingCorner {
        double phase{};
        double slopeChange{};
    };

    static double wrapPhase(double phase);
    static void corner(double slopeChange, double t, Correction& correction);
    void walk(const Shape& shape, const Stretch& stretch, double dt, Correction& correction);

    double sampleRate_{48000.0};
    double frequency_{440.0};
    double shape_{0.5};
    double pulseWidth_{0.5};
    double meanNormalizationAmount_{1.0};
    double syncResetPhase_{0.0};
    HardSyncCorrectionMode hardSyncCorrectionMode_{HardSyncCorrectionMode::LegacyStepOnly};
    HardSyncTrace lastHardSyncTrace_{};
    double phase_{0.0};
    double held_{0.0};
    std::array<std::optional<ClosingCorner>, 2> closing_{};
    bool wrapped_{false};
    double wrapOvershoot_{};
};

} // namespace p6

