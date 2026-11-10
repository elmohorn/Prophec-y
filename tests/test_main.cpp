#include "p6/ADSREnvelope.h"
#include "p6/AnalogSlop.h"
#include "p6/CandidateFilters.h"
#include "p6/P6Voice.h"
#include "p6/P6LFO.h"
#include "p6/P6Engine.h"
#include "p6/P6Sequence.h"
#include "p6/P6Clock.h"
#include "p6/P6Arpeggiator.h"
#include "p6/P6SequencerPlayer.h"
#include "p6/P6PerformanceScheduler.h"
#include "p6/P6MidiRouter.h"
#include "p6/NrpnCodec.h"
#include "p6/ParameterRegistry.h"
#include "p6/ParameterMapper.h"
#include "p6/ProgramLayout.h"
#include "p6/PolyBLEPOscillator.h"
#include "p6/SysexCodec.h"
#include "p6/SysexStream.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

#ifdef NDEBUG
#error "p6core_tests requires assertions to be enabled"
#endif

using namespace p6;

static P6PatchState makePatch(const std::string& name)
{
    P6PatchState patch;
    patch.name = name;
    for (std::size_t i = 0; i < patch.rawPayload.size(); ++i)
        patch.rawPayload[i] = static_cast<std::uint8_t>((i * 37u + 11u) & 0xFFu);
    std::fill(patch.rawPayload.begin() + SysexCodec::kPatchNameOffset,
              patch.rawPayload.begin() + SysexCodec::kPatchNameOffset + SysexCodec::kPatchNameLength,
              static_cast<std::uint8_t>(' '));
    std::copy_n(name.begin(), std::min(name.size(), SysexCodec::kPatchNameLength),
                patch.rawPayload.begin() + SysexCodec::kPatchNameOffset);
    return patch;
}

static void testPackingRoundTrip()
{
    std::vector<std::uint8_t> input(1024);
    for (std::size_t i = 0; i < input.size(); ++i)
        input[i] = static_cast<std::uint8_t>((i * 73u + 19u) & 0xFFu);

    const auto packed = SysexCodec::pack7Bit(input);
    assert(packed.size() == SysexCodec::kPackedPayloadSize);
    const auto output = SysexCodec::unpack7Bit(packed);
    assert(output == input);
}

static void testProgramAndEditBufferAreDistinct()
{
    const auto patch = makePatch("P6 CODEC TEST");

    const auto program = SysexCodec::encodeProgramDump(patch, 3, 42);
    assert(program.size() == SysexCodec::kProgramDumpSize);
    assert(program[3] == SysexCodec::kProgramDataCommand);
    auto decoded = SysexCodec::decode(program);
    assert(decoded.ok);
    assert(decoded.kind == SysexKind::ProgramDump);
    assert(decoded.patch.bank == 3);
    assert(decoded.patch.program == 42);
    assert(decoded.patch.name == "P6 CODEC TEST");
    assert(decoded.patch.rawPayload == patch.rawPayload);

    const auto edit = SysexCodec::encodeEditBuffer(patch);
    assert(edit.size() == SysexCodec::kEditBufferSize);
    assert(edit[3] == SysexCodec::kEditBufferDataCommand);
    decoded = SysexCodec::decode(edit);
    assert(decoded.ok);
    assert(decoded.kind == SysexKind::EditBuffer);
    assert(decoded.patch.bank == -1);
    assert(decoded.patch.program == -1);
    assert(decoded.patch.rawPayload == patch.rawPayload);

    // Regression test for the earlier bug: command and length cannot be mixed.
    auto badProgramAsEdit = program;
    badProgramAsEdit[3] = SysexCodec::kEditBufferDataCommand;
    assert(!SysexCodec::decode(badProgramAsEdit).ok);

    auto badEditAsProgram = edit;
    badEditAsProgram[3] = SysexCodec::kProgramDataCommand;
    assert(!SysexCodec::decode(badEditAsProgram).ok);
}

static void testOscillatorFinite()
{
    PolyBLEPOscillator osc;
    osc.prepare(48000.0);
    osc.setFrequency(440.0);
    osc.setPulseWidth(0.37);

    for (double shape : {0.0, 0.25, 0.5, 0.75, 1.0}) {
        osc.setShape(shape);
        for (int i = 0; i < 5000; ++i) {
            const auto y = osc.process();
            assert(std::isfinite(y));
            assert(std::abs(y) < 2.5);
        }
    }
}

static void testPulseMeanNormalizationCalibration()
{
    // The calibration hook must leave a centered square unchanged because its
    // DC mean is zero, while relaxing the legacy normalization must increase
    // the AC level of a narrow pulse.
    PolyBLEPOscillator squareA, squareB;
    for (auto* o : {&squareA, &squareB}) {
        o->prepare(48000.0);
        o->reset(0.173);
        o->setFrequency(440.0);
        o->setShape(1.0);
        o->setPulseWidth(0.5);
    }
    squareA.setMeanNormalizationAmount(1.0);
    squareB.setMeanNormalizationAmount(0.0);
    double squareDiff = 0.0;
    for (int i = 0; i < 4096; ++i)
        squareDiff += std::abs(squareA.process() - squareB.process());
    assert(squareDiff < 1.0e-9);

    PolyBLEPOscillator narrowLegacy, narrowRelaxed;
    for (auto* o : {&narrowLegacy, &narrowRelaxed}) {
        o->prepare(48000.0);
        o->reset(0.173);
        o->setFrequency(440.0);
        o->setShape(1.0);
        o->setPulseWidth(0.04);
    }
    narrowLegacy.setMeanNormalizationAmount(1.0);
    narrowRelaxed.setMeanNormalizationAmount(0.0);
    double legacyEnergy = 0.0, relaxedEnergy = 0.0;
    for (int i = 0; i < 8192; ++i) {
        const double a = narrowLegacy.process();
        const double b = narrowRelaxed.process();
        legacyEnergy += a * a;
        relaxedEnergy += b * b;
    }
    assert(relaxedEnergy > legacyEnergy * 2.5);
}

static void testMorphAndSubSampleSync()
{
    PolyBLEPOscillator master;
    PolyBLEPOscillator slave;
    master.prepare(48000.0);
    slave.prepare(48000.0);
    master.reset(0.93);
    slave.reset(0.31);
    master.setFrequency(6000.0);
    slave.setFrequency(9500.0);
    master.setShape(0.5);
    slave.setShape(0.83);
    slave.setPulseWidth(0.33);

    bool sawSyncEvent = false;
    double energy = 0.0;
    for (int i = 0; i < 4000; ++i) {
        const auto syncAt = master.nextWrapFraction();
        const auto m = master.process();
        const auto y = slave.process(syncAt);
        (void)m;
        if (syncAt)
            sawSyncEvent = true;
        assert(std::isfinite(y));
        assert(std::abs(y) < 3.0);
        energy += y * y;
    }
    assert(sawSyncEvent);
    assert(energy > 100.0);

    // The continuous morph should not collapse in level between labelled
    // shapes, which was a weakness of a straight waveform crossfade.
    for (double shape : {0.0, 0.25, 0.5, 0.75, 1.0}) {
        PolyBLEPOscillator o;
        o.prepare(48000.0);
        o.setFrequency(440.0);
        o.setShape(shape);
        o.setPulseWidth(0.5);
        double e = 0.0;
        for (int i = 0; i < 4800; ++i) {
            const double y = o.process();
            e += y * y;
        }
        const double rms = std::sqrt(e / 4800.0);
        assert(rms > 0.20);
        assert(rms < 1.20);
    }
}

static void testSyncCalibrationHooks()
{
    PolyBLEPOscillator masterA, masterB, slaveLegacy, slaveExplicit, slaveShifted;
    for (auto* o : {&masterA, &masterB, &slaveLegacy, &slaveExplicit, &slaveShifted}) {
        o->prepare(48000.0);
        o->setFrequency(o == &masterA || o == &masterB ? 1800.0 : 7300.0);
        o->setShape(0.61);
        o->setPulseWidth(0.37);
    }
    masterA.reset(0.173);
    masterB.reset(0.173);
    slaveLegacy.reset(0.411);
    slaveExplicit.reset(0.411);
    slaveShifted.reset(0.411);
    slaveExplicit.setSyncResetPhase(0.0);
    slaveShifted.setSyncResetPhase(0.31);

    bool sawWrap = false;
    bool sawQuarterCrossing = false;
    double shiftedDifference = 0.0;
    for (int i = 0; i < 12000; ++i) {
        const auto wrapA = masterA.nextWrapFraction();
        const auto wrapB = masterB.nextPhaseCrossingFraction(0.0);
        assert(bool(wrapA) == bool(wrapB));
        if (wrapA && wrapB) {
            sawWrap = true;
            assert(std::abs(*wrapA - *wrapB) < 1.0e-12);
        }
        if (masterA.nextPhaseCrossingFraction(0.25))
            sawQuarterCrossing = true;

        const double yLegacy = slaveLegacy.process(wrapA);
        const double yExplicit = slaveExplicit.process(wrapB);
        const double yShifted = slaveShifted.process(wrapA);
        assert(std::isfinite(yLegacy));
        assert(std::isfinite(yExplicit));
        assert(std::isfinite(yShifted));
        assert(std::abs(yLegacy - yExplicit) < 1.0e-12);
        shiftedDifference += std::abs(yLegacy - yShifted);
        (void)masterA.process();
        (void)masterB.process();
    }
    assert(sawWrap);
    assert(sawQuarterCrossing);
    assert(shiftedDifference > 1.0);
}

