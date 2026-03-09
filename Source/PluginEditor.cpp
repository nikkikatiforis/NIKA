#include "PluginEditor.h"
#include <cmath>

//==============================================================================
// Colour palette
//==============================================================================
const juce::Colour NIKAAudioProcessorEditor::kBg     { 0xFF080C08 };
const juce::Colour NIKAAudioProcessorEditor::kBright { 0xFFB8C9A8 };
const juce::Colour NIKAAudioProcessorEditor::kDim    { 0xFF3A4A3A };

//==============================================================================
// Layout constants
//==============================================================================
namespace
{
    constexpr int   kW      = 384;
    constexpr int   kH      = 320;
    constexpr int   kLM     = 20;       // left/right margin
    constexpr float kFontSz = 13.0f;
    constexpr int   kLH     = 22;       // line height (px)

    // ---- Row Y positions ---------------------------------------------------
    constexpr int yHdr    = 20;   // header: title + preset box
    constexpr int yR0     = 44;   // rule
    constexpr int ySecOsc = 57;   // "OSC" section label
    constexpr int yOsc1   = 78;   // OSC row 1: SAW SQR PLS  |  CUTOFF RESO
    constexpr int yOsc2   = 102;  // OSC row 2: PW  SUB NSE
    constexpr int yR1     = 128;  // rule
    constexpr int ySecAds = 141;  // "ADSR" label (left) / "FX" label + patch (right)
    constexpr int yAdsr1  = 162;  // ADSR row 1: ATK DEC SUS
    constexpr int yAdsr2  = 186;  // ADSR row 2: REL FAMT
    constexpr int yFxMix  = 163;  // FX/KS row 2: MIX:## (left) | DEPTH:## (right)
    constexpr int yKsDots   = 220;  // KS indicator dot row (below ADSR section)
    constexpr int yKsShapes = 234;  // KS articulation label row (14px below dot centres)
    constexpr int yR2     = 262;  // rule
    constexpr int yFtr    = 278;  // footer

    // ---- Column X positions ------------------------------------------------
    // Left section  (OSC params, ADSR params)
    constexpr int kLC0 = 20;    // col 0: SAW, PW,  ATK, REL
    constexpr int kLC1 = 76;    // col 1: SQR, SUB, DEC, FAMT
    constexpr int kLC2 = 132;   // col 2: PLS, NSE, SUS
    // Right section (VCF params, FX params)
    constexpr int kRC0 = 212;   // col 0: CUTOFF, FX-patch ◄, MIX
    constexpr int kRC1 = 300;   // col 1: RESO,               DEPTH

    // ---- UTF-8 Cyrillic strings --------------------------------------------
    // Drive off: Latin title
    static const char kTitleLatin[] = "NIKA SYNTHESISER V.01";
    // Drive on: Cyrillic title with double space before В
    static const char kTitle[]   = "\xd0\x9d\xd0\x98\xd0\x9a\xd0\x90 "
                                   "\xd0\xa1\xd0\x98\xd0\x9d\xd0\xa2\xd0\x95\xd0\x97\xd0\x90\xd0\xa2\xd0\x9e\xd0\xa0  "
                                   "\xd0\x92.01";
    // Footer parts
    static const char kFtrPre[]  = "\xd0\x9c\xd0\xab \xd0\xa0\xd0\x9e\xd0\x96\xd0\x94\xd0\x95\xd0\x9d\xd0\xab, "
                                   "\xd0\xa7\xd0\xa2\xd0\x9e\xd0\x91 ";
    static const char kFtrSk[]   = "\xd0\xa1\xd0\x9a\xd0\x90\xd0\x97\xd0\x9a\xd0\xa3";
    static const char kFtrPost[] = " \xd0\xa1\xd0\x94\xd0\x95\xd0\x9b\xd0\x90\xd0\xa2\xd0\xac "
                                   "\xd0\x91\xd0\xab\xd0\x9b\xd0\xac\xd0\xae";
    constexpr int kFtrPreGlyphs  = 17;
    constexpr int kFtrSkGlyphs   = 6;
    constexpr int kFtrPostGlyphs = 14;

    // ---- Unicode filled-triangle arrows (UTF-8) ----------------------------
    // ◄ U+25C4   ► U+25BA
    static const char kArrowL[] = "\xe2\x97\x84";
    static const char kArrowR[] = "\xe2\x96\xba";

    // ---- СКАЗКУ story arc scripts ------------------------------------------
    // Each script is a flat sequence of {lit, duration_ms} beats.
    // Scripts cycle A→B→C→D→E→A without repetition.
    struct ScriptBeat { bool lit; float ms; };

    // A: 2 slow blinks → 1 medium → 5 fast → cut
    static constexpr ScriptBeat kScriptA[] = {
        {true, 610},{false, 377}, {true, 610},{false, 377},   // slow ×2
        {true, 233},{false, 144},                              // medium ×1
        {true, 89},{false, 55},  {true, 89},{false, 55},
        {true, 89},{false, 55},  {true, 89},{false, 55},
        {true, 89},{false, 55},                                // fast ×5
    };
    // B: 1 medium → 3 fast → 2 slow → cut
    static constexpr ScriptBeat kScriptB[] = {
        {true, 233},{false, 144},                              // medium ×1
        {true, 89},{false, 55},  {true, 89},{false, 55},
        {true, 89},{false, 55},                                // fast ×3
        {true, 610},{false, 377},{true, 610},{false, 377},    // slow ×2
    };
    // C: 8 fast → sudden stop
    static constexpr ScriptBeat kScriptC[] = {
        {true, 89},{false, 55},  {true, 89},{false, 55},
        {true, 89},{false, 55},  {true, 89},{false, 55},
        {true, 89},{false, 55},  {true, 89},{false, 55},
        {true, 89},{false, 55},  {true, 89},                  // 8th: cut on last on-beat
    };
    // D: 2 slow → medium → burst 34ms rapid for ~377ms → cut
    static constexpr ScriptBeat kScriptD[] = {
        {true, 610},{false, 377},{true, 610},{false, 377},    // slow ×2
        {true, 233},{false, 144},                              // medium
        {true, 34},{false, 34},  {true, 34},{false, 34},
        {true, 34},{false, 34},  {true, 34},{false, 34},
        {true, 34},{false, 34},  {true, 34},                  // burst ×5+1 (374ms)
    };
    // E: 3 medium → silence 233ms → 4 fast → 2 slow → cut
    static constexpr ScriptBeat kScriptE[] = {
        {true, 233},{false, 144},{true, 233},{false, 144},
        {true, 233},{false, 144},                              // medium ×3
        {false, 233},                                          // silence
        {true, 89},{false, 55},  {true, 89},{false, 55},
        {true, 89},{false, 55},  {true, 89},{false, 55},      // fast ×4
        {true, 610},{false, 377},{true, 610},{false, 377},    // slow ×2
    };

