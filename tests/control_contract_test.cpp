#include "p6/P6Engine.h"
#include "p6/P6MidiRouter.h"
#include "p6/P6PerformanceScheduler.h"
#include "p6/VersionedEditMailbox.h"

#include <array>
#include <atomic>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <thread>

namespace {

[[noreturn]] void fail(const char* expression, int line)
{
    std::cerr << "CHECK failed at line " << line << ": " << expression << '\n';
    std::exit(1);
}

#define P6_CHECK(expr) do { if (!(expr)) fail(#expr, __LINE__); } while (false)

void updateMax(std::atomic<std::uint64_t>& target, std::uint64_t value)
{
    auto current = target.load(std::memory_order_relaxed);
    while (current < value
           && !target.compare_exchange_weak(current, value,
                                            std::memory_order_relaxed,
                                            std::memory_order_relaxed)) {}
}

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

    static_cast<void>(mailbox.publishMidi(3, 90));
    const auto newest = mailbox.publishMidi(3, 100);
    const auto collapsed = mailbox.takeCanonicalIfLatest(3);
    P6_CHECK(collapsed.has_value());
    P6_CHECK(collapsed->generation == newest);
    P6_CHECK(collapsed->value == 100);

    static_cast<void>(mailbox.publishMidi(4, 44));
    static_cast<void>(mailbox.markHost(3));
    const auto independent = mailbox.takeCanonicalIfLatest(4);
    P6_CHECK(independent.has_value());
    P6_CHECK(independent->value == 44);
}

void testVersionedMailboxClearIsAGenerationBarrier()
{
    p6::VersionedEditMailbox<4> mailbox;
    const auto before = mailbox.publishMidi(1, 11);
    mailbox.clear();
    P6_CHECK(!mailbox.takeCanonicalIfLatest(1));
    P6_CHECK(!mailbox.takeMirrorIfLatest(1));
    const auto after = mailbox.publishMidi(1, 22);
    P6_CHECK(after > before);
    P6_CHECK(mailbox.latestGeneration(1) == after);
}

void testVersionedMailboxNeverRegressesAcrossThreads()
{
    p6::VersionedEditMailbox<2> mailbox;
    std::atomic<std::uint64_t> maximumGeneration {0};

    constexpr int iterations = 20000;
    std::thread midi([&]
    {
        for (int i = 0; i < iterations; ++i)
            updateMax(maximumGeneration,
                      mailbox.publishMidi(0, static_cast<std::uint16_t>(i & 0x7f)));
    });
    std::thread host([&]
    {
        for (int i = 0; i < iterations; ++i)
            updateMax(maximumGeneration, mailbox.markHost(0));
    });
    midi.join();
    host.join();

    P6_CHECK(mailbox.latestGeneration(0) == maximumGeneration.load());
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
    P6_CHECK(std::abs(engine.pitchBendSemitones()) < 1.0e-12);

    P6_CHECK(patch.rawPayload == payloadBefore);
}

void testPitchWheelUsesFull14BitTravelAndRecentersOnReset()
{
    p6::P6PatchState patch;
    patch.rawPayload.fill(0);
    patch.rawParameters.push_back(p6::RawParameter{31, 2, 0.0f, true});

    p6::P6Engine engine;
    engine.prepare(48000.0, 1);
    p6::P6PerformanceScheduler scheduler(engine);
    scheduler.prepare(48000.0);
    scheduler.loadPatch(patch);
    p6::P6MidiRouter router;

    P6_CHECK(std::abs(engine.pitchBendRangeSemitones() - 2.0) < 1.0e-12);

    const std::array<std::uint8_t, 3> fullUp {0xE0, 0x7f, 0x7f};
    P6_CHECK(router.process(fullUp, patch, scheduler).consumed);
    P6_CHECK(std::abs(engine.pitchBendSemitones() - 2.0) < 1.0e-12);

    const std::array<std::uint8_t, 3> center {0xE0, 0x00, 0x40};
    P6_CHECK(router.process(center, patch, scheduler).consumed);
    P6_CHECK(std::abs(engine.pitchBendSemitones()) < 1.0e-12);

    const std::array<std::uint8_t, 3> fullDown {0xE0, 0x00, 0x00};
    P6_CHECK(router.process(fullDown, patch, scheduler).consumed);
    P6_CHECK(std::abs(engine.pitchBendSemitones() + 2.0) < 1.0e-12);

    P6_CHECK(router.process(fullUp, patch, scheduler).consumed);
    const std::array<std::uint8_t, 3> resetControllers {0xB0, 121, 0};
    P6_CHECK(router.process(resetControllers, patch, scheduler).consumed);
    P6_CHECK(std::abs(engine.pitchBendSemitones()) < 1.0e-12);
}

