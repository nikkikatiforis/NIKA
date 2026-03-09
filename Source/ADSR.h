#pragma once
#include <JuceHeader.h>

//==============================================================================
// NIKAADSR
//
// Custom ADSR envelope generator with analog-style curve shapes:
//   Attack  — exponential RC-circuit rise from 0 → 1 (SH-101 style):
//             envelope approaches a virtual target of (1 + kAttackOffset)
//             and clamps to 1.0, giving the characteristic fast-start,
//             slow-finish curve of a capacitor charging through a resistor.
//   Decay   — exponential fall from 1 → 0, clipped at sustainLevel
//   Sustain — hold at sustainLevel until noteOff
//   Release — exponential fall from sustainLevel → 0
//
// Exponential coefficients are computed so the envelope travels ~60 dB
// (level × 0.001) over the user-specified time, matching the feel of an
// analog RC network.
//
// The envelope gates output via isActive(): callers should silence their
// audio path when isActive() returns false, allowing release tails to
// play out naturally in all routing modes.
//==============================================================================
class NIKAADSR
{
public:
    void  prepare       (double sampleRate);
    void  setParameters (float attackSec, float decaySec,
                         float sustainLevel, float releaseSec);
    void  noteOn();
    void  noteOnFromLevel (double startLevel);  // retrigger without snapping to 0
    void  noteOff();
    float getNextSample();
    float getLevel()  const noexcept { return (float)level; }
    bool  isActive()  const noexcept { return state != State::Idle; }
    void  reset();

private:
    // Returns the per-sample exponential coefficient for a curve that falls
    // to 0.001 (−60 dB) of its starting value after `timeSec` seconds.
    static double calcExpCoeff (double timeSec, double sampleRate);

    // Virtual overshoot target for RC-style attack: the envelope rises toward
    // (1 + kAttackOffset), clamping to 1.0, so it reaches 1.0 in approximately
    // attackSec seconds — the same timing guarantee as decay and release.
    static constexpr double kAttackOffset = 0.001;

    enum class State { Idle, Attack, Decay, Sustain, Release };

    double sampleRate   = 44100.0;
    State  state        = State::Idle;
    double level        = 0.0;

    double sustainLevel = 0.0;
    double attackCoeff  = 0.0;   // RC-style per-sample coeff during Attack
    double decayCoeff   = 0.0;   // multiplied per sample during Decay
    double releaseCoeff = 0.0;   // multiplied per sample during Release
};
