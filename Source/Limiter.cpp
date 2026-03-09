#include "Limiter.h"

//==============================================================================
void NIKALimiter::prepare (double sampleRate)
{
    // 20 ms release: transparent recovery, fast enough to avoid pumping
    releaseCoeff = 1.0f - std::exp (-1.0f / float (0.020 * sampleRate));
    reset();
}

void NIKALimiter::reset()
{
    std::fill (std::begin (delayBufL), std::end (delayBufL), 0.0f);
    std::fill (std::begin (delayBufR), std::end (delayBufR), 0.0f);
    writePos    = 0;
    currentGain = 1.0f;
}

//==============================================================================
void NIKALimiter::process (float& L, float& R)
{
    // 1. Write current input into lookahead buffer
    delayBufL[writePos] = L;
    delayBufR[writePos] = R;

    // 2. Detect peak of the CURRENT (look-ahead) input
    const float peak       = std::max (std::abs (L), std::abs (R));
    const float targetGain = (peak > kCeiling) ? kCeiling / peak : 1.0f;

    // 3. Apply gain:
    //    Attack — instant: gain snaps to targetGain if it would exceed ceiling.
    //    Release — smooth: gain recovers exponentially once the peak has passed.
    //    When the delayed sample finally reaches the output, the gain has been
    //    held at targetGain for the full kLookahead-sample warning period.
    if (targetGain < currentGain)
        currentGain = targetGain;
    else
        currentGain += (1.0f - currentGain) * releaseCoeff;

    // 4. Read from kLookahead samples ago and apply gain
    const int readPos = (writePos - kLookahead + kBufSize) & kBufMask;
    L = delayBufL[readPos] * currentGain;
    R = delayBufR[readPos] * currentGain;

    writePos = (writePos + 1) & kBufMask;
}
