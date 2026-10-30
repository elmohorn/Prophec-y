#pragma once

#include "p6/P6PatchState.h"

#include <array>
#include <cstddef>
#include <cstdint>

namespace p6 {

// Prophet-6 program dumps reserve 768 bytes for the polyphonic sequencer:
// six tracks, each with 64 note bytes followed by 64 velocity/flag bytes.
//
// The low seven bits of the velocity byte are the MIDI velocity. Factory
// programs also use bit 7. Its exact firmware meaning is intentionally exposed
// as a raw flag rather than normalised away; current evidence is consistent
// with a gate/retrigger marker, but that interpretation is not required for
// lossless decoding.
struct SequenceCell {
    std::uint8_t noteRaw{};
    std::uint8_t velocityRaw{};
    std::uint8_t velocity{};       // velocityRaw & 0x7f
    bool highBitFlag{};            // velocityRaw & 0x80, semantics preserved

    [[nodiscard]] bool hasVelocity() const noexcept { return velocity != 0; }
    // Factory dumps initialise unused note slots to middle C (60), so the
    // velocity/flag byte is the reliable indicator that a slot is populated.
    [[nodiscard]] bool isPopulated() const noexcept { return velocityRaw != 0; }
    [[nodiscard]] bool hasRawData() const noexcept { return noteRaw != 0 || velocityRaw != 0; }
};

struct P6Sequence {
    static constexpr std::size_t kTracks = 6;
    static constexpr std::size_t kSteps = 64;
    static constexpr std::size_t kBytesPerTrack = 128;
    static constexpr std::size_t kPayloadOffset = 128;
    static constexpr std::size_t kPayloadSize = kTracks * kBytesPerTrack;

    using Track = std::array<SequenceCell, kSteps>;
    std::array<Track, kTracks> tracks{};

    [[nodiscard]] std::size_t populatedCellCount() const noexcept;
    [[nodiscard]] std::size_t velocityEventCount() const noexcept;
    [[nodiscard]] std::size_t highBitCount() const noexcept;
};

[[nodiscard]] P6Sequence decodeSequence(const P6PatchState& patch);

// Writes only bytes 128..895. All other program bytes stay untouched.
void encodeSequence(P6PatchState& patch, const P6Sequence& sequence);

// Map an official sequence-note/velocity NRPN (256..1023) to its cell.
// Returns false for non-sequence NRPNs. This mapping is the documented NRPN
// namespace; it is deliberately separate from raw SysEx byte offsets.
struct SequenceNrpnAddress {
    std::size_t track{};
    std::size_t step{};
    bool velocity{};
};
[[nodiscard]] bool sequenceAddressForNrpn(std::uint16_t nrpn, SequenceNrpnAddress& out) noexcept;

} // namespace p6