    static const ScriptBeat* const kScripts[] =
        { kScriptA, kScriptB, kScriptC, kScriptD, kScriptE };
    static constexpr int kScriptLengths[] = {
        (int)std::size (kScriptA),
        (int)std::size (kScriptB),
        (int)std::size (kScriptC),
        (int)std::size (kScriptD),
        (int)std::size (kScriptE),
    };

    // ---- KS articulation ASCII labels (dim phosphor, indexed by slot) ------
    static const char* const kKsAscii[] = {
        "<",   // cresc
        ">",   // dim
        "/_",  // fp
        "<>",  // swell
        "^",   // sfz
        "o",   // pizz
        "~",   // trem
    };

    juce::Font termFont()
    {
        return juce::Font (juce::FontOptions (kFontSz)
                               .withName  ("Courier New")
                               .withStyle ("Bold"));
    }

    juce::String fmt2 (int s)
    {
        return juce::String (s).paddedLeft ('0', 2);
    }
}

//==============================================================================
// Constructor
//==============================================================================
NIKAAudioProcessorEditor::NIKAAudioProcessorEditor (NIKAAudioProcessor& p)
    : AudioProcessorEditor (&p), proc (p)
{
    auto& av = proc.apvts;

    // ---- Linear OSC attachments: step/32 → [minV, maxV] --------------------
    auto oscPA = [&] (const char* id, int* sp,
                      float minV, float maxV) -> std::unique_ptr<PA>
    {
        auto pa = std::make_unique<PA> (
            *av.getParameter (id),
            [this, sp, minV, maxV] (float v) {
                *sp = juce::jlimit (0, 32,
                    juce::roundToInt (juce::jmap (v, minV, maxV, 0.0f, 32.0f)));
                repaint();
            });
        pa->sendInitialUpdate();
        return pa;
    };

    paSaw   = oscPA ("sawLevel",   &sSaw,   0.0f,  1.0f);
    paSqr   = oscPA ("squareLevel",&sSqr,   0.0f,  1.0f);
    paPls   = oscPA ("pulseLevel", &sPls,   0.0f,  1.0f);
    paPw    = oscPA ("pulseWidth", &sPw,    0.05f, 0.95f);
    paSub   = oscPA ("subLevel",   &sSub,   0.0f,  1.0f);
    paNoise = oscPA ("noiseLevel", &sNoise, 0.0f,  1.0f);

    // ---- Cutoff: log-octave mapping -----------------------------------------
    paCutoff = std::make_unique<PA> (
        *av.getParameter ("cutoff"),
        [this] (float hz) {
            const float n = std::log (juce::jmax (16.0f, hz) / 16.0f)
                            / std::log (1024.0f);
            sCutoff = juce::jlimit (0, 32, juce::roundToInt (n * 32.0f));
            repaint();
        });
    paCutoff->sendInitialUpdate();

    // ---- Resonance: linear 0-1 ----------------------------------------------
    paReso = std::make_unique<PA> (
        *av.getParameter ("resonance"),
        [this] (float v) {
            sReso = juce::jlimit (0, 32, juce::roundToInt (v * 32.0f));
            repaint();
        });
    paReso->sendInitialUpdate();

    // ---- ADSR: exponential time mapping -------------------------------------
    auto adsrPA = [&] (const char* id, int* sp,
                       float mn, float mx) -> std::unique_ptr<PA>
    {
        auto pa = std::make_unique<PA> (
            *av.getParameter (id),
            [this, sp, mn, mx] (float v) {
                const float n = std::log (v / mn) / std::log (mx / mn);
                *sp = juce::jlimit (0, 32, juce::roundToInt (n * 32.0f));
                repaint();
            });
        pa->sendInitialUpdate();
        return pa;
    };

    paAtk = adsrPA ("attack",  &sAtk, 0.0005f, 4.0f);
    paDec = adsrPA ("decay",   &sDec, 0.004f,  4.0f);
    paRel = adsrPA ("release", &sRel, 0.002f,  8.0f);

    paSus = std::make_unique<PA> (
        *av.getParameter ("sustain"),
        [this] (float v) {
            sSus = juce::jlimit (0, 32, juce::roundToInt (v * 32.0f));
            repaint();
        });
    paSus->sendInitialUpdate();

    paEnvAmt = std::make_unique<PA> (
        *av.getParameter ("filterEnvAmt"),
        [this] (float v) {
            sEnvAmt = juce::jlimit (0, 32, juce::roundToInt (v * 32.0f));
            repaint();
        });
    paEnvAmt->sendInitialUpdate();

    // ---- Drive toggle -------------------------------------------------------
    paDrive = std::make_unique<PA> (
        *av.getParameter ("satDrive"),
        [this] (float v) {
            driveOn_ = (v > 0.5f);
            syncTimer();
            repaint();
        });
    paDrive->sendInitialUpdate();

    // ---- Mono toggle --------------------------------------------------------
    paMono = std::make_unique<PA> (
        *av.getParameter ("mono"),
        [this] (float v) {
            monoOn_ = (v > 0.5f);
            repaint();
        });
    paMono->sendInitialUpdate();

    // ---- FX patch (1-7) -----------------------------------------------------
    paPatch = std::make_unique<PA> (
        *av.getParameter ("fxPatch"),
        [this] (float v) {
            sPatch = juce::jlimit (1, 7, juce::roundToInt (v));
            repaint();
        });
    paPatch->sendInitialUpdate();

    // ---- FX mix / KS depth --------------------------------------------------
    paMix = std::make_unique<PA> (
        *av.getParameter ("fxMix"),
        [this] (float v) {
            sMix = juce::jlimit (0, 32, juce::roundToInt (v * 32.0f));
            repaint();
        });
    paMix->sendInitialUpdate();

    paDepth = std::make_unique<PA> (
        *av.getParameter ("ksDepth"),
        [this] (float v) {
            sDepth = juce::jlimit (0, 32, juce::roundToInt (v * 32.0f));
            repaint();
        });
    paDepth->sendInitialUpdate();

    // ---- Factory presets — slots 0–6, displayed as 01–07 -------------------
    // Fields: saw sqr pls pw sub noise cutoff reso atk dec sus rel envAmt patch mix depth drive mono
    presets_[0] = { 18,  0,  0,  2,  3,  0, 19,  5,  7, 13,  5, 21, 13,  1, 21,  8, false, false };
    presets_[1] = { 32, 32, 20, 32, 32,  4, 16, 16, 32, 32, 32, 32, 32,  5, 24,  8, false, false };
    presets_[2] = { 20,  0,  0,  2, 24,  0,  8,  6,  3, 16, 18, 10, 11,  5, 10,  6, false, false };
    presets_[3] = { 26,  5,  0,  2,  0,  3, 21,  2, 24, 18, 26, 26,  5,  2, 24,  3, false, false };
    presets_[4] = {  0, 21,  0, 24,  0,  0, 16,  3,  5, 21, 13, 24,  3,  7, 26,  4, false, false };
    presets_[5] = {  5,  0, 18, 21,  5,  3, 18, 13,  5, 16, 16, 18,  8,  3, 26, 10, false, false };
    presets_[6] = { 16,  0,  0,  2, 13,  2, 20, 10, 28, 26, 21, 32, 24,  7, 28, 13, false, false };

    currentPreset_ = proc.currentPresetIndex;

    loadPreset (0);
    setSize (kW, kH);
    syncTimer();
}

