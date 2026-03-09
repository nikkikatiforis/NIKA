#pragma once
#include <JuceHeader.h>

//==============================================================================
// NIKACompressor — Fairchild 670-inspired program-dependent VCA compressor.
//
// Character
//   Threshold -16 dB, ratio 1.618:1 (golden ratio φ), narrow soft knee (4 dB).
//   Intended as a "glue" stage on the output bus — it never clamps, it just
//   holds the programme together.
//
// Program-dependent attack (Fairchild 670 behaviour)
//   Attack time interpolates between 8 ms (fast transients) and 32 ms
//   (sustained material) based on real-time transient content.  A fast envelope
//   follower (≈5 ms) is compared against the slow RMS level; when the fast
//   envelope exceeds the RMS by a significant margin the signal is transient-
//   rich and attack tightens toward 8 ms.  On sustained programme material the
//   ratio collapses to 1:1 and attack relaxes to 32 ms, imparting the gentle
//   "breathing" character of the original hardware.
//
// Program-dependent release
//   The release time constant is interpolated between a fast value (70 ms)
//   and a slow value (250 ms) based on how much gain reduction is currently
//   being applied.  Light compression → fast release; heavy compression →
//   slow release.
//
// Detector
//   Stereo-linked RMS with a 50 ms integration window.  Long window = the
//   compressor reacts to the programme level, not individual transients.
//
// Ballistics
//   Gain smoothed in the linear domain with a first-order IIR.
//   Attack: 1 - exp(-1 / (t × sr)), t ∈ [0.008, 0.032] s — program-dependent.
//   Release: interpolated between fast (70 ms) and slow (250 ms) coefficients.
//==============================================================================
class NIKACompressor
{
public:
    void prepare       (double sampleRate);
    void setParameters (float thresholdDB, float ratio);   // ratio ≥ 1.0
    void process       (float& L, float& R);
    void reset();

private:
    static constexpr float kKneeDB = 4.0f;    // soft knee width (dB)

    double sampleRate  = 44100.0;
    float  thresholdDB = -16.0f;
    float  ratio       =  1.618f;

    // Stereo-linked RMS detector state
    float rmsState = 0.0f;
    float rmsCoeff = 0.0f;           // 50 ms integration

    // Fast envelope follower for transient detection
    float fastEnvState = 0.0f;
    float fastEnvCoeff = 0.0f;       // ≈5 ms — tracks peaks quickly

    // Current applied gain (linear, updated each sample)
    float currentGain      = 1.0f;
    float attackCoeffFast  = 0.0f;   // 8 ms — engaged on fast transients
    float attackCoeffSlow  = 0.0f;   // 32 ms — engaged on sustained material
    float releaseCoeffFast = 0.0f;   // 70 ms — used at low gain reduction
    float releaseCoeffSlow = 0.0f;   // 250 ms — used at high gain reduction
};
