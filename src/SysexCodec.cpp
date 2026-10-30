#include "p6/SysexCodec.h"
#include "p6/ProgramLayout.h"

#include <algorithm>
#include <cctype>
#include <stdexcept>

namespace p6 {
namespace {

std::string decodeName(std::span<const std::uint8_t> payload)
{
    if (payload.size() < SysexCodec::kPatchNameOffset + SysexCodec::kPatchNameLength)
        return {};

    std::string name;
    name.reserve(SysexCodec::kPatchNameLength);
    for (std::size_t i = 0; i < SysexCodec::kPatchNameLength; ++i) {
        const auto c = static_cast<char>(payload[SysexCodec::kPatchNameOffset + i]);
        if (c == '\0')
            break;
        name.push_back(std::isprint(static_cast<unsigned char>(c)) ? c : ' ');
    }

    while (!name.empty() && name.back() == ' ')
        name.pop_back();
    return name;
}

bool hasEnvelope(std::span<const std::uint8_t> b)
{
    return b.size() >= 5 && b[0] == 0xF0 &&
           b[1] == SysexCodec::kSequentialManufacturer &&
           b[2] == SysexCodec::kProphet6Id &&
           b.back() == 0xF7;
}

std::vector<std::uint8_t> encodeWithHeader(const P6PatchState& patch,
                                           std::span<const std::uint8_t> header)
{
    const auto packed = SysexCodec::pack7Bit(patch.rawPayload);
    if (packed.size() != SysexCodec::kPackedPayloadSize)
        throw std::runtime_error("Unexpected packed Prophet-6 payload size");

    std::vector<std::uint8_t> out;
    out.reserve(header.size() + packed.size() + 1);
    for (const auto b : header)
        out.push_back(b);
    for (const auto b : packed)
        out.push_back(b);
    out.push_back(0xF7);
    return out;
}

} // namespace

std::vector<std::uint8_t> SysexCodec::unpack7Bit(std::span<const std::uint8_t> packed)
{
    std::vector<std::uint8_t> out;
    out.reserve((packed.size() / 8) * 7 + 7);

    std::size_t pos = 0;
    while (pos < packed.size()) {
        const std::uint8_t msb = packed[pos++];
        const std::size_t count = std::min<std::size_t>(7, packed.size() - pos);
        for (std::size_t i = 0; i < count; ++i) {
            const std::uint8_t low7 = packed[pos++] & 0x7F;
            const std::uint8_t high = ((msb >> i) & 0x01u) << 7u;
            out.push_back(static_cast<std::uint8_t>(low7 | high));
        }
    }
    return out;
}

std::vector<std::uint8_t> SysexCodec::pack7Bit(std::span<const std::uint8_t> unpacked)
{
    std::vector<std::uint8_t> out;
    out.reserve(unpacked.size() + (unpacked.size() + 6) / 7);

    std::size_t pos = 0;
    while (pos < unpacked.size()) {
        const std::size_t count = std::min<std::size_t>(7, unpacked.size() - pos);
        std::uint8_t msb = 0;
        for (std::size_t i = 0; i < count; ++i)
            msb |= static_cast<std::uint8_t>(((unpacked[pos + i] >> 7u) & 1u) << i);
        out.push_back(msb);
        for (std::size_t i = 0; i < count; ++i)
            out.push_back(unpacked[pos + i] & 0x7F);
        pos += count;
    }
    return out;
}

SysexDecodeResult SysexCodec::decode(std::span<const std::uint8_t> bytes)
{
    SysexDecodeResult r;
    if (!hasEnvelope(bytes)) {
        r.error = "Not a Prophet-6 SysEx data message";
        return r;
    }

    std::size_t payloadOffset = 0;
    const auto command = bytes[3];

    if (command == kProgramDataCommand) {
        if (bytes.size() != kProgramDumpSize) {
            r.error = "Program Data command 0x02 requires a 1178-byte Prophet-6 dump";
            return r;
        }
        r.kind = SysexKind::ProgramDump;
        const int bank = bytes[4] & 0x0F;
        const int program = bytes[5] & 0x7F;
        if (bank > 9 || program > 99) {
            r.error = "Prophet-6 bank/program out of range";
            return r;
        }
        r.patch.bank = bank;
        r.patch.program = program;
        payloadOffset = 6;
    } else if (command == kEditBufferDataCommand) {
        if (bytes.size() != kEditBufferSize) {
            r.error = "Edit Buffer Data command 0x03 requires a 1176-byte Prophet-6 dump";
            return r;
        }
        r.kind = SysexKind::EditBuffer;
        r.patch.bank = -1;
        r.patch.program = -1;
        payloadOffset = 4;
    } else {
        r.error = "Unsupported Prophet-6 SysEx command";
        return r;
    }

    const auto packed = bytes.subspan(payloadOffset, kPackedPayloadSize);
    const auto unpacked = unpack7Bit(packed);
    if (unpacked.size() != kUnpackedPayloadSize) {
        r.error = "Unexpected unpacked Prophet-6 payload size";
        return r;
    }

    std::copy(unpacked.begin(), unpacked.end(), r.patch.rawPayload.begin());
    r.patch.name = decodeName(unpacked);
    populateRawParametersFromPayload(r.patch);
    r.ok = true;
    return r;
}

std::vector<std::uint8_t> SysexCodec::encodeProgramDump(const P6PatchState& patch,
                                                        std::uint8_t bank,
                                                        std::uint8_t program)
{
    if (bank > 9 || program > 99)
        throw std::invalid_argument("Prophet-6 bank/program out of range");
    const std::array<std::uint8_t, 6> header{
        0xF0, kSequentialManufacturer, kProphet6Id, kProgramDataCommand, bank, program
    };
    return encodeWithHeader(patch, header);
}

std::vector<std::uint8_t> SysexCodec::encodeEditBuffer(const P6PatchState& patch)
{
    const std::array<std::uint8_t, 4> header{
        0xF0, kSequentialManufacturer, kProphet6Id, kEditBufferDataCommand
    };
    return encodeWithHeader(patch, header);
}

} // namespace p6
