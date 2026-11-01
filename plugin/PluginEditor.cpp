#include "PluginEditor.h"
#include <algorithm>
#include <cmath>

namespace {
const juce::Colour panel{0xffd8d9d5},ink{0xff252c30},muted{0xff586369},amber{0xffdb7a2c};
}
void P6PluginEditor::MetalLook::drawRotarySlider(juce::Graphics& g,int x,int y,int w,int h,
                                                float value,float start,float end,juce::Slider&)
{
    const float r=static_cast<float>(std::min(w,h))*.5f-7.f;
    const float cx=static_cast<float>(x)+static_cast<float>(w)*.5f;
    const float cy=static_cast<float>(y)+static_cast<float>(h)*.5f;
    g.setColour(juce::Colours::black.withAlpha(.12f));g.fillEllipse(cx-r-1,cy-r+3,2*r+2,2*r+2);
    g.setGradientFill(juce::ColourGradient(juce::Colour{0xff4b5354},cx,cy-r,juce::Colour{0xff171f21},cx,cy+r,false));
    g.fillEllipse(cx-r,cy-r,2*r,2*r);
    g.setColour(juce::Colours::white.withAlpha(.2f));g.drawEllipse(cx-r,cy-r,2*r,2*r,1.f);
    juce::Path track;track.addCentredArc(cx,cy,r+4,r+4,0,start,end,true);
    g.setColour(muted.withAlpha(.35f));g.strokePath(track,juce::PathStrokeType(1.f));
    const float angle=start+(end-start)*value;
    g.setColour(juce::Colour{0xffe9bd79});
    g.drawLine(cx+std::sin(angle)*r*.53f,cy-std::cos(angle)*r*.53f,
               cx+std::sin(angle)*r*.86f,cy-std::cos(angle)*r*.86f,2.5f);
}
P6PluginEditor::P6PluginEditor(P6AudioProcessor& p):AudioProcessorEditor(p),processor_(p)
{
    look_.setColour(juce::ResizableWindow::backgroundColourId,panel);
    look_.setColour(juce::Slider::rotarySliderFillColourId,amber);
    look_.setColour(juce::Slider::rotarySliderOutlineColourId,juce::Colour{0xff8c9697});
    look_.setColour(juce::Slider::thumbColourId,ink);
    look_.setColour(juce::Slider::textBoxTextColourId,ink);
    look_.setColour(juce::Slider::textBoxBackgroundColourId,juce::Colours::transparentBlack);
    look_.setColour(juce::Slider::textBoxOutlineColourId,juce::Colours::transparentBlack);
    look_.setColour(juce::Label::textColourId,ink);
    look_.setColour(juce::ToggleButton::textColourId,ink);
    look_.setColour(juce::TextButton::buttonColourId,juce::Colour{0xffb9bfbd});
    look_.setColour(juce::TextButton::textColourOffId,ink);
    look_.setColour(juce::ComboBox::backgroundColourId,juce::Colour{0xffe8e9e3});
    look_.setColour(juce::ComboBox::textColourId,ink);
    setLookAndFeel(&look_);
    glow_=std::min(1.f,p.characterExcitation()*4.f);
    for(auto* c:std::array<juce::Component*,9>{&module_,&studio_,&previous_,&next_,&collapse_,&bypass_,&motion_,&type_,&position_})
        if(c!=nullptr) {c->setLookAndFeel(&look_);addAndMakeVisible(c);}
    for(auto* c:{&type_,&position_}) {
        c->setColour(juce::ComboBox::textColourId,ink);
        c->setColour(juce::ComboBox::arrowColourId,muted);
    }
    for(auto* c:{&bypass_,&motion_}) {
        c->setColour(juce::ToggleButton::textColourId,ink);
        c->setColour(juce::ToggleButton::tickColourId,amber);
    }
    type_.addItem("None",1); type_.addItem("Tube",2);
    type_.setTitle("Prophec-y effect");
    position_.addItem("Start of FX chain",1); position_.addItem("End of FX chain",2);
    position_.setTitle("Effect position");
    typeBinding_=std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(p.parameters(),p6plugin::kCharacterTypeID,type_);
    positionBinding_=std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(p.parameters(),p6plugin::kCharacterPositionID,position_);
    bypassBinding_=std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(p.parameters(),p6plugin::kCharacterBypassID,bypass_);
    previous_.onClick=[this]{setType(0);}; next_.onClick=[this]{setType(1);};
    module_.onClick=[this]{studioView_=false;refreshVisibility();};
    studio_.onClick=[this]{studioView_=true;refreshVisibility();};
    collapse_.onClick=[this]{collapsed_=!collapsed_;refreshVisibility();};
    motion_.setToggleState(true,juce::dontSendNotification);
    motion_.setTooltip("Disable signal animation; audio keeps running.");
    collapse_.setTooltip("Hide module controls; the selected effect keeps running.");
    type_.setTooltip("None removes the character effect. Tube settings are retained.");
    position_.setTooltip("Start: before distortion, FX A and FX B. End: after FX B.");
    constexpr std::array<int,10> nrpns{2,7,45,46,49,33,67,68,69,70};
    constexpr std::array<const char*,10> names{"Osc 1 level","Osc 2 level","LP cutoff","Resonance","HP cutoff","Slop","Attack","Decay","Sustain","Release"};
    for(std::size_t i=0;i<synth_.size();++i)
        makeKnob(synth_[i],"nrpn_"+juce::String(nrpns[i]).paddedLeft('0',4),names[i]);
    for(std::size_t i=0;i<tube_.size();++i) {
        makeKnob(tube_[i],p6plugin::kTubeIDs[i],p6plugin::kTubeNames[i]);
        tube_[i].slider.textFromValueFunction=[i](double v) {
            if(i==0) return juce::String(-12.+48.*v,1)+" dB";
            if(i==6) return juce::String(-24.+36.*v,1)+" dB";
            return juce::String(v*100.,0)+" %";
        };
        tube_[i].slider.valueFromTextFunction=[i](const juce::String& s) {
            const auto value=s.getDoubleValue();
            return i==0?(value+12.)/48.:i==6?(value+24.)/36.:value/100.;
        };
        tube_[i].slider.updateText();
    }
    diagnostic_=std::make_unique<juce::GenericAudioProcessorEditor>(p);
    addChildComponent(*diagnostic_);
    setResizable(true,false); setResizeLimits(1000,500,1600,1100); setSize(1180,790);
    refreshVisibility(); startTimerHz(30);
}
P6PluginEditor::~P6PluginEditor() { stopTimer(); setLookAndFeel(nullptr); }
void P6PluginEditor::makeKnob(Knob& k,const juce::String& id,const juce::String& name)
{
    k.slider.setLookAndFeel(&look_);
    k.slider.setColour(juce::Slider::textBoxTextColourId,ink);
    k.slider.setColour(juce::Slider::textBoxOutlineColourId,juce::Colours::transparentBlack);
    k.slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    k.slider.setTextBoxStyle(juce::Slider::TextBoxBelow,false,88,22);
    k.slider.setTitle(name); k.slider.setTooltip(name+" — drag, shift for fine adjustment, or enter a value.");
    k.label.setText(name,juce::dontSendNotification); k.label.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(k.slider); addAndMakeVisible(k.label);
    k.attachment=std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor_.parameters(),id,k.slider);
}
void P6PluginEditor::setType(int value)
{
    if(auto* p=processor_.parameters().getParameter(p6plugin::kCharacterTypeID)) {
        p->beginChangeGesture();p->setValueNotifyingHost(p->convertTo0to1(static_cast<float>(value)));p->endChangeGesture();
    }
    refreshVisibility();
}
void P6PluginEditor::timerCallback()
{
    if(!isShowing()) return;
    const bool selected=processor_.parameters().getRawParameterValue(p6plugin::kCharacterTypeID)->load()>=.5f;
    if(selected!=showTube_) refreshVisibility();
    if(showTube_&&!collapsed_&&!studioView_&&motion_.getToggleState()) {
        glow_=.8f*glow_+.2f*std::min(1.f,processor_.characterExcitation()*4.f);
        repaint(tubeArea_);
    }
}
void P6PluginEditor::refreshVisibility()
{
    showTube_=processor_.parameters().getRawParameterValue(p6plugin::kCharacterTypeID)->load()>=.5f;
    for(auto& k:synth_) {k.slider.setVisible(!studioView_);k.label.setVisible(!studioView_);}
    for(auto& k:tube_) {k.slider.setVisible(!studioView_&&showTube_&&!collapsed_);k.label.setVisible(k.slider.isVisible());}
    diagnostic_->setVisible(studioView_);
    for(auto* c:std::array<juce::Component*,7>{&previous_,&next_,&type_,&position_,&bypass_,&collapse_,&motion_}) c->setVisible(!studioView_);
    position_.setEnabled(showTube_); bypass_.setEnabled(showTube_);
    collapse_.setVisible(!studioView_&&showTube_); motion_.setVisible(!studioView_&&showTube_&&!collapsed_);
    collapse_.setButtonText(collapsed_?"Expand":"Collapse");
    previous_.setEnabled(showTube_); next_.setEnabled(!showTube_);
    const bool expanded=studioView_||(showTube_&&!collapsed_);
    setResizeLimits(1000,expanded?700:500,1600,1100);
    if(!expanded&&getHeight()>500) {expandedHeight_=getHeight();setSize(getWidth(),500);}
    else if(expanded&&getHeight()<700) setSize(getWidth(),expandedHeight_);
    resized();repaint();
}
void P6PluginEditor::resized()
{
    const int w=getWidth();
    module_.setBounds(w-228,25,96,32);studio_.setBounds(w-122,25,96,32);
    diagnostic_->setBounds(24,90,w-48,getHeight()-112);
    const int cell=(w-72)/5;
    for(std::size_t i=0;i<synth_.size();++i) {
        const int x=36+static_cast<int>(i%5)*cell,y=120+static_cast<int>(i/5)*135;
        synth_[i].label.setBounds(x,y,cell,24);synth_[i].slider.setBounds(x+12,y+26,cell-24,101);
    }
    previous_.setBounds(34,413,32,30);type_.setBounds(74,413,190,30);next_.setBounds(272,413,32,30);
    position_.setBounds(325,413,190,30);bypass_.setBounds(535,413,90,30);
    collapse_.setBounds(w-232,413,100,30);motion_.setBounds(w-123,413,110,30);
    tubeArea_={32,463,w-64,std::max(130,getHeight()-500)};
    const int knobWidth=(w-330)/7;
    for(std::size_t i=0;i<tube_.size();++i) {
        const int x=272+static_cast<int>(i)*knobWidth,y=tubeArea_.getY()+65;
        tube_[i].label.setBounds(x,y,knobWidth,24);tube_[i].slider.setBounds(x,y+25,knobWidth,112);
    }
}
void P6PluginEditor::paint(juce::Graphics& g)
{
    g.fillAll(panel);g.setColour(ink);g.setFont(29.f);g.drawText("PROPHEC-Y",32,24,300,38,juce::Justification::centredLeft);
    g.setFont(12.f);g.setColour(muted);g.drawText("SIX VOICES / MODULE + STUDIO / v0.20 PROTOTYPE",34,65,600,18,juce::Justification::centredLeft);
    const juce::Rectangle<int> display(360,22,getWidth()-620,48);
    g.setColour(juce::Colour{0xff263a38});g.fillRoundedRectangle(display.toFloat(),5.f);
    g.setColour(juce::Colour{0xffe9b66b});g.setFont(17.f);
    g.drawText(processor_.getProgramName(0),display.reduced(14,5),juce::Justification::centredLeft);
    if(studioView_) return;
    g.setColour(muted.withAlpha(.35f));g.drawLine(32,106.f,static_cast<float>(getWidth()-32),106.f);
    g.drawLine(32,395.f,static_cast<float>(getWidth()-32),395.f);
    if(!showTube_) {
        g.setColour(muted);g.setFont(17.f);g.drawText("Select a Prophec-y module to open its controls.",32,453,getWidth()-64,30,juce::Justification::centred);
        return;
    }
    if(collapsed_) {
        g.setColour(muted);g.drawText("Tube selected — module controls collapsed",32,453,getWidth()-64,30,juce::Justification::centred);
        return;
    }
    const auto area=tubeArea_.toFloat();
    g.setColour(juce::Colour{0xffb9bfbc});g.fillRoundedRectangle(area,12.f);
    g.setColour(ink);g.setFont(20.f);g.drawText("TUBE / 01",static_cast<int>(area.getX()+24),static_cast<int>(area.getY()+14),210,32,juce::Justification::centredLeft);
    g.setFont(12.f);g.setColour(muted);g.drawText("VALVE SATURATION",272,static_cast<int>(area.getY()+24),400,20,juce::Justification::centredLeft);
    const float x=area.getX()+60.f,y=area.getY()+72.f;
    g.setColour(ink);g.fillRoundedRectangle(x-8,y+119,156,22,5);
    g.setColour(juce::Colour{0xff78847f}.withAlpha(.6f));g.fillRoundedRectangle(x,y,140,126,26);
    g.setColour(juce::Colours::white.withAlpha(.35f));g.drawRoundedRectangle(x,y,140,126,26,1.5f);
    // Signal telemetry drives only light. Animation off shows a fixed lamp.
    const float energy=motion_.getToggleState()?glow_:.12f;
    juce::ColourGradient light(amber.withAlpha(.15f+.65f*energy),x+70,y+74,
                               amber.withAlpha(0.f),x+136,y+110,true);
    g.setGradientFill(light);g.fillEllipse(x+8,y+13,124,112);
    g.setColour(ink.withAlpha(.75f));g.fillRoundedRectangle(x+37,y+30,66,66,6);
    g.setColour(amber.withAlpha(.4f+.6f*energy));
    g.drawLine(x+57,y+40,x+57,y+111,3.f);g.drawLine(x+83,y+40,x+83,y+111,3.f);
    g.setColour(juce::Colours::white.withAlpha(.22f));g.drawLine(x+18,y+28,x+18,y+92,4.f);
    g.setColour(muted);g.setFont(12.f);
    g.drawText("Signal-driven light · fixed host latency: 71 samples",32,getHeight()-30,getWidth()-64,18,juce::Justification::centredRight);
}
