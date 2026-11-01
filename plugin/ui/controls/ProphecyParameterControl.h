#pragma once
#include "../ParameterPresentation.h"
#include <juce_audio_processors/juce_audio_processors.h>
#include <functional>

// First common control: a Slider keeps JUCE's established parameter binding,
// automation and accessibility. The descriptor adds editing/help, not DSP.
class ProphecyParameterControl : public juce::Slider {
public:
    std::function<void()> onSelected;
    void focusGained(juce::Component::FocusChangeType) override;
    using ContextProvider=std::function<p6plugin::ui::ParameterContext()>;
    void configure(juce::RangedAudioParameter&,int nrpn,ContextProvider);
    bool applyExactText(const juce::String&,bool rawInput=false);
    void resetToInit();
    int parameterNrpn() const noexcept { return nrpn_; }
    bool hasDescriptor() const noexcept { return parameter_!=nullptr; }
    void mouseDown(const juce::MouseEvent&) override;
    bool keyPressed(const juce::KeyPress&) override;
    void paint(juce::Graphics&) override;
private:
    void applyRaw(double);
    void showContextMenu();
    void showExactEditor(bool rawInput);
    juce::RangedAudioParameter* parameter_{};
    int nrpn_{};
    ContextProvider context_;
};
