#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace p6 {

enum class ArpMode : std::uint8_t {
    Up = 0,
    Down = 1,
    UpDown = 2,
    Random = 3,
    Assign = 4
};

struct ArpNote {
    int note{};
    double velocity{1.0};
};

class P6Arpeggiator {
public:
    static constexpr std::size_t kMaxKeys = 16;
    // Field evidence from Prophet-6 users identifies a 32-press Assign
    // scratchpad and, importantly, repeated pitches. Keep this event history
    // separate from the deduplicated set of physically/latched keys.
    static constexpr std::size_t kMaxAssignEvents = 32;
    static constexpr std::size_t kMaxExpandedNotes = kMaxAssignEvents * 3;

    void setMode(ArpMode mode);
    void setOctaves(std::uint8_t octaves); // 1..3
    void setHold(bool enabled);
    void setRandomSeed(std::uint32_t seed) noexcept;

    void noteOn(int midiNote, double velocity01);
    void noteOff(int midiNote);
    void clear();
    void resetStep() noexcept { step_ = 0; }

    [[nodiscard]] std::optional<ArpNote> next();
    [[nodiscard]] bool empty() const noexcept { return arpCount_ == 0; }
    [[nodiscard]] std::size_t keyCount() const noexcept { return arpCount_; }
    [[nodiscard]] std::size_t assignEventCount() const noexcept { return assignCount_; }
    [[nodiscard]] bool hold() const noexcept { return hold_; }
    [[nodiscard]] std::size_t physicalKeyCount() const noexcept { return physicalCount_; }
    [[nodiscard]] int physicalNote(std::size_t index) const noexcept { return index < physicalCount_ ? physical_[index].note : -1; }
    [[nodiscard]] double physicalVelocity(std::size_t index) const noexcept { return index < physicalCount_ ? physical_[index].velocity : 0.0; }
    [[nodiscard]] ArpMode mode() const noexcept { return mode_; }
    [[nodiscard]] std::uint8_t octaves() const noexcept { return octaves_; }

private:
    struct Key {
        int note{-1};
        double velocity{1.0};
        std::uint64_t order{};
    };

    static void eraseNote(std::array<Key, kMaxKeys>& keys, std::size_t& count, int note);
    static void appendKey(std::array<Key, kMaxKeys>& keys, std::size_t& count, Key key);
    void rebuildFromPhysical();
    void rebuildAssignFromArpKeys();
    void appendAssignEvent(Key key);
    void eraseAssignNote(int note);
    std::size_t buildExpanded(std::array<ArpNote, kMaxExpandedNotes>& out, bool assignOrder) const;
    std::uint32_t nextRandom() noexcept;

    std::array<Key, kMaxKeys> physical_{};
    std::size_t physicalCount_{};
    std::array<Key, kMaxKeys> arpKeys_{};
    std::size_t arpCount_{};
    std::array<Key, kMaxAssignEvents> assignEvents_{};
    std::size_t assignCount_{};
    ArpMode mode_{ArpMode::Up};
    std::uint8_t octaves_{1};
    bool hold_{};
    std::uint64_t orderCounter_{};
    std::uint64_t step_{};
    std::uint32_t rng_{0x6d2b79f5u};
};

} // namespace p6
