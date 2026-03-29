# Snare Rhythm Generator (VST3/AU) - Claude Code Context

## Project Overview
- JUCE 7.0.12 + CMake plugin generating snare MIDI patterns with genre awareness.
- Formats: VST3, AU (macOS), Standalone
- Source: `Source/` — PluginProcessor, PluginEditor, SnareEngine
- IS_SYNTH (not MIDI effect) for Bitwig/Ableton/Logic compatibility.

## Build

### Windows (VST3)
```
cmake -B build -S .
cmake --build build --config Release
```
Install: copy `build/.../VST3/SnareRhythmGenerator.vst3` → `C:\Program Files\Common Files\VST3\`

### macOS (VST3 + AU)
Requirements: Xcode CLI tools + CMake (`brew install cmake`)
```
cmake -B build -S .
cmake --build build --config Release
```
Install:
```
cp -r build/.../VST3/SnareRhythmGenerator.vst3 ~/Library/Audio/Plug-Ins/VST3/
cp -r build/.../AU/SnareRhythmGenerator.component ~/Library/Audio/Plug-Ins/Components/
```
Validate AU: `auval -v aumu SRhG Snrg`

## Open TODOs
- APVTS for host automation
- MIDI drag-export
- Preset system
