#include "p6/P6Engine.h"
#include "p6/P6Effects.h"
#include "p6/P6MidiRouter.h"
#include "p6/P6PerformanceScheduler.h"
#include "p6/ParameterMapper.h"
#include "p6/ProgramLayout.h"

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdlib>
#include <iostream>
#include <new>

namespace {
std::atomic<bool> gTrackAllocations {false};
std::atomic<std::size_t> gAllocationCount {0};

void recordAllocation() noexcept
{
    if (gTrackAllocations.load(std::memory_order_relaxed))
        gAllocationCount.fetch_add(1, std::memory_order_relaxed);
}

void* allocateRaw(std::size_t size) noexcept
{
    return std::malloc(size == 0 ? 1 : size);
}
} // namespace

void* operator new(std::size_t size)
{
    recordAllocation();
    if (void* p = allocateRaw(size))
        return p;
    throw std::bad_alloc();
}

void* operator new[](std::size_t size)
{
    return ::operator new(size);
}

void* operator new(std::size_t size, const std::nothrow_t&) noexcept
{
    recordAllocation();
    return allocateRaw(size);
}

void* operator new[](std::size_t size, const std::nothrow_t& tag) noexcept
{
    return ::operator new(size, tag);
}

void operator delete(void* p) noexcept { std::free(p); }
void operator delete[](void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }
void operator delete[](void* p, std::size_t) noexcept { std::free(p); }
void operator delete(void* p, const std::nothrow_t&) noexcept { std::free(p); }
void operator delete[](void* p, const std::nothrow_t&) noexcept { std::free(p); }

namespace {

[[noreturn]] void fail(const char* expression, int line)
{
    std::cerr << "CHECK failed at line " << line << ": " << expression << '\n';
    std::exit(1);
}

#define P6_CHECK(expr) do { if (!(expr)) fail(#expr, __LINE__); } while (false)

class AllocationScope {
public:
    AllocationScope()
    {
        gAllocationCount.store(0, std::memory_order_relaxed);
        gTrackAllocations.store(true, std::memory_order_release);
    }

    ~AllocationScope()
    {
        gTrackAllocations.store(false, std::memory_order_release);
    }

    std::size_t count() const noexcept
    {
        return gAllocationCount.load(std::memory_order_acquire);
    }
};

void testCoreRealtimePathDoesNotAllocateAfterPrepare()
{
    p6::P6PatchState patch;
    patch.rawPayload.fill(0);
    patch.rawParameters.reserve(1024);
    p6::populateRawParametersFromPayload(patch);

    p6::P6Engine engine;
    engine.prepare(48000.0, 2);
    p6::P6PerformanceScheduler scheduler(engine);
    scheduler.prepare(48000.0);
    scheduler.loadPatch(patch);
    p6::P6MidiRouter router;

    const std::array<std::uint8_t, 3> noteOn {0x90, 60, 100};
    const std::array<std::uint8_t, 3> noteOff {0x80, 60, 0};
    const std::array<std::uint8_t, 3> modWheel {0xB0, 1, 80};
    const std::array<std::uint8_t, 3> sustainOn {0xB0, 64, 127};
    const std::array<std::uint8_t, 3> sustainOff {0xB0, 64, 0};
    const std::array<std::uint8_t, 3> pitchBend {0xE0, 0, 80};
    const std::array<std::uint8_t, 3> nrpnMsb {0xB0, 99, 0};
    const std::array<std::uint8_t, 3> nrpnLsb {0xB0, 98, 45}; // LPF cutoff
    const std::array<std::uint8_t, 3> dataMsb {0xB0, 6, 80};

    // Warm every path before the allocation fence. Preparation, vector reserve,
    // codec setup and any one-time library work are explicitly non-realtime.
    router.process(noteOn, patch, scheduler);
    for (int i = 0; i < 64; ++i)
        static_cast<void>(scheduler.process());
    router.process(noteOff, patch, scheduler);
    router.process(nrpnMsb, patch, scheduler);
    router.process(nrpnLsb, patch, scheduler);
    router.process(dataMsb, patch, scheduler);

    std::size_t allocations = 0;
    {
        AllocationScope guard;
        for (int block = 0; block < 128; ++block)
        {
            router.process(noteOn, patch, scheduler);
            router.process(modWheel, patch, scheduler);
            router.process(pitchBend, patch, scheduler);
            router.process(sustainOn, patch, scheduler);
            router.process(noteOff, patch, scheduler);

            // Exercise a hardware-style live patch edit while voices and the
            // scheduler are running. rawParameters already has fixed capacity.
            router.process(nrpnMsb, patch, scheduler);
            router.process(nrpnLsb, patch, scheduler);
            router.process(dataMsb, patch, scheduler);

            for (int sample = 0; sample < 64; ++sample)
                static_cast<void>(scheduler.process());
            router.process(sustainOff, patch, scheduler);
        }
        allocations = guard.count();
    }

    P6_CHECK(allocations == 0);
}

void testPreparedActiveEffectsDoNotAllocate()
{
    p6::P6Effects effects;
    effects.prepare(48000.0);

    const std::array<p6::P6EffectSlotSettings, 4> activeFamilies{{
        {p6::P6EffectType::BbdDelay, 0.7, 0.18, 0.6},
        {p6::P6EffectType::Phaser1, 0.8, 0.9, 0.7},
        {p6::P6EffectType::Flanger1, 0.8, 0.4, 0.8},
        {p6::P6EffectType::Hall, 0.6, 0.65, 0.5},
    }};

    p6::P6EffectsSettings settings;
    settings.enabled = true;
    settings.a = activeFamilies[0];
    settings.b = {p6::P6EffectType::Plate, 0.35, 0.5, 0.4};
    effects.setSettings(settings);

    // Warm transcendental/libm and every prepared state machine outside the
    // allocation fence. setSettings() itself is a fixed-size copy.
    for (const auto family : activeFamilies) {
        settings.a = family;
        effects.setSettings(settings);
        for (int i = 0; i < 512; ++i)
            static_cast<void>(effects.process(0.1, -0.1));
    }

    std::size_t allocations = 0;
    {
        AllocationScope guard;
        for (int block = 0; block < 128; ++block) {
            settings.a = activeFamilies[static_cast<std::size_t>(block) % activeFamilies.size()];
            effects.setSettings(settings);
            for (int sample = 0; sample < 64; ++sample) {
                const double x = (sample & 1) ? 0.15 : -0.12;
                static_cast<void>(effects.process(x, -x));
            }
        }
        effects.reset(); // clearing tails must also remain realtime-safe
        allocations = guard.count();
    }

    P6_CHECK(allocations == 0);
}

} // namespace

int main()
{
    testCoreRealtimePathDoesNotAllocateAfterPrepare();
    testPreparedActiveEffectsDoNotAllocate();
    std::cout << "p6 v0.18 realtime allocation contract passed\n";
    return 0;
}
