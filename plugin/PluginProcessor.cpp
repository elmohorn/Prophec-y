#include "PluginProcessor.h"
#include "PluginStateCodec.h"
#include "PluginEditor.h"

#include "p6/NrpnCodec.h"
#include "p6/ParameterRegistry.h"
#include "p6/ProgramLayout.h"
#include "p6/SysexCodec.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>

namespace
{
constexpr std::array<std::uint16_t, 4> kSequenceControlNrpns { 168, 169, 170, 171 };
constexpr const char* kOversamplingID = "wrapper_oversampling";
constexpr const char* kClockSourceID = "wrapper_clock_source";
constexpr const char* kMidiStartStopID = "wrapper_midi_start_stop";
constexpr const char* kArpHoldID = "wrapper_arp_hold";
constexpr const char* kArpBeatSyncID = "wrapper_arp_beat_sync";
constexpr const char* kSlopModeID = "wrapper_slop_mode";
constexpr const char* kCalibrationID = "wrapper_calibration";
constexpr const char* kSequencePlayID = "runtime_sequence_play";

void setRaw (p6::P6PatchState& patch, std::uint16_t nrpn, std::uint16_t value)
{
    p6::applyRawNrpn (patch, { nrpn, value });
}

std::uint8_t clampByte (int v)
{
    return static_cast<std::uint8_t> (std::clamp (v, 0, 255));
}

constexpr std::size_t kRealtimeRawParameterCapacity = 1024;

void reserveRealtimePatchStorage (p6::P6PatchState& patch)
{
    patch.rawParameters.reserve (kRealtimeRawParameterCapacity);
    patch.name.reserve (p6::SysexCodec::kPatchNameLength);
}

void copyPatchIntoRealtimeStorage (p6::P6PatchState& dst, const p6::P6PatchState& src)
{
    // The audio copy deliberately does not need the display name. All fixed
    // state is trivially assigned; rawParameters are pushed into a capacity
    // reserved before audio starts, so this path does not grow the vector.
    dst.bank = src.bank;
    dst.program = src.program;
    dst.osc1 = src.osc1;
    dst.osc2 = src.osc2;
    dst.subLevel = src.subLevel;
    dst.noiseLevel = src.noiseLevel;
    dst.mixerDrive = src.mixerDrive;
    dst.highPass = src.highPass;
    dst.lowPass = src.lowPass;
    dst.ampEnv = src.ampEnv;
    dst.polyMod = src.polyMod;
    dst.lfo = src.lfo;
    dst.performance = src.performance;
    dst.rawPayload = src.rawPayload;
    dst.rawParameters.clear();
    for (const auto& raw : src.rawParameters)
    {
        if (dst.rawParameters.size() >= dst.rawParameters.capacity())
            break; // should be unreachable for the supported NRPN namespace
        dst.rawParameters.push_back (raw);
    }
}

} // namespace

P6AudioProcessor::P6AudioProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      patch_ (makeInitPatch()),
      audioPatch_ (patch_),
      apvts_ (*this, nullptr, "P6_PARAMETERS", createParameterLayout (patch_))
{
    reserveRealtimePatchStorage (patch_);
    reserveRealtimePatchStorage (audioPatch_);
    clearPendingNrpn();
    for (auto& value : hostObservedRaw_) value.store (0, std::memory_order_relaxed);
    initialiseBindings();
    for(std::size_t i=0;i<tubeParameters_.size();++i)
        tubeParameters_[i]=apvts_.getRawParameterValue(p6plugin::kTubeIDs[i]);
    masterDb_=apvts_.getRawParameterValue(p6plugin::kMasterOutputID);
    characterParameters_={apvts_.getRawParameterValue(p6plugin::kCharacterTypeID),
                          apvts_.getRawParameterValue(p6plugin::kCharacterPositionID),
                          apvts_.getRawParameterValue(p6plugin::kCharacterBypassID)};
    syncHostParametersFromPatch (patch_);
    hostSequencePlayObserved_.store (
        apvts_.getRawParameterValue (kSequencePlayID)->load() >= 0.5f ? 1 : 0,
        std::memory_order_relaxed);

    // Poll the lock-free MIDI->host mirror from JUCE's message thread. Using
    // Timer rather than AsyncUpdater is deliberate: JUCE documents that
    // triggerAsyncUpdate() may block when called from a realtime/audio thread.
    adoptPreset("factory-init","Init P6 Core");
    startTimerHz (30);
}

P6AudioProcessor::~P6AudioProcessor()
{
    stopTimer();
}