NIKAAudioProcessorEditor::~NIKAAudioProcessorEditor()
{
    flickerTimer_.stopTimer();
}

//==============================================================================
// resized — compute hit rectangles
//==============================================================================
void NIKAAudioProcessorEditor::resized()
{
    // Measure actual monospace character width
    {
        juce::GlyphArrangement ga;
        ga.addLineOfText (termFont(), "W", 0.0f, 0.0f);
        cw_ = ga.getBoundingBox (0, 1, true).getWidth();
    }

    const int cw  = (int)std::ceil (cw_);
    const int hh  = kLH;

    // Helper: x, y, nChars wide + 4 px padding, full row height
    auto hr = [&] (int x, int y, int nChars) -> juce::Rectangle<int> {
        return { x, y, nChars * cw + 4, hh };
    };

    // ---- OSC row 1: SAW SQR PLS  (label 4 + value 2 = 6 chars each) --------
    hitRects_[kSaw] = hr (kLC0, yOsc1, 6);
    hitRects_[kSqr] = hr (kLC1, yOsc1, 6);
    hitRects_[kPls] = hr (kLC2, yOsc1, 6);

    // ---- OSC row 2: PW SUB NSE ----------------------------------------------
    hitRects_[kPw]    = hr (kLC0, yOsc2, 5);   // "PW:XX"
    hitRects_[kSub]   = hr (kLC1, yOsc2, 6);   // "SUB:XX"
    hitRects_[kNoise] = hr (kLC2, yOsc2, 6);   // "NSE:XX"

    // ---- VCF: CUTOFF RESO — right side of OSC row 1 ------------------------
    hitRects_[kCutoff] = hr (kRC0, yOsc1, 9);   // "CUTOFF:XX"
    hitRects_[kReso]   = hr (kRC1, yOsc1, 7);   // "RESO:XX"

    // ---- ADSR row 1: ATK DEC SUS --------------------------------------------
    hitRects_[kAtk] = hr (kLC0, yAdsr1, 6);
    hitRects_[kDec] = hr (kLC1, yAdsr1, 6);
    hitRects_[kSus] = hr (kLC2, yAdsr1, 6);

    // ---- ADSR row 2: REL FAMT -----------------------------------------------
    hitRects_[kRel]    = hr (kLC0, yAdsr2, 6);
    hitRects_[kEnvAmt] = hr (kLC1, yAdsr2, 7);  // "FAMT:XX"

    // ---- FX section — left of kRC1 ------------------------------------------
    // "FX " label (3 cw) + "◄ N ►" patch selector
    const int fxLblW = 3 * cw;
    hitRects_[kPatchPrev] = { kRC0 + fxLblW,          ySecAds, cw, hh };
    hitRects_[kPatchNext] = { kRC0 + fxLblW + 4 * cw, ySecAds, cw, hh };
    hitRects_[kMix]       = hr (kRC0, yFxMix, 6);   // "MIX:XX"

    // ---- KS section — right of kRC1 -----------------------------------------
    hitRects_[kDepth] = hr (kRC1, yFxMix, 8);   // "DEPTH:XX"

    // ---- Header right cluster: [◄ 01 ►] [INIT] ----------------------------
    {
        const int gap   = 8;
        const int initW = (int)(4.0f * cw_) + 4;
        const int pbW   = (int)(6.0f * cw_) + 8;
        const int initX = kW - kLM - initW;
        const int pbX   = initX - gap - pbW;

        hitRects_[kPresetPrev] = { pbX + 4,           yHdr, cw,     hh };
        hitRects_[kPresetNext] = { pbX + 4 + 5 * cw,  yHdr, cw,     hh };
        hitRects_[kPresetNum]  = { pbX + 4 + cw,       yHdr, 4 * cw, hh };
        hitRects_[kInit]       = { initX, yHdr, initW, hh };
    }

    // ---- MONO: same row as KS dots, right of cluster
    {
        const int monoW = (int)(4.0f * cw_) + 4;
        const int monoX = kW - kLM - monoW;
        hitRects_[kMono] = { monoX, yKsDots - kLH / 2, monoW, hh };
    }

    // ---- СКАЗКУ (drive toggle) in footer — centred --------------------------
    {
        const int totalFtrW = juce::roundToInt ((kFtrPreGlyphs + kFtrSkGlyphs + kFtrPostGlyphs) * cw_);
        const int skazku_x  = (kW - totalFtrW) / 2 + juce::roundToInt (kFtrPreGlyphs * cw_);
        const int skazku_w  = juce::roundToInt (kFtrSkGlyphs * cw_) + 4;
        hitRects_[kDrive] = { skazku_x, yFtr, skazku_w, hh };
    }

    // ---- KS indicator dots — clustered, ornamental --------------------------
    {
        const float oldSpacing = (float)(kW - 2 * kLM) / 6.0f;
        const float spacing    = oldSpacing * 3.0f / 7.0f;  // ~24 px
        const float totalSpan  = 6.0f * spacing;
        const float startX     = kW * 0.5f - totalSpan * 0.5f;
        const float cy         = (float)yKsDots;
        const float hitSz      = 14.0f;

        for (int i = 0; i < 7; ++i)
        {
            const float cx = startX + (float)i * spacing;
            hitRects_[kKsDot0 + i] = juce::Rectangle<float> (
                cx - hitSz * 0.5f, cy - hitSz * 0.5f, hitSz, hitSz).toNearestInt();
        }
    }
}

//==============================================================================
// visibilityChanged — reset the boot-burst window to when the GUI opens
//==============================================================================
void NIKAAudioProcessorEditor::visibilityChanged()
{
    if (isVisible())
        loadTimeMs_ = juce::Time::getMillisecondCounterHiRes();
}

//==============================================================================
// paint
//==============================================================================
void NIKAAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (kBg);

    // Phosphor glow bloom (drive active) — drawn behind main content.
    // 6 stacked offset passes give visibly lit phosphor (4× original intensity).
    if (glowFade_ > 0.01f)
    {
        const float t   = (float)(juce::Time::getMillisecondCounterHiRes() * 0.001);
        const float brt = glowFade_
            * (0.6f + 0.4f * std::sin (juce::MathConstants<float>::twoPi * 0.3f * t));
        drawGlowLabels (g, +1, brt * 0.90f);
        drawGlowLabels (g, -1, brt * 0.75f);
        drawGlowLabels (g, +2, brt * 0.60f);
        drawGlowLabels (g, -2, brt * 0.50f);
        drawGlowLabels (g, +3, brt * 0.35f);
        drawGlowLabels (g, -3, brt * 0.25f);
    }

    drawContent (g, 0, 1.0f);   // normal render
    drawContent (g, 1, 0.15f);  // bloom: y+1, 15 % opacity

    drawVignette  (g);
    drawScanlines (g);
}

