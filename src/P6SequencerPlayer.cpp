#include "p6/P6SequencerPlayer.h"

#include <algorithm>

namespace p6 {
namespace {

bool rawParameter(const P6PatchState& patch, std::uint16_t id, std::uint16_t& value)
{
    for (const auto& p : patch.rawParameters) {
        if (p.id == id) {
            value = p.raw;
            return true;
        }
    }
    return false;
}

} // namespace

std::size_t P6SequencerPlayer::inferredLength(const P6Sequence& sequence) noexcept
{
    std::size_t last = 0;
    bool any = false;
    for (std::size_t step = 0; step < P6Sequence::kSteps; ++step) {
        for (std::size_t track = 0; track < P6Sequence::kTracks; ++track) {
            if (sequence.tracks[track][step].isPopulated()) {
                last = step;
                any = true;
            }
        }
    }
    return any ? last + 1 : 1;
}

void P6SequencerPlayer::loadPatch(const P6PatchState& patch)
{
    sequence_ = decodeSequence(patch);
    step_ = 0;
    transposeSemitones_ = 0;

    std::uint16_t raw = 0;
    if (rawParameter(patch, 169, raw))
        setLengthRaw(static_cast<std::uint8_t>(std::min<std::uint16_t>(raw, 63)));
    else
        length_ = inferredLength(sequence_);

    seqEnabledRaw_ = rawParameter(patch, 168, raw) && raw != 0;
    seqModeRaw_ = rawParameter(patch, 170, raw)
        ? static_cast<std::uint8_t>(std::min<std::uint16_t>(raw, 1)) : 0;
    seqPlayModeRaw_ = rawParameter(patch, 171, raw)
        ? static_cast<std::uint8_t>(std::min<std::uint16_t>(raw, 1)) : 0;
}

void P6SequencerPlayer::updatePatch(const P6PatchState& patch)
{
    const auto oldStep = step_;
    const auto oldTranspose = transposeSemitones_;
    sequence_ = decodeSequence(patch);

    std::uint16_t raw = 0;
    if (rawParameter(patch, 169, raw))
        setLengthRaw(static_cast<std::uint8_t>(std::min<std::uint16_t>(raw, 63)));
    else
        length_ = inferredLength(sequence_);

    seqEnabledRaw_ = rawParameter(patch, 168, raw) && raw != 0;
    seqModeRaw_ = rawParameter(patch, 170, raw)
        ? static_cast<std::uint8_t>(std::min<std::uint16_t>(raw, 1)) : 0;
    seqPlayModeRaw_ = rawParameter(patch, 171, raw)
        ? static_cast<std::uint8_t>(std::min<std::uint16_t>(raw, 1)) : 0;

    transposeSemitones_ = oldTranspose;
    step_ = length_ > 0 ? oldStep % length_ : 0;
}

void P6SequencerPlayer::setSequence(const P6Sequence& sequence)
{
    sequence_ = sequence;
    length_ = inferredLength(sequence_);
    step_ = 0;
}

void P6SequencerPlayer::setLength(std::size_t steps)
{
    length_ = std::clamp<std::size_t>(steps, 1, P6Sequence::kSteps);
    if (step_ >= length_)
        step_ %= length_;
}

void P6SequencerPlayer::setTransposeSemitones(int semitones) noexcept
{
    transposeSemitones_ = std::clamp(semitones, -96, 96);
}

void P6SequencerPlayer::setTransposeKey(int midiNote) noexcept
{
    setTransposeSemitones(std::clamp(midiNote, 0, 127) - 60);
}

SequencerStepNotes P6SequencerPlayer::nextStep()
{
    SequencerStepNotes out;
    out.step = step_;

    for (std::size_t track = 0; track < P6Sequence::kTracks; ++track) {
        const auto& cell = sequence_.tracks[track][step_];
        out.opaqueHighBitPresent = out.opaqueHighBitPresent || cell.highBitFlag;
        if (!cell.hasVelocity())
            continue;

        const int note = std::clamp(static_cast<int>(cell.noteRaw) + transposeSemitones_, 0, 127);
        out.notes[out.count] = note;
        out.velocities[out.count] = static_cast<double>(cell.velocity) / 127.0;
        ++out.count;
    }

    step_ = (step_ + 1) % length_;
    return out;
}

} // namespace p6
