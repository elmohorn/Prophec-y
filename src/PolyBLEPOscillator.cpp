#include "p6/PolyBLEPOscillator.h"

#include <algorithm>
#include <cmath>

namespace p6 {
namespace {
constexpr double kEdgeMin = 1.0e-6;
constexpr double kDutyMin = 0.04;
}

void PolyBLEPOscillator::prepare(double sampleRate)
{
    sampleRate_ = std::max(8000.0, sampleRate);
}

void PolyBLEPOscillator::reset(double phase01)
{
    phase_ = wrapPhase(phase01);
    held_ = 0.0;
    closing_ = {};
    wrapped_ = false;
    wrapOvershoot_ = 0.0;
    lastHardSyncTrace_ = {};
}

void PolyBLEPOscillator::setFrequency(double hz)
{
    frequency_ = std::clamp(hz, 0.01, sampleRate_ * 0.45);
}

void PolyBLEPOscillator::setShape(double shape01)
{
    shape_ = std::clamp(shape01, 0.0, 1.0);
}

void PolyBLEPOscillator::setPulseWidth(double width01)
{
    pulseWidth_ = std::clamp(width01, kDutyMin, 1.0 - kDutyMin);
}

void PolyBLEPOscillator::setMeanNormalizationAmount(double amount01)
{
    meanNormalizationAmount_ = std::clamp(amount01, 0.0, 1.0);
}

void PolyBLEPOscillator::setSyncResetPhase(double phase01)
{
    syncResetPhase_ = wrapPhase(phase01);
}

double PolyBLEPOscillator::nominalWaveform(double phase,double shape,double pulseWidth,double normalization)
{
    return Shape::at(shape,pulseWidth,normalization).value(wrapPhase(phase));
}

PolyBLEPOscillator::Shape PolyBLEPOscillator::Shape::at(double morph, double duty, double normalizationAmount)
{
    const double w = std::clamp(morph, 0.0, 1.0);
    const double d = std::clamp(duty, kDutyMin, 1.0 - kDutyMin);
    const double e = kEdgeMin;

    double rise = 0.5;
    double high = 0.0;
    double fall = 0.5;

    if (w < 0.5) {
        // Triangle -> saw: the rising ramp progressively occupies the cycle
        // while the falling ramp collapses to an edge. Peak amplitude remains
        // stable, avoiding the level/DC dip caused by waveform crossfading.
        const double a = w * 2.0;
        rise = 0.5 + a * (0.5 - e);
        high = 0.0;
        fall = 1.0 - rise;
    } else {
        // Saw -> pulse: shrink the ramp while flat top/bottom sections grow in
        // the ratio requested by pulse width.
        const double b = (w - 0.5) * 2.0;
        const double span = 1.0 - 2.0 * e;
        rise = (1.0 - e) - b * span;
        high = b * d * span;
        fall = e;
    }

    const double low = 1.0 - rise - high - fall;
    const double mean = high - low;
    // The original candidate normalized the DC-removed waveform to bounded
    // peaks with 1/(1+|mean|). At narrow pulse widths that also attenuates
    // every AC harmonic by almost 6 dB. Reference-A calibration can now
    // continuously relax that extra attenuation without changing duty cycle.
    const double amount = std::clamp(normalizationAmount, 0.0, 1.0);
    const double scale = std::pow(1.0 + std::abs(mean), -amount);
    return Shape{rise, high, fall, mean, scale};
}

double PolyBLEPOscillator::Shape::value(double phase) const
{
    const double a = rise;
    const double b = a + high;
    const double c = b + fall;
    double raw = -1.0;

    if (phase < a)
        raw = 2.0 * phase / std::max(a, kEdgeMin) - 1.0;
    else if (phase < b)
        raw = 1.0;
    else if (phase < c)
        raw = 1.0 - 2.0 * (phase - b) / std::max(fall, kEdgeMin);

    return (raw - mean) * scale;
}


double PolyBLEPOscillator::Shape::slopePerPhase(double phase) const
{
    const double a = rise;
    const double b = a + high;
    const double c = b + fall;
    if (phase < a)
        return (2.0 / std::max(a, kEdgeMin)) * scale;
    if (phase < b)
        return 0.0;
    if (phase < c)
        return (-2.0 / std::max(fall, kEdgeMin)) * scale;
    return 0.0;
}

std::array<std::array<double, 3>, 2> PolyBLEPOscillator::Shape::edges() const
{
    return {{{0.0, rise, 2.0}, {rise + high, fall, -2.0}}};
}

double PolyBLEPOscillator::Stretch::place(double reached) const
{
    if (span <= 0.0)
        return t0;
    return t0 + (reached / span) * (t1 - t0);
}

double PolyBLEPOscillator::wrapPhase(double phase)
{
    return phase - std::floor(phase);
}

void PolyBLEPOscillator::corner(double slopeChange, double t, Correction& correction)
{
    const double u = std::clamp(t, 0.0, 1.0);
    const double back = 1.0 - u;
    correction.before += slopeChange * back * back * back / 6.0;
    correction.after += slopeChange * u * u * u / 6.0;
}

void PolyBLEPOscillator::walk(const Shape& shape,
                              const Stretch& stretch,
                              double dt,
                              Correction& correction)
{
    const double start = stretch.from;
    const double span = stretch.span;
    if (span <= 0.0)
        return;

    const double scale = dt * shape.scale;
    const auto edges = shape.edges();
    for (std::size_t index = 0; index < edges.size(); ++index) {
        const double at = edges[index][0];
        const double width = edges[index][1];
        const double height = edges[index][2];
        if (width <= 0.0)
            continue;

        if (closing_[index]) {
            const double reached = std::fmod(closing_[index]->phase - start + 1.0, 1.0);
            if (reached < span) {
                corner(closing_[index]->slopeChange, stretch.place(reached), correction);
                closing_[index].reset();
            }
        }

        const double reached = std::fmod(at - start + 1.0, 1.0);
        if (reached < span) {
            if (closing_[index]) {
                corner(closing_[index]->slopeChange, stretch.place(reached), correction);
                closing_[index].reset();
            }

            const double m = height / width * scale;
            corner(m, stretch.place(reached), correction);
            if (width < span) {
                corner(-m, stretch.place(std::min(reached + width, span)), correction);
            } else {
                closing_[index] = ClosingCorner{wrapPhase(at + width), -m};
            }
        }
    }
}

std::optional<double> PolyBLEPOscillator::nextWrapFraction() const
{
    return nextPhaseCrossingFraction(0.0);
}

std::optional<double> PolyBLEPOscillator::nextPhaseCrossingFraction(double phase01) const
{
    const double dt = frequency_ / sampleRate_;
    if (dt <= 0.0)
        return std::nullopt;
    const double target = wrapPhase(phase01);
    double distance = wrapPhase(target - phase_);
    // If exactly on the target, the next crossing is one cycle away rather
    // than an event at the start of every sample.
    if (distance < 1.0e-12)
        distance = 1.0;
    if (distance <= dt)
        return std::clamp(distance / dt, 0.0, 1.0);
    return std::nullopt;
}

double PolyBLEPOscillator::process(std::optional<double> syncAt)
{
    const double dt = frequency_ / sampleRate_;
    const auto currentShape = Shape::at(shape_, pulseWidth_, meanNormalizationAmount_);
    Correction correction{};
    lastHardSyncTrace_ = {};

    const double oldPhase = phase_;
    double endPhase = 0.0;

    if (syncAt) {
        const double u = std::clamp(*syncAt, 0.0, 1.0);
        const double preSpan = u * dt;
        walk(currentShape, Stretch{phase_, preSpan, 0.0, u}, dt, correction);

        const double phaseAtReset = wrapPhase(phase_ + preSpan);
        const double resetPhase = syncResetPhase_;
        const double valueBefore = currentShape.value(phaseAtReset);
        const double valueAfter = currentShape.value(resetPhase);
        const double jump = valueAfter - valueBefore;
        const double rest = 1.0 - u;

        Correction stepCorrection{};
        stepCorrection.before += 0.5 * jump * rest * rest;
        stepCorrection.after -= 0.5 * jump * u * u;
        correction.before += stepCorrection.before;
        correction.after += stepCorrection.after;

        // A phase reset of a piecewise-linear oscillator generally changes
        // both value and first derivative. v0.10 only corrected the value
        // discontinuity. v0.11 can additionally apply the same polyBLAMP
        // corner primitive used by ordinary waveform corners to the derivative
        // jump. It remains opt-in until hardware evidence supports promotion.
        // A ramp shorter than one sample is handled as a band-limited step
        // by walk(). Its instantaneous geometric slope is not a resolvable
        // derivative: applying it again at reset would create a huge impulse.
        const auto resolvedSlope = [&](double phase) {
            if ((phase < currentShape.rise && currentShape.rise < dt) ||
                (phase >= currentShape.rise + currentShape.high &&
                 phase < currentShape.rise + currentShape.high + currentShape.fall && currentShape.fall < dt))
                return 0.0;
            return currentShape.slopePerPhase(phase) * dt;
        };
        const double slopeBefore = resolvedSlope(phaseAtReset);
        const double slopeAfter = resolvedSlope(resetPhase);
        const double slopeJump = slopeAfter - slopeBefore;
        Correction slopeCorrection{};
        if (hardSyncCorrectionMode_ == HardSyncCorrectionMode::StepAndSlope) {
            corner(slopeJump, u, slopeCorrection);
            correction.before += slopeCorrection.before;
            correction.after += slopeCorrection.after;
        }

        lastHardSyncTrace_.occurred = true;
        lastHardSyncTrace_.syncFraction = u;
        lastHardSyncTrace_.phaseBeforeReset = phaseAtReset;
        lastHardSyncTrace_.resetPhase = resetPhase;
        lastHardSyncTrace_.valueBeforeReset = valueBefore;
        lastHardSyncTrace_.valueAfterReset = valueAfter;
        lastHardSyncTrace_.valueJump = jump;
        lastHardSyncTrace_.slopeBeforePerSample = slopeBefore;
        lastHardSyncTrace_.slopeAfterPerSample = slopeAfter;
        lastHardSyncTrace_.slopeJumpPerSample = slopeJump;
        lastHardSyncTrace_.stepCorrectionBefore = stepCorrection.before;
        lastHardSyncTrace_.stepCorrectionAfter = stepCorrection.after;
        lastHardSyncTrace_.slopeCorrectionBefore = slopeCorrection.before;
        lastHardSyncTrace_.slopeCorrectionAfter = slopeCorrection.after;

        // A reset can truncate a ramp before its closing corner is reached.
        // Close that pending pair at the reset time: discarding it leaves a
        // large, unmatched BLAMP term when a nearly vertical edge is crossed.
        for (const auto& pending : closing_)
            if (pending)
                corner(pending->slopeChange, u, correction);
        closing_ = {};

        const double postSpan = rest * dt;
        walk(currentShape, Stretch{resetPhase, postSpan, u, 1.0}, dt, correction);
        endPhase = wrapPhase(resetPhase + postSpan);
        lastHardSyncTrace_.totalCorrectionBefore = correction.before;
        lastHardSyncTrace_.totalCorrectionAfter = correction.after;
        lastHardSyncTrace_.endPhase = endPhase;
    } else {
        walk(currentShape, Stretch{phase_, dt, 0.0, 1.0}, dt, correction);
        endPhase = wrapPhase(phase_ + dt);
    }

    const double out = held_ + correction.before;
    held_ = currentShape.value(endPhase) + correction.after;
    phase_ = endPhase;

    wrapped_ = (!syncAt && oldPhase + dt >= 1.0);
    wrapOvershoot_ = wrapped_ ? wrapPhase(oldPhase + dt) : 0.0;
    return out;
}

} // namespace p6

