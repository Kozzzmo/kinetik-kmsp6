# Kinetik KMSP-6 — Mono Synth Program

A 6-track monophonic synthesizer plugin (VST3 / AU / CLAP): eight machines (SAW, PULSE, SUPER, FM,
WAVE, VOX, PLUCK, NOISE), AHDSR envelope, dual filter with envelope, 3 LFOs per track, an arpeggiator
per track, a 64-step note sequencer with slides, note lengths, parameter locks, conditional trigs,
ratchets and live recording, an on-screen keyboard, four stereo outputs, master echo / reverb /
compressor, 4 interface skins and a resizable window. Built with [DPF](https://github.com/DISTRHO/DPF).
All synthesis is original code; no samples or firmware.

## Download (no compiling)

Go to the **Releases** page (right-hand column of the repository, or `/releases`) and download the zip
for your system: `Kinetik-KMSP6-macOS.zip` (universal: Apple Silicon + Intel), `-Windows.zip` or `-Linux.zip`.

### Install on macOS

1. Unzip, then copy:
   - `KinetikKMSP6.vst3` → `~/Library/Audio/Plug-Ins/VST3/`
   - `KinetikKMSP6.component` → `~/Library/Audio/Plug-Ins/Components/` (Audio Unit, for Logic / GarageBand)
   - `KinetikKMSP6.clap` → `~/Library/Audio/Plug-Ins/CLAP/`
2. The plugin is not notarized by Apple. Remove the download quarantine once, in Terminal:
   ```sh
   xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/VST3/KinetikKMSP6.vst3
   xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/Components/KinetikKMSP6.component
   xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/CLAP/KinetikKMSP6.clap
   ```
3. Rescan plugins in your DAW (Ableton Live: Settings → Plug-Ins → Rescan).

## Build it yourself on a Mac

Requirements: Xcode command line tools (`xcode-select --install`), git, python3.

```sh
git clone <this repo> kinetik-kmsp6
cd kinetik-kmsp6
make setup        # downloads DPF at the tested revision and applies the patches in scripts/
make universal    # or just `make` for your Mac's own architecture
```

The plugins appear in `bin/`. Ad-hoc sign them before loading on Apple Silicon:

```sh
codesign --force --deep --sign - bin/KinetikKMSP6.vst3 bin/KinetikKMSP6.component bin/KinetikKMSP6.clap
```

Other systems: `make` on Linux; on Linux with MinGW,
`make WINDOWS=true CC=x86_64-w64-mingw32-gcc CXX=x86_64-w64-mingw32-g++` builds the Windows version.
Run `make clean` when switching target systems.

## Layout

| Path | Contents |
|---|---|
| `src/` | the plugin: engine (`Engine.hpp`, `Voice.hpp`, `Dsp.hpp`), sequencer data (`Bank.hpp`), UI (`MonoUI.cpp`), plugin glue (`MonoPlugin.cpp`), presets, help texts, skins |
| `scripts/dpfpatch.py` | small DPF patches: ignore stale host values after UI edits (Ableton), auxiliary output buses, resize fix |
| `tests/` | offline engine tests (`g++ -O2 -std=c++17 tests/features.cpp && ./a.out`) |
| `fonts/` | licences of the embedded fonts (SIL Open Font License) |
| `LICENSE`, `THIRD_PARTY_NOTICES.txt` | proprietary licence; third-party licences to ship with the product |
| `VERSION` | version number: pushing to `main` publishes the matching GitHub Release |

## User data

- User presets: `~/Library/Application Support/Kinetik/KMSP-6/Presets/*.kmsp` (Windows: `%APPDATA%\Kinetik\KMSP-6\Presets`)
- Default skin and size: `~/Library/Preferences/KinetikKMSP6-view.txt`

## Licence

Proprietary — all rights reserved (see `LICENSE`). Third-party components (DPF, pugl, NanoVG, CLAP headers,
fonts) are used under their own permissive licences, listed in `THIRD_PARTY_NOTICES.txt`, which must ship
with every build. VST is a trademark of Steinberg Media Technologies GmbH.

---

### En bref (FR)

Pour Mac : page **Releases** du dépôt → télécharger **Kinetik-KMSP6-macOS.zip**, puis copier les fichiers
dans `~/Library/Audio/Plug-Ins/…` et lancer les commandes `xattr` ci-dessus. Pour compiler soi-même :
`make setup` puis `make universal`.
