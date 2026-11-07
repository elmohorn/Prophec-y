#include "PluginProcessor.h"
#include "PluginStateCodec.h"
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <functional>
#include <vector>
#include <thread>

struct PauseOnChange final : juce::AudioProcessorParameter::Listener {
    std::atomic<bool> entered{},proceed{};
    void parameterValueChanged(int,float) override {
        entered.store(true,std::memory_order_release);
        while(!proceed.load(std::memory_order_acquire))std::this_thread::yield();
    }
    void parameterGestureChanged(int,bool) override {}
};
struct Watchdog {
    std::atomic<bool> done{};
    std::thread thread{[this]{for(int i=0;i<800;++i){if(done.load(std::memory_order_acquire))return;std::this_thread::sleep_for(std::chrono::milliseconds(50));}std::cerr<<"restore probe timed out\n";std::_Exit(124);}};
    ~Watchdog(){done.store(true,std::memory_order_release);thread.join();}
};
int main()
{
    Watchdog watchdog;
    juce::ScopedJuceInitialiser_GUI gui;P6AudioProcessor processor,reference;processor.prepareToPlay(48000,128);reference.prepareToPlay(48000,128);
    auto state=processor.presetSnapshot();state.character.type=4;state.masterOutputDb=-42.f;state.extras[6]=1;
    const auto bytes=p6plugin::encodeState(state);
    auto* selector=processor.parameters().getParameter(p6plugin::kMasterOutputID);
    PauseOnChange pause;selector->addListener(&pause);
    std::thread restore([&]{processor.setStateInformation(bytes.data(),static_cast<int>(bytes.size()));});
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(5);
    while(!pause.entered.load(std::memory_order_acquire)&&std::chrono::steady_clock::now()<deadline)std::this_thread::yield();
    if(!pause.entered.load(std::memory_order_acquire)){pause.proceed=true;restore.join();selector->removeListener(&pause);std::cerr<<"restore did not reach the controlled pause\n";return 2;}
    juce::AudioBuffer<float> audio(2,128),referenceAudio(2,128);juce::MidiBuffer midi;
    // NRPN arriving mid-restore must use the old calibration too. Compare the
    // complete audio with an untouched reference instance receiving the same MIDI.
    for(const auto& cc:std::array<std::pair<int,int>,4>{{{99,0},{98,67},{6,0},{38,80}}})
        midi.addEvent(juce::MidiMessage::controllerEvent(1,cc.first,cc.second),0);
    midi.addEvent(juce::MidiMessage::noteOn(1,60,.8f),1);
    bool audioMatchesOldState=true;
    for(int i=0;i<20;++i){
        processor.processBlock(audio,midi);reference.processBlock(referenceAudio,midi);
        for(int channel=0;channel<2;++channel)for(int n=0;n<128;++n)
            audioMatchesOldState=audioMatchesOldState&&std::abs(audio.getSample(channel,n)-referenceAudio.getSample(channel,n))<1.e-6f;
        midi.clear();
    }
    const auto type=processor.characterTelemetry().type;
    const auto rawMaster=processor.parameters().getRawParameterValue(p6plugin::kMasterOutputID)->load();
    const auto meters=processor.consumeOutputReadings();
    const float observedGain=meters.prePeak[0]>1.e-8f?meters.peak[0]/meters.prePeak[0]:-1.f;
    // A simultaneous save must wait for the writer and receive the complete
    // restored state. No host or GUI callback is made while holding the audio gate.
    std::atomic<bool> saveStarted{},saveFinished{};juce::MemoryBlock saved;
    std::thread saver([&]{saveStarted.store(true,std::memory_order_release);processor.getStateInformation(saved);saveFinished.store(true,std::memory_order_release);});
    while(!saveStarted.load(std::memory_order_acquire))std::this_thread::yield();
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    const bool coherentBefore=audioMatchesOldState && type==0 && observedGain>0.f && std::abs(observedGain-std::pow(10.f,-18.f/20.f))<.001f;
    const bool saveWaited=!saveFinished.load(std::memory_order_acquire);
    pause.proceed.store(true,std::memory_order_release);restore.join();saver.join();selector->removeListener(&pause);
    p6plugin::WrapperState savedState;const bool savedOk=p6plugin::decodeState({static_cast<const std::uint8_t*>(saved.getData()),saved.getSize()},savedState);
    for(int i=0;i<20;++i)processor.processBlock(audio,midi);
    const bool coherentAfter=processor.characterTelemetry().type==4
        && std::abs(processor.parameters().getRawParameterValue(p6plugin::kMasterOutputID)->load()+42.f)<.01f;
    const bool completeSave=savedOk&&savedState.character.type==4&&savedState.extras[6]==1&&std::abs(savedState.masterOutputDb+42.f)<.01f;
    std::cout<<"During paused restore: Character="<<type<<", host Master="<<rawMaster<<" dB, audio gain="<<observedGain
             <<"; audio matches old state="<<audioMatchesOldState<<", save waited="<<saveWaited<<", complete save="<<completeSave<<", coherent after="<<coherentAfter<<'\n';
    // Competing state writers must publish one complete state, never a
    // combination, while the stream continues to process blocks.
    auto a=processor.presetSnapshot(),b=a;
    a.character.type=2;a.masterOutputDb=-12.f;a.midiInputChannel=1;a.patch.rawPayload[1023]=31;
    b.character.type=5;b.masterOutputDb=-54.f;b.midiInputChannel=2;b.patch.rawPayload[1023]=213;
    const auto bytesA=p6plugin::encodeState(a),bytesB=p6plugin::encodeState(b);
    bool competingWritersCoherent=true;
    for(int iteration=0;iteration<24;++iteration) {
        std::atomic<bool> start{};std::atomic<int> finished{};
        auto load=[&](const std::vector<std::uint8_t>& data){
            while(!start.load(std::memory_order_acquire))std::this_thread::yield();
            processor.setStateInformation(data.data(),static_cast<int>(data.size()));finished.fetch_add(1,std::memory_order_release);
        };
        std::thread first(load,std::cref(bytesA)),second(load,std::cref(bytesB));
        start.store(true,std::memory_order_release);
        for(int n=0;n<500 && finished.load(std::memory_order_acquire)<2;++n)processor.processBlock(audio,midi);
        first.join();second.join();const auto current=processor.presetSnapshot();
        const bool matchesA=current.character.type==2&&std::abs(current.masterOutputDb+12.f)<.01f&&current.midiInputChannel==1&&current.patch.rawPayload[1023]==31;
        const bool matchesB=current.character.type==5&&std::abs(current.masterOutputDb+54.f)<.01f&&current.midiInputChannel==2&&current.patch.rawPayload[1023]==213;
        competingWritersCoherent=competingWritersCoherent&&(matchesA||matchesB);
    }
    std::cout<<"24 competing-writer rounds: "<<(competingWritersCoherent?"coherent":"mixed state observed")<<'\n';
    processor.releaseResources();reference.releaseResources();
    return coherentBefore&&saveWaited&&completeSave&&coherentAfter&&competingWritersCoherent?0:1;
}
