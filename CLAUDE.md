# Kinetik KMSP-6 — project memory for Claude

Kinetik KMSP-6 ("Kinetik Mono Synth Program") is a 6-track monophonic synthesizer plugin (VST3 / AU / CLAP,
built with DPF and NanoVG) with a note sequencer, inspired by the workflow of classic synth grooveboxes. All
synthesis is original code. Sister product: **Kinetik KDSP-16** (drums), repository `Kozzzmo/kinetik-kdsp16`.

## Source map (`src/`)

| File | Role |
|---|---|
| `Params.hpp` | 6 tracks, 128 steps, 16 patterns; 56 lockable params per track: 0–7 machine, AMP 8–15, FILTER 16–23 (CUTOFF RESO TYPE LOW CUT ENV ATK ENV DEC ENV AMT KEY TRK), FX 24–31, LFO1/2/3 at 32/38/44 (RATE DEPTH WAVE MODE DEST TARGET; DEST index 32 = PITCH) + SYNC/PHASE at 50+2l (`lfoParam(l,k)`); machines SAW PULSE SUPER FM WAVE VOX PLUCK NOISE CHORD (`kChordShapeDefs`, 3-8 voices, paraphonic); kit fields OUT MIDI TRNSP(=PITCH) LEGATO KEYTRK(unused, old banks) ARP mode/oct/speed/chord SCALE ROOT; scales `kScaleDefs`; master params incl. ECHO/REVERB MODEL + MOD |
| `MasterFx.hpp` | master echo (5 models) / reverb (5 models, own decay ranges, ROOM/HALL early reflections, SPRING tank loop) / EQ / compressor / limiter; MOD is model-specific (`kEchoModLabel` / `kReverbModLabel`, UI `masterName()`) — **identical in kinetik-kdsp16** |
| `Bank.hpp` | like KDSP-16 plus per-step `note` (60 = C3, Ableton naming), `gate` (index into `kGateSteps`) and `chord` (index into `kChords`, played by LINK); `Bank::glob` = global scale + LINK settings; header `KMSP2`; extra commands `n` `g` `h` (chord), `G scale root`, `LK first voices mode copy detune` |
| `Wavetable.hpp` | 32 band-limited wave frames × 11 mip levels, generated at start-up by FFT |
| `Voice.hpp` | `MonoVoice`: the 8 machines (VOX and NOISE have a slow AGC so resonance does not change loudness) |
| `Engine.hpp` | LINK (`linkMaster`, `pickVoice`, `linkNoteOn/Off`: tracks first..first+N-1 are the voices of the first; POLY / CYCLE / UNISON / RANDOM; COPY = voices use the first track's sound via `soundSource`), global scale (`scaleOf`); sequencer with notes, slide (glide without retrigger), legato, AHDSR, HP+LP filter with AD envelope, 3 LFOs, arpeggiator (sequencer chord or held notes), live note recording queue, master FX, 4 output buses |
| `MonoShared.hpp` | `MonoDirect` interface: the UI talks to the DSP directly (DISTRHO_PLUGIN_WANT_DIRECT_ACCESS) for live notes, recording queue, held-note display |
| `MonoPlugin.cpp` | DPF glue; MIDI routing: a track whose MIDI channel matches gets the notes, otherwise the selected track (AUTO) |
| `MonoUI.cpp` | the UI (1180×800 canvas): 9 pages (SYNTH AMP FILTER FX LFO1-3 TRACK ARP); the upper LCD draws a graphic per page (`drawPageGraphic`: scope rendered by a private `MonoVoice`, envelope, filter curve, FX, LFO, scale keyboard, arp roll); LFO destination menu (`ovDest`), master tabs ECHO / REVERB / MASTER / FILTER / LINK (`drawLink`; FILTER = `gMF_HP/LP/RESO`, KMSP-only globals after `gREV_MOD`, processed by `Engine::masterFilter` after `MasterFx` so MasterFx.hpp stays identical to KDSP-16), global SCALE box bottom left, RANDOMIZE SOUND / ALL SOUNDS (every track, no steps) / SEQUENCE / PATTERN / EVERYTHING + UNDO, preset name fitted inside the PRESETS button (`fitNumber`), 9 step modes (TRIG ACCENT SLIDE NOTE LEN RATCH COND TIME CHORD), multi-step selection (right-click on steps), multi-track group (right-click on track buttons, `trackGroup`: knobs = relative move via `groupBegin/groupFollow/groupEnd`, machine and kit fields = same value via `setMachineGroup` / `setKitGroup`; step locks stay on the selected track only), on-screen 3-octave keyboard, PC keys, step entry |
| `Presets.hpp` | 18 factory presets; `PB::seq()` takes note strings ("C2 . ~D#2 - !G1", x/X = hit on C5) |

`MonoUI.cpp` is the edited file (it was first assembled from parts; edit it directly now).
User presets: `.../Kinetik/KMSP-6/Presets/*.kmsp` (header `KMSP6-PRESET 2`).

## Working with the owner (read first)

- The owner (GitHub **Kozzzmo**) writes in **French**: answer in French. The plugin UI, code, comments and
  README stay in **English**.
- Git: commit as `Kozzzmo <Kozzzmo@users.noreply.github.com>`. **No `Co-Authored-By` or any Claude
  attribution** in commits, PRs or files — the owner asked explicitly not to appear as co-authored.
- The code is **proprietary** (see `LICENSE`). Never add GPL/LGPL code. Permissive third-party code is OK only
  if it is added to `THIRD_PARTY_NOTICES.txt`.
- Legal red lines: no Elektron names, firmware, samples, graphics or trade dress ("Machinedrum", "Monomachine",
  "Elektron" never appear in product text); no presets named after real artists; no VST logo (text mention only).
- UI preferences the owner validated: English UI; large readable text (labels use `textSize()` floor 9.5 px ×1.13
  mono); one logo for every skin (serif italic "Kinetik" + four-point star); **NEON** is the default skin;
  machine names without prefixes; no instruction text lines in the lower part of the window; window resizable
  (zoom buttons 80–200 % + drag grip); SAVE AS PRESET under PRESETS; factory "Init" = base params + empty
  sequencer; RANDOMIZE (sound / sequence / pattern / everything) with UNDO; REC records knob moves as locks.
- Launch tracker (claude.ai artifact, tasks in its db collection `tasks`):
  https://claude.ai/artifact/3DAkMghoFDeC59JUTAcdkn — update task status with the ArtifactData tool when work
  from the list is done.

## Build, test, release

- `make setup` (clones DPF at the pinned revision into `dpf/` and applies `scripts/dpfpatch.py`), then `make`
  (native) or `make universal` (macOS arm64 + x86_64). Windows cross-build from Linux:
  `make clean && make WINDOWS=true CC=x86_64-w64-mingw32-gcc CXX=x86_64-w64-mingw32-g++`.
  `make clean` also wipes `dpf/build` — required when switching target OS, otherwise DGL links fail.
- DPF patches (`scripts/dpfpatch.py`), all needed:
  1. VST3: ignore stale host parameter values right after a UI edit (Ableton Live sends the pre-edit value back).
  2. VST3: auxiliary stereo outputs flagged as aux buses, not main.
  3. Resize: size constraint follows the dragged edge (window could shrink but not grow).
- Tests are standalone programs: `g++ -O2 -std=c++17 tests/<name>.cpp -o /tmp/t && /tmp/t`.
  Also run Steinberg's VST3 `validator` on the built `.vst3` when touching plugin glue (all tests must pass).
- CI (`.github/workflows/build.yml`): macOS universal (+ `auval` AU validation, architectures reported as
  annotations), Windows (MinGW), Linux. Every push to `main` publishes/refreshes the GitHub Release named
  `v$(cat VERSION)`. Bump `VERSION` for a new release. Pushing git tags is refused by the session proxy —
  the workflow creates the tag itself.
- Read CI results with `gh api repos/Kozzzmo/<repo>/actions/runs` and
  `gh api repos/Kozzzmo/<repo>/check-runs/<job id>/annotations` (artifact zips cannot be downloaded from here).

## Architecture rules that bit us before

- Host parameters = continuous sound parameters only. **Machine, mute and all TRACK/kit settings live in the
  Bank** (state), never as host parameters: Ableton sends stale values for them (machine "offset" bug, mute
  needing three clicks).
- UI → DSP edits go through `setState("cmd", "<lines>")` (idempotent text commands, see `applyCommand` in
  `Bank.hpp`); the UI applies the same command to its own Bank copy. Whole bank: state `bank`. View: state
  `skin` = "<skin index> <zoom %>" (also saved to a per-user file for new instances).
- DSP side: the master Bank is guarded by a mutex; `run()` only `try_lock`s and copies dirty patterns
  (bitmask, `kKitBit` for kit changes). Never block or allocate in `run()`.
- UI parameter edits are sent twice: host parameter + `pv <index> <value>` line in the cmd state (`setParam` queues,
  `flushPv` in `uiIdle`). The plugin applies `pv` itself and, for 1.5 s, ignores host values that disagree
  (`uiEdit` / `uiHold`): hosts sent stale values and lost parts of bulk edits (randomize needed two presses).
- Solo: `Bank::solo` (command `S t 0/1`, serialized only when on), `trackSilenced()`; the plugin copies `solo`
  with `mute` on kit changes.
- Output parameters: `out_step` (absolute step counter modulo 2^20), `out_leds`, `out_pattern`.
- `paramSymbol()` tables have a `static_assert` on their size — a too-short table crashed Windows hosts once.
- Engine: per-block event list (sorted by frame then kind), swing, micro-timing, ratchets, conditions
  (probability, FILL, PRE, 1ST, A:B), pattern change at the master length, 4 stereo buses (Main + Out 2–4),
  master echo / FDN reverb / EQ / compressor / drive / safety limiter, denormals flushed (SSE and ARM64).
- Parameter names must stay readable and must not copy any hardware maker's abbreviations.
- Bank text header `KMSP2` (`KMSP1` = 0.1 filter layout, locks converted on load); user preset header `KMSP6-PRESET 2` (v1 converted by `presetIndexV1` + `migrateFilterV1`).
- Screenshots: `jackd -d dummy &`, `Xvfb :77 &`, run `bin/KinetikKMSP6` with DISPLAY=:77, `xdotool` to click, `import` to grab.
- `Skins.hpp`, `Fonts.hpp` (embedded OFL fonts), `MasterFx.hpp`, `Dsp.hpp`, `FsUtf8.hpp`, `MidiFile.hpp`, `Updater.*`,
  `License.hpp` and `tests/regress.cpp` are shared with the sister plugin
  (kinetik-kdsp16 ⇄ kinetik-kmsp6): keep them identical in both repositories.

## Added in KDSP-16 0.10 / KMSP-6 0.5 (both plug-ins)

- Undo/redo (`MonoUI.cpp`): stacks of `Snap` (values + serialized bank + label), depth 40. `setParam` / `sendCmd` call
  `autoSnap` (coalesced: 1.2 s for knobs, 0.5 s for edits; `undoable()` skips live / transport commands);
  `applyState` sets `restoring` so bulk changes are one step. Right-click UNDO = redo; Ctrl/Cmd+Z, Ctrl+Shift+Z, Ctrl+Y.
- Tooltips: `tipAt(x, y)` text table, shown by `uiIdle` after 0.8 s of hover, hidden on click (`tipBlockUntil`).
- Preset browser: `presetView` (filtered list of `PEntry`), tabs FACTORY / USER / FAVOURITES (`presetPage`), search
  typed in the window (`presetType`), favourites in `<Presets folder>/favourites.txt` ("F name" factory, "U name"
  user). MORPH slider (`morphBegin/morphDrag/morphApply`): base = sound when first moved; switch-like knobs
  (`switchTrackParam` / `switchGlobal`: LFO wave/mode/dest/target/sync, or at most 12 distinct value texts) and the
  machines (+ their synth knobs) change at 50 %; tempo, play, pattern never morph.
- MIDI export (`exportMidi`, `MidiFile.hpp`, shared): the edited pattern over its master length to
  `fs::exportDir("<product> MIDI")` (= Documents/Kinetik/...), then `upd::openFolder`. Conditions are not applied.
- Track tools: `Pattern::rotateTrack` (shift buttons), COPY also keeps the sound (`soundClip`), right-click PASTE
  pastes it; EUCLID / ROTATE on the TRACK page (`euclidSlot`, `applyEuclid`).
- `FsUtf8.hpp` (shared): every file path is UTF-8; on Windows the helpers use the wide CRT. Use `fs::open`,
  `fs::list`, `fs::makeDir`, `fs::remove`, `fs::env` — never fopen/opendir/getenv on user paths.
- `DriveAA` (`Dsp.hpp`): ADAA tanh with a 3-tap lift (flat to 10 kHz), one per channel: track DRIVE (`Track::distAA`)
  and master DRIVE (`MasterFx::drvL/drvR`). Voice-internal drives still use `drive()`.
- `MasterFx` idle: echo / reverb stop when their sends are silent and the tail is below -120 dB (estimated from
  feedback / decay); their buffers are cleared when they fall asleep. Reverb line modulation every 32 samples.
- Unspecified evaluation order: never call a random generator twice in one function call's arguments
  (`f(r.range(), r.range())`): gcc and clang evaluate them in opposite orders, so presets differed between Windows and macOS.
- Sound regression: `tests/regress.cpp` (shared) fingerprints every factory preset against `tests/regress-ref.txt`
  (1 dB tolerance; Linux CI strict, macOS report only). After an intended change of sound run
  `./regress --write` from the repo root and say so in the commit.
- Shared files: `scripts/check-shared.sh` checks them against `scripts/shared.sha256` (CI); after editing one,
  copy it to the sister repo and run `scripts/check-shared.sh <sister repo>` to refresh both lists.
- MIDI CC learn is not built in on purpose: every sound knob is a host parameter, so the host's own MIDI learn /
  mapping works on all of them; a plug-in-side learn would fight host automation and DPF's CLAP wrapper cannot
  report parameter changes made by the DSP.
- MIDI expression (0.5): `Engine::midiBend/midiMod/midiPress` per track (routed like notes), bend ±2 st and mod-wheel
  vibrato (5.5 Hz) added to the pitch in render, aftertouch opens the cutoff; `resetExpression()` on CC 121 / panic.

## Licences and demo mode

- `src/License.hpp` (identical in every Kinetik repo) verifies Ed25519-signed licence files with Monocypher
  (`src/third_party/monocypher`, BSD-2/CC0). Format: `KINETIK-LICENSE 1` + `product` (KDSP-16, KMSP-6 or BUNDLE),
  `name`, `email`, `order`, `issued`, then `signature` (hex) over the previous lines.
- Installed licences: `<user data>/Kinetik/Licenses/*.kinetik-license` (APPDATA on Windows,
  ~/Library/Application Support on macOS, ~/.config on Linux).
- **Beta: demo mode is OFF** (`lic::kDemoModeEnabled = false` in `License.hpp`); set it to true before selling.
- Without a valid licence (and demo mode on) the DSP mutes the outputs for 3 s every 45 s (`demoGate` in the plugin glue). The UI
  shows "DEMO - CLICK TO REGISTER" under the logo; the licence window loads a file (DPF file browser) or pastes
  text (button or Ctrl/Cmd+V), installs it and sends `cmd` "lic" so the DSP re-checks.
- Licences are made with `tools/make_license.py` and the PRIVATE key, which the owner keeps outside every
  repository. Never commit it, never ask for it to be pasted into a conversation that is shared.
- `tests/license.cpp` uses fixtures signed with a fixed test key that the plugins never trust.

## Updates (`src/Updater.hpp/.cpp`, identical in kinetik-kdsp16 and kinetik-kmsp6)

- The UI owns an `upd::Checker`: at most every 20 h (setting `auto` in `<user data>/Kinetik/updates.txt`, also
  `last-<product>` and `skip-<product>`) a thread downloads `releases/latest/download/update.txt` with the
  system curl (macOS `/usr/bin/curl`, Windows `System32\curl.exe`). Manifest `KINETIK-UPDATE 1`: product, version,
  notes, page, mac / mac-sha512, win / win-sha512, then an Ed25519 `signature` by the **update key** (not the licence
  key). Installer URLs must start with `https://github.com/Kozzzmo/`; the installer is opened only if its SHA-512
  matches. Logo subtitle "UPDATE x AVAILABLE - CLICK"; logo click = update window (`ovUpdate`).
- The release job signs `dist/update.txt` with `tools/sign_update.py` when the secret `KINETIK_UPDATE_KEY` (hex
  Ed25519 seed) exists, otherwise it only warns. The private key stays with the owner, never in a repository.
- `KN_VERSION` comes from `VERSION` (Makefile). Test: `tests/update.cpp` (links the two monocypher .c files).
- Copies older than KDSP-16 0.9 / KMSP-6 0.4 have no update check.
- The plug-in version (`getVersion`, bundle Info.plist) comes from `VERSION` via `KN_VER_MAJOR/MINOR/MICRO`
  (Makefile) — it was hard-coded once and macOS Installer then skipped the "unchanged" bundles. `make-pkg.sh` also
  sets `BundleIsVersionChecked false`; the NSIS installer refuses to continue while the plug-in file is locked.
  `upd::strayCopy` warns about old copies in the per-user plug-in folders.

## Packaging and signing

- Installers are built in CI: `packaging/macos/make-pkg.sh` (productbuild .pkg into /Library/Audio/Plug-Ins,
  EULA shown, bundles not relocatable) and `packaging/windows/installer.nsi` (NSIS, built on Linux with
  `makensis -NOCD`, installs VST3 + CLAP into Common Files, uninstaller registered). The `windows-install-test`
  job installs and uninstalls the real installer on a Windows runner.
- Signing switches on by itself when the repository secrets exist (listed at the top of the workflow):
  Developer ID + notarization on macOS, `scripts/sign-windows.sh` (osslsigncode, .pfx) on Windows.
- `EULA.txt` (draft with [PLACEHOLDERS] the owner must fill) and `THIRD_PARTY_NOTICES.txt` ship in every package.
