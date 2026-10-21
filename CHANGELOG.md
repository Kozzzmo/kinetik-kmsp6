# Changelog

## 0.5.0

- **MIDI expression**: pitch bend (±2 semitones), mod wheel (vibrato) and channel aftertouch (brighter filter) now
  play the tracks that receive the notes; MIDI CC 121 and panic reset them.
- **Undo on 40 levels**: UNDO now steps back through knob moves, step edits, randomizes, presets and pastes, one
  action at a time (Ctrl/Cmd+Z). Right-click on UNDO, Ctrl+Shift+Z or Ctrl+Y = redo. The button shows what the next
  undo will revert.
- **Tooltips**: hover a knob or a button for a moment to see what it does.
- **Preset browser**: type to search (name and description), a FAVOURITES tab (click the star of any preset), and a
  **MORPH** slider that moves the current sound towards the selected preset (switches and machines change at 50 %;
  tempo and patterns are not touched; one undo step per move).
- **MIDI export**: MIDI in the PATTERNS box writes the edited pattern as a .mid file into
  Documents / Kinetik / KMSP-6 MIDI and opens the folder: drag it onto a MIDI track of the host (one MIDI
  track per track on channels 1-6, chords of LINK groups included).
- **Track tools**: two arrow buttons move the steps of the track one step earlier / later (locks follow); COPY also
  copies the track's sound and right-click on PASTE pastes the sound only; EUCLID and ROTATE on the TRACK page spread
  N hits evenly over the track.
- **Anti-aliased DRIVE** (track and master): much less aliasing (harsh, unrelated high tones) when driven hard,
  same level and tone.
- **Lower CPU**: the echo and reverb sleep while nothing is sent to them and their tail has died away; the reverb's
  modulation is computed less often. With the demo preset: playing 2.3 % -> 1.6 % of one core, stopped 1.2 % -> 0.3 %.
- Windows: user presets, favourites and licences work in user folders whose name uses other alphabets.
- Updates: the downloaded installer is kept in a private folder of the user (Kinetik / Updates) until it is opened.
- Tests: the sound of every factory preset is now checked automatically at each build.

## 0.4.8

- Windows: the update check works when the user folder has accents (e.g. C:\Users\Hélène); before, it always said
  "Could not reach the update server" there. A stuck download is now stopped after its time limit.
- User presets: values from a preset file are kept inside each parameter's range (a damaged or hand-edited file
  could send invalid values to the sound engine), and loading a preset clears any solo left from before.

## 0.4.7

- **Echo and reverb models rebuilt so each one really sounds different**, and MOD now does something specific to
  the model (its name changes with the model):
  echo DIGITAL (clean, exact repeats; MOD = SPREAD), TAPE (wow, flutter, drift, saturation, hiss, darker at every
  pass; MOD = WOW), BBD (dark and gritty, stereo chorus; MOD = CHORUS), PING-PONG (MOD = OFFSET, swung bounces),
  LO-FI (telephone band, low sample rate, few bits; MOD = CRUSH);
  reverb ROOM (short, strong early reflections; MOD = EARLY), PLATE (instant, dense, bright; MOD = BRIGHT),
  HALL (long, wide, early field; MOD = MOTION), SPRING (a real "boing" tank loop; MOD = DRIP),
  SHIMMER (long, rising an octave; MOD = SHIMMER). Each reverb model has its own decay range.
- New instances start with a 1/8D echo and feedback 45 (1/8T with feedback 50 blurred into a wash that sounded like reverb)

## 0.4.6

- The note a track is playing is shown in the top row of its button (it covered the S / M buttons)

## 0.4.5

- **Master filter** (new FILTER tab next to MASTER): HIGH PASS and LOW PASS with RESONANCE, after the echo, reverb
  and master chain, so it sweeps the whole mix. OFF at both ends; a dot on the tab shows it is active.

## 0.4.4

- **Solo** button (S) next to mute on every track: when a track is soloed, only the soloed tracks play
- **Knob values always match the sound**: the plug-in also receives every change made in its window directly, so
  hosts that sent an old value back (or lost part of a randomize / undo / preset) can no longer leave the sound out of
  step with the display — randomize works on the first press
- NEON skin: the pink outlines are red

## 0.4.3

- **Edit several tracks at once**: right-click track buttons to add them to the selected track (pink frame,
  "+2,3" in the display). Knobs move every track of the group by the same amount (each keeps its own offset);
  machine, TRACK and ARP settings are set on all of them. Left-click a track to end the group. Right-click on the
  selected track still auditions it.

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
