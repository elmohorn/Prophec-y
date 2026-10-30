#include "p6/P6Engine.h"
#include "p6/P6MidiRouter.h"
#include "p6/P6PerformanceScheduler.h"
#include "p6/VersionedEditMailbox.h"

#include <array>
#include <cmath>
#include <cstdlib>
#include <iostream>

namespace {

[[noreturn]] void fail(const char* expression, int line)
{
    std::cerr << "CHECK failed at line " << line << ": " << expression << '\n';
    std::exit(1);
}

#define P6_CHECK(expr) do { if (!(expr)) fail(#expr, __LINE__); } while (false)

void testVersionedMailboxLastWriterWins()
{
    p6::VersionedEditMailbox<16> mailbox;

    const auto midi1 = mailbox.publishMidi(3, 70);
    P6_CHECK(midi1 != 0);
    const auto host = mailbox.markHost(3);
    P6_CHECK(host > midi1);
    P6_CHECK(!mailbox.takeCanonicalIfLatest(3));
    P6_CHECK(!mailbox.takeMirrorIfLatest(3));

    const auto midi2 = mailbox.publishMidi(3, 80);
    const auto canonical = mailbox.takeCanonicalIfLatest(3);
    const auto mirror = mailbox.takeMirrorIfLatest(3);
    P6_CHECK(canonical.has_value());
    P6_CHECK(mirror.has_value());
    P6_CHECK(canonical->generation == midi2);
    P6_CHECK(canonical->value == 80);
    P6_CHECK(mirror->value == 80);

    mailbox.publishMidi(3, 90);
    const auto newest = mailbox.publishMidi(3, 100);
    const auto collapsed = mailbox.takeCanonicalIfLatest(3);
    P6_CHECK(collapsed.has_value());
    P6_CHECK(collapsed->generation == newest);
    P6_CHECK(collapsed->value == 100);

    mailbox.publishMidi(4, 44);
    mailbox.markHost(3);
    const auto independent = mailbox.takeCanonicalIfLatest(4);
    P6_CHECK(independent.has_value());
    P6_CHECK(independent->value == 44);
}

void testSustainSeparatesPhysicalKeysFromSoundingVoices()
{
    p6::P6Engine engine;
    engine.prepare(48000.0, 1);

    engine.noteOn(60, 1.0);
    engine.setSustainPedal(true);
    engine.noteOff(60);

    P6_CHECK(engine.sustainPedal());
    P6_CHECK(engine.sustainedVoiceCount() == 1);
    // The note still sounds, but it is no longer physically held and therefore
    // must not be available to the chord-memory capture gesture.
    P6_CHECK(!engine.captureChordMemoryFromHeld());

    engine.setSustainPedal(false);
    P6_CHECK(!engine.sustainPedal());
    P6_CHECK(engine.sustainedVoiceCount() == 0);
}

void testSustainDoesNotExtendGeneratedNoteOwnership()
{
    p6::P6Engine engine;
    engine.prepare(48000.0, 1);
    constexpr std::uint32_t generatedOwner = 0x41525031u; // ARP1

    engine.setSustainPedal(true);
    engine.noteOnOwned(64, 1.0, generatedOwner);
    engine.noteOffOwned(64, generatedOwner);
    P6_CHECK(engine.sustainedVoiceCount() == 0);
}

void testUnisonSustainReleasesAsOneStack()
{
    p6::P6Engine engine;
    engine.prepare(48000.0, 1);
    engine.setUnison(true, 0.0, p6::P6Engine::kVoiceCount);
    engine.noteOn(60, 1.0);
    engine.setSustainPedal(true);
    engine.noteOff(60);
    P6_CHECK(engine.sustainedVoiceCount() == p6::P6Engine::kVoiceCount);
    P6_CHECK(!engine.captureChordMemoryFromHeld());
    engine.setSustainPedal(false);
    P6_CHECK(engine.sustainedVoiceCount() == 0);
}

void testMidiRouterPerformanceControllersStayOutOfPatch()
{
    p6::P6PatchState patch;
    patch.rawPayload.fill(0);
    const auto payloadBefore = patch.rawPayload;

    p6::P6Engine engine;
    engine.prepare(48000.0, 1);
    p6::P6PerformanceScheduler scheduler(engine);
    scheduler.prepare(48000.0);
    p6::P6MidiRouter router;

    const std::array<std::uint8_t, 3> noteOn {0x90, 60, 100};
    const std::array<std::uint8_t, 3> sustainOn {0xB0, 64, 64};
    const std::array<std::uint8_t, 3> noteOff {0x80, 60, 0};
    P6_CHECK(router.process(noteOn, patch, scheduler).consumed);
    P6_CHECK(router.process(sustainOn, patch, scheduler).consumed);
    P6_CHECK(router.process(noteOff, patch, scheduler).consumed);
    P6_CHECK(engine.sustainPedal());
    P6_CHECK(engine.sustainedVoiceCount() == 1);

    const std::array<std::uint8_t, 3> volume {0xB0, 7, 32};
    P6_CHECK(router.process(volume, patch, scheduler).consumed);
    P6_CHECK(std::abs(engine.midiVolume() - (32.0 / 127.0)) < 1.0e-12);

    const std::array<std::uint8_t, 3> wheel {0xB0, 1, 127};
    P6_CHECK(router.process(wheel, patch, scheduler).consumed);
    P6_CHECK(std::abs(engine.modWheel() - 1.0) < 1.0e-12);

    const std::array<std::uint8_t, 3> resetControllers {0xB0, 121, 0};
    P6_CHECK(router.process(resetControllers, patch, scheduler).consumed);
    P6_CHECK(!engine.sustainPedal());
    P6_CHECK(engine.sustainedVoiceCount() == 0);
    P6_CHECK(std::abs(engine.midiVolume() - 1.0) < 1.0e-12);
    P6_CHECK(std::abs(engine.modWheel()) < 1.0e-12);

    // CC7/CC64/CC121 are performance state and must not mutate saved program
    // bytes merely because the controller was received.
    P6_CHECK(patch.rawPayload == payloadBefore);
}

void testSustainThresholdIsExactly64()
{
    p6::P6PatchState patch;
    p6::P6Engine engine;
    engine.prepare(48000.0, 1);
    p6::P6PerformanceScheduler scheduler(engine);
    scheduler.prepare(48000.0);
    p6::P6MidiRouter router;

    const std::array<std::uint8_t, 3> below {0xB0, 64, 63};
    const std::array<std::uint8_t, 3> at {0xB0, 64, 64};
    router.process(below, patch, scheduler);
    P6_CHECK(!engine.sustainPedal());
    router.process(at, patch, scheduler);
    P6_CHECK(engine.sustainPedal());
}

} // namespace

int main()
{
    testVersionedMailboxLastWriterWins();
    testSustainSeparatesPhysicalKeysFromSoundingVoices();
    testSustainDoesNotExtendGeneratedNoteOwnership();
    testUnisonSustainReleasesAsOneStack();
    testMidiRouterPerformanceControllersStayOutOfPatch();
    testSustainThresholdIsExactly64();
    std::cout << "p6 v0.17 control-contract tests passed\n";
    return 0;
}
