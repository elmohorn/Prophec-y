#include "PluginEditor.h"
#include <algorithm>
#include <cmath>

namespace {
const juce::Colour ink{0xff252827},muted{0xff636a68},amber{0xffe68c40},silver{0xffd7d6d0};
juce::String hardwareID(int nrpn) { return "nrpn_"+juce::String(nrpn).paddedLeft('0',4); }
constexpr float designWidth=1460.f;
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
    g.setColour(juce::Colours::black.withAlpha(.12f));g.fillEllipse(cx-r-2,cy-r+4,2*r+4,2*r+4);
    g.setGradientFill(juce::ColourGradient(juce::Colour{0xffeff0e9},cx-r,cy-r,juce::Colour{0xff777d79},cx+r,cy+r,false));
    g.fillEllipse(cx-r-1,cy-r-1,2*r+2,2*r+2);
    g.setColour(juce::Colour{0xff171c1c});g.fillEllipse(cx-r,cy-r,2*r,2*r);
    g.setGradientFill(juce::ColourGradient(juce::Colour{0xff454c4b},cx-r*.5f,cy-r,juce::Colour{0xff222827},cx+r*.6f,cy+r,false));
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
P6PluginEditor::P6PluginEditor(P6AudioProcessor& p):AudioProcessorEditor(p),processor_(p)
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
    for(auto* c:std::array<juce::Component*,10>{&module_,&studio_,&previous_,&next_,&collapse_,&bypass_,&motion_,&type_,&position_,&keyTrack_}) {
        c->setLookAndFeel(&look_);addAndMakeVisible(c);
    }
    for(auto* c:{&type_,&position_,&keyTrack_}) c->setColour(juce::ComboBox::textColourId,ink);
    type_.addItem("None",1);type_.addItem("Tube",2);type_.setTitle("Prophec-y effect");
    position_.addItem("START OF FX",1);position_.addItem("END OF FX",2);position_.setTitle("Effect position");
    keyTrack_.addItem("OFF",1);keyTrack_.addItem("HALF",2);keyTrack_.addItem("FULL",3);keyTrack_.setTitle("Low-pass keyboard tracking");
    auto& params=p.parameters();
    typeBinding_=std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(params,p6plugin::kCharacterTypeID,type_);
    positionBinding_=std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(params,p6plugin::kCharacterPositionID,position_);
    keyBinding_=std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(params,hardwareID(47),keyTrack_);
    bypassBinding_=std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(params,p6plugin::kCharacterBypassID,bypass_);
    previous_.onClick=[this]{setType(0);};next_.onClick=[this]{setType(1);};
    module_.onClick=[this]{studioView_=false;refreshVisibility();};studio_.onClick=[this]{studioView_=true;refreshVisibility();};
    collapse_.onClick=[this]{collapsed_=!collapsed_;refreshVisibility();};motion_.setToggleState(true,juce::dontSendNotification);
    motion_.setTooltip("Pause the light animation; audio continues.");collapse_.setTooltip("Hide the device, keeping its audio active.");
    type_.setTooltip("None removes the character effect; Tube settings are retained.");
    position_.setTooltip("Start: before distortion, FX A and FX B. End: after FX B.");
    constexpr std::array<int,36> ids{0,3,4,2,5,8,9,7,6,49,45,46,77,143,144,88,89,90,78,79,80,81,67,68,69,70,30,33,63,62,120,128,2,7,27,32};
    constexpr std::array<const char*,36> labels{"FREQ","SHAPE","PULSE WIDTH","LEVEL","FREQ","SHAPE","PULSE WIDTH","LEVEL","FINE","HIGH PASS","LOW PASS","RESONANCE","ENV AMOUNT","FILTER ENV","OSC 2","RATE","INITIAL AMT","WAVEFORM","ATTACK","DECAY","SUSTAIN","RELEASE","ATTACK","DECAY","SUSTAIN","RELEASE","GLIDE","SLOP","PAN SPREAD","OUTPUT","MIX A","MIX B","OSC 1","OSC 2","SUB","NOISE"};
    for(std::size_t i=0;i<synth_.size();++i)makeKnob(synth_[i],hardwareID(ids[i]),labels[i]);
    for(std::size_t i=30;i<synth_.size();++i)synth_[i].slider.setSliderStyle(i<32?juce::Slider::LinearHorizontal:juce::Slider::LinearVertical);
    constexpr std::array<int,18> switchIds{1,10,11,156,29,48,135,91,145,146,147,148,149,93,94,95,97,98};
    constexpr std::array<const char*,18> switchNames{"SYNC","LOW FREQ","KEYBOARD","UNISON","GLIDE ON","VELOCITY","FX ON","SYNC","FREQ 1","SHAPE 1","PW 1","LP","HP","OSC 1","OSC 2","PW 1+2","LP","HP"};
    for(std::size_t i=0;i<switches_.size();++i) {
        auto& sw=switches_[i];sw.button.setLookAndFeel(&look_);sw.button.setButtonText(switchNames[i]);
        sw.button.setTitle(juce::String(i<13&&i>=8?"Poly Mod ":i>=13?"LFO ":"")+switchNames[i]);
        if(i>=8)sw.button.getProperties().set("ledTile",true);
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
        const auto c=static_cast<juce::uint8>(196+random.nextInt(11)+(y%3==0?2:0));grain_.setPixelAt(x,y,juce::Colour(c,c,static_cast<juce::uint8>(c-2)));
    }
    diagnostic_=std::make_unique<juce::GenericAudioProcessorEditor>(p);addChildComponent(*diagnostic_);
    presetName_=p.getProgramName(0);glow_=std::min(1.f,p.characterExcitation()*4.f);
    focusValue_=juce::String(static_cast<int>(params.getRawParameterValue(hardwareID(45))->load()))+" RAW";
    setResizable(true,false);setResizeLimits(1240,compactHeight,1920,1200);setSize(1460,compactHeight);
    refreshVisibility();startTimerHz(30);
}
P6PluginEditor::~P6PluginEditor(){stopTimer();setLookAndFeel(nullptr);}
void P6PluginEditor::makeKnob(Knob& k,const juce::String& id,const juce::String& name,bool values)
{
    k.slider.setLookAndFeel(&look_);k.slider.setColour(juce::Slider::textBoxTextColourId,ink);
    k.slider.setColour(juce::Slider::textBoxOutlineColourId,juce::Colours::transparentBlack);
    k.slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    k.slider.setTextBoxStyle(values?juce::Slider::TextBoxBelow:juce::Slider::NoTextBox,false,90,20);
    k.slider.setPopupDisplayEnabled(!values,!values,this);
    k.slider.setTitle(name);k.slider.setTooltip(name+" — drag to edit; Studio provides numeric entry.");
    k.label.setText(name,juce::dontSendNotification);k.label.setColour(juce::Label::textColourId,ink);
    k.label.setFont(juce::Font(10.5f).withExtraKerningFactor(.05f));k.label.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(k.slider);addAndMakeVisible(k.label);
    k.attachment=std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor_.parameters(),id,k.slider);
    auto focus=[this,&k,name,values]{focusName_=name;focusValue_=k.slider.getTextFromValue(k.slider.getValue())+(values?"":" RAW");focusPosition_=static_cast<float>(k.slider.valueToProportionOfLength(k.slider.getValue()));repaint(displayArea_);};
    k.slider.onDragStart=focus;k.slider.onValueChange=focus;
}
void P6PluginEditor::setType(int value)
{
    if(auto* p=processor_.parameters().getParameter(p6plugin::kCharacterTypeID)){p->beginChangeGesture();p->setValueNotifyingHost(p->convertTo0to1(static_cast<float>(value)));p->endChangeGesture();}refreshVisibility();
}
void P6PluginEditor::timerCallback()
{
    if(!isShowing())return;
    const bool selected=processor_.parameters().getRawParameterValue(p6plugin::kCharacterTypeID)->load()>=.5f;
    if(selected!=showTube_)refreshVisibility();
    const auto name=processor_.getProgramName(0);if(name!=presetName_){presetName_=name;repaint(displayArea_);}
    if(showTube_&&!collapsed_&&!studioView_&&motion_.getToggleState()){glow_=.8f*glow_+.2f*std::min(1.f,processor_.characterExcitation()*4.f);repaint(tubeArea_);}
}
void P6PluginEditor::refreshVisibility()
{
    showTube_=processor_.parameters().getRawParameterValue(p6plugin::kCharacterTypeID)->load()>=.5f;
    for(auto& k:synth_){k.slider.setVisible(!studioView_);k.label.setVisible(!studioView_);}
    for(auto& s:switches_)s.button.setVisible(!studioView_);
    for(auto& k:tube_){k.slider.setVisible(!studioView_&&showTube_&&!collapsed_);k.label.setVisible(k.slider.isVisible());}
    diagnostic_->setVisible(studioView_);
    for(auto* c:std::array<juce::Component*,8>{&previous_,&next_,&type_,&position_,&bypass_,&collapse_,&motion_,&keyTrack_})c->setVisible(!studioView_);
    collapse_.setVisible(!studioView_&&showTube_);motion_.setVisible(!studioView_&&showTube_&&!collapsed_);
    position_.setEnabled(showTube_);bypass_.setEnabled(showTube_);previous_.setEnabled(showTube_);next_.setEnabled(!showTube_);
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
    module_.setBounds(rect(1190,36,90,27));studio_.setBounds(rect(1290,36,90,27));
    diagnostic_->setBounds(36,92,getWidth()-72,getHeight()-125);
    auto knob=[&](std::size_t i,float x,float y,float w=80,float h=66){synth_[i].slider.setBounds(rect(x,y,w,h));synth_[i].label.setBounds(rect(x-5,y+h+1,w+10,20));};
    for(std::size_t i=0;i<4;++i){knob(i,48+94.f*static_cast<float>(i),127);knob(i+4,48+94.f*static_cast<float>(i),277);}
    knob(8,350,371,58,48);knob(9,492,359,85,78);knob(10,645,317,133,126);knob(11,797,359,92,78);knob(12,914,359,92,78);
    knob(13,1044,125,88,66);knob(14,1150,125,88,66);
    knob(15,1044,265,85,60);knob(16,1150,265,85,60);knob(17,1256,265,85,60);
    for(std::size_t i=0;i<4;++i){knob(18+i,1040+92.f*static_cast<float>(i),397,82,60);knob(22+i,1040+92.f*static_cast<float>(i),507,82,60);}
    for(std::size_t i=0;i<4;++i)knob(26+i,585+105.f*static_cast<float>(i),548,78,62);
    for(std::size_t i=0;i<2;++i){synth_[30+i].slider.setBounds(rect(1135+137.f*static_cast<float>(i),608,107,20));synth_[30+i].label.setBounds(rect(1135+137.f*static_cast<float>(i),588,107,18));}
    for(std::size_t i=0;i<4;++i){synth_[32+i].slider.setBounds(rect(65+92.f*static_cast<float>(i),471,62,122));synth_[32+i].label.setBounds(rect(60+92.f*static_cast<float>(i),598,72,18));}
    switches_[0].button.setBounds(rect(58,379,78,24));switches_[1].button.setBounds(rect(152,379,94,24));switches_[2].button.setBounds(rect(250,379,100,24));
    switches_[3].button.setBounds(rect(498,559,85,24));switches_[4].button.setBounds(rect(498,589,85,24));switches_[5].button.setBounds(rect(804,477,114,24));
    switches_[6].button.setBounds(rect(1048,606,78,24));switches_[7].button.setBounds(rect(1343,280,66,24));
    for(std::size_t i=0;i<5;++i){switches_[8+i].button.setBounds(rect(1050+69.f*static_cast<float>(i),207,63,31));switches_[13+i].button.setBounds(rect(1050+69.f*static_cast<float>(i),346,63,31));}
    keyTrack_.setBounds(rect(590,477,135,24));
    previous_.setBounds(rect(304,644,27,24));type_.setBounds(rect(340,644,135,24));next_.setBounds(rect(484,644,27,24));
    position_.setBounds(rect(551,644,151,24));bypass_.setBounds(rect(733,644,90,24));collapse_.setBounds(rect(1190,644,90,24));motion_.setBounds(rect(1301,644,91,24));
    displayArea_=rect(490,104,511,142);tubeArea_={32,688,getWidth()-64,std::max(225,getHeight()-712)};
    for(std::size_t i=0;i<tube_.size();++i){const float x=390+140.f*static_cast<float>(i);tube_[i].label.setBounds(rect(x,738,120,20));tube_[i].slider.setBounds(rect(x,760,120,118));}
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
    g.setGradientFill(juce::ColourGradient(juce::Colour{0xffe8e6df},36,10,juce::Colour{0xffc8c8c0},w*.75f,h,false));g.fillRoundedRectangle(face,7.f);
    g.setTiledImageFill(grain_,0,0,.22f);g.fillRoundedRectangle(face,7.f);
    g.setColour(juce::Colours::white.withAlpha(.78f));g.drawRoundedRectangle(face.reduced(.5f),7.f,1.f);
    g.setColour(juce::Colour{0xff727c74});g.drawLine(22,24,22,h-29,2.f);g.drawLine(w-22,24,w-22,h-29,2.f);
}
void P6PluginEditor::paintReadout(juce::Graphics& g)
{
    auto r=displayArea_.toFloat();g.setColour(juce::Colour{0xff8b948b});g.fillRoundedRectangle(r.expanded(2),6.f);
    g.setGradientFill(juce::ColourGradient(juce::Colour{0xff162024},r.getX(),r.getY(),juce::Colour{0xff080f12},r.getRight(),r.getBottom(),false));g.fillRoundedRectangle(r,4.f);
    g.setColour(juce::Colours::white.withAlpha(.12f));g.drawRoundedRectangle(r.reduced(.5f),4.f,1.f);
    g.setColour(juce::Colour{0xffcee2e5});g.setFont(18.f);g.drawText(presetName_,displayArea_.reduced(19,12).removeFromTop(25),juce::Justification::centredLeft);
    g.setColour(juce::Colour{0xff708f98});g.setFont(10.f);g.drawText("SIX VOICES",displayArea_.getRight()-110,displayArea_.getY()+18,90,18,juce::Justification::centredRight);
    g.setColour(juce::Colour{0xff476269}.withAlpha(.4f));g.drawLine(r.getX()+18,r.getY()+48,r.getRight()-18,r.getY()+48,1.f);
    const auto graph=juce::Rectangle<float>(r.getX()+20,r.getY()+64,r.getWidth()*.43f,54);
    juce::Path curve;curve.startNewSubPath(graph.getX(),graph.getY()+8);
    const float knee=graph.getX()+graph.getWidth()*(.15f+.55f*focusPosition_);
    curve.lineTo(knee,graph.getY()+8);curve.cubicTo(knee+20,graph.getY()+8,knee+25,graph.getBottom()-7,graph.getRight(),graph.getBottom()-7);
    g.setColour(juce::Colour{0xffb6d2db});g.strokePath(curve,juce::PathStrokeType(1.3f));
    g.setColour(juce::Colour{0xff6d929d});g.setFont(8.f);g.drawText("CONTROL POSITION",static_cast<int>(graph.getX()),static_cast<int>(graph.getBottom()),170,14,juce::Justification::centredLeft);
    const int tx=static_cast<int>(r.getX()+r.getWidth()*.54f),tw=static_cast<int>(r.getWidth()*.42f);
    g.setColour(juce::Colour{0xff8eb1bb});g.setFont(11.f);g.drawText(focusName_,tx,static_cast<int>(r.getY()+66),tw,20,juce::Justification::centredLeft);
    g.setColour(juce::Colour{0xffd6e6e9});g.setFont(22.f);g.drawText(focusValue_,tx,static_cast<int>(r.getY()+90),tw,30,juce::Justification::centredLeft);
}
void P6PluginEditor::paintTube(juce::Graphics& g)
{
    const float sx=static_cast<float>(getWidth())/designWidth;
    g.setColour(muted.withAlpha(.3f));g.drawLine(52*sx,698,1408*sx,698,1.f);
    section(g,"TUBE / 01",65,711,270);
    g.setColour(muted);g.setFont(10.f);g.drawText("VALVE SATURATION",static_cast<int>(390*sx),710,400,18,juce::Justification::centredLeft);
    const float x=115*sx,y=752;
    const float energy=motion_.getToggleState()?glow_:.12f;
    g.setColour(juce::Colour{0xff4a534c});g.fillRoundedRectangle(x-38,y-20,200*sx,150,12);
    g.setGradientFill(juce::ColourGradient(juce::Colour{0xff141c19},x,y-20,juce::Colour{0xff566052},x+160*sx,y+130,false));g.fillRoundedRectangle(x-35,y-17,194*sx,145,10);
    g.setGradientFill(juce::ColourGradient(amber.withAlpha(.14f+.35f*energy),x+65*sx,y+75,amber.withAlpha(0.f),x+157*sx,y+75,true));g.fillEllipse(x-30,y-10,185*sx,145);
    const float bw=93*sx,bh=126;
    juce::Path glass;glass.startNewSubPath(x,y+bh);glass.lineTo(x,y+34);glass.cubicTo(x,y-4,x+bw,y-4,x+bw,y+34);glass.lineTo(x+bw,y+bh);glass.closeSubPath();
    g.setGradientFill(juce::ColourGradient(juce::Colour{0xffd9eee5}.withAlpha(.24f),x,y,juce::Colour{0xff758479}.withAlpha(.08f),x+bw,y+bh,false));g.fillPath(glass);
    g.setColour(juce::Colour{0xffdce3d4}.withAlpha(.48f));g.strokePath(glass,juce::PathStrokeType(1.f));
    g.setColour(juce::Colour{0xff252d29});g.fillRoundedRectangle(x+22*sx,y+31,49*sx,65,3.f);
    for(int i=0;i<8;++i){g.setColour(juce::Colour{0xff637166}.withAlpha(.6f));g.drawLine(x+26*sx,y+37+static_cast<float>(i)*7,x+67*sx,y+37+static_cast<float>(i)*7,1.f);}
    g.setGradientFill(juce::ColourGradient(juce::Colour{0xffffa54c}.withAlpha(.4f+.55f*energy),x+46*sx,y+80,amber.withAlpha(0.f),x+76*sx,y+80,true));g.fillEllipse(x+12*sx,y+41,69*sx,77);
    g.setColour(juce::Colour{0xffffbe77}.withAlpha(.55f+.45f*energy));
    juce::Path heater;heater.startNewSubPath(x+38*sx,y+106);for(int i=0;i<10;++i)heater.lineTo(x+(i%2==0?38.f:55.f)*sx,y+98-static_cast<float>(i)*5.f);g.strokePath(heater,juce::PathStrokeType(1.6f));
    g.setColour(juce::Colours::white.withAlpha(.32f));g.drawLine(x+13*sx,y+26,x+13*sx,y+92,3.f);
    g.setColour(juce::Colour{0xff222c25});g.fillRoundedRectangle(x-8*sx,y+117,109*sx,16,3.f);
    for(int i=0;i<5;++i){g.setColour(juce::Colour{0xff909b83});g.drawLine(x+static_cast<float>(i+1)*15*sx,y+133,x+static_cast<float>(i+1)*15*sx,y+141,2.f);}
}
void P6PluginEditor::paint(juce::Graphics& g)
{
    paintSurface(g);const float sx=static_cast<float>(getWidth())/designWidth;
    g.setColour(ink);g.setFont(juce::Font(24.f).withExtraKerningFactor(.22f));g.drawText("PROPHEC-Y",static_cast<int>(55*sx),33,370,34,juce::Justification::centredLeft);
    g.setColour(muted);g.setFont(9.f);g.drawText("P O L Y P H O N I C   S Y N T H E S I Z E R",static_cast<int>(55*sx),70,420,16,juce::Justification::centredLeft);
    if(studioView_)return;
    section(g,"OSCILLATOR 1",56,103,360);section(g,"OSCILLATOR 2",56,253,360);section(g,"MIXER",56,441,360);
    section(g,"FILTER",496,284,505);section(g,"PERFORMANCE",496,519,505);
    section(g,"POLY MOD",1050,103,357);section(g,"LFO",1050,244,357);section(g,"FILTER ENVELOPE",1050,375,357);section(g,"AMPLIFIER ENVELOPE",1050,485,357);
    g.setColour(muted.withAlpha(.3f));g.drawLine(445*sx,107,445*sx,615,1.f);g.drawLine(1027*sx,107,1027*sx,615,1.f);
    paintReadout(g);
    g.setColour(muted);g.setFont(10.f);g.drawText("KEY TRACK",static_cast<int>(497*sx),477,static_cast<int>(90*sx),24,juce::Justification::centredLeft);
    g.setColour(muted.withAlpha(.3f));g.drawLine(52*sx,633,1408*sx,633,1.f);
    g.setColour(ink);g.setFont(juce::Font(11.f).withExtraKerningFactor(.09f));g.drawText("PROPHEC-Y / CHARACTER",static_cast<int>(55*sx),644,static_cast<int>(240*sx),24,juce::Justification::centredLeft);
    if(showTube_&&!collapsed_)paintTube(g);
}
