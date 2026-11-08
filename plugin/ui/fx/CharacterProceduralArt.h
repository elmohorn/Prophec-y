#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "CharacterVisualDynamics.h"
#include "CharacterHardwarePrimitives.h"
#include <algorithm>
#include <array>
#include <cmath>

// Vector-only fallbacks for when photo assets are unavailable.
// No UI state, DSP interaction or image extraction here. Frames remain
// consistent with CharacterHardwareWindow's original design coordinates.
class CharacterProceduralArt final
{
private:
public:
    static void drawTube(juce::Graphics& g,const juce::Image& valve,
                         bool active,float heater,float input,float output)
    {
        const auto warm=juce::Colour{0xfff7a24d};
        // Chamber vents, transformer housings and tube socket.
        for(int i=0;i<11;++i)
        {
            g.setColour(juce::Colour{0xff5a422a}.withAlpha(.3f));
            g.fillRoundedRectangle(32.f+i*13.f,47.f,6.f,74.f,2.f);
        }
        for(float x : {145.f,420.f})
        {
            g.setGradientFill(juce::ColourGradient(juce::Colour{0xff4d4a41},x,54.f,
                                                     juce::Colour{0xff0b0e0c},x+110.f,137.f,false));
            g.fillRoundedRectangle(x,65.f,72.f,72.f,8.f);
            CharacterHardwarePrimitives::screw(g,x+12.f,77.f); CharacterHardwarePrimitives::screw(g,x+59.f,124.f);
        }
        // Heater bloom follows measured RMS and the DIFFERENCE caused by the
        // tube algorithm. Camera brightness is not a random or note-on blink.
        const float radiance=active
            ? .04f+.96f*heater : .02f;
        auto halo=[&](float radius, float alpha)
        {
            g.setGradientFill(juce::ColourGradient(warm.withAlpha(alpha*radiance),320.f,102.f,
                                                     warm.withAlpha(0.f),320.f+radius,102.f,true));
            g.fillEllipse(320.f-radius,102.f-radius,radius*2.f,radius*2.f);
        };
        halo(125.f,.21f); halo(75.f,.28f);
        if(valve.isValid())
        {
            g.setOpacity(active?.84f:.5f);
            g.drawImage(valve,juce::Rectangle<float>(274.f,15.f,92.f,145.f));
            g.setOpacity(1.f);
        }
        g.setGradientFill(juce::ColourGradient(juce::Colour{0xff676152},289.f,136.f,
                                                 juce::Colour{0xff151715},352.f,157.f,false));
        g.fillRoundedRectangle(289.f,137.f,64.f,23.f,5.f);
        for(float x : {304.f,336.f})
        {
            g.setColour(warm.withAlpha(.17f+.7f*radiance));
            g.drawLine(x,77.f,x,118.f,3.f);
            g.setColour(juce::Colour{0xffffdcb2}.withAlpha(.55f*radiance));
            g.drawLine(x,78.f,x,115.f,1.2f);
        }
        g.setColour(juce::Colour{0xfff2d3ab}); g.setFont(juce::Font(19.f).boldened());
        g.drawText("12AX7",juce::Rectangle<float>(285.f,91.f,70.f,24.f),juce::Justification::centred);
        CharacterHardwarePrimitives::meter(g,"IN",32.f,151.f,220.f,input);
        CharacterHardwarePrimitives::meter(g,"OUT",393.f,151.f,220.f,output);
    }

