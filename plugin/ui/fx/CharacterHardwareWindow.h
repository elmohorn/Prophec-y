#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "BinaryData.h"
#include <array>
#include <algorithm>
#include <cmath>

// Vector/asset hardware window for the approved v0.20 Character mockups.
// Rendering only. Never owns or mutates DSP parameters. Audio telemetry is
// passed from PluginEditor at UI timer rate, never fetched on the audio thread.
// Optical Tremolo is deliberately a VISUAL PREVIEW until its DSP/state exists.
class CharacterHardwareWindow final : public juce::Component
{
public:
    enum class Mode { Tube, OpticalTremoloPreview, Spring };

    CharacterHardwareWindow()
    {
        valve_ = juce::ImageCache::getFromMemory(BinaryData::tube_glass_png,
                                                 BinaryData::tube_glass_pngSize);
        setInterceptsMouseClicks(false, false);
        setTitle("Character hardware chamber");
        setDescription("Recessed hardware illustration; illumination follows effect telemetry "
                       "except in the explicitly labelled optical tremolo design preview.");
    }

    void setState(Mode mode, bool enabled, bool motion, float inputRms, float outputRms,
                  float changeRms, const std::array<float, 3>& springPickups = {},
                  float previewLamp = 0.6f)
    {
        const auto finite01 = [](float v) {
            return std::isfinite(v) ? std::clamp(v, 0.f, 1.f) : 0.f;
        };
        const auto pick = [&finite01](float rms) {
            return finite01(std::sqrt(std::max(0.f, rms)) * 1.9f);
        };
        const float nextIn = enabled ? pick(inputRms) : 0.f;
        const float nextOut = enabled ? pick(outputRms) : 0.f;
        const float nextChanged = enabled ? pick(changeRms) : 0.f;
        const float nextLamp = motion ? finite01(previewLamp) : lamp_;
        std::array<float,3> nextSprings{};
        for (int i = 0; i < 3; ++i)
            nextSprings[static_cast<std::size_t>(i)] =
                enabled ? finite01(springPickups[static_cast<std::size_t>(i)] * 8.f) : 0.f;
        // Motion stops visual time-varying state; bypass always extinguishes it.
        if (!motion && mode == mode_ && enabled)
        {
            setDescription("Character animation paused; audio is unaffected.");
            return;
        }
        if (mode_ == mode && active_ == enabled && input_ == nextIn &&
            output_ == nextOut && changed_ == nextChanged && lamp_ == nextLamp &&
            pickups_ == nextSprings) return;
        mode_ = mode; active_ = enabled; input_ = nextIn; output_ = nextOut;
        changed_ = nextChanged; lamp_ = nextLamp; pickups_ = nextSprings;
        repaint();
    }

    void paint(juce::Graphics& g) override
    {
        // Independent design-space axes fill the panoramic window without
        // distorting the glass image (drawn in its own bounds below).
        const float sx = static_cast<float>(getWidth()) / 640.f;
        const float sy = static_cast<float>(getHeight()) / 190.f;
        if (sx <= 0.f || sy <= 0.f) return;
        juce::Graphics::ScopedSaveState saved(g);
        g.addTransform(juce::AffineTransform::scale(sx, sy));
        const juce::Rectangle<float> outer(1, 1, 638, 188);
        const juce::Rectangle<float> cavity(12, 12, 616, 164);
        g.setColour(juce::Colours::black.withAlpha(.28f));
        g.fillRoundedRectangle(outer.translated(1.f, 2.f), 14.f);
        g.setGradientFill(juce::ColourGradient(juce::Colour{0xffe5e0d4}, 0.f, 0.f,
                                                 juce::Colour{0xff77796d}, 0.f, 189.f, false));
        g.fillRoundedRectangle(outer, 13.f);
        g.setColour(juce::Colour{0xff242622}); g.fillRoundedRectangle(outer.reduced(5), 10.f);
        g.setColour(juce::Colour{0xff878579});
        g.drawRoundedRectangle(outer.reduced(7), 9.f, 1.5f);

        {
            juce::Graphics::ScopedSaveState inner(g);
            juce::Path mask; mask.addRoundedRectangle(cavity, 7.f);
            g.reduceClipRegion(mask);
            g.setGradientFill(juce::ColourGradient(juce::Colour{0xff020403}, 320.f, 20.f,
                                                     juce::Colour{0xff24201a}, 320.f, 180.f, false));
            g.fillRect(cavity);
            g.setColour(juce::Colour{0xff131411}); g.fillRect(15.f, 145.f, 610.f, 29.f);
            g.setColour(juce::Colour{0xff665b46}.withAlpha(.4f)); g.drawLine(18.f, 145.f, 622.f, 145.f, 1.f);

            if (mode_ == Mode::Tube) drawTube(g);
            else if (mode_ == Mode::Spring) drawSpring(g);
            else drawOptical(g);

            g.setGradientFill(juce::ColourGradient(juce::Colours::black.withAlpha(.82f), 0.f, 13.f,
                                                     juce::Colours::transparentBlack, 0.f, 52.f, false));
            g.fillRect(12.f, 12.f, 616.f, 40.f);
            g.setColour(juce::Colours::black.withAlpha(.64f));
            g.drawRoundedRectangle(cavity, 7.f, 5.f);
        }

        for (float xx : {18.f, 622.f}) for (float yy : {18.f, 172.f}) screw(g, xx, yy);
    }

private:
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

