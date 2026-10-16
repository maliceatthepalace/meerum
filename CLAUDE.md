# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

Meerum is a Serum-style wavetable synth plugin (VST3 / AU / LV2 / Standalone) built with JUCE 8 (git submodule in `external/JUCE`, pinned to 8.0.4) and CMake. The user communicates in German; README is German.

## Build & test

```bash
# This Mac's Command Line Tools have a stale partial libc++ in /Library/Developer/CommandLineTools/usr/include/c++
# that shadows the SDK headers ("'algorithm' file not found"). This env var is needed for EVERY cmake call
# (configure and build) — the Bash tool doesn't keep env vars between calls, so prefix each command with it:
export CPLUS_INCLUDE_PATH="$(xcrun --show-sdk-path)/usr/include/c++/v1"
cmake -B build -S . -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=arm64   # arm64 only = faster; omit for the default universal binary
cmake --build build                               # all formats + test
cmake --build build --target Meerum_Standalone    # just the standalone app
cmake --build build --target MeerumRenderTest && (cd build && ./MeerumRenderTest_artefacts/Release/MeerumRenderTest test-renders)
```

- `MeerumRenderTest` (tests/RenderTest.cpp) is the only test: renders every factory preset + every built-in table to WAV, checks NaN/silence/clipping, custom-WAV import, state round-trip, prints CPU usage, and writes `ui-osc/fx/matrix.png` editor snapshots (useful to visually check UI changes without launching a host). Exit code ≠ 0 on failure.
- Plugin validation: `tools/pluginval/pluginval.app/Contents/MacOS/pluginval --strictness-level 10 --validate-in-process build/Meerum_artefacts/Release/VST3/Meerum.vst3` (tools/ is git-ignored; re-download from Tracktion/pluginval releases if missing). AU: copy the .component to `~/Library/Audio/Plug-Ins/Components/` then `auval -v aumu Meer Mlcp`.
- CI (`.github/workflows/build.yml`) builds, runs the render test and packages zips on macOS, Windows and Linux — the only place Windows/Linux get compiled. Pushing a `v*` tag additionally publishes a GitHub Release with those archives and `.github/release-notes.md` as body. Release = bump `project(... VERSION)` in CMakeLists.txt, commit, `git tag vX.Y.Z && git push origin main vX.Y.Z`.
- macOS packaging re-signs the bundles ad-hoc (`codesign --force --deep -s -`) and zips without xattrs; JUCE adds files after linking, which otherwise leaves an invalid seal ("damaged" on users' Macs).
- Sources are globbed (`Source/*.cpp`, `CONFIGURE_DEPENDS`), so new files need no CMake edit; the render test compiles the same sources a second time with `JucePlugin_Name` defined manually.

## Architecture

- **`Source/Parameters.h/.cpp`** is the contract between DSP and UI: all parameter IDs (built with `Params::osc/env/lfo/mod(i, name)` helpers), ranges, choice lists, and the mod-matrix source/destination enums. `ParamCache` holds raw `std::atomic<float>*` for lock-free audio-thread reads. Adding a parameter = add it in `createLayout()`, `ParamCache` + `init()`, then snapshot it in `SynthEngine::takeSnapshot`. Choice-list order matters: enums like `ModDest` and `Params::customTableIndex()` index into them, and changing IDs/order breaks saved projects and presets.
- **Audio path** (`MeerumProcessor::processBlock`): `SynthEngine` (voices, replaces buffer) → `FxChain` (dist → chorus → delay → reverb) → master gain.
- **`SynthEngine`** copies all params once per block into a plain `SynthSnapshot`, then renders voices between MIDI events (sample-accurate). Handles poly voice stealing, mono note stack/legato, sustain pedal, glide (from last note, also in poly).
- **`Voice`** renders in control blocks of 32 samples: mod sources are sampled at block start, mod-matrix sums into `mods[ModDest]`, gains/frame position ramp linearly within the block. ENV 1 is hard-wired to amplitude and evaluated per sample. Signal: osc A/B/sub/noise → per-source routing into filtered or dry bus → stereo SVF (`Modulators.h`) → amp.
- **`Wavetable`**: frames of 2048 samples, each stored as 11 band-limited mip levels built with an in-house FFT (deliberately not `juce::dsp::FFT`, whose scaling differs per platform backend). `levelForIncrement` picks the mip level from the phase increment. `WavetableBank::builtIns()` generates the 8 factory tables procedurally (once, in the processor constructor). The last `tableNames()` entry = "Custom (WAV)", which uses the oscillator's imported table.
- **Custom tables**: owned by the processor as `shared_ptr`, swapped under a `SpinLock`; the audio thread holds its own reference per block, and replaced tables are parked in `retiredTables` so they are never freed on the audio thread. Imported tables are serialised (base64 raw frames) into plugin state and `.mrpreset` files.
- **Presets**: factory presets in `FactoryPresets.h` (only non-default values; exposed as host programs, applying one resets everything to defaults first). User presets are the full state XML in `~/Documents/Meerum/Presets/*.mrpreset`. `presetGeneration` is bumped on any load so the editor can refresh.
- **UI** (`PluginEditor` + `Source/ui/`): editor lays everything out at a fixed 1060×720 inside one content component and scales it via transform (fixed aspect ratio). `Theme` = mutable global palette (dark/light + user accent, default green; persisted via `PropertiesFile` in the user app-data folder, shared by all instances) + LookAndFeel; editors poll `Theme::generation()` and call `refreshTheme()`, which re-applies LookAndFeel colours and `sendLookAndFeelChange()` — components that set explicit colours must re-apply them in `lookAndFeelChanged()`. `ThemePanel` = the THEME call-out, `Widgets` = APVTS-attached knob/toggle/combo wrappers, `Pages` = OSC/FX/MATRIX pages, `Displays` = wavetable 3D view, envelope/LFO/filter graphs. Displays poll parameters/processor with timers and call `timerCallback()` once in the constructor so the first paint is correct. `FilterCurve` duplicates the SVF resonance mapping (`k = 2 - 1.96 * res`) — keep in sync with `SvfFilter::setCoefficients`.
