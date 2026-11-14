// Standalone native JUCE preview of the approved Prophec-Y component kit.
// Build with -DP6_BUILD_PLUGIN=ON -DP6_BUILD_HARDWARE_GALLERY=ON.
// Demo values are illustrative; this application is NOT an audio plugin.
#include "HardwareDisplays.h"
#include <juce_gui_extra/juce_gui_extra.h>
#include <cmath>
#include <memory>
#include <utility>
#include <vector>

namespace p6plugin::hardware {

class HardwareGalleryContent final : public juce::Component {
public:
    HardwareGalleryContent() {
        setSize(1450,1090);
        // 1. The complete knob family and the approved alternate caps.
        const std::array<DialSize,6> sizes{{DialSize::hero,DialSize::large,DialSize::medium,
            DialSize::trim,DialSize::micro,DialSize::nano}};
        const std::array<juce::String,6> sizeNames{{"HERO 64","LARGE 48","MEDIUM 36",
            "TRIM 24","MICRO 18","NANO 14"}};
        for(int i=0;i<6;++i){
            auto& d=make<HardwareDial>(30+i*128,110,100,94,sizes[static_cast<std::size_t>(i)]);
            d.setValue(.68,juce::dontSendNotification);
            label(sizeNames[static_cast<std::size_t>(i)],30+i*128,204,100);
        }
        for(int i=0;i<4;++i){
            auto& d=make<HardwareDial>(815+i*148,114,100,92,DialSize::large,
                        static_cast<Cap>(i));
            d.setValue(.35+.14*i,juce::dontSendNotification);
            label(std::array<juce::String,4>{{"BRUSHED","SOFT BLACK","KNURLED","SCALLOPED"}}[
                        static_cast<std::size_t>(i)],815+i*148,205,100);
        }

        // 2. Long, compact and mini faders, plus three horizontal lengths.
        const std::array<SliderSize,6> faders{{
            SliderSize::longVertical,SliderSize::compactVertical,SliderSize::miniVertical,
            SliderSize::horizontal,SliderSize::compactHorizontal,SliderSize::trimHorizontal
        }};
        const std::array<juce::String,6> faderNames{{
            "VERTICAL 80","VERTICAL 56","VERTICAL 40",
            "HORIZONTAL 96","HORIZONTAL 72","TRIM 48"
        }};
        for(int i=0;i<6;++i) {
            const bool vertical=i<3;
            const int x=35+i*210;
            auto& f=make<HardwareFader>(x,294,
                vertical?52:150, vertical?103:45,faders[static_cast<std::size_t>(i)]);
            f.setValue(i%2==0?.7:.25,juce::dontSendNotification);
            label(faderNames[static_cast<std::size_t>(i)],x,407,170);
        }

        // 3. Physical buttons and hardware switches.
        const std::array<ButtonKind,6> kinds{{
            ButtonKind::rectangular,ButtonKind::square,ButtonKind::round,
            ButtonKind::utility,ButtonKind::capacitive,ButtonKind::step
        }};
        const std::array<juce::String,6> titles{{
            "ON","REC","MUTE","...","PRESETS","5"
        }};
        const std::array<juce::String,6> names{{
            "LOW PROFILE","SQUARE","ROUND","UTILITY","CAPACITIVE","STEP"
        }};
        for(int i=0;i<6;++i) {
            const int x=27+i*171;
            auto& b=make<HardwareButton>(x,496,i==4?142:90,46,
                titles[static_cast<std::size_t>(i)],kinds[static_cast<std::size_t>(i)]);
            b.setToggleState(i%2==0,juce::dontSendNotification);
            label(names[static_cast<std::size_t>(i)],x,546,145);
        }
        auto& toggle=make<HardwareSwitch>(1085,496,104,45,"SYNC");
        toggle.setToggleState(true,juce::dontSendNotification);
        label("SWITCH",1085,546,115);
        auto& triple=make<HardwareThreePosition>(1220,498,122,42);
        label("3-POS",1220,546,105);

        // 4. Step sequence, preset, dropdown, LEDs and meters.
        for(int i=0;i<8;++i) {
            auto& b=make<HardwareStepButton>(32+i*54,639,45,40,i+1);
            b.setToggleState(i==0||i==3||i==7,juce::dontSendNotification);
            if(i==3)b.setPlaying(true);
        }
        label("8 STEP SEQUENCER",32,684,465);
        auto& preset=make<HardwarePresetStrip>(520,637,365,50);
        preset.setPreset(6,"BRIGHT PAD");
        label("OLED / PRESET STRIP",520,691,365);
        auto& dropdown=make<HardwareDropdown>(911,637,190,42);
        dropdown.addItem("Classic Saw",1);dropdown.addItem("Pulse 50",2);
        dropdown.addItem("Triangle",3);dropdown.addItem("Noise",4);
        dropdown.setSelectedId(1,juce::dontSendNotification);
        label("DROPDOWN",911,690,190);
        const std::array<LampKind,5> lampKinds{{LampKind::red,LampKind::amber,
            LampKind::green,LampKind::blue,LampKind::silver}};
        for(int i=0;i<5;++i){
            auto& lamp=make<HardwareLamp>(1153+i*49,645,28,28,lampKinds[static_cast<std::size_t>(i)]);
            lamp.setState(i<4);
        }
        label("STATUS LAMPS",1152,690,270);

        // 5. Monitors, waveform views and editable XY/envelope.
        auto& meter=make<HardwareMeter>(31,790,220,75,MeterKind::stereoLed);
        meter.setLevels(.77f,.63f);label("STEREO LED METER",31,871,220);
        auto& vu=make<HardwareMeter>(265,790,205,75,MeterKind::analogVu);
        vu.setLevels(.62f,.56f);label("ANALOG VU",265,871,205);
        auto& spectrum=make<HardwareSignalDisplay>(486,790,207,75,SignalKind::spectrum);
        auto& osc=make<HardwareSignalDisplay>(709,790,207,75,SignalKind::oscilloscope);
        auto& morph=make<HardwareSignalDisplay>(932,790,207,75,SignalKind::morph);
        std::vector<float> bins(32),wave(120);
        for(std::size_t i=0;i<bins.size();++i)
            bins[i]=.18f+.75f*std::abs(std::sin(static_cast<float>(i)*.72f))*
                     (1.f-static_cast<float>(i)/static_cast<float>(bins.size()));
        for(std::size_t i=0;i<wave.size();++i)
            wave[i]=std::sin(static_cast<float>(i)*.22f)*.69f;
        spectrum.setSamples(std::move(bins));
        osc.setSamples(wave);morph.setSamples(std::move(wave));
        label("SPECTRUM DEMO",486,871,207);
        label("OSCILLOSCOPE DEMO",709,871,207);
        label("MORPH DEMO",932,871,207);
        auto& xy=make<HardwareXYPad>(1160,776,150,109);
        xy.setNormalized(.64f,.45f);
        label("XY PAD",1160,890,150);

        // 6. Composite interactions and display-only envelope.
        auto& envelope=make<HardwareEnvelopeDisplay>(30,961,310,96);
        envelope.setAdsr(.25f,.45f,.65f,.32f);
        label("ADSR DISPLAY",30,1058,310);
        auto& waveSelect=make<HardwareWaveSelector>(365,970,316,41);
        label("LFO WAVE SELECTOR",365,1018,316);
        auto& tabs=make<HardwareTabs>(702,970,362,41);
        tabs.setTabs({"OSC","FILTER","ENV","LFO","MOD","FX"});
        label("MODULE TABS",702,1018,362);
        auto& progress=make<HardwareProgress>(1092,978,283,28);
        progress.setProgress(.71f);
        label("PROGRESS",1092,1018,283);

        // A composable modulation row is available separately and shown on
        // demand in normal editor layouts: HardwareModulationRow.
    }

