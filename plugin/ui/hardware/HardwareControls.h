#pragma once
#include "HardwareTheme.h"
#include "../controls/ProphecyParameterControl.h"
#include "../controls/SegmentedLedRing.h"
#include <array>
#include <functional>

namespace p6plugin::hardware {

namespace detail {
inline juce::String property(const juce::Component& c, const char* key) {
    return c.getProperties()[juce::Identifier(key)].toString();
}
inline void put(juce::Component& c, const char* key, const juce::var& value) {
    c.getProperties().set(juce::Identifier(key), value);
}
inline float normalized(const juce::Slider& s) {
    return static_cast<float>(std::clamp(s.valueToProportionOfLength(s.getValue()), 0., 1.));
}
} // namespace detail

// One shared, stable LookAndFeel. Consumers MUST outlive neither this nor the
// controls using it. This is a function static, not one instance per knob.
class HardwareLookAndFeel final : public juce::LookAndFeel_V4 {
public:
    HardwareLookAndFeel() {
        setColour(juce::ComboBox::backgroundColourId, Palette::colour(Palette::chassis));
        setColour(juce::ComboBox::textColourId, Palette::colour(Palette::silver));
        setColour(juce::ComboBox::outlineColourId, Palette::colour(Palette::rim));
        setColour(juce::PopupMenu::backgroundColourId, Palette::colour(Palette::surface));
        setColour(juce::PopupMenu::textColourId, Palette::colour(Palette::silver));
        setColour(juce::PopupMenu::highlightedBackgroundColourId, Palette::colour(Palette::red));
    }