//==============================================================================
// drawContent
//==============================================================================
void NIKAAudioProcessorEditor::drawContent (juce::Graphics& g,
                                          int yOff, float alpha) const
{
    const juce::Font font = termFont();
    g.setFont (font);

    auto txt = [&] (const juce::String& s, int x, int y, int w, int h,
                    juce::Colour c,
                    juce::Justification j = juce::Justification::centredLeft)
    {
        g.setColour (c.withAlpha (c.getFloatAlpha() * alpha));
        g.drawText  (s, x, y + yOff, w, h, j, false);
    };

    auto rule = [&] (int y)
    {
        g.setColour (kDim.withAlpha (0.4f));
        g.fillRect  (kLM, y + yOff, kW - kLM * 2, 1);
    };

    // ---- Header: title left | [◄ 01 ►] [INIT] right -----------------------
    // Drive off: Latin title at normal brightness.
    // Drive on: Cyrillic title (glow drawn by drawGlowLabels; base layer here).
    txt (juce::String (juce::CharPointer_UTF8 ((driveOn_ && glowFade_ >= 1.0f) ? kTitle : kTitleLatin)),
         kLM, yHdr, kW / 2, kLH, kBright);

    {
        const int gap   = 8;
        const int initW = (int)(4.0f * cw_) + 4;
        const int pbW   = (int)(6.0f * cw_) + 8;
        const int initX = kW - kLM - initW;
        const int pbX   = initX - gap - pbW;

        const juce::String presetStr =
            juce::String (juce::CharPointer_UTF8 (kArrowL))
            + " " + fmt2 (currentPreset_ + 1) + " "
            + juce::String (juce::CharPointer_UTF8 (kArrowR));
        txt (presetStr, pbX + 2, yHdr, pbW - 4, kLH,
             initMode_ ? kDim : kBright, juce::Justification::centred);
        txt ("INIT", initX, yHdr, initW, kLH, initMode_ ? kBright : kDim);
    }

    // ---- MONO: same row as KS dots, right of cluster -----------------------
    {
        const int monoW = (int)(4.0f * cw_) + 4;
        const int monoX = kW - kLM - monoW;
        const juce::Colour monoCol = monoOn_ ? kBright : kDim;
        txt ("MONO", monoX, yKsDots - kLH / 2, monoW, kLH, monoCol);
    }

    rule (yR0);

    // ---- OSC + VCF merged section -------------------------------------------
    txt ("OSC", kLM,  ySecOsc, kRC0 - kLM, kLH, kBright.withAlpha (0.5f));
    txt ("VCF", kRC0, ySecOsc, kW - kLM - kRC0, kLH, kBright.withAlpha (0.5f));

    // OSC row 1: SAW SQR PLS  (left)  +  CUTOFF RESO  (right)
    drawField (g, kSaw,    "SAW:",    sSaw,    false, yOff, alpha);
    drawField (g, kSqr,    "SQR:",    sSqr,    false, yOff, alpha);
    drawField (g, kPls,    "PLS:",    sPls,    false, yOff, alpha);
    drawField (g, kCutoff, "CUTOFF:", sCutoff, false, yOff, alpha);
    drawField (g, kReso,   "RESO:",   sReso,   false, yOff, alpha);

    // OSC row 2: PW SUB NSE  (left)
    drawField (g, kPw,    "PW:",  sPw,    true,  yOff, alpha);
    drawField (g, kSub,   "SUB:", sSub,   true,  yOff, alpha);
    drawField (g, kNoise, "NSE:", sNoise, true,  yOff, alpha);

    rule (yR1);

    // ---- ADSR section (left) ------------------------------------------------
    txt ("ADSR", kLM, ySecAds, kRC0 - kLM, kLH, kBright.withAlpha (0.5f));

    // ADSR row 1: ATK DEC SUS
    drawField (g, kAtk, "ATK:", sAtk, false, yOff, alpha);
    drawField (g, kDec, "DEC:", sDec, false, yOff, alpha);
    drawField (g, kSus, "SUS:", sSus, false, yOff, alpha);

    // ADSR row 2: REL FAMT
    drawField (g, kRel,    "REL:",  sRel,    false, yOff, alpha);
    drawField (g, kEnvAmt, "FAMT:", sEnvAmt, false, yOff, alpha);

    // ---- FX section (left of kRC1) + KS section (right of kRC1) — same rows --
    // Row 1: "FX ◄N►"  |  "KS"
    {
        const int fxLblW = (int)(3.0f * cw_);
        txt ("FX", kRC0, ySecAds, fxLblW, kLH, kBright.withAlpha (0.5f));

        const juce::String patchStr =
            juce::String (juce::CharPointer_UTF8 (kArrowL))
            + " " + juce::String (sPatch) + " "
            + juce::String (juce::CharPointer_UTF8 (kArrowR));

        g.setColour (kBright.withAlpha (alpha));
        g.drawText (patchStr, kRC0 + fxLblW, ySecAds + yOff,
                    kRC1 - kRC0 - fxLblW, kLH,
                    juce::Justification::centredLeft, false);

        txt ("KS", kRC1, ySecAds, kW - kLM - kRC1, kLH, kBright.withAlpha (0.5f));
    }

    // Row 2: MIX:##  |  DEPTH:##
    drawField (g, kMix,   "MIX:",   sMix,   false, yOff, alpha);
    drawField (g, kDepth, "DEPTH:", sDepth, false, yOff, alpha);

    rule (yR2);

    // ---- Footer: МЫ РОЖДЕНЫ, ЧТОБ [СКАЗКУ] СДЕЛАТЬ БЫЛЬЮ — centred --------
    // Drive ON:  whole line in kBright, breathing at 0.05 Hz between 60–100%.
    //            СКАЗКУ stays anchored at 100% brightness.
    // Drive OFF: pre/post lines dim static; СКАЗКУ flickers with Fibonacci osc.
    {
        const int preW      = juce::roundToInt (kFtrPreGlyphs  * cw_);
        const int skW       = juce::roundToInt (kFtrSkGlyphs   * cw_);
        const int postW     = juce::roundToInt (kFtrPostGlyphs  * cw_);
        const int totalFtrW = preW + skW + postW;
        const int x0        = (kW - totalFtrW) / 2;
        const int xSk       = x0 + preW;
        const int xPost     = xSk + skW;

        if (driveOn_)
        {
            const float breath = breathBrightness();   // [0.6, 1.0]

            g.setColour (kBright.withAlpha (breath * alpha));
            g.drawText (juce::String (juce::CharPointer_UTF8 (kFtrPre)),
                        x0, yFtr + yOff, preW, kLH,
                        juce::Justification::centredLeft, false);

            // СКАЗКУ: full brightness — anchor point of the breathing line
            g.setColour (kBright.withAlpha (alpha));
            g.drawText (juce::String (juce::CharPointer_UTF8 (kFtrSk)),
                        xSk, yFtr + yOff, skW + 4, kLH,
                        juce::Justification::centredLeft, false);

            g.setColour (kBright.withAlpha (breath * alpha));
            g.drawText (juce::String (juce::CharPointer_UTF8 (kFtrPost)),
                        xPost, yFtr + yOff, postW, kLH,
                        juce::Justification::centredLeft, false);
        }
        else
        {
            g.setColour (kDim.withAlpha (alpha));
            g.drawText (juce::String (juce::CharPointer_UTF8 (kFtrPre)),
                        x0, yFtr + yOff, preW, kLH,
                        juce::Justification::centredLeft, false);

            const juce::Colour skCol =
                kDim.interpolatedWith (kBright, flickerBrightness());
            g.setColour (skCol.withAlpha (alpha));
            g.drawText (juce::String (juce::CharPointer_UTF8 (kFtrSk)),
                        xSk, yFtr + yOff, skW + 4, kLH,
                        juce::Justification::centredLeft, false);

            g.setColour (kDim.withAlpha (alpha));
            g.drawText (juce::String (juce::CharPointer_UTF8 (kFtrPost)),
                        xPost, yFtr + yOff, postW, kLH,
                        juce::Justification::centredLeft, false);
        }
    }

    // ---- KS indicator dots — clustered ornamental row, fade 0.5 s ----------
    {
        const double now       = juce::Time::getMillisecondCounterHiRes();
        const float  fadeMs    = 500.0f;
        const float  restAlph  = 0.15f;
        const float  dotR      = 3.0f;
        const float  dotCy     = (float)(yKsDots + yOff);
        const float  oldSpacing = (float)(kW - 2 * kLM) / 6.0f;
        const float  spacing   = oldSpacing * 3.0f / 7.0f;   // ~24 px — ornamental cluster
        const float  totalSpan = 6.0f * spacing;
        const float  startX    = kW * 0.5f - totalSpan * 0.5f;

        const bool allHeld = proc.voiceLimitActive.load();

        for (int i = 0; i < 7; ++i)
        {
            const float  cx        = startX + (float)i * spacing;
            const double trigMs    = proc.ksTriggerTimesMs[i].load();
            const double releaseMs = proc.ksReleaseTimesMs[i].load();
            const bool   held      = proc.ksHeldFlags[i].load();

            float dotAlpha = restAlph;
            if (trigMs > 0.0)
            {
                if (held || releaseMs <= 0.0)
                {
                    // Note is still held — stay fully bright
                    dotAlpha = 1.0f;
                }
                else
                {
                    // Fade from release time
                    const float elapsed = (float)(now - releaseMs);
                    if (elapsed < fadeMs)
                        dotAlpha = 1.0f - (elapsed / fadeMs) * (1.0f - restAlph);
                }
            }

            g.setColour (kBright.withAlpha (dotAlpha * alpha));
            g.fillEllipse (cx - dotR, dotCy - dotR, 2.0f * dotR, 2.0f * dotR);

            // ASCII articulation label below each dot (dim phosphor, static)
            {
                const int lw = juce::roundToInt (2.5f * cw_);
                const int lx = juce::roundToInt (cx) - lw / 2;
                g.setColour (kDim.withAlpha (alpha));
                g.drawText (juce::String (kKsAscii[i]),
                            lx, yKsShapes + yOff, lw, kLH,
                            juce::Justification::centred, false);
            }
        }

        // ---- Voice limit indicator "7" — below MONO button ------------------
        {
            const int monoW = (int)(4.0f * cw_) + 4;
            const int monoX = kW - kLM - monoW;
            const juce::Colour col7 = allHeld ? kBright : kDim;
            g.setColour (col7.withAlpha (alpha));
            g.drawText ("7", monoX, yKsDots - kLH / 2 + kLH + yOff, monoW, kLH,
                        juce::Justification::centred, false);
        }
    }
}

