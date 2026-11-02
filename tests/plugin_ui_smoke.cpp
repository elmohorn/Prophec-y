#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "PluginStateCodec.h"
#include "p6/SysexCodec.h"
#include <juce_audio_processors/juce_audio_processors.h>
#include <cmath>
#include <iostream>

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
    p6plugin::PresetLibrary library(root,factory);CHECK(library.entries().size()==1);CHECK(!root.exists());
    auto patch=factory.patch;patch.rawPayload[1023]=217;
    auto first=p6::SysexCodec::encodeProgramDump(patch,2,10),second=p6::SysexCodec::encodeProgramDump(patch,2,9);
    std::vector<std::uint8_t> bank=first;bank.insert(bank.end(),second.begin(),second.end());
    int imported=0;CHECK(library.importSysex(bank,"Local test bank",imported).wasOk());CHECK(imported==2);CHECK(library.entries().size()==3);
    juce::String importedId,bankKey;for(const auto& p:library.entries())if(p.source==p6plugin::PresetSource::Imported&&p.program==10){importedId=p.id;bankKey=p.bankKey;}
    CHECK(!importedId.isEmpty());CHECK(library.exportBank(bankKey)==bank);
    CHECK(library.setFavorite(importedId,true).wasOk());CHECK(library.setFavorite("factory-init",true).wasOk());
    p6plugin::PresetLibrary reopened(root,factory);CHECK(reopened.find(importedId)->favorite);CHECK(reopened.find("factory-init")->favorite);
    CHECK(reopened.exportBank(bankKey)==bank);CHECK(reopened.find(importedId)->state.patch.rawPayload[1023]==217);
    auto broken=bank;broken.push_back(0x55);CHECK(reopened.importSysex(broken,"bad",imported).failed());CHECK(imported==0);CHECK(reopened.entries().size()==3);
    P6AudioProcessor target;set(target,p6plugin::kMasterOutputID,-24);set(target,"wrapper_clock_source",2);set(target,"wrapper_calibration",1);
    auto user=target.presetSnapshot();user.character.type=1;user.character.tube[0]=.73f;
    juce::String userId;CHECK(reopened.saveUser({},user,"User test patch","Tests",0,0,userId).wasOk());
    CHECK(reopened.saveUser({},user,"Duplicate slot","",0,0,userId).failed());
    const auto stored=*reopened.find(userId);target.loadMusicalPreset(stored.state,stored.id,stored.name);
    CHECK(!target.presetDirty());CHECK(target.parameters().getRawParameterValue(p6plugin::kMasterOutputID)->load()==-24.f);
    CHECK(target.parameters().getRawParameterValue("wrapper_clock_source")->load()==2.f);CHECK(target.parameters().getRawParameterValue("wrapper_calibration")->load()==1.f);
    CHECK(std::abs(target.parameters().getRawParameterValue(p6plugin::kTubeIDs[0])->load()-.73f)<1.e-5f);
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
        for(auto* child:gallery.getChildren())if(auto* button=dynamic_cast<juce::TextButton*>(child);button&&button->getTitle()=="Preset browser")button->onClick();
        CHECK(browser->isVisible());CHECK(screenshot(gallery,captureFolder.getChildFile("module_browser.png")));
        juce::TextEditor* search=nullptr;for(auto* child:browser->getChildren())if(auto* edit=dynamic_cast<juce::TextEditor*>(child);edit&&edit->getTitle()=="Search presets")search=edit;
        CHECK(search!=nullptr);search->setText("not-a-preset",false);search->onTextChange();
        juce::ListBox* list=nullptr;for(auto* child:browser->getChildren())if(auto* l=dynamic_cast<juce::ListBox*>(child))list=l;
        CHECK(list!=nullptr&&list->getModel()->getNumRows()==0);search->setText("User test",false);search->onTextChange();CHECK(list->getModel()->getNumRows()==1);
        search->setText({},false);search->onTextChange();CHECK(list->getModel()->getNumRows()==4);
        for(int width:{1240,1460,1780}){gallery.setSize(width,P6PluginEditor::expandedHeight);CHECK(gallery.getLocalBounds().contains(browser->getBounds()));for(auto* child:browser->getChildren())if(child->isVisible())CHECK(browser->getLocalBounds().contains(child->getBounds()));}
    }
    std::cout<<"Preset library: persistence, exact banks, favorites, occupied slots, stale writes, globals, dirty tracking and browser filtering verified\n";
    return 0;
}
int main(int argc,char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    CHECK(presetLibraryChecks(argc>1?juce::File(juce::String(argv[1])):juce::File{})==0);
    P6AudioProcessor p;
    CHECK(p.parameters().getRawParameterValue(p6plugin::kMasterOutputID)->load()==p6plugin::kNewMasterDb);
    p.prepareToPlay(48000,128);CHECK(p.getLatencySamples()==71);
    const auto sysex=p.exportEditBuffer();
    set(p,p6plugin::kCharacterTypeID,1.f);set(p,p6plugin::kTubeIDs[0],.612f);
    set(p,p6plugin::kCharacterPositionID,0.f);
    juce::MemoryBlock state;p.getStateInformation(state);
    P6AudioProcessor restored;restored.setStateInformation(state.getData(),static_cast<int>(state.getSize()));
    CHECK(restored.parameters().getRawParameterValue(p6plugin::kCharacterTypeID)->load()==1.f);
    CHECK(std::abs(restored.parameters().getRawParameterValue(p6plugin::kTubeIDs[0])->load()-.612f)<1e-5f);
    CHECK(restored.exportEditBuffer()==sysex);
    CHECK(restored.parameters().getRawParameterValue(p6plugin::kMasterOutputID)->load()==p6plugin::kNewMasterDb);
    // Loading a pre-master session must restore unity, regardless of the new
    // instance default, while retaining its payload and Character settings.
    std::vector<std::uint8_t> legacy(static_cast<const std::uint8_t*>(state.getData()),static_cast<const std::uint8_t*>(state.getData())+p6plugin::kV2StateBytes);
    legacy[4]=2;legacy[5]=0;restored.setStateInformation(legacy.data(),static_cast<int>(legacy.size()));
    CHECK(restored.parameters().getRawParameterValue(p6plugin::kMasterOutputID)->load()==0.f);
    CHECK(restored.exportEditBuffer()==sysex);
    std::unique_ptr<juce::AudioProcessorEditor> editor(p.createEditor());
    CHECK(editor!=nullptr);
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
    // The two cutoff knobs share one raw/host parameter and common control
    // contract. Invalid text must not produce an edit or a host gesture.
    ProphecyParameterControl* cutoff=nullptr;
    int commonCutoffs=0,commonControls=0;
    for(auto* child:editor->getChildren())
        if(auto* control=dynamic_cast<ProphecyParameterControl*>(child);control&&control->hasDescriptor()) {
            ++commonControls;
            if(control->parameterNrpn()==45){++commonCutoffs;if(!cutoff)cutoff=control;}
        }
    CHECK(commonCutoffs==2&&cutoff!=nullptr&&commonControls==37);
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
    TubeChamber* chamber=nullptr;
    juce::TextButton* fold=nullptr;
    for(auto* child:editor->getChildren()) {
        if(auto* device=dynamic_cast<TubeChamber*>(child))chamber=device;
        if(auto* button=dynamic_cast<juce::TextButton*>(child);button&&button->getButtonText()=="HIDE")fold=button;
    }
    CHECK(chamber!=nullptr&&chamber->isVisible()&&fold!=nullptr);
    juce::MemoryBlock beforeFold;p.getStateInformation(beforeFold);
    fold->onClick();CHECK(!chamber->isVisible());CHECK(editor->getHeight()==P6PluginEditor::compactHeight);
    juce::MemoryBlock afterFold;p.getStateInformation(afterFold);CHECK(beforeFold==afterFold);
    fold->onClick();CHECK(chamber->isVisible());CHECK(editor->getHeight()==P6PluginEditor::expandedHeight);
    for(int width:{1240,1460,1780}) {
        editor->setSize(width,P6PluginEditor::expandedHeight);
        for(auto* child:editor->getChildren()) if(child->isVisible()) {
            CHECK(child->getX()>=0 && child->getY()>=0);
            CHECK(child->getRight()<=editor->getWidth());
            CHECK(child->getBottom()<=editor->getHeight()-28);
        }
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
    editor.reset();p.releaseResources();
    std::cout<<"Native editor, restored controls, SysEx isolation and live audio smoke passed\n";
}

