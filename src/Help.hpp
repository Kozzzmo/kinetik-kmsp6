// Kinetik KMSP-6 — built-in help texts (machines and parameters)
#pragma once
#include "Params.hpp"

namespace kn {

struct MachineHelp {
    const char* summary;
    const char* p[8];
};

static const MachineHelp kMachineHelp[kMachines] = {
    {"Empty track. Makes no sound; useful to free a track.",
     {"-", "-", "-", "-", "-", "-", "-", "-"}},
    // SAW
    {"Classic subtractive voice: two band-limited oscillators that morph saw > pulse > triangle, plus a sub-octave and noise.",
     {"Shape of oscillator 1: saw (0), pulse (64), triangle (127).",
      "Shape of oscillator 2, same range.",
      "Fine detune of oscillator 2 (64 = in tune, about +/- 50 cents).",
      "Balance between oscillator 1 (0) and oscillator 2 (127).",
      "Octave of oscillator 2: -2, -1, 0, +1, +2.",
      "Pulse width used by both oscillators when they are on a pulse shape.",
      "Level of the square sub-oscillator one octave below.",
      "Level of white noise."}},
    // PULSE
    {"Pulse-width voice with PWM, hard sync, ring modulation, sub-oscillator and bit crusher. Great for basses and chip leads.",
     {"Pulse width (5% to 95%).",
      "Speed of the built-in pulse-width LFO.",
      "Depth of the pulse-width modulation.",
      "Hard sync: a slave oscillator up to 8x higher is reset by the main one (sweeping it gives the classic sync scream).",
      "Ring modulation with a sine a fifth above.",
      "Level of the square sub-oscillator.",
      "Level of white noise.",
      "Bit and sample-rate crusher."}},
    // SUPER
    {"Up to seven detuned saws (or squares) stacked: big trance leads, pads and chords-in-one-note.",
     {"Detune spread of the side voices.",
      "Level of the side voices against the centre voice.",
      "Number of voices: 3 to 7.",
      "Waveform: saw (below 64) or square (above).",
      "Level of the sub-oscillator.",
      "Octave of the whole stack: -2 to +2.",
      "Level of white noise.",
      "Soft saturation of the stack."}},
    // FM
    {"Three-operator FM with four algorithms and its own modulation envelope: bells, electric pianos, metallic basses.",
     {"Frequency ratio of operator 2 (0.5 to 12).",
      "Modulation index of operator 2.",
      "Frequency ratio of operator 3.",
      "Modulation index of operator 3 (or level of the second carrier in algorithm 3).",
      "Feedback of the top operator (towards noise and saw).",
      "Algorithm: 1 = 3>2>1 stack, 2 = 2+3>1, 3 = 2>1 plus carrier 3, 4 = 3>(1 and 2).",
      "Decay time of the modulation envelope (the 'pluck' of the FM).",
      "Level the modulation settles to after the decay."}},
    // WAVE
    {"Wavetable oscillator: 32 waves (saw to square, vocal formants, organ drawbars, narrow pulses) with a scanning LFO, second oscillator, folder and crusher.",
     {"Position in the wavetable.",
      "Speed of the built-in scan LFO.",
      "Depth of the scan LFO.",
      "Octave of oscillator 2: -2 to +2.",
      "Detune of oscillator 2.",
      "Level of oscillator 2.",
      "Wave folder: adds bright, moving harmonics.",
      "Bit and sample-rate crusher."}},
    // VOX
    {"Formant voice: a glottal source through three resonant formant filters that morph between the vowels A E I O U.",
     {"Vowel: A, E, I, O, U (continuous morph).",
      "Formant shift: smaller to bigger 'throat'.",
      "Breath noise added to the source.",
      "Resonance of the formants.",
      "Vibrato depth.",
      "Vibrato speed.",
      "Brightness of the glottal source.",
      "Chorus: a second, slightly detuned source."}},
    // PLUCK
    {"Physical string model (Karplus-Strong): plucked strings, harps, kotos, mallets. Its own decay sits on top of the AMP envelope.",
     {"Decay time of the string.",
      "Brightness of the pluck.",
      "Pick position: short = thin and bright, long = round.",
      "Damping of high frequencies along the string.",
      "Stiffness: inharmonic, bell-like or metallic tones.",
      "Excitation: smooth triangle (0) or noise burst (127).",
      "Body resonance.",
      "Soft saturation."}},
    // NOISE
    {"Filtered noise that can follow the keyboard: wind, surf, hi-hats, pitched noise leads and grainy textures.",
     {"Colour: centre frequency of the noise filter.",
      "Resonance of the noise filter (high values whistle at the note).",
      "Key tracking of the noise filter: 0 = fixed colour, 127 = follows the played note.",
      "Bit and sample-rate crusher.",
      "Granular amplitude flicker.",
      "Mixes a sine at the note pitch.",
      "LP mix: band-pass (0) to low-pass (127).",
      "Soft saturation."}},
    // CHORD
    {"A whole chord from one note: 3 to 8 wavetable voices through one envelope and one filter (paraphonic). Pads, stabs, chord leads. For separate envelopes per voice, use LINK.",
     {"Wave: position in the wavetable (saw, square, vocal, organ...).",
      "Chord: MAJ, MIN, SUS2, SUS4, MAJ7, MIN7, DOM7, MIN7b5, 6TH, MIN6, ADD9, MAJ9, MIN9, DIM, AUG, QUARTAL.",
      "Voices: 3 to 8 (chord notes repeat an octave up to fill the voices).",
      "Invert: moves the lowest notes up an octave (inversions).",
      "Spread: opens the chord over more octaves.",
      "Detune: slight detuning between the voices.",
      "Strum: the voices start one after the other, like a strummed guitar.",
      "Bright: tone of the chord (low-pass)."}},
};

// help for track parameters 8..55
static const char* const kFxHelp[kTrackParams - 8] = {
    // AMP
    "Attack time of the amplitude envelope.",
    "Hold: time the envelope stays at full level before decaying.",
    "Decay time towards the sustain level.",
    "Sustain level while the note is held (gate length / MIDI key).",
    "Release time after the gate closes.",
    "Track level.",
    "Stereo position (L64 to R63).",
    "Glide: time to slide from one note to the next. Applies to legato notes and to SLIDE steps.",
    // FILTER
    "Cutoff frequency of the filter (20 Hz to 20 kHz).",
    "Resonance: emphasis at the cutoff, up to whistling.",
    "Filter type: LP 12 (soft low-pass), LP 24 (steep low-pass), BAND, HIGH (high-pass) or NOTCH.",
    "Low cut: a gentle high-pass that removes the lows before the filter (OFF at 0).",
    "Attack of the filter envelope.",
    "Decay of the filter envelope.",
    "Envelope amount: how far the envelope moves the cutoff (0 = none, + opens, - closes). The classic 'acid' sweep.",
    "Key tracking: how much the cutoff follows the played note (100% = one octave per octave).",
    // FX
    "Tremolo depth (towards ring modulation at high rates).",
    "Tremolo rate.",
    "EQ frequency (40 Hz to 16 kHz).",
    "EQ gain (+/- 18 dB at that frequency).",
    "Crush: lowers the sample rate for a gritty, digital sound.",
    "Drive: saturation.",
    "Send to the master delay (ECHO tab).",
    "Send to the master reverb (REVERB tab).",
    // LFO x3
    "Rate: speed in Hz (0.03 to 30), or a note length when SYNC is TEMPO.", "Depth: how much the LFO moves its destination (0 = off).",
    "Wave: triangle, sine, square, saw up, saw down, random.",
    "Mode: FREE runs on its own, RETRIG restarts at each note, HOLD keeps one value per note.",
    "Destination: click the knob (or the box in the display) for the list of everything it can move, or PITCH (up to +/- 12 semitones).",
    "Target track: THIS track or any of the six tracks.",
    "Rate: speed in Hz (0.03 to 30), or a note length when SYNC is TEMPO.", "Depth: how much the LFO moves its destination (0 = off).",
    "Wave: triangle, sine, square, saw up, saw down, random.",
    "Mode: FREE runs on its own, RETRIG restarts at each note, HOLD keeps one value per note.",
    "Destination: click the knob (or the box in the display) for the list of everything it can move, or PITCH (up to +/- 12 semitones).",
    "Target track: THIS track or any of the six tracks.",
    "Rate: speed in Hz (0.03 to 30), or a note length when SYNC is TEMPO.", "Depth: how much the LFO moves its destination (0 = off).",
    "Wave: triangle, sine, square, saw up, saw down, random.",
    "Mode: FREE runs on its own, RETRIG restarts at each note, HOLD keeps one value per note.",
    "Destination: click the knob (or the box in the display) for the list of everything it can move, or PITCH (up to +/- 12 semitones).",
    "Target track: THIS track or any of the six tracks.",
    // SYNC and PHASE of LFO 1, 2, 3
    "Sync: FREE = rate in Hz; TEMPO = rate as a note length, locked to the song.", "Phase: where in its cycle the LFO starts (0 to 360 degrees).",
    "Sync: FREE = rate in Hz; TEMPO = rate as a note length, locked to the song.", "Phase: where in its cycle the LFO starts (0 to 360 degrees).",
    "Sync: FREE = rate in Hz; TEMPO = rate as a note length, locked to the song.", "Phase: where in its cycle the LFO starts (0 to 360 degrees).",
};

static const char* const kKitHelp[kKitFields] = {
    "Audio output of the track: MAIN or the extra stereo outputs 2-4 (enable them in Ableton's I/O section).",
    "MIDI channel the track listens to. AUTO = the track selected on screen receives the keyboard.",
    "Pitch of the whole track (sequence and keyboard), in semitones (-24 to +24).",
    "Legato: when a new note starts while the previous one is still held, the envelopes do not restart.",
    "(old setting, now the FILTER parameter KEY TRK)",
    "Arpeggiator: OFF, UP, DOWN, UP-DOWN, RANDOM or ORDER (as played). Plays the chord below, or the keys you hold.",
    "Range of the arpeggio in octaves.",
    "Length of each arpeggio note, in steps.",
    "Chord built on each sequencer note when the arpeggiator runs (NOTE = the note alone, plus octaves).",
    "Scale: every note of the track (sequence, keyboard, arpeggio) snaps to the nearest note of this scale. OFF = chromatic.",
    "Root note of the scale.",
};

} // namespace kn
