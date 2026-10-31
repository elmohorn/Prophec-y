#include "p6/P6ReverbEffects.h"

#include <algorithm>
#include <cmath>

namespace p6 {
namespace {

constexpr double kTau = 6.28318530717958647692;

std::size_t msToSamples(double sampleRate, double ms)
{
    return std::max<std::size_t>(1u,
        static_cast<std::size_t>(std::llround(sampleRate * ms * 0.001)));
}

struct ReverbVoicing {
    std::array<double, 4> delayMs;
    double minTimeSeconds;
    double maxTimeSeconds;
    double dampingHz;
    double earlyMs;
    double diffusion;
};

ReverbVoicing voicing(P6ReverbEffects::Type type, double secondary)
{
    switch (type) {
    case P6ReverbEffects::Type::Hall:
        return {{{31.3, 37.7, 43.9, 53.1}}, 0.7, 8.0, 5200.0, 17.0, 0.82};
    case P6ReverbEffects::Type::Room:
        return {{{11.7, 15.1, 19.3, 23.9}}, 0.18, 3.0, 7000.0, 7.0, 0.68};
    case P6ReverbEffects::Type::Plate:
        return {{{23.3, 29.9, 35.3, 41.9}}, 0.45, 6.0, 8200.0, 11.0, 0.90};
    case P6ReverbEffects::Type::Spring: {
        const double toneHz = 1400.0 * std::pow(10.0, std::clamp(secondary, 0.0, 1.0));
        return {{{17.9, 24.7, 32.9, 46.1}}, 0.25, 5.0, toneHz, 5.5, 0.55};
    }
    }
    return {{{23.3, 29.9, 35.3, 41.9}}, 0.45, 6.0, 8200.0, 11.0, 0.90};
}

} // namespace

void P6ReverbEffects::prepare(double sampleRate)
{
    sampleRate_ = std::max(8000.0, sampleRate);
    // Delay lengths and damping coefficients are sample-rate dependent.
    // Re-prepare must therefore invalidate the coefficient cache even when
    // effect type and knob values are unchanged.
    cached_.valid = false;
    const auto capacity = msToSamples(sampleRate_, 80.0) + 4u;
    for (auto& line : lines_)
        line.buffer.assign(capacity, 0.0);
    early_.assign(msToSamples(sampleRate_, 40.0) + 4u, 0.0);
    reset();
}

void P6ReverbEffects::reset() noexcept
{
    for (auto& line : lines_)
        std::fill(line.buffer.begin(), line.buffer.end(), 0.0);
    std::fill(early_.begin(), early_.end(), 0.0);
    invalidateRealtimeState();
}

void P6ReverbEffects::invalidateRealtimeState() noexcept
{
    for (auto& line : lines_) {
        line.write = 0;
        line.validSamples = 0;
        line.dampingState = 0.0;
    }
    earlyWrite_ = 0;
    earlyValidSamples_ = 0;
    // Coefficients are independent of delay-buffer contents; keep a valid
    // cache across panic/type-state clears when the parameters are unchanged.
}

double P6ReverbEffects::readDelay(const Line& line, std::size_t delaySamples) noexcept
{
    if (line.buffer.empty())
        return 0.0;
    const auto delay = std::clamp<std::size_t>(delaySamples, 1u, line.buffer.size() - 1u);
    if (line.validSamples < delay)
        return 0.0;
    const auto index = (line.write + line.buffer.size() - delay) % line.buffer.size();
    return line.buffer[index];
}

void P6ReverbEffects::writeDelay(Line& line, double value) noexcept
{
    if (line.buffer.empty())
        return;
    line.buffer[line.write] = value;
    line.write = (line.write + 1u) % line.buffer.size();
    line.validSamples = std::min(line.validSamples + 1u, line.buffer.size());
}

void P6ReverbEffects::updateCachedCoefficients(Type type,
                                               double primary,
                                               double secondary) noexcept
{
    if (cached_.valid && cached_.type == type
        && cached_.primary == primary && cached_.secondary == secondary)
        return;

    const auto v = voicing(type, secondary);
    const double timeSeconds = v.minTimeSeconds
        + (v.maxTimeSeconds - v.minTimeSeconds) * primary * primary;

    cached_.type = type;
    cached_.primary = primary;
    cached_.secondary = secondary;
    cached_.dampingAlpha = 1.0 - std::exp(-kTau * v.dampingHz / sampleRate_);
    cached_.diffusion = v.diffusion;
    cached_.earlyDelaySamples = std::clamp<std::size_t>(
        msToSamples(sampleRate_, v.earlyMs), 1u, early_.size() - 1u);
    cached_.spring = type == Type::Spring;
    cached_.earlyAmount = cached_.spring ? 0.24 : secondary;

    for (std::size_t i = 0; i < kLines; ++i) {
        cached_.delaySamples[i] = msToSamples(sampleRate_, v.delayMs[i]);
        const double lineSeconds = v.delayMs[i] * 0.001;
        cached_.feedbackGain[i] = std::pow(
            0.001, lineSeconds / std::max(0.05, timeSeconds));
    }
    cached_.valid = true;
}

P6ReverbStereoSample P6ReverbEffects::process(double left, double right,
                                               Type type,
                                               double primary01,
                                               double secondary01) noexcept
{
    if (lines_[0].buffer.empty() || early_.empty())
        return {};

    const double primary = std::clamp(primary01, 0.0, 1.0);
    const double secondary = std::clamp(secondary01, 0.0, 1.0);
    updateCachedCoefficients(type, primary, secondary);

    const double input = 0.5 * (left + right);
    std::array<double, kLines> y{};
    std::array<double, kLines> feedback{};
    for (std::size_t i = 0; i < kLines; ++i)
        y[i] = readDelay(lines_[i], cached_.delaySamples[i]);

    feedback[0] = 0.5 * ( y[0] + y[1] + y[2] + y[3]);
    feedback[1] = 0.5 * ( y[0] - y[1] + y[2] - y[3]);
    feedback[2] = 0.5 * ( y[0] + y[1] - y[2] - y[3]);
    feedback[3] = 0.5 * ( y[0] - y[1] - y[2] + y[3]);

    for (std::size_t i = 0; i < kLines; ++i) {
        auto& line = lines_[i];
        line.dampingState += cached_.dampingAlpha * (feedback[i] - line.dampingState);
        const double injection = input * ((i & 1u) ? -cached_.diffusion : cached_.diffusion);
        writeDelay(line, injection + line.dampingState * cached_.feedbackGain[i]);
    }

    double early = 0.0;
    if (earlyValidSamples_ >= cached_.earlyDelaySamples) {
        const auto earlyIndex = (earlyWrite_ + early_.size() - cached_.earlyDelaySamples) % early_.size();
        early = early_[earlyIndex];
    }
    early_[earlyWrite_] = input;
    earlyWrite_ = (earlyWrite_ + 1u) % early_.size();
    earlyValidSamples_ = std::min(earlyValidSamples_ + 1u, early_.size());

    double wetL = 0.34 * (y[0] + y[2]) - 0.22 * (y[1] + y[3]);
    double wetR = 0.34 * (y[1] + y[3]) - 0.22 * (y[0] + y[2]);
    wetL += early * cached_.earlyAmount * 0.45;
    wetR -= early * cached_.earlyAmount * 0.35;

    if (cached_.spring) {
        wetL += 0.10 * (y[0] - y[2]);
        wetR += 0.10 * (y[1] - y[3]);
    }

    return {wetL, wetR};
}

} // namespace p6
