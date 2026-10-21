// Kinetik KMSP-6 — engine: 6 monophonic synth tracks, envelopes, filters, 3 LFOs, arpeggiator,
// sequencer with notes, master FX, 4 stereo outputs
#pragma once
#include "Bank.hpp"
#include "MasterFx.hpp"
#include "Voice.hpp"
#include <vector>
#include <algorithm>
#include <atomic>
#include <climits>
#if defined(__SSE__) || defined(_M_X64) || defined(__x86_64__)
#include <xmmintrin.h>
#endif
// flush denormals to zero (avoids CPU spikes in decaying filters and reverbs)
inline void knDenormalsOff()
{
#if defined(__SSE__) || defined(_M_X64) || defined(__x86_64__)
    _mm_setcsr(_mm_getcsr() | 0x8040);
#elif defined(__aarch64__)
    uint64_t fpcr;
    __asm__ __volatile__("mrs %0, fpcr" : "=r"(fpcr));
    __asm__ __volatile__("msr fpcr, %0" : : "r"(fpcr | (1ull << 24)));
#endif
}

namespace kn {

// note on / off coming from MIDI or from the on-screen keyboard
struct NoteEvent { uint32_t frame; int track; float velocity; int note; bool on; };

// a note played live while REC is armed, to be written into the pattern by the UI
struct RecEvent { int pattern, track, step, note; bool accent; int gate; };

struct Transport {
    bool hostValid = false;
    bool hostPlaying = false;
    double beat = 0.0;
    double bpm = 120.0;
};

static constexpr int kOutBuses = 4;

inline float noteHz(float n) { return 440.f * std::exp2((n - 69.f) / 12.f); }

// ---------------------------------------------------------------------------
struct Track {
    MonoVoice voice;
    DriveAA distAA;   // anti-aliased track DRIVE
    float sr = 48000.f;
    // amplitude envelope (AHDSR)
    enum Stage { stIdle, stAttack, stHold, stDecay, stSustain, stRelease };
    Stage stage = stIdle; float env = 0.f; uint32_t holdLeft = 0;
    // filter envelope (AD, free-running after the trig)
    float fenv = 0.f; bool fAttack = false;
    // pitch
    float curNote = 60.f, targetNote = 60.f, glideTime = 0.f; float vel = 1.f;
    int64_t gateLeft = 0;          // samples until the sequencer note is released (-1 = held by MIDI/keyboard)
    int heldNotes[16] = {}; int heldCount = 0;  // live notes (last-note priority)
    // arpeggiator
    int arpNotes[32] = {}; int arpCount = 0, arpIdx = 0, arpDir = 1; int64_t arpLeft = 0, arpPeriod = 0; bool arpOn = false;
    // fx
    float amPh = 0;
    Biquad eq; float eqDb = 0;
    Svf hp, lp, lp2; bool hpOn = false, lpOn = false; int ftype = 0; float bpGain = 1.f;
    int srrN = 1, srrCnt = 0; float srrHold = 0;
    float vol = 0, panL = 0.707f, panR = 0.707f, del = 0, rev = 0;
    float cVol = 0, cPanL = 0.707f, cPanR = 0.707f, cDel = 0, cRev = 0;
    float amd = 0, amf = 1, dist = 0;
    // lfos
    float lfoPh[3] = {}, lfoHeld[3] = {}, lfoRnd[3] = {};
    float pitchMod = 0.f;
    // LINK voice state
    float uniDetune = 0.f;       // UNISON spread, in semitones
    int linkNote = -1;           // live note this voice is holding (LINK)
    uint32_t startSerial = 0;    // when the voice last started (LINK voice stealing)
    // locks
    uint8_t lockVal[kLockable] = {}; bool locked[kLockable] = {};
    float slideFrom[kLockable] = {}; bool sliding[kLockable] = {}; uint32_t slidePos = 0, slideLen = 1;
    float eff[kLockable] = {};
    float mod[kLockable] = {};
    uint32_t ledTimer = 0, idle = 1 << 30;
    int64_t decK = INT64_MIN; bool decOk = true, pre = true;
    int64_t recSkipK = INT64_MIN;     // a just-recorded live note must not be replayed immediately
    int recNote = -1, recStep = 0, recPattern = 0; double recBeat = 0;  // note being recorded (for its length)
    std::vector<float> buf;

    void init(float s)
    {
        sr = s; voice.init(s); buf.assign(64, 0.f);
        std::fill(locked, locked + kLockable, false);
        std::fill(sliding, sliding + kLockable, false);
    }
};

class Engine {
public:
    float trackParam[kTracks][kTrackParams];
    float global[kGlobalParams];
    // MIDI expression per track (set from the MIDI input): pitch bend in semitones (+-2), mod wheel 0..1 (vibrato),
    // channel pressure 0..1 (opens the filter)
    float midiBend[kTracks] = {}, midiMod[kTracks] = {}, midiPress[kTracks] = {};
    float vibPh[kTracks] = {};
    void resetExpression() { for (int t = 0; t < kTracks; ++t) { midiBend[t] = midiMod[t] = midiPress[t] = 0.f; } }
    Bank bank;
    bool fill = false;
    bool recording = false;          // REC armed in the UI
    int outStep = -1, outPattern = 0;
    uint32_t outLeds = 0;
    std::atomic<uint32_t> outGate{0};             // tracks with an open gate
    std::atomic<int> outNote[kTracks];            // last note played per track
    uint32_t trigCount[kTracks] = {};

    // live-recorded notes (audio thread -> UI thread)
    static constexpr int kRecQ = 256;
    RecEvent recQ[kRecQ];
    std::atomic<int> recW{0}, recR{0};
    bool popRec(RecEvent& e)
    {
        const int r = recR.load(std::memory_order_relaxed);
        if (r == recW.load(std::memory_order_acquire)) return false;
        e = recQ[r]; recR.store((r + 1) % kRecQ, std::memory_order_release);
        return true;
    }

    Engine()
    {
        for (int t = 0; t < kTracks; ++t)
            for (int p = 0; p < kTrackParams; ++p) trackParam[t][p] = trackParamDefault(t, p);
        for (int g = 0; g < kGlobalParams; ++g) global[g] = 0;
        for (int i = 0; i < kMasterCount; ++i) global[gECHO_TIME + i] = kMasterDefaults[i];
        global[gTEMPO] = 120;
        bank.initDefault();
        for (int t = 0; t < kTracks; ++t) outNote[t].store(-1);
    }

