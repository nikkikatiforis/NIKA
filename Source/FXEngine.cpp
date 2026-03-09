#include "FXEngine.h"

// Next power of two, minimum 4.
static int np2 (int n) noexcept
{
    int p = 4;
    while (p < n) p <<= 1;
    return p;
}

//==============================================================================
// Comb
//==============================================================================
void NIKAFXEngine::Comb::alloc (int maxSamples)
{
    buf.assign (np2 (maxSamples + 2), 0.0f);
    wPos = 0; lp = 0.0f;
}

void NIKAFXEngine::Comb::setParams (int delaySamples, float feedback, float damping)
{
    dLen = delaySamples;
    g    = feedback;
    damp = damping;
}

void NIKAFXEngine::Comb::clear ()
{
    std::fill (buf.begin(), buf.end(), 0.0f);
    wPos = 0; lp = 0.0f;
}

float NIKAFXEngine::Comb::tick (float x)
{
    const int  mask = int (buf.size()) - 1;
    const int  rPos = (wPos - dLen + int (buf.size())) & mask;
    const float out = buf[rPos];
    lp = out * (1.0f - damp) + lp * damp;   // Moorer LPF damping in loop
    buf[wPos] = x + lp * g;
    wPos = (wPos + 1) & mask;
    return out;
}

//==============================================================================
// Allpass
//==============================================================================
void NIKAFXEngine::Allpass::alloc (int maxSamples)
{
    buf.assign (np2 (maxSamples + 2), 0.0f);
    wPos = 0;
}

void NIKAFXEngine::Allpass::setParams (int delaySamples, float gain)
{
    dLen = delaySamples;
    g    = gain;
}

void NIKAFXEngine::Allpass::clear ()
{
    std::fill (buf.begin(), buf.end(), 0.0f);
    wPos = 0;
}

float NIKAFXEngine::Allpass::tick (float x)
{
    const int  mask = int (buf.size()) - 1;
    const int  rPos = (wPos - dLen + int (buf.size())) & mask;
    const float z   = buf[rPos];
    buf[wPos] = x + z * g;          // Schroeder allpass
    wPos = (wPos + 1) & mask;
    return z - x * g;               // |H| = 1 for all frequencies
}

//==============================================================================
// TapeDly
//==============================================================================
void NIKAFXEngine::TapeDly::alloc (int maxSamples)
{
    buf.assign (np2 (maxSamples + 4), 0.0f);
    wPos = 0; lp = 0.0f; lfoPh = 0.0f;
}

void NIKAFXEngine::TapeDly::setParams (double sr, float delaySec, float feedback,
                                      float lpCutHz, float wowHz, float wowDepthSamples)
{
    center   = float (delaySec * sr);
    fb       = feedback;
    lpCoeff  = std::exp (-juce::MathConstants<float>::twoPi * lpCutHz / float (sr));
    lfoRate  = juce::MathConstants<float>::twoPi * wowHz / float (sr);
    lfoDepth = wowDepthSamples;
}

void NIKAFXEngine::TapeDly::clear ()
{
    std::fill (buf.begin(), buf.end(), 0.0f);
    wPos = 0; lp = 0.0f; lfoPh = 0.0f;
}

float NIKAFXEngine::TapeDly::tick (float x)
{
    const float mod = lfoDepth * std::sin (lfoPh);
    lfoPh += lfoRate;
    if (lfoPh >= juce::MathConstants<float>::twoPi)
        lfoPh -= juce::MathConstants<float>::twoPi;

    float rPos = float (wPos) - (center + mod);
    if (rPos < 0.0f)
        rPos += float (buf.size());

    const float out = interp (rPos);
    lp = out * (1.0f - lpCoeff) + lp * lpCoeff;  // LPF warms each echo
    buf[wPos] = x + lp * fb;
    wPos = (wPos + 1) & (int (buf.size()) - 1);
    return out;
}

float NIKAFXEngine::TapeDly::interp (float rPos) const
{
    const int  mask = int (buf.size()) - 1;
    const int    n0   = int (rPos) & mask;
    const float  f    = rPos - float (int (rPos));
    return buf[size_t (n0)] * (1.0f - f) + buf[size_t ((n0 + 1) & mask)] * f;
}

//==============================================================================
// ChorusVoice
//==============================================================================
void NIKAFXEngine::ChorusVoice::alloc (int maxSamples)
{
    buf.assign (np2 (maxSamples + 4), 0.0f);
    wPos = 0; lfoPh = 0.0f;
}

void NIKAFXEngine::ChorusVoice::setParams (double sr, float centerMs,
                                          float lfoHz, float depthMs)
{
    center  = float (centerMs * 0.001 * sr);
    depth   = float (depthMs  * 0.001 * sr);
    lfoRate = juce::MathConstants<float>::twoPi * lfoHz / float (sr);
}

