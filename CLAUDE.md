# NIKA — Project Cheat Sheet

## Identity
NIKA is a JUCE VST3 + Standalone synthesizer. **G2 was the working title — it is gone.**
- Aesthetic: Soviet CRT terminal / ISKRA phosphor
- Language: C++17, JUCE framework
- JUCE path: `../JUCE` (sibling to project root)

## Build
```
cmake --build build --config Release
```
- Build system: CMake + Unix Makefiles (NOT Ninja — existing cache)
- Outputs: `build/NIKA_artefacts/Release/VST3/NIKA.vst3`
           `build/NIKA_artefacts/Release/Standalone/NIKA.app`
- Ad-hoc codesigned post-build (CMake custom command handles it)

## Source files — all in `Source/`
| File | Class | Role |
|------|-------|------|
| PluginProcessor.cpp/h | NIKAAudioProcessor | APVTS, voice engine, MIDI, processBlock |
| PluginEditor.cpp/h | NIKAAudioProcessorEditor | Full GUI |
| Oscillator.cpp/h | NIKAOscillator | PolyBLEP, double-precision phase, 5 waveforms |
| LadderFilter.cpp/h | NIKALadderFilter | Huovilainen Moog 4-pole, 2× oversampling |
| ADSR.cpp/h | NIKAADSR | Linear attack, exp RC decay/release |
| Compressor.cpp/h | NIKACompressor | Feed-forward, stereo-linked RMS, soft knee |
| Saturator.cpp/h | NIKASaturator | Asymmetric tanh, HF rolloff, noise floor |
| Limiter.cpp/h | NIKALimiter | 64-sample lookahead, 0 dBFS ceiling |
| KeyswitchEngine.cpp/h | NIKAKeyswitchEngine | Notes 0–6 reserved, one-shot filter env |
| FXEngine.cpp/h | NIKAFXEngine | 7 patch slots (2 impl): 480L reverb, RE-201 echo |

## Signal chain (per sample)
```
[7 voices] OSC → LadderFilter → ADSR → mix
           ↓
     Compressor → Saturator → FXEngine → M/S width → output gain → Limiter
```
- FXEngine and Limiter **always run** (reverb/delay tails survive note-off)
- KS engine advances every sample even when all voices are idle
- Latency reported to host: `NIKALimiter::kLookahead` samples

## APVTS Parameters
| ID | Type | Range | Notes |
|----|------|-------|-------|
| sawLevel | float | 0–32 steps | e² amplitude curve |
| squareLevel | float | 0–32 steps | |
| pulseLevel | float | 0–32 steps | |
| pulseWidth | float | 0–32 steps | mapped → 0.05–0.95 |
| subLevel | float | 0–32 steps | |
| noiseLevel | float | 0–32 steps | |
| cutoff | float | 16–16384 Hz | log-skew 1/e |
| resonance | float | 0–32 steps | |
| attack | float | 0–32 steps | → 0.5ms–4s exp |
| decay | float | 0–32 steps | → 4ms–4s exp |
| sustain | float | 0–32 steps | linear |
| release | float | 0–32 steps | → 2ms–8s exp |
| filterEnvAmt | float | 0–32 steps | up to 5 octaves |
| satDrive | float | 0–1 | СКАЗКУ toggle; also shifts comp threshold -24→-8 dB |
| ksDepth | float | 0–32 steps | |
| fxPatch | int | 1–7 | |
| fxMix | float | 0–32 steps | |
| mono | bool | — | mono legato mode |

envTarget is hardcoded to 2 (both amp + filter) — no UI param.

## Voice engine
- 7 voices, polyphonic
- Allocation: retrigger same note → free voice → steal oldest
- Two-phase steal: 7 ms fade-out before reassigning
- 2 ms fade-in on every trigger (masks phase/filter transients)
- Portamento: 40 ms one-pole glide (mono mode)
- Mono legato: note stack, glide on held-note retrigger, voice[0] only