    void allNotesOff()
    {
        for (auto& tr : tracks) {
            tr.heldCount = 0; tr.arpOn = false; tr.gateLeft = 0;
            if (tr.stage != Track::stIdle) tr.stage = Track::stRelease;
        }
    }

    void init(double sampleRate)
    {
        sr = (float)sampleRate;
        for (auto& tr : tracks) tr.init(sr);
        for (int t = 0; t < kTracks; ++t) { tracks[t].voice.machine = -1; tracks[t].voice.setMachine(bank.machine[t]); }
        fx.init(sr);
        sendDL.assign(64, 0); sendDR.assign(64, 0); sendRL.assign(64, 0); sendRR.assign(64, 0);
        for (int b = 0; b < kOutBuses; ++b) { busL[b].assign(64, 0); busR[b].assign(64, 0); }
        events.reserve(4096);
        reset();
    }

    void reset()
    {
        for (auto& tr : tracks) { tr.stage = Track::stIdle; tr.env = 0; tr.heldCount = 0; tr.arpOn = false; tr.gateLeft = 0; }
        fx.reset();
        wasPlaying = false; internalBeat = 0; prevPlayParam = false; usingInternal = false;
    }

    void process(float* outL, float* outR, uint32_t frames, const Transport& tp, const NoteEvent* notes, uint32_t nNotes)
    {
        if (dummy.size() < frames) dummy.assign(frames, 0.f);
        float* outs[2 * kOutBuses] = {outL, outR};
        for (int i = 2; i < 2 * kOutBuses; ++i) outs[i] = dummy.data();
        process(outs, frames, tp, notes, nNotes);
    }

    void process(float* const* outs, uint32_t frames, const Transport& tp, const NoteEvent* notes, uint32_t nNotes)
    {
        knDenormalsOff();
        if (frames == 0) return;
        for (int t = 0; t < kTracks; ++t) tracks[t].voice.setMachine(machineOf(t));
        // LINK: detune of each voice for UNISON
        {
            const int V = linkVoices();
            for (int i = 0; i < kTracks; ++i) tracks[i].uniDetune = 0.f;
            if (V >= 2 && bank.glob.linkMode == lmUnison)
                for (int i = 0; i < V; ++i)
                    tracks[bank.glob.linkFirst + i].uniDetune = ((float)i / (V - 1) - 0.5f) * bank.glob.linkDetune / 127.f;
        }

        const bool playParam = global[gPLAY] > 0.5f;
        double bpm = tp.hostValid && tp.bpm > 1.0 ? tp.bpm : (double)global[gTEMPO];
        bpm = std::max(20.0, std::min(400.0, bpm));
        curBpm = bpm;
        bool playing = false, restarted = false;
        double b0 = 0;
        if (tp.hostValid && tp.hostPlaying) {
            playing = true; b0 = tp.beat;
            if (!wasPlaying || usingInternal || std::fabs(b0 - prevB1) > 0.01) restarted = true;
            usingInternal = false;
        } else if (playParam) {
            if (!prevPlayParam || !wasPlaying || !usingInternal) { internalBeat = 0; restarted = true; }
            playing = true; b0 = internalBeat; usingInternal = true;
        }
        prevPlayParam = playParam;
        const double beatsPerFrame = bpm / (60.0 * sr);
        const double b1 = b0 + beatsPerFrame * frames;
        blockB0 = b0; blockBpf = beatsPerFrame; blockPlaying = playing;
        if (playing) lfoClock = b0;     // synced LFOs follow the song position; stopped, they keep the tempo
        if (!playing) {
            if (wasPlaying) for (auto& tr : tracks) if (tr.gateLeft > 0) tr.gateLeft = 1; // release sequencer notes on stop
            wasPlaying = false; usingInternal = false; outStep = -1;
            curPattern = clampi((int)global[gPATTERN], 0, kPatterns - 1);
        }

        events.clear();
        for (uint32_t i = 0; i < nNotes; ++i)
            events.push_back({std::min(notes[i].frame, frames - 1), notes[i].on ? 3 : 0, notes[i].track, notes[i].velocity, 0, 0, notes[i].note});

        if (playing) {
            const double sb = stepBeats();
            const double winStart = restarted ? b0 - 1e-9 : std::max(b0, prevB1);
            if (restarted) {
                lastBoundaryK = (int64_t)std::ceil(b0 / sb - 1e-9) - 1;
                playStartK = lastBoundaryK + 1;
                for (auto& tr : tracks) { tr.decK = INT64_MIN; tr.pre = true; }
            }
            const Pattern& pat0 = bank.patterns[curPattern];
            const double swingOff = (pat0.swing / 100.0 * 2.0 - 1.0) * sb;
            const double maxMicro = (double)kMicroMax / 24.0 * sb;
            const int64_t kFirst = (int64_t)std::floor((b0 - swingOff - maxMicro) / sb) - 2;
            const int64_t kLast = (int64_t)std::floor((b1 + maxMicro) / sb) + 1;
            auto toFrame = [&](double tk) { return (uint32_t)std::min<double>(frames - 1, std::max(0.0, (tk - b0) / beatsPerFrame)); };
            const int want = clampi((int)global[gPATTERN], 0, kPatterns - 1);
            const int master = pat0.length ? pat0.length : 16;
            const int64_t nextSwitch = ((lastBoundaryK + 1 + master - 1) / master) * master;
            auto patFor = [&](int64_t k) -> const Pattern& {
                return (want != curPattern && k >= nextSwitch) ? bank.patterns[want] : bank.patterns[curPattern];
            };
            for (int64_t k = std::max<int64_t>(kFirst, lastBoundaryK + 1); k <= kLast; ++k) {
                const double tb = k * sb;
                if (tb < winStart || tb >= b1 || tb < -1e-9) continue;
                events.push_back({toFrame(tb), 1, -1, 1.f, k, 0, 0});
                lastBoundaryK = k;
            }
            for (int t = 0; t < kTracks; ++t) {
                for (int64_t k = kFirst; k <= kLast; ++k) {
                    if (k < 0) continue;
                    const Pattern& P = patFor(k);
                    const int s = P.trackStep(t, k);
                    if (!(P.flags[t][s] & sfTrig)) continue;
                    const int n = std::max<int>(1, P.ratch[t][s]);
                    const double base = k * sb + ((k & 1) ? (P.swing / 100.0 * 2.0 - 1.0) * sb : 0.0) + P.micro[t][s] / 24.0 * sb;
                    for (int j = 0; j < n; ++j) {
                        const double tk = base + j * sb / n;
                        if (tk < winStart || tk >= b1 || tk < -1e-9) continue;
                        events.push_back({toFrame(tk), 2, t, 1.f, k, j, 0});
                    }
                }
            }
            wasPlaying = true;
            prevB1 = b1;
            if (usingInternal) internalBeat = b1;
        }
        // order: note-offs, boundaries, sequencer trigs, note-ons
        std::stable_sort(events.begin(), events.end(), [](const Ev& a, const Ev& b) {
            return a.frame != b.frame ? a.frame < b.frame : a.kind < b.kind; });

        uint32_t pos = 0; size_t ei = 0;
        while (pos < frames) {
            while (ei < events.size() && events[ei].frame <= pos) { fire(events[ei], events[ei].frame); ++ei; }
            uint32_t end = std::min<uint32_t>(frames, pos + kCtl);
            if (ei < events.size() && events[ei].frame < end) end = events[ei].frame;
            if (end <= pos) end = pos + 1;
            float* seg[2 * kOutBuses];
            for (int i = 0; i < 2 * kOutBuses; ++i) seg[i] = outs[i] + pos;
            renderSegment(seg, end - pos);
            pos = end;
        }

        uint32_t leds = 0;
        for (int t = 0; t < kTracks; ++t) {
            if (tracks[t].ledTimer > 0) {
                leds |= 1u << t;
                tracks[t].ledTimer = tracks[t].ledTimer > frames ? tracks[t].ledTimer - frames : 0;
            }
        }
        outLeds = leds;
        outPattern = curPattern;
        uint32_t gm = 0;
        for (int t = 0; t < kTracks; ++t)
            if (tracks[t].stage != Track::stIdle && tracks[t].stage != Track::stRelease) gm |= 1u << t;
        outGate.store(gm);
        for (int t = 0; t < kTracks; ++t) outNote[t].store((int)std::lround(tracks[t].targetNote));
    }

