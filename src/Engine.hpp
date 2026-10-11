// Kinetik KMSP-6 — engine: 6 monophonic synth tracks, envelopes, filters, 3 LFOs, arpeggiator,
// sequencer with notes, master FX, 4 stereo outputs
#pragma once
#include "Bank.hpp"
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
    Svf hp, lp; bool hpOn = false, lpOn = false;
    int srrN = 1, srrCnt = 0; float srrHold = 0;
    float vol = 0, panL = 0.707f, panR = 0.707f, del = 0, rev = 0;
    float cVol = 0, cPanL = 0.707f, cPanR = 0.707f, cDel = 0, cRev = 0;
    float amd = 0, amf = 1, dist = 0;
    // lfos
    float lfoPh[3] = {}, lfoHeld[3] = {}, lfoRnd[3] = {};
    float pitchMod = 0.f;
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
        for (int i = 0; i < 20; ++i) global[gECHO_TIME + i] = kMasterDefaults[i];
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
        const size_t echoLen = (size_t)(sr * 4.2f) + 8;
        echoL.assign(echoLen, 0.f); echoR.assign(echoLen, 0.f);
        static const float revMs[8] = {29.7f, 37.1f, 41.1f, 43.7f, 53.3f, 59.9f, 67.7f, 73.1f};
        for (int i = 0; i < 8; ++i) {
            revLen[i] = (int)(revMs[i] * 0.001f * sr * 1.3f);
            revLine[i].assign(revLen[i] + 2, 0.f);
            revPos[i] = 0; revLp[i] = 0;
        }
        preBuf.assign((size_t)(sr * 0.25f) + 4, 0.f);
        sendDL.assign(64, 0); sendDR.assign(64, 0); sendRL.assign(64, 0); sendRR.assign(64, 0);
        for (int b = 0; b < kOutBuses; ++b) { busL[b].assign(64, 0); busR[b].assign(64, 0); }
        events.reserve(4096);
        echoW = 0; preW = 0;
        reset();
    }

    void reset()
    {
        for (auto& tr : tracks) { tr.stage = Track::stIdle; tr.env = 0; tr.heldCount = 0; tr.arpOn = false; tr.gateLeft = 0; }
        std::fill(echoL.begin(), echoL.end(), 0.f); std::fill(echoR.begin(), echoR.end(), 0.f);
        for (int i = 0; i < 8; ++i) std::fill(revLine[i].begin(), revLine[i].end(), 0.f);
        std::fill(preBuf.begin(), preBuf.end(), 0.f);
        wasPlaying = false; internalBeat = 0; prevPlayParam = false; usingInternal = false;
        cmpEnv = 0;
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
        for (int t = 0; t < kTracks; ++t) tracks[t].voice.setMachine(bank.machine[t]);

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

    std::vector<float> echoL, echoR; uint32_t echoW = 0; float echoDelay = 1000, echoLpL = 0, echoLpR = 0, echoHpL = 0, echoHpR = 0;
    std::vector<float> revLine[8]; int revLen[8] = {}; int revPos[8] = {}; float revLp[8] = {};
    std::vector<float> preBuf; uint32_t preW = 0; float revHpZ = 0, gateEnv = 0, gateGain = 1; uint32_t gateHold = 0;
    Biquad eqLowL, eqLowR, eqHighL, eqHighR;
    float cmpEnv = 0;
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
        if (e.sub == 0) {
            tr.decK = e.k;
            tr.decOk = evalCond(tr, P.cond[t][s], e.k, P.trackLen[t]);
        } else if (tr.decK != e.k) return;
        if (!tr.decOk || bank.mute[t]) return;
        const float acc = 1.f + P.accent / 127.f * 0.6f;
        const int n = std::max<int>(1, P.ratch[t][s]);
        const double stepLen = stepSamples();
        double gate = kGateSteps[P.gate[t][s] % kGateCount] * stepLen;
        if (n > 1) gate = std::min(gate, stepLen / n * 0.8);
        // slide: glide into the note and keep the envelopes going
        const bool slide = (f & sfSlide) != 0;
        applyLocks(t, P.locks[t][s], slide, (uint32_t)(stepLen / n));
        const int note = P.note[t][s] + bank.kit[t][kfTrnsp] - 24;
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
                if (type == 1) tr.lfoPh[l] = 0;
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
        return tr.locked[p] ? (float)tr.lockVal[p] : trackParam[t][p];
    }

    float lfoShape(int t, int l)
    {
        Track& tr = tracks[t];
        const int shp = lfoShapeIndex((int)effParam(t, kLfoBase[l] + 2));
        const float ph = tr.lfoPh[l];
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
                const float hz = expmap(effParam(t, b) / 127.f, 0.03f, 30.f);
                tr.lfoPh[l] += hz * n / sr;
                while (tr.lfoPh[l] >= 1.f) { tr.lfoPh[l] -= 1.f; tr.lfoRnd[l] = tr.voice.rng.next(); }
                const float dep = effParam(t, b + 1);
                if (dep < 0.5f) continue;
                const int type = lfoTypeIndex((int)effParam(t, b + 3));
                const float out = type == 2 ? tr.lfoHeld[l] : lfoShape(t, l);
                const int dst = std::min(kLfoDests, lfoDestIndex((int)effParam(t, b + 4)));
                const int trk = lfoTrackIndex((int)effParam(t, b + 5));
                const int target = trk == 0 ? t : trk - 1;
                if (dst >= kLfoDests) tracks[target].pitchMod += out * dep / 127.f * 2.f; // PITCH: up to +-2 semitones
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
        const float kt = bank.kit[t][kfKeyTrk] * 0.25f * (tr.curNote - 60.f) / 12.f;
        const float baseOct = e[tpBASE] / 127.f * 10.f, widthOct = e[tpWDTH] / 127.f * 10.f;
        const float bof = (e[tpBOF] - 64.f) / 64.f * 6.f * tr.fenv, wof = (e[tpWOF] - 64.f) / 64.f * 6.f * tr.fenv;
        const float hpHz = 20.f * std::exp2(baseOct + bof + kt);
        const float lpHz = 20.f * std::exp2(baseOct + widthOct + bof + wof + kt);
        const float hq = 0.5f + e[tpHPQ] / 127.f * 11.f, lq = 0.5f + e[tpLPQ] / 127.f * 17.f;
        tr.hpOn = hpHz > 24.f;
        if (tr.hpOn) tr.hp.setup(hpHz, hq, sr);
        tr.lpOn = lpHz < 19000.f || lq > 0.8f;
        if (tr.lpOn) tr.lp.setup(std::min(lpHz, 20000.f), lq, sr);
        tr.srrN = e[tpSRR] > 0.5f ? (int)expmap(e[tpSRR] / 127.f, 1.f, 64.f) : 1;
        tr.dist = e[tpDIST] / 127.f;
        const float v = e[tpVOL] / 127.f;
        tr.vol = bank.mute[t] ? 0.f : v * v * 0.9f;
        const float pan = e[tpPAN] / 127.f;
        tr.panL = std::cos(pan * kPi * 0.5f); tr.panR = std::sin(pan * kPi * 0.5f);
        tr.del = e[tpDEL] / 127.f; tr.rev = e[tpREV] / 127.f;
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
        const float inv = 1.f / n;
        float envBuf[64];
        for (int t = 0; t < kTracks; ++t) {
            Track& tr = tracks[t];
            if (tr.stage != Track::stIdle) tr.idle = 0; else tr.idle += n;
            if (tr.idle > (uint32_t)(sr * 0.5f) || bank.machine[t] == mOFF) continue;
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
            tr.voice.render(b, (int)n, noteHz(n0 + tr.pitchMod), noteHz(tr.curNote + tr.pitchMod));
            static const float kMachineGain[kMachines] = {0.f, 1.4f, 0.95f, 1.0f, 1.0f, 1.3f, 0.75f, 2.2f, 1.0f};
            const float mg = kMachineGain[bank.machine[t] % kMachines];
            for (uint32_t i = 0; i < n; ++i) b[i] *= mg;
            const int bus = std::min<int>(kOutBuses - 1, bank.kit[t][kfOut]);
            float* mL = busL[bus].data(); float* mR = busR[bus].data();
            const float dVol = (tr.vol - tr.cVol) * inv, dPL = (tr.panL - tr.cPanL) * inv, dPR = (tr.panR - tr.cPanR) * inv;
            const float dDel = (tr.del - tr.cDel) * inv, dRev = (tr.rev - tr.cRev) * inv;
            const float amInc = tr.amf / sr;
            const bool eqOn = std::fabs(tr.eqDb) > 0.05f;
            for (uint32_t i = 0; i < n; ++i) {
                float s = b[i];
                if (tr.hpOn) { tr.hp.tick(s); s = tr.hp.hp; }
                if (tr.lpOn) { tr.lp.tick(s); s = tr.lp.lp; }
                s *= envBuf[i];
                s = drive(s, tr.dist);
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
        masterFx(outs[0], outs[1], n);
        const float mv = global[gVOLUME] / 127.f; const float vol = mv * mv * 1.2f;
        for (int b = 1; b < kOutBuses; ++b)
            for (uint32_t i = 0; i < n; ++i) { outs[2 * b][i] = safety(busL[b][i] * vol); outs[2 * b + 1][i] = safety(busR[b][i] * vol); }
    }

    void masterFx(float* L, float* R, uint32_t n)
    {
        const float* g = global;
        // ---- echo ----
        const int div = echoDivIndex((int)g[gECHO_TIME]);
        float targetDelay = (float)(kEchoBeats[div] * 60.0 / curBpm * sr);
        targetDelay = clampf(targetDelay, 4.f, (float)echoL.size() - 4.f);
        const float fb = std::min(1.02f, g[gECHO_FB] / 127.f * 1.05f);
        const float lpA = std::exp(-kTwoPi * expmap(g[gECHO_LPF] / 127.f, 500.f, 20000.f) / sr);
        const float hpA = std::exp(-kTwoPi * expmap(g[gECHO_HPF] / 127.f, 20.f, 3000.f) / sr);
        const float w = g[gECHO_WIDTH] / 127.f;
        const float eLev = g[gECHO_LEV] / 127.f * 1.4f;
        // ---- reverb ----
        const float rt = expmap(g[gREV_SIZE] / 127.f, 0.2f, 10.f);
        const float damp = 0.05f + g[gREV_DAMP] / 127.f * 0.85f;
        const float rHpA = std::exp(-kTwoPi * expmap(g[gREV_HPF] / 127.f, 20.f, 2000.f) / sr);
        const uint32_t psz = (uint32_t)preBuf.size();
        const uint32_t pre = std::min<uint32_t>(psz - 1, (uint32_t)(g[gREV_PRE] / 127.f * 0.2f * sr));
        const float rLev = g[gREV_LEV] / 127.f * 1.2f;
        const bool gate = g[gREV_GATE] > 0.5f;
        const uint32_t gateHoldS = (uint32_t)(expmap(g[gREV_GATE] / 127.f, 0.03f, 1.0f) * sr);
        float revG[8];
        for (int i = 0; i < 8; ++i) revG[i] = std::pow(10.f, -3.f * revLen[i] / (rt * sr));
        // ---- master ----
        eqLowL.setShelf(false, 120.f, (g[gEQ_LOW] - 64.f) / 64.f * 15.f, sr); copyCoef(eqLowR, eqLowL);
        eqHighL.setShelf(true, 7000.f, (g[gEQ_HIGH] - 64.f) / 64.f * 15.f, sr); copyCoef(eqHighR, eqHighL);
        const float thrDb = -40.f + g[gCMP_THR] / 127.f * 40.f;
        const float ratio = 1.f + g[gCMP_RAT] / 127.f * 19.f;
        const float atk = std::exp(-1.f / (expmap(g[gCMP_ATK] / 127.f, 0.0001f, 0.08f) * sr));
        const float rel = std::exp(-1.f / (expmap(g[gCMP_REL] / 127.f, 0.02f, 1.0f) * sr));
        const float makeup = std::pow(10.f, (-thrDb * (1.f - 1.f / ratio) * 0.4f) / 20.f);
        const float drv = g[gDRIVE] / 127.f;
        const float mv = g[gVOLUME] / 127.f; const float vol = mv * mv * 1.2f;
        const uint32_t esz = (uint32_t)echoL.size();

        for (uint32_t i = 0; i < n; ++i) {
            // echo
            echoDelay += (targetDelay - echoDelay) * 0.0005f;
            float rp = (float)echoW - echoDelay; if (rp < 0) rp += esz;
            const uint32_t i0 = (uint32_t)rp % esz; const uint32_t i1 = (i0 + 1) % esz; const float fr = rp - std::floor(rp);
            const float dl = echoL[i0] + (echoL[i1] - echoL[i0]) * fr;
            const float dr = echoR[i0] + (echoR[i1] - echoR[i0]) * fr;
            const float sm = 0.5f * (sendDL[i] + sendDR[i]);
            float inL = sendDL[i] * (1.f - w) + sm * w + fb * (w * dr + (1.f - w) * dl);
            float inR = sendDR[i] * (1.f - w) + fb * (w * dl + (1.f - w) * dr);
            echoLpL = inL + lpA * (echoLpL - inL); echoLpR = inR + lpA * (echoLpR - inR);
            echoHpL = echoLpL + hpA * (echoHpL - echoLpL); echoHpR = echoLpR + hpA * (echoHpR - echoLpR);
            echoL[echoW] = std::tanh(echoLpL - echoHpL); echoR[echoW] = std::tanh(echoLpR - echoHpR);
            echoW = (echoW + 1) % esz;

            // reverb (8-line FDN, Householder feedback)
            float rin = 0.5f * (sendRL[i] + sendRR[i]);
            revHpZ = rin + rHpA * (revHpZ - rin); rin = rin - revHpZ;
            preBuf[preW] = rin;
            const float rinD = preBuf[(preW + psz - pre) % psz];
            preW = (preW + 1) % psz;
            float o[8], sum = 0.f;
            for (int k = 0; k < 8; ++k) { o[k] = revLine[k][revPos[k]]; sum += o[k]; }
            const float hh = sum * 0.25f;
            float outRL = 0.f, outRR = 0.f;
            for (int k = 0; k < 8; ++k) {
                float v = (o[k] - hh) * revG[k];
                revLp[k] = v + damp * (revLp[k] - v);
                v = revLp[k] + rinD * ((k & 1) ? 0.5f : -0.5f);
                revLine[k][revPos[k]] = v;
                if (++revPos[k] >= revLen[k]) revPos[k] = 0;
                if (k < 4) outRL += o[k] * ((k & 1) ? -1.f : 1.f); else outRR += o[k] * ((k & 1) ? 1.f : -1.f);
            }
            if (gate) {
                const float a = std::fabs(rin);
                gateEnv = std::max(a, gateEnv * 0.9995f);
                if (gateEnv > 0.003f) gateHold = gateHoldS; else if (gateHold > 0) --gateHold;
                const float tgt = gateHold > 0 ? 1.f : 0.f;
                gateGain += (tgt - gateGain) * (tgt > gateGain ? 0.02f : 0.004f);
            } else gateGain = 1.f;

            float l = busL[0][i] + dl * eLev + outRL * 0.35f * rLev * gateGain;
            float r = busR[0][i] + dr * eLev + outRR * 0.35f * rLev * gateGain;
            l = eqHighL.tick(eqLowL.tick(l)); r = eqHighR.tick(eqLowR.tick(r));
            // compressor (linked peak)
            const float pk = std::max(std::fabs(l), std::fabs(r));
            cmpEnv = pk > cmpEnv ? pk + atk * (cmpEnv - pk) : pk + rel * (cmpEnv - pk);
            if (ratio > 1.01f && thrDb < -0.1f) {
                const float lv = 20.f * std::log10(cmpEnv + 1e-9f);
                const float over = lv - thrDb;
                float gain = makeup;
                if (over > 0.f) gain *= std::pow(10.f, -over * (1.f - 1.f / ratio) / 20.f);
                l *= gain; r *= gain;
            }
            l = drive(l, drv) * vol; r = drive(r, drv) * vol;
            L[i] = safety(l); R[i] = safety(r);
        }
    }

    // transparent below 0.9, smooth ceiling at 1.0
    static inline float safety(float x)
    {
        const float a = std::fabs(x);
        if (a <= 0.9f) return x;
        const float y = 0.9f + 0.1f * std::tanh((a - 0.9f) * 10.f);
        return x < 0.f ? -y : y;
    }

    static void copyCoef(Biquad& dst, const Biquad& src)
    {
        dst.b0 = src.b0; dst.b1 = src.b1; dst.b2 = src.b2; dst.a1 = src.a1; dst.a2 = src.a2;
    }
};

} // namespace kn
