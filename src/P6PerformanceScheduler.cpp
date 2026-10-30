#include "p6/P6PerformanceScheduler.h"

#include <algorithm>

namespace p6 {

void P6PerformanceScheduler::prepare(double sampleRate)
{
    clock_.prepare(sampleRate);
    engine_.setEffectsRuntimeBpm(clock_.bpm());
    updateClockRunning(true);
}

void P6PerformanceScheduler::loadPatch(const P6PatchState& patch,
                                       const MapperCalibration& calibration)
{
    engine_.loadPatch(patch, calibration);
    const auto mapped = mapPatchToDSP(patch, calibration);

    releaseArpNote();
    releaseSequenceNotes();
    sequencePlaying_ = false;

    arpeggiator_.clear();
    arpeggiator_.setMode(static_cast<ArpMode>(mapped.arpMode));
    arpeggiator_.setOctaves(static_cast<std::uint8_t>(mapped.arpRange + 1));
    arpEnabled_ = mapped.arpOn;

    setClockBpm(mapped.clockBpm);
    clock_.setDivision(static_cast<ClockDivision>(mapped.clockDivision));
    clock_.restart(true);

    sequencer_.loadPatch(patch);
    updateClockRunning(false);
}

void P6PerformanceScheduler::updatePatch(const P6PatchState& patch,
                                         const MapperCalibration& calibration)
{
    engine_.updatePatch(patch, calibration);
    const auto mapped = mapPatchToDSP(patch, calibration);

    arpeggiator_.setMode(static_cast<ArpMode>(mapped.arpMode));
    arpeggiator_.setOctaves(static_cast<std::uint8_t>(mapped.arpRange + 1));
    if (arpEnabled_ != mapped.arpOn)
        setArpeggiatorEnabled(mapped.arpOn);

    setClockBpm(mapped.clockBpm);
    clock_.setDivision(static_cast<ClockDivision>(mapped.clockDivision));
    sequencer_.updatePatch(patch);
    updateClockRunning(false);
}

void P6PerformanceScheduler::updateClockRunning(bool restartOnStart)
{
    bool shouldRun = sequencePlaying_ || arpEnabled_;
    if (clock_.syncMode() == ClockSyncMode::MidiClock && midiStartStopEnabled_)
        shouldRun = shouldRun && midiTransportRunning_;
    clock_.setRunning(shouldRun, restartOnStart);
}

void P6PerformanceScheduler::setClockSyncMode(ClockSyncMode mode)
{
    clock_.setSyncMode(mode);
    if (mode == ClockSyncMode::MidiClock)
        midiTransportRunning_ = false;
    updateClockRunning(false);
}

void P6PerformanceScheduler::setMidiStartStopEnabled(bool enabled)
{
    midiStartStopEnabled_ = enabled;
    updateClockRunning(false);
}

void P6PerformanceScheduler::midiClockStart()
{
    if (!midiStartStopEnabled_)
        return;
    midiTransportRunning_ = true;
    clock_.midiStart();
    updateClockRunning(false);
}

void P6PerformanceScheduler::midiClockContinue()
{
    if (!midiStartStopEnabled_)
        return;
    midiTransportRunning_ = true;
    clock_.midiContinue();
    updateClockRunning(false);
}

void P6PerformanceScheduler::midiClockStop()
{
    if (!midiStartStopEnabled_)
        return;
    midiTransportRunning_ = false;
    clock_.midiStop();
}

void P6PerformanceScheduler::stopDirectPhysicalKeys()
{
    for (std::size_t i = 0; i < arpeggiator_.physicalKeyCount(); ++i) {
        const int note = arpeggiator_.physicalNote(i);
        if (note >= 0)
            engine_.noteOffOwned(note, 0);
    }
}

void P6PerformanceScheduler::restoreDirectPhysicalKeys()
{
    for (std::size_t i = 0; i < arpeggiator_.physicalKeyCount(); ++i) {
        const int note = arpeggiator_.physicalNote(i);
        if (note >= 0)
            engine_.noteOnOwned(note, arpeggiator_.physicalVelocity(i), 0);
    }
}

void P6PerformanceScheduler::noteOn(int midiNote, double velocity01)
{
    midiNote = std::clamp(midiNote, 0, 127);
    velocity01 = std::clamp(velocity01, 0.0, 1.0);

    const bool firstPhysicalKey = arpeggiator_.physicalKeyCount() == 0;
    if (firstPhysicalKey)
        engine_.resetSyncedLfoPhase();

    arpeggiator_.noteOn(midiNote, velocity01);

    if (sequencePlaying_) {
        engine_.noteOnOwned(midiNote, velocity01, 0);
        return;
    }

    if (!arpEnabled_) {
        engine_.noteOnOwned(midiNote, velocity01, 0);
        return;
    }

    if (!arpBeatSync_) {
        const bool externalReady = clock_.syncMode() == ClockSyncMode::Internal
            || (clock_.syncMode() == ClockSyncMode::HostPpq && clock_.running() && clock_.hostAnchorSeen())
            || (clock_.syncMode() == ClockSyncMode::MidiClock && clock_.running() && clock_.midiClockSeen());
        if (externalReady) {
            fireArpStep();
            clock_.restart(false);
        }
    }
}

void P6PerformanceScheduler::noteOff(int midiNote)
{
    midiNote = std::clamp(midiNote, 0, 127);
    arpeggiator_.noteOff(midiNote);

    if (sequencePlaying_) {
        engine_.noteOffOwned(midiNote, 0);
        return;
    }

    if (!arpEnabled_) {
        engine_.noteOffOwned(midiNote, 0);
        return;
    }

    if (!arpBeatSync_) {
        const bool externalReady = clock_.syncMode() == ClockSyncMode::Internal
            || (clock_.syncMode() == ClockSyncMode::HostPpq && clock_.running() && clock_.hostAnchorSeen())
            || (clock_.syncMode() == ClockSyncMode::MidiClock && clock_.running() && clock_.midiClockSeen());
        if (arpeggiator_.empty()) {
            releaseArpNote();
        } else if (externalReady) {
            fireArpStep();
            clock_.restart(false);
        }
    }
}

void P6PerformanceScheduler::allNotesOff()
{
    arpeggiator_.clear();
    releaseArpNote();
    releaseSequenceNotes();
    engine_.allNotesOff();
}

void P6PerformanceScheduler::setArpeggiatorEnabled(bool enabled)
{
    if (arpEnabled_ == enabled)
        return;

    if (enabled) {
        stopDirectPhysicalKeys();
        releaseArpNote();
        arpeggiator_.resetStep();
        arpEnabled_ = true;
        const bool externalReady = clock_.syncMode() == ClockSyncMode::Internal
            || (clock_.syncMode() == ClockSyncMode::HostPpq && clock_.running() && clock_.hostAnchorSeen())
            || (clock_.syncMode() == ClockSyncMode::MidiClock && clock_.running() && clock_.midiClockSeen());
        const bool fireNow = !sequencePlaying_ && !arpBeatSync_ && !arpeggiator_.empty() && externalReady;
        if (fireNow)
            fireArpStep();
        clock_.restart(!fireNow);
    } else {
        releaseArpNote();
        arpEnabled_ = false;
        if (!sequencePlaying_)
            restoreDirectPhysicalKeys();
        clock_.restart(true);
    }
    updateClockRunning(false);
}

void P6PerformanceScheduler::setArpMode(ArpMode mode)
{
    arpeggiator_.setMode(mode);
}

void P6PerformanceScheduler::setArpOctaves(std::uint8_t octaves)
{
    arpeggiator_.setOctaves(octaves);
}

void P6PerformanceScheduler::setArpHold(bool enabled)
{
    arpeggiator_.setHold(enabled);
}

void P6PerformanceScheduler::releaseArpNote()
{
    if (activeArpNote_ < 0)
        return;
    engine_.noteOffOwned(activeArpNote_, kArpOwner);
    activeArpNote_ = -1;
}

void P6PerformanceScheduler::fireArpStep()
{
    if (sequencePlaying_ || !arpEnabled_)
        return;

    releaseArpNote();
    const auto next = arpeggiator_.next();
    if (!next)
        return;

    activeArpNote_ = next->note;
    engine_.noteOnOwned(next->note, next->velocity, kArpOwner);
}

void P6PerformanceScheduler::releaseSequenceNotes()
{
    for (std::size_t i = 0; i < activeSequenceCount_; ++i)
        engine_.noteOffOwned(activeSequenceNotes_[i], kSeqOwner);
    activeSequenceCount_ = 0;
}

void P6PerformanceScheduler::fireSequenceStep()
{
    if (!sequencePlaying_)
        return;

    releaseSequenceNotes();
    const auto step = sequencer_.nextStep();
    for (std::size_t i = 0; i < step.count; ++i) {
        engine_.noteOnOwned(step.notes[i], step.velocities[i], kSeqOwner);
        activeSequenceNotes_[activeSequenceCount_++] = step.notes[i];
    }
}

void P6PerformanceScheduler::setSequencePlaying(bool playing)
{
    if (sequencePlaying_ == playing)
        return;

    if (playing) {
        releaseArpNote();
        sequencePlaying_ = true;
        sequencer_.reset();
        clock_.restart(true);
    } else {
        releaseSequenceNotes();
        sequencePlaying_ = false;
        const bool externalReady = clock_.syncMode() == ClockSyncMode::Internal
            || (clock_.syncMode() == ClockSyncMode::HostPpq && clock_.running() && clock_.hostAnchorSeen())
            || (clock_.syncMode() == ClockSyncMode::MidiClock && clock_.running() && clock_.midiClockSeen());
        const bool fireArpNow = arpEnabled_ && !arpBeatSync_ && !arpeggiator_.empty() && externalReady;
        if (fireArpNow)
            fireArpStep();
        clock_.restart(!fireArpNow);
    }
    updateClockRunning(false);
}

StereoSample P6PerformanceScheduler::process()
{
    if (clock_.tick()) {
        if (sequencePlaying_)
            fireSequenceStep();
        else if (arpEnabled_)
            fireArpStep();
    }
    return engine_.process();
}

} // namespace p6