    double stepBeats() const { return 0.25 / kScaleMult[bank.patterns[curPattern].scale % 7]; }

    // ---- LINK group and global scale
    int linkVoices() const
    {
        const int v = bank.glob.linkVoices;
        return v >= 2 ? std::min(v, kTracks - (int)bank.glob.linkFirst) : 0;
    }
    int linkMaster(int t) const   // master track of the group t belongs to, or -1
    {
        const int V = linkVoices();
        return (V >= 2 && t >= bank.glob.linkFirst && t < bank.glob.linkFirst + V) ? bank.glob.linkFirst : -1;
    }
    // muted, or not soloed while others are (LINK voices follow their first track's solo)
    bool silenced(int t) const
    {
        if (bank.mute[t]) return true;
        bool any = false;
        for (int i = 0; i < kTracks; ++i) any |= bank.solo[i] != 0;
        const int m = linkMaster(t);
        return any && !bank.solo[t] && !(m >= 0 && bank.solo[m]);
    }
    int soundSource(int t) const { const int m = linkMaster(t); return (m >= 0 && bank.glob.linkCopy) ? m : t; }
    int machineOf(int t) const { return bank.machine[soundSource(t)]; }
    int noteSource(int t) const { const int m = linkMaster(t); return m >= 0 ? m : t; }   // whose pitch / scale settings apply
    void scaleOf(int t, int& scale, int& root) const
    {
        if (bank.glob.scale > 0) { scale = bank.glob.scale; root = bank.glob.root; return; }
        const int s = noteSource(t);
        scale = bank.kit[s][kfScale]; root = bank.kit[s][kfRoot];
    }
    double stepSamples() const { return stepBeats() * 60.0 / curBpm * sr; }
    bool isSounding(int t) const { return tracks[t].stage != Track::stIdle; }

private:
    struct Ev { uint32_t frame; int kind; int track; float vel; int64_t k; int sub; int note; }; // kind: 0 off, 1 boundary, 2 step, 3 on
    static constexpr uint32_t kCtl = 32;

    float sr = 48000.f;
    Track tracks[kTracks];
    std::vector<Ev> events;
    std::vector<float> dummy;
    int curPattern = 0;
    int64_t lastBoundaryK = -1, playStartK = 0;
    bool wasPlaying = false, prevPlayParam = false, usingInternal = false, blockPlaying = false;
    double internalBeat = 0, prevB1 = 0, curBpm = 120, blockB0 = 0, blockBpf = 0;
    Rng condRng;

    MasterFx fx;
    double lfoClock = 0;             // beat position used by tempo-synced LFOs
    std::vector<float> sendDL, sendDR, sendRL, sendRR;
    std::vector<float> busL[kOutBuses], busR[kOutBuses];

    // -----------------------------------------------------------------------
    bool evalCond(Track& tr, int c, int64_t k, int len)
    {
        if (c <= 0 || c >= kNumConds) return true;
        const CondDef& cd = kConds[c];
        bool ok = true, updates = true;
        switch (cd.kind) {
        case ckProb: ok = condRng.uni() * 100.f < (float)cd.a; break;
        case ckFill: ok = fill; break;
        case ckNotFill: ok = !fill; break;
        case ckPre: ok = tr.pre; updates = false; break;
        case ckNotPre: ok = !tr.pre; updates = false; break;
        case ckFirst: ok = (k - playStartK) / std::max(1, len) == 0; break;
        case ckNotFirst: ok = (k - playStartK) / std::max(1, len) != 0; break;
        case ckRatio: { const int64_t it = (k >= 0 ? k : 0) / std::max(1, len); ok = (it % cd.b) == cd.a - 1; break; }
        default: break;
        }
        if (updates) tr.pre = ok;
        return ok;
    }

