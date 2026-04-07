#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout NIKAAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    // --- Oscillator mixer levels (steps 0-32) --------------------------------
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        "sawLevel", "Saw",
        juce::NormalisableRange<float> (0.0f, 32.0f, 1.0f), 32.0f));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        "squareLevel", "Sqr",
        juce::NormalisableRange<float> (0.0f, 32.0f, 1.0f), 2.0f));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        "pulseLevel", "Pls",
        juce::NormalisableRange<float> (0.0f, 32.0f, 1.0f), 4.0f));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        "pulseWidth", "PW",
        juce::NormalisableRange<float> (0.0f, 32.0f, 1.0f), 16.0f));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        "subLevel", "Sub",
        juce::NormalisableRange<float> (0.0f, 32.0f, 1.0f), 0.0f));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        "noiseLevel", "Noise",
        juce::NormalisableRange<float> (0.0f, 32.0f, 1.0f), 2.0f));

    // --- VCF ----------------------------------------------------------------
    // Log-skewed range: skew = 1/e ≈ 0.368; range 16–16384 Hz
    // Default = step 16 = 16 * 1024^0.5 = 512 Hz (Preset 01)
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        "cutoff", "Cut",
        juce::NormalisableRange<float> (16.0f, 16384.0f, 0.1f,
                                        1.0f / juce::MathConstants<float>::euler), 512.0f));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        "resonance", "Res",
        juce::NormalisableRange<float> (0.0f, 32.0f, 1.0f), 3.0f));

    // --- ADSR ---------------------------------------------------------------
    // Steps 0-32; processor converts to seconds using exp curve.
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        "attack", "Atk",
        juce::NormalisableRange<float> (0.0f, 32.0f, 1.0f), 7.0f));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        "decay", "Dec",
        juce::NormalisableRange<float> (0.0f, 32.0f, 1.0f), 26.0f));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        "sustain", "Sus",
        juce::NormalisableRange<float> (0.0f, 32.0f, 1.0f), 10.0f));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        "release", "Rel",
        juce::NormalisableRange<float> (0.0f, 32.0f, 1.0f), 24.0f));

    // How far the envelope opens the filter above the base cutoff.
    // Modulation is logarithmic: at full depth, env sweeps up to 5 octaves.
    // Env target is hardcoded at 2 (Both) — no UI parameter.
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        "filterEnvAmt", "FAmt",
        juce::NormalisableRange<float> (0.0f, 32.0f, 1.0f), 19.0f));

    // --- Output stage -------------------------------------------------------
    // Saturator
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        "satDrive", "Fairy Tales",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.0f));

    // --- Keyswitch ----------------------------------------------------------
    // Master depth scales how far the ks filter envelope opens the cutoff.
    // Velocity further scales within this ceiling.
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        "ksDepth", "Depth",
        juce::NormalisableRange<float> (0.0f, 32.0f, 1.0f), 20.0f));

    // --- Effects ------------------------------------------------------------
    // 7 patch slots.
    layout.add (std::make_unique<juce::AudioParameterInt> (
        "fxPatch", "Patch", 1, 7, 1));

    // 0 = dry only, 32 = fully wet.
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        "fxMix", "Mix",
        juce::NormalisableRange<float> (0.0f, 32.0f, 1.0f), 8.0f));

    // --- Mono mode ----------------------------------------------------------
    layout.add (std::make_unique<juce::AudioParameterBool> (
        "mono", "Mono", false));

    return layout;
}

//==============================================================================
NIKAAudioProcessor::NIKAAudioProcessor()
    : AudioProcessor (BusesProperties()
                      .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "NIKAState", createParameterLayout())
{
    for (auto& t : ksTriggerTimesMs)  t.store (0.0);
    for (auto& t : ksReleaseTimesMs) t.store (0.0);
    for (auto& f : ksHeldFlags)      f.store (false);
    sawLevelParam     = apvts.getRawParameterValue ("sawLevel");
    squareLevelParam  = apvts.getRawParameterValue ("squareLevel");
    pulseLevelParam   = apvts.getRawParameterValue ("pulseLevel");
    pulseWidthParam   = apvts.getRawParameterValue ("pulseWidth");
    subLevelParam     = apvts.getRawParameterValue ("subLevel");
    noiseLevelParam   = apvts.getRawParameterValue ("noiseLevel");
    cutoffParam       = apvts.getRawParameterValue ("cutoff");
    resonanceParam    = apvts.getRawParameterValue ("resonance");
    attackParam       = apvts.getRawParameterValue ("attack");
    decayParam        = apvts.getRawParameterValue ("decay");
    sustainParam      = apvts.getRawParameterValue ("sustain");
    releaseParam      = apvts.getRawParameterValue ("release");
    filterEnvAmtParam  = apvts.getRawParameterValue ("filterEnvAmt");
    satDriveParam      = apvts.getRawParameterValue ("satDrive");
    ksDepthParam       = apvts.getRawParameterValue ("ksDepth");
    fxPatchParam       = apvts.getRawParameterValue ("fxPatch");
    fxMixParam         = apvts.getRawParameterValue ("fxMix");
    monoParam          = apvts.getRawParameterValue ("mono");
}

