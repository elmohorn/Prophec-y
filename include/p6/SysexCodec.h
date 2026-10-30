#pragma once

#include "p6/P6PatchState.h"
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace p6 {

enum class SysexKind {
    ProgramDump,
    EditBuffer
};

struct SysexDecodeResult {
    bool ok{};
    SysexKind kind{SysexKind::ProgramDump};
    P6PatchState patch{};
    std::string error;
};

class SysexCodec {
public:
    static constexpr std::uint8_t kSequentialManufacturer = 0x01;
    static constexpr std::uint8_t kProphet6Id = 0x2D;
    static constexpr std::uint8_t kProgramDataCommand = 0x02;
    static constexpr std::uint8_t kEditBufferDataCommand = 0x03;
    static constexpr std::size_t kPackedPayloadSize = 1171;
    static constexpr std::size_t kUnpackedPayloadSize = 1024;
    static constexpr std::size_t kProgramDumpSize = 1178; // F0 01 2D 02 bank prog + 1171 + F7
    static constexpr std::size_t kEditBufferSize = 1176;  // F0 01 2D 03 + 1171 + F7
    static constexpr std::size_t kPatchNameOffset = 107;
    static constexpr std::size_t kPatchNameLength = 20;

    static SysexDecodeResult decode(std::span<const std::uint8_t> bytes);

    // Lossless export of the canonical raw payload. This intentionally does not
    // synthesize parameter bytes from normalized DSP values yet; the raw P6
    // payload remains the source of truth until the mapper is fully calibrated.
    static std::vector<std::uint8_t> encodeProgramDump(const P6PatchState& patch,
                                                       std::uint8_t bank,
                                                       std::uint8_t program);
    static std::vector<std::uint8_t> encodeEditBuffer(const P6PatchState& patch);

    // Standard 7-bit MIDI packing used by Sequential: one MSB header followed
    // by up to seven data bytes.
    static std::vector<std::uint8_t> unpack7Bit(std::span<const std::uint8_t> packed);
    static std::vector<std::uint8_t> pack7Bit(std::span<const std::uint8_t> unpacked);
};

} // namespace p6