void NIKAFXEngine::ChorusVoice::clear ()
{
    std::fill (buf.begin(), buf.end(), 0.0f);
    wPos = 0; lfoPh = 0.0f;
}

float NIKAFXEngine::ChorusVoice::tick (float x)
{
    buf[wPos] = x;

    const float mod = depth * std::sin (lfoPh);
    lfoPh += lfoRate;
    if (lfoPh >= juce::MathConstants<float>::twoPi)
        lfoPh -= juce::MathConstants<float>::twoPi;

    float rPos = float (wPos) - (center + mod);
    if (rPos < 0.0f)
        rPos += float (buf.size());

    const float out = interp (rPos);
    wPos = (wPos + 1) & (int (buf.size()) - 1);
    return out;
}

float NIKAFXEngine::ChorusVoice::interp (float rPos) const
{
    const int    mask = int (buf.size()) - 1;
    const int    n0   = int (rPos) & mask;
    const float  f    = rPos - float (int (rPos));
    return buf[size_t (n0)] * (1.0f - f) + buf[size_t ((n0 + 1) & mask)] * f;
}

//==============================================================================
// ShimmerVoice
//
// Two Hann-windowed read heads advance through a circular buffer at `ratio`
// samples per output sample.  When a head's grain phase wraps, the read
// position is reset to kGrain*3 samples behind the write cursor, ensuring it
// never reads ahead of what has been written (valid for ratio ≤ 2).
// Linear interpolation is used between adjacent samples.
// Self-feedback (fbLevel) feeds the pitch-shifted output back into the input.
//==============================================================================
void NIKAFXEngine::ShimmerVoice::init (float pitchRatio, float fb)
{
    ratio   = pitchRatio;
    fbLevel = fb;
    buf.assign (kBufSize, 0.0f);
    wPos      = 0;
    rPos[0]   = float ((kBufSize - kGrain * 3) & kBufMask);
    rPos[1]   = float ((kBufSize - kGrain * 3 + kGrain) & kBufMask);
    ph[0]     = 0.0f;
    ph[1]     = 0.5f;
    lastOut   = 0.0f;
}

float NIKAFXEngine::ShimmerVoice::tick (float x)
{
    // Write input + self-feedback into the delay buffer
    buf[wPos] = x + lastOut * fbLevel;

    float outSum = 0.0f, wSum = 0.0f;

    for (int h = 0; h < 2; ++h)
    {
        // Advance grain phase; reset read head when grain completes
        ph[h] += 1.0f / float (kGrain);
        if (ph[h] >= 1.0f)
        {
            ph[h] -= 1.0f;
            rPos[h] = float ((wPos - kGrain * 3 + kBufSize) & kBufMask);
        }

        // Advance read position at pitch ratio
        rPos[h] += ratio;
        if (rPos[h] >= float (kBufSize))
            rPos[h] -= float (kBufSize);

        // Linear interpolation
        const int   n0 = int (rPos[h]) & kBufMask;
        const float  f = rPos[h] - float (int (rPos[h]));
        const float  s = buf[size_t (n0)] * (1.0f - f)
                       + buf[size_t ((n0 + 1) & kBufMask)] * f;

        // Hann window weight
        const float w = 0.5f - 0.5f * std::cos (
            juce::MathConstants<float>::twoPi * ph[h]);
        outSum += w * s;
        wSum   += w;
    }

    wPos    = (wPos + 1) & kBufMask;
    lastOut = outSum / (wSum + 1e-6f);
    return lastOut;
}

//==============================================================================
// NIKAFXEngine
//==============================================================================
void NIKAFXEngine::prepare (double sr)
{
    sampleRate  = sr;
    activePatch = 0;

    // Allocate all buffers to worst-case size at this sample rate.
    // setPatch() only reconfigures parameters and clears buffers — no alloc.

    const int maxComb = int (0.100 * sr) + 2;   // 100 ms headroom
    for (int i = 0; i < kC; ++i) { cmbL[i].alloc (maxComb); cmbR[i].alloc (maxComb); }

    const int maxAP = int (0.015 * sr) + 2;     // 15 ms headroom
    for (int i = 0; i < kA; ++i) { apL[i].alloc (maxAP); apR[i].alloc (maxAP); }

    const int maxPD = int (0.035 * sr) + 2;     // 35 ms pre-delay (P7 uses 32 ms)
    pdBuf.assign (size_t (np2 (maxPD)), 0.0f);
    pdPos = 0; pdLen = 0;

    // 2.2 s covers 1/2-note at 60 BPM (worst-case tempo-synced delay).
    const int maxTD = int (2.2 * sr) + 6;
    tdL.alloc (maxTD);
    tdR.alloc (maxTD);

    const int maxCh = int (0.055 * sr) + 4;     // 55 ms (P6 centre 30 + depth 18 = 48 ms)
    chL1.alloc (maxCh); chL2.alloc (maxCh);
    chR1.alloc (maxCh); chR2.alloc (maxCh);

    // Drive smoother (128 ms time constant)
    driveSlewCoeff_ = 1.0f - std::exp (-1.0f / (0.128f * float (sr)));

    // Shimmer fade smoother (512 ms time constant)
    shFadeSlewCoeff_ = 1.0f - std::exp (-1.0f / (0.512f * float (sr)));

    // Shimmer pre-delay buffers: 32 ms per channel; use next pow2
    shPdLen_ = int (0.032 * sr);
    const int shPdBufSize = np2 (shPdLen_ + 2);
    shPreDelayL_.assign (size_t (shPdBufSize), 0.0f);
    shPreDelayR_.assign (size_t (shPdBufSize), 0.0f);
    shPdPosL_ = 0; shPdPosR_ = 0;

    // Shimmer voice buffers (stereo pair)
    shFifthL_.buf.assign  (ShimmerVoice::kBufSize, 0.0f);
    shOctaveL_.buf.assign (ShimmerVoice::kBufSize, 0.0f);
    shFifthR_.buf.assign  (ShimmerVoice::kBufSize, 0.0f);
    shOctaveR_.buf.assign (ShimmerVoice::kBufSize, 0.0f);
}