NIKAAudioProcessor::~NIKAAudioProcessor() {}

//==============================================================================
void NIKAAudioProcessor::prepareToPlay (double sampleRate, int)
{
    const float portCoeff40ms = 1.0f - std::exp (-1.0f / (0.040f * float (sampleRate)));

    for (auto& v : voices)
    {
        v.osc.prepare    (sampleRate);
        v.filter.prepare (sampleRate);
        v.adsr.prepare   (sampleRate);
        v.note        = -1;
        v.velocity    = 0.0f;
        v.triggeredAt = 0;
        v.fadeGain    = 1.0f;
        v.fadeInc     = 0.0f;
        v.portFreq    = 440.0f;
        v.portTarget  = 440.0f;
        v.portCoeff   = portCoeff40ms;
        v.gliding     = false;
        v.stealing    = false;
    }
    voiceAge     = 0;
    noteStackTop_ = 0;
    voiceLimitActive.store (false);

    compressor.prepare (sampleRate);
    saturator.prepare  (sampleRate);
    limiter.prepare    (sampleRate);
    ksEngine.prepare   (sampleRate);
    fxEngine.prepare   (sampleRate);
    setLatencySamples  (NIKALimiter::kLookahead);

    cutoffSmoothed_    = cutoffParam->load();
    resonanceSmoothed_ = resonanceParam->load();
    pitchBendMult_     = 1.0f;
    modWheelOctaves_   = 0.0f;
    modWheelSmoothed_  = 0.0f;
    msWidthSmoothed_   = 1.0f;
    msWidthSlewCoeff_  = 1.0f - std::exp (-1.0f / (0.128f * float (sampleRate)));
    satDriveSmoothed_  = satDriveParam->load();
    satDriveSlewCoeff_ = 1.0f - std::exp (-1.0f / (0.004f * float (sampleRate)));
}

//==============================================================================
// Voice allocation
//
//  1. Same note already active → retrigger that voice in-place.
//  2. Free (idle) voice        → use it.
//  3. All busy                 → steal the oldest (lowest triggeredAt value).
//==============================================================================
int NIKAAudioProcessor::allocateVoice (int note) noexcept
{
    // Retrigger
    for (int i = 0; i < kNumVoices; ++i)
        if (voices[i].note == note && voices[i].adsr.isActive())
            return i;

    // Free voice
    for (int i = 0; i < kNumVoices; ++i)
        if (!voices[i].adsr.isActive())
            return i;

    // Steal oldest
    int oldest = 0;
    for (int i = 1; i < kNumVoices; ++i)
        if (voices[i].triggeredAt < voices[oldest].triggeredAt)
            oldest = i;
    return oldest;
}

void NIKAAudioProcessor::releaseResources() {}

bool NIKAAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

