#include "Compressor.h"

//==============================================================================
void NIKACompressor::prepare (double sr)
{
    sampleRate = sr;

    rmsCoeff        = std::exp (-1.0f / float (0.050 * sr));        // 50 ms RMS window
    fastEnvCoeff    = std::exp (-1.0f / float (0.005 * sr));        // 5 ms fast env
    attackCoeffFast = 1.0f - std::exp (-1.0f / float (0.008 * sr)); // 8 ms — transients
    attackCoeffSlow = 1.0f - std::exp (-1.0f / float (0.032 * sr)); // 32 ms — sustained
    releaseCoeffFast = 1.0f - std::exp (-1.0f / float (0.070 * sr)); // 70 ms fast
    releaseCoeffSlow = 1.0f - std::exp (-1.0f / float (0.250 * sr)); // 250 ms slow

    reset();
}

void NIKACompressor::setParameters (float thDB, float r)
{
    thresholdDB = thDB;
    ratio       = juce::jlimit (1.0f, 8.0f, r);
}

void NIKACompressor::reset()
{
    rmsState    = 0.0f;
    fastEnvState = 0.0f;
    currentGain = 1.0f;
}

//==============================================================================
void NIKACompressor::process (float& L, float& R)
{
    // --- Stereo-linked RMS detection ----------------------------------------
    const float inSq = 0.5f * (L * L + R * R);
    rmsState = rmsState * rmsCoeff + inSq * (1.0f - rmsCoeff);
    const float rmsLvl = std::sqrt (rmsState + 1e-30f);
    const float lvlDB  = 20.0f * std::log10 (rmsLvl + 1e-30f);

    // --- Program-dependent attack (Fairchild 670 behaviour) -----------------
    //
    // A fast envelope follower (5 ms) tracks peak amplitude; dividing by the
    // slower RMS level yields a transient index > 1 when peaks overshoot the
    // programme level.  tNorm maps this ratio from [1, 4] → [0, 1]:
    //   tNorm = 1  (strong transient)  → fast attack (8 ms)
    //   tNorm = 0  (sustained material) → slow attack (32 ms)
    //
    const float peak = std::sqrt (inSq * 2.0f + 1e-30f);   // instantaneous peak
    fastEnvState = fastEnvState * fastEnvCoeff + peak * (1.0f - fastEnvCoeff);
    const float transientRatio = fastEnvState / (rmsLvl + 1e-30f);
    const float tNorm          = juce::jlimit (0.0f, 1.0f, (transientRatio - 1.0f) / 3.0f);
    const float attackCoeff    = attackCoeffSlow + tNorm * (attackCoeffFast - attackCoeffSlow);

    // --- Gain computer: 4 dB soft knee --------------------------------------
    //
    // Three regions (W = kKneeDB):
    //   2×(lev - thr) < -W  →  no compression (below knee)
    //   |2×(lev - thr)| ≤ W  →  quadratic blend (inside knee)
    //   2×(lev - thr) > +W  →  full ratio (above knee)
    //
    const float slope  = 1.0f - 1.0f / ratio;    // ≥ 0
    const float excess = lvlDB - thresholdDB;
    float grDB = 0.0f;

    if (2.0f * excess > -kKneeDB && 2.0f * excess < kKneeDB)
    {
        const float k = excess + kKneeDB * 0.5f;
        grDB = k * k / (2.0f * kKneeDB) * slope;
    }
    else if (2.0f * excess >= kKneeDB)
    {
        grDB = excess * slope;
    }

    const float targetGain = std::pow (10.0f, -grDB / 20.0f);

    // --- Program-dependent release ------------------------------------------
    //
    // currentGRdB: how much gain reduction is currently being applied.
    // grNorm maps [0, 12 dB] → [0, 1].
    //   grNorm = 0 → fast release (70 ms): lightly touched, recover quickly.
    //   grNorm = 1 → slow release (250 ms): heavily compressed, hold longer.
    //
    const float currentGRdB = -20.0f * std::log10 (currentGain + 1e-30f);
    const float grNorm       = juce::jlimit (0.0f, 1.0f, currentGRdB / 12.0f);
    const float relCoeff     = releaseCoeffFast
                             + grNorm * (releaseCoeffSlow - releaseCoeffFast);

    if (targetGain < currentGain)
        currentGain += (targetGain - currentGain) * attackCoeff;
    else
        currentGain += (targetGain - currentGain) * relCoeff;

    L *= currentGain;
    R *= currentGain;
}
