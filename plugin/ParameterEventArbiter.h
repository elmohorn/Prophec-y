#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace p6plugin {

// Fixed-size causal arbiter for realtime MIDI edits mirrored onto a host-facing
// parameter surface. It deliberately owns no JUCE types so the ordering policy
// can be regression-tested without constructing a plug-in.
//
// Every MIDI or host edit receives one monotonic epoch. Pending MIDI events are
// stored as one packed atomic word per parameter, so there is no torn
// value/epoch pair and no allocation. The message thread discards a delayed
// MIDI mirror if a newer host edit has already been observed for that same
// parameter.
class ParameterEventArbiter {
public:
    // Program/sequence addresses occupy 0..1023. Runtime Seq Play/Stop is the
    // documented NRPN 1088, so one table covers both without a special queue.
    static constexpr std::size_t kParameterSlots = 1089;
    static constexpr std::uint16_t kSequencePlayNrpn = 1088;

    struct Event {
        std::uint16_t parameter{};
        std::uint16_t value{};
        std::uint64_t epoch{};
    };

    ParameterEventArbiter() noexcept { clearPending(); }

    [[nodiscard]] std::uint64_t noteHostEdit(std::uint16_t parameter) noexcept
    {
        if (parameter >= kParameterSlots)
            return 0;
        const auto epoch = nextEpoch();
        latestHostEpoch_[parameter].store(epoch, std::memory_order_release);
        return epoch;
    }

    [[nodiscard]] std::uint64_t queueMidiEdit(std::uint16_t parameter,
                                               std::uint16_t value) noexcept
    {
        if (parameter >= kParameterSlots)
            return 0;
        const auto epoch = nextEpoch();
        const auto packed = pack(epoch, value);
        pendingCanonical_[parameter].store(packed, std::memory_order_release);
        pendingHostMirror_[parameter].store(packed, std::memory_order_release);
        return epoch;
    }

    // Used for program-state addresses only. Runtime controls such as 1088
    // should be drained only through takeHostMirror().
    [[nodiscard]] std::optional<Event> takeCanonical(std::uint16_t parameter) noexcept
    {
        if (parameter >= kParameterSlots)
            return std::nullopt;
        return takeIfCurrent(parameter, pendingCanonical_[parameter]);
    }

    [[nodiscard]] std::optional<Event> takeHostMirror(std::uint16_t parameter) noexcept
    {
        if (parameter >= kParameterSlots)
            return std::nullopt;
        return takeIfCurrent(parameter, pendingHostMirror_[parameter]);
    }

    void clearPending() noexcept
    {
        for (auto& value : pendingCanonical_)
            value.store(0, std::memory_order_relaxed);
        for (auto& value : pendingHostMirror_)
            value.store(0, std::memory_order_relaxed);
    }

    [[nodiscard]] std::uint64_t latestHostEpoch(std::uint16_t parameter) const noexcept
    {
        return parameter < kParameterSlots
            ? latestHostEpoch_[parameter].load(std::memory_order_acquire)
            : 0;
    }

private:
    // Low 16 bits hold value+1, so packed zero remains the empty sentinel.
    // Prophet values are at most 14-bit; the remaining 48 bits are more than
    // enough for a monotonic epoch over any realistic plug-in lifetime.
    static constexpr std::uint64_t kValueMask = 0xffffu;
    static constexpr unsigned kEpochShift = 16;

    [[nodiscard]] std::uint64_t nextEpoch() noexcept
    {
        return serial_.fetch_add(1, std::memory_order_acq_rel) + 1;
    }

    static std::uint64_t pack(std::uint64_t epoch, std::uint16_t value) noexcept
    {
        return (epoch << kEpochShift) | (static_cast<std::uint64_t>(value) + 1u);
    }

    static Event unpack(std::uint16_t parameter, std::uint64_t packed) noexcept
    {
        return Event {
            parameter,
            static_cast<std::uint16_t>((packed & kValueMask) - 1u),
            packed >> kEpochShift
        };
    }

    [[nodiscard]] std::optional<Event> takeIfCurrent(
        std::uint16_t parameter,
        std::atomic<std::uint64_t>& slot) noexcept
    {
        const auto packed = slot.exchange(0, std::memory_order_acq_rel);
        if (packed == 0)
            return std::nullopt;
        const auto event = unpack(parameter, packed);
        const auto hostEpoch = latestHostEpoch_[parameter].load(std::memory_order_acquire);
        if (event.epoch < hostEpoch)
            return std::nullopt;
        return event;
    }

    std::atomic<std::uint64_t> serial_{0};
    std::array<std::atomic<std::uint64_t>, kParameterSlots> latestHostEpoch_{};
    std::array<std::atomic<std::uint64_t>, kParameterSlots> pendingCanonical_{};
    std::array<std::atomic<std::uint64_t>, kParameterSlots> pendingHostMirror_{};
};

} // namespace p6plugin
