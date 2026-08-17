# Snare Rhythm Generator

A genre-aware snare pattern generator for VST3 / AU / Standalone, built with [JUCE](https://juce.com) 7.0.12 and CMake.

Pick a genre, turn a few knobs, and the plugin composes a multi-bar snare part — backbeats, ghost notes, accents, flams, and fills — then plays it back as MIDI (and optionally through a loaded one-shot sample). Every pattern is scored against genre expectations so you can see at a glance whether it sits right.

- **Formats:** VST3, AU (macOS only), Standalone
- **Version:** 1.2 — *by Gleinkaa*
- **13 genres:** boom-bap, breakbeat, drum-and-bass, experimental, funk, hip-hop, house, jazz, pop, reggae, reggaeton, techno, trap

---

## Features

| | |
|---|---|
| **Genre profiles** | Each genre carries its own BPM range, backbeat strength, default swing, ghost-note affinity, fill style (buildup / breakout / minimal / rolls), syncopation and density bias, and per-subdivision beat weights. |
| **Motif-based generation** | A one-bar motif is generated first, then varied per bar — variation ramps up over the phrase and spikes on every 4th bar. |
| **Hit types** | Primary, Ghost, Accent, Fill, and Flam, each with its own velocity curve and timing feel. |
| **Humanize & swing** | Per-hit velocity jitter and timing looseness (ghosts and fills drift more than backbeats); swing pushes off-beat subdivisions. |
| **Probability gating** | Hits can be dropped stochastically — backbeats are protected when Backbeat Lock is on. |
| **Quality scoring** | Density, backbeat, dynamics, ghost balance, variation, and coverage, plus a weighted overall score, shown as live meters. |
| **Sample player** | Drop a `.wav` / `.aiff` / `.mp3` / `.flac` / `.ogg` onto the editor (or use LOAD SAMPLE) for 8-voice velocity-scaled playback, auto-resampled to the host rate. |
| **Presets** | Save/load all parameters — including the sample path — as `.srpreset` XML; full state is also persisted in the DAW session. |
| **MIDI export** | Write the current pattern to a standard `.mid` file (480 PPQ). |
| **Deterministic seeding** | Set a seed (0–9999) to reproduce a pattern exactly; `-1` uses a random device. |

---

## Build

Requirements: **CMake ≥ 3.22** and a C++17 compiler. JUCE 7.0.12 is fetched automatically by CMake (`FetchContent`) on the first configure, so the first build needs network access and takes a while.

```bash
cmake -B build -S .
cmake --build build --config Release
```

Artefacts land in `build/SnareRhythmGenerator_artefacts/Release/`:

```
VST3/SnareRhythmGen.vst3
AU/SnareRhythmGen.component      # macOS only
Standalone/SnareRhythmGen(.exe)
```

> The bundle is named after `PRODUCT_NAME` (`SnareRhythmGen`), **not** after the CMake target (`SnareRhythmGenerator`). See [Known issues](#known-issues).

### Windows

`install.bat` (double-click) self-elevates to admin, configures, builds Release, and copies the VST3 to `C:\Program Files\Common Files\VST3\`. It converts the repo path to an 8.3 short path first, working around a `juceaide` crash on non-ASCII characters (umlauts) in the path.

Manual install:

```
copy build\SnareRhythmGenerator_artefacts\Release\VST3\SnareRhythmGen.vst3
  -> C:\Program Files\Common Files\VST3\
```

### macOS

Requires Xcode Command Line Tools and CMake (`brew install cmake`).

```bash
cp -r build/SnareRhythmGenerator_artefacts/Release/VST3/SnareRhythmGen.vst3 ~/Library/Audio/Plug-Ins/VST3/
cp -r build/SnareRhythmGenerator_artefacts/Release/AU/SnareRhythmGen.component ~/Library/Audio/Plug-Ins/Components/
```

Validate the AU:

```bash
auval -v aumu SRhG Snrg
```

### Linux

Not part of the release pipeline, but VST3 and Standalone build with the standard JUCE Linux dependencies (X11, freetype, ALSA, etc.). AU is macOS-only and is silently skipped elsewhere.

---

## Usage

The plugin is built as an **instrument** (`IS_SYNTH TRUE`, not a MIDI effect) so hosts like Bitwig, Ableton Live, and Logic load it on an instrument track. It both **produces MIDI** (channel 10) and renders audio when a sample is loaded — route its MIDI output to a drum instrument, or just load a snare sample into the plugin itself.

Default MIDI note is **38** (acoustic snare, GM); change it with the `NOTE` knob.

### Editor walkthrough

1. **Genre bar** (top strip) — click a genre. Selecting one applies that genre's defaults for swing, ghost notes, syncopation, density, and complexity, then regenerates.
2. **Knobs** — drag vertically to change, hold **Shift** for fine adjustment, **scroll** to nudge, **double-click** to reset to default. Every change regenerates the pattern immediately.
3. **Sample drop zone** — drag an audio file anywhere onto the editor, or click `LOAD SAMPLE`. A loaded sample shows a green border and its name in the header.
4. **Buttons** — `GENERATE` (new pattern), `PLAY`/`STOP` (internal transport with a moving cursor), `SAVE`/`LOAD` (`.srpreset`), `EXPORT MIDI` (`.mid`).
5. **Pattern grid** — one row per bar, one column per subdivision. Colours: cyan = primary, orange = accent/fill, gray = ghost, teal = flam. Opacity tracks velocity.
6. **Quality panel** — six scores plus an overall percentage, recomputed on every generation.

Playback follows the host tempo when a playhead is available, falling back to the `bpm` parameter otherwise.

### Parameters

| Group | Knob | Range | Meaning |
|---|---|---|---|
| Rhythm | `CMPLX` | 0–1 | Pattern complexity — how many off-motif placements are considered |
| Rhythm | `DENSE` | 0–1 | Overall hit density |
| Rhythm | `SYNC` | 0–1 | Syncopation — weight given to off-beat subdivisions |
| Rhythm | `SWING` | 0–1 | Swing amount (0 falls back to the genre default) |
| Feel | `HUMAN` | 0–1 | Velocity and timing randomisation |
| Feel | `ACCNT` | 0–1 | Accent strength on strong beats |
| Feel | `GHOST` | 0–1 | Ghost-note amount (scaled by genre ghost affinity) |
| Feel | `LOOSE` | 0–1 | Timing looseness, multiplied by Humanize |
| Structure | `VARI` | 0–1 | Bar-to-bar variation |
| Structure | `MOTIF` | 0–1 | How strictly each bar follows the motif |
| Structure | `FILL` | 0–1 | Fill frequency — ≥0.8 every bar after the first, ≥0.5 every 2nd, ≥0.25 every 4th, else last bar only |
| Structure | `FLAM` | 0–1 | Probability of a grace note before primaries/accents |
| Output | `V.MIN` / `V.MAX` | 1–127 | Velocity range |
| Output | `GATE` | 0.25–2.0 | Note-length multiplier |
| Output | `S.VOL` | 0–1 | Sample-player volume |
| Right | `BARS` | 1–16 | Phrase length |
| Right | `NOTE` | 0–127 | MIDI note number |
| Right | `SEED` | 0–9999 | Random seed |
| Toggle | `Backbeat Lock` | — | Forces accents on the genre's strong beats and protects them from probability gating |

---

## Key files

| Path | What's in it |
|---|---|
| `Source/SnareEngine.h` | Header-only generator — genre profiles, `Params`, motif construction, fills, flams, swing, humanize, probability gating, and scoring. No JUCE dependency. |
| `Source/PluginProcessor.h/.cpp` | `SnareProcessor` — audio thread, MIDI scheduling, 8-voice sample player, preset and DAW state (de)serialisation. Generated data is published to the audio and GUI threads via `shared_ptr` + `std::atomic_load/store`. |
| `Source/PluginEditor.h/.cpp` | Custom `Knob` component, blue dark theme (`Col::`), pattern grid, quality meters, drag-and-drop sample loading. |
| `CMakeLists.txt` | `juce_add_plugin` config — formats, plugin codes (`SRhG` / `Snrg`), bundle ID `com.gleinkaa.snare-rhythm-gen`. |
| `install.bat` | Windows build-and-install helper (self-elevating). |
| `installer/windows/snare_installer.iss` | Inno Setup 6 script — VST3 + Standalone installer. |
| `installer/macos/build_pkg.sh` | macOS `.pkg` builder (`pkgbuild` + `productbuild`), with VST3/AU as selectable choices. |
| `.github/workflows/build.yml` | CI — builds Windows and macOS installers on `v*` tags or manual dispatch, then publishes a GitHub Release. |
| `CLAUDE.md` | Context notes for Claude Code. |

Thread-safety and timing notes worth knowing before touching `processBlock`: display and audio data are swapped atomically rather than mutated in place, note-offs that cross a block boundary are carried in `pendingOffs`, tempo is read as a `double` from the host playhead to avoid drift, and the GUI extrapolates the playback cursor from the published sample counter.

---

## Known issues

The build scripts look for artefact names that CMake doesn't produce. `PRODUCT_NAME` is `SnareRhythmGen`, so the bundle is `SnareRhythmGen.vst3`, but:

- `install.bat` expects `Snare Rhythm Generator.vst3`
- `installer/windows/snare_installer.iss` expects `SnareRhythmGenerator.vst3` / `SnareRhythmGenerator.exe`
- `installer/macos/build_pkg.sh` expects `SnareRhythmGenerator.vst3` / `SnareRhythmGenerator.component`

All three need their paths aligned with `PRODUCT_NAME` (or `PRODUCT_NAME` changed to match) before packaging works. The installer scripts also still declare version `1.1` while the project is at `1.2.0`.

## Roadmap

- APVTS for host automation
- MIDI drag-export from the pattern grid
- Built-in preset browser

## License

Not yet declared at the repo root. The macOS installer embeds a permissive MIT-style notice: *Copyright (c) 2026 SnareGen / Gleinkaa — provided "as is", without warranty of any kind.*
