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
    static constexpr int compactHeight=756, expandedHeight=986;
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
    struct ArpStepButton final : juce::TextButton {
        bool playing{};
        void paint(juce::Graphics& g) override {
            auto bounds=getLocalBounds().toFloat();
            auto indicator=bounds.removeFromTop(8.f);
            auto r=bounds.reduced(2.f);
            g.setColour(juce::Colour{0xff111212});g.fillRoundedRectangle(r,4.f);
            g.setColour(juce::Colour{0xff656663});g.drawRoundedRectangle(r,4.f,1.f);
            g.setColour(getToggleState()?juce::Colour{0xffe74235}:juce::Colour{0xff41302e});
            g.fillEllipse(r.getCentreX()-3.f,r.getCentreY()-3.f,6.f,6.f);
            if(playing){g.setColour(juce::Colour{0xffff6354});g.fillEllipse(indicator.getCentreX()-3.f,indicator.getY()+1.f,6.f,6.f);}
        }
    };
    struct ArpPanelSwitch final : juce::TextButton {
        void paint(juce::Graphics& g) override {
            auto r=getLocalBounds().toFloat();
            g.setColour(juce::Colour{0xff252827});g.setFont(9.f);
            g.drawText(getButtonText(),r.removeFromLeft(r.getWidth()*.52f),juce::Justification::centred);
            auto well=r.reduced(3.f,2.f);
            g.setColour(juce::Colour{0xff2d302f});g.fillRoundedRectangle(well,4.f);
            const float knobX=getButtonText()=="HIDE"?well.getX()+3.f:well.getRight()-14.f;
            g.setGradientFill(juce::ColourGradient(juce::Colour{0xffe6e7e2},knobX,well.getY(),juce::Colour{0xff72756f},knobX+10.f,well.getBottom(),false));
            g.fillRoundedRectangle(knobX,well.getY()+2.f,10.f,well.getHeight()-4.f,2.f);
        }
    };
    struct MetalLook final : juce::LookAndFeel_V4 {
        void drawButtonBackground(juce::Graphics&,juce::Button&,const juce::Colour&,bool,bool) override;
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
    juce::TextButton headerMenu_{"..."};
    juce::Label headerPreset_;
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
    std::array<Knob,36> synth_;
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
    std::uint32_t lastLiveOutputRevision_{};
    int staleLiveOutputTicks_{};
    int staleVisualTicks_{},pitchDisplayTicks_{},pitchBendRaw14_{8192};
    bool studioView_{},collapsed_{},showTube_{},opticalPreviewActive_{};
    bool arpOpen_{};
    int arpOffset() const noexcept { return arpOpen_ ? 160 : 56; }
    ArpPanelSwitch arpPanelSwitch_;
    juce::ToggleButton arpOn_{"ON"},arpHold_{"HOLD"},arpSync_{"SYNC"},arpCharEnabled_{"CHAR"},arpFreeze_{"FREEZE"};
    juce::ComboBox arpMode_,arpOctaves_,arpDivision_,arpClock_;
    juce::Slider arpRhythm_,arpMotion_,arpBpm_;
    std::array<ArpStepButton,8> arpSteps_;
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>,8> arpStepBindings_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> arpOnBinding_,arpHoldBinding_,arpSyncBinding_,arpCharacterBinding_,arpFreezeBinding_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> arpModeBinding_,arpOctaveBinding_,arpDivisionBinding_,arpClockBinding_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> arpRhythmBinding_,arpMotionBinding_,arpBpmBinding_;
    float previewLfoPhase_{},previewAudioLevel_{};
    double lastUiFrameTimeMs_{};
    juce::Rectangle<int> tubeArea_,displayArea_;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(P6PluginEditor)
};