static void testHardSyncTraceAndSlopeCandidate()
{
    PolyBLEPOscillator master, legacy, candidate;
    for (auto* o : {&master, &legacy, &candidate}) {
        o->prepare(96000.0);
        o->setShape(0.0); // triangle: reset can flip first derivative
        o->setPulseWidth(0.5);
    }
    master.setFrequency(1800.0);
    legacy.setFrequency(7310.0);
    candidate.setFrequency(7310.0);
    master.reset(0.173);
    legacy.reset(0.411);
    candidate.reset(0.411);
    legacy.setSyncResetPhase(0.0);
    candidate.setSyncResetPhase(0.0);
    candidate.setHardSyncCorrectionMode(HardSyncCorrectionMode::StepAndSlope);

    bool sawTrace = false;
    bool sawSlopeCorrection = false;
    double difference = 0.0;
    for (int i = 0; i < 20000; ++i) {
        const auto syncAt = master.nextWrapFraction();
        (void)master.process();
        const double a = legacy.process(syncAt);
        const double b = candidate.process(syncAt);
        difference += std::abs(a - b);
        if (syncAt) {
            const auto& la = legacy.lastHardSyncTrace();
            const auto& cb = candidate.lastHardSyncTrace();
            assert(la.occurred && cb.occurred);
            assert(std::abs(la.valueJump - cb.valueJump) < 1.0e-12);
            assert(std::abs(la.slopeJumpPerSample - cb.slopeJumpPerSample) < 1.0e-12);
            assert(std::abs(la.slopeCorrectionBefore) < 1.0e-15);
            assert(std::abs(la.slopeCorrectionAfter) < 1.0e-15);
            if (std::abs(cb.slopeCorrectionBefore) + std::abs(cb.slopeCorrectionAfter) > 1.0e-12)
                sawSlopeCorrection = true;
            sawTrace = true;
        }
        assert(std::isfinite(a) && std::isfinite(b));
    }
    assert(sawTrace);
    assert(sawSlopeCorrection);
    assert(difference > 1.0e-4);
}

static void testHardSyncUnresolvedEdgeRemainsBounded()
{
    // A reset at the pulse edge must not inject the infinite geometric
    // derivative of an edge narrower than the sample interval into BLAMP.
    for (const auto frequency : {110.0, 440.0, 1373.0, 5000.0, 12000.0}) {
        PolyBLEPOscillator master, slave;
        master.prepare(96000.0); slave.prepare(96000.0);
        master.setFrequency(997.3); master.reset(0.314);
        slave.setFrequency(frequency); slave.setShape(1.0);
        slave.setPulseWidth(0.43); slave.reset(0.123);
        slave.setHardSyncCorrectionMode(HardSyncCorrectionMode::StepAndSlope);
        for (int i = 0; i < 50000; ++i) {
            const auto sync = master.nextWrapFraction();
            (void)master.process();
            const double sample = slave.process(sync);
            assert(std::isfinite(sample));
            assert(std::abs(sample) < 3.0);
        }
    }
}

static void testGlideModes()
{
    auto params = [] {
        VoiceParams p;
        p.osc1.level = 1.0;
        p.osc2.level = 0.0;
        p.subLevel = 0.0;
        p.noiseLevel = 0.0;
        p.lowPass.cutoffControl = 164.0;
        p.highPass.cutoffControl = 0.0;
        p.ampAttack = 0.0;
        p.ampDecay = 0.001;
        p.ampSustain = 1.0;
        p.ampRelease = 0.001;
        p.glideOn = true;
        p.glideSeconds = 0.1;
        return p;
    };

    // Fixed-rate: one octave per glideSeconds, so a two-octave jump takes
    // twice as long. Fixed-time: the whole interval takes glideSeconds.
    P6Voice fixedRate, fixedTime;
    fixedRate.prepare(48000.0, 0x1111u);
    fixedTime.prepare(48000.0, 0x2222u);
    auto fr = params(); fr.glideMode = 0;
    auto ft = params(); ft.glideMode = 2;
    fixedRate.setParams(fr);
    fixedTime.setParams(ft);
    fixedRate.noteOn(48, 1.0);
    fixedTime.noteOn(48, 1.0);
    fixedRate.noteOn(72, 1.0);
    fixedTime.noteOn(72, 1.0);
    assert(std::abs(fixedRate.currentPitchNote() - 48.0) < 1.0e-12);
    assert(std::abs(fixedTime.currentPitchNote() - 48.0) < 1.0e-12);
    for (int i = 0; i < 2400; ++i) { // 50 ms at host rate
        fixedRate.process();
        fixedTime.process();
    }
    assert(fixedRate.currentPitchNote() > 53.8 && fixedRate.currentPitchNote() < 54.2);
    assert(fixedTime.currentPitchNote() > 59.8 && fixedTime.currentPitchNote() < 60.2);
    for (int i = 0; i < 2400; ++i)
        fixedTime.process();
    assert(std::abs(fixedTime.currentPitchNote() - 72.0) < 0.02);
    assert(fixedRate.currentPitchNote() < 61.0);

    // The A variants glide only while a previous envelope is alive. Once the
    // voice is fully idle they jump, while the corresponding non-A mode keeps
    // the last pitch and glides from it.
    P6Voice alwaysGlide, legatoOnly;
    alwaysGlide.prepare(48000.0, 0x3333u);
    legatoOnly.prepare(48000.0, 0x4444u);
    auto a0 = params(); a0.glideMode = 0; a0.ampRelease = 0.0005;
    auto a1 = params(); a1.glideMode = 1; a1.ampRelease = 0.0005;
    alwaysGlide.setParams(a0);
    legatoOnly.setParams(a1);
    alwaysGlide.noteOn(60, 1.0);
    legatoOnly.noteOn(60, 1.0);
    alwaysGlide.noteOff();
    legatoOnly.noteOff();
    for (int i = 0; i < 400; ++i) {
        alwaysGlide.process();
        legatoOnly.process();
    }
    assert(!alwaysGlide.active() && !legatoOnly.active());
    alwaysGlide.noteOn(72, 1.0);
    legatoOnly.noteOn(72, 1.0);
    assert(alwaysGlide.currentPitchNote() < 61.0);
    assert(std::abs(legatoOnly.currentPitchNote() - 72.0) < 1.0e-12);
}

static void testFreeRunningOscillators()
{
    VoiceParams p;
    p.osc1.level = 1.0;
    p.osc2.level = 0.0;
    p.subLevel = 0.0;
    p.noiseLevel = 0.0;
    p.lowPass.cutoffControl = 164.0;
    p.highPass.cutoffControl = 0.0;
    p.ampAttack = 0.0;
    p.ampDecay = 0.001;
    p.ampSustain = 1.0;
    p.ampRelease = 0.001;

    P6Voice immediate;
    P6Voice delayed;
    immediate.prepare(48000.0, 0x1234u);
    delayed.prepare(48000.0, 0x1234u);
    immediate.setParams(p);
    delayed.setParams(p);

    for (int i = 0; i < 137; ++i)
        delayed.process(); // VCO must advance even while inactive

    immediate.noteOn(60, 1.0);
    delayed.noteOn(60, 1.0);
    const auto a = immediate.process().preFilter;
    const auto b = delayed.process().preFilter;
    assert(std::abs(a - b) > 1.0e-4);
}

static void testDynamicSlop()
{
    P6Voice voice;
    voice.prepare(48000.0, 0x5eedu);
    VoiceParams p;
    p.osc1.level = 0.5;
    p.osc2.level = 0.5;
    p.slopDepthCents = 25.0;
    p.ampSustain = 1.0;
    p.lowPass.cutoffControl = 164.0;
    voice.setParams(p);
    voice.noteOn(60, 1.0);

    double lo = 1e9, hi = -1e9;
    for (int i = 0; i < 96000; ++i) {
        const auto sample = voice.process();
        lo = std::min(lo, sample.slop1Cents);
        hi = std::max(hi, sample.slop1Cents);
    }
    assert(hi - lo > 0.25);
}

static void testSsmCandidateSelfOscillation()
{
    CandidateLowPass4Pole filter;
    filter.prepare(48000.0);
    filter.start(1.0);
    double energy = 0.0;
    for (int i = 0; i < 12000; ++i) {
        const double y = filter.process(0.0, 440.0, 1.0, 1.0);
        assert(std::isfinite(y));
        energy += y * y;
    }
    assert(energy > 1.0e-4);
}

static void testSawLfoWrapCorrection()
{
    for (const auto shape : {P6LFO::Shape::Saw, P6LFO::Shape::ReverseSaw}) {
        P6LFO lfo;
        lfo.prepare(8000.0); lfo.setRate(1000.0); lfo.setShape(shape);
        double value = 0.0;
        for (int i = 0; i < 8; ++i) value = lfo.process();
        // At the wrap a two-sided BLEP crosses the midpoint. The opposite
        // sign instead gets clamped to an endpoint, leaving a hard step.
        assert(std::abs(value - 0.5) < 1.0e-12);
    }
}

static void testGlobalLfoShapesAndRouting()
{
    P6LFO lfo;
    lfo.prepare(48000.0);
    lfo.setRate(5.0);

    lfo.setShape(P6LFO::Shape::Triangle);
    double triMin = 1e9, triMax = -1e9;
    for (int i = 0; i < 12000; ++i) {
        const double y = lfo.process();
        triMin = std::min(triMin, y);
        triMax = std::max(triMax, y);
    }
    assert(triMin < -0.8 && triMax > 0.8);

    lfo.reset();
    lfo.setShape(P6LFO::Shape::Saw);
    for (int i = 0; i < 12000; ++i) {
        const double y = lfo.process();
        assert(y >= -1.0e-9 && y <= 1.0 + 1.0e-9);
    }

    // Same-seed voices should diverge when global LFO pitch routing is enabled.
    VoiceParams p;
    p.osc1.level = 1.0;
    p.osc2.level = 0.0;
    p.lowPass.cutoffControl = 164.0;
    p.ampAttack = 0.0;
    p.ampDecay = 0.001;
    p.ampSustain = 1.0;
    p.lfo.osc1Freq = true;

    P6Voice dry, mod;
    dry.prepare(48000.0, 123u);
    mod.prepare(48000.0, 123u);
    dry.setParams(p);
    mod.setParams(p);
    dry.noteOn(60, 1.0);
    mod.noteOn(60, 1.0);

    double difference = 0.0;
    for (int i = 0; i < 4000; ++i) {
        const double a = dry.process(0.0).preFilter;
        const double b = mod.process(0.5).preFilter;
        difference += std::abs(a - b);
    }
    assert(difference > 10.0);

    assert(rawLfoHz(0) < rawLfoHz(127));
    assert(rawLfoHz(127) < rawLfoHz(254));
}

