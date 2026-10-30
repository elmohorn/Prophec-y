#include "p6/P6Voice.h"

#include <algorithm>
#include <cmath>

namespace p6 {

void P6Voice::prepare(double sampleRate, std::uint32_t seed, int oversampleFactor)
{
    hostSampleRate_ = std::max(8000.0, sampleRate);
    oversampleFactor_ = sanitizedOversampleFactor(oversampleFactor);
    sampleRate_ = hostSampleRate_ * static_cast<double>(oversampleFactor_);
    rng_ = seed ? seed : 1u;

    initialPhase1_ = phaseFromSeed(rng_ ^ 0x9e3779b9u);
    initialPhase2_ = phaseFromSeed(rng_ ^ 0x85ebca6bu);
    initialSubPhase_ = phaseFromSeed(rng_ ^ 0xc2b2ae35u);

    osc1_.prepare(sampleRate_);
    osc2_.prepare(sampleRate_);
    sub_.prepare(sampleRate_);
    slop1_.prepare(sampleRate_, rng_ ^ 0xa341316cu);
    slop2_.prepare(sampleRate_, rng_ ^ 0xc8013ea4u);
    ampEnv_.prepare(sampleRate_);
    filterEnv_.prepare(sampleRate_);
    lowPass_.prepare(sampleRate_);
    highPass_.prepare(sampleRate_);
    configureDecimator();
    setParams(params_);
    reset();
}

void P6Voice::setParams(const VoiceParams& params)
{
    params_ = params;
    ampEnv_.setTimes(params_.ampAttack, params_.ampDecay, params_.ampRelease);
    ampEnv_.setSustain(params_.ampSustain);
    filterEnv_.setTimes(params_.filterAttack, params_.filterDecay, params_.filterRelease);
    filterEnv_.setSustain(params_.filterSustain);
    osc1_.setSyncResetPhase(params_.syncResetPhase);
    osc1_.setHardSyncCorrectionMode(params_.syncCorrectionMode);
}

void P6Voice::noteOn(int midiNote, double velocity01,
                     bool retriggerEnvelope, bool resetEnvelopeFromZero)
{
    // Prophet-6 glide is per voice. Fixed-rate modes take the same time per
    // octave; fixed-time modes take the same total time for any interval. The
    // two "A" modes glide only while the previous envelope is still alive.
    // In Unison's non-retrigger key modes a new winning key retunes the stack
    // without restarting either envelope.
    const bool legato = active_ || !ampEnv_.isIdle();
    midiNote_ = midiNote;
    targetNote_ = static_cast<double>(midiNote);
    const bool glide = params_.glideOn &&
        ((params_.glideMode & 1u) ? legato : pitched_);
    if (!pitched_ || !glide) {
        glideNote_ = targetNote_;
        pitched_ = true;
    }
    const double seconds = std::max(params_.glideSeconds, 1.0e-6);
    glideStep_ = params_.glideMode >= 2
        ? std::abs(targetNote_ - glideNote_) / (seconds * sampleRate_)
        : 12.0 / (seconds * sampleRate_);
    baseHz_ = midiToHz(glideNote_ + pitchBendSemitones_);
    velocity_ = std::clamp(velocity01, 0.0, 1.0);

    if (!legato || retriggerEnvelope) {
        if (resetEnvelopeFromZero && legato) {
            ampEnv_.noteOnFromZero();
            filterEnv_.noteOnFromZero();
        } else {
            ampEnv_.noteOn();
            filterEnv_.noteOn();
        }
        lowPass_.start(params_.lowPass.resonance);
    }
    active_ = true;
}

void P6Voice::noteOff()
{
    ampEnv_.noteOff();
    filterEnv_.noteOff();
}

void P6Voice::reset()
{
    osc1_.reset(initialPhase1_);
    osc2_.reset(initialPhase2_);
    sub_.reset(initialSubPhase_);
    slop1_.reset();
    slop2_.reset();
    ampEnv_.reset(0.0);
    filterEnv_.reset(0.0);
    lowPass_.reset();
    highPass_.reset();
    active_ = false;
    pitched_ = false;
    glideNote_ = targetNote_ = static_cast<double>(midiNote_);
    glideStep_ = 0.0;
    std::fill(decimatorHistory_.begin(), decimatorHistory_.end(), 0.0);
    decimatorWrite_ = 0;
}

double P6Voice::midiToHz(double note)
{
    return 440.0 * std::pow(2.0, (note - 69.0) / 12.0);
}

double P6Voice::phaseFromSeed(std::uint32_t seed)
{
    seed ^= seed >> 16;
    seed *= 0x7feb352du;
    seed ^= seed >> 15;
    seed *= 0x846ca68bu;
    seed ^= seed >> 16;
    return static_cast<double>(seed & 0x00ffffffu) / static_cast<double>(0x01000000u);
}

double P6Voice::filterCutoffHz(const VoiceFilterParams& filter, bool highPass,
                               double env, double polySource,
                               double lfoSemitones,
                               double pressureSemitones) const
{
    const double keyOct = filter.keyTrack * (static_cast<double>(midiNote_) - 60.0) / 12.0;
    const double velScale = filter.velocity ? velocity_ : 1.0;
    const double envOct = env * filter.envAmountOctaves * velScale;
    const double pmOct = polySource * filter.polyModAmountOctaves;
    const double lawBaseHz = highPass && params_.highPassFilterBaseHz > 0.0
        ? params_.highPassFilterBaseHz : params_.filterBaseHz;
    const double lawSlope = highPass && params_.highPassFilterSemitonesPerRaw > 0.0
        ? params_.highPassFilterSemitonesPerRaw : params_.filterSemitonesPerRaw;
    double base = 0.0;
    // HPF and LPF have independent calibration laws. A convex HPF curve is
    // useful because the first part of the hardware control can remain near
    // bypass while the top still reaches well into the audible band.
    if (highPass && params_.highPassFilterCurveExponent > 0.0 &&
        params_.highPassFilterTopHz > lawBaseHz) {
        const double control = std::clamp(filter.cutoffControl, 0.0, 164.0) / 164.0;
        const double t = std::pow(control, params_.highPassFilterCurveExponent);
        base = lawBaseHz * std::pow(params_.highPassFilterTopHz / lawBaseHz, t);
    } else if (!highPass && params_.filterCurveExponent > 0.0 && params_.filterTopHz > lawBaseHz) {
        const double control = std::clamp(filter.cutoffControl, 0.0, 164.0) / 164.0;
        const double t = std::pow(control, params_.filterCurveExponent);
        base = lawBaseHz *
               std::pow(params_.filterTopHz / lawBaseHz, t);
    } else if (!highPass && params_.filterHighSemitonesPerRaw > 0.0) {
        const double control = std::clamp(filter.cutoffControl, 0.0, 164.0);
        const double knee = std::clamp(params_.filterKneeRaw, 0.0, 164.0);
        const double lowPart = std::min(control, knee) * lawSlope;
        const double highPart = std::max(0.0, control - knee) * params_.filterHighSemitonesPerRaw;
        base = lawBaseHz * std::pow(2.0, (lowPart + highPart) / 12.0);
    } else {
        base = lawBaseHz *
               std::pow(2.0, filter.cutoffControl * lawSlope / 12.0);
    }
    const double hz = base * std::pow(2.0, keyOct + envOct + pmOct +
                                      (lfoSemitones + pressureSemitones) / 12.0);
    return std::clamp(hz, 5.0, sampleRate_ * 0.45);
}

double P6Voice::noise()
{
    rng_ ^= rng_ << 13;
    rng_ ^= rng_ >> 17;
    rng_ ^= rng_ << 5;
    const double u = static_cast<double>(rng_) / 4294967295.0;
    return u * 2.0 - 1.0;
}

int P6Voice::sanitizedOversampleFactor(int factor)
{
    return factor == 1 || factor == 2 || factor == 4 || factor == 8 ? factor : 2;
}

void P6Voice::configureDecimator()
{
    if (oversampleFactor_ == 1) {
        decimatorTaps_ = {1.0};
        decimatorHistory_ = {0.0};
        decimatorWrite_ = 0;
        return;
    }

    if (oversampleFactor_ == 2) {
        // Preserve the v0.9 clean-room 31-tap response bit-for-bit so 2x
        // remains a valid historical baseline while higher factors are purely
        // diagnostic experiments.
        decimatorTaps_ = {
            -0.0045008481327892709, -3.9951677602018973e-06, 0.0070884249415108184,
             2.7819696099880195e-07, -0.012457607023340873, -7.5118250422239079e-06,
             0.020566233912965352, 5.6543610983244625e-06, -0.033089432751335759,
            -1.1055962468726037e-05, 0.054406576637674564, 4.3321895465555246e-06,
            -0.10059227382572718, -9.3020689158754156e-06, 0.31758521603022044,
             0.502030620974806, 0.31758521603022044, -9.3020689158754156e-06,
            -0.10059227382572718, 4.3321895465555246e-06, 0.054406576637674564,
            -1.1055962468726037e-05, -0.033089432751335759, 5.6543610983244625e-06,
             0.020566233912965352, -7.5118250422239079e-06, -0.012457607023340873,
             2.7819696099880195e-07, 0.0070884249415108184, -3.9951677602018973e-06,
            -0.0045008481327892709
        };
    } else {
        // Diagnostic 4x/8x decimator: long Blackman-windowed sinc, with the
        // cutoff kept just inside the host Nyquist. It intentionally favors
        // rejection quality over realtime CPU because its job is to answer
        // whether hard-sync/Poly-Mod residuals are oversampling-sensitive.
        const std::size_t taps = static_cast<std::size_t>(oversampleFactor_ * 32 + 1);
        const double fc = 0.45 / static_cast<double>(oversampleFactor_);
        const double pi = 3.14159265358979323846;
        const double mid = static_cast<double>(taps - 1) * 0.5;
        decimatorTaps_.resize(taps);
        double sum = 0.0;
        for (std::size_t n = 0; n < taps; ++n) {
            const double x = static_cast<double>(n) - mid;
            const double sinc = std::abs(x) < 1.0e-12
                ? 2.0 * fc
                : std::sin(2.0 * pi * fc * x) / (pi * x);
            const double phase = 2.0 * pi * static_cast<double>(n) / static_cast<double>(taps - 1);
            const double window = 0.42 - 0.5 * std::cos(phase) + 0.08 * std::cos(2.0 * phase);
            decimatorTaps_[n] = sinc * window;
            sum += decimatorTaps_[n];
        }
        if (std::abs(sum) > 1.0e-12)
            for (auto& c : decimatorTaps_) c /= sum;
    }

    decimatorHistory_.assign(decimatorTaps_.size(), 0.0);
    decimatorWrite_ = 0;
}

VoiceSample P6Voice::process(double lfoSignal, double pressure01)
{
    if (oversampleFactor_ == 1)
        return processInternal(lfoSignal, pressure01);

    VoiceSample last{};
    for (int os = 0; os < oversampleFactor_; ++os) {
        last = processInternal(lfoSignal, pressure01);
        decimatorHistory_[decimatorWrite_] = last.dry;
        decimatorWrite_ = (decimatorWrite_ + 1) % decimatorHistory_.size();
    }

    double y = 0.0;
    for (std::size_t k = 0; k < decimatorTaps_.size(); ++k) {
        const std::size_t index = (decimatorWrite_ + decimatorHistory_.size() - 1 - k) % decimatorHistory_.size();
        y += decimatorTaps_[k] * decimatorHistory_[index];
    }
    last.dry = y;
    return last;
}

VoiceSample P6Voice::processInternal(double lfoSignal, double pressure01)
{
    VoiceSample out;

    // The digitally controlled VCOs are still analog oscillators: their phase
    // does not restart at note-on. We therefore advance the oscillator/slop
    // state on every engine sample, even while the VCA envelope is idle.
    out.filterEnvelope = filterEnv_.process();
    out.ampEnvelope = ampEnv_.process();

    if (params_.glideOn && pitched_) {
        const double remaining = targetNote_ - glideNote_;
        if (std::abs(remaining) <= glideStep_ || glideStep_ <= 0.0)
            glideNote_ = targetNote_;
        else
            glideNote_ += std::copysign(glideStep_, remaining);
    } else if (pitched_) {
        glideNote_ = targetNote_;
    }
    baseHz_ = midiToHz(glideNote_ + pitchBendSemitones_);

    out.slop1Cents = slop1_.process() * params_.slopDepthCents;
    out.slop2Cents = slop2_.process() * params_.slopDepthCents;

    const double osc2Reference = params_.osc2KeyTrack
        ? baseHz_
        : midiToHz(60.0 + pitchBendSemitones_);
    const double pressure = std::clamp(pressure01, 0.0, 1.0) * params_.aftertouch.amount;
    const double at2Semi = params_.aftertouch.osc2Freq ? pressure * params_.aftertouch.pitchRangeSemitones : 0.0;
    const double lfo2Semi = params_.lfo.osc2Freq ? lfoSignal * params_.lfo.pitchRangeSemitones : 0.0;
    double f2 = osc2Reference * std::pow(2.0,
        (params_.osc2.semitones + lfo2Semi + at2Semi) / 12.0 +
        (params_.osc2.fineCents + out.slop2Cents) / 1200.0);
    if (params_.osc2LowFrequency)
        f2 /= 64.0; // isolated calibration constant pending hardware capture
    f2 = std::max(0.01, f2);

    osc2_.setFrequency(f2);
    osc2_.setShape(params_.osc2.shape);
    osc2_.setMeanNormalizationAmount(params_.osc2.meanNormalizationAmount);
    const double lfoPw = params_.lfo.pulseWidth ? lfoSignal * params_.lfo.pulseWidthRange : 0.0;
    osc2_.setPulseWidth(std::clamp(params_.osc2.pulseWidth + lfoPw, 0.04, 0.96));
    const bool syncEnabled = params_.sync || diagnostics_.forceSync;
    const auto syncAt = syncEnabled ? osc2_.nextPhaseCrossingFraction(params_.syncTriggerPhase) : std::nullopt;
    const double b = osc2_.process();
    out.osc2 = b;

    // Prophet Poly Mod sums its two bipolar sources per voice, then applies
    // that combined audio-rate signal to the enabled destinations.
    const double rawPolySource = out.filterEnvelope * params_.polyMod.filterEnvAmount +
                                 b * params_.polyMod.osc2Amount;
    out.polyModSource = rawPolySource;
    const double polySource = diagnostics_.bypassPolyMod ? 0.0 : rawPolySource;

    const double at1Semi = params_.aftertouch.osc1Freq ? pressure * params_.aftertouch.pitchRangeSemitones : 0.0;
    const double lfo1Semi = params_.lfo.osc1Freq ? lfoSignal * params_.lfo.pitchRangeSemitones : 0.0;
    const double f1Base = baseHz_ * std::pow(2.0,
        (params_.osc1.semitones + lfo1Semi + at1Semi) / 12.0 +
        (params_.osc1.fineCents + out.slop1Cents) / 1200.0);
    const double f1 = std::max(0.01,
        f1Base * std::pow(2.0, polySource * params_.polyMod.osc1FreqAmount));

    osc1_.setFrequency(f1);
    osc1_.setShape(std::clamp(params_.osc1.shape +
                              polySource * params_.polyMod.osc1ShapeAmount, 0.0, 1.0));
    osc1_.setMeanNormalizationAmount(params_.osc1.meanNormalizationAmount);
    osc1_.setPulseWidth(std::clamp(params_.osc1.pulseWidth + lfoPw +
                                   polySource * params_.polyMod.osc1PulseWidthAmount, 0.04, 0.96));
    const double a = osc1_.process(syncAt);

    // Triangle sub follows Osc1 pitch, including audio-rate pitch modulation
    // and slow slop, then divides it by two.
    sub_.setFrequency(std::max(0.01, f1 * 0.5));
    sub_.setShape(0.0);
    sub_.setPulseWidth(0.5);
    const double sub = sub_.process();

    const double noiseSample = noise();
    switch (diagnostics_.mix) {
    case VoiceDiagnosticMix::OscillatorsOnly:
        out.preFilter = a * params_.osc1.level +
                        b * params_.osc2.level * params_.osc2MixerPolarity;
        break;
    case VoiceDiagnosticMix::Osc1Only:
        out.preFilter = a;
        break;
    case VoiceDiagnosticMix::Osc2Only:
        out.preFilter = b;
        break;
    case VoiceDiagnosticMix::Full:
    default:
        out.preFilter = a * params_.osc1.level +
                        b * params_.osc2.level * params_.osc2MixerPolarity +
                        sub * params_.subLevel +
                        noiseSample * params_.noiseLevel;
        break;
    }

    const double hpLfoSemi = params_.lfo.highPass ? lfoSignal * params_.lfo.filterRangeSemitones : 0.0;
    const double lpLfoSemi = params_.lfo.lowPass ? lfoSignal * params_.lfo.filterRangeSemitones : 0.0;
    const double hpAtSemi = params_.aftertouch.highPass ? pressure * params_.aftertouch.filterRangeSemitones : 0.0;
    const double lpAtSemi = params_.aftertouch.lowPass ? pressure * params_.aftertouch.filterRangeSemitones : 0.0;
    const double hpHz = filterCutoffHz(params_.highPass, true, out.filterEnvelope, polySource, hpLfoSemi, hpAtSemi);
    const double lpHz = filterCutoffHz(params_.lowPass, false, out.filterEnvelope, polySource, lpLfoSemi, lpAtSemi);

    // Hardware-style serial path: mixer -> resonant HPF -> resonant LPF -> VCA.
    // Diagnostic filter bypass keeps oscillator/sync experiments from being
    // confounded by an imperfect cutoff or resonance calibration.
    if (diagnostics_.bypassFilters) {
        out.highPass = out.preFilter;
        out.lowPass = out.preFilter;
    } else {
        out.highPass = highPass_.process(out.preFilter, hpHz, params_.highPass.resonance);
        out.lowPass = lowPass_.process(out.highPass, lpHz, params_.lowPass.resonance, params_.filterDrive);
    }

    const double velocityGain = params_.ampVelocity ? velocity_ : 1.0;
    double vca = out.ampEnvelope * params_.ampEnvAmount * velocityGain;
    if (params_.lfo.amp)
        vca += lfoSignal;
    if (params_.aftertouch.amp)
        vca += pressure;
    vca = std::clamp(vca, 0.0, 1.0);
    out.dry = active_ ? out.lowPass * vca * params_.voiceGain : 0.0;

    if (active_ && ampEnv_.isIdle())
        active_ = false;
    return out;
}

} // namespace p6