    void fire(const Ev& e, uint32_t frame)
    {
        if (e.kind == 1) {
            const Pattern& pat = bank.patterns[curPattern];
            const int master = pat.length ? pat.length : 16;
            if ((((e.k % master) + master) % master) == 0) {
                const int want = clampi((int)global[gPATTERN], 0, kPatterns - 1);
                if (want != curPattern) curPattern = want;
            }
            outStep = (int)(((e.k % 1048576) + 1048576) % 1048576);
            return;
        }
        if (e.kind == 3) { liveNoteOn(e.track, e.note, e.vel, frame); return; }
        if (e.kind == 0) { liveNoteOff(e.track, e.note, frame); return; }
        // sequencer trig
        Track& tr = tracks[e.track];
        const Pattern& P = bank.patterns[curPattern];
        const int t = e.track;
        const int s = P.trackStep(t, e.k);
        const uint8_t f = P.flags[t][s];
        if (!(f & sfTrig)) return;
        if (e.k == tr.recSkipK) return;
        const int lm = linkMaster(t);
        if (lm >= 0 && lm != t) return;          // the other tracks of a LINK group are voices: their own steps do not play
        if (e.sub == 0) {
            tr.decK = e.k;
            tr.decOk = evalCond(tr, P.cond[t][s], e.k, P.trackLen[t]);
        } else if (tr.decK != e.k) return;
        if (!tr.decOk || silenced(t)) return;
        const float acc = 1.f + P.accent / 127.f * 0.6f;
        const int n = std::max<int>(1, P.ratch[t][s]);
        const double stepLen = stepSamples();
        double gate = kGateSteps[P.gate[t][s] % kGateCount] * stepLen;
        if (n > 1) gate = std::min(gate, stepLen / n * 0.8);
        // slide: glide into the note and keep the envelopes going
        const bool slide = (f & sfSlide) != 0;
        applyLocks(t, P.locks[t][s], slide, (uint32_t)(stepLen / n));
        const int note = P.note[t][s] + bank.kit[t][kfTrnsp] - 24;
        if (lm == t) {
            // LINK: the step's note (and chord) goes to the group's voices
            const ChordDef& cd = kChords[P.chord[t][s] % 10];
            const int V = linkVoices();
            const int nn = bank.glob.linkMode == lmUnison ? 1 : std::min(cd.n, V);
            for (int i = 0; i < nn; ++i) {
                const int nnote = note + (bank.glob.linkMode == lmUnison ? 0 : cd.notes[i]);
                const int cnt = bank.glob.linkMode == lmUnison ? V : 1;
                for (int j = 0; j < cnt; ++j) {
                    const int v = bank.glob.linkMode == lmUnison ? bank.glob.linkFirst + j : pickVoice();
                    applyLocks(v, P.locks[t][s], slide, (uint32_t)(stepLen / n));
                    tracks[v].linkNote = -1;
                    startNote(v, nnote, (f & sfAccent) ? acc : 1.f, slide, (int64_t)std::max(1.0, gate));
                }
            }
            return;
        }
        startNote(t, note, (f & sfAccent) ? acc : 1.f, slide, (int64_t)std::max(1.0, gate));
    }

    void applyLocks(int t, const uint8_t* locks, bool slide, uint32_t slideLen)
    {
        Track& tr = tracks[t];
        for (int p = 0; p < kLockable; ++p) {
            const bool l = locks && locks[p] != kNoLock;
            if (slide && l) { tr.slideFrom[p] = tr.eff[p]; tr.sliding[p] = true; }
            else tr.sliding[p] = false;
            tr.locked[p] = l;
            if (l) tr.lockVal[p] = locks[p];
        }
        tr.slidePos = 0; tr.slideLen = std::max<uint32_t>(1, slideLen);
    }

    // start a note on a track: sequencer (gate > 0) or live (gate = -1)
    void startNote(int t, int note, float vel, bool slide, int64_t gate)
    {
        Track& tr = tracks[t];
        note = clampi(note, 0, 127);
        const bool sounding = tr.stage != Track::stIdle;
        const bool gateOpen = tr.stage != Track::stIdle && tr.stage != Track::stRelease;
        const bool legato = sounding && (slide || (bank.kit[t][kfLegato] && gateOpen));
        const int arpMode = bank.kit[t][kfArpMode];
        tr.gateLeft = gate;
        tr.vel = vel;
        if (arpMode > 0) {
            buildArp(t, note, nullptr, 0);
            note = tr.arpNotes[0];
            tr.arpOn = true;
        } else tr.arpOn = false;
        noteTo(t, note, legato, slide);
        ++trigCount[t];
    }

    void noteTo(int t, int note, bool legato, bool forceGlide)
    {
        Track& tr = tracks[t];
        computeEffective(t);
        { int sc = 0, rt = 0; scaleOf(t, sc, rt); note = clampi(quantizeNote(note, sc, rt), 0, 127); }
        tr.startSerial = ++serialCounter;
        const float port = tr.eff[tpPORT];
        tr.targetNote = (float)note;
        if (tr.stage == Track::stIdle) tr.curNote = (float)note;
        if (forceGlide || (legato && port > 0.5f)) tr.glideTime = port > 0.5f ? expmap(port / 127.f, 0.005f, 2.f) : 0.06f;
        else { tr.glideTime = 0.f; tr.curNote = (float)note; }
        if (legato) {
            if (tr.stage == Track::stRelease) tr.stage = Track::stDecay;
            tr.voice.noteOn(false);
        } else {
            // retrigger from the current level (no click)
            tr.stage = Track::stAttack; tr.holdLeft = (uint32_t)(tr.eff[tpHOLD] / 127.f * 2.f * sr);
            tr.fenv = 0.f; tr.fAttack = true;
            for (int l = 0; l < 3; ++l) {
                const int type = lfoTypeIndex((int)effParam(t, kLfoBase[l] + 3));
                if (type == 1) tr.lfoPh[l] = 0;          // RETRIG restarts the cycle at the PHASE point
                if (type == 2) tr.lfoHeld[l] = lfoShape(t, l);
            }
            tr.voice.control(tr.eff);
            tr.voice.noteOn(true);
        }
        tr.idle = 0;
        tr.ledTimer = (uint32_t)(0.08f * sr);
    }

    void buildArp(int t, int root, const int* held, int nHeld)
    {
        Track& tr = tracks[t];
        const int mode = bank.kit[t][kfArpMode], octs = bank.kit[t][kfArpOct] + 1;
        int base[16]; int nb = 0;
        if (nHeld > 0) {
            for (int i = 0; i < nHeld && nb < 16; ++i) base[nb++] = held[i];
            if (mode != 5) std::sort(base, base + nb);
        } else {
            const ChordDef& c = kChords[bank.kit[t][kfArpChord] % 10];
            for (int i = 0; i < c.n; ++i) base[nb++] = root + c.notes[i];
        }
        tr.arpCount = 0;
        for (int o = 0; o < octs; ++o)
            for (int i = 0; i < nb && tr.arpCount < 32; ++i) tr.arpNotes[tr.arpCount++] = clampi(base[i] + 12 * o, 0, 127);
        if (mode == 2) std::reverse(tr.arpNotes, tr.arpNotes + tr.arpCount);
        tr.arpIdx = 0; tr.arpDir = 1;
        tr.arpPeriod = (int64_t)std::max(1.0, kArpSpeeds[bank.kit[t][kfArpSpd] % 6] * stepSamples());
        tr.arpLeft = tr.arpPeriod;
    }

