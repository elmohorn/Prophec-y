#include "p6/P6Clock.h"

#include <algorithm>
#include <cmath>

namespace p6 {

ClockDivisionInfo clockDivisionInfo(ClockDivision division) noexcept
{
    switch (division) {
    case ClockDivision::Half:              return {2.0, false};
    case ClockDivision::Quarter:           return {1.0, false};
    case ClockDivision::Eighth:            return {0.5, false};
    case ClockDivision::EighthDotted:      return {0.75, false};
    case ClockDivision::EighthSwing:       return {0.5, true};
    case ClockDivision::EighthTriplet:     return {1.0 / 3.0, false};
    case ClockDivision::Sixteenth:         return {0.25, false};
    case ClockDivision::SixteenthSwing:    return {0.25, true};
    case ClockDivision::SixteenthTriplet:  return {1.0 / 6.0, false};
    case ClockDivision::ThirtySecond:      return {0.125, false};
    }
    return {0.25, false};
}

const char* clockDivisionName(ClockDivision division) noexcept
{
    switch (division) {
    case ClockDivision::Half:             return "Half";
    case ClockDivision::Quarter:          return "Qtr";
    case ClockDivision::Eighth:           return "8th";
    case ClockDivision::EighthDotted:     return "8th D";
    case ClockDivision::EighthSwing:      return "8th S";
    case ClockDivision::EighthTriplet:    return "8th T";
    case ClockDivision::Sixteenth:        return "16th";
    case ClockDivision::SixteenthSwing:   return "16th S";
    case ClockDivision::SixteenthTriplet: return "16th T";
    case ClockDivision::ThirtySecond:     return "32nd";
    }
    return "16th";
}

void P6Clock::prepare(double sampleRate)
{
    sampleRate_ = std::max(8000.0, sampleRate);
    restart(true);
}

void P6Clock::setBpm(double bpm)
{
    bpm_ = std::clamp(bpm, 30.0, 250.0);
}

void P6Clock::setDivision(ClockDivision division)
{
    const auto raw = static_cast<unsigned>(division);
    division_ = raw <= static_cast<unsigned>(ClockDivision::ThirtySecond)
        ? division : ClockDivision::Sixteenth;
}

void P6Clock::setSyncMode(ClockSyncMode mode) noexcept
{
    syncMode_ = mode;
    midiBoundaryPending_ = false;
    if (mode == ClockSyncMode::HostPpq)
        hostAnchorSeen_ = false;
    if (mode == ClockSyncMode::MidiClock) {
        midiPulseIndex_ = 0;
        midiClockSeen_ = false;
    }
}

void P6Clock::setRunning(bool running, bool restartOnStart)
{
    if (running && !running_ && restartOnStart)
        restart(true);
    running_ = running;
}

void P6Clock::restart(bool fireImmediately)
{
    stepIndex_ = 0;
    midiPulseIndex_ = 0;
    midiBoundaryPending_ = false;
    samplesUntilNext_ = fireImmediately ? 0.0 : nominalStepSamples();
}

double P6Clock::nominalStepSamples() const noexcept
{
    const auto info = clockDivisionInfo(division_);
    return sampleRate_ * (60.0 / std::max(1.0, bpm_)) * info.quarterNotesPerStep;
}

std::uint32_t P6Clock::midiClocksPerStep() const noexcept
{
    // All ten documented Prophet divisions fall on integer MIDI-clock counts:
    // 48,24,12,18,12,8,6,6,4,3 clocks. Swing remains timing-opaque, so its
    // conservative fallback uses the corresponding straight average spacing.
    const double clocks = 24.0 * clockDivisionInfo(division_).quarterNotesPerStep;
    return static_cast<std::uint32_t>(std::max(1.0, std::round(clocks)));
}

bool P6Clock::swingTimingIsOpaque() const noexcept
{
    return clockDivisionInfo(division_).swingOpaque;
}

void P6Clock::alignToHostPpq(double ppq, double bpm, bool playing)
{
    syncMode_ = ClockSyncMode::HostPpq;
    hostAnchorSeen_ = true;
    setBpm(bpm);
    running_ = playing;
    midiBoundaryPending_ = false;
    if (!playing)
        return;

    const double stepQn = clockDivisionInfo(division_).quarterNotesPerStep;
    if (!(stepQn > 0.0) || !std::isfinite(ppq)) {
        samplesUntilNext_ = 0.0;
        return;
    }
    double phase = std::fmod(ppq, stepQn);
    if (phase < 0.0)
        phase += stepQn;
    const double eps = 1.0e-9;
    if (phase < eps || stepQn - phase < eps) {
        samplesUntilNext_ = 0.0;
    } else {
        const double samplesPerQn = sampleRate_ * 60.0 / bpm_;
        samplesUntilNext_ = (stepQn - phase) * samplesPerQn;
    }
    stepIndex_ = static_cast<std::uint64_t>(std::max(0.0, std::floor(ppq / stepQn)));
}

void P6Clock::midiStart() noexcept
{
    if (syncMode_ != ClockSyncMode::MidiClock)
        return;
    running_ = true;
    midiPulseIndex_ = 0;
    stepIndex_ = 0;
    midiBoundaryPending_ = false;
}

void P6Clock::midiContinue() noexcept
{
    if (syncMode_ == ClockSyncMode::MidiClock)
        running_ = true;
}

void P6Clock::midiStop() noexcept
{
    if (syncMode_ == ClockSyncMode::MidiClock)
        running_ = false;
}

void P6Clock::midiTimingClock() noexcept
{
    if (syncMode_ != ClockSyncMode::MidiClock)
        return;
    midiClockSeen_ = true;
    if (!running_)
        return;
    const auto pulses = midiClocksPerStep();
    if ((midiPulseIndex_ % pulses) == 0)
        midiBoundaryPending_ = true;
    ++midiPulseIndex_;
}

bool P6Clock::tick()
{
    if (!running_)
        return false;

    if (syncMode_ == ClockSyncMode::MidiClock) {
        const bool fired = midiBoundaryPending_;
        midiBoundaryPending_ = false;
        if (fired)
            ++stepIndex_;
        return fired;
    }

    bool fired = false;
    if (samplesUntilNext_ <= 1.0e-12) {
        fired = true;
        ++stepIndex_;
        // The Prophet-6 manual identifies 8th-S and 16th-S as swing divisions
        // but does not publish their long:short ratio. Until hardware timing is
        // measured, those two values intentionally fall back to their straight
        // base spacing instead of embedding an invented 2:1/3:1 constant.
        samplesUntilNext_ += std::max(1.0, nominalStepSamples());
    }
    samplesUntilNext_ -= 1.0;
    return fired;
}

} // namespace p6
