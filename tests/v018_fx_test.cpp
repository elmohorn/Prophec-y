#include "p6/P6Effects.h"

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
        if (i > 1500)
            maxStereoDifference = std::max(maxStereoDifference, std::abs(out.left - out.right));
    }
    P6_CHECK(maxStereoDifference > 1.0e-4);
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
    std::cout << "p6 v0.18 FX core tests passed\n";
    return 0;
}
