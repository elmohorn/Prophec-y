#include "p6/P6Engine.h"
#include "p6/P6EffectMapper.h"

#include <algorithm>
#include <cmath>

namespace p6 {

void P6Engine::prepare(double sampleRate, int oversampleFactor)
{
    sampleRate_ = std::max(8000.0, sampleRate);
    oversampleFactor_ = (oversampleFactor == 1 || oversampleFactor == 2 || oversampleFactor == 4 || oversampleFactor == 8) ? oversampleFactor : 2;
    for (std::size_t i = 0; i < voices_.size(); ++i) {
        voices_[i].voice.prepare(sampleRate_, static_cast<std::uint32_t>(0x9e3779b9u * (i + 1)), oversampleFactor_);
        voices_[i].voice.setDiagnostics(diagnostics_);
        voices_[i].voice.setPitchBendSemitones(0.0);
        voices_[i].voice.setParams(paramsForVoice(i));
        voices_[i].note = -1;
        voices_[i].keyDown = false;
        voices_[i].sustainHeld = false;
        voices_[i].age = 0;
        voices_[i].owner = 0;
    }
    lfo_.prepare(sampleRate_);
    effects_.prepare(sampleRate_);
    clock_ = 0;
    pressOrder_ = 0;
    heldCount_ = 0;
    distDcL_ = {};
    distDcR_ = {};
    channelPressure_ = 0.0;
    modWheel_ = 0.0;
    pitchBend_ = 0.0;
    midiVolume_ = 1.0;
    sustainPedal_ = false;
    unisonSustainHeld_ = false;
    constexpr double tau = 6.28318530717958647692;
    dcBlockA_ = std::exp(-tau * 12.0 / sampleRate_);
}

void P6Engine::setVoiceParams(const VoiceParams& params)
{
    params_ = params;
    for (std::size_t i = 0; i < voices_.size(); ++i)
        voices_[i].voice.setParams(paramsForVoice(i));
}

void P6Engine::setVoiceDiagnostics(const VoiceDiagnostics& diagnostics)
{
    diagnostics_ = diagnostics;
    for (auto& slot : voices_)
        slot.voice.setDiagnostics(diagnostics_);
}

void P6Engine::loadPatch(const P6PatchState& patch, const MapperCalibration& calibration)
{
    applyPatch(patch, calibration, true);
}

void P6Engine::updatePatch(const P6PatchState& patch, const MapperCalibration& calibration)
{
    applyPatch(patch, calibration, false);
}

void P6Engine::applyPatch(const P6PatchState& patch,
                          const MapperCalibration& calibration,
                          bool newProgramLoad)
{
    const auto mapped = mapPatchToDSP(patch, calibration);
    params_ = mapped.voice;
    panSpread_ = std::clamp(mapped.panSpread, 0.0, 1.0);
    programVolume_ = std::clamp(mapped.programVolume, 0.0, 1.0);
    distortion_ = std::clamp(mapped.distortion, 0.0, 1.0);
    effects_.setSettings(mapEffectsFromPatch(patch));
    slopAmount_ = std::clamp(mapped.slopAmount, 0.0, 1.0);
    slopMode_ = mapped.slopMode;
    keyMode_ = std::min<std::uint8_t>(mapped.keyMode, 5);
    pitchBendRangeSemitones_ = std::clamp(mapped.pitchBendRangeSemitones, 0.0, 12.0);
    chordMode_ = mapped.chordMode;
    // Chord-memory bytes have not yet been independently identified in the
    // 1024-byte program payload. A true program load must not carry a runtime
    // capture into the new program, but ordinary host automation/editor refresh
    // must preserve it.
    if (newProgramLoad)
        chordLength_ = 0;

    lfo_.setRate(mapped.lfo.rateHz);
    lfo_.setShape(static_cast<P6LFO::Shape>(mapped.lfo.shape));
    lfo_.setNoiseMode(mapped.lfo.noiseMode);
    lfoInitialAmount_ = std::clamp(mapped.lfo.initialAmount, 0.0, 1.0);
    lfoClockSync_ = mapped.lfo.clockSync;

    const std::size_t voices = mapped.chordMode
        ? kVoiceCount
        : (mapped.unisonMode < 6 ? static_cast<std::size_t>(mapped.unisonMode) + 1 : kVoiceCount);

    // No invented hidden detune on hardware patches. Slop/Vintage and the
    // free-running VCO phases provide the spread.
    setUnison(mapped.unison, 0.0, voices);

    for (std::size_t i = 0; i < voices_.size(); ++i) {
        voices_[i].voice.setParams(paramsForVoice(i));
        voices_[i].voice.setPitchBendSemitones(pitchBendSemitones());
    }
}

void P6Engine::setPanSpread(double amount01)
{
    panSpread_ = std::clamp(amount01, 0.0, 1.0);
}

void P6Engine::setProgramVolume(double amount01)
{
    programVolume_ = std::clamp(amount01, 0.0, 1.0);
}

void P6Engine::setMidiVolume(double amount01)
{
    midiVolume_ = std::clamp(amount01, 0.0, 1.0);
}

void P6Engine::setDistortion(double amount01)
{
    distortion_ = std::clamp(amount01, 0.0, 1.0);
}

void P6Engine::setSlop(double amount01, SlopMode mode)
{
    slopAmount_ = std::clamp(amount01, 0.0, 1.0);
    slopMode_ = mode;
    for (std::size_t i = 0; i < voices_.size(); ++i)
        voices_[i].voice.setParams(paramsForVoice(i));
}

void P6Engine::setChannelPressure(double pressure01)
{
    channelPressure_ = std::clamp(pressure01, 0.0, 1.0);
}

void P6Engine::setModWheel(double amount01)
{
    modWheel_ = std::clamp(amount01, 0.0, 1.0);
}

void P6Engine::setPitchBend(double bipolar)
{
    pitchBend_ = std::clamp(bipolar, -1.0, 1.0);
    const double semitones = pitchBendSemitones();
    for (auto& slot : voices_)
        slot.voice.setPitchBendSemitones(semitones);
}

void P6Engine::setSustainPedal(bool down)
{
    if (sustainPedal_ == down)
        return;

    sustainPedal_ = down;
    if (down)
        return;

    if (unison_) {
        if (unisonSustainHeld_ && heldCount_ == 0)
            releaseUnison();
        unisonSustainHeld_ = false;
        return;
    }

    for (auto& slot : voices_) {
        if (!slot.sustainHeld)
            continue;
        slot.sustainHeld = false;
        if (!slot.keyDown)
            slot.voice.noteOff();
    }
}

double P6Engine::effectiveLfoDepth() const noexcept
{
    double depth = lfoInitialAmount_ + modWheel_ * (1.0 - lfoInitialAmount_);
    if (params_.aftertouch.lfoAmount)
        depth += channelPressure_ * params_.aftertouch.amount;
    return std::clamp(depth, -1.0, 1.0);
}

void P6Engine::setKeyMode(std::uint8_t mode)
{
    keyMode_ = std::min<std::uint8_t>(mode, 5);
    if (unison_ && heldCount_ > 0)
        retargetUnison(false);
}

void P6Engine::setChordMode(bool enabled)
{
    chordMode_ = enabled;
    if (enabled) {
        unison_ = true;
        unisonVoiceCount_ = kVoiceCount;
    }
    if (unison_ && heldCount_ > 0)
        retargetUnison(false);
}

bool P6Engine::captureChordMemoryFromHeld()
{
    if (heldCount_ == 0)
        return false;

    // The documented clear gesture is a one-note capture with Unison off.
    // Treat a one-note capture as clearing the stored voicing rather than as
    // a redundant one-note chord memory.
    if (heldCount_ == 1) {
        clearChordMemory();
        return true;
    }

    std::array<int, kMaxHeldKeys> notes{};
    const std::size_t count = std::min<std::size_t>(heldCount_, kVoiceCount);
    for (std::size_t i = 0; i < heldCount_; ++i)
        notes[i] = held_[i].note;
    std::sort(notes.begin(), notes.begin() + static_cast<std::ptrdiff_t>(heldCount_));
    const int base = notes[0];
    chordLength_ = count;
    for (std::size_t i = 0; i < count; ++i)
        chord_[i] = notes[i] - base;
    chordMode_ = true;
    unison_ = true;
    unisonVoiceCount_ = kVoiceCount;
    retargetUnison(false);
    return true;
}

void P6Engine::clearChordMemory()
{
    chord_.fill(0);
    chordLength_ = 0;
    chordMode_ = false;
}

void P6Engine::setUnison(bool enabled, double detuneCents, std::size_t voiceCount)
{
    unison_ = enabled;
    unisonDetuneCents_ = std::clamp(detuneCents, 0.0, 50.0);
    unisonVoiceCount_ = std::clamp<std::size_t>(voiceCount, 1, kVoiceCount);
    if (!unison_)
        unisonSustainHeld_ = false;
    for (std::size_t i = 0; i < voices_.size(); ++i)
        voices_[i].voice.setParams(paramsForVoice(i));
}

P6Engine::Slot& P6Engine::allocateVoice()
{
    for (auto& s : voices_)
        if (!s.voice.active())
            return s;

    Slot* best = nullptr;
    for (auto& s : voices_) {
        if (!s.keyDown && (!best || s.age < best->age))
            best = &s;
    }
    if (best)
        return *best;

    return *std::min_element(voices_.begin(), voices_.end(),
        [](const Slot& a, const Slot& b) { return a.age < b.age; });
}

void P6Engine::rememberHeld(int midiNote, double velocity01)
{
    midiNote = std::clamp(midiNote, 0, 127);
    // Repeated Note On moves the key to the end of last-note priority.
    forgetHeld(midiNote);
    if (heldCount_ >= held_.size()) {
        for (std::size_t i = 1; i < heldCount_; ++i)
            held_[i - 1] = held_[i];
        --heldCount_;
    }
    held_[heldCount_++] = HeldKey{midiNote, std::clamp(velocity01, 0.0, 1.0), ++pressOrder_};
}

void P6Engine::forgetHeld(int midiNote)
{
    for (std::size_t i = 0; i < heldCount_; ++i) {
        if (held_[i].note != midiNote)
            continue;
        for (std::size_t j = i + 1; j < heldCount_; ++j)
            held_[j - 1] = held_[j];
        --heldCount_;
        return;
    }
}

const P6Engine::HeldKey* P6Engine::winningHeldKey() const noexcept
{
    if (heldCount_ == 0)
        return nullptr;

    const unsigned priority = keyMode_ % 3u;
    const HeldKey* winner = &held_[0];
    for (std::size_t i = 1; i < heldCount_; ++i) {
        const auto& k = held_[i];
        if ((priority == 0 && k.note < winner->note) ||
            (priority == 1 && k.note > winner->note) ||
            (priority == 2 && k.order > winner->order))
            winner = &k;
    }
    return winner;
}

void P6Engine::releaseUnison()
{
    for (auto& s : voices_) {
        s.sustainHeld = false;
        if (s.voice.active()) {
            s.keyDown = false;
            s.owner = 0;
            s.voice.noteOff();
        }
    }
    unisonSustainHeld_ = false;
}

void P6Engine::retargetUnison(bool retrigger)
{
    const auto* winner = winningHeldKey();
    if (!winner) {
        releaseUnison();
        return;
    }

    unisonSustainHeld_ = false;
    std::array<int, kVoiceCount> notes{};
    std::size_t noteCount = 1;
    notes[0] = winner->note;

    if (chordMode_ && chordLength_ > 0) {
        noteCount = chordLength_;
        const int anchor = (keyMode_ % 3u) == 1u ? chord_[chordLength_ - 1] : 0;
        for (std::size_t i = 0; i < noteCount; ++i)
            notes[i] = std::clamp(winner->note + chord_[i] - anchor, 0, 127);
    }

    const std::size_t stack = chordMode_ ? kVoiceCount : unisonVoiceCount_;
    for (std::size_t i = 0; i < stack; ++i) {
        auto& s = voices_[i];
        s.voice.setParams(paramsForVoice(i));
        s.voice.setPitchBendSemitones(pitchBendSemitones());
        s.note = notes[i % noteCount];
        s.keyDown = true;
        s.sustainHeld = false;
        s.age = clock_;
        s.voice.noteOn(s.note, winner->velocity, retrigger, retrigger);
    }
    for (std::size_t i = stack; i < voices_.size(); ++i) {
        auto& s = voices_[i];
        s.sustainHeld = false;
        if (s.voice.active()) {
            s.keyDown = false;
            s.owner = 0;
            s.voice.noteOff();
        }
    }
}

VoiceParams P6Engine::paramsForVoice(std::size_t index) const
{
    auto p = params_;
    static constexpr std::array<double, kVoiceCount> positions{-1.0, -0.6, -0.2, 0.2, 0.6, 1.0};

    if (unison_ && index < unisonVoiceCount_ && unisonDetuneCents_ > 0.0) {
        const double cents = positions[index] * unisonDetuneCents_;
        p.osc1.fineCents += cents;
        p.osc2.fineCents += cents;
    }

    if (slopAmount_ > 0.0) {
        const double t2 = slopAmount_ * slopAmount_;
        static constexpr std::array<double, kVoiceCount> pitch{-0.82, 0.31, 0.95, -0.24, 0.57, -0.63};
        static constexpr std::array<double, kVoiceCount> filt{-0.75, 0.42, -0.18, 0.91, -0.48, 0.11};
        static constexpr std::array<double, kVoiceCount> env{0.93, 1.06, 0.97, 1.08, 0.94, 1.03};
        static constexpr std::array<double, kVoiceCount> gain{-0.55, 0.20, 0.75, -0.15, 0.42, -0.68};

        if (slopMode_ == SlopMode::Standard) {
            p.slopDepthCents = 100.0 * t2;
        } else {
            p.slopDepthCents = 35.0 * t2;
            p.osc1.fineCents += pitch[index] * 6.0 * slopAmount_;
            p.osc2.fineCents -= pitch[(index + 3) % kVoiceCount] * 5.0 * slopAmount_;
            p.osc1.shape = std::clamp(p.osc1.shape + filt[index] * 0.012 * slopAmount_, 0.0, 1.0);
            p.osc2.shape = std::clamp(p.osc2.shape - filt[index] * 0.010 * slopAmount_, 0.0, 1.0);
            p.lowPass.cutoffControl += filt[index] * 2.0 * slopAmount_;
            p.highPass.cutoffControl += filt[(index + 2) % kVoiceCount] * 1.5 * slopAmount_;
            const double envScale = 1.0 + (env[index] - 1.0) * slopAmount_;
            p.ampAttack *= envScale;
            p.ampDecay /= envScale;
            p.ampRelease *= envScale;
            p.filterAttack /= envScale;
            p.filterDecay *= envScale;
            p.filterRelease /= envScale;
            p.voiceGain *= 1.0 + gain[index] * 0.05 * slopAmount_;
        }
    }
    return p;
}

void P6Engine::noteOn(int midiNote, double velocity01)
{
    noteOnOwned(midiNote, velocity01, 0);
}

void P6Engine::noteOff(int midiNote)
{
    noteOffOwned(midiNote, 0);
}

void P6Engine::noteOnOwned(int midiNote, double velocity01, std::uint32_t owner)
{
    midiNote = std::clamp(midiNote, 0, 127);
    velocity01 = std::clamp(velocity01, 0.0, 1.0);

    if (unison_) {
        // The hardware's Unison path is one keyboard stack rather than six
        // independently owned note slots. Arp/sequence callers ensure only the
        // intended generated note is active before retargeting this path.
        unisonSustainHeld_ = false;
        for (auto& slot : voices_)
            slot.sustainHeld = false;
        rememberHeld(midiNote, velocity01);
        ++clock_;
        retargetUnison(keyMode_ >= 3);
        return;
    }

    // Chord-memory capture is based on physically held live keys, not on
    // whether Unison happens to be enabled. Generated arp/sequencer notes use
    // non-zero owners and must not enter this capture set.
    if (owner == 0)
        rememberHeld(midiNote, velocity01);

    ++clock_;
    auto& slot = allocateVoice();
    const auto index = static_cast<std::size_t>(&slot - voices_.data());
    slot.voice.setParams(paramsForVoice(index));
    slot.voice.setPitchBendSemitones(pitchBendSemitones());
    slot.note = midiNote;
    slot.keyDown = true;
    slot.sustainHeld = false;
    slot.age = clock_;
    slot.owner = owner;
    slot.voice.noteOn(midiNote, velocity01);
}

void P6Engine::noteOffOwned(int midiNote, std::uint32_t owner)
{
    if (unison_) {
        forgetHeld(midiNote);
        if (heldCount_ > 0) {
            retargetUnison(false);
        } else if (owner == 0 && sustainPedal_) {
            unisonSustainHeld_ = true;
            for (auto& slot : voices_) {
                if (slot.voice.active()) {
                    slot.keyDown = false;
                    slot.sustainHeld = true;
                }
            }
        } else {
            releaseUnison();
        }
        return;
    }

    if (owner == 0)
        forgetHeld(midiNote);

    for (auto& slot : voices_) {
        if (slot.keyDown && slot.note == midiNote && slot.owner == owner) {
            slot.keyDown = false;
            if (owner == 0 && sustainPedal_) {
                slot.sustainHeld = true;
            } else {
                slot.sustainHeld = false;
                slot.voice.noteOff();
            }
        }
    }
}

void P6Engine::allNotesOff()
{
    heldCount_ = 0;
    unisonSustainHeld_ = false;
    for (auto& s : voices_) {
        s.sustainHeld = false;
        if (s.voice.active()) {
            s.keyDown = false;
            s.owner = 0;
            s.voice.noteOff();
        }
    }
}

double P6Engine::panPosition(std::size_t index, double spread)
{
    static constexpr std::array<double, kVoiceCount> positions{-1.0, 0.6, -0.2, 0.2, -0.6, 1.0};
    return positions[index] * spread;
}

double P6Engine::applyCandidateDistortion(double x, DcBlockState& state) const
{
    if (distortion_ <= 0.0)
        return x;

    constexpr double knee = 20.0;
    constexpr double rail = 2.0;
    constexpr double asymmetry = 0.65;
    const double k = distortion_ * knee;
    const double bias = x < 0.0 ? asymmetry : 1.0;
    const double y = x * (1.0 + k) / (1.0 + k * bias * std::abs(x) / rail);

    const double hp = y - state.x1 + dcBlockA_ * state.y1;
    state.x1 = y;
    state.y1 = hp;
    return hp;
}

StereoSample P6Engine::process()
{
    StereoSample out;
    constexpr double pi = 3.14159265358979323846;

    // The wheel adds modulation on top of Initial Amount, but its useful travel
    // is the remaining range so full wheel always reaches 100%.
    const double lfoSignal = lfo_.process() * effectiveLfoDepth();

    for (std::size_t i = 0; i < voices_.size(); ++i) {
        auto& s = voices_[i];
        const auto voiceSample = s.voice.process(lfoSignal, channelPressure_);
        const double sample = voiceSample.dry;
        if (sample == 0.0)
            continue;

        const double pan = panPosition(i, panSpread_);
        const double angle = (pan + 1.0) * (pi * 0.25);
        out.left += sample * std::cos(angle);
        out.right += sample * std::sin(angle);
    }

    const double performanceGain = programVolume_ * midiVolume_;
    out.left = applyCandidateDistortion(out.left * performanceGain, distDcL_);
    out.right = applyCandidateDistortion(out.right * performanceGain, distDcR_);

    // Prophet-6 digital effects are a serial global path: Effect A feeds
    // Effect B. The single P6Effects instance owns that order and its true
    // bypass behavior; no scheduler/wrapper effect path exists in parallel.
    const auto effected = effects_.process(out.left, out.right);
    out.left = effected.left;
    out.right = effected.right;
    return out;
}

std::size_t P6Engine::activeVoiceCount() const noexcept
{
    std::size_t n = 0;
    for (const auto& s : voices_)
        if (s.voice.active())
            ++n;
    return n;
}

std::size_t P6Engine::sustainedVoiceCount() const noexcept
{
    std::size_t n = 0;
    for (const auto& s : voices_)
        if (s.sustainHeld)
            ++n;
    return n;
}

std::array<int, P6Engine::kVoiceCount> P6Engine::soundingNotes() const noexcept
{
    std::array<int, kVoiceCount> notes{};
    notes.fill(-1);
    for (std::size_t i = 0; i < voices_.size(); ++i)
        if (voices_[i].voice.active())
            notes[i] = voices_[i].note;
    return notes;
}

} // namespace p6
