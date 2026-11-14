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

    void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                          float position, float startAngle, float endAngle,
                          juce::Slider& slider) override {
        const auto cap = detail::property(slider, "p6.hardware.cap");
        const auto size = detail::property(slider, "p6.hardware.size");
        const bool large = size == "hero" || size == "large";
        const float diameter = std::min(static_cast<float>(width), static_cast<float>(height)) * .76f;
        const float cx = x + width*.5f, cy = y + height*.5f;
        const float radius = diameter*.5f;
        auto centre = juce::Point<float>(cx,cy);
        const float angle = startAngle + position * (endAngle-startAngle);
        const auto red = Palette::colour(Palette::redBright);
        const auto dim = Palette::colour(Palette::rim);
        const bool led = detail::property(slider,"p6.hardware.led") != "0";
        if (led) {
            std::optional<float> neutral;
            const auto zero = detail::property(slider,"p6.hardware.zero");
            if (zero.isNotEmpty()) neutral = std::clamp(zero.getFloatValue(),0.f,1.f);
            for(int i=0;i<kLedRingSegments;++i) {
                const float a=startAngle+(endAngle-startAngle)*static_cast<float>(i)/(kLedRingSegments-1);
                const auto state=ledSegmentState(i,position,neutral);
                const auto inner=radius+5.f, outer=inner+(large?5.f:3.f);
                g.setColour(state.lit ? red : dim.withAlpha(.65f));
                g.drawLine(cx+std::sin(a)*inner,cy-std::cos(a)*inner,
                           cx+std::sin(a)*outer,cy-std::cos(a)*outer,state.tip?2.2f:1.5f);
            }
        }
        g.setColour(juce::Colours::black.withAlpha(.65f));
        g.fillEllipse(cx-radius-2.f,cy-radius+1.f,diameter+4.f,diameter+4.f);
        g.setGradientFill(juce::ColourGradient(Palette::colour(Palette::rim),
            cx-radius,cy-radius,Palette::colour(Palette::chassis),cx+radius,cy+radius,false));
        g.fillEllipse(cx-radius,cy-radius,diameter,diameter);
        const float recess = (cap == "knurled" || cap == "scalloped") ? 4.f : 3.f;
        const auto disc = juce::Rectangle<float>(cx-radius+recess,cy-radius+recess,
                                                  diameter-2*recess,diameter-2*recess);
        juce::Colour top = cap=="brushed" ? Palette::colour(Palette::silver) : Palette::colour(0xff57595d);
        juce::Colour bottom = cap=="brushed" ? Palette::colour(0xff585b60) : Palette::colour(0xff090b0d);
        g.setGradientFill(juce::ColourGradient(top,disc.getX(),disc.getY(),bottom,disc.getRight(),disc.getBottom(),false));
        g.fillEllipse(disc);
        g.setColour(Palette::colour(Palette::silver).withAlpha(cap=="brushed"?.5f:.17f));
        g.drawEllipse(disc.reduced(.8f),1.f);
        if (cap=="knurled" || cap=="scalloped") {
            const int marks=cap=="scalloped"?12:32;
            g.setColour(Palette::colour(Palette::chassis).withAlpha(.75f));
            for(int i=0;i<marks;++i) {
                const float a=juce::MathConstants<float>::twoPi*i/marks;
                g.drawLine(cx+std::cos(a)*(radius-2.f),cy+std::sin(a)*(radius-2.f),
                           cx+std::cos(a)*(radius-5.f),cy+std::sin(a)*(radius-5.f),1.f);
            }
        }
        g.setColour(cap=="brushed"?Palette::colour(Palette::chassis):Palette::colour(Palette::silver));
        g.drawLine(cx+std::sin(angle)*radius*.2f,cy-std::cos(angle)*radius*.2f,
                   cx+std::sin(angle)*radius*.78f,cy-std::cos(angle)*radius*.78f,
                   large?2.6f:1.8f);
        if(slider.hasKeyboardFocus(false)) {
            g.setColour(red.withAlpha(.85f));
            g.drawEllipse(juce::Rectangle<float>(cx-radius-5,cy-radius-5,diameter+10,diameter+10),1.f);
        }
    }

    void drawLinearSlider(juce::Graphics& g, int x, int y, int width, int height,
                          float sliderPos, float, float, juce::Slider::SliderStyle style,
                          juce::Slider& slider) override {
        const bool vertical = style==juce::Slider::LinearVertical ||
                              style==juce::Slider::LinearBarVertical;
        auto bounds = juce::Rectangle<float>(static_cast<float>(x),static_cast<float>(y),
                                              static_cast<float>(width),static_cast<float>(height));
        auto track=vertical ? juce::Rectangle<float>(bounds.getCentreX()-3.f,bounds.getY()+5.f,
                                                     6.f,bounds.getHeight()-10.f)
                            : juce::Rectangle<float>(bounds.getX()+5.f,bounds.getCentreY()-3.f,
                                                     bounds.getWidth()-10.f,6.f);
        g.setColour(Palette::colour(Palette::chassis));
        g.fillRoundedRectangle(track.expanded(2.f),4.f);
        g.setColour(Palette::colour(Palette::rim));
        g.drawRoundedRectangle(track.expanded(1.f),3.f,.8f);
        const auto red=Palette::colour(Palette::redBright);
        if(vertical) {
            const float pos=std::clamp(sliderPos,track.getY(),track.getBottom());
            auto fill=juce::Rectangle<float>(track.getX()+1.f,pos,track.getWidth()-2.f,track.getBottom()-pos);
            g.setColour(red.withAlpha(.8f));g.fillRect(fill);
            for(int i=0;i<9;++i){const float yy=track.getY()+track.getHeight()*i/8.f;
                g.setColour(Palette::colour(Palette::rim));
                g.drawLine(track.getX()-10.f,yy,track.getX()-5.f,yy,1.f);}
            auto thumb=juce::Rectangle<float>(track.getCentreX()-11.f,pos-7.f,22.f,14.f);
            paintThumb(g,thumb);
        } else {
            const float pos=std::clamp(sliderPos,track.getX(),track.getRight());
            g.setColour(red.withAlpha(.8f));
            g.fillRect(juce::Rectangle<float>(track.getX(),track.getY()+1.f,pos-track.getX(),track.getHeight()-2.f));
            auto thumb=juce::Rectangle<float>(pos-7.f,track.getCentreY()-10.f,14.f,20.f);
            paintThumb(g,thumb);
        }
        if(slider.hasKeyboardFocus(false)){
            g.setColour(red.withAlpha(.8f));g.drawRoundedRectangle(bounds.reduced(1.f),4.f,1.f);
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