//==============================================================================
// drawField
//==============================================================================
void NIKAAudioProcessorEditor::drawField (juce::Graphics& g, int slot,
                                        const char* label, int step,
                                        bool dim, int yOff, float alpha) const
{
    const juce::String text = juce::String (label) + fmt2 (step);
    const auto rect = hitRects_[slot].translated (0, yOff);

    if (yOff == 0 && slot == activeSlot_) {
        g.setColour (kBright);
        g.fillRect  (rect.toFloat());
        g.setColour (kBg);
    } else {
        const juce::Colour base = dim ? kDim : kBright;
        g.setColour (base.withAlpha (base.getFloatAlpha() * alpha));
    }

    g.drawText (text, rect, juce::Justification::centredLeft, false);
}

//==============================================================================
// drawGlowLabels — phosphor bloom pass for title, labels, and footer.
// Drawn at yOff = ±1 behind the main content layer. alpha carries pre-multiplied
// glow intensity; colour is always kBright.
//==============================================================================
void NIKAAudioProcessorEditor::drawGlowLabels (juce::Graphics& g,
                                              int yOff, float alpha) const
{
    const juce::Font font = termFont();
    g.setFont (font);
    g.setColour (kBright.withAlpha (alpha));

    const int W = kW - kLM;  // available width from kLM

    // Helper: draw a string at (x, y+yOff) in the glow colour
    auto gl = [&] (const juce::String& s, int x, int y, int w)
    {
        g.drawText (s, x, y + yOff, w, kLH, juce::Justification::centredLeft, false);
    };

    // ---- Title --------------------------------------------------------------
    gl (juce::String (juce::CharPointer_UTF8 (kTitle)), kLM, yHdr, kW / 2);

    // ---- Header right cluster: preset ---------------------------------------
    {
        const int gap   = 8;
        const int initW = (int)(4.0f * cw_) + 4;
        const int pbW   = (int)(6.0f * cw_) + 8;
        const int initX = kW - kLM - initW;
        const int pbX   = initX - gap - pbW;

        const juce::String presetStr =
            juce::String (juce::CharPointer_UTF8 (kArrowL))
            + " " + juce::String::formatted ("%02d", currentPreset_ + 1) + " "
            + juce::String (juce::CharPointer_UTF8 (kArrowR));
        g.setColour (kBright.withAlpha (alpha));
        g.drawText (presetStr, pbX + 2, yHdr + yOff, pbW - 4, kLH,
                    juce::Justification::centred, false);
    }

    // MONO uses a plain colour swap — no glow pass needed.

    // ---- Section labels -----------------------------------------------------
    gl ("OSC",  kLM,  ySecOsc, kRC0 - kLM);
    gl ("VCF",  kRC0, ySecOsc, W - kRC0);
    gl ("ADSR", kLM,  ySecAds, kRC0 - kLM);
    gl ("FX",   kRC0, ySecAds, (int)(3.0f * cw_));
    gl ("KS",   kRC1, ySecAds, W - kRC1);

    // ---- OSC parameter labels -----------------------------------------------
    gl ("SAW:",    kLC0, yOsc1, (int)(4.0f * cw_));
    gl ("SQR:",    kLC1, yOsc1, (int)(4.0f * cw_));
    gl ("PLS:",    kLC2, yOsc1, (int)(4.0f * cw_));
    gl ("PW:",     kLC0, yOsc2, (int)(3.0f * cw_));
    gl ("SUB:",    kLC1, yOsc2, (int)(4.0f * cw_));
    gl ("NSE:",    kLC2, yOsc2, (int)(4.0f * cw_));

    // ---- VCF parameter labels -----------------------------------------------
    gl ("CUTOFF:", kRC0, yOsc1, (int)(7.0f * cw_));
    gl ("RESO:",   kRC1, yOsc1, (int)(5.0f * cw_));

    // ---- ADSR parameter labels ----------------------------------------------
    gl ("ATK:",  kLC0, yAdsr1, (int)(4.0f * cw_));
    gl ("DEC:",  kLC1, yAdsr1, (int)(4.0f * cw_));
    gl ("SUS:",  kLC2, yAdsr1, (int)(4.0f * cw_));
    gl ("REL:",  kLC0, yAdsr2, (int)(4.0f * cw_));
    gl ("FAMT:", kLC1, yAdsr2, (int)(5.0f * cw_));

    // ---- FX patch selector "◄ N ►" -----------------------------------------
    {
        const int fxLblW = (int)(3.0f * cw_);
        const juce::String patchStr =
            juce::String (juce::CharPointer_UTF8 (kArrowL))
            + " " + juce::String (sPatch) + " "
            + juce::String (juce::CharPointer_UTF8 (kArrowR));
        g.drawText (patchStr, kRC0 + fxLblW, ySecAds + yOff,
                    kRC1 - kRC0 - fxLblW, kLH,
                    juce::Justification::centredLeft, false);
    }

    // ---- FX / KS parameter labels -------------------------------------------
    gl ("MIX:",   kRC0, yFxMix, (int)(4.0f * cw_));
    gl ("DEPTH:", kRC1, yFxMix, (int)(6.0f * cw_));

    // ---- Footer — full quote, centred ---------------------------------------
    {
        const int preW      = juce::roundToInt (kFtrPreGlyphs  * cw_);
        const int skW       = juce::roundToInt (kFtrSkGlyphs   * cw_);
        const int postW     = juce::roundToInt (kFtrPostGlyphs  * cw_);
        const int totalFtrW = preW + skW + postW;
        const int x0        = (kW - totalFtrW) / 2;
        gl (juce::String (juce::CharPointer_UTF8 (kFtrPre)),  x0,           yFtr, preW);
        gl (juce::String (juce::CharPointer_UTF8 (kFtrSk)),   x0 + preW,    yFtr, skW + 4);
        gl (juce::String (juce::CharPointer_UTF8 (kFtrPost)), x0 + preW + skW, yFtr, postW);
    }
}

