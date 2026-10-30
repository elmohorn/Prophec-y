#include "p6/P6Effects.h"

#include <algorithm>
#include <cmath>

namespace p6 {
namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr double kTau = 6.28318530717958647692;
constexpr double kDelayMaxSeconds = 1.0;
constexpr double kBbdLossHz = 2600.0;
constexpr double kBbdWow = 0.0025;
constexpr double kBbdWowHz = 0.6;
constexpr double kChorusCentreSeconds = 0.0072;
constexpr double kChorusSweepSeconds = 0.0048;
constexpr double kChorusMaxSeconds = 0.02;

[[nodiscard]] double tanhApprox(double x) noexcept
{
    const double x2 = x * x;
    return x * (27.0 + x2) / (27.0 + 9.0 * x2);
}

[[nodiscard]] std::size_t wrappedIndex(std::ptrdiff_t index, std::size_t size) noexcept
{
    const auto n = static_cast<std::ptrdiff_t>(size);
    index %= n;
    if (index < 0)
        index += n;
    return static_cast<std::size_t>(index);
}

P6EffectsStereoSample mixWet(double left, double right,
                             double wetLeft, double wetRight,
                             double mix) noexcept
{
    const double wet = std::clamp(mix, 0.0, 1.0);
    return {
        left * (1.0 - wet) + wetLeft * wet,
        right * (1.0 - wet) + wetRight * wet,
    };
}

} // namespace

void P6Effects::Slot::prepare(double sampleRate, std::uint32_t seed)
{
    rng = seed != 0u ? seed : 0x5a5a1234u;

    // Longest possible delay plus BBD clock-wander margin and interpolation
    // guard samples. These vectors are never resized from process().
    const auto delayFrames = static_cast<std::size_t>(
        std::ceil(sampleRate * kDelayMaxSeconds * (1.0 + kBbdWow))) + 4u;
    delay.assign(std::max<std::size_t>(delayFrames, 8u), 0.0);

    const auto chorusFrames = static_cast<std::size_t>(
        std::ceil(sampleRate * kChorusMaxSeconds)) + 4u;
    chorusLeft.assign(std::max<std::size_t>(chorusFrames, 8u), 0.0);
    chorusRight.assign(std::max<std::size_t>(chorusFrames, 8u), 0.0);

    modulation.prepare(sampleRate);
    reverb.prepare(sampleRate);
    reset();
}

void P6Effects::Slot::reset() noexcept
{
    std::fill(delay.begin(), delay.end(), 0.0);
    std::fill(chorusLeft.begin(), chorusLeft.end(), 0.0);
    std::fill(chorusRight.begin(), chorusRight.end(), 0.0);
    delayWrite = 0;
    chorusWrite = 0;
    loopLowPass = 0.0;
    chorusPhase = 0.0;
    wowPhase = 0.0;
    wowFrom = 0.0;
    wowTo = 0.0;
    modulation.reset();
    reverb.reset();
}

double P6Effects::Slot::randomBipolar() noexcept
{
    // Small deterministic xorshift generator. This is effect-local clock drift,
    // not a claim about any Prophet firmware RNG.
    rng ^= rng << 13u;
    rng ^= rng >> 17u;
    rng ^= rng << 5u;
    constexpr double scale = 1.0 / 4294967295.0;
    return static_cast<double>(rng) * scale * 2.0 - 1.0;
}

double P6Effects::Slot::wander(double sampleRate) noexcept
{
    wowPhase += kBbdWowHz / sampleRate;
    if (wowPhase >= 1.0) {
        wowPhase -= std::floor(wowPhase);
        wowFrom = wowTo;
        wowTo = randomBipolar();
    }
    const double t = std::clamp(wowPhase, 0.0, 1.0);
    const double smooth = t * t * (3.0 - 2.0 * t);
    return wowFrom + (wowTo - wowFrom) * smooth;
}

double P6Effects::Slot::tapLinear(const std::vector<double>& buffer,
                                  std::size_t write,
                                  double samplesBack) noexcept
{
    const auto frames = buffer.size();
    if (frames < 4u)
        return 0.0;

    const double back = std::clamp(samplesBack, 1.0, static_cast<double>(frames - 2u));
    const double read = static_cast<double>(write) - back;
    const auto base = static_cast<std::ptrdiff_t>(std::floor(read));
    const double fraction = read - std::floor(read);
    const double a = buffer[wrappedIndex(base, frames)];
    const double b = buffer[wrappedIndex(base + 1, frames)];
    return a + (b - a) * fraction;
}

double P6Effects::Slot::tapCubic(const std::vector<double>& buffer,
                                 std::size_t write,
                                 double samplesBack) noexcept
{
    const auto frames = buffer.size();
    if (frames < 6u)
        return 0.0;

    const double back = std::clamp(samplesBack, 2.0, static_cast<double>(frames - 3u));
    const double read = static_cast<double>(write) - back;
    const auto i1 = static_cast<std::ptrdiff_t>(std::floor(read));
    const double f = read - std::floor(read);
    const double p0 = buffer[wrappedIndex(i1 - 1, frames)];
    const double p1 = buffer[wrappedIndex(i1, frames)];
    const double p2 = buffer[wrappedIndex(i1 + 1, frames)];
    const double p3 = buffer[wrappedIndex(i1 + 2, frames)];

    // Catmull-Rom cubic interpolation.
    const double a = -p0 + 3.0 * p1 - 3.0 * p2 + p3;
    const double b = 2.0 * p0 - 5.0 * p1 + 4.0 * p2 - p3;
    const double c = -p0 + p2;
    return p1 + 0.5 * f * (c + f * (b + f * a));
}

