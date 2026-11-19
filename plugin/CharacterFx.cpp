#include "CharacterFx.h"
#include "CustomCharacter.h"
#include "adapter.h"
#include "Effects/TubePreamp.h"
#include <algorithm>
#include <cmath>

namespace p6plugin {
struct CharacterFx::Impl {
    dspark::TubePreamp<float> tube;
    CustomCharacter custom;
    RotarySetBfree rotary; bool rotaryReady{}; float wheel{},rotaryGain{1.f}; int rotarySpeedMode{-1};
    CharacterState requested{}, active{};
    std::array<p6::StereoSample, 512> dry{}, rotaryWetDelay{};
    std::size_t write{};
    int delay{}, fadeSamples{480}, hold{};
    float gain{1.f}, blend{}, meter{};
    bool ready{}, initial{true}, parametersInitial{true};
    std::array<float,7> lastParameters{};
    float step{1.f/480.f};
    double inputPower{}, outputPower{}, changePower{}, meterAlpha{};
};
CharacterFx::CharacterFx() : impl_(std::make_unique<Impl>()) {}
CharacterFx::~CharacterFx() = default;
void CharacterFx::prepare(double sr)
{
    auto& p=*impl_;
    p.tube.setStages(1);
    p.tube.setOversampling(2);
    p.tube.prepare({sr, 1, 2});
    p.delay=p.tube.getLatency();p.custom.prepare(sr,p.delay); p.rotaryReady=p.rotary.prepare(sr);p.rotarySpeedMode=-1;
    p.dry.fill({}); p.rotaryWetDelay.fill({}); p.write=0; p.gain=1; p.blend=0; p.hold=0; p.meter=0;
    p.fadeSamples=std::max(1, static_cast<int>(sr*.01));
    p.step=1.f/static_cast<float>(p.fadeSamples);
    p.ready=true; p.initial=true; p.parametersInitial=true;
    p.inputPower=p.outputPower=p.changePower=0.;
    p.meterAlpha=1.-std::exp(-1./(sr*.02));
}
void CharacterFx::setState(const CharacterState& state) noexcept
{
    auto& p=*impl_;
    p.requested=state;
    p.requested.type=std::clamp(state.type,0,kCharacterTypes-1);
    p.requested.position=std::clamp(state.position,0,1);
    p.requested.plasmaMode=std::clamp(state.plasmaMode,0,1);
    for(std::size_t i=0;i<state.tube.size();++i)
        p.requested.tube[i]=std::isfinite(state.tube[i]) ? std::clamp(state.tube[i],0.f,1.f) : kTubeDefaults[i];
    for(std::size_t d=0;d<4;++d)for(std::size_t i=0;i<7;++i)p.requested.custom[d][i]=std::isfinite(state.custom[d][i])?std::clamp(state.custom[d][i],0.f,1.f):kCustomDefaults[d][i];
    for(size_t i=0;i<5;++i)p.requested.rotary[i]=std::isfinite(state.rotary[i])?std::clamp(state.rotary[i],0.f,1.f):kRotaryDefaults[i];
    const auto& selected=p.requested.type>=2 && p.requested.type<=5?p.requested.custom[static_cast<std::size_t>(p.requested.type-2)]:p.requested.tube;
    if(p.initial) {
        p.active=p.requested;
        if(p.requested.type==0 || p.requested.bypass || (p.requested.type==6?p.requested.rotary[3]:selected[5])==0.f) p.active.position=1;
        p.initial=false;
    }
    p.rotaryGain=std::pow(10.f,(-12.f+24.f*p.requested.rotary[4])/20.f);
    if(p.requested.type==6){const float speed=p.requested.rotary[2];const int mode=speed<.333f?0:speed<.666f?1:2; if(mode!=p.rotarySpeedMode){p.rotary.setSpeed(mode==0?RotarySetBfree::Slow:mode==1?RotarySetBfree::Stop:RotarySetBfree::Fast);p.rotarySpeedMode=mode;}}
    const auto& v=p.requested.tube;
    if(p.parametersInitial || v[0]!=p.lastParameters[0]) p.tube.setDrive(-12.f+48.f*v[0]);
    if(p.parametersInitial || v[1]!=p.lastParameters[1]) p.tube.setSag(v[1]);
    if(p.parametersInitial || v[2]!=p.lastParameters[2]) p.tube.setBass(v[2]);
    if(p.parametersInitial || v[3]!=p.lastParameters[3]) p.tube.setMiddle(v[3]);
    if(p.parametersInitial || v[4]!=p.lastParameters[4]) p.tube.setTreble(v[4]);
    p.tube.setMix(1.f); p.tube.setOutput(-24.f+36.f*v[6]);
    p.lastParameters=v;p.parametersInitial=false;
}
bool CharacterFx::advanceRoute() noexcept
{
    auto& p=*impl_;
    // None uses a final pure delay: legacy time-varying FX see their original
    // input. Each route contains exactly one delay, independent of selection.
    const auto& selected=p.requested.type>=2 && p.requested.type<=5?p.requested.custom[static_cast<std::size_t>(p.requested.type-2)]:p.requested.tube;
    const int desired=(p.requested.type>0 && !p.requested.bypass && (p.requested.type==6?p.requested.rotary[3]:selected[5])>0.f)
        ? p.requested.position : 1;
    if(desired!=p.active.position || p.active.type!=p.requested.type ||
       (p.active.type==2 && p.active.plasmaMode!=p.requested.plasmaMode)) {
        p.gain=std::max(0.f,p.gain-p.step);
        if(p.gain==0.f) {
            p.active.position=desired;p.active.type=p.requested.type;p.active.plasmaMode=p.requested.plasmaMode;
            p.tube.reset();p.custom.reset(); p.dry.fill({}); p.rotaryWetDelay.fill({}); p.write=0; p.blend=0;
            p.hold=p.delay;
            return true;
        }
    } else if(p.hold>0) --p.hold;
    else p.gain=std::min(1.f,p.gain+p.step);
    return false;
}
p6::StereoSample CharacterFx::process(p6::StereoSample x) noexcept
{
    auto& p=*impl_;
    if(!p.ready) return x;
    const auto index=(p.write+p.dry.size()-static_cast<std::size_t>(p.delay))%p.dry.size();
    const auto dry=p.dry[index]; p.dry[p.write]=x; p.write=(p.write+1)%p.dry.size();
    const auto& selected=p.active.type>=2 && p.active.type<=5?p.requested.custom[static_cast<std::size_t>(p.active.type-2)]:p.requested.tube;
    const bool enabled=p.active.type>0 && !p.requested.bypass;
    const float target=enabled?(p.active.type==6?p.requested.rotary[3]:selected[5]):0.f;
    // 10 ms linear crossfade, aligned to the wet latency. Finish the fade
    // before suspending the tube; reset on the stream thread before reuse.
    if(enabled && p.blend==0.f && target>0.f){p.tube.reset();p.custom.reset();if(p.active.type==6){p.rotary.resetAudio();p.rotaryWetDelay.fill({});}}
    p.blend+=std::clamp(target-p.blend,-p.step,p.step);
    p6::StereoSample wet=dry;
    if(enabled || p.blend>0.f) {
        if(p.active.type==6) {
            if(p.rotaryReady){
                float mono=static_cast<float>((x.left+x.right)*.5),l=0,r=0;
                p.rotary.setMotion(p.requested.rotary[0]); p.rotary.setWheelDepth(p.requested.rotary[1]); p.rotary.setModWheel(p.wheel);
                p.rotary.process(&mono,&l,&r,1);
                // Preserve the input side signal and add the wrapper delay to
                // the speaker model; physical rotor propagation remains variable.
                const auto delayed=p.rotaryWetDelay[index];
                const auto prev=(p.write+p.rotaryWetDelay.size()-1)%p.rotaryWetDelay.size();
                p.rotaryWetDelay[prev]={l,r};
                const double side=(dry.left-dry.right)*.5;
                wet={(delayed.left+side)*p.rotaryGain,(delayed.right-side)*p.rotaryGain};
            }
        }
        else if(p.active.type>=2)wet=p.custom.process(x,p.active.type,selected,p.active.plasmaMode);
        else {
        float l=static_cast<float>(x.left),r=static_cast<float>(x.right);
        float* channels[]{&l,&r};
        p.tube.processBlock(dspark::AudioBufferView<float>(channels,2,1));
        wet={l,r};
        }
    }
    p.meter=std::max(static_cast<float>(std::max(std::abs(x.left),std::abs(x.right))), p.meter*.9995f);
    const p6::StereoSample out{dry.left+(wet.left-dry.left)*p.blend, dry.right+(wet.right-dry.right)*p.blend};
    const double dl=out.left-dry.left, dr=out.right-dry.right;
    p.inputPower+=p.meterAlpha*(.5*(x.left*x.left+x.right*x.right)-p.inputPower);
    p.outputPower+=p.meterAlpha*(.5*(out.left*out.left+out.right*out.right)-p.outputPower);
    p.changePower+=p.meterAlpha*(.5*(dl*dl+dr*dr)-p.changePower);
    return out;
}
void CharacterFx::setModWheel(float x) noexcept {impl_->wheel=std::isfinite(x)?std::clamp(x,0.f,1.f):0.f;}
bool CharacterFx::beforeEffects() const noexcept { return impl_->active.position==0; }
float CharacterFx::routeGain() const noexcept { return impl_->gain; }
int CharacterFx::latency() const noexcept { return impl_->delay; }
float CharacterFx::excitation() const noexcept { return impl_->requested.type>0&&!impl_->requested.bypass ? impl_->meter*impl_->blend : 0.f; }
CharacterTelemetry CharacterFx::telemetry() const noexcept
{
    const auto& p=*impl_;
    auto device=p.custom.telemetry();
    if(p.active.type==6){device.values[0]=static_cast<float>(p.rotary.hornAngle());device.values[1]=static_cast<float>(p.rotary.drumAngle());}
    return {static_cast<float>(std::sqrt(std::max(0.,p.inputPower))),
            static_cast<float>(std::sqrt(std::max(0.,p.outputPower))),
            static_cast<float>(std::sqrt(std::max(0.,p.changePower))),
            p.ready&&p.active.type>0&&!p.requested.bypass&&p.blend>0.f,
            p.active.type,device};
}
} // namespace p6plugin