    void arpNext(int t)
    {
        Track& tr = tracks[t];
        if (tr.arpCount <= 0) return;
        const int mode = bank.kit[t][kfArpMode];
        if (mode == 4) tr.arpIdx = (int)(condRng.uni() * tr.arpCount) % tr.arpCount;
        else if (mode == 3) {
            if (tr.arpCount > 1) {
                if (tr.arpIdx + tr.arpDir >= tr.arpCount || tr.arpIdx + tr.arpDir < 0) tr.arpDir = -tr.arpDir;
                tr.arpIdx += tr.arpDir;
            }
        } else tr.arpIdx = (tr.arpIdx + 1) % tr.arpCount;
        noteTo(t, tr.arpNotes[tr.arpIdx], bank.kit[t][kfLegato] != 0, false);
    }

    void liveNoteOn(int t, int note, float vel, uint32_t frame)
    {
        if (t < 0 || t >= kTracks) return;
        const int lm = linkMaster(t);
        if (lm >= 0) { linkNoteOn(lm, note, vel, frame); return; }
        Track& tr = tracks[t];
        // remember the note (last-note priority)
        for (int i = 0; i < tr.heldCount; ++i) if (tr.heldNotes[i] == note) { std::memmove(tr.heldNotes + i, tr.heldNotes + i + 1, sizeof(int) * (tr.heldCount - i - 1)); --tr.heldCount; break; }
        if (tr.heldCount < 16) tr.heldNotes[tr.heldCount++] = note;
        const int played = clampi(note + bank.kit[t][kfTrnsp] - 24, 0, 127);
        if (bank.kit[t][kfArpMode] > 0) {
            int held[16] = {};
            for (int i = 0; i < tr.heldCount; ++i) held[i] = clampi(tr.heldNotes[i] + bank.kit[t][kfTrnsp] - 24, 0, 127);
            const bool wasOn = tr.arpOn && tr.heldCount > 1;
            buildArp(t, played, held, tr.heldCount);
            tr.arpOn = true; tr.gateLeft = -1; tr.vel = vel;
            if (!wasOn) { noteTo(t, tr.arpNotes[0], false, false); ++trigCount[t]; }
        } else {
            const bool legato = tr.heldCount > 1 && bank.kit[t][kfLegato] && tr.stage != Track::stIdle;
            tr.arpOn = false; tr.gateLeft = -1; tr.vel = vel;
            noteTo(t, played, legato, false);
            ++trigCount[t];
        }
        // live recording: write the note on the nearest step
        if (recording && blockPlaying) {
            const double beat = blockB0 + frame * blockBpf;
            const int64_t k = (int64_t)std::llround(beat / stepBeats());
            const Pattern& P = bank.patterns[curPattern];
            pushRec({curPattern, t, P.trackStep(t, k), note, vel > 1.05f, -1});
            tr.recSkipK = k;
            tr.recNote = note; tr.recStep = P.trackStep(t, k); tr.recPattern = curPattern; tr.recBeat = beat;
        }
    }

    void pushRec(const RecEvent& e)
    {
        const int w = recW.load(std::memory_order_relaxed), nw = (w + 1) % kRecQ;
        if (nw == recR.load(std::memory_order_acquire)) return;
        recQ[w] = e;
        recW.store(nw, std::memory_order_release);
    }

    void liveNoteOff(int t, int note, uint32_t frame)
    {
        if (t < 0 || t >= kTracks) return;
        const int lm = linkMaster(t);
        if (lm >= 0) { linkNoteOff(lm, note, frame); return; }
        Track& tr = tracks[t];
        if (recording && blockPlaying && tr.recNote == note) {
            // store the held length as the step's gate (nearest available length)
            const double held = (blockB0 + frame * blockBpf - tr.recBeat) / stepBeats();
            int best = 0;
            for (int g = 1; g < kGateCount; ++g)
                if (std::fabs(kGateSteps[g] - held) < std::fabs(kGateSteps[best] - held)) best = g;
            pushRec({tr.recPattern, t, tr.recStep, note, false, best});
            tr.recNote = -1;
        }
        for (int i = 0; i < tr.heldCount; ++i)
            if (tr.heldNotes[i] == note) { std::memmove(tr.heldNotes + i, tr.heldNotes + i + 1, sizeof(int) * (tr.heldCount - i - 1)); --tr.heldCount; break; }
        if (tr.gateLeft != -1) return; // the sequencer owns the gate
        if (tr.heldCount > 0) {
            const int prev = clampi(tr.heldNotes[tr.heldCount - 1] + bank.kit[t][kfTrnsp] - 24, 0, 127);
            if (bank.kit[t][kfArpMode] > 0) {
                int held[16];
                for (int i = 0; i < tr.heldCount; ++i) held[i] = clampi(tr.heldNotes[i] + bank.kit[t][kfTrnsp] - 24, 0, 127);
                buildArp(t, prev, held, tr.heldCount);
            } else noteTo(t, prev, true, false);
        } else {
            tr.arpOn = false;
            if (tr.stage != Track::stIdle) tr.stage = Track::stRelease;
            tr.gateLeft = 0;
        }
    }

    float effParam(int t, int p) const
    {
        const Track& tr = tracks[t];
        return tr.locked[p] ? (float)tr.lockVal[p] : trackParam[soundSource(t)][p];
    }

