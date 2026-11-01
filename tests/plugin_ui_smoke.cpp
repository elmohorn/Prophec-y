#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <juce_audio_processors/juce_audio_processors.h>
#include <cmath>
#include <iostream>

#define CHECK(x) do{if(!(x)){std::cerr<<"failed: " #x <<" line "<<__LINE__<<'\n';return 1;}}while(false)
static void set(P6AudioProcessor& p,const char* id,float v)
{
    auto* parameter=p.parameters().getParameter(id);
    parameter->beginChangeGesture();parameter->setValueNotifyingHost(parameter->convertTo0to1(v));parameter->endChangeGesture();
}
static void screenshot(juce::Component& c,const juce::File& file)
{
    juce::Image image(juce::Image::ARGB,c.getWidth(),c.getHeight(),true);
    juce::Graphics g(image);c.paintEntireComponent(g,true);
    file.deleteFile();
    juce::FileOutputStream stream(file);juce::PNGImageFormat().writeImageToStream(image,stream);
}
int main(int argc,char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    P6AudioProcessor p;
    p.prepareToPlay(48000,128);CHECK(p.getLatencySamples()==71);
    const auto sysex=p.exportEditBuffer();
    set(p,p6plugin::kCharacterTypeID,1.f);set(p,p6plugin::kTubeIDs[0],.612f);
    set(p,p6plugin::kCharacterPositionID,0.f);
    juce::MemoryBlock state;p.getStateInformation(state);
    P6AudioProcessor restored;restored.setStateInformation(state.getData(),static_cast<int>(state.getSize()));
    CHECK(restored.parameters().getRawParameterValue(p6plugin::kCharacterTypeID)->load()==1.f);
    CHECK(std::abs(restored.parameters().getRawParameterValue(p6plugin::kTubeIDs[0])->load()-.612f)<1e-5f);
    CHECK(restored.exportEditBuffer()==sysex);
    std::unique_ptr<juce::AudioProcessorEditor> editor(p.createEditor());
    CHECK(editor!=nullptr);
    editor->setSize(1460,P6PluginEditor::expandedHeight);
    bool driveDisplayFormatted=false;
    for(auto* child:editor->getChildren())
        if(auto* slider=dynamic_cast<juce::Slider*>(child);slider&&slider->getTitle()=="Drive")
            for(auto* text:slider->getChildren())
                if(auto* label=dynamic_cast<juce::Label*>(text);label&&label->getText().contains("dB"))driveDisplayFormatted=true;
    CHECK(driveDisplayFormatted);
    CHECK(p.exportEditBuffer()==sysex);
    // Every visible control must remain inside the resized physical surface.
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
    // Reopen after live automation: attachments must reflect the current host
    // values without a timer turn or an audio restart.
    editor.reset();editor.reset(p.createEditor());editor->setSize(1460,P6PluginEditor::expandedHeight);
    if(argc>1) {
        const juce::File folder{juce::String(argv[1])};folder.createDirectory();
        screenshot(*editor,folder.getChildFile("module_tube.png"));
        int frame=0;
        float quietLevel=0.f,loudLevel=0.f;
        for(int volume:{0,12,24,64,100,127,100,64,24,12,0,0}) {
            midi.addEvent(juce::MidiMessage::controllerEvent(1,7,volume),0);
            for(int block=0;block<32;++block){p.processBlock(audio,midi);midi.clear();}
            if(frame==0)quietLevel=p.characterTelemetry().inputRms;
            if(frame==5)loudLevel=p.characterTelemetry().inputRms;
            editor.reset();editor.reset(p.createEditor());
            screenshot(*editor,folder.getChildFile("tube_signal_"+juce::String(frame++).paddedLeft('0',2)+".png"));
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
        screenshot(*editor,folder.getChildFile("module_tube_bypass.png"));
        set(p,p6plugin::kCharacterBypassID,0.f);
        for(auto* child:editor->getChildren())
            if(auto* button=dynamic_cast<juce::TextButton*>(child);button&&button->getButtonText()=="STUDIO")button->onClick();
        screenshot(*editor,folder.getChildFile("studio.png"));
        set(p,p6plugin::kCharacterTypeID,0.f);
        editor.reset();editor.reset(p.createEditor());
        CHECK(editor->getHeight()==P6PluginEditor::compactHeight);
        screenshot(*editor,folder.getChildFile("module_none.png"));
    }
    editor.reset();p.releaseResources();
    std::cout<<"Native editor, restored controls, SysEx isolation and live audio smoke passed\n";
}

