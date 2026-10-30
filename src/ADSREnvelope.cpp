#include "p6/ADSREnvelope.h"

#include <algorithm>
#include <cmath>

namespace p6 {
namespace {
constexpr double kAttackAim = 1.581976706869326; // 1/(1-e^-1)
constexpr double kEnvConstants = 3.5;
constexpr double kUndershoot = 0.0311448698550066; // e^-3.5/(1-e^-3.5)
}

void ADSREnvelope::prepare(double sampleRate)
{
    sampleRate_ = std::max(8000.0, sampleRate);
    refreshCoefficients();
}

void ADSREnvelope::reset(double value)
{
    value_ = std::clamp(value, 0.0, 1.0);
    aim_ = value_;
    stage_ = value_ > 0.0 ? Stage::Sustain : Stage::Idle;
}

void ADSREnvelope::setTimes(double attackSeconds, double decaySeconds, double releaseSeconds)
{
    attackSeconds_ = std::max(0.0, attackSeconds);
    decaySeconds_ = std::max(0.0, decaySeconds);
    releaseSeconds_ = std::max(0.0, releaseSeconds);
    refreshCoefficients();
}

void ADSREnvelope::setSustain(double sustain01)
{
    sustain_ = std::clamp(sustain01, 0.0, 1.0);
}

double ADSREnvelope::segmentRate(double seconds, double constants, double sampleRate)
{
    if (seconds <= 0.0)
        return 1.0;
    return std::min(1.0, 1.0 - std::exp(-constants / (seconds * sampleRate)));
}

void ADSREnvelope::refreshCoefficients()
{
    attackRate_ = segmentRate(attackSeconds_, 1.0, sampleRate_);
    decayRate_ = segmentRate(decaySeconds_, kEnvConstants, sampleRate_);
    releaseRate_ = segmentRate(releaseSeconds_, kEnvConstants, sampleRate_);
}

void ADSREnvelope::noteOn()
{
    stage_ = Stage::Attack;
    aim_ = kAttackAim;
}

void ADSREnvelope::noteOnFromZero()
{
    value_ = 0.0;
    noteOn();
}

void ADSREnvelope::noteOff()
{
    if (stage_ != Stage::Idle) {
        stage_ = Stage::Release;
        aim_ = -kUndershoot * value_;
    }
}

void ADSREnvelope::enterDecay()
{
    stage_ = Stage::Decay;
    aim_ = sustain_ - kUndershoot * (value_ - sustain_);
}

void ADSREnvelope::enterSustain()
{
    value_ = sustain_;
    stage_ = sustain_ <= 0.0 ? Stage::Idle : Stage::Sustain;
}

double ADSREnvelope::process()
{
    switch (stage_) {
    case Stage::Idle:
        value_ = 0.0;
        break;

    case Stage::Attack:
        value_ += attackRate_ * (aim_ - value_);
        if (value_ >= 1.0) {
            value_ = 1.0;
            enterDecay();
            if (value_ <= sustain_)
                enterSustain();
        }
        break;

    case Stage::Decay:
        value_ += decayRate_ * (aim_ - value_);
        if (value_ <= sustain_)
            enterSustain();
        break;

    case Stage::Sustain:
        value_ = sustain_;
        break;

    case Stage::Release:
        value_ += releaseRate_ * (aim_ - value_);
        if (value_ <= 0.0) {
            value_ = 0.0;
            stage_ = Stage::Idle;
        }
        break;
    }
    return value_;
}

} // namespace p6