p6::P6PatchState P6AudioProcessor::makeInitPatch()
{
    p6::P6PatchState patch;
    patch.rawPayload.fill (0);

    // A conservative, immediately playable init patch. Only verified one-byte
    // mappings are written; all unknown payload bytes remain zero/preserved.
    const std::array<std::pair<std::uint16_t, std::uint16_t>, 34> init {{
        {0, 24}, {1, 0}, {2, 110}, {3, 127}, {4, 127},
        {5, 24}, {6, 127}, {7, 0}, {8, 127}, {9, 127}, {10, 0}, {11, 1},
        {27, 0}, {28, 0}, {29, 0}, {30, 0}, {31, 2}, {32, 0}, {33, 0},
        {45, 164}, {46, 0}, {47, 0}, {48, 0}, {49, 0}, {50, 0}, {51, 0}, {52, 0},
        {62, 110}, {63, 0}, {64, 0}, {66, 127}, {67, 0}, {68, 0}, {69, 127}
    }};
    for (const auto [nrpn, value] : init)
        setRaw (patch, nrpn, value);

    setRaw (patch, 70, 20);
    setRaw (patch, 71, 0);
    setRaw (patch, 77, 127);
    setRaw (patch, 78, 0);
    setRaw (patch, 79, 0);
    setRaw (patch, 80, 0);
    setRaw (patch, 81, 0);
    setRaw (patch, 82, 127);
    setRaw (patch, 88, 64);
    setRaw (patch, 89, 0);
    setRaw (patch, 90, 0);
    setRaw (patch, 91, 0);
    setRaw (patch, 109, 127);
    setRaw (patch, 119, 0);
    setRaw (patch, 127, 0);
    setRaw (patch, 135, 0);
    setRaw (patch, 143, 127);
    setRaw (patch, 144, 127);
    setRaw (patch, 156, 0);
    setRaw (patch, 157, 0);
    setRaw (patch, 158, 2);
    setRaw (patch, 160, 0);
    setRaw (patch, 161, 0);
    setRaw (patch, 162, 0);
    setRaw (patch, 163, 6);
    setRaw (patch, 167, 120);

    std::fill (patch.rawPayload.begin() + static_cast<std::ptrdiff_t> (p6::SysexCodec::kPatchNameOffset),
               patch.rawPayload.begin() + static_cast<std::ptrdiff_t> (p6::SysexCodec::kPatchNameOffset + p6::SysexCodec::kPatchNameLength),
               static_cast<std::uint8_t> (' '));
    constexpr char name[] = "Init P6 Core";
    std::copy_n (reinterpret_cast<const std::uint8_t*> (name), sizeof (name) - 1,
                 patch.rawPayload.begin() + static_cast<std::ptrdiff_t> (p6::SysexCodec::kPatchNameOffset));
    patch.name = name;

    p6::populateRawParametersFromPayload (patch);
    setRaw (patch, 168, 0);
    setRaw (patch, 169, 63);
    setRaw (patch, 170, 0);
    setRaw (patch, 171, 0);
    return patch;
}

juce::String P6AudioProcessor::hardwareParameterID (std::uint16_t nrpn)
{
    return "nrpn_" + juce::String (static_cast<int> (nrpn)).paddedLeft ('0', 4);
}

int P6AudioProcessor::rawValueFor (const p6::P6PatchState& patch, std::uint16_t nrpn, int fallback)
{
    for (const auto& raw : patch.rawParameters)
        if (raw.id == nrpn)
            return static_cast<int> (raw.raw);

    if (const auto* serialized = p6::findSerializedByNrpn (nrpn))
        return static_cast<int> (patch.rawPayload[serialized->sysexOffset]);

    return fallback;
}

juce::AudioProcessorValueTreeState::ParameterLayout
P6AudioProcessor::createParameterLayout (const p6::P6PatchState& defaults)
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    for (const auto& serialized : p6::serializedParameterLayout())
    {
        const auto* metadata = p6::findProgramParameter (serialized.nrpn);
        const auto name = metadata != nullptr ? juce::String (metadata->name.data(), metadata->name.size())
                                              : juce::String (serialized.name.data(), serialized.name.size());
        const auto def = std::clamp (rawValueFor (defaults, serialized.nrpn, serialized.minimum),
                                     static_cast<int> (serialized.minimum),
                                     static_cast<int> (serialized.maximum));
        layout.add (std::make_unique<juce::AudioParameterInt> (
            juce::ParameterID { hardwareParameterID (serialized.nrpn), 1 },
            name,
            static_cast<int> (serialized.minimum),
            static_cast<int> (serialized.maximum),
            def));
    }

    for (const auto nrpn : kSequenceControlNrpns)
    {
        const auto* metadata = p6::findProgramParameter (nrpn);
        if (metadata == nullptr)
            continue;
        layout.add (std::make_unique<juce::AudioParameterInt> (
            juce::ParameterID { hardwareParameterID (nrpn), 1 },
            juce::String (metadata->name.data(), metadata->name.size()),
            static_cast<int> (metadata->minimum),
            static_cast<int> (metadata->maximum),
            std::clamp (rawValueFor (defaults, nrpn, metadata->minimum),
                        static_cast<int> (metadata->minimum),
                        static_cast<int> (metadata->maximum))));
    }

    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { kOversamplingID, 1 }, "DSP Oversampling",
        juce::StringArray { "1x", "2x", "4x", "8x" }, 1));
    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { kClockSourceID, 1 }, "Clock Source",
        juce::StringArray { "Internal", "Host PPQ", "MIDI Clock" }, 1));
    layout.add (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { kMidiStartStopID, 1 }, "MIDI Start/Stop", true));
    layout.add (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { kArpHoldID, 1 }, "Arp Hold", false));
    layout.add (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { kArpBeatSyncID, 1 }, "Arp Beat Sync", false));
    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { kSlopModeID, 1 }, "Slop Model",
        juce::StringArray { "Standard", "Vintage" }, 0));
    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { kCalibrationID, 1 }, "Calibration",
        juce::StringArray { "Default", "Reference A v0.9" }, 0));
    layout.add (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { kSequencePlayID, 1 }, "Sequence Play", false));

    layout.add(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{p6plugin::kCharacterTypeID,1},"Prophec-y Module",juce::StringArray{"None","Tube"},0));
    layout.add(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{p6plugin::kCharacterPositionID,1},"Prophec-y Position",juce::StringArray{"Start","End"},1));
    layout.add(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{p6plugin::kCharacterBypassID,1},"Prophec-y Bypass",false));
    for(std::size_t i=0;i<p6plugin::kTubeIDs.size();++i)
        layout.add(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{p6plugin::kTubeIDs[i],1},juce::String("Tube ")+p6plugin::kTubeNames[i],
            juce::NormalisableRange<float>{0.f,1.f,.001f},p6plugin::kTubeDefaults[i]));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{p6plugin::kMasterOutputID,1},"Master Output",
        juce::NormalisableRange<float>{-60.f,0.f,.1f},p6plugin::kNewMasterDb));
    return layout;
}

