# Kinetik KMSP-6 — project memory for Claude

Kinetik KMSP-6 ("Kinetik Mono Synth Program") is a 6-track monophonic synthesizer plugin (VST3 / AU / CLAP,
built with DPF and NanoVG) with a note sequencer, inspired by the workflow of classic synth grooveboxes. All
synthesis is original code. Sister product: **Kinetik KDSP-16** (drums), repository `Kozzzmo/kinetik-kdsp16`.

## Source map (`src/`)

| File | Role |
|---|---|
| `Params.hpp` | 6 tracks, 64 steps, 16 patterns; 50 lockable params per track: 0–7 machine, AMP 8–15, FILTER 16–23, EFX 24–31, LFO1/2/3 at 32/38/44 (SPD DEP SHP TYP DEST TRK; DEST index 32 = PITCH); machines SAW PULSE SUPER FM WAVE VOX PLUCK NOISE; kit fields OUT MIDI TRNSP LEGATO KEYTRK + ARP mode/oct/speed/chord; gate lengths `kGateSteps` |
| `Bank.hpp` | like KDSP-16 plus per-step `note` (60 = C3, Ableton naming) and `gate` (index into `kGateSteps`); header `KMSP1`; extra commands `n` (note) and `g` (gate) |
| `Wavetable.hpp` | 32 band-limited wave frames × 11 mip levels, generated at start-up by FFT |
| `Voice.hpp` | `MonoVoice`: the 8 machines (VOX and NOISE have a slow AGC so resonance does not change loudness) |
| `Engine.hpp` | sequencer with notes, slide (glide without retrigger), legato, AHDSR, HP+LP filter with AD envelope, 3 LFOs, arpeggiator (sequencer chord or held notes), live note recording queue, master FX, 4 output buses |
| `MonoShared.hpp` | `MonoDirect` interface: the UI talks to the DSP directly (DISTRHO_PLUGIN_WANT_DIRECT_ACCESS) for live notes, recording queue, held-note display |
| `MonoPlugin.cpp` | DPF glue; MIDI routing: a track whose MIDI channel matches gets the notes, otherwise the selected track (AUTO) |
| `MonoUI.cpp` | the UI (1180×800 canvas): 9 pages (SYNTH AMP FILTER EFX LFO1-3 TRACK ARP), 8 step modes (TRIG ACCENT SLIDE NOTE LEN RATCH COND TIME), on-screen 3-octave keyboard, PC keys, step entry |
| `Presets.hpp` | 18 factory presets; `PB::seq()` takes note strings ("C2 . ~D#2 - !G1", x/X = hit on C5) |

`MonoUI.cpp` is the edited file (it was first assembled from parts; edit it directly now).
User presets: `.../Kinetik/KMSP-6/Presets/*.kmsp` (header `KMSP6-PRESET 1`).

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
- Output parameters: `out_step` (absolute step counter modulo 2^20), `out_leds`, `out_pattern`.
- `paramSymbol()` tables have a `static_assert` on their size — a too-short table crashed Windows hosts once.
- Engine: per-block event list (sorted by frame then kind), swing, micro-timing, ratchets, conditions
  (probability, FILL, PRE, 1ST, A:B), pattern change at the master length, 4 stereo buses (Main + Out 2–4),
  master echo / FDN reverb / EQ / compressor / drive / safety limiter, denormals flushed (SSE and ARM64).
- `Skins.hpp`, `Fonts.hpp` (embedded OFL fonts) and `Dsp.hpp` are shared with the sister plugin
  (kinetik-kdsp16 ⇄ kinetik-kmsp6): keep them identical in both repositories.