static void testAftertouchRouting()
{
    VoiceParams p;
    p.osc1.level = 1.0;
    p.osc2.level = 0.0;
    p.lowPass.cutoffControl = 164.0;
    p.ampAttack = 0.0;
    p.ampDecay = 0.001;
    p.ampSustain = 1.0;
    p.aftertouch.amount = 1.0;
    p.aftertouch.osc1Freq = true;

    P6Voice a, b;
    a.prepare(48000.0, 0x99u);
    b.prepare(48000.0, 0x99u);
    a.setParams(p);
    b.setParams(p);
    a.noteOn(60, 1.0);
    b.noteOn(60, 1.0);

    double diff = 0.0;
    for (int i = 0; i < 4000; ++i)
        diff += std::abs(a.process(0.0, 0.0).preFilter - b.process(0.0, 1.0).preFilter);
    assert(diff > 10.0);
}

static void testEnvelopeLifecycle()
{
    ADSREnvelope env;
    env.prepare(48000.0);
    env.setTimes(0.005, 0.02, 0.03);
    env.setSustain(0.4);
    env.noteOn();
    double peak = 0.0;
    for (int i = 0; i < 10000; ++i)
        peak = std::max(peak, env.process());
    assert(peak > 0.99);
    assert(std::abs(env.value() - 0.4) < 1.0e-3);
    env.noteOff();
    for (int i = 0; i < 10000; ++i)
        env.process();
    assert(env.isIdle());
}

static void testParameterRegistry()
{
    const auto* osc1 = findProgramParameter(0);
    const auto* lp = findProgramParameter(45);
    const auto* fx = findProgramParameter(135);
    assert(osc1 && osc1->maximum == 60);
    assert(lp && lp->maximum == 164);
    assert(fx && fx->maximum == 1);
    assert(findProgramParameter(999) == nullptr);
}

static void testVoiceAudioRatePath()
{
    P6Voice voice;
    voice.prepare(48000.0, 0x12345678u);
    VoiceParams p;
    p.osc1.shape = 0.55;
    p.osc1.level = 0.5;
    p.osc2.shape = 0.25;
    p.osc2.level = 0.35;
    p.osc2.semitones = 7.0;
    p.subLevel = 0.15;
    p.noiseLevel = 0.01;
    p.sync = true;
    p.polyMod.filterEnvAmount = 0.5;
    p.polyMod.osc2Amount = 0.6;
    p.polyMod.osc1FreqAmount = 0.3;
    p.polyMod.osc1ShapeAmount = 0.1;
    p.polyMod.osc1PulseWidthAmount = 0.08;
    p.ampAttack = 0.002;
    p.ampDecay = 0.02;
    p.ampSustain = 0.7;
    p.ampRelease = 0.02;
    voice.setParams(p);
    voice.noteOn(60, 0.8);

    double energy = 0.0;
    double pmMovement = 0.0;
    double lastPm = 0.0;
    for (int i = 0; i < 12000; ++i) {
        const auto s = voice.process();
        assert(std::isfinite(s.dry));
        energy += s.dry * s.dry;
        pmMovement += std::abs(s.polyModSource - lastPm);
        lastPm = s.polyModSource;
    }
    assert(energy > 1.0);
    assert(pmMovement > 1.0);

    voice.noteOff();
    for (int i = 0; i < 12000; ++i)
        voice.process();
    assert(!voice.active());
}

static void testSerializedLayoutMapping()
{
    auto patch = makePatch("LAYOUT TEST");
    patch.rawPayload.fill(0);
    patch.rawPayload[0] = 12;   // Osc1Freq
    patch.rawPayload[7] = 101;  // Osc1Level
    patch.rawPayload[19] = 77;  // LP cutoff raw
    patch.rawPayload[77] = 200; // PolyMod filter env raw signed-centered
    std::fill(patch.rawPayload.begin() + SysexCodec::kPatchNameOffset,
              patch.rawPayload.begin() + SysexCodec::kPatchNameOffset + SysexCodec::kPatchNameLength,
              static_cast<std::uint8_t>(' '));
    const std::string name = "LAYOUT TEST";
    std::copy(name.begin(), name.end(), patch.rawPayload.begin() + SysexCodec::kPatchNameOffset);

    const auto syx = SysexCodec::encodeEditBuffer(patch);
    const auto decoded = SysexCodec::decode(syx);
    assert(decoded.ok);
    assert(decoded.patch.rawParameters.size() == serializedParameterLayout().size());

    auto findRaw = [&](std::uint16_t nrpn) -> std::uint16_t {
        for (const auto& rp : decoded.patch.rawParameters)
            if (rp.id == nrpn) return rp.raw;
        return 0xffff;
    };
    assert(findRaw(0) == 12);
    assert(findRaw(2) == 101);
    assert(findRaw(45) == 77);
    assert(findRaw(143) == 200);

    const auto* p = findSerializedByNrpn(143);
    assert(p && p->sysexOffset == 77);
    assert(p->type == SerializedValueType::SignedCentered);

    // Factory-bank correlations show two serialized aftertouch destination bytes
    // are swapped relative to the printed NRPN ordering.  Keep the NRPN identity
    // correct while mapping it to the byte the hardware actually stores.
    const auto* atHp = findSerializedByNrpn(113);
    const auto* atAmp = findSerializedByNrpn(114);
    assert(atHp && atHp->sysexOffset == 75);
    assert(atAmp && atAmp->sysexOffset == 74);

    // Effect type domains in the 2015 factory payload are A=0..5, B=0..9.
    const auto* fxA = findSerializedByNrpn(119);
    const auto* fxB = findSerializedByNrpn(127);
    assert(fxA && fxA->maximum == 5);
    assert(fxB && fxB->maximum == 9);
}

static void testNrpnDecoder()
{
    NrpnDecoder d;
    assert(!d.processCC(99, 0));
    assert(!d.processCC(98, 45));
    assert(!d.processCC(6, 1));
    const auto u = d.processCC(38, 10); // 138, within LP cutoff 0..164
    assert(u);
    assert(u->parameter == 45);
    assert(u->value == 138);

    // Preserve the currently supported fine-only update until hardware
    // evidence justifies a different NRPN transaction policy.
    const auto fine = d.processCC(38, 11);
    assert(fine && fine->parameter == 45 && fine->value == 139);

    P6PatchState patch;
    applyRawNrpn(patch, *u);
    assert(patch.rawParameters.size() == 1);
    assert(patch.rawParameters[0].raw == 138);
    assert(patch.rawParameters[0].normalized > 0.8f);

    // Clamp through official registry.
    applyRawNrpn(patch, NrpnUpdate{45, 500});
    assert(patch.rawParameters[0].raw == 164);
    assert(patch.rawParameters[0].normalized == 1.0f);

    // Sequence notes have a nonzero lower bound; out-of-range MIDI is clamped.
    applyRawNrpn(patch, NrpnUpdate{256, 0});
    const auto sequence = std::find_if(patch.rawParameters.begin(), patch.rawParameters.end(),
                                       [](const auto& parameter) { return parameter.id == 256; });
    assert(sequence != patch.rawParameters.end() && sequence->raw == 12 && sequence->normalized == 0.0f);
}

static void testSixVoiceAllocator()
{
    P6Engine engine;
    engine.prepare(48000.0);
    VoiceParams p;
    p.osc1.level = 0.4;
    p.osc2.level = 0.2;
    p.ampAttack = 0.001;
    p.ampRelease = 0.01;
    engine.setVoiceParams(p);
    engine.setPanSpread(1.0);

    for (int n = 60; n < 66; ++n)
        engine.noteOn(n, 0.8);
    assert(engine.activeVoiceCount() == 6);

    double energyL = 0.0, energyR = 0.0;
    for (int i = 0; i < 2000; ++i) {
        const auto s = engine.process();
        energyL += s.left * s.left;
        energyR += s.right * s.right;
    }
    assert(energyL > 0.1 && energyR > 0.1);

    // Seventh note must steal rather than exceed hardware polyphony.
    engine.noteOn(72, 0.8);
    assert(engine.activeVoiceCount() == 6);
    engine.allNotesOff();
    for (int i = 0; i < 6000; ++i)
        engine.process();
    assert(engine.activeVoiceCount() == 0);

    engine.setUnison(true, 10.0);
    engine.noteOn(60, 1.0);
    assert(engine.activeVoiceCount() == 6);
    engine.noteOff(60);
}

