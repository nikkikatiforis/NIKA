# NIKA — Status

## Last updated
2026-03-16

## Build status
BUILT — VST3 + Standalone compiling clean, installed and working in Ableton.
`build/NIKA_artefacts/Release/VST3/NIKA.vst3`
`build/NIKA_artefacts/Release/Standalone/NIKA.app`

Install cmd:
`sudo cp -r build/NIKA_artefacts/Release/VST3/NIKA.vst3 ~/Library/Audio/Plug-Ins/VST3/`

## What's done
- [x] Full audio engine: OSC, LadderFilter, ADSR, Compressor, Saturator, Limiter, KSEngine, FXEngine
- [x] 7-voice polyphony with two-phase voice stealing and 2ms fade-in
- [x] Mono legato mode with portamento and note stack
- [x] Pitch bend (±2 semitones), mod wheel (0–4 octave cutoff offset)
- [x] CRT terminal UI — draggable text fields, no knobs
- [x] СКАЗКУ drive toggle + 5-script story-arc flicker system
- [x] Phosphor glow bloom (6-pass, drive ON)
- [x] Scanlines + radial vignette
- [x] 7 factory presets (Preset 01 confirmed, 02–07 in place)
- [x] Preset arrows [◄ 01 ►] with save/load
- [x] INIT button
- [x] KS indicator dots with 500ms fade and articulation labels
- [x] Voice limit indicator ("7")
- [x] MONO toggle
- [x] M/S width boost on drive
- [x] BPM sync to host transport (FX engine)
- [x] State persistence (getStateInformation / setStateInformation)
- [x] **FIXED: minimise/reopen no longer resets to factory preset**
- [x] **FIXED: fresh load now correctly shows Preset 01**
- [x] **FIXED: footer period (БЫЛЬЮ.) no longer clipped**
- [x] Preset 01 values confirmed from screenshot
- [x] **Pre-distribution code audit — real-world references reviewed, all clear**
- [x] **ksDepth values rebalanced for presets 02–05** (see below)
- [x] **CMakeLists.txt JUCE path fixed** — now points to `../vendor/JUCE`
- [x] **Preset 03 renamed** — Brazil Funk → Speed (comment only, UI shows number)
- [x] **Standalone custom title bar** — fully styled to NIKA aesthetic (session 2026-03-16)
  - Frameless window via `JUCE_USE_CUSTOM_PLUGIN_STANDALONE_APP=1` + `NIKAStandaloneWindow`
  - Pure black title bar (`kTitleBg=0xFF000000`), `kDim` separator via `paintOverChildren`
  - Courier New Bold 13px throughout; OPTIONS left-aligned at kLM=20
  - `-` and `X` right-aligned to `getWidth()-kLM` (matches INIT/MONO column)
  - macOS compositor gradient killed via `StandaloneNative_Mac.mm` ObjC (`setOpaque:YES`)
  - `Standalone.cpp` renamed to `Standalone.mm`; `StandaloneNative_Mac.mm` added
- [x] **OPTIONS popup menu styled** — kBg background, kDim border, Courier New Bold 13px
  - Inverted highlight (kBright bg / kBg text), 4px border padding, all text uppercased
  - Global LAF set via `LookAndFeel::setDefaultLookAndFeel` so popup picks it up

## ksDepth values (current)
| # | Name | ksDepth |
|---|------|---------|
| 01 | Oklou | 20 |
| 02 | MkGee | 12 |
| 03 | Speed | 12 |
| 04 | 4 Strings | 7 |
| 05 | 50s | 8 |
| 06 | Rhodes | 10 |
| 07 | Temple of Time | 12 |

## Key fixes (previous session, 2026-03-12)
1. Removed `loadPreset()` from editor constructor — editor now reads APVTS via
   `sendInitialUpdate()` on open, preserving any tweaks made before minimise.
2. APVTS parameter defaults in `createParameterLayout()` now match Preset 01 exactly,
   so a brand new plugin instance (no saved state) opens with Preset 01.
3. Removed `apvts.replaceState(xml)` from `setStateInformation()` — it was firing
   async value-tree notifications that arrived after `setP()` and clobbered Preset 01
   with stale Ableton-saved values.
4. `kFtrPostGlyphs` bumped 15→16 so the trailing period on the footer quote isn't clipped.

## Standalone source files
| File | Role |
|------|------|
| `Source/Standalone.mm` | Custom app entry point, NIKATitleBarLAF, NIKAStandaloneWindow, NIKAApplication |
| `Source/StandaloneNative_Mac.mm` | macOS-only: `nikaForceOpaqueWindow()` — sets NSWindow opaque, kills compositor gradient |

## What's next
- [ ] **Laptop keyboard → MIDI** in standalone (KeyListener mapping A=C, W=C#, etc.)
- [ ] **Distribution build — Mac + Windows**
  - Mac: proper code signing (Apple Developer cert, notarisation) or ad-hoc for personal use
  - Windows: CMake toolchain for MSVC or MinGW, VST3 output, installer (NSIS/Inno Setup?)
  - Consider: universal binary (arm64 + x86_64) on Mac
  - Consider: what installer/packaging format (drag-to-folder vs. pkg vs. zip)
  - Consider: whether to ship Standalone as well as VST3 on both platforms
