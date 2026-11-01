#include "PluginStateCodec.h"
#include "p6/SysexCodec.h"
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <cmath>
#define CHECK(x) do{if(!(x)){std::cerr<<"failed: " #x <<" line "<<__LINE__<<'\n';return 1;}}while(false)
int main()
{
    for(int version:{17,18,19,20}) {
        const auto path=std::string(P6_STATE_FIXTURE_DIR)+"/v0."+std::to_string(version)+".state";
        std::ifstream in(path,std::ios::binary);CHECK(in.good());
        const std::vector<std::uint8_t> bytes{std::istreambuf_iterator<char>(in),std::istreambuf_iterator<char>()};
        CHECK(bytes.size()==(version==20?p6plugin::kV2StateBytes:p6plugin::kV1StateBytes));
        p6plugin::WrapperState state;CHECK(p6plugin::decodeState(bytes,state));
        CHECK(state.patch.bank==version-18);CHECK(state.patch.program==version*7);
        const std::string name="Historical "+std::to_string(version);CHECK(state.patch.name==name);
        for(std::size_t i=0;i<1024;++i) {
            const auto expected=i>=p6::SysexCodec::kPatchNameOffset&&i<p6::SysexCodec::kPatchNameOffset+p6::SysexCodec::kPatchNameLength
                ? static_cast<std::uint8_t>(i-p6::SysexCodec::kPatchNameOffset<name.size()?name[i-p6::SysexCodec::kPatchNameOffset]:' ')
                :static_cast<std::uint8_t>((i*73u+static_cast<unsigned>(version)*11u)&255u);
            CHECK(state.patch.rawPayload[i]==expected);
        }
        CHECK((state.sequenceControls==std::array<std::uint16_t,4>{1,37,2,3}));
        CHECK((state.extras==std::array<std::uint8_t,8>{3,2,1,1,0,1,1,1}));
        if(version<20)CHECK(state.character.type==0&&state.character.tube==p6plugin::kTubeDefaults);
        else {
            CHECK(state.character.type==1&&state.character.position==0&&state.character.bypass);
            CHECK(std::abs(state.character.tube[0]-.123f)<1.e-6f);
        }
        CHECK(state.masterOutputDb==0.f);
        const auto upgraded=p6plugin::encodeState(state);
        p6plugin::WrapperState again;CHECK(p6plugin::decodeState(upgraded,again));
        CHECK(again.patch.rawPayload==state.patch.rawPayload);CHECK(again.extras==state.extras);
        CHECK(again.sequenceControls==state.sequenceControls);CHECK(again.character.tube==state.character.tube);
        // Failed reads must leave the caller's existing session untouched.
        for(std::size_t size=0;size<bytes.size();++size) {
            CHECK(!p6plugin::decodeState(std::span<const std::uint8_t>(bytes.data(),size),state));
            CHECK(p6plugin::encodeState(state)==upgraded);
        }
        auto future=bytes;future[4]=99;CHECK(!p6plugin::decodeState(future,state));
        CHECK(p6plugin::encodeState(state)==upgraded);
    }
    std::cout<<"Historical encoder fixtures v0.17-v0.20: payload, wrapper, migration and rejection verified\n";
}
