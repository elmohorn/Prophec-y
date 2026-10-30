#include "p6/AnalogSlop.h"

#include <algorithm>
#include <cmath>

namespace p6 {
namespace {
constexpr double kSlopHz = 0.7;
constexpr double kTargetUpdateHz = 40.0;
constexpr double kGain = 4.0;
constexpr double kTau = 6.28318530717958647692;
}

std::uint32_t AnalogSlop::mix32(std::uint32_t x)
{
    x ^= x >> 16;
    x *= 0x7feb352du;
    x ^= x >> 15;
    x *= 0x846ca68bu;
    x ^= x >> 16;
    return x;
}

void AnalogSlop::prepare(double sampleRate, std::uint32_t seed)
{
    sampleRate_ = std::max(8000.0, sampleRate);
    state_ = mix32(seed ? seed : 1u);
    stride_ = (seed ? seed : 1u) | 1u;
    updateStep_ = kTargetUpdateHz / sampleRate_;
    smoothing_ = 1.0 - std::exp(-kTau * kSlopHz / sampleRate_);
    reset();
}

void AnalogSlop::reset()
{
    target_ = 0.0;
    value_ = 0.0;
    updatePhase_ = 0.0;
}

double AnalogSlop::nextNoise()
{
    state_ += stride_;
    const auto bits = mix32(state_) >> 8;
    return static_cast<double>(bits) / static_cast<double>(1u << 23) - 1.0;
}

double AnalogSlop::process()
{
    updatePhase_ += updateStep_;
    if (updatePhase_ >= 1.0) {
        updatePhase_ -= std::floor(updatePhase_);
        target_ = nextNoise();
    }
    value_ += smoothing_ * (target_ - value_);
    return std::clamp(value_ * kGain, -1.0, 1.0);
}

} // namespace p6
