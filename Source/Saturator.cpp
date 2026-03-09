#include "Saturator.h"

//==============================================================================
// Biquad coefficient setters
//==============================================================================

// Peaking EQ (constant-Q, Audio EQ Cookbook §EQ peaking filter).
// Coefficients only — state (s1, s2) is untouched.
void NIKASaturator::Biquad::peak (double sr, double freqHz, double dBgain, double Q)
{
    const double A   = std::sqrt (std::pow (10.0, dBgain / 20.0));
    const double w0  = juce::MathConstants<double>::twoPi * freqHz / sr;
    const double alp = std::sin (w0) / (2.0 * Q);
    const double cw0 = std::cos (w0);

    const double invA0 = 1.0 / (1.0 + alp / A);
    b0 = float ((1.0 + alp * A)  * invA0);
    b1 = float ((-2.0 * cw0)     * invA0);
    b2 = float ((1.0 - alp * A)  * invA0);
    a1 = float ((-2.0 * cw0)     * invA0);
    a2 = float ((1.0 - alp / A)  * invA0);
}

// 2-pole Butterworth lowpass (bilinear transform, pre-warped at fc).
// At Q = 1/√2 (0.7071) this gives maximally flat passband (Butterworth).
void NIKASaturator::Biquad::lp2 (double sr, double freqHz, double Q)
{
    const double w0  = juce::MathConstants<double>::twoPi * freqHz / sr;
    const double alp = std::sin (w0) / (2.0 * Q);
    const double cw0 = std::cos (w0);

    const double B0 = (1.0 - cw0) * 0.5;
    const double B1 =  1.0 - cw0;
    const double B2 = (1.0 - cw0) * 0.5;
    const double A0 =  1.0 + alp;
    const double A1 = -2.0 * cw0;
    const double A2 =  1.0 - alp;

    const double inv = 1.0 / A0;
    b0 = float (B0 * inv);
    b1 = float (B1 * inv);
    b2 = float (B2 * inv);
    a1 = float (A1 * inv);
    a2 = float (A2 * inv);
}

//==============================================================================
void NIKASaturator::prepare (double sr)
{
    sampleRate = sr;
    buildFilters();

    // Wow/flutter: 0.05 Hz LFO, 0.3 ms peak depth
    wowLfoRate  = float (juce::MathConstants<double>::twoPi * 0.05 / sr);
    wowDepthSmp = float (0.0003 * sr);

    reset();
}

void NIKASaturator::buildFilters()
{
    // Head bump: +2.618 dB @ 80 Hz, Q = 0.618 (low-end warmth / transformer resonance)
    headBumpL.peak (sampleRate, 80.0,          2.618, 0.618);
    headBumpR.peak (sampleRate, 80.0,          2.618, 0.618);

    // Upper mid push: +1.618 dB @ e^8 Hz ≈ 2981 Hz, Q = 1 (body / presence)
    upperMidL.peak (sampleRate, std::exp (8.0), 1.618, 1.0);
    upperMidR.peak (sampleRate, std::exp (8.0), 1.618, 1.0);

    // HF rolloff: 2-pole Butterworth LP @ 13 kHz, Q = 1/√2 (12 dB/oct)
    const double bwQ = 1.0 / std::sqrt (2.0);
    hfRollL.lp2 (sampleRate, 13000.0, bwQ);
    hfRollR.lp2 (sampleRate, 13000.0, bwQ);
}

void NIKASaturator::reset()
{
    headBumpL.clear();  headBumpR.clear();
    upperMidL.clear();  upperMidR.clear();
    hfRollL.clear();    hfRollR.clear();

    std::fill (std::begin (wowBufL), std::end (wowBufL), 0.0f);
    std::fill (std::begin (wowBufR), std::end (wowBufR), 0.0f);
    wowPosL = wowPosR = 0;
    wowLfoPhase = 0.0f;
}

