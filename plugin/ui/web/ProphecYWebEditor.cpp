#include "ProphecYWebEditor.h"
#include "../../PluginProcessor.h"
#include "BinaryData.h"
#include "../../CharacterFx.h"
#include "../../PresetLibrary.h"

#if !JUCE_WEB_BROWSER_RESOURCE_PROVIDER_AVAILABLE
 #error "Prophec-Y v3 requires a WebView resource provider; enable JUCE_USE_WIN_WEBVIEW2 on Windows"
#endif
#include <algorithm>
#include <array>
#include <cmath>

namespace {
constexpr std::array<int,30> kKnobNrpn{0,3,4,2,5,8,9,7,45,46,77,49,120,128,143,144,
                                       88,89,78,79,80,81,67,68,69,70,30,33,63,62};
constexpr std::array<int,4> kFaderNrpn{2,7,27,32};
constexpr std::array<int,14> kSwitchNrpn{135,145,146,147,148,149,91,93,94,95,97,156,29,71};
juce::String nrpnId(int id){return "nrpn_"+juce::String(id).paddedLeft('0',4);}
juce::String property(const juce::var& object,const char* name){
    if(auto* o=object.getDynamicObject())return o->getProperty(name).toString();
    return {};
}
} // namespace

juce::WebBrowserComponent::Options ProphecYWebEditor::makeOptions()
{
    auto options=juce::WebBrowserComponent::Options{}
        .withNativeIntegrationEnabled()
        .withEventListener("prophecyParameter",[this](juce::var v){handleParameter(v);})
        .withEventListener("prophecyGesture",[this](juce::var v){handleGesture(v);})
        .withEventListener("prophecyResize",[this](juce::var v){handleResize(v);})
        .withEventListener("prophecyPresetNavigate",[this](juce::var v){handlePresetNavigate(v);})
        .withEventListener("prophecyPresetCatalogRequest",[this](juce::var){
            // Another plugin instance may have updated favorites or User presets.
            // Refresh only on an explicit UI request, never from the audio thread.
            juce::Result result=juce::Result::ok();
            if(presetLibrary_)result=presetLibrary_->scan();
            emitPresetCatalog();
            if(result.failed()){
                auto* error=new juce::DynamicObject;
                error->setProperty("message",result.getErrorMessage());
                browser_.emitEventIfBrowserIsVisible("prophecyPresetError",juce::var(error));
            }
        })
        .withEventListener("prophecyPresetLoad",[this](juce::var v){handlePresetLoad(v);})
        .withEventListener("prophecyPresetFavorite",[this](juce::var v){handlePresetFavorite(v);})
        .withEventListener("prophecySequenceEdit",[this](juce::var v){handleSequenceEdit(v);})
        .withEventListener("prophecySequenceRequest",[this](juce::var){emitSequenceState();})
        .withEventListener("prophecyReady",[this](juce::var){ready_=true;forceFullState_=true;});
#if JUCE_WINDOWS
    options=options.withBackend(juce::WebBrowserComponent::Options::Backend::webview2);
#endif
#if JUCE_WEB_BROWSER_RESOURCE_PROVIDER_AVAILABLE
    options=options.withResourceProvider([](const juce::String& path)
        ->std::optional<juce::WebBrowserComponent::Resource> {
        if(path!="/"&&path!="/index.html")return std::nullopt;
        const auto* first=reinterpret_cast<const std::byte*>(BinaryData::ProphecY_v3_0_html);
        return juce::WebBrowserComponent::Resource{
            std::vector<std::byte>(first,first+BinaryData::ProphecY_v3_0_htmlSize),"text/html"};
    });
#endif
    return options;
}

ProphecYWebEditor::ProphecYWebEditor(P6AudioProcessor& p)
    :juce::AudioProcessorEditor(p),processor_(p),browser_(makeOptions())
{
    presetLibrary_=std::make_unique<p6plugin::PresetLibrary>(
        p6plugin::PresetLibrary::defaultRoot(),P6AudioProcessor::factoryInitPreset());
    (void)presetLibrary_->scan();
    addAndMakeVisible(browser_);
    setSize(1670,865);
    browser_.goToURL(juce::WebBrowserComponent::getResourceProviderRoot());
    startTimerHz(20);
}

ProphecYWebEditor::~ProphecYWebEditor()
{
    stopTimer();
    for(const auto& id:gestures_)
        if(auto* p=processor_.parameters().getParameter(id))p->endChangeGesture();
}

void ProphecYWebEditor::resized(){browser_.setBounds(getLocalBounds());}