    // The visible diameter is a design token, never a fraction of the hit box.
    // An oversized interaction box must not silently turn NANO into HERO.
    void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                          float position, float startAngle, float endAngle,
                          juce::Slider& slider) override {
        const auto cap = detail::property(slider, "p6.hardware.cap");
        const auto size = detail::property(slider, "p6.hardware.size");
        const float nominal = size == "hero" ? Geometry::dialDiameter(DialSize::hero)
            : size == "large" ? Geometry::dialDiameter(DialSize::large)
            : size == "trim" ? Geometry::dialDiameter(DialSize::trim)
            : size == "micro" ? Geometry::dialDiameter(DialSize::micro)
            : size == "nano" ? Geometry::dialDiameter(DialSize::nano)
            : Geometry::dialDiameter(DialSize::medium);
        const bool led = detail::property(slider, "p6.hardware.led") != "0";
        const bool prominent = nominal >= 48.f;
        const float clearance = led ? std::max(4.f, nominal * .16f) : 2.f;
        const float diameter = std::min(nominal, std::max(4.f,
            std::min(static_cast<float>(width), static_cast<float>(height)) - 2.f * clearance));
        const float radius = diameter * .5f;
        const float cx = x + width * .5f, cy = y + height * .5f;
        const float angle = startAngle + position * (endAngle - startAngle);
        const auto silver = Palette::colour(Palette::silver);
        const auto rim = Palette::colour(Palette::rim);
        const auto red = Palette::colour(Palette::redBright);

        if (led) {
            std::optional<float> neutral;
            const auto zero = detail::property(slider, "p6.hardware.zero");
            if (zero.isNotEmpty()) neutral = std::clamp(zero.getFloatValue(), 0.f, 1.f);
            const float inner = radius + std::max(1.2f, diameter * .055f);
            const float outer = inner + std::clamp(diameter * .09f, 1.5f, 4.7f);
            for (int i = 0; i < kLedRingSegments; ++i) {
                const float a = startAngle + (endAngle - startAngle) *
                    static_cast<float>(i) / static_cast<float>(kLedRingSegments - 1);
                const auto state = ledSegmentState(i, position, neutral);
                const float dx = std::sin(a), dy = -std::cos(a);
                g.setColour(state.lit ? red : rim.withAlpha(.42f));
                g.drawLine(cx + dx * inner, cy + dy * inner,
                           cx + dx * outer, cy + dy * outer,
                           state.tip ? (prominent ? 2.0f : 1.2f) : (prominent ? 1.5f : .85f));
            }
        }

        // Recess, machined metal rim, top and bottom bevel, and contact shadow.
        g.setColour(juce::Colours::black.withAlpha(.66f));
        g.fillEllipse(cx - radius - 1.5f, cy - radius + 1.5f,
                      diameter + 3.f, diameter + 3.f);
        g.setGradientFill(juce::ColourGradient(silver.withAlpha(.70f),
            cx - radius, cy - radius, Palette::colour(Palette::chassis),
            cx + radius, cy + radius, false));
        g.fillEllipse(cx - radius, cy - radius, diameter, diameter);
        g.setColour(silver.withAlpha(.28f));
        g.drawEllipse(cx - radius + .7f, cy - radius + .7f,
                      diameter - 1.4f, diameter - 1.4f, prominent ? 1.f : .6f);

        const float bevel = std::clamp(diameter * .085f, 1.f, 4.5f);
        const float faceRadius = radius - bevel;
        const auto face = juce::Rectangle<float>(cx - faceRadius, cy - faceRadius,
                                                faceRadius * 2.f, faceRadius * 2.f);
        const bool metallic = cap == "brushed";
        g.setGradientFill(juce::ColourGradient(
            metallic ? Palette::colour(0xffc9ccce) : Palette::colour(0xff575a60),
            face.getX(), face.getY(),
            metallic ? Palette::colour(0xff4e535a) : Palette::colour(0xff0c0d10),
            face.getRight(), face.getBottom(), false));
        g.fillEllipse(face);
        g.setColour(silver.withAlpha(metallic ? .48f : .16f));
        g.drawEllipse(face.reduced(.5f), prominent ? 1.f : .65f);

        // Subtle visible brushing lines are physically confined to the circular face.
        if (metallic && diameter >= 24.f) {
            g.setColour(juce::Colours::white.withAlpha(.075f));
            for (float offset = -faceRadius + 3.f; offset < faceRadius - 2.f; offset += 2.6f) {
                const float chord = std::sqrt(std::max(0.f, faceRadius * faceRadius - offset * offset));
                g.drawLine(cx - chord, cy + offset, cx + chord, cy + offset, .55f);
            }
        }
        if (cap == "knurled" || cap == "scalloped") {
            const int marks = cap == "scalloped" ? 12 : (diameter >= 36.f ? 36 : 18);
            g.setColour(juce::Colours::black.withAlpha(.67f));
            for (int i = 0; i < marks; ++i) {
                const float a = juce::MathConstants<float>::twoPi *
                    static_cast<float>(i) / static_cast<float>(marks);
                const float r1 = radius - std::max(1.5f, bevel * 1.05f);
                const float r2 = radius - .5f;
                g.drawLine(cx + std::cos(a) * r1, cy + std::sin(a) * r1,
                           cx + std::cos(a) * r2, cy + std::sin(a) * r2,
                           diameter >= 36.f ? 1.f : .6f);
            }
        }

        // Engraved index remains legible on the four distinct cap finishes.
        g.setColour(metallic ? Palette::colour(Palette::chassis) : silver);
        g.drawLine(cx + std::sin(angle) * radius * .23f,
                   cy - std::cos(angle) * radius * .23f,
                   cx + std::sin(angle) * faceRadius * .78f,
                   cy - std::cos(angle) * faceRadius * .78f,
                   std::clamp(diameter * .039f, .85f, 2.6f));
        if (slider.hasKeyboardFocus(false)) {
            g.setColour(red.withAlpha(.9f));
            g.drawEllipse(cx - radius - clearance, cy - radius - clearance,
                          diameter + clearance * 2.f, diameter + clearance * 2.f, 1.f);
        }
    }

