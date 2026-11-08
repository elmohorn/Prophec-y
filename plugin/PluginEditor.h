#pragma once
#include "PluginProcessor.h"
#include "ui/controls/ProphecyParameterControl.h"
#include "ui/display/ContextDisplay.h"
#include "ui/fx/TubeChamber.h"
#include "ui/fx/CharacterDeviceView.h"
#include "ui/fx/CharacterHardwareWindow.h"
#include "ui/display/OutputMeter.h"
#include "ui/preset/PresetBrowser.h"
#include <memory>

class P6PluginEditor final : public juce::AudioProcessorEditor, private juce::Timer {
public:
    static constexpr int compactHeight=700, expandedHeight=930;
    explicit P6PluginEditor(P6AudioProcessor&,juce::File presetRoot=p6plugin::PresetLibrary::defaultRoot());
    ~P6PluginEditor() override;
    void paint(juce::Graphics&) override;
    void resized() override;
private:
    struct Knob {
        ProphecyParameterControl slider;
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
        void drawComboBox(juce::Graphics&,int,int,bool,int,int,int,int,juce::ComboBox&) override;
    } look_;
    void makeKnob(Knob&,const juce::String&,const juce::String&,bool values=false);
    void timerCallback() override;
    void refreshVisibility();
    void setType(int);
    void bindDevice(int);
    void updateDevice();
    void paintCharacterPanel(juce::Graphics&);
    void paintSurface(juce::Graphics&);
    void refreshDisplay();
    void paintTube(juce::Graphics&);
    void section(juce::Graphics&,const juce::String&,float,float,float);
    P6AudioProcessor& processor_;
    std::unique_ptr<PresetBrowser> browser_;
    juce::TextButton browserButton_{"PRESETS"};
    unsigned browserRefreshTicks_{};
    bool presetIsDirty_{};
    ContextDisplay display_;
    TubeChamber chamber_;
    CharacterDeviceView device_;
    CharacterHardwareWindow hardware_;
    int boundType_{-1};
    OutputMeter outputMeter_;
    juce::Slider master_;
    juce::Label masterLabel_,midiMonitor_;
    juce::TextButton clearClips_{"RESET CLIP"};
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> masterBinding_;
    unsigned lastOutputRevision_{};
    int staleOutputTicks_{};
    juce::TextButton module_{"MODULE"},studio_{"STUDIO"},previous_{"<"},next_{">"},collapse_{"HIDE"};
    juce::TextButton opticalPreview_{"OPTO PREVIEW"};
    juce::ToggleButton bypass_{"BYPASS"},motion_{"MOTION"};
    juce::ComboBox type_,position_,keyTrack_,fxAType_,fxBType_,midiInputChannel_,plasmaMode_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> midiChannelBinding_,positionBinding_,keyBinding_,fxATypeBinding_,fxBTypeBinding_,plasmaModeBinding_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassBinding_;
    std::array<Knob,37> synth_;
    std::array<Switch,18> switches_;
    std::array<Knob,7> tube_;
    // UI-only concept controls: these NEVER have APVTS bindings or modify audio.
    // Only display them while previewing the not-yet-implemented optical DSP.
    std::array<Knob,3> opticalConcept_;
    juce::ComboBox opticalWave_;
    std::unique_ptr<juce::GenericAudioProcessorEditor> diagnostic_;
    juce::Image grain_;
    juce::String focusName_{"LOW PASS"},focusValue_{"100 %"},presetName_;
    int focusNrpn_{45};
    float focusPosition_{1.f},glow_{},characterGlow_{};
    p6plugin::CharacterTelemetry visual_{};
    p6plugin::CharacterTelemetry lastCoherentTelemetry_{};
    unsigned lastVisualRevision_{},lastPitchBendRevision_{};
    int staleVisualTicks_{},pitchDisplayTicks_{},pitchBendRaw14_{8192};
    bool studioView_{},collapsed_{},showTube_{},opticalPreviewActive_{};
    float previewLfoPhase_{},previewAudioLevel_{};
    double lastUiFrameTimeMs_{};
    juce::Rectangle<int> tubeArea_,displayArea_;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(P6PluginEditor)
};