    // ---- LINK voices
    uint32_t serialCounter = 0;
    int cycleNext = 0;
    int pickVoice()
    {
        const int first = bank.glob.linkFirst, V = linkVoices();
        switch (bank.glob.linkMode) {
        case lmCycle: cycleNext = (cycleNext + 1) % V; return first + cycleNext;
        case lmRandom: {
            int v = (int)(condRng.uni() * V) % V;
            if (V > 1 && first + v == lastPicked) v = (v + 1) % V;
            return lastPicked = first + v;
        }
        default: {
            // POLY: a free voice, else a released one, else the oldest
            int best = -1; uint32_t bestSerial = UINT32_MAX;
            for (int i = 0; i < V; ++i) if (tracks[first + i].stage == Track::stIdle) { const int v = first + i; if (tracks[v].startSerial < bestSerial) { best = v; bestSerial = tracks[v].startSerial; } }
            if (best >= 0) return best;
            for (int i = 0; i < V; ++i) if (tracks[first + i].stage == Track::stRelease) { const int v = first + i; if (tracks[v].startSerial < bestSerial) { best = v; bestSerial = tracks[v].startSerial; } }
            if (best >= 0) return best;
            for (int i = 0; i < V; ++i) { const int v = first + i; if (tracks[v].startSerial < bestSerial) { best = v; bestSerial = tracks[v].startSerial; } }
            return best;
        }
        }
    }
    int lastPicked = -1;
    void linkNoteOn(int m, int note, float vel, uint32_t frame)
    {
        const int played = clampi(note + bank.kit[m][kfTrnsp] - 24, 0, 127);
        const int V = linkVoices();
        if (bank.glob.linkMode == lmUnison) {
            for (int i = 0; i < V; ++i) {
                Track& tr = tracks[m + i];
                tr.linkNote = note; tr.arpOn = false; tr.gateLeft = -1; tr.vel = vel;
                noteTo(m + i, played, false, false);
            }
        } else {
            const int v = pickVoice();
            Track& tr = tracks[v];
            tr.linkNote = note; tr.arpOn = false; tr.gateLeft = -1; tr.vel = vel;
            noteTo(v, played, false, false);
        }
        ++trigCount[m];
        Track& mt = tracks[m];
        if (recording && blockPlaying) {
            const double beat = blockB0 + frame * blockBpf;
            const int64_t k = (int64_t)std::llround(beat / stepBeats());
            const Pattern& P = bank.patterns[curPattern];
            pushRec({curPattern, m, P.trackStep(m, k), note, vel > 1.05f, -1});
            mt.recSkipK = k;
            mt.recNote = note; mt.recStep = P.trackStep(m, k); mt.recPattern = curPattern; mt.recBeat = beat;
        }
    }
    void linkNoteOff(int m, int note, uint32_t frame)
    {
        Track& mt = tracks[m];
        if (recording && blockPlaying && mt.recNote == note) {
            const double held = (blockB0 + frame * blockBpf - mt.recBeat) / stepBeats();
            int best = 0;
            for (int g = 1; g < kGateCount; ++g)
                if (std::fabs(kGateSteps[g] - held) < std::fabs(kGateSteps[best] - held)) best = g;
            pushRec({mt.recPattern, m, mt.recStep, note, false, best});
            mt.recNote = -1;
        }
        const int V = linkVoices();
        for (int i = 0; i < V; ++i) {
            Track& tr = tracks[m + i];
            if (tr.linkNote != note || tr.gateLeft != -1) continue;
            tr.linkNote = -1; tr.gateLeft = 0;
            if (tr.stage != Track::stIdle) tr.stage = Track::stRelease;
        }
    }

    float lfoShape(int t, int l)
    {
        Track& tr = tracks[t];
        const int shp = lfoShapeIndex((int)effParam(t, kLfoBase[l] + 2));
        float ph = tr.lfoPh[l] + effParam(t, lfoParam(l, 7)) / 128.f;
        if (ph >= 1.f) ph -= 1.f;
        switch (shp) {
        case 0: return 1.f - 4.f * std::fabs(ph - 0.5f);
        case 1: return std::sin(kTwoPi * ph);
        case 2: return ph < 0.5f ? 1.f : -1.f;
        case 3: return 2.f * ph - 1.f;
        case 4: return 1.f - 2.f * ph;
        default: return tr.lfoRnd[l];
        }
    }

    void computeEffective(int t)
    {
        Track& tr = tracks[t];
        const float sl = tr.slideLen > 0 ? std::min(1.f, (float)tr.slidePos / (float)tr.slideLen) : 1.f;
        for (int p = 0; p < kLockable; ++p) {
            float v = effParam(t, p);
            if (tr.sliding[p]) v = tr.slideFrom[p] + (v - tr.slideFrom[p]) * sl;
            if (tr.mod[p] != 0.f) v = clampf(v + tr.mod[p], 0.f, 127.f);
            tr.eff[p] = v;
        }
    }

    void runLfos(uint32_t n)
    {
        for (auto& tr : tracks) { std::fill(tr.mod, tr.mod + kLockable, 0.f); tr.pitchMod = 0.f; }
        for (int t = 0; t < kTracks; ++t) {
            Track& tr = tracks[t];
            for (int l = 0; l < 3; ++l) {
                const int b = kLfoBase[l];
                const int type = lfoTypeIndex((int)effParam(t, b + 3));
                if (lfoSynced(effParam(t, lfoParam(l, 6)))) {
                    const double cyc = kLfoSyncBeats[lfoSyncDivIndex((int)effParam(t, b))];
                    if (type == 0) {
                        // FREE + SYNC: locked to the song position
                        const double ph = lfoClock / cyc - std::floor(lfoClock / cyc);
                        if ((float)ph < tr.lfoPh[l]) tr.lfoRnd[l] = tr.voice.rng.next();
                        tr.lfoPh[l] = (float)ph;
                    } else {
                        tr.lfoPh[l] += (float)(blockBpf * n / cyc);
                        while (tr.lfoPh[l] >= 1.f) { tr.lfoPh[l] -= 1.f; tr.lfoRnd[l] = tr.voice.rng.next(); }
                    }
                } else {
                    tr.lfoPh[l] += lfoFreeHz(effParam(t, b)) * n / sr;
                    while (tr.lfoPh[l] >= 1.f) { tr.lfoPh[l] -= 1.f; tr.lfoRnd[l] = tr.voice.rng.next(); }
                }
                const float dep = effParam(t, b + 1);
                if (dep < 0.5f) continue;
                const float out = type == 2 ? tr.lfoHeld[l] : lfoShape(t, l);
                const int dst = std::min(kLfoDests, lfoDestIndex((int)effParam(t, b + 4)));
                const int trk = lfoTrackIndex((int)effParam(t, b + 5));
                const int target = trk == 0 ? t : trk - 1;
                if (dst >= kLfoDests) { const float d = dep / 127.f; tracks[target].pitchMod += out * d * d * 12.f; } // PITCH: up to +-12 semitones
                else tracks[target].mod[dst] += out * dep * 0.5f;
            }
        }
    }