void NIKAFXEngine::clearAll ()
{
    for (int i = 0; i < kC; ++i) { cmbL[i].clear(); cmbR[i].clear(); }
    for (int i = 0; i < kA; ++i) { apL[i].clear();  apR[i].clear(); }
    std::fill (pdBuf.begin(), pdBuf.end(), 0.0f); pdPos = 0;
    tdL.clear(); tdR.clear();
    chL1.clear(); chL2.clear(); chR1.clear(); chR2.clear();
    dlyTempoSync     = false;
    dlyBeatMultL     = 0.0f;
    dlyBeatMultR     = 0.0f;

    // Shimmer
    if (!shPreDelayL_.empty()) std::fill (shPreDelayL_.begin(), shPreDelayL_.end(), 0.0f);
    if (!shPreDelayR_.empty()) std::fill (shPreDelayR_.begin(), shPreDelayR_.end(), 0.0f);
    shPdPosL_ = 0; shPdPosR_ = 0;
}

//==============================================================================
// storeBaseParams — called at the end of every buildPatchN().
//
// Captures the base comb feedback (g), tape delay feedback (fb), and chorus
// depth values that were just set by buildPatch.  The drive enhancement lerps
// these in process() each sample: drive off → base values, drive on →
// drive-target values (RT60 × sqrt2, feedback × sqrt2, depth × sqrt2).
//==============================================================================
void NIKAFXEngine::storeBaseParams ()
{
    static constexpr float kInvSqrt2 = 0.70710678f;   // 1 / sqrt(2)
    static constexpr float kSqrt2    = 1.41421356f;

    for (int i = 0; i < kC; ++i)
    {
        gBase_L[i]  = cmbL[i].g;
        gBase_R[i]  = cmbR[i].g;
        // RT60 × sqrt2 ≡ g^(1/sqrt2): longer decay with drive on
        gDrive_L[i] = std::pow (cmbL[i].g, kInvSqrt2);
        gDrive_R[i] = std::pow (cmbR[i].g, kInvSqrt2);
    }

    fbBaseL_ = tdL.fb;
    fbBaseR_ = tdR.fb;

    depthBaseL1_ = chL1.depth;
    depthBaseL2_ = chL2.depth;
    depthBaseR1_ = chR1.depth;
    depthBaseR2_ = chR2.depth;

    // Re-initialise shimmer voices on every patch change
    shFifthL_.init  (1.5f, 0.32f);   // L: perfect fifth
    shOctaveL_.init (2.0f, 0.32f);   // L: octave
    shFifthR_.init  (1.5f, 0.32f);   // R: perfect fifth
    shOctaveR_.init (2.0f, 0.32f);   // R: octave

    (void) kSqrt2;  // used in process(); suppresses unused-variable warning
}

void NIKAFXEngine::setPatch (int patch)
{
    if (patch == activePatch || sampleRate == 0.0)
        return;

    activePatch = patch;
    clearAll();   // flush all delay lines on patch change

    switch (patch)
    {
        case 1:  buildPatch1(); break;
        case 2:  buildPatch2(); break;
        case 3:  buildPatch3(); break;
        case 4:  buildPatch4(); break;
        case 5:  buildPatch5(); break;
        case 6:  buildPatch6(); break;
        case 7:  buildPatch7(); break;
        default: revWet = dlyWet = chWet = 0.0f; break;
    }
}

