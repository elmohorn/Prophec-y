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
    // Bypass works even while motion is paused, clearing stored energy.
    dynamics.advance(false,false,.9f,.9f,{.9f,.9f,.9f},1.f);
    const auto bypass=dynamics.frame();
    assert(bypass.heater==0.f && bypass.lamp==0.f && bypass.ldr==0.f);
    assert(bypass.springEnergy[0]==0.f && bypass.springEnergy[1]==0.f);
    assert(bypass.springShift[0]==0.f && bypass.springShift[1]==0.f);
    std::cout<<"Character visual dynamics: audio correlation, two coils / three taps, "
                "pause, tail decay, NaN safety and bypass passed\n";
}
