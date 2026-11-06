#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <algorithm>

// Small native JUCE drawing primitives shared by the brushed-metal chassis
// and its procedural fallback modules. No DSP, heap allocation or state.
class CharacterHardwarePrimitives final
{
public:
    static void screw(juce::Graphics& g, float x, float y)
    {
        g.setColour(juce::Colour{0xff161a17}); g.fillEllipse(x-5.f, y-5.f, 10.f, 10.f);
        g.setGradientFill(juce::ColourGradient(juce::Colour{0xffefebe0},x-4.f,y-4.f,
                                                 juce::Colour{0xff707369},x+4.f,y+4.f,false));
        g.fillEllipse(x-3.5f,y-3.5f,7.f,7.f);
        g.setColour(juce::Colour{0xff343b36}); g.drawLine(x-2.f,y+1.f,x+2.f,y-1.f,1.2f);
    }

    static void meter(juce::Graphics& g, const juce::String& label,
                      float x, float y, float width, float level)
    {
        level = std::clamp(level, 0.f, 1.f);
        g.setFont(juce::Font(12.f).boldened());
        g.setColour(juce::Colour{0xffc9bca5}); g.drawText(label, juce::Rectangle<float>(x,y,88.f,16.f),
                                                           juce::Justification::centredLeft);
        const float trackX=x+83.f;
        const int count=15;
        for(int i=0;i<count;++i)
        {
            auto cell=juce::Rectangle<float>(trackX+i*(width-83.f)/count,y+2.f,
                                            (width-83.f)/count-3.f,11.f);
            const bool lit=(static_cast<float>(i)+.5f)/count<level;
            g.setColour(lit ? juce::Colour{0xffeeb357} :
                        juce::Colour{0xff37312a});
            g.fillRoundedRectangle(cell,1.3f);
            if(lit){g.setColour(juce::Colour{0xfffad69b}.withAlpha(.7f));
                    g.fillRect(cell.reduced(2.f,2.f));}
        }
    }

};