void P6AudioProcessor::initialiseBindings()
{
    bindings_.clear();
    bindings_.reserve (p6::serializedParameterLayout().size() + kSequenceControlNrpns.size());

    auto bind = [this] (std::uint16_t nrpn)
    {
        if (auto* value = apvts_.getRawParameterValue (hardwareParameterID (nrpn)))
            bindings_.push_back ({ nrpn, value });
    };

    for (const auto& serialized : p6::serializedParameterLayout())
        bind (serialized.nrpn);
    for (const auto nrpn : kSequenceControlNrpns)
        bind (nrpn);
}

void P6AudioProcessor::syncHostParametersFromPatch (const p6::P6PatchState& source)
{
    // This function is non-realtime. setValueNotifyingHost is intentional: in
    // JUCE 9, plain setValue() does not notify APVTS's ParameterAdapter, so
    // getRawParameterValue() would otherwise remain stale after state/SysEx load.
    for (auto& binding : bindings_)
    {
        const auto* metadata = p6::findProgramParameter (binding.nrpn);
        const int minValue = metadata != nullptr ? metadata->minimum : 0;
        const int maxValue = metadata != nullptr ? metadata->maximum : 16383;
        const int raw = std::clamp (rawValueFor (source, binding.nrpn, minValue), minValue, maxValue);
        if (auto* parameter = apvts_.getParameter (hardwareParameterID (binding.nrpn)))
            parameter->setValueNotifyingHost (parameter->convertTo0to1 (static_cast<float> (raw)));
        if (binding.nrpn < hostObservedRaw_.size())
            hostObservedRaw_[binding.nrpn].store (raw, std::memory_order_release);
    }
}

bool P6AudioProcessor::applyHostParameterChangesToPatch (p6::P6PatchState& target, bool updateBindingCache,
                                                           p6::P6PatchState* mirrorTarget)
{
    bool changed = false;
    for (auto& binding : bindings_)
    {
        if (binding.value == nullptr)
            continue;
        const int current = static_cast<int> (std::lround (binding.value->load()));
        const int previous = binding.nrpn < hostObservedRaw_.size()
            ? hostObservedRaw_[binding.nrpn].load (std::memory_order_acquire) : current;
        if (current == previous)
            continue;

        // Establish causal order before applying the host value. Any delayed
        // MIDI edit with an older generation will be discarded by the UI/state
        // mailbox rather than resurfacing after this newer DAW automation.
        if (binding.nrpn < 1024)
            static_cast<void> (nrpnMailbox_.markHost (binding.nrpn));

        const p6::NrpnUpdate update { binding.nrpn, static_cast<std::uint16_t> (std::max (0, current)) };
        p6::applyRawNrpn (target, update);
        if (mirrorTarget != nullptr)
            p6::applyRawNrpn (*mirrorTarget, update);
        if (updateBindingCache && binding.nrpn < hostObservedRaw_.size())
            hostObservedRaw_[binding.nrpn].store (current, std::memory_order_release);
        changed = true;
    }
    return changed;
}

void P6AudioProcessor::invalidateMidiMirrorsForUnobservedHostChanges() noexcept
{
    // The host can change APVTS before processBlock has consumed the new value.
    // Mark that parameter as newer now so the message-thread timer cannot write
    // an older MIDI mirror back over pending host automation.
    for (const auto& binding : bindings_)
    {
        if (binding.value == nullptr || binding.nrpn >= hostObservedRaw_.size())
            continue;
        const int current = static_cast<int> (std::lround (binding.value->load()));
        const int observed = hostObservedRaw_[binding.nrpn].load (std::memory_order_acquire);
        if (current != observed)
            static_cast<void> (nrpnMailbox_.markHost (binding.nrpn));
    }

    const int currentSeq = apvts_.getRawParameterValue (kSequencePlayID)->load() >= 0.5f ? 1 : 0;
    const int observedSeq = hostSequencePlayObserved_.load (std::memory_order_acquire);
    if (currentSeq != observedSeq)
        static_cast<void> (sequencePlayMailbox_.markHost (0));
}

