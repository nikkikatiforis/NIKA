#include "LadderFilter.h"

//==============================================================================
void NIKALadderFilter::prepare (double sr)
{
    sampleRate = sr;
    reset();
    setParameters (1000.0f, 0.0f);   // safe defaults
}

//==============================================================================
// Coefficient update.
//
// fc  = cutoffHz / sampleRate   (normalised to OUTPUT rate, not oversampled rate)
// f   = fc × 1.16               Huovilainen's empirically fitted constant for the
//                                2× oversampled forward-Euler integration.
// k   = resonance × 4.0         At k = 4 the loop gain equals 1 in the linear
//                                regime; tanh saturation bounds self-oscillation.
//==============================================================================
void NIKALadderFilter::setParameters (float cutoffHz, float resonance)
{
    const double fc = juce::jlimit (1.0, 20000.0, (double)cutoffHz) / sampleRate;
    f = fc * 1.16;
    k = (double)juce::jlimit (0.0f, 1.0f, resonance) * 4.0;
}

//==============================================================================
void NIKALadderFilter::reset()
{
    y1 = y2 = y3 = y4 = 0.0;
    xi = y1i = y2i = y3i = 0.0;
    prevIn = 0.0;
}

//==============================================================================
// One integration step at the 2× oversampled rate.
//
// The algorithm (Huovilainen 2004/2006):
//
//   xn = tanh(x − k·y4)                         resonance feedback + input clip
//
//   y1 ← y1 + f·(tanh(xn  + 0.3·xi)  − tanh(y1))   stage 1
//   y2 ← y2 + f·(tanh(y1n + 0.3·y1i) − tanh(y2))   stage 2
//   y3 ← y3 + f·(tanh(y2n + 0.3·y2i) − tanh(y3))   stage 3
//   y4 ← y4 + f·(tanh(y3n + 0.3·y3i) − tanh(y4))   stage 4
//
// The cross-coupling term (0.3 × previous stage input) models the parasitic
// coupling between transistor stages in the physical ladder and significantly
// improves the accuracy of the passband and resonance peak shape.
//==============================================================================
void NIKALadderFilter::tick (double x)
{
    // Non-linear input drive with resonance feedback
    const double xn = std::tanh (x - k * y4);

    // Stage 1
    const double y1n = y1 + f * (std::tanh (xn  + 0.3 * xi)  - std::tanh (y1));
    xi  = xn;

    // Stage 2
    const double y2n = y2 + f * (std::tanh (y1n + 0.3 * y1i) - std::tanh (y2));
    y1i = y1n;

    // Stage 3
    const double y3n = y3 + f * (std::tanh (y2n + 0.3 * y2i) - std::tanh (y3));
    y2i = y2n;

    // Stage 4
    const double y4n = y4 + f * (std::tanh (y3n + 0.3 * y3i) - std::tanh (y4));
    y3i = y3n;

    y1 = y1n;
    y2 = y2n;
    y3 = y3n;
    y4 = y4n;
}

//==============================================================================
// Public per-sample entry point.
//
// Upsampling:  linear interpolation produces a midpoint sample between
//              prevIn and x — simple but adequate; the nonlinearity's
//              harmonics land above the output Nyquist after decimation.
// Downsampling: take only the second tick's output (drop the midpoint).
//==============================================================================
float NIKALadderFilter::process (float x)
{
    const double xd = (double)x;
    tick (0.5 * (prevIn + xd));   // interpolated midpoint
    tick (xd);                    // current sample
    prevIn = xd;
    return (float)y4;
}
