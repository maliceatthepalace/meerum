**Meerum** – free & open-source wavetable synthesizer (VST3 / AU / LV2 / Standalone).
**Meerum** – kostenloser Open-Source-Wavetable-Synthesizer (VST3 / AU / LV2 / Standalone).

## Downloads

| System | File | Contains |
|---|---|---|
| Windows (64-bit) | `Meerum-*-Windows.zip` | VST3, Standalone |
| macOS (Apple Silicon + Intel) | `Meerum-*-macOS.zip` | VST3, AU, Standalone |
| Linux (x86_64) | `Meerum-*-Linux.tar.gz` | VST3, LV2, Standalone |

## Installation (Deutsch)

**Windows:** ZIP entpacken und den Ordner `Meerum.vst3` nach `C:\Program Files\Common Files\VST3\` kopieren.
In FL Studio: *Options → Manage plugins → Find installed plugins*. In Ableton: *Plug-Ins → neu scannen*.

**macOS:** ZIP entpacken und kopieren:
`Meerum.vst3` → `~/Library/Audio/Plug-Ins/VST3/`, `Meerum.component` → `~/Library/Audio/Plug-Ins/Components/`.
Meerum ist nicht bei Apple notarisiert, deshalb blockiert macOS es zunächst. Einmal im Terminal ausführen:
```
xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/VST3/Meerum.vst3 ~/Library/Audio/Plug-Ins/Components/Meerum.component
```
Danach in der DAW neu scannen.

**Linux:** Archiv entpacken, `Meerum.vst3` nach `~/.vst3/` und `Meerum.lv2` nach `~/.lv2/` kopieren.

## Installation (English)

**Windows:** Unzip and copy the `Meerum.vst3` folder to `C:\Program Files\Common Files\VST3\`, then rescan plugins in your DAW.

**macOS:** Unzip, copy `Meerum.vst3` to `~/Library/Audio/Plug-Ins/VST3/` and `Meerum.component` to
`~/Library/Audio/Plug-Ins/Components/`. Meerum is not notarized by Apple, so remove the quarantine flag once:
```
xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/VST3/Meerum.vst3 ~/Library/Audio/Plug-Ins/Components/Meerum.component
```

**Linux:** Extract, copy `Meerum.vst3` to `~/.vst3/` and `Meerum.lv2` to `~/.lv2/`.

---
Licensed under AGPLv3 · Built with JUCE · Source code: see below.