//==============================================================================
// process()
//
// All patches share this series routing:
//
//   Stage 1 — Chorus (if chWet > 0):
//     dry L/R → 2 chorus voices per channel
//     L/R = dry × (1−chWet) + chorus × chWet
//
//   Stage 2 — Tape delay (if dlyWet > 0):
//     chorus out → tdL/tdR
//     L/R = chorus × (1−dlyWet) + delay × dlyWet
//
//   Stage 3 — Reverb (if revWet > 0):
//     delay out → mono fold → optional pre-delay → 4 parallel combs
//                           → 2 series allpass
//     L/R = delay × (1−revWet) + reverb × revWet
//
//   Shimmer (parallel, drive on only):
//     reverb out → 32 ms pre-delay → two pitch-shift voices (fifth + octave)
//     L/R += shimmer × 0.24 × shimFade
//
//   Output:
//     L/R = dry × (1−dw) + stage3_out × dw
//==============================================================================
void NIKAFXEngine::process (float& L, float& R, float dryWet)
{
    // Always update smoothers so they track drive state even when bypassed
    driveSmoothed_  += driveSlewCoeff_  * (driveTarget_  - driveSmoothed_);
    shFadeSmoothed_ += shFadeSlewCoeff_ * (driveSmoothed_ - shFadeSmoothed_);

    if (activePatch == 0 || dryWet == 0.0f)
        return;

    // --- Apply drive enhancement multipliers to live FX params ---------------
    // Interpolate between base (drive=0) and drive-target (drive=1) values.
    {
        const float d    = driveSmoothed_;
        const float sq2m1 = 0.41421356f;   // sqrt(2) − 1

        for (int i = 0; i < kC; ++i)
        {
            cmbL[i].g = gBase_L[i] + (gDrive_L[i] - gBase_L[i]) * d;
            cmbR[i].g = gBase_R[i] + (gDrive_R[i] - gBase_R[i]) * d;
        }

        tdL.fb = juce::jmin (0.95f, fbBaseL_ + fbBaseL_ * sq2m1 * d);
        tdR.fb = juce::jmin (0.95f, fbBaseR_ + fbBaseR_ * sq2m1 * d);

        chL1.depth = depthBaseL1_ + depthBaseL1_ * sq2m1 * d;
        chL2.depth = depthBaseL2_ + depthBaseL2_ * sq2m1 * d;
        chR1.depth = depthBaseR1_ + depthBaseR1_ * sq2m1 * d;
        chR2.depth = depthBaseR2_ + depthBaseR2_ * sq2m1 * d;
    }

    // Tempo-synced delays: recompute center every sample from current BPM.
    // Clamped to 60 BPM minimum so center never exceeds the pre-allocated buffer.
    if (dlyTempoSync)
    {
        const float beatSamples = float (sampleRate * 60.0 / juce::jmax (60.0, currentBpm));
        tdL.center = dlyBeatMultL * beatSamples;
        tdR.center = dlyBeatMultR * beatSamples;
    }

    const float dryL = L, dryR = R;

    // --- Stage 1: Chorus -----------------------------------------------------
    if (chWet > 0.0f)
    {
        const float cl = (chL1.tick (L) + chL2.tick (L)) * 0.5f;
        const float cr = (chR1.tick (R) + chR2.tick (R)) * 0.5f;
        L = L * (1.0f - chWet) + cl * chWet;
        R = R * (1.0f - chWet) + cr * chWet;
    }

    // --- Stage 2: Tape delay -------------------------------------------------
    if (dlyWet > 0.0f)
    {
        const float dl = tdL.tick (L);
        const float dr = tdR.tick (R);
        L = L * (1.0f - dlyWet) + dl * dlyWet;
        R = R * (1.0f - dlyWet) + dr * dlyWet;
    }

    // --- Stage 3: Reverb (combs + allpass) -----------------------------------
    float rl = 0.0f, rr = 0.0f;   // reverb output — also read by shimmer

    if (revWet > 0.0f)
    {
        // Mono fold + optional pre-delay
        const float mono = (L + R) * 0.5f;
        const int   mask = int (pdBuf.size()) - 1;
        pdBuf[size_t (pdPos)] = mono;
        const float revIn = (pdLen > 0)
            ? pdBuf[size_t ((pdPos - pdLen + int (pdBuf.size())) & mask)]
            : mono;
        pdPos = (pdPos + 1) & mask;

        for (int i = 0; i < kC; ++i) { rl += cmbL[i].tick (revIn); rr += cmbR[i].tick (revIn); }
        rl *= (1.0f / kC); rr *= (1.0f / kC);
        for (int i = 0; i < kA; ++i) { rl = apL[i].tick (rl); rr = apR[i].tick (rr); }

        L = L * (1.0f - revWet) + rl * revWet;
        R = R * (1.0f - revWet) + rr * revWet;
    }

    // --- Shimmer (parallel, drive active only) --------------------------------
    // Reads from stereo reverb allpass output (rl, rr); L and R processed
    // independently.  Fifth panned left, octave panned right for stereo spread.
    if (shFadeSmoothed_ > 1e-4f && revWet > 0.0f)
    {
        const int shMask = int (shPreDelayL_.size()) - 1;

        // Left channel pre-delay (fed from rl)
        shPreDelayL_[size_t (shPdPosL_)] = rl;
        const float delayedL = shPreDelayL_[size_t ((shPdPosL_ - shPdLen_
                                + int (shPreDelayL_.size())) & shMask)];
        shPdPosL_ = (shPdPosL_ + 1) & shMask;

        // Right channel pre-delay (fed from rr)
        shPreDelayR_[size_t (shPdPosR_)] = rr;
        const float delayedR = shPreDelayR_[size_t ((shPdPosR_ - shPdLen_
                                + int (shPreDelayR_.size())) & shMask)];
        shPdPosR_ = (shPdPosR_ + 1) & shMask;

        // Pitch-shift voices per channel
        const float fifthL  = shFifthL_.tick  (delayedL);
        const float octaveL = shOctaveL_.tick (delayedL);
        const float fifthR  = shFifthR_.tick  (delayedR);
        const float octaveR = shOctaveR_.tick (delayedR);

        // Pan: fifth biased left (0.7/0.3), octave biased right (0.3/0.7)
        const float shimL = fifthL * 0.7f + octaveL * 0.3f;
        const float shimR = fifthR * 0.3f + octaveR * 0.7f;

        // Add to wet bus with 512 ms fade envelope
        const float gain = 0.16f * shFadeSmoothed_;
        L += shimL * gain;
        R += shimR * gain;
    }

    // --- Master dry/wet mix --------------------------------------------------
    L = dryL * (1.0f - dryWet) + L * dryWet;
    R = dryR * (1.0f - dryWet) + R * dryWet;
}

