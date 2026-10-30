#pragma once

#include "p6/P6PatchState.h"
#include "p6/P6Sequence.h"

#include <array>
#include <cstddef>
#include <cstdint>

namespace p6 {

struct SequencerStepNotes {
    std::array<int, P6Sequence::kTracks> notes{};
    std::array<double, P6Sequence::kTracks> velocities{};
    std::size_t count{};
    bool opaqueHighBitPresent{};
    std::size_t step{};
};

// Playback view over the lossless 6x64 sequence payload. v0.13 intentionally
// does not assign semantics to velocity bit 7: steps carrying it are reported
// through opaqueHighBitPresent, while low-seven-bit note/velocity events can be
// auditioned with a conservative one-step gate in P6PerformanceScheduler.
class P6SequencerPlayer {
public:
    void loadPatch(const P6PatchState& patch);
    // Live patch refresh: update sequence bytes/controls without resetting the
    // current transport step or transpose. Used by host automation/NRPN edits.
    void updatePatch(const P6PatchState& patch);
    void setSequence(const P6Sequence& sequence);
    void setLength(std::size_t steps); // 1..64
    void setLengthRaw(std::uint8_t raw) { setLength(static_cast<std::size_t>(raw) + 1); }
    void setTransposeSemitones(int semitones) noexcept;
    void setTransposeKey(int midiNote) noexcept; // middle C / MIDI 60 = no transpose
    void reset() noexcept { step_ = 0; }

    [[nodiscard]] SequencerStepNotes nextStep();
    [[nodiscard]] std::size_t length() const noexcept { return length_; }
    [[nodiscard]] std::size_t currentStep() const noexcept { return step_; }
    [[nodiscard]] int transposeSemitones() const noexcept { return transposeSemitones_; }
    [[nodiscard]] bool hasOpaqueHighBits() const noexcept { return sequence_.highBitCount() != 0; }
    [[nodiscard]] std::uint8_t seqModeRaw() const noexcept { return seqModeRaw_; }
    [[nodiscard]] std::uint8_t seqPlayModeRaw() const noexcept { return seqPlayModeRaw_; }
    [[nodiscard]] bool seqEnabledRaw() const noexcept { return seqEnabledRaw_; }

private:
    static std::size_t inferredLength(const P6Sequence& sequence) noexcept;

    P6Sequence sequence_{};
    std::size_t length_{1};
    std::size_t step_{};
    int transposeSemitones_{};
    bool seqEnabledRaw_{};
    std::uint8_t seqModeRaw_{};
    std::uint8_t seqPlayModeRaw_{};
};

} // namespace p6
