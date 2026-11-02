#include "CharacterDeviceView.h"
#include "../../CustomCharacter.h"
#include <cmath>
CharacterDeviceView::CharacterDeviceView()
{
    setTitle("Character recessed device");setInterceptsMouseClicks(false,false);
}
void CharacterDeviceView::update(int type,const std::array<float,7>& parameters,const p6plugin::CharacterTelemetry& reading,bool motion)
{
    if(type_!=type)reading_={};
    type_=type;parameters_=parameters;
    if(motion)reading_=reading;
    if(!reading.active){reading_.active=false;reading_.device={};}
    setDescription(juce::String(p6plugin::kCharacterNames[static_cast<std::size_t>(type)])+": measured audio activity. Motion pauses live visualization.");repaint();
}
void CharacterDeviceView::paint(juce::Graphics& g)
{
    const float scale=std::min(getWidth()/320.f,getHeight()/184.f);
    g.addTransform(juce::AffineTransform::scale(scale).translated((getWidth()-320.f*scale)*.5f,(getHeight()-184.f*scale)*.5f));
    const juce::Rectangle<float> rim(1,1,318,182),opening(12,12,296,160);
    g.setGradientFill(juce::ColourGradient(juce::Colour{0xffe2dfd4},0,0,juce::Colour{0xff666c65},0,184,false));g.fillRoundedRectangle(rim,10);
    g.setColour(juce::Colour{0xff30342f});g.fillRoundedRectangle(rim.reduced(3),8);
    g.setColour(juce::Colour{0xff878d7f});g.drawRoundedRectangle(rim.reduced(5),7,2);
    {
        juce::Graphics::ScopedSaveState save(g);juce::Path clip;clip.addRoundedRectangle(opening,5);g.reduceClipRegion(clip);
        g.setGradientFill(juce::ColourGradient(juce::Colour{0xff030608},160,12,juce::Colour{0xff1c2421},160,172,false));g.fillRect(opening);
        // Recessed rear chassis, fasteners and lower socket rails.
        g.setColour(juce::Colour{0xff434a3e});g.fillRect(24.f,146.f,272.f,10.f);
        g.setColour(juce::Colour{0xff7d765b});g.drawLine(28,147,292,147,1);
        for(float x:{26.f,294.f})for(float y:{26.f,157.f}){g.setColour(juce::Colour{0xff4a4c40});g.fillEllipse(x-3,y-3,6,6);g.setColour(juce::Colours::black);g.drawLine(x-1,y-1,x+1,y+1,1);}
        const auto& d=reading_.device.values;
        const float energy=reading_.active?std::clamp(std::sqrt(reading_.outputRms)*2.f,0.f,1.f):0.f;
        auto glow=[&](const juce::Path& path,juce::Colour c,float level){g.setColour(c.withAlpha(.09f*level));g.strokePath(path,juce::PathStrokeType(14));g.setColour(c.withAlpha(.3f*level));g.strokePath(path,juce::PathStrokeType(6));g.setColour(c.brighter(.6f).withAlpha(.95f*level));g.strokePath(path,juce::PathStrokeType(1.5f));};
        juce::Colour accent{0xffb08cf8};juce::String caption;
        if(type_==2) {
            // Sealed horizontal discharge tube seated between two electrodes.
            g.setGradientFill(juce::ColourGradient(juce::Colour{0xff1c2530},50,60,juce::Colour{0xff060b11},50,122,false));g.fillRoundedRectangle(43,56,234,69,29);
            g.setColour(juce::Colour{0xff657680}.withAlpha(.55f));g.drawRoundedRectangle(43,56,234,69,29,1);
            g.setColour(juce::Colour{0xffc2d1d8}.withAlpha(.13f));g.drawLine(68,64,249,64,2);
            for(float x:{42.f,263.f}){g.setGradientFill(juce::ColourGradient(juce::Colour{0xff8e8268},x,65,juce::Colour{0xff282a25},x,121,false));g.fillRoundedRectangle(x,66,16,54,3);g.setColour(juce::Colour{0xffa2a38f});g.drawLine(x+8,118,x+8,146,3);}
            juce::Path arc;for(std::size_t i=0;i<32;++i){const float x=59.f+202.f*static_cast<float>(i)/31.f;const float y=91.f+std::clamp(reading_.device.wet[i],-1.f,1.f)*19.f;if(i==0)arc.startNewSubPath(x,y);else arc.lineTo(x,y);}
            glow(arc,accent,reading_.active?std::clamp(d[0],0.f,1.f):0.f);
            caption="CONDUCTION  "+juce::String(reading_.active?100.f*d[0]:0.f,1)+" %";
        } else if(type_==3) {
            accent=juce::Colour{0xffe3a264};
            for(int row=0;row<3;++row){const float y=53.f+30.f*static_cast<float>(row);const float level=reading_.active?std::clamp(d[static_cast<std::size_t>(row+2)]*8.f,0.f,1.f):0.f;
                g.setColour(juce::Colour{0xff6c6551});g.drawLine(33,y,287,y,1);
                juce::Path coil;coil.startNewSubPath(42,y);for(int j=0;j<240;++j){const float x=42.f+static_cast<float>(j);const float phase=static_cast<float>(j)*.67f;coil.lineTo(x,y+std::sin(phase)*(6.f+level*std::clamp(reading_.device.wet[static_cast<std::size_t>(j)%32],-1.f,1.f)*3.f));}
                g.setColour(juce::Colour{0xff867252});g.strokePath(coil,juce::PathStrokeType(1.3f));glow(coil,accent,level*.4f);
                const float px=68.f+parameters_[3]*180.f;g.setColour(juce::Colour{0xff354b43});g.fillRoundedRectangle(px-6,y-11,12,22,3);g.setColour(juce::Colour{0xff7fdfbc}.withAlpha(.15f+.85f*level));g.fillRect(px-1,y-7,2.f,14.f);
            }caption="PICKUPS  1 / 2 / 3  ·  TAP ENERGY";
        } else if(type_==4) {
            accent=juce::Colour{0xff79c9c7};const float envelope=reading_.active?std::clamp(d[6]*4.f,0.f,1.f):0.f;
            g.setColour(juce::Colour{0xff475a57});g.drawRoundedRectangle(44,33,232,109,9,1);g.setColour(juce::Colour{0xff121c20});g.fillRoundedRectangle(49,38,222,99,7);
            juce::Path fluid;fluid.startNewSubPath(52,137);fluid.lineTo(52,118);
            for(int j=0;j<=216;++j){const float x=52.f+static_cast<float>(j),phase=static_cast<float>(j)*.3f;const float peak=std::pow(.5f+.5f*std::cos(phase),5.f);const float y=117.f-(9.f+45.f*envelope)*(.25f+parameters_[1]*.75f)*(.35f+.65f*std::sin(3.14159265f*static_cast<float>(j)/216.f))*(.75f+.25f*std::abs(std::clamp(reading_.device.wet[static_cast<std::size_t>(j)%32],-1.f,1.f)))*peak;fluid.lineTo(x,y);}fluid.lineTo(268,137);fluid.closeSubPath();
            g.setGradientFill(juce::ColourGradient(juce::Colour{0xff8ba5a2},160,68,juce::Colour{0xff04090e},160,137,false));g.fillPath(fluid);g.setColour(accent.withAlpha(.25f+.4f*energy));g.strokePath(fluid,juce::PathStrokeType(.8f));
            g.setColour(juce::Colour{0xff535f56});g.fillRoundedRectangle(112,142,96,7,2);
            caption="ENVELOPE  ·  "+juce::String(reading_.active?d[5]:20.f*std::pow(900.f,parameters_[0]),1)+" Hz";
        } else {
            accent=juce::Colour{0xfff28baf};
            g.setColour(juce::Colour{0xff293732});for(int x=48;x<285;x+=28)g.drawVerticalLine(x,38,141);for(int y=42;y<145;y+=24)g.drawHorizontalLine(y,38,283);
            juce::Path curve;for(int j=0;j<=240;++j){const float x=-1.f+2.f*static_cast<float>(j)/240.f;const float y=p6plugin::CustomCharacter::neonTransfer(x,parameters_);if(j==0)curve.startNewSubPath(40,89-43*y);else curve.lineTo(40.f+static_cast<float>(j),89-43*y);}glow(curve,accent,.65f+.35f*energy);
            // Actual wet output trace, separate from the nominal transfer plot.
            juce::Path trace;for(std::size_t j=0;j<32;++j){const float x=40.f+240.f*static_cast<float>(j)/31.f,y=134.f-6.f*std::clamp(reading_.device.wet[j],-1.f,1.f);if(j==0)trace.startNewSubPath(x,y);else trace.lineTo(x,y);}glow(trace,juce::Colour{0xff85ded4},energy);
            caption="TRANSFER · PRE TONE  /  WET TRACE";
        }
        g.setColour(accent.withAlpha(.7f));g.setFont(juce::Font(8.f).withExtraKerningFactor(.08f));g.drawText(caption,31,151,258,14,juce::Justification::centred);
        g.setGradientFill(juce::ColourGradient(juce::Colours::black.withAlpha(.8f),0,12,juce::Colours::transparentBlack,0,35,false));g.fillRect(opening.withHeight(24));g.setColour(juce::Colours::black.withAlpha(.5f));g.drawRoundedRectangle(opening.reduced(1),5,5);
    }
    for(float x:{9.f,311.f})for(float y:{9.f,175.f}){g.setColour(juce::Colour{0xffb9bcad});g.fillEllipse(x-3,y-3,6,6);g.setColour(juce::Colour{0xff30372e});g.drawLine(x-2,y+1,x+2,y-1,1);}
}
