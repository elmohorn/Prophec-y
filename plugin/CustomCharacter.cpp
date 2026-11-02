#include "CustomCharacter.h"
#include "p6/P6ReverbEffects.h"
#include "Core/Oversampling.h"
#include "Source/Effects/wavefolder.h"
#include "Source/Filters/svf.h"
#include "Source/Utility/dcblock.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
namespace p6plugin {
namespace {
float gain(float db){return std::pow(10.f,db/20.f);}
float pole(float hz,double rate){return 1.f-std::exp(static_cast<float>(-6.283185307179586*hz/rate));}
float bounded(double x){return std::isfinite(x)?static_cast<float>(32.*std::tanh(x/32.)):0.f;}
}
struct CustomCharacter::Impl {
    dspark::Oversampling<float,2> os{2,dspark::Oversampling<float,2>::Quality::High};
    p6::P6ReverbEffects spring;
    struct Channel {
        daisysp::Svf filter;daisysp::DcBlock dc;daisysp::Wavefolder folder;
        std::array<float,6> dispersion{};
        float envelope{},conduction{},tone{},hold{};bool lit{};
    };
    std::array<Channel,2> channels;
    std::array<p6::StereoSample,512> pad{};std::size_t write{};int delay{};std::uint32_t tick{},scopeTick{};
    std::array<float,7> current{};bool first{true};double rate{48000},highRate{96000};
    float springDecay{},springDamping{};
    float parameterAlpha{},meterAlpha{},drive{},threshold{},holdSamples{},toneAlpha{},output{},folds{},symmetry{},bias{},responseAlpha{};
    std::array<double,3> tapPower{};
    float conductionMean{},foldMean{},actualCutoff{};
    std::array<float,32> input{},wet{};std::size_t scope{};
};
CustomCharacter::CustomCharacter():p_(std::make_unique<Impl>()){}
CustomCharacter::~CustomCharacter()=default;
void CustomCharacter::prepare(double rate,int latency)
{
    auto& p=*p_;p.rate=std::max(rate,8000.);p.highRate=p.rate*2.;p.os.prepare({p.rate,1,2});p.spring.prepare(p.highRate);
    p.delay=std::max(0,latency-p.os.getLatency());p.parameterAlpha=1.f-std::exp(static_cast<float>(-1./(.02*p.highRate)));p.meterAlpha=1.f-std::exp(static_cast<float>(-1./(.02*p.highRate)));reset();
}
void CustomCharacter::reset() noexcept
{
    auto& p=*p_;p.os.reset();p.spring.invalidateRealtimeState();p.pad.fill({});p.write=0;p.tick=0;p.scopeTick=0;p.first=true;p.tapPower={};p.conductionMean=p.foldMean=p.actualCutoff=0;p.input={};p.wet={};p.scope=0;
    for(auto& c:p.channels){c.filter.Init(static_cast<float>(p.highRate));c.filter.SetDrive(.25f);c.dc.Init(static_cast<float>(p.highRate));c.folder.Init();c.dispersion={};c.envelope=c.conduction=c.tone=c.hold=0;c.lit=false;}
}
float CustomCharacter::neonTransfer(float x,const std::array<float,7>& v) noexcept
{
    daisysp::Wavefolder folder;folder.Init();const float asym=(v[2]*2.f-1.f)*.7f;
    folder.SetGain(gain(24.f*v[0])*(1.f+7.f*v[1])*(x>=0?1.f+asym:1.f-asym));folder.SetOffset((v[3]*2.f-1.f)*.35f);
    return folder.Process(std::clamp(x,-32.f,32.f));
}
p6::StereoSample CustomCharacter::process(p6::StereoSample input,int type,const std::array<float,7>& values) noexcept
{
    auto& p=*p_;if(p.first){p.current=values;p.first=false;}
    float left=bounded(input.left),right=bounded(input.right);float* pointers[]{&left,&right};
    dspark::AudioBufferView<float> block(pointers,2,1);auto up=p.os.upsample(block);
    for(int n=0;n<up.getNumSamples();++n) {
        for(std::size_t i=0;i<7;++i)p.current[i]+=p.parameterAlpha*(values[i]-p.current[i]);
        const auto& v=p.current;
        if(p.tick++%16==0) {
            p.springDecay=v[1];p.springDamping=1.f-v[4];
            p.drive=type==3?gain(-12.f+24.f*v[0]):type==5?gain(24.f*v[0]):gain(36.f*v[0]);
            p.threshold=.015f+.735f*v[1];p.holdSamples=static_cast<float>(p.highRate)*(.00005f+.00995f*v[2]);
            p.toneAlpha=pole(500.f*std::pow(36.f,v[4]),p.highRate);p.output=gain(-24.f+36.f*v[6]);
            p.folds=1.f+7.f*v[1];p.symmetry=(2.f*v[2]-1.f)*.7f;p.bias=(2.f*v[3]-1.f)*.35f;
            p.responseAlpha=1.f-std::exp(static_cast<float>(-1./(p.highRate*(.01+.29*v[3]))));
            if(type==4)for(auto& c:p.channels)c.filter.SetRes(.1f+.8f*v[1]);
        }
        std::array<float,2> samples{up.getChannel(0)[n],up.getChannel(1)[n]};
        if(type==3) {
            // Existing spring FDN plus a six-stage dispersive input network.
            // Three real internal delay-line taps become optical pickups.
            for(std::size_t c=0;c<2;++c)for(auto& z:p.channels[c].dispersion){const float a=.15f+.65f*v[2],y=a*samples[c]+z;z=samples[c]-a*y;samples[c]=y;}
            (void)p.spring.process(std::tanh(samples[0]*p.drive),std::tanh(samples[1]*p.drive),p6::P6ReverbEffects::Type::Spring,p.springDecay,p.springDamping);
            const auto& taps=p.spring.pickupTaps();
            for(std::size_t i=0;i<3;++i)p.tapPower[i]+=p.meterAlpha*(taps[i]*taps[i]-p.tapPower[i]);
            const float pickup=v[3];samples={static_cast<float>((1.-pickup)*taps[0]+pickup*taps[1]-.4*taps[2]),static_cast<float>((1.-pickup)*taps[2]+pickup*taps[1]-.4*taps[0])};
            for(std::size_t c=0;c<2;++c)samples[c]=p.channels[c].dc.Process(std::tanh(1.5f*samples[c]))*p.output;
        } else for(std::size_t c=0;c<2;++c) {
            auto& channel=p.channels[c];const float x=samples[c];float y=x;
            if(type==2) {
                const float voltage=x*p.drive;
                if(std::abs(voltage)>p.threshold){channel.lit=true;channel.hold=p.holdSamples;}
                else if(std::abs(voltage)<p.threshold*.65f){channel.hold=std::max(0.f,channel.hold-1.f);if(channel.hold==0)channel.lit=false;}
                const float target=channel.lit?1.f:0.f;channel.conduction+=.04f*(target-channel.conduction);
                const float character=v[3],offset=(character-.5f)*.2f;
                y=channel.conduction*(std::tanh((2.f+8.f*character)*(voltage+offset))-std::tanh((2.f+8.f*character)*offset));
                channel.tone+=p.toneAlpha*(y-channel.tone);y=channel.dc.Process(channel.tone)*p.output;
                if(c==0)p.conductionMean+=p.meterAlpha*(target-p.conductionMean);
            } else if(type==4) {
                const float a=std::abs(x)>channel.envelope?.01f:p.responseAlpha;channel.envelope+=a*(std::abs(x)-channel.envelope);
                if(p.tick%16==1) {
                    const float base=20.f*std::pow(900.f,v[0]);const float cutoff=std::clamp(base*std::pow(2.f,6.f*v[2]*(2.f*v[4]-1.f)*std::min(channel.envelope*4.f,1.f)),20.f,std::min(18000.f,static_cast<float>(p.highRate/3.)));
                    channel.filter.SetFreq(cutoff);if(c==0)p.actualCutoff=cutoff;
                }
                channel.filter.Process(std::tanh(1.5f*x));y=channel.dc.Process(channel.filter.Low())*p.output;
            } else if(type==5) {
                const float g=p.drive*p.folds*(x>=0?1.f+p.symmetry:1.f-p.symmetry);
                channel.folder.SetGain(g);channel.folder.SetOffset(p.bias);y=channel.folder.Process(x);
                if(c==0){const float folding=std::abs((x+p.bias)*g)>1.f?1.f:0.f;p.foldMean+=p.meterAlpha*(folding-p.foldMean);}
                channel.tone+=p.toneAlpha*(y-channel.tone);y=channel.dc.Process(channel.tone)*p.output;
            }
            samples[c]=std::isfinite(y)?y:0.f;
        }
        up.getChannel(0)[n]=samples[0];up.getChannel(1)[n]=samples[1];
    }
    p.os.downsample(block);
    const auto index=(p.write+p.pad.size()-static_cast<std::size_t>(p.delay))%p.pad.size();
    const auto out=p.delay>0?p.pad[index]:p6::StereoSample{left,right};p.pad[p.write]={left,right};p.write=(p.write+1)%p.pad.size();
    // Decimate only the UI history: 32 points span 248 base-rate samples.
    if(p.scopeTick++%8==0){p.input[p.scope]=static_cast<float>(input.left);p.wet[p.scope]=static_cast<float>(out.left);p.scope=(p.scope+1)%p.input.size();}
    return out;
}
DeviceTelemetry CustomCharacter::telemetry() const noexcept
{
    const auto& p=*p_;DeviceTelemetry t;
    t.values={p.conductionMean,p.threshold,static_cast<float>(std::sqrt(p.tapPower[0])),static_cast<float>(std::sqrt(p.tapPower[1])),static_cast<float>(std::sqrt(p.tapPower[2])),p.actualCutoff,p.channels[0].envelope,p.foldMean};
    for(std::size_t i=0;i<32;++i){const auto k=(p.scope+i)%32;t.input[i]=p.input[k];t.wet[i]=p.wet[k];}return t;
}
}