//==============================================================================
// Patch 1 — Wide chorus 0.08 Hz / 3 ms, 1/4-note delay fb=0.618 LPF=4 kHz,
//           RT60=4 s pre-delay=2 ms damping=2 kHz.
//
// Comb feedback g = exp(−6.908 × 0.075 / 4.0) ≈ 0.879
// Chorus phases: L=0/π, R=π/0 for wide stereo image.
//==============================================================================
void NIKAFXEngine::buildPatch1 ()
{
    const double sr  = sampleRate;
    const auto   ms  = [&] (double t) { return int (t * 0.001 * sr); };

    pdLen = ms (2.0);

    cmbL[0].setParams (ms (71.3), 0.879f, 0.75f);
    cmbL[1].setParams (ms (76.7), 0.879f, 0.75f);
    cmbL[2].setParams (ms (82.1), 0.879f, 0.75f);
    cmbL[3].setParams (ms (88.5), 0.879f, 0.75f);

    cmbR[0].setParams (ms (72.1), 0.879f, 0.75f);
    cmbR[1].setParams (ms (77.8), 0.879f, 0.75f);
    cmbR[2].setParams (ms (83.3), 0.879f, 0.75f);
    cmbR[3].setParams (ms (89.4), 0.879f, 0.75f);

    apL[0].setParams (ms (5.5),  0.5f);
    apL[1].setParams (ms (12.0), 0.5f);
    apR[0].setParams (ms (5.9),  0.5f);
    apR[1].setParams (ms (12.5), 0.5f);

    // Slow wide chorus: L and R antiphase for stereo width
    chL1.setParams (sr, 15.0f, 0.08f, 3.0f);
    chL2.setParams (sr, 18.0f, 0.09f, 2.5f);
    chR1.setParams (sr, 15.0f, 0.08f, 3.0f);
    chR2.setParams (sr, 18.0f, 0.09f, 2.5f);

    chL1.lfoPh = 0.0f;
    chL2.lfoPh = float (juce::MathConstants<double>::pi);
    chR1.lfoPh = float (juce::MathConstants<double>::pi);       // antiphase = wide
    chR2.lfoPh = 0.0f;

    // 1/4-note at 120 BPM = 0.5 s; recomputed each sample from currentBpm
    tdL.setParams (sr, 0.500f, 0.618f, 4000.0f, 1.2f, 1.5f);
    tdR.setParams (sr, 0.500f, 0.618f, 4000.0f, 1.0f, 1.5f);

    dlyBeatMultL = 1.0f;
    dlyBeatMultR = 1.0f;
    dlyTempoSync = true;
    revWet       = 0.30f;
    dlyWet       = 0.28f;
    chWet        = 0.18f;

    storeBaseParams();
}