static void testParameterMapper()
{
    P6PatchState patch;
    patch.rawPayload.fill(0);
    patch.rawPayload[0] = 24;    // Osc1 unison reference (factory-bank evidence)
    patch.rawPayload[1] = 31;    // Osc2 +7 semitones
    patch.rawPayload[2] = 254;   // Osc2 fine +50 cents
    patch.rawPayload[3] = 127;   // Osc1 saw midpoint
    patch.rawPayload[4] = 254;   // Osc2 pulse end
    patch.rawPayload[5] = 127;   // PW near square
    patch.rawPayload[6] = 127;
    patch.rawPayload[7] = 127;
    patch.rawPayload[8] = 64;
    patch.rawPayload[9] = 32;
    patch.rawPayload[10] = 0;
    patch.rawPayload[11] = 1;    // sync
    patch.rawPayload[12] = 1;    // Osc2 keyboard
    patch.rawPayload[17] = 24;   // legacy NRPN metadata allows 24; hardware/manual cap is 12
    patch.rawPayload[18] = 64;   // vintage/slop
    patch.rawPayload[19] = 100;  // LP cutoff
    patch.rawPayload[20] = 128;  // LP resonance
    patch.rawPayload[21] = 2;    // full key tracking
    patch.rawPayload[23] = 12;   // HP cutoff
    patch.rawPayload[29] = 127;  // neutral LP env amount
    patch.rawPayload[30] = 127;  // neutral HP env amount
    patch.rawPayload[31] = 127;  // VCA env amount
    patch.rawPayload[36] = 0;
    patch.rawPayload[38] = 20;
    patch.rawPayload[40] = 100;
    patch.rawPayload[42] = 20;
    patch.rawPayload[43] = 1;    // amp velocity
    patch.rawPayload[77] = 254;  // Poly Mod filter env full positive
    patch.rawPayload[78] = 127;  // Poly Mod Osc2 neutral
    patch.rawPayload[79] = 1;    // Osc1 freq destination
    patch.rawPayload[82] = 1;    // LP destination
    patch.rawPayload[59] = 127;  // LFO frequency
    patch.rawPayload[63] = 200;  // LFO initial amount
    patch.rawPayload[62] = 3;    // LFO square
    patch.rawPayload[64] = 1;    // LFO -> Osc1
    patch.rawPayload[67] = 1;    // LFO -> LPF
    patch.rawPayload[70] = 200;  // aftertouch amount positive
    patch.rawPayload[71] = 1;    // aftertouch -> Osc1
    patch.rawPayload[74] = 1;    // aftertouch -> amp (serialized byte order differs from NRPN order)
    patch.rawPayload[84] = 1;    // unison on
    patch.rawPayload[85] = 2;    // 3 unison voices
    patch.rawPayload[86] = 5;    // last-note retrigger
    populateRawParametersFromPayload(patch);

    const auto m = mapPatchToDSP(patch);
    assert(std::abs(m.voice.osc1.semitones) < 1.0e-9);
    assert(std::abs(m.voice.osc2.semitones - 7.0) < 1.0e-9);
    assert(std::abs(m.voice.osc2.fineCents - 50.0) < 1.0e-6);
    assert(std::abs(m.voice.osc1.shape - 0.5) < 0.01);
    assert(m.voice.sync);
    assert(m.voice.osc2KeyTrack);
    assert(std::abs(m.voice.lowPass.keyTrack - 1.0) < 1.0e-9);
    assert(std::abs(m.voice.lowPass.envAmountOctaves) < 1.0e-9);
    assert(m.voice.polyMod.filterEnvAmount > 0.99);
    assert(m.voice.polyMod.osc1FreqAmount > 0.0);
    assert(m.voice.lowPass.polyModAmountOctaves > 0.0);
    assert(m.lfo.rateHz > 1.0);
    assert(m.lfo.shape == 3);
    assert(m.lfo.initialAmount > 0.7);
    assert(m.voice.lfo.osc1Freq && m.voice.lfo.lowPass);
    assert(m.voice.aftertouch.amount > 0.5);
    assert(m.voice.aftertouch.osc1Freq && m.voice.aftertouch.amp);
    assert(m.unison && m.unisonMode == 2);
    assert(m.keyMode == 5);
    assert(std::abs(m.pitchBendRangeSemitones - 12.0) < 1.0e-12);
    assert(!m.chordMode);
    assert(m.slopAmount > 0.49 && m.slopAmount < 0.52);
    assert(m.slopMode == SlopMode::Standard);

    assert(rawEnvelopeTime(0, 22.0) < rawEnvelopeTime(64, 22.0));
    assert(rawEnvelopeTime(64, 22.0) < rawEnvelopeTime(127, 22.0));
    assert(rawFilterCutoffHz(120, 48000.0) > rawFilterCutoffHz(60, 48000.0));

    // Calibration hooks must alter DSP-domain mapping only, never raw state.
    MapperCalibration c;
    c.filterSemitonesPerRaw = 0.5;
    c.lowPassResonanceScale = 0.5;
    const auto mc = mapPatchToDSP(patch, c);
    assert(mc.voice.lowPass.resonance > 0.24 && mc.voice.lowPass.resonance < 0.26);
    assert(rawFilterCutoffHz(100, 48000.0, c) < rawFilterCutoffHz(100, 48000.0));

    // Optional nonlinear cutoff law must preserve endpoints and monotonicity.
    MapperCalibration curved;
    curved.filterBaseHz = 80.0;
    curved.filterTopHz = 18000.0;
    curved.filterCurveExponent = 1.5;
    const double c0 = rawFilterCutoffHz(0, 48000.0, curved);
    const double c82 = rawFilterCutoffHz(82, 48000.0, curved);
    const double c164 = rawFilterCutoffHz(164, 48000.0, curved);
    assert(std::abs(c0 - 80.0) < 1.0e-6);
    assert(c82 > c0 && c164 > c82);
    assert(std::abs(c164 - 18000.0) < 1.0);
    const auto mcurved = mapPatchToDSP(patch, curved);
    assert(std::abs(mcurved.voice.filterCurveExponent - 1.5) < 1.0e-9);
    assert(std::abs(mcurved.voice.filterTopHz - 18000.0) < 1.0e-9);

    MapperCalibration piecewise;
    piecewise.filterBaseHz = 90.0;
    piecewise.filterSemitonesPerRaw = 0.37;
    piecewise.filterKneeRaw = 100.0;
    piecewise.filterHighSemitonesPerRaw = 0.9;
    const double p90 = rawFilterCutoffHz(90, 48000.0, piecewise);
    const double p100 = rawFilterCutoffHz(100, 48000.0, piecewise);
    const double p140 = rawFilterCutoffHz(140, 48000.0, piecewise);
    assert(p100 > p90 && p140 > p100);
    const auto mpiece = mapPatchToDSP(patch, piecewise);
    assert(std::abs(mpiece.voice.filterKneeRaw - 100.0) < 1.0e-9);
    assert(std::abs(mpiece.voice.filterHighSemitonesPerRaw - 0.9) < 1.0e-9);

    const auto refA = referenceACandidateV07();
    assert(std::abs(refA.filterBaseHz - 88.97694831931565) < 1.0e-9);
    assert(std::abs(refA.filterSemitonesPerRaw - 0.37) < 1.0e-12);
    assert(std::abs(refA.filterEnvelopeCurveExponent - 3.0) < 1.0e-12);
    assert(std::abs(refA.filterAttackMaxSeconds - 2.5) < 1.0e-12);
    assert(std::abs(refA.lowPassResonanceScale - 0.7) < 1.0e-12);

    const auto refA8 = referenceACandidateV08();
    assert(std::abs(refA8.filterBaseHz - refA.filterBaseHz) < 1.0e-12);
    assert(std::abs(refA8.highPassFilterBaseHz - 40.0) < 1.0e-12);
    assert(std::abs(refA8.highPassFilterCurveExponent - 4.0) < 1.0e-12);
    assert(std::abs(refA8.highPassFilterTopHz - 20000.0) < 1.0e-9);
    const auto refA8Mapped = mapPatchToDSP(patch, refA8);
    assert(std::abs(refA8Mapped.voice.highPassFilterBaseHz - 40.0) < 1.0e-12);
    assert(std::abs(refA8Mapped.voice.highPassFilterCurveExponent - 4.0) < 1.0e-12);

    const auto refA9 = referenceACandidateV09();
    assert(std::abs(refA9.polyModOscPitchRangeOctaves - 4.0) < 1.0e-12);
    assert(std::abs(refA9.polyModShapeRange - 1.0) < 1.0e-12);
    assert(std::abs(refA9.polyModPulseWidthRange - 0.5) < 1.0e-12);
    assert(std::abs(refA9.polyModFilterRangeOctaves - 7.0) < 1.0e-12);
    assert(std::abs(refA9.syncResetPhase) < 1.0e-12);
    assert(std::abs(refA9.syncTriggerPhase) < 1.0e-12);
    assert(std::abs(refA9.osc2MixerPolarity - 1.0) < 1.0e-12);
    const auto refA9Mapped = mapPatchToDSP(patch, refA9);
    assert(std::abs(refA9Mapped.voice.polyMod.osc1FreqAmount - 4.0) < 1.0e-12);
    assert(std::abs(refA9Mapped.voice.lowPass.polyModAmountOctaves - 7.0) < 1.0e-12);

    // Glide lives at raw offsets 14..16 / NRPN 30,28,29.
    auto glidePatch = patch;
    glidePatch.rawPayload[14] = 70;
    glidePatch.rawPayload[15] = 3;
    glidePatch.rawPayload[16] = 1;
    populateRawParametersFromPayload(glidePatch);
    const auto mg = mapPatchToDSP(glidePatch);
    assert(mg.voice.glideOn);
    assert(mg.voice.glideMode == 3);
    assert(mg.voice.glideSeconds > 0.1 && mg.voice.glideSeconds < 0.2);

    assert(patch.rawPayload[19] == 100 && patch.rawPayload[20] == 128);
}

static void testPatchToEnginePath()
{
    P6PatchState patch;
    patch.rawPayload.fill(0);
    patch.rawPayload[0] = 24;
    patch.rawPayload[1] = 24;
    patch.rawPayload[3] = 127;
    patch.rawPayload[4] = 127;
    patch.rawPayload[5] = 127;
    patch.rawPayload[6] = 127;
    patch.rawPayload[7] = 100;
    patch.rawPayload[8] = 50;
    patch.rawPayload[19] = 135;  // mostly open LP candidate
    patch.rawPayload[23] = 0;
    patch.rawPayload[27] = 127;  // program volume
    patch.rawPayload[31] = 127;  // amp env amount
    patch.rawPayload[36] = 0;
    patch.rawPayload[38] = 10;
    patch.rawPayload[40] = 110;
    patch.rawPayload[42] = 10;
    patch.rawPayload[84] = 1;
    patch.rawPayload[85] = 2;    // three voices
    populateRawParametersFromPayload(patch);

    P6Engine engine;
    engine.prepare(48000.0);
    engine.loadPatch(patch);
    assert(engine.unisonVoiceCount() == 3);
    engine.noteOn(60, 1.0);
    assert(engine.activeVoiceCount() == 3);

    double energy = 0.0;
    for (int i = 0; i < 6000; ++i) {
        const auto s = engine.process();
        assert(std::isfinite(s.left) && std::isfinite(s.right));
        energy += s.left * s.left + s.right * s.right;
    }
    assert(energy > 0.001);
    engine.noteOff(60);
}

static void testSysexStreamBankDecode()
{
    auto a = makePatch("BANK A");
    auto b = makePatch("BANK B");
    auto c = makePatch("EDIT C");
    const auto sa = SysexCodec::encodeProgramDump(a, 5, 0);
    const auto sb = SysexCodec::encodeProgramDump(b, 5, 1);
    const auto sc = SysexCodec::encodeEditBuffer(c);
    std::vector<std::uint8_t> bank{0x7e, 0x00}; // harmless junk before stream
    bank.insert(bank.end(), sa.begin(), sa.end());
    bank.insert(bank.end(), sb.begin(), sb.end());
    bank.insert(bank.end(), sc.begin(), sc.end());
    bank.push_back(0x00);

    const auto decoded = decodeP6SysexStream(bank);
    assert(decoded.errors.empty());
    assert(decoded.entries.size() == 3);
    assert(decoded.skippedBytes == 3);
    assert(decoded.entries[0].patch.bank == 5 && decoded.entries[0].patch.program == 0);
    assert(decoded.entries[1].patch.name == "BANK B");
    assert(decoded.entries[2].kind == SysexKind::EditBuffer);

    // A packed SysEx payload is seven-bit clean. Do not silently strip the
    // high bit from corrupt data; still recover the following valid message.
    for (std::uint8_t bad : {std::uint8_t{0x80}, std::uint8_t{0xf0}, std::uint8_t{0xf7}}) {
        auto corrupt = sa;
        corrupt[20] = bad;
        assert(!SysexCodec::decode(corrupt).ok);
        corrupt.insert(corrupt.end(), sb.begin(), sb.end());
        const auto recovered = decodeP6SysexStream(corrupt);
        assert(!recovered.errors.empty());
        assert(recovered.entries.size() == 1);
        assert(recovered.entries[0].patch.program == 1);
    }
}

