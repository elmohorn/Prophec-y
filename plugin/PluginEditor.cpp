#include "PluginEditor.h"
#include "BinaryData.h"
#include <algorithm>
#include <cmath>
#include <utility>
#include <cstdlib>

namespace {
const juce::Colour ink{0xff252827},muted{0xff636a68},amber{0xffe68c40},silver{0xffd7d6d0};
juce::String hardwareID(int nrpn) { return "nrpn_"+juce::String(nrpn).paddedLeft('0',4); }
constexpr float designWidth=1460.f;

int nrpnFromID(const juce::String& id)
{
    return id.startsWith("nrpn_") ? id.substring(5).getIntValue() : -1;
}


}
void P6PluginEditor::MetalLook::drawRotarySlider(juce::Graphics& g,int x,int y,int w,int h,
                                                float value,float start,float end,juce::Slider&)
{
    const float r=static_cast<float>(std::min(w,h))*.5f-8.f;
    const float cx=static_cast<float>(x)+static_cast<float>(w)*.5f;
    const float cy=static_cast<float>(y)+static_cast<float>(h)*.5f;
    for(int i=0;i<=24;++i) {
        const float a=start+(end-start)*static_cast<float>(i)/24.f;
        const float d=(i%6==0)?1.3f:.8f;
        g.setColour(ink.withAlpha(.7f));g.fillEllipse(cx+std::sin(a)*(r+5)-d,cy-std::cos(a)*(r+5)-d,2*d,2*d);
    }
    for(int layer=5;layer>0;--layer) {
        const float spread=static_cast<float>(layer)*1.15f;
        g.setColour(juce::Colours::black.withAlpha(.035f));
        g.fillEllipse(cx-r-spread,cy-r+4.f-spread*.2f,2*r+2*spread,2*r+2*spread);
    }
    g.setGradientFill(juce::ColourGradient(juce::Colour{0xffeff0e9},cx-r,cy-r,juce::Colour{0xff777d79},cx+r,cy+r,false));
    g.fillEllipse(cx-r-1,cy-r-1,2*r+2,2*r+2);
    g.setColour(juce::Colour{0xff0c0e0e});g.fillEllipse(cx-r,cy-r,2*r,2*r);
    g.setGradientFill(juce::ColourGradient(juce::Colour{0xff555653},cx-r*.5f,cy-r,juce::Colour{0xff181a19},cx+r*.6f,cy+r,false));
    g.fillEllipse(cx-r+3,cy-r+2,2*r-6,2*r-7);
    g.setColour(juce::Colours::white.withAlpha(.10f));g.drawEllipse(cx-r+3,cy-r+2,2*r-6,2*r-7,1.f);
    const float a=start+(end-start)*value;
    g.setColour(juce::Colour{0xffedece1});
    g.drawLine(cx+std::sin(a)*r*.52f,cy-std::cos(a)*r*.52f,cx+std::sin(a)*r*.85f,cy-std::cos(a)*r*.85f,2.f);
}
void P6PluginEditor::MetalLook::drawLinearSlider(juce::Graphics& g,int x,int y,int w,int h,
                                                float pos,float,float,juce::Slider::SliderStyle style,juce::Slider&)
{
    const bool vertical=style==juce::Slider::LinearVertical;
    const float cx=static_cast<float>(x)+static_cast<float>(w)*.5f,cy=static_cast<float>(y)+static_cast<float>(h)*.5f;
    const auto rail=vertical?juce::Rectangle<float>(cx-2,static_cast<float>(y),4,static_cast<float>(h)):
                             juce::Rectangle<float>(static_cast<float>(x),cy-2,static_cast<float>(w),4);
    g.setColour(juce::Colour{0xffa2a6a0});g.drawRoundedRectangle(rail.expanded(1.f),2.f,1.f);
    g.setColour(juce::Colour{0xff242b29});g.fillRoundedRectangle(rail,2.f);
    if(vertical) for(int i=0;i<=10;++i) {
        const float yy=static_cast<float>(y)+static_cast<float>(h*i)/10.f;
        g.setColour(muted.withAlpha(.65f));g.drawLine(cx-17,yy,cx-9,yy,1.f);g.drawLine(cx+9,yy,cx+17,yy,1.f);
    }
    const auto handle=vertical?juce::Rectangle<float>(cx-17,pos-9,34,18):juce::Rectangle<float>(pos-7,cy-9,14,18);
    g.setColour(juce::Colours::black.withAlpha(.16f));g.fillRoundedRectangle(handle.translated(1,3),2.f);
    g.setGradientFill(juce::ColourGradient(juce::Colour{0xff505853},handle.getX(),handle.getY(),juce::Colour{0xff171d1b},handle.getX(),handle.getBottom(),false));
    g.fillRoundedRectangle(handle,2.f);g.setColour(amber);
    if(vertical)g.drawLine(handle.getX()+4,pos,handle.getRight()-4,pos,2.f);
    else g.drawLine(pos,handle.getY()+3,pos,handle.getBottom()-3,2.f);
}
void P6PluginEditor::MetalLook::drawToggleButton(juce::Graphics& g,juce::ToggleButton& b,bool,bool)
{
    const bool tile=static_cast<bool>(b.getProperties().getWithDefault("ledTile",false));
    const float xx=tile?static_cast<float>(b.getWidth())*.5f-6.f:2.f;
    const float yy=tile?1.f:static_cast<float>(b.getHeight())*.5f-6.f;
    g.setColour(juce::Colour{0xff212724});g.fillRoundedRectangle(xx,yy,12,12,2.f);
    g.setColour(juce::Colours::white.withAlpha(.28f));g.drawRoundedRectangle(xx,yy,12,12,2.f,1.f);
    if(b.getToggleState()) {
        g.setGradientFill(juce::ColourGradient(amber.withAlpha(.65f),xx+6,yy+6,amber.withAlpha(0.f),xx+16,yy+6,true));
        g.fillEllipse(xx-5,yy-5,22,22);g.setColour(juce::Colour{0xffffbe77});g.fillEllipse(xx+4,yy+4,4,4);
    }
    g.setColour(b.isEnabled()?ink:muted);g.setFont(10.5f);
    g.drawText(b.getButtonText(),tile?0:20,tile?16:0,tile?b.getWidth():b.getWidth()-20,tile?14:b.getHeight(),tile?juce::Justification::centred:juce::Justification::centredLeft);
}
juce::Label* P6PluginEditor::MetalLook::createComboBoxTextBox(juce::ComboBox& c)
{
    auto* label=juce::LookAndFeel_V4::createComboBoxTextBox(c);
    label->setColour(juce::Label::textColourId,ink);return label;
}
void P6PluginEditor::MetalLook::drawComboBox(juce::Graphics& g,int w,int h,bool down,int bx,int by,int bw,int bh,juce::ComboBox& box)
{
    if(!static_cast<bool>(box.getProperties().getWithDefault("characterPlate",false))) {
        juce::LookAndFeel_V4::drawComboBox(g,w,h,down,bx,by,bw,bh,box);return;
    }
    const juce::Rectangle<float> plate(1,1,static_cast<float>(w-2),static_cast<float>(h-2));
    g.setColour(juce::Colour{0xff5b6058});g.fillRoundedRectangle(plate,3);
    g.setGradientFill(juce::ColourGradient(juce::Colour{0xffe2e0d5},0,0,juce::Colour{down?0xffa0a498:0xffb8bbb0},0,static_cast<float>(h),false));g.fillRoundedRectangle(plate.reduced(1),2);
    g.setColour(juce::Colours::white.withAlpha(.55f));g.drawLine(3,2,static_cast<float>(w-3),2,1);
    const float x=static_cast<float>(w-12),y=static_cast<float>(h)*.5f;
    juce::Path arrow;arrow.startNewSubPath(x-3,y-1);arrow.lineTo(x,y+2);arrow.lineTo(x+3,y-1);
    g.setColour(ink.withAlpha(box.isEnabled()?.7f:.25f));g.strokePath(arrow,juce::PathStrokeType(1));
}
P6PluginEditor::P6PluginEditor(P6AudioProcessor& p,juce::File presetRoot):AudioProcessorEditor(p),processor_(p)
{
    look_.setColour(juce::ResizableWindow::backgroundColourId,silver);
    look_.setColour(juce::Slider::textBoxTextColourId,ink);
    look_.setColour(juce::Slider::textBoxBackgroundColourId,juce::Colours::transparentBlack);
    look_.setColour(juce::Slider::textBoxOutlineColourId,juce::Colours::transparentBlack);
    look_.setColour(juce::Label::textColourId,ink);
    look_.setColour(juce::TextButton::buttonColourId,juce::Colour{0xffc6c9c1});
    look_.setColour(juce::TextButton::buttonOnColourId,juce::Colour{0xff37423c});
    look_.setColour(juce::TextButton::textColourOffId,ink);
    look_.setColour(juce::TextButton::textColourOnId,juce::Colour{0xffe9d5ad});
    look_.setColour(juce::ComboBox::backgroundColourId,juce::Colour{0xffe1e2d9});
    look_.setColour(juce::ComboBox::textColourId,ink);
    look_.setColour(juce::ComboBox::arrowColourId,muted);
    setLookAndFeel(&look_);
    addAndMakeVisible(display_);addChildComponent(chamber_);addChildComponent(device_);
    for(auto* c:std::array<juce::Component*,12>{&module_,&studio_,&previous_,&next_,&collapse_,&bypass_,&motion_,&type_,&position_,&keyTrack_,&fxAType_,&fxBType_}) {
        c->setLookAndFeel(&look_);addAndMakeVisible(c);
    }
    for(auto* c:{&type_,&position_,&keyTrack_,&fxAType_,&fxBType_}) c->setColour(juce::ComboBox::textColourId,ink);
    for(int t=0;t<p6plugin::kCharacterTypes;++t)type_.addItem(juce::String(p6plugin::kCharacterNames[static_cast<std::size_t>(t)]).toUpperCase(),t+1);type_.setTitle("Prophec-y effect");
    type_.getProperties().set("characterPlate",true);position_.getProperties().set("characterPlate",true);
    position_.addItem("START OF FX",1);position_.addItem("END OF FX",2);position_.setTitle("Effect position");
    keyTrack_.addItem("OFF",1);keyTrack_.addItem("HALF",2);keyTrack_.addItem("FULL",3);keyTrack_.setTitle("Low-pass keyboard tracking");
    fxAType_.addItemList(juce::StringArray{"A · OFF","A · BBD DELAY","A · DIGITAL DELAY","A · CHORUS","A · PHASER 1","A · PHASER 2"},1);
    fxBType_.addItemList(juce::StringArray{"B · OFF","B · BBD DELAY","B · DIGITAL DELAY","B · CHORUS","B · PHASER 1","B · PHASER 2","B · HALL","B · ROOM","B · PLATE","B · SPRING"},1);
    fxAType_.setTitle("Standard FX A type");fxBType_.setTitle("Standard FX B type");
    midiInputChannel_.setLookAndFeel(&look_);midiInputChannel_.setTitle("MIDI input channel");
    midiInputChannel_.setTooltip("MIDI input: Omni or channel 1–16. Changing channel silences notes, clears standard FX tails and resets controllers.");
    midiInputChannel_.addItem("MIDI: OMNI",1);for(int channel=1;channel<=16;++channel)midiInputChannel_.addItem("MIDI: "+juce::String(channel),channel+1);
    addAndMakeVisible(midiInputChannel_);
    midiMonitor_.setTitle("Recent MIDI input");midiMonitor_.setFont(juce::Font(11.f));
    midiMonitor_.setColour(juce::Label::textColourId,muted);
    midiMonitor_.setTooltip("Recent pitch bend and CC input. * marks a message filtered by the selected MIDI channel.");
    midiMonitor_.setText("MIDI: awaiting input",juce::dontSendNotification);addAndMakeVisible(midiMonitor_);
    auto& params=p.parameters();
    midiChannelBinding_=std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(params,p6plugin::kMidiInputChannelID,midiInputChannel_);
    master_.setLookAndFeel(&look_);master_.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    master_.setTextBoxStyle(juce::Slider::TextBoxBelow,false,95,18);master_.setTitle("Master Output");
    master_.setDescription("Final output attenuation, separate from Prophet Program Volume. Minus 60 to zero dB.");
    master_.setTooltip("Master Output after all effects. New instances: -18 dB; legacy sessions: 0 dB. Double-click resets to -18 dB.");
    master_.setWantsKeyboardFocus(true);addAndMakeVisible(master_);addAndMakeVisible(outputMeter_);
    masterBinding_=std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(params,p6plugin::kMasterOutputID,master_);
    master_.textFromValueFunction=[](double v){return juce::String(v,1)+" dB";};
    master_.valueFromTextFunction=[this](const juce::String& text){
        const auto clean=text.trim().replace("dB", "",true).trim();const auto utf8=clean.toStdString();char* end=nullptr;
        const double value=std::strtod(utf8.c_str(),&end);
        return end!=utf8.c_str()&&end&&*end=='\0'&&std::isfinite(value)&&value>=-60.&&value<=0.?value:master_.getValue();
    };
    master_.setDoubleClickReturnValue(true,p6plugin::kNewMasterDb);master_.updateText();
    masterLabel_.setText("MASTER",juce::dontSendNotification);masterLabel_.setFont(juce::Font(10.f));masterLabel_.setJustificationType(juce::Justification::centred);addAndMakeVisible(masterLabel_);
    clearClips_.setLookAndFeel(&look_);clearClips_.setTooltip("Clear latched output clip and pre-master over-zero indicators.");
    clearClips_.onClick=[this]{processor_.resetOutputClips();outputMeter_.update(processor_.consumeOutputReadings());};addAndMakeVisible(clearClips_);
    outputMeter_.update(processor_.consumeOutputReadings());lastOutputRevision_=processor_.outputRevision();
    type_.onChange=[this]{setType(type_.getSelectedId()-1);};
    positionBinding_=std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(params,p6plugin::kCharacterPositionID,position_);
    keyBinding_=std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(params,hardwareID(47),keyTrack_);
    fxATypeBinding_=std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(params,hardwareID(119),fxAType_);
    fxBTypeBinding_=std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(params,hardwareID(127),fxBType_);
    bypassBinding_=std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(params,p6plugin::kCharacterBypassID,bypass_);
    previous_.onClick=[this]{setType(processor_.selectedCharacterType()-1);};next_.onClick=[this]{setType(processor_.selectedCharacterType()+1);};
    module_.onClick=[this]{studioView_=false;refreshVisibility();};studio_.onClick=[this]{studioView_=true;refreshVisibility();};
    collapse_.onClick=[this]{collapsed_=!collapsed_;refreshVisibility();};motion_.setToggleState(true,juce::dontSendNotification);
    motion_.setTooltip("Pause the light animation; audio continues.");collapse_.setTooltip("Hide the device, keeping its audio active.");
    type_.setTooltip("Select a Character device. Every device retains its own settings.");
    position_.setTooltip("Start: before distortion, FX A and FX B. End: after FX B.");
    fxAType_.setTooltip("Select standard Prophet-6 FX slot A. Use FX ON to enable the standard chain.");
    fxBType_.setTooltip("Select standard Prophet-6 FX slot B. Slot B also contains the reverbs.");
    constexpr std::array<int,37> ids{0,3,4,2,5,8,9,7,6,49,45,46,77,143,144,88,89,90,78,79,80,81,67,68,69,70,30,33,63,62,120,128,2,7,27,32,45};
    constexpr std::array<const char*,37> labels{"FREQ","SHAPE","PULSE WIDTH","LEVEL","FREQ","SHAPE","PULSE WIDTH","LEVEL","FINE","HIGH PASS","LOW PASS","RESONANCE","ENV AMOUNT","FILTER ENV","OSC 2","RATE","INITIAL AMT","WAVEFORM","ATTACK","DECAY","SUSTAIN","RELEASE","ATTACK","DECAY","SUSTAIN","RELEASE","GLIDE","SLOP","PAN SPREAD","PROGRAM VOL","MIX A","MIX B","OSC 1","OSC 2","SUB","NOISE","LOW PASS"};
    for(std::size_t i=0;i<synth_.size();++i)makeKnob(synth_[i],hardwareID(ids[i]),labels[i]);
    for(std::size_t i=32;i<36;++i)synth_[i].slider.setSliderStyle(juce::Slider::LinearVertical);
    constexpr std::array<int,18> switchIds{1,10,11,156,29,48,135,91,145,146,147,148,149,93,94,95,97,98};
    constexpr std::array<const char*,18> switchNames{"SYNC","LOW FREQ","KEYBOARD","UNISON","GLIDE ON","VELOCITY","FX ON","SYNC","FREQ 1","SHAPE 1","PW 1","LP","HP","OSC 1","OSC 2","PW 1+2","LP","HP"};
    for(std::size_t i=0;i<switches_.size();++i) {
        auto& sw=switches_[i];sw.button.setLookAndFeel(&look_);sw.button.setButtonText(switchNames[i]);
        sw.button.setTitle(juce::String(i<13&&i>=8?"Poly Mod ":i>=13?"LFO ":"")+switchNames[i]);
        if(i>=8||i<4||i==6)sw.button.getProperties().set("ledTile",true);
        addAndMakeVisible(sw.button);
        sw.attachment=std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(params,hardwareID(switchIds[i]),sw.button);
    }
    for(std::size_t i=0;i<tube_.size();++i) {
        makeKnob(tube_[i],p6plugin::kTubeIDs[i],p6plugin::kTubeNames[i],true);
        tube_[i].slider.textFromValueFunction=[i](double v){return i==0?juce::String(-12.+48.*v,1)+" dB":i==6?juce::String(-24.+36.*v,1)+" dB":juce::String(v*100.,0)+" %";};
        tube_[i].slider.valueFromTextFunction=[i](const juce::String& text){const auto v=text.getDoubleValue();return i==0?(v+12.)/48.:i==6?(v+24.)/36.:v/100.;};
        tube_[i].slider.updateText();
    }

    grain_=juce::Image(juce::Image::RGB,384,192,false);juce::Random random(643218);
    for(int y=0;y<grain_.getHeight();++y)for(int x=0;x<grain_.getWidth();++x) {
        const auto c=static_cast<juce::uint8>(196+random.nextInt(5)+(y%3==0?1:0));grain_.setPixelAt(x,y,juce::Colour(c,c,static_cast<juce::uint8>(c-2)));
    }
    diagnostic_=std::make_unique<juce::GenericAudioProcessorEditor>(p);addChildComponent(*diagnostic_);
    presetName_=p.getProgramName(0);visual_=p.characterTelemetry();
    lastVisualRevision_=p.characterTelemetryRevision();
    lastPitchBendRevision_=p.midiPitchBendRevision();pitchBendRaw14_=p.midiPitchBendRaw14();
    if(params.getRawParameterValue(p6plugin::kCharacterBypassID)->load()>=.5f)visual_.active=false;
    glow_=visual_.active?std::clamp(std::sqrt(visual_.inputRms)*1.8f,0.f,1.f):0.f;
    characterGlow_=visual_.active?std::clamp(std::sqrt(visual_.changeRms)*2.f,0.f,1.f):0.f;
    chamber_.setIllumination(visual_.active,motion_.getToggleState(),glow_,characterGlow_);
    focusValue_=juce::String(p6plugin::ui::formatParameterValue(45,params.getRawParameterValue(hardwareID(45))->load(),p.parameterPresentationContext()));
    setResizable(true,false);setResizeLimits(1240,compactHeight,1920,1200);setSize(1460,compactHeight);
    browser_=std::make_unique<PresetBrowser>(processor_,std::move(presetRoot));addChildComponent(*browser_);
    browserButton_.setLookAndFeel(&look_);browserButton_.setTitle("Preset browser");addAndMakeVisible(browserButton_);
    browserButton_.onClick=[this]{browser_->setVisible(!browser_->isVisible());if(browser_->isVisible()){browser_->refreshLibrary();browser_->toFront(true);}};
    browser_->onClose=[this]{browser_->setVisible(false);browserButton_.grabKeyboardFocus();};
    presetIsDirty_=browser_->refreshCurrent();browserButton_.setButtonText(presetIsDirty_?"PRESETS *":"PRESETS");
    refreshVisibility();refreshDisplay();startTimerHz(30);
}
P6PluginEditor::~P6PluginEditor(){stopTimer();setLookAndFeel(nullptr);}
void P6PluginEditor::makeKnob(Knob& k,const juce::String& id,const juce::String& name,bool values)
{
    k.slider.setLookAndFeel(&look_);k.slider.setColour(juce::Slider::textBoxTextColourId,ink);
    k.slider.setColour(juce::Slider::textBoxOutlineColourId,juce::Colours::transparentBlack);
    k.slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    k.slider.setTextBoxStyle(values?juce::Slider::TextBoxBelow:juce::Slider::NoTextBox,false,90,20);
    k.slider.setPopupDisplayEnabled(!values,!values,this);
    const int nrpn=nrpnFromID(id);
    k.slider.setTitle(name);k.slider.setTooltip(name+" — drag to edit; Studio provides numeric entry.");
    k.label.setText(name,juce::dontSendNotification);k.label.setColour(juce::Label::textColourId,ink);
    k.label.setFont(juce::Font(10.5f).withExtraKerningFactor(.05f));k.label.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(k.slider);addAndMakeVisible(k.label);
    k.attachment=std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor_.parameters(),id,k.slider);
    // Attach first: JUCE installs its own formatter during attachment setup.
    if(nrpn>=0)k.slider.textFromValueFunction=[this,nrpn](double raw){return juce::String(p6plugin::ui::formatParameterValue(nrpn,raw,processor_.parameterPresentationContext()));};
    if(nrpn>=0)k.slider.configure(*processor_.parameters().getParameter(id),nrpn,[this]{return processor_.parameterPresentationContext();});
    auto focus=[this,&k,name,nrpn]{focusNrpn_=nrpn;focusName_=name;focusValue_=k.slider.getTextFromValue(k.slider.getValue());focusPosition_=static_cast<float>(k.slider.valueToProportionOfLength(k.slider.getValue()));pitchDisplayTicks_=0;refreshDisplay();};
    k.slider.onDragStart=focus;k.slider.onSelected=focus;k.slider.onValueChange=focus;
}
void P6PluginEditor::setType(int value)
{
    processor_.selectCharacterType(value);refreshVisibility();
}
void P6PluginEditor::timerCallback()
{
    if(!isShowing())return;
    const auto events=processor_.recentMidiEvents();juce::String monitor;
    for(const auto& event:events)if(event.valid) {
        if(monitor.isNotEmpty())monitor+="\n";
        monitor+="CH "+juce::String(event.channel)+(event.pitchBend?" PB ":" CC "+juce::String(event.controller)+" ")+juce::String(event.value)+(event.filtered?" *":"");
    }
    if(monitor.isNotEmpty())midiMonitor_.setText(monitor,juce::dontSendNotification);
    const auto outputRevision=processor_.outputRevision();
    staleOutputTicks_=outputRevision==lastOutputRevision_?std::min(31,staleOutputTicks_+1):0;
    lastOutputRevision_=outputRevision;
    auto readings=processor_.consumeOutputReadings();
    if(staleOutputTicks_>=30){readings.peak={};readings.rms={};readings.prePeak={};}
    outputMeter_.update(readings);
    if(processor_.selectedCharacterType()!=boundType_)refreshVisibility();
    if(++browserRefreshTicks_%6==0){presetIsDirty_=browser_->refreshCurrent();browserButton_.setButtonText(presetIsDirty_?"PRESETS *":"PRESETS");}
    const auto name=processor_.activePresetName()+(presetIsDirty_?" *":"");if(name!=presetName_){presetName_=name;repaint(displayArea_);}
    if(focusNrpn_>=0) {
        const auto* raw=processor_.parameters().getRawParameterValue(hardwareID(focusNrpn_));
        if(raw) {
            const auto value=juce::String(p6plugin::ui::formatParameterValue(focusNrpn_,raw->load(),processor_.parameterPresentationContext()));
            if(value!=focusValue_){focusValue_=value;repaint(displayArea_);}
        }
    }
    const auto pitchRevision=processor_.midiPitchBendRevision();
    if(pitchRevision!=lastPitchBendRevision_) {
        lastPitchBendRevision_=pitchRevision;
        pitchBendRaw14_=processor_.midiPitchBendRaw14();
        pitchDisplayTicks_=pitchBendRaw14_==8192?30:60;
        repaint(displayArea_);
    } else if(pitchDisplayTicks_>0) {
        --pitchDisplayTicks_;
        if(pitchDisplayTicks_==0)repaint(displayArea_);
    }
    const auto revision=processor_.characterTelemetryRevision();
    staleVisualTicks_=revision==lastVisualRevision_?std::min(30,staleVisualTicks_+1):0;
    lastVisualRevision_=revision;
    const auto sample=processor_.characterTelemetry();
    visual_.type=sample.type;visual_.device=sample.device;
    const bool bypassed=processor_.parameters().getRawParameterValue(p6plugin::kCharacterBypassID)->load()>=.5f;
    const bool live=staleVisualTicks_<4;
    visual_.active=sample.active&&!bypassed&&live;
    const auto follow=[](float previous,float target){return previous+(target>previous?.55f:.16f)*(target-previous);};
    visual_.inputRms=follow(visual_.inputRms,live?sample.inputRms:0.f);
    visual_.outputRms=follow(visual_.outputRms,live?sample.outputRms:0.f);
    visual_.changeRms=follow(visual_.changeRms,visual_.active?sample.changeRms:0.f);
    // No random flicker: the heater follows level, the inner glow follows the
    // actual latency-aligned waveform change. Neither changes the audio.
    glow_=visual_.active?std::clamp(std::sqrt(visual_.inputRms)*1.8f,0.f,1.f):0.f;
    characterGlow_=visual_.active?std::clamp(std::sqrt(visual_.changeRms)*2.f,0.f,1.f):0.f;
    chamber_.setIllumination(visual_.active,motion_.getToggleState(),glow_,characterGlow_);
    updateDevice();
    if(showTube_&&!collapsed_&&!studioView_)repaint(tubeArea_);
    refreshDisplay();
}
void P6PluginEditor::refreshVisibility()
{
    const int selected=processor_.selectedCharacterType();
    if(boundType_!=selected)bindDevice(selected);
    showTube_=selected>0;
    type_.setSelectedId(selected+1,juce::dontSendNotification);
    for(auto& k:synth_){k.slider.setVisible(!studioView_);k.label.setVisible(!studioView_);}
    for(auto& s:switches_)s.button.setVisible(!studioView_);
    fxAType_.setVisible(!studioView_);fxBType_.setVisible(!studioView_);
    for(auto& k:tube_){k.slider.setVisible(!studioView_&&showTube_&&!collapsed_);k.label.setVisible(k.slider.isVisible());}
    diagnostic_->setVisible(studioView_);display_.setVisible(!studioView_);
    chamber_.setVisible(!studioView_&&selected==1&&!collapsed_);
    device_.setVisible(!studioView_&&selected>=2&&!collapsed_);updateDevice();
    for(auto* c:std::array<juce::Component*,8>{&previous_,&next_,&type_,&position_,&bypass_,&collapse_,&motion_,&keyTrack_})c->setVisible(!studioView_);
    collapse_.setVisible(!studioView_&&showTube_);motion_.setVisible(!studioView_&&showTube_&&!collapsed_);
    position_.setEnabled(showTube_);bypass_.setEnabled(showTube_);previous_.setEnabled(selected>0);next_.setEnabled(selected<p6plugin::kCharacterTypes-1);
    module_.setToggleState(!studioView_,juce::dontSendNotification);studio_.setToggleState(studioView_,juce::dontSendNotification);
    collapse_.setButtonText(collapsed_?"SHOW":"HIDE");
    const bool expanded=!studioView_&&showTube_&&!collapsed_;
    setResizeLimits(1240,expanded?expandedHeight:compactHeight,1920,1200);
    setSize(getWidth(),expanded?std::max(getHeight(),expandedHeight):compactHeight);resized();repaint();
}
void P6PluginEditor::resized()
{
    const float sx=static_cast<float>(getWidth())/designWidth;
    auto rect=[sx](float x,float y,float w,float h){return juce::Rectangle<int>(juce::roundToInt(x*sx),juce::roundToInt(y),juce::roundToInt(w*sx),juce::roundToInt(h));};
    browserButton_.setBounds(rect(505,65,85,23));
    if(browser_)browser_->setBounds(55,98,getWidth()-110,450);
    master_.setBounds(rect(602,12,100,67));masterLabel_.setBounds(rect(602,78,100,15));
    outputMeter_.setBounds(rect(724,25,350,60));clearClips_.setBounds(rect(1085,49,105,24));
    midiInputChannel_.setBounds(rect(1085,16,105,24));midiMonitor_.setBounds(rect(350,62,146,26));
    module_.setBounds(rect(1210,36,84,25));studio_.setBounds(rect(1300,36,84,25));
    diagnostic_->setBounds(36,92,getWidth()-72,getHeight()-125);
    auto knob=[&](std::size_t i,float x,float y,float w=80,float h=72){synth_[i].slider.setBounds(rect(x,y,w,h));synth_[i].label.setBounds(rect(x-5,y+h+1,w+10,18));};
    for(std::size_t i=0;i<4;++i){knob(i,54+94.f*static_cast<float>(i),119);knob(i+4,54+94.f*static_cast<float>(i),269);}
    knob(8,337,366,49,47);
    knob(9,502,318,79,77);knob(10,621,279,139,137);knob(11,785,318,79,77);knob(12,890,318,79,77);
    knob(13,1016,115,76,68);knob(14,1108,115,76,68);
    knob(15,1016,224,62,65);knob(16,1090,224,62,65);knob(17,1164,224,62,65);
    for(std::size_t i=0;i<4;++i){knob(18+i,1030+94.f*static_cast<float>(i),344,75,68);knob(22+i,1030+94.f*static_cast<float>(i),444,75,68);}
    knob(26,553,535,72,65);knob(27,653,535,72,65);knob(28,753,535,72,65);knob(29,1302,539,89,78);
    for(std::size_t i=0;i<2;++i)knob(30+i,1032+88.f*static_cast<float>(i),549,65,58);
    for(std::size_t i=0;i<4;++i){synth_[32+i].slider.setBounds(rect(72+84.f*static_cast<float>(i),463,55,117));synth_[32+i].label.setBounds(rect(64+84.f*static_cast<float>(i),583,71,18));}
    switches_[0].button.setBounds(rect(70,373,71,36));switches_[1].button.setBounds(rect(171,373,71,36));switches_[2].button.setBounds(rect(265,373,71,36));
    switches_[3].button.setBounds(rect(475,557,70,36));switches_[4].button.setBounds(rect(558,607,70,19));switches_[5].button.setBounds(rect(795,446,98,24));
    switches_[6].button.setBounds(rect(916,552,76,36));switches_[7].button.setBounds(rect(1288,273,65,24));
    for(std::size_t i=0;i<5;++i){switches_[8+i].button.setBounds(rect(1201+40.f*static_cast<float>(i),126,39,38));switches_[13+i].button.setBounds(rect(1240+32.f*static_cast<float>(i),231,32,38));}
    fxAType_.setBounds(rect(916,590,100,18));fxBType_.setBounds(rect(916,611,100,18));
    keyTrack_.setBounds(rect(587,446,132,24));
    const bool expanded=showTube_&&!collapsed_;
    previous_.setBounds(rect(66,expanded?689:652,25,23));type_.setBounds(rect(99,expanded?689:652,158,23));next_.setBounds(rect(265,expanded?689:652,25,23));
    position_.setBounds(rect(1248,expanded?811:652,137,24));bypass_.setBounds(rect(1262,expanded?753:652,100,24));
    if(!expanded){previous_.setBounds(rect(305,652,25,23));type_.setBounds(rect(338,652,155,23));next_.setBounds(rect(501,652,25,23));position_.setBounds(rect(550,652,145,24));bypass_.setBounds(rect(735,652,100,24));}
    collapse_.setBounds(rect(1190,652,90,24));motion_.setBounds(rect(1301,652,91,24));
    knob(36,883,109,88,88);
    displayArea_=rect(526,96,322,122);display_.setBounds(displayArea_);tubeArea_={32,637,getWidth()-64,std::max(51,getHeight()-660)};
    chamber_.setBounds(rect(65,720,320,180));device_.setBounds(chamber_.getBounds());
    for(std::size_t i=0;i<tube_.size();++i){const float x=421+114.f*static_cast<float>(i);tube_[i].label.setBounds(rect(x,759,106,20));tube_[i].slider.setBounds(rect(x,782,106,106));}

}
void P6PluginEditor::section(juce::Graphics& g,const juce::String& text,float x,float y,float width)
{
    const float sx=static_cast<float>(getWidth())/designWidth;
    g.setColour(ink);g.setFont(juce::Font(12.f).boldened().withExtraKerningFactor(.09f));
    g.drawText(text,static_cast<int>(x*sx),static_cast<int>(y),static_cast<int>(width*sx),19,juce::Justification::centredLeft);
    const float lineStart=(x+static_cast<float>(text.length())*8.f+14.f)*sx;
    g.setColour(juce::Colours::white.withAlpha(.6f));g.drawLine(lineStart,y+10,(x+width)*sx,y+10,1.f);
    g.setColour(muted.withAlpha(.30f));g.drawLine(lineStart,y+11,(x+width)*sx,y+11,1.f);
}
void P6PluginEditor::paintSurface(juce::Graphics& g)
{
    const float w=static_cast<float>(getWidth()),h=static_cast<float>(getHeight());
    g.fillAll(juce::Colour{0xffb4b5ae});
    g.setColour(juce::Colours::black.withAlpha(.18f));g.fillRoundedRectangle(17,18,w-34,h-25,12.f);
    g.setColour(juce::Colour{0xff242925});g.fillRoundedRectangle(18,12,w-36,h-28,10.f);
    const juce::Rectangle<float> face(25,10,w-50,h-33);
    g.setGradientFill(juce::ColourGradient(juce::Colour{0xffdedbd5},36,10,juce::Colour{0xffbab9b3},w*.75f,h,false));g.fillRoundedRectangle(face,7.f);
    g.setTiledImageFill(grain_,0,0,.10f);g.fillRoundedRectangle(face,7.f);
    g.setColour(juce::Colours::white.withAlpha(.78f));g.drawRoundedRectangle(face.reduced(.5f),7.f,1.f);
    g.setColour(juce::Colour{0xff727c74});g.drawLine(22,24,22,h-29,2.f);g.drawLine(w-22,24,w-22,h-29,2.f);
}
void P6PluginEditor::refreshDisplay()
{
    if(studioView_)return;
    ContextDisplay::Content content;
    content.preset=presetName_;content.title=focusName_;content.value=focusValue_;
    content.position=focusPosition_;content.caption="CONTROL POSITION / MODEL RESPONSE PENDING";
    auto raw=[this](int nrpn){return static_cast<double>(processor_.parameters().getRawParameterValue(hardwareID(nrpn))->load());};
    const auto context=processor_.parameterPresentationContext();
    if(pitchDisplayTicks_>0) {
        const double bipolar=pitchBendRaw14_>=8192?static_cast<double>(pitchBendRaw14_-8192)/8191.:static_cast<double>(pitchBendRaw14_-8192)/8192.;
        content.pitchBend=true;content.pitchRaw=pitchBendRaw14_;content.position=bipolar;content.title="PITCH BEND";
        const double semitones=bipolar*std::clamp(raw(31),0.,12.);
        content.value=juce::String(semitones>=0.?"+":"")+juce::String(semitones,2)+" st";
        content.caption="LIVE MIDI / RAW14 "+juce::String(pitchBendRaw14_);
    } else if((focusNrpn_>=0&&focusNrpn_<=11)) {
        const bool second=focusNrpn_>=5;
        content.spec.kind=p6plugin::ui::PreviewKind::Oscillator;
        content.spec.values={raw(second?8:3)/254.,p6::mappedPulseWidth(static_cast<std::uint16_t>(raw(second?9:4)),context.calibration),context.calibration.pulseMeanNormalization,0.,0.};
        content.title=juce::String(second?"OSC 2 / ":"OSC 1 / ")+focusName_;
        content.caption="NOMINAL CYCLE / PHASE 0-1 / NO SYNC OR MOD";
    } else if((focusNrpn_>=67&&focusNrpn_<=70)||(focusNrpn_>=78&&focusNrpn_<=81)||focusNrpn_==77) {
        const bool filter=focusNrpn_>=77;
        const int first=filter?78:67;
        auto time=[&](int nrpn){return p6plugin::ui::parameterValue(nrpn,raw(nrpn),context).value;};
        content.spec.kind=p6plugin::ui::PreviewKind::Envelope;
        content.spec.values={time(first),time(first+1),raw(first+2)/127.,time(first+3),.25};
        content.title=juce::String(filter?"FILTER ENV / ":"AMP ENV / ")+focusName_;
        const double gate=content.spec.values[0]+content.spec.values[1]+.25;
        const double span=gate+content.spec.values[3];
        content.caption="MODEL ADSR / GATE "+juce::String(gate,2)+" s / AXIS 0-"+juce::String(span,2)+" s";
    } else if(focusNrpn_==45||focusNrpn_==49||focusNrpn_==46) {
        content.caption="CUTOFF CONTROL / NOT A FILTER RESPONSE";
    }
    display_.setContent(std::move(content));
}
void P6PluginEditor::paintCharacterPanel(juce::Graphics& g)
{
    const float sx=static_cast<float>(getWidth())/designWidth;
    const juce::Rectangle<float> panel(30*sx,638,getWidth()-60*sx,static_cast<float>(getHeight()-660));
    g.setColour(juce::Colour{0xff343833});g.fillRoundedRectangle(panel.expanded(2),7);
    g.setGradientFill(juce::ColourGradient(juce::Colour{0xffd1cec5},panel.getX(),638,juce::Colour{0xffafafa7},panel.getRight(),panel.getBottom(),false));g.fillRoundedRectangle(panel,6);
    g.setTiledImageFill(grain_,0,638,.1f);g.fillRoundedRectangle(panel,6);
    g.setColour(juce::Colours::white.withAlpha(.8f));g.drawRoundedRectangle(panel.reduced(1),5,1);
    for(float x:{44*sx,getWidth()-44*sx})for(float y:{650.f,panel.getBottom()-12}) {
        g.setColour(juce::Colours::black.withAlpha(.35f));g.fillEllipse(x-5,y-4,11,11);
        g.setGradientFill(juce::ColourGradient(juce::Colour{0xfff5f1e4},x-4,y-4,juce::Colour{0xff676d63},x+4,y+4,false));g.fillEllipse(x-4,y-4,8,8);
        g.setColour(juce::Colour{0xff34382f});g.drawLine(x-2,y+1,x+2,y-1,1.2f);
    }
}
void P6PluginEditor::paintTube(juce::Graphics& g)
{
    const float sx=static_cast<float>(getWidth())/designWidth;
    g.setColour(muted);g.setFont(juce::Font(10.f).boldened());
    g.drawText(juce::String(p6plugin::kCharacterNames[static_cast<std::size_t>(std::max(0,boundType_))]).toUpperCase(),static_cast<int>(430*sx),696,static_cast<int>(240*sx),18,juce::Justification::centredLeft);
    g.drawText(boundType_==1?"TONE":"CHARACTER",static_cast<int>(770*sx),696,static_cast<int>(200*sx),18,juce::Justification::centredLeft);
    g.drawText("LEVEL",static_cast<int>(1085*sx),696,static_cast<int>(130*sx),18,juce::Justification::centredLeft);
    g.drawText("INSERTION",static_cast<int>(1260*sx),789,static_cast<int>(120*sx),18,juce::Justification::centredLeft);
    g.setColour(muted.withAlpha(.3f));g.drawLine(405*sx,691,405*sx,898,1);g.drawLine(1238*sx,731,1238*sx,893,1);
    // Signal-change meter deliberately is not labelled saturation or THD: EQ,
    // output gain, drive and wet/dry mix all contribute to this difference.
    const std::array<float,3> values{visual_.inputRms,visual_.outputRms,visual_.changeRms};
    const std::array<const char*,3> labels{"IN","OUT","CHANGE"};
    for(std::size_t i=0;i<3;++i) {
        const float x=(430+270.f*static_cast<float>(i))*sx,y=741.f;
        g.setColour(muted);g.setFont(9.f);g.drawText(labels[i],static_cast<int>(x),static_cast<int>(y)-10,70,13,juce::Justification::centredLeft);
        g.setColour(ink.withAlpha(.12f));g.fillRoundedRectangle(x+45*sx,y-6,180*sx,4,2.f);
        const float level=std::clamp((20.f*std::log10(std::max(values[i],.000001f))+60.f)/60.f,0.f,1.f);
        g.setColour(amber.darker(.15f));g.fillRoundedRectangle(x+45*sx,y-6,180*sx*level,4,2.f);
    }
}
void P6PluginEditor::paint(juce::Graphics& g)
{
    paintSurface(g);const float sx=static_cast<float>(getWidth())/designWidth;
    g.setColour(ink);g.setFont(juce::Font(30.f).withExtraKerningFactor(.42f));g.drawText("PROPHEC-Y",static_cast<int>(55*sx),33,370,34,juce::Justification::centredLeft);
    g.setColour(muted);g.setFont(juce::Font(9.f).withExtraKerningFactor(.22f));g.drawText("POLYPHONIC SYNTHESIZER",static_cast<int>(375*sx),45,325,16,juce::Justification::centredLeft);
    if(studioView_)return;
    section(g,"OSCILLATOR 1",56,90,360);section(g,"OSCILLATOR 2",56,240,360);section(g,"MIXER",56,431,360);
    section(g,"FILTER",478,264,495);section(g,"PERFORMANCE",478,510,370);section(g,"FX",916,528,315);
    section(g,"POLY MOD",1020,85,373);section(g,"LFO",1020,200,373);section(g,"FILTER ENVELOPE",1020,316,373);section(g,"AMPLIFIER ENVELOPE",1020,426,373);
    g.setColour(muted.withAlpha(.3f));g.drawLine(445*sx,107,445*sx,615,1.f);g.drawLine(1027*sx,107,1027*sx,615,1.f);

    g.setColour(muted);g.setFont(10.f);g.drawText("KEY TRACK",static_cast<int>(491*sx),446,static_cast<int>(90*sx),24,juce::Justification::centredLeft);
    paintCharacterPanel(g);
    g.setColour(ink);g.setFont(juce::Font(11.f).withExtraKerningFactor(.09f));g.drawText("PROPHEC-Y / CHARACTER",static_cast<int>(55*sx),651,static_cast<int>(240*sx),24,juce::Justification::centredLeft);
    if(showTube_&&!collapsed_)paintTube(g);
}

