# NIKA — Status

## Last updated
2026-04-07

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

## Session 2026-04-07
- [x] **Param name consistency fix** — FAMT→FAmt, MIX→Mix, PATCH→Patch (Ableton automation display names)
- [x] **CMake cache wiped and reconfigured** — old path `/claude-projects/` was stale, fresh configure from `/claude/NIKA/`
- [x] **Full rebuild** — VST3 + Standalone + AU, all clean
- [x] **Repacked dist/NIKA-1.0.pkg** — fresh installer with updated build
- [x] **Standalone installed** to `/Applications/NIKA.app`
- [x] **dist/ cleaned** — removed unnamed stale .pkg (22 Mar artifact) and pkgroot staging folder
- [x] **Git committed and pushed** — all previously uncommitted files included (Standalone.mm, StandaloneNative_Mac.mm, CLAUDE.md, STATUS.md, dist/, CMakeLists.txt, PluginEditor, PluginProcessor, KeyswitchEngine). GitHub is now current.

## Session 2026-04-12 — Brief + visual assets
- [x] **Section 4 NIKA rewritten** — SOMA Laboratory style, confirmed by user. Technical sections factual; romance sparse (one precise line per section). Two additions over original: RE-201 "deviation was the point" line, SVEMA hard stop before "these were not considered features."
- [x] **СКАЗКУ copy updated** — factual mechanics listed: compressor threshold shift, SVEMA saturation/wow/flutter, shimmer reverb. "Phantom circuit" kept as stylistic choice. "Blooms" fixed to intransitive ("a parallel shimmer engine blooms").
- [x] **Limiter added to output chain copy** — Weiss 102-inspired brickwall, 0 dBFS ceiling. Was missing (three stages listed, only two named).
- [x] **Classical Articulations entry extended** — ~1.5× original length, technical register, confirmed by user.
- [x] **Subtitle iterated** — "EVA SOURCE CODE" replaced. Landed on "EVA DECODE" (user confirmed in PDF). EVA PLAINTEXT/CLEARTEXT also strong candidates explored.
- [x] **Visual assets sourced** — `seeds/NIKA [EVA]/` created with 13 images + PLACEMENT.md:
  - `hero/` — NIKA plugin UI (from nikkivst/public/nika-v2.png)
  - `references/` — ISKRA terminal, 1937 Soviet heroes postcard, Bach manuscript, Roland SH-101, golden ratio spiral SVG, SVEMA tape box
  - `lore/` — SOMA LYRA-8, Sleepnet vinyl, Gosha AW16 lookbook, Metro Exodus cover
  - `market/` — Serum UI screenshot
  - `PLACEMENT.md` — per-image placement instructions for Google Docs insertion

## What's next
- [ ] **Windows build** — deferred until after Mac launch (estimated >1 week out)
  - CMake toolchain for MSVC or MinGW, VST3 output, installer (NSIS/Inno Setup?)
  - Consider: whether to ship Standalone on Windows too
- [ ] **Laptop keyboard → MIDI** in standalone — deferred indefinitely (low priority)
  - KeyListener mapping A=C, W=C#, S=D, E=D#, D=E, F=F, T=F#, G=G, Y=G#, H=A, U=A#, J=B