    void drawLinearSlider(juce::Graphics& g, int x, int y, int width, int height,
                          float, float, float, juce::Slider::SliderStyle style,
                          juce::Slider& slider) override {
        const bool vertical = style == juce::Slider::LinearVertical ||
                              style == juce::Slider::LinearBarVertical;
        const auto variant = detail::property(slider, "p6.hardware.slider");
        const auto kind = variant == "long-v" ? SliderSize::longVertical
            : variant == "compact-v" ? SliderSize::compactVertical
            : variant == "mini-v" ? SliderSize::miniVertical
            : variant == "long-h" ? SliderSize::horizontal
            : variant == "trim-h" ? SliderSize::trimHorizontal
            : SliderSize::compactHorizontal;
        const auto nominal = Geometry::sliderSize(kind);
        const auto bounds = juce::Rectangle<float>(
            static_cast<float>(x), static_cast<float>(y),
            static_cast<float>(width), static_cast<float>(height));
        const float room = vertical ? bounds.getHeight() : bounds.getWidth();
        const float targetLength = static_cast<float>(vertical ? nominal.y : nominal.x);
        // Track is centered and capped at its token length even if the click target is larger.
        const float length = std::max(5.f, std::min(targetLength, room - 18.f));
        const auto track = vertical
            ? juce::Rectangle<float>(bounds.getCentreX() - 2.7f,
                                     bounds.getCentreY() - length * .5f, 5.4f, length)
            : juce::Rectangle<float>(bounds.getCentreX() - length * .5f,
                                     bounds.getCentreY() - 2.7f, length, 5.4f);
        g.setColour(juce::Colours::black.withAlpha(.72f));
        g.fillRoundedRectangle(track.expanded(2.1f), 3.3f);
        g.setColour(Palette::colour(Palette::rim).withAlpha(.75f));
        g.drawRoundedRectangle(track.expanded(.7f), 2.8f, .8f);
        const auto red = Palette::colour(Palette::redBright);
        const float fraction = detail::normalized(slider);
        const float position = vertical ? track.getBottom() - fraction * track.getHeight()
                                        : track.getX() + fraction * track.getWidth();
        g.setColour(red.withAlpha(.73f));
        if (vertical)
            g.fillRect(juce::Rectangle<float>(track.getX() + 1.f, position,
                       track.getWidth() - 2.f, track.getBottom() - position));
        else
            g.fillRect(juce::Rectangle<float>(track.getX(), track.getY() + 1.f,
                       position - track.getX(), track.getHeight() - 2.f));

        const float thumbW = vertical ? (kind == SliderSize::longVertical ? 23.f :
            kind == SliderSize::compactVertical ? 19.f : 15.f) : 12.f;
        const float thumbH = vertical ? 11.f : (kind == SliderSize::horizontal ? 23.f : 18.f);
        auto thumb = juce::Rectangle<float>(
            (vertical ? track.getCentreX() : position) - thumbW * .5f,
            (vertical ? position : track.getCentreY()) - thumbH * .5f,
            thumbW, thumbH);
        paintThumb(g, thumb);
        if (slider.hasKeyboardFocus(false)) {
            g.setColour(red.withAlpha(.8f));
            g.drawRoundedRectangle(bounds.reduced(1.f), 4.f, 1.f);
        }
    }

    void drawButtonBackground(juce::Graphics& g, juce::Button& button,
                              const juce::Colour&, bool hover, bool down) override {
        auto r=button.getLocalBounds().toFloat().reduced(2.f);
        const auto kind=detail::property(button,"p6.hardware.button");
        const bool active=button.getToggleState() || down;
        const bool round=kind=="round" || kind=="utility";
        const bool cap=kind=="capacitive";
        const float rad=round?r.getHeight()*.5f:(cap?12.f:4.f);
        g.setColour(Palette::colour(Palette::chassis));
        g.fillRoundedRectangle(r,rad);
        g.setColour(Palette::colour(Palette::rim));
        g.drawRoundedRectangle(r,rad,1.f);
        r=r.reduced(cap?2.f:3.f);
        const auto top=active ? Palette::colour(0xffbb2b27) :
                        (hover?Palette::colour(0xff45474b):Palette::colour(Palette::plate));
        const auto bottom=active ? Palette::colour(Palette::redBright) :
                           Palette::colour(Palette::chassis);
        g.setGradientFill(juce::ColourGradient(top,r.getX(),r.getY(),bottom,r.getX(),r.getBottom(),false));
        g.fillRoundedRectangle(r,std::max(2.f,rad-2.f));
        if(active){
            paintGlow(g,r.getCentre(),std::max(r.getWidth(),r.getHeight())*.65f,
                      Palette::colour(Palette::redBright),cap?.18f:.28f);
        }
        if(button.hasKeyboardFocus(false)){
            g.setColour(Palette::colour(Palette::silver));g.drawRoundedRectangle(r,rad,1.f);
        }
    }

