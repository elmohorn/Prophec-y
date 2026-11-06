#pragma once

#include <array>
#include <atomic>
#include <cstdint>
#include <type_traits>
#include <utility>

// Single audio-thread producer, single UI-thread consumer. A three-slot
// snapshot mailbox publishes *complete* telemetry frames, not unrelated
// per-field atomics. Neither endpoint waits for the other or uses a mutex.
//
// A slot state is 0=free, 1=writer-owned, 2=reader-owned. The audio thread
// claims only slots other than the current published slot. The reader first
// acquires the published index and then claims its slot; if the writer has
// begun recycling it, the claim fails and the reader retries. Once claimed,
// the writer cannot touch the slot until the UI releases it. This avoids the
// C++ data races of a seqlock with ordinary, non-atomic payload fields.
//
// The UI can fail to read during an extreme race; it should retain the last
// validated snapshot and try next tick. The writer similarly skips a block
// rather than waiting (although a third slot is normally free).
namespace p6plugin
{
template<typename Snapshot>
class RealtimeSnapshotMailbox final
{
    static_assert(std::is_trivially_copyable_v<Snapshot>,
                  "A snapshot must be a trivially-copyable value type");
    static_assert(std::atomic<std::uint8_t>::is_always_lock_free &&
                  std::atomic<unsigned>::is_always_lock_free,
                  "This realtime mailbox requires lock-free byte/index atomics");
public:
    RealtimeSnapshotMailbox() noexcept
    {
        for(auto& state: states_)
            state.store(0,std::memory_order_relaxed);
    }

    RealtimeSnapshotMailbox(const RealtimeSnapshotMailbox&)=delete;
    RealtimeSnapshotMailbox& operator=(const RealtimeSnapshotMailbox&)=delete;

    [[nodiscard]] bool publish(const Snapshot& value) noexcept
    {
        const unsigned current=published_.load(std::memory_order_relaxed);
        for(unsigned step=1;step<3;++step)
        {
            const unsigned next=(current+step)%3u;
            std::uint8_t free=0;
            if(!states_[next].compare_exchange_strong(
                   free,1,std::memory_order_acquire,
                   std::memory_order_relaxed))
                continue;

            slots_[next].value=value;
            slots_[next].generation=++writerGeneration_;
            states_[next].store(0,std::memory_order_release);
            published_.store(next,std::memory_order_release);
            publishedGeneration_.store(writerGeneration_,std::memory_order_release);
            return true;
        }
        return false;
    }

    [[nodiscard]] bool tryRead(Snapshot& value,std::uint32_t& generation) const noexcept
    {
        for(unsigned attempt=0;attempt<3;++attempt)
        {
            const unsigned index=published_.load(std::memory_order_acquire);
            std::uint8_t free=0;
            if(!states_[index].compare_exchange_strong(
                   free,2,std::memory_order_acquire,
                   std::memory_order_relaxed))
                continue;

            // This is a complete, immutable slot while reader-owned.
            value=slots_[index].value;
            generation=slots_[index].generation;
            states_[index].store(0,std::memory_order_release);
            return true;
        }
        return false;
    }

    [[nodiscard]] std::uint32_t revision() const noexcept
    {
        return publishedGeneration_.load(std::memory_order_acquire);
    }

private:
    struct Slot {
        Snapshot value{};
        std::uint32_t generation{};
    };
    std::array<Slot,3> slots_{};
    mutable std::array<std::atomic<std::uint8_t>,3> states_{};
    std::atomic<unsigned> published_{0};
    std::atomic<std::uint32_t> publishedGeneration_{0};
    // Only ever touched by the audio thread.
    std::uint32_t writerGeneration_{};
};
} // namespace p6plugin
