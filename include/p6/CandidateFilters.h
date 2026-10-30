#pragma once

#include "p6/FilterModels.h"

namespace p6 {

// TPT state-variable 2-pole resonant high-pass. The Q law follows the
// dedicated P6 candidate in Phosphor rather than a generic linear-Q mapping.
class CandidateHighPass2Pole final : public HighPass2Pole {
public:
    void prepare(double sampleRate) override;
    void reset() override;
    double process(double input, double cutoffHz, double resonance) override;

private:
    double sampleRate_{48000.0};
    double s1_{};
    double s2_{};
};

// Four TPT one-pole integrators with compensated global resonance feedback.
// The forward path stays linear; saturation lives mainly in the feedback loop,
// matching the useful SSM2040-lineage hypothesis from joshjetson/phosphor.
// This remains a calibration model, not a claim of the Prophet-6 schematic.
class CandidateLowPass4Pole final : public LowPass4Pole {
public:
    void prepare(double sampleRate) override;
    void reset() override;
    double process(double input, double cutoffHz, double resonance, double drive) override;
    void start(double resonance); // seed documented LPF self-oscillation when otherwise silent

private:
    static double tanhApprox(double x);
    static double softRail(double x);

    double sampleRate_{48000.0};
    double s_[4]{};
};

// v0.3's nonlinear four-stage candidate is retained for A/B regression tests.
class LegacyLowPass4Pole final : public LowPass4Pole {
public:
    void prepare(double sampleRate) override;
    void reset() override;
    double process(double input, double cutoffHz, double resonance, double drive) override;

private:
    double sampleRate_{48000.0};
    double z1_{};
    double z2_{};
    double z3_{};
    double z4_{};
};

} // namespace p6