int P6AudioProcessor::selectedOversampling() const
{
    const auto index = static_cast<int> (std::lround (apvts_.getRawParameterValue (kOversamplingID)->load()));
    constexpr std::array<int, 4> values { 1, 2, 4, 8 };
    return values[static_cast<std::size_t> (std::clamp (index, 0, 3))];
}

P6AudioProcessor::ClockSource P6AudioProcessor::selectedClockSource() const
{
    const auto value = static_cast<int> (std::lround (apvts_.getRawParameterValue (kClockSourceID)->load()));
    return static_cast<ClockSource> (std::clamp (value, 0, 2));
}

p6::MapperCalibration P6AudioProcessor::selectedCalibration() const
{
    const auto profile = static_cast<int> (std::lround (apvts_.getRawParameterValue (kCalibrationID)->load()));
    auto calibration = profile == static_cast<int> (CalibrationProfile::ReferenceA09)
        ? p6::referenceACandidateV09()
        : p6::MapperCalibration {};
    calibration.slopMode = static_cast<int> (std::lround (apvts_.getRawParameterValue (kSlopModeID)->load())) == 0
        ? p6::SlopMode::Standard
        : p6::SlopMode::Vintage;
    return calibration;
}

void P6AudioProcessor::prepareToPlay (double sampleRate, int maximumExpectedSamplesPerBlock)
{
    output_.prepare(sampleRate,masterDb_->load(std::memory_order_relaxed));
    outputClips_.store(0,std::memory_order_relaxed);
    for(std::size_t c=0;c<2;++c){outputPeak_[c].store(0);preMasterPeak_[c].store(0);outputRms_[c].store(0);}

    juce::ignoreUnused (maximumExpectedSamplesPerBlock);
    sampleRate_ = sampleRate;
    oversampling_ = selectedOversampling();
    engine_.prepare (sampleRate_, oversampling_);
    character_.prepare(sampleRate_);
    setLatencySamples(character_.latency());
    engine_.setCharacterInsert(&character_, [](void* context,p6::StereoSample x) noexcept {
        return static_cast<p6plugin::CharacterFx*>(context)->process(x);
    },false);
    scheduler_.prepare (sampleRate_);

    const juce::SpinLock::ScopedLockType lock (patchLock_);
    reserveRealtimePatchStorage (audioPatch_);
    copyPatchIntoRealtimeStorage (audioPatch_, patch_);
    midiRouter_.reset();
    scheduler_.loadPatch (audioPatch_, selectedCalibration());
    prepared_ = true;
    fullPatchLoadPending_.store (false, std::memory_order_release);
    applyRuntimeControls (true);
}

void P6AudioProcessor::releaseResources()
{
    prepared_ = false;
}

bool P6AudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainInputChannelSet().isDisabled()
        && layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void P6AudioProcessor::applyRuntimeControls (bool force)
{
    const auto newOversampling = selectedOversampling();
    const auto newClock = selectedClockSource();
    const auto newMidiStartStop = apvts_.getRawParameterValue (kMidiStartStopID)->load() >= 0.5f;
    const auto newArpHold = apvts_.getRawParameterValue (kArpHoldID)->load() >= 0.5f;
    const auto newBeatSync = apvts_.getRawParameterValue (kArpBeatSyncID)->load() >= 0.5f;
    const auto newSlop = apvts_.getRawParameterValue (kSlopModeID)->load() < 0.5f
        ? p6::SlopMode::Standard : p6::SlopMode::Vintage;
    const auto newCalibration = static_cast<CalibrationProfile> (std::clamp (
        static_cast<int> (std::lround (apvts_.getRawParameterValue (kCalibrationID)->load())), 0, 1));
    const auto newSequencePlay = apvts_.getRawParameterValue (kSequencePlayID)->load() >= 0.5f;
    const int newSequencePlayInt = newSequencePlay ? 1 : 0;
    const int previousHostSequencePlay = hostSequencePlayObserved_.load (std::memory_order_acquire);
    const bool actualHostSequencePlayChanged = newSequencePlayInt != previousHostSequencePlay;
    const bool applySequencePlay = force || actualHostSequencePlayChanged;

    const bool qualityChanged = newOversampling != oversampling_;
    const bool mappingChanged = newSlop != slopMode_ || newCalibration != calibrationProfile_;
    const bool clockChanged = force || newClock != clockSource_;

    // JUCE parameters may be automated from the realtime thread. P6Voice::prepare()
    // resizes the decimator buffers, so changing oversampling here would allocate
    // in processBlock(). Cache the requested quality now and apply it only from
    // prepareToPlay(), where allocation/reinitialisation is explicitly allowed.
    oversampling_ = newOversampling;
    clockSource_ = newClock;
    midiStartStop_ = newMidiStartStop;
    arpHold_ = newArpHold;
    arpBeatSync_ = newBeatSync;
    slopMode_ = newSlop;
    calibrationProfile_ = newCalibration;

    if (qualityChanged && prepared_)
    {
        // Deliberately no prepare() here. The requested value is persisted and
        // becomes active at the next host prepareToPlay(). This keeps parameter
        // automation realtime-safe and avoids a hidden heap allocation.
    }

    if (mappingChanged && prepared_)
        scheduler_.updatePatch (audioPatch_, selectedCalibration());

    if (clockChanged)
    {
        switch (clockSource_)
        {
            case ClockSource::Internal: scheduler_.setClockSyncMode (p6::ClockSyncMode::Internal); break;
            case ClockSource::Host:     scheduler_.setClockSyncMode (p6::ClockSyncMode::HostPpq); break;
            case ClockSource::Midi:     scheduler_.setClockSyncMode (p6::ClockSyncMode::MidiClock); break;
        }
    }
    scheduler_.setMidiStartStopEnabled (midiStartStop_);
    scheduler_.setArpHold (arpHold_);
    scheduler_.setArpBeatSync (arpBeatSync_);
    if (applySequencePlay)
    {
        if (actualHostSequencePlayChanged)
            static_cast<void> (sequencePlayMailbox_.markHost (0));
        sequencePlay_.store (newSequencePlay, std::memory_order_release);
        scheduler_.setSequencePlaying (newSequencePlay);
        hostSequencePlayObserved_.store (newSequencePlayInt, std::memory_order_release);
    }
}