    void controlTrack(int t, uint32_t n)
    {
        Track& tr = tracks[t];
        if (tr.slidePos < tr.slideLen) tr.slidePos += n;
        computeEffective(t);
        const float* e = tr.eff;
        tr.voice.control(e);
        tr.amd = e[tpAMD] / 127.f;
        tr.amf = expmap(e[tpAMF] / 127.f, 0.5f, 2000.f);
        const float db = (e[tpEQG] - 64.f) / 64.f * 18.f;
        tr.eqDb = db;
        if (std::fabs(db) > 0.05f) tr.eq.setPeak(expmap(e[tpEQF] / 127.f, 40.f, 16000.f), 1.0f, db, sr);
        // filter envelope (AD)
        if (tr.fAttack) {
            const float atk = e[tpFATK] > 0.5f ? expmap(e[tpFATK] / 127.f, 0.001f, 4.f) : 0.f;
            tr.fenv = atk > 0.f ? std::min(1.f, tr.fenv + n / (atk * sr)) : 1.f;
            if (tr.fenv >= 1.f) tr.fAttack = false;
        } else {
            tr.fenv *= std::pow(decayCoef(expmap(e[tpFDEC] / 127.f, 0.005f, 6.f), sr), (float)n);
        }
        // filter: main multimode filter (CUTOFF / RESO / TYPE, envelope, key tracking) + a gentle LOW CUT
        const float kt = e[tpKEYTRK] / 127.f * (tr.curNote - 60.f) / 12.f;
        const float envOct = (e[tpFENV] - 64.f) / 64.f * 7.f * tr.fenv;
        const float press = midiPress[noteSource(t)] * 3.f;   // aftertouch: up to 3 octaves brighter
        const float cutHz = 20.f * std::exp2(e[tpCUTOFF] / 127.f * 10.f + envOct + kt + press);
        const float q = 0.5f + e[tpRESO] / 127.f * 17.f;
        const float lowHz = cutoffHz(e[tpLOWCUT]);
        tr.ftype = filterTypeIndex((int)e[tpFTYPE]);
        tr.hpOn = lowHz > 24.f;
        if (tr.hpOn) tr.hp.setup(lowHz, 0.707f, sr);
        tr.lpOn = tr.ftype != 0 || cutHz < 19000.f || q > 0.8f;
        if (tr.lpOn) {
            tr.lp.setup(std::min(cutHz, 20000.f), q, sr);
            if (tr.ftype == 1) tr.lp2.setup(std::min(cutHz, 20000.f), 0.6f, sr);
            tr.bpGain = std::sqrt(tr.lp.k) * 1.4f;   // band-pass: louder than unity-peak, tamer than raw at high resonance
        }
        tr.srrN = e[tpSRR] > 0.5f ? (int)expmap(e[tpSRR] / 127.f, 1.f, 64.f) : 1;
        tr.dist = e[tpDIST] / 127.f;
        const float v = e[tpVOL] / 127.f;
        tr.vol = silenced(t) ? 0.f : v * v * 0.9f;
        const float pan = e[tpPAN] / 127.f;
        tr.panL = std::cos(pan * kPi * 0.5f); tr.panR = std::sin(pan * kPi * 0.5f);
        tr.del = e[tpDEL] / 127.f * 1.4f; tr.rev = e[tpREV] / 127.f * 1.4f;   // sends go above unity
    }

    // envelope + sequencer gate + arpeggiator for one block
    void advanceTrack(int t, uint32_t n, float* envOut)
    {
        Track& tr = tracks[t];
        const float* e = tr.eff;
        // gate and arpeggiator clocks
        if (tr.gateLeft > 0) {
            tr.gateLeft -= n;
            if (tr.gateLeft <= 0) { tr.gateLeft = 0; tr.arpOn = false; if (tr.stage != Track::stIdle) tr.stage = Track::stRelease; }
        }
        if (tr.arpOn && (tr.gateLeft != 0)) {
            tr.arpLeft -= n;
            if (tr.arpLeft <= 0) { tr.arpLeft += tr.arpPeriod; arpNext(t); }
        }
        const float atkS = e[tpATK] > 0.5f ? expmap(e[tpATK] / 127.f, 0.0005f, 5.f) * sr : 48.f;
        const float sus = e[tpSUS] / 127.f;
        const float decC = decayCoef(expmap(e[tpDEC] / 127.f, 0.005f, 6.f), sr);
        const float relC = decayCoef(expmap(e[tpREL] / 127.f, 0.005f, 8.f), sr);
        const float peak = std::min(1.6f, tr.vel);
        for (uint32_t i = 0; i < n; ++i) {
            switch (tr.stage) {
            case Track::stAttack:
                tr.env += peak / atkS;
                if (tr.env >= peak) { tr.env = peak; tr.stage = tr.holdLeft ? Track::stHold : Track::stDecay; }
                break;
            case Track::stHold: if (tr.holdLeft > 0) --tr.holdLeft; else tr.stage = Track::stDecay; break;
            case Track::stDecay:
                tr.env = sus * peak + (tr.env - sus * peak) * decC;
                if (std::fabs(tr.env - sus * peak) < 1e-4f) tr.stage = Track::stSustain;
                break;
            case Track::stSustain: tr.env += (sus * peak - tr.env) * 0.002f; break;
            case Track::stRelease:
                tr.env *= relC;
                if (tr.env < 1e-4f) { tr.env = 0.f; tr.stage = Track::stIdle; }
                break;
            default: tr.env = 0.f; break;
            }
            envOut[i] = tr.env;
        }
    }

