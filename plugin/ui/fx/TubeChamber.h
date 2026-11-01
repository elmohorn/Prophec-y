#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

// A recessed device window, independent of effect controls and audio processing.
class TubeChamber final : public juce::Component {
public:
    TubeChamber();
    void setIllumination(bool active, bool motion, float inputEnergy, float changeEnergy);
    void paint(juce::Graphics&) override;
private:
    juce::Image interior_,valve_;
    bool active_{},motion_{true};
    float energy_{},change_{};
};
