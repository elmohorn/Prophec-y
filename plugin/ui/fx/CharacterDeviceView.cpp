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
    if(motion){reading_=reading;if(reading.active)animationPhase_=std::fmod(animationPhase_+.09f,1000.f);}
    if(!reading.active){reading_.active=false;reading_.device={};}
    setDescription(juce::String(p6plugin::kCharacterNames[static_cast<std::size_t>(type)])+": measured audio activity on the dominant input channel. Motion pauses live visualization.");repaint();
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
            // A glass cartridge, with a multi-strand procedural discharge.
            // Activity comes from the DSP; geometry is an illustrative visual.
            const float conduction=reading_.active?std::clamp(d[0],0.f,1.f):0.f;
            const float light=std::clamp(conduction*.8f+energy*.2f,0.f,1.f);
            g.setColour(juce::Colours::black.withAlpha(.7f));g.fillRoundedRectangle(31,45,258,91,37);
            g.setGradientFill(juce::ColourGradient(juce::Colour{0xff49545a},160,45,juce::Colour{0xff090c15},160,135,false));g.fillRoundedRectangle(41,50,238,82,34);
            g.setGradientFill(juce::ColourGradient(juce::Colour{0xff10101e},160,56,juce::Colour{0xff04050a},160,122,false));g.fillRoundedRectangle(48,56,224,68,29);
            g.setColour(juce::Colour{0xff9da9b0}.withAlpha(.45f));g.drawRoundedRectangle(41,50,238,82,34,1.4f);
            for(float x:{39.f,263.f}){
                g.setGradientFill(juce::ColourGradient(juce::Colour{0xffc4ad86},x,60,juce::Colour{0xff353127},x,126,false));g.fillRoundedRectangle(x,61,18,61,4);
                g.setColour(juce::Colour{0xffd6c4a2}.withAlpha(.5f));g.drawLine(x+3,65,x+3,116,1);
                g.setColour(juce::Colour{0xff8c806b});g.drawLine(x+9,121,x+9,147,3);
            }
            if(light>.001f){
                for(int strand=0;strand<4;++strand){
                    juce::Path arc;const float offset=static_cast<float>(strand)-1.5f;
                    for(int j=0;j<=64;++j){
                        const float t=static_cast<float>(j)/64.f;
                        const float taper=std::sin(juce::MathConstants<float>::pi*t);
                        const float noise=std::sin(t*31.f+animationPhase_*2.f+offset*2.f)*3.4f+std::sin(t*73.f-animationPhase_*3.f+offset)*1.5f;
                        const float audio=std::clamp(reading_.device.wet[static_cast<std::size_t>(j/2)%32],-1.f,1.f)*5.f;
                        const float y=90.f+taper*(noise+audio+offset*6.f);
                        if(j==0)arc.startNewSubPath(57,y);else arc.lineTo(57.f+206.f*t,y);
                    }
                    glow(arc,juce::Colour{0xffaa65ff},light*(strand==0?1.f:.55f));
                }
                g.setColour(juce::Colour{0xffefb8ff}.withAlpha(.6f*light));g.fillEllipse(55,87,5,6);g.fillEllipse(260,87,5,6);
            }
            g.setColour(juce::Colours::white.withAlpha(.16f));g.drawLine(76,59,244,59,2);
            g.setColour(juce::Colour{0xff9cadc2}.withAlpha(.12f));g.drawLine(65,116,254,116,2);
            caption="PLASMA  /  "+juce::String(100.f*conduction,0)+" %";
        } else if(type_==3) {
            accent=juce::Colour{0xffe3a264};
            for(int row=0;row<3;++row){const float y=53.f+30.f*static_cast<float>(row);const float level=reading_.active?std::clamp(d[static_cast<std::size_t>(row+2)]*8.f,0.f,1.f):0.f;
                g.setColour(juce::Colour{0xff6c6551});g.drawLine(33,y,287,y,1);
                juce::Path coil;coil.startNewSubPath(42,y);for(int j=0;j<240;++j){const float x=42.f+static_cast<float>(j);const float phase=static_cast<float>(j)*.67f;coil.lineTo(x,y+std::sin(phase)*(6.f+level*std::clamp(reading_.device.wet[static_cast<std::size_t>(j)%32],-1.f,1.f)*3.f));}
                g.setColour(juce::Colour{0xff867252});g.strokePath(coil,juce::PathStrokeType(1.3f));glow(coil,accent,level*.4f);
                const float px=68.f+parameters_[3]*180.f;g.setColour(juce::Colour{0xff354b43});g.fillRoundedRectangle(px-6,y-11,12,22,3);g.setColour(juce::Colour{0xff7fdfbc}.withAlpha(.15f+.85f*level));g.fillRect(px-1,y-7,2.f,14.f);
            }caption="PICKUPS  1 / 2 / 3  ·  TAP ENERGY";
        } else if(type_==4) {
            accent=juce::Colour{0xff85b7b2};const float envelope=reading_.active?std::clamp(d[6]*4.f,0.f,1.f):0.f;
            // Lit glass capsule with a glossy, audio-reactive magnetic-fluid silhouette.
            g.setColour(juce::Colours::black.withAlpha(.7f));g.fillRoundedRectangle(42,27,236,119,17);
            g.setGradientFill(juce::ColourGradient(juce::Colour{0xff314649},160,31,juce::Colour{0xff080f13},160,142,false));g.fillRoundedRectangle(47,30,226,112,15);
            g.setColour(juce::Colour{0xff9eb8b3}.withAlpha(.4f));g.drawRoundedRectangle(47,30,226,112,15,1.2f);
            g.setColour(juce::Colour{0xff91b9b2}.withAlpha(.08f));g.fillEllipse(75,38,170,80);
            const float amplitude=(7.f+40.f*envelope)*(.35f+.65f*parameters_[1]);
            juce::Path fluid;fluid.startNewSubPath(57,132);fluid.lineTo(57,111);
            for(int j=0;j<=206;++j){
                const float t=static_cast<float>(j)/206.f;
                const float wave=std::pow(.5f+.5f*std::cos(t*juce::MathConstants<float>::twoPi*7.f+animationPhase_*.25f),7.f);
                const float body=std::sin(juce::MathConstants<float>::pi*t);
                const float y=111.f-body*(4.f+amplitude*wave);
                fluid.lineTo(57.f+static_cast<float>(j),y);
            }
            fluid.lineTo(263,132);fluid.closeSubPath();
            g.setGradientFill(juce::ColourGradient(juce::Colour{0xffb9d0c7},130,64,juce::Colour{0xff05090b},160,132,false));g.fillPath(fluid);
            g.setColour(juce::Colour{0xffdcf5e7}.withAlpha(.32f+.28f*energy));g.strokePath(fluid,juce::PathStrokeType(1.f));
            for(int j=0;j<7;++j){const float x=75.f+static_cast<float>(j)*28.f;g.setColour(juce::Colour{0xffd8f9e9}.withAlpha(.09f+.16f*envelope));g.drawLine(x,107,x+5,110,1.2f);}
            g.setColour(juce::Colours::white.withAlpha(.13f));g.drawLine(69,39,244,39,2);g.drawLine(54,48,54,103,1);
            g.setGradientFill(juce::ColourGradient(juce::Colour{0xff87918a},160,137,juce::Colour{0xff313a35},160,148,false));g.fillRoundedRectangle(96,137,128,10,3);
            caption="FERROFLUID  /  "+juce::String(reading_.active?d[5]:20.f*std::pow(900.f,parameters_[0]),0)+" Hz";
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
