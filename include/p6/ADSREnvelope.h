#pragma once

namespace p6 {

// Exponential ADSR with segment durations defined by explicit RC-like targets.
// The curve/timing model is adapted from the MIT-licensed Prophet-6 envelope
// in joshjetson/phosphor; see THIRD_PARTY_NOTICES.md.
class ADSREnvelope {
public:
    enum class Stage { Idle, Attack, Decay, Sustain, Release };

    void prepare(double sampleRate);
    void reset(double value = 0.0);
    void setTimes(double attackSeconds, double decaySeconds, double releaseSeconds);
    void setSustain(double sustain01);
    void noteOn();             // retrigger from current level
    void noteOnFromZero();     // useful for P6 retrigger key modes
    void noteOff();
    double process();

    double value() const noexcept { return value_; }
    Stage stage() const noexcept { return stage_; }
    bool isIdle() const noexcept { return stage_ == Stage::Idle; }

private:
    static double segmentRate(double seconds, double constants, double sampleRate);
    void refreshCoefficients();
    void enterDecay();
    void enterSustain();

    double sampleRate_{48000.0};
    double attackSeconds_{0.01};
    double decaySeconds_{0.2};
    double releaseSeconds_{0.4};
    double sustain_{0.7};
    double value_{};
    double aim_{};
    double attackRate_{};
    double decayRate_{};
    double releaseRate_{};
    Stage stage_{Stage::Idle};
};

} // namespace p6
