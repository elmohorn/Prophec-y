#pragma once
#include "HardwareControls.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <functional>
#include <memory>
#include <vector>

namespace p6plugin::hardware {

// Passive displays must be fed by the editor/message thread; never call
// their setters from processBlock. No waveform is synthesized in paint().
class HardwareMeter final : public juce::Component {
public:
    explicit HardwareMeter(MeterKind kind=MeterKind::stereoLed):kind_(kind){}
    void setMeterKind(MeterKind kind) {kind_=kind;repaint();}
    void setLevels(float left,float right) {
        left_=std::clamp(std::isfinite(left)?left:0.f,0.f,1.f);
        right_=std::clamp(std::isfinite(right)?right:0.f,0.f,1.f);
        repaint();
    }
    void paint(juce::Graphics& g) override {
        auto r=getLocalBounds().toFloat().reduced(3.f);
        paintPlate(g,r);
        if(kind_==MeterKind::analogVu) {
            // Small physical meter: recessed face, graduated scale, distinct
            // warning arc, needle and pivot. This is a live meter, not a bitmap.
            const auto housing = r.reduced(4.f, 2.f);
            g.setColour(juce::Colours::black.withAlpha(.70f));
            g.fillRoundedRectangle(housing, 5.f);
            g.setGradientFill(juce::ColourGradient(
                Palette::colour(0xff626065), housing.getX(), housing.getY(),
                Palette::colour(0xff141517), housing.getRight(), housing.getBottom(), false));
            g.drawRoundedRectangle(housing.reduced(.7f), 4.5f, 1.2f);
            const auto face = housing.reduced(5.f, 3.f);
            g.setGradientFill(juce::ColourGradient(
                Palette::colour(0xff25272b), face.getX(), face.getY(),
                Palette::colour(0xff0b0d10), face.getX(), face.getBottom(), false));
            g.fillRoundedRectangle(face, 3.f);
            const auto centre=juce::Point<float>(face.getCentreX(),face.getBottom()-2.5f);
            const float radius=std::min(face.getWidth()*.44f,face.getHeight()*.89f);
            for(int i=0;i<=10;++i) {
                const float a=-1.15f+2.3f*static_cast<float>(i)/10.f;
                const float dx=std::sin(a), dy=-std::cos(a);
                const float inner=radius*(i%2==0 ? .76f : .82f);
                const float outer=radius*.95f;
                g.setColour(i>=9 ? Palette::colour(Palette::redBright).withAlpha(.85f)
                                 : Palette::colour(Palette::silver).withAlpha(.64f));
                g.drawLine(centre.x+dx*inner,centre.y+dy*inner,
                           centre.x+dx*outer,centre.y+dy*outer,i%2==0?1.15f:.7f);
            }
            g.setColour(Palette::colour(Palette::silver).withAlpha(.76f));
            g.setFont(juce::Font(9.5f));
            g.drawText("VU",juce::Rectangle<int>(
                static_cast<int>(face.getCentreX()-12.f),
                static_cast<int>(face.getY()+3.f),24,12),juce::Justification::centred);
            const float angle=-1.15f+2.3f*((left_+right_)*.5f);
            g.setColour(juce::Colours::black.withAlpha(.78f));
            g.drawLine(centre.x+1.f,centre.y+1.f,
                       centre.x+std::sin(angle)*radius*.83f+1.f,
                       centre.y-std::cos(angle)*radius*.83f+1.f,2.5f);
            g.setColour(Palette::colour(Palette::redBright));
            g.drawLine(centre.x,centre.y,
                       centre.x+std::sin(angle)*radius*.83f,
                       centre.y-std::cos(angle)*radius*.83f,1.5f);
            g.setColour(Palette::colour(Palette::silver));
            g.fillEllipse(centre.x-3.4f,centre.y-3.4f,6.8f,6.8f);
            g.setColour(Palette::colour(Palette::chassis));
            g.fillEllipse(centre.x-1.3f,centre.y-1.3f,2.6f,2.6f);
            return;
        }
        const int segments=kind_==MeterKind::ledBar?18:14;
        const int channels=kind_==MeterKind::ledBar?1:2;
        for(int ch=0;ch<channels;++ch) {
            const float level=ch==0?left_:right_;
            const float gap=2.5f;
            auto line=juce::Rectangle<float>(r.getX()+9.f,r.getY()+9.f+ch*(r.getHeight()*.45f),
                r.getWidth()-18.f, std::max(3.f,r.getHeight()*(channels==2?.22f:.55f)));
            const float w=(line.getWidth()-(segments-1)*gap)/segments;
            for(int i=0;i<segments;++i) {
                const float t=static_cast<float>(i+1)/segments;
                const auto c = t>.85f ? Palette::colour(Palette::redBright) :
                               t>.65f ? Palette::colour(Palette::amber) :
                                        Palette::colour(Palette::green);
                g.setColour(t<=level?c:Palette::colour(0xff25282a));
                g.fillRoundedRectangle(line.getX()+i*(w+gap),line.getY(),w,line.getHeight(),1.5f);
            }
        }
    }
private:
    MeterKind kind_;float left_{},right_{};
};

class HardwareSignalDisplay : public juce::Component {
public:
    explicit HardwareSignalDisplay(SignalKind kind=SignalKind::oscilloscope):kind_(kind){}
    void setKind(SignalKind k){kind_=k;repaint();}
    SignalKind kind() const noexcept {return kind_;}
    // Pre-decimated, normalized samples or spectrum magnitudes. GUI-thread only.
    void setSamples(std::vector<float> values) {
        if(values.size()>2048)values.resize(2048);
        for(auto& v:values)v=std::clamp(std::isfinite(v)?v:0.f,-1.f,1.f);
        samples_=std::move(values);repaint();
    }
    void paint(juce::Graphics& g) override {
        auto r=getLocalBounds().toFloat().reduced(2.f);
        paintPlate(g,r,5.f);
        auto area=r.reduced(9.f);
        g.setColour(Palette::colour(Palette::cyan).withAlpha(.1f));
        for(int i=1;i<4;++i)g.drawHorizontalLine(
            static_cast<int>(area.getY()+area.getHeight()*i/4.f),area.getX(),area.getRight());
        g.setColour(Palette::colour(Palette::rim).withAlpha(.45f));
        g.drawLine(area.getX(),area.getCentreY(),area.getRight(),area.getCentreY(),.8f);
        if(samples_.empty()){
            g.setColour(Palette::colour(Palette::muted));
            g.setFont(juce::Font(10.f));
            g.drawText("NO SIGNAL",area.toNearestInt(),juce::Justification::centred);
            return;
        }
        const auto accent=kind_==SignalKind::morph?Palette::colour(Palette::redBright):
                          Palette::colour(Palette::cyan);
        const int n=static_cast<int>(samples_.size());
        if(kind_==SignalKind::spectrum){
            const float barWidth=area.getWidth()/n;
            for(int i=0;i<n;++i){
                const float amp=std::clamp(std::abs(samples_[static_cast<std::size_t>(i)]),0.f,1.f);
                g.setColour(accent.withAlpha(.45f+.5f*amp));
                const float h=amp*area.getHeight();
                g.fillRect(juce::Rectangle<float>(area.getX()+i*barWidth,area.getBottom()-h,
                    std::max(1.f,barWidth-1.f),h));
            }
        } else {
            juce::Path waveform;
            for(int i=0;i<n;++i) {
                const float px=area.getX()+area.getWidth()*i/std::max(1,n-1);
                const float py=area.getCentreY()-samples_[static_cast<std::size_t>(i)]*
                                area.getHeight()*.46f;
                if(i==0)waveform.startNewSubPath(px,py);
                else waveform.lineTo(px,py);
            }
            g.setColour(accent.withAlpha(.23f));g.strokePath(waveform,juce::PathStrokeType(4.f));
            g.setColour(accent);g.strokePath(waveform,juce::PathStrokeType(1.4f));
        }
    }
private:
    SignalKind kind_;std::vector<float> samples_;
};

// An illustrative envelope shape based on the four normalized control values.
// Display-only; it does not run the DSP envelope integrator.
class HardwareEnvelopeDisplay final : public juce::Component {
public:
    void setAdsr(float attack,float decay,float sustain,float release) {
        a_=std::clamp(attack,0.f,1.f);d_=std::clamp(decay,0.f,1.f);
        s_=std::clamp(sustain,0.f,1.f);r_=std::clamp(release,0.f,1.f);repaint();
    }
    void paint(juce::Graphics& g) override {
        auto bounds=getLocalBounds().toFloat().reduced(2.f);paintPlate(g,bounds);
        auto plot=bounds.reduced(12.f,14.f);
        const float aWidth=plot.getWidth()*(.05f+.20f*a_);
        const float dWidth=plot.getWidth()*(.05f+.20f*d_);
        const float rWidth=plot.getWidth()*(.05f+.20f*r_);
        const float sx=plot.getX()+aWidth+dWidth;
        const float rx=plot.getRight()-rWidth;
        const std::array<juce::Point<float>,5> pts{{
            {plot.getX(),plot.getBottom()},
            {plot.getX()+aWidth,plot.getY()},
            {sx,plot.getBottom()-plot.getHeight()*s_},
            {rx,plot.getBottom()-plot.getHeight()*s_},
            {plot.getRight(),plot.getBottom()}
        }};
        juce::Path path;path.startNewSubPath(pts.front());
        for(std::size_t i=1;i<pts.size();++i)path.lineTo(pts[i]);
        g.setColour(Palette::colour(Palette::red).withAlpha(.25f));
        auto filled=path;filled.lineTo(plot.getRight(),plot.getBottom());
        filled.lineTo(plot.getX(),plot.getBottom());filled.closeSubPath();g.fillPath(filled);
        g.setColour(Palette::colour(Palette::redBright));
        g.strokePath(path,juce::PathStrokeType(2.f));
        for(const auto& p:pts)g.fillEllipse(p.x-2.5f,p.y-2.5f,5.f,5.f);
    }
private:
    float a_{.17f},d_{.35f},s_{.65f},r_{.25f};
};

class HardwareXYPad final : public juce::Component {
public:
    std::function<void(float,float)> onChanged;
    HardwareXYPad() {setWantsKeyboardFocus(true);setMouseClickGrabsKeyboardFocus(true);}
    void setNormalized(float x,float y, bool notify=false) {
        x_=std::clamp(std::isfinite(x)?x:.5f,0.f,1.f);
        y_=std::clamp(std::isfinite(y)?y:.5f,0.f,1.f);
        repaint();if(notify && onChanged)onChanged(x_,y_);
    }
    float x() const noexcept {return x_;}
    float y() const noexcept {return y_;}
    void mouseDown(const juce::MouseEvent& e) override {update(e.position);}
    void mouseDrag(const juce::MouseEvent& e) override {update(e.position);}
    bool keyPressed(const juce::KeyPress& key) override {
        float dx=0.f,dy=0.f;
        if(key.getKeyCode()==juce::KeyPress::leftKey)dx=-.02f;
        else if(key.getKeyCode()==juce::KeyPress::rightKey)dx=.02f;
        else if(key.getKeyCode()==juce::KeyPress::upKey)dy=-.02f;
        else if(key.getKeyCode()==juce::KeyPress::downKey)dy=.02f;
        else return false;
        setNormalized(x_+dx,y_+dy,true);return true;
    }
    void paint(juce::Graphics& g) override {
        auto r=getLocalBounds().toFloat().reduced(3.f);paintPlate(g,r);
        auto area=r.reduced(10.f);
        g.setColour(Palette::colour(Palette::rim).withAlpha(.55f));
        g.drawLine(area.getCentreX(),area.getY(),area.getCentreX(),area.getBottom(),.8f);
        g.drawLine(area.getX(),area.getCentreY(),area.getRight(),area.getCentreY(),.8f);
        const auto p=juce::Point<float>(area.getX()+x_*area.getWidth(),
                                        area.getBottom()-y_*area.getHeight());
        paintGlow(g,p,19.f,Palette::colour(Palette::redBright),.35f);
        g.setColour(Palette::colour(Palette::silver));g.drawEllipse(p.x-5.f,p.y-5.f,10.f,10.f,1.5f);
        if(hasKeyboardFocus(false)){
            g.setColour(Palette::colour(Palette::redBright));g.drawRect(r,1.f);
        }
    }
private:
    void update(juce::Point<float> p) {
        auto r=getLocalBounds().toFloat().reduced(13.f);
        if(r.getWidth()<=0.f||r.getHeight()<=0.f)return;
        setNormalized((p.x-r.getX())/r.getWidth(),1.f-(p.y-r.getY())/r.getHeight(),true);
    }
    float x_{.5f},y_{.5f};
};

class HardwarePresetStrip final : public juce::Component, public juce::SettableTooltipClient {
public:
    std::function<void()> onPrevious,onNext;
    HardwarePresetStrip() {setWantsKeyboardFocus(true);}
    void setPreset(int bank,juce::String name) {
        bank_=std::max(0,bank);name_=std::move(name);
        setTooltip(name_);repaint();
    }
    void mouseDown(const juce::MouseEvent& e) override {
        if(e.x<30 && onPrevious)onPrevious();
        else if(e.x>=getWidth()-30 && onNext)onNext();
    }
    bool keyPressed(const juce::KeyPress& key) override {
        if(key==juce::KeyPress::leftKey && onPrevious){onPrevious();return true;}
        if(key==juce::KeyPress::rightKey && onNext){onNext();return true;}
        return false;
    }
    void paint(juce::Graphics& g) override {
        auto r=getLocalBounds().toFloat().reduced(2.f);paintPlate(g,r,5.f);
        auto inner=r.reduced(6.f);
        g.setColour(Palette::colour(Palette::cyan));
        g.setFont(juce::Font(15.f));
        g.drawText("<",juce::Rectangle<int>(static_cast<int>(inner.getX()),0,24,getHeight()),
                   juce::Justification::centred);
        g.drawText(">",juce::Rectangle<int>(getWidth()-30,0,24,getHeight()),
                   juce::Justification::centred);
        g.setFont(juce::Font(13.f));
        g.drawFittedText(juce::String(bank_).paddedLeft('0',3)+"  "+name_,
            juce::Rectangle<int>(32,0,std::max(0,getWidth()-64),getHeight()),
            juce::Justification::centred,1);
    }
private:
    int bank_{};juce::String name_{"INIT PATCH"};
};

class HardwareWaveSelector final : public juce::Component {
public:
    std::function<void(int)> onSelected;
    HardwareWaveSelector() {
        const std::array<juce::String,6> labels{{"SINE","TRI","SQR","SAW","RND","STEP"}};
        for(int i=0;i<6;++i) {
            auto& button=buttons_[static_cast<std::size_t>(i)];
            button=std::make_unique<HardwareButton>(labels[static_cast<std::size_t>(i)],ButtonKind::utility);
            button->setClickingTogglesState(false);
            button->onClick=[this,i]{setSelected(i,true);};
            addAndMakeVisible(*button);
        }
        setSelected(0,false);
    }
    void setSelected(int i,bool notify=false) {
        selected_=std::clamp(i,0,5);
        for(int j=0;j<6;++j)buttons_[static_cast<std::size_t>(j)]->setToggleState(
            j==selected_,juce::dontSendNotification);
        if(notify && onSelected)onSelected(selected_);
    }
    int selected() const noexcept{return selected_;}
    void resized() override {
        auto r=getLocalBounds();
        for(int i=0;i<6;++i) {
            const int w=(i==5?r.getWidth():getWidth()/6);
            buttons_[static_cast<std::size_t>(i)]->setBounds(r.removeFromLeft(w).reduced(1));
        }
    }
private:
    std::array<std::unique_ptr<HardwareButton>,6> buttons_;
    int selected_{};
};

class HardwareTabs final : public juce::Component {
public:
    std::function<void(int)> onSelected;
    void setTabs(const std::vector<juce::String>& labels) {
        buttons_.clear();selected_=0;
        for(std::size_t i=0;i<labels.size();++i) {
            auto b=std::make_unique<HardwareButton>(labels[i],ButtonKind::rectangular);
            b->setClickingTogglesState(false);
            b->onClick=[this,i]{setSelected(static_cast<int>(i),true);};
            addAndMakeVisible(*b);buttons_.push_back(std::move(b));
        }
        setSelected(0,false);resized();
    }
    void setSelected(int index,bool notify=false) {
        if(buttons_.empty())return;
        selected_=std::clamp(index,0,static_cast<int>(buttons_.size())-1);
        for(std::size_t i=0;i<buttons_.size();++i)
            buttons_[i]->setToggleState(static_cast<int>(i)==selected_,juce::dontSendNotification);
        if(notify && onSelected)onSelected(selected_);
    }
    int selected() const noexcept{return selected_;}
    void resized() override {
        if(buttons_.empty())return;
        auto r=getLocalBounds();const int w=getWidth()/static_cast<int>(buttons_.size());
        for(std::size_t i=0;i<buttons_.size();++i) {
            buttons_[i]->setBounds(r.removeFromLeft(i+1==buttons_.size()?r.getWidth():w).reduced(1));
        }
    }
private:
    std::vector<std::unique_ptr<HardwareButton>> buttons_;
    int selected_{};
};

class HardwareProgress final : public juce::Component {
public:
    void setProgress(float v) {progress_=std::clamp(std::isfinite(v)?v:0.f,0.f,1.f);repaint();}
    float progress() const noexcept {return progress_;}
    void paint(juce::Graphics& g) override {
        auto r=getLocalBounds().toFloat().reduced(3.f);paintPlate(g,r,r.getHeight()*.5f);
        r=r.reduced(4.f);
        if(progress_>0.f) {
            auto fill=r.withWidth(r.getWidth()*progress_);
            g.setGradientFill(juce::ColourGradient(Palette::colour(Palette::red),
                fill.getX(),fill.getY(),Palette::colour(Palette::redBright),fill.getRight(),fill.getY(),false));
            g.fillRoundedRectangle(fill,r.getHeight()*.5f);
        }
    }
private:
    float progress_{};
};

class HardwareModulationRow final : public juce::Component {
public:
    std::function<void()> onAdd,onRemove;
    HardwareDropdown source,destination;
    HardwareFader amount{SliderSize::compactHorizontal};
    HardwareButton add{"+",ButtonKind::utility},remove{"-",ButtonKind::utility};
    HardwareModulationRow(){
        addAndMakeVisible(source);addAndMakeVisible(destination);addAndMakeVisible(amount);
        addAndMakeVisible(add);addAndMakeVisible(remove);
        amount.setRange(-1.,1.,.001);amount.setValue(0.,juce::dontSendNotification);
        add.setClickingTogglesState(false);remove.setClickingTogglesState(false);
        add.onClick=[this]{if(onAdd)onAdd();};
        remove.onClick=[this]{if(onRemove)onRemove();};
    }
    void setSources(const std::vector<juce::String>& labels){
        source.clear(juce::dontSendNotification);
        int id=1;for(const auto& v:labels)source.addItem(v,id++);
        if(id>1)source.setSelectedId(1,juce::dontSendNotification);
    }
    void setDestinations(const std::vector<juce::String>& labels){
        destination.clear(juce::dontSendNotification);
        int id=1;for(const auto& v:labels)destination.addItem(v,id++);
        if(id>1)destination.setSelectedId(1,juce::dontSendNotification);
    }
    void resized() override {
        auto r=getLocalBounds().reduced(2);
        const int w=r.getWidth();
        source.setBounds(r.removeFromLeft(static_cast<int>(w*.26f)).reduced(2));
        destination.setBounds(r.removeFromLeft(static_cast<int>(w*.27f)).reduced(2));
        remove.setBounds(r.removeFromRight(30).reduced(2));
        add.setBounds(r.removeFromRight(30).reduced(2));
        amount.setBounds(r.reduced(2));
    }
};

} // namespace p6plugin::hardware
