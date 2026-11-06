#include "ui/fx/CharacterVisualDynamics.h"

#include <array>
#include <cassert>
#include <cmath>
#include <limits>
#include <iostream>

int main()
{
    using p6plugin::ui::CharacterVisualDynamics;
    CharacterVisualDynamics dynamics;
    const std::array<float,3> tap{{.02f,.05f,.03f}};
    const auto idle=dynamics.frame();
    assert(idle.heater==0.f && idle.lamp==0.f && idle.ldr==0.f);
    dynamics.advance(true,true,.25f,.09f,tap,.8f);
    const auto active=dynamics.frame();
    assert(active.heater>.05f);
    assert(active.springEnergy[0]>0.f && active.springEnergy[1]>0.f);
    assert(active.springShift[0]!=0.f || active.springShift[1]!=0.f);
    assert(active.lamp>0.f && active.ldr>0.f && active.ldr<active.lamp);
    // Frozen visuals must be exactly unchanged even when the DSP changes.
    dynamics.advance(true,false,0.f,0.f,{},0.f);
    const auto paused=dynamics.frame();
    assert(paused.heater==active.heater);
    assert(paused.springShift==active.springShift);
    assert(paused.springEnergy==active.springEnergy);
    assert(paused.lamp==active.lamp && paused.ldr==active.ldr);
    // Sustained, real tap energy causes ongoing displacement at a 30-Hz UI
    // cadence even when the RMS itself is momentarily constant.
    dynamics.advance(true,true,.25f,.09f,tap,.8f);
    const auto moving=dynamics.frame();
    assert(moving.springShift != active.springShift);
    // The coil tail remains visible momentarily but must decay once the
    // actual pickups stop ringing; no fake constant idle movement.
    for(int i=0;i<150;++i)
        dynamics.advance(true,true,0.f,0.f,{},0.f);
    const auto quiet=dynamics.frame();
    assert(quiet.heater<.0001f);
    assert(quiet.springEnergy[0]<.0001f && quiet.springEnergy[1]<.0001f);
    assert(std::abs(quiet.springShift[0])<.0005f && std::abs(quiet.springShift[1])<.0005f);
    assert(quiet.lamp<.0001f && quiet.ldr<.0001f);
    // All three measured DSP taps matter, without painting a third spring.
    dynamics.reset();
    for(int i=0;i<10;++i)dynamics.advance(true,true,0.f,0.f,{.11f,0.f,0.f},0.f);
    assert(dynamics.frame().springEnergy[0] > dynamics.frame().springEnergy[1]);
    dynamics.reset();
    for(int i=0;i<10;++i)dynamics.advance(true,true,0.f,0.f,{0.f,0.f,.11f},0.f);
    assert(dynamics.frame().springEnergy[1] > dynamics.frame().springEnergy[0]);
    // NaNs in a telemetry handoff never reach a graphics transform.
    const float nan=std::numeric_limits<float>::quiet_NaN();
    dynamics.advance(true,true,nan,nan,{nan,nan,nan},nan);
    const auto finite=dynamics.frame();
    assert(std::isfinite(finite.heater));
    assert(std::isfinite(finite.springShift[0]));
    assert(std::isfinite(finite.springShift[1]));
    assert(std::isfinite(finite.lamp) && std::isfinite(finite.ldr));
    // Visual RESPONSE is real: a high-response lamp heats more quickly,
    // while the LDR still lags behind it. Both are visual-only state.
    CharacterVisualDynamics slowPhoto,fastPhoto;
    slowPhoto.advance(true,true,0.f,0.f,{},1.f,0.f);
    fastPhoto.advance(true,true,0.f,0.f,{},1.f,1.f);
    assert(fastPhoto.frame().lamp>slowPhoto.frame().lamp);
    assert(fastPhoto.frame().ldr>slowPhoto.frame().ldr);
    assert(fastPhoto.frame().ldr<fastPhoto.frame().lamp);
    assert(slowPhoto.frame().ldr<slowPhoto.frame().lamp);
    for(int step=0;step<5;++step)
    {
        slowPhoto.advance(true,true,0.f,0.f,{},1.f,0.f);
        fastPhoto.advance(true,true,0.f,0.f,{},1.f,1.f);
    }
    assert(fastPhoto.frame().lamp>slowPhoto.frame().lamp);
    // A busy DAW may deliver 30Hz, 60Hz or 120Hz visual frames. Same
    // measured energy over the same real second must produce the same
    // amplitude, envelope and phase independently of refresh rate.
    CharacterVisualDynamics fps30,fps120;
    const std::array<float,3> steady{{.03f,.05f,.08f}};
    for(int tick=0;tick<30;++tick)
        fps30.advance(true,true,.20f,.07f,steady,.65f,.6f,1.f/30.f);
    for(int tick=0;tick<120;++tick)
        fps120.advance(true,true,.20f,.07f,steady,.65f,.6f,1.f/120.f);
    const auto a=fps30.frame(),b=fps120.frame();
    assert(std::abs(a.heater-b.heater)<.0001f);
    assert(std::abs(a.springEnergy[0]-b.springEnergy[0])<.0001f);
    assert(std::abs(a.springEnergy[1]-b.springEnergy[1])<.0001f);
    assert(std::abs(a.springShift[0]-b.springShift[0])<.001f);
    assert(std::abs(a.springShift[1]-b.springShift[1])<.001f);
    assert(std::abs(a.lamp-b.lamp)<.0001f);
    // An exceptionally long UI stall is capped instead of jumping forward.
    CharacterVisualDynamics delayed,reference;
    delayed.advance(true,true,.20f,.07f,steady,.65f,.6f,20.f);
    reference.advance(true,true,.20f,.07f,steady,.65f,.6f,.1f);
    assert(std::abs(delayed.frame().heater-reference.frame().heater)<.00001f);
    assert(std::abs(delayed.frame().lamp-reference.frame().lamp)<.00001f);
    // Bypass works even while motion is paused, clearing stored energy.
    dynamics.advance(false,false,.9f,.9f,{.9f,.9f,.9f},1.f);
    const auto bypass=dynamics.frame();
    assert(bypass.heater==0.f && bypass.lamp==0.f && bypass.ldr==0.f);
    assert(bypass.springEnergy[0]==0.f && bypass.springEnergy[1]==0.f);
    assert(bypass.springShift[0]==0.f && bypass.springShift[1]==0.f);
    std::cout<<"Character visual dynamics: audio correlation, two coils / three taps, "
                "pause, tail decay, NaN safety and bypass passed\n";
}
