#include "OutputMeter.h"
namespace {
float db(float x){return 20.f*std::log10(std::max(x,.000001f));}
juce::String level(float x){return x<.000001f?juce::String("-inf"):juce::String(db(x),1);}
}
void OutputMeter::paint(juce::Graphics& g)
{
    // The meter is part of the approved single continuous header screen.
    // Paint transparently over the parent OLED instead of a separate box.
    const float width=static_cast<float>(getWidth());
    g.setFont(10.f);g.setColour(juce::Colour{0xffd4cec3});
    g.drawText("OUTPUT / SAMPLE PEAK + RMS",8,2,getWidth()-100,13,juce::Justification::centredLeft);
    const bool hot=readings_.clips&12u;
    g.setColour(hot?juce::Colour{0xffffbc72}:juce::Colour{0xffaaa49a});
    g.drawText(hot?"PRE > 0 dBFS":"PRE OK",getWidth()-100,2,93,13,juce::Justification::centredRight);
    for(std::size_t c=0;c<2;++c) {
        const float y=20.f+static_cast<float>(c)*20.f;
        const bool clip=readings_.clips&(1u<<c);
        g.setColour(juce::Colour{0xffe0d9ce});g.drawText(c==0?"L":"R",6,static_cast<int>(y)-2,14,13,juce::Justification::centredLeft);
        const float rail=width-152;
        g.setColour(juce::Colour{0xff35332f});g.fillRect(22.f,y,rail,7.f);
        auto proportion=[](float x){return std::clamp((db(x)+60.f)/60.f,0.f,1.f);};
        g.setColour(juce::Colour{0xffe4b879});g.fillRect(22.f,y,rail*proportion(readings_.rms[c]),7.f);
        const float px=22+rail*proportion(readings_.peak[c]);
        g.setColour(clip?juce::Colour{0xffff735e}:juce::Colour{0xfff0bf7d});g.drawLine(px,y-2,px,y+9,1.5f);
        g.drawText(clip?"CLIP":"",static_cast<int>(width)-36,static_cast<int>(y)-2,33,14,juce::Justification::centredRight);
        g.setColour(juce::Colour{0xffe0d9ce});g.drawText(level(readings_.peak[c])+" / "+level(readings_.rms[c]),static_cast<int>(width)-124,static_cast<int>(y)-2,84,14,juce::Justification::centredRight);
    }
}

