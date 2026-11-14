#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace p6plugin::hardware {

// The library uses the same visual tokens across all controls. Colours and
// pixels are presentation-only: the existing parameter domains remain authoritative.
struct Palette final {
    static constexpr std::uint32_t chassis = 0xff080b0c;
    static constexpr std::uint32_t surface = 0xff1a1a1d;
    static constexpr std::uint32_t plate = 0xff2e2f33;
    static constexpr std::uint32_t rim = 0xff5b6065;
    static constexpr std::uint32_t silver = 0xffd8d8d8;
    static constexpr std::uint32_t muted = 0xff7a7a7a;
    static constexpr std::uint32_t red = 0xffe53935;
    static constexpr std::uint32_t redBright = 0xffff6254;
    static constexpr std::uint32_t cyan = 0xff0ce6ff; // displays only
    static constexpr std::uint32_t amber = 0xffffad48;
    static constexpr std::uint32_t green = 0xff53cc78; // semantic status only
    static juce::Colour colour(std::uint32_t argb) noexcept { return juce::Colour(argb); }
};

enum class DialSize { hero, large, medium, trim, micro, nano };
enum class Cap { brushedMetal, softBlack, knurled, scalloped };
enum class SliderSize { longVertical, compactVertical, miniVertical,
                        horizontal, compactHorizontal, trimHorizontal };
enum class ButtonKind { rectangular, square, round, utility, capacitive, step };
enum class MeterKind { stereoLed, analogVu, ledBar };
enum class SignalKind { spectrum, oscilloscope, morph, envelope };
enum class LampKind { red, amber, green, blue, silver };

struct Geometry final {
    static constexpr float grid = 8.f;
    static constexpr float normalGap = 16.f;
    static constexpr float denseGap = 8.f;
    static constexpr float modulePadding = 16.f;
    static constexpr float minimumHitSize = 28.f;
    static constexpr int dialDiameter(DialSize s) noexcept {
        switch (s) {
            case DialSize::hero: return 64;
            case DialSize::large: return 48;
            case DialSize::medium: return 36;
            case DialSize::trim: return 24;
            case DialSize::micro: return 18;
            case DialSize::nano: return 14;
        }
        return 36;
    }
    static juce::Point<int> sliderSize(SliderSize s) noexcept {
        switch (s) {
            case SliderSize::longVertical: return {30, 80};
            case SliderSize::compactVertical: return {26, 56};
            case SliderSize::miniVertical: return {24, 40};
            case SliderSize::horizontal: return {96, 26};
            case SliderSize::compactHorizontal: return {72, 24};
            case SliderSize::trimHorizontal: return {48, 22};
        }
        return {72, 24};
    }
};

inline juce::Colour lampColour(LampKind kind) noexcept {
    switch (kind) {
        case LampKind::red: return Palette::colour(Palette::redBright);
        case LampKind::amber: return Palette::colour(Palette::amber);
        case LampKind::green: return Palette::colour(Palette::green);
        case LampKind::blue: return Palette::colour(Palette::cyan);
        case LampKind::silver: return Palette::colour(Palette::silver);
    }
    return Palette::colour(Palette::silver);
}

inline void paintPlate(juce::Graphics& g, juce::Rectangle<float> bounds, float radius = 7.f) {
    g.setColour(Palette::colour(Palette::chassis));
    g.fillRoundedRectangle(bounds, radius);
    g.setGradientFill(juce::ColourGradient(Palette::colour(Palette::plate),
                       bounds.getX(), bounds.getY(),
                       Palette::colour(Palette::surface),
                       bounds.getX(), bounds.getBottom(), false));
    g.fillRoundedRectangle(bounds.reduced(1.f), radius - 1.f);
    g.setColour(Palette::colour(Palette::rim).withAlpha(.75f));
    g.drawRoundedRectangle(bounds.reduced(.6f), radius, 1.f);
}

inline void paintHardwareScrew(juce::Graphics& g, juce::Point<float> p, float radius = 5.f) {
    g.setColour(Palette::colour(Palette::chassis)); g.fillEllipse(p.x-radius,p.y-radius,2*radius,2*radius);
    g.setGradientFill(juce::ColourGradient(Palette::colour(Palette::silver),
        p.x-radius,p.y-radius, Palette::colour(Palette::rim),p.x+radius,p.y+radius,false));
    g.fillEllipse(p.x-radius*.75f,p.y-radius*.75f,radius*1.5f,radius*1.5f);
    g.setColour(Palette::colour(Palette::surface));
    g.drawLine(p.x-radius*.38f,p.y+radius*.38f,p.x+radius*.38f,p.y-radius*.38f,1.2f);
}

inline void paintGlow(juce::Graphics& g, juce::Point<float> centre,
                      float radius, juce::Colour colour, float alpha = .30f) {
    g.setGradientFill(juce::ColourGradient(colour.withAlpha(alpha),centre.x,centre.y,
                       colour.withAlpha(0.f),centre.x+radius,centre.y,true));
    g.fillEllipse(centre.x-radius,centre.y-radius,radius*2.f,radius*2.f);
}

} // namespace p6plugin::hardware
