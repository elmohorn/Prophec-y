#include "OutputStage.h"
#include "PluginStateCodec.h"
#include <iostream>
#include <limits>
#define CHECK(x) do{if(!(x)){std::cerr<<"failed: " #x <<" line "<<__LINE__<<'\n';return 1;}}while(false)
int main()
{
    using namespace p6plugin;
    for(double rate:{44100.,48000.,96000.,192000.}) {
        OutputStage stage;stage.prepare(rate,0);
        auto x=stage.process(.125f,-.75f);CHECK(x[0]==.125f&&x[1]==-.75f);
        stage.setDecibels(-18);float previous=1;
        for(int i=0;i<static_cast<int>(rate*.02);++i){auto y=stage.process(1,-1);CHECK(y[0]<=previous&&y[0]>0);CHECK(y[0]==-y[1]);previous=y[0];}
        const float gain=std::pow(10.f,-18.f/20.f);CHECK(std::abs(previous-gain)<1.e-6f);
        stage.prepare(rate,-18);
        // Measured hottest no-FX diagnostic peak: +16.91 dBFS -> -1.09 dBFS.
        const float hot=std::pow(10.f,16.91f/20.f);x=stage.process(hot,-hot);
        CHECK(std::abs(20*std::log10(x[0])+1.09f)<.001f);
        CHECK(stage.readings().clips==12);CHECK(stage.readings().peak[0]<1);
        stage.prepare(rate,0);stage.process(1.1f,-1.2f);CHECK(stage.readings().clips==15);
        stage.beginBlock();stage.process(.1f,.2f);CHECK(stage.readings().clips==0);
        // RMS is amplitude-correct and independent of block boundaries.
        OutputStage a,b;a.prepare(rate,0);b.prepare(rate,0);
        for(int i=0;i<static_cast<int>(rate);++i){const float sine=std::sin(static_cast<float>(i)*2.f*3.14159265f*1000.f/static_cast<float>(rate));a.process(sine,.5f*sine);if(i%17==0)b.beginBlock();b.process(sine,.5f*sine);}
        CHECK(std::abs(a.readings().rms[0]-.7071f)<.003f);CHECK(a.readings().rms==b.readings().rms);
    }
    WrapperState state;state.masterOutputDb=-23.4f;auto bytes=encodeState(state);
    WrapperState decoded;CHECK(decodeState(bytes,decoded));CHECK(std::abs(decoded.masterOutputDb+23.4f)<1.e-4f);
    for(int version:{1,2}) {
        auto legacy=bytes;legacy[4]=static_cast<std::uint8_t>(version);legacy.resize(version==1?kV1StateBytes:kV2StateBytes);
        decoded.masterOutputDb=-60;CHECK(decodeState(legacy,decoded));CHECK(decoded.masterOutputDb==0);
    }
    auto invalid=bytes;invalid.back()=127;CHECK(!decodeState(invalid,decoded));CHECK(decoded.masterOutputDb==0);
    invalid=bytes;invalid[kV2StateBytes]=1;invalid[kV2StateBytes+1]=0;CHECK(!decodeState(invalid,decoded));
    for(int channel=0;channel<=16;++channel){state.midiInputChannel=static_cast<std::uint8_t>(channel);const auto encoded=encodeState(state);CHECK(decodeState(encoded,decoded));CHECK(decoded.midiInputChannel==channel);}

    for(std::size_t n=0;n<bytes.size();++n)CHECK(!decodeState(std::span<const std::uint8_t>(bytes.data(),n),decoded));
    std::cout<<"Master: unity migration, -18 dB headroom, ramp timing, stereo RMS and codec v3 verified\n";
}
