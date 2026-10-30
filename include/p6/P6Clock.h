#pragma once

#include <cstdint>

namespace p6 {

// Prophet-6 CLOCK/VALUE order from the operation manual / NRPN 163.
enum class ClockDivision : std::uint8_t {
    Half = 0,
    Quarter = 1,
    Eighth = 2,
    EighthDotted = 3,
    EighthSwing = 4,
    EighthTriplet = 5,
    Sixteenth = 6,
    SixteenthSwing = 7,
    SixteenthTriplet = 8,
    ThirtySecond = 9
};

enum class ClockSyncMode : std::uint8_t {
    Internal = 0,
    HostPpq = 1,
    MidiClock = 2
};

struct ClockDivisionInfo {
    double quarterNotesPerStep{1.0};
    bool swingOpaque{};
};

[[nodiscard]] ClockDivisionInfo clockDivisionInfo(ClockDivision division) noexcept;
[[nodiscard]] const char* clockDivisionName(ClockDivision division) noexcept;

// One clock for every performance source. Internal timing, a DAW/host PPQ
// anchor and MIDI Timing Clock all converge on the same sample-boundary event
// consumed by the arpeggiator/sequencer. This prevents the plugin wrapper from
// growing a second timing implementation later.
class P6Clock {
public:
    void prepare(double sampleRate);
    void setBpm(double bpm);
    void setDivision(ClockDivision division);
    void setSyncMode(ClockSyncMode mode) noexcept;
    void setRunning(bool running, bool restartOnStart = true);
    void restart(bool fireImmediately = true);

    // Host/DAW path: call at the first sample represented by `ppq` (normally a
    // process-block boundary). The next clock edge is derived from musical
    // position rather than accumulated block lengths, avoiding long-term drift.
    void alignToHostPpq(double ppq, double bpm, bool playing);

    // MIDI 1.0 System Real Time path. The wrapper should call midiTimingClock()
    // at the exact sample offset where F8 arrives, before tick() for that sample.
    // MIDI defines 24 timing clocks per quarter note. Start resets clock phase;
    // Continue resumes it; Stop pauses it. Whether Start/Stop should be ignored
    // for Prophet "In, No Start/Stop" is a wrapper/global policy, not a clock law.
    void midiStart() noexcept;
    void midiContinue() noexcept;
    void midiStop() noexcept;
    void midiTimingClock() noexcept;

    // Call exactly once per output sample. Returns true on a step boundary.
    [[nodiscard]] bool tick();

    [[nodiscard]] double sampleRate() const noexcept { return sampleRate_; }
    [[nodiscard]] double bpm() const noexcept { return bpm_; }
    [[nodiscard]] ClockDivision division() const noexcept { return division_; }
    [[nodiscard]] ClockSyncMode syncMode() const noexcept { return syncMode_; }
    [[nodiscard]] bool running() const noexcept { return running_; }
    [[nodiscard]] std::uint64_t stepIndex() const noexcept { return stepIndex_; }
    [[nodiscard]] double nominalStepSamples() const noexcept;
    [[nodiscard]] std::uint32_t midiClocksPerStep() const noexcept;
    [[nodiscard]] bool swingTimingIsOpaque() const noexcept;
    [[nodiscard]] bool midiClockSeen() const noexcept { return midiClockSeen_; }
    [[nodiscard]] bool hostAnchorSeen() const noexcept { return hostAnchorSeen_; }

private:
    double sampleRate_{48000.0};
    double bpm_{120.0};
    ClockDivision division_{ClockDivision::Sixteenth};
    ClockSyncMode syncMode_{ClockSyncMode::Internal};
    bool running_{};
    double samplesUntilNext_{};
    std::uint64_t stepIndex_{};
    std::uint64_t midiPulseIndex_{};
    bool midiBoundaryPending_{};
    bool midiClockSeen_{};
    bool hostAnchorSeen_{};
};

} // namespace p6