P6EffectsStereoSample P6Effects::Slot::process(double left, double right,
                                               const P6EffectSlotSettings& settings,
                                               double sampleRate) noexcept
{
    const double mix = std::clamp(settings.mix, 0.0, 1.0);
    if (settings.type == P6EffectType::Off || mix <= 0.0)
        return {left, right};

    switch (settings.type) {
    case P6EffectType::BbdDelay:
    case P6EffectType::DigitalDelay: {
        if (delay.empty())
            return {left, right};

        const double mono = 0.5 * (left + right);
        double samplesBack = std::clamp(settings.primary, 0.0, kDelayMaxSeconds) * sampleRate;
        double delayed = 0.0;
        if (settings.type == P6EffectType::BbdDelay) {
            samplesBack *= 1.0 + wander(sampleRate) * kBbdWow;
            delayed = tapCubic(delay, delayWrite, samplesBack);
        } else {
            delayed = tapLinear(delay, delayWrite, samplesBack);
        }

        double feedbackSample = delayed;
        if (settings.type == P6EffectType::BbdDelay) {
            const double coefficient = 1.0 - std::exp(-kTau * kBbdLossHz / sampleRate);
            loopLowPass += coefficient * (delayed - loopLowPass);
            feedbackSample = tanhApprox(loopLowPass);
        }

        const double feedback = std::clamp(settings.secondary, 0.0, 0.999);
        delay[delayWrite] = mono + feedbackSample * feedback;
        delayWrite = (delayWrite + 1u) % delay.size();

        // Prophet-6-style single delay line: wet repeat is mono and is mixed
        // equally into both channels while the dry path remains stereo.
        return mixWet(left, right, delayed, delayed, mix);
    }

    case P6EffectType::Chorus: {
        if (chorusLeft.empty() || chorusRight.empty())
            return {left, right};

        const double rateHz = std::clamp(settings.primary, 0.0, sampleRate * 0.25);
        chorusPhase += rateHz / sampleRate;
        chorusPhase -= std::floor(chorusPhase);

        const double sweepLeft = std::sin(kTau * chorusPhase);
        const double sweepRight = std::sin(kTau * chorusPhase + kPi * 0.5);
        const double depthSeconds = std::clamp(settings.secondary, 0.0, 1.0) * kChorusSweepSeconds;
        const double backLeft = (kChorusCentreSeconds + depthSeconds * sweepLeft) * sampleRate;
        const double backRight = (kChorusCentreSeconds + depthSeconds * sweepRight) * sampleRate;

        const double wetLeft = tapLinear(chorusLeft, chorusWrite, backLeft);
        const double wetRight = tapLinear(chorusRight, chorusWrite, backRight);
        chorusLeft[chorusWrite] = left;
        chorusRight[chorusWrite] = right;
        chorusWrite = (chorusWrite + 1u) % chorusLeft.size();

        return mixWet(left, right, wetLeft, wetRight, mix);
    }

    case P6EffectType::Phaser1:
    case P6EffectType::Phaser2:
    case P6EffectType::Phaser3: {
        const int variant = settings.type == P6EffectType::Phaser1 ? 1
            : (settings.type == P6EffectType::Phaser2 ? 2 : 3);
        const auto wet = modulation.processPhaser(
            left, right, settings.primary, settings.secondary, variant);
        return mixWet(left, right, wet.left, wet.right, mix);
    }

    case P6EffectType::Flanger1:
    case P6EffectType::Flanger2: {
        const auto wet = modulation.processFlanger(
            left, right, settings.primary, settings.secondary,
            settings.type == P6EffectType::Flanger1);
        return mixWet(left, right, wet.left, wet.right, mix);
    }

    case P6EffectType::RingMod: {
        const auto wet = modulation.processRingMod(left, right, settings.primary);
        return mixWet(left, right, wet.left, wet.right, mix);
    }

    case P6EffectType::Hall:
    case P6EffectType::Room:
    case P6EffectType::Plate:
    case P6EffectType::Spring: {
        P6ReverbEffects::Type type = P6ReverbEffects::Type::Hall;
        if (settings.type == P6EffectType::Room)
            type = P6ReverbEffects::Type::Room;
        else if (settings.type == P6EffectType::Plate)
            type = P6ReverbEffects::Type::Plate;
        else if (settings.type == P6EffectType::Spring)
            type = P6ReverbEffects::Type::Spring;
        const auto wet = reverb.process(left, right, type,
                                        settings.primary, settings.secondary);
        return mixWet(left, right, wet.left, wet.right, mix);
    }

    case P6EffectType::Unknown:
    case P6EffectType::Off:
        return {left, right};
    }

    return {left, right};
}

void P6Effects::prepare(double sampleRate)
{
    sampleRate_ = std::max(8000.0, sampleRate);
    slotA_.prepare(sampleRate_, 0x6a09e667u);
    slotB_.prepare(sampleRate_, 0xbb67ae85u);
}

void P6Effects::reset() noexcept
{
    slotA_.reset();
    slotB_.reset();
}

void P6Effects::setSettings(const P6EffectsSettings& settings) noexcept
{
    settings_ = settings;
    settings_.a.mix = std::clamp(settings_.a.mix, 0.0, 1.0);
    settings_.b.mix = std::clamp(settings_.b.mix, 0.0, 1.0);
}

P6EffectsStereoSample P6Effects::process(double left, double right) noexcept
{
    // The hardware ON/OFF control is specified as a true bypass. At this layer
    // bypass is bit-transparent and does not touch the wet state. Whether the
    // hardware continues advancing tails while bypassed remains a measurement
    // question and is deliberately not guessed here.
    if (!settings_.enabled)
        return {left, right};

    const auto afterA = slotA_.process(left, right, settings_.a, sampleRate_);
    return slotB_.process(afterA.left, afterA.right, settings_.b, sampleRate_);
}

} // namespace p6
