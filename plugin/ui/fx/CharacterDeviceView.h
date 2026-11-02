#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "../../CharacterFx.h"
// Native drawing only. No timer or animation on the audio thread.
class CharacterDeviceView final : public juce::Component {
public:
    CharacterDeviceView();
    void update(int,const std::array<float,7>&,const p6plugin::CharacterTelemetry&,bool motion);
    void paint(juce::Graphics&) override;
private:
    int type_{2};std::array<float,7> parameters_{};
    p6plugin::CharacterTelemetry reading_{};
    float animationPhase_{};
};
