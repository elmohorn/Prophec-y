#pragma once

namespace p6 {

// Deliberately interfaces only. A generic JUCE ladder is NOT used here because
// the P6 low-pass and high-pass are calibration-critical parts of the model.
class HighPass2Pole {
public:
    virtual ~HighPass2Pole() = default;
    virtual void prepare(double sampleRate) = 0;
    virtual void reset() = 0;
    virtual double process(double input, double cutoffHz, double resonance) = 0;
};

class LowPass4Pole {
public:
    virtual ~LowPass4Pole() = default;
    virtual void prepare(double sampleRate) = 0;
    virtual void reset() = 0;
    virtual double process(double input, double cutoffHz, double resonance, double drive) = 0;
};

} // namespace p6