//==============================================================================
void NIKAAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                     juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();

    const bool monoMode = monoParam->load() > 0.5f;

    // --- MIDI ---------------------------------------------------------------
    // Notes 12–18 (C-1 to F#-1, Ableton) are keyswitches.
    //
    // The range check (note >= kLowNote && note <= kHighNote) is evaluated
    // first, before any JUCE note-type query.  A `continue` then
    // unconditionally skips the rest of the loop body so that a keyswitch
    // message can never fall through to the voice allocator regardless of
    // velocity, host behaviour, or JUCE isNoteOn/isNoteOff edge-cases.
    {
        juce::MidiBuffer filtered;
        for (const auto& meta : midi)
        {
            const auto msg  = meta.getMessage();
            const int  note = msg.getNoteNumber();

            // ---- Keyswitch interception (notes 0–11) ---------------------
            if (note >= NIKAKeyswitchEngine::kLowNote
                    && note <= NIKAKeyswitchEngine::kHighNote
                    && (msg.isNoteOn() || msg.isNoteOff()))
            {
                if (msg.isNoteOn())
                {
                    ksEngine.trigger (note, msg.getFloatVelocity());
                    const int ksIdx = note - NIKAKeyswitchEngine::kLowNote;
                    ksTriggerTimesMs[ksIdx].store (juce::Time::getMillisecondCounterHiRes());
                    ksReleaseTimesMs[ksIdx].store (0.0);
                    ksHeldFlags[ksIdx].store (true);
                }
                else
                {
                    ksEngine.noteOff (note);
                    const int ksIdx = note - NIKAKeyswitchEngine::kLowNote;
                    ksReleaseTimesMs[ksIdx].store (juce::Time::getMillisecondCounterHiRes());
                    ksHeldFlags[ksIdx].store (false);
                }
                continue;   // consumed — must not reach voice allocator
            }

            // ---- Silent note range (notes 19–23, G-1 to B-1 in Ableton) ---
            if (note >= 19 && note <= 23 && (msg.isNoteOn() || msg.isNoteOff()))
                continue;

            // ---- Pitch bend ±2 semitones -----------------------------------
            if (msg.isPitchWheel())
            {
                const float semitones = float (msg.getPitchWheelValue() - 8192) / 8192.0f * 2.0f;
                pitchBendMult_ = std::exp2 (semitones / 12.0f);
            }

            // ---- Mod wheel (CC1) → 0–4 octave cutoff offset ---------------
            if (msg.isController() && msg.getControllerNumber() == 1)
                modWheelOctaves_ = float (msg.getControllerValue()) / 127.0f * 4.0f;

            // ---- All other messages -------------------------------------
            filtered.addEvent (msg, meta.samplePosition);

            if (msg.isNoteOn())
            {
                const float noteFreq = juce::MidiMessage::getMidiNoteInHertz (note);
                const float fadeInc  = 1.0f / float (std::max (int (0.007 * getSampleRate()), 1));

                if (monoMode)
                {
                    // Update note stack: remove existing occurrence, push to top
                    int newTop = 0;
                    for (int s = 0; s < noteStackTop_; ++s)
                        if (noteStack_[s] != note)
                            noteStack_[newTop++] = noteStack_[s];
                    if (newTop < kNoteStackSize)
                        noteStack_[newTop++] = note;
                    noteStackTop_ = newTop;

                    // Release any active non-primary voices
                    for (int vi = 1; vi < kNumVoices; ++vi)
                        if (voices[vi].adsr.isActive())
                        {
                            voices[vi].note = -1;
                            voices[vi].adsr.noteOff();
                        }

                    Voice& v = voices[0];
                    const bool wasActive = v.adsr.isActive();

                    v.note        = note;
                    v.velocity    = msg.getFloatVelocity();
                    v.triggeredAt = voiceAge++;
                    v.portTarget  = noteFreq;
                    v.portCoeff   = 1.0f - std::exp (-1.0f / (0.040f * float (getSampleRate())));

                    if (wasActive)
                    {
                        // Glide from current portFreq — no amplitude or phase jump
                        v.gliding = true;
                        v.adsr.noteOnFromLevel ((double)v.adsr.getLevel());
                        // fadeGain stays at current level — no gap
                    }
                    else
                    {
                        // Fresh voice: snap immediately, no glide
                        v.portFreq = noteFreq;
                        v.gliding  = false;
                        v.filter.reset();
                        v.adsr.noteOn();
                        v.fadeGain = 0.0f;
                        v.fadeInc  = fadeInc;
                    }
                }
                else
                {
                    const int vi = allocateVoice (note);
                    Voice& v     = voices[vi];
                    const bool stolen = v.adsr.isActive();

                    if (stolen)
                    {
                        // Phase 1: initiate fade-out; defer note assignment.
                        // If already stealing, just update the pending target.
                        v.triggeredAt  = voiceAge++;
                        v.stealing     = true;
                        v.stealNote_   = note;
                        v.stealFreq_   = noteFreq;
                        v.stealVel_    = msg.getFloatVelocity();
                        v.stealFadeInc_ = -fadeInc;   // negative = fade out
                    }
                    else
                    {
                        // Free voice: normal immediate start
                        v.note        = note;
                        v.velocity    = msg.getFloatVelocity();
                        v.triggeredAt = voiceAge++;
                        v.portFreq    = noteFreq;
                        v.portTarget  = noteFreq;
                        v.gliding     = false;
                        v.stealing    = false;
                        v.filter.reset();
                        v.adsr.noteOn();
                        v.fadeGain = 0.0f;
                        v.fadeInc  = fadeInc;
                    }
                }
            }
            else if (msg.isNoteOff())
            {
                if (monoMode)
                {
                    // Remove released note from stack
                    int newTop = 0;
                    for (int s = 0; s < noteStackTop_; ++s)
                        if (noteStack_[s] != note)
                            noteStack_[newTop++] = noteStack_[s];
                    noteStackTop_ = newTop;

                    if (voices[0].note == note && voices[0].adsr.isActive())
                    {
                        if (noteStackTop_ > 0)
                        {
                            // Retrigger the most recently held note with glide
                            const int   prevNote = noteStack_[noteStackTop_ - 1];
                            const float prevFreq = juce::MidiMessage::getMidiNoteInHertz (prevNote);
                            Voice& v = voices[0];
                            v.note       = prevNote;
                            v.portTarget = prevFreq;
                            v.portCoeff  = 1.0f - std::exp (-1.0f / (0.040f * float (getSampleRate())));
                            v.gliding    = true;
                            v.adsr.noteOnFromLevel ((double)v.adsr.getLevel());
                            // velocity and fadeGain unchanged — seamless continuation
                        }
                        else
                        {
                            voices[0].note = -1;
                            voices[0].adsr.noteOff();
                        }
                    }
                }
                else
                {
                    bool handled = false;
                    for (int vi = 0; vi < kNumVoices && !handled; ++vi)
                    {
                        Voice& v = voices[vi];

                        // Note-off arrives for a note pending as a steal target:
                        // cancel the steal so it never starts playing.
                        if (v.stealing && v.stealNote_ == note)
                        {
                            v.stealing   = false;
                            v.stealNote_ = -1;
                            handled = true;
                        }
                        else if (v.note == note && v.adsr.isActive())
                        {
                            v.note = -1;
                            v.adsr.noteOff();
                            // If this voice was mid-steal, cancel the pending note
                            // and prevent the fade-in ramp from re-triggering.
                            if (v.stealing)
                            {
                                v.stealing   = false;
                                v.stealNote_ = -1;
                                v.fadeInc    = 0.0f;
                            }
                            handled = true;
                        }
                    }
                }
            }
        }
        midi.swapWith (filtered);
    }

    // --- Voice limit indicator ----------------------------------------------
    {
        int nActive = 0;
        for (auto& v : voices)
            if (v.adsr.isActive()) ++nActive;
        voiceLimitActive.store (nActive >= kNumVoices);
    }

    // --- Snapshot all block-level params ------------------------------------
    // Steps [0..32] → amplitudes via e² curve: amp = (exp(2x)−1)/(e²−1), x=step/32
    static constexpr float kE2 = 6.38905609893f;
    auto stepToAmp = [] (float step) {
        const float x = step / 32.0f;
        return (std::exp (2.0f * x) - 1.0f) / kE2;
    };

    const float ksDepth   = ksDepthParam->load()   / 32.0f;
    const float fxMix     = fxMixParam->load()     / 32.0f;
    const float sawLvl    = stepToAmp (sawLevelParam->load());
    const float sqLvl     = stepToAmp (squareLevelParam->load());
    const float pulLvl    = stepToAmp (pulseLevelParam->load());
    const float pw        = juce::jmap (pulseWidthParam->load(), 0.0f, 32.0f, 0.05f, 0.95f);
    const float subLvl    = stepToAmp (subLevelParam->load());
    const float noiseLvl  = stepToAmp (noiseLevelParam->load());
    const float cutoffTarget    = cutoffParam->load();
    const float resonanceTarget = resonanceParam->load() / 32.0f;
    const float slewCoeff       = 1.0f - std::exp (-1.0f / (0.016f * float (getSampleRate())));
    constexpr float gain  = 0.8f;
    constexpr int envTarget = 2;   // hardcoded: Both (amp + filter)
    const float fEnvAmt   = filterEnvAmtParam->load() / 32.0f;  // steps → [0..1] → up to 5 octaves

    // ADSR: steps [0,32] → seconds via exp curve
    const float attackSec  = 0.0005f * std::pow (4.0f    / 0.0005f, attackParam->load()  / 32.0f);
    const float decaySec   = 0.004f  * std::pow (4.0f    / 0.004f,  decayParam->load()   / 32.0f);
    const float sustainLvl = sustainParam->load() / 32.0f;
    const float releaseSec = 0.002f  * std::pow (8.0f    / 0.002f,  releaseParam->load() / 32.0f);
    for (auto& v : voices)
        v.adsr.setParameters (attackSec, decaySec, sustainLvl, releaseSec);

    // Drive (СКАЗКУ): 16 ms slew on sat drive for click-free switching.
    // Compressor threshold shifts: -24 dB (off) → -8 dB (on). Ratio fixed at φ.
    const float satDriveTarget = satDriveParam->load();
    satDriveSmoothed_ += satDriveSlewCoeff_ * (satDriveTarget - satDriveSmoothed_);
    const float compThresh = -24.0f + satDriveSmoothed_ * 16.0f;  // -24 dB → -8 dB
    compressor.setParameters (compThresh, 1.618f);
    saturator.setDrive (satDriveSmoothed_);
    fxEngine.setDriveTarget   (satDriveTarget > 0.5f ? 1.0f : 0.0f);
    fxEngine.setPatch         ((int)fxPatchParam->load());

    // BPM from host transport; fall back to 120 if unavailable.
    double bpm = 120.0;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
            if (auto bpmOpt = pos->getBpm())
                bpm = *bpmOpt;
    fxEngine.setBpm (bpm);

    auto* L = buffer.getWritePointer (0);
    auto* R = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1) : nullptr;

    // --- Per-sample signal chain --------------------------------------------
    //
    // Each voice: osc → filter → ADSR gate → mix
    // Shared post-mix: compressor → saturator → FX → M/S width → +13 dB → limiter
    //
    // Filter modulation (log-octave, additive before exp2()):
    //   Active voices:   ksOctaves + filterEnv (if envTarget 1/2)
    //   Idle voices:     ksOctaves only (pre-sets filter for next note-on)
    //
    // Keyswitch ksOctaves is applied to every voice simultaneously.
    //
    // FX and limiter always run so reverb/delay tails survive note-off.
    //
    for (int i = 0; i < buffer.getNumSamples(); ++i)
    {
        // 16 ms one-pole smoothers (shared slewCoeff)
        cutoffSmoothed_    += slewCoeff * (cutoffTarget    - cutoffSmoothed_);
        resonanceSmoothed_ += slewCoeff * (resonanceTarget - resonanceSmoothed_);
        modWheelSmoothed_  += slewCoeff * (modWheelOctaves_ - modWheelSmoothed_);

        // Keyswitch envelope advances every sample regardless of voice state
        const float ksOctaves = ksEngine.getNextOctaves() * ksDepth;

        float lSamp = 0.0f, rSamp = 0.0f;

        for (int vi = 0; vi < kNumVoices; ++vi)
        {
            Voice& v = voices[vi];

            // Advance portamento glide
            if (v.gliding)
            {
                v.portFreq += v.portCoeff * (v.portTarget - v.portFreq);
                if (std::abs (v.portTarget - v.portFreq) < 0.01f)
                {
                    v.portFreq = v.portTarget;
                    v.gliding  = false;
                }
            }

            // Two-phase steal: advance fade-out; complete steal when gain hits 0
            if (v.stealing)
            {
                v.fadeGain = std::max (0.0f, v.fadeGain + v.stealFadeInc_);
                if (v.fadeGain <= 0.0f)
                {
                    const double savedLevel = (double)v.adsr.getLevel();
                    v.stealing   = false;
                    v.note       = v.stealNote_;
                    v.velocity   = v.stealVel_;
                    v.portFreq   = v.stealFreq_;
                    v.portTarget = v.stealFreq_;
                    v.gliding    = false;
                    v.adsr.noteOnFromLevel (savedLevel);
                    v.fadeInc  = 1.0f / float (std::max (int (0.007 * getSampleRate()), 1));
                    v.fadeGain = 0.0f;
                    // filter NOT reset — intentional carry
                }
            }

            // Apply pitch bend after steal may have updated portFreq
            v.osc.setFrequency (v.portFreq * pitchBendMult_);

            if (v.adsr.isActive())
            {
                const float envValue = v.adsr.getNextSample();

                float totalOctaves = ksOctaves + modWheelSmoothed_;
                if (envTarget == 1 || envTarget == 2)
                    totalOctaves += fEnvAmt * 5.0f * envValue;

                const float modCutoff = (totalOctaves != 0.0f)
                    ? juce::jlimit (16.0f, 20000.0f, cutoffSmoothed_ * std::exp2 (totalOctaves))
                    : cutoffSmoothed_;
                v.filter.setParameters (modCutoff, resonanceSmoothed_);

                // Advance fade-in ramp (not during steal fade-out)
                if (!v.stealing && v.fadeGain < 1.0f)
                    v.fadeGain = std::min (1.0f, v.fadeGain + v.fadeInc);

                float sample = v.osc.process (sawLvl, sqLvl, pulLvl, pw, subLvl, noiseLvl);

                sample = v.filter.process (sample);

                if (envTarget == 0 || envTarget == 2)
                    sample *= envValue;

                const float voiceOut = sample * v.velocity * gain * 0.36788f * v.fadeGain;  // 1/e

                lSamp += voiceOut;
                rSamp += voiceOut;
            }
            else
            {
                // Idle voice: track keyswitch so filter is ready on next note-on
                const float idleOctaves = ksOctaves + modWheelSmoothed_;
                const float modCutoff = (idleOctaves != 0.0f)
                    ? juce::jlimit (16.0f, 20000.0f, cutoffSmoothed_ * std::exp2 (idleOctaves))
                    : cutoffSmoothed_;
                v.filter.setParameters (modCutoff, resonanceSmoothed_);
            }
        }

        compressor.process (lSamp, rSamp);
        saturator.process  (lSamp, rSamp);

        // FX always runs — reverb/delay tails survive note-off
        fxEngine.process (lSamp, rSamp, fxMix);

        // M/S width boost — drive on: +4.2 dB side channel (× φ), 128 ms slew
        {
            const float widthTarget = 1.0f + satDriveSmoothed_ * 0.618033988f;
            msWidthSmoothed_ += msWidthSlewCoeff_ * (widthTarget - msWidthSmoothed_);
            const float m = (lSamp + rSamp) * 0.5f;
            const float s = (lSamp - rSamp) * 0.5f * msWidthSmoothed_;
            lSamp = m + s;
            rSamp = m - s;
        }

        // Output gain: φ constant (+4.2 dB); drive-on adds √φ makeup (+2.1 dB)
        const float outMult = 1.618033988f * (1.0f + satDriveSmoothed_ * 0.27201964951f);
        lSamp *= outMult;
        rSamp *= outMult;

        // Brickwall limiter — must be last; catches any post-gain transients
        limiter.process  (lSamp, rSamp);   // ceiling: 0 dBFS

        L[i] = lSamp;
        if (R) R[i] = rSamp;
    }
}

