#pragma once

#include "p6/P6Effects.h"
#include "p6/P6Voice.h"
#include "p6/P6LFO.h"
#include "p6/P6PatchState.h"
#include "p6/ParameterMapper.h"

#include <array>
#include <cstdint>
#include <cstddef>

namespace p6 {

struct StereoSample {
    double left{};
    double right{};
};

class P6Engine {
public:
    static constexpr std::size_t kVoiceCount = 6;

    void prepare(double sampleRate, int oversampleFactor = 2);
    void setVoiceParams(const VoiceParams& params);
    void setVoiceDiagnostics(const VoiceDiagnostics& diagnostics);
    void loadPatch(const P6PatchState& patch,
                   const MapperCalibration& calibration = {});
    void updatePatch(const P6PatchState& patch,
                     const MapperCalibration& calibration = {});
    void setPanSpread(double amount01);
    void setProgramVolume(double amount01);
    void setDistortion(double amount01);
    void setSlop(double amount01, SlopMode mode);
    void setChannelPressure(double pressure01);
    void setModWheel(double amount01);
    void setPitchBend(double bipolar);
    void setSustainPedal(bool down);
    void setMidiVolume(double amount01);
    // Scheduler/clock owns tempo. The engine only forwards that physical BPM
    // into the global effects path for synced delays.
    void setEffectsRuntimeBpm(double bpm) noexcept { effects_.setRuntimeBpm(bpm); }
    void setKeyMode(std::uint8_t mode);
    void setChordMode(bool enabled);
    bool captureChordMemoryFromHeld();
    void clearChordMemory();

    void resetSyncedLfoPhase() noexcept { if (lfoClockSync_) lfo_.reset(); }

    void setUnison(bool enabled, double detuneCents = 0.0,
                   std::size_t voiceCount = kVoiceCount);

    void noteOn(int midiNote, double velocity01 = 1.0);
    void noteOff(int midiNote);
    void noteOnOwned(int midiNote, double velocity01, std::uint32_t owner);
    void noteOffOwned(int midiNote, std::uint32_t owner);
    void allNotesOff();
    void midiAllNotesOff();
    void midiAllSoundOff();
    StereoSample process();

    // Non-owning optional global insert. The stream owner sets it while stopped
    // or from its own audio thread; the callback must be allocation/lock free.
    using InsertCallback = StereoSample (*)(void*, StereoSample) noexcept;
    void setCharacterInsert(void* context, InsertCallback callback, bool beforeEffects) noexcept
    { insertContext_ = context; insertCallback_ = callback; insertBeforeEffects_ = beforeEffects; }

    std::size_t activeVoiceCount() const noexcept;
    std::size_t unisonVoiceCount() const noexcept { return unisonVoiceCount_; }
    std::uint8_t keyMode() const noexcept { return keyMode_; }
    double pitchBendRangeSemitones() const noexcept { return pitchBendRangeSemitones_; }
    double pitchBendSemitones() const noexcept { return pitchBend_ * pitchBendRangeSemitones_; }
    double modWheel() const noexcept { return modWheel_; }
    double midiVolume() const noexcept { return midiVolume_; }
    bool sustainPedal() const noexcept { return sustainPedal_; }
    std::size_t sustainedVoiceCount() const noexcept;
    double effectiveLfoDepth() const noexcept;
    double lfoPhaseForDiagnostics() const noexcept { return lfo_.phaseForDiagnostics(); }
    const P6EffectsSettings& effectsSettingsForDiagnostics() const noexcept { return effects_.settings(); }
    double effectsRuntimeBpmForDiagnostics() const noexcept { return effects_.runtimeBpm(); }
    bool chordMode() const noexcept { return chordMode_; }
    bool unisonEnabled() const noexcept { return unison_; }
    std::size_t chordMemorySize() const noexcept { return chordLength_; }
    int chordInterval(std::size_t index) const noexcept { return index < chordLength_ ? chord_[index] : 0; }
    std::array<int, kVoiceCount> soundingNotes() const noexcept;

private:
    struct Slot {
        P6Voice voice;
        int note{-1};
        bool keyDown{};
        bool sustainHeld{};
        std::uint64_t age{};
        std::uint32_t owner{};
    };

    struct DcBlockState {
        double x1{};
        double y1{};
    };

    struct HeldKey {
        int note{-1};
        double velocity{1.0};
        std::uint64_t order{};
    };

    Slot& allocateVoice();
    void rememberHeld(int midiNote, double velocity01);
    void forgetHeld(int midiNote);
    const HeldKey* winningHeldKey() const noexcept;
    void retargetUnison(bool retrigger, bool resetEnvelopeFromZero = true);
    void releaseUnison();
    VoiceParams paramsForVoice(std::size_t index) const;
    static double panPosition(std::size_t index, double spread);
    void updatePanGains() noexcept;
    double applyCandidateDistortion(double x, DcBlockState& state) const;
    void applyPatch(const P6PatchState& patch,
                    const MapperCalibration& calibration,
                    bool newProgramLoad);

    std::array<Slot, kVoiceCount> voices_{};
    VoiceParams params_{};
    double sampleRate_{48000.0};
    int oversampleFactor_{2};
    VoiceDiagnostics diagnostics_{};
    double panSpread_{};
    std::array<double, kVoiceCount> panGainLeft_{};
    std::array<double, kVoiceCount> panGainRight_{};
    double programVolume_{1.0};
    double midiVolume_{1.0};
    double distortion_{};
    P6Effects effects_{};
    void* insertContext_{};
    InsertCallback insertCallback_{};
    bool insertBeforeEffects_{};
    P6LFO lfo_{};
    double lfoInitialAmount_{};
    bool lfoClockSync_{};
    double channelPressure_{};
    double modWheel_{};
    double pitchBend_{};
    double pitchBendRangeSemitones_{};
    double slopAmount_{};
    SlopMode slopMode_{SlopMode::Standard};
    bool sustainPedal_{};
    bool unisonSustainHeld_{};
    bool unison_{};
    std::size_t unisonVoiceCount_{kVoiceCount};
    double unisonDetuneCents_{};
    std::uint8_t keyMode_{};
    bool chordMode_{};
    std::array<int, kVoiceCount> chord_{};
    std::size_t chordLength_{};
    static constexpr std::size_t kMaxHeldKeys = 16;
    std::array<HeldKey, kMaxHeldKeys> held_{};
    std::size_t heldCount_{};
    std::uint64_t pressOrder_{};
    std::uint64_t clock_{};
    double dcBlockA_{0.995};
    double voiceDcBlockA_{0.999};
    DcBlockState voiceDcL_{};
    DcBlockState voiceDcR_{};
    mutable DcBlockState distDcL_{};
    mutable DcBlockState distDcR_{};
};

} // namespace p6
