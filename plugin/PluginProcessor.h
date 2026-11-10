#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "p6/P6Engine.h"
#include "p6/P6PerformanceScheduler.h"
#include "p6/P6MidiRouter.h"
#include "p6/P6PatchState.h"
#include "p6/ParameterMapper.h"
#include "p6/VersionedEditMailbox.h"
#include "CharacterFx.h"
#include "RealtimeSnapshotMailbox.h"
#include "ui/display/LiveSpectrum.h"
#include "OutputStage.h"
#include "ArpCharacterParameters.h"
#include "PresetBankCodec.h"
#include "ui/ParameterPresentation.h"

#include <array>
#include <atomic>
#include <mutex>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

class P6AudioProcessor final : public juce::AudioProcessor, private juce::Timer
{
public:
    P6AudioProcessor();
    ~P6AudioProcessor() override;

    void prepareToPlay (double sampleRate, int maximumExpectedSamplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override;
    void changeProgramName (int, const juce::String&) override;

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // Non-realtime import/export API for the future preset browser. SysEx is
    // deliberately not decoded in processBlock(), because the codec owns
    // strings/vectors and is not an audio-thread operation.
    bool loadSysex (std::span<const std::uint8_t> bytes, juce::String& error);
    std::vector<std::uint8_t> exportEditBuffer();

    // Non-realtime preset operations. Browsing changes musical state only.
    p6plugin::WrapperState presetSnapshot();
    static p6plugin::WrapperState factoryInitPreset();
    void loadMusicalPreset(const p6plugin::WrapperState&,const juce::String& id,const juce::String& name);
    void adoptPreset(const juce::String& id,const juce::String& name,const p6plugin::WrapperState* baseline=nullptr);
    bool presetDirty();
    juce::String activePresetId() const;
    juce::String activePresetName() const;
    juce::AudioProcessorValueTreeState& parameters() noexcept { return apvts_; }
    const p6::P6PatchState& canonicalPatchUnsafeForAudioThread() const noexcept { return patch_; }
    p6plugin::ui::ParameterContext parameterPresentationContext() const
    {
        return {selectedCalibration(),apvts_.getRawParameterValue("nrpn_0091")->load()>=.5f};
    }
    float characterExcitation() const noexcept { return characterExcitation_.load(std::memory_order_relaxed); }
    // Never return a torn audio frame: all 8 device meters and 64 scope
    // samples come from the same completed processBlock publication.
    [[nodiscard]] bool readCharacterTelemetry(
        p6plugin::CharacterTelemetry& out,std::uint32_t& revision) const noexcept
    {
        return characterTelemetryMailbox_.tryRead(out,revision);
    }
    p6plugin::CharacterTelemetry characterTelemetry() const noexcept
    {
        p6plugin::CharacterTelemetry result{};
        std::uint32_t revision{};
        (void)readCharacterTelemetry(result,revision);
        return result;
    }
    int selectedCharacterType() const noexcept;
    void selectCharacterType(int);
    unsigned characterTelemetryRevision() const noexcept { return characterTelemetryMailbox_.revision(); }
    void attachLiveDisplay() noexcept { liveDisplayCount_.fetch_add(1,std::memory_order_relaxed); }
    void detachLiveDisplay() noexcept { liveDisplayCount_.fetch_sub(1,std::memory_order_relaxed); }
    [[nodiscard]] bool readLiveOutput(p6plugin::ui::LiveAudioFrame& frame,std::uint32_t& revision) const noexcept
    { return liveOutputMailbox_.tryRead(frame,revision); }
    p6plugin::OutputReadings consumeOutputReadings() noexcept;
    void resetOutputClips() noexcept { outputClips_.store(0,std::memory_order_relaxed); }
    unsigned outputRevision() const noexcept { return outputRevision_.load(std::memory_order_relaxed); }
    int arpPlaybackStep() const noexcept { return arpPlaybackStep_.load(std::memory_order_relaxed); }
    std::array<p6::MidiMonitorEvent,2> recentMidiEvents() const noexcept { return midiRouter_.recentMidiEvents(); }
    int midiPitchBendRaw14() const noexcept { return midiRouter_.pitchBendRaw14(); }
    unsigned midiPitchBendRevision() const noexcept { return midiRouter_.pitchBendRevision(); }

private:
    struct StateUpdateGuard {
        explicit StateUpdateGuard(P6AudioProcessor&);
        ~StateUpdateGuard();
        P6AudioProcessor& owner;
        std::unique_lock<std::recursive_mutex> writer;
    };

    struct HostBinding
    {
        std::uint16_t nrpn{};
        std::atomic<float>* value{};
    };

    enum class ClockSource : int { Internal = 0, Host = 1, Midi = 2 };
    enum class CalibrationProfile : int { Default = 0, ReferenceA09 = 1 };

    static p6::P6PatchState makeInitPatch();
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout (const p6::P6PatchState& defaults);
    static juce::String hardwareParameterID (std::uint16_t nrpn);
    static int rawValueFor (const p6::P6PatchState&, std::uint16_t nrpn, int fallback = 0);

    void initialiseBindings();
    void syncHostParametersFromPatch (const p6::P6PatchState& source);
    bool applyHostParameterChangesToPatch (p6::P6PatchState& target, bool updateBindingCache,
                                           p6::P6PatchState* mirrorTarget = nullptr);
    void invalidateMidiMirrorsForUnobservedHostChanges() noexcept;
    void applyRuntimeControls (bool force = false);
    void applyHostTransport();
    void handleMidiMessage (const juce::MidiMessageMetadata& metadata);
    void timerCallback() override;
    void queueNrpnMirror (p6::NrpnUpdate update) noexcept;
    void drainPendingNrpnToCanonical (bool mirrorHost);
    void clearPendingNrpn() noexcept;
    p6::MapperCalibration selectedCalibration() const;
    int selectedOversampling() const;
    ClockSource selectedClockSource() const;

    // patch_ is owned by the host/UI side. audioPatch_ is owned by the audio
    // thread so MIDI NRPN edits never race state save/load or the editor.
    p6::P6PatchState patch_;
    p6::P6PatchState audioPatch_;
    p6::P6Engine engine_;
    p6::MapperCalibration audioCalibration_{};
    p6plugin::CharacterFx character_;
    p6plugin::OutputStage output_;
    std::atomic<float>* masterDb_{};
    std::atomic<float>* midiInputChannel_{};
    std::atomic<float>* plasmaMode_{};
    std::atomic<float>* arpCharacterEnabled_{};
    std::atomic<float>* arpCharacterMotion_{};
    std::atomic<float>* arpCharacterRhythm_{};
    std::atomic<float>* arpCharacterFreeze_{};
    std::array<std::atomic<float>*,8> arpSteps_{};
    std::atomic<int> arpPlaybackStep_{-1};
    std::atomic<std::uint32_t> arpVariationSeed_{0x735a2d97u},arpRestoreSeed_{0x735a2d97u},arpRestoreRevision_{};
    std::uint32_t appliedArpRestoreRevision_{};
    std::array<std::atomic<float>,2> outputPeak_{},outputRms_{},preMasterPeak_{};
    std::atomic<unsigned> outputClips_{},outputRevision_{};
    std::atomic<float> characterExcitation_{};
    // Single producer on the audio thread; bounded, lock-free snapshot
    // handoff to the message thread. No separate per-field atomics.
    p6plugin::RealtimeSnapshotMailbox<p6plugin::CharacterTelemetry>
        characterTelemetryMailbox_{};
    static_assert(std::atomic<unsigned>::is_always_lock_free);
    std::atomic<unsigned> liveDisplayCount_{0};
    p6plugin::RealtimeSnapshotMailbox<p6plugin::ui::LiveAudioFrame> liveOutputMailbox_{};
    p6plugin::ui::LiveAudioFrame liveOutputFrame_{};
    std::size_t liveOutputCount_{};
    std::atomic<float>* customType_{};
    std::array<std::array<std::atomic<float>*,7>,4> customParameters_{};
    std::array<std::atomic<float>*, 7> tubeParameters_{};
    std::array<std::atomic<float>*, 3> characterParameters_{};
    p6::P6PerformanceScheduler scheduler_ { engine_ };
    p6::P6MidiRouter midiRouter_{};
    juce::AudioProcessorValueTreeState apvts_;
    mutable juce::CriticalSection presetIdentityLock_; // host/UI only, never audio
    std::vector<std::uint8_t> presetBaseline_;
    juce::String activePresetId_,activePresetName_;
    std::vector<HostBinding> bindings_;
    std::array<std::atomic<int>, 1024> hostObservedRaw_{};

    // Audio-thread MIDI edits are published with a generation. Host automation
    // advances the same per-parameter generation, so an older delayed MIDI
    // mirror can never overwrite a newer DAW edit on the message thread.
    p6::VersionedEditMailbox<1024> nrpnMailbox_{};
    p6::VersionedEditMailbox<1> sequencePlayMailbox_{};
    std::atomic<int> hostSequencePlayObserved_ {0};

    // Writers may notify the host without holding the audio gate. The audio
    // thread only tries the gate and retains its previous coherent snapshot
    // while a restore/import is in progress.
    std::recursive_mutex stateWriterMutex_;
    unsigned stateUpdateDepth_{};
    std::atomic<bool> stateUpdateInProgress_{};
    juce::SpinLock stateUpdateGate_;
    mutable juce::SpinLock patchLock_;
    std::atomic<bool> fullPatchLoadPending_ { true };
    bool prepared_{};
    double sampleRate_{48000.0};

    // Cached wrapper-only controls. Hardware-program bytes remain canonical in
    // patch_; these values intentionally live outside the SysEx payload.
    int oversampling_{2};
    ClockSource clockSource_{ClockSource::Host};
    bool midiStartStop_{true};
    bool arpHold_{};
    bool arpBeatSync_{};
    p6::SlopMode slopMode_{p6::SlopMode::Standard};
    CalibrationProfile calibrationProfile_{CalibrationProfile::Default};
    std::atomic<bool> sequencePlay_ {false};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (P6AudioProcessor)
};
