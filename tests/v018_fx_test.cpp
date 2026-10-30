#include "p6/P6EffectMapper.h"
#include "p6/P6Effects.h"
#include "p6/P6Engine.h"

#include <algorithm>
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

void setRaw(p6::P6PatchState& patch, std::uint16_t id, std::uint16_t raw)
{
    patch.rawParameters.push_back({id, raw, 0.0f, true});
}

void testGlobalBypassIsTransparent()
{
    p6::P6Effects fx;
    fx.prepare(48000.0);

    p6::P6EffectsSettings settings;
    settings.enabled = false;
    settings.a = {p6::P6EffectType::DigitalDelay, 1.0, 0.01, 0.5};
    settings.b = {p6::P6EffectType::Chorus, 1.0, 1.0, 1.0};
    fx.setSettings(settings);

    for (int i = 0; i < 1024; ++i) {
        const double left = std::sin(0.013 * static_cast<double>(i));
        const double right = std::cos(0.009 * static_cast<double>(i));
        const auto out = fx.process(left, right);
        P6_CHECK(out.left == left);
        P6_CHECK(out.right == right);
    }
}

void testOffSlotsAreTransparent()
{
    p6::P6Effects fx;
    fx.prepare(48000.0);

    p6::P6EffectsSettings settings;
    settings.enabled = true;
    fx.setSettings(settings);

    for (int i = 0; i < 256; ++i) {
        const double left = static_cast<double>(i) / 255.0 - 0.5;
        const double right = -left;
        const auto out = fx.process(left, right);
        P6_CHECK(out.left == left);
        P6_CHECK(out.right == right);
    }
}

void testDigitalDelayTiming()
{
    constexpr double sampleRate = 48000.0;
    constexpr double delaySeconds = 0.01;
    constexpr int expected = static_cast<int>(sampleRate * delaySeconds);

    p6::P6Effects fx;
    fx.prepare(sampleRate);
    p6::P6EffectsSettings settings;
    settings.enabled = true;
    settings.a = {p6::P6EffectType::DigitalDelay, 1.0, delaySeconds, 0.0};
    fx.setSettings(settings);

    double atExpected = 0.0;
    for (int i = 0; i <= expected + 2; ++i) {
        const auto out = fx.process(i == 0 ? 1.0 : 0.0, i == 0 ? 1.0 : 0.0);
        if (i < expected)
            P6_CHECK(std::abs(out.left) < 1.0e-12);
        if (i == expected)
            atExpected = out.left;
    }
    P6_CHECK(atExpected > 0.99);
}

void testSlotsAreSerialAThenB()
{
    constexpr double sampleRate = 48000.0;
    constexpr double delaySeconds = 0.01;
    constexpr int oneDelay = static_cast<int>(sampleRate * delaySeconds);
    constexpr int twoDelays = oneDelay * 2;

    p6::P6Effects fx;
    fx.prepare(sampleRate);
    p6::P6EffectsSettings settings;
    settings.enabled = true;
    settings.a = {p6::P6EffectType::DigitalDelay, 1.0, delaySeconds, 0.0};
    settings.b = {p6::P6EffectType::DigitalDelay, 1.0, delaySeconds, 0.0};
    fx.setSettings(settings);

    double atOne = 0.0;
    double atTwo = 0.0;
    for (int i = 0; i <= twoDelays + 2; ++i) {
        const auto out = fx.process(i == 0 ? 1.0 : 0.0, i == 0 ? 1.0 : 0.0);
        if (i == oneDelay)
            atOne = out.left;
        if (i == twoDelays)
            atTwo = out.left;
    }
    P6_CHECK(std::abs(atOne) < 1.0e-12);
    P6_CHECK(atTwo > 0.99);
}

void testBbdSilenceStaysSilentAndFinite()
{
    p6::P6Effects fx;
    fx.prepare(48000.0);
    p6::P6EffectsSettings settings;
    settings.enabled = true;
    settings.a = {p6::P6EffectType::BbdDelay, 1.0, 1.0, 0.92};
    fx.setSettings(settings);

    for (int i = 0; i < 100000; ++i) {
        const auto out = fx.process(0.0, 0.0);
        P6_CHECK(std::isfinite(out.left));
        P6_CHECK(std::isfinite(out.right));
        P6_CHECK(out.left == 0.0);
        P6_CHECK(out.right == 0.0);
    }
}

void testChorusCreatesStereoModulation()
{
    constexpr double sampleRate = 48000.0;
    p6::P6Effects fx;
    fx.prepare(sampleRate);
    p6::P6EffectsSettings settings;
    settings.enabled = true;
    settings.a = {p6::P6EffectType::Chorus, 1.0, 1.5, 1.0};
    fx.setSettings(settings);

    double maxStereoDifference = 0.0;
    for (int i = 0; i < 6000; ++i) {
        const double input = std::sin(6.28318530717958647692 * 440.0 * static_cast<double>(i) / sampleRate);
        const auto out = fx.process(input, input);
        P6_CHECK(std::isfinite(out.left));
        P6_CHECK(std::isfinite(out.right));
        if (i > 1500)
            maxStereoDifference = std::max(maxStereoDifference, std::abs(out.left - out.right));
    }
    P6_CHECK(maxStereoDifference > 1.0e-4);
}

