#pragma once

#include "p6/SysexCodec.h"

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace p6 {

struct SysexStreamEntry {
    std::size_t byteOffset{};
    SysexKind kind{SysexKind::ProgramDump};
    P6PatchState patch{};
};

struct SysexStreamResult {
    std::vector<SysexStreamEntry> entries;
    std::vector<std::string> errors;
    std::size_t skippedBytes{};
};

// Scan a raw .syx byte stream containing one program, a bank, all banks, or
// mixed Prophet-6 Program/Edit Buffer dumps. Unknown MIDI bytes are skipped;
// malformed candidate P6 messages are reported rather than silently accepted.
SysexStreamResult decodeP6SysexStream(std::span<const std::uint8_t> bytes);

} // namespace p6
