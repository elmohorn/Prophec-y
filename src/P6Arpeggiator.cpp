#include "p6/P6Arpeggiator.h"

#include <algorithm>
#include <cmath>

namespace p6 {

void P6Arpeggiator::eraseNote(std::array<Key, kMaxKeys>& keys, std::size_t& count, int note)
{
    for (std::size_t i = 0; i < count; ++i) {
        if (keys[i].note != note)
            continue;
        for (std::size_t j = i + 1; j < count; ++j)
            keys[j - 1] = keys[j];
        --count;
        return;
    }
}

void P6Arpeggiator::appendKey(std::array<Key, kMaxKeys>& keys, std::size_t& count, Key key)
{
    eraseNote(keys, count, key.note);
    if (count >= keys.size()) {
        for (std::size_t i = 1; i < count; ++i)
            keys[i - 1] = keys[i];
        --count;
    }
    keys[count++] = key;
}

void P6Arpeggiator::setMode(ArpMode mode)
{
    const auto raw = static_cast<unsigned>(mode);
    mode_ = raw <= static_cast<unsigned>(ArpMode::Assign) ? mode : ArpMode::Up;
    if (mode_ == ArpMode::Assign && assignCount_ == 0)
        rebuildAssignFromArpKeys();
    resetStep();
}

void P6Arpeggiator::setOctaves(std::uint8_t octaves)
{
    octaves_ = std::clamp<std::uint8_t>(octaves, 1, 3);
    resetStep();
}

void P6Arpeggiator::setHold(bool enabled)
{
    if (hold_ == enabled)
        return;
    hold_ = enabled;
    if (!hold_)
        rebuildFromPhysical();
    else if (arpCount_ == 0)
        rebuildFromPhysical();
    resetStep();
}

void P6Arpeggiator::setRandomSeed(std::uint32_t seed) noexcept
{
    rng_ = seed != 0 ? seed : 0x6d2b79f5u;
}

void P6Arpeggiator::rebuildFromPhysical()
{
    arpKeys_ = physical_;
    arpCount_ = physicalCount_;
    rebuildAssignFromArpKeys();
}

void P6Arpeggiator::rebuildAssignFromArpKeys()
{
    assignCount_ = 0;
    std::array<Key, kMaxKeys> ordered = arpKeys_;
    std::stable_sort(ordered.begin(), ordered.begin() + static_cast<std::ptrdiff_t>(arpCount_),
                     [](const Key& a, const Key& b) { return a.order < b.order; });
    for (std::size_t i = 0; i < arpCount_ && assignCount_ < assignEvents_.size(); ++i)
        assignEvents_[assignCount_++] = ordered[i];
}

void P6Arpeggiator::appendAssignEvent(Key key)
{
    // The observed Prophet-6 Assign scratchpad is finite (32 presses). Do not
    // silently turn that into a sliding window: once full, further presses are
    // ignored until the latch is replaced/cleared.
    if (assignCount_ >= assignEvents_.size())
        return;
    assignEvents_[assignCount_++] = key;
}

void P6Arpeggiator::eraseAssignNote(int note)
{
    std::size_t write = 0;
    for (std::size_t read = 0; read < assignCount_; ++read)
        if (assignEvents_[read].note != note)
            assignEvents_[write++] = assignEvents_[read];
    assignCount_ = write;
}

void P6Arpeggiator::noteOn(int midiNote, double velocity01)
{
    midiNote = std::clamp(midiNote, 0, 127);
    velocity01 = std::clamp(velocity01, 0.0, 1.0);
    const bool wasPhysicallyEmpty = physicalCount_ == 0;
    const Key key{midiNote, velocity01, ++orderCounter_};
    appendKey(physical_, physicalCount_, key);

    if (!hold_) {
        appendKey(arpKeys_, arpCount_, key);
        // Without HOLD, Assign is simply the press order of the currently held
        // unique keys. A repeated physical pitch cannot be held twice.
        rebuildAssignFromArpKeys();
    } else {
        // Prophet HOLD auto-latch: after every physical key has been released,
        // the next newly played chord replaces the old latch. While at least
        // one key remains held, additional notes are added to the arpeggio.
        if (wasPhysicallyEmpty) {
            arpCount_ = 0;
            assignCount_ = 0;
            step_ = 0;
        }
        appendKey(arpKeys_, arpCount_, key);
        // Assign on the P6 behaves as an event scratchpad rather than a set:
        // repeated pitches are retained, up to the observed 32 presses.
        appendAssignEvent(key);
    }
}

void P6Arpeggiator::noteOff(int midiNote)
{
    eraseNote(physical_, physicalCount_, midiNote);
    if (!hold_) {
        eraseNote(arpKeys_, arpCount_, midiNote);
        eraseAssignNote(midiNote);
    }
    if (arpCount_ == 0)
        resetStep();
}

void P6Arpeggiator::clear()
{
    physicalCount_ = 0;
    arpCount_ = 0;
    assignCount_ = 0;
    step_ = 0;
}

std::size_t P6Arpeggiator::buildExpanded(std::array<ArpNote, kMaxExpandedNotes>& out,
                                         bool assignOrder) const
{
    if (arpCount_ == 0)
        return 0;

    std::array<Key, kMaxAssignEvents> keys{};
    std::size_t keyCount = 0;
    if (assignOrder) {
        keyCount = assignCount_;
        for (std::size_t i = 0; i < keyCount; ++i)
            keys[i] = assignEvents_[i];
    } else {
        keyCount = arpCount_;
        for (std::size_t i = 0; i < keyCount; ++i)
            keys[i] = arpKeys_[i];
        std::stable_sort(keys.begin(), keys.begin() + static_cast<std::ptrdiff_t>(keyCount),
                         [](const Key& a, const Key& b) {
                             if (a.note != b.note) return a.note < b.note;
                             return a.order < b.order;
                         });
    }

    std::size_t n = 0;
    for (std::uint8_t octave = 0; octave < octaves_; ++octave) {
        const int shift = static_cast<int>(octave) * 12;
        for (std::size_t i = 0; i < keyCount; ++i) {
            const int note = keys[i].note + shift;
            if (note > 127)
                continue;
            out[n++] = ArpNote{note, keys[i].velocity};
        }
    }

    if (!assignOrder) {
        std::stable_sort(out.begin(), out.begin() + static_cast<std::ptrdiff_t>(n),
                         [](const ArpNote& a, const ArpNote& b) { return a.note < b.note; });
    }
    return n;
}

std::uint32_t P6Arpeggiator::nextRandom() noexcept
{
    // xorshift32: deterministic for regression tests. The Prophet's exact RNG
    // is not documented, so this is intentionally not presented as a hardware
    // random-sequence clone.
    std::uint32_t x = rng_;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    rng_ = x != 0 ? x : 0x6d2b79f5u;
    return rng_;
}

std::optional<ArpNote> P6Arpeggiator::next()
{
    std::array<ArpNote, kMaxExpandedNotes> expanded{};
    const bool assign = mode_ == ArpMode::Assign;
    const std::size_t n = buildExpanded(expanded, assign);
    if (n == 0)
        return std::nullopt;

    std::size_t index = 0;
    switch (mode_) {
    case ArpMode::Up:
    case ArpMode::Assign:
        index = static_cast<std::size_t>(step_ % n);
        break;
    case ArpMode::Down:
        index = n - 1 - static_cast<std::size_t>(step_ % n);
        break;
    case ArpMode::UpDown:
        if (n <= 1) {
            index = 0;
        } else {
            // End points are not repeated: 0,1,2,1,0,1,2...
            const std::size_t period = 2 * (n - 1);
            const std::size_t p = static_cast<std::size_t>(step_ % period);
            index = p < n ? p : period - p;
        }
        break;
    case ArpMode::Random:
        index = static_cast<std::size_t>(nextRandom() % static_cast<std::uint32_t>(n));
        break;
    }

    ++step_;
    return expanded[index];
}

} // namespace p6
