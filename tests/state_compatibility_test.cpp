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
    for(int version:{17,18,19,20,21,22}) {
        const auto path=std::string(P6_STATE_FIXTURE_DIR)+(version==22?"/v020_codec4.state":version==21?"/v020_codec3.state":"/v0."+std::to_string(version)+".state");
        std::ifstream in(path,std::ios::binary);CHECK(in.good());
        const std::vector<std::uint8_t> bytes{std::istreambuf_iterator<char>(in),std::istreambuf_iterator<char>()};
        CHECK(bytes.size()==(version==22?p6plugin::kV4StateBytes:version==21?p6plugin::kV3StateBytes:version==20?p6plugin::kV2StateBytes:p6plugin::kV1StateBytes));
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
        CHECK(version>=21?std::abs(state.masterOutputDb+23.4f)<.001f:state.masterOutputDb==0.f);
        CHECK(state.midiInputChannel==0); // v1-v4 migrate to Omni
        CHECK(state.character.plasmaMode==0); // historical sessions preserve Legacy
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
    // v3 master/Tubes survive migration; v4 preserves all inactive modules.
    p6plugin::WrapperState custom;custom.masterOutputDb=-23.4f;
    for(std::size_t d=0;d<4;++d)for(std::size_t i=0;i<7;++i)custom.character.custom[d][i]=static_cast<float>(d*7+i)/100.f;
    for(int type=0;type<6;++type){custom.character.type=type;custom.character.legacyType=1;auto bytes=p6plugin::encodeState(custom);p6plugin::WrapperState decoded;CHECK(p6plugin::decodeState(bytes,decoded));CHECK(decoded.character.type==type);CHECK(decoded.character.legacyType==(type<2?type:1));CHECK(decoded.character.custom==custom.character.custom);}
    custom.character.type=1;auto bytes=p6plugin::encodeState(custom);bytes[4]=3;bytes.resize(p6plugin::kV3StateBytes);
    p6plugin::WrapperState decoded;CHECK(p6plugin::decodeState(bytes,decoded));CHECK(decoded.character.type==1);CHECK(std::abs(decoded.masterOutputDb+23.4f)<.001f);CHECK(decoded.character.custom==p6plugin::kCustomDefaults);
    // The v5 layout has no Plasma algorithm byte and must migrate to Legacy.
    p6plugin::WrapperState plasma;plasma.character.type=2;plasma.character.plasmaMode=1;
    const auto v6=p6plugin::encodeState(plasma);CHECK(p6plugin::decodeState(v6,decoded));CHECK(decoded.character.plasmaMode==1);
    auto v5=v6;v5[4]=5;v5.resize(p6plugin::kV5StateBytes);CHECK(p6plugin::decodeState(v5,decoded));CHECK(decoded.character.type==2&&decoded.character.plasmaMode==0);
    const auto migrated=p6plugin::encodeState(decoded);p6plugin::WrapperState reopened;CHECK(p6plugin::decodeState(migrated,reopened));CHECK(reopened.character.plasmaMode==0);
    auto invalid=v6;invalid[p6plugin::kV5StateBytes]=2;CHECK(!p6plugin::decodeState(invalid,reopened));CHECK(reopened.character.plasmaMode==0);
    // Independent fixture emitted by the unchanged codec v5 implementation.
    std::ifstream v5file(std::string(P6_STATE_FIXTURE_DIR)+"/v020_codec5_plasma.state",std::ios::binary);CHECK(v5file.good());
    const std::vector<std::uint8_t> oldPlasma{std::istreambuf_iterator<char>(v5file),std::istreambuf_iterator<char>()};
    CHECK(oldPlasma.size()==p6plugin::kV5StateBytes);CHECK(p6plugin::decodeState(oldPlasma,reopened));
    CHECK(reopened.character.type==2&&reopened.character.plasmaMode==0&&reopened.midiInputChannel==7);CHECK(std::abs(reopened.character.custom[0][5]-.8f)<1.e-6f);
    std::cout<<"Historical encoder fixtures v0.17-v0.20: payload, wrapper, migration and rejection verified\n";
}
