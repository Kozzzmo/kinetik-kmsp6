# Kinetik KMSP-6 — project memory for Claude

Kinetik KMSP-6 ("Kinetik Mono Synth Program") is an 8-track (6 before 0.10) monophonic synthesizer plugin (VST3 / AU / CLAP,
built with DPF and NanoVG) with a note sequencer, inspired by the workflow of classic synth grooveboxes. All
synthesis is original code. Sister product: **Kinetik KDSP-16** (drums), repository `Kozzzmo/kinetik-kdsp16`.

## Source map (`src/`)

| File | Role |
|---|---|
| `Params.hpp` | 8 tracks (see 0.10), 128 steps, 16 patterns; 56 lockable params per track: 0–7 machine, AMP 8–15, FILTER 16–23 (CUTOFF RESO TYPE LOW CUT ENV ATK ENV DEC ENV AMT KEY TRK), FX 24–31, LFO1/2/3 at 32/38/44 (RATE DEPTH WAVE MODE DEST TARGET; DEST index 32 = PITCH) + SYNC/PHASE at 50+2l (`lfoParam(l,k)`); machines SAW PULSE SUPER FM WAVE VOX PLUCK NOISE CHORD (`kChordShapeDefs`, 3-8 voices, paraphonic); kit fields OUT MIDI TRNSP(=PITCH) LEGATO KEYTRK(unused, old banks) ARP mode/oct/speed/chord SCALE ROOT; scales `kScaleDefs`; master params incl. ECHO/REVERB MODEL + MOD |
| `MasterFx.hpp` | master echo (5 models) / reverb (5 models, own decay ranges, ROOM/HALL early reflections, SPRING tank loop) / EQ / compressor / limiter; MOD is model-specific (`kEchoModLabel` / `kReverbModLabel`, UI `masterName()`) — **identical in kinetik-kdsp16** |
| `Bank.hpp` | like KDSP-16 plus per-step `note` (60 = C3, Ableton naming), `gate` (index into `kGateSteps`) and `chord` (index into `kChords`, played by LINK); `Bank::glob` = global scale + LINK settings; header `KMSP2`; extra commands `n` `g` `h` (chord), `G scale root`, `LK first voices mode copy detune` |
| `Wavetable.hpp` | `WaveSet` (up to 32 band-limited frames × 11 mip levels, built by FFT); `WaveBank` = 8 factory tables built at start-up on several threads (CLASSIC = the table before 0.9); `WaveUser.hpp` = user tables (import, state codec) |
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
  mono); one logo for every skin (the "Orbit" mark + product name in Unbounded, see 0.13 / 0.8); **NEON** is the default skin;
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

## Added in KDSP-16 0.11 / KMSP-6 0.6 (both plug-ins)

- New globals (same order in both `Params.hpp`, before the outputs, so `MasterFx` indices match): `gMF_HP/LP/RESO`,
  chorus `gCHO_MODEL..gCHO_LEV` (8), CHAOS (ex glitch) `gGL_CHANCE..gGL_MIX` (7), then `gCHO_SEND + t` = one chorus send per track
  (a global host parameter, not lockable). `gOUT_STEP = gCHO_SEND + kTracks`. Outputs moved (not automatable).
- `MasterFx::process(..., sendCL, sendCR, beat0, playing, ...)`: chorus = send effect (CHORUS / ENSEMBLE / FLANGER,
  150 Hz high-pass on its input, sleeps like echo / reverb); glitch on the summed mix before the EQ: a slice
  (`kGlSizeBeats`) captured at a beat (chance per bar) or on HOLD (`glHold`, UI command `gh 0/1`, `Engine::setGlitchHold`)
  is replayed for `kGlLenBeats` with pitch glide, fade + darkening, random reverse, crossfaded edges. Engines pass the
  segment's song position (`blockB0 + segPos * blockBpf`).
- UI master tabs: ECHO REVERB MASTER FILTER CHORUS GLITCH (+ LINK on KMSP), width from `kMPages`; `fxTabDots()` marks
  active effects; GLITCH knob 8 = HOLD pad (`glHoldPad`, right-click latches); chorus send knob `choSlot(k)` / `dkChoSend`.
