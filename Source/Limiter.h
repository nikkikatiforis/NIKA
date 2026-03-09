#pragma once
#include <JuceHeader.h>

//==============================================================================
// NIKALimiter — Weiss 102-inspired transparent brickwall limiter.
//
// Algorithm
//   Lookahead delay: input is written into a circular delay buffer, and the
//   output reads kLookahead samples behind the write cursor.  Level detection
//   is performed on the current (future) input; gain reduction is applied to
//   the delayed output.  This gives the limiter up to kLookahead samples of
//   warning before any peak arrives at the output.
//
//   Attack: instant — the required gain is applied in the same sample the
//   peak is detected.  By the time that sample reaches the output (kLookahead
//   samples later) the gain has been holding for the full lookahead period.
//
//   Release: single-pole exponential, 20 ms time constant.  Fast enough to
//   stay out of the way of the programme; slow enough to avoid zipper noise.
//
// Ceiling: 0 dBFS (kCeiling = 1.0).  Limiter runs after all gain staging
//   (M/S width matrix + +13 dB output gain) and acts as the absolute last
//   safeguard before the DAW's input.  Hardcoded — no user parameters.
//
// Latency: kLookahead samples.  Callers should report this via
//   setLatencySamples(NIKALimiter::kLookahead) in prepareToPlay().
//
// Stereo
//   Stereo-linked detection (gain from max(|L|, |R|)), independent delay
//   buffers for L and R.
//==============================================================================
class NIKALimiter
{
public:
    static constexpr float kCeiling   = 1.0f;  // 0 dBFS — true brickwall
    static constexpr int   kLookahead = 64;         // ~1.45 ms at 44.1 kHz

private:
    static constexpr int   kBufMask   = 127;        // power-of-2 − 1 for fast wrap
    static constexpr int   kBufSize   = kBufMask + 1;

public:
    void prepare (double sampleRate);
    void process (float& L, float& R);
    void reset();

private:
    float delayBufL[kBufSize] = {};
    float delayBufR[kBufSize] = {};
    int   writePos     = 0;
    float currentGain  = 1.0f;
    float releaseCoeff = 0.0f;
};
