#include "ADSR.h"

//==============================================================================
void NIKAADSR::prepare (double sr)
{
    sampleRate = sr;
    reset();
}

void NIKAADSR::reset()
{
    state = State::Idle;
    level = 0.0;
}

//==============================================================================
// coeff^n = 0.001  =>  coeff = exp(log(0.001) / n) = exp(-6.908 / n)
double NIKAADSR::calcExpCoeff (double timeSec, double sampleRate)
{
    const double n = std::max (timeSec * sampleRate, 1.0);
    return std::exp (-6.908 / n);
}

//==============================================================================
void NIKAADSR::setParameters (float attackSec, float decaySec,
                             float sus, float releaseSec)
{
    sustainLevel = (double)juce::jlimit (0.0f, 1.0f, sus);
    attackCoeff  = calcExpCoeff ((double)attackSec,   sampleRate);
    decayCoeff   = calcExpCoeff ((double)decaySec,    sampleRate);
    releaseCoeff = calcExpCoeff ((double)releaseSec,  sampleRate);
}

//==============================================================================
void NIKAADSR::noteOn()
{
    level = 0.0;
    state = State::Attack;
}

void NIKAADSR::noteOnFromLevel (double startLevel)
{
    level = startLevel;          // continue attack from current level — no snap
    state = State::Attack;
}

void NIKAADSR::noteOff()
{
    if (state != State::Idle)
        state = State::Release;
}

//==============================================================================
float NIKAADSR::getNextSample()
{
    switch (state)
    {
        case State::Idle:
            break;

        case State::Attack:
            // RC-style exponential rise: approach virtual target (1 + kAttackOffset).
            // Clamps to 1.0 when reached, matching SH-101 capacitor-charge behaviour.
            level = (1.0 + kAttackOffset) + (level - (1.0 + kAttackOffset)) * attackCoeff;
            if (level >= 1.0)
            {
                level = 1.0;
                state = State::Decay;
            }
            break;

        case State::Decay:
            // Exponential fall toward 0; clip at sustainLevel.
            // Extra guard (level < 1e-4) handles sustainLevel == 0 since the
            // curve is asymptotic and never reaches exactly 0.
            level *= decayCoeff;
            if (level <= sustainLevel || level < 1e-4)
            {
                level = sustainLevel;
                state = State::Sustain;
            }
            break;

        case State::Sustain:
            // Track sustainLevel in real time so moving the knob during hold
            // takes effect without a state transition.
            level = sustainLevel;
            break;

        case State::Release:
            level *= releaseCoeff;
            if (level < 1e-4)
            {
                level = 0.0;
                state = State::Idle;
            }
            break;
    }

    return (float)level;
}
