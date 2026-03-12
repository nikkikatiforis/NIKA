#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"

//==============================================================================
// NIKAAudioProcessorEditor — ISKRA terminal aesthetic
//
// All parameters displayed as draggable text fields on a CRT-phosphor terminal.
// Drag up/down on any field to change its value.  Scroll-wheel also works.
// Active field is inverted (dark text, bright bg).
//
// Layout (384 × 310 px):
//   Header : "НИКА СИНТЕЗАТОР В.01" left  |  [◄ 01 ►] preset box right
//   OSC+VCF: SAW SQR PLS / PW SUB NSE     |  CUTOFF  RESO
//   ADSR+FX: ATK DEC SUS / REL FAMT       |  FX◄N►  KS
//                                          |  MIX:##  DEPTH:##
//   Footer : МЫ РОЖДЕНЫ, ЧТОБ [СКАЗКУ] СДЕЛАТЬ БЫЛЬЮ
//   KS dots: 7 clustered ornamental dots below footer
//
// Visual effects (outermost last):
//   1. Background fill
//   2. Content draw  (normal alpha)
//   3. Bloom layer   (y+1, 15 % alpha)
//   4. Radial vignette
//   5. Scanlines     (1 px dark band every 2 px)
//
// СКАЗКУ flicker system (drive OFF only):
//   Master gate G(t) = sin(2π×0.08t)×sin(2π×0.05t+π/2) + 0.382×sin(2π×0.03t)
//   Active when G(t) > 0.382.  Boot burst adds 1.0 to G(t) for first 3 s.
//   Layer 1: phosphor flutter — three oscillators normalised 0-1
//   Layer 2: noise gate stutter — Fibonacci toggle periods {16,21,34,55} ms
//   Layer 3: hard blackout spikes — 8% / 100 ms, durations {8,13,21,34} ms
//   Silent passage (G≤0.382): dim static phosphor (no animation)
//==============================================================================
class NIKAAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit NIKAAudioProcessorEditor (NIKAAudioProcessor&);
    ~NIKAAudioProcessorEditor() override;

    void paint             (juce::Graphics&) override;
    void resized           () override;
    void visibilityChanged () override;
    void mouseDown      (const juce::MouseEvent&) override;
    void mouseDrag      (const juce::MouseEvent&) override;
    void mouseUp        (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&,
                         const juce::MouseWheelDetails&) override;

