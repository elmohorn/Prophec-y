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
    static_assert(std::atomic<std::uint64_t>::is_always_lock_free,
                  "VersionedEditMailbox requires lock-free 64-bit atomics on the release target");

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

        // Generation allocation and publication happen on different threads.
        // Never use a plain store here: a producer that obtained an older
        // generation and was then descheduled must not overwrite a newer host
        // generation when it resumes.
        if (!advanceLatest(index, generation))
            return generation;

#ifdef P6_MAILBOX_TEST_HOOK
        P6_MAILBOX_TEST_HOOK(1, value);
#endif
        const auto packed = pack(generation, value);
        storeIfNewer(canonical_[index], packed, generation);
        storeIfNewer(mirror_[index], packed, generation);
        return generation;
    }

    // Mark a host edit as causally newer than any MIDI value already queued.
    // The caller remains responsible for applying the host value itself.
    [[nodiscard]] std::uint64_t markHost(std::size_t index) noexcept
    {
        if (index >= N)
            return 0;
        const auto generation = nextGeneration();
        static_cast<void>(advanceLatest(index, generation));
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

    [[nodiscard]] std::uint64_t latestGeneration(std::size_t index) const noexcept
    {
        return index < N ? latest_[index].load(std::memory_order_acquire) : 0;
    }

    void clear() noexcept
    {
        // Clearing may race an audio-thread MIDI publisher during a host state
        // load. A generation barrier invalidates older queue entries without
        // erasing an edit published after the barrier was allocated. Stale
        // queue entries are discarded lazily by takeIfLatest. The global
        // counter is never reset during the object's lifetime.
        const auto barrier = nextGeneration();
#ifdef P6_MAILBOX_TEST_HOOK
        P6_MAILBOX_TEST_HOOK(2, 0);
#endif
        for (std::size_t i = 0; i < N; ++i)
            static_cast<void>(advanceLatest(i, barrier));
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

    [[nodiscard]] bool advanceLatest(std::size_t index, std::uint64_t generation) noexcept
    {
        auto current = latest_[index].load(std::memory_order_acquire);
        while (current < generation)
        {
            if (latest_[index].compare_exchange_weak(current, generation,
                                                     std::memory_order_acq_rel,
                                                     std::memory_order_acquire))
                return true;
            // On failure compare_exchange_weak refreshes current. If another
            // producer has already published a higher generation, the loop
            // exits without regressing the slot.
        }
        return false;
    }

    [[nodiscard]] static std::uint64_t pack(std::uint64_t generation, std::uint16_t value) noexcept
    {
        return (generation << kValueBits) | static_cast<std::uint64_t>(value);
    }

    static void storeIfNewer(std::atomic<std::uint64_t>& slot, std::uint64_t packed,
                             std::uint64_t generation) noexcept
    {
        auto current = slot.load(std::memory_order_relaxed);
        while ((current >> kValueBits) < generation &&
               !slot.compare_exchange_weak(current, packed,
                                            std::memory_order_release,
                                            std::memory_order_relaxed)) {}
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
