// Kinetik KMSP-6 — direct UI <-> DSP access (same process, DISTRHO_PLUGIN_WANT_DIRECT_ACCESS)
#pragma once
#include <cstdint>

namespace kn {

struct MonoDirect {
    virtual ~MonoDirect() {}
    // notes played live while REC is armed; gate < 0 = note start, gate >= 0 = length index for that step
    virtual bool popRecorded(int& pattern, int& track, int& step, int& note, bool& accent, int& gate) = 0;
    virtual uint32_t heldMask() const = 0;     // tracks whose gate is open
    virtual int currentNote(int track) const = 0;
    virtual void setRecording(bool on) = 0;
    virtual void setClick(bool on) = 0;     // metronome while recording
    virtual void setSelectedTrack(int t) = 0;  // MIDI AUTO routing
    virtual void playNote(int track, int note, int velocity, bool on) = 0; // on-screen / computer keyboard
    virtual void panic() = 0;
};

} // namespace kn
