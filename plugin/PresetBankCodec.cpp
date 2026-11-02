#include "PresetBankCodec.h"
#include "p6/SysexCodec.h"
#include <algorithm>
#include <set>

namespace p6plugin {
bool decodePresetBank(std::span<const std::uint8_t> bytes,std::vector<ImportedProgram>& destination,std::string& error)
{
    if(bytes.empty()||bytes.size()>16u*1024u*1024u){error="Empty or oversized SysEx file (maximum 16 MiB)";return false;}
    std::vector<ImportedProgram> result;
    std::set<int> addresses;
    std::size_t offset=0;
    while(offset<bytes.size()) {
        if(bytes[offset]!=0xf0){error="Expected a complete Prophet-6 SysEx message";return false;}
        const auto end=std::find(bytes.begin()+static_cast<std::ptrdiff_t>(offset),bytes.end(),0xf7);
        if(end==bytes.end()){error="Truncated SysEx message";return false;}
        const auto length=static_cast<std::size_t>(end-(bytes.begin()+static_cast<std::ptrdiff_t>(offset)))+1;
        const auto frame=bytes.subspan(offset,length);
        // MIDI packed data must be seven-bit; don't silently normalize invalid
        // messages into a different byte stream during import.
        if(std::any_of(frame.begin()+1,frame.end()-1,[](auto b){return b>127;})){error="Invalid high-bit byte in SysEx data";return false;}
        const auto decoded=p6::SysexCodec::decode(frame);
        if(!decoded.ok){error=decoded.error;return false;}
        if(decoded.patch.bank>=0&&!addresses.insert(decoded.patch.bank*100+decoded.patch.program).second){error="Duplicate bank/program address in SysEx file";return false;}
        ImportedProgram program;program.state.patch=decoded.patch;program.state.sequenceControls={0,63,0,0};
        program.original.assign(frame.begin(),frame.end());result.push_back(std::move(program));
        offset+=length;
        if(result.size()>10000){error="Too many programs";return false;}
    }
    destination=std::move(result);error.clear();return true;
}
std::vector<std::uint8_t> musicalPresetBytes(WrapperState state)
{
    // Globals and runtime playback must never produce a spurious unsaved mark
    // or get replaced by browsing a musical patch.
    state.extras={};state.masterOutputDb=0;
    return encodeState(state);
}
} // namespace p6plugin
