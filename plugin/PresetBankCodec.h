#pragma once
#include "PluginStateCodec.h"
#include <string>

namespace p6plugin {
struct ImportedProgram {
    WrapperState state;
    std::vector<std::uint8_t> original;
};
// Entire file is validated before returning any entries. Banks are concatenated
// complete Prophet-6 dumps, not an invented bank-container format.
bool decodePresetBank(std::span<const std::uint8_t>,std::vector<ImportedProgram>&,std::string& error);
std::vector<std::uint8_t> musicalPresetBytes(WrapperState state);
} // namespace p6plugin
