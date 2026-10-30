#pragma once

#include "p6/P6PatchState.h"

#include <array>
#include <cstdint>
#include <span>
#include <vector>

namespace p6plugin {

struct WrapperState {
    p6::P6PatchState patch;
    std::array<std::uint16_t, 4> sequenceControls{}; // NRPN 168..171
    std::array<std::uint8_t, 8> extras{};            // wrapper/runtime controls
};

inline constexpr std::array<std::uint8_t, 4> kStateMagic { 'P', '6', 'V', '1' };
inline constexpr std::uint16_t kStateVersion = 1;
inline constexpr std::size_t kEncodedStateBytes = 4 + 2 + 2 + 2 + 1024 + 8 + 8;

std::vector<std::uint8_t> encodeState(const WrapperState& state);
bool decodeState(std::span<const std::uint8_t> bytes, WrapperState& state);

} // namespace p6plugin
