#include "p6/SysexStream.h"

#include <sstream>

namespace p6 {

SysexStreamResult decodeP6SysexStream(std::span<const std::uint8_t> bytes)
{
    SysexStreamResult result;
    std::size_t i = 0;
    while (i < bytes.size()) {
        const bool header = i + 3 < bytes.size() &&
                            bytes[i] == 0xf0 &&
                            bytes[i + 1] == SysexCodec::kSequentialManufacturer &&
                            bytes[i + 2] == SysexCodec::kProphet6Id;
        if (!header) {
            ++result.skippedBytes;
            ++i;
            continue;
        }

        const auto command = bytes[i + 3];
        std::size_t expected = 0;
        if (command == SysexCodec::kProgramDataCommand)
            expected = SysexCodec::kProgramDumpSize;
        else if (command == SysexCodec::kEditBufferDataCommand)
            expected = SysexCodec::kEditBufferSize;
        else {
            ++result.skippedBytes;
            ++i;
            continue;
        }

        if (i + expected > bytes.size()) {
            std::ostringstream os;
            os << "truncated Prophet-6 SysEx at byte " << i;
            result.errors.push_back(os.str());
            break;
        }

        const auto slice = bytes.subspan(i, expected);
        auto decoded = SysexCodec::decode(slice);
        if (!decoded.ok) {
            std::ostringstream os;
            os << "invalid Prophet-6 SysEx at byte " << i << ": " << decoded.error;
            result.errors.push_back(os.str());
            // Move to the next byte so a malformed length/header cannot hide a
            // valid message that begins shortly afterwards.
            ++i;
            continue;
        }

        result.entries.push_back(SysexStreamEntry{i, decoded.kind, std::move(decoded.patch)});
        i += expected;
    }
    return result;
}

} // namespace p6
