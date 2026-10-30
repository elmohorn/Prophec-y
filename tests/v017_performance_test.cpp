#include "p6/NrpnCodec.h"
#include "p6/P6Engine.h"
#include "p6/P6MidiRouter.h"
#include "p6/P6PerformanceScheduler.h"

#include <array>
#include <cstdint>
#include <iostream>

namespace {

#define P6_CHECK(expr) do { \
    if (!(expr)) { \
        std::cerr << "P6_CHECK failed: " #expr " at " << __FILE__ << ':' << __LINE__ << '\n'; \
        return 1; \
    } \
} while (false)

p6::P6PatchState makeSustainPatch()
{
    p6::P6PatchState patch;
    patch.rawPayload.fill(0);
    const std::array<p6::NrpnUpdate, 13> values {{
        {0, 24},    // Osc 1 reference pitch
        {2, 127},   // Osc 1 level
        {3, 127},   // Osc 1 shape
        {4, 127},   // Osc 1 pulse width
        {45, 164},  // LPF open
        {62, 127},  // Voice volume
        {66, 127},  // VCA envelope amount
        {67, 0},    // Amp attack
        {68, 0},    // Amp decay
        {69, 127},  // Amp sustain
        {70, 0},    // Amp release
        {77, 127},  // Filter envelope neutral
        {158, 2},   // Last-note priority
    }};
    for (const auto value : values)
        p6::applyRawNrpn(patch, value);
    return patch;
}

} // namespace

int main()
{
    p6::P6Engine engine;
    engine.prepare(48000.0, 1);
    p6::P6PerformanceScheduler scheduler(engine);
    scheduler.prepare(48000.0);
    auto patch = makeSustainPatch();
    scheduler.loadPatch(patch);
    p6::P6MidiRouter router;

    const std::array<std::uint8_t, 3> noteOn {0x90, 60, 100};
    const std::array<std::uint8_t, 3> noteOff {0x80, 60, 0};
    const std::array<std::uint8_t, 3> pedalOn {0xB0, 64, 127};
    const std::array<std::uint8_t, 3> pedalOff {0xB0, 64, 0};
    const std::array<std::uint8_t, 3> pedalBelowThreshold {0xB0, 64, 63};
    const std::array<std::uint8_t, 3> pedalAtThreshold {0xB0, 64, 64};

    P6_CHECK(router.process(noteOn, patch, scheduler).consumed);
    for (int i = 0; i < 64; ++i)
        (void)scheduler.process();

    P6_CHECK(router.process(pedalOn, patch, scheduler).consumed);
    P6_CHECK(engine.sustainPedal());
    P6_CHECK(router.process(noteOff, patch, scheduler).consumed);

    // Damper holds the envelope, but the physical key is gone immediately.
    P6_CHECK(engine.sustainedVoiceCount() == 1);
    P6_CHECK(scheduler.arpeggiator().physicalKeyCount() == 0);
    P6_CHECK(!engine.captureChordMemoryFromHeld());

    for (int i = 0; i < 64; ++i)
        (void)scheduler.process();
    P6_CHECK(engine.sustainedVoiceCount() == 1);

    P6_CHECK(router.process(pedalOff, patch, scheduler).consumed);
    P6_CHECK(!engine.sustainPedal());
    P6_CHECK(engine.sustainedVoiceCount() == 0);

    // Sequential's documented Damper threshold is 64.
    P6_CHECK(router.process(pedalBelowThreshold, patch, scheduler).consumed);
    P6_CHECK(!engine.sustainPedal());
    P6_CHECK(router.process(pedalAtThreshold, patch, scheduler).consumed);
    P6_CHECK(engine.sustainPedal());
    P6_CHECK(router.process(pedalOff, patch, scheduler).consumed);

    // Without the pedal, an ordinary Note Off must not create a deferred voice.
    P6_CHECK(router.process(noteOn, patch, scheduler).consumed);
    P6_CHECK(router.process(noteOff, patch, scheduler).consumed);
    P6_CHECK(engine.sustainedVoiceCount() == 0);

    std::cout << "p6 v0.17 sustain/performance tests passed\n";
    return 0;
}
