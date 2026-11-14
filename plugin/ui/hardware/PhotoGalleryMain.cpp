// Photo UI gallery: every sprite is sourced from the accepted concept boards.
// Unlike the previous procedural gallery, native JUCE draws the actual artwork.
// Dials support mouse/keyboard motion; panels are display samples, not DSP views.
#include "PhotoHardwareControls.h"
#include <juce_gui_extra/juce_gui_extra.h>
#include <memory>
#include <vector>
#include <cmath>

namespace p6plugin::hardware::photo {

class PhotoTile final : public juce::Component {
public:
    PhotoTile(juce::String caption,juce::String group,juce::String id,
              bool isDial,bool isToggle=false)
      : caption_(std::move(caption)),group_(std::move(group)),id_(std::move(id)) {
        if(isDial) {
            auto dial=std::make_unique<Dial>(id_,2.f);
            dial->setValue(.5,juce::dontSendNotification);
            dial->setDoubleClickReturnValue(true,.5);
            dial->setMouseWheelEnabled(true);
            control_=std::move(dial);
        } else if(isToggle) {
            auto button=std::make_unique<Toggle>(id_);
            button->setToggleState(true,juce::dontSendNotification);
            control_=std::move(button);
        } else {
            control_=std::make_unique<StaticModule>(id_);
        }
        addAndMakeVisible(*control_);
    }
    void resized() override{
        auto r=getLocalBounds().reduced(9);
        r.removeFromTop(33);
        r.removeFromBottom(17);
        control_->setBounds(r);
    }
    void paint(juce::Graphics& g) override {
        auto r=getLocalBounds().toFloat().reduced(.5f);
        g.setGradientFill(juce::ColourGradient(juce::Colour(0xff222325),
            r.getX(),r.getY(),juce::Colour(0xff121315),
            r.getRight(),r.getBottom(),false));
        g.fillRoundedRectangle(r,7.f);
        g.setColour(juce::Colour(0xff52545a).withAlpha(.7f));
        g.drawRoundedRectangle(r,7.f,.8f);
        g.setColour(juce::Colour(0xfff1eeec));
        g.setFont(juce::Font(juce::FontOptions(11.f,juce::Font::bold)));
        g.drawFittedText(caption_,juce::Rectangle<int>(9,7,getWidth()-18,17),
                         juce::Justification::centredLeft,1);
        g.setColour(juce::Colour(0xffa8a8a8));
        g.setFont(juce::Font(juce::FontOptions(9.f)));
        g.drawFittedText(group_,juce::Rectangle<int>(9,getHeight()-18,
                          getWidth()-18,14),juce::Justification::centred,1);
        g.setColour(juce::Colour(0xfff05b50).withAlpha(.8f));
        g.fillRoundedRectangle(juce::Rectangle<float>(10,27,28,1.6f),.6f);
    }
private:
    juce::String caption_,group_,id_;
    std::unique_ptr<juce::Component> control_;
};
class GalleryContent final : public juce::Component {
public:
    GalleryContent() {
        setSize(1610,1580);
        for(const auto& d:dials)
            addTile(juce::String(d.id).toUpperCase(), "DIAL / INTERACTIVE",d.id,true,false);
        for(const auto& m:modules) {
            const juce::String id(m.id);
            const bool toggle=id.contains("button") || id.contains("switch") ||
                id.contains("toggle") || id.contains("step") || id.contains("rocker") ||
                id.contains("capacitive");
            addTile(id.toUpperCase(),toggle?"PHOTO BUTTON · CLICKABLE":
                    "APPROVED PHOTO / STATIC",m.id,false,toggle);
        }
        resized();
    }
    void resized() override {
        constexpr int startX=25, cols=8, tileW=190, tileH=166, gutter=7;
        constexpr int startY=115;
        for(std::size_t i=0;i<tiles_.size();++i) {
            const int row=static_cast<int>(i)/cols,col=static_cast<int>(i)%cols;
            tiles_[i]->setBounds(startX+col*(tileW+gutter),
                  startY+row*(tileH+gutter),tileW,tileH);
        }
    }
    void paint(juce::Graphics& g) override {
        g.fillAll(juce::Colour(0xff0b0c0e));
        auto r=getLocalBounds().toFloat();
        g.setGradientFill(juce::ColourGradient(juce::Colour(0xff282628),
             r.getCentreX(),0,juce::Colour(0xff0b0c0e),
             r.getCentreX(),r.getBottom(),false));
        g.fillRect(r);
        g.setColour(juce::Colour(0xffededed));
        g.setFont(juce::Font(juce::FontOptions(32.f,juce::Font::bold)));
        g.drawText("PROPHEC-Y",28,18,440,47,juce::Justification::centredLeft);
        g.setColour(juce::Colour(0xffff4941));
        g.fillRect(29,70,getWidth()-58,2);
        g.setColour(juce::Colour(0xffc7c7c9));
        g.setFont(juce::Font(juce::FontOptions(16.f)));
        g.drawText("HARDWARE COMPONENTS • APPROVED PHOTOGRAPHIC ARTWORK",
                    490,24,1090,32,juce::Justification::centredRight);
        g.setFont(juce::Font(juce::FontOptions(11.f)));
        g.setColour(juce::Colour(0xff9a9ca0));
        g.drawText("DIALS ARE EDITABLE • MODULES DISPLAY SOURCE PHOTOS • NO DSP DATA",
                   28,82,1540,24,juce::Justification::centredLeft);
        g.drawText("REFERENCE ASSETS: 15 DIALS + 39 MODULE PARTS — NATIVE JUCE",28,
                   getHeight()-31,getWidth()-56,20,juce::Justification::centredRight);
    }
private:
    void addTile(const juce::String& label,const juce::String& group,
                 const juce::String& id,bool isDial,bool isToggle){
        auto tile=std::make_unique<PhotoTile>(label,group,id,isDial,isToggle);
        addAndMakeVisible(*tile);
        tiles_.push_back(std::move(tile));
    }
    std::vector<std::unique_ptr<PhotoTile>> tiles_;
};

class GalleryApp final : public juce::JUCEApplication {
public:
    const juce::String getApplicationName() override{return "Prophec-Y Photo Gallery";}
    const juce::String getApplicationVersion() override{return "2.1";}
    void initialise(const juce::String&) override{window_=std::make_unique<Window>();}
    void shutdown() override{window_.reset();}
private:
    class Window final : public juce::DocumentWindow {
    public:
        Window():juce::DocumentWindow("Prophec-Y Photo Hardware Gallery",
                juce::Colour(0xff080b0d),allButtons) {
            setUsingNativeTitleBar(true);
            setContentOwned(new GalleryContent(),true);
            centreWithSize(1610,1580);
            setResizable(false,false);
            setVisible(true);
        }
        void closeButtonPressed() override{
            juce::JUCEApplication::getInstance()->systemRequestedQuit();
        }
    };
    std::unique_ptr<Window> window_;
};
} // namespace p6plugin::hardware::photo
START_JUCE_APPLICATION(p6plugin::hardware::photo::GalleryApp)
