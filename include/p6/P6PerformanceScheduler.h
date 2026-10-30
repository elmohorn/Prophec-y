#pragma once

#include "p6/P6Arpeggiator.h"
#include "p6/P6Clock.h"
#include "p6/P6Engine.h"
#include "p6/P6SequencerPlayer.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>

namespace p6 {

// Higher-level musical scheduler that sits above P6Engine. It owns no synth
// program state: SysEx/NRPN remains canonical in P6PatchState and audio
// synthesis stays in P6Engine. Runtime performance controllers live here.
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
    void setModWheel(double amount01) { engine_.setModWheel(amount01); }
    void setChannelPressure(double pressure01) { engine_.setChannelPressure(pressure01); }
    void setPitchBend(double bipolar) { engine_.setPitchBend(bipolar); }
    void setSustainPedal(bool enabled) { engine_.setSustainPedal(enabled); }
    // CC7 is documented as being combined with Master Volume and Voice Volume.
    // Keep it as independent performance gain so it never rewrites patch bytes
    // or changes the amount driven into the synth's distortion stage.
    void setMidiVolume(double amount01) noexcept { midiVolume_ = std::clamp(amount01, 0.0, 1.0); }

    void setArpeggiatorEnabled(bool enabled);
    void setArpMode(ArpMode mode);
    void setArpOctaves(std::uint8_t octaves);
    void setArpHold(bool enabled);
    void setArpBeatSync(bool enabled) noexcept { arpBeatSync_ = enabled; }

    void setClockBpm(double bpm) { clock_.setBpm(bpm); }
    void setClockDivision(ClockDivision division) { clock_.setDivision(division); }
    void setClockSyncMode(ClockSyncMode mode);
    void alignToHostPpq(double ppq, double bpm, bool playing) { clock_.alignToHostPpq(ppq, bpm, playing); }
    void setMidiStartStopEnabled(bool enabled);
    void midiClockStart();
    void midiClockContinue();
    void midiClockStop();
    void midiTimingClock() { clock_.midiTimingClock(); }

    void setSequencePlaying(bool playing);
    void setSequenceTransposeKey(int midiNote) { sequencer_.setTransposeKey(midiNote); }
    void setSequenceTransposeSemitones(int semitones) { sequencer_.setTransposeSemitones(semitones); }
    void setSequenceLength(std::size_t steps) { sequencer_.setLength(steps); }

    [[nodiscard]] StereoSample process();

    [[nodiscard]] bool arpeggiatorEnabled() const noexcept { return arpEnabled_; }
    [[nodiscard]] bool sequencePlaying() const noexcept { return sequencePlaying_; }
    [[nodiscard]] bool arpBeatSync() const noexcept { return arpBeatSync_; }
    [[nodiscard]] double midiVolume() const noexcept { return midiVolume_; }
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
    double midiVolume_{1.0};
    int activeArpNote_{-1};
    std::array<int, P6Sequence::kTracks> activeSequenceNotes_{};
    std::size_t activeSequenceCount_{};
};

} // namespace p6
