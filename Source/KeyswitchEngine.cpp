#include "KeyswitchEngine.h"

//==============================================================================
// Patch table — indexed by (note − kLowNote), i.e. slot 0 = C-1, slot 11 = B-1
//
// Each segment transitions exponentially (RC-style) from the current level
// to targetLevel over timeSec seconds.  holdHere pauses the envelope until
// noteOff() is called; releaseSec then governs the fall back to 0.
//
// Positive maxOctaves → filter opens.  Negative → filter closes.
//==============================================================================
// Patch table — indexed by (note − kLowNote).
//
// maxOctaves is a direction sign (±1); actual modulation depth is set by
// the intensity formula in getNextOctaves():
//   intensity = clamp(ksDepth×0.67 + pow(velocity,0.5)×0.33, 0, 1)
//   output    = level × intensity × 5 octaves × patchMaxOctaves
const NIKAKeyswitchEngine::Patch NIKAKeyswitchEngine::kPatches[] =
{
    // C-1 (note 12) — cresc: filter opens slowly (2 s), holds at peak, 1 s release
    { 1.0f, 1, {
        { 1.0f, 2.000f, true, 0.0f }
    }, 1.0f, false, 0.0f },

    // C#-1 (note 13) — dim: filter closes slowly (3 s), holds closed, 1 s release
    { -1.0f, 1, {
        { 1.0f, 3.000f, true, 0.0f }
    }, 1.0f, false, 0.0f },

    // D-1 (note 14) — fp: instant spike, dips below neutral, recovers
    { 1.0f, 3, {
        { 1.0f,   0.032f, false, 0.0f },  // spike
        { -0.2f,  0.512f, false, 0.0f },  // drop below neutral
        { 0.0f,   2.048f, false, 0.0f }   // recover to neutral
    }, 0.0f, false, 0.0f },

    // D#-1 (note 15) — swell: filter opens (1 s) then closes back (1 s)
    { 1.0f, 2, {
        { 1.0f, 1.000f, false, 0.0f },
        { 0.0f, 1.000f, false, 0.0f }
    }, 0.0f, false, 0.0f },

    // E-1 (note 16) — sfz: instant spike, slow decay back to neutral
    { 1.0f, 2, {
        { 1.0f, 0.002f, false, 0.0f },
        { 0.0f, 0.512f, false, 0.0f }
    }, 0.0f, false, 0.0f },

    // F-1 (note 17) — pizz: fast spike (8 ms), fast decay (64 ms)
    { 1.0f, 2, {
        { 1.0f, 0.016f, false, 0.0f },
        { 0.0f, 0.128f, false, 0.0f }
    }, 0.0f, false, 0.0f },

    // F#-1 (note 18) — trem: LFO oscillation at 6 Hz while held, 50 ms release
    { 1.0f, 1, {
        { 0.0f, 0.001f, true, 0.0f }   // instant to 0, hold immediately (LFO takes over)
    }, 0.05f, true, 6.0f },
};

//==============================================================================
void NIKAKeyswitchEngine::prepare (double sr)
{
    sampleRate = sr;
    state      = State::Idle;
    level      = 0.0f;
}

//==============================================================================
// Load segment idx: compute the per-sample RC coefficient.
// If timeSec ≤ 1 ms the transition is treated as instantaneous (segCoeff = 0).
//==============================================================================
void NIKAKeyswitchEngine::loadSegment (int idx)
{
    segIdx = idx;
    const Segment& s = segs[idx];

    if (s.timeSec <= 0.001f)
    {
        segInstant = true;
        segCoeff   = 0.0f;
    }
    else
    {
        segInstant = false;
        const double n = s.timeSec * sampleRate;
        segCoeff = float (std::exp (-6.908 / n));
    }
}

//==============================================================================
void NIKAKeyswitchEngine::trigger (int note, float velocity)
{
    const int index = note - kLowNote;
    jassert (index >= 0 && index < 7);

    const Patch& p  = kPatches[index];
    patchMaxOctaves = p.maxOctaves;
    velocityScale   = velocity;
    numSegs         = p.numSegs;
    for (int i = 0; i < numSegs; ++i) segs[i] = p.segs[i];
    patchIsTrem     = p.isTrem;
    patchTremRateHz = p.tremRateHz;
    tremPhase       = 0.0f;

    if (p.releaseSec > 0.001f)
    {
        const double rN = p.releaseSec * sampleRate;
        releaseCoeff = float (std::exp (-6.908 / rN));
    }
    else
    {
        releaseCoeff = 0.0f;   // no release — idle immediately on noteOff
    }

    holdCountdown = 0;
    level = 0.0f;
    loadSegment (0);
    state = State::Running;
}

void NIKAKeyswitchEngine::noteOff (int)
{
    if (state != State::Idle && state != State::Release)
        state = State::Release;
}

//==============================================================================
float NIKAKeyswitchEngine::getNextOctaves()
{
    if (state == State::Idle)
        return 0.0f;

    // Velocity-only intensity; caller multiplies by ksDepth
    const float intensity = std::pow (velocityScale, 0.5f) * 0.33f + 0.67f;

    if (state == State::Hold)
    {
        if (patchIsTrem)
        {
            tremPhase += float (patchTremRateHz / sampleRate);
            if (tremPhase >= 1.0f) tremPhase -= 1.0f;
            level = 0.5f * std::sin (juce::MathConstants<float>::twoPi * tremPhase);
        }
        return level * intensity * 5.0f * patchMaxOctaves;
    }

    if (state == State::TimedHold)
    {
        if (--holdCountdown <= 0)
        {
            holdCountdown = 0;
            if (segs[segIdx].holdHere)
                state = State::Hold;
            else if (segIdx + 1 < numSegs)
                loadSegment (segIdx + 1);
            else
            {
                level = 0.0f;
                state = State::Idle;
            }
        }
        return level * intensity * 5.0f * patchMaxOctaves;
    }

    if (state == State::Release)
    {
        if (releaseCoeff > 0.0f)
            level *= releaseCoeff;
        else
            level = 0.0f;

        if (level < 1e-4f)
        {
            level = 0.0f;
            state = State::Idle;
        }
        return level * intensity * 5.0f * patchMaxOctaves;
    }

    // State::Running — advance current segment
    jassert (segIdx < numSegs);
    const float target = segs[segIdx].targetLevel;

    if (segInstant)
    {
        level = target;
    }
    else
    {
        // RC exponential approach: level → target with per-sample coeff
        level = target + (level - target) * segCoeff;
    }

    // Segment complete when close enough to target or when instant
    if (segInstant || std::abs (level - target) < 0.001f)
    {
        level = target;
        if (segs[segIdx].holdSec > 0.0f)
        {
            holdCountdown = juce::roundToInt (segs[segIdx].holdSec * (float)sampleRate);
            state = State::TimedHold;
        }
        else if (segs[segIdx].holdHere)
        {
            state = State::Hold;
        }
        else if (segIdx + 1 < numSegs)
        {
            loadSegment (segIdx + 1);
        }
        else
        {
            level = 0.0f;
            state = State::Idle;
        }
    }

    return level * intensity * 5.0f * patchMaxOctaves;
}