//==============================================================================
// drawVignette — heavy CRT-style radial darkening from corners
//==============================================================================
void NIKAAudioProcessorEditor::drawVignette (juce::Graphics& g) const
{
    // Pass 1: main vignette — transparent centre, mid-stop at 45 % radius,
    //         heavy darkening at the corners (85 % alpha)
    {
        juce::ColourGradient vig (
            juce::Colours::transparentBlack, kW * 0.5f, kH * 0.5f,
            juce::Colour (0, 0, 0).withAlpha (0.85f), 0.0f, 0.0f,
            true);
        vig.addColour (0.45, juce::Colour (0, 0, 0).withAlpha (0.06f));
        g.setGradientFill (vig);
        g.fillRect (0, 0, kW, kH);
    }

    // Pass 2: outer-ring boost — extra layer of corner darkening
    //         (transparent until 60 % radius, 45 % at the very edge)
    {
        juce::ColourGradient vig2 (
            juce::Colours::transparentBlack, kW * 0.5f, kH * 0.5f,
            juce::Colour (0, 0, 0).withAlpha (0.45f), 0.0f, 0.0f,
            true);
        vig2.addColour (0.60, juce::Colours::transparentBlack);
        g.setGradientFill (vig2);
        g.fillRect (0, 0, kW, kH);
    }
}

//==============================================================================
// drawScanlines — 1 px black band every 2 px, outermost layer
//==============================================================================
void NIKAAudioProcessorEditor::drawScanlines (juce::Graphics& g) const
{
    g.setColour (juce::Colour (0, 0, 0).withAlpha (0.45f));
    for (int y = 0; y < kH; y += 2)
        g.fillRect (0, y, kW, 1);
}

//==============================================================================
// Mouse interaction
//==============================================================================
void NIKAAudioProcessorEditor::mouseDown (const juce::MouseEvent& e)
{
    const auto pos = e.getPosition();
    activeSlot_ = -1;

    for (int i = 0; i < kNumSlots; ++i) {
        if (hitRects_[i].contains (pos)) {
            activeSlot_ = i;
            break;
        }
    }

    // ---- Click-only slots ---------------------------------------------------
    if (activeSlot_ == kPatchPrev) {
        sPatch = juce::jlimit (1, 7, sPatch - 1);
        pushParam (kPatchPrev);
        activeSlot_ = -1;
        repaint();
        return;
    }
    if (activeSlot_ == kPatchNext) {
        sPatch = juce::jlimit (1, 7, sPatch + 1);
        pushParam (kPatchNext);
        activeSlot_ = -1;
        repaint();
        return;
    }
    if (activeSlot_ == kDrive) {
        driveOn_ = !driveOn_;
        pushParam (kDrive);
        syncTimer();
        activeSlot_ = -1;
        repaint();
        return;
    }
    if (activeSlot_ == kMono) {
        monoOn_ = !monoOn_;
        pushParam (kMono);
        activeSlot_ = -1;
        repaint();
        return;
    }
    if (activeSlot_ == kInit) {
        initMode_ = true;
        sSaw=32; sSqr=0; sPls=0; sPw=16; sSub=0; sNoise=0;
        sCutoff=32; sReso=0;
        sAtk=0; sDec=32; sSus=32; sRel=0; sEnvAmt=0;
        sPatch=1; sMix=0; sDepth=32;
        monoOn_ = false;
        driveOn_ = false;
        pushAllParams();
        syncTimer();
        activeSlot_ = -1;
        repaint();
        return;
    }

    // ---- Preset arrows: cycle slots 0–6, displayed as 01–07 ----------------
    if (activeSlot_ == kPresetPrev) {
        currentPreset_ = (currentPreset_ + kNumPresets - 1) % kNumPresets;
        initMode_ = false;
        loadPreset (currentPreset_);
        activeSlot_ = -1;
        repaint();
        return;
    }
    if (activeSlot_ == kPresetNext) {
        currentPreset_ = (currentPreset_ + 1) % kNumPresets;
        initMode_ = false;
        loadPreset (currentPreset_);
        activeSlot_ = -1;
        repaint();
        return;
    }
    // ---- Preset number click — exits INIT mode, restores preset selector ----
    if (activeSlot_ == kPresetNum && initMode_) {
        initMode_ = false;
        loadPreset (currentPreset_);
        activeSlot_ = -1;
        repaint();
        return;
    }

    // ---- KS dot press — trigger on down, release on up ---------------------
    if (activeSlot_ >= kKsDot0 && activeSlot_ <= kKsDot6) {
        proc.triggerKeyswitch (activeSlot_ - kKsDot0, 1.0f);
        repaint();
        return;   // keep activeSlot_ set so mouseUp can call releaseKeyswitch
    }

    // ---- Draggable slots ----------------------------------------------------
    if (activeSlot_ >= 0) {
        dragStartY_ = e.y;
        if (auto* sp = stepPtr (activeSlot_))
            dragStartStep_ = *sp;
    }

    repaint();
}

