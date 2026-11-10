#include "PluginEditor.h"
#include "BinaryData.h"
#include "ui/controls/SegmentedLedRing.h"
#include <algorithm>
#include <cmath>
#include <utility>
#include <cstdlib>

namespace {
const juce::Colour ink{0xff252827},muted{0xff636a68},amber{0xffd83b31},silver{0xffd7d6d0};
juce::String hardwareID(int nrpn) { return "nrpn_"+juce::String(nrpn).paddedLeft('0',4); }
constexpr float designWidth=1460.f;

// Use the chosen font as embedded outlines, independent of host fonts.
void drawNeonBrand(juce::Graphics& g, float sx)
{
    static const juce::Path lettering = [] {
        const auto face = juce::Typeface::createSystemTypefaceFor(
            BinaryData::michroma_ttf, BinaryData::michroma_ttfSize);
        juce::Font font(face);
        font.setHeight(42.f);
        juce::GlyphArrangement glyphs;
        glyphs.addLineOfText(font, "PROPHEC-Y", 0.f, 0.f);
        juce::Path path;
        glyphs.createPath(path);
        return path;
    }();
    auto path = lettering;
    path.applyTransform(path.getTransformToScaleToFit(60.f*sx, 76.f, 365.f*sx, 44.f, true));
    g.setColour(juce::Colour{0xffff734d}.withAlpha(.045f));
    g.fillPath(path, juce::AffineTransform::translation(0.f, 1.5f));
    for (const auto layer : std::array<std::pair<float,float>,3>{{{9.f,.018f},{5.f,.055f},{2.5f,.19f}}}) {
        g.setColour(juce::Colour{0xffff4e32}.withAlpha(layer.second));
        g.strokePath(path, juce::PathStrokeType(layer.first, juce::PathStrokeType::curved,
                                               juce::PathStrokeType::rounded));
    }
    g.setColour(juce::Colour{0xffffdfcb});
    g.fillPath(path);
}

int nrpnFromID(const juce::String& id)
{
    return id.startsWith("nrpn_") ? id.substring(5).getIntValue() : -1;
}


}
void P6PluginEditor::MetalLook::drawButtonBackground(juce::Graphics& g,juce::Button& button,const juce::Colour& colour,bool highlighted,bool down)
{
    if(button.getTitle()=="Arp octave") {
        const auto r=button.getLocalBounds().toFloat().reduced(4.f,2.f);
        g.setColour(juce::Colour{0xff393d3b});g.fillRoundedRectangle(r,3.f);
        g.setColour(juce::Colour{0xff171919});g.fillRoundedRectangle(r.reduced(2.f),2.f);
        if(button.getToggleState()) {
            const auto c=r.getCentre();
            g.setGradientFill(juce::ColourGradient(juce::Colour{0xfff74a3b}.withAlpha(.48f),c.x,c.y,
                juce::Colour{0xffc82a21}.withAlpha(0.f),c.x+r.getWidth()*.7f,c.y,true));
            g.fillEllipse(r.expanded(4.f));
            g.setColour(juce::Colour{0xffff5544});g.fillRoundedRectangle(r.reduced(5.f),2.f);
        }
        return;
    }
    if(button.getTitle()=="Arpeggiator panel") {
        const auto r=button.getLocalBounds().toFloat().reduced(2.f);
        g.setColour(juce::Colour{0xff555752});g.fillRoundedRectangle(r,4.f);
        g.setGradientFill(juce::ColourGradient(juce::Colour{0xffd7d8d1},r.getX(),r.getY(),juce::Colour{0xff777a74},r.getX(),r.getBottom(),false));
        g.fillRoundedRectangle(r.reduced(2.f),3.f);return;
    }
    const bool preset=button.getTitle()=="Preset browser";
    const bool menu=button.getTitle()=="Header settings";
    if(!preset&&!menu) {
        juce::LookAndFeel_V4::drawButtonBackground(g,button,colour,highlighted,down);return;
    }
    const auto bounds = button.getLocalBounds().toFloat().reduced(1.f);
    const bool active = button.getToggleState();
    g.setColour(juce::Colour{down ? 0xff222122 : 0xff141416});
    g.fillRoundedRectangle(bounds, 5.f);
    const auto edge = juce::Colour{0xffb8a79b};
    g.setColour(edge.withAlpha(highlighted || active ? .62f : preset ? .28f : .18f));
    g.drawRoundedRectangle(bounds, 5.f, .8f);
}
void P6PluginEditor::MetalLook::drawButtonText(juce::Graphics& g, juce::TextButton& button,
                                             bool highlighted, bool down)
{
    if (button.getTitle() == "Header settings") {
        g.setColour(juce::Colour{0xffb7aaa1});
        const auto centre=button.getLocalBounds().toFloat().getCentre();
        for(int dot=-1;dot<=1;++dot)g.fillEllipse(centre.x+static_cast<float>(dot)*6.f-1.3f,centre.y-1.3f,2.6f,2.6f);
        return;
    }
    if (button.getTitle() != "Preset browser") {
        juce::LookAndFeel_V4::drawButtonText(g,button,highlighted,down);
        return;
    }
    g.setColour(juce::Colour{0xffded9d4});
    g.setFont(13.f);
    g.drawText(button.getButtonText(),18,0,108,button.getHeight(),juce::Justification::centredLeft);
}

void P6PluginEditor::MetalLook::drawRotarySlider(juce::Graphics& g,int x,int y,int w,int h,
                                                float value,float start,float end,juce::Slider& slider)
{
    const float size = static_cast<float>(std::min(w,h));
    const float r = std::max(4.f,size*.5f-11.f);
    const float cx = static_cast<float>(x)+static_cast<float>(w)*.5f;
    const float cy = static_cast<float>(y)+static_cast<float>(h)*.5f;
    std::optional<float> zero;
    if (auto* parameter = dynamic_cast<ProphecyParameterControl*>(&slider))
        zero = parameter->ledNeutralProportion();
    else if (slider.getProperties().contains("ledNeutral"))
        zero = static_cast<float>(slider.valueToProportionOfLength(
            static_cast<double>(slider.getProperties()["ledNeutral"])));
    const float segmentWidth = std::clamp(size*.012f,1.f,2.6f);
    const float segmentLength = std::clamp(size*.045f,2.7f,6.f);
    const juce::Rectangle<float> slot(cx-segmentWidth*.5f,cy-r-6.f-segmentLength*.5f,
                                     segmentWidth,segmentLength);
    for (int i=0;i<p6plugin::ui::kLedRingSegments;++i) {
        const float angle = start+(end-start)*static_cast<float>(i)
                                  /static_cast<float>(p6plugin::ui::kLedRingSegments-1);
        const auto state = p6plugin::ui::ledSegmentState(i,value,zero);
        const auto transform = juce::AffineTransform::rotation(angle,cx,cy);
        juce::Path recess,lens;
        recess.addRoundedRectangle(slot.expanded(.65f,.7f),.7f);
        lens.addRoundedRectangle(slot,.45f);
        g.setColour(juce::Colour{0xff858a89});
        g.fillPath(recess,transform.translated(0.f,.4f));
        g.setColour(juce::Colour{0xff3d4040});
        g.fillPath(recess,transform);
        // Fixed 100% intensity, matching the reviewed prototype. Only the
        // final segment is brighter; no animated halo or audio-thread state.
        g.setColour(state.lit ? juce::Colour{state.tip ? 0xffeb3226 : 0xffc92b21}
                             : juce::Colour{0xff3d4040});
        g.fillPath(lens,transform);
    }
    // Cast shadow and contact edge belong to the physical knob above the panel.
    // Keep them inside the slider's bounds so they do not cover nearby labels.
    const bool isMaster=slider.getTitle()=="Master Output";
    for(int layer=5;layer>0;--layer) {
        const float spread=static_cast<float>(layer)*(isMaster?.65f:1.f);
        g.setColour(juce::Colours::black.withAlpha(isMaster?.045f:.035f));
        g.fillEllipse(cx-r-spread,cy-r+2.f-spread*.2f,2*r+2*spread,2*r+2*spread);
    }
    g.setColour(juce::Colours::black.withAlpha(isMaster?.42f:.25f));
    g.fillEllipse(cx-r-1.f,cy-r+1.5f,2*r+2.f,2*r+2.f);
    g.setGradientFill(juce::ColourGradient(juce::Colour{isMaster?0xff686966:0xffeff0e9},cx-r,cy-r,juce::Colour{isMaster?0xff242625:0xff777d79},cx+r,cy+r,false));
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
        g.fillEllipse(xx-5,yy-5,22,22);g.setColour(juce::Colour{0xffff6255});g.fillEllipse(xx+4,yy+4,4,4);
    }
    g.setColour(b.isEnabled()?ink:muted);g.setFont(tile?9.5f:10.5f);
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
    hardware_.setValveAsset(BinaryData::tube_glass_png, BinaryData::tube_glass_pngSize);