void P6AudioProcessor::applyHostTransport()
{
    if (clockSource_ != ClockSource::Host)
        return;

    if (auto* playHead = getPlayHead())
    {
        if (const auto position = playHead->getPosition())
        {
            const auto ppq = position->getPpqPosition();
            if (ppq.hasValue())
            {
                const auto bpm = position->getBpm().orFallback (scheduler_.clock().bpm());
                scheduler_.alignToHostPpq (*ppq, bpm, position->getIsPlaying());
                return;
            }
        }
    }

    // Missing host timing should not silently invent a PPQ position. Fall back
    // to the internal clock for this block while retaining the user's Host
    // preference, so notes remain playable in hosts that omit PositionInfo.
    scheduler_.setClockSyncMode (p6::ClockSyncMode::Internal);
}

void P6AudioProcessor::queueNrpnMirror (p6::NrpnUpdate update) noexcept
{
    if (update.parameter >= 1024)
        return;
    static_cast<void> (nrpnMailbox_.publishMidi (update.parameter, update.value));
}

void P6AudioProcessor::clearPendingNrpn() noexcept
{
    nrpnMailbox_.clear();
    sequencePlayMailbox_.clear();
}

void P6AudioProcessor::drainPendingNrpnToCanonical (bool mirrorHost)
{
    // A host edit may be visible in APVTS before the audio callback has applied
    // it. Invalidate older MIDI mirrors first so delayed UI work cannot win.
    invalidateMidiMirrorsForUnobservedHostChanges();

    {
        const juce::SpinLock::ScopedLockType lock (patchLock_);
        for (std::size_t i = 0; i < 1024; ++i)
        {
            const auto edit = nrpnMailbox_.takeCanonicalIfLatest (i);
            if (! edit || ! nrpnMailbox_.isLatest (i, edit->generation))
                continue;
            p6::applyRawNrpn (patch_, { static_cast<std::uint16_t> (i), edit->value });
        }
    }

    if (! mirrorHost)
        return;

    // Re-check after canonical state work; host automation can move while this
    // message-thread callback is running.
    invalidateMidiMirrorsForUnobservedHostChanges();

    // This runs on JUCE's message thread via Timer, never in the audio
    // callback. Mirror only the latest causally valid hardware-style MIDI edit.
    for (std::size_t i = 0; i < 1024; ++i)
    {
        const auto edit = nrpnMailbox_.takeMirrorIfLatest (i);
        if (! edit || ! nrpnMailbox_.isLatest (i, edit->generation))
            continue;
        if (auto* p = apvts_.getParameter (hardwareParameterID (static_cast<std::uint16_t> (i))))
        {
            p->setValueNotifyingHost (p->convertTo0to1 (static_cast<float> (edit->value)));
            hostObservedRaw_[i].store (edit->value, std::memory_order_release);
        }
    }
}

void P6AudioProcessor::timerCallback()
{
    // Message-thread-only host/UI mirror. The audio callback only performs
    // atomic stores into the versioned mailbox and never posts OS messages.
    drainPendingNrpnToCanonical (true);

    const int currentHostSeq = apvts_.getRawParameterValue (kSequencePlayID)->load() >= 0.5f ? 1 : 0;
    const int observedSeq = hostSequencePlayObserved_.load (std::memory_order_acquire);
    if (currentHostSeq != observedSeq)
        static_cast<void> (sequencePlayMailbox_.markHost (0));

    // Runtime sequence play is already applied sample-accurately to the audio
    // scheduler. The canonical side has no program byte, so consume that queue
    // solely to keep the mailbox bounded and mirror only the host/UI side.
    static_cast<void> (sequencePlayMailbox_.takeCanonicalIfLatest (0));
    const auto seqPlay = sequencePlayMailbox_.takeMirrorIfLatest (0);
    if (seqPlay && sequencePlayMailbox_.isLatest (0, seqPlay->generation))
    {
        if (auto* p = apvts_.getParameter (kSequencePlayID))
            p->setValueNotifyingHost (p->convertTo0to1 (static_cast<float> (seqPlay->value != 0)));
        hostSequencePlayObserved_.store (seqPlay->value != 0 ? 1 : 0, std::memory_order_release);
    }
}

