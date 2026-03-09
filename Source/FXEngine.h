#pragma once
#include <JuceHeader.h>

//==============================================================================
// NIKAFXEngine
//
// 7-slot effects section.  All internal parameters are hardcoded per patch.
// User-facing controls: patch index (1-7) and a single dry/wet mix knob.
//
// Patch 1 — wide chorus 0.08 Hz/3 ms, 1/4-note delay fb=0.618 LPF=4 kHz,
//           reverb RT60=4 s 2 ms pre-delay 2 kHz damping.
//
// Patch 2 — heavy chorus 0.13 Hz/13 ms, 1/8-note delay fb=0.382 LPF=8 kHz,
//           reverb RT60=2 s 8 ms pre-delay 4 kHz damping.
//
// Patch 3 — slow chorus 0.05 Hz/5 ms, triplet-1/4 delay fb=0.333 LPF=8 kHz,
//           reverb RT60=3 s 3 ms pre-delay 3 kHz damping.
//
// Patch 4 — subtle chorus 0.05 Hz/3 ms, 3/8-note delay fb=0.5 LPF=8 kHz,
//           reverb RT60=1 s 8 ms pre-delay 8 kHz damping.
//
// Patch 5 — Sun Studio slapback: no chorus, 128 ms fixed delay fb=0 LPF=3 kHz,
//           dead room reverb RT60=0.5 s no pre-delay 1 kHz damping.
//
// Patch 6 — Psychedelic: deep chorus 0.21 Hz/21 ms, dotted-1/4 delay
//           fb=0.786 LPF=2 kHz, reverb RT60=8 s 16 ms pre-delay 1 kHz damping.
//
// Patch 7 — Седьмая Печать (Seventh Seal): no chorus, 1/2-note delay
//           fb=0.256 LPF=1 kHz no flutter, reverb RT60=8 s 32 ms pre-delay
//           512 Hz damping.  Maximum darkness.
//
// Tempo-synced patches (1-4, 6, 7) read BPM from the host AudioPlayHead
// via NIKAAudioProcessor::processBlock(), which calls setBpm() once per block.
// Falls back to 120 BPM when no transport is available.
//
// Placement in signal chain: after saturator, before limiter.
// process() is called every sample regardless of ADSR state so that
// reverb decay and delay echoes continue after a note ends.
//
// Drive enhancements (active when satDrive > 0.5, setDriveTarget(1)):
//   1. FX param multipliers — 128 ms slew:
//        Reverb RT60 × sqrt(2):    comb g → pow(gBase, 1/sqrt2)
//        Delay feedback × sqrt(2): tdL/R.fb → min(0.95, fbBase × sqrt2)
//        Chorus depth × sqrt(2):   depth → depthBase × sqrt2
//   2. M/S width — applied in PluginProcessor after limiter, 128 ms slew.
//   3. Shimmer — parallel shimmer reverb, 512 ms fade, two pitch voices
//        (perfect fifth ×1.5, octave ×2.0), pre-delay 32 ms, mix 0.24.
//==============================================================================
class NIKAFXEngine
{
public:
    void prepare  (double sampleRate);
    void setPatch (int patch);                          // 1-based
    void process  (float& L, float& R, float dryWet);  // dryWet [0..1]

    // Call once per processBlock with host BPM (or 120 if unavailable).
    void setBpm (double bpm) noexcept { currentBpm = bpm; }

    // Call once per processBlock with drive state (0=off, 1=on).
    void setDriveTarget (float t) noexcept { driveTarget_ = t; }

private:
    //==========================================================================
    // Moorer-style comb filter
    // delay line with 1-pole LPF in the feedback path (models air absorption)
    //==========================================================================
    struct Comb
    {
        std::vector<float> buf;
        int   wPos = 0, dLen = 0;
        float lp   = 0.0f;    // LPF one-pole state
        float g    = 0.0f;    // feedback gain  (controls RT60)
        float damp = 0.0f;    // LPF damping coeff  (0 = bright, ↑ = darker)

        void  alloc     (int maxSamples);
        void  setParams (int delaySamples, float feedback, float damping);
        void  clear     ();
        float tick      (float x);
    };

    //==========================================================================
    // Schroeder allpass diffuser
    // H(z) = (z^{-M} − g) / (1 − g·z^{-M})  |H| = 1 for all ω
    //==========================================================================
    struct Allpass
    {
        std::vector<float> buf;
        int   wPos = 0, dLen = 0;
        float g    = 0.5f;

        void  alloc     (int maxSamples);
        void  setParams (int delaySamples, float gain = 0.5f);
        void  clear     ();
        float tick      (float x);
    };

    //==========================================================================
    // Tape delay
    // Circular delay + 1-pole LPF on feedback (warmth/HF rolloff per echo)
    // + sinusoidal wow/flutter LFO on the read position (tape instability)
    //==========================================================================
    struct TapeDly
    {
        std::vector<float> buf;
        int   wPos     = 0;
        float lp       = 0.0f;    // feedback LPF state
        float lfoPh    = 0.0f;    // wow/flutter LFO phase (radians)
        float lfoRate  = 0.0f;    // rad/sample
        float lfoDepth = 0.0f;    // peak deviation in samples
        float center   = 0.0f;    // nominal delay in samples
        float fb       = 0.0f;    // feedback gain
        float lpCoeff  = 0.0f;    // 1-pole LPF coefficient (feedback path)