#if defined(P6_HAS_CHARACTER_ATLAS)
    hardware_.setPhotoAtlas(BinaryData::character_cameras_atlas_jpg,
                             BinaryData::character_cameras_atlas_jpgSize);
#endif
    // The native RGBA assets take precedence over lossy JPEG segmentation.
    // CMake enables each family only if all four resources are present;
    // invalid images also fall back in CharacterHardwareWindow.
#if defined(P6_HAS_SPRING_RGBA)
    hardware_.setSpringArtwork(
        juce::ImageCache::getFromMemory(BinaryData::spring_backplate_png,
                                        BinaryData::spring_backplate_pngSize),
        std::array<juce::Image,3>{{
            juce::ImageCache::getFromMemory(BinaryData::spring_upper_coil_png,
                                            BinaryData::spring_upper_coil_pngSize),
            juce::ImageCache::getFromMemory(BinaryData::spring_lower_coil_png,
                                            BinaryData::spring_lower_coil_pngSize),
            juce::ImageCache::getFromMemory(BinaryData::spring_reflection_overlay_png,
                                            BinaryData::spring_reflection_overlay_pngSize)
        }});
#endif
#if defined(P6_HAS_OPTICAL_RGBA)
    hardware_.setOpticalArtwork(
        juce::ImageCache::getFromMemory(BinaryData::optical_backplate_png,
                                        BinaryData::optical_backplate_pngSize),
        juce::ImageCache::getFromMemory(BinaryData::optical_lamp_off_png,
                                        BinaryData::optical_lamp_off_pngSize),
        juce::ImageCache::getFromMemory(BinaryData::optical_lamp_on_png,
                                        BinaryData::optical_lamp_on_pngSize),
        juce::ImageCache::getFromMemory(BinaryData::optical_ldr_overlay_png,
                                        BinaryData::optical_ldr_overlay_pngSize));
