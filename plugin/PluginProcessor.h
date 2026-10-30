#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "p6/P6Engine.h"
#include "p6/P6PerformanceScheduler.h"
#include "p6/P6MidiRouter.h"
#include "p6/P6PatchState.h"
#include "p6/ParameterMapper.h"
#include "p6/VersionedEditMailbox.h"

#include <array>
#include <atomic>
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

    juce::AudioProcessorValueTreeState& parameters() noexcept { return apvts_; }
    const p6::P6PatchState& canonicalPatchUnsafeForAudioThread() const noexcept { return patch_; }

private:
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
    p6::P6PerformanceScheduler scheduler_ { engine_ };
    p6::P6MidiRouter midiRouter_{};
    juce::AudioProcessorValueTreeState apvts_;
    std::vector<HostBinding> bindings_;
    std::array<std::atomic<int>, 1024> hostObservedRaw_{};

    // Audio-thread MIDI edits are published with a generation. Host automation
    // advances the same per-parameter generation, so an older delayed MIDI
    // mirror can never overwrite a newer DAW edit on the message thread.
    p6::VersionedEditMailbox<1024> nrpnMailbox_{};
    p6::VersionedEditMailbox<1> sequencePlayMailbox_{};
    std::atomic<int> hostSequencePlayObserved_ {0};

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