void NIKAAudioProcessorEditor::mouseDrag (const juce::MouseEvent& e)
{
    if (activeSlot_ >= 0 && stepPtr (activeSlot_) != nullptr) {
        const int delta = (dragStartY_ - e.y) / 4;
        setStep (activeSlot_, dragStartStep_ + delta);
    }
    repaint();
}

void NIKAAudioProcessorEditor::mouseUp (const juce::MouseEvent&)
{
    if (activeSlot_ >= kKsDot0 && activeSlot_ <= kKsDot6)
        proc.releaseKeyswitch (activeSlot_ - kKsDot0);

    activeSlot_ = -1;
    repaint();
}

void NIKAAudioProcessorEditor::mouseWheelMove (const juce::MouseEvent& e,
                                             const juce::MouseWheelDetails& w)
{
    const auto pos = e.getPosition();
    const int  dir = (w.deltaY > 0.0f) ? 1 : -1;

    for (int i = 0; i < kNumSlots; ++i) {
        if (!hitRects_[i].contains (pos)) continue;

        if (i == kPatchPrev || i == kPatchNext) {
            sPatch = juce::jlimit (1, 7, sPatch + dir);
            pushParam (kPatchPrev);
        } else if (i == kPresetPrev || i == kPresetNext) {
            currentPreset_ = juce::jlimit (0, kNumPresets - 1, currentPreset_ + dir);
            initMode_ = false;
            loadPreset (currentPreset_);
        } else if (i == kMono) {
            monoOn_ = (dir > 0);
            pushParam (kMono);
        } else if (auto* sp = stepPtr (i)) {
            setStep (i, *sp + dir);
        }
        break;
    }

    repaint();
}

//==============================================================================
// stepPtr
//==============================================================================
int* NIKAAudioProcessorEditor::stepPtr (int slot) noexcept
{
    switch (slot) {
        case kSaw:    return &sSaw;
        case kSqr:    return &sSqr;
        case kPls:    return &sPls;
        case kPw:     return &sPw;
        case kSub:    return &sSub;
        case kNoise:  return &sNoise;
        case kCutoff: return &sCutoff;
        case kReso:   return &sReso;
        case kAtk:    return &sAtk;
        case kDec:    return &sDec;
        case kSus:    return &sSus;
        case kRel:    return &sRel;
        case kEnvAmt: return &sEnvAmt;
        case kMix:    return &sMix;
        case kDepth:  return &sDepth;
        default:      return nullptr;
    }
}

//==============================================================================
// setStep
//==============================================================================
void NIKAAudioProcessorEditor::setStep (int slot, int newStep)
{
    newStep = juce::jlimit (0, 32, newStep);
    auto* sp = stepPtr (slot);
    if (!sp || *sp == newStep) return;
    *sp = newStep;
    pushParam (slot);
    repaint();
}

//==============================================================================
// pushParam — convert step to parameter value and notify APVTS
//==============================================================================
void NIKAAudioProcessorEditor::pushParam (int slot)
{
    auto lin = [] (int s) { return s / 32.0f; };
    auto exp = [] (int s, float mn, float mx) {
        return mn * std::pow (mx / mn, s / 32.0f);
    };
    // e^2 curve: y = (e^(2x) - 1) / (e^2 - 1), x = s/32
    // Gives more resolution in the lower range; full amplitude not reached until step 32.
    // At step 20/32: 0.390 (vs 0.625 linear)
    static constexpr float kE2 = 6.38905609893f;  // e^2 - 1
    auto oscExp = [&] (int s) {
        const float x = s / 32.0f;
        return (std::exp (2.0f * x) - 1.0f) / kE2;
    };

    switch (slot) {
        case kSaw:
            paSaw->setValueAsCompleteGesture (oscExp (sSaw)); break;
        case kSqr:
            paSqr->setValueAsCompleteGesture (oscExp (sSqr)); break;
        case kPls:
            paPls->setValueAsCompleteGesture (oscExp (sPls)); break;
        case kPw:
            paPw->setValueAsCompleteGesture (
                juce::jmap ((float)sPw, 0.0f, 32.0f, 0.05f, 0.95f)); break;
        case kSub:
            paSub->setValueAsCompleteGesture (oscExp (sSub)); break;
        case kNoise:
            paNoise->setValueAsCompleteGesture (oscExp (sNoise)); break;

        case kCutoff:
            paCutoff->setValueAsCompleteGesture (
                16.0f * std::pow (1024.0f, sCutoff / 32.0f)); break;
        case kReso:
            paReso->setValueAsCompleteGesture (lin (sReso)); break;

        case kAtk:
            paAtk->setValueAsCompleteGesture (exp (sAtk, 0.0005f, 4.0f)); break;
        case kDec:
            paDec->setValueAsCompleteGesture (exp (sDec, 0.004f,  4.0f)); break;
        case kSus:
            paSus->setValueAsCompleteGesture (lin (sSus)); break;
        case kRel:
            paRel->setValueAsCompleteGesture (exp (sRel, 0.002f,  8.0f)); break;
        case kEnvAmt:
            paEnvAmt->setValueAsCompleteGesture (lin (sEnvAmt)); break;

        case kPatchPrev:
        case kPatchNext:
            paPatch->setValueAsCompleteGesture ((float)sPatch); break;

        case kMix:
            paMix->setValueAsCompleteGesture (lin (sMix)); break;
        case kDepth:
            paDepth->setValueAsCompleteGesture (lin (sDepth)); break;

        case kDrive:
            paDrive->setValueAsCompleteGesture (driveOn_ ? 1.0f : 0.0f); break;

        case kMono:
            paMono->setValueAsCompleteGesture (monoOn_ ? 1.0f : 0.0f); break;

        default: break;
    }
}