void ProphecYWebEditor::handleParameter(const juce::var& payload)
{
    const auto id=property(payload,"id");
    auto* object=payload.getDynamicObject();
    if(!object||id.isEmpty())return;
    const auto value=object->getProperty("value");
    if(!value.isDouble()&&!value.isInt()&&!value.isInt64())return;
    const float normalized=static_cast<float>(static_cast<double>(value));
    if(!std::isfinite(normalized))return;
    if(auto* p=processor_.parameters().getParameter(id))
        p->setValueNotifyingHost(std::clamp(normalized,0.f,1.f));
}

void ProphecYWebEditor::handleGesture(const juce::var& payload)
{
    const auto id=property(payload,"id"),phase=property(payload,"phase");
    if(auto* p=processor_.parameters().getParameter(id)){
        if(phase=="begin"&&gestures_.insert(id).second)p->beginChangeGesture();
        else if(phase=="end"&&gestures_.erase(id)>0)p->endChangeGesture();
    }
}

void ProphecYWebEditor::handleResize(const juce::var& payload)
{
    if(auto* object=payload.getDynamicObject()){
        const bool shown=static_cast<bool>(object->getProperty("character"));
        const int desiredWidth=shown?1960:1670;
        if(getWidth()!=desiredWidth)setSize(desiredWidth,865);
    }
}

void ProphecYWebEditor::handlePresetNavigate(const juce::var& payload)
{
    if (!presetLibrary_) return;
    const auto& entries=presetLibrary_->entries();
    if (entries.empty()) return;
    const int delta=property(payload,"delta").getIntValue();
    if (delta==0) return;
    const auto current=processor_.activePresetId();
    int index=-1;
    for (int i=0;i<static_cast<int>(entries.size());++i)
        if (entries[static_cast<std::size_t>(i)].id==current){index=i;break;}
    const int count=static_cast<int>(entries.size());
    const int next=index<0?(delta>0?0:count-1):(index+(delta>0?1:-1)+count)%count;
    const auto& selected=entries[static_cast<std::size_t>(next)];
    processor_.loadMusicalPreset(selected.state,selected.id,selected.name);
    forceFullState_=true;
    lastPresetId_.clear();
}

void ProphecYWebEditor::emitPresetCatalog()
{
    if(!presetLibrary_)return;
    juce::Array<juce::var> list;
    for(const auto& entry:presetLibrary_->entries()){
        auto* item=new juce::DynamicObject;
        item->setProperty("id",entry.id);
        item->setProperty("name",entry.name);
        item->setProperty("category",entry.category);
        item->setProperty("bank",entry.bankLabel);
        item->setProperty("source",entry.source==p6plugin::PresetSource::Factory?"FACTORY":
                                    entry.source==p6plugin::PresetSource::Imported?"IMPORTED":"USER");
        item->setProperty("bankNumber",entry.bank);
        item->setProperty("program",entry.program);
        item->setProperty("favorite",entry.favorite);
        list.add(juce::var(item));
    }
    browser_.emitEventIfBrowserIsVisible("prophecyPresetCatalog",juce::var(list));
}

void ProphecYWebEditor::handlePresetLoad(const juce::var& payload)
{
    if(!presetLibrary_)return;
    const auto id=property(payload,"id");
    if(id.isEmpty())return;
    if(const auto* preset=presetLibrary_->find(id)){
        processor_.loadMusicalPreset(preset->state,preset->id,preset->name);
        forceFullState_=true;
        lastPresetId_.clear();
        return;
    }
    // A deleted or replaced User preset must not leave the UI stuck at LOADING.
    auto* error=new juce::DynamicObject;
    error->setProperty("message","Preset not found; refresh the library");
    browser_.emitEventIfBrowserIsVisible("prophecyPresetError",juce::var(error));
}

void ProphecYWebEditor::handlePresetFavorite(const juce::var& payload)
{
    if(!presetLibrary_)return;
    const auto id=property(payload,"id");
    auto* object=payload.getDynamicObject();
    if(!object||id.isEmpty())return;
    const bool favorite=static_cast<bool>(object->getProperty("favorite"));
    const auto result=presetLibrary_->setFavorite(id,favorite);
    if(result.wasOk())emitPresetCatalog();
    else{
        auto* error=new juce::DynamicObject;
        error->setProperty("message",result.getErrorMessage());
        browser_.emitEventIfBrowserIsVisible("prophecyPresetError",juce::var(error));
        emitPresetCatalog();
    }
}

void ProphecYWebEditor::handleSequenceEdit(const juce::var& payload)
{
    if(!payload.getDynamicObject())return;
    const int track=property(payload,"track").getIntValue();
    const int step=property(payload,"step").getIntValue();
    const int note=property(payload,"note").getIntValue();
    const int velocity=property(payload,"velocity").getIntValue();
    if(processor_.editSequenceCell(track,step,note,velocity))
        emitSequenceState();
}