    void drawTube(juce::Graphics& g)
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
            screw(g,x+12.f,77.f); screw(g,x+59.f,124.f);
        }
        const float radiance=active_ ? std::clamp(.25f+.55f*input_+.2f*changed_,0.f,1.f) : .05f;
        auto halo=[&](float radius, float alpha)
        {
            g.setGradientFill(juce::ColourGradient(warm.withAlpha(alpha*radiance),320.f,102.f,
                                                     warm.withAlpha(0.f),320.f+radius,102.f,true));
            g.fillEllipse(320.f-radius,102.f-radius,radius*2.f,radius*2.f);
        };
        halo(125.f,.21f); halo(75.f,.28f);
        if(valve_.isValid())
        {
            g.setOpacity(active_?.84f:.5f);
            g.drawImage(valve_,juce::Rectangle<float>(274.f,15.f,92.f,145.f));
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
        meter(g,"IN",32.f,151.f,220.f,input_);
        meter(g,"OUT",393.f,151.f,220.f,output_);
    }

    void drawOptical(juce::Graphics& g)
    {
        // Visual concept only. Lamp drive will be supplied by the future LFO
        // and actual LDR conductance once the optical DSP is implemented.
        const float l=std::clamp(lamp_,0.f,1.f);
        for(float x : {42.f,513.f})
        {
            g.setColour(juce::Colour{0xff363833});
            g.fillRoundedRectangle(x,45.f,90.f,82.f,6.f);
            screw(g,x+15.f,59.f); screw(g,x+75.f,112.f);
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
        meter(g,"LAMP",37.f,152.f,265.f,l);
        meter(g,"LDR",343.f,152.f,254.f,l*.78f);
    }

    void drawSpring(juce::Graphics& g)
    {
        const auto metal=juce::Colour{0xffceac75};
        for(int row=0;row<3;++row)
        {
            const float y=55.f+38.f*static_cast<float>(row);
            const float level=pickups_[static_cast<std::size_t>(row)];
            g.setColour(juce::Colour{0xff3e3a31});
            g.fillRoundedRectangle(35.f,y-14.f,40.f,28.f,4.f);
            g.fillRoundedRectangle(562.f,y-14.f,40.f,28.f,4.f);
            screw(g,46.f,y); screw(g,590.f,y);
            g.setColour(juce::Colour{0xff78694c});
            g.drawLine(74.f,y,564.f,y,2.f);
            juce::Path spring;
            for(int j=0;j<=390;++j)
            {
                const float x=79.f+static_cast<float>(j)*1.23f;
                const float phase=static_cast<float>(j)*.68f;
                const float amplitude=6.f+level*1.8f;
                const float yy=y+std::sin(phase)*amplitude;
                if(j==0)spring.startNewSubPath(x,yy);else spring.lineTo(x,yy);
            }
            g.setColour(juce::Colours::black.withAlpha(.43f));
            g.strokePath(spring,juce::PathStrokeType(4.4f));
            g.setColour(metal.withMultipliedBrightness(.55f+.45f*level));
            g.strokePath(spring,juce::PathStrokeType(2.2f));
            g.setColour(juce::Colour{0xfff8cf7a}.withAlpha(level*.2f));
            g.strokePath(spring,juce::PathStrokeType(7.f));
        }
        const float total=std::clamp((pickups_[0]+pickups_[1]+pickups_[2])/3.f,0.f,1.f);
        meter(g,"ENERGY",37.f,152.f,276.f,total);
        meter(g,"TAIL",344.f,152.f,267.f,output_);
    }

    juce::Image valve_;
    Mode mode_{Mode::Tube};
    bool active_{}, motion_{true};
    float input_{}, output_{}, changed_{}, lamp_{.6f};
    std::array<float,3> pickups_{};
};
