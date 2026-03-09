#pragma once
#include <JuceHeader.h>

//==============================================================================
// NIKALadderFilter
//
// 4-pole (24 dB/oct) resonant lowpass filter modelled after the Huovilainen
// (2004/2006) improved nonlinear Moog ladder algorithm.
//
// Topology
//   – Four cascaded 1-pole OTA stages with tanh saturation
//   – Cross-coupling between adjacent stages (Huovilainen's key addition)
//   – Resonance feedback path with tanh input clipping
//   – 2× internal oversampling: algorithm runs at 2× output sample rate,
//     upsampled via linear interpolation, downsampled by decimation
//
// Self-oscillation
//   At resonance → 1.0 the loop gain (k = 4.0) reaches the theoretical
//   oscillation point. tanh saturation bounds the amplitude, producing a
//   clean sine at the cutoff frequency rather than a runaway output.
//
// Usage
//   call prepare()       once on playback start
//   call setParameters() once per block (or whenever params change)
//   call process()       once per output sample
//   call reset()         on transport stop / note reset
//==============================================================================
class NIKALadderFilter
{
public:
    void  prepare       (double sampleRate);
    void  setParameters (float cutoffHz, float resonance);   // resonance [0..1]
    float process       (float x);
    void  reset();

private:
    // One integration step at the 2× oversampled rate.
    void tick (double x);

    double sampleRate = 44100.0;

    // Stage outputs (double precision avoids accumulation error)
    double y1 = 0.0, y2 = 0.0, y3 = 0.0, y4 = 0.0;

    // Cross-coupling state: previous input to each stage (Huovilainen §3)
    double xi = 0.0, y1i = 0.0, y2i = 0.0, y3i = 0.0;

    // Last input sample — used to produce a linearly interpolated midpoint
    // for the 2× oversampling upsampler.
    double prevIn = 0.0;

    // Filter coefficients (recomputed in setParameters)
    double f = 0.0;   // integration coefficient ≈ 2π·fc (normalised, scaled)
    double k = 0.0;   // resonance feedback gain; k = 4.0 → self-oscillation
};