void testHardwareMapperLaws()
{
    P6_CHECK(std::abs(p6::rawEffectDelaySeconds(0) - 0.002) < 1.0e-12);
    P6_CHECK(std::abs(p6::rawEffectDelaySeconds(255) - 1.0) < 1.0e-12);
    P6_CHECK(std::abs(p6::rawEffectChorusRateHz(0) - 0.05) < 1.0e-12);
    P6_CHECK(std::abs(p6::rawEffectChorusRateHz(255) - 8.0) < 1.0e-12);
    P6_CHECK(std::abs(p6::rawEffectFeedback(127) - 0.92) < 1.0e-12);

    // One beat at 120 BPM = 0.5 s. Four beats at 60 BPM would be 4 s,
    // but Sequential documents repeated halving until the 1-second maximum.
    P6_CHECK(std::abs(p6::syncedEffectDelaySeconds(5, 120.0) - 0.5) < 1.0e-12);
    P6_CHECK(std::abs(p6::syncedEffectDelaySeconds(0, 60.0) - 1.0) < 1.0e-12);
}

void testPatchMappingPreservesLegacyDomainsAndUnknowns()
{
    p6::P6PatchState patch;
    setRaw(patch, 135, 1);   // global effects on
    setRaw(patch, 167, 120); // BPM

    setRaw(patch, 119, 2);   // FX A DDL
    setRaw(patch, 120, 127); // full wet
    setRaw(patch, 121, 255); // max free delay time
    setRaw(patch, 122, 127); // max feedback

    setRaw(patch, 127, 6);   // FX B Hall in verified legacy B domain
    setRaw(patch, 128, 64);
    setRaw(patch, 129, 128);
    setRaw(patch, 130, 32);

    const auto mapped = p6::mapEffectsFromPatch(patch);
    P6_CHECK(mapped.enabled);
    P6_CHECK(mapped.a.type == p6::P6EffectType::DigitalDelay);
    P6_CHECK(mapped.a.mix == 1.0);
    P6_CHECK(std::abs(mapped.a.primary - 1.0) < 1.0e-12);
    P6_CHECK(std::abs(mapped.a.secondary - 0.92) < 1.0e-12);
    P6_CHECK(mapped.b.type == p6::P6EffectType::Hall);

    p6::P6PatchState phaser;
    setRaw(phaser, 119, 4);   // FX A PH1
    setRaw(phaser, 120, 127);
    setRaw(phaser, 121, 255); // max rate
    setRaw(phaser, 122, 127); // max depth
    setRaw(phaser, 135, 1);
    const auto phaserMapped = p6::mapEffectsFromPatch(phaser);
    P6_CHECK(phaserMapped.a.type == p6::P6EffectType::Phaser1);
    P6_CHECK(std::abs(phaserMapped.a.primary - 8.0) < 1.0e-12);
    P6_CHECK(phaserMapped.a.secondary == 1.0);

    p6::P6PatchState unknownA;
    setRaw(unknownA, 119, 6); // outside verified legacy A domain
    setRaw(unknownA, 120, 127);
    setRaw(unknownA, 135, 1);
    const auto unknownMapped = p6::mapEffectsFromPatch(unknownA);
    P6_CHECK(unknownMapped.a.type == p6::P6EffectType::Unknown);

    p6::P6Effects fx;
    fx.prepare(48000.0);
    fx.setSettings(unknownMapped);
    const auto out = fx.process(0.25, -0.5);
    P6_CHECK(out.left == 0.25);
    P6_CHECK(out.right == -0.5);
}

void testLegacyPhasersAreAudibleAndFinite()
{
    constexpr double sampleRate = 48000.0;
    for (const auto type : {p6::P6EffectType::Phaser1, p6::P6EffectType::Phaser2}) {
        p6::P6Effects fx;
        fx.prepare(sampleRate);
        p6::P6EffectsSettings settings;
        settings.enabled = true;
        settings.a = {type, 1.0, 0.8, 0.75};
        fx.setSettings(settings);

        double difference = 0.0;
        for (int i = 0; i < 12000; ++i) {
            const double input = 0.35 * std::sin(6.28318530717958647692 * 330.0 * i / sampleRate);
            const auto out = fx.process(input, input);
            P6_CHECK(std::isfinite(out.left));
            P6_CHECK(std::isfinite(out.right));
            if (i > 500)
                difference = std::max(difference, std::abs(out.left - input));
        }
        P6_CHECK(difference > 1.0e-4);
    }
}

