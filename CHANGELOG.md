# Changelog

## 0.4.2

- **Updates really replace the plug-in**: the installers now always overwrite the installed files (on macOS an update
  could leave the old version in place), and the plug-in reports its real version to hosts
- The Windows installer asks you to close your music software when the plug-in is in use
- The update window warns when an older copy sits in your personal plug-in folder (hosts may load it instead)

## 0.4.1

- **RANDOMIZE ALL SOUNDS**: new sounds on every track at once, machines kept, sequencer untouched (UNDO restores)
- The preset name now sits inside the PRESETS button and shrinks to fit (it overlapped UNDO)

## 0.4

- **Update check**: the plug-in tells you when a new version is out (at most once a day, nothing is sent). Click the
  logo: DOWNLOAD & INSTALL fetches the installer, checks its signature and opens it (close your DAW to finish).
  Automatic check can be turned off in the same window.

## 0.3

- **LINK** (MASTER section, LINK tab): 2 to 6 tracks become the voices of one polyphonic instrument, played from
  the first one (sequencer, keyboard, MIDI). Modes POLY, CYCLE (voices take turns), UNISON (all voices, detuned),
  RANDOM; each voice keeps its sound or copies the first track's. Steps can carry a chord (new CHORD step mode).
- **CHORD machine**: 3 to 8 wavetable voices playing a chord (16 shapes), with inversion, spread, detune, strum.
- **Global scale** (bottom left): every track snaps to the same scale and root.
- **128 steps** per track (8 pages).
- **Multi-step editing**: each right-click adds a step to the selection; knobs and step modes edit all of them.
- Randomizing a sound keeps the track's level, pan and sends.

## 0.2.1

- Beta test: demo mode switched off (no silences, no licence needed). It comes back for the commercial release
  (`kDemoModeEnabled` in `src/License.hpp`).

## 0.2

- **Readable parameter names** everywhere (ATTACK, DECAY, CUTOFF, RESO, TREMOLO, EQ FREQ, DRIVE...), machine
  parameters included.
- **New filter section**: CUTOFF, RESO, TYPE (LP 12, LP 24, BAND, HIGH, NOTCH), LOW CUT, ENV ATK, ENV DEC,
  ENV AMT, KEY TRK. Presets and user presets made with 0.1 are converted.
- **Graphic display**: the upper display of each page draws what its knobs do: the machine's waveform, the
  amplitude envelope, the filter curve with its envelope sweep, the effects, the LFO wave and its destination,
  the scale on a keyboard, the arpeggio.
- **LFOs**: destination list (click DEST, or the box in the display), SYNC (rate as a note length locked to the
  song) and PHASE; pitch modulation up to +/- 12 semitones.
- **Scales**: each track can be locked to a scale (16 scales) with a root note; out-of-scale keys are dimmed.
  PITCH (semitones) on the TRACK page.
- **Delay and reverb models**: DIGITAL, TAPE, BBD, PING-PONG, LO-FI echoes; ROOM, PLATE, HALL, SPRING, SHIMMER
  reverbs, each with a MOD knob. Levels and sends go much higher than before.

Note: projects saved by a host with 0.1 keep their old filter knob positions, which mean something else now.
