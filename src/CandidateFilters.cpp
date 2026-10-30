#include "p6/CandidateFilters.h"

#include <algorithm>
#include <cmath>

namespace p6 {
namespace {
constexpr double kPi = 3.14159265358979323846;
constexpr double kLpResMax = 4.6;
constexpr double kLpPassbandComp = 0.9;
constexpr double kLpFeedbackHeadroom = 2.5;
constexpr double kVoiceKnee = 2.4;
constexpr double kVoiceRail = 4.5;
constexpr double kHpQMin = 0.5;
constexpr double kHpQMax = 8.0;
}

void CandidateHighPass2Pole::prepare(double sampleRate)
{
    sampleRate_ = std::max(8000.0, sampleRate);
    reset();
}

void CandidateHighPass2Pole::reset()
{
    s1_ = 0.0;
    s2_ = 0.0;
}

double CandidateHighPass2Pole::process(double input, double cutoffHz, double resonance)
{
    const double fc = std::clamp(cutoffHz, 5.0, sampleRate_ * 0.45);
    const double g = std::tan(kPi * fc / sampleRate_);
    const double r = std::clamp(resonance, 0.0, 1.0);
    const double q = kHpQMin * std::pow(kHpQMax / kHpQMin, r);
    const double damping = 1.0 / q;

    const double hp = (input - (damping + g) * s1_ - s2_) /
                      (1.0 + damping * g + g * g);
    const double v1 = g * hp;
    const double bp = v1 + s1_;
    s1_ = std::clamp(bp + v1, -8.0, 8.0);
    const double v2 = g * bp;
    const double lp = v2 + s2_;
    s2_ = std::clamp(lp + v2, -8.0, 8.0);
    return hp;
}

void CandidateLowPass4Pole::prepare(double sampleRate)
{
    sampleRate_ = std::max(8000.0, sampleRate);
    reset();
}

void CandidateLowPass4Pole::reset()
{
    for (double& s : s_)
        s = 0.0;
}

double CandidateLowPass4Pole::tanhApprox(double x)
{
    const double x2 = x * x;
    return x * (27.0 + x2) / (27.0 + 9.0 * x2);
}

double CandidateLowPass4Pole::softRail(double x)
{
    const double m = std::abs(x);
    if (m <= kVoiceKnee)
        return x;
    const double span = kVoiceRail - kVoiceKnee;
    const double u = (m - kVoiceKnee) / span;
    return std::copysign(kVoiceKnee + span * u / (1.0 + u), x);
}

void CandidateLowPass4Pole::start(double resonance)
{
    const double r = std::clamp(resonance, 0.0, 1.0);
    constexpr double knee = 0.82;
    constexpr double seed = 0.04;
    double stateEnergy = 0.0;
    for (const double s : s_)
        stateEnergy += std::abs(s);
    if (stateEnergy > 1.0e-8 || r <= knee)
        return;
    const double past = std::clamp((r - knee) / (1.0 - knee), 0.0, 1.0);
    for (double& s : s_)
        s = past * seed;
}

double CandidateLowPass4Pole::process(double input,
                                       double cutoffHz,
                                       double resonance,
                                       double drive)
{
    const double fc = std::clamp(cutoffHz, 5.0, sampleRate_ * 0.45);
    const double g = std::tan(kPi * fc / sampleRate_);
    const double gg = g / (1.0 + g);
    const double r = std::clamp(resonance, 0.0, 1.0);
    const double k = r * kLpResMax;

    // Keep drive as an explicit calibration hook, but unlike the v0.3 model it
    // does not saturate every forward-path pole. Moderate drive changes how
    // early the feedback limiter is reached instead.
    const double headroom = kLpFeedbackHeadroom / std::clamp(drive, 0.5, 4.0);
    const double feedback = headroom * tanhApprox(s_[3] / headroom);
    double x = input * (1.0 + kLpPassbandComp * k) - k * feedback;

    for (double& s : s_) {
        const double v = (x - s) * gg;
        const double y = v + s;
        s = y + v;
        if (std::abs(s) < 1.0e-18)
            s = 0.0;
        x = y;
    }

    s_[3] = std::clamp(s_[3], -16.0, 16.0);
    return softRail(x);
}

void LegacyLowPass4Pole::prepare(double sampleRate)
{
    sampleRate_ = std::max(8000.0, sampleRate);
    reset();
}

void LegacyLowPass4Pole::reset()
{
    z1_ = z2_ = z3_ = z4_ = 0.0;
}

double LegacyLowPass4Pole::process(double input, double cutoffHz, double resonance, double drive)
{
    const double fc = std::clamp(cutoffHz, 8.0, sampleRate_ * 0.45);
    const double g = 1.0 - std::exp(-2.0 * kPi * fc / sampleRate_);
    const double r = std::clamp(resonance, 0.0, 1.0);
    const double feedback = 4.15 * r;
    const double inputDrive = std::clamp(drive, 0.25, 8.0);
    const double u = std::tanh(input * inputDrive - feedback * z4_);

    z1_ += g * (std::tanh(u) - std::tanh(z1_));
    z2_ += g * (std::tanh(z1_) - std::tanh(z2_));
    z3_ += g * (std::tanh(z2_) - std::tanh(z3_));
    z4_ += g * (std::tanh(z3_) - std::tanh(z4_));
    return z4_;
}

} // namespace p6