//==============================================================================
// Patch 2 — Heavy chorus 0.13 Hz / 13 ms, 1/8-note delay fb=0.382 LPF=8 kHz,
//           RT60=2 s pre-delay=8 ms damping=4 kHz.  Striking and immersive.
//
// Comb g = exp(−6.908 × 0.060 / 2.0) ≈ 0.813
//==============================================================================
void NIKAFXEngine::buildPatch2 ()
{
    const double sr  = sampleRate;
    const auto   ms  = [&] (double t) { return int (t * 0.001 * sr); };

    pdLen = ms (8.0);

    cmbL[0].setParams (ms (55.0), 0.813f, 0.57f);
    cmbL[1].setParams (ms (60.3), 0.813f, 0.57f);
    cmbL[2].setParams (ms (65.7), 0.813f, 0.57f);
    cmbL[3].setParams (ms (71.1), 0.813f, 0.57f);

    cmbR[0].setParams (ms (56.1), 0.813f, 0.57f);
    cmbR[1].setParams (ms (61.4), 0.813f, 0.57f);
    cmbR[2].setParams (ms (66.8), 0.813f, 0.57f);
    cmbR[3].setParams (ms (72.2), 0.813f, 0.57f);

    apL[0].setParams (ms (4.8),  0.5f);
    apL[1].setParams (ms (11.0), 0.5f);
    apR[0].setParams (ms (5.2),  0.5f);
    apR[1].setParams (ms (11.5), 0.5f);

    // Heavy chorus — deep 13 ms depth creates striking movement
    chL1.setParams (sr, 20.0f, 0.13f, 13.0f);
    chL2.setParams (sr, 25.0f, 0.15f, 11.0f);
    chR1.setParams (sr, 20.0f, 0.13f, 13.0f);
    chR2.setParams (sr, 25.0f, 0.15f, 11.0f);

    chL1.lfoPh = 0.0f;
    chL2.lfoPh = float (juce::MathConstants<double>::pi);
    chR1.lfoPh = float (juce::MathConstants<double>::pi);
    chR2.lfoPh = 0.0f;

    // 1/8-note at 120 BPM = 0.25 s
    tdL.setParams (sr, 0.250f, 0.382f, 8000.0f, 0.8f, 1.0f);
    tdR.setParams (sr, 0.250f, 0.382f, 8000.0f, 0.7f, 1.0f);

    dlyBeatMultL = 0.5f;
    dlyBeatMultR = 0.5f;
    dlyTempoSync = true;
    revWet       = 0.25f;
    dlyWet       = 0.25f;
    chWet        = 0.30f;

    storeBaseParams();
}

//==============================================================================
// Patch 3 — Slow chorus 0.05 Hz / 5 ms, triplet-1/4 delay fb=0.333 LPF=8 kHz,
//           RT60=3 s pre-delay=3 ms damping=3 kHz.
//
// Triplet-quarter = 2/3 of a beat.  Comb g = exp(−6.908 × 0.068 / 3.0) ≈ 0.855
//==============================================================================
void NIKAFXEngine::buildPatch3 ()
{
    const double sr  = sampleRate;
    const auto   ms  = [&] (double t) { return int (t * 0.001 * sr); };

    pdLen = ms (3.0);

    cmbL[0].setParams (ms (63.0), 0.855f, 0.65f);
    cmbL[1].setParams (ms (68.3), 0.855f, 0.65f);
    cmbL[2].setParams (ms (74.1), 0.855f, 0.65f);
    cmbL[3].setParams (ms (80.5), 0.855f, 0.65f);

    cmbR[0].setParams (ms (64.1), 0.855f, 0.65f);
    cmbR[1].setParams (ms (69.4), 0.855f, 0.65f);
    cmbR[2].setParams (ms (75.2), 0.855f, 0.65f);
    cmbR[3].setParams (ms (81.6), 0.855f, 0.65f);

    apL[0].setParams (ms (5.3),  0.5f);
    apL[1].setParams (ms (11.8), 0.5f);
    apR[0].setParams (ms (5.7),  0.5f);
    apR[1].setParams (ms (12.3), 0.5f);

    // Very slow chorus — glacial movement, wide stereo
    chL1.setParams (sr, 15.0f, 0.05f, 5.0f);
    chL2.setParams (sr, 20.0f, 0.06f, 4.0f);
    chR1.setParams (sr, 15.0f, 0.05f, 5.0f);
    chR2.setParams (sr, 20.0f, 0.06f, 4.0f);

    chL1.lfoPh = 0.0f;
    chL2.lfoPh = float (juce::MathConstants<double>::pi);
    chR1.lfoPh = float (juce::MathConstants<double>::pi);
    chR2.lfoPh = 0.0f;

    // Triplet-quarter = 2/3 beat; at 120 BPM = 0.333 s
    tdL.setParams (sr, 0.333f, 0.333f, 8000.0f, 0.5f, 0.5f);
    tdR.setParams (sr, 0.333f, 0.333f, 8000.0f, 0.4f, 0.5f);

    dlyBeatMultL = 2.0f / 3.0f;
    dlyBeatMultR = 2.0f / 3.0f;
    dlyTempoSync = true;
    revWet       = 0.28f;
    dlyWet       = 0.22f;
    chWet        = 0.20f;

    storeBaseParams();
}

