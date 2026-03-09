#include "Oscillator.h"

void NIKAOscillator::prepare (double sr)
{
    sampleRate = sr;
    reset();
}

void NIKAOscillator::setFrequency (double freqHz)
{
    dt    = freqHz / sampleRate;
    subDt = dt * 0.5;
}

void NIKAOscillator::reset()
{
    phase    = 0.0;
    subPhase = 0.0;
}

//==============================================================================
// Classic 2nd-order PolyBLEP residual.
//
// Returns a correction value to subtract from (or add to) a naive discontinuous
// waveform near its transition points, smoothing the step into a band-limited
// polynomial blend.
//
//   Near t = 0  (just after a discontinuity):  correction ∈ [-1, 0]
//   Near t = 1  (just before a discontinuity): correction ∈ [0, +1]
//   Elsewhere:  0
//==============================================================================
float NIKAOscillator::polyBlep (double t, double dt)
{
    if (t < dt)
    {
        t /= dt;
        return (float)(t + t - t * t - 1.0);   // 2t - t² - 1, range [-1, 0]
    }
    if (t > 1.0 - dt)
    {
        t = (t - 1.0) / dt;
        return (float)(t * t + t + t + 1.0);   // t² + 2t + 1, range [0, +1]
    }
    return 0.0f;
}

//==============================================================================
float NIKAOscillator::process (float sawLevel,
                              float squareLevel,
                              float pulseLevel,  float pulseWidth,
                              float subLevel,
                              float noiseLevel)
{
    float out = 0.0f;

    // --- Saw (rising ramp: -1 → +1, discontinuity at phase wrap) -----------
    if (sawLevel > 0.0f)
    {
        float saw  = (float)(2.0 * phase - 1.0);
        saw -= polyBlep (phase, dt);        // smooth the wrap-around step
        out += saw * sawLevel;
    }

    // --- Square (50% duty cycle) --------------------------------------------
    if (squareLevel > 0.0f)
    {
        float sq = phase < 0.5 ? 1.0f : -1.0f;
        sq += polyBlep (phase, dt);          // smooth rising edge  (phase = 0)
        double t2 = phase + 0.5;
        if (t2 >= 1.0) t2 -= 1.0;
        sq -= polyBlep (t2, dt);             // smooth falling edge (phase = 0.5)
        out += sq * squareLevel;
    }

    // --- Pulse (variable PW, DC-corrected) ----------------------------------
    // After removing DC the output is zero-mean; peak amplitude varies with PW
    // (max ~1.9 at extreme widths) — use the level knob to compensate.
    if (pulseLevel > 0.0f)
    {
        double pw = (double)juce::jlimit (0.05f, 0.95f, pulseWidth);

        float pulse = phase < pw ? 1.0f : -1.0f;
        pulse += polyBlep (phase, dt);       // smooth rising edge  (phase = 0)
        double t2 = phase + (1.0 - pw);     // maps falling edge → phase 0
        if (t2 >= 1.0) t2 -= 1.0;
        pulse -= polyBlep (t2, dt);          // smooth falling edge (phase = pw)
        pulse -= (float)(2.0 * pw - 1.0);   // subtract DC offset
        out += pulse * pulseLevel;
    }

    // --- Sub (square, one octave below = half frequency) -------------------
    if (subLevel > 0.0f)
    {
        float sub = subPhase < 0.5 ? 1.0f : -1.0f;
        sub += polyBlep (subPhase, subDt);
        double t2 = subPhase + 0.5;
        if (t2 >= 1.0) t2 -= 1.0;
        sub -= polyBlep (t2, subDt);
        out += sub * subLevel;
    }

    // --- Noise (white) -------------------------------------------------------
    if (noiseLevel > 0.0f)
        out += (rng.nextFloat() * 2.0f - 1.0f) * noiseLevel;

    // --- Advance phases ------------------------------------------------------
    phase += dt;
    if (phase >= 1.0) phase -= 1.0;

    subPhase += subDt;
    if (subPhase >= 1.0) subPhase -= 1.0;

    return out;
}
