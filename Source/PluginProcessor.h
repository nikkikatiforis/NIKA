#pragma once
#include <JuceHeader.h>
#include "Oscillator.h"
#include "LadderFilter.h"
#include "ADSR.h"
#include "Compressor.h"
#include "Saturator.h"
#include "Limiter.h"
#include "KeyswitchEngine.h"
#include "FXEngine.h"

class NIKAAudioProcessor : public juce::AudioProcessor
{
public:
    NIKAAudioProcessor();
    ~NIKAAudioProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool  acceptsMidi()    const override { return true; }
    bool  producesMidi()   const override { return false; }
    bool  isMidiEffect()   const override { return false; }
    double getTailLengthSeconds() const override { return 10.0; }

    int  getNumPrograms()    override { return 1; }
    int  getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    juce::AudioProcessorValueTreeState apvts;

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    int currentPresetIndex = 0;

    // Per-keyswitch last-trigger timestamp (ms). Index 0 = C-1 (note 12).
    // Written on audio thread, read on UI thread for indicator dots.
    std::atomic<double> ksTriggerTimesMs[7];

    // Per-keyswitch release timestamp (ms). Reset to 0 on trigger.
    // UI reads this to start the fade only after note-off.
    std::atomic<double> ksReleaseTimesMs[7];

    // Per-keyswitch held flag. True while note is physically held.
    std::atomic<bool> ksHeldFlags[7];

    // True when all voices are simultaneously active (voice limit reached).
    // Written every audio block on the audio thread; read by the UI thread for VLI.
    std::atomic<bool> voiceLimitActive { false };

    // UI-driven keyswitch trigger/release (called from editor mouseDown/Up).
    void triggerKeyswitch (int idx, float vel);
    void releaseKeyswitch (int idx);

private:
    //==========================================================================
    // Voice — one complete synthesis path (osc → filter → ADSR)
    //
    // Three voices provide true polyphony.  The compressor, saturator, FX
    // engine, and limiter are shared and receive the summed mix.
    //
    // Fade-in:   on every trigger (stolen or free) a 2 ms linear ramp from 0
    //            to 1 is applied to fadeGain, masking phase/filter transients.
    // No phase reset: osc.setFrequency() is called without osc.reset() so the
    //            oscillator phase is continuous across retriggers.
    //==========================================================================
    struct Voice
    {
        NIKAOscillator   osc;
        NIKALadderFilter filter;
        NIKAADSR         adsr;

        int   note        = -1;    // MIDI note currently assigned, -1 = free
        float velocity    = 0.0f;
        int   triggeredAt = 0;     // global voiceAge stamp — for oldest-first steal

        float fadeGain    = 1.0f;  // current fade-in gain [0..1]
        float fadeInc     = 0.0f;  // per-sample increment during fade-in

        float portFreq    = 440.0f;  // current gliding frequency
        float portTarget  = 440.0f;  // glide target frequency
        float portCoeff   = 0.0f;    // one-pole glide coefficient (40 ms)
        bool  gliding     = false;   // true while portamento is active

        // Two-phase steal: fade-out old note before reassigning
        bool  stealing      = false;   // true during 7 ms fade-out
        int   stealNote_    = -1;      // pending MIDI note number
        float stealFreq_    = 440.0f;  // pending note frequency (Hz)
        float stealVel_     = 0.0f;    // pending velocity
        float stealFadeInc_ = 0.0f;    // negative per-sample decrement for fade-out

    };

    static constexpr int kNumVoices = 7;

    Voice             voices[kNumVoices];
    int               voiceAge = 0;       // monotonically increasing trigger counter

    // Returns index of the voice to use for `note`:
    //   1. same note already active → retrigger that voice
    //   2. free (idle) voice exists → use it
    //   3. all busy → steal the oldest (lowest triggeredAt)
    int allocateVoice (int note) noexcept;

    //==========================================================================
    // Shared post-mix processing
    NIKACompressor      compressor;
    NIKASaturator       saturator;
    NIKALimiter         limiter;
    NIKAKeyswitchEngine ksEngine;
    NIKAFXEngine        fxEngine;

    //==========================================================================
    // Cached param pointers — written once at construction, read on audio thread
    std::atomic<float>* sawLevelParam     = nullptr;
    std::atomic<float>* squareLevelParam  = nullptr;
    std::atomic<float>* pulseLevelParam   = nullptr;
    std::atomic<float>* pulseWidthParam   = nullptr;
    std::atomic<float>* subLevelParam     = nullptr;
    std::atomic<float>* noiseLevelParam   = nullptr;
    std::atomic<float>* cutoffParam       = nullptr;
    std::atomic<float>* resonanceParam    = nullptr;
    std::atomic<float>* attackParam       = nullptr;
    std::atomic<float>* decayParam        = nullptr;
    std::atomic<float>* sustainParam      = nullptr;
    std::atomic<float>* releaseParam      = nullptr;
    std::atomic<float>* filterEnvAmtParam = nullptr;
    std::atomic<float>* satDriveParam      = nullptr;
    std::atomic<float>* ksDepthParam       = nullptr;
    std::atomic<float>* fxPatchParam       = nullptr;
    std::atomic<float>* fxMixParam         = nullptr;
    std::atomic<float>* monoParam          = nullptr;

    // One-pole smoothers — 16 ms time constant on audio thread
    float cutoffSmoothed_    = 1000.0f;
    float resonanceSmoothed_ = 0.09375f;

    // Pitch bend and mod wheel state (audio thread)
    float pitchBendMult_    = 1.0f;
    float modWheelOctaves_  = 0.0f;
    float modWheelSmoothed_ = 0.0f;

    // M/S width smoother — 128 ms TC; target sqrt(2) when drive on, 1.0 off
    float msWidthSmoothed_  = 1.0f;
    float msWidthSlewCoeff_ = 0.0f;

    // Drive slew — 4 ms TC; smooths sat drive toggle to remove click
    float satDriveSmoothed_  = 0.0f;
    float satDriveSlewCoeff_ = 0.0f;

    // Mono note stack — tracks held notes oldest-first; top = most recent
    static constexpr int kNoteStackSize = 8;
    int noteStack_[kNoteStackSize] = {};
    int noteStackTop_ = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NIKAAudioProcessor)
};
