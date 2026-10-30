#pragma once

#include <cstdint>
#include <span>
#include <string_view>

namespace p6 {

enum class ParameterGroup : std::uint8_t {
    Oscillator,
    Mixer,
    Glide,
    Filter,
    Amplifier,
    LFO,
    Pressure,
    Effects,
    PolyMod,
    Performance,
    Arpeggiator,
    Sequencer,
    Name
};

struct ParameterInfo {
    std::uint16_t nrpn{};
    std::uint16_t minimum{};
    std::uint16_t maximum{};
    std::string_view name;
    ParameterGroup group{};
};

// Official program-NRPN metadata. This registry intentionally does not claim
// that NRPN number == SysEx payload offset; those are separate coordinate
// systems and will be joined by the calibrated ParameterMapper.
std::span<const ParameterInfo> programParameterRegistry();
const ParameterInfo* findProgramParameter(std::uint16_t nrpn);

} // namespace p6