//==============================================================================
// Patch 4 — Subtle chorus 0.05 Hz / 3 ms moderate stereo, 3/8-note delay
//           fb=0.5 LPF=8 kHz, RT60=1 s pre-delay=8 ms damping=8 kHz (bright).
//
// 3/8-note = 1.5 beats.  Comb g = exp(−6.908 × 0.045 / 1.0) ≈ 0.733
//==============================================================================
void NIKAFXEngine::buildPatch4 ()
{
    const double sr  = sampleRate;
    const auto   ms  = [&] (double t) { return int (t * 0.001 * sr); };

    pdLen = ms (8.0);

    cmbL[0].setParams (ms (40.0), 0.733f, 0.32f);
    cmbL[1].setParams (ms (45.2), 0.733f, 0.32f);
    cmbL[2].setParams (ms (50.6), 0.733f, 0.32f);
    cmbL[3].setParams (ms (55.9), 0.733f, 0.32f);

    cmbR[0].setParams (ms (41.1), 0.733f, 0.32f);
    cmbR[1].setParams (ms (46.3), 0.733f, 0.32f);
    cmbR[2].setParams (ms (51.7), 0.733f, 0.32f);
    cmbR[3].setParams (ms (57.0), 0.733f, 0.32f);

    apL[0].setParams (ms (4.1), 0.5f);
    apL[1].setParams (ms (9.0), 0.5f);
    apR[0].setParams (ms (4.5), 0.5f);
    apR[1].setParams (ms (9.5), 0.5f);

    // Moderate stereo: R offset by π/2 (quadrature) rather than antiphase
    chL1.setParams (sr, 15.0f, 0.05f, 3.0f);
    chL2.setParams (sr, 18.0f, 0.06f, 2.5f);
    chR1.setParams (sr, 15.0f, 0.05f, 3.0f);
    chR2.setParams (sr, 18.0f, 0.06f, 2.5f);

    chL1.lfoPh = 0.0f;
    chL2.lfoPh = float (juce::MathConstants<double>::pi);
    chR1.lfoPh = float (juce::MathConstants<double>::pi * 0.5);   // 90° = moderate width
    chR2.lfoPh = float (juce::MathConstants<double>::pi * 1.5);

    // 3/8-note = 1.5 beats; at 120 BPM = 0.75 s
    tdL.setParams (sr, 0.750f, 0.5f, 8000.0f, 0.5f, 0.5f);
    tdR.setParams (sr, 0.750f, 0.5f, 8000.0f, 0.4f, 0.5f);

    dlyBeatMultL = 1.5f;
    dlyBeatMultR = 1.5f;
    dlyTempoSync = true;
    revWet       = 0.20f;
    dlyWet       = 0.22f;
    chWet        = 0.15f;

    storeBaseParams();
}

//==============================================================================
// Patch 5 — Sun Studio slapback.
//
// No chorus.  128 ms fixed delay (single repeat, fb=0), heavy flutter (32%)
// for that 1950s tape instability.  Dead room: very short warm combs,
// RT60 ≈ 0.5 s, 1 kHz damping — no pre-delay.
//
// Comb g = exp(−6.908 × 0.015 / 0.5) ≈ 0.813
//==============================================================================
void NIKAFXEngine::buildPatch5 ()
{
    const double sr  = sampleRate;
    const auto   ms  = [&] (double t) { return int (t * 0.001 * sr); };

    pdLen = 0;

    // Dead room: very short combs, warm damping, no RT
    cmbL[0].setParams (ms (13.1), 0.813f, 0.87f);
    cmbL[1].setParams (ms (15.0), 0.813f, 0.87f);
    cmbL[2].setParams (ms (17.3), 0.813f, 0.87f);
    cmbL[3].setParams (ms (19.7), 0.813f, 0.87f);

    cmbR[0].setParams (ms (14.0), 0.813f, 0.87f);
    cmbR[1].setParams (ms (16.1), 0.813f, 0.87f);
    cmbR[2].setParams (ms (18.4), 0.813f, 0.87f);
    cmbR[3].setParams (ms (20.8), 0.813f, 0.87f);

    apL[0].setParams (ms (2.5), 0.5f);
    apL[1].setParams (ms (5.0), 0.5f);
    apR[0].setParams (ms (2.9), 0.5f);
    apR[1].setParams (ms (5.4), 0.5f);

    // 128 ms fixed slapback, single repeat (fb=0), heavy flutter
    tdL.setParams (sr, 0.128f, 0.0f, 3000.0f, 2.0f, 2.5f);
    tdR.setParams (sr, 0.130f, 0.0f, 3000.0f, 1.8f, 2.5f);

    dlyTempoSync = false;
    revWet       = 0.18f;
    dlyWet       = 0.50f;
    chWet        = 0.0f;

    storeBaseParams();
}

