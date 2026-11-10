#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "PluginStateCodec.h"
#include "p6/SysexCodec.h"
#include <juce_audio_processors/juce_audio_processors.h>
#include <cmath>
#include <iostream>
#include <tuple>

#define CHECK(x) do{if(!(x)){std::cerr<<"failed: " #x <<" line "<<__LINE__<<'\n';return 1;}}while(false)
static void set(P6AudioProcessor& p,const char* id,float v)
{
    auto* parameter=p.parameters().getParameter(id);
    parameter->beginChangeGesture();parameter->setValueNotifyingHost(parameter->convertTo0to1(v));parameter->endChangeGesture();
}
struct GestureProbe : juce::AudioProcessorParameter::Listener {
    int begins{},ends{};
    void parameterValueChanged(int,float) override {}
    void parameterGestureChanged(int,bool starting) override {if(starting)++begins;else ++ends;}
};
static bool screenshot(juce::Component& c,const juce::File& file)
{
    juce::Image image(juce::Image::ARGB,c.getWidth(),c.getHeight(),true);
    juce::Graphics g(image);c.paintEntireComponent(g,true);
    juce::MemoryOutputStream stream;
    if(!juce::PNGImageFormat().writeImageToStream(image,stream))return false;
    return file.replaceWithData(stream.getData(),stream.getDataSize());
}
static int presetLibraryChecks(const juce::File& captureFolder)
{
    const auto root=juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("p6-browser-test-"+juce::Uuid().toString());
    struct Cleanup {juce::File root;~Cleanup(){root.deleteRecursively();}} cleanup{root};
    const auto factory=P6AudioProcessor::factoryInitPreset();
    p6plugin::PresetLibrary library(root,factory);CHECK(library.entries().size()==771);CHECK(!root.exists());
    int sequential=0,omom=0;for(const auto& p:library.entries()){if(p.id.startsWith("sequential-factory-"))++sequential;if(p.id.startsWith("sequential-omom-"))++omom;}CHECK(sequential==500);CHECK(omom==270);
    CHECK(library.find("sequential-factory-0-0")!=nullptr);CHECK(library.find("sequential-omom-0-0")!=nullptr);
    CHECK(library.find("sequential-factory-0-0")->bankKey!=library.find("sequential-omom-0-0")->bankKey);
    CHECK(library.find("sequential-factory-0-24")->category=="Auto / Bass");
    CHECK(library.find("sequential-omom-0-14")->category=="Auto / Organ");
    // A stopped DAW must still allow auditioning an arpeggiated factory preset.
    {
        struct StoppedHost final : juce::AudioPlayHead {
            juce::Optional<PositionInfo> getPosition() const override {
                PositionInfo p;p.setBpm(120.0);p.setPpqPosition(0.0);p.setIsPlaying(false);return p;
            }
        } host;
        const auto* preset=library.find("sequential-omom-0-31");CHECK(preset!=nullptr);
        P6AudioProcessor synth;synth.setPlayHead(&host);synth.prepareToPlay(48000,128);
        synth.loadMusicalPreset(preset->state,preset->id,preset->name);
        juce::AudioBuffer<float> audio(2,128);juce::MidiBuffer midi;
        midi.addEvent(juce::MidiMessage::noteOn(1,60,juce::uint8{110}),0);double energy=0;
        for(int block=0;block<900;++block){synth.processBlock(audio,midi);midi.clear();for(int i=0;i<128;++i){const auto v=audio.getSample(0,i);CHECK(std::isfinite(v));energy+=v*v;}}
        CHECK(energy>1.e-5);synth.releaseResources();
    }
    // Exercise the same processor load path used by the browser and compare actual audio.
    const auto renderOfficial=[&](const char* id) {
        P6AudioProcessor synth;synth.prepareToPlay(48000,128);
        const auto* preset=library.find(id);
        if(!preset)return std::vector<float>{};
        synth.loadMusicalPreset(preset->state,preset->id,preset->name);
        juce::AudioBuffer<float> buffer(2,128);juce::MidiBuffer midi;
        midi.addEvent(juce::MidiMessage::noteOn(1,60,juce::uint8{100}),0);
        std::vector<float> samples;
        for(int block=0;block<120;++block){synth.processBlock(buffer,midi);midi.clear();for(int i=0;i<128;++i)samples.push_back(buffer.getSample(0,i));}
        synth.releaseResources();return samples;
    };
    const auto brass=renderOfficial("sequential-factory-0-0"),strings=renderOfficial("sequential-factory-0-1");
    CHECK(!brass.empty()&&brass.size()==strings.size());
    double brassEnergy=0,stringsEnergy=0,difference=0;
    for(std::size_t i=0;i<brass.size();++i){CHECK(std::isfinite(brass[i])&&std::isfinite(strings[i]));brassEnergy+=brass[i]*brass[i];stringsEnergy+=strings[i]*strings[i];const auto d=brass[i]-strings[i];difference+=d*d;}
    CHECK(brassEnergy>1.e-7&&stringsEnergy>1.e-7&&difference>1.e-7);
    std::cout<<"Official presets render distinct audio: energies "<<brassEnergy<<", "<<stringsEnergy<<", difference "<<difference<<"\n";
    CHECK(library.setFavorite("sequential-factory-0-0",true).wasOk());
    // OMOM Church Organ: three held notes formerly generated a large DC
    // offset in audio-rate filter Poly Mod, amplified by the serial reverb.
    {
        const auto* organ=library.find("sequential-omom-0-14");CHECK(organ!=nullptr);
        P6AudioProcessor synth;synth.prepareToPlay(48000,128);set(synth,p6plugin::kMasterOutputID,0.f);
        synth.loadMusicalPreset(organ->state,organ->id,organ->name);
        juce::AudioBuffer<float> audio(2,128);double sum[2]{},energy[2]{},peak=0,afterReleaseEnergy=0;int count=0;
        for(int block=0;block<1800;++block){
            juce::MidiBuffer midi;const int start=block*128;
            for(const auto [time,note,on]:std::array<std::tuple<int,int,bool>,4>{{{4800,60,true},{52800,64,true},{100800,67,true},{168000,67,false}}})
                if(time>=start&&time<start+128)midi.addEvent(on?juce::MidiMessage::noteOn(1,note,juce::uint8{110}):juce::MidiMessage::noteOff(1,note),time-start);
            synth.processBlock(audio,midi);
            for(int i=0;i<128;++i)if(start+i>=144000&&start+i<163200){for(int ch=0;ch<2;++ch){const double v=audio.getSample(ch,i);CHECK(std::isfinite(v));sum[ch]+=v;energy[ch]+=v*v;peak=std::max(peak,std::abs(v));}++count;}
            for(int i=0;i<128;++i)if(start+i>=201600&&start+i<220800){const double v=audio.getSample(0,i);CHECK(std::isfinite(v));afterReleaseEnergy+=v*v;}
        }
        CHECK(count>0&&peak<1.0&&afterReleaseEnergy>0.01);
        for(int ch=0;ch<2;++ch){const double mean=sum[ch]/count;CHECK(std::abs(mean)<0.10);CHECK(energy[ch]/count-mean*mean>1.e-5);}
        synth.releaseResources();
    }
    auto patch=factory.patch;patch.rawPayload[1023]=217;
    auto first=p6::SysexCodec::encodeProgramDump(patch,2,10),second=p6::SysexCodec::encodeProgramDump(patch,2,9);
    std::vector<std::uint8_t> bank=first;bank.insert(bank.end(),second.begin(),second.end());
    int imported=0;CHECK(library.importSysex(bank,"Local test bank",imported).wasOk());CHECK(imported==2);CHECK(library.entries().size()==773);
    juce::String importedId,bankKey;for(const auto& p:library.entries())if(p.source==p6plugin::PresetSource::Imported&&p.program==10){importedId=p.id;bankKey=p.bankKey;}
    CHECK(!importedId.isEmpty());CHECK(library.exportBank(bankKey)==bank);
    CHECK(library.setFavorite(importedId,true).wasOk());CHECK(library.setFavorite("factory-init",true).wasOk());
    p6plugin::PresetLibrary reopened(root,factory);CHECK(reopened.find(importedId)->favorite);CHECK(reopened.find("factory-init")->favorite);CHECK(reopened.find("sequential-factory-0-0")->favorite);
    CHECK(reopened.exportBank(bankKey)==bank);CHECK(reopened.find(importedId)->state.patch.rawPayload[1023]==217);
    auto broken=bank;broken.push_back(0x55);CHECK(reopened.importSysex(broken,"bad",imported).failed());CHECK(imported==0);CHECK(reopened.entries().size()==773);
    P6AudioProcessor target;set(target,p6plugin::kMasterOutputID,-24);set(target,"wrapper_clock_source",2);set(target,"wrapper_calibration",1);set(target,p6plugin::kMidiInputChannelID,7);
    auto user=target.presetSnapshot();user.character.type=1;user.character.tube[0]=.73f;
    juce::String userId;CHECK(reopened.saveUser({},user,"User test patch","Tests",0,0,userId).wasOk());
    CHECK(reopened.saveUser({},user,"Duplicate slot","",0,0,userId).failed());
    const auto stored=*reopened.find(userId);target.loadMusicalPreset(stored.state,stored.id,stored.name);
    CHECK(!target.presetDirty());CHECK(target.parameters().getRawParameterValue(p6plugin::kMasterOutputID)->load()==-24.f);
    CHECK(target.parameters().getRawParameterValue(p6plugin::kMidiInputChannelID)->load()==7.f);
    CHECK(target.parameters().getRawParameterValue("wrapper_clock_source")->load()==2.f);CHECK(target.parameters().getRawParameterValue("wrapper_calibration")->load()==1.f);
    CHECK(std::abs(target.parameters().getRawParameterValue(p6plugin::kTubeIDs[0])->load()-.73f)<1.e-5f);
    set(target,p6plugin::kMidiInputChannelID,3);CHECK(!target.presetDirty());
    set(target,p6plugin::kMasterOutputID,-30);CHECK(!target.presetDirty());set(target,p6plugin::kTubeIDs[0],.5f);CHECK(target.presetDirty());
    target.loadMusicalPreset(stored.state,stored.id,stored.name);CHECK(!target.presetDirty());
    auto fresh=target.presetSnapshot();fresh.character.tube[1]=.6f;CHECK(reopened.saveUser(userId,fresh,stored.name,stored.category,0,0,userId).wasOk());
    auto stale=stored.state;stale.character.tube[1]=.2f;
    // The older library instance must not overwrite a changed saved patch.
    p6plugin::PresetLibrary oldView(root,factory);auto newer=fresh;newer.character.tube[2]=.3f;
    CHECK(reopened.saveUser(userId,newer,stored.name,stored.category,0,0,userId).wasOk());
    CHECK(oldView.saveUser(userId,stale,stored.name,stored.category,0,0,userId).failed());
    CHECK(reopened.exportBank(bankKey)==bank);
    const auto blockedRoot=root.getChildFile("blocked-path");CHECK(blockedRoot.replaceWithText("keep this file"));
    p6plugin::PresetLibrary blocked(blockedRoot,factory);juce::String unused;
    set(target,"nrpn_0045",90);CHECK(target.presetDirty());
    CHECK(blocked.saveUser({},target.presetSnapshot(),"Cannot save","",0,0,unused).failed());
    CHECK(target.presetDirty());CHECK(blockedRoot.loadFileAsString()=="keep this file");
    target.loadMusicalPreset(stored.state,stored.id,stored.name);
    if(captureFolder!=juce::File{}) {
        captureFolder.createDirectory();
        P6PluginEditor gallery(target,root);gallery.setSize(1460,P6PluginEditor::expandedHeight);
        PresetBrowser* browser=nullptr;
        for(auto* child:gallery.getChildren())if(auto* b=dynamic_cast<PresetBrowser*>(child))browser=b;
        CHECK(browser!=nullptr);
        juce::TextButton* browserButton=nullptr;
        for(auto* child:gallery.getChildren())if(auto* button=dynamic_cast<juce::TextButton*>(child);button&&button->getTitle()=="Preset browser")browserButton=button;
        CHECK(browserButton!=nullptr);browserButton->onClick();
        CHECK(browser->isVisible()&&browserButton->getToggleState());CHECK(screenshot(gallery,captureFolder.getChildFile("module_browser.png")));
        CHECK(browser->keyPressed(juce::KeyPress(juce::KeyPress::escapeKey)));CHECK(!browser->isVisible()&&!browserButton->getToggleState());
        browserButton->onClick();CHECK(browser->isVisible()&&browserButton->getToggleState());
        juce::TextEditor* search=nullptr;for(auto* child:browser->getChildren())if(auto* edit=dynamic_cast<juce::TextEditor*>(child);edit&&edit->getTitle()=="Search presets")search=edit;
        CHECK(search!=nullptr);search->setText("not-a-preset",false);search->onTextChange();
        juce::ListBox* list=nullptr;for(auto* child:browser->getChildren())if(auto* l=dynamic_cast<juce::ListBox*>(child))list=l;
        CHECK(list!=nullptr&&list->getModel()->getNumRows()==0);
        juce::ComboBox* sorting=nullptr;for(auto* child:browser->getChildren())if(auto* box=dynamic_cast<juce::ComboBox*>(child);box&&box->getTitle()=="Sort presets")sorting=box;
        CHECK(sorting!=nullptr);sorting->setSelectedId(2,juce::sendNotificationSync);sorting->setSelectedId(3,juce::sendNotificationSync);sorting->setSelectedId(1,juce::sendNotificationSync);
        search->setText("User test",false);search->onTextChange();CHECK(list->getModel()->getNumRows()==1);
        search->setText({},false);search->onTextChange();CHECK(list->getModel()->getNumRows()==774);
        for(int width:{1240,1460,1780}){
            gallery.setSize(width,P6PluginEditor::expandedHeight);
            CHECK(browser->getY()==88);CHECK(gallery.getLocalBounds().contains(browser->getBounds()));
            for(auto* child:browser->getChildren())if(child->isVisible())CHECK(browser->getLocalBounds().contains(child->getBounds()));
            CHECK((list->getHeight()-2)%list->getRowHeight()==0);
            CHECK(screenshot(gallery,captureFolder.getChildFile("module_browser_"+juce::String(width)+".png")));
        }
        browserButton->onClick();CHECK(!browser->isVisible()&&!browserButton->getToggleState());
    }
    std::cout<<"Preset library: persistence, exact banks, favorites, occupied slots, stale writes, globals, dirty tracking and browser filtering verified\n";
    return 0;
}
int main(int argc,char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    {
        ContextDisplay live;live.setSize(322,145);
        p6plugin::ui::LiveAudioFrame tone{},silence{};
        for(std::size_t i=0;i<tone.left.size();++i) {
            tone.left[i]=std::sin(2.f*std::numbers::pi_v<float>*32.f*static_cast<float>(i)/static_cast<float>(tone.left.size()));
            tone.right[i]=-tone.left[i];
        }
        live.setLiveFrame(silence);
        const auto quiet=live.createComponentSnapshot(live.getLocalBounds());
        live.setLiveFrame(tone);
        const auto active=live.createComponentSnapshot(live.getLocalBounds());
        bool different=false;
        for(int y=58;y<125;++y)for(int x=17;x<305;++x)different|=quiet.getPixelAt(x,y)!=active.getPixelAt(x,y);
        CHECK(different);
        CHECK(live.keyPressed(juce::KeyPress(juce::KeyPress::spaceKey)));
        const auto waveform=live.createComponentSnapshot(live.getLocalBounds());
        different=false;
        for(int y=58;y<125;++y)for(int x=17;x<305;++x)different|=waveform.getPixelAt(x,y)!=active.getPixelAt(x,y);
        CHECK(different);
        if(argc>1) {
            const juce::File folder{juce::String(argv[1])};folder.createDirectory();
            CHECK(screenshot(live,folder.getChildFile("display_live_waveform.png")));
            CHECK(live.keyPressed(juce::KeyPress(juce::KeyPress::spaceKey)));
            CHECK(live.keyPressed(juce::KeyPress(juce::KeyPress::spaceKey)));
            CHECK(screenshot(live,folder.getChildFile("display_live_spectrum.png")));
        }
    }
    // Motion pauses the current heater frame; bypass extinguishes it.
    TubeChamber motionTestChamber;motionTestChamber.setSize(320,184);motionTestChamber.setIllumination(true,true,.8f,.5f);
    const auto lit=motionTestChamber.createComponentSnapshot(motionTestChamber.getLocalBounds());
    motionTestChamber.setIllumination(true,false,.05f,.05f);
    const auto frozen=motionTestChamber.createComponentSnapshot(motionTestChamber.getLocalBounds());
    bool equal=true;for(int y=0;y<184;++y)for(int x=0;x<320;++x)equal=equal&&(lit.getPixelAt(x,y)==frozen.getPixelAt(x,y));
    CHECK(equal);motionTestChamber.setIllumination(false,false,0,0);
    const auto off=motionTestChamber.createComponentSnapshot(motionTestChamber.getLocalBounds());
    bool differs=false;for(int y=0;y<184;++y)for(int x=0;x<320;++x)differs=differs||(lit.getPixelAt(x,y)!=off.getPixelAt(x,y));
    CHECK(differs);
    // The Spring coil must react to sustained measured energy, not freeze as
    // a static helix. MOTION freezes the exact frame; bypass removes the
    // illumination and displacement without touching processor state.
    CharacterHardwareWindow springVisual;
    springVisual.setSize(640, 190);
    const std::array<float, 3> springPickups{{.022f, .035f, .05f}};
    springVisual.setState(CharacterHardwareWindow::Mode::Spring, true, true,
                          .1f, .18f, .08f, springPickups);
    const auto springFrameA = springVisual.createComponentSnapshot(springVisual.getLocalBounds());
    springVisual.setState(CharacterHardwareWindow::Mode::Spring, true, true,
                          .1f, .18f, .08f, springPickups);
    const auto springFrameB = springVisual.createComponentSnapshot(springVisual.getLocalBounds());
    bool springMoved = false;
    for (int y = 0; y < springFrameA.getHeight(); ++y)
        for (int x = 0; x < springFrameA.getWidth(); ++x)
            springMoved = springMoved ||
                          springFrameA.getPixelAt(x, y) != springFrameB.getPixelAt(x, y);
    CHECK(springMoved);
    springVisual.setState(CharacterHardwareWindow::Mode::Spring, true, false,
                          .1f, .18f, .08f, springPickups);
    const auto springPaused = springVisual.createComponentSnapshot(springVisual.getLocalBounds());
    bool springFrozen = true;
    for (int y = 0; y < springFrameB.getHeight(); ++y)
        for (int x = 0; x < springFrameB.getWidth(); ++x)
            springFrozen = springFrozen &&
                           springFrameB.getPixelAt(x, y) == springPaused.getPixelAt(x, y);
    CHECK(springFrozen);
    springVisual.setState(CharacterHardwareWindow::Mode::Spring, false, false, 0.f, 0.f, 0.f);
    const auto springBypass = springVisual.createComponentSnapshot(springVisual.getLocalBounds());
    bool springDimmed = false;
    for (int y = 0; y < springPaused.getHeight(); ++y)
        for (int x = 0; x < springPaused.getWidth(); ++x)
            springDimmed = springDimmed ||
                           springPaused.getPixelAt(x, y) != springBypass.getPixelAt(x, y);
    CHECK(springDimmed);
    if (argc > 1) {
        const juce::File folder{juce::String(argv[1])};
        folder.createDirectory();
        CHECK(screenshot(springVisual, folder.getChildFile("spring_tank_bypass.png")));
        springVisual.setState(CharacterHardwareWindow::Mode::Spring, true, true,
                              .1f, .18f, .08f, springPickups);
        CHECK(screenshot(springVisual, folder.getChildFile("spring_tank_active.png")));
    }
    // Rendered photographic layers must animate without touching the fixed
    // physical Tube bezel / glass. Small synthetic RGBA assets make the test
    // independent of artistic assets and keep the native smoke deterministic.
    juce::Image springBack(juce::Image::ARGB,640,190,true);
    {
        juce::Graphics g(springBack);
        g.setColour(juce::Colour{0xff292019});
        g.fillRect(15,15,610,140);
    }
    std::array<juce::Image,3> springLayers;
    for(int i=0;i<3;++i) {
        springLayers[static_cast<std::size_t>(i)]=juce::Image(juce::Image::ARGB,640,190,true);
        juce::Graphics g(springLayers[static_cast<std::size_t>(i)]);
        g.setColour(juce::Colour{0xffcda56a});
        g.fillRect(120,38+i*40,400,12);
    }
    springVisual.setSpringArtwork(springBack,springLayers);
    springVisual.setState(CharacterHardwareWindow::Mode::Spring,true,true,
                          .2f,.16f,.1f,springPickups);
    const auto artLive=springVisual.createComponentSnapshot(springVisual.getLocalBounds());
    springVisual.setState(CharacterHardwareWindow::Mode::Spring,true,true,
                          .2f,.16f,.1f,springPickups);
    const auto artNext=springVisual.createComponentSnapshot(springVisual.getLocalBounds());
    bool artMoved=false;
    for(int y=0;y<190;++y)for(int x=0;x<640;++x)
        artMoved=artMoved||(artLive.getPixelAt(x,y)!=artNext.getPixelAt(x,y));
    CHECK(artMoved);
    springVisual.setState(CharacterHardwareWindow::Mode::Spring,true,false,
                          .2f,.16f,.1f,springPickups);
    const auto artFrozen=springVisual.createComponentSnapshot(springVisual.getLocalBounds());
    bool photoFrozen=true;
    for(int y=0;y<190;++y)for(int x=0;x<640;++x)
        photoFrozen=photoFrozen&&(artNext.getPixelAt(x,y)==artFrozen.getPixelAt(x,y));
    CHECK(photoFrozen);
    springVisual.setState(CharacterHardwareWindow::Mode::Spring,false,false,0.f,0.f,0.f);
    const auto artOff=springVisual.createComponentSnapshot(springVisual.getLocalBounds());
    bool photoBypassChanged=false;
    for(int y=0;y<190;++y)for(int x=0;x<640;++x)
        photoBypassChanged=photoBypassChanged||
                           artFrozen.getPixelAt(x,y)!=artOff.getPixelAt(x,y);
    CHECK(photoBypassChanged);
    // Optical artwork is a preview, not a DSP state change. Distinct lit
    // filament pixels must track LFO light level across UI frames.
    juce::Image opticalBack(juce::Image::ARGB,640,190,true);
    juce::Image opticalOff(juce::Image::ARGB,640,190,true);
    juce::Image opticalOn(juce::Image::ARGB,640,190,true);
    juce::Image opticalLdr(juce::Image::ARGB,640,190,true);
    {
        juce::Graphics g(opticalOn);
        g.setColour(juce::Colours::orange);
        g.fillEllipse(170.f,58.f,65.f,65.f);
        juce::Graphics sensor(opticalLdr);
        sensor.setColour(juce::Colours::orange.withAlpha(.45f));
        sensor.fillEllipse(395.f,66.f,28.f,28.f);
    }
    CharacterHardwareWindow opticalVisual;
    opticalVisual.setSize(640,190);
    opticalVisual.setOpticalArtwork(opticalBack,opticalOff,opticalOn,opticalLdr);
    opticalVisual.setState(CharacterHardwareWindow::Mode::OpticalTremoloPreview,
                           true,true,0.f,0.f,0.f,{},0.f);
    const auto opticalDark=opticalVisual.createComponentSnapshot(opticalVisual.getLocalBounds());
    opticalVisual.setState(CharacterHardwareWindow::Mode::OpticalTremoloPreview,
                           true,true,0.f,0.f,0.f,{},1.f);
    const auto opticalBright=opticalVisual.createComponentSnapshot(opticalVisual.getLocalBounds());
    CHECK(opticalDark.getPixelAt(200,90)!=opticalBright.getPixelAt(200,90));
    if(argc>1) {
        const juce::File folder{juce::String(argv[1])};
        CHECK(screenshot(springVisual,folder.getChildFile("spring_layered_asset_probe.png")));
        CHECK(screenshot(opticalVisual,folder.getChildFile("optical_layered_asset_probe.png")));
    }
    // Exercise the actual JPEG atlas composition path (not the vector
    // fallback): measured spring taps move the metallic foreground under
    // a fixed hardware bezel; pause freezes and bypass stops movement.
    juce::Image syntheticPhoto(juce::Image::RGB,448,360,true);
    {
        juce::Graphics g(syntheticPhoto);
        g.fillAll(juce::Colour{0xff1a100a});
        g.setColour(juce::Colour{0xffd8c3a3});
        for(int row=0;row<2;++row)
            for(int x=90;x<370;x+=8)
                g.fillRect(x,23+row*45,3,25);
        // Dark bulb, then genuinely brighter bulb plus independent sensor.
        g.setColour(juce::Colour{0xff593c21});
        g.fillEllipse(160.f,165.f,95.f,70.f);
        g.setColour(juce::Colour{0xffffc05e});
        g.fillEllipse(160.f,285.f,95.f,70.f);
        g.setColour(juce::Colour{0xffffa943});
        g.fillEllipse(370.f,291.f,42.f,58.f);
    }
    juce::MemoryOutputStream photoBytes;
    CHECK(juce::JPEGImageFormat().writeImageToStream(syntheticPhoto,photoBytes));
    CharacterHardwareWindow photoVisual;
    photoVisual.setSize(640,190);
    photoVisual.setPhotoAtlas(photoBytes.getData(),static_cast<int>(photoBytes.getDataSize()));
    photoVisual.setState(CharacterHardwareWindow::Mode::Spring,true,true,
                          .16f,.18f,.09f,springPickups);
    const auto photoA=photoVisual.createComponentSnapshot(photoVisual.getLocalBounds());
    photoVisual.setState(CharacterHardwareWindow::Mode::Spring,true,true,
                          .16f,.18f,.09f,springPickups);
    const auto photoB=photoVisual.createComponentSnapshot(photoVisual.getLocalBounds());
    bool photoMoved=false;
    for(int y=30;y<140;++y)for(int x=75;x<560;++x)
        photoMoved=photoMoved||photoA.getPixelAt(x,y)!=photoB.getPixelAt(x,y);
    CHECK(photoMoved);
    // Both photographic coil bands must respond, not merely a global meter.
    bool upperPhotoMoved=false,lowerPhotoMoved=false,fastenersStayedFixed=true;
    for(int y=45;y<78;++y)for(int x=170;x<485;++x)
        upperPhotoMoved|=photoA.getPixelAt(x,y)!=photoB.getPixelAt(x,y);
    for(int y=93;y<138;++y)for(int x=170;x<485;++x)
        lowerPhotoMoved|=photoA.getPixelAt(x,y)!=photoB.getPixelAt(x,y);
    for(int y=30;y<140;++y)for(int x=39;x<75;++x)
        fastenersStayedFixed&=photoA.getPixelAt(x,y)==photoB.getPixelAt(x,y);
    CHECK(upperPhotoMoved&&lowerPhotoMoved&&fastenersStayedFixed);

    // An extra-wide host editor must LETTERBOX the exact same physical
    // chamber, not distort the screws, metal bezel or coil pitch.
    CharacterHardwareWindow photoAspect;
    photoAspect.setSize(640,190);
    photoAspect.setPhotoAtlas(photoBytes.getData(),static_cast<int>(photoBytes.getDataSize()));
    photoAspect.setState(CharacterHardwareWindow::Mode::Spring,true,false,
                         0.f,0.f,0.f,{});
    const auto normalAspect=photoAspect.createComponentSnapshot(photoAspect.getLocalBounds());
    photoAspect.setSize(800,190);
    const auto wideAspect=photoAspect.createComponentSnapshot(photoAspect.getLocalBounds());
    for(int y=17;y<183;y+=17)for(int x=30;x<615;x+=19)
        CHECK(normalAspect.getPixelAt(x,y)==wideAspect.getPixelAt(x+80,y));

    photoVisual.setState(CharacterHardwareWindow::Mode::Spring,true,false,
                          0.f,0.f,0.f,{});
    const auto photoPaused=photoVisual.createComponentSnapshot(photoVisual.getLocalBounds());
    bool photoHolds=true;
    for(int y=0;y<190;++y)for(int x=0;x<640;++x)
        photoHolds=photoHolds&&(photoB.getPixelAt(x,y)==photoPaused.getPixelAt(x,y));
    CHECK(photoHolds);
    photoVisual.setState(CharacterHardwareWindow::Mode::Spring,false,false,
                          0.f,0.f,0.f,{});
    const auto photoBypassed=photoVisual.createComponentSnapshot(photoVisual.getLocalBounds());
    bool photoBypassWorks=false;
    for(int y=30;y<140;++y)for(int x=75;x<560;++x)
        photoBypassWorks=photoBypassWorks||
            photoPaused.getPixelAt(x,y)!=photoBypassed.getPixelAt(x,y);
    CHECK(photoBypassWorks);
    // Lamp and LDR have distinct response times in the same photograph;
    // the test's preview value stands in for the real-output-gated LFO.
    photoVisual.setState(CharacterHardwareWindow::Mode::OpticalTremoloPreview,
                          true,true,0.f,0.f,0.f,{},0.f);
    const auto photoLampDark=photoVisual.createComponentSnapshot(photoVisual.getLocalBounds());
    for(int n=0;n<5;++n)
        photoVisual.setState(CharacterHardwareWindow::Mode::OpticalTremoloPreview,
                              true,true,.3f,.3f,0.f,{},1.f);
    const auto photoLampBright=photoVisual.createComponentSnapshot(photoVisual.getLocalBounds());
    CHECK(photoLampDark.getPixelAt(270,90)!=photoLampBright.getPixelAt(270,90));
    // Filament and sensor are reactive, fixed socket and chassis are not.
    CHECK(photoLampDark.getPixelAt(530,94)!=photoLampBright.getPixelAt(530,94));
    for(int y=35;y<136;y+=11)
    {
        CHECK(photoLampDark.getPixelAt(48,y)==photoLampBright.getPixelAt(48,y));
        CHECK(photoLampDark.getPixelAt(603,y)==photoLampBright.getPixelAt(603,y));
    }
    if(argc>1) {
        const juce::File folder{juce::String(argv[1])};
        CHECK(screenshot(photoVisual,folder.getChildFile("optical_audio_reactive_probe.png")));
        photoVisual.setState(CharacterHardwareWindow::Mode::Spring,
                              true,true,.16f,.18f,.09f,springPickups);
        CHECK(screenshot(photoVisual,folder.getChildFile("spring_photo_reactive_probe.png")));
    }
    // Authored transparent RGBA artwork must take precedence over heuristic
    // masks extracted from the lossy atlas, preserving exact background
    // and moving-coil pixels without relying on JPEG chroma thresholds.
    CharacterHardwareWindow explicitOnly,explicitOverJpeg;
    explicitOnly.setSize(640,190);explicitOverJpeg.setSize(640,190);
    explicitOverJpeg.setPhotoAtlas(photoBytes.getData(),
                                   static_cast<int>(photoBytes.getDataSize()));
    explicitOnly.setSpringArtwork(springBack,springLayers);
    explicitOverJpeg.setSpringArtwork(springBack,springLayers);
    explicitOnly.setState(CharacterHardwareWindow::Mode::Spring,
                          true,true,.2f,.16f,.1f,springPickups);
    explicitOverJpeg.setState(CharacterHardwareWindow::Mode::Spring,
                              true,true,.2f,.16f,.1f,springPickups);
    const auto springSource=explicitOnly.createComponentSnapshot(explicitOnly.getLocalBounds());
    const auto springOverride=explicitOverJpeg.createComponentSnapshot(explicitOverJpeg.getLocalBounds());
    for(int y=0;y<190;y+=7)for(int x=0;x<640;x+=11)
        CHECK(springSource.getPixelAt(x,y)==springOverride.getPixelAt(x,y));
    explicitOnly.setOpticalArtwork(opticalBack,opticalOff,opticalOn,opticalLdr);
    explicitOverJpeg.setOpticalArtwork(opticalBack,opticalOff,opticalOn,opticalLdr);
    explicitOnly.setState(CharacterHardwareWindow::Mode::OpticalTremoloPreview,
                          true,true,0.f,0.f,0.f,{},.9f);
    explicitOverJpeg.setState(CharacterHardwareWindow::Mode::OpticalTremoloPreview,
                              true,true,0.f,0.f,0.f,{},.9f);
    const auto opticalSource=explicitOnly.createComponentSnapshot(explicitOnly.getLocalBounds());
    const auto opticalOverride=explicitOverJpeg.createComponentSnapshot(explicitOverJpeg.getLocalBounds());
    for(int y=0;y<190;y+=7)for(int x=0;x<640;x+=11)
        CHECK(opticalSource.getPixelAt(x,y)==opticalOverride.getPixelAt(x,y));

    // A malformed/opaque authored mask must not overwrite the real
    // photographic fallback. Invalid dimensions and missing alpha should
    // be rejected, rather than stretched across the camera.
    CharacterHardwareWindow fallbackOnly,invalidOverride;
    fallbackOnly.setSize(640,190);
    invalidOverride.setSize(640,190);
    fallbackOnly.setPhotoAtlas(photoBytes.getData(),
                               static_cast<int>(photoBytes.getDataSize()));
    invalidOverride.setPhotoAtlas(photoBytes.getData(),
                                  static_cast<int>(photoBytes.getDataSize()));
    auto invalidLayers=springLayers;
    invalidLayers[0]=juce::Image(juce::Image::ARGB,32,32,true);
    invalidOverride.setSpringArtwork(springBack,invalidLayers);
    fallbackOnly.setState(CharacterHardwareWindow::Mode::Spring,
                           true,true,.2f,.16f,.1f,springPickups);
    invalidOverride.setState(CharacterHardwareWindow::Mode::Spring,
                              true,true,.2f,.16f,.1f,springPickups);
    const auto fallbackPixels=fallbackOnly.createComponentSnapshot(fallbackOnly.getLocalBounds());
    const auto rejectedPixels=invalidOverride.createComponentSnapshot(invalidOverride.getLocalBounds());
    for(int y=0;y<190;y+=7)for(int x=0;x<640;x+=11)
        CHECK(fallbackPixels.getPixelAt(x,y)==rejectedPixels.getPixelAt(x,y));

    // A fully opaque mask stored in ARGB is NOT valid alpha artwork.
    // Rejected material must leave the photographic atlas unchanged.
    juce::Image opaqueArgb(juce::Image::ARGB,640,190,true);
    {
        juce::Graphics paint(opaqueArgb);
        paint.fillAll(juce::Colours::red);
    }
    auto opaqueLayers=springLayers;
    opaqueLayers[0]=opaqueArgb;
    invalidOverride.setSpringArtwork(springBack,opaqueLayers);
    const auto rejectedOpaque=invalidOverride.createComponentSnapshot(invalidOverride.getLocalBounds());
    for(int y=0;y<190;y+=7)for(int x=0;x<640;x+=11)
        CHECK(fallbackPixels.getPixelAt(x,y)==rejectedOpaque.getPixelAt(x,y));

    // A fully transparent mask is invalid as well; it cannot animate.
    auto emptyLayers=springLayers;
    emptyLayers[0]=juce::Image(juce::Image::ARGB,640,190,true);
    invalidOverride.setSpringArtwork(springBack,emptyLayers);
    const auto rejectedEmpty=invalidOverride.createComponentSnapshot(invalidOverride.getLocalBounds());
    for(int y=0;y<190;y+=7)for(int x=0;x<640;x+=11)
        CHECK(fallbackPixels.getPixelAt(x,y)==rejectedEmpty.getPixelAt(x,y));

    fallbackOnly.setState(CharacterHardwareWindow::Mode::OpticalTremoloPreview,
                          true,true,.2f,.2f,0.f,{},.7f);
    invalidOverride.setOpticalArtwork(opticalBack,opticalOff,opaqueArgb,opticalLdr);
    invalidOverride.setState(CharacterHardwareWindow::Mode::OpticalTremoloPreview,
                             true,true,.2f,.2f,0.f,{},.7f);
    const auto opticalFallback=fallbackOnly.createComponentSnapshot(fallbackOnly.getLocalBounds());
    const auto rejectedOptical=invalidOverride.createComponentSnapshot(invalidOverride.getLocalBounds());
    for(int y=0;y<190;y+=7)for(int x=0;x<640;x+=11)
        CHECK(opticalFallback.getPixelAt(x,y)==rejectedOptical.getPixelAt(x,y));

    CHECK(presetLibraryChecks(argc>1?juce::File(juce::String(argv[1])):juce::File{})==0);
    {
        P6AudioProcessor probe,baseline;probe.prepareToPlay(48000,128);baseline.prepareToPlay(48000,128);
        p6plugin::ui::LiveAudioFrame captured{};std::uint32_t revision{};
        CHECK(!probe.readLiveOutput(captured,revision));
        probe.attachLiveDisplay();
        std::array<float,p6plugin::ui::kLiveFrameSize> expectedL{},expectedR{};
        juce::AudioBuffer<float> b(2,128),reference(2,128);juce::MidiBuffer events;
        events.addEvent(juce::MidiMessage::noteOn(1,60,juce::uint8{100}),0);
        for(int block=0;block<8;++block) {
            probe.processBlock(b,events);baseline.processBlock(reference,events);events.clear();
            for(int i=0;i<128;++i) {
                CHECK(b.getSample(0,i)==reference.getSample(0,i));
                CHECK(b.getSample(1,i)==reference.getSample(1,i));
                expectedL[static_cast<std::size_t>(block*128+i)]=b.getSample(0,i);
                expectedR[static_cast<std::size_t>(block*128+i)]=b.getSample(1,i);
            }
        }
        CHECK(probe.readLiveOutput(captured,revision));CHECK(captured.sampleRate==48000.);
        CHECK(captured.left==expectedL && captured.right==expectedR);
        const auto lastRevision=revision;probe.detachLiveDisplay();
        for(int i=0;i<8;++i)probe.processBlock(b,events);
        CHECK(probe.readLiveOutput(captured,revision) && revision==lastRevision);
        probe.releaseResources();baseline.releaseResources();
    }
    {
        P6AudioProcessor synth;synth.prepareToPlay(48000,128);
        set(synth,"wrapper_clock_source",0.f);
        set(synth,"nrpn_0160",1.f);
        set(synth,p6plugin::kArpCharacterEnabledID,1.f);
        set(synth,p6plugin::kArpCharacterFreezeID,1.f);
        set(synth,p6plugin::kArpCharacterMotionID,.4f);
        set(synth,p6plugin::kArpStepIDs[2],0.f);
        juce::AudioBuffer<float> audio(2,128);juce::MidiBuffer midi;
        midi.addEvent(juce::MidiMessage::noteOn(1,60,juce::uint8{100}),0);
        synth.processBlock(audio,midi);midi.clear();
        CHECK(synth.arpPlaybackStep()>=0&&synth.arpPlaybackStep()<8);
        juce::MemoryBlock state;synth.getStateInformation(state);
        P6AudioProcessor reopened;reopened.setStateInformation(state.getData(),static_cast<int>(state.getSize()));
        CHECK(reopened.parameters().getRawParameterValue(p6plugin::kArpCharacterEnabledID)->load()>=.5f);
        CHECK(reopened.parameters().getRawParameterValue(p6plugin::kArpCharacterFreezeID)->load()>=.5f);
        CHECK(reopened.parameters().getRawParameterValue(p6plugin::kArpStepIDs[2])->load()<.5f);
        CHECK(std::abs(reopened.parameters().getRawParameterValue(p6plugin::kArpCharacterMotionID)->load()-.4f)<.002f);
        const auto sysex=synth.exportEditBuffer();juce::String error;
        CHECK(synth.loadSysex(sysex,error));
        CHECK(synth.parameters().getRawParameterValue(p6plugin::kArpCharacterEnabledID)->load()<.5f);
        synth.releaseResources();
    }
    P6AudioProcessor p;
    CHECK(p.parameters().getRawParameterValue(p6plugin::kMasterOutputID)->load()==p6plugin::kNewMasterDb);
    CHECK(p.parameters().getRawParameterValue(p6plugin::kPlasmaModeID)->load()==1.f);
    CHECK(p.parameters().getRawParameterValue(p6plugin::kCustomIDs[0][5])->load()==.5f);
    p.prepareToPlay(48000,128);CHECK(p.getLatencySamples()==71);
    const auto sysex=p.exportEditBuffer();
    set(p,p6plugin::kMidiInputChannelID,7);
    set(p,p6plugin::kCharacterTypeID,1.f);set(p,p6plugin::kTubeIDs[0],.612f);
    set(p,p6plugin::kCharacterPositionID,0.f);
    juce::MemoryBlock state;p.getStateInformation(state);
    P6AudioProcessor restored;restored.setStateInformation(state.getData(),static_cast<int>(state.getSize()));
    CHECK(restored.parameters().getRawParameterValue(p6plugin::kMidiInputChannelID)->load()==7.f);
    CHECK(restored.parameters().getRawParameterValue(p6plugin::kPlasmaModeID)->load()==1.f);
    CHECK(restored.parameters().getRawParameterValue(p6plugin::kCharacterTypeID)->load()==1.f);
    CHECK(std::abs(restored.parameters().getRawParameterValue(p6plugin::kTubeIDs[0])->load()-.612f)<1e-5f);
    CHECK(restored.exportEditBuffer()==sysex);
    CHECK(restored.parameters().getRawParameterValue(p6plugin::kMasterOutputID)->load()==p6plugin::kNewMasterDb);
    // Loading a pre-master session must restore unity, regardless of the new
    // instance default, while retaining its payload and Character settings.
    std::vector<std::uint8_t> legacy(static_cast<const std::uint8_t*>(state.getData()),static_cast<const std::uint8_t*>(state.getData())+p6plugin::kV2StateBytes);
    legacy[4]=2;legacy[5]=0;restored.setStateInformation(legacy.data(),static_cast<int>(legacy.size()));
    CHECK(restored.parameters().getRawParameterValue(p6plugin::kMasterOutputID)->load()==0.f);
    CHECK(restored.parameters().getRawParameterValue(p6plugin::kMidiInputChannelID)->load()==0.f);
    CHECK(restored.parameters().getRawParameterValue(p6plugin::kPlasmaModeID)->load()==0.f);
    set(p,p6plugin::kMidiInputChannelID,0);
    CHECK(restored.exportEditBuffer()==sysex);
    std::unique_ptr<juce::AudioProcessorEditor> editor(p.createEditor());
    CHECK(editor!=nullptr);
    for(auto* child:editor->getChildren())if(auto* box=dynamic_cast<juce::ComboBox*>(child);box&&box->getTitle()=="Prophec-y effect")box->setSelectedId(3,juce::sendNotificationSync);
    bool foundPlasmaMode=false;
    for(auto* child:editor->getChildren())if(auto* box=dynamic_cast<juce::ComboBox*>(child);box&&box->getTitle()=="Plasma algorithm") {
        foundPlasmaMode=true;CHECK(box->isVisible());CHECK(box->getSelectedId()==2);
        box->setSelectedId(1,juce::sendNotificationSync);CHECK(p.parameters().getRawParameterValue(p6plugin::kPlasmaModeID)->load()==0.f);
        box->setSelectedId(2,juce::sendNotificationSync);CHECK(p.parameters().getRawParameterValue(p6plugin::kPlasmaModeID)->load()==1.f);
    }
    CHECK(foundPlasmaMode);
    // Restore Tube, which subsequent chamber checks exercise.
    for(auto* child:editor->getChildren())if(auto* box=dynamic_cast<juce::ComboBox*>(child);box&&box->getTitle()=="Prophec-y effect")box->setSelectedId(2,juce::sendNotificationSync);
    bool foundMidiSelector=false,foundMidiMonitor=false;
    for(auto* child:editor->getChildren()) {
        if(auto* box=dynamic_cast<juce::ComboBox*>(child);box&&box->getTitle()=="MIDI input channel") {
            foundMidiSelector=true;CHECK(box->getNumItems()==17);box->setSelectedId(4,juce::sendNotificationSync);
            CHECK(p.parameters().getRawParameterValue(p6plugin::kMidiInputChannelID)->load()==3.f);
            box->setSelectedId(1,juce::sendNotificationSync);
        }
        if(child->getTitle()=="Recent MIDI input")foundMidiMonitor=true;
    }
    CHECK(foundMidiSelector&&foundMidiMonitor);
    editor->setSize(1460,P6PluginEditor::expandedHeight);
    bool foundMaster=false;
    for(auto* child:editor->getChildren())if(auto* slider=dynamic_cast<juce::Slider*>(child);slider&&slider->getTitle()=="Master Output") {
        foundMaster=true;CHECK(slider->getTextFromValue(-18).contains("dB"));
        CHECK(slider->getValueFromText("-24 dB")==-24.);CHECK(slider->getValueFromText("nan")==slider->getValue());CHECK(slider->getValueFromText("999")==slider->getValue());
    }
    CHECK(foundMaster);
    bool driveDisplayFormatted=false;
    for(auto* child:editor->getChildren())
        if(auto* slider=dynamic_cast<juce::Slider*>(child);slider&&slider->getTitle()=="Drive")
            for(auto* text:slider->getChildren())
                if(auto* label=dynamic_cast<juce::Label*>(text);label&&label->getText().contains("dB"))driveDisplayFormatted=true;
    CHECK(driveDisplayFormatted);
    CHECK(p.exportEditBuffer()==sysex);
    // The single cutoff knob retains its raw/host parameter and common control
    // contract. Invalid text must not produce an edit or a host gesture.
    ProphecyParameterControl* cutoff=nullptr;
    int commonCutoffs=0,commonControls=0;
    for(auto* child:editor->getChildren())
        if(auto* control=dynamic_cast<ProphecyParameterControl*>(child);control&&control->hasDescriptor()) {
            ++commonControls;
            if(control->parameterNrpn()==45){++commonCutoffs;if(!cutoff)cutoff=control;}
        }
    CHECK(commonCutoffs==1&&cutoff!=nullptr&&commonControls==36);
    auto* cutoffParameter=p.parameters().getParameter("nrpn_0045");
    GestureProbe gestures;cutoffParameter->addListener(&gestures);
    CHECK(cutoff->applyExactText("50 %"));
    CHECK(cutoff->getValueFromText("50 %")==82.);
    CHECK(p.parameters().getRawParameterValue("nrpn_0045")->load()==82.f);
    CHECK(!cutoff->applyExactText("nan"));CHECK(!cutoff->applyExactText("999",true));
    CHECK(gestures.begins==1&&gestures.ends==1);
    CHECK(cutoff->keyPressed(juce::KeyPress(juce::KeyPress::rightKey)));
    CHECK(p.parameters().getRawParameterValue("nrpn_0045")->load()==83.f);
    cutoff->resetToInit();CHECK(gestures.begins==3&&gestures.ends==3);
    cutoffParameter->removeListener(&gestures);
    CHECK(p.exportEditBuffer()==sysex);
    CHECK(cutoff->getWantsKeyboardFocus());CHECK(cutoff->isAccessible());
    CHECK(cutoff->getTitle().isNotEmpty()&&cutoff->getDescription().isNotEmpty());
    // The standalone smoke editor is normally hosted without an OS peer.
    // Do not manufacture an unhosted AudioProcessorEditor window: bare Xvfb
    // has no window-manager atoms, unlike a real DAW-owned plugin window.
    if(editor->getWindowHandle()!=nullptr)CHECK(cutoff->getAccessibilityHandler()!=nullptr);
    else std::cout<<"Native peer unavailable locally; accessibility metadata checked, OS integration requires host testing\n";
    CHECK(cutoff->getTextFromValue(cutoff->getValue()).contains("%"));
    const auto previousAttack=p.parameters().getRawParameterValue("nrpn_0078")->load();
    set(p,"nrpn_0078",64.f);set(p,"wrapper_calibration",1.f);
    bool effectiveEnvelopeText=false;
    for(auto* child:editor->getChildren())
        if(auto* slider=dynamic_cast<juce::Slider*>(child);slider&&slider->getTitle()=="ATTACK"&&slider->getValue()==64.)
            effectiveEnvelopeText=slider->getTextFromValue(slider->getValue())=="320 ms";
    CHECK(effectiveEnvelopeText);
    set(p,"nrpn_0078",previousAttack);set(p,"wrapper_calibration",0.f);
    CHECK(p.exportEditBuffer()==sysex);
    // Every visible control must remain inside the resized physical surface.

    // The screwed-on header is a separate surface: its controls must not
    // collide with each other or with Master at any supported editor width.
    TubeChamber* chamber=nullptr;
    juce::TextButton* fold=nullptr;
    juce::TextButton* presets=nullptr;
    juce::Label* midiReadout=nullptr;
    juce::Label* masterLabel=nullptr;
    juce::Slider* master=nullptr;
    OutputMeter* meter=nullptr;
    juce::TextButton* resetClip=nullptr;
    juce::TextButton* moduleButton=nullptr;
    juce::TextButton* studioButton=nullptr;
    juce::ComboBox* midiChannel=nullptr;
    for(auto* child:editor->getChildren()) {
        if(auto* device=dynamic_cast<TubeChamber*>(child))chamber=device;
        if(auto* output=dynamic_cast<OutputMeter*>(child))meter=output;
        if(auto* button=dynamic_cast<juce::TextButton*>(child)) {
            if(button->getButtonText()=="HIDE")fold=button;
            if(button->getTitle()=="Preset browser")presets=button;
            if(button->getButtonText()=="RESET CLIP")resetClip=button;
            if(button->getButtonText()=="MODULE")moduleButton=button;
            if(button->getButtonText()=="STUDIO")studioButton=button;
        }
        if(auto* label=dynamic_cast<juce::Label*>(child)) {
            if(label->getTitle()=="Recent MIDI input")midiReadout=label;
            if(label->getText()=="MASTER")masterLabel=label;
        }
        if(auto* slider=dynamic_cast<juce::Slider*>(child);slider&&slider->getTitle()=="Master Output")master=slider;
        if(auto* combo=dynamic_cast<juce::ComboBox*>(child);combo&&combo->getTitle()=="MIDI input channel")midiChannel=combo;
    }
    CHECK(chamber!=nullptr&&chamber->isVisible()&&fold!=nullptr);
    CHECK(presets&&midiReadout&&masterLabel&&master&&meter&&resetClip&&moduleButton&&studioButton&&midiChannel);
    // This test protects the cramped FX and Performance controls that were
    // previously intersecting at the bottom edge of the synth panel.
    juce::ComboBox* fxA=nullptr;
    juce::ComboBox* fxB=nullptr;
    juce::ToggleButton* fxOn=nullptr;
    juce::ToggleButton* glideOn=nullptr;
    juce::Label* glideLabel=nullptr;
    juce::Slider* mixA=nullptr;
    juce::Slider* mixB=nullptr;
    juce::Slider* programVol=nullptr;
    for(auto* child:editor->getChildren()) {
        if(auto* combo=dynamic_cast<juce::ComboBox*>(child)) {
            if(combo->getTitle()=="Standard FX A type")fxA=combo;
            if(combo->getTitle()=="Standard FX B type")fxB=combo;
        }
        if(auto* toggle=dynamic_cast<juce::ToggleButton*>(child)) {
            if(toggle->getButtonText()=="FX ON")fxOn=toggle;
            if(toggle->getButtonText()=="GLIDE ON")glideOn=toggle;
        }
        if(auto* label=dynamic_cast<juce::Label*>(child);label&&label->getText()=="GLIDE")glideLabel=label;
        if(auto* slider=dynamic_cast<juce::Slider*>(child)) {
            if(slider->getTitle()=="MIX A")mixA=slider;
            if(slider->getTitle()=="MIX B")mixB=slider;
            if(slider->getTitle()=="PROGRAM VOL")programVol=slider;
        }
    }
    CHECK(fxA&&fxB&&fxOn&&glideOn&&glideLabel&&mixA&&mixB&&programVol);
    CHECK(fxA->getItemText(0)=="A: OFF"&&fxB->getItemText(0)=="B: OFF");

    CharacterHardwareWindow* hardware=nullptr;
    TubeChamber* originalTube=nullptr;
    juce::TextButton* optoPreview=nullptr;
    for(auto* child:editor->getChildren()) {
        if(auto* device=dynamic_cast<CharacterHardwareWindow*>(child))hardware=device;
        if(auto* tube=dynamic_cast<TubeChamber*>(child))originalTube=tube;
        if(auto* button=dynamic_cast<juce::TextButton*>(child)) {
            if(button->getButtonText()=="HIDE")fold=button;
            if(button->getButtonText()=="OPTO PREVIEW")optoPreview=button;
        }
    }
    // The Tube artwork is the original TubeChamber; Spring and optical
    // previews use the new panoramic photo window instead.
    CHECK(hardware!=nullptr&&originalTube!=nullptr&&fold!=nullptr&&optoPreview!=nullptr);
#if defined(P6_HAS_SPRING_RGBA)
    CHECK(hardware->hasSpringArtwork());
#else
    CHECK(!hardware->hasSpringArtwork());
#endif
#if defined(P6_HAS_OPTICAL_RGBA)
    CHECK(hardware->hasOpticalArtwork());
#else
    CHECK(!hardware->hasOpticalArtwork());
#endif
    CHECK(originalTube->isVisible()&&!hardware->isVisible());

    juce::MemoryBlock beforeFold;p.getStateInformation(beforeFold);
    fold->onClick();
    CHECK(!originalTube->isVisible()&&!hardware->isVisible());
    CHECK(editor->getHeight()==P6PluginEditor::compactHeight);
    juce::MemoryBlock afterFold;p.getStateInformation(afterFold);
    CHECK(beforeFold==afterFold);
    fold->onClick();
    CHECK(originalTube->isVisible()&&!hardware->isVisible());
    CHECK(editor->getHeight()==P6PluginEditor::expandedHeight);
    // Optical preview must replace Tube VISUALLY but never alter any
    // musical parameter, preset or wrapper state.
    juce::MemoryBlock beforePreview;p.getStateInformation(beforePreview);
    optoPreview->onClick();
    CHECK(hardware->isVisible()&&!originalTube->isVisible()&&optoPreview->getToggleState());
    // Interactive concept knobs and waveform must be visible but must NEVER
    // mutate any actual audio/host parameter or saved preset.
    int livePreviewControls=0;
    bool liveWaveform=false;
    for(auto* child:editor->getChildren())
    {
        if(auto* slider=dynamic_cast<juce::Slider*>(child);
           slider&&slider->getTitle().startsWith("Optical preview "))
        {
            CHECK(slider->isVisible());
            ++livePreviewControls;
            slider->setValue(.72,juce::sendNotificationSync);
        }
        if(auto* wave=dynamic_cast<juce::ComboBox*>(child);
           wave&&wave->getTitle()=="Optical preview LFO waveform")
        {
            CHECK(wave->isVisible()&&wave->getNumItems()==4);
            wave->setSelectedId(2,juce::sendNotificationSync);
            liveWaveform=true;
        }
    }
    CHECK(livePreviewControls==3&&liveWaveform);
    // Exercise the exact installed atlas in the actual editor (not only a
    // synthetic isolated fixture). A visual preview can light the filament
    // without mutating host state or enabling nonexistent optical DSP.
    const auto actualDark=hardware->createComponentSnapshot(hardware->getLocalBounds());
    if(argc>1) {
        const juce::File folder{juce::String(argv[1])};folder.createDirectory();
        CHECK(screenshot(*editor,folder.getChildFile("module_optical_tremolo_preview_dark.png")));
    }
    for(int step=0;step<24;++step)
        hardware->setState(CharacterHardwareWindow::Mode::OpticalTremoloPreview,
                           true,true,.2f,.2f,0.f,{},1.f,.72f,1.f/30.f);
    const auto actualLit=hardware->createComponentSnapshot(hardware->getLocalBounds());
    bool actualLampChanged=false;
    for(int y=25;y<actualDark.getHeight()-35;y+=3)
        for(int x=35;x<actualDark.getWidth()-35;x+=3)
            actualLampChanged |= actualDark.getPixelAt(x,y)!=actualLit.getPixelAt(x,y);
    CHECK(actualLampChanged);
    if(argc>1) {
        const juce::File folder{juce::String(argv[1])};folder.createDirectory();
        CHECK(screenshot(*editor,folder.getChildFile("module_optical_tremolo_preview_lit.png")));
        CHECK(screenshot(*editor,folder.getChildFile("module_optical_tremolo_preview.png")));
        for (int width:{1240,1780})
        {
            editor->setSize(width,P6PluginEditor::expandedHeight);
            CHECK(screenshot(*editor,folder.getChildFile(
                "module_optical_tremolo_preview_"+juce::String(width)+".png")));
        }
        editor->setSize(1460,P6PluginEditor::expandedHeight);
    }
    optoPreview->onClick();
    CHECK(!hardware->isVisible()&&originalTube->isVisible()&&!optoPreview->getToggleState());
    juce::MemoryBlock afterPreview;p.getStateInformation(afterPreview);
    CHECK(beforePreview==afterPreview);
    // Selecting Spring must expose the Spring camera and reuse all seven
    // parameter attachments with Spring parameter IDs, never the Tube IDs.
    const float springInitial=p.parameters().getRawParameterValue(
        p6plugin::kCustomIDs[1][0])->load();
    p.selectCharacterType(3);
    editor.reset(p.createEditor());
    bool springCamera=false;
    std::array<bool,7> springControlBound{};
    std::array<float,7> tubeValuesBefore{},springValuesBefore{};
    for(std::size_t i=0;i<7;++i)
    {
        tubeValuesBefore[i]=p.parameters().getRawParameterValue(
            p6plugin::kTubeIDs[i])->load();
        springValuesBefore[i]=p.parameters().getRawParameterValue(
            p6plugin::kCustomIDs[1][i])->load();
    }
    for(auto* child:editor->getChildren())
    {
        if(auto* camera=dynamic_cast<CharacterHardwareWindow*>(child))
            springCamera=camera->isVisible();
        if(auto* tube=dynamic_cast<TubeChamber*>(child))
            CHECK(!tube->isVisible());
        if(auto* slider=dynamic_cast<juce::Slider*>(child);slider&&slider->isVisible()&&
           slider->getTooltip().contains("Optical Spring"))
        {
            for(std::size_t i=0;i<7;++i)
            {
                if(slider->getTitle()!=p6plugin::kCustomNames[1][i])continue;
                springControlBound[i]=true;
                const float testValue=.2f+.085f*static_cast<float>(i);
                slider->setValue(testValue,juce::sendNotificationSync);
                CHECK(std::abs(p.parameters().getRawParameterValue(
                    p6plugin::kCustomIDs[1][i])->load()-testValue)<.002f);
                for(std::size_t j=0;j<7;++j)
                    CHECK(p.parameters().getRawParameterValue(
                        p6plugin::kTubeIDs[j])->load()==tubeValuesBefore[j]);
                slider->setValue(springValuesBefore[i],juce::sendNotificationSync);
            }
        }
    }
    CHECK(springCamera);
    for(bool bound:springControlBound)CHECK(bound);
    for(std::size_t i=0;i<7;++i)
        CHECK(std::abs(p.parameters().getRawParameterValue(
            p6plugin::kCustomIDs[1][i])->load()-springValuesBefore[i])<.002f);
    if(argc>1)
    {
        const juce::File folder{juce::String(argv[1])};
        CHECK(screenshot(*editor,folder.getChildFile("module_optical_spring.png")));
        for (int width:{1240,1780})
        {
            editor->setSize(width,P6PluginEditor::expandedHeight);
            CHECK(screenshot(*editor,folder.getChildFile(
                "module_optical_spring_"+juce::String(width)+".png")));
        }
        editor->setSize(1460,P6PluginEditor::expandedHeight);
    }
    p.selectCharacterType(1);
    editor.reset(p.createEditor());
    {
        juce::Button* panelSwitch=nullptr;
        for(auto* child:editor->getChildren())
            if(child->getTitle()=="Arpeggiator panel")panelSwitch=dynamic_cast<juce::Button*>(child);
        CHECK(panelSwitch!=nullptr);
        panelSwitch->onClick();
        CHECK(editor->getHeight()==P6PluginEditor::expandedHeight+104);
        int visibleSteps=0;
        juce::Button* firstStep=nullptr;
        for(auto* child:editor->getChildren())
            if(child->getTitle().startsWith("Arpeggiator step ")) {
                CHECK(child->isVisible());
                ++visibleSteps;
                if(child->getTitle()=="Arpeggiator step 1")firstStep=dynamic_cast<juce::Button*>(child);
            }
        CHECK(visibleSteps==8&&firstStep!=nullptr);
        firstStep->setToggleState(false,juce::sendNotificationSync);
        CHECK(p.parameters().getRawParameterValue(p6plugin::kArpStepIDs[0])->load()<.5f);
        firstStep->setToggleState(true,juce::sendNotificationSync);
        if(argc>1){const juce::File folder{juce::String(argv[1])};folder.createDirectory();CHECK(screenshot(*editor,folder.getChildFile("module_arpeggiator.png")));}
        panelSwitch->onClick();
        CHECK(editor->getHeight()==P6PluginEditor::expandedHeight);
    }
    // The Spring checks recreate the editor, so reacquire every pointer before
    // testing layout. The previous editor and its components no longer exist.
    presets=nullptr;midiReadout=nullptr;masterLabel=nullptr;master=nullptr;meter=nullptr;
    resetClip=nullptr;moduleButton=nullptr;studioButton=nullptr;midiChannel=nullptr;
    fxA=nullptr;fxB=nullptr;fxOn=nullptr;glideOn=nullptr;glideLabel=nullptr;
    mixA=nullptr;mixB=nullptr;programVol=nullptr;
    for(auto* child:editor->getChildren()) {
        if(auto* output=dynamic_cast<OutputMeter*>(child))meter=output;
        if(auto* button=dynamic_cast<juce::TextButton*>(child)) {
            if(button->getTitle()=="Preset browser")presets=button;
            if(button->getButtonText()=="RESET CLIP")resetClip=button;
            if(button->getButtonText()=="MODULE")moduleButton=button;
            if(button->getButtonText()=="STUDIO")studioButton=button;
        }
        if(auto* label=dynamic_cast<juce::Label*>(child)) {
            if(label->getTitle()=="Recent MIDI input")midiReadout=label;
            if(label->getText()=="MASTER")masterLabel=label;
            if(label->getText()=="GLIDE")glideLabel=label;
        }
        if(auto* slider=dynamic_cast<juce::Slider*>(child)) {
            if(slider->getTitle()=="Master Output")master=slider;
            if(slider->getTitle()=="MIX A")mixA=slider;
            if(slider->getTitle()=="MIX B")mixB=slider;
            if(slider->getTitle()=="PROGRAM VOL")programVol=slider;
        }
        if(auto* combo=dynamic_cast<juce::ComboBox*>(child)) {
            if(combo->getTitle()=="MIDI input channel")midiChannel=combo;
            if(combo->getTitle()=="Standard FX A type")fxA=combo;
            if(combo->getTitle()=="Standard FX B type")fxB=combo;
        }
        if(auto* toggle=dynamic_cast<juce::ToggleButton*>(child)) {
            if(toggle->getButtonText()=="FX ON")fxOn=toggle;
            if(toggle->getButtonText()=="GLIDE ON")glideOn=toggle;
        }
    }
    CHECK(presets&&midiReadout&&masterLabel&&master&&meter&&resetClip&&moduleButton&&studioButton&&midiChannel);
    CHECK(fxA&&fxB&&fxOn&&glideOn&&glideLabel&&mixA&&mixB&&programVol);
    for(int width:{1240,1460,1780}) {
        editor->setSize(width,P6PluginEditor::expandedHeight);
        for(auto* child:editor->getChildren()) if(child->isVisible()) {
            CHECK(child->getX()>=0 && child->getY()>=0);
            CHECK(child->getRight()<=editor->getWidth());
            CHECK(child->getBottom()<=editor->getHeight()-28);
        }
        const float sx=static_cast<float>(width)/1460.f;
        // First envelope knob aligns with the first Poly Mod knob; the first
        // LFO knob shares that center as well.
        juce::Slider *polyFirst=nullptr,*lfoFirst=nullptr,*envFirst=nullptr;
        for(auto* child:editor->getChildren())if(auto* slider=dynamic_cast<juce::Slider*>(child);slider&&slider->isVisible()) {
            if(slider->getTitle()=="FILTER ENV"&&slider->getY()<200)polyFirst=slider;
            if(slider->getTitle()=="RATE"&&slider->getY()>200&&slider->getY()<300)lfoFirst=slider;
            if(slider->getTitle()=="ATTACK"&&slider->getY()>300&&slider->getY()<400)envFirst=slider;
        }
        CHECK(polyFirst&&lfoFirst&&envFirst);
        CHECK(std::abs(polyFirst->getBounds().getCentreX()-envFirst->getBounds().getCentreX())<=2);
        CHECK(std::abs(polyFirst->getBounds().getCentreX()-lfoFirst->getBounds().getCentreX())<=2);
        // The column separator must not run through any visible control.
        // Its lower end stops before the standard FX section spans columns.
        const juce::Rectangle<int> divider(juce::roundToInt(996.f*sx),107,1,403);
        for(auto* child:editor->getChildren())
            if(child->isVisible())CHECK(!divider.intersects(child->getBounds()));
        const juce::Rectangle<int> screen(juce::roundToInt(40.f*sx),14,juce::roundToInt(1380.f*sx),71);
        juce::Component* menu=nullptr;juce::Component* presetTitle=nullptr;
        for(auto* child:editor->getChildren()){if(child->getTitle()=="Header settings")menu=child;if(child->getTitle()=="Current preset")presetTitle=child;}
        CHECK(menu&&presetTitle&&menu->isVisible()&&presetTitle->isVisible());
        int lowPassKnobs=0;for(auto* child:editor->getChildren())if(auto* slider=dynamic_cast<juce::Slider*>(child);slider&&slider->isVisible()&&slider->getTitle()=="LOW PASS")++lowPassKnobs;
        CHECK(lowPassKnobs==1);
        CHECK(!midiReadout->isVisible()&&!midiChannel->isVisible()&&!resetClip->isVisible()&&!moduleButton->isVisible()&&!studioButton->isVisible());
        const std::array<juce::Component*,4> headerControls{presets,meter,menu,presetTitle};
        const juce::Rectangle<int> lowerGlass(juce::roundToInt(501.f*sx),73,juce::roundToInt(477.f*sx),180);
        CHECK(lowerGlass.contains(master->getBounds())&&lowerGlass.contains(masterLabel->getBounds()));
        CHECK(!master->getBounds().intersects(masterLabel->getBounds()));
        if(width==1460){
            studioButton->onClick();CHECK(master->isVisible());
            juce::Component* studioPanel=nullptr;for(auto* child:editor->getChildren())if(dynamic_cast<juce::GenericAudioProcessorEditor*>(child))studioPanel=child;
            CHECK(studioPanel&&studioPanel->isVisible()&&!studioPanel->getBounds().intersects(master->getBounds()));
            moduleButton->onClick();CHECK(!studioPanel->isVisible());
        }
        for(auto* control:headerControls)CHECK(screen.contains(control->getBounds()));
        for(std::size_t i=0;i<headerControls.size();++i)
            for(std::size_t j=i+1;j<headerControls.size();++j)
                CHECK(!headerControls[i]->getBounds().intersects(headerControls[j]->getBounds()));
        // Two FX rows and their controls fit in the shared synthesizer panel.
        const juce::Rectangle<int> fxBay(juce::roundToInt(906.f*sx),528,juce::roundToInt(496.f*sx),104);
        for(auto* fx:std::array<juce::Component*,6>{fxA,fxB,fxOn,mixA,mixB,programVol})
            CHECK(fxBay.contains(fx->getBounds()));
        CHECK(fxA->getWidth()>=juce::roundToInt(165.f*sx));
        CHECK(fxB->getWidth()>=juce::roundToInt(165.f*sx));
        CHECK(!fxA->getBounds().intersects(fxB->getBounds()));
        CHECK(!fxA->getBounds().intersects(mixA->getBounds()));
        CHECK(!fxB->getBounds().intersects(mixA->getBounds()));
        CHECK(!mixA->getBounds().intersects(mixB->getBounds()));
        CHECK(!mixB->getBounds().intersects(programVol->getBounds()));
        CHECK(!glideOn->getBounds().intersects(glideLabel->getBounds()));
        juce::GlyphArrangement glideText;
        glideText.addLineOfText(juce::Font(10.5f),glideOn->getButtonText(),0.f,0.f);
        CHECK(glideText.getBoundingBox(0,-1,true).getWidth()<=static_cast<float>(glideOn->getWidth()-20));
        // Check rendered label width, not just button rectangles: the long
        // original LFO and Poly Mod names were truncated even without overlap.
        int modulationButtons=0;
        for(auto* child:editor->getChildren())
            if(auto* toggle=dynamic_cast<juce::ToggleButton*>(child);
               toggle && (toggle->getTitle().startsWith("Poly Mod ") || toggle->getTitle().startsWith("LFO "))
               && static_cast<bool>(toggle->getProperties().getWithDefault("ledTile",false))) {
                ++modulationButtons;
                juce::GlyphArrangement text;
                text.addLineOfText(juce::Font(9.5f),toggle->getButtonText(),0.f,0.f);
                CHECK(text.getBoundingBox(0,-1,true).getWidth() <= static_cast<float>(toggle->getWidth()));
                CHECK(toggle->getTooltip().isNotEmpty());
            }
        CHECK(modulationButtons==10);
    }
    editor->setSize(1460,P6PluginEditor::expandedHeight);

    juce::AudioBuffer<float> audio(2,128);juce::MidiBuffer midi;
    midi.addEvent(juce::MidiMessage::noteOn(1,60,.8f),0);
    double energy=0;
    for(int block=0;block<150;++block) {
        if(block==30)set(p,p6plugin::kCharacterPositionID,1.f);
        if(block==60)set(p,p6plugin::kCharacterBypassID,1.f);
        if(block==90)set(p,p6plugin::kCharacterTypeID,0.f);
        if(block==120){set(p,p6plugin::kCharacterTypeID,1.f);set(p,p6plugin::kCharacterBypassID,0.f);}
        p.processBlock(audio,midi);midi.clear();
        for(int c=0;c<2;++c)for(int n=0;n<128;++n){CHECK(std::isfinite(audio.getSample(c,n)));energy+=audio.getSample(c,n)*audio.getSample(c,n);}
    }
    CHECK(energy>0);
    // Verify the full processor->UI mailbox is wired, not only that the
    // independent mailbox template behaves correctly under concurrency.
    p6plugin::CharacterTelemetry coherentFrame{};
    std::uint32_t coherentGeneration{};
    CHECK(p.readCharacterTelemetry(coherentFrame,coherentGeneration));
    CHECK(coherentGeneration>0);
    CHECK(coherentGeneration==p.characterTelemetryRevision());
    CHECK(std::isfinite(coherentFrame.inputRms));
    CHECK(std::isfinite(coherentFrame.outputRms));
    CHECK(std::isfinite(coherentFrame.changeRms));
    for(float value:coherentFrame.device.values)CHECK(std::isfinite(value));
    for(float value:coherentFrame.device.input)CHECK(std::isfinite(value));
    for(float value:coherentFrame.device.wet)CHECK(std::isfinite(value));
    auto meters=p.consumeOutputReadings();CHECK(meters.prePeak[0]>0&&meters.peak[0]>0);
    CHECK(std::abs(meters.peak[0]/meters.prePeak[0]-std::pow(10.f,-18.f/20.f))<.0001f);
    CHECK(p.consumeOutputReadings().peak[0]==0); // UI consumption retains short peaks until read
    set(p,p6plugin::kMasterOutputID,-24.f);
    p.getStateInformation(state);restored.setStateInformation(state.getData(),static_cast<int>(state.getSize()));
    CHECK(restored.parameters().getRawParameterValue(p6plugin::kMasterOutputID)->load()==-24.f);
    CHECK(p.exportEditBuffer()==sysex);set(p,p6plugin::kMasterOutputID,p6plugin::kNewMasterDb);
    for(int block=0;block<8;++block)p.processBlock(audio,midi);
    // Reopen after live automation: attachments must reflect the current host
    // values without a timer turn or an audio restart.
    editor.reset();editor.reset(p.createEditor());editor->setSize(1460,P6PluginEditor::expandedHeight);
    if(argc>1) {
        const juce::File folder{juce::String(argv[1])};folder.createDirectory();
        CHECK(screenshot(*editor,folder.getChildFile("module_tube.png")));
        for (int width:{1240,1780})
        {
            editor->setSize(width,P6PluginEditor::expandedHeight);
            CHECK(screenshot(*editor,folder.getChildFile(
                "module_tube_"+juce::String(width)+".png")));
        }
        editor->setSize(1460,P6PluginEditor::expandedHeight);
        // Capture the actual contextual display and prove selection alone
        // preserves both the canonical patch and complete wrapper state.
        const auto oldA=p.parameters().getRawParameterValue("nrpn_0067")->load();
        const auto oldD=p.parameters().getRawParameterValue("nrpn_0068")->load();
        const auto oldS=p.parameters().getRawParameterValue("nrpn_0069")->load();
        const auto oldR=p.parameters().getRawParameterValue("nrpn_0070")->load();
        set(p,"nrpn_0067",80);set(p,"nrpn_0068",60);set(p,"nrpn_0069",80);set(p,"nrpn_0070",70);
        juce::MemoryBlock selectionBefore;p.getStateInformation(selectionBefore);
        ContextDisplay* display=nullptr;
        for(auto* child:editor->getChildren())if(auto* candidate=dynamic_cast<ContextDisplay*>(child))display=candidate;
        CHECK(display!=nullptr);
        for(int nrpn:{3,67,78,45}) {
            bool selected=false;
            for(auto* child:editor->getChildren())
                if(auto* control=dynamic_cast<ProphecyParameterControl*>(child);control&&control->hasDescriptor()&&control->parameterNrpn()==nrpn) {
                    CHECK(static_cast<bool>(control->onSelected));control->onSelected();selected=true;break;
                }
            CHECK(selected);
            CHECK(display->content().spec.kind==(nrpn==3?p6plugin::ui::PreviewKind::Oscillator:nrpn==45?p6plugin::ui::PreviewKind::Position:p6plugin::ui::PreviewKind::Envelope));
            if(nrpn!=45)CHECK(display->curve().valid&&!display->curve().points.empty());
            CHECK(screenshot(*display,folder.getChildFile("display_"+juce::String(nrpn)+".png")));
            if(nrpn==67)CHECK(screenshot(*editor,folder.getChildFile("module_contextual.png")));
        }
        juce::MemoryBlock selectionAfter;p.getStateInformation(selectionAfter);CHECK(selectionAfter==selectionBefore);
        set(p,"nrpn_0067",oldA);set(p,"nrpn_0068",oldD);set(p,"nrpn_0069",oldS);set(p,"nrpn_0070",oldR);

        int frame=0;
        float quietLevel=0.f,loudLevel=0.f;
        for(int volume:{0,12,24,64,100,127,100,64,24,12,0,0}) {
            midi.addEvent(juce::MidiMessage::controllerEvent(1,7,volume),0);
            for(int block=0;block<32;++block){p.processBlock(audio,midi);midi.clear();}
            if(frame==0)quietLevel=p.characterTelemetry().inputRms;
            if(frame==5)loudLevel=p.characterTelemetry().inputRms;
            editor.reset();editor.reset(p.createEditor());
            CHECK(screenshot(*editor,folder.getChildFile("tube_signal_"+juce::String(frame++).paddedLeft('0',2)+".png")));
        }
        CHECK(loudLevel>quietLevel*4.f);
        midi.addEvent(juce::MidiMessage::controllerEvent(1,7,127),0);
        for(int block=0;block<32;++block){p.processBlock(audio,midi);midi.clear();}
        // A second native frame proves that bypass extinguishes the heater
        // while leaving the complete tube/control surface available.
        set(p,p6plugin::kCharacterBypassID,1.f);
        for(int i=0;i<200;++i)p.processBlock(audio,midi);
        CHECK(!p.characterTelemetry().active);
        editor.reset();editor.reset(p.createEditor());
        CHECK(screenshot(*editor,folder.getChildFile("module_tube_bypass.png")));
        set(p,p6plugin::kCharacterBypassID,0.f);
        for(auto* child:editor->getChildren())
            if(auto* button=dynamic_cast<juce::TextButton*>(child);button&&button->getButtonText()=="STUDIO")button->onClick();
        CHECK(screenshot(*editor,folder.getChildFile("studio.png")));
        set(p,p6plugin::kCharacterTypeID,0.f);
        editor.reset();editor.reset(p.createEditor());
        CHECK(editor->getHeight()==P6PluginEditor::compactHeight);
        CHECK(screenshot(*editor,folder.getChildFile("module_none.png")));
    }
    // Added IDs do not alter the old None/Tube normalized automation domain.
    CHECK(p.parameters().getParameter(p6plugin::kCharacterTypeID)->convertFrom0to1(1.f)==1.f);
    for(int type=2;type<6;++type){
        p.selectCharacterType(type);CHECK(p.selectedCharacterType()==type);
        for(std::size_t i=0;i<7;++i)set(p,p6plugin::kCustomIDs[static_cast<std::size_t>(type-2)][i],p6plugin::kCustomDefaults[static_cast<std::size_t>(type-2)][i]);
        for(int block=0;block<300;++block)p.processBlock(audio,midi);
        CHECK(p.characterTelemetry().type==type);CHECK(p.characterTelemetry().active);
        editor.reset();editor.reset(p.createEditor());CHECK(editor->getHeight()==P6PluginEditor::expandedHeight);
        int controls=0;
        for(auto* child:editor->getChildren())if(auto* slider=dynamic_cast<juce::Slider*>(child);slider&&slider->isVisible()){
            for(std::size_t i=0;i<7;++i)if(slider->getTitle()==p6plugin::kCustomNames[static_cast<std::size_t>(type-2)][i]){
                ++controls;CHECK(slider->getValueFromText("nan")==slider->getValue());CHECK(slider->getValueFromText("999999")==slider->getValue());const float value=i==5?.8f:.42f;slider->setValue(value,juce::sendNotificationSync);
                CHECK(std::abs(p.parameters().getRawParameterValue(p6plugin::kCustomIDs[static_cast<std::size_t>(type-2)][i])->load()-value)<.001f);
            }
        }
        CHECK(controls==7);p.getStateInformation(state);restored.setStateInformation(state.getData(),static_cast<int>(state.getSize()));
        CHECK(restored.selectedCharacterType()==type);CHECK(restored.exportEditBuffer()==sysex);
        CHECK(p6plugin::encodeState(restored.presetSnapshot())==p6plugin::encodeState(p.presetSnapshot()));
        for(int block=0;block<300;++block)p.processBlock(audio,midi);
        editor.reset();editor.reset(p.createEditor());
        if(argc>1){const juce::File folder{juce::String(argv[1])};CHECK(screenshot(*editor,folder.getChildFile("module_custom_"+juce::String(type)+".png")));}
    }
    editor.reset();p.releaseResources();
    std::cout<<"Native editor, restored controls, SysEx isolation and live audio smoke passed\n";
}