static void testSequenceCodecAndNrpnMapping()
{
    P6PatchState patch;
    patch.rawPayload.fill(0);

    // Track 1, step 1. The stored velocity high bit must survive decode/edit.
    patch.rawPayload[P6Sequence::kPayloadOffset + 0] = 69;
    patch.rawPayload[P6Sequence::kPayloadOffset + 64] = static_cast<std::uint8_t>(0x80u | 71u);
    // Track 2, step 3 proves the 128-byte stride.
    patch.rawPayload[P6Sequence::kPayloadOffset + 128 + 2] = 48;
    patch.rawPayload[P6Sequence::kPayloadOffset + 128 + 64 + 2] = 96;

    const auto seq = decodeSequence(patch);
    assert(seq.tracks[0][0].noteRaw == 69);
    assert(seq.tracks[0][0].velocity == 71);
    assert(seq.tracks[0][0].highBitFlag);
    assert(seq.tracks[1][2].noteRaw == 48);
    assert(seq.tracks[1][2].velocity == 96);

    auto edited = patch;
    applyRawNrpn(edited, NrpnUpdate{256, 72});      // track1 note1
    applyRawNrpn(edited, NrpnUpdate{320, 55});      // track1 velocity1
    applyRawNrpn(edited, NrpnUpdate{384 + 2, 64});  // track2 note3
    assert(edited.rawPayload[P6Sequence::kPayloadOffset] == 72);
    // High-bit metadata is preserved while low seven velocity bits change.
    assert(edited.rawPayload[P6Sequence::kPayloadOffset + 64] == (0x80u | 55u));
    assert(edited.rawPayload[P6Sequence::kPayloadOffset + 128 + 2] == 64);

    SequenceNrpnAddress a{};
    assert(sequenceAddressForNrpn(1023, a));
    assert(a.track == 5 && a.step == 63 && a.velocity);
    assert(!sequenceAddressForNrpn(1024, a));

    auto seq2 = decodeSequence(edited);
    P6PatchState roundTrip;
    roundTrip.rawPayload.fill(0x5a);
    encodeSequence(roundTrip, seq2);
    for (std::size_t i = 0; i < P6Sequence::kPayloadSize; ++i)
        assert(roundTrip.rawPayload[P6Sequence::kPayloadOffset + i] ==
               edited.rawPayload[P6Sequence::kPayloadOffset + i]);
    assert(roundTrip.rawPayload[0] == 0x5a); // encoder touches sequence only
}

static void testNrpnEditsCanonicalPayload()
{
    P6PatchState patch;
    patch.rawPayload.fill(0);
    populateRawParametersFromPayload(patch);

    applyRawNrpn(patch, NrpnUpdate{45, 123});
    assert(patch.rawPayload[19] == 123);

    for (std::size_t i = 0; i < SysexCodec::kPatchNameLength; ++i)
        patch.rawPayload[SysexCodec::kPatchNameOffset + i] = ' ';
    applyRawNrpn(patch, NrpnUpdate{236, static_cast<std::uint16_t>('P')});
    applyRawNrpn(patch, NrpnUpdate{237, static_cast<std::uint16_t>('6')});
    assert(patch.name == "P6");

    const auto encoded = SysexCodec::encodeEditBuffer(patch);
    const auto decoded = SysexCodec::decode(encoded);
    assert(decoded.ok);
    assert(decoded.patch.rawPayload[19] == 123);
    assert(decoded.patch.name == "P6");
}


static void testDiagnosticOversamplingAndBypasses()
{
    VoiceParams p;
    p.osc1.shape = 0.58;
    p.osc1.pulseWidth = 0.41;
    p.osc1.level = 0.7;
    p.osc2.shape = 0.42;
    p.osc2.pulseWidth = 0.55;
    p.osc2.level = 0.6;
    p.osc2.semitones = -12.0;
    p.subLevel = 0.25;
    p.noiseLevel = 0.03;
    p.sync = false;
    p.polyMod.filterEnvAmount = 0.7;
    p.polyMod.osc2Amount = 0.8;
    p.polyMod.osc1FreqAmount = 1.5;
    p.polyMod.osc1ShapeAmount = 0.5;
    p.polyMod.osc1PulseWidthAmount = 0.2;
    p.lowPass.cutoffControl = 30.0;
    p.highPass.cutoffControl = 80.0;
    p.ampAttack = 0.0;
    p.ampDecay = 0.001;
    p.ampSustain = 1.0;
    p.ampRelease = 0.001;
    p.filterAttack = 0.0;
    p.filterDecay = 0.01;
    p.filterSustain = 1.0;
    p.filterRelease = 0.01;

    // All supported oversampling factors must remain finite and produce
    // non-trivial audio. This is a diagnostic capability, not a new default.
    for (int factor : {1, 2, 4, 8}) {
        P6Voice v;
        v.prepare(48000.0, 0x55aa11u, factor);
        v.setParams(p);
        VoiceDiagnostics d;
        d.mix = VoiceDiagnosticMix::Osc1Only;
        d.bypassFilters = true;
        d.bypassPolyMod = true;
        v.setDiagnostics(d);
        v.noteOn(60, 1.0);
        double energy = 0.0;
        for (int i = 0; i < 4096; ++i) {
            const auto x = v.process();
            assert(std::isfinite(x.dry));
            energy += x.dry * x.dry;
        }
        assert(energy > 10.0);
    }

    // Poly-Mod bypass must make the audible result independent of destination
    // depths while retaining the raw source in VoiceSample for inspection.
    P6Voice a, b;
    a.prepare(48000.0, 0x123abc, 2);
    b.prepare(48000.0, 0x123abc, 2);
    auto p2 = p;
    p2.polyMod.osc1FreqAmount = 4.0;
    p2.polyMod.osc1ShapeAmount = 1.0;
    p2.polyMod.osc1PulseWidthAmount = 0.5;
    a.setParams(p);
    b.setParams(p2);
    VoiceDiagnostics bypass;
    bypass.mix = VoiceDiagnosticMix::Osc1Only;
    bypass.bypassFilters = true;
    bypass.bypassPolyMod = true;
    a.setDiagnostics(bypass);
    b.setDiagnostics(bypass);
    a.noteOn(60, 1.0);
    b.noteOn(60, 1.0);
    double diff = 0.0;
    double sourceMovement = 0.0;
    for (int i = 0; i < 4096; ++i) {
        const auto xa = a.process();
        const auto xb = b.process();
        diff += std::abs(xa.dry - xb.dry);
        sourceMovement += std::abs(xa.polyModSource);
    }
    assert(diff < 1.0e-9);
    assert(sourceMovement > 1.0);

    // Force-sync is deliberately separate from the patch bit so a dry sync
    // corpus can be generated without mutating canonical hardware state.
    P6Voice freeRun, forced;
    freeRun.prepare(48000.0, 0x778899u, 4);
    forced.prepare(48000.0, 0x778899u, 4);
    auto ps = p;
    ps.osc1.semitones = 19.0;
    ps.osc2.semitones = 0.0;
    ps.polyMod = {};
    ps.sync = false;
    freeRun.setParams(ps);
    forced.setParams(ps);
    VoiceDiagnostics dfree;
    dfree.mix = VoiceDiagnosticMix::Osc1Only;
    dfree.bypassFilters = true;
    dfree.bypassPolyMod = true;
    VoiceDiagnostics dsync = dfree;
    dsync.forceSync = true;
    freeRun.setDiagnostics(dfree);
    forced.setDiagnostics(dsync);
    freeRun.noteOn(48, 1.0);
    forced.noteOn(48, 1.0);
    double syncDifference = 0.0;
    for (int i = 0; i < 6000; ++i)
        syncDifference += std::abs(freeRun.process().dry - forced.process().dry);
    assert(syncDifference > 10.0);
}


static VoiceParams performanceTestVoiceParams()
{
    VoiceParams p;
    p.osc1.level = 1.0;
    p.osc2.level = 0.0;
    p.subLevel = 0.0;
    p.noiseLevel = 0.0;
    p.ampAttack = 0.001;
    p.ampDecay = 0.005;
    p.ampSustain = 1.0;
    p.ampRelease = 0.01;
    p.filterAttack = 0.001;
    p.filterDecay = 0.005;
    p.filterSustain = 1.0;
    p.filterRelease = 0.01;
    p.lowPass.cutoffControl = 164.0;
    p.highPass.cutoffControl = 0.0;
    return p;
}

static void testPitchBendAndModWheelPerformance()
{
    P6PatchState patch;
    patch.rawPayload.fill(0);
    patch.rawPayload[0] = 24;
    patch.rawPayload[1] = 24;
    patch.rawPayload[7] = 127;
    patch.rawPayload[17] = 24; // preserve raw, clamp DSP behavior to documented 12
    patch.rawPayload[19] = 164;
    patch.rawPayload[27] = 127;
    patch.rawPayload[31] = 127;
    patch.rawPayload[40] = 127;
    patch.rawPayload[63] = 64; // LFO initial amount ~= 0.251
    populateRawParametersFromPayload(patch);

    P6Engine engine;
    engine.prepare(48000.0);
    engine.loadPatch(patch);
    assert(std::abs(engine.pitchBendRangeSemitones() - 12.0) < 1.0e-12);
    engine.setPitchBend(0.5);
    assert(std::abs(engine.pitchBendSemitones() - 6.0) < 1.0e-12);
    engine.setPitchBend(-2.0); // clamp host input
    assert(std::abs(engine.pitchBendSemitones() + 12.0) < 1.0e-12);

    const double initial = 64.0 / 255.0;
    engine.setModWheel(0.0);
    assert(std::abs(engine.effectiveLfoDepth() - initial) < 1.0e-12);
    engine.setModWheel(0.5);
    assert(std::abs(engine.effectiveLfoDepth() - (initial + 0.5 * (1.0 - initial))) < 1.0e-12);
    engine.setModWheel(1.0);
    assert(std::abs(engine.effectiveLfoDepth() - 1.0) < 1.0e-12);

    P6Voice voice;
    voice.prepare(48000.0, 0x1234u);
    voice.setParams(performanceTestVoiceParams());
    voice.noteOn(60, 1.0);
    voice.setPitchBendSemitones(7.0);
    assert(std::abs(voice.effectiveBasePitchNote() - 67.0) < 1.0e-12);
}

