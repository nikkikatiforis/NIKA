#pragma once
#include <JuceHeader.h>

//==============================================================================
// NIKAOscillator
//
// Five-source oscillator inspired by the SH-101 architecture:
//   Saw    — rising sawtooth, bandlimited via PolyBLEP
//   Square — 50% duty-cycle square, bandlimited via PolyBLEP
//   Pulse  — variable pulse width (0.05–0.95), DC-corrected, bandlimited
//   Sub    — square at one octave below the main pitch, bandlimited
//   Noise  — white noise
//
// Each source is scaled by its own level [0..1] and summed.
// Call prepare() once, setFrequency() per note, process() per sample.
//==============================================================================
class NIKAOscillator
{
public:
    void prepare (double sampleRate);
    void setFrequency (double freqHz);
    void reset();

    // Returns one mixed output sample.
    // sawLevel, squareLevel, pulseLevel, subLevel, noiseLevel: 0..1
    // pulseWidth: 0.05..0.95 (clamped internally)
    float process (float sawLevel,
                   float squareLevel,
                   float pulseLevel,  float pulseWidth,
                   float subLevel,
                   float noiseLevel);

private:
    // 2nd-order PolyBLEP residual.
    // t  = current phase in [0, 1)
    // dt = normalised phase increment (freq / sampleRate)
    static float polyBlep (double t, double dt);

    double sampleRate = 44100.0;
    double phase      = 0.0;   // main oscillator phase [0, 1)
    double subPhase   = 0.0;   // sub-oscillator phase  [0, 1)
    double dt         = 0.0;   // phase increment per sample (main)
    double subDt      = 0.0;   // phase increment per sample (sub = dt/2)

    juce::Random rng;
};