    void drawButtonText(juce::Graphics& g, juce::TextButton& button, bool, bool) override {
        auto r=button.getLocalBounds();
        const auto kind=detail::property(button,"p6.hardware.button");
        g.setFont(juce::Font(juce::FontOptions(kind=="utility"?10.f:12.f,
                   kind=="capacitive"?juce::Font::bold:juce::Font::plain)));
        g.setColour(button.getToggleState()?Palette::colour(Palette::silver):
                    Palette::colour(0xffc8c8c8));
        g.drawFittedText(button.getButtonText(),r.reduced(3),juce::Justification::centred,1);
    }

    void drawToggleButton(juce::Graphics& g, juce::ToggleButton& button,
                          bool over, bool down) override {
        drawButtonBackground(g,button,Palette::colour(Palette::surface),over,down);
        g.setColour(Palette::colour(Palette::silver));
        g.setFont(juce::Font(11.f));
        g.drawFittedText(button.getButtonText(),button.getLocalBounds().reduced(4),
                         juce::Justification::centred,1);
    }

    void drawComboBox(juce::Graphics& g, int width, int height, bool, int, int, int, int,
                      juce::ComboBox& box) override {
        paintPlate(g,juce::Rectangle<float>(0,0,static_cast<float>(width),static_cast<float>(height)),5.f);
        const float cx=width-17.f, cy=height*.5f;
        juce::Path path;
        path.startNewSubPath(cx-5.f,cy-2.f);path.lineTo(cx,cy+3.f);path.lineTo(cx+5.f,cy-2.f);
        g.setColour(Palette::colour(Palette::redBright));g.strokePath(path,juce::PathStrokeType(1.8f));
        if(box.hasKeyboardFocus(false)) {
            g.setColour(Palette::colour(Palette::silver));
            g.drawRoundedRectangle(juce::Rectangle<float>(1,1,width-2.f,height-2.f),4.f,1.f);
        }
    }

private:
    static void paintThumb(juce::Graphics& g,juce::Rectangle<float> bounds) {
        g.setColour(Palette::colour(Palette::chassis));g.fillRoundedRectangle(bounds.expanded(1.f),3.f);
        g.setGradientFill(juce::ColourGradient(Palette::colour(Palette::silver),
            bounds.getX(),bounds.getY(),Palette::colour(Palette::rim),
            bounds.getRight(),bounds.getBottom(),false));
        g.fillRoundedRectangle(bounds,2.f);
        g.setColour(Palette::colour(Palette::chassis));g.drawLine(bounds.getX()+2.f,bounds.getCentreY(),
                         bounds.getRight()-2.f,bounds.getCentreY(),1.f);
    }
};

inline HardwareLookAndFeel& lookAndFeel() {
    static HardwareLookAndFeel instance;
    return instance;
}

class HardwareDial : public ProphecyParameterControl {
public:
    explicit HardwareDial(DialSize size=DialSize::medium, Cap cap=Cap::softBlack) {
        setSliderStyle(juce::Slider::RotaryVerticalDrag);
        setTextBoxStyle(juce::Slider::NoTextBox,false,0,0);
        setRange(0.,1.,.001);setValue(.5,juce::dontSendNotification);
        setLookAndFeel(&lookAndFeel());setSizeVariant(size);setCap(cap);
        detail::put(*this,"p6.hardware.led",1);
        setWantsKeyboardFocus(true);
    }
    ~HardwareDial() override { setLookAndFeel(nullptr); }
    void setSizeVariant(DialSize size) {
        static constexpr const char* names[]={"hero","large","medium","trim","micro","nano"};
        detail::put(*this,"p6.hardware.size",names[static_cast<int>(size)]);
        size_=size;repaint();
    }
    void setCap(Cap cap) {
        static constexpr const char* names[]={"brushed","black","knurled","scalloped"};
        detail::put(*this,"p6.hardware.cap",names[static_cast<int>(cap)]);repaint();
    }
    void setLedEnabled(bool on) {detail::put(*this,"p6.hardware.led",on?1:0);repaint();}
    // Position in the normalized domain; optional semantic bipolar zero.
    void setNeutralPosition(std::optional<float> normalized) {
        detail::put(*this,"p6.hardware.zero",normalized?juce::var(*normalized):juce::var{});
        repaint();
    }
    DialSize sizeVariant() const noexcept {return size_;}
private:
    DialSize size_{DialSize::medium};
};

