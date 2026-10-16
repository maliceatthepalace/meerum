# Meerum

Ein Wavetable-Synthesizer, inspiriert von modernen Wavetable-Synths – als **VST3** (FL Studio, Ableton Live, Bitwig, Reaper …),
**AU** (Ableton/Logic auf dem Mac), **LV2** (Linux) und als **Standalone-App**.

## Features

- **2 Wavetable-Oszillatoren** (A/B) mit morphbarer WT-Position, Oktave/Halbton/Fein, bis zu **16 Unison-Stimmen**
  (Detune, Stereo-Breite), Phase/Random-Phase und **Warp-Modi** (Sync, Bend ±, Mirror, PWM, Quantize)
- 8 eingebaute Wavetables (Basic Shapes, Pulse Width, Harmonic Sweep, Vowels, Sync Saw, FM Bells, Wavefold, Digital Steps)
- **Eigene Wavetables laden**: WAV/AIFF/FLAC per „WAV“-Button oder Drag & Drop auf die Anzeige.
  Serum-kompatibel: Dateien aus aufeinanderfolgenden 2048-Sample-Frames werden als Frames erkannt.
  Importierte Tables werden im Projekt/Preset mitgespeichert.
- Band-limitierte Mip-Maps → sauberer Klang ohne Aliasing auch in hohen Lagen
- **Sub-Oszillator** (Sine/Triangle/Saw/Square) und **Noise** (White/Pink)
- **Filter**: LP 12/24, HP 12/24, BP, Notch mit Resonanz, Drive, Keytracking, Mix; pro Quelle routbar (A/B/S/N)
- **3 Hüllkurven** (AHDSR, ENV 1 = Lautstärke), **3 LFOs** (7 Formen, Tempo-Sync, Retrig/Free/One-shot)
- **Mod-Matrix** mit 8 Slots: Quellen Env 1–3, LFO 1–3, Velocity, Note, Mod Wheel, Aftertouch, Random →
  WT-Pos, Pitch, Level, Pan, Detune, Warp, Filter, LFO-Rate, Amp …
- **Effekte**: Distortion (Soft/Hard/Fold/Crush), Chorus, Delay (Sync, Ping-Pong), Reverb
- Poly bis 16 Stimmen, Mono/Legato, Glide, Pitch-Bend-Bereich, Sustain-Pedal
- 8 Factory-Presets, eigene Presets speichern (`Dokumente/Meerum/Presets/*.mrpreset`)
- **Eigene Farben**: Über „THEME“ oben rechts Dark-/Light-Modus und Akzentfarbe (Standard: Grün) wählen –
  wird als persönliche Einstellung gespeichert und gilt für alle Projekte

## Download

Fertige Versionen für **Windows, macOS und Linux** gibt es unter
[**Releases**](https://github.com/maliceatthepalace/meerum/releases/latest) – inklusive Installationsanleitung.

## Bauen

Voraussetzungen: **CMake ≥ 3.22**, **Git** und ein C++17-Compiler.

```bash
git clone --recursive https://github.com/maliceatthepalace/meerum.git    # oder nachträglich: git submodule update --init
cd meerum
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel
```

Die fertigen Plugins liegen danach in `build/Meerum_artefacts/Release/` (`VST3/`, `AU/`, `LV2/`, `Standalone/`).
Mit `-DMEERUM_COPY_AFTER_BUILD=ON` beim ersten `cmake`-Aufruf werden sie automatisch in die Plugin-Ordner kopiert.

### macOS
Xcode oder die Command Line Tools (`xcode-select --install`) und `brew install cmake`.
Standardmäßig wird ein Universal Binary (Apple Silicon + Intel) gebaut.

Plugins installieren:
```bash
cp -R build/Meerum_artefacts/Release/VST3/Meerum.vst3 ~/Library/Audio/Plug-Ins/VST3/
cp -R build/Meerum_artefacts/Release/AU/Meerum.component ~/Library/Audio/Plug-Ins/Components/
```

> **Fehler `'algorithm' file not found`?** Dann verdeckt eine veraltete Kopie unter
> `/Library/Developer/CommandLineTools/usr/include/c++` die SDK-Header. Workaround für den Build:
> `export CPLUS_INCLUDE_PATH="$(xcrun --show-sdk-path)/usr/include/c++/v1"` – in jedem Terminal **vor jedem**
> `cmake`-Aufruf (Konfigurieren *und* Bauen). Dauerhaft behebt es eine Neuinstallation der Command Line Tools
> (`sudo rm -rf /Library/Developer/CommandLineTools && xcode-select --install`).

### Windows
Visual Studio 2022 (Workload „Desktopentwicklung mit C++“) + CMake. Die Befehle oben in der
„Developer PowerShell“ ausführen. `Meerum.vst3` nach `C:\Program Files\Common Files\VST3\` kopieren.

### Linux
```bash
sudo apt install build-essential cmake git libasound2-dev libjack-jackd2-dev libfreetype6-dev \
  libfontconfig1-dev libx11-dev libxcomposite-dev libxcursor-dev libxext-dev libxinerama-dev \
  libxrandr-dev libxrender-dev libglu1-mesa-dev mesa-common-dev
```
VST3 nach `~/.vst3/`, LV2 nach `~/.lv2/` kopieren.

### Fertige Builds über GitHub
Das Repo enthält einen GitHub-Actions-Workflow (`.github/workflows/build.yml`), der bei jedem Push für
macOS, Windows und Linux baut, testet und die Plugins als Download-Artefakte bereitstellt.

## In der DAW

- **FL Studio**: *Options → Manage plugins → Find plugins*, danach im Plugin-Browser unter „Meerum“.
- **Ableton Live**: *Einstellungen → Plug-Ins → VST3 Custom Folder / Plug-In-Quellen* aktivieren und neu scannen.
  Auf dem Mac erscheint Meerum zusätzlich als Audio Unit.

## Test

`MeerumRenderTest` rendert alle Presets offline als WAV, prüft auf NaN/Stille/Übersteuerung, testet
Wavetable-Import und State-Wiederherstellung, misst die CPU-Last und speichert Screenshots der Oberfläche:

```bash
cmake --build build --target MeerumRenderTest
cd build && ./MeerumRenderTest_artefacts/Release/MeerumRenderTest test-renders
```

## Lizenz

Copyright © 2026 maliceatthepalace

Meerum ist freie Software unter der **GNU Affero General Public License v3.0** (siehe [LICENSE](LICENSE)).
Du darfst Meerum benutzen, weitergeben und verändern. Wer eine veränderte oder unveränderte Version
weitergibt, muss den vollständigen Quellcode unter derselben Lizenz mitliefern.

Meerum basiert auf [JUCE](https://juce.com) 8 (ebenfalls AGPLv3) und dem darin enthaltenen VST3 SDK.
VST ist eine Marke der Steinberg Media Technologies GmbH. Meerum ist ein unabhängiges Projekt und
nicht mit Xfer Records (Serum) verbunden.