static void testUnisonKeyModesAndRetrigger()
{
    const int expected[3] = {60, 72, 72};
    for (std::uint8_t mode = 0; mode < 3; ++mode) {
        P6Engine engine;
        engine.prepare(48000.0);
        engine.setVoiceParams(performanceTestVoiceParams());
        engine.setUnison(true, 0.0, 3);
        engine.setKeyMode(mode);
        engine.noteOn(60, 0.7);
        engine.noteOn(72, 0.9);
        const auto notes = engine.soundingNotes();
        for (int i = 0; i < 3; ++i)
            assert(notes[static_cast<std::size_t>(i)] == expected[mode]);
        if (mode == 2) {
            engine.noteOff(72);
            const auto fallback = engine.soundingNotes();
            for (int i = 0; i < 3; ++i)
                assert(fallback[static_cast<std::size_t>(i)] == 60);
        }
    }

    // Low-retrigger leaves the winning pitch at the low key when a higher key
    // is pressed, but restarts the envelopes from zero. Plain low priority does not.
    auto build = [](std::uint8_t mode) {
        P6Engine e;
        e.prepare(48000.0);
        auto p = performanceTestVoiceParams();
        p.ampAttack = 0.05;
        p.filterAttack = 0.05;
        e.setVoiceParams(p);
        VoiceDiagnostics d;
        d.mix = VoiceDiagnosticMix::Osc1Only;
        d.bypassFilters = true;
        e.setVoiceDiagnostics(d);
        e.setUnison(true, 0.0, 1);
        e.setKeyMode(mode);
        return e;
    };
    auto plain = build(0);
    auto retrig = build(3);
    plain.noteOn(60, 1.0);
    retrig.noteOn(60, 1.0);
    for (int i = 0; i < 12000; ++i) {
        plain.process();
        retrig.process();
    }
    plain.noteOn(72, 1.0);
    retrig.noteOn(72, 1.0);
    assert(plain.soundingNotes()[0] == 60 && retrig.soundingNotes()[0] == 60);
    double plainEnergy = 0.0, retrigEnergy = 0.0;
    for (int i = 0; i < 256; ++i) {
        const auto a = plain.process();
        const auto b = retrig.process();
        plainEnergy += a.left * a.left + a.right * a.right;
        retrigEnergy += b.left * b.left + b.right * b.right;
    }
    assert(retrigEnergy < plainEnergy * 0.15);
}

static void testUnisonNewPhraseReattacksReleaseTail()
{
    for (std::uint8_t mode = 0; mode < 6; ++mode) {
        for (const int gapSamples : {0, 960}) {
            P6Engine e;
            e.prepare(48000.0);
            auto p = performanceTestVoiceParams();
            p.ampAttack = 0.001;
            p.ampSustain = 1.0;
            p.ampRelease = 0.08;
            e.setVoiceParams(p);
            VoiceDiagnostics d;
            d.mix = VoiceDiagnosticMix::Osc1Only;
            d.bypassFilters = true;
            e.setVoiceDiagnostics(d);
            e.setUnison(true, 0.0, 1);
            e.setKeyMode(mode);
            e.noteOn(60, 1.0);
            for (int i = 0; i < 9600; ++i) e.process();
            e.noteOff(60);
            for (int i = 0; i < gapSamples; ++i) e.process();
            e.noteOn(62, 1.0);
            // A first key after all physical keys were released starts a new
            // phrase, even while the old voice's release remains audible.
            for (int i = 0; i < 9600; ++i) e.process();
            double energy = 0.0;
            for (int i = 0; i < 1024; ++i) {
                const auto x = e.process();
                energy += x.left*x.left + x.right*x.right;
            }
            assert(e.activeVoiceCount() == 1);
            assert(e.soundingNotes()[0] == 62);
            assert(energy > 0.01);
        }
    }
}

static void testChordMemoryRuntime()
{
    P6Engine engine;
    engine.prepare(48000.0);
    engine.setVoiceParams(performanceTestVoiceParams());

    // Capture C major as intervals from the lowest held note.
    engine.noteOn(60, 0.8);
    engine.noteOn(64, 0.8);
    engine.noteOn(67, 0.8);
    assert(engine.captureChordMemoryFromHeld());
    assert(engine.chordMode());
    assert(engine.chordMemorySize() == 3);
    assert(engine.chordInterval(0) == 0);
    assert(engine.chordInterval(1) == 4);
    assert(engine.chordInterval(2) == 7);
    engine.allNotesOff();

    engine.setKeyMode(0); // low-note anchor: played key is chord's lowest note
    engine.noteOn(72, 1.0);
    auto notes = engine.soundingNotes();
    const int lowExpected[6] = {72, 76, 79, 72, 76, 79};
    for (std::size_t i = 0; i < notes.size(); ++i)
        assert(notes[i] == lowExpected[i]);
    engine.allNotesOff();

    engine.setKeyMode(1); // high-note anchor: played key is chord's highest note
    engine.noteOn(72, 1.0);
    notes = engine.soundingNotes();
    const int highExpected[6] = {65, 69, 72, 65, 69, 72};
    for (std::size_t i = 0; i < notes.size(); ++i)
        assert(notes[i] == highExpected[i]);
    engine.allNotesOff();
}


static void testLivePatchUpdatePreservesRuntimeChordMemory()
{
    P6PatchState patch;
    patch.rawPayload.fill(0);
    patch.rawPayload[84] = 1; // Unison on
    patch.rawPayload[85] = 6; // chord mode
    patch.rawPayload[86] = 0; // low priority
    patch.rawPayload[19] = 100;
    populateRawParametersFromPayload(patch);

    P6Engine engine;
    engine.prepare(48000.0);
    engine.loadPatch(patch);
    engine.noteOn(60, 0.8);
    engine.noteOn(64, 0.8);
    engine.noteOn(67, 0.8);
    assert(engine.captureChordMemoryFromHeld());
    assert(engine.chordMemorySize() == 3);

    // A host automation/editor refresh must not look like loading a new
    // Prophet program and therefore must not erase runtime chord memory.
    patch.rawPayload[19] = 120;
    populateRawParametersFromPayload(patch);
    engine.updatePatch(patch);
    assert(engine.chordMemorySize() == 3);
    assert(engine.chordInterval(1) == 4);
    assert(engine.chordInterval(2) == 7);

    // A true program load still clears it because chord-memory serialization
    // has not been proven in the hardware payload.
    engine.loadPatch(patch);
    assert(engine.chordMemorySize() == 0);
}

static void testMappedPerformanceModes()
{
    P6PatchState patch;
    patch.rawPayload.fill(0);
    patch.rawPayload[17] = 24;
    patch.rawPayload[84] = 1;
    patch.rawPayload[85] = 6;
    patch.rawPayload[86] = 5;
    populateRawParametersFromPayload(patch);
    const auto mapped = mapPatchToDSP(patch);
    assert(mapped.unison);
    assert(mapped.unisonMode == 6);
    assert(mapped.chordMode);
    assert(mapped.keyMode == 5);
    assert(std::abs(mapped.pitchBendRangeSemitones - 12.0) < 1.0e-12);
}


static void testClockDivisionAndSampleTiming()
{
    P6Clock clock;
    clock.prepare(48000.0);
    clock.setBpm(120.0);

    struct Case { ClockDivision d; double samples; bool opaque; };
    const Case cases[] = {
        {ClockDivision::Half, 48000.0, false},
        {ClockDivision::Quarter, 24000.0, false},
        {ClockDivision::Eighth, 12000.0, false},
        {ClockDivision::EighthDotted, 18000.0, false},
        {ClockDivision::EighthSwing, 12000.0, true},
        {ClockDivision::EighthTriplet, 8000.0, false},
        {ClockDivision::Sixteenth, 6000.0, false},
        {ClockDivision::SixteenthSwing, 6000.0, true},
        {ClockDivision::SixteenthTriplet, 4000.0, false},
        {ClockDivision::ThirtySecond, 3000.0, false},
    };
    for (const auto& c : cases) {
        clock.setDivision(c.d);
        assert(std::abs(clock.nominalStepSamples() - c.samples) < 1.0e-9);
        assert(clock.swingTimingIsOpaque() == c.opaque);
    }

    clock.setDivision(ClockDivision::Sixteenth);
    clock.restart(true);
    clock.setRunning(true, false);
    std::vector<int> boundaries;
    for (int sample = 0; sample <= 18000; ++sample)
        if (clock.tick()) boundaries.push_back(sample);
    assert((boundaries == std::vector<int>{0, 6000, 12000, 18000}));

    // A host PPQ anchor at half-way through a 16th (0.125 QN into a 0.25-QN
    // step) must schedule the next boundary 3000 samples later at 120 BPM.
    clock.setDivision(ClockDivision::Sixteenth);
    clock.alignToHostPpq(4.125, 120.0, true);
    int hostBoundary = -1;
    for (int sample = 0; sample < 6001; ++sample) {
        if (clock.tick()) { hostBoundary = sample; break; }
    }
    assert(hostBoundary == 3000);

    // MIDI Timing Clock is 24 PPQN. A 16th is therefore exactly six F8 pulses.
    clock.setSyncMode(ClockSyncMode::MidiClock);
    clock.setDivision(ClockDivision::Sixteenth);
    assert(clock.midiClocksPerStep() == 6);
    clock.midiStart();
    std::vector<int> midiEdges;
    for (int pulse = 0; pulse < 19; ++pulse) {
        clock.midiTimingClock();
        if (clock.tick()) midiEdges.push_back(pulse);
    }
    assert((midiEdges == std::vector<int>{0, 6, 12, 18}));
    clock.midiStop();
    clock.midiTimingClock();
    assert(!clock.tick());
    clock.midiContinue();
    // Continue preserves pulse phase rather than restarting at pulse zero.
    for (int i = 0; i < 5; ++i) { clock.midiTimingClock(); assert(!clock.tick()); }
    clock.midiTimingClock();
    assert(clock.tick());
}

