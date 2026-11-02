#include "PluginStateCodec.h"

#include "p6/NrpnCodec.h"
#include "p6/ProgramLayout.h"
#include "p6/SysexCodec.h"

#include <algorithm>
#include <cstddef>
#include <cmath>

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
    out.push_back(static_cast<std::uint8_t>(std::clamp(state.character.type,0,kCharacterTypes-1)));
    out.push_back(static_cast<std::uint8_t>(std::clamp(state.character.position,0,1)));
    out.push_back(static_cast<std::uint8_t>(state.character.bypass));
    for(std::size_t i=0;i<state.character.tube.size();++i) {
        const float v=state.character.tube[i];
        putU16(out,static_cast<std::uint16_t>(std::lround(1000.f*(std::isfinite(v)?std::clamp(v,0.f,1.f):kTubeDefaults[i]))));
    }
    const float db=std::isfinite(state.masterOutputDb)?std::clamp(state.masterOutputDb,-60.f,0.f):0.f;
    putU16(out,static_cast<std::uint16_t>(static_cast<std::int16_t>(std::lround(db*10.f))));
    for(std::size_t d=0;d<4;++d)for(std::size_t i=0;i<7;++i) {
        const float v=state.character.custom[d][i];
        putU16(out,static_cast<std::uint16_t>(std::lround(1000.f*(std::isfinite(v)?std::clamp(v,0.f,1.f):kCustomDefaults[d][i]))));
    }
    out.push_back(static_cast<std::uint8_t>(std::clamp(state.character.type<2?state.character.type:state.character.legacyType,0,1)));
    return out;
}

bool decodeState(std::span<const std::uint8_t> bytes, WrapperState& state)
{
    if (bytes.size() < kV1StateBytes
        || !std::equal(kStateMagic.begin(), kStateMagic.end(), bytes.begin()))
        return false;

    std::size_t offset = kStateMagic.size();
    const auto version=getU16(bytes, offset);
    if (version<1 || version>kStateVersion || (version==2 && bytes.size()<kV2StateBytes) || (version==3 && bytes.size()<kV3StateBytes) || (version==4 && bytes.size()<kEncodedStateBytes))
        return false;

    WrapperState decoded;
    decoded.masterOutputDb=0.f; // v1/v2 must retain the historical unity output

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
    if(version>=2) {
        decoded.character.type=bytes[offset++];
        decoded.character.position=bytes[offset++];
        const auto bypass=bytes[offset++];
        if(decoded.character.type>(version>=4?kCharacterTypes-1:1) || decoded.character.position>1 || bypass>1) return false;
        decoded.character.bypass=bypass!=0;
        for(auto& v:decoded.character.tube) {
            const auto raw=getU16(bytes,offset);
            if(raw>1000) return false;
            v=static_cast<float>(raw)/1000.f;
        }
    }

    if(version>=3) {
        const auto raw=static_cast<std::int16_t>(getU16(bytes,offset));
        if(raw>0 || raw < -600)return false;
        decoded.masterOutputDb=static_cast<float>(raw)/10.f;
    }
    if(version>=4)for(auto& device:decoded.character.custom)for(auto& v:device) {
        const auto raw=getU16(bytes,offset);if(raw>1000)return false;v=static_cast<float>(raw)/1000.f;
    }
    decoded.character.legacyType=decoded.character.type<2?decoded.character.type:0;
    if(version>=4){const auto legacy=bytes[offset++];if(legacy>1)return false;decoded.character.legacyType=legacy;}
    state = std::move(decoded);
    return true;
}

} // namespace p6plugin