void P6PluginEditor::bindDevice(int type)
{
    boundType_=type;
    for(std::size_t i=0;i<7;++i) {
        auto& k=tube_[i];
        k.slider.onValueChange={};k.slider.onDragStart={};k.slider.onSelected={};
        k.attachment.reset();
        const auto* id=type>=2?p6plugin::kCustomIDs[static_cast<std::size_t>(type-2)][i]:p6plugin::kTubeIDs[i];
        const auto* name=type>=2?p6plugin::kCustomNames[static_cast<std::size_t>(type-2)][i]:p6plugin::kTubeNames[i];
        k.slider.setTitle(name);k.label.setText(name,juce::dontSendNotification);
        k.slider.setTooltip(juce::String(p6plugin::kCharacterNames[static_cast<std::size_t>(type)])+" "+name+". Drag, arrow keys, Shift for fine adjustment; double-click to reset.");
        k.attachment=std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor_.parameters(),id,k.slider);
        k.slider.setDoubleClickReturnValue(true,type>=2?p6plugin::kCustomDefaults[static_cast<std::size_t>(type-2)][i]:p6plugin::kTubeDefaults[i]);
        k.slider.setWantsKeyboardFocus(true);k.slider.setDescription(k.slider.getTooltip());
        const bool drive=i==0&&type!=4;
        const double minimum=type==1?-12.:type==3?-12.:0.,span=type==1?48.:type==3?24.:type==5?24.:36.;
        k.slider.textFromValueFunction=[i,type,drive,minimum,span](double v){
            if(i==6)return juce::String(-24.+36.*v,1)+" dB";
            if(drive)return juce::String(minimum+span*v,1)+" dB";
            if(type==4&&i==0)return juce::String(20.*std::pow(900.,v),1)+" Hz";
            if(type==4&&i==3)return juce::String(10.+290.*v,1)+" ms";
            if(type==4&&i==4)return juce::String(200.*v-100.,0)+" %";
            return juce::String(v*100.,0)+" %";
        };
        k.slider.valueFromTextFunction=[i,type,drive,minimum,span,&k](const juce::String& text){
            const juce::String unit=i==6||drive?"dB":type==4&&i==0?"Hz":type==4&&i==3?"ms":"%";
            const auto clean=text.trim().replace(unit,"",true).trim().toStdString();char* end=nullptr;
            const double v=std::strtod(clean.c_str(),&end);
            if(end==clean.c_str()||!end||*end!='\0'||!std::isfinite(v))return k.slider.getValue();
            double normalized=v/100.;
            if(i==6)normalized=(v+24.)/36.;
            else if(drive)normalized=(v-minimum)/span;
            else if(type==4&&i==0)normalized=v>0?std::log(v/20.)/std::log(900.):-1.;
            else if(type==4&&i==3)normalized=(v-10.)/290.;
            else if(type==4&&i==4)normalized=(v+100.)/200.;
            return normalized>=0.&&normalized<=1.?normalized:k.slider.getValue();
        };
        k.slider.onValueChange=[this,&k]{focusNrpn_=-1;focusName_=k.slider.getTitle();focusValue_=k.slider.getTextFromValue(k.slider.getValue());refreshDisplay();updateDevice();};
        k.slider.onDragStart=k.slider.onValueChange;k.slider.onSelected=k.slider.onValueChange;
        k.slider.updateText();
    }
}
void P6PluginEditor::updateDevice()
{
    if(boundType_<2)return;
    std::array<float,7> values{};
    for(std::size_t i=0;i<7;++i)values[i]=processor_.parameters().getRawParameterValue(p6plugin::kCustomIDs[static_cast<std::size_t>(boundType_-2)][i])->load();
    auto reading=visual_;reading.active=reading.active&&reading.type==boundType_;
    device_.update(boundType_,values,reading,motion_.getToggleState());
}
