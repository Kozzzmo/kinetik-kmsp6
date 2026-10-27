# Changelog

## 0.14.0

- CHAIN: play a list of patterns in a loop, each a number of times (CHAIN button in the pattern box; the view follows the
  playing pattern).

## 0.13.2

- The second row of pattern buttons (A09-A16) no longer sticks out of the PATTERNS box.

## 0.13.1

- Sequencer: a thin mark under the step numbers 1, 5, 9 and 13 (the beats).

## 0.13.0

- **ALL**: hold **Alt** (Option on Mac) while turning a knob, or switch on the new **ALL** button (right of the page tabs),
  and the same knob moves on every track at once — each track keeps its own offset. Synth knobs only move tracks with
  the same machine. Alt + Shift = fine. One UNDO step. (Right-click groups still work as before.)
- **Metronome**: a click on every beat while REC records, toggled by the new **CLICK** button under REC (remembered).
- NEON skin: the track activity LEDs are green.

## 0.12.0

- **Knobs moved by an LFO**: the knob's arc (or ring of dots) now follows the value the LFO gives, in the LFO's colour, while
  the pointer stays on the value you set. The L1 / L2 / L3 badge takes the same colour, and so does the wave on the LFO page.
- **LFO colours**: COLOR on each LFO page opens eight colours (green, yellow, white by default; also cyan, orange, violet, red, blue);
  the choice is kept for every project and new instance.
- **Presets reworked**: 32 factory presets (8 new: Full Band, Vowel Sequence, Sync Lead, Fold Bass, Organ Groove,
  Digital Arp, FM Keys, Reverse Pads — using the wavetables and the 8 tracks), sorted by style with category buttons in
  the browser; the search also finds styles. Every factory preset now plays at about the same loudness.

## 0.11.0

- **MORPH removed**: the MORPH tab (scenes A / B, fader, MORPH steps), the TO SCENE buttons of the presets window and the
  MORPH destination of the LFOs are gone. Projects that used scenes now play the knobs as they are; an LFO that was set
  to MORPH does nothing (choose another destination). The "Scene Morph" automation parameter is hidden.
- NEON skin: active steps of the sequencer in red (accents in a lighter red).
- Factory presets: "Morph Acid" is now "Acid Drive", "Morph Pad" is now "Choir Pad" (an LFO opens the pad).

## 0.10.1

- Larger, bolder text: labels and buttons in IBM Plex Mono SemiBold, 8 % larger; knob values and step numbers 15 % larger.

## 0.10.0

- **8 tracks** (was 6): tracks 7 and 8 start as VOX and CHORD and have everything the others have (sends, LFOs,
  LINK, MIDI channel, output). Projects and presets made with 6 tracks open unchanged: their tracks 1-6 keep every
  setting and automation, LFO targets keep their track. The product keeps the name KMSP-6.
- Fix: scenes saved by 0.7 are loaded again (the parameters added since take their default).

## 0.9.0

- **Wavetables**: the WAVE and CHORD machines get a TABLE choice (SYNTH page, next to the page name): CLASSIC (the
  table so far), BASIC (sine to square), VOCAL (vowels A E I O U), ORGAN (drawbar registrations), SYNC (hard-synced
  saw), FOLD (wavefolder), DIGITAL (switched harmonics) and FM (two operators). POSITION scans through the table.
- **Your own wavetables**: LOAD imports a WAV as the track's USER table (frames of 2048 samples, up to 32 kept; 1024,
  4096, 512, 256 also recognised; a shorter file = a single cycle). It is saved with the project and with user presets.
- Existing projects and presets sound the same (TABLE = CLASSIC).

## 0.8.0

- **Echo rework**: three new models, **MULTI-TAP** (four taps before each repeat, scattered left / right; MOD = spacing),
  **REVERSE** (every repeat plays backwards; MOD blends forward repeats in) and **DIFFUSE** (the repeats smear into a
  wash; MOD = diffusion). New knobs on the ECHO tab: **R TIME** (right side shorter or longer than the left),
  **DRIVE** (saturation inside the loop), **DUCK** (the echo backs off while the sound sent to it plays) and a
  **FREEZE** pad (the echo keeps repeating what it holds). Projects saved before keep their echo model.