class HardwareFader : public ProphecyParameterControl {
public:
    explicit HardwareFader(SliderSize size=SliderSize::longVertical) {
        setTextBoxStyle(juce::Slider::NoTextBox,false,0,0);
        setRange(0.,1.,.001);setValue(.5,juce::dontSendNotification);
        setLookAndFeel(&lookAndFeel());setSizeVariant(size);
        setWantsKeyboardFocus(true);
    }
    ~HardwareFader() override {setLookAndFeel(nullptr);}
    void setSizeVariant(SliderSize size) {
        size_=size;
        static constexpr const char* names[] = {
            "long-v", "compact-v", "mini-v", "long-h", "compact-h", "trim-h"
        };
        detail::put(*this, "p6.hardware.slider", names[static_cast<int>(size)]);
        setSliderStyle(static_cast<int>(size)<=static_cast<int>(SliderSize::miniVertical)
            ? juce::Slider::LinearVertical : juce::Slider::LinearHorizontal);
        repaint();
    }
    SliderSize sizeVariant() const noexcept {return size_;}
private:
    SliderSize size_{SliderSize::longVertical};
};

class HardwareButton : public juce::TextButton {
public:
    explicit HardwareButton(const juce::String& label={},ButtonKind kind=ButtonKind::square)
        : juce::TextButton(label) {
        setLookAndFeel(&lookAndFeel());setKind(kind);
        setClickingTogglesState(true);setWantsKeyboardFocus(true);
    }
    ~HardwareButton() override {setLookAndFeel(nullptr);}
    void setKind(ButtonKind kind) {
        static constexpr const char* names[]={"rectangular","square","round","utility","capacitive","step"};
        detail::put(*this,"p6.hardware.button",names[static_cast<int>(kind)]);repaint();
    }
};

class HardwareSwitch : public juce::ToggleButton {
public:
    explicit HardwareSwitch(const juce::String& text={}) : juce::ToggleButton(text) {
        setLookAndFeel(&lookAndFeel());detail::put(*this,"p6.hardware.button","rectangular");
        setWantsKeyboardFocus(true);
    }
    ~HardwareSwitch() override {setLookAndFeel(nullptr);}
};

class HardwareStepButton : public HardwareButton {
public:
    explicit HardwareStepButton(int step=1) : HardwareButton(juce::String(step),ButtonKind::step),step_(step) {}
    void setPlaying(bool playing) {playing_=playing;repaint();}
    int step() const noexcept {return step_;}
    bool playing() const noexcept {return playing_;}
    void paint(juce::Graphics& g) override {
        HardwareButton::paint(g);
        if(playing_ && getToggleState()) {
            g.setColour(Palette::colour(Palette::silver));
            g.fillEllipse(getWidth()*.5f-2.f,2.f,4.f,4.f);
        }
    }
private:
    int step_{}; bool playing_{};
};

class HardwareDropdown : public juce::ComboBox {
public:
    HardwareDropdown() {setLookAndFeel(&lookAndFeel());setWantsKeyboardFocus(true);}
    ~HardwareDropdown() override {setLookAndFeel(nullptr);}
};

// Three discrete positions are represented as a standard ComboBox so that
// APVTS ComboBoxAttachment, keyboard navigation and host automation work.
class HardwareThreePosition : public HardwareDropdown {
public:
    HardwareThreePosition() {
        addItem("A",1);addItem("B",2);addItem("C",3);setSelectedId(2,juce::dontSendNotification);
    }
};