//==============================================================================
void NIKAAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    xml->setAttribute ("currentPresetIndex", currentPresetIndex);
    copyXmlToBinary (*xml, destData);
}

void NIKAAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml (getXmlFromBinary (data, sizeInBytes));
    if (xml != nullptr && xml->hasTagName (apvts.state.getType()))
    {
        currentPresetIndex = xml->getIntAttribute ("currentPresetIndex", 0);
        apvts.replaceState (juce::ValueTree::fromXml (*xml));
    }
}

juce::AudioProcessorEditor* NIKAAudioProcessor::createEditor()
{
    return new NIKAAudioProcessorEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new NIKAAudioProcessor();
}

//==============================================================================
void NIKAAudioProcessor::triggerKeyswitch (int idx, float vel)
{
    jassert (idx >= 0 && idx < 7);
    ksEngine.trigger (NIKAKeyswitchEngine::kLowNote + idx, vel);
    ksTriggerTimesMs[idx].store (juce::Time::getMillisecondCounterHiRes());
    ksReleaseTimesMs[idx].store (0.0);
    ksHeldFlags[idx].store (true);
}

void NIKAAudioProcessor::releaseKeyswitch (int idx)
{
    jassert (idx >= 0 && idx < 7);
    ksEngine.noteOff (NIKAKeyswitchEngine::kLowNote + idx);
    ksReleaseTimesMs[idx].store (juce::Time::getMillisecondCounterHiRes());
    ksHeldFlags[idx].store (false);
}