- 4 new echo presets (Scatter Taps, Backwards, Smear, Ducked Dub).
- **New Kinetik logo**: the "Orbit" mark, in every skin, with a larger product name.

## 0.7.0

- **Scenes A/B and MORPH** (new MORPH tab): take the current sound as scene A or B (click; right-click empties it),
  or send a preset there (PRESETS > TO SCENE A / B). The MORPH fader blends the sound from A to B in real time: it is
  automatable in the music software, each step of the pattern can set its own morph position (MORPH STEPS), and an LFO
  can move it (new LFO destination MORPH). An empty scene stands for the knobs as they are. Scenes are saved with the
  project and with user presets.
- **LFO rings**: a knob moved by an LFO gets a coloured ring that moves with the LFO, and a badge (L1, L2... or T3 L1
  when the LFO is on another track).
- **LFOs modulate LFOs**: new destinations LFO1/LFO2/LFO3 RATE and DEPTH (any LFO can speed up, slow down or fade
  another one), plus MORPH. Existing LFO settings keep their destination.
- **Master section reordered**: ECHO, REVERB, CHORUS, CHAOS, FILTER, LINK, MORPH, MASTER.
- **GLITCH is now CHAOS**, with a **CHAOS send per track** (FX page, next to the new CHORUS send knob): only the tracks
  sent to it are repeated; the others and the effect returns keep playing.
- **FX page / FILTER page**: the EQ moved to the FILTER page (10 knobs); the CHORUS and CHAOS sends sit on the FX page,
  next to DELAY and REVERB.
- **Master filter ON / OFF** button.
- **Effect presets**: click the active tab of ECHO, REVERB, CHORUS, CHAOS, FILTER or MASTER for 6 to 8 presets of that
  effect (Slapback, Dub Tape, Gated Snare, Shimmer Pad, Jet Flanger, Stutter, Roll Up, Telephone, Glue...).
- **Presets window**: LOAD ALL, LOAD SOUND or LOAD PATTERNS; TO SCENE A / B. (The old MORPH slider of this window
  became the MORPH tab.)
- **6 new factory presets** showing the scenes, CHAOS, chorus and SPEED: Morph Acid, Ensemble Strings, Chaos Arp, Speed Phase, Morph Pad, Glitch Bass.

## 0.6.0

- **Chorus** (new CHORUS tab in the master section): CHORUS, ENSEMBLE (three voices per side, lush) and FLANGER
  models with RATE, DEPTH, TIME, FEEDBACK, WIDTH, TONE and LEVEL. Each track sends to it with its new CHORUS knob
  (TRACK page, last knob); the lows are kept out of it so a bass or a kick is never hollowed out.
- **Glitch / beat repeat** (new GLITCH tab): grabs a slice of the mix and repeats it in time. CHANCE (per bar, on the
  beats), SIZE (1/4 to 1/64, triplets), LENGTH (1/8 bar to 2 bars), PITCH (repeats glide up or down to +-12
  semitones), FADE (quieter and darker at each repeat), REVERSE (chance of backwards repeats), MIX. The HOLD pad
  repeats while it is pressed (right-click = latch).
- **SPEED per track** (TRACK page; EUCLID and ROTATE moved to the ARP page): 1x, 2x, 4x, 3/2x, 3/4x, 1/2x, 1/4x, 1/8x against the pattern, for polyrhythms,
  half-time hats or double-time rolls. The step display, REC and the MIDI export follow it.
- **Softer RESONANCE curve** on every filter (track filters and master filter): the knob is now exponential, the first
  half stays musical (about 6 dB of peak at 50 % instead of 16-19 dB) and the full resonance is still at the top.
  The factory presets were converted so they sound the same; user presets and projects keep their knob values, so
  their resonance is now gentler.
- **Automatic updates**: when a new version is found, it is downloaded, checked and installed by itself, without the
  installer window (one system prompt: Yes on Windows, administrator password on macOS). Nothing stops while it
  installs; the new version is used the next time the plug-in loads (reopen the project or restart the music
  software). On Windows the update now works even while the music software has the plug-in open. AUTO INSTALL can be
  turned off in the update window (logo click).
- Fixed a crash risk when a CHORD voice was pushed above the audible range (found by the automatic tests).

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
