#pragma once

#include "p6/P6Arpeggiator.h"
#include "p6/P6ArpCharacter.h"
#include "p6/P6Clock.h"
#include "p6/P6Engine.h"
#include "p6/P6SequencerPlayer.h"

#include <array>
#include <cstddef>
#include <cstdint>

namespace p6 {

class P6PerformanceScheduler {
public:
    explicit P6PerformanceScheduler(P6Engine& engine) : engine_(engine) {}

    void prepare(double sampleRate);
    void loadPatch(const P6PatchState& patch,
                   const MapperCalibration& calibration = {});
    void updatePatch(const P6PatchState& patch,
                     const MapperCalibration& calibration = {});

    void noteOn(int midiNote, double velocity01 = 1.0);
    void noteOff(int midiNote);
    void allNotesOff();
    void midiAllNotesOff();
    void midiAllSoundOff();
    void setModWheel(double amount01) { engine_.setModWheel(amount01); }
    void setChannelPressure(double pressure01) { engine_.setChannelPressure(pressure01); }
    void setPitchBend(double bipolar) { engine_.setPitchBend(bipolar); }
    void setSustainPedal(bool down) { engine_.setSustainPedal(down); }
    void setMidiVolume(double amount01) { engine_.setMidiVolume(amount01); }
    void resetMidiControllers()
    {
        engine_.setModWheel(0.0);
        engine_.setPitchBend(0.0);
        engine_.setChannelPressure(0.0);
        engine_.setSustainPedal(false);
        engine_.setMidiVolume(1.0);
    }

    void setArpeggiatorEnabled(bool enabled);
    void setArpMode(ArpMode mode);
    void setArpOctaves(std::uint8_t octaves);
    void setArpHold(bool enabled);
    void setArpBeatSync(bool enabled) noexcept { arpBeatSync_ = enabled; }
    void setArpCharacterEnabled(bool enabled) noexcept { arpCharacter_.setEnabled(enabled); }
    void setArpCharacterMask(std::uint8_t mask) noexcept { arpCharacter_.setMask(mask); }
    void setArpCharacterRhythm(int pulses) noexcept { arpCharacter_.setRhythm(pulses); }
    void setArpCharacterMotion(double amount) noexcept { arpCharacter_.setMotion(amount); }
    void setArpCharacterFreeze(bool freeze) noexcept { arpCharacter_.setFreeze(freeze); }

    void setClockBpm(double bpm)
    {
        clock_.setBpm(bpm);
        engine_.setEffectsRuntimeBpm(clock_.bpm());
    }
    void setClockDivision(ClockDivision division) { clock_.setDivision(division); }
    void setClockSyncMode(ClockSyncMode mode);
    void alignToHostPpq(double ppq, double bpm, bool playing)
    {
        clock_.alignToHostPpq(ppq, bpm, playing);
        engine_.setEffectsRuntimeBpm(clock_.bpm());
    }
    void setMidiStartStopEnabled(bool enabled);
    void midiClockStart();
    void midiClockContinue();
    void midiClockStop();
    void midiTimingClock()
    {
        clock_.midiTimingClock();
        engine_.setEffectsRuntimeBpm(clock_.bpm());
    }

    void setSequencePlaying(bool playing);
    void setSequenceTransposeKey(int midiNote) { sequencer_.setTransposeKey(midiNote); }
    void setSequenceTransposeSemitones(int semitones) { sequencer_.setTransposeSemitones(semitones); }
    void setSequenceLength(std::size_t steps) { sequencer_.setLength(steps); }

    [[nodiscard]] StereoSample process();

    [[nodiscard]] bool arpeggiatorEnabled() const noexcept { return arpEnabled_; }
    [[nodiscard]] bool sequencePlaying() const noexcept { return sequencePlaying_; }
    [[nodiscard]] bool arpBeatSync() const noexcept { return arpBeatSync_; }
    [[nodiscard]] int arpCharacterStep() const noexcept {
        return arpEnabled_ && !sequencePlaying_ && !arpeggiator_.empty() ? arpCharacterStep_ : -1;
    }
    [[nodiscard]] bool swingTimingIsOpaque() const noexcept { return clock_.swingTimingIsOpaque(); }
    [[nodiscard]] bool sequenceHasOpaqueHighBits() const noexcept { return sequencer_.hasOpaqueHighBits(); }
    [[nodiscard]] std::size_t sequenceLength() const noexcept { return sequencer_.length(); }
    [[nodiscard]] std::size_t sequenceStep() const noexcept { return sequencer_.currentStep(); }
    [[nodiscard]] const P6Clock& clock() const noexcept { return clock_; }
    [[nodiscard]] const P6Arpeggiator& arpeggiator() const noexcept { return arpeggiator_; }
    [[nodiscard]] const P6SequencerPlayer& sequencer() const noexcept { return sequencer_; }

private:
    static constexpr std::uint32_t kArpOwner = 0x41525031u;
    static constexpr std::uint32_t kSeqOwner = 0x53455131u;

    void updateClockRunning(bool restartOnStart = false);
    void releaseArpNote();
    void fireArpStep();
    void releaseSequenceNotes();
    void fireSequenceStep();
    void stopDirectPhysicalKeys();
    void restoreDirectPhysicalKeys();

    P6Engine& engine_;
    P6Clock clock_{};
    P6Arpeggiator arpeggiator_{};
    P6ArpCharacter arpCharacter_{};
    int arpCharacterStep_{-1};
    P6SequencerPlayer sequencer_{};
    bool arpEnabled_{};
    bool arpBeatSync_{};
    bool sequencePlaying_{};
    bool midiStartStopEnabled_{true};
    bool midiTransportRunning_{};
    int activeArpNote_{-1};
    std::array<int, P6Sequence::kTracks> activeSequenceNotes_{};
    std::size_t activeSequenceCount_{};
};

} // namespace p6