private:
    using PA = juce::ParameterAttachment;

    NIKAAudioProcessor& proc;

    //==========================================================================
    // Display steps (0–32) for each parameter
    int sSaw{0}, sSqr{0}, sPls{0}, sPw{16};
    int sSub{0}, sNoise{0};
    int sCutoff{16}, sReso{0};
    int sAtk{2}, sDec{12}, sSus{24}, sRel{16};
    int sEnvAmt{0};
    int sPatch{1};
    int sMix{16}, sDepth{8};
    bool driveOn_{false};
    bool monoOn_{false};
    bool initMode_{false};

    //==========================================================================
    // Presets — 7 factory slots
    static constexpr int kNumPresets = 7;
    int currentPreset_ { 0 };

    struct PresetData
    {
        int  saw{0}, sqr{0}, pls{0}, pw{16};
        int  sub{0}, noise{0};
        int  cutoff{16}, reso{0};
        int  atk{2}, dec{12}, sus{24}, rel{16};
        int  envAmt{0}, patch{1}, mix{16}, depth{8};
        bool drive{false}, mono{false};
    };
    PresetData presets_[kNumPresets];   // default-init fills all to above values

    //==========================================================================
    // Parameter attachments
    std::unique_ptr<PA> paSaw, paSqr, paPls, paPw, paSub, paNoise;
    std::unique_ptr<PA> paCutoff, paReso;
    std::unique_ptr<PA> paAtk, paDec, paSus, paRel, paEnvAmt;
    std::unique_ptr<PA> paPatch, paMix, paDepth, paDrive, paMono;

    //==========================================================================
    // Interactive slot table
    enum Slot
    {
        kSaw=0, kSqr, kPls, kPw, kSub, kNoise,   // 0-5  OSC
        kCutoff, kReso,                            // 6-7  VCF
        kAtk, kDec, kSus, kRel, kEnvAmt,          // 8-12 ADSR
        kPatchPrev, kPatchNext,                    // 13-14 FX patch arrows
        kMix, kDepth,                              // 15-16 FX mix / KS depth
        kDrive,                                    // 17   СКАЗКУ toggle
        kPresetPrev, kPresetNext, kPresetNum,      // 18-20 preset arrows + number
        kMono, kInit,                              // 21-22 header toggles
        kKsDot0, kKsDot1, kKsDot2, kKsDot3,       // 22-28 KS indicator dots
        kKsDot4, kKsDot5, kKsDot6,
        kNumSlots
    };

    juce::Rectangle<int> hitRects_[kNumSlots];
    int  activeSlot_    { -1 };
    int  dragStartY_    {  0 };
    int  dragStartStep_ {  0 };
    bool suppressCallbacks_ { false };   // true during loadPreset to block PA callback overwrites

    //==========================================================================
    float cw_ { 7.5f };   // measured monospace character width (set in resized)

    //==========================================================================
    // Helpers
    int*  stepPtr     (int slot) noexcept;
    void  setStep     (int slot, int newStep);
    void  pushParam   (int slot);
    void  savePreset  (int idx);
    void  loadPreset  (int idx);
    void  pushAllParams();

    void  drawContent  (juce::Graphics&, int yOff, float alpha) const;
    void  drawField    (juce::Graphics&, int slot,
                        const char* label, int step,
                        bool dim, int yOff, float alpha) const;
    void  drawScanlines (juce::Graphics&) const;
    void  drawVignette  (juce::Graphics&) const;

    //==========================================================================
    // Flicker system — updated once per timer tick in updateFlickerState()
    double loadTimeMs_          { juce::Time::getMillisecondCounterHiRes() };

    float  cachedFlicker_       { 0.0f };   // read by flickerBrightness()

    // Story arc script playback
    int    scriptIdx_           { 0 };      // next script to run [0..4], cycles A→E
    int    beatIdx_             { 0 };      // current beat index within script
    double beatEndMs_           { 0.0 };   // absolute time when current beat ends
    bool   scriptRunning_       { false };
    bool   beatLit_             { false };  // true = on-beat (bright)

    // Layer 3: hard blackout spikes
    double blackoutUntilMs_     { 0.0 };
    double nextBlackoutCheckMs_ { 0.0 };

    // Phosphor glow (drive active → labels/title/footer bloom)
    float  glowFade_            { 0.0f };   // [0,1] fades in/out 0.5 s with drive

    juce::Random flickerRng_;

    void  updateFlickerState() noexcept;
    void  drawGlowLabels (juce::Graphics&, int yOff, float alpha) const;

    // Flicker timer (inner class avoids multiple-inheritance with juce::Timer)
    class FlickerTimer : public juce::Timer
    {
    public:
        explicit FlickerTimer (NIKAAudioProcessorEditor& e) noexcept : owner (e) {}
        void timerCallback() override
        {
            owner.updateFlickerState();
            owner.repaint();
        }
    private:
        NIKAAudioProcessorEditor& owner;
        JUCE_DECLARE_NON_COPYABLE (FlickerTimer)
    };

    FlickerTimer flickerTimer_ { *this };
    float        flickerBrightness() const noexcept;
    float        breathBrightness()  const noexcept;  // 0.05 Hz sine → [0.6, 1.0]
    void         syncTimer()         noexcept;

    //==========================================================================
    static const juce::Colour kBg, kBright, kDim;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NIKAAudioProcessorEditor)
};