//==============================================================================
// Patch 6 — Psychedelic.
//
// Aggressive deep chorus 0.21 Hz / 21 ms, dotted-1/4 delay fb=0.786 LPF=2 kHz,
// vast reverb RT60=8 s 16 ms pre-delay 1 kHz damping.
//
// Dotted-1/4 = 1.5 beats.  Comb g = exp(−6.908 × 0.092 / 8.0) ≈ 0.924
// Chorus center=25-30 ms + depth up to 21 ms — wide, dream-like movement.
//==============================================================================
void NIKAFXEngine::buildPatch6 ()
{
    const double sr  = sampleRate;
    const auto   ms  = [&] (double t) { return int (t * 0.001 * sr); };

    pdLen = ms (16.0);

    cmbL[0].setParams (ms (85.0), 0.924f, 0.87f);
    cmbL[1].setParams (ms (90.3), 0.924f, 0.87f);
    cmbL[2].setParams (ms (95.7), 0.924f, 0.87f);
    cmbL[3].setParams (ms (99.4), 0.924f, 0.87f);

    cmbR[0].setParams (ms (86.1), 0.924f, 0.87f);
    cmbR[1].setParams (ms (91.4), 0.924f, 0.87f);
    cmbR[2].setParams (ms (96.8), 0.924f, 0.87f);
    cmbR[3].setParams (ms (100.3), 0.924f, 0.87f);

    apL[0].setParams (ms (5.2),  0.5f);
    apL[1].setParams (ms (12.5), 0.5f);
    apR[0].setParams (ms (5.6),  0.5f);
    apR[1].setParams (ms (13.0), 0.5f);

    // Deep, wide psychedelic chorus — substantial pitch movement
    chL1.setParams (sr, 25.0f, 0.21f, 21.0f);
    chL2.setParams (sr, 30.0f, 0.24f, 18.0f);
    chR1.setParams (sr, 25.0f, 0.21f, 21.0f);
    chR2.setParams (sr, 30.0f, 0.24f, 18.0f);

    chL1.lfoPh = 0.0f;
    chL2.lfoPh = float (juce::MathConstants<double>::pi);
    chR1.lfoPh = float (juce::MathConstants<double>::pi);
    chR2.lfoPh = 0.0f;

    // Dotted-1/4 = 1.5 beats; at 120 BPM = 0.75 s; heavy flutter
    tdL.setParams (sr, 0.750f, 0.786f, 2000.0f, 2.0f, 2.5f);
    tdR.setParams (sr, 0.750f, 0.786f, 2000.0f, 1.8f, 2.5f);

    dlyBeatMultL = 1.5f;
    dlyBeatMultR = 1.5f;
    dlyTempoSync = true;
    revWet       = 0.32f;
    dlyWet       = 0.28f;
    chWet        = 0.35f;

    storeBaseParams();
}

//==============================================================================
// Patch 7 — Седьмая Печать (The Seventh Seal).
//
// No chorus.  1/2-note delay fb=0.256 LPF=1 kHz, no flutter.
// Vast dark reverb: RT60=8 s, 32 ms pre-delay, 512 Hz damping — sub-bass
// frequencies only survive.
//
// 1/2-note = 2 beats.  Comb g = exp(−6.908 × 0.092 / 8.0) ≈ 0.924
// Damp 0.93 → extremely dark, only the lowest partials feed back.
//==============================================================================
void NIKAFXEngine::buildPatch7 ()
{
    const double sr  = sampleRate;
    const auto   ms  = [&] (double t) { return int (t * 0.001 * sr); };

    pdLen = ms (32.0);

    cmbL[0].setParams (ms (85.0), 0.924f, 0.93f);
    cmbL[1].setParams (ms (90.3), 0.924f, 0.93f);
    cmbL[2].setParams (ms (95.7), 0.924f, 0.93f);
    cmbL[3].setParams (ms (99.4), 0.924f, 0.93f);

    cmbR[0].setParams (ms (87.2), 0.924f, 0.93f);
    cmbR[1].setParams (ms (91.5), 0.924f, 0.93f);
    cmbR[2].setParams (ms (96.9), 0.924f, 0.93f);
    cmbR[3].setParams (ms (100.8), 0.924f, 0.93f);

    apL[0].setParams (ms (5.4),  0.5f);
    apL[1].setParams (ms (13.0), 0.5f);
    apR[0].setParams (ms (5.8),  0.5f);
    apR[1].setParams (ms (13.5), 0.5f);

    // 1/2-note = 2 beats; at 120 BPM = 1.0 s; no flutter
    tdL.setParams (sr, 1.000f, 0.256f, 1000.0f, 0.0f, 0.0f);
    tdR.setParams (sr, 1.000f, 0.256f, 1000.0f, 0.0f, 0.0f);

    dlyBeatMultL = 2.0f;
    dlyBeatMultR = 2.0f;
    dlyTempoSync = true;
    revWet       = 0.40f;
    dlyWet       = 0.25f;
    chWet        = 0.0f;

    storeBaseParams();
}
