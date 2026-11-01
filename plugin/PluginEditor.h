#pragma once
#include "PluginProcessor.h"
#include <memory>

class P6PluginEditor final : public juce::AudioProcessorEditor, private juce::Timer {
public:
    static constexpr int compactHeight=700, expandedHeight=930;
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
    struct Switch {
        juce::ToggleButton button;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> attachment;
    };
    struct MetalLook final : juce::LookAndFeel_V4 {
        void drawRotarySlider(juce::Graphics&,int,int,int,int,float,float,float,juce::Slider&) override;
        void drawLinearSlider(juce::Graphics&,int,int,int,int,float,float,float,juce::Slider::SliderStyle,juce::Slider&) override;
        void drawToggleButton(juce::Graphics&,juce::ToggleButton&,bool,bool) override;
        juce::Label* createComboBoxTextBox(juce::ComboBox&) override;
    } look_;
    void makeKnob(Knob&,const juce::String&,const juce::String&,bool values=false);
    void timerCallback() override;
    void refreshVisibility();
    void setType(int);
    void paintSurface(juce::Graphics&);
    void paintReadout(juce::Graphics&);
    void paintTube(juce::Graphics&);
    void section(juce::Graphics&,const juce::String&,float,float,float);
    P6AudioProcessor& processor_;
    juce::TextButton module_{"MODULE"},studio_{"STUDIO"},previous_{"<"},next_{">"},collapse_{"HIDE"};
    juce::ToggleButton bypass_{"BYPASS"},motion_{"MOTION"};
    juce::ComboBox type_,position_,keyTrack_,fxAType_,fxBType_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> typeBinding_,positionBinding_,keyBinding_,fxATypeBinding_,fxBTypeBinding_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassBinding_;
    std::array<Knob,37> synth_;
    std::array<Switch,18> switches_;
    std::array<Knob,7> tube_;
    std::unique_ptr<juce::GenericAudioProcessorEditor> diagnostic_;
    juce::Image grain_,tubeGlass_;
    juce::String focusName_{"LOW PASS"},focusValue_{"164 RAW"},presetName_;
    float focusPosition_{1.f},glow_{},characterGlow_{};
    p6plugin::CharacterTelemetry visual_{};
    unsigned lastVisualRevision_{};
    int staleVisualTicks_{};
    bool studioView_{},collapsed_{},showTube_{};
    juce::Rectangle<int> tubeArea_,displayArea_;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(P6PluginEditor)
};