    static void drawOptical(juce::Graphics& g,float l,float sensor)
    {
        for(float x : {42.f,513.f})
        {
            g.setColour(juce::Colour{0xff363833});
            g.fillRoundedRectangle(x,45.f,90.f,82.f,6.f);
            CharacterHardwarePrimitives::screw(g,x+15.f,59.f); CharacterHardwarePrimitives::screw(g,x+75.f,112.f);
        }
        const auto gold=juce::Colour{0xffffb458};
        g.setGradientFill(juce::ColourGradient(gold.withAlpha(.32f*l),205.f,86.f,
                                                 gold.withAlpha(0.f),205.f+190.f,86.f,true));
        g.fillEllipse(75.f,5.f,390.f,155.f);
        g.setColour(juce::Colour{0xff847868});g.fillRoundedRectangle(123.f,60.f,40.f,50.f,6.f);
        g.setColour(juce::Colour{0xffe3bc86}.withAlpha(.55f+.42f*l));
        g.drawEllipse(161.f,52.f,89.f,89.f,2.f);
        g.setGradientFill(juce::ColourGradient(gold.withAlpha(.95f*l),206.f,96.f,
                                                 gold.withAlpha(0.f),254.f,96.f,true));
        g.fillEllipse(157.f,47.f,98.f,98.f);
        g.setColour(juce::Colour{0xffffe4a1}.withAlpha(.15f+.8f*l));
        juce::Path wire; wire.startNewSubPath(176.f,99.f);
        wire.lineTo(192.f,84.f);wire.lineTo(209.f,104.f);
        wire.lineTo(225.f,81.f);wire.lineTo(236.f,96.f);
        g.strokePath(wire,juce::PathStrokeType(3.f));
        g.setGradientFill(juce::ColourGradient(gold.withAlpha(.18f*l),246.f,98.f,
                                                 gold.withAlpha(0.f),514.f,98.f,false));
        g.fillRect(247.f,74.f,267.f,44.f);
        // LDR spiral inset / glazed receptor.
        g.setColour(juce::Colour{0xffc19762});
        g.fillEllipse(493.f,52.f,87.f,87.f);
        g.setColour(juce::Colour{0xff38251b});g.fillEllipse(499.f,58.f,75.f,75.f);
        juce::Path ldr;
        for(int i=0;i<=48;++i)
        {
            const float x=510.f+static_cast<float>(i)*1.1f;
            const float y=96.f+std::sin(static_cast<float>(i)*.65f)*24.f;
            if(i==0)ldr.startNewSubPath(x,y);else ldr.lineTo(x,y);
        }
        g.setColour(juce::Colour{0xffefb76d});g.strokePath(ldr,juce::PathStrokeType(2.f));
        CharacterHardwarePrimitives::meter(g,"LAMP",37.f,152.f,265.f,l);
        CharacterHardwarePrimitives::meter(g,"LDR",343.f,152.f,254.f,sensor);
    }

