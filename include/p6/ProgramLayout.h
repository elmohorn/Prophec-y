#pragma once

#include "p6/P6PatchState.h"

#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

namespace p6 {

enum class SerializedValueType : std::uint8_t {
    Unsigned,
    SignedCentered,
    Enum,
    Boolean
};

struct SerializedParameterInfo {
    std::string_view name;
    std::size_t sysexOffset{};
    std::uint16_t nrpn{};
    std::uint16_t minimum{};
    std::uint16_t maximum{};
    SerializedValueType type{SerializedValueType::Unsigned};
};

// Known one-byte synthesis/program parameters in the unpacked 1024-byte P6
// payload. Program name and sequence blobs are represented separately.
std::span<const SerializedParameterInfo> serializedParameterLayout();
const SerializedParameterInfo* findSerializedByNrpn(std::uint16_t nrpn);
const SerializedParameterInfo* findSerializedByOffset(std::size_t offset);

// Populate lossless raw parameter values from a decoded P6 payload. This is
// intentionally separate from physical DSP mapping.
void populateRawParametersFromPayload(P6PatchState& patch);

} // namespace p6
