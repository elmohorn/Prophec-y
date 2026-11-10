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
        bool rhythmGate{true};
        bool characterEnabled{};
        void paint(juce::Graphics& g) override {
            auto r=getLocalBounds().toFloat().reduced(2.f);
            g.setColour(juce::Colours::black.withAlpha(.28f));
            g.fillRoundedRectangle(r.translated(0.f,2.f),5.f);
            g.setGradientFill(juce::ColourGradient(juce::Colour{0xffa5a6a1},r.getX(),r.getY(),
                                                   juce::Colour{0xff555754},r.getX(),r.getBottom(),false));
            g.fillRoundedRectangle(r,5.f);
            auto cap=r.reduced(2.f);
            g.setGradientFill(juce::ColourGradient(juce::Colour{0xff444543},cap.getX(),cap.getY(),
                                                   juce::Colour{0xff101111},cap.getX(),cap.getBottom(),false));
            g.fillRoundedRectangle(cap,3.5f);
            const bool enabled=characterEnabled && getToggleState() && rhythmGate;
            if(enabled) {
                const auto c=cap.getCentre();
                const auto intensity=playing?.95f:.32f;
                // The active lamp illuminates the recess around the key, not
                // just its painted surface. Inactive steps stay nearly dark.
                if(playing) {
                    g.setGradientFill(juce::ColourGradient(juce::Colour{0xffff5141}.withAlpha(.36f),c.x,c.y,
                        juce::Colour{0xffc42a20}.withAlpha(0.f),c.x+cap.getWidth()*.7f,c.y,true));
                    g.fillRoundedRectangle(r.expanded(2.f),7.f);
                }
                g.setGradientFill(juce::ColourGradient(juce::Colour{0xffff493a}.withAlpha(intensity),c.x,c.y,
                    juce::Colour{0xffbb211b}.withAlpha(0.f),c.x+cap.getWidth()*.54f,c.y,true));
                g.fillRoundedRectangle(cap.reduced(1.f),3.f);
                g.setColour(juce::Colour{0xffffb1a1}.withAlpha(playing?.95f:.6f));
                g.fillEllipse(c.x-2.f,c.y-2.f,4.f,4.f);
            }
            g.setColour(juce::Colours::white.withAlpha(.17f));
            g.drawLine(cap.getX()+3.f,cap.getY()+2.f,cap.getRight()-3.f,cap.getY()+2.f,1.f);
        }
    };
    struct PanelSwitch final : juce::TextButton {
        void paint(juce::Graphics& g) override {
            auto r=getLocalBounds().toFloat().reduced(1.f);
            const bool open=getButtonText()=="HIDE";
            g.setColour(juce::Colour{0xff777a75});g.fillRoundedRectangle(r,4.f);
            g.setGradientFill(juce::ColourGradient(juce::Colour{0xffd2d4cd},r.getX(),r.getY(),
                                                   juce::Colour{0xff888c85},r.getX(),r.getBottom(),false));
            g.fillRoundedRectangle(r.reduced(1.f),3.f);
            auto label=r.removeFromLeft(r.getWidth()-49.f);
            auto track=r.reduced(2.f,4.f);
            g.setColour(juce::Colour{0xff292b2a});g.fillRoundedRectangle(track,3.f);
            g.setColour(juce::Colour{0xff555a55});g.drawRoundedRectangle(track,3.f,1.f);
            auto rocker=track.reduced(2.f);rocker.setWidth(rocker.getWidth()*.53f);
            if(open) rocker.setX(track.getRight()-rocker.getWidth()-2.f);
            g.setGradientFill(juce::ColourGradient(juce::Colour{0xffc2c4be},rocker.getX(),rocker.getY(),
                                                   juce::Colour{0xff777a75},rocker.getX(),rocker.getBottom(),false));
            g.fillRoundedRectangle(rocker,2.f);
            g.setColour(juce::Colour{0xff303330});g.setFont(juce::Font(11.f).boldened());
            g.drawText(open?"HIDE":"SHOW",label,juce::Justification::centred);
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
    juce::TextButton module_{"MODULE"},studio_{"STUDIO"},previous_{"<"},next_{">"};
    PanelSwitch collapse_;
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
    PanelSwitch arpPanelSwitch_;
    juce::ToggleButton arpOn_{"ON"},arpHold_{"HOLD"},arpSync_{"SYNC"},arpCharEnabled_{"CHAR"},arpFreeze_{"FREEZE"};
    juce::ComboBox arpMode_,arpOctaves_,arpDivision_,arpClock_;
    juce::Slider arpRhythm_,arpMotion_,arpBpm_,arpRate_;
    std::array<juce::TextButton,3> arpOctaveButtons_;
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