    void renderSegment(float* const* outs, uint32_t n)
    {
        for (int b = 0; b < kOutBuses; ++b) { std::fill(busL[b].begin(), busL[b].begin() + n, 0.f); std::fill(busR[b].begin(), busR[b].begin() + n, 0.f); }
        std::fill(sendDL.begin(), sendDL.begin() + n, 0.f); std::fill(sendDR.begin(), sendDR.begin() + n, 0.f);
        std::fill(sendRL.begin(), sendRL.begin() + n, 0.f); std::fill(sendRR.begin(), sendRR.begin() + n, 0.f);
        runLfos(n);
        lfoClock += blockBpf * n;
        const float inv = 1.f / n;
        float envBuf[64];
        for (int t = 0; t < kTracks; ++t) {
            Track& tr = tracks[t];
            if (tr.stage != Track::stIdle) tr.idle = 0; else tr.idle += n;
            if (tr.idle > (uint32_t)(sr * 0.5f) || machineOf(t) == mOFF) continue;
            controlTrack(t, n);
            if (tr.cVol == 0.f && tr.idle == 0) { tr.cVol = tr.vol; tr.cPanL = tr.panL; tr.cPanR = tr.panR; tr.cDel = tr.del; tr.cRev = tr.rev; }
            advanceTrack(t, n, envBuf);
            // glide
            const float n0 = tr.curNote;
            if (tr.curNote != tr.targetNote) {
                if (tr.glideTime <= 0.f) tr.curNote = tr.targetNote;
                else {
                    tr.curNote += (tr.targetNote - tr.curNote) * (1.f - std::exp(-(float)n / (tr.glideTime * 0.35f * sr)));
                    if (std::fabs(tr.curNote - tr.targetNote) < 0.002f) tr.curNote = tr.targetNote;
                }
            }
            float* b = tr.buf.data();
            // MIDI pitch bend + mod wheel vibrato (5.5 Hz, up to half a semitone); LINK voices follow their first track
            const int es = noteSource(t);
            vibPh[t] += 5.5f * (float)n / sr; if (vibPh[t] >= 1.f) vibPh[t] -= 1.f;
            const float expr = midiBend[es] + (midiMod[es] > 0.f ? midiMod[es] * 0.5f * std::sin(kTwoPi * vibPh[t]) : 0.f);
            tr.voice.render(b, (int)n, noteHz(n0 + tr.pitchMod + tr.uniDetune + expr), noteHz(tr.curNote + tr.pitchMod + tr.uniDetune + expr));
            static const float kMachineGain[kMachines] = {0.f, 1.4f, 0.95f, 1.0f, 1.0f, 1.3f, 0.75f, 2.2f, 1.0f, 1.2f};
            const float mg = kMachineGain[machineOf(t) % kMachines];
            for (uint32_t i = 0; i < n; ++i) b[i] *= mg;
            const int bus = std::min<int>(kOutBuses - 1, bank.kit[soundSource(t)][kfOut]);
            float* mL = busL[bus].data(); float* mR = busR[bus].data();
            const float dVol = (tr.vol - tr.cVol) * inv, dPL = (tr.panL - tr.cPanL) * inv, dPR = (tr.panR - tr.cPanR) * inv;
            const float dDel = (tr.del - tr.cDel) * inv, dRev = (tr.rev - tr.cRev) * inv;
            const float amInc = tr.amf / sr;
            const bool eqOn = std::fabs(tr.eqDb) > 0.05f;
            for (uint32_t i = 0; i < n; ++i) {
                float s = b[i];
                if (tr.hpOn) { tr.hp.tick(s); s = tr.hp.hp; }
                if (tr.lpOn) {
                    tr.lp.tick(s);
                    switch (tr.ftype) {
                    case 0: s = tr.lp.lp; break;
                    case 1: tr.lp2.tick(tr.lp.lp); s = tr.lp2.lp; break;
                    case 2: s = tr.lp.bp * tr.bpGain; break;
                    case 3: s = tr.lp.hp; break;
                    default: s = tr.lp.lp + tr.lp.hp; break;
                    }
                }
                s *= envBuf[i];
                s = tr.distAA.process(s, tr.dist);
                if (tr.amd > 0.f) {
                    tr.amPh += amInc; if (tr.amPh >= 1.f) tr.amPh -= 1.f;
                    s *= 1.f - tr.amd * (0.5f - 0.5f * std::sin(kTwoPi * tr.amPh));
                }
                if (eqOn) s = tr.eq.tick(s);
                if (tr.srrN > 1) { if (--tr.srrCnt <= 0) { tr.srrCnt = tr.srrN; tr.srrHold = s; } s = tr.srrHold; }
                tr.cVol += dVol; tr.cPanL += dPL; tr.cPanR += dPR; tr.cDel += dDel; tr.cRev += dRev;
                s *= tr.cVol;
                const float l = s * tr.cPanL, r = s * tr.cPanR;
                mL[i] += l; mR[i] += r;
                sendDL[i] += l * tr.cDel; sendDR[i] += r * tr.cDel;
                sendRL[i] += l * tr.cRev; sendRR[i] += r * tr.cRev;
            }
        }
        fx.process(global, (float)curBpm, busL[0].data(), busR[0].data(), sendDL.data(), sendDR.data(), sendRL.data(), sendRR.data(),
                   outs[0], outs[1], n);
        masterFilter(outs[0], outs[1], n);
        const float mv = global[gVOLUME] / 127.f; const float vol = mv * mv * 1.2f;
        for (int b = 1; b < kOutBuses; ++b)
            for (uint32_t i = 0; i < n; ++i) { outs[2 * b][i] = safety(busL[b][i] * vol); outs[2 * b + 1][i] = safety(busR[b][i] * vol); }
    }

    static inline float safety(float x) { return MasterFx::safety(x); }

    // ---- master filter: high pass + low pass (12 dB/oct state-variable) with a shared resonance, on everything
    Svf mfHp[2], mfLp[2];
    float mfHpHz = 20.f, mfLpHz = 20000.f;
    void masterFilter(float* L, float* R, uint32_t n)
    {
        const bool hpOn = global[gMF_HP] > 0.5f, lpOn = global[gMF_LP] < 126.5f;
        if (!hpOn && !lpOn) { mfHpHz = 20.f; mfLpHz = 20000.f; for (auto* f : {&mfHp[0], &mfHp[1], &mfLp[0], &mfLp[1]}) f->reset(); return; }
        const float q = 0.707f + global[gMF_RESO] / 127.f * 9.f;
        // glide the cutoffs a little per block (no zipper noise when turning the knobs)
        mfHpHz += (cutoffHz(global[gMF_HP]) - mfHpHz) * 0.35f;
        mfLpHz += (cutoffHz(global[gMF_LP]) - mfLpHz) * 0.35f;
        for (int c = 0; c < 2; ++c) { mfHp[c].setup(mfHpHz, q, sr); mfLp[c].setup(mfLpHz, q, sr); }
        float* ch[2] = {L, R};
        for (int c = 0; c < 2; ++c)
            for (uint32_t i = 0; i < n; ++i) {
                float s = ch[c][i];
                if (hpOn) { mfHp[c].tick(s); s = mfHp[c].hp; }
                if (lpOn) { mfLp[c].tick(s); s = mfLp[c].lp; }
                ch[c][i] = safety(s);
            }
    }
};

} // namespace kn
