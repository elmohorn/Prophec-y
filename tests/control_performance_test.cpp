#include "p6/P6Engine.h"
#include "p6/P6MidiRouter.h"
#include "p6/P6PatchState.h"
#include "p6/P6PerformanceScheduler.h"

#include <array>
#include <cassert>
#include <cstdint>
#include <span>

using namespace p6;

namespace {

VoiceParams playableVoice()
{
    VoiceParams p;
    p.osc1.level = 0.8;
    p.osc2.level = 0.0;
    p.subLevel = 0.0;
    p.noiseLevel = 0.0;
    p.lowPass.cutoffControl = 164.0;
    p.highPass.cutoffControl = 0.0;
    p.ampAttack = 0.0005;
    p.ampDecay = 0.001;
    p.ampSustain = 0.8;
    p.ampRelease = 0.001;
    p.filterAttack = 0.0005;
    p.filterDecay = 0.001;
    p.filterSustain = 0.0;
    p.filterRelease = 0.001;
    return p;
}

MidiRouteResult route(P6MidiRouter& router,
                      P6PatchState& patch,
                      P6PerformanceScheduler& scheduler,
                      std::uint8_t status,
                      std::uint8_t d1,
                      std::uint8_t d2)
{
    const std::array<std::uint8_t, 3> msg { status, d1, d2 };
    return router.process(std::span<const std::uint8_t>(msg), patch, scheduler);
}

void render(P6PerformanceScheduler& scheduler, int samples)
{
    for (int i = 0; i < samples; ++i)
        (void)scheduler.process();
}

void testDamperKeepsEnvelopeButNotPhysicalKey()
{
    P6Engine engine;
    engine.prepare(48000.0, 1);
    engine.setVoiceParams(playableVoice());
    P6PerformanceScheduler scheduler(engine);
    scheduler.prepare(48000.0);

    scheduler.noteOn(60, 1.0);
    scheduler.setSustainPedal(true);
    scheduler.noteOff(60);

    assert(engine.sustainPedal());
    assert(engine.sustainedVoiceCount() == 1);
    // Damper must not turn a physically released key into Chord Memory input.
    assert(!engine.captureChordMemoryFromHeld());

    render(scheduler, 2048);
    assert(engine.activeVoiceCount() == 1);

    scheduler.setSustainPedal(false);
    assert(!engine.sustainPedal());
    assert(engine.sustainedVoiceCount() == 0);
    render(scheduler, 2048);
    assert(engine.activeVoiceCount() == 0);
}

void testStolenSustainMarkerCannotKillReusedVoice()
{
    P6Engine engine;
    engine.prepare(48000.0, 1);
    engine.setVoiceParams(playableVoice());
    P6PerformanceScheduler scheduler(engine);
    scheduler.prepare(48000.0);

    scheduler.setSustainPedal(true);
    for (int note = 60; note < 66; ++note) {
        scheduler.noteOn(note, 1.0);
        scheduler.noteOff(note);
    }
    assert(engine.sustainedVoiceCount() == P6Engine::kVoiceCount);

    // Reuse every sustained slot while the pedal is still down. The old age
    // markers must become stale, so pedal-up cannot release the new key-down voices.
    for (int note = 72; note < 78; ++note)
        scheduler.noteOn(note, 1.0);

    scheduler.setSustainPedal(false);
    assert(engine.sustainedVoiceCount() == 0);
    assert(engine.activeVoiceCount() == P6Engine::kVoiceCount);

    render(scheduler, 512);
    assert(engine.activeVoiceCount() == P6Engine::kVoiceCount);
    scheduler.allNotesOff();
}

void testCc64ThresholdAndResetControllers()
{
    P6Engine engine;
    engine.prepare(48000.0, 1);
    engine.setVoiceParams(playableVoice());
    P6PerformanceScheduler scheduler(engine);
    scheduler.prepare(48000.0);
    P6MidiRouter router;
    P6PatchState patch;

    auto result = route(router, patch, scheduler, 0xB0, 64, 63);
    assert(result.consumed);
    assert(!engine.sustainPedal());

    result = route(router, patch, scheduler, 0xB0, 64, 64);
    assert(result.consumed);
    assert(engine.sustainPedal());

    result = route(router, patch, scheduler, 0xB0, 1, 127);
    assert(result.consumed);
    assert(engine.modWheel() > 0.999);

    // Prophet-6 CC121 clears MIDI controllers to zero (volume is handled
    // separately when CC7 is added). It must at least clear the implemented
    // Mod Wheel and Damper states now.
    result = route(router, patch, scheduler, 0xB0, 121, 0);
    assert(result.consumed);
    assert(!engine.sustainPedal());
    assert(engine.modWheel() == 0.0);
}

void testAllNotesOffOverridesDeferredDamperVoices()
{
    P6Engine engine;
    engine.prepare(48000.0, 1);
    engine.setVoiceParams(playableVoice());
    P6PerformanceScheduler scheduler(engine);
    scheduler.prepare(48000.0);
    P6MidiRouter router;
    P6PatchState patch;

    route(router, patch, scheduler, 0x90, 60, 127);
    route(router, patch, scheduler, 0xB0, 64, 127);
    route(router, patch, scheduler, 0x80, 60, 0);
    assert(engine.sustainedVoiceCount() == 1);

    auto result = route(router, patch, scheduler, 0xB0, 123, 0);
    assert(result.consumed);
    render(scheduler, 2048);
    assert(engine.activeVoiceCount() == 0);
}

} // namespace

int main()
{
    testDamperKeepsEnvelopeButNotPhysicalKey();
    testStolenSustainMarkerCannotKillReusedVoice();
    testCc64ThresholdAndResetControllers();
    testAllNotesOffOverridesDeferredDamperVoices();
    return 0;
}
