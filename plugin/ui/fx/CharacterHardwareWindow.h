#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "CharacterVisualDynamics.h"
#include "CharacterPhotoLayers.h"
#include "CharacterProceduralArt.h"
#include "CharacterHardwarePrimitives.h"
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

    // Preferred full-size static background, TWO transparent moving coils
    // and an OPTIONAL stationary energy reflection overlay. All are 640x190.
    // These override the JPEG compatibility mattes. Mounts never move.
    void setSpringArtwork(const juce::Image& backdrop,
                          const std::array<juce::Image, 3>& coils)
    {
        // Reject bad dimensions or opaque coil masks: malformed artwork
        // must never displace a working atlas/fallback with stretched pixels.
        if(isFullSizeArtwork(backdrop) &&
           isFullSizeArtwork(coils[0],true) &&
           isFullSizeArtwork(coils[1],true) &&
           (!coils[2].isValid() || isFullSizeArtwork(coils[2],true)))
        {
            springBackdrop_=backdrop;
            springCoils_=coils;  // index 2: optional fixed reflection
        }
        else
        {
            springBackdrop_={};
            springCoils_={};
        }
        repaint();
    }

    // Preferred production artwork: static backdrop, dark lamp fixture,
    // TRANSPARENT lamp emission and TRANSPARENT LDR emission, all on the same
    // 640x190 design canvas. No crossfade of the whole photographed housing.
    // This takes precedence over the approximate JPEG segmentation.
    void setOpticalArtwork(const juce::Image& backdrop, const juce::Image& lampOff,
                           const juce::Image& lampOn, const juce::Image& ldr)
    {
        if(isFullSizeArtwork(backdrop) && isFullSizeArtwork(lampOff) &&
           isFullSizeArtwork(lampOn,true) && isFullSizeArtwork(ldr,true))
        {
            opticalBackdrop_=backdrop;
            opticalLampOff_=lampOff;
            opticalLampOn_=lampOn;
            opticalLdr_=ldr;
        }
        else
        {
            opticalBackdrop_={};
            opticalLampOff_={};
            opticalLampOn_={};
            opticalLdr_={};
        }
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

            if (mode_ == Mode::Tube)
                CharacterProceduralArt::drawTube(g,valve_,active_,
                                                 visualDynamics_.frame().heater,input_,output_);
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

        for (float xx : {18.f, 622.f}) for (float yy : {18.f, 172.f}) CharacterHardwarePrimitives::screw(g, xx, yy);
    }

private:
    static bool isFullSizeArtwork(const juce::Image& image,
                                  bool needsAlpha=false) noexcept
    {
        if (!image.isValid() || image.getWidth()!=640 ||
            image.getHeight()!=190) return false;
        if (!needsAlpha) return true;
        // An ARGB pixel format does not imply meaningful transparency:
        // an opaque "coil" layer could cover and animate the fixed chassis.
        // Validate masks at asset load, never on the audio or paint threads.
        if (image.getFormat()!=juce::Image::ARGB) return false;
        int transparent=0, painted=0;
        constexpr int pixels=640*190;
        for(int y=0;y<190;++y) for(int x=0;x<640;++x)
        {
            const auto alpha=image.getPixelAt(x,y).getAlpha();
            if(alpha<255) ++transparent;
            if(alpha>0) ++painted;
        }
        // Separate emission/coil layers must leave substantial clear
        // background and actually contain visible material/light.
        return transparent>=pixels/10 && painted>=16;
    }

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

    void drawOptical(juce::Graphics& g)
    {
        // Visual concept only. Lamp drive will be supplied by the future LFO
        // and actual LDR conductance once the optical DSP is implemented.
        const float l=std::clamp(visualDynamics_.frame().lamp,0.f,1.f);
        const float sensor=std::clamp(visualDynamics_.frame().ldr,0.f,1.f);
        // Explicit, professionally isolated RGBA layers are the preferred
        // photographic source. Keep the metal/lampholder in still images;
        // lampOn and LDR must be TRANSPARENT light layers. JPEG-derived
        // threshold masks are only a compatibility fallback.
        if (opticalBackdrop_.isValid() && opticalLampOff_.isValid() &&
            opticalLampOn_.isValid() && opticalLdr_.isValid())
        {
            drawArtwork(g, opticalBackdrop_);
            drawArtwork(g, opticalLampOff_);
            {
                juce::Graphics::ScopedSaveState light(g);
                g.setOpacity(active_?l:0.f);
                drawArtwork(g, opticalLampOn_);
            }
            {
                juce::Graphics::ScopedSaveState ldrLight(g);
                g.setOpacity(active_?sensor:0.f);
                drawArtwork(g, opticalLdr_);
            }
            CharacterHardwarePrimitives::meter(g,"LAMP",37.f,152.f,265.f,active_?l:0.f);
            CharacterHardwarePrimitives::meter(g,"LDR",343.f,152.f,254.f,active_?sensor:0.f);
            return;
        }
        if(photo_.hasOptical())
        {
            photo_.drawOptical(g,active_?l:0.f,active_?sensor:0.f);
            CharacterHardwarePrimitives::meter(g,"LAMP",37.f,152.f,265.f,active_ ? l : 0.f);
            CharacterHardwarePrimitives::meter(g,"LDR",343.f,152.f,254.f,active_ ? sensor : 0.f);
            return;
        }
        CharacterProceduralArt::drawOptical(g,l,sensor);
    }

    void drawSpring(juce::Graphics& g)
    {
        // Preferred authored physical layers, JPEG compatibility scene,
        // then procedural 2.5D fallback. All share the same bezel and meter.
        const auto amber=juce::Colour{0xffe9a45d};
        const auto& springFrame = visualDynamics_.frame();
        const float energy = std::clamp((springFrame.springEnergy[0] + springFrame.springEnergy[1]) * .5f, 0.f, 1.f);

        // A clean background plus two alpha-separated coils are superior to
        // heuristics inferred from a lossy JPEG. Prefer authored artwork.
        if (springBackdrop_.isValid() && springCoils_[0].isValid() &&
            springCoils_[1].isValid())
        {
            drawArtwork(g, springBackdrop_);
            // Exactly TWO coils are movable; the third, if present, is
            // stationary reflected light, not an invented third spring.
            for (std::size_t row = 0; row < 2; ++row)
            {
                const float displacement = active_ && motion_
                    ? visualDynamics_.frame().springShift[row] : 0.f;
                drawArtwork(g, springCoils_[row], displacement);
            }
            if (springCoils_[2].isValid())
            {
                juce::Graphics::ScopedSaveState light(g);
                g.setOpacity(active_ ? .08f + .75f * energy : 0.f);
                drawArtwork(g, springCoils_[2]);
            }
            CharacterHardwarePrimitives::meter(g, "ENERGY", 37.f, 152.f, 276.f,
                  (visualDynamics_.frame().springEnergy[0] +
                   visualDynamics_.frame().springEnergy[1]) * .5f);
            CharacterHardwarePrimitives::meter(g, "TAIL", 344.f, 152.f, 267.f, output_);
            return;
        }

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
            CharacterHardwarePrimitives::meter(g,"ENERGY",37.f,152.f,276.f,
                  (f.springEnergy[0]+f.springEnergy[1])*.5f);
            CharacterHardwarePrimitives::meter(g,"TAIL",344.f,152.f,267.f,output_);
            return;
        }
        CharacterProceduralArt::drawSpring(g,springFrame,springPhase_,energy,output_);
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
