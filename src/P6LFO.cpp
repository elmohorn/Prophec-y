#include "p6/P6LFO.h"

#include <algorithm>
#include <cmath>

namespace p6 {

void P6LFO::prepare(double sampleRate)
{
    sampleRate_ = std::max(8000.0, sampleRate);
    reset();
}

void P6LFO::reset()
{
    phase_ = 0.0;
    sampleHold_ = 0.0;
}

void P6LFO::setRate(double hz)
{
    rateHz_ = std::clamp(hz, 0.0, sampleRate_ * 0.45);
}

void P6LFO::setShape(Shape shape)
{
    shape_ = shape;
}

void P6LFO::setNoiseMode(bool enabled)
{
    noiseMode_ = enabled;
}

double P6LFO::polyBlep(double t, double dt)
{
    if (dt <= 0.0)
        return 0.0;
    if (t < dt) {
        const double s = t / dt;
        return 2.0 * s - s * s - 1.0;
    }
    if (t > 1.0 - dt) {
        const double s = (t - 1.0) / dt;
        return s * s + 2.0 * s + 1.0;
    }
    return 0.0;
}

double P6LFO::whiteNoise()
{
    rng_ ^= rng_ << 13;
    rng_ ^= rng_ >> 17;
    rng_ ^= rng_ << 5;
    return static_cast<double>(rng_) / 2147483647.5 - 1.0;
}

double P6LFO::process()
{
    const double dt = std::clamp(rateHz_ / sampleRate_, 0.0, 0.45);
    phase_ += dt;
    const bool wrapped = phase_ >= 1.0;
    if (wrapped)
        phase_ -= std::floor(phase_);

    const double n = whiteNoise();
    if (wrapped)
        sampleHold_ = n;

    const double t = phase_;
    switch (shape_) {
    case Shape::Saw:
        // The downward wrap needs a positive correction at phase zero.
        return std::clamp(t - 0.5 * polyBlep(t, dt), 0.0, 1.0);
    case Shape::ReverseSaw:
        // Reverse saw wraps upward and therefore uses the opposite sign.
        return std::clamp(1.0 - t + 0.5 * polyBlep(t, dt), 0.0, 1.0);
    case Shape::Square: {
        double v = t < 0.5 ? 1.0 : 0.0;
        v += 0.5 * polyBlep(t, dt);
        double shifted = t - 0.5;
        if (shifted < 0.0)
            shifted += 1.0;
        v -= 0.5 * polyBlep(shifted, dt);
        return std::clamp(v, 0.0, 1.0);
    }
    case Shape::Random:
        return noiseMode_ ? n : sampleHold_;
    case Shape::Triangle:
    default:
        return t < 0.5 ? 4.0 * t - 1.0 : 3.0 - 4.0 * t;
    }
}

} // namespace p6