#endif
    focusReadout_.setTitle("Parameter value");
    focusReadout_.setFont(juce::Font(11.f));
    focusReadout_.setColour(juce::Label::textColourId,juce::Colour{0xffbfb3ac});
    focusReadout_.setJustificationType(juce::Justification::centred);
    focusReadout_.setInterceptsMouseClicks(false,false);
    addAndMakeVisible(focusReadout_);addChildComponent(chamber_);addChildComponent(device_);addChildComponent(hardware_);
    for(auto* c:std::array<juce::Component*,12>{&module_,&studio_,&previous_,&next_,&collapse_,&bypass_,&motion_,&type_,&position_,&keyTrack_,&fxAType_,&fxBType_}) {
        c->setLookAndFeel(&look_);addAndMakeVisible(c);
    }
    for(auto* c:{&type_,&position_,&keyTrack_,&fxAType_,&fxBType_}) c->setColour(juce::ComboBox::textColourId,ink);
    for(int t=0;t<p6plugin::kCharacterTypes;++t)type_.addItem(juce::String(p6plugin::kCharacterNames[static_cast<std::size_t>(t)]).toUpperCase(),t+1);type_.setTitle("Prophec-y effect");
    type_.getProperties().set("characterPlate",true);position_.getProperties().set("characterPlate",true);
    position_.addItem("START OF FX",1);position_.addItem("END OF FX",2);position_.setTitle("Effect position");
    keyTrack_.addItem("OFF",1);keyTrack_.addItem("HALF",2);keyTrack_.addItem("FULL",3);keyTrack_.setTitle("Low-pass keyboard tracking");
    fxAType_.addItemList(juce::StringArray{"A: OFF","A: BBD DELAY","A: DIGITAL DELAY","A: CHORUS","A: PHASER 1","A: PHASER 2"},1);
    fxBType_.addItemList(juce::StringArray{"B: OFF","B: BBD DELAY","B: DIGITAL DELAY","B: CHORUS","B: PHASER 1","B: PHASER 2","B: HALL","B: ROOM","B: PLATE","B: SPRING"},1);
    fxAType_.setTitle("Standard FX A type");fxBType_.setTitle("Standard FX B type");
    midiInputChannel_.setLookAndFeel(&look_);midiInputChannel_.setTitle("MIDI input channel");
    midiInputChannel_.setTooltip("MIDI input: Omni or channel 1–16. Changing channel silences notes, clears standard FX tails and resets controllers.");
    midiInputChannel_.addItem("MIDI: OMNI",1);for(int channel=1;channel<=16;++channel)midiInputChannel_.addItem("MIDI: "+juce::String(channel),channel+1);
    addAndMakeVisible(midiInputChannel_);
    midiMonitor_.setTitle("Recent MIDI input");midiMonitor_.setFont(juce::Font(9.f));
    midiMonitor_.setColour(juce::Label::textColourId,juce::Colour{0xffb9c8bf});
    midiMonitor_.setTooltip("Recent pitch bend and CC input. * marks a message filtered by the selected MIDI channel.");
    midiMonitor_.setText("MIDI: awaiting input",juce::dontSendNotification);addAndMakeVisible(midiMonitor_);
    auto& params=p.parameters();
    arpPanelSwitch_.setTitle("Arpeggiator panel");arpPanelSwitch_.setTooltip("Show or hide the arpeggiator panel");
    addAndMakeVisible(arpPanelSwitch_);arpPanelSwitch_.onClick=[this]{arpOpen_=!arpOpen_;refreshVisibility();};
    for(auto* b:{&arpOn_,&arpHold_,&arpSync_,&arpCharEnabled_,&arpFreeze_}){b->setLookAndFeel(&look_);addAndMakeVisible(b);}
    arpCharEnabled_.setTooltip("Enable the Prophec-Y character layer; classic arpeggiator is unchanged when off.");
    arpFreeze_.setTooltip("Freeze the current eight-step velocity variation.");
    for(auto* b:{&arpMode_,&arpOctaves_,&arpDivision_,&arpClock_}){b->setLookAndFeel(&look_);addAndMakeVisible(b);}
    arpMode_.addItemList(juce::StringArray{"UP","DOWN","UP / DOWN","RANDOM","ASSIGN"},1);
    arpOctaves_.addItemList(juce::StringArray{"1","2","3"},1);
    arpOctaves_.onChange=[this]{
        for(std::size_t i=0;i<arpOctaveButtons_.size();++i)
            arpOctaveButtons_[i].setToggleState(arpOctaves_.getSelectedId()==static_cast<int>(i+1),juce::dontSendNotification);
    };
    arpDivision_.addItemList(juce::StringArray{"1/2","1/4","1/8","1/8 D","1/8 S","1/8 T","1/16","1/16 S","1/16 T","1/32"},1);
    for(std::size_t i=0;i<arpOctaveButtons_.size();++i){
        auto& b=arpOctaveButtons_[i];b.setTitle("Arp octave");b.setButtonText(juce::String(static_cast<int>(i+1)));
        b.setLookAndFeel(&look_);b.setColour(juce::TextButton::textColourOffId,ink);
        b.setTooltip("Arpeggiator range: "+juce::String(static_cast<int>(i+1))+" octave(s)");
        addAndMakeVisible(b);
        b.onClick=[this,i]{arpOctaves_.setSelectedId(static_cast<int>(i+1),juce::sendNotificationSync);};
    }
    arpClock_.addItemList(juce::StringArray{"INTERNAL","HOST","MIDI CLOCK"},1);
    arpMode_.setTitle("Arpeggiator mode");arpOctaves_.setTitle("Arpeggiator octaves");arpDivision_.setTitle("Arpeggiator clock division");
    arpDivision_.setTooltip("Clock divisions follow the Prophet-6 mapping. Swing divisions currently use straight timing pending hardware measurement.");
    arpClock_.setTitle("Arpeggiator clock source");arpSync_.setTooltip("Beat Sync: quantize arpeggiator start to the clock grid.");
    for(auto* s:{&arpRhythm_,&arpMotion_}){s->setLookAndFeel(&look_);s->setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);s->setRotaryParameters(juce::MathConstants<float>::pi*1.25f,juce::MathConstants<float>::pi*2.75f,true);s->setTextBoxStyle(juce::Slider::TextBoxBelow,false,60,17);addAndMakeVisible(s);}
    arpRhythm_.setTitle("Arp Rhythm");arpMotion_.setTitle("Arp Motion");
    arpBpm_.setLookAndFeel(&look_);arpBpm_.setSliderStyle(juce::Slider::LinearHorizontal);
    arpBpm_.setTextBoxStyle(juce::Slider::TextBoxRight,false,50,20);arpBpm_.setTitle("Arpeggiator BPM");addAndMakeVisible(arpBpm_);
    arpRate_.setLookAndFeel(&look_);arpRate_.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    arpRate_.setRotaryParameters(juce::MathConstants<float>::pi*1.25f,juce::MathConstants<float>::pi*2.75f,true);
    arpRate_.setRange(1,10,1);arpRate_.setTextBoxStyle(juce::Slider::TextBoxBelow,false,68,18);
    arpRate_.setTitle("Arpeggiator rate");arpRate_.setTooltip("Clock division; swing ratios are not yet measured from hardware.");
    arpRate_.textFromValueFunction=[](double value){return juce::StringArray{"1/2","1/4","1/8","1/8 D","1/8 S","1/8 T","1/16","1/16 S","1/16 T","1/32"}[static_cast<int>(juce::jlimit(1.0,10.0,value))-1];};
    arpRate_.onValueChange=[this]{arpDivision_.setSelectedId(static_cast<int>(arpRate_.getValue()),juce::sendNotificationSync);};
    arpDivision_.onChange=[this]{arpRate_.setValue(arpDivision_.getSelectedId(),juce::dontSendNotification);};
    addAndMakeVisible(arpRate_);
    arpRhythm_.setTooltip("Number of evenly spaced pulses; the step buttons can mute individual pulses.");
    arpMotion_.setTooltip("Amount of evolving velocity variation.");
    arpOnBinding_=std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(params,hardwareID(160),arpOn_);
    arpHoldBinding_=std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(params,"wrapper_arp_hold",arpHold_);
    arpSyncBinding_=std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(params,"wrapper_arp_beat_sync",arpSync_);
    arpCharacterBinding_=std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(params,p6plugin::kArpCharacterEnabledID,arpCharEnabled_);
    arpFreezeBinding_=std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(params,p6plugin::kArpCharacterFreezeID,arpFreeze_);
    arpModeBinding_=std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(params,hardwareID(161),arpMode_);
    arpOctaveBinding_=std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(params,hardwareID(162),arpOctaves_);
    arpOctaves_.onChange();
    arpDivisionBinding_=std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(params,hardwareID(163),arpDivision_);
    arpRate_.setValue(arpDivision_.getSelectedId(),juce::dontSendNotification);
    arpClockBinding_=std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(params,"wrapper_clock_source",arpClock_);
    arpBpmBinding_=std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(params,hardwareID(167),arpBpm_);
    arpRhythmBinding_=std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(params,p6plugin::kArpCharacterRhythmID,arpRhythm_);
    arpMotionBinding_=std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(params,p6plugin::kArpCharacterMotionID,arpMotion_);
    for(std::size_t i=0;i<arpSteps_.size();++i){
        auto& b=arpSteps_[i];b.setTitle("Arpeggiator step "+juce::String(static_cast<int>(i+1)));
        b.setTooltip("Toggle this step between a note and a rest.");b.setClickingTogglesState(true);addAndMakeVisible(b);
        arpStepBindings_[i]=std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(params,p6plugin::kArpStepIDs[i],b);
    }
    plasmaMode_.setLookAndFeel(&look_);plasmaMode_.setTitle("Plasma algorithm");
    plasmaMode_.setTooltip("Hybrid is the new Plasma sound. Legacy preserves the sound of older sessions.");
    plasmaMode_.addItem("LEGACY",1);plasmaMode_.addItem("HYBRID",2);addChildComponent(plasmaMode_);
    plasmaModeBinding_=std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(params,p6plugin::kPlasmaModeID,plasmaMode_);
    midiChannelBinding_=std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(params,p6plugin::kMidiInputChannelID,midiInputChannel_);
    master_.setLookAndFeel(&look_);master_.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    master_.setRotaryParameters(juce::MathConstants<float>::pi*1.25f,juce::MathConstants<float>::pi*2.75f,true);
    master_.setTextBoxStyle(juce::Slider::TextBoxBelow,false,80,18);master_.setTitle("Master Output");
    master_.setDescription("Final output attenuation, separate from Prophet Program Volume. Minus 60 to zero dB.");
    master_.setTooltip("Master Output after all effects. New instances: -18 dB; legacy sessions: 0 dB. Double-click resets to -18 dB.");
    master_.setColour(juce::Slider::textBoxTextColourId,juce::Colour{0xffe4e9e2});
    master_.setWantsKeyboardFocus(true);addAndMakeVisible(master_);addAndMakeVisible(outputMeter_);
    masterBinding_=std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(params,p6plugin::kMasterOutputID,master_);
    master_.textFromValueFunction=[](double v){return juce::String(v,1)+" dB";};
    master_.valueFromTextFunction=[this](const juce::String& text){
        const auto clean=text.trim().replace("dB", "",true).trim();const auto utf8=clean.toStdString();char* end=nullptr;
        const double value=std::strtod(utf8.c_str(),&end);
        return end!=utf8.c_str()&&end&&*end=='\0'&&std::isfinite(value)&&value>=-60.&&value<=0.?value:master_.getValue();
    };
    master_.setDoubleClickReturnValue(true,p6plugin::kNewMasterDb);master_.updateText();
    master_.onValueChange=[this]{
        focusNrpn_=-1;focusName_="MASTER";
        focusValue_=master_.getTextFromValue(master_.getValue());
        focusTicks_=90;refreshDisplay();
    };
    masterLabel_.setText("MASTER",juce::dontSendNotification);masterLabel_.setFont(juce::Font(10.5f));masterLabel_.setColour(juce::Label::textColourId,juce::Colour{0xffd8e2da});masterLabel_.setJustificationType(juce::Justification::centredLeft);addAndMakeVisible(masterLabel_);
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
    // Secondary host controls live together behind the header menu.
    module_.setVisible(false);studio_.setVisible(false);midiInputChannel_.setVisible(false);midiMonitor_.setVisible(false);clearClips_.setVisible(false);
    headerMenu_.setLookAndFeel(&look_);headerMenu_.setTitle("Header settings");headerMenu_.setTooltip("MIDI channel, view and clip reset");addAndMakeVisible(headerMenu_);
    headerPreset_.setName(processor_.activePresetName());
    headerPreset_.setInterceptsMouseClicks(false,false);
    browserButton_.addAndMakeVisible(headerPreset_);
    headerMenu_.onClick=[this]{
        headerMenu_.setToggleState(true,juce::dontSendNotification);
        juce::PopupMenu menu,midi;const int selected=midiInputChannel_.getSelectedId();
        for(int i=1;i<=17;++i)midi.addItem(100+i,i==1?juce::String("Omni"):"Channel "+juce::String(i-1),true,i==selected);
        menu.addSubMenu("MIDI input",midi);menu.addSeparator();menu.addItem(1,"Module",true,!studioView_);menu.addItem(2,"Studio",true,studioView_);menu.addSeparator();menu.addItem(3,"Reset clip indicators");
        const juce::Component::SafePointer<P6PluginEditor> safe(this);
        menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&headerMenu_),[safe](int id){if(!safe)return;safe->headerMenu_.setToggleState(false,juce::dontSendNotification);if(id==1&&safe->module_.onClick)safe->module_.onClick();else if(id==2&&safe->studio_.onClick)safe->studio_.onClick();else if(id==3&&safe->clearClips_.onClick)safe->clearClips_.onClick();else if(id>=101&&id<=117)safe->midiInputChannel_.setSelectedId(id-100,juce::sendNotificationSync);});
    };
    collapse_.setTitle("Character panel");
    collapse_.onClick=[this]{collapsed_=!collapsed_;refreshVisibility();};motion_.setToggleState(true,juce::dontSendNotification);
    motion_.setTooltip("Pause the light animation; audio continues.");collapse_.setTooltip("Hide the device, keeping its audio active.");
    opticalPreview_.setLookAndFeel(&look_);
    opticalPreview_.setTooltip("Preview the approved optical tremolo hardware design only. No tremolo DSP is present; existing audio and selected Character stay unchanged.");
    opticalPreview_.setClickingTogglesState(false);
    opticalPreview_.onClick=[this] {
        opticalPreviewActive_=!opticalPreviewActive_;
        previewLfoPhase_=0.f;
        previewAudioLevel_=0.f;
        refreshVisibility();
    };
    addAndMakeVisible(opticalPreview_);
    // Optical design preview: real interactive JUCE knobs, not painted dummy
    // circles. Their values exist solely in this editor, never in APVTS.
    constexpr std::array<const char*,3> opticalNames{"RATE","DEPTH","RESPONSE"};
    constexpr std::array<double,3> opticalDefaults{.25,.80,.40};
    for(std::size_t i=0;i<opticalConcept_.size();++i)
    {
        auto& k=opticalConcept_[i];
        k.slider.setLookAndFeel(&look_);
        k.slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    k.slider.setRotaryParameters(juce::MathConstants<float>::pi*1.25f,juce::MathConstants<float>::pi*2.75f,true);
        k.slider.setTextBoxStyle(juce::Slider::TextBoxBelow,false,84,20);
        k.slider.setRange(0.0,1.0,.01);
        k.slider.setValue(opticalDefaults[i],juce::dontSendNotification);
        k.slider.setTitle(juce::String("Optical preview ")+opticalNames[i]);
        k.slider.setTooltip(juce::String("Visual concept only: ")+opticalNames[i]+
                            ". Does not modify audio, MIDI, presets or saved state.");
        k.label.setText(opticalNames[i],juce::dontSendNotification);
        k.label.setFont(juce::Font(10.5f).boldened());
        k.label.setJustificationType(juce::Justification::centred);
        k.label.setColour(juce::Label::textColourId,ink);
        addAndMakeVisible(k.slider);addAndMakeVisible(k.label);
    }
    opticalWave_.setLookAndFeel(&look_);
    opticalWave_.setTitle("Optical preview LFO waveform");
    opticalWave_.setTooltip("Visual preview only; choose the animation waveform. No audio processing.");
    opticalWave_.addItem("SINE",1);
    opticalWave_.addItem("TRIANGLE",2);
    opticalWave_.addItem("RAMP",3);
    opticalWave_.addItem("SQUARE",4);
    opticalWave_.setSelectedId(1,juce::dontSendNotification);
    addAndMakeVisible(opticalWave_);
    type_.setTooltip("Select a Character device. Every device retains its own settings.");
    position_.setTooltip("Start: before distortion, FX A and FX B. End: after FX B.");
    fxAType_.setTooltip("Select standard Prophet-6 FX slot A. Use FX ON to enable the standard chain.");
    fxBType_.setTooltip("Select standard Prophet-6 FX slot B. Slot B also contains the reverbs.");
    constexpr std::array<int,36> ids{0,3,4,2,5,8,9,7,6,49,45,46,77,143,144,88,89,90,78,79,80,81,67,68,69,70,30,33,63,62,120,128,2,7,27,32};
    constexpr std::array<const char*,36> labels{"FREQ","SHAPE","PULSE WIDTH","LEVEL","FREQ","SHAPE","PULSE WIDTH","LEVEL","FINE","HIGH PASS","LOW PASS","RESONANCE","ENV AMOUNT","FILTER ENV","OSC 2","RATE","INITIAL AMT","WAVEFORM","ATTACK","DECAY","SUSTAIN","RELEASE","ATTACK","DECAY","SUSTAIN","RELEASE","GLIDE","SLOP","PAN SPREAD","PROGRAM VOL","MIX A","MIX B","OSC 1","OSC 2","SUB","NOISE"};
    for(std::size_t i=0;i<synth_.size();++i)makeKnob(synth_[i],hardwareID(ids[i]),labels[i]);
    for(std::size_t i=32;i<36;++i)synth_[i].slider.setSliderStyle(juce::Slider::LinearVertical);
    constexpr std::array<int,18> switchIds{1,10,11,156,29,48,135,91,145,146,147,148,149,93,94,95,97,98};
    constexpr std::array<const char*,18> switchNames{"SYNC","LOW FREQ","KEYBOARD","UNISON","GLIDE ON","VELOCITY","FX ON","SYNC","FRQ1","SHP1","PW1","LP","HP","OSC1","OSC2","PW","LP","HP"};
    constexpr std::array<const char*,18> switchDescriptions{"Sync","Low frequency","Keyboard","Unison","Glide on","Velocity","FX on","LFO sync","Oscillator 1 frequency","Oscillator 1 shape","Oscillator 1 pulse width","Low pass","High pass","Oscillator 1 frequency","Oscillator 2 frequency","Oscillators 1 and 2 pulse width","Low pass","High pass"};
    for(std::size_t i=0;i<switches_.size();++i) {
        auto& sw=switches_[i];sw.button.setLookAndFeel(&look_);sw.button.setButtonText(switchNames[i]);
        const auto description=juce::String(i<13&&i>=8?"Poly Mod ":i>=13?"LFO ":"")+switchDescriptions[i];
        sw.button.setTitle(description);sw.button.setTooltip(description);
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
        const auto c=static_cast<juce::uint8>(196+random.nextInt(5)+(y%3==0?1:0));grain_.setPixelAt(x,y,juce::Colour(c,c,c));
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
    setResizable(true,false);setResizeLimits(1240,compactHeight,2000,1500);setSize(1600,compactHeight);
    browser_=std::make_unique<PresetBrowser>(processor_,std::move(presetRoot));addChildComponent(*browser_);
    browserButton_.setLookAndFeel(&look_);browserButton_.setTitle("Preset browser");
    browserButton_.setColour(juce::TextButton::buttonColourId,juce::Colour{0xff242424});
    browserButton_.setColour(juce::TextButton::buttonOnColourId,juce::Colour{0xff59452f});
    browserButton_.setColour(juce::TextButton::textColourOffId,juce::Colour{0xffe8e8e6});
    browserButton_.setColour(juce::TextButton::textColourOnId,juce::Colour{0xffffd399});
    browserButton_.setTooltip(processor_.activePresetName()+" — open preset library");
    browserButton_.setDescription("Browse presets. Current preset: "+processor_.activePresetName());
    addAndMakeVisible(browserButton_);
    browserButton_.onClick=[this]{const bool open=!browser_->isVisible();browser_->setVisible(open);browserButton_.setToggleState(open,juce::dontSendNotification);if(open){browser_->refreshLibrary();browser_->toFront(true);}repaint();};
    browser_->onClose=[this]{browser_->setVisible(false);browserButton_.setToggleState(false,juce::dontSendNotification);browserButton_.grabKeyboardFocus();repaint();};
    presetIsDirty_=browser_->refreshCurrent();browserButton_.setButtonText(juce::String("PRESETS  ")+juce::String::charToString(0x25be));
    refreshVisibility();refreshDisplay();startTimerHz(30);
}
P6PluginEditor::~P6PluginEditor(){stopTimer();setLookAndFeel(nullptr);}
void P6PluginEditor::makeKnob(Knob& k,const juce::String& id,const juce::String& name,bool values)
{
    k.slider.setLookAndFeel(&look_);k.slider.setColour(juce::Slider::textBoxTextColourId,ink);
    k.slider.setColour(juce::Slider::textBoxOutlineColourId,juce::Colours::transparentBlack);
    k.slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    k.slider.setRotaryParameters(juce::MathConstants<float>::pi*1.25f,juce::MathConstants<float>::pi*2.75f,true);
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
    auto focus=[this,&k,name,nrpn]{focusNrpn_=nrpn;focusName_=name;focusValue_=k.slider.getTextFromValue(k.slider.getValue());focusTicks_=90;pitchDisplayTicks_=0;refreshDisplay();};
    k.slider.onDragStart=focus;k.slider.onSelected=focus;k.slider.onValueChange=focus;
}
void P6PluginEditor::setType(int value)
{
    opticalPreviewActive_=false; // The concept view must not masquerade as an audio effect.
    processor_.selectCharacterType(value);refreshVisibility();
}
void P6PluginEditor::timerCallback()
{
    const int arpStep=processor_.arpPlaybackStep();
    const auto rhythm=p6::P6ArpCharacter::rhythmMask(static_cast<int>(arpRhythm_.getValue()));
    const bool characterEnabled=arpCharEnabled_.getToggleState();
    for(std::size_t i=0;i<arpSteps_.size();++i){
        const bool playing=arpStep==static_cast<int>(i);
        const bool gate=!characterEnabled||(rhythm&(1u<<i))!=0;
        if(arpSteps_[i].playing!=playing||arpSteps_[i].rhythmGate!=gate||arpSteps_[i].characterEnabled!=characterEnabled){
            arpSteps_[i].playing=playing;arpSteps_[i].rhythmGate=gate;
            arpSteps_[i].characterEnabled=characterEnabled;arpSteps_[i].repaint();
        }
    }
    // A timer configured at 30Hz does NOT guarantee 33.3ms delivery in a DAW.
    // No iterative catch-up loop: LFO phase advances modulo 2pi, and
    // physical envelopes integrate elapsed time analytically. Retaining the
    // whole gap avoids the slow/incorrect phase after a blocked DAW UI.
    const double nowMs=juce::Time::getMillisecondCounterHiRes();
    const float elapsedSeconds=lastUiFrameTimeMs_>0.0?
        static_cast<float>(std::max(0.0,(nowMs-lastUiFrameTimeMs_)*.001)):
        1.f/30.f;
    lastUiFrameTimeMs_=nowMs;
    if(!isShowing())return;
    if (focusTicks_>0 && --focusTicks_==0) refreshDisplay();
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
    if(++browserRefreshTicks_%6==0){presetIsDirty_=browser_->refreshCurrent();browserButton_.setButtonText(juce::String("PRESETS  ")+juce::String::charToString(0x25be));}
    const auto name=processor_.activePresetName()+(presetIsDirty_?" *":"");
    headerPreset_.setName(name);
    if(!browserButton_.isMouseOverOrDragging()&&!browserButton_.hasKeyboardFocus(true))headerPreset_.tick();
    if(name!=presetName_) {
        presetName_=name;
        browserButton_.setTooltip(name+" — open preset library");
        browserButton_.setDescription("Browse presets. Current preset: "+name);
        refreshDisplay();
    }
    if(focusNrpn_>=0) {
        const auto* raw=processor_.parameters().getRawParameterValue(hardwareID(focusNrpn_));
        if(raw) {
            const auto value=juce::String(p6plugin::ui::formatParameterValue(focusNrpn_,raw->load(),processor_.parameterPresentationContext()));
            if(value!=focusValue_){focusValue_=value;refreshDisplay();}
        }
    }
    const auto pitchRevision=processor_.midiPitchBendRevision();
    if(pitchRevision!=lastPitchBendRevision_) {
        lastPitchBendRevision_=pitchRevision;
        pitchBendRaw14_=processor_.midiPitchBendRaw14();
        pitchDisplayTicks_=pitchBendRaw14_==8192?30:60;
        refreshDisplay();
    } else if(pitchDisplayTicks_>0) {
        --pitchDisplayTicks_;
        if(pitchDisplayTicks_==0)refreshDisplay();
    }
    std::uint32_t revision=lastVisualRevision_;
    p6plugin::CharacterTelemetry sample{};
    const bool coherent=processor_.readCharacterTelemetry(sample,revision);
    if(coherent)
    {
        staleVisualTicks_=revision==lastVisualRevision_
            ?std::min(30,staleVisualTicks_+1):0;
        lastVisualRevision_=revision;
        lastCoherentTelemetry_=sample;
    }
    else
    {
        // Bounded read retries can fail if the producer recycles a slot.
        // Never blend partial frames; keep the last VALID one and age it out.
        sample=lastCoherentTelemetry_;
        staleVisualTicks_=std::min(30,staleVisualTicks_+1);
    }
    visual_.type=sample.type;
    // A newly selected module can precede the next audio block. Never show
    // the former module's pickup data in the new physical chamber.
    const bool typeMatches=sample.type==boundType_;
    visual_.device=typeMatches?sample.device:p6plugin::DeviceTelemetry{};
    const bool bypassed=processor_.parameters().getRawParameterValue(p6plugin::kCharacterBypassID)->load()>=.5f;
    const bool live=staleVisualTicks_<4;
    visual_.active=sample.active&&!bypassed&&live&&typeMatches;
    const auto follow=[elapsedSeconds](float previous,float target){
        const float tau=target>previous?.042f:.191f;
        const float alpha=-std::expm1(-elapsedSeconds/tau);
        return previous+alpha*(target-previous);
    };
    visual_.inputRms=follow(visual_.inputRms,live?sample.inputRms:0.f);
    visual_.outputRms=follow(visual_.outputRms,live?sample.outputRms:0.f);
    visual_.changeRms=follow(visual_.changeRms,visual_.active?sample.changeRms:0.f);
    // No random flicker: the heater follows level, the inner glow follows the
    // actual latency-aligned waveform change. Neither changes the audio.
    glow_=visual_.active?std::clamp(std::sqrt(visual_.inputRms)*1.8f,0.f,1.f):0.f;
    characterGlow_=visual_.active?std::clamp(std::sqrt(visual_.changeRms)*2.f,0.f,1.f):0.f;
    chamber_.setIllumination(visual_.active,motion_.getToggleState(),glow_,characterGlow_);
    // The optical chamber is still a non-DSP PREVIEW. Its incandescent bulb
    // now receives the actual final-output RMS envelope rather than pulsing
    // at full intensity while the instrument is silent. The demonstration
    // LFO remains a visual metaphor until an optical DSP exists.
    const float audioRms=std::clamp(
        std::max({0.f,readings.rms[0],readings.rms[1]}),0.f,1.f);
    const float targetPreviewEnvelope=std::clamp(
        std::sqrt(audioRms)*2.4f,0.f,1.f);
    const float previewTau=targetPreviewEnvelope>previewAudioLevel_?.050f:.280f;
    const float previewAlpha=-std::expm1(-elapsedSeconds/previewTau);
    previewAudioLevel_+=previewAlpha*(targetPreviewEnvelope-previewAudioLevel_);
    if(opticalPreviewActive_)
    {
        if(motion_.getToggleState())
        {
            const float rate=static_cast<float>(opticalConcept_[0].slider.getValue());
            const float hz=.25f+3.5f*rate*rate;
            const double twoPi=juce::MathConstants<double>::twoPi;
            previewLfoPhase_=static_cast<float>(std::fmod(
                static_cast<double>(previewLfoPhase_)+twoPi*
                static_cast<double>(hz)*static_cast<double>(elapsedSeconds),
                twoPi));
        }
        const float cycle=previewLfoPhase_/juce::MathConstants<float>::twoPi;
        float lfo=.5f+.5f*std::sin(previewLfoPhase_);
        switch(opticalWave_.getSelectedId())
        {
            case 2: lfo=1.f-std::abs(2.f*cycle-1.f); break;
            case 3: lfo=cycle; break;
            case 4: lfo=cycle<.5f?1.f:0.f; break;
            default: break;
        }
        const float depth=static_cast<float>(opticalConcept_[1].slider.getValue());
        const float response=static_cast<float>(opticalConcept_[2].slider.getValue());
        const float lampDrive=previewAudioLevel_*(1.f-depth+depth*lfo);
        hardware_.setState(CharacterHardwareWindow::Mode::OpticalTremoloPreview,
                           true,motion_.getToggleState(),
                           audioRms,audioRms,0.f,{},lampDrive,response,elapsedSeconds);
    }
    else if(boundType_==1)
    {
        hardware_.setState(CharacterHardwareWindow::Mode::Tube,
                           visual_.active,motion_.getToggleState(),
                           visual_.inputRms,visual_.outputRms,visual_.changeRms,
                           {},.6f,.5f,elapsedSeconds);
    }
    else if(boundType_==3)
    {
        const auto& v=visual_.device.values;
        hardware_.setState(CharacterHardwareWindow::Mode::Spring,
                           visual_.active,motion_.getToggleState(),
                           visual_.inputRms,visual_.outputRms,visual_.changeRms,
                           {v[2],v[3],v[4]},.6f,.5f,elapsedSeconds);
    }
    updateDevice();
    if((showTube_||opticalPreviewActive_)&&!collapsed_&&!studioView_)repaint(tubeArea_);
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
    for(auto& k:tube_){k.slider.setVisible(!studioView_&&showTube_&&!collapsed_&&!opticalPreviewActive_);k.label.setVisible(k.slider.isVisible());}
    diagnostic_->setVisible(studioView_);
    // Tube/Spring use the approved panoramic artwork; existing experimental
    // Character modes retain their original specialized view.
    // Tube is the approved original; never replace or recreate its bezel.
    chamber_.setVisible(!studioView_&&!collapsed_&&selected==1&&!opticalPreviewActive_);
    // The panoramic photo chamber is only for Spring and optical preview.
    hardware_.setVisible(!studioView_&&!collapsed_&&(opticalPreviewActive_||selected==3));
    device_.setVisible(!studioView_&&selected>=2&&selected!=3&&!collapsed_&&!opticalPreviewActive_);
    opticalPreview_.setVisible(false); // Visual concept has no audio processing.
    opticalPreview_.setToggleState(opticalPreviewActive_,juce::dontSendNotification);
    // Preview controls are interactive but intentionally unbound to the host.
    for(auto& k:opticalConcept_)
    {
        k.slider.setVisible(!studioView_&&!collapsed_&&opticalPreviewActive_);
        k.label.setVisible(k.slider.isVisible());
    }
    opticalWave_.setVisible(!studioView_&&!collapsed_&&opticalPreviewActive_);
    // Initialize the selected artwork immediately, not only on a timer tick:
    // unhosted UI-smoke captures and the first frame must be truthful.
    if(opticalPreviewActive_)
        hardware_.setState(CharacterHardwareWindow::Mode::OpticalTremoloPreview,
                           true,motion_.getToggleState(),
                           0.f,0.f,0.f,{},0.f,
                           static_cast<float>(opticalConcept_[2].slider.getValue()));
    else if(selected==1)
        hardware_.setState(CharacterHardwareWindow::Mode::Tube,
                           visual_.active,motion_.getToggleState(),
                           visual_.inputRms,visual_.outputRms,visual_.changeRms);
    else if(selected==3)
        hardware_.setState(CharacterHardwareWindow::Mode::Spring,
                           visual_.active,motion_.getToggleState(),
                           visual_.inputRms,visual_.outputRms,visual_.changeRms,
                           {visual_.device.values[2],visual_.device.values[3],visual_.device.values[4]});
    updateDevice();
    for(auto* c:std::array<juce::Component*,8>{&previous_,&next_,&type_,&position_,&bypass_,&collapse_,&motion_,&keyTrack_})c->setVisible(!studioView_);
    collapse_.setVisible(!studioView_&&(showTube_||opticalPreviewActive_));motion_.setVisible(!studioView_&&(showTube_||opticalPreviewActive_)&&!collapsed_);
    plasmaMode_.setVisible(!studioView_&&selected==2&&!opticalPreviewActive_);
    position_.setEnabled(showTube_&&!opticalPreviewActive_);bypass_.setEnabled(showTube_&&!opticalPreviewActive_);previous_.setEnabled(selected>0);next_.setEnabled(selected<p6plugin::kCharacterTypes-1);
    module_.setToggleState(!studioView_,juce::dontSendNotification);studio_.setToggleState(studioView_,juce::dontSendNotification);
    collapse_.setButtonText(collapsed_?"SHOW":"HIDE");
    arpPanelSwitch_.setButtonText(arpOpen_?"HIDE":"SHOW");
    arpPanelSwitch_.setVisible(!studioView_);arpOn_.setVisible(!studioView_);
    for(auto& b:arpOctaveButtons_)b.setVisible(!studioView_&&arpOpen_);
    arpRate_.setVisible(!studioView_&&arpOpen_);
    for(auto* c:std::array<juce::Component*,11>{&arpHold_,&arpSync_,&arpCharEnabled_,&arpFreeze_,&arpMode_,&arpOctaves_,&arpDivision_,&arpClock_,&arpRhythm_,&arpMotion_,&arpBpm_})
        c->setVisible(!studioView_&&arpOpen_);
    arpOctaves_.setVisible(false);arpDivision_.setVisible(false);
    for(auto& b:arpSteps_) {
        b.characterEnabled=arpCharEnabled_.getToggleState();
        b.setVisible(!studioView_&&arpOpen_);
        b.repaint();
    }
    const bool expanded=!studioView_&&(showTube_||opticalPreviewActive_)&&!collapsed_;
    const int base=expanded?expandedHeight:compactHeight;
    const int needed=base+(studioView_?0:arpOffset()-56);
    setResizeLimits(1240,needed,2000,1500);
    setSize(getWidth(),needed);resized();repaint();
}
void P6PluginEditor::resized()
{
    const float sx=static_cast<float>(getWidth())/designWidth;
    auto rect=[sx](float x,float y,float w,float h){return juce::Rectangle<int>(juce::roundToInt(x*sx),juce::roundToInt(y),juce::roundToInt(w*sx),juce::roundToInt(h));};
    const float arpY=static_cast<float>(arpOffset());
    auto characterRect=[&](float x,float y,float w,float h){return rect(x,y+arpY+bodyOffset,w,h);};
    browserButton_.setBounds(rect(475,65,545,68));
    headerPreset_.setBounds(136,14,browserButton_.getWidth()-156,40);
    headerMenu_.setBounds(rect(1368,80,38,36));
    displayArea_=rect(475,136,545,24);
    focusReadout_.setBounds(displayArea_);
    if(browser_)browser_->setBounds(rect(36,headerHeight-5,1388,450));
    master_.setBounds(rect(1260,60,94,110));
    masterLabel_.setBounds(rect(1260,35,94,20));
    masterLabel_.setJustificationType(juce::Justification::centred);
    outputMeter_.setBounds(rect(1056,69,175,72));
    diagnostic_->setBounds(36,headerHeight+12,getWidth()-72,getHeight()-headerHeight-45);
    auto bodyRect=[&](float x,float y,float w,float h){return rect(x,y+bodyOffset,w,h);};
    auto knob=[&](std::size_t i,float x,float y,float w=80,float h=72){synth_[i].slider.setBounds(bodyRect(x,y,w,h));synth_[i].label.setBounds(bodyRect(x-5,y+h+1,w+10,18));};
    for(std::size_t i=0;i<4;++i){knob(i,54+94.f*static_cast<float>(i),119);knob(i+4,54+94.f*static_cast<float>(i),269);}
    knob(8,337,366,49,47);
    knob(9,502,167,79,77);knob(10,621,126,139,137);knob(11,785,167,79,77);knob(12,890,167,79,77);
    knob(13,1016,115,76,68);knob(14,1108,115,76,68);
    knob(15,1023,224,62,65);knob(16,1097,224,62,65);knob(17,1171,224,62,65);
    // Align the first two envelope centers with the Poly Mod controls above.
    for(std::size_t i=0;i<4;++i){knob(18+i,1016+94.f*static_cast<float>(i),338,75,65);knob(22+i,1016+94.f*static_cast<float>(i),445,75,64);}
    knob(26,553,456,72,65);knob(27,653,456,72,65);knob(28,753,456,72,65);knob(29,1301,548,83,62);
    for(std::size_t i=0;i<2;++i)knob(30+i,1110+94.f*static_cast<float>(i),548,76,62);
    for(std::size_t i=0;i<4;++i){synth_[32+i].slider.setBounds(bodyRect(72+84.f*static_cast<float>(i),463,55,117));synth_[32+i].label.setBounds(bodyRect(64+84.f*static_cast<float>(i),583,71,18));}
    switches_[0].button.setBounds(bodyRect(70,373,71,36));switches_[1].button.setBounds(bodyRect(171,373,71,36));switches_[2].button.setBounds(bodyRect(265,373,71,36));
    switches_[3].button.setBounds(bodyRect(475,478,70,36));switches_[4].button.setBounds(bodyRect(462,521,83,27));switches_[5].button.setBounds(bodyRect(795,290,98,24));
    switches_[6].button.setBounds(bodyRect(925,553,92,26));switches_[7].button.setBounds(bodyRect(1288,273,65,24));
    for(std::size_t i=0;i<5;++i){switches_[8+i].button.setBounds(bodyRect(1201+40.f*static_cast<float>(i),126,39,38));switches_[13+i].button.setBounds(bodyRect(1240+32.f*static_cast<float>(i),231,32,38));}
    fxAType_.setBounds(bodyRect(923,582,171,20));fxBType_.setBounds(bodyRect(923,607,171,20));
    keyTrack_.setBounds(bodyRect(587,290,132,24));
    arpPanelSwitch_.setBounds(bodyRect(1260,644,120,29));arpOn_.setBounds(bodyRect(260,646,75,25));
    arpHold_.setBounds(bodyRect(340,646,95,25));arpMode_.setBounds(bodyRect(90,705,150,28));
    arpOctaves_.setBounds(bodyRect(275,705,75,28));arpDivision_.setBounds(bodyRect(380,705,100,28));
    for(std::size_t i=0;i<arpOctaveButtons_.size();++i)arpOctaveButtons_[i].setBounds(bodyRect(275.f+33.f*static_cast<float>(i),707,30,30));
    arpRate_.setBounds(bodyRect(425,696,88,73));
    arpSync_.setBounds(bodyRect(520,705,80,28));arpCharEnabled_.setBounds(bodyRect(610,646,90,25));
    arpBpm_.setBounds(bodyRect(85,746,160,24));arpClock_.setBounds(bodyRect(305,746,100,24));
    arpRhythm_.setBounds(bodyRect(640,701,70,70));arpMotion_.setBounds(bodyRect(730,701,70,70));
    arpFreeze_.setBounds(bodyRect(815,705,100,26));
    for(std::size_t i=0;i<arpSteps_.size();++i)arpSteps_[i].setBounds(bodyRect(950.f+55.f*static_cast<float>(i),704,48,50));
    opticalPreview_.setBounds(characterRect(306,652,177,25));
    previous_.setBounds(characterRect(306,652,31,25));type_.setBounds(characterRect(343,652,260,25));next_.setBounds(characterRect(609,652,31,25));
    position_.setBounds(characterRect(660,652,195,25));bypass_.setBounds(characterRect(875,652,110,25));
    plasmaMode_.setBounds(characterRect(1000,652,140,25));motion_.setBounds(characterRect(1150,652,100,25));collapse_.setBounds(characterRect(1265,650,120,29));
    tubeArea_={32,637+arpOffset()+bodyOffset,getWidth()-64,std::max(51,getHeight()-660-arpOffset()-bodyOffset)};
    hardware_.setBounds(characterRect(55,710,610,190));
    // Restore the exact original Tube camera proportions and placement.
    chamber_.setBounds(characterRect(65,720,320,180));
    device_.setBounds(hardware_.getBounds());
    // Three actual local preview controls and an interactive waveform switch.
    // No drawn placeholder knobs and no hidden APVTS attachments.
    for(std::size_t i=0;i<opticalConcept_.size();++i)
    {
        const float x=720.f+160.f*static_cast<float>(i);
        opticalConcept_[i].label.setBounds(characterRect(x,718,130,18));
        opticalConcept_[i].slider.setBounds(characterRect(x,738,130,117));
    }
    opticalWave_.setBounds(characterRect(1220,760,151,30));
    // Existing Tube: preserve original seven-knob single-row module.
    // Spring: keep the approved wider two-row physical-control arrangement.
    for(std::size_t i=0;i<tube_.size();++i)
    {
        if(boundType_==1 && !opticalPreviewActive_)
        {
            const float x=421.f+114.f*static_cast<float>(i);
            tube_[i].label.setBounds(characterRect(x,759,106,20));
            tube_[i].slider.setBounds(characterRect(x,782,106,106));
        }
        else
        {
            const bool top=i<5;
            const float x=top?682.f+136.f*static_cast<float>(i)
                             :893.f+206.f*static_cast<float>(i-5);
            const float y=top?702.f:800.f;
            tube_[i].label.setBounds(characterRect(x,y,110,18));
            tube_[i].slider.setBounds(characterRect(x,y+17.f,110,81));
        }
    }

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
    g.fillAll(juce::Colour{0xffb3b5b6});
    g.setColour(juce::Colours::black.withAlpha(.18f));g.fillRoundedRectangle(17,18,w-34,h-25,12.f);
    g.setColour(juce::Colour{0xff242925});g.fillRoundedRectangle(18,12,w-36,h-28,10.f);
    const juce::Rectangle<float> face(25,10,w-50,h-33);
    g.setGradientFill(juce::ColourGradient(juce::Colour{0xffdedfe0},36,10,juce::Colour{0xffb9bbbd},w*.75f,h,false));g.fillRoundedRectangle(face,7.f);
    g.setTiledImageFill(grain_,0,0,.08f);g.fillRoundedRectangle(face,7.f);
    g.setColour(juce::Colours::white.withAlpha(.78f));g.drawRoundedRectangle(face.reduced(.5f),7.f,1.f);
    g.setColour(juce::Colour{0xff727c74});g.drawLine(22,24,22,h-29,2.f);g.drawLine(w-22,24,w-22,h-29,2.f);
}
void P6PluginEditor::refreshDisplay()
{
    juce::String text;
    if (pitchDisplayTicks_>0) {
        const double bend = pitchBendRaw14_>=8192 ? (pitchBendRaw14_-8192)/8191.0
                                                : (pitchBendRaw14_-8192)/8192.0;
        const auto* range = processor_.parameters().getRawParameterValue(hardwareID(31));
        const double semitones = bend*std::clamp(static_cast<double>(range->load()),0.0,12.0);
        text = "PITCH BEND  " + juce::String(semitones>=0 ? "+" : "")
             + juce::String(semitones,2) + " st";
    } else if (focusTicks_>0) {
        text = focusName_ + "  " + focusValue_;
    }
    focusReadout_.setText(text,juce::dontSendNotification);
}
void P6PluginEditor::paintCharacterPanel(juce::Graphics& g)
{
    const float sx=static_cast<float>(getWidth())/designWidth;
    const juce::Rectangle<float> panel(30*sx,638,getWidth()-60*sx,static_cast<float>(getHeight()-660-arpOffset()-bodyOffset));
    g.setColour(juce::Colour{0xff343833});g.fillRoundedRectangle(panel.expanded(2),7);
    g.setGradientFill(juce::ColourGradient(juce::Colour{0xffd1d2d3},panel.getX(),638,juce::Colour{0xffadafb1},panel.getRight(),panel.getBottom(),false));g.fillRoundedRectangle(panel,6);
    g.setTiledImageFill(grain_,0,638,.08f);g.fillRoundedRectangle(panel,6);
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
    if(boundType_==1&&!opticalPreviewActive_)
    {
        // Restore the entire original Tube treatment, not just its chamber:
        // section labels, meters, borders and placement stay as approved.
        g.setColour(muted);g.setFont(juce::Font(10.f).boldened());
        g.drawText(juce::String(p6plugin::kCharacterNames[1]).toUpperCase(),
                   static_cast<int>(430*sx),696,static_cast<int>(240*sx),18,
                   juce::Justification::centredLeft);
        g.drawText("TONE",static_cast<int>(770*sx),696,
                   static_cast<int>(200*sx),18,juce::Justification::centredLeft);
        g.drawText("LEVEL",static_cast<int>(1085*sx),696,
                   static_cast<int>(130*sx),18,juce::Justification::centredLeft);
        g.drawText("INSERTION",static_cast<int>(1260*sx),789,
                   static_cast<int>(120*sx),18,juce::Justification::centredLeft);
        g.setColour(muted.withAlpha(.3f));
        g.drawLine(405*sx,691,405*sx,898,1);
        g.drawLine(1238*sx,731,1238*sx,893,1);
        const std::array<float,3> levels{
            visual_.inputRms,visual_.outputRms,visual_.changeRms};
        const std::array<const char*,3> names{"IN","OUT","CHANGE"};
        for(std::size_t i=0;i<levels.size();++i)
        {
            const float x=(430.f+270.f*static_cast<float>(i))*sx;
            g.setColour(muted);g.setFont(9.f);
            g.drawText(names[i],static_cast<int>(x),731,70,13,
                       juce::Justification::centredLeft);
            g.setColour(ink.withAlpha(.12f));
            g.fillRoundedRectangle(x+45*sx,735.f,180*sx,4.f,2.f);
            const float level=std::clamp(
                (20.f*std::log10(std::max(levels[i],.000001f))+60.f)/60.f,0.f,1.f);
            g.setColour(amber.darker(.15f));
            g.fillRoundedRectangle(x+45*sx,735.f,180*sx*level,4.f,2.f);
        }
        return;
    }
    g.setColour(juce::Colour{0xff76726b}.withAlpha(.6f));
    g.drawLine(675.f*sx,702.f,675.f*sx,899.f,1.f);
    g.setColour(juce::Colour{0xff74776f}.withAlpha(.35f));
    g.drawLine(689.f*sx,804.f,1383.f*sx,804.f,1.f);
    if(opticalPreviewActive_)
    {
        g.setColour(juce::Colour{0xff6d4b29});
        g.setFont(juce::Font(12.f).boldened());
        g.drawText("OPTICAL TREMOLO / VISUAL PREVIEW / NO AUDIO PROCESSING",
                   static_cast<int>(695.f*sx),686,static_cast<int>(688.f*sx),19,
                   juce::Justification::centred);
        g.setColour(muted);
        g.setFont(juce::Font(10.5f));
        const auto activeAudioName=juce::String(
            p6plugin::kCharacterNames[static_cast<std::size_t>(
                std::clamp(boundType_,0,p6plugin::kCharacterTypes-1))]).toUpperCase();
        g.drawText("VISUAL CONTROLS ONLY / AUDIO STILL USES: "+activeAudioName,
                   static_cast<int>(690.f*sx),868,static_cast<int>(698.f*sx),20,
                   juce::Justification::centred);
        return;
    }
    // The hardware window owns its IN/OUT (or spring energy/tail) meters.
    // Avoid drawing a second set behind the real Mix/Output text boxes.
}
void P6PluginEditor::paint(juce::Graphics& g)
{
    paintSurface(g);const float sx=static_cast<float>(getWidth())/designWidth;
    const juce::Rectangle<float> glass(36.f*sx,10.f,1388.f*sx,184.f);
    g.setColour(juce::Colour{0xff111314});
    g.fillRoundedRectangle(glass.expanded(2.f),7.f);
    g.setColour(juce::Colours::white.withAlpha(.28f));
    g.drawLine(glass.getX()+4.f,glass.getBottom()+2.f,glass.getRight()-4.f,glass.getBottom()+2.f,1.f);
    g.setGradientFill(juce::ColourGradient(juce::Colour{0xff191a1b},glass.getX(),glass.getY(),
                                         juce::Colour{0xff0b0c0d},glass.getRight(),glass.getBottom(),false));
    g.fillRoundedRectangle(glass,6.f);
    g.setColour(juce::Colours::black.withAlpha(.6f));
    g.drawRoundedRectangle(glass.reduced(1.f),5.f,3.f);
    g.setColour(juce::Colours::white.withAlpha(.14f));
    g.drawLine(glass.getX()+6.f,glass.getY()+2.f,glass.getRight()-6.f,glass.getY()+2.f,.8f);
    juce::Path reflection;
    reflection.startNewSubPath(225.f*sx,13.f);
    reflection.lineTo(390.f*sx,13.f);
    reflection.lineTo(160.f*sx,191.f);
    reflection.lineTo(40.f*sx,191.f);
    reflection.closeSubPath();
    g.setColour(juce::Colours::white.withAlpha(.032f));
    g.fillPath(reflection);
    if(browser_&&browser_->isVisible()) {
        g.setColour(juce::Colour{0xff121414});
        g.fillRect(glass.getX()+2.f,glass.getBottom()-4.f,glass.getWidth()-4.f,10.f);
    }
    drawNeonBrand(g,sx);
    if(studioView_)return;
    g.saveState();
    g.addTransform(juce::AffineTransform::translation(0.f,static_cast<float>(bodyOffset)));
    section(g,"OSCILLATOR 1",56,90,360);section(g,"OSCILLATOR 2",56,240,360);section(g,"MIXER",56,431,360);
    section(g,"FILTER",478,90,495);section(g,"PERFORMANCE",478,431,370);
    section(g,"POLY MOD",1020,94,373);section(g,"LFO",1020,200,373);section(g,"FILTER ENVELOPE",1020,312,373);section(g,"AMPLIFIER ENVELOPE",1020,425,373);
    section(g,"FX",916,531,475);
    g.setColour(muted.withAlpha(.3f));
    g.drawLine(445*sx,107,445*sx,615,1.f);
    // Keep the column divider in the gap between Filter and modulation.
    // Stop above FX, whose controls span across both columns.
    g.drawLine(996*sx,107,996*sx,510,1.f);

    g.setColour(muted);g.setFont(10.f);g.drawText("KEY TRACK",static_cast<int>(491*sx),290,static_cast<int>(90*sx),24,juce::Justification::centredLeft);
    {
        const juce::Rectangle<float> arpPanel(30*sx,638.f,getWidth()-60*sx,static_cast<float>(arpOffset()-8));
        g.setColour(juce::Colour{0xff343833});g.fillRoundedRectangle(arpPanel.expanded(2),7);
        g.setGradientFill(juce::ColourGradient(juce::Colour{0xffd1d2d3},arpPanel.getX(),arpPanel.getY(),juce::Colour{0xffadafb1},arpPanel.getRight(),arpPanel.getBottom(),false));
        g.fillRoundedRectangle(arpPanel,6);
        g.setColour(juce::Colours::white.withAlpha(.6f));g.drawRoundedRectangle(arpPanel.reduced(1),5,1);
        for(const float x:{44.f*sx,static_cast<float>(getWidth())-44.f*sx}) {
            g.setColour(juce::Colour{0xff777a74});g.fillEllipse(x-4.f,646.f,8.f,8.f);
            g.setColour(juce::Colour{0xffe1e2dd});g.drawLine(x-2.f,650.f,x+2.f,648.f,1.f);
        }
        g.setColour(ink);g.setFont(juce::Font(13.f).boldened());g.drawText("ARPEGGIATOR",static_cast<int>(55*sx),646,static_cast<int>(190*sx),24,juce::Justification::centredLeft);
        if(arpOpen_) {
            g.setFont(juce::Font(10.f).boldened());
            g.drawText("CLASSIC",static_cast<int>(455*sx),649,static_cast<int>(115*sx),18,juce::Justification::centredLeft);
            g.drawText("PROPHEC-Y CHARACTER",static_cast<int>(710*sx),649,static_cast<int>(220*sx),18,juce::Justification::centredLeft);
            g.setColour(muted.withAlpha(.3f));
            g.drawLine(600.f*sx,679.f,600.f*sx,777.f,1.f);
            g.drawLine(930.f*sx,679.f,930.f*sx,777.f,1.f);
            g.setColour(ink);g.setFont(11.f);
            for(const auto& label:std::array<std::pair<const char*,float>,5>{{{"MODE",90},{"OCTAVES",275},{"RATE",425},{"RHYTHM",640},{"MOTION",730}}})
                g.drawText(label.first,static_cast<int>(label.second*sx),680,static_cast<int>(85*sx),16,juce::Justification::centredLeft);
            for(int i=0;i<8;++i)
                g.drawText(juce::String(i+1),static_cast<int>((950.f+55.f*i)*sx),682,static_cast<int>(48*sx),16,juce::Justification::centred);
            g.drawText("BPM",static_cast<int>(50*sx),750,static_cast<int>(35*sx),16,juce::Justification::centredLeft);
            g.drawText("CLOCK",static_cast<int>(255*sx),750,static_cast<int>(50*sx),16,juce::Justification::centredLeft);
        }
    }
    g.saveState();g.addTransform(juce::AffineTransform::translation(0.f,static_cast<float>(arpOffset())));
    paintCharacterPanel(g);
    g.setColour(ink);g.setFont(juce::Font(11.f).withExtraKerningFactor(.09f));
    g.drawText("PROPHEC-Y / CHARACTER",static_cast<int>(55*sx),651,
               static_cast<int>(240*sx),24,juce::Justification::centredLeft);
    if((showTube_||opticalPreviewActive_)&&!collapsed_)paintTube(g);
    g.restoreState();
    g.restoreState();
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
        k.slider.getProperties().remove("ledNeutral");
        if(type==4 && i==4)k.slider.getProperties().set("ledNeutral",.5);
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
        k.slider.onValueChange=[this,&k]{focusNrpn_=-1;focusName_=k.slider.getTitle();focusValue_=k.slider.getTextFromValue(k.slider.getValue());focusTicks_=90;refreshDisplay();updateDevice();};
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


