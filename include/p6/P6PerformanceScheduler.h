#pragma once

#include "p6/P6Arpeggiator.h"
#include "p6/P6Clock.h"
#include "p6/P6Engine.h"
#include "p6/P6SequencerPlayer.h"

#include <array>
#include <cstddef>
#include <cstdint>

namespace p6 {

// Higher-level musical scheduler that sits above P6Engine. It owns no synth
// state: SysEx/NRPN remains canonical in P6PatchState and audio synthesis stays
// in P6Engine. This layer only decides when notes should reach the engine.
class P6PerformanceScheduler {
public:
    explicit P6PerformanceScheduler(P6Engine& engine) : engine_(engine) {}

    void prepare(double sampleRate);
    void loadPatch(const P6PatchState& patch,
                   const MapperCalibration& calibration = {});
    // Refresh host-automated/editor parameters without treating them as a new
    // program load. This preserves chord memory, held keys, arp latch and
    // sequencer transport while updating the mapped hardware controls.
    void updatePatch(const P6PatchState& patch,
                     const MapperCalibration& calibration = {});

    void noteOn(int midiNote, double velocity01 = 1.0);
    void noteOff(int midiNote);
    void allNotesOff();
    void setModWheel(double amount01) { engine_.setModWheel(amount01); }
    void setChannelPressure(double pressure01) { engine_.setChannelPressure(pressure01); }
    void setPitchBend(double bipolar) { engine_.setPitchBend(bipolar); }
    void setSustainPedal(bool down) { engine_.setSustainPedal(down); }
    void setMidiVolume(double amount01) { engine_.setMidiVolume(amount01); }
    // CC121 resets the continuous controllers currently modeled by this core.
    // Channel pressure and pitch bend are separate MIDI message classes and are
    // intentionally not rewritten by this CC operation.
    void resetMidiControllers()
    {
        engine_.setModWheel(0.0);
        engine_.setSustainPedal(false);
        engine_.setMidiVolume(1.0);
    }

    void setArpeggiatorEnabled(bool enabled);
    void setArpMode(ArpMode mode);
    void setArpOctaves(std::uint8_t octaves);
    void setArpHold(bool enabled);
    // OS 1.5.9+ Global: false = legacy immediate retrigger, true = quantized
    // chord changes only take effect on the next clock division.
    void setArpBeatSync(bool enabled) noexcept { arpBeatSync_ = enabled; }

    void setClockBpm(double bpm) { clock_.setBpm(bpm); }
    void setClockDivision(ClockDivision division) { clock_.setDivision(division); }
    void setClockSyncMode(ClockSyncMode mode);
    void alignToHostPpq(double ppq, double bpm, bool playing) { clock_.alignToHostPpq(ppq, bpm, playing); }
    // true models Prophet MIDI Clock `In`; false models `In, No Start/Stop`.
    void setMidiStartStopEnabled(bool enabled);
    void midiClockStart();
    void midiClockContinue();
    void midiClockStop();
    void midiTimingClock() { clock_.midiTimingClock(); }

    // Sequencer PLAY is a performance/control state (NRPN 1088), not a stored
    // program byte. Starting it suppresses the arpeggiator, matching hardware.
    void setSequencePlaying(bool playing);
    void setSequenceTransposeKey(int midiNote) { sequencer_.setTransposeKey(midiNote); }
    void setSequenceTransposeSemitones(int semitones) { sequencer_.setTransposeSemitones(semitones); }
    void setSequenceLength(std::size_t steps) { sequencer_.setLength(steps); }

    [[nodiscard]] StereoSample process();

    [[nodiscard]] bool arpeggiatorEnabled() const noexcept { return arpEnabled_; }
    [[nodiscard]] bool sequencePlaying() const noexcept { return sequencePlaying_; }
    [[nodiscard]] bool arpBeatSync() const noexcept { return arpBeatSync_; }
    [[nodiscard]] bool swingTimingIsOpaque() const noexcept { return clock_.swingTimingIsOpaque(); }
    [[nodiscard]] bool sequenceHasOpaqueHighBits() const noexcept { return sequencer_.hasOpaqueHighBits(); }
    [[nodiscard]] std::size_t sequenceLength() const noexcept { return sequencer_.length(); }
    [[nodiscard]] std::size_t sequenceStep() const noexcept { return sequencer_.currentStep(); }
    [[nodiscard]] const P6Clock& clock() const noexcept { return clock_; }
    [[nodiscard]] const P6Arpeggiator& arpeggiator() const noexcept { return arpeggiator_; }
    [[nodiscard]] const P6SequencerPlayer& sequencer() const noexcept { return sequencer_; }

private:
    static constexpr std::uint32_t kArpOwner = 0x41525031u; // "ARP1"
    static constexpr std::uint32_t kSeqOwner = 0x53455131u; // "SEQ1"

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