void P6AudioProcessor::handleMidiMessage (const juce::MidiMessageMetadata& metadata)
{
    if (metadata.numBytes <= 0 || metadata.data == nullptr)
        return;

    const auto result = midiRouter_.process (
        std::span<const std::uint8_t> (metadata.data, static_cast<std::size_t> (metadata.numBytes)),
        audioPatch_, scheduler_, selectedCalibration());

    if (result.nrpnUpdate && result.nrpnUpdate->parameter < 1024)
        queueNrpnMirror (*result.nrpnUpdate);
    if (result.sequencePlay)
    {
        const auto value = static_cast<std::uint16_t> (*result.sequencePlay ? 1 : 0);
        sequencePlay_.store (*result.sequencePlay, std::memory_order_release);
        static_cast<void> (sequencePlayMailbox_.publishMidi (0, value));
    }

    // A complete Prophet-6 SysEx delivered by the host is recognised by the
    // JUCE-independent router, but v0.17 still defers decoding to the explicit
    // non-realtime loadSysex() API. A fixed RT->UI SysEx queue remains a later
    // control-contract item; never allocate/parse a 1176/1178-byte dump here.
}

void P6AudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();
    output_.setDecibels(masterDb_->load(std::memory_order_relaxed));output_.beginBlock();

    // Never block the realtime thread on preset/state edits. If another thread
    // owns patchLock_, this block simply renders the last coherent patch.
    if (patchLock_.tryEnter())
    {
        const bool fullLoad = fullPatchLoadPending_.exchange (false, std::memory_order_acq_rel);
        const bool hardwareChanged = applyHostParameterChangesToPatch (patch_, true, &audioPatch_);

        if (fullLoad)
        {
            copyPatchIntoRealtimeStorage (audioPatch_, patch_);
            midiRouter_.reset();
            scheduler_.loadPatch (audioPatch_, selectedCalibration());
        }
        else if (hardwareChanged)
        {
            // applyHostParameterChangesToPatch() already mirrored each changed
            // NRPN into audioPatch_. Do not replace the whole audio patch here:
            // doing so could erase a newer sample-accurate MIDI NRPN that has
            // not yet been mirrored back to canonical patch_ by the UI timer.
            scheduler_.updatePatch (audioPatch_, selectedCalibration());
        }

        applyRuntimeControls (false);
        patchLock_.exit();
    }

    applyHostTransport();
    p6plugin::CharacterState characterState;
    characterState.type=static_cast<int>(characterParameters_[0]->load());
    characterState.position=static_cast<int>(characterParameters_[1]->load());
    characterState.bypass=characterParameters_[2]->load()>=.5f;
    for(std::size_t i=0;i<tubeParameters_.size();++i) characterState.tube[i]=tubeParameters_[i]->load();
    character_.setState(characterState);
    engine_.setCharacterInsert(&character_, [](void* context,p6::StereoSample x) noexcept {
        return static_cast<p6plugin::CharacterFx*>(context)->process(x);
    },character_.beforeEffects());

    auto event = midi.cbegin();
    const auto end = midi.cend();
    const int numSamples = buffer.getNumSamples();
    auto* left = buffer.getWritePointer (0);
    auto* right = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1) : left;

    for (int sample = 0; sample < numSamples; ++sample)
    {
        while (event != end && (*event).samplePosition <= sample)
        {
            if ((*event).samplePosition == sample)
                handleMidiMessage (*event);
            ++event;
        }

        if(character_.advanceRoute())
            engine_.setCharacterInsert(&character_, [](void* context,p6::StereoSample x) noexcept {
                return static_cast<p6plugin::CharacterFx*>(context)->process(x);
            },character_.beforeEffects());
        const auto out = scheduler_.process();
        const auto final=output_.process(static_cast<float>(out.left)*character_.routeGain(),static_cast<float>(out.right)*character_.routeGain());
        left[sample]=final[0];right[sample]=final[1];
    }
    const auto readings=output_.readings();
    for(std::size_t c=0;c<2;++c) {
        auto retainMax=[](std::atomic<float>& slot,float value){auto old=slot.load(std::memory_order_relaxed);while(old<value&&!slot.compare_exchange_weak(old,value,std::memory_order_relaxed)){};};
        retainMax(outputPeak_[c],readings.peak[c]);retainMax(preMasterPeak_[c],readings.prePeak[c]);
        outputRms_[c].store(readings.rms[c],std::memory_order_relaxed);
    }
    outputClips_.fetch_or(readings.clips,std::memory_order_relaxed);outputRevision_.fetch_add(1,std::memory_order_relaxed);
    characterExcitation_.store(character_.excitation(),std::memory_order_relaxed);
    const auto visual=character_.telemetry();
    characterInputRms_.store(visual.inputRms,std::memory_order_relaxed);
    characterOutputRms_.store(visual.outputRms,std::memory_order_relaxed);
    characterChangeRms_.store(visual.changeRms,std::memory_order_relaxed);
    characterActive_.store(visual.active,std::memory_order_relaxed);
    characterTelemetryRevision_.fetch_add(1,std::memory_order_relaxed);
}

