#pragma once

#include "p6/P6Effects.h"
#include "p6/P6PatchState.h"

#include <cstdint>

namespace p6 {

// Maps the verified legacy Prophet-6 FX NRPN domains into the physical settings
// consumed by P6Effects. The canonical patch is never modified.
[[nodiscard]] P6EffectsSettings mapEffectsFromPatch(const P6PatchState& patch);

// Exposed for deterministic tests and future hardware calibration.
[[nodiscard]] double rawEffectDelaySeconds(std::uint16_t raw);
[[nodiscard]] double rawEffectChorusRateHz(std::uint16_t raw);
[[nodiscard]] double rawEffectFeedback(std::uint16_t raw);
[[nodiscard]] double syncedEffectDelaySeconds(std::uint8_t division,
                                              double bpm);

} // namespace p6
