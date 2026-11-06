#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "CharacterVisualDynamics.h"
#include "CharacterPhotoLayers.h"
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
        setInterceptsMouseClicks(false, false);
        setTitle("Character hardware chamber");
        setDescription("Recessed hardware illustration; illumination follows effect telemetry "
                       "except in the explicitly labelled optical tremolo design preview.");
    }

    void setValveAsset(const void* memory, int byteCount)
    {
        valve_ = juce::ImageCache::getFromMemory(memory, byteCount);
        repaint();
    }

    // Matched photorealistic camera study: 448x360 JPEG atlas with three
    // 448x120 rows: Spring, optical lamp dark, optical lamp lit. ONLY the
    // inner mechanical scene is copied; the authentic Tube bezel, recession,
    // glass overlay and rotary controls remain native JUCE components.
    void setPhotoAtlas(const void* data, int bytes)
    {
        photo_.setAtlas(data,bytes);
        repaint();
    }

    // Optional pre-rendered transparent layers aligned to the recessed bezel.
    // The first TWO coil layers represent the two approved physical springs.
    // A legacy third image is ignored; missing coil layers use the procedural
    // two-spring fallback. Backdrop owns the fixed supports and tank.
    void setSpringArtwork(const juce::Image& backdrop,
                          const std::array<juce::Image, 3>& coils)
    {
        springBackdrop_ = backdrop;
        springCoils_ = coils;
        repaint();
    }

    // The lamp off/on images are matched photographic renders: interpolation
    // occurs at draw time, while socket, LDR, and hardware stay stationary.
    // All four images use the same transparent 640x190 design canvas.
    void setOpticalArtwork(const juce::Image& backdrop, const juce::Image& lampOff,
                           const juce::Image& lampOn, const juce::Image& ldr)
    {
        opticalBackdrop_ = backdrop;
        opticalLampOff_ = lampOff;
        opticalLampOn_ = lampOn;
        opticalLdr_ = ldr;
        repaint();
    }

    void setState(Mode mode, bool enabled, bool motion, float inputRms, float outputRms,
                  float changeRms, const std::array<float, 3>& springPickups = {},
                  float previewLamp = 0.6f, float previewResponse = 0.5f,
                  float elapsedSeconds = 1.f/30.f)
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
        // Dynamics uses analytical exponential integration; a delayed
        // host/UI frame must advance the *whole* elapsed interval.
        const float dt=std::isfinite(elapsedSeconds)?
            std::max(0.f,elapsedSeconds):1.f/30.f;
        // Continue repainting a sustained Spring note: the pickup energies
        // control a visual, damped displacement, not a prerecorded animation.
        // The geometry stays still at zero energy and when motion is paused.
        const float springDrive = std::max({nextSprings[0], nextSprings[1], nextSprings[2]});
        const bool springAnimating = mode == Mode::Spring && enabled && motion &&
                                     springDrive > 0.005f;
        if (springAnimating)
            springPhase_ = std::fmod(springPhase_+
                                    (0.23f+0.17f*springDrive)*30.f*dt,
                                    6.28318530718f);
        // Motion stops visual time-varying state; bypass always extinguishes it.
        if (!motion && mode == mode_ && enabled && active_)
        {
            // Freeze frame while enabled, but NEVER swallow a bypass or
            // mode change: the off state must still extinguish visual energy.
            // Keep the last rendered motion/phase cached so a paused
            // snapshot is pixel-identical. Only enabled -> false may clear it.
            setDescription("Character animation paused; audio is unaffected.");
            return;
        }
        if (mode != mode_) visualDynamics_.reset();
        const auto before = visualDynamics_.frame();
        visualDynamics_.advance(enabled, motion, inputRms, changeRms,
                                springPickups, nextLamp, previewResponse,dt);
        const auto after = visualDynamics_.frame();
        // The measured RMS can stay constant while a physically damped
        // coil continues to oscillate or a lamp/LDR decays. Repaint for the
        // *visible* frame change, not only for raw telemetry changes.
        const bool visualChanged =
            std::abs(before.heater - after.heater) > .0005f ||
            std::abs(before.lamp - after.lamp) > .0005f ||
            std::abs(before.ldr - after.ldr) > .0005f ||
            std::abs(before.springShift[0] - after.springShift[0]) > .0005f ||
            std::abs(before.springShift[1] - after.springShift[1]) > .0005f ||
            std::abs(before.springEnergy[0] - after.springEnergy[0]) > .0005f ||
            std::abs(before.springEnergy[1] - after.springEnergy[1]) > .0005f;
        if (mode_ == mode && active_ == enabled && motion_ == motion &&
            input_ == nextIn && output_ == nextOut && changed_ == nextChanged &&
            lamp_ == nextLamp && pickups_ == nextSprings &&
            !springAnimating && !visualChanged) return;
        mode_ = mode; active_ = enabled; motion_ = motion;
        input_ = nextIn; output_ = nextOut; changed_ = nextChanged;
        lamp_ = nextLamp; pickups_ = nextSprings;
        repaint();
    }

    void paint(juce::Graphics& g) override
    {
        // One isotropic transform for both hardware and photographic interior.
        // The letterboxed margins belong to the parent editor's aluminum panel.
        const float scale=std::min(getWidth()/640.f,getHeight()/190.f);
        if (scale<=0.f) return;
        const float tx=(getWidth()-640.f*scale)*.5f;
        const float ty=(getHeight()-190.f*scale)*.5f;
        juce::Graphics::ScopedSaveState saved(g);
        g.addTransform(juce::AffineTransform::scale(scale).translated(tx,ty));
        const juce::Rectangle<float> outer(1, 1, 638, 188);
        const juce::Rectangle<float> cavity(12, 12, 616, 164);
        g.setColour(juce::Colours::black.withAlpha(.28f));
        g.fillRoundedRectangle(outer.translated(1.f, 2.f), 14.f);
        // Match the APPROVED TubeChamber hardware language: same brushed
        // aluminum gradient, graphite inset and machined edge. Only the
        // panoramic aperture and its interchangeable physical contents differ.
        g.setGradientFill(juce::ColourGradient(juce::Colour{0xfff0ede3},0.f,0.f,
                                                 juce::Colour{0xff747771},0.f,190.f,false));
        g.fillRoundedRectangle(outer, 13.f);
        g.setColour(juce::Colour{0xff30332f}); g.fillRoundedRectangle(outer.reduced(4), 10.f);
        g.setGradientFill(juce::ColourGradient(juce::Colour{0xff575a52},0.f,6.f,
                                                 juce::Colour{0xffbab9aa},0.f,188.f,false));
        g.drawRoundedRectangle(outer.reduced(6), 9.f, 1.5f);

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
            // The physical glass sits IN FRONT of all three hero objects.
            // Never bake it into the spring/lamp art or redesign the bezel.
            if (mode_ != Mode::Tube) drawGlass(g, cavity);
        }

        for (float xx : {18.f, 622.f}) for (float yy : {18.f, 172.f}) screw(g, xx, yy);
    }