void ProphecYWebEditor::emitSequenceState()
{
    const auto sequence=processor_.sequenceSnapshot();
    juce::Array<juce::var> tracks;
    for(const auto& track:sequence.tracks){
        juce::Array<juce::var> cells;
        for(const auto& cell:track){
            auto* item=new juce::DynamicObject;
            item->setProperty("note",static_cast<int>(cell.noteRaw));
            item->setProperty("velocity",static_cast<int>(cell.velocity));
            // Firmware flag semantics are unknown. Preserve, never edit as tie.
            item->setProperty("flag",cell.highBitFlag);
            cells.add(juce::var(item));
        }
        tracks.add(juce::var(cells));
    }
    auto* state=new juce::DynamicObject;
    state->setProperty("tracks",juce::var(tracks));
    browser_.emitEventIfBrowserIsVisible("prophecySequenceState",juce::var(state));
    lastSequenceRevision_=processor_.sequenceRevision();
}

void ProphecYWebEditor::timerCallback()
{
    if(!ready_ || !browser_.isShowing())return;
    auto* object=new juce::DynamicObject;
    bool changed=false;
    auto append=[&](const juce::String& id){
        if(auto* p=processor_.parameters().getParameter(id)){
            const float value=p->getValue();
            auto it=lastValues_.find(id);
            if(forceFullState_||it==lastValues_.end()||std::abs(it->second-value)>0.0001f){
                object->setProperty(juce::Identifier(id),value);
                lastValues_[id]=value;
                changed=true;
            }
        }
    };
    for(int nrpn:kKnobNrpn)append(nrpnId(nrpn));
    for(int nrpn:kFaderNrpn)append(nrpnId(nrpn));
    for(int nrpn:kSwitchNrpn)append(nrpnId(nrpn));
    append("wrapper_master_output_db");
    append("wrapper_arp_hold");
    append("wrapper_arp_beat_sync");
    append("wrapper_clock_source");
    for(const int nrpn:{47,90,119,127,161,162,163})append(nrpnId(nrpn));
    append(nrpnId(167));
    append(nrpnId(168));
    append(nrpnId(169));
    append("runtime_sequence_play");
    append("arp_character_rhythm");
    append("arp_character_motion");
    append("arp_character_enabled");
    for(const auto* id:p6plugin::kArpStepIDs)append(id);
    append(nrpnId(160));
    append(nrpnId(168));
    append("character_bypass");
    append("character_type");
    append("character_custom_type");
    for (const auto* id : p6plugin::kTubeIDs) append(id);
    for (const auto& device : p6plugin::kCustomIDs)
        for (const auto* id : device) append(id);
    forceFullState_=false;
    const auto presetId=processor_.activePresetId();
    const auto presetName=processor_.activePresetName();
    if(presetId!=lastPresetId_||presetName!=lastPresetName_){
        auto* state=new juce::DynamicObject;
        state->setProperty("name",presetName);
        state->setProperty("id",presetId);
        int index=-1,total=0;
        if(presetLibrary_){
            const auto& entries=presetLibrary_->entries();
            total=static_cast<int>(entries.size());
            for(int i=0;i<total;++i)
                if(entries[static_cast<std::size_t>(i)].id==presetId){index=i;break;}
        }
        state->setProperty("index",index);
        state->setProperty("total",total);
        browser_.emitEventIfBrowserIsVisible("prophecyPresetState",juce::var(state));
        lastPresetId_=presetId;
        lastPresetName_=presetName;
    }
    if(lastSequenceRevision_!=processor_.sequenceRevision())emitSequenceState();
    if(changed)browser_.emitEventIfBrowserIsVisible("prophecyHostState",juce::var(object));
    else delete object;
    const auto levels=processor_.consumeOutputReadings();
    auto* meter=new juce::DynamicObject;
    meter->setProperty("leftPeak",std::clamp(levels.peak[0],0.f,1.f));
    meter->setProperty("rightPeak",std::clamp(levels.peak[1],0.f,1.f));
    meter->setProperty("leftRms",std::clamp(levels.rms[0],0.f,1.f));
    meter->setProperty("rightRms",std::clamp(levels.rms[1],0.f,1.f));
    meter->setProperty("clips",static_cast<int>(levels.clips));
    meter->setProperty("playStep",processor_.arpPlaybackStep());
    meter->setProperty("sequenceStep",processor_.sequencePlaybackStep());
    browser_.emitEventIfBrowserIsVisible("prophecyTelemetry",juce::var(meter));
}
