#include "p6/P6EffectMapper.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace p6 {
namespace {

constexpr double kDelayMinSeconds = 0.002;
constexpr double kDelayMaxSeconds = 1.0;
constexpr double kDelayFeedbackMax = 0.92;
constexpr double kChorusMinHz = 0.05;
constexpr double kChorusMaxHz = 8.0;

constexpr std::array<double, 11> kSyncBeats{
    4.0,
    3.0,
    2.0,
    2.0 / 3.0,
    1.5,
    1.0,
    0.75,
    0.5,
    1.0 / 3.0,
    0.375,
    0.25,
};

std::uint16_t rawByNrpn(const P6PatchState& patch,
                        std::uint16_t nrpn,
                        std::uint16_t fallback = 0)
{
    for (const auto& parameter : patch.rawParameters)
        if (parameter.id == nrpn)
            return parameter.raw;
    return fallback;
}

double unit(std::uint16_t raw, double maximum)
{
    if (maximum <= 0.0)
        return 0.0;
    return std::clamp(static_cast<double>(raw) / maximum, 0.0, 1.0);
}

P6EffectType legacyType(std::uint16_t raw, bool slotB)
{
    const auto maximum = static_cast<std::uint16_t>(slotB ? 9 : 5);
    if (raw > maximum)
        return P6EffectType::Unknown;
    return static_cast<P6EffectType>(static_cast<std::uint8_t>(raw));
}

P6EffectSlotSettings mapSlot(const P6PatchState& patch,
                             bool slotB,
                             double bpm)
{
    const std::uint16_t typeNrpn = slotB ? 127 : 119;
    const std::uint16_t mixNrpn = slotB ? 128 : 120;
    const std::uint16_t p1Nrpn = slotB ? 129 : 121;
    const std::uint16_t p2Nrpn = slotB ? 130 : 122;
    const std::uint16_t syncNrpn = slotB ? 131 : 123;
    const std::uint16_t divisionNrpn = slotB ? 132 : 124;

    P6EffectSlotSettings out;
    out.type = legacyType(rawByNrpn(patch, typeNrpn, 0), slotB);
    out.mix = unit(rawByNrpn(patch, mixNrpn, 0), 127.0);

    const auto p1Raw = rawByNrpn(patch, p1Nrpn, 0);
    const auto p2Raw = rawByNrpn(patch, p2Nrpn, 0);

    if (out.type == P6EffectType::Chorus) {
        out.primary = rawEffectChorusRateHz(p1Raw);
        out.secondary = unit(p2Raw, 127.0);
        return out;
    }

    if (out.type == P6EffectType::BbdDelay || out.type == P6EffectType::DigitalDelay) {
        if (rawByNrpn(patch, syncNrpn, 0) != 0) {
            out.primary = syncedEffectDelaySeconds(
                static_cast<std::uint8_t>(std::min<std::uint16_t>(
                    rawByNrpn(patch, divisionNrpn, 5), 10)), bpm);
        } else {
            out.primary = rawEffectDelaySeconds(p1Raw);
        }
        out.secondary = rawEffectFeedback(p2Raw);
        return out;
    }

    // The remaining legacy effect families and unknown later-OS selectors are
    // mapped structurally, but their physical parameter laws stay neutral
    // until their DSP implementations/encodings are established.
    out.primary = unit(p1Raw, 255.0);
    out.secondary = unit(p2Raw, 127.0);
    return out;
}

} // namespace

double rawEffectDelaySeconds(std::uint16_t raw)
{
    const double t = unit(std::min<std::uint16_t>(raw, 255), 255.0);
    return kDelayMinSeconds * std::pow(kDelayMaxSeconds / kDelayMinSeconds, t);
}

double rawEffectChorusRateHz(std::uint16_t raw)
{
    const double t = unit(std::min<std::uint16_t>(raw, 255), 255.0);
    return kChorusMinHz * std::pow(kChorusMaxHz / kChorusMinHz, t);
}

double rawEffectFeedback(std::uint16_t raw)
{
    return unit(std::min<std::uint16_t>(raw, 127), 127.0) * kDelayFeedbackMax;
}

double syncedEffectDelaySeconds(std::uint8_t division, double bpm)
{
    const auto index = std::min<std::size_t>(division, kSyncBeats.size() - 1u);
    const double safeBpm = std::clamp(bpm, 30.0, 250.0);
    double seconds = kSyncBeats[index] * 60.0 / safeBpm;

    // Sequential documents that synced delay values beyond the one-second
    // maximum are repeatedly halved until they fit the delay range.
    while (seconds > kDelayMaxSeconds)
        seconds *= 0.5;
    return seconds;
}

P6EffectsSettings mapEffectsFromPatch(const P6PatchState& patch)
{
    P6EffectsSettings out;
    out.enabled = rawByNrpn(patch, 135, 0) != 0;
    const double bpm = static_cast<double>(
        std::clamp<std::uint16_t>(rawByNrpn(patch, 167, 120), 30, 250));
    out.a = mapSlot(patch, false, bpm);
    out.b = mapSlot(patch, true, bpm);
    return out;
}

} // namespace p6