void testLegacyReverbsCreateFiniteTails()
{
    constexpr std::array<p6::P6EffectType, 4> types{
        p6::P6EffectType::Hall,
        p6::P6EffectType::Room,
        p6::P6EffectType::Plate,
        p6::P6EffectType::Spring,
    };

    for (const auto type : types) {
        p6::P6Effects fx;
        fx.prepare(48000.0);
        p6::P6EffectsSettings settings;
        settings.enabled = true;
        settings.a = {type, 1.0, 0.65, 0.55};
        fx.setSettings(settings);

        double tailEnergy = 0.0;
        for (int i = 0; i < 20000; ++i) {
            const double input = i == 0 ? 1.0 : 0.0;
            const auto out = fx.process(input, input);
            P6_CHECK(std::isfinite(out.left));
            P6_CHECK(std::isfinite(out.right));
            if (i > 200)
                tailEnergy += std::abs(out.left) + std::abs(out.right);
        }
        P6_CHECK(tailEnergy > 1.0e-5);
    }
}

void testLaterOsInternalCandidatesAreAudibleButNotRawMapped()
{
    constexpr double sampleRate = 48000.0;
    const std::array<p6::P6EffectSlotSettings, 4> candidates{{
        {p6::P6EffectType::Phaser3, 1.0, 0.7, 0.7},
        {p6::P6EffectType::RingMod, 1.0, 137.0, 0.0},
        {p6::P6EffectType::Flanger1, 1.0, 0.35, 0.9},
        {p6::P6EffectType::Flanger2, 1.0, 0.35, 0.9},
    }};

    for (const auto candidate : candidates) {
        p6::P6Effects fx;
        fx.prepare(sampleRate);
        p6::P6EffectsSettings settings;
        settings.enabled = true;
        settings.a = candidate;
        fx.setSettings(settings);

        double difference = 0.0;
        for (int i = 0; i < 12000; ++i) {
            const double input = 0.3 * std::sin(6.28318530717958647692 * 220.0 * i / sampleRate);
            const auto out = fx.process(input, input);
            P6_CHECK(std::isfinite(out.left));
            P6_CHECK(std::isfinite(out.right));
            if (i > 1500)
                difference = std::max(difference, std::abs(out.left - input));
        }
        P6_CHECK(difference > 1.0e-5);
    }
}

p6::P6PatchState audibleDelayPatch()
{
    p6::P6PatchState patch;
    // A simple open-filter oscillator patch with immediate amp sustain.
    setRaw(patch, 0, 24);    // Osc1 reference octave
    setRaw(patch, 2, 127);   // Osc1 level
    setRaw(patch, 3, 127);   // Osc1 saw-ish shape
    setRaw(patch, 4, 127);   // square-centred pulse width
    setRaw(patch, 45, 164);  // LPF open
    setRaw(patch, 49, 0);    // HPF low/bypass-like
    setRaw(patch, 62, 127);  // program volume
    setRaw(patch, 66, 127);  // VCA envelope amount
    setRaw(patch, 67, 0);    // attack
    setRaw(patch, 68, 0);    // decay
    setRaw(patch, 69, 127);  // sustain
    setRaw(patch, 70, 0);    // release

    setRaw(patch, 119, 2);   // FX A digital delay
    setRaw(patch, 120, 127); // full wet
    setRaw(patch, 121, 66);  // about 10 ms
    setRaw(patch, 122, 0);   // no feedback required for this regression
    setRaw(patch, 127, 0);   // FX B off
    setRaw(patch, 135, 1);   // effects on
    return patch;
}

void testEngineOwnsFxMappingAndAllSoundOffClearsTail()
{
    p6::P6Engine engine;
    engine.prepare(48000.0, 1);
    const auto patch = audibleDelayPatch();
    engine.loadPatch(patch);

    const auto& settings = engine.effectsSettingsForDiagnostics();
    P6_CHECK(settings.enabled);
    P6_CHECK(settings.a.type == p6::P6EffectType::DigitalDelay);
    P6_CHECK(settings.a.mix == 1.0);
    P6_CHECK(settings.b.type == p6::P6EffectType::Off);

    engine.noteOn(60, 1.0);
    double beforeKillPeak = 0.0;
    for (int i = 0; i < 2000; ++i) {
        const auto out = engine.process();
        beforeKillPeak = std::max(beforeKillPeak, std::max(std::abs(out.left), std::abs(out.right)));
    }
    P6_CHECK(beforeKillPeak > 1.0e-6);

    engine.midiAllSoundOff();
    for (int i = 0; i < 2000; ++i) {
        const auto out = engine.process();
        P6_CHECK(out.left == 0.0);
        P6_CHECK(out.right == 0.0);
    }
}

} // namespace

int main()
{
    testGlobalBypassIsTransparent();
    testOffSlotsAreTransparent();
    testDigitalDelayTiming();
    testSlotsAreSerialAThenB();
    testBbdSilenceStaysSilentAndFinite();
    testChorusCreatesStereoModulation();
    testHardwareMapperLaws();
    testPatchMappingPreservesLegacyDomainsAndUnknowns();
    testLegacyPhasersAreAudibleAndFinite();
    testLegacyReverbsCreateFiniteTails();
    testLaterOsInternalCandidatesAreAudibleButNotRawMapped();
    testEngineOwnsFxMappingAndAllSoundOffClearsTail();
    std::cout << "p6 v0.18 FX core tests passed\n";
    return 0;
}
