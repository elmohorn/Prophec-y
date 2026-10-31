#include "p6/P6EffectMapper.h"

#include <algorithm>
#include <cmath>

namespace p6 {
namespace {

constexpr double kDelayMinSeconds = 0.002;
constexpr double kDelayMaxSeconds = 1.0;
constexpr double kDelayFeedbackMax = 0.92;
constexpr double kModulationMinHz = 0.05;
constexpr double kModulationMaxHz = 8.0;

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

P6EffectSlotSettings mapSlot(const P6PatchState& patch, bool slotB)
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

    if (out.type == P6EffectType::Chorus
        || out.type == P6EffectType::Phaser1
        || out.type == P6EffectType::Phaser2) {
        out.primary = rawEffectChorusRateHz(p1Raw);
        out.secondary = unit(p2Raw, 127.0);
        return out;
    }

    if (out.type == P6EffectType::BbdDelay || out.type == P6EffectType::DigitalDelay) {
        // Keep the free-time law and the sync intent separately. Runtime tempo
        // belongs to the shared scheduler clock, not to patch mapping.
        out.primary = rawEffectDelaySeconds(p1Raw);
        out.secondary = rawEffectFeedback(p2Raw);
        out.tempoSync = rawByNrpn(patch, syncNrpn, 0) != 0;
        out.syncDivision = static_cast<std::uint8_t>(std::min<std::uint16_t>(
            rawByNrpn(patch, divisionNrpn, 5), 10));
        return out;
    }

    if (out.type == P6EffectType::Hall
        || out.type == P6EffectType::Room
        || out.type == P6EffectType::Plate
        || out.type == P6EffectType::Spring) {
        out.primary = unit(p1Raw, 255.0);
        out.secondary = unit(p2Raw, 127.0);
        return out;
    }

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
    return kModulationMinHz * std::pow(kModulationMaxHz / kModulationMinHz, t);
}

double rawEffectFeedback(std::uint16_t raw)
{
    return unit(std::min<std::uint16_t>(raw, 127), 127.0) * kDelayFeedbackMax;
}

P6EffectsSettings mapEffectsFromPatch(const P6PatchState& patch)
{
    P6EffectsSettings out;
    out.enabled = rawByNrpn(patch, 135, 0) != 0;
    out.a = mapSlot(patch, false);
    out.b = mapSlot(patch, true);
    return out;
}

} // namespace p6
