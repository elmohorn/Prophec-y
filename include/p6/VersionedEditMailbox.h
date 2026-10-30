#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace p6 {

// Fixed-size, allocation-free last-writer-wins bridge for realtime control
// edits. A MIDI edit is published to two independent consumers: canonical
// state and host/UI mirroring. Host automation does not need a queued value;
// it only advances the per-parameter generation so an older MIDI mirror can
// no longer win later on the message thread.
//
// One 64-bit atomic contains both generation and value, preventing torn
// generation/value pairs. The lower 16 bits are the raw control value; the
// upper 48 bits are a monotonically increasing generation.
template <std::size_t N>
class VersionedEditMailbox {
public:
    struct Edit {
        std::uint64_t generation{};
        std::uint16_t value{};
    };

    VersionedEditMailbox() noexcept { clear(); }

    [[nodiscard]] std::uint64_t publishMidi(std::size_t index, std::uint16_t value) noexcept
    {
        if (index >= N)
            return 0;
        const auto generation = nextGeneration();
        latest_[index].store(generation, std::memory_order_release);
        const auto packed = pack(generation, value);
        canonical_[index].store(packed, std::memory_order_release);
        mirror_[index].store(packed, std::memory_order_release);
        return generation;
    }

    // Mark a host edit as causally newer than any MIDI value already queued.
    // The caller remains responsible for applying the host value itself.
    [[nodiscard]] std::uint64_t markHost(std::size_t index) noexcept
    {
        if (index >= N)
            return 0;
        const auto generation = nextGeneration();
        latest_[index].store(generation, std::memory_order_release);
        return generation;
    }

    [[nodiscard]] std::optional<Edit> takeCanonicalIfLatest(std::size_t index) noexcept
    {
        return takeIfLatest(canonical_, index);
    }

    [[nodiscard]] std::optional<Edit> takeMirrorIfLatest(std::size_t index) noexcept
    {
        return takeIfLatest(mirror_, index);
    }

    [[nodiscard]] bool isLatest(std::size_t index, std::uint64_t generation) const noexcept
    {
        return index < N && latest_[index].load(std::memory_order_acquire) == generation;
    }

    void clear() noexcept
    {
        next_.store(0, std::memory_order_relaxed);
        for (auto& value : latest_)
            value.store(0, std::memory_order_relaxed);
        for (auto& value : canonical_)
            value.store(0, std::memory_order_relaxed);
        for (auto& value : mirror_)
            value.store(0, std::memory_order_relaxed);
    }

private:
    static constexpr std::uint64_t kValueMask = 0xffffu;
    static constexpr unsigned kValueBits = 16;

    [[nodiscard]] std::uint64_t nextGeneration() noexcept
    {
        // Generation zero is reserved for "nothing published". At practical
        // MIDI/automation rates a 48-bit counter cannot wrap during a session.
        return next_.fetch_add(1, std::memory_order_acq_rel) + 1;
    }

    [[nodiscard]] static std::uint64_t pack(std::uint64_t generation, std::uint16_t value) noexcept
    {
        return (generation << kValueBits) | static_cast<std::uint64_t>(value);
    }

    [[nodiscard]] static Edit unpack(std::uint64_t packed) noexcept
    {
        return Edit { packed >> kValueBits,
                      static_cast<std::uint16_t>(packed & kValueMask) };
    }

    [[nodiscard]] std::optional<Edit> takeIfLatest(
        std::array<std::atomic<std::uint64_t>, N>& queue,
        std::size_t index) noexcept
    {
        if (index >= N)
            return std::nullopt;
        const auto packed = queue[index].exchange(0, std::memory_order_acq_rel);
        if (packed == 0)
            return std::nullopt;
        const auto edit = unpack(packed);
        if (!isLatest(index, edit.generation))
            return std::nullopt;
        return edit;
    }

    std::atomic<std::uint64_t> next_ {0};
    std::array<std::atomic<std::uint64_t>, N> latest_{};
    std::array<std::atomic<std::uint64_t>, N> canonical_{};
    std::array<std::atomic<std::uint64_t>, N> mirror_{};
};

} // namespace p6
