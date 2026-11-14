#pragma once
// Approved, photoreal hardware artwork from the Prophec-Y UI v2 concept.
// The original design board is the source of truth; the atlas is reusable
// geometry, not a screenshot of this component or a DSP implementation.
#include <juce_audio_processors/juce_audio_processors.h>
#include <BinaryData.h>
#include "../controls/ProphecyParameterControl.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <functional>
#include <optional>

namespace p6plugin::hardware::photo {
struct Region { int x,y,w,h; };
struct DialDescriptor {
    const char* id;
    int nominalDiameter;
    int sourceCapRadius;
    Region ring,cap,full;
};
struct ModuleDescriptor { const char* id; Region crop; };

inline constexpr std::array<DialDescriptor,15> dials{{
    { "xl-hero", 64, 59, {145,2,140,140}, {288,2,140,140}, {2,2,140,140} },
    { "l-standard", 48, 47, {2,145,120,120}, {125,145,120,120}, {1181,2,120,120} },
    { "m-compact", 36, 35, {2,268,94,94}, {99,268,94,94}, {1235,145,94,94} },
    { "s-trim", 24, 23, {1293,268,62,62}, {2,365,62,62}, {1228,268,62,62} },
    { "l-brushed-cap", 48, 37, {287,268,88,88}, {378,268,88,88}, {196,268,88,88} },
    { "l-dark-cap", 48, 36, {815,268,82,82}, {900,268,82,82}, {730,268,82,82} },
    { "m-knurled-cap", 36, 35, {556,268,84,84}, {643,268,84,84}, {469,268,84,84} },
    { "m-scalloped-cap", 36, 37, {1066,268,78,78}, {1147,268,78,78}, {985,268,78,78} },
    { "encoder-led", 48, 40, {1037,145,96,96}, {1136,145,96,96}, {938,145,96,96} },
    { "macro-morph", 64, 45, {712,145,110,110}, {825,145,110,110}, {599,145,110,110} },
    { "rotary-selector", 48, 54, {556,2,122,122}, {681,2,122,122}, {431,2,122,122} },
    { "micro", 18, 19, {118,365,48,48}, {169,365,48,48}, {67,365,48,48} },
    { "nano", 14, 19, {269,365,46,46}, {318,365,46,46}, {220,365,46,46} },
    { "l-pointer-cap", 48, 49, {931,2,122,122}, {1056,2,122,122}, {806,2,122,122} },
    { "detented-encoder", 36, 47, {365,145,114,114}, {482,145,114,114}, {248,145,114,114} }
}};
inline constexpr std::array<ModuleDescriptor,39> modules{{
    { "fader-vertical-long", {2,2,63,139} },
    { "fader-vertical-compact", {68,2,43,113} },
    { "fader-vertical-mini", {537,2,42,87} },
    { "slider-horizontal", {349,396,170,38} },
    { "slider-horizontal-compact", {522,396,118,33} },
    { "slider-horizontal-trim", {864,396,116,22} },
    { "switch-low-profile-on", {529,342,90,47} },
    { "toggle-mini", {987,2,58,74} },
    { "switch-3-position", {885,280,82,51} },
    { "toggle-pill", {179,342,73,49} },
    { "button-square-on", {2,144,76,68} },
    { "button-round-on", {1081,144,54,63} },
    { "button-utility", {1072,215,57,60} },
    { "button-icon", {1132,215,49,60} },
    { "step-sequencer", {2,215,282,62} },
    { "capacitive-preset", {287,215,247,62} },
    { "dropdown-preview", {757,215,312,61} },
    { "oled-preset-strip", {2,280,388,59} },
    { "vu-analog", {582,2,180,86} },
    { "meter-dual-led", {148,2,93,89} },
    { "scope-spectrum", {81,144,166,68} },
    { "scope-oscilloscope", {250,144,170,68} },
    { "scope-morph", {423,144,176,68} },
    { "xy-performance", {765,2,219,86} },
    { "adsr-envelope", {1048,2,303,71} },
    { "lfo-wave-selector", {602,144,308,68} },
    { "modulation-matrix", {622,342,575,44} },
    { "navigation-tabs", {2,396,344,42} },
    { "progress-red", {643,396,218,32} },
    { "rocker-2-pos", {913,144,165,65} },
    { "toggle-3-pos", {244,2,68,89} },
    { "vu-amber-compact", {315,2,219,88} },
    { "status-oled-mini", {537,215,217,62} },
    { "led-bar-vertical", {114,2,31,108} },
    { "led-bar-horizontal", {255,342,271,49} },
    { "value-chip", {393,280,205,58} },
    { "module-header", {970,280,251,51} },
    { "icon-button-set", {601,280,281,56} },
    { "status-lamps", {2,342,174,51} }
}};

inline const juce::Image& dialAtlas() {
    static const auto image=juce::ImageCache::getFromMemory(
        BinaryData::dials_atlas_png,BinaryData::dials_atlas_pngSize);
    return image;
}
inline const juce::Image& moduleAtlas() {
    static const auto image=juce::ImageCache::getFromMemory(
        BinaryData::modules_atlas_png,BinaryData::modules_atlas_pngSize);
    return image;
}
inline int dialIndex(const juce::String& key) {
    for(int i=0;i<static_cast<int>(dials.size());++i)
        if(key==dials[static_cast<std::size_t>(i)].id)return i;
    return 0;
}
inline int moduleIndex(const juce::String& key) {
    for(int i=0;i<static_cast<int>(modules.size());++i)
        if(key==modules[static_cast<std::size_t>(i)].id)return i;
    return 0;
}
inline void blit(juce::Graphics& g,const juce::Image& image,
                 Region src,juce::Rectangle<float> dst) {
    if(!image.isValid() || dst.isEmpty())return;
    g.drawImage(image,
                static_cast<int>(std::round(dst.getX())),
                static_cast<int>(std::round(dst.getY())),
                static_cast<int>(std::round(dst.getWidth())),
                static_cast<int>(std::round(dst.getHeight())),
                src.x,src.y,src.w,src.h);
}

inline float photoOuterDiameter(const DialDescriptor& d,float uiScale=1.f) {
    // Source cap circle has precisely the nominal visible metal diameter.
    // The source crop includes bevel/ticks and soft shadow outside that face.
    return d.nominalDiameter*uiScale*
        static_cast<float>(d.full.w)/static_cast<float>(2*d.sourceCapRadius);
}

class PhotoDialLookAndFeel final : public juce::LookAndFeel_V4 {
public:
    void drawRotarySlider(juce::Graphics& g,int x,int y,int width,int height,
                          float normalized,float,float,juce::Slider& slider) override {
        const int id=std::clamp(static_cast<int>(slider.getProperties()["p6.photo.dial"]),0,
            static_cast<int>(dials.size())-1);
        const auto& part=dials[static_cast<std::size_t>(id)];
        const float scale=std::clamp(static_cast<float>(
            static_cast<double>(slider.getProperties()["p6.photo.scale"])),.5f,4.f);
        const float outer=std::min(photoOuterDiameter(part,scale),
                     static_cast<float>(std::min(width,height))-2.f);
        if(outer<=2.f)return;
        const float cx=x+width*.5f,cy=y+height*.5f;
        const juce::Rectangle<float> dst{cx-outer*.5f,cy-outer*.5f,outer,outer};
        if(!slider.isEnabled())g.setOpacity(.44f);
        // The recessed ring remains static. Only the photographed cap rotates.
        blit(g,dialAtlas(),part.ring,dst);
        {
            juce::Graphics::ScopedSaveState saved(g);
            const float rotation=(std::clamp(normalized,0.f,1.f)-.5f)*4.38f;
            g.addTransform(juce::AffineTransform::rotation(rotation,cx,cy));
            blit(g,dialAtlas(),part.cap,dst);
        }
        // Focus ring surrounds the physical part but not the entire hit target.
        if(slider.hasKeyboardFocus(false)) {
            g.setColour(juce::Colour(0xfff14b44).withAlpha(.9f));
            g.drawEllipse(dst.expanded(1.f),1.2f);
        }
    }
};
inline PhotoDialLookAndFeel& lookAndFeel() {
    static PhotoDialLookAndFeel instance;
    return instance;
}

// Native JUCE parameter slider: mouse drag, wheel, arrows, automation,
// and original ProphecyParameterControl context behaviour remain intact.
class Dial final : public ProphecyParameterControl {
public:
    explicit Dial(const juce::String& variant="xl-hero",float previewScale=1.f) {
        setSliderStyle(juce::Slider::RotaryVerticalDrag);
        setTextBoxStyle(juce::Slider::NoTextBox,false,0,0);
        setRange(0.,1.,.001);
        setValue(.5,juce::dontSendNotification);
        setVariant(variant);
        setPreviewScale(previewScale);
        setLookAndFeel(&photo::lookAndFeel());
        setWantsKeyboardFocus(true);
    }
    ~Dial() override {setLookAndFeel(nullptr);}
    void setVariant(const juce::String& name) {
        auto index=dialIndex(name);
        getProperties().set("p6.photo.dial",index);
        setTooltip(juce::String(dials[static_cast<std::size_t>(index)].id));
        repaint();
    }
    void setPreviewScale(float scale) {
        getProperties().set("p6.photo.scale",std::clamp(scale,.5f,4.f));
        repaint();
    }
};

// Other photographed controls are composable independent photo sprites.
// Display-only class: not a substitute for an APVTS-bound fader/button.
class StaticModule : public juce::Component {
public:
    explicit StaticModule(const juce::String& id) {setModule(id);}
    void setModule(const juce::String& id) {
        module_=moduleIndex(id);setTooltip(id);repaint();
    }
    const ModuleDescriptor& descriptor() const noexcept {
        return modules[static_cast<std::size_t>(module_)];
    }
    void paint(juce::Graphics& g) override {
        auto src=descriptor().crop;
        auto bounds=getLocalBounds().toFloat();
        const auto scale=std::min(bounds.getWidth()/src.w,bounds.getHeight()/src.h);
        auto dst=juce::Rectangle<float>(
            bounds.getCentreX()-src.w*scale*.5f,
            bounds.getCentreY()-src.h*scale*.5f,
            src.w*scale,src.h*scale);
        if(!isEnabled())g.setOpacity(.45f);
        blit(g,moduleAtlas(),src,dst);
    }
private:
    int module_{};
};

// Photo-style toggles use JUCE's native Button input and event semantics.
// Full on/off source-state sprites will replace the current opacity preview.
class Toggle final : public juce::Button {
public:
    explicit Toggle(const juce::String& id) : juce::Button(id) {
        module_=moduleIndex(id);setClickingTogglesState(true);
        setWantsKeyboardFocus(true);
        setTooltip(id);
    }
    void paintButton(juce::Graphics& g,bool hover,bool pressed) override {
        const auto source=modules[static_cast<std::size_t>(module_)].crop;
        auto bounds=getLocalBounds().toFloat().reduced(1.f);
        const float scale=std::min(bounds.getWidth()/source.w,bounds.getHeight()/source.h);
        auto dst=juce::Rectangle<float>(
            bounds.getCentreX()-source.w*scale*.5f,
            bounds.getCentreY()-source.h*scale*.5f,
            source.w*scale,source.h*scale);
        if(!isEnabled())g.setOpacity(.35f);
        else if(!getToggleState()&&!pressed)g.setOpacity(hover?.65f:.42f);
        blit(g,moduleAtlas(),source,dst);
        if(hasKeyboardFocus(false)){
            g.setColour(juce::Colour(0xffff6359));
            g.drawRoundedRectangle(bounds.reduced(1.f),4.f,1.f);
        }
    }
private:
    int module_{};
};

} // namespace p6plugin::hardware::photo
