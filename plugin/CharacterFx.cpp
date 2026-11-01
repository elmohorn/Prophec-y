#include "CharacterFx.h"
#include "Effects/TubePreamp.h"
#include <algorithm>
#include <cmath>

namespace p6plugin {
struct CharacterFx::Impl {
    dspark::TubePreamp<float> tube;
    CharacterState requested{}, active{};
    std::array<p6::StereoSample, 512> dry{};
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
    p.delay=p.tube.getLatency();
    p.dry.fill({}); p.write=0; p.gain=1; p.blend=0; p.hold=0; p.meter=0;
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
    p.requested.type=std::clamp(state.type,0,1);
    p.requested.position=std::clamp(state.position,0,1);
    for(std::size_t i=0;i<state.tube.size();++i)
        p.requested.tube[i]=std::isfinite(state.tube[i]) ? std::clamp(state.tube[i],0.f,1.f) : kTubeDefaults[i];
    if(p.initial) {
        p.active=p.requested;
        if(p.requested.type==0 || p.requested.bypass || p.requested.tube[5]==0.f) p.active.position=1;
        p.initial=false;
    }
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
    const int desired=(p.requested.type==1 && !p.requested.bypass && p.requested.tube[5]>0.f)
        ? p.requested.position : 1;
    if(desired!=p.active.position) {
        p.gain=std::max(0.f,p.gain-p.step);
        if(p.gain==0.f) {
            p.active.position=desired;
            p.tube.reset(); p.dry.fill({}); p.write=0; p.blend=0;
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
    const bool enabled=p.requested.type==1 && !p.requested.bypass;
    const float target=enabled?p.requested.tube[5]:0.f;
    // 10 ms linear crossfade, aligned to the wet latency. Finish the fade
    // before suspending the tube; reset on the stream thread before reuse.
    if(enabled && p.blend==0.f && target>0.f) p.tube.reset();
    p.blend+=std::clamp(target-p.blend,-p.step,p.step);
    p6::StereoSample wet=dry;
    if(enabled || p.blend>0.f) {
        float l=static_cast<float>(x.left),r=static_cast<float>(x.right);
        float* channels[]{&l,&r};
        p.tube.processBlock(dspark::AudioBufferView<float>(channels,2,1));
        wet={l,r};
    }
    p.meter=std::max(static_cast<float>(std::max(std::abs(x.left),std::abs(x.right))), p.meter*.9995f);
    const p6::StereoSample out{dry.left+(wet.left-dry.left)*p.blend, dry.right+(wet.right-dry.right)*p.blend};
    const double dl=out.left-dry.left, dr=out.right-dry.right;
    p.inputPower+=p.meterAlpha*(.5*(x.left*x.left+x.right*x.right)-p.inputPower);
    p.outputPower+=p.meterAlpha*(.5*(out.left*out.left+out.right*out.right)-p.outputPower);
    p.changePower+=p.meterAlpha*(.5*(dl*dl+dr*dr)-p.changePower);
    return out;
}
bool CharacterFx::beforeEffects() const noexcept { return impl_->active.position==0; }
float CharacterFx::routeGain() const noexcept { return impl_->gain; }
int CharacterFx::latency() const noexcept { return impl_->delay; }
float CharacterFx::excitation() const noexcept { return impl_->requested.type==1&&!impl_->requested.bypass ? impl_->meter*impl_->blend : 0.f; }
CharacterTelemetry CharacterFx::telemetry() const noexcept
{
    const auto& p=*impl_;
    return {static_cast<float>(std::sqrt(std::max(0.,p.inputPower))),
            static_cast<float>(std::sqrt(std::max(0.,p.outputPower))),
            static_cast<float>(std::sqrt(std::max(0.,p.changePower))),
            p.ready&&p.requested.type==1&&!p.requested.bypass&&p.requested.tube[5]>0.f};
}
} // namespace p6plugin