## GUI — PluginEditor
- Window: **384 × 320 px**
- Style: CRT terminal, monospace Courier New Bold 13px
- Colours: kBg=0xFF080C08 · kBright=0xFFB8C9A8 · kDim=0xFF3A4A3A
- All params shown as draggable text fields e.g. `SAW:00`, `CUTOFF:16`
  - Drag up/down (4px = 1 step) or scroll wheel
  - Active field inverts (dark text on bright bg)
- No knobs, no sliders, no Label components

### Layout (row Y positions)
```
yHdr=20    Header: title | [◄ 01 ►] [INIT]
yR0=44     Rule
ySecOsc=57 "OSC"                    "VCF"
yOsc1=78   SAW: SQR: PLS:           CUTOFF: RESO:
yOsc2=102  PW:  SUB: NSE:
yR1=128    Rule
ySecAds=141 "ADSR"                  "FX ◄N►"  "KS"
yAdsr1=162 ATK: DEC: SUS:
yAdsr2=186 REL: FAMT:               MIX:      DEPTH:
yKsDots=220  ● ● ● ● ● ● ●  (KS dots)        MONO / 7
yKsShapes=234  < > /_ <> ^ o ~  (articulation labels)
yR2=262    Rule
yFtr=278   МЫ РОЖДЕНЫ, ЧТОБ [СКАЗКУ] СДЕЛАТЬ БЫЛЬЮ
```

### Column X positions
```
kLM=20  kLC0=20  kLC1=76  kLC2=132  |  kRC0=212  kRC1=300
```

### Slots (hit rectangles, enum Slot)
kSaw kSqr kPls kPw kSub kNoise · kCutoff kReso · kAtk kDec kSus kRel kEnvAmt
kPatchPrev kPatchNext · kMix kDepth · kDrive · kPresetPrev kPresetNext kPresetNum
kMono kInit · kKsDot0–kKsDot6

### Visual layers (paint order)
1. Background fill
2. Phosphor glow bloom — 6 stacked ±offset passes (drive ON only)
3. drawContent (yOff=0, alpha=1.0)
4. Bloom pass (yOff=+1, alpha=0.15)
5. Radial vignette (2-pass gradient)
6. Scanlines (1px dark band every 2px)

## СКАЗКУ / Drive system
- `satDrive` param doubles as drive toggle (0 = off, 1 = on)
- Drive ON:  Cyrillic title, full phosphor glow, compressor threshold -8 dB, M/S width boost (×φ)
- Drive OFF: Latin title, СКАЗКУ word flickers via story-arc script system
- Flicker: master gate G(t) → 5 scripts (A–E) cycling, layer-3 blackout spikes, layer-1 phosphor flutter
- Glow fade: 0.5 s ramp (0.032/tick at 60fps)

## Presets (7 factory, slots 0–6 displayed as 01–07)
01 Oklou · 02 MkGee · 03 Brazil Funk · 04 4 Strings · 05 50s · 06 Rhodes · 07 Temple of Time

Presets live in the editor only (not persisted to APVTS state).
`setStateInformation` always restores to Preset 01 values on load.

## KS (Keyswitch) system
- MIDI notes 0–6 (C-2 to F#-2 in Ableton) are keyswitches — intercepted before voice allocator
- Notes 19–23 silently consumed (gap between KS range and playable range)
- 7 KS dots in UI; mouseDown triggers, mouseUp releases
- Dot brightness: held = full, release = 500 ms fade to 15%
- Articulations: < (cresc) · > (dim) · /_ (fp) · <> (swell) · ^ (sfz) · o (pizz) · ~ (trem)
- Voice limit indicator: "7" shown below MONO, lights bright when all 7 voices active

## Key design decisions
- `setLatencySamples(NIKALimiter::kLookahead)` reported to host
- FX buffers pre-allocated in `prepare()`; `setPatch()` only zeros them (no RT alloc)
- Log-octave filter mod: ADSR + KS contributions additive before `exp2()`
- Output gain: φ constant (+4.2 dB); drive-on adds √φ makeup (+2.1 dB)
- M/S width: 128 ms slew; drive target = 1 + satDrive × 0.618
- Compressor ratio fixed at φ (1.618)
- Cutoff smoother: 16 ms TC · Resonance smoother: 16 ms TC · Mod wheel smoother: 16 ms TC