    void paint(juce::Graphics& g) override {
        g.fillAll(Palette::colour(Palette::chassis));
        g.setColour(Palette::colour(Palette::silver));
        g.setFont(juce::Font(28.f).boldened());
        g.drawText("PROPHEC-Y  /  HARDWARE UI KIT",20,16,getWidth()-40,34,
                   juce::Justification::centredLeft);
        g.setColour(Palette::colour(Palette::redBright));
        g.fillRect(22,61,getWidth()-44,2);
        title(g,"DIALS / CAPS",20,80);
        title(g,"FADERS / SLIDERS",20,260);
        title(g,"BUTTONS / SWITCHES",20,454);
        title(g,"PRESET / STEPS / STATUS",20,600);
        title(g,"METERS / SIGNAL",20,742);
        title(g,"COMPOSITE CONTROLS",20,932);
        g.setColour(Palette::colour(Palette::muted));
        g.setFont(juce::Font(10.5f));
        for(const auto& entry:labels_)
            g.drawFittedText(entry.first,entry.second,juce::Justification::centred,1);
        g.drawText("ALL VALUES IN THIS GALLERY ARE VISUAL DEMO DATA — NOT DSP OUTPUT",
                   25,1068,getWidth()-50,16,juce::Justification::centredRight);
    }

private:
    template<typename T,typename...Args>
    T& make(int x,int y,int w,int h,Args&&...args){
        auto ptr=std::make_unique<T>(std::forward<Args>(args)...);
        auto& ref=*ptr;ptr->setBounds(x,y,w,h);
        addAndMakeVisible(*ptr);
        controls_.push_back(std::move(ptr));return ref;
    }
    void label(const juce::String& s,int x,int y,int w){
        labels_.emplace_back(s,juce::Rectangle<int>(x,y,w,19));
    }
    static void title(juce::Graphics& g,const juce::String& s,int x,int y) {
        g.setColour(Palette::colour(Palette::silver).withAlpha(.86f));
        g.setFont(juce::Font(14.f).boldened());
        g.drawText(s,x,y,450,20,juce::Justification::centredLeft);
        g.setColour(Palette::colour(Palette::rim).withAlpha(.5f));
        g.drawLine(static_cast<float>(x),y+24.f,1430.f,y+24.f,1.f);
    }
    std::vector<std::unique_ptr<juce::Component>> controls_;
    std::vector<std::pair<juce::String,juce::Rectangle<int>>> labels_;
};

class HardwareGalleryHost final : public juce::Component {
public:
    HardwareGalleryHost() {
        addAndMakeVisible(viewport_);
        viewport_.setViewedComponent(new HardwareGalleryContent(),true);
        viewport_.setScrollBarsShown(true,true);
    }
    void resized() override {viewport_.setBounds(getLocalBounds());}
private:
    juce::Viewport viewport_;
};

class HardwareGalleryApp final : public juce::JUCEApplication {
public:
    const juce::String getApplicationName() override {return "Prophec-Y Hardware Gallery";}
    const juce::String getApplicationVersion() override{return "2.0";}
    void initialise(const juce::String&) override {
        window_=std::make_unique<Window>();
    }
    void shutdown() override{window_.reset();}
private:
    class Window final : public juce::DocumentWindow {
    public:
        Window():juce::DocumentWindow("Prophec-Y UI Component Library v2",
                  Palette::colour(Palette::chassis),allButtons){
            setUsingNativeTitleBar(true);
            setContentOwned(new HardwareGalleryHost(),true);
            setResizable(true,true);centreWithSize(1440,850);setVisible(true);
        }
        void closeButtonPressed() override{juce::JUCEApplication::getInstance()->systemRequestedQuit();}
    };
    std::unique_ptr<Window> window_;
};
} // namespace p6plugin::hardware

START_JUCE_APPLICATION(p6plugin::hardware::HardwareGalleryApp)
