#include "p6/P6ModulationEffects.h"

#include <algorithm>
#include <cmath>

namespace p6 {
namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr double kTau = 6.28318530717958647692;
constexpr double kFlangerMaxSeconds = 0.02;

std::size_t wrapIndex(std::ptrdiff_t index, std::size_t size) noexcept
{
    const auto n = static_cast<std::ptrdiff_t>(size);
    index %= n;
    if (index < 0)
        index += n;
    return static_cast<std::size_t>(index);
}

} // namespace

void P6ModulationEffects::prepare(double sampleRate)
{
    sampleRate_ = std::max(8000.0, sampleRate);
    const auto frames = static_cast<std::size_t>(std::ceil(sampleRate_ * kFlangerMaxSeconds)) + 4u;
    flangerLeft_.assign(std::max<std::size_t>(frames, 16u), 0.0);
    flangerRight_.assign(std::max<std::size_t>(frames, 16u), 0.0);
    reset();
}

void P6ModulationEffects::reset() noexcept
{
    phaserPhase_ = 0.0;
    flangerPhase_ = 0.0;
    ringPhase_ = 0.0;
    phaserLeft_ = {};
    phaserRight_ = {};
    std::fill(flangerLeft_.begin(), flangerLeft_.end(), 0.0);
    std::fill(flangerRight_.begin(), flangerRight_.end(), 0.0);
    flangerWrite_ = 0;
    flangerFeedbackLeft_ = 0.0;
    flangerFeedbackRight_ = 0.0;
}

double P6ModulationEffects::phaserChannel(double input,
                                          PhaserChannel& state,
                                          double lfo,
                                          double depth01,
                                          int variant) noexcept
{
    // All Prophet/OB family descriptions identify a six-stage phaser. The
    // three variants differ here by resonance and sweep voicing only; these are
    // calibration candidates, not claims about the original DSP constants.
    double feedback = 0.35;
    double centreHz = 850.0;
    double sweepOctaves = 2.2;
    if (variant == 1) {
        feedback = 0.72; // PH1: documented high resonance
        centreHz = 950.0;
        sweepOctaves = 2.5;
    } else if (variant == 3) {
        feedback = 0.56; // PH3: Oberheim-style candidate
        centreHz = 700.0;
        sweepOctaves = 2.0;
    }

    const double depth = std::clamp(depth01, 0.0, 1.0);
    const double frequency = std::clamp(
        centreHz * std::pow(2.0, lfo * depth * sweepOctaves),
        35.0, sampleRate_ * 0.42);
    const double g = std::tan(kPi * frequency / sampleRate_);
    const double a = (1.0 - g) / (1.0 + g);

    double x = input + state.feedback * feedback;
    for (auto& z : state.state) {
        // H(z) = (a + z^-1) / (1 + a z^-1)
        const double y = a * x + z;
        z = x - a * y;
        x = y;
    }
    state.feedback = x;

    // A phaser's audible notches come from mixing the direct and phase-rotated
    // signals. The outer FX-slot mix remains independent of this internal 50/50.
    return 0.5 * (input + x);
}

P6ModulationStereoSample P6ModulationEffects::processPhaser(
    double left, double right, double rateHz, double depth01, int variant) noexcept
{
    const double rate = std::clamp(rateHz, 0.01, 20.0);
    phaserPhase_ += rate / sampleRate_;
    phaserPhase_ -= std::floor(phaserPhase_);

    const double lfoLeft = std::sin(kTau * phaserPhase_);
    const double lfoRight = std::sin(kTau * (phaserPhase_ + 0.25));
    return {
        phaserChannel(left, phaserLeft_, lfoLeft, depth01, variant),
        phaserChannel(right, phaserRight_, lfoRight, depth01, variant),
    };
}

double P6ModulationEffects::tapLinear(const std::vector<double>& buffer,
                                      std::size_t write,
                                      double samplesBack) noexcept
{
    if (buffer.size() < 4u)
        return 0.0;
    const double back = std::clamp(samplesBack, 1.0, static_cast<double>(buffer.size() - 2u));
    const double read = static_cast<double>(write) - back;
    const auto base = static_cast<std::ptrdiff_t>(std::floor(read));
    const double fraction = read - std::floor(read);
    const double a = buffer[wrapIndex(base, buffer.size())];
    const double b = buffer[wrapIndex(base + 1, buffer.size())];
    return a + (b - a) * fraction;
}

P6ModulationStereoSample P6ModulationEffects::processFlanger(
    double left, double right, double rateHz, double depth01, bool highFeedback) noexcept
{
    if (flangerLeft_.empty() || flangerRight_.empty())
        return {left, right};

    const double rate = std::clamp(rateHz, 0.01, 20.0);
    flangerPhase_ += rate / sampleRate_;
    flangerPhase_ -= std::floor(flangerPhase_);

    const double depth = std::clamp(depth01, 0.0, 1.0);
    constexpr double centreSeconds = 0.0032;
    constexpr double sweepSeconds = 0.0029;
    const double centre = centreSeconds * sampleRate_;
    const double sweep = sweepSeconds * depth * sampleRate_;
    const double lfoL = std::sin(kTau * flangerPhase_);
    const double lfoR = std::sin(kTau * (flangerPhase_ + 0.25));

    // Delaying the reference path lets the modulated path cross it, giving the
    // characteristic through-zero cancellation without requiring negative delay.
    const double dryL = tapLinear(flangerLeft_, flangerWrite_, centre);
    const double dryR = tapLinear(flangerRight_, flangerWrite_, centre);
    const double wetL = tapLinear(flangerLeft_, flangerWrite_, centre + sweep * lfoL);
    const double wetR = tapLinear(flangerRight_, flangerWrite_, centre + sweep * lfoR);

    const double feedback = highFeedback ? 0.68 : 0.0;
    flangerLeft_[flangerWrite_] = left + flangerFeedbackLeft_ * feedback;
    flangerRight_[flangerWrite_] = right + flangerFeedbackRight_ * feedback;
    flangerFeedbackLeft_ = wetL;
    flangerFeedbackRight_ = wetR;
    flangerWrite_ = (flangerWrite_ + 1u) % flangerLeft_.size();

    return {0.5 * (dryL + wetL), 0.5 * (dryR + wetR)};
}

P6ModulationStereoSample P6ModulationEffects::processRingMod(
    double left, double right, double modulatorHz) noexcept
{
    const double hz = std::clamp(modulatorHz, 0.01, sampleRate_ * 0.45);
    ringPhase_ += hz / sampleRate_;
    ringPhase_ -= std::floor(ringPhase_);
    const double carrier = std::sin(kTau * ringPhase_);
    return {left * carrier, right * carrier};
}

} // namespace p6