p6plugin::OutputReadings P6AudioProcessor::consumeOutputReadings() noexcept
{
    p6plugin::OutputReadings r;
    for(std::size_t c=0;c<2;++c){r.peak[c]=outputPeak_[c].exchange(0,std::memory_order_relaxed);r.prePeak[c]=preMasterPeak_[c].exchange(0,std::memory_order_relaxed);r.rms[c]=outputRms_[c].load(std::memory_order_relaxed);}
    r.clips=outputClips_.load(std::memory_order_relaxed);return r;
}

juce::AudioProcessorEditor* P6AudioProcessor::createEditor()
{
    return new P6PluginEditor(*this);
}

const juce::String P6AudioProcessor::getProgramName (int)
{
    const juce::SpinLock::ScopedLockType lock (patchLock_);
    return patch_.name;
}

void P6AudioProcessor::changeProgramName (int, const juce::String& newName)
{
    const juce::SpinLock::ScopedLockType lock (patchLock_);
    auto ascii = newName.substring (0, static_cast<int> (p6::SysexCodec::kPatchNameLength));
    for (std::size_t i = 0; i < p6::SysexCodec::kPatchNameLength; ++i)
    {
        const auto code = i < static_cast<std::size_t> (ascii.length())
            ? static_cast<std::uint16_t> (std::clamp (static_cast<int> (ascii[static_cast<int> (i)]), 32, 125))
            : static_cast<std::uint16_t> (' ');
        p6::applyRawNrpn (patch_, { static_cast<std::uint16_t> (236 + i), code });
    }
}

void P6AudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    // State save may occur before the message-thread mirror callback. Flush any
    // causally valid pending MIDI NRPN edits first so the saved payload is current.
    drainPendingNrpnToCanonical (false);
    p6plugin::WrapperState state;
    {
        const juce::SpinLock::ScopedLockType lock (patchLock_);
        state.patch = patch_;
        // Capture automation/editor changes even if the transport is stopped and
        // processBlock() has not yet consumed them. Untouched out-of-range raw
        // bytes remain byte-exact because only values differing from the cached
        // host view are applied.
        applyHostParameterChangesToPatch (state.patch, false);
    }

    for (std::size_t i = 0; i < kSequenceControlNrpns.size(); ++i)
        state.sequenceControls[i] = static_cast<std::uint16_t> (
            rawValueFor (state.patch, kSequenceControlNrpns[i], i == 1 ? 63 : 0));

    const auto os = selectedOversampling();
    state.extras = {
        clampByte (os == 1 ? 0 : os == 2 ? 1 : os == 4 ? 2 : 3),
        clampByte (static_cast<int> (selectedClockSource())),
        static_cast<std::uint8_t> (apvts_.getRawParameterValue (kMidiStartStopID)->load() >= 0.5f),
        static_cast<std::uint8_t> (apvts_.getRawParameterValue (kArpHoldID)->load() >= 0.5f),
        static_cast<std::uint8_t> (apvts_.getRawParameterValue (kArpBeatSyncID)->load() >= 0.5f),
        static_cast<std::uint8_t> (apvts_.getRawParameterValue (kSlopModeID)->load() >= 0.5f),
        clampByte (static_cast<int> (std::lround (apvts_.getRawParameterValue (kCalibrationID)->load()))),
        static_cast<std::uint8_t> ([this]
        {
            const bool hostValue = apvts_.getRawParameterValue (kSequencePlayID)->load() >= 0.5f;
            const int observed = hostSequencePlayObserved_.load (std::memory_order_acquire);
            // If the host value moved since the audio thread last observed it,
            // prefer that pending automation. Otherwise preserve the latest
            // sample-accurate MIDI NRPN 1088 runtime state.
            return (hostValue ? 1 : 0) != observed
                ? hostValue
                : sequencePlay_.load (std::memory_order_acquire);
        }())
    };

    state.character.type=static_cast<int>(characterParameters_[0]->load());
    state.character.position=static_cast<int>(characterParameters_[1]->load());
    state.character.bypass=characterParameters_[2]->load()>=.5f;
    for(std::size_t i=0;i<tubeParameters_.size();++i) state.character.tube[i]=tubeParameters_[i]->load();
    state.masterOutputDb=masterDb_->load(std::memory_order_relaxed);
    const auto bytes = p6plugin::encodeState (state);
    destData.replaceAll (bytes.data(), bytes.size());
}