static std::vector<int> collectArp(P6Arpeggiator& arp, int n)
{
    std::vector<int> out;
    for (int i = 0; i < n; ++i) {
        const auto note = arp.next();
        assert(note.has_value());
        out.push_back(note->note);
    }
    return out;
}

static void testArpeggiatorModesAndHold()
{
    P6Arpeggiator arp;
    arp.noteOn(67, 0.7);
    arp.noteOn(60, 0.6);
    arp.noteOn(64, 0.8);

    arp.setMode(ArpMode::Up);
    assert((collectArp(arp, 4) == std::vector<int>{60, 64, 67, 60}));
    arp.setMode(ArpMode::Down);
    assert((collectArp(arp, 4) == std::vector<int>{67, 64, 60, 67}));
    arp.setMode(ArpMode::UpDown);
    assert((collectArp(arp, 7) == std::vector<int>{60, 64, 67, 64, 60, 64, 67}));
    arp.setMode(ArpMode::Assign);
    assert((collectArp(arp, 4) == std::vector<int>{67, 60, 64, 67}));

    arp.setMode(ArpMode::Up);
    arp.setOctaves(2);
    assert((collectArp(arp, 7) == std::vector<int>{60, 64, 67, 72, 76, 79, 60}));

    // HOLD keeps the latched chord after all physical keys are released, then
    // the next newly played chord replaces it instead of accumulating forever.
    arp.clear();
    arp.setOctaves(1);
    arp.setHold(true);
    arp.noteOn(60, 1.0);
    arp.noteOn(64, 1.0);
    arp.noteOff(60);
    arp.noteOff(64);
    assert((collectArp(arp, 2) == std::vector<int>{60, 64}));
    arp.noteOn(67, 1.0);
    assert(arp.keyCount() == 1);
    assert((collectArp(arp, 2) == std::vector<int>{67, 67}));

    // Prophet-6 Assign behaves as a press-event scratchpad under HOLD, not a
    // mathematical set. Repeated pitches must survive in their entered order.
    arp.clear();
    arp.setHold(true);
    arp.setMode(ArpMode::Assign);
    arp.noteOn(60, 1.0);             // keep C physically held as latch anchor
    arp.noteOn(64, 1.0); arp.noteOff(64);
    arp.noteOn(67, 1.0); arp.noteOff(67);
    arp.noteOn(64, 1.0); arp.noteOff(64);
    assert(arp.assignEventCount() == 4);
    assert((collectArp(arp, 5) == std::vector<int>{60, 64, 67, 64, 60}));
}

static void testSequencerPlayerConservativePlayback()
{
    P6PatchState patch;
    patch.rawPayload.fill(0);
    P6Sequence seq;
    seq.tracks[0][0].noteRaw = 60;
    seq.tracks[0][0].velocityRaw = 100;
    seq.tracks[0][0].velocity = 100;
    seq.tracks[1][0].noteRaw = 64;
    seq.tracks[1][0].velocityRaw = 0x80 | 90;
    seq.tracks[1][0].velocity = 90;
    seq.tracks[1][0].highBitFlag = true;
    seq.tracks[0][2].noteRaw = 67;
    seq.tracks[0][2].velocityRaw = 110;
    seq.tracks[0][2].velocity = 110;
    encodeSequence(patch, seq);
    patch.rawParameters.push_back({169, 2, 2.0f / 63.0f, false}); // length raw => 3 steps
    patch.rawParameters.push_back({170, 1, 1.0f, false});
    patch.rawParameters.push_back({171, 1, 1.0f, false});

    P6SequencerPlayer player;
    player.loadPatch(patch);
    assert(player.length() == 3);
    assert(player.seqModeRaw() == 1);
    assert(player.seqPlayModeRaw() == 1);
    assert(player.hasOpaqueHighBits());

    player.setTransposeKey(72); // +12 from middle-C reference
    const auto a = player.nextStep();
    assert(a.step == 0 && a.count == 2 && a.opaqueHighBitPresent);
    assert(a.notes[0] == 72 && a.notes[1] == 76);
    assert(std::abs(a.velocities[0] - 100.0 / 127.0) < 1e-12);
    const auto b = player.nextStep();
    assert(b.step == 1 && b.count == 0);
    const auto c = player.nextStep();
    assert(c.step == 2 && c.count == 1 && c.notes[0] == 79);
    assert(player.currentStep() == 0);
}

static void testOwnedGeneratedNotesDoNotKillLiveNotes()
{
    P6Engine engine;
    engine.prepare(48000.0);
    auto p = performanceTestVoiceParams();
    p.ampAttack = 0.001;
    p.ampRelease = 0.002;
    engine.setVoiceParams(p);

    constexpr std::uint32_t seqOwner = 0x53455131u;
    engine.noteOnOwned(60, 1.0, 0);
    engine.noteOnOwned(60, 0.8, seqOwner);
    assert(engine.activeVoiceCount() >= 2);
    engine.noteOffOwned(60, seqOwner);
    for (int i = 0; i < 1000; ++i) engine.process();

    // One C must still be sustained by the live owner. Releasing the keyboard
    // owner then lets the last C decay fully.
    bool hasC = false;
    for (int n : engine.soundingNotes()) hasC = hasC || n == 60;
    assert(hasC);
    engine.noteOffOwned(60, 0);
    for (int i = 0; i < 5000; ++i) engine.process();
    assert(engine.activeVoiceCount() == 0);
}


static void testPerformanceSchedulerTimingAndPriority()
{
    P6Engine engine;
    engine.prepare(48000.0);
    auto params = performanceTestVoiceParams();
    params.ampAttack = 0.0001;
    params.ampRelease = 0.0001;
    engine.setVoiceParams(params);

    P6PerformanceScheduler scheduler(engine);
    scheduler.prepare(48000.0);
    scheduler.setClockBpm(120.0);
    scheduler.setClockDivision(ClockDivision::Sixteenth);
    scheduler.setArpBeatSync(true);
    scheduler.setArpeggiatorEnabled(true);
    scheduler.setArpMode(ArpMode::Up);
    scheduler.noteOn(67, 1.0);
    scheduler.noteOn(60, 1.0);
    scheduler.noteOn(64, 1.0);

    // Beat Sync waits for the grid. First process sample is the aligned edge.
    (void)scheduler.process();
    for (int i = 0; i < 100; ++i) (void)scheduler.process();
    auto notes = engine.soundingNotes();
    assert(std::find(notes.begin(), notes.end(), 60) != notes.end());

    // 6000 samples per 16th at 120 BPM. At the next edge Up advances to E.
    for (int i = 101; i <= 6100; ++i) (void)scheduler.process();
    notes = engine.soundingNotes();
    assert(std::find(notes.begin(), notes.end(), 64) != notes.end());

    // Starting the sequencer suppresses arp generation but does not erase its
    // latch; stopping can return to the same arp later.
    scheduler.setSequencePlaying(true);
    assert(scheduler.sequencePlaying());
    assert(scheduler.arpeggiatorEnabled());
    scheduler.setSequencePlaying(false);
    assert(!scheduler.sequencePlaying());
}


static void testSchedulerMidiClockGlobalPolicies()
{
    auto configure = []() {
        P6Engine e;
        e.prepare(48000.0);
        auto p = performanceTestVoiceParams();
        p.ampAttack = 0.0001;
        p.ampRelease = 0.0001;
        e.setVoiceParams(p);
        return e;
    };

    // Prophet MIDI Clock "In": timing clocks alone do not run the arp until
    // Start/Continue has enabled transport.
    auto engine = configure();
    P6PerformanceScheduler s(engine);
    s.prepare(48000.0);
    s.setClockSyncMode(ClockSyncMode::MidiClock);
    s.setMidiStartStopEnabled(true);
    s.setArpeggiatorEnabled(true);
    s.noteOn(60, 1.0);
    assert(engine.activeVoiceCount() == 0);
    s.midiTimingClock();
    (void)s.process();
    assert(engine.activeVoiceCount() == 0);
    s.midiClockStart();
    s.midiTimingClock();
    (void)s.process();
    assert(engine.activeVoiceCount() != 0);
    s.setArpBeatSync(true);
    s.midiClockStop();
    s.noteOff(60);
    for (int i = 0; i < 48000; ++i)
        (void)s.process();
    assert(engine.activeVoiceCount() == 0);

    // Stop silences generated notes without erasing the HOLD latch.
    auto engineHold = configure();
    P6PerformanceScheduler held(engineHold);
    held.prepare(48000.0);
    held.setClockSyncMode(ClockSyncMode::MidiClock);
    held.setMidiStartStopEnabled(true);
    held.setArpBeatSync(true);
    held.setArpHold(true);
    held.setArpeggiatorEnabled(true);
    held.noteOn(64, 1.0);
    held.midiClockStart();
    held.midiTimingClock();
    (void)held.process();
    held.midiClockStop();
    held.noteOff(64);
    for (int i = 0; i < 48000; ++i)
        (void)held.process();
    assert(engineHold.activeVoiceCount() == 0);
    assert(!held.arpeggiator().empty());
    held.midiClockContinue();
    for (int i = 0; i < 6; ++i) {
        held.midiTimingClock();
        (void)held.process();
    }
    assert(engineHold.activeVoiceCount() != 0);

    // "In, No Start/Stop": F8 is sufficient; FA/FC are intentionally ignored.
    auto engine2 = configure();
    P6PerformanceScheduler s2(engine2);
    s2.prepare(48000.0);
    s2.setClockSyncMode(ClockSyncMode::MidiClock);
    s2.setMidiStartStopEnabled(false);
    s2.setArpeggiatorEnabled(true);
    s2.noteOn(60, 1.0);
    assert(engine2.activeVoiceCount() == 0);
    s2.midiTimingClock();
    (void)s2.process();
    assert(engine2.activeVoiceCount() != 0);
    s2.midiClockStop(); // ignored under no-start/stop policy
    for (int i = 0; i < 6; ++i) {
        s2.midiTimingClock();
        (void)s2.process();
    }
    assert(s2.clock().running());
}