// Rotary selector with stepped values and optional human-readable labels.
// Bind using an existing parameter's SliderAttachment (not a new state).
class HardwareRotarySelector : public HardwareDial {
public:
    HardwareRotarySelector() : HardwareDial(DialSize::large,Cap::softBlack) {
        setLedEnabled(false);
        setPositions({"LP","BP","HP","NOTCH"});
    }
    void setPositions(const juce::StringArray& items) {
        positions_=items;
        const int count=std::max(1,items.size());
        setRange(0.,static_cast<double>(count-1),1.);
        setValue(0.,juce::dontSendNotification);
    }
    juce::String selectedLabel() const {
        if(positions_.isEmpty()) return {};
        const int index=std::clamp(static_cast<int>(std::lround(getValue())),
                                   0,std::max(0,positions_.size()-1));
        return positions_[index];
    }
private:
    juce::StringArray positions_;
};

class HardwareEncoder : public HardwareDial {
public:
    explicit HardwareEncoder(DialSize size=DialSize::medium)
        : HardwareDial(size,Cap::knurled) {
        setLedEnabled(true);
    }
};

// Distinct mechanical mini-toggle rather than a resized rectangular button.
// Inherits JUCE Button for keyboard/accessibility and APVTS ButtonAttachment.
class HardwareMiniToggle final : public juce::ToggleButton {
public:
    HardwareMiniToggle() {
        setWantsKeyboardFocus(true);
        setMouseClickGrabsKeyboardFocus(true);
        setTitle("Mini toggle");
    }
    void paint(juce::Graphics& g) override {
        const float cx=getWidth()*.5f, cy=getHeight()*.5f;
        const bool on=getToggleState();
        auto well=juce::Rectangle<float>(cx-12.f,cy-15.f,24.f,30.f);
        paintPlate(g,well,11.f);
        if(on)paintGlow(g,{cx,cy-7.f},17.f,Palette::colour(Palette::redBright),.28f);
        const float endpoint=cy+(on?-11.f:11.f);
        g.setColour(Palette::colour(Palette::chassis));
        g.drawLine(cx+2.f,cy+4.f,cx-3.f,endpoint+1.f,6.f);
        g.setGradientFill(juce::ColourGradient(Palette::colour(Palette::silver),
            cx-3.f,endpoint-11.f,Palette::colour(Palette::rim),
            cx+5.f,endpoint+4.f,false));
        g.drawLine(cx,cy+3.f,cx,endpoint,5.f);
        g.fillEllipse(cx-5.f,endpoint-5.f,10.f,10.f);
        if(hasKeyboardFocus(false)){
            g.setColour(Palette::colour(Palette::redBright));g.drawRoundedRectangle(
                getLocalBounds().toFloat().reduced(1.f),5.f,1.f);
        }
    }
};

class HardwareLamp final : public juce::Component {
public:
    explicit HardwareLamp(LampKind kind=LampKind::red) : kind_(kind) {}
    void setState(bool on) {if(on_!=on){on_=on;repaint();}}
    void setKind(LampKind k) {kind_=k;repaint();}
    bool state() const noexcept {return on_;}
    void paint(juce::Graphics& g) override {
        const float r=std::min(getWidth(),getHeight())*.27f;
        const juce::Point<float> p(getWidth()*.5f,getHeight()*.5f);
        if(on_)paintGlow(g,p,r*2.f,lampColour(kind_),.4f);
        g.setColour(Palette::colour(Palette::chassis));g.fillEllipse(p.x-r-2,p.y-r-2,2*r+4,2*r+4);
        g.setGradientFill(juce::ColourGradient(on_?lampColour(kind_):Palette::colour(Palette::rim),
            p.x-r,p.y-r,Palette::colour(Palette::chassis),p.x+r,p.y+r,false));
        g.fillEllipse(p.x-r,p.y-r,2*r,2*r);
        g.setColour(juce::Colours::white.withAlpha(on_?.8f:.2f));
        g.fillEllipse(p.x-r*.5f,p.y-r*.6f,r*.45f,r*.45f);
    }
private:
    LampKind kind_; bool on_{};
};

class HardwareScrew final : public juce::Component {
public:
    void paint(juce::Graphics& g) override {
        paintHardwareScrew(g,{getWidth()*.5f,getHeight()*.5f},
                           std::min(getWidth(),getHeight())*.36f);
    }
};

} // namespace p6plugin::hardware