void P6AudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (data == nullptr || sizeInBytes <= 0)
        return;

    p6plugin::WrapperState state;
    if (! p6plugin::decodeState (
            std::span<const std::uint8_t> (static_cast<const std::uint8_t*> (data),
                                           static_cast<std::size_t> (sizeInBytes)), state))
        return;

    clearPendingNrpn();
    p6::P6PatchState hostSnapshot;
    {
        const juce::SpinLock::ScopedLockType lock (patchLock_);
        patch_ = std::move (state.patch);
        hostSnapshot = patch_;
    }
    syncHostParametersFromPatch (hostSnapshot);

    auto setActual = [this] (const char* id, int actual)
    {
        if (auto* p = apvts_.getParameter (id))
            p->setValueNotifyingHost (p->convertTo0to1 (static_cast<float> (actual)));
    };
    setActual (kOversamplingID, std::clamp<int> (state.extras[0], 0, 3));
    setActual (kClockSourceID, std::clamp<int> (state.extras[1], 0, 2));
    setActual (kMidiStartStopID, state.extras[2] != 0);
    setActual (kArpHoldID, state.extras[3] != 0);
    setActual (kArpBeatSyncID, state.extras[4] != 0);
    setActual (kSlopModeID, state.extras[5] != 0);
    setActual (kCalibrationID, std::clamp<int> (state.extras[6], 0, 1));
    setActual (kSequencePlayID, state.extras[7] != 0);
    setActual(p6plugin::kCharacterTypeID,state.character.type);
    setActual(p6plugin::kCharacterPositionID,state.character.position);
    setActual(p6plugin::kCharacterBypassID,state.character.bypass?1:0);
    for(std::size_t i=0;i<p6plugin::kTubeIDs.size();++i)
        if(auto* p=apvts_.getParameter(p6plugin::kTubeIDs[i])) p->setValueNotifyingHost(state.character.tube[i]);
    if(auto* master=apvts_.getParameter(p6plugin::kMasterOutputID))
        master->setValueNotifyingHost(master->convertTo0to1(state.masterOutputDb));
    sequencePlay_.store (state.extras[7] != 0, std::memory_order_release);
    hostSequencePlayObserved_.store (state.extras[7] != 0 ? 1 : 0, std::memory_order_release);

    fullPatchLoadPending_.store (true, std::memory_order_release);
    adoptPreset({},getProgramName(0));
}

p6plugin::WrapperState P6AudioProcessor::presetSnapshot()
{
    juce::MemoryBlock bytes;getStateInformation(bytes);p6plugin::WrapperState state;
    p6plugin::decodeState({static_cast<const std::uint8_t*>(bytes.getData()),bytes.getSize()},state);return state;
}
p6plugin::WrapperState P6AudioProcessor::factoryInitPreset()
{
    p6plugin::WrapperState state;state.patch=makeInitPatch();state.sequenceControls={0,63,0,0};return state;
}
void P6AudioProcessor::adoptPreset(const juce::String& id,const juce::String& name,const p6plugin::WrapperState* baseline)
{
    const auto bytes=p6plugin::musicalPresetBytes(baseline?*baseline:presetSnapshot());
    const juce::ScopedLock lock(presetIdentityLock_);presetBaseline_=bytes;activePresetId_=id;activePresetName_=name;
}
bool P6AudioProcessor::presetDirty()
{
    const auto bytes=p6plugin::musicalPresetBytes(presetSnapshot());
    const juce::ScopedLock lock(presetIdentityLock_);return bytes!=presetBaseline_;
}
juce::String P6AudioProcessor::activePresetId() const {const juce::ScopedLock lock(presetIdentityLock_);return activePresetId_;}
juce::String P6AudioProcessor::activePresetName() const {const juce::ScopedLock lock(presetIdentityLock_);return activePresetName_;}
void P6AudioProcessor::loadMusicalPreset(const p6plugin::WrapperState& source,const juce::String& id,const juce::String& name)
{
    auto current=presetSnapshot();current.patch=source.patch;current.sequenceControls=source.sequenceControls;current.character=source.character;
    const auto bytes=p6plugin::encodeState(current);setStateInformation(bytes.data(),static_cast<int>(bytes.size()));adoptPreset(id,name,&source);
}

bool P6AudioProcessor::loadSysex (std::span<const std::uint8_t> bytes, juce::String& error)
{
    const auto decoded = p6::SysexCodec::decode (bytes);
    if (! decoded.ok)
    {
        error = decoded.error;
        return false;
    }

    clearPendingNrpn();
    p6::P6PatchState hostSnapshot;
    {
        const juce::SpinLock::ScopedLockType lock (patchLock_);
        patch_ = decoded.patch;
        // NRPN-only sequence controls do not have verified payload offsets. Give
        // them conservative defaults without modifying any unknown SysEx bytes.
        setRaw (patch_, 168, 0);
        setRaw (patch_, 169, 63);
        setRaw (patch_, 170, 0);
        setRaw (patch_, 171, 0);
        hostSnapshot = patch_;
    }
    syncHostParametersFromPatch (hostSnapshot);
    fullPatchLoadPending_.store (true, std::memory_order_release);
    error.clear();
    return true;
}

std::vector<std::uint8_t> P6AudioProcessor::exportEditBuffer()
{
    drainPendingNrpnToCanonical (false);
    const juce::SpinLock::ScopedLockType lock (patchLock_);
    auto snapshot = patch_;
    applyHostParameterChangesToPatch (snapshot, false);
    return p6::SysexCodec::encodeEditBuffer (snapshot);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new P6AudioProcessor();
}

