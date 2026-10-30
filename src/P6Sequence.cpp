#include "p6/P6Sequence.h"

namespace p6 {

std::size_t P6Sequence::populatedCellCount() const noexcept
{
    std::size_t n = 0;
    for (const auto& track : tracks)
        for (const auto& cell : track)
            n += cell.isPopulated() ? 1u : 0u;
    return n;
}

std::size_t P6Sequence::velocityEventCount() const noexcept
{
    std::size_t n = 0;
    for (const auto& track : tracks)
        for (const auto& cell : track)
            n += cell.hasVelocity() ? 1u : 0u;
    return n;
}

std::size_t P6Sequence::highBitCount() const noexcept
{
    std::size_t n = 0;
    for (const auto& track : tracks)
        for (const auto& cell : track)
            n += cell.highBitFlag ? 1u : 0u;
    return n;
}

P6Sequence decodeSequence(const P6PatchState& patch)
{
    P6Sequence sequence;
    for (std::size_t track = 0; track < P6Sequence::kTracks; ++track) {
        const auto base = P6Sequence::kPayloadOffset + track * P6Sequence::kBytesPerTrack;
        for (std::size_t step = 0; step < P6Sequence::kSteps; ++step) {
            auto& cell = sequence.tracks[track][step];
            cell.noteRaw = patch.rawPayload[base + step];
            cell.velocityRaw = patch.rawPayload[base + P6Sequence::kSteps + step];
            cell.velocity = static_cast<std::uint8_t>(cell.velocityRaw & 0x7fu);
            cell.highBitFlag = (cell.velocityRaw & 0x80u) != 0;
        }
    }
    return sequence;
}

void encodeSequence(P6PatchState& patch, const P6Sequence& sequence)
{
    for (std::size_t track = 0; track < P6Sequence::kTracks; ++track) {
        const auto base = P6Sequence::kPayloadOffset + track * P6Sequence::kBytesPerTrack;
        for (std::size_t step = 0; step < P6Sequence::kSteps; ++step) {
            const auto& cell = sequence.tracks[track][step];
            patch.rawPayload[base + step] = cell.noteRaw;
            // Preserve the exact raw byte. velocity/highBitFlag are views of it.
            patch.rawPayload[base + P6Sequence::kSteps + step] = cell.velocityRaw;
        }
    }
}

bool sequenceAddressForNrpn(std::uint16_t nrpn, SequenceNrpnAddress& out) noexcept
{
    if (nrpn < 256 || nrpn > 1023)
        return false;

    // Official table: 64 notes then 64 velocities, repeated for tracks 1..6.
    const auto relative = static_cast<std::uint16_t>(nrpn - 256);
    const auto track = static_cast<std::size_t>(relative / 128);
    const auto within = static_cast<std::size_t>(relative % 128);
    if (track >= P6Sequence::kTracks)
        return false;

    out.track = track;
    out.velocity = within >= 64;
    out.step = within % 64;
    return true;
}

} // namespace p6
