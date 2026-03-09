#pragma once
#include <JuceHeader.h>

//==============================================================================
// NIKAKeyswitchEngine
//
// MIDI notes 12–18 (C-1 to F#-1 in Ableton) are reserved as keyswitches.  They never trigger
// the oscillator; instead each note fires a pre-defined filter envelope that
// runs independently of the ADSR.
//
// Envelope shapes are built from ordered segment sequences.  Each segment
// transitions exponentially (RC-style) from the current level to a target
// level over a specified time.  A segment flagged holdHere pauses the engine
// at that level until noteOff() is called, after which a separate release
// curve (releaseSec) returns the level to 0.
//
// Positive maxOctaves opens the filter cutoff; negative closes it.
// Output formula: level × velocityScale × ksDepth × maxOctaves (log-octaves).
//
// Intensity formula (replaces per-patch maxOctaves scaling):
//   intensity = clamp(ksDepth×0.67 + pow(velocity,0.5)×0.33, 0, 1)
//   output    = level × intensity × 5 octaves × sign(patchMaxOctaves)
//
// Slot  Note   Name   Description
// ────  ─────  ─────  ──────────────────────────────────────────────────────
//   0   C-1    cresc  filter opens slowly over attack, holds at peak
//   1   C#-1   dim    filter closes slowly over decay, holds closed
//   2   D-1    fp     instant spike then dips below current cutoff level
//   3   D#-1   swell  filter opens then closes back to starting point
//   4   E-1    sfz    instant spike, slow decay back to starting point
//   5   F-1    pizz   very fast spike, very fast decay, percussive
//   6   F#-1   trem   rapid LFO oscillation on filter cutoff while held
//==============================================================================
class NIKAKeyswitchEngine
{
public:
    static constexpr int kLowNote  = 12;   // C-1 (Ableton convention)
    static constexpr int kHighNote = 18;  // F#-1 (Ableton convention)

    static bool isKeyswitch (int note) noexcept
    {
        return note >= kLowNote && note <= kHighNote;
    }

    void  prepare (double sampleRate);
    void  trigger (int note, float velocity);  // call on noteOn for ks notes
    void  noteOff (int note);                  // triggers release on held patches

    // Returns octaves of filter modulation for this sample and advances state.
    // ksDepth is the [0..1] master depth from APVTS.
    float getNextOctaves();

    bool  isActive()  const noexcept { return state != State::Idle; }

private:
    //==========================================================================
    struct Segment
    {
        float targetLevel;  // level to reach at end of this segment
        float timeSec;      // transition time (≤ 0.001 s treated as instant)
        bool  holdHere;     // if true, hold at targetLevel until noteOff()
        float holdSec;      // fixed timed pause at target before advancing (0 = none)
    };

    struct Patch
    {
        float   maxOctaves;          // direction sign: +1 opens, -1 closes filter
        int     numSegs;             // number of active segments (max 8)
        Segment segs[8];             // ordered segment list
        float   releaseSec;          // release time after hold is released
        bool    isTrem;              // if true, Hold state runs LFO instead of constant level
        float   tremRateHz;          // LFO rate in Hz (used when isTrem is true)
    };

    static const Patch kPatches[];   // indexed by (note − kLowNote)

    //==========================================================================
    double sampleRate     = 44100.0;

    enum class State { Idle, Running, TimedHold, Hold, Release };
    State state = State::Idle;

    float level           = 0.0f;
    float velocityScale   = 0.0f;
    float patchMaxOctaves = 0.0f;

    // Running state
    int     numSegs    = 0;
    int     segIdx     = 0;
    float   segCoeff   = 0.0f;   // RC per-sample coeff; 0.0 = instant jump
    bool    segInstant = false;
    Segment segs[8];              // working copy of current patch's segments

    // Release state
    float releaseCoeff  = 0.0f;

    // Timed hold state
    int   holdCountdown = 0;

    // Trem LFO state
    bool  patchIsTrem     = false;
    float patchTremRateHz = 0.0f;
    float tremPhase       = 0.0f;

    void loadSegment (int idx);
};