//==============================================================================
// Preset management
//==============================================================================
void NIKAAudioProcessorEditor::savePreset (int idx)
{
    auto& p  = presets_[idx];
    p.saw    = sSaw;    p.sqr    = sSqr;    p.pls    = sPls;    p.pw     = sPw;
    p.sub    = sSub;    p.noise  = sNoise;
    p.cutoff = sCutoff; p.reso   = sReso;
    p.atk    = sAtk;    p.dec    = sDec;    p.sus    = sSus;    p.rel    = sRel;
    p.envAmt = sEnvAmt; p.patch  = sPatch;  p.mix    = sMix;    p.depth  = sDepth;
    p.drive  = driveOn_; p.mono = monoOn_;
}

void NIKAAudioProcessorEditor::loadPreset (int idx)
{
    const auto& p = presets_[idx];
    sSaw    = p.saw;    sSqr    = p.sqr;    sPls    = p.pls;    sPw     = p.pw;
    sSub    = p.sub;    sNoise  = p.noise;
    sCutoff = p.cutoff; sReso   = p.reso;
    sAtk    = p.atk;    sDec    = p.dec;    sSus    = p.sus;    sRel    = p.rel;
    sEnvAmt = p.envAmt; sPatch  = p.patch;  sMix    = p.mix;    sDepth  = p.depth;
    driveOn_ = p.drive; monoOn_ = p.mono;
    proc.currentPresetIndex = currentPreset_;
    pushAllParams();
    syncTimer();
}

void NIKAAudioProcessorEditor::pushAllParams()
{
    for (int s : { kSaw, kSqr, kPls, kPw, kSub, kNoise,
                   kCutoff, kReso,
                   kAtk, kDec, kSus, kRel, kEnvAmt,
                   kPatchPrev,   // pushes sPatch
                   kMix, kDepth, kDrive, kMono })
        pushParam (s);
}

//==============================================================================
// Breathing helper — 0.05 Hz sine mapped to [0.6, 1.0]
//==============================================================================
float NIKAAudioProcessorEditor::breathBrightness() const noexcept
{
    const double t    = juce::Time::getMillisecondCounterHiRes() * 0.001;
    const double sine = std::sin (juce::MathConstants<double>::twoPi * 0.05 * t);
    return static_cast<float> (0.8 + 0.2 * sine);   // sin=-1 → 0.6, sin=+1 → 1.0
}

//==============================================================================
// Flicker helpers
//==============================================================================
void NIKAAudioProcessorEditor::syncTimer() noexcept
{
    // Timer drives breathing (drive ON) and СКАЗКУ flicker (drive OFF).
    // Always running — stop only when the editor itself is being torn down.
    flickerTimer_.startTimer (16);   // ~60 fps
}

float NIKAAudioProcessorEditor::flickerBrightness() const noexcept
{
    return cachedFlicker_;
}

//==============================================================================
// updateFlickerState — called once per timer tick (~16 ms)
//
// Gate and script design:
//   • G(t) is evaluated every tick for the life of the plugin — never one-shot.
//   • While gate is open AND no script is running, one starts immediately.
//   • When a script finishes, the next script starts at once (continuous loop).
//   • Gate closing resets the script; gate reopening picks up the next script.
//   • Beat-advance loop is guarded to prevent spin on any timer stall.
//==============================================================================
void NIKAAudioProcessorEditor::updateFlickerState() noexcept
{
    const double nowMs = juce::Time::getMillisecondCounterHiRes();
    const double t     = nowMs * 0.001;
    const double pi2   = juce::MathConstants<double>::twoPi;
    const double piH   = juce::MathConstants<double>::pi * 0.5;

    // ---- Phosphor glow fade (0.5 s ramp with drive state) ------------------
    glowFade_ = driveOn_
        ? std::min (1.0f, glowFade_ + 0.032f)
        : std::max (0.0f, glowFade_ - 0.032f);

    // ---- Master gate G(t) — evaluated every tick, loops forever ------------
    const double Gt = std::sin (pi2 * 0.08 * t) * std::sin (pi2 * 0.05 * t + piH)
                    + 0.382 * std::sin (pi2 * 0.03 * t);
    const double bootBias   = ((nowMs - loadTimeMs_) < 3000.0) ? 1.0 : 0.0;
    const bool   masterGate = (Gt + bootBias) > 0.5;

    if (!masterGate)
    {
        // Gate closed: stop any running script, dim static phosphor
        scriptRunning_ = false;
        beatLit_       = false;
        cachedFlicker_ = 0.08f;
        return;
    }

    // Gate open: if no script is running, start the next one immediately.
    // This handles both the first-open case and seamless post-script restart.
    if (!scriptRunning_)
    {
        scriptIdx_     = juce::jlimit (0, 4, scriptIdx_);
        scriptRunning_ = true;
        beatIdx_       = 0;
        beatLit_       = kScripts[scriptIdx_][0].lit;
        beatEndMs_     = nowMs + (double)kScripts[scriptIdx_][0].ms;
    }

    // ---- Layer 3: hard blackout spikes (8% / 100 ms) -----------------------
    if (nowMs >= nextBlackoutCheckMs_)
    {
        nextBlackoutCheckMs_ = nowMs + 100.0;
        if (flickerRng_.nextFloat() < 0.08f)
        {
            static constexpr double kDurMs[] = { 8.0, 13.0, 21.0, 34.0 };
            blackoutUntilMs_ = nowMs + kDurMs[flickerRng_.nextInt (4)];
        }
    }
    if (nowMs < blackoutUntilMs_)
    {
        cachedFlicker_ = 0.0f;
        return;
    }

    // ---- Story arc script playback — guard = 64 prevents spin on stall -----
    for (int guard = 0; scriptRunning_ && nowMs >= beatEndMs_ && guard < 64; ++guard)
    {
        beatIdx_++;
        const int len = kScriptLengths[juce::jlimit (0, 4, scriptIdx_)];
        if (beatIdx_ >= len)
        {
            // Script finished — advance to next script and start immediately
            scriptIdx_ = (scriptIdx_ + 1) % 5;
            beatIdx_   = 0;
            beatLit_   = kScripts[scriptIdx_][0].lit;
            beatEndMs_ = nowMs + (double)kScripts[scriptIdx_][0].ms;
            break;   // restart from beat 0 of the new script
        }
        beatLit_    = kScripts[scriptIdx_][beatIdx_].lit;
        beatEndMs_ += (double)kScripts[scriptIdx_][beatIdx_].ms;
    }

    if (!beatLit_)
    {
        cachedFlicker_ = 0.0f;
        return;
    }

    // ---- Layer 1: phosphor flutter (three oscillators, normalised 0–1) -----
    const double L1 = std::sin (pi2 * 0.3 * t)
                    + std::sin (pi2 * 0.5 * t)
                    + std::sin (pi2 * 0.8 * t);
    cachedFlicker_ = (float)((L1 + 3.0) / 6.0);
}