        void  alloc     (int maxSamples);
        void  setParams (double sr, float delaySec, float feedback,
                         float lpCutHz, float wowHz, float wowDepthSamples);
        void  clear     ();
        float tick      (float x);
    private:
        float interp    (float rPos) const;
    };

    //==========================================================================
    // Chorus voice
    // LFO-modulated short delay line, no feedback.
    // Two voices per channel (L1/L2, R1/R2) at slightly different rates and
    // quadrature phases for stereo spread.
    //==========================================================================
    struct ChorusVoice
    {
        std::vector<float> buf;
        int   wPos    = 0;
        float lfoPh   = 0.0f;
        float lfoRate = 0.0f;
        float center  = 0.0f;    // nominal delay in samples
        float depth   = 0.0f;    // LFO depth in samples

        void  alloc     (int maxSamples);
        void  setParams (double sr, float centerMs, float lfoHz, float depthMs);
        void  clear     ();
        float tick      (float x);
    private:
        float interp    (float rPos) const;
    };

    //==========================================================================
    // Shimmer pitch-shift voice
    //
    // Two overlapping read heads with Hann cross-fade, each advancing at
    // `ratio` samples per output sample.  When a grain completes, the read
    // head jumps back to kGrain*3 samples behind the write position so that
    // it never overtakes the write cursor.  Feedback (0.32) feeds the
    // pitch-shifted output back into the delay buffer.
    //==========================================================================
    struct ShimmerVoice
    {
        static constexpr int kBufBits = 14;              // 16384 ≈ 371 ms at 44.1 k
        static constexpr int kBufSize = 1 << kBufBits;
        static constexpr int kBufMask = kBufSize - 1;
        static constexpr int kGrain   = kBufSize / 8;   // 2048 ≈ 46 ms grain window

        std::vector<float> buf;
        int    wPos     = 0;
        float  rPos[2]  = {};
        float  ph[2]    = { 0.0f, 0.5f };  // Hann phase [0..1), offset by half grain
        float  ratio    = 1.5f;
        float  fbLevel  = 0.32f;
        float  lastOut  = 0.0f;

        void  init (float pitchRatio, float fb);
        float tick (float x);
    };

    //==========================================================================
    static constexpr int kC = 4, kA = 2;

    double sampleRate  = 44100.0;
    int    activePatch = 0;    // 0 = uninitialised

    // Reverb bank: 4 Moorer combs + 2 Schroeder allpass, per channel
    Comb    cmbL[kC], cmbR[kC];
    Allpass  apL[kA],  apR[kA];
    float   revWet = 0.0f;

    // Mono pre-delay for reverb input
    std::vector<float> pdBuf;
    int pdPos = 0, pdLen = 0;

    // Tape delay (stereo)
    TapeDly tdL, tdR;
    float   dlyWet = 0.0f;

    // Chorus (2 voices per channel)
    ChorusVoice chL1, chL2, chR1, chR2;
    float       chWet = 0.0f;

    // Tempo-sync state
    double currentBpm    = 120.0;
    bool   dlyTempoSync  = false;
    float  dlyBeatMultL  = 0.0f;  // beat multiplier for L delay center
    float  dlyBeatMultR  = 0.0f;  // beat multiplier for R delay center

    //==========================================================================
    // Drive enhancement state (all updated each sample in process())
    float driveTarget_    = 0.0f;
    float driveSmoothed_  = 0.0f;
    float driveSlewCoeff_ = 0.0f;   // 128 ms one-pole TC

    // Base and drive-target values for per-patch comb feedback
    // gDrive = pow(gBase, 1/sqrt2) — equivalent to RT60 x sqrt(2)
    float gBase_L[kC]  = {}, gBase_R[kC]  = {};
    float gDrive_L[kC] = {}, gDrive_R[kC] = {};

    // Base delay feedback (drive target = min(0.95, fbBase * sqrt2))
    float fbBaseL_ = 0.0f, fbBaseR_ = 0.0f;

    // Base chorus depth in samples (drive target = depthBase * sqrt2)
    float depthBaseL1_ = 0.0f, depthBaseL2_ = 0.0f;
    float depthBaseR1_ = 0.0f, depthBaseR2_ = 0.0f;

    //==========================================================================
    // Shimmer module
    ShimmerVoice       shFifthL_,  shOctaveL_;   // L channel: fifth ×1.5, octave ×2.0
    ShimmerVoice       shFifthR_,  shOctaveR_;   // R channel: fifth ×1.5, octave ×2.0
    float              shFadeSmoothed_  = 0.0f;
    float              shFadeSlewCoeff_ = 0.0f;   // 512 ms one-pole TC
    std::vector<float> shPreDelayL_;              // 32 ms pre-delay, fed from rl
    std::vector<float> shPreDelayR_;              // 32 ms pre-delay, fed from rr
    int                shPdPosL_ = 0, shPdPosR_ = 0, shPdLen_ = 0;

    //==========================================================================
    void clearAll      ();
    void storeBaseParams ();   // called at end of each buildPatch — captures base values
    void buildPatch1   ();
    void buildPatch2   ();
    void buildPatch3   ();
    void buildPatch4   ();
    void buildPatch5   ();
    void buildPatch6   ();
    void buildPatch7   ();
};