void testNrpnSelectionIsPerChannel()
{
    p6::P6PatchState patch; p6::P6Engine engine;engine.prepare(48000.,1);
    p6::P6PerformanceScheduler scheduler(engine);scheduler.prepare(48000.);p6::P6MidiRouter router;
    auto send=[&](int channel,int cc,int value){const std::array<std::uint8_t,3> m{static_cast<std::uint8_t>(0xB0+channel),static_cast<std::uint8_t>(cc),static_cast<std::uint8_t>(value)};return router.process(m,patch,scheduler);};
    send(0,99,0);send(0,98,45); // CH1 selects cutoff
    send(1,99,0);send(1,98,46); // CH2 selects resonance
    send(0,6,0);send(1,6,0);
    const auto a=send(0,38,70);const auto b=send(1,38,80);
    P6_CHECK(a.nrpnUpdate && a.nrpnUpdate->parameter==45);
    P6_CHECK(b.nrpnUpdate && b.nrpnUpdate->parameter==46);
    router.reset();P6_CHECK(!send(0,6,70).nrpnUpdate);
}

void testMidiInputChannelFilter()
{
    p6::P6PatchState patch;p6::P6Engine engine;engine.prepare(48000.,1);
    p6::P6PerformanceScheduler scheduler(engine);scheduler.prepare(48000.);p6::P6MidiRouter router;
    const std::array<std::uint8_t,3> ch1Note{0x90,60,100},ch2Note{0x91,64,100};
    const std::array<std::uint8_t,3> ch1Bend{0xE0,127,127},ch2Bend{0xE1,127,127};
    const std::array<std::uint8_t,1> clock{0xF8};
    router.setInputChannel(2,scheduler);P6_CHECK(router.inputChannel()==2);
    P6_CHECK(!router.process(ch1Note,patch,scheduler).consumed);P6_CHECK(engine.activeVoiceCount()==0);
    P6_CHECK(router.process(ch2Note,patch,scheduler).consumed);P6_CHECK(engine.activeVoiceCount()>0);
    P6_CHECK(!router.process(ch1Bend,patch,scheduler).consumed);P6_CHECK(router.pitchBendRaw14()==8192);
    P6_CHECK(router.process(ch2Bend,patch,scheduler).consumed);P6_CHECK(router.pitchBendRaw14()==16383);
    const auto events=router.recentMidiEvents();
    P6_CHECK(events[0].valid&&events[0].pitchBend&&events[0].channel==2&&events[0].value==16383&&!events[0].filtered);
    P6_CHECK(events[1].valid&&events[1].pitchBend&&events[1].channel==1&&events[1].value==16383&&events[1].filtered);
    P6_CHECK(router.process(clock,patch,scheduler).consumed);
    router.setInputChannel(1,scheduler);P6_CHECK(engine.activeVoiceCount()==0);P6_CHECK(router.pitchBendRaw14()==8192);
    P6_CHECK(!router.process(ch2Note,patch,scheduler).consumed);P6_CHECK(router.process(ch1Note,patch,scheduler).consumed);
    router.setInputChannel(0,scheduler);P6_CHECK(router.process(ch2Note,patch,scheduler).consumed);
}

