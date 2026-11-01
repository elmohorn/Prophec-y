#include "PluginStateCodec.h"
#include "p6/SysexCodec.h"
#include <fstream>
#ifndef P6_FIXTURE_SEED
#error P6_FIXTURE_SEED is required
#endif
int main(int argc,char** argv)
{
    if(argc!=2)return 1;
    p6plugin::WrapperState s;s.patch.bank=P6_FIXTURE_SEED-18;s.patch.program=P6_FIXTURE_SEED*7;
    for(std::size_t i=0;i<s.patch.rawPayload.size();++i)s.patch.rawPayload[i]=static_cast<std::uint8_t>((i*73u+P6_FIXTURE_SEED*11u)&255u);
    const std::string name="Historical "+std::to_string(P6_FIXTURE_SEED);
    for(std::size_t i=0;i<p6::SysexCodec::kPatchNameLength;++i)s.patch.rawPayload[p6::SysexCodec::kPatchNameOffset+i]=static_cast<std::uint8_t>(i<name.size()?name[i]:' ');
    s.sequenceControls={1,37,2,3};s.extras={3,2,1,1,0,1,1,1};
#ifdef P6_FIXTURE_CHARACTER
    s.character.type=1;s.character.position=0;s.character.bypass=true;s.character.tube={.123f,.456f,.789f,.1f,.2f,.3f,.9f};
#endif
    const auto bytes=p6plugin::encodeState(s);
    std::ofstream out(argv[1],std::ios::binary);out.write(reinterpret_cast<const char*>(bytes.data()),static_cast<std::streamsize>(bytes.size()));
    return out.good()?0:2;
}
