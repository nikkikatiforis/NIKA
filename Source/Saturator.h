#pragma once
#include <JuceHeader.h>

//==============================================================================
// NIKASaturator — SVEMA MG-56 Soviet tape saturation model.
//
// Signal chain per sample (both channels):
//   [wow/flutter delay] → head bump EQ → upper mid EQ
//       → [harmonic saturation] → HF rolloff → noise floor
//
// Frequency shaping (always active, drive-independent):
//   Head bump  :  +2.618 dB @ 80 Hz,   Q = 0.618   (low-end warmth)
//   Upper mid  :  +1.618 dB @ 2981 Hz, Q = 1.0     (e^8 Hz, body)
//   HF rolloff :  2-pole Butterworth LP @ 13 kHz    (12 dB/oct, always on)
//   No presence peak — SVEMA suppresses HF, it does not flatter it.
//
// Harmonic saturation (active when driveActive):
//   Transfer: y = tanh((x + bias) * driveGain) − tanh(bias * driveGain)
//   Bias collapses to 0 above 65% drive → crossover from even (2nd) to
//   odd (3rd) harmonic dominance.  At the binary full-drive state (drive=1),
//   bias = 0: pure tanh, 3rd harmonic dominant.
//   φ-power taper: driveTapered = drive^1.618.
//
// Wow / flutter (active when driveActive):
//   Sinusoidal pitch instability: 0.05 Hz LFO (Fibonacci rate), ~0.3 ms depth.
//   Shared LFO phase between L and R — same tape transport.
//   Implemented as a variable-delay line with linear interpolation.
//
// Compressor threshold:
//   Processor overrides threshold to −8 dBFS when drive is active.
//   See PluginProcessor::processBlock.
//==============================================================================
class NIKASaturator
{
public:
    void prepare  (double sampleRate);
    void setDrive (float drive);          // [0..1]; >0.5 = on (СКАЗКУ toggle)
    void process  (float& L, float& R);
    void reset    ();

private:
    //==========================================================================
    // Biquad filter — transposed direct-form II.
    // Default coefficients (b0=1, rest=0) give unity-gain passthrough.
    //==========================================================================
    struct Biquad
    {
        float b0 = 1.0f, b1 = 0.0f, b2 = 0.0f;
        float              a1 = 0.0f, a2 = 0.0f;
        float s1 = 0.0f,  s2 = 0.0f;   // TDF-II state

        // Peaking EQ  — Audio EQ Cookbook constant-Q form
        void peak (double sr, double freqHz, double dBgain, double Q);

        // 2-pole Butterworth lowpass
        void lp2  (double sr, double freqHz, double Q);

        void clear () noexcept { s1 = s2 = 0.0f; }

        float tick (float x) noexcept
        {
            const float y = b0 * x + s1;
            s1 = b1 * x - a1 * y + s2;
            s2 = b2 * x - a2 * y;
            return y;
        }
    };

    //==========================================================================
    void  buildFilters ();
    float processChannel (float x,
                          Biquad& headBump, Biquad& upperMid, Biquad& hfRoll,
                          float*  wowBuf,   int&    wowPos);

    //==========================================================================
    double sampleRate  = 44100.0;
    bool   driveActive = false;

    // Saturation parameters (drive-dependent, updated by setDrive)
    float driveGain  = 1.0f;   // pre-gain into waveshaper   [1..5]
    float outGain    = 1.0f;   // 1/driveGain — unity small-signal normalisation
    float dcBias     = 0.0f;   // DC asymmetry term          [0..0.12]
    float dcBiasSat  = 0.0f;   // tanh(dcBias × driveGain)   DC removal offset
    float noiseLevel = 0.0f;   // noise floor amplitude
    float driveMix   = 0.0f;   // smooth crossfade weight [0..1] — set from satDriveSmoothed_

    // Frequency shaping — per channel, always active
    Biquad headBumpL, headBumpR;   // +2.618 dB @  80 Hz, Q = 0.618
    Biquad upperMidL, upperMidR;   // +1.618 dB @ 2981 Hz, Q = 1.0
    Biquad hfRollL,   hfRollR;    // 2-pole Butterworth LP @ 13 kHz

    // Wow / flutter — active when driveActive
    static constexpr int kWowBuf = 64;   // ≥ 2 × (center + depth) samples
    float wowBufL[kWowBuf] {};
    float wowBufR[kWowBuf] {};
    int   wowPosL     = 0;
    int   wowPosR     = 0;
    float wowLfoPhase = 0.0f;
    float wowLfoRate  = 0.0f;   // rad/sample
    float wowDepthSmp = 0.0f;   // peak deviation in samples (~0.3 ms)

    juce::Random rng;
};
