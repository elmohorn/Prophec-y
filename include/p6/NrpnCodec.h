#pragma once

#include "p6/P6PatchState.h"

#include <cstdint>
#include <optional>

namespace p6 {

struct NrpnUpdate {
    std::uint16_t parameter{};
    std::uint16_t value{};
};

// Stateful decoder for standard MIDI NRPN CC streams used by the Prophet-6.
class NrpnDecoder {
public:
    void reset();
    std::optional<NrpnUpdate> processCC(std::uint8_t controller, std::uint8_t value);
    [[nodiscard]] std::optional<std::uint16_t> selectedParameter() const noexcept;

private:
    std::uint8_t paramMsb_{};
    std::uint8_t paramLsb_{};
    std::uint8_t valueMsb_{};
    bool haveParamMsb_{};
    bool haveParamLsb_{};
    bool haveValueMsb_{};
    bool rpnResetMsbSeen_{};
};

void applyRawNrpn(P6PatchState& patch, NrpnUpdate update);

} // namespace p6
