#pragma once

#include "p6/RealtimeSysexDecoder.h"
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>

namespace p6 {
// Ordered, bounded transport for an audio-thread producer and one control-
// thread consumer. A failed push means the caller must report overflow and
// must not silently treat the remaining stream as complete.
struct RealtimeControlEvent {
    enum class Type : std::uint8_t { sysex, nrpn };
    Type type{Type::sysex};
    RealtimeSysexPayload sysex{};
    std::uint16_t parameter{}, value{};
};

template<std::size_t Capacity>
class RealtimeControlQueue {
    static_assert(Capacity > 0);
    static_assert(std::atomic<std::size_t>::is_always_lock_free);
public:
    bool push(const RealtimeControlEvent& event) noexcept {
        const auto write = write_.load(std::memory_order_relaxed);
        const auto next = (write + 1) % slots_.size();
        if (next == read_.load(std::memory_order_acquire)) return false;
        slots_[write] = event;
        write_.store(next, std::memory_order_release);
        return true;
    }
    bool pop(RealtimeControlEvent& event) noexcept {
        const auto read = read_.load(std::memory_order_relaxed);
        if (read == write_.load(std::memory_order_acquire)) return false;
        event = slots_[read];
        read_.store((read + 1) % slots_.size(), std::memory_order_release);
        return true;
    }
private:
    std::array<RealtimeControlEvent, Capacity + 1> slots_{};
    std::atomic<std::size_t> write_{0}, read_{0};
};
}