- Per-track SPEED: kit field `kfSpeed` (index into `kTrackSpeedMult`, 0 = 1x). The sequencer runs each track on its own
  grid (`sbt = sb / mult`; patterns still switch on the global grid; FIRST conditions use the track's start step).
  `out_step` is now in 1/24 steps (`kFineSteps`, modulo 32768 steps); UI `fineStep()`, `absStep()`, `trackK(t)`.
  Test: `tests/speed.cpp` (shared).
- RESONANCE: `resoQ(v, qMin, qMax)` in `Dsp.hpp` (exponential, x^1.25) for track filters, master filter and the
  filter graphic. Factory presets are converted in `buildPreset` (knob values and locks) so they sound as before.
- Updates: silent automatic install (see Updates below).
- KMSP-6 0.6 also: TRACK page = OUT MIDI PITCH LEGATO SCALE ROOT SPEED CHORUS; EUCLID / ROTATE moved to the ARP page (slots 7-8). `MonoVoice::adv` wraps phases above Nyquist (a CHORD voice could read outside the wave table).

## Added in KDSP-16 0.12 / KMSP-6 0.7 (both plug-ins)

- New saved globals appended after the chorus sends (rule: saved host parameters never move, new ones go just before
  `gOUT_STEP`): `gCHA_SEND + t` (CHAOS send per track, default 127), `gMF_ON` (default 1), `gMORPH`. Defaults of every
  saved global: `globalDefault(g)` in `Params.hpp` (used by the UI, the engine and the preset builder).
- `Scene.hpp` (shared): `Scenes` in the Bank (two sound snapshots, `uint8` 0..127 for every saved parameter + machines),
  bank lines `Z a <hex> <hex>` / `Zc a`, pattern MORPH lane `Pattern::morph[step]` (`z p s v`, 255 = fader).
  `Engine::process` wraps `processBlock`: `applyMorph()` blends trackParam / global between the scenes for the block and
  restores them after (the knobs never change). Morph = fader, or the lane value on lane steps, + LFOs sent to MORPH;
  switch-like params (`morphSwitch`) jump at 50 %; synth knobs of tracks whose machine differs are not morphed.
- LFO destinations: `kDestOfBin` (bins of 4 values on KDSP, 3 on KMSP) keeps every pre-0.12 saved DEST value and adds
  virtual destinations `kDestL1Rate..`, `kDestMorph` (KMSP also `kDestPitch`). LFO -> LFO and -> MORPH use the previous
  block's LFO output (`lfoRateNext`, `lfoDepthNext`, `morphLfoNext`). Rate mod = +-2 octaves, depth mod = x0..x2.
- CHAOS (MasterFx glitch) works on `sendGL/GR` (main-bus tracks x CHAOS send): the slice is taken from the sends and
  the repeats replace the sent tracks' dry sound (`l += (repeat - send) * mix`).
- UI: master tabs ECHO REVERB CHORUS CHAOS FILTER (LINK) MORPH MASTER; clicking the active tab opens `ovFx` (effect
  presets, `FxPresets.hpp`, shared); FILTER knob 4 = ON pad (`mfOnPad`); MORPH tab = `drawMorphTab` / `morphTabClick`
  (scene pads, fader `dkMFader`, lane `dkLane`); send knobs = `sendG(k)` (FX page); LFO rings = `lfoHits` /
  `drawLfoRing` (UI-side LFO simulation `lfoUiValue`, repaint at 30 fps while visible). Presets window: TO SCENE A/B
  (`presetToScene`), LOAD PATTERNS (`loadPresetPatterns`); sound loads bring the preset's scenes.
- Factory presets: `PresetDef::expReso` = voiced with the exponential RESONANCE (skips the old-curve conversion);
  builder helpers `scene()`, `lane()`, `speed()`, `tpi()`, `gpi()`. Tests: `tests/morph.cpp` (shared).
- KMSP-6: the FILTER page has 10 knobs (`slots()`, `knob(k)` sized from it; loops over the track knobs go to 10).

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
- Automatic install (0.11 / 0.6, setting `autoinstall`, default on, toggle AUTO INSTALL in the update window): after a
  check finds a newer version, `doInstall(automatic)` downloads + verifies it and runs `runInstallerSilently` (Windows:
  NSIS `/S` via ShellExecuteEx, one UAC prompt; macOS: `osascript ... /usr/sbin/installer -pkg ... with administrator
  privileges`, path passed as argv). Lock file `Updates/installing-<product>.lock` (20 min stale), `installed-<product>`
  remembers it (other windows then show "UPDATED TO x - REOPEN TO USE IT"). The checker's thread only uses a
  `shared_ptr<Shared>` block and is detached if the window closes mid-install. The NSIS installer renames a loaded
  plug-in file to `<file>.<ticks>.old` (MoveAside macro) instead of failing; old files are deleted by the next install /
  uninstall; CI `windows-install-test` updates over a DLL loaded with LoadLibrary.
- `KN_VERSION` comes from `VERSION` (Makefile). Test: `tests/update.cpp` (links the two monocypher .c files).
- Copies older than KDSP-16 0.9 / KMSP-6 0.4 have no update check.
- The plug-in version (`getVersion`, bundle Info.plist) comes from `VERSION` via `KN_VER_MAJOR/MINOR/MICRO`
  (Makefile) — it was hard-coded once and macOS Installer then skipped the "unchanged" bundles. `make-pkg.sh` also
  sets `BundleIsVersionChecked false`; the NSIS installer moves a locked (loaded) plug-in file aside.
  `upd::strayCopy` warns about old copies in the per-user plug-in folders.

## Packaging and signing

- Installers are built in CI: `packaging/macos/make-pkg.sh` (productbuild .pkg into /Library/Audio/Plug-Ins,
  EULA shown, bundles not relocatable) and `packaging/windows/installer.nsi` (NSIS, built on Linux with
  `makensis -NOCD`, installs VST3 + CLAP into Common Files, uninstaller registered). The `windows-install-test`
  job installs and uninstalls the real installer on a Windows runner.
- Signing switches on by itself when the repository secrets exist (listed at the top of the workflow):
  Developer ID + notarization on macOS, `scripts/sign-windows.sh` (osslsigncode, .pfx) on Windows.
- `EULA.txt` (draft with [PLACEHOLDERS] the owner must fill) and `THIRD_PARTY_NOTICES.txt` ship in every package.

## Added in KDSP-16 0.13 / KMSP-6 0.8 (both plug-ins)

- Echo (`MasterFx.hpp`): 8 models; MODEL knob = 8 bins of 16 values (`kEchoModelOfBin`) so the pre-0.13 values
  (0, 26, 52, 77, 103 = 5 models) keep their model. MULTI-TAP (taps at d·(1 - k·spacing), output only), REVERSE (two
  triangular grains reading the previous window backwards; delay limited to esz / 2.4; the reversed signal feeds the
  loop), DIFFUSE (3 allpasses per channel inside the loop). New saved globals after `gMORPH`: `gECHO_RTIME` (default
  64, right time x0.5..x1.5), `gECHO_DRIVE`, `gECHO_DUCK` (envelope of the echo send), `gECHO_FREEZE` (0/1: the loop
  rewrites itself with whole-sample reads, no input; keeps the echo awake). ECHO tab = 12 slots, 6 per row
  (`mSlots()`, `mKnob(k)` depends on `mpage`), slot 12 = FREEZE pad. Echo presets reset the extras they do not set.
  Test: `tests/echo.cpp` (shared).
- Logo: the Kinetik "Orbit" mark (`LogoMark.hpp`, shared: `drawOrbitMark`, colours per `LogoStyle`; RISO = black
  badge) + product name in Unbounded Bold (`fnLogo`, `kFontUnbounded` in `Fonts.hpp`, ASCII subset, OFL). The star
  (`drawSparkle`) is no longer used by the logo. J-Slice and KDYN-10 draw the same mark with their toolkits.

## Added in KMSP-6 0.9

- Wavetables: kit field `kfTable` (0..7 factory = `kWaveTableNames` in `Params.hpp`, 8 = USER; default 0 = CLASSIC, so old
  banks are unchanged). `Engine::waveSetFor(t)` (TABLE of the sound source; USER without a table = CLASSIC) sets
  `MonoVoice::waves` before each render; `waveRead` maps the 0..31 frame position onto the table's frame count.
- User tables (`WaveUser.hpp`): `wav::make` (mono PCM -> `UserWave`: frame length guessed 2048 / 1024 / 4096 / 512 / 256,
  else one cycle; at most 32 frames kept; spectrum via FFT, mip levels), state lines `w<t> name\tframeLen\tbase64` of
  the packed PCM (`Sample.hpp`, now shared with KDSP-16). Plugin states `waves` (all tracks, saved) and `wav` (one UI
  command): decoded and built in `setState`, handed to the engine in `run()` with the pending / live / grave scheme of
  KDSP-16's samples (`engine->userWaves[t]`). UI: `showTable()`, `tabPrev/Name/Next/Load`, `loadWaveFile` (dr_wav),
  `uiWaves[t]` (scope + names), user presets carry `WAV <line>` lines. Test: `tests/wavetable.cpp` (needs -pthread).

## Added in KMSP-6 0.10 — 8 tracks

- `kTracks = 8`, `kLegacyTracks = 6`. **Host parameter layout of 0.9 is kept**: tracks 0-5 = indices 0..335, then the
  globals (`kGlobalBase = 336`, the chorus / CHAOS send blocks stay 6 wide), then after `gECHO_FREEZE`: `gCHO_SEND2`,
  `gCHA_SEND2` (tracks 6-7) and `gEXT_TP` (all parameters of tracks 6-7). **Never compute `t * kTrackParams + p`**: use
  `tpIndex(t, p)` (host index), `tpOf(i, t, p)` (back), `choSendG(t)` / `chaSendG(t)` (send globals). Each track's 56
  parameters stay contiguous, so `&values[tpIndex(t, 0)]` covers a whole track. `globalDefault` returns
  `trackParamDefault` for the extra track block. `kDefaultKit` tracks 7-8 = VOX, CHORD.
- LFO TARGET: `lfoTrackIndex` keeps the 7 bins of 0.9 and splits the last (110..127) into tracks 6 / 7 / 8.
- Scenes: machine bytes and values may be shorter (older lines), see `sceneFromLine`. Engine `applyMorph` uses `tpOf`.
- UI: track buttons 142.5 px wide, LINK diagram sized from `kTracks`, MIDI export channels 1-8.
- Test: `tests/tracks8.cpp` (0.9 indices unchanged, round trip, LFO targets, track 8 plays, 6-track bank loads).

- Text (0.10.1, same as KDSP-16 0.14.1): `kFontMono` = IBM Plex Mono SemiBold, `textSize()` mono x1.13 x1.08,
  `number()` x1.15 when size <= 16.

## Removed in KDSP-16 0.15 / KMSP-6 0.11 — MORPH (owner's request)

- Scenes A/B, the MORPH fader / tab / step lane, TO SCENE (presets window) and the LFO destination MORPH are removed.
  Kept for compatibility only: `Scene.hpp` (old `Z` / `z` bank lines are still parsed, no longer written; `Bank::scenes`
  and `Pattern::morph` stay but nothing reads them), `gMORPH` (host parameter hidden, named "Unused", index kept),
  `kDestMorph` (in the bins so saved LFO DEST values keep their meaning; shown as "-", not offered in the menu, does
  nothing). `Engine::process` = `processBlock` (no `applyMorph`). `tests/morph.cpp` now checks that scenes are ignored /
  not written, CHAOS sends and the old scene-line parser. The 0.12 / 0.7 morph notes above are history.
- NEON: active step LEDs red `Color(236, 36, 50)`, accents `Color(255, 140, 120)` (drawSequencer, `th->logo == lgNeon`).