private:
    static void drawGlass(juce::Graphics& g, juce::Rectangle<float> cavity)
    {
        const auto inner = cavity.reduced(3.f);
        g.setGradientFill(juce::ColourGradient(
            juce::Colour{0xffaac2c4}.withAlpha(.045f), 25.f, 20.f,
            juce::Colours::transparentBlack, 580.f, 170.f, false));
        g.fillRoundedRectangle(inner, 5.f);

        juce::Path reflection;
        reflection.startNewSubPath(24.f, 21.f);
        reflection.lineTo(167.f, 21.f);
        reflection.lineTo(273.f, 167.f);
        reflection.lineTo(189.f, 167.f);
        reflection.closeSubPath();
        g.setGradientFill(juce::ColourGradient(
            juce::Colours::white.withAlpha(.055f), 58.f, 23.f,
            juce::Colours::white.withAlpha(0.f), 271.f, 160.f, false));
        g.fillPath(reflection);
        g.setColour(juce::Colour{0xffc8d1c9}.withAlpha(.16f));
        g.drawRoundedRectangle(inner, 5.f, 0.75f);
        g.setColour(juce::Colours::white.withAlpha(.09f));
        g.drawLine(34.f, 22.f, 605.f, 22.f, 0.7f);
    }

    static void drawArtwork(juce::Graphics& g, const juce::Image& image,
                            float yShift = 0.f)
    {
        if (image.isValid())
            g.drawImage(image, juce::Rectangle<float>{0.f, yShift, 640.f, 190.f},
                        juce::RectanglePlacement::stretchToFit);
    }

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
        // Heater bloom follows measured RMS and the DIFFERENCE caused by the
        // tube algorithm. Camera brightness is not a random or note-on blink.
        const float radiance=active_
            ? .04f+.96f*visualDynamics_.frame().heater : .02f;
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
        const float l=std::clamp(visualDynamics_.frame().lamp,0.f,1.f);
        const float sensor=std::clamp(visualDynamics_.frame().ldr,0.f,1.f);
        if(photo_.hasOptical())
        {
            photo_.drawOptical(g,active_?l:0.f,active_?sensor:0.f);
            meter(g,"LAMP",37.f,152.f,265.f,active_ ? l : 0.f);
            meter(g,"LDR",343.f,152.f,254.f,active_ ? sensor : 0.f);
            return;
        }
        if (opticalBackdrop_.isValid() && opticalLampOff_.isValid() &&
            opticalLampOn_.isValid() && opticalLdr_.isValid())
        {
            drawArtwork(g, opticalBackdrop_);
            drawArtwork(g, opticalLampOff_);
            {
                juce::Graphics::ScopedSaveState light(g);
                g.setOpacity(.025f + .975f * l);
                drawArtwork(g, opticalLampOn_);
            }
            drawArtwork(g, opticalLdr_);
            meter(g,"LAMP",37.f,152.f,265.f,l);
            meter(g,"LDR",343.f,152.f,254.f,l*.78f);
            return;
        }
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
        // A miniature illuminated mechanical tank, shared with the Tube
        // chamber's recess / bezel. This is a 2.5D projection of real helix
        // geometry: the front turns occlude the rear turns and have different
        // diffuse and specular materials. No web renderer or extra GPU needed.
        const auto amber = juce::Colour{0xffe9a45d};
        const auto steel = juce::Colour{0xffbaad93};
        const auto& springFrame = visualDynamics_.frame();
        const float energy = std::clamp((springFrame.springEnergy[0] + springFrame.springEnergy[1]) * .5f, 0.f, 1.f);

        if(photo_.hasAtlas())
        {
            const auto& f=visualDynamics_.frame();
            photo_.drawSpring(g,f.springShift,active_);
            // Backlight shimmers with the *two visible* coils, each receiving
            // a weighted mix of the three actual spring FDN pickup energies.
            // Source crop (y=14..114) mapped at 1.3x: metal windings lie
            // near y=49 and y=113 in the physical 640x190 design surface.
            constexpr std::array<float,2> centerY{49.f,113.f};
            for (std::size_t row=0;row<2;++row)
            {
                const float level=active_ ? f.springEnergy[row] : 0.f;
                const float y=centerY[row];
                const float x=320.f+55.f*std::sin(springPhase_+row*1.8f);
                g.setGradientFill(juce::ColourGradient(
                    amber.withAlpha(.22f*level),x,y,
                    amber.withAlpha(0.f),x+175.f,y,true));
                g.fillEllipse(x-165.f,y-15.f,330.f,30.f);
            }
            meter(g,"ENERGY",37.f,152.f,276.f,
                  (f.springEnergy[0]+f.springEnergy[1])*.5f);
            meter(g,"TAIL",344.f,152.f,267.f,output_);
            return;
        }
        if (springBackdrop_.isValid() && springCoils_[0].isValid() &&
            springCoils_[1].isValid())
        {
            drawArtwork(g, springBackdrop_);
            // Legacy layered assets may include a third image, but the approved
            // hardware has TWO coils. Never paint a phantom third spring.
            for (std::size_t row = 0; row < 2; ++row)
            {
                const float displacement = active_ && motion_
                    ? visualDynamics_.frame().springShift[row] : 0.f;
                drawArtwork(g, springCoils_[row], displacement);
            }
            meter(g, "ENERGY", 37.f, 152.f, 276.f,
                  (visualDynamics_.frame().springEnergy[0] +
                   visualDynamics_.frame().springEnergy[1]) * .5f);
            meter(g, "TAIL", 344.f, 152.f, 267.f, output_);
            return;
        }

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
            const float level = visualDynamics_.frame().springEnergy[static_cast<std::size_t>(row)];
            const float phaseOffset = static_cast<float>(row) * 1.7f;

            // The spring is fixed at both ends. Movement and reflections are
            // an illustrative response to the three actual spring pickups.
            auto point = [&](int index)
            {
                const float t = static_cast<float>(index) / static_cast<float>(samples);
                const float a = 2.f * pi * static_cast<float>(turns) * t + phaseOffset;
                const float envelope = std::sin(pi * t);
                const float wobble = 5.8f * level * envelope *
                                     std::sin(springPhase_ + 13.f * t + phaseOffset);
                const float x = 84.f + 470.f * t +
                                .7f * level * envelope * std::sin(springPhase_ + 8.f * t);
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
                screw(g, x + 12.f, y);
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
        meter(g, "ENERGY", 37.f, 152.f, 276.f, energy);
        meter(g, "TAIL", 344.f, 152.f, 267.f, output_);
    }

    p6plugin::ui::CharacterVisualDynamics visualDynamics_;
    CharacterPhotoLayers photo_;
    juce::Image valve_;
    juce::Image springBackdrop_;
    std::array<juce::Image, 3> springCoils_{};
    juce::Image opticalBackdrop_, opticalLampOff_, opticalLampOn_, opticalLdr_;
    Mode mode_{Mode::Tube};
    bool active_{}, motion_{true};
    float input_{}, output_{}, changed_{}, lamp_{.6f};
    std::array<float,3> pickups_{};
    float springPhase_{};
};
