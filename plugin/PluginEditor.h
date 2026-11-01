#pragma once
#include "PluginProcessor.h"
#include <memory>

// First native surface: focused Module controls plus complete diagnostic
// Studio. The shared parameter attachments are the only edit path.
class P6PluginEditor final : public juce::AudioProcessorEditor, private juce::Timer {
public:
    explicit P6PluginEditor(P6AudioProcessor&);
    ~P6PluginEditor() override;
    void paint(juce::Graphics&) override;
    void resized() override;
private:
    struct Knob {
        juce::Slider slider;
        juce::Label label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    };
    void makeKnob(Knob&,const juce::String& id,const juce::String& label);
    void timerCallback() override;
    void refreshVisibility();
    void setType(int);
    P6AudioProcessor& processor_;
    struct MetalLook final : juce::LookAndFeel_V4 {
        void drawRotarySlider(juce::Graphics&,int,int,int,int,float,float,float,juce::Slider&) override;
    } look_;
    juce::TextButton module_{"MODULE"},studio_{"STUDIO"},previous_{"<"},next_{">"},collapse_{"Collapse"};
    juce::ToggleButton bypass_{"Bypass"},motion_{"Animation"};
    juce::ComboBox type_,position_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> typeBinding_,positionBinding_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassBinding_;
    std::array<Knob,10> synth_;
    std::array<Knob,7> tube_;
    std::unique_ptr<juce::GenericAudioProcessorEditor> diagnostic_;
    bool studioView_{},collapsed_{},showTube_{};
    float glow_{};
    int expandedHeight_{790};
    juce::Rectangle<int> tubeArea_;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(P6PluginEditor)
};