void testOmniNoteOwnership()
{
    p6::P6PatchState patch;p6::P6Engine engine;engine.prepare(48000.,1);
    p6::P6PerformanceScheduler scheduler(engine);scheduler.prepare(48000.);p6::P6MidiRouter router;
    const std::array<std::uint8_t,3> on1{0x90,60,100},on2{0x91,60,80},off1{0x80,60,0},off2{0x81,60,0},off3{0x82,60,0};
    router.process(on1,patch,scheduler);router.process(on2,patch,scheduler);
    P6_CHECK(engine.activeVoiceCount()==1);P6_CHECK(engine.captureChordMemoryFromHeld());
    router.process(off3,patch,scheduler);P6_CHECK(engine.captureChordMemoryFromHeld());
    router.reset(); // Program reload resets NRPN but must retain held ownership.
    router.process(off1,patch,scheduler);P6_CHECK(engine.captureChordMemoryFromHeld());
    router.process(off2,patch,scheduler);P6_CHECK(!engine.captureChordMemoryFromHeld());
    router.process(on1,patch,scheduler);
    const std::array<std::uint8_t,3> panic{0xB0,120,0};router.process(panic,patch,scheduler);
    router.process(on2,patch,scheduler);P6_CHECK(engine.activeVoiceCount()==1);
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

void testAllNotesOffRespectsDamper()
{
    p6::P6PatchState patch;
    const auto payloadBefore = patch.rawPayload;
    p6::P6Engine engine;
    engine.prepare(48000.0, 1);
    p6::P6PerformanceScheduler scheduler(engine);
    scheduler.prepare(48000.0);
    p6::P6MidiRouter router;

    const std::array<std::uint8_t, 3> noteOn {0x90, 60, 100};
    const std::array<std::uint8_t, 3> sustainOn {0xB0, 64, 127};
    const std::array<std::uint8_t, 3> allNotesOff {0xB0, 123, 0};
    const std::array<std::uint8_t, 3> sustainOff {0xB0, 64, 0};

    router.process(noteOn, patch, scheduler);
    router.process(sustainOn, patch, scheduler);
    P6_CHECK(router.process(allNotesOff, patch, scheduler).consumed);
    P6_CHECK(engine.sustainPedal());
    P6_CHECK(engine.sustainedVoiceCount() == 1);
    P6_CHECK(engine.activeVoiceCount() == 1);
    P6_CHECK(!engine.captureChordMemoryFromHeld());

    router.process(sustainOff, patch, scheduler);
    P6_CHECK(engine.sustainedVoiceCount() == 0);
    // Ordinary envelope release is allowed to remain active after pedal-up.
    P6_CHECK(patch.rawPayload == payloadBefore);
}

void testAllSoundOffIsImmediateButDoesNotResetControllers()
{
    p6::P6PatchState patch;
    const auto payloadBefore = patch.rawPayload;
    p6::P6Engine engine;
    engine.prepare(48000.0, 1);
    p6::P6PerformanceScheduler scheduler(engine);
    scheduler.prepare(48000.0);
    p6::P6MidiRouter router;

    const std::array<std::uint8_t, 3> noteOn {0x90, 64, 100};
    const std::array<std::uint8_t, 3> sustainOn {0xB0, 64, 127};
    const std::array<std::uint8_t, 3> allSoundOff {0xB0, 120, 0};

    router.process(noteOn, patch, scheduler);
    router.process(sustainOn, patch, scheduler);
    P6_CHECK(engine.activeVoiceCount() == 1);
    P6_CHECK(router.process(allSoundOff, patch, scheduler).consumed);
    P6_CHECK(engine.activeVoiceCount() == 0);
    P6_CHECK(engine.sustainedVoiceCount() == 0);
    // CC120 is not Reset All Controllers; Damper remains logically down until
    // CC64/CC121 changes it.
    P6_CHECK(engine.sustainPedal());
    P6_CHECK(patch.rawPayload == payloadBefore);
}

p6::P6PatchState makeLfoSyncPatch(bool sync)
{
    p6::P6PatchState patch;
    patch.rawPayload.fill(0);
    // Use a midrange free-running rate so phase advancement is easily visible.
    patch.rawParameters.push_back(p6::RawParameter{88, 100, 0.0f, true});
    patch.rawParameters.push_back(p6::RawParameter{90, 0, 0.0f, true});
    patch.rawParameters.push_back(p6::RawParameter{91, static_cast<std::uint16_t>(sync ? 1 : 0), 0.0f, true});
    return patch;
}

void advanceLfo(p6::P6PerformanceScheduler& scheduler, int samples = 256)
{
    for (int i = 0; i < samples; ++i)
        static_cast<void>(scheduler.process());
}

void testSyncedLfoResetsOnlyOnFirstPhysicalKey()
{
    p6::P6Engine engine;
    engine.prepare(48000.0, 1);
    p6::P6PerformanceScheduler scheduler(engine);
    scheduler.prepare(48000.0);
    auto patch = makeLfoSyncPatch(true);
    scheduler.loadPatch(patch);

    advanceLfo(scheduler);
    P6_CHECK(engine.lfoPhaseForDiagnostics() > 0.0);

    scheduler.noteOn(60, 1.0);
    P6_CHECK(engine.lfoPhaseForDiagnostics() == 0.0);

    advanceLfo(scheduler, 64);
    const double phaseWithFirstHeld = engine.lfoPhaseForDiagnostics();
    P6_CHECK(phaseWithFirstHeld > 0.0);

    // An overlapping additional key belongs to the same physical chord and
    // must not restart the synced LFO.
    scheduler.noteOn(64, 1.0);
    P6_CHECK(engine.lfoPhaseForDiagnostics() == phaseWithFirstHeld);

    scheduler.noteOff(60);
    scheduler.noteOff(64);
    advanceLfo(scheduler, 64);
    P6_CHECK(engine.lfoPhaseForDiagnostics() > 0.0);

    // After all physical keys are released, the next first key starts a new
    // synced-LFO cycle.
    scheduler.noteOn(67, 1.0);
    P6_CHECK(engine.lfoPhaseForDiagnostics() == 0.0);
}

void testUnsyncedLfoDoesNotResetOnFirstPhysicalKey()
{
    p6::P6Engine engine;
    engine.prepare(48000.0, 1);
    p6::P6PerformanceScheduler scheduler(engine);
    scheduler.prepare(48000.0);
    auto patch = makeLfoSyncPatch(false);
    scheduler.loadPatch(patch);

    advanceLfo(scheduler);
    const double phaseBefore = engine.lfoPhaseForDiagnostics();
    P6_CHECK(phaseBefore > 0.0);
    scheduler.noteOn(60, 1.0);
    P6_CHECK(engine.lfoPhaseForDiagnostics() == phaseBefore);
}

} // namespace

int main()
{
    testVersionedMailboxLastWriterWins();
    testVersionedMailboxClearIsAGenerationBarrier();
    testVersionedMailboxNeverRegressesAcrossThreads();
    testSustainSeparatesPhysicalKeysFromSoundingVoices();
    testSustainDoesNotExtendGeneratedNoteOwnership();
    testUnisonSustainReleasesAsOneStack();
    testMidiRouterPerformanceControllersStayOutOfPatch();
    testPitchWheelUsesFull14BitTravelAndRecentersOnReset();
    testNrpnSelectionIsPerChannel();
    testMidiInputChannelFilter();
    testOmniNoteOwnership();
    testSustainThresholdIsExactly64();
    testAllNotesOffRespectsDamper();
    testAllSoundOffIsImmediateButDoesNotResetControllers();
    testSyncedLfoResetsOnlyOnFirstPhysicalKey();
    testUnsyncedLfoDoesNotResetOnFirstPhysicalKey();
    std::cout << "p6 v0.17 control-contract tests passed\n";
    return 0;
}