//==============================================================================
void NIKASaturator::setDrive (float drive)
{
    drive       = juce::jlimit (0.0f, 1.0f, drive);
    driveActive = (drive > 0.5f);

    // φ-power taper (golden ratio exponent).  Since the UI is a binary toggle,
    // rawDrive is 0 or 1; driveTapered = rawDrive^1.618 = rawDrive.
    static constexpr float kPhi = 1.618f;
    const float rawDrive     = driveActive ? 1.0f : 0.0f;
    const float driveTapered = std::pow (rawDrive, kPhi);   // 0 or 1

    // Pre-gain into waveshaper: 1× (transparent) → 5× (hard saturation)
    driveGain = 1.0f + driveTapered * 4.0f;
    outGain   = 1.0f / driveGain;

    // DC bias — even (2nd) harmonic source.
    // biasScale = 1 at drive=0, ramps to 0 at drive=0.65, stays 0 above.
    // At full drive (1.0), biasScale = max(0, 1 - 1/0.65) = 0:
    //   → pure tanh → odd harmonic (3rd) dominance.
    const float biasScale = juce::jmax (0.0f, 1.0f - rawDrive / 0.65f);
    dcBias    = 0.12f * biasScale;
    dcBiasSat = std::tanh (dcBias * driveGain);

    // Noise floor: −96 dBFS (off) → −80 dBFS (on)
    noiseLevel = std::pow (10.0f, (-96.0f + rawDrive * 16.0f) / 20.0f);
}

//==============================================================================
float NIKASaturator::processChannel (float x,
                                    Biquad& headBump, Biquad& upperMid, Biquad& hfRoll,
                                    float*  wowBuf,   int&    wowPos)
{
    // 1. Wow / flutter — sinusoidal pitch instability via variable delay.
    //    Only active when driveActive.  Shared LFO phase (advanced in process()).
    if (driveActive)
    {
        wowBuf[wowPos] = x;

        // Read position: offset by (center ± depth×sin(lfo))
        const float center = wowDepthSmp + 1.0f;
        const float rPos0  = (float)wowPos - center
                             - wowDepthSmp * std::sin (wowLfoPhase);

        // Wrap into circular buffer
        float rPos = rPos0;
        if (rPos < 0.0f)           rPos += (float)kWowBuf;
        if (rPos >= (float)kWowBuf) rPos -= (float)kWowBuf;

        // Linear interpolation between adjacent buffer samples
        const int   r0   = (int)rPos;
        const float frac = rPos - (float)r0;
        const int   r1   = (r0 + 1) % kWowBuf;
        x = wowBuf[r0] * (1.0f - frac) + wowBuf[r1] * frac;

        wowPos = (wowPos + 1) % kWowBuf;
    }

    // 2. Frequency shaping — always active regardless of drive state.
    //    Head bump and upper mid apply before saturation (shape the drive character).
    x = headBump.tick (x);
    x = upperMid.tick (x);

    // 3. Harmonic saturation — only when driveActive.
    //    Transfer: y = tanh((x + bias) × G) − tanh(bias × G)
    //    At full drive, bias = 0 → odd harmonic (3rd) dominance.
    if (driveActive)
    {
        const float sat = std::tanh ((x + dcBias) * driveGain) - dcBiasSat;
        x = sat * outGain;
    }

    // 4. HF rolloff — always active (two-pole, 12 dB/oct @ 13 kHz).
    //    Applied after saturation to smooth harmonic content.
    x = hfRoll.tick (x);

    // 5. Noise floor — slight hiss, elevated when drive is on.
    x += (rng.nextFloat() * 2.0f - 1.0f) * noiseLevel;

    return x;
}

void NIKASaturator::process (float& L, float& R)
{
    // Advance wow/flutter LFO once per sample.
    // Both channels share the same phase — tape transport is mono.
    if (driveActive)
    {
        wowLfoPhase += wowLfoRate;
        if (wowLfoPhase >= juce::MathConstants<float>::twoPi)
            wowLfoPhase -= juce::MathConstants<float>::twoPi;
    }

    L = processChannel (L, headBumpL, upperMidL, hfRollL, wowBufL, wowPosL);
    R = processChannel (R, headBumpR, upperMidR, hfRollR, wowBufR, wowPosR);
}