static void testNrpnSelectionIncrementAndMidiRouter()
{
    NrpnDecoder decoder;
    assert(!decoder.selectedParameter());
    assert(!decoder.processCC(99, 0));
    assert(!decoder.processCC(98, 45));
    assert(decoder.selectedParameter() && *decoder.selectedParameter() == 45);
    // RPN/NRPN reset is the documented 127/127 pair, not either CC alone.
    assert(!decoder.processCC(100, 127));
    assert(decoder.selectedParameter() && *decoder.selectedParameter() == 45);
    assert(!decoder.processCC(101, 127));
    assert(decoder.selectedParameter() && *decoder.selectedParameter() == 45);
    assert(!decoder.processCC(100, 127));
    assert(!decoder.selectedParameter());

    P6PatchState patch;
    patch.rawPayload.fill(0);
    populateRawParametersFromPayload(patch);

    P6Engine engine;
    engine.prepare(48000.0);
    P6PerformanceScheduler scheduler(engine);
    scheduler.prepare(48000.0);
    scheduler.loadPatch(patch);
    P6MidiRouter router;

    const std::array<std::uint8_t, 3> pMsb {0xB0, 99, 0};
    const std::array<std::uint8_t, 3> pLsb {0xB0, 98, 45};
    const std::array<std::uint8_t, 3> vMsb {0xB0, 6, 1};
    const std::array<std::uint8_t, 3> vLsb {0xB0, 38, 10};
    assert(router.process(pMsb, patch, scheduler).consumed);
    assert(router.process(pLsb, patch, scheduler).consumed);
    assert(router.process(vMsb, patch, scheduler).consumed);
    const auto update = router.process(vLsb, patch, scheduler);
    assert(update.patchChanged && update.nrpnUpdate);
    assert(update.nrpnUpdate->parameter == 45);
    assert(update.nrpnUpdate->value == 138);
    assert(patch.rawPayload[19] == 138);

    // Prophet-6 Appendix C explicitly accepts NRPN Data Increment/Decrement.
    const std::array<std::uint8_t, 3> inc {0xB0, 96, 0};
    const auto incResult = router.process(inc, patch, scheduler);
    assert(incResult.patchChanged && incResult.nrpnUpdate->value == 139);
    assert(patch.rawPayload[19] == 139);
    const std::array<std::uint8_t, 3> dec {0xB0, 97, 0};
    const auto decResult = router.process(dec, patch, scheduler);
    assert(decResult.patchChanged && decResult.nrpnUpdate->value == 138);
    assert(patch.rawPayload[19] == 138);

    // Ordinary performance MIDI also uses the same JUCE-independent router.
    const std::array<std::uint8_t, 3> wheel {0xB0, 1, 127};
    router.process(wheel, patch, scheduler);
    assert(std::abs(engine.modWheel() - 1.0) < 1e-12);
    const std::array<std::uint8_t, 3> bendUp {0xE0, 0x7f, 0x7f};
    router.process(bendUp, patch, scheduler);
    assert(engine.pitchBendSemitones() >= 0.0);

    // Control NRPN 1088 is documented as Seq Play/Stop and must remain
    // runtime-only: it changes the scheduler without inventing a program byte.
    const std::array<std::uint8_t, 3> seqParamMsb {0xB0, 99, 8};
    const std::array<std::uint8_t, 3> seqParamLsb {0xB0, 98, 64};
    const std::array<std::uint8_t, 3> seqValueMsb {0xB0, 6, 0};
    const std::array<std::uint8_t, 3> seqValueOn  {0xB0, 38, 1};
    router.process(seqParamMsb, patch, scheduler);
    router.process(seqParamLsb, patch, scheduler);
    router.process(seqValueMsb, patch, scheduler);
    const auto seqOn = router.process(seqValueOn, patch, scheduler);
    assert(seqOn.sequencePlay && *seqOn.sequencePlay);
    assert(scheduler.sequencePlaying());
    assert(std::none_of(patch.rawParameters.begin(), patch.rawParameters.end(),
        [](const RawParameter& p) { return p.id == 1088; }));
    const auto seqOff = router.process(dec, patch, scheduler);
    assert(seqOff.sequencePlay && !*seqOff.sequencePlay);
    assert(!scheduler.sequencePlaying());

    const auto rawCountBeforeUnknown = patch.rawParameters.size();
    const std::array<std::uint8_t, 3> unknownParamMsb {0xB0, 99, 15};
    const std::array<std::uint8_t, 3> unknownParamLsb {0xB0, 98, 0};
    router.process(unknownParamMsb, patch, scheduler);
    router.process(unknownParamLsb, patch, scheduler);
    router.process(vMsb, patch, scheduler);
    const auto unknown = router.process(vLsb, patch, scheduler);
    assert(unknown.consumed && !unknown.patchChanged && !unknown.nrpnUpdate);
    assert(patch.rawParameters.size() == rawCountBeforeUnknown);

    const std::array<std::uint8_t, 4> sysexPrefix {0xF0, 0x01, 0x2d, 0x03};
    const auto sysex = router.process(sysexPrefix, patch, scheduler);
    assert(sysex.consumed && sysex.sysexDeferred && !sysex.patchChanged);
}

static void testSequencerLivePatchRefreshPreservesTransport()
{
    P6PatchState patch;
    patch.rawPayload.fill(0);
    populateRawParametersFromPayload(patch);
    applyRawNrpn(patch, {169, 1}); // two steps
    applyRawNrpn(patch, {256, 60});
    applyRawNrpn(patch, {320, 100});
    applyRawNrpn(patch, {257, 62});
    applyRawNrpn(patch, {321, 100});

    P6SequencerPlayer player;
    player.loadPatch(patch);
    player.setTransposeSemitones(2);
    const auto first = player.nextStep();
    assert(first.count == 1 && first.notes[0] == 62);
    assert(player.currentStep() == 1);

    // A live NRPN edit to step 2 must become audible without resetting the
    // current sequencer step or its performance transpose.
    applyRawNrpn(patch, {257, 67});
    player.updatePatch(patch);
    assert(player.currentStep() == 1);
    assert(player.transposeSemitones() == 2);
    const auto second = player.nextStep();
    assert(second.count == 1 && second.notes[0] == 69);
}

static void testMappedClockArpAndSequenceControls()
{
    P6PatchState patch;
    patch.rawPayload.fill(0);
    patch.rawParameters = {
        {160, 1, 1.0f, true},
        {161, 4, 1.0f, true},
        {162, 2, 1.0f, true},
        {163, 8, 0.8f, true},
        {167, 137, 0.5f, true},
        {168, 1, 1.0f, false},
        {169, 15, 15.0f / 63.0f, false},
        {170, 1, 1.0f, false},
        {171, 1, 1.0f, false},
    };
    const auto mapped = mapPatchToDSP(patch);
    assert(mapped.arpOn);
    assert(mapped.arpMode == 4);
    assert(mapped.arpRange == 2);
    assert(mapped.clockDivision == 8);
    assert(std::abs(mapped.clockBpm - 137.0) < 1e-12);
    assert(mapped.seqOn);
    assert(mapped.seqLengthRaw == 15);
    assert(mapped.seqModeRaw == 1);
    assert(mapped.seqPlayModeRaw == 1);
}

static void testHardSyncTruncatedRampRemainsBounded()
{
    // At these phases a reset intersects an almost vertical pulse edge. A
    // discarded closing BLAMP previously produced peaks above 50,000.
    for (double rate : {44100.0, 48000.0, 96000.0})
        for (double width : {0.04, 0.5, 0.96})
            for (double frequency : {110.0, 8000.0})
                for (double ratio : {0.5, 1.0}) {
                    PolyBLEPOscillator slave;
                    slave.prepare(rate);
                    slave.setFrequency(frequency);
                    slave.setShape(1.0);
                    slave.setPulseWidth(width);
                    slave.reset(0.25);
                    double masterPhase = 0.0;
                    const double step = frequency * ratio / rate;
                    for (int i = 0; i < static_cast<int>(rate * 1.5); ++i) {
                        std::optional<double> sync;
                        if (masterPhase + step >= 1.0)
                            sync = (1.0 - masterPhase) / step;
                        masterPhase += step;
                        masterPhase -= std::floor(masterPhase);
                        const double sample = slave.process(sync);
                        assert(std::isfinite(sample));
                        assert(std::abs(sample) < 10.0);
                    }
                }
}

int main()
{
    testPackingRoundTrip();
    testProgramAndEditBufferAreDistinct();
    testOscillatorFinite();
    testPulseMeanNormalizationCalibration();
    testMorphAndSubSampleSync();
    testHardSyncTruncatedRampRemainsBounded();
    testSyncCalibrationHooks();
    testHardSyncTraceAndSlopeCandidate();
    testHardSyncUnresolvedEdgeRemainsBounded();
    testDiagnosticOversamplingAndBypasses();
    testGlideModes();
    testFreeRunningOscillators();
    testDynamicSlop();
    testSsmCandidateSelfOscillation();
    testSawLfoWrapCorrection();
    testGlobalLfoShapesAndRouting();
    testAftertouchRouting();
    testEnvelopeLifecycle();
    testParameterRegistry();
    testVoiceAudioRatePath();
    testSerializedLayoutMapping();
    testNrpnDecoder();
    testNrpnSelectionIncrementAndMidiRouter();
    testSixVoiceAllocator();
    testPitchBendAndModWheelPerformance();
    testUnisonKeyModesAndRetrigger();
    testUnisonNewPhraseReattacksReleaseTail();
    testChordMemoryRuntime();
    testLivePatchUpdatePreservesRuntimeChordMemory();
    testMappedPerformanceModes();
    testClockDivisionAndSampleTiming();
    testArpeggiatorModesAndHold();
    testSequencerPlayerConservativePlayback();
    testSequencerLivePatchRefreshPreservesTransport();
    testOwnedGeneratedNotesDoNotKillLiveNotes();
    testPerformanceSchedulerTimingAndPriority();
    testSchedulerMidiClockGlobalPolicies();
    testMappedClockArpAndSequenceControls();
    testParameterMapper();
    testPatchToEnginePath();
    testSysexStreamBankDecode();
    testSequenceCodecAndNrpnMapping();
    testNrpnEditsCanonicalPayload();
    std::cout << "p6core v0.15 tests passed\n";
    return 0;
}
