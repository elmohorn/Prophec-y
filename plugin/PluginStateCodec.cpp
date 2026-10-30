#include "PluginStateCodec.h"

#include "p6/NrpnCodec.h"
#include "p6/ProgramLayout.h"
#include "p6/SysexCodec.h"

#include <algorithm>
#include <cstddef>

namespace p6plugin {
namespace {

void putU16(std::vector<std::uint8_t>& out, std::uint16_t value)
{
    out.push_back(static_cast<std::uint8_t>(value & 0xffu));
    out.push_back(static_cast<std::uint8_t>((value >> 8u) & 0xffu));
}

std::uint16_t getU16(std::span<const std::uint8_t> bytes, std::size_t& offset)
{
    const auto lo = static_cast<std::uint16_t>(bytes[offset++]);
    const auto hi = static_cast<std::uint16_t>(bytes[offset++]);
    return static_cast<std::uint16_t>(lo | (hi << 8u));
}

void populateName(p6::P6PatchState& patch)
{
    patch.name.clear();
    patch.name.reserve(p6::SysexCodec::kPatchNameLength);
    for (std::size_t i = 0; i < p6::SysexCodec::kPatchNameLength; ++i) {
        const auto c = patch.rawPayload[p6::SysexCodec::kPatchNameOffset + i];
        patch.name.push_back(c >= 32 && c <= 126 ? static_cast<char>(c) : ' ');
    }
    while (!patch.name.empty() && patch.name.back() == ' ')
        patch.name.pop_back();
}

} // namespace

std::vector<std::uint8_t> encodeState(const WrapperState& state)
{
    std::vector<std::uint8_t> out;
    out.reserve(kEncodedStateBytes);
    out.insert(out.end(), kStateMagic.begin(), kStateMagic.end());
    putU16(out, kStateVersion);
    putU16(out, static_cast<std::uint16_t>(static_cast<std::int16_t>(state.patch.bank)));
    putU16(out, static_cast<std::uint16_t>(static_cast<std::int16_t>(state.patch.program)));
    out.insert(out.end(), state.patch.rawPayload.begin(), state.patch.rawPayload.end());
    for (const auto value : state.sequenceControls)
        putU16(out, value);
    out.insert(out.end(), state.extras.begin(), state.extras.end());
    return out;
}

bool decodeState(std::span<const std::uint8_t> bytes, WrapperState& state)
{
    if (bytes.size() < kEncodedStateBytes
        || !std::equal(kStateMagic.begin(), kStateMagic.end(), bytes.begin()))
        return false;

    std::size_t offset = kStateMagic.size();
    if (getU16(bytes, offset) != kStateVersion)
        return false;

    WrapperState decoded;
    decoded.patch.bank = static_cast<std::int16_t>(getU16(bytes, offset));
    decoded.patch.program = static_cast<std::int16_t>(getU16(bytes, offset));

    std::copy_n(bytes.begin() + static_cast<std::ptrdiff_t>(offset),
                decoded.patch.rawPayload.size(), decoded.patch.rawPayload.begin());
    offset += decoded.patch.rawPayload.size();

    p6::populateRawParametersFromPayload(decoded.patch);
    populateName(decoded.patch);

    for (std::size_t i = 0; i < decoded.sequenceControls.size(); ++i) {
        decoded.sequenceControls[i] = getU16(bytes, offset);
        p6::applyRawNrpn(decoded.patch,
                         { static_cast<std::uint16_t>(168 + i), decoded.sequenceControls[i] });
    }
    for (auto& value : decoded.extras)
        value = bytes[offset++];

    state = std::move(decoded);
    return true;
}

} // namespace p6plugin
