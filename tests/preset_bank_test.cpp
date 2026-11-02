#include "PresetBankCodec.h"
#include "p6/SysexCodec.h"
#include <iostream>
#define CHECK(x) do{if(!(x)){std::cerr<<"failed: " #x <<" line "<<__LINE__<<'\n';return 1;}}while(false)
int main()
{
    using namespace p6plugin;
    WrapperState state;for(std::size_t i=0;i<1024;++i)state.patch.rawPayload[i]=static_cast<std::uint8_t>((i*73u+17u)&255u);
    state.patch.bank=3;state.patch.program=4;
    const auto one=p6::SysexCodec::encodeProgramDump(state.patch,3,4);
    auto other=one;other[5]=5;
    std::vector<std::uint8_t> bank=one;bank.insert(bank.end(),other.begin(),other.end());
    std::vector<ImportedProgram> programs;std::string error;
    CHECK(decodePresetBank(bank,programs,error));CHECK(programs.size()==2);
    CHECK(programs[0].original==one);CHECK(programs[1].original==other);
    CHECK(programs[0].state.patch.rawPayload==state.patch.rawPayload);
    CHECK(programs[1].state.patch.bank==3&&programs[1].state.patch.program==5);
    const auto baseline=musicalPresetBytes(state);
    auto globals=state;globals.extras={3,2,1,1,1,1,1,1};globals.masterOutputDb=-60;globals.midiInputChannel=7;
    CHECK(musicalPresetBytes(globals)==baseline);
    globals.character.tube[0]=.123f;CHECK(musicalPresetBytes(globals)!=baseline);
    globals=state;globals.patch.rawPayload[1023]^=1;CHECK(musicalPresetBytes(globals)!=baseline);
    for(std::size_t size=1;size<one.size();++size) {
        CHECK(!decodePresetBank({one.data(),size},programs,error));CHECK(programs.size()==2&&programs[0].original==one);
    }
    auto invalid=bank;invalid.push_back(0x22);CHECK(!decodePresetBank(invalid,programs,error));CHECK(programs.size()==2);
    invalid=bank;invalid[one.size()+5]=4;CHECK(!decodePresetBank(invalid,programs,error));
    invalid=one;invalid[50]=0x80;CHECK(!decodePresetBank(invalid,programs,error));
    const auto edit=p6::SysexCodec::encodeEditBuffer(state.patch);CHECK(decodePresetBank(edit,programs,error));CHECK(programs[0].state.patch.bank==-1);CHECK(programs[0].original==edit);
    std::cout<<"Preset bank: transactional parsing, frame/payload preservation, duplicate/truncation rejection and musical dirty signature verified\n";
}
