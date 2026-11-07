#pragma once
#include "CharacterFx.h"
#include <memory>
namespace p6plugin {
class CustomCharacter {
public:
    CustomCharacter();~CustomCharacter();
    void prepare(double sampleRate,int totalLatency);
    void reset() noexcept;
    p6::StereoSample process(p6::StereoSample,int type,const std::array<float,7>&,int plasmaMode=1) noexcept;
    DeviceTelemetry telemetry() const noexcept;
    static float neonTransfer(float input,const std::array<float,7>&) noexcept;
private:
    struct Impl;std::unique_ptr<Impl> p_;
};
}
