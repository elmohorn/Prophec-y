#include "TubeChamber.h"
#include "BinaryData.h"
#include <algorithm>

TubeChamber::TubeChamber()
{
    interior_=juce::ImageCache::getFromMemory(BinaryData::tube_chamber_jpg,BinaryData::tube_chamber_jpgSize);
    valve_=juce::ImageCache::getFromMemory(BinaryData::tube_glass_png,BinaryData::tube_glass_pngSize);
    setTitle("Tube recessed chamber");
    setDescription("Valve mounted inside the dark chassis cavity. Warm illumination follows measured signal; bypass extinguishes it.");
    setInterceptsMouseClicks(false,false);
}
void TubeChamber::setIllumination(bool active,bool motion,float inputEnergy,float changeEnergy)
{
    const float energy=active?(motion?std::clamp(inputEnergy,0.f,1.f):.12f):0.f;
    const float change=active&&motion?std::clamp(changeEnergy,0.f,1.f):0.f;
    if(active_==active&&motion_==motion&&energy_==energy&&change_==change)return;
    active_=active;motion_=motion;energy_=energy;change_=change;repaint();
}
void TubeChamber::paint(juce::Graphics& g)
{
    // One uniform design transform preserves the valve and hardware geometry.
    const float scale=std::min(getWidth()/320.f,getHeight()/184.f);
    const float dx=(getWidth()-320.f*scale)*.5f,dy=(getHeight()-184.f*scale)*.5f;
    g.addTransform(juce::AffineTransform::scale(scale).translated(dx,dy));
    const juce::Rectangle<float> rim(1,1,318,182),opening(12,12,296,160);
    g.setColour(juce::Colours::black.withAlpha(.3f));g.fillRoundedRectangle(rim.translated(1,2),10);
    g.setGradientFill(juce::ColourGradient(juce::Colour{0xfff0ede3},0,0,juce::Colour{0xff747771},0,184,false));g.fillRoundedRectangle(rim,10);
    g.setColour(juce::Colour{0xff30332f});g.fillRoundedRectangle(rim.reduced(3),8);
    g.setGradientFill(juce::ColourGradient(juce::Colour{0xff575a52},0,5,juce::Colour{0xffbab9aa},0,180,false));g.drawRoundedRectangle(rim.reduced(5),7,2);
    {
        juce::Graphics::ScopedSaveState saved(g);
        juce::Path clip;clip.addRoundedRectangle(opening,5);g.reduceClipRegion(clip);
        g.drawImage(interior_,opening);
        // The background is an unlit cavity; emission is a separate UI layer.
        g.setColour(juce::Colours::black.withAlpha(.23f));g.fillRect(opening);
        const auto amber=juce::Colour{0xfff59b40};
        auto halo=[&](float radius,float alpha){g.setGradientFill(juce::ColourGradient(amber.withAlpha(alpha),160,114,amber.withAlpha(0.f),160+radius,114,true));g.fillEllipse(160-radius,114-radius,2*radius,2*radius);};
        if(active_)halo(108,.06f+.18f*energy_+.13f*change_);
        // Resource has transparent margins. Its pins end at the socket below
        // the aperture lip; the glass tip stays well behind the upper bezel.
        const juce::Rectangle<float> valve(109,15,102,153);
        g.setOpacity(.82f);g.drawImage(valve_,valve);g.setOpacity(1.f);
        if(active_) {
            halo(28,.06f+.18f*energy_+.2f*change_);
            for(float offset:{-6.6f,6.6f}) {
                g.setColour(amber.withAlpha(.18f+.6f*energy_));g.drawLine(160+offset,105,160+offset,123,2.4f);
                g.setColour(juce::Colour{0xffffd7a3}.withAlpha(.12f+.55f*change_));g.drawLine(160+offset,107,160+offset,120,1.f);
            }
        }
        // Socket's front half occludes the pins, seating the valve in the
        // circuit instead of leaving it floating above the chamber floor.
        g.setGradientFill(juce::ColourGradient(juce::Colour{0xff646052},140,145,juce::Colour{0xff181a16},179,160,false));
        g.fillRoundedRectangle(140,145,40,16,3);
        g.setColour(juce::Colour{0xff9b9278}.withAlpha(.6f));g.drawLine(144,146,176,146,1);
        // Inner top/side shadows and front lip put the entire device behind
        // the panel, rather than on a flat black display or external socket.
        g.setGradientFill(juce::ColourGradient(juce::Colours::black.withAlpha(.85f),0,12,juce::Colours::transparentBlack,0,35,false));g.fillRect(opening.withHeight(24));
        g.setColour(juce::Colours::black.withAlpha(.4f));g.drawRoundedRectangle(opening.reduced(1),5,5);
        g.setColour(juce::Colour{0xff211d16});g.fillRect(12.f,164.f,296.f,8.f);
        g.setColour(juce::Colour{0xff99907a}.withAlpha(.35f));g.drawLine(15,171,305,171,1);
        g.setColour(juce::Colours::white.withAlpha(.025f));g.drawLine(24,21,298,35,1);
    }
    // Four flush screws sit on the machined bezel, never inside the glass.
    for(float x:{9.f,311.f})for(float y:{9.f,175.f}) {
        g.setColour(juce::Colour{0xff242823});g.fillEllipse(x-4,y-4,8,8);
        g.setGradientFill(juce::ColourGradient(juce::Colour{0xffdeddd0},x-3,y-3,juce::Colour{0xff64665e},x+3,y+3,false));g.fillEllipse(x-3,y-3,6,6);
        g.setColour(juce::Colour{0xff34362f});g.drawLine(x-2,y+1,x+2,y-1,1);
    }
}