    static void drawSpring(juce::Graphics& g,
                           const p6plugin::ui::CharacterVisualDynamics::Frame& frame,
                           float phase,float energy,float output)
    {
        const auto amber=juce::Colour{0xffe9a45d};
        const auto steel=juce::Colour{0xffbaad93};
        g.setGradientFill(juce::ColourGradient(
            juce::Colour{0xff181a18}, 320.f, 34.f,
            juce::Colour{0xff090a09}, 320.f, 149.f, false));
        g.fillRoundedRectangle(59.f, 33.f, 522.f, 113.f, 5.f);

        // Constant low-level backlight keeps silent springs readable. Only its
        // reflected intensity responds to measured pickup energy.
        g.setGradientFill(juce::ColourGradient(
            amber.withAlpha(.034f + .15f * energy), 320.f, 107.f,
            amber.withAlpha(0.f), 560.f, 107.f, true));
        g.fillEllipse(65.f, 10.f, 510.f, 175.f);
        g.setColour(juce::Colour{0xff977650}.withAlpha(.10f));
        g.drawLine(63.f, 30.f, 577.f, 30.f, 1.f);

        // A narrow lamp behind the coils creates tangential highlights and
        // sense of depth, without flashing the entire tank.
        g.setGradientFill(juce::ColourGradient(
            amber.withAlpha(.038f + .11f * energy), 320.f, 87.f,
            amber.withAlpha(0.f), 320.f, 151.f, false));
        g.fillRoundedRectangle(82.f, 36.f, 472.f, 108.f, 7.f);

        constexpr int turns = 48;
        constexpr int samplesPerTurn = 18;
        constexpr int samples = turns * samplesPerTurn;
        constexpr float pi = 3.14159265358979323846f;

        for (int row = 0; row < 2; ++row)
        {
            // The two visual bodies combine all three measured DSP pickups.
            const float y = 64.f + 60.f * static_cast<float>(row);
            const float level = frame.springEnergy[static_cast<std::size_t>(row)];
            const float phaseOffset = static_cast<float>(row) * 1.7f;

            // The spring is fixed at both ends. Movement and reflections are
            // an illustrative response to the three actual spring pickups.
            auto point = [&](int index)
            {
                const float t = static_cast<float>(index) / static_cast<float>(samples);
                const float a = 2.f * pi * static_cast<float>(turns) * t + phaseOffset;
                const float envelope = std::sin(pi * t);
                const float wobble = 5.8f * level * envelope *
                                     std::sin(phase + 13.f * t + phaseOffset);
                const float x = 84.f + 470.f * t +
                                .7f * level * envelope * std::sin(phase + 8.f * t);
                return juce::Point<float>{x, y + 7.7f * std::sin(a) + wobble};
            };
            // The z coordinate of the helix decides which turns cross in front.
            // Splitting paths is essential: a thick sine-wave stroke is not a
            // wound wire, and was the source of the old flat appearance.
            juce::Path rear, front;
            bool wasFront = false;
            for (int i = 0; i <= samples; ++i)
            {
                const float t = static_cast<float>(i) / static_cast<float>(samples);
                const float a = 2.f * pi * static_cast<float>(turns) * t + phaseOffset;
                const bool isFront = std::cos(a) >= 0.f;
                const auto p = point(i);
                auto& current = isFront ? front : rear;
                if (i == 0 || isFront != wasFront) current.startNewSubPath(p);
                else current.lineTo(p);
                wasFront = isFront;
            }

            // Real end clamps: dark rubber blocks, brass washers and steel pins.
            for (float x : {47.f, 556.f})
            {
                g.setGradientFill(juce::ColourGradient(
                    juce::Colour{0xff5a554b}, x, y - 13.f,
                    juce::Colour{0xff171a17}, x + 31.f, y + 14.f, false));
                g.fillRoundedRectangle(x, y - 13.f, 31.f, 26.f, 3.f);
                g.setColour(juce::Colour{0xff968775});
                g.drawRoundedRectangle(x + 2.f, y - 11.f, 27.f, 22.f, 2.f, 1.f);
                CharacterHardwarePrimitives::screw(g, x + 12.f, y);
            }
            g.setColour(steel.withAlpha(.78f));
            g.drawLine(72.f, y, 86.f, y, 3.f);
            g.drawLine(551.f, y, 565.f, y, 3.f);
            g.setColour(juce::Colour{0xffe1b97b}.withAlpha(.65f));
            for (float x : {82.f, 552.f})
                g.drawEllipse(x - 2.f, y - 4.f, 4.f, 8.f, 1.2f);

            // Rear segments absorb more light; foreground segments catch an
            // edge highlight and a fine secondary specular reflection.
            g.setColour(juce::Colour{0xff2d2923});
            g.strokePath(rear, juce::PathStrokeType(4.f));
            g.setColour(juce::Colour{0xff877761});
            g.strokePath(rear, juce::PathStrokeType(2.4f));
            g.setColour(juce::Colour{0xff110f0d}.withAlpha(.9f));
            g.strokePath(front, juce::PathStrokeType(4.8f),
                         juce::AffineTransform::translation(0.f, .75f));
            g.setColour(steel.interpolatedWith(amber, .14f + .27f * level));
            g.strokePath(front, juce::PathStrokeType(3.3f));
            g.setColour(juce::Colour{0xffffe0ab}.withAlpha(.52f + .38f * level));
            g.strokePath(front, juce::PathStrokeType(1.15f),
                         juce::AffineTransform::translation(0.f, -.85f));
            g.setColour(amber.withAlpha(.05f + .12f * level));
            g.strokePath(front, juce::PathStrokeType(5.f));
        }
        CharacterHardwarePrimitives::meter(g, "ENERGY", 37.f, 152.f, 276.f, energy);
        CharacterHardwarePrimitives::meter(g, "TAIL", 344.f, 152.f, 267.f, output);
    }
};
