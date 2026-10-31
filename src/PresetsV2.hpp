// Kinetik KMSP-6 — factory presets, second generation (0.15). All original material.
// Each preset is a 4-bar (64 steps) piece: bass, chords / stabs, pad, lead and colour tracks with slides, ties,
// accents, conditions, ratchets, micro-timing, per-step locks (filter, envelope, chord shape...), LFOs (synced),
// arpeggiators, the echo / reverb / chorus / CHAOS sends and, where it helps, several patterns in a CHAIN.
// Track levels are balanced with the mix checker (bass on top, chords and lead a few dB under, pads and colours lower).
// Included by Presets.hpp after the PB builder (inside namespace kn).
#pragma once

inline std::string mBars(std::initializer_list<const char*> bs)
{
    std::string s; for (auto x : bs) { if (!s.empty()) s += " | "; s += x; } return s;
}
inline std::string mRep(const char* bar, int n) { std::string s; for (int i = 0; i < n; ++i) { if (i) s += " | "; s += bar; } return s; }

struct PB2 {
    PB& p;
    explicit PB2(PB& b) : p(b) {}
    void seq(int pat, int t, const std::string& s) { p.seq(pat, t, s.c_str()); }
    // a per-trig value cycling over the trigs of a track (any lockable param)
    void cyc(int pat, int t, int prm, std::initializer_list<int> vals)
    {
        const int n = (int)vals.size(); int i = 0; const int* a = vals.begin();
        for (int s = 0; s < kSteps; ++s) if (p.b.patterns[pat].flags[t][s] & sfTrig) p.lock(pat, t, s, prm, a[i++ % n]);
    }
    void cond(int pat, int t, std::initializer_list<int> steps, int c) { for (int s : steps) p.cond(pat, t, s, c); }
    void micro(int pat, int t, std::initializer_list<int> steps, int m) { for (int s : steps) p.b.patterns[pat].micro[t][s] = (int8_t)m; }
    // synced LFO: div = index into kLfoSyncNames, shape TRI SINE SQR SAW+ SAW- RND, type FREE RETRIG HOLD
    void lfoS(int t, int l, int div, int depth, int shape, int type, int dest, int trk = 0)
    {
        p.lfo(t, l, div * 8 + 4, depth, shape, type, dest, trk);
        p.fx(t, lfoParam(l, 6), 100);
    }
    void copy(int src, int dst) { p.copyPattern(dst, src); }
    void chain(const char* spec) { p.b.chain.parse(spec, kPatterns); }
    void vol(int t, int v) { p.fx(t, tpVOL, v); }
    void pan(int t, int v) { p.fx(t, tpPAN, v); }
    void send(int t, int del, int rev, int cho = 0) { p.fx(t, tpDEL, del); p.fx(t, tpREV, rev); p.g(choSendG(t), cho); }
    void chaos(int t, int amount) { p.g(chaSendG(t), amount); }
    void echo(int div, int fb, int model, int mod, int lev) { p.g(gECHO_TIME, fxDiv(div)); p.g(gECHO_FB, fb); p.g(gECHO_MODEL, echoModelValue(model)); p.g(gECHO_MOD, mod); p.g(gECHO_LEV, lev); }
    void verb(int size, int damp, int model, int mod, int lev, int pre = 10) { p.g(gREV_SIZE, size); p.g(gREV_DAMP, damp); p.g(gREV_MODEL, reverbModelValue(model)); p.g(gREV_MOD, mod); p.g(gREV_LEV, lev); p.g(gREV_PRE, pre); }
    void table(int t, int tb) { p.b.kit[t][kfTable] = (uint8_t)tb; }
    // level correction per track in dB (from the mix checker): VOL knob and VOL locks of every pattern
    void mix(std::initializer_list<float> db)
    {
        int t = 0;
        for (float d : db) {
            const float k = std::pow(10.f, d / 40.f);
            float& v = tparam(p.v, t, tpVOL); v = std::min(127.f, std::round(v * k));
            for (auto& P : p.b.patterns) for (int s = 0; s < kSteps; ++s) { uint8_t& l = P.locks[t][s][tpVOL]; if (l != kNoLock) l = (uint8_t)std::min(127.f, std::round(l * k)); }
            if (++t >= kTracks) break;
        }
    }
    void off(int t) { p.b.machine[t] = mOFF; }
};
// LFO sync divisions (kLfoSyncNames)
enum { ls16Bars, ls8Bars, ls4Bars, ls2Bars, ls1Bar, lsHalf, ls4D, ls4, ls4T, ls8D, ls8, ls8T, ls16, ls16T, ls32, ls64 };
static int shapeV2(int i) { return i * 8 + 4; }   // CHORD machine: value of chord shape i (kChordShapeDefs)
enum { csMaj, csMin, csSus2, csSus4, csMaj7, csMin7, csDom7, csMin7b5, cs6th, csMin6, csAdd9, csMaj9, csMin9, csDim, csAug, csQuartal };

// ======================================================================== TECHNO
// Detroit Strings 128: rolling bass, minor 7th chord stabs that change shape every bar, string pad, bell answers, a lead
// that only comes in for the last two bars. B = breakdown (no bass, the pad opens).
static void m2Detroit(PB& b)
{
    PB2 q(b);
    b.kit(0, mSAW, {0, 30, 70, 50, 32, 64, 70, 0}); b.amp(0, 0, 0, 50, 50, 25); b.flt2(0, 46, 50, 0, 10, 0, 42, 92); b.fx(0, tpPORT, 18);
    b.kit(1, mCHORD, {30, shapeV2(csMin7), 40, 0, 20, 30, 6, 80}); b.amp(1, 0, 0, 58, 36, 40); b.flt2(1, 84, 30, 0, 20, 0, 40, 80);
    b.kit(2, mSUPER, {40, 90, 127, 0, 0, 64, 0, 0}); b.amp(2, 90, 0, 90, 110, 100); b.flt2(2, 62, 20, 0, 15, 0, 50, 64);
    b.kit(3, mFM, {48, 50, 112, 30, 0, 3, 60, 0}); b.amp(3, 0, 0, 70, 0, 80);
    b.kit(4, mWAVE, {30, 30, 40, 64, 70, 30, 10, 0}); q.table(4, 4); b.amp(4, 5, 0, 60, 80, 50); b.flt2(4, 92, 20, 0, 20, 0, 50, 64);
    b.fx(4, tpPORT, 25); b.legato(4); b.lfo(4, 1, 72, 10, 1, 0, PITCH);
    b.kit(5, mNOISE, {120, 20, 0, 0, 0, 0, 0, 0}); b.amp(5, 0, 0, 18, 0, 14); b.flt2(5, 127, 20, 0, 72, 0, 50, 64);
    q.seq(0, 0, mBars({"F1 . F1 F2 | . F1 . F1 | . F1 F2 . | F1 . ~Ab1 .", "F1 . F1 F2 | . F1 . F1 | . F1 F2 . | Eb1 . ~F1 .",
                       "Db1 . Db1 Db2 | . Db1 . Db1 | . Db1 Db2 . | Db1 . ~Eb1 .", "Eb1 . Eb1 Eb2 | . Eb1 . Eb1 | Eb2 . Eb1 . | !C2 . ~Eb1 ."}));
    q.cyc(0, 0, tpCUTOFF, {44, 46, 50, 46, 52, 48});
    q.seq(0, 1, mBars({". . F3 . | . . . F3 | . . F3 . | . . . .", ". . F3 . | . . . F3 | . . F3 . | . . Eb3 .",
                       ". . Db3 . | . . . Db3 | . . Db3 . | . . . .", ". . Eb3 . | . . . Eb3 | . . Eb3 . | . . C3 ."}));
    q.cyc(0, 1, 1, {shapeV2(csMin7), shapeV2(csMin7), shapeV2(csMin7), shapeV2(csMin9), shapeV2(csMin7), shapeV2(csMin7), shapeV2(csMin7), shapeV2(csSus4),
                    shapeV2(csMaj7), shapeV2(csMaj7), shapeV2(csMaj7), shapeV2(csDom7), shapeV2(csDom7), shapeV2(csMaj9), shapeV2(csMin7)});
    q.cond(0, 1, {7, 23, 39, 55}, 3);
    q.seq(0, 2, mBars({"Ab3 - - - | - - - - | - - - - | - - - -", "G3 - - - | - - - - | - - - - | - - - -",
                       "F3 - - - | - - - - | - - - - | - - - -", "G3 - - - | - - - - | Bb3 - - - | - - - -"}));
    q.seq(0, 3, mBars({". . . . | . . . . | . . . . | . . C5 .", ". . . . | . . . . | . . . . | . . Eb5 .",
                       ". . . . | . . . . | . . . . | . . F5 .", ". . . . | . . . . | . . G5 . | . . Ab5 ."}));
    q.seq(0, 4, mBars({". . . . | . . . . | . . . . | . . . .", ". . . . | . . . . | . . . . | . . . .",
                       "C4 - - Eb4 | - - F4 - | ~G4 - - - | Eb4 - - -", "F4 - - Eb4 | - - C4 - | ~Bb3 - - - | C4 - - -"}));
    q.seq(0, 5, mRep(". . x . | . . x . | . . x . | . . x x", 4)); q.cyc(0, 5, tpVOL, {70, 60, 66, 56});
    q.micro(0, 5, {2, 6, 10, 14, 18, 22, 26, 30, 34, 38, 42, 46, 50, 54, 58, 62}, 2);
    b.settings(0, 64, 54);
    q.copy(0, 1);
    q.seq(1, 0, mBars({". . . . | . . . . | . . . . | . . . .", ". . . . | . . . . | . . . . | . . . .",
                       ". . . . | . . . . | . . . . | . . . .", ". . . . | . . . . | Eb1 . Eb1 . | Eb1 Eb1 Eb1 Eb1"}));
    q.seq(1, 2, mBars({"Ab3 - - - | - - - - | - - - - | - - - -", "G3 - - - | - - - - | - - - - | - - - -",
                       "F3 - - - | - - - - | - - - - | - - - -", "G3 - - - | - - - - | - - - - | - - - -"}));
    b.lock(1, 2, 0, tpCUTOFF, 50); b.lock(1, 2, 16, tpCUTOFF, 70); b.lock(1, 2, 32, tpCUTOFF, 88); b.lock(1, 2, 48, tpCUTOFF, 104);
    b.settings(1, 64, 54);
    q.lfoS(2, 0, ls2Bars, 24, 1, 0, tpCUTOFF);
    q.lfoS(1, 0, ls4Bars, 30, 0, 0, tpCUTOFF);
    q.send(0, 0, 0); q.send(1, 55, 30, 30); q.send(2, 10, 80, 40); q.send(3, 50, 70); q.send(4, 45, 30); q.send(5, 0, 20);
    q.pan(1, 46); q.pan(3, 88); q.pan(4, 58); q.pan(5, 76);
    q.echo(d8D, 58, emPingPong, 30, 66); q.verb(90, 60, rmHall, 40, 70, 20);
    q.chain("1 2 0:3 1:1");
    b.g(gCMP_THR, 80); b.g(gCMP_RAT, 30); b.g(gCMP_ATK, 50); b.g(gCMP_REL, 40);   // glue
    b.g(gTEMPO, 128);
    q.vol(0, 100); q.vol(1, 80); q.vol(2, 66); q.vol(3, 66); q.vol(4, 80); q.vol(5, 60);
    q.off(6); q.off(7);
    q.mix({6.4f, 3.1f, -7.7f, 8.0f, -5.3f, 20.5f});
}

// Acid Pressure 132: a 64-step acid line (accents, slides, filter envelope locks that build), a second line answering
// an octave up with an LFO on its cutoff, chord stab, noise hats; B = the line transposed with more resonance.
static void m2Acid(PB& b)
{
    PB2 q(b);
    b.kit(0, mSAW, {0, 0, 64, 0, 64, 64, 0, 0}); b.amp(0, 0, 0, 45, 50, 20); b.flt2(0, 40, 92, 0, 0, 0, 48, 108);
    b.fx(0, tpDIST, 36); b.fx(0, tpPORT, 22);
    b.kit(1, mPULSE, {64, 0, 0, 0, 0, 30, 0, 0}); b.amp(1, 0, 0, 40, 40, 20); b.flt2(1, 46, 80, 0, 0, 0, 42, 100); b.fx(1, tpPORT, 22); b.fx(1, tpDIST, 20);
    b.kit(2, mCHORD, {20, shapeV2(csMin), 30, 0, 10, 30, 0, 80}); b.amp(2, 0, 0, 58, 36, 40); b.flt2(2, 86, 30, 0, 20, 0, 35, 76);
    b.kit(3, mNOISE, {110, 40, 0, 0, 0, 0, 0, 0}); b.amp(3, 0, 0, 22, 0, 18); b.flt2(3, 127, 20, 0, 72, 0, 50, 64);
    b.kit(4, mSUPER, {40, 90, 100, 0, 0, 64, 0, 20}); b.amp(4, 60, 0, 70, 90, 80); b.flt2(4, 60, 20, 0, 20, 0, 50, 64);
    const char* line1 = "!A1 A1 ~A2 A1 | . A1 ~C2 !A1 | G1 . A1 ~E2 | A1 !A2 . ~G2";
    const char* line2 = "A1 . ~C2 A1 | !A2 . A1 ~G1 | . A1 !C2 ~D2 | A1 . E2 ~A1";
    q.seq(0, 0, mBars({line1, line2, line1, "!A1 A1 ~A2 A1 | . A1 ~C2 !A1 | ~D2 ~E2 !G2 ~A2 | !C3 A2 ~G2 E2"}));
    q.cyc(0, 0, tpFENV, {96, 100, 104, 100, 108, 104, 112, 108, 116, 112, 120, 116, 124});
    b.ratch(0, 0, 60, 2);
    q.seq(0, 1, mBars({". . . . | . . . . | . . . . | . . . .", ". . A2 . | ~C3 . . A2 | . . E3 . | . G2 . .",
                       ". . . . | . . . . | . . . . | . . . .", ". . A2 . | ~C3 . . A2 | . . E3 ~G3 | . A3 . ."}));
    q.seq(0, 2, mRep(". . . . | . . A3 . | . . . . | . . . .", 4)); q.cond(0, 2, {6, 38}, 3);
    q.seq(0, 3, mRep(". . X . | . . X . | . . X . | . . X x", 4)); q.cyc(0, 3, tpVOL, {72, 72, 72, 54});
    q.seq(0, 4, mBars({"A2 - - - | - - - - | - - - - | - - - -", ". . . . | . . . . | . . . . | . . . .",
                       "F2 - - - | - - - - | - - - - | - - - -", "G2 - - - | - - - - | - - - - | - - - -"}));
    b.settings(0, 64, 56);
    q.copy(0, 1);
    for (int s = 0; s < 64; ++s) if (b.b.patterns[1].flags[0][s] & sfTrig) b.b.patterns[1].note[0][s] = (uint8_t)(b.b.patterns[1].note[0][s] + 5);
    q.cyc(1, 0, tpRESO, {100, 104, 108, 112});
    b.settings(1, 64, 56);
    q.lfoS(1, 0, ls1Bar, 40, 0, 0, tpCUTOFF);
    q.lfoS(0, 0, ls8Bars, 20, 1, 0, tpCUTOFF);
    q.send(0, 25, 0); q.send(1, 45, 10); q.send(2, 60, 40); q.send(3, 0, 10); q.send(4, 10, 70, 40);
    q.pan(1, 80); q.pan(2, 44); q.pan(3, 70);
    q.echo(d16D, 52, emTape, 40, 60); q.verb(60, 60, rmPlate, 40, 60);
    b.g(gDRIVE, 18);
    q.chain("1 2 0:2 1:2");
    b.g(gTEMPO, 132);
    q.vol(0, 96); q.vol(1, 80); q.vol(2, 70); q.vol(3, 62); q.vol(4, 60);
    q.off(5); q.off(6); q.off(7);
    q.mix({-4.7f, 0.3f, 14.0f, 19.2f, -1.5f});
}

// Hard Groove 136: inspired by the owner's "Hard Groove": rolling FM bass in 16ths with ghost notes, pulse stabs,
// metallic FM hits, noise rides.
static void m2HardGroove(PB& b)
{
    PB2 q(b);
    b.kit(0, mFM, {16, 60, 64, 0, 30, 0, 30, 20}); b.amp(0, 0, 0, 34, 20, 18); b.flt2(0, 64, 30, 0, 10, 0, 30, 84); b.fx(0, tpDIST, 30);
    b.kit(1, mPULSE, {40, 60, 40, 30, 20, 0, 0, 0}); b.amp(1, 0, 0, 26, 0, 22); b.flt2(1, 76, 40, 0, 20, 0, 30, 84);
    b.kit(2, mFM, {80, 90, 100, 60, 30, 3, 20, 0}); b.amp(2, 0, 0, 20, 0, 20); b.fx(2, tpSRR, 20);
    b.kit(3, mNOISE, {120, 40, 0, 20, 0, 40, 0, 0}); b.amp(3, 0, 0, 30, 0, 24); b.flt2(3, 127, 40, 0, 72, 0, 50, 64);
    b.kit(4, mSUPER, {60, 100, 127, 0, 0, 64, 0, 30}); b.amp(4, 0, 0, 60, 30, 60); b.flt2(4, 70, 30, 0, 30, 0, 40, 84);
    q.seq(0, 0, mRep("D1 D1 D2 D1 | . D1 D2 D1 | D1 . D2 D1 | D1 D1 ~F1 D1", 3) + " | D1 D1 D2 D1 | . D1 D2 D1 | ~F1 F1 G1 ~A1 | C2 . A1 G1");
    q.cyc(0, 0, tpVOL, {100, 70, 90, 70, 100, 70, 84, 64}); q.cyc(0, 0, tpFENV, {84, 76, 92, 80});
    q.seq(0, 1, mBars({". . . . | . . D3 . | . . . . | . . . D3", ". . . . | . . D3 . | . . . . | . . F3 .",
                       ". . . . | . . D3 . | . . . . | . . . D3", ". . . . | . . D3 . | . . D3 . | F3 . G3 ."}));
    q.cond(0, 1, {15, 47}, 2);
    q.seq(0, 2, mRep(". x . . | x . . x | . . x . | . x . .", 4)); q.cyc(0, 2, 0, {80, 96, 70, 104});
    q.cond(0, 2, {1, 17, 33, 49}, 3);
    q.seq(0, 3, mRep("x . x . | x . x . | x . x . | x . x x", 4)); q.cyc(0, 3, tpVOL, {74, 56});
    q.seq(0, 4, mBars({". . . . | . . . . | . . . . | . . . .", ". . . . | . . . . | . . . . | . . . .",
                       ". . . . | . . . . | . . . . | . . . .", "D3 . . D3 | . . D3 . | . D3 . . | F3 . . ."}));
    b.settings(0, 64, 57);
    q.lfoS(0, 0, ls2Bars, 30, 0, 0, tpCUTOFF);
    q.lfoS(1, 0, ls4Bars, 40, 3, 0, tpCUTOFF);
    q.send(1, 50, 20); q.send(2, 40, 30); q.send(3, 0, 20); q.send(4, 40, 50);
    q.pan(1, 44); q.pan(2, 84); q.pan(3, 70); q.pan(4, 54);
    q.echo(d16D, 50, emBbd, 40, 60); q.verb(45, 70, rmRoom, 50, 55);
    b.g(gDRIVE, 22);
    b.g(gCMP_THR, 80); b.g(gCMP_RAT, 30); b.g(gCMP_ATK, 50); b.g(gCMP_REL, 40);   // glue
    b.g(gTEMPO, 136);
    q.vol(0, 100); q.vol(1, 80); q.vol(2, 66); q.vol(3, 60); q.vol(4, 70);
    q.off(5); q.off(6); q.off(7);
    q.mix({-4.0f, 20.4f, 7.6f, 0.5f, -7.4f});
}

// Berlin Sequence 112: a 16th-note sequence whose cutoff an LFO sweeps over 4 bars, a counter sequence of 12 steps
// (polymetric), slow pad, choir, plucks in the echo.
static void m2Berlin(PB& b)
{
    PB2 q(b);
    b.kit(0, mSAW, {0, 30, 66, 30, 64, 64, 50, 0}); b.amp(0, 0, 0, 50, 40, 40); b.flt2(0, 56, 70, 0, 0, 0, 50, 92); b.fx(0, tpPORT, 8);
    b.kit(1, mSAW, {0, 64, 70, 50, 96, 64, 0, 0}); b.amp(1, 0, 0, 45, 0, 40); b.flt2(1, 70, 60, 0, 10, 0, 40, 88);
    b.kit(2, mSUPER, {40, 80, 127, 64, 0, 64, 0, 0}); b.amp(2, 90, 0, 90, 110, 100); b.flt2(2, 66, 20, 0, 20, 0, 50, 64);
    b.kit(3, mVOX, {20, 60, 20, 60, 30, 64, 40, 60}); b.amp(3, 60, 0, 80, 100, 90);
    b.kit(4, mPLUCK, {60, 70, 50, 40, 0, 50, 40, 0}); b.amp(4, 0, 0, 90, 100, 60);
    q.seq(0, 0, mBars({"E1 E2 B1 E2 | D2 E2 B1 E2 | G1 E2 B1 E2 | A1 E2 D2 B1", "E1 E2 B1 E2 | D2 E2 B1 E2 | G1 E2 B1 E2 | A1 E2 D2 B1",
                       "C1 C2 G1 C2 | B1 C2 G1 C2 | E1 C2 G1 C2 | F1 C2 B1 G1", "D1 D2 A1 D2 | C2 D2 A1 D2 | F#1 D2 A1 D2 | G1 D2 C2 A1"}));
    q.cyc(0, 0, tpFENV, {100, 84, 90, 84});
    q.seq(0, 1, "E3 B3 G3 E4 D4 B3 G3 B3 E3 B3 F#4 D4");
    q.cond(0, 1, {9, 11}, 3);
    q.seq(0, 2, mBars({"E3 - - - | - - - - | - - - - | - - - -", "G3 - - - | - - - - | - - - - | - - - -",
                       "C3 - - - | - - - - | - - - - | - - - -", "D3 - - - | - - - - | F#3 - - - | - - - -"}));
    q.seq(0, 3, mBars({". . . . | . . . . | . . . . | . . . .", "B3 - - - | - - - - | - - - - | - - - -",
                       ". . . . | . . . . | . . . . | . . . .", "A3 - - - | - - - - | - - - - | - - - -"}));
    q.seq(0, 4, mBars({". . . . | . . . . | . . . . | . . B4 .", ". . . . | . . . . | . . G4 . | . . E4 .",
                       ". . . . | . . . . | . . . . | . . C5 .", ". . . . | . . . . | . . A4 . | . . F#4 ."}));
    b.settings(0, 64, 50);
    q.lfoS(0, 0, ls4Bars, 50, 0, 0, tpCUTOFF);
    q.lfoS(1, 0, ls1Bar, 30, 1, 0, tpCUTOFF);
    q.lfoS(3, 0, ls2Bars, 40, 1, 0, 0);
    q.send(0, 30, 0); q.send(1, 70, 30); q.send(2, 0, 90, 40); q.send(3, 20, 90); q.send(4, 70, 50);
    q.pan(1, 34); q.pan(3, 80); q.pan(4, 94);
    q.echo(d8D, 70, emTape, 40, 70); b.g(gECHO_LPF, 80); q.verb(110, 60, rmHall, 50, 80, 20);
    b.g(gTEMPO, 112);
    q.vol(0, 92); q.vol(1, 74); q.vol(2, 66); q.vol(3, 62); q.vol(4, 72);
    q.off(5); q.off(6); q.off(7);
    q.mix({6.4f, 10.5f, -9.1f, 3.2f, 39.8f});
}

// ======================================================================== HOUSE
// K-House Keys 124: inspired by the owner's "K-House": FM bass, organ chords on the off-beats (ORGAN table), plucked
// answers, wave pad, a vocal hook every other bar.
static void m2KHouse(PB& b)
{
    PB2 q(b);
    b.kit(0, mFM, {16, 40, 64, 0, 10, 0, 30, 40}); b.amp(0, 0, 0, 44, 40, 24); b.flt2(0, 60, 20, 0, 10, 0, 40, 76);
    b.kit(1, mCHORD, {60, shapeV2(csMin7), 30, 0, 0, 10, 0, 90}); q.table(1, 3); b.amp(1, 0, 0, 58, 36, 40); b.flt2(1, 106, 10, 0, 20, 0, 50, 64);
    b.kit(2, mPLUCK, {65, 100, 40, 40, 0, 40, 30, 0}); b.amp(2, 0, 0, 70, 100, 50);
    b.kit(3, mWAVE, {40, 10, 80, 64, 66, 40, 0, 0}); b.amp(3, 80, 0, 90, 100, 90);
    b.kit(4, mVOX, {30, 64, 30, 60, 20, 50, 30, 30}); b.amp(4, 10, 0, 60, 80, 50); b.fx(4, tpPORT, 30); b.legato(4);
    b.kit(5, mNOISE, {120, 20, 0, 0, 0, 0, 0, 0}); b.amp(5, 0, 0, 16, 0, 12); b.flt2(5, 127, 20, 0, 72, 0, 50, 64);
    q.seq(0, 0, mBars({"A1 . . A1 | . . A2 . | A1 . . A1 | . . G1 .", "F1 . . F1 | . . F2 . | F1 . . F1 | . . E1 .",
                       "D1 . . D1 | . . D2 . | D1 . . D1 | . . C2 .", "E1 . . E1 | . . E2 . | E1 . ~G1 . | ~A1 . E1 ."}));
    q.seq(0, 1, mBars({". . A3 . | . . A3 . | . . A3 . | . . A3 .", ". . F3 . | . . F3 . | . . F3 . | . . F3 .",
                       ". . D3 . | . . D3 . | . . D3 . | . . D3 .", ". . E3 . | . . E3 . | . . E3 . | . E3 . E3"}));
    q.cyc(0, 1, 1, {shapeV2(csMin7), shapeV2(csMin7), shapeV2(csMin9), shapeV2(csMin7), shapeV2(csMaj7), shapeV2(csMaj7), shapeV2(csMaj9), shapeV2(csMaj7),
                    shapeV2(csMin7), shapeV2(csMin7), shapeV2(csMin9), shapeV2(csMin7), shapeV2(csDom7), shapeV2(csDom7), shapeV2(csSus4), shapeV2(csDom7), shapeV2(csDom7)});
    q.micro(0, 1, {2, 6, 10, 14, 18, 22, 26, 30, 34, 38, 42, 46, 50, 54}, 2);
    q.seq(0, 2, mRep(". . . . | . E4 . . | . . . C4 | . . . .", 4)); q.cyc(0, 2, 0, {64, 60, 70, 56});
    q.cond(0, 2, {11, 43}, 3);
    q.seq(0, 3, mBars({"C4 - - - | - - - - | - - - - | - - - -", "A3 - - - | - - - - | - - - - | - - - -",
                       "F3 - - - | - - - - | - - - - | - - - -", "G#3 - - - | - - - - | B3 - - - | - - - -"}));
    q.seq(0, 4, mBars({". . . . | . . . . | . . . . | . . . .", "E4 - - D4 | - - C4 - | ~A3 - - - | . . . .",
                       ". . . . | . . . . | . . . . | . . . .", "E4 - - G4 | - - E4 - | ~D4 - - - | . . . ."}));
    q.seq(0, 5, mRep(". . x . | . . x . | . . x . | . . x .", 4));
    b.settings(0, 64, 57);
    q.lfoS(3, 0, ls4Bars, 40, 1, 0, tpCUTOFF);
    q.lfoS(1, 0, ls2Bars, 20, 1, 0, tpCUTOFF);
    q.send(1, 30, 30, 50); q.send(2, 60, 40); q.send(3, 0, 80, 40); q.send(4, 40, 60); q.send(5, 0, 10);
    q.pan(1, 50); q.pan(2, 84); q.pan(3, 40); q.pan(5, 74);
    q.echo(d8D, 50, emPingPong, 30, 62); q.verb(70, 60, rmPlate, 40, 66);
    b.g(gCHO_MODEL, 50); b.g(gCHO_LEV, 60);
    b.g(gCMP_THR, 80); b.g(gCMP_RAT, 30); b.g(gCMP_ATK, 50); b.g(gCMP_REL, 40);   // glue
    b.g(gTEMPO, 124);
    q.vol(0, 100); q.vol(1, 80); q.vol(2, 70); q.vol(3, 60); q.vol(4, 72); q.vol(5, 56);
    q.off(6); q.off(7);
    q.mix({7.7f, 1.2f, 7.8f, 0.2f, -2.1f, 12.5f});
}

// Dub Chords 120: one minor chord stab into a long tape echo that ducks, sub bass, hiss, a pluck that comes and goes.
static void m2DubChords(PB& b)
{
    PB2 q(b);
    b.kit(0, mSAW, {127, 127, 64, 0, 64, 64, 90, 0}); b.amp(0, 0, 0, 70, 90, 40); b.flt2(0, 36, 10, 0, 0, 0, 50, 64);
    b.kit(1, mCHORD, {30, shapeV2(csMin7), 40, 0, 20, 30, 0, 70}); b.amp(1, 0, 0, 58, 36, 40); b.flt2(1, 80, 40, 0, 30, 0, 32, 80);
    b.kit(2, mNOISE, {80, 50, 0, 0, 40, 0, 20, 0}); b.amp(2, 80, 0, 90, 90, 100);
    b.kit(3, mPLUCK, {50, 50, 60, 60, 20, 60, 60, 0}); b.amp(3, 0, 0, 80, 100, 60);
    b.kit(4, mSUPER, {20, 60, 127, 64, 0, 64, 0, 0}); b.amp(4, 110, 0, 110, 110, 115); b.flt2(4, 54, 10, 0, 10, 0, 50, 64);
    q.seq(0, 0, mBars({"C2 - - - | - - . . | C2 - - - | . . Eb2 .", "C2 - - - | - - . . | C2 - - - | . . G1 .",
                       "Ab1 - - - | - - . . | Ab1 - - - | . . Bb1 .", "C2 - - - | - - . . | C2 - . ~Eb2 | - - G1 ."}));
    q.seq(0, 1, mRep(". . . . | . . C3 . | . . . . | . . . .", 3) + " | . . . . | . . C3 . | . . . . | . C3 . .");
    q.cyc(0, 1, tpCUTOFF, {60, 66, 72, 64, 80});
    q.cond(0, 1, {61}, 3);
    q.seq(0, 2, "x - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -");
    q.seq(0, 3, mBars({". . . . | . . . . | . . . . | . . G4 .", ". . . . | . . . . | . . Eb4 . | . . . .",
                       ". . . . | . . . . | . . . . | . . C5 .", ". . . . | . . . . | . . Bb4 . | . . G4 ."}));
    q.cond(0, 3, {14, 26, 46, 58}, 2);
    q.seq(0, 4, mBars({"Eb3 - - - | - - - - | - - - - | - - - -", "G3 - - - | - - - - | - - - - | - - - -",
                       "C3 - - - | - - - - | - - - - | - - - -", "Eb3 - - - | - - - - | - - - - | - - - -"}));
    b.settings(0, 64, 53);
    q.lfoS(1, 0, ls8Bars, 40, 1, 0, tpDEL);
    q.lfoS(2, 0, ls4Bars, 50, 1, 0, 0);
    q.send(1, 100, 50, 30); q.send(2, 0, 90); q.send(3, 80, 70); q.send(4, 0, 100);
    q.pan(1, 52); q.pan(3, 94); q.pan(2, 40);
    q.echo(d8D, 86, emTape, 60, 80); b.g(gECHO_LPF, 70); b.g(gECHO_HPF, 30); b.g(gECHO_DUCK, 50); q.verb(105, 70, rmHall, 50, 80, 30);
    b.g(gCMP_THR, 80); b.g(gCMP_RAT, 30); b.g(gCMP_ATK, 50); b.g(gCMP_REL, 40);   // glue
    b.g(gTEMPO, 120);
    q.vol(0, 96); q.vol(1, 80); q.vol(2, 50); q.vol(3, 64); q.vol(4, 56);
    q.off(5); q.off(6); q.off(7);
    q.mix({0.7f, 3.2f, -3.7f, 32.6f, -7.2f});
}

// Deep Garage 130: swung organ chords, a bouncing pulse bass with slides, a vocal chop with LFO on the vowel, bells.
static void m2Garage(PB& b)
{
    PB2 q(b);
    b.kit(0, mPULSE, {40, 0, 0, 0, 0, 80, 0, 0}); b.amp(0, 0, 0, 50, 60, 20); b.flt2(0, 66, 40, 0, 0, 0, 40, 90); b.fx(0, tpPORT, 20); b.legato(0);
    b.kit(1, mCHORD, {60, shapeV2(csMin9), 30, 0, 0, 10, 4, 90}); q.table(1, 3); b.amp(1, 0, 0, 58, 36, 40); b.flt2(1, 104, 10, 0, 20, 0, 50, 64);
    b.kit(2, mVOX, {40, 50, 10, 80, 0, 64, 70, 0}); b.amp(2, 0, 0, 40, 60, 30); b.fx(2, tpPORT, 30);
    b.kit(3, mFM, {48, 50, 112, 30, 0, 3, 60, 0}); b.amp(3, 0, 0, 70, 0, 80);
    b.kit(4, mSUPER, {35, 80, 110, 0, 0, 64, 0, 0}); b.amp(4, 90, 0, 90, 110, 100); b.flt2(4, 64, 10, 0, 20, 0, 50, 64);
    q.seq(0, 0, mBars({"F1 . . . | . . F1 . | . . ~Ab1 . | F1 . . .", "Db1 . . . | . . Db1 . | . . ~Eb1 . | Db1 . . .",
                       "Bb0 . . . | . . Bb0 . | . . ~Db1 . | Bb0 . . .", "C1 . . . | . . C1 . | ~Eb1 . . F1 | . . ~E1 ."}));
    q.seq(0, 1, mBars({". . . F3 | . . . . | . F3 . . | . . . .", ". . . Db3 | . . . . | . Db3 . . | . . . .",
                       ". . . Bb2 | . . . . | . Bb2 . . | . . . .", ". . . C3 | . . . . | . C3 . . | . . C3 ."}));
    q.seq(0, 2, mBars({". . . . | . . . . | . . . . | C4 . Eb4 .", ". . . . | . . . . | . . . . | . . . .",
                       ". . . . | . . . . | . . . . | F4 . Eb4 .", ". . . . | . . . . | . . . . | C4 . Bb3 ."}));
    q.cyc(0, 2, 0, {20, 60, 100, 40, 80, 30});
    q.seq(0, 3, mRep(". . . . | . . . . | . . . . | . . . .", 3) + " | . . . . | . . . . | . . G5 . | . . F5 .");
    q.seq(0, 4, mBars({"Ab3 - - - | - - - - | - - - - | - - - -", "F3 - - - | - - - - | - - - - | - - - -",
                       "Db3 - - - | - - - - | - - - - | - - - -", "Eb3 - - - | - - - - | E3 - - - | - - - -"}));
    b.settings(0, 64, 64);
    q.lfoS(2, 0, ls8, 50, 0, 1, 1);
    q.send(1, 40, 30, 40); q.send(2, 50, 40); q.send(3, 50, 60); q.send(4, 0, 80);
    q.pan(1, 46); q.pan(2, 80); q.pan(3, 90);
    q.echo(d8D, 48, emPingPong, 30, 60); q.verb(60, 60, rmPlate, 40, 60);
    b.g(gCMP_THR, 80); b.g(gCMP_RAT, 30); b.g(gCMP_ATK, 50); b.g(gCMP_REL, 40);   // glue
    b.g(gTEMPO, 130);
    q.vol(0, 98); q.vol(1, 80); q.vol(2, 70); q.vol(3, 60); q.vol(4, 60);
    q.off(5); q.off(6); q.off(7);
    q.mix({2.4f, -2.4f, -0.7f, -1.4f, -10.3f});
}

// ======================================================================== TRANCE / SYNTH-POP
// Trance Gate 138: off-beat bass, a pad gated in 16ths by a square LFO on its level, plucked arp (UP, 2 octaves),
// a supersaw lead in bar 3-4 and a noise riser in pattern B.
static void m2Trance(PB& b)
{
    PB2 q(b);
    b.kit(0, mSAW, {0, 0, 64, 0, 64, 64, 90, 0}); b.amp(0, 0, 0, 50, 70, 20); b.flt2(0, 62, 40, 0, 0, 0, 40, 84);
    b.kit(1, mSUPER, {50, 100, 127, 0, 0, 64, 0, 10}); b.amp(1, 20, 0, 90, 110, 80); b.flt2(1, 76, 20, 0, 20, 0, 50, 64);
    b.kit(2, mPLUCK, {60, 110, 30, 30, 0, 40, 30, 0}); b.amp(2, 0, 0, 80, 100, 50); b.arp(2, 1, 2, 2, 2);
    b.kit(3, mSUPER, {70, 110, 127, 0, 20, 64, 10, 20}); b.amp(3, 0, 0, 60, 80, 50); b.flt2(3, 84, 30, 0, 30, 0, 50, 80); b.fx(3, tpPORT, 20); b.legato(3);
    b.kit(4, mNOISE, {90, 20, 0, 0, 0, 0, 30, 0}); b.amp(4, 100, 0, 110, 110, 100);
    q.seq(0, 0, mBars({". . A1 . | . . A1 . | . . A1 . | . . A1 .", ". . F1 . | . . F1 . | . . F1 . | . . F1 .",
                       ". . C2 . | . . C2 . | . . C2 . | . . C2 .", ". . G1 . | . . G1 . | . . G1 . | . . G1 G1"}));
    q.seq(0, 1, mBars({"A3 - - - | - - - - | - - - - | - - - -", "F3 - - - | - - - - | - - - - | - - - -",
                       "C4 - - - | - - - - | - - - - | - - - -", "G3 - - - | - - - - | - - - - | - - - -"}));
    q.seq(0, 2, mBars({"A3 - - - | - - - - | - - - - | - - - -", "F3 - - - | - - - - | - - - - | - - - -",
                       "C4 - - - | - - - - | - - - - | - - - -", "G3 - - - | - - - - | - - - - | - - - -"}));
    q.seq(0, 3, mBars({". . . . | . . . . | . . . . | . . . .", ". . . . | . . . . | . . . . | . . . .",
                       "E4 - - - | ~G4 - - - | E4 - - D4 | - - C4 -", "D4 - - - | - - B3 - | ~C4 - - - | - - . ."}));
    b.settings(0, 64, 50);
    q.copy(0, 1);
    q.seq(1, 0, mRep(". . . . | . . . . | . . . . | . . . .", 4));
    q.seq(1, 4, "x - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -");
    q.seq(1, 3, mRep(". . . . | . . . . | . . . . | . . . .", 4));
    b.settings(1, 64, 50);
    q.lfoS(1, 0, ls16, 110, 2, 0, tpVOL);
    q.lfoS(4, 0, ls16Bars, 90, 3, 1, tpCUTOFF);
    b.flt2(4, 30, 60, 2, 0, 0, 50, 64);
    q.lfoS(2, 0, ls4Bars, 30, 1, 0, tpCUTOFF);
    q.send(1, 30, 70, 40); q.send(2, 70, 40); q.send(3, 40, 60, 30); q.send(4, 30, 90);
    q.pan(2, 30); q.pan(3, 64);
    q.echo(d8D, 60, emPingPong, 30, 66); q.verb(110, 60, rmHall, 50, 80, 20);
    q.chain("1 2 0:3 1:1");
    b.g(gTEMPO, 138);
    q.vol(0, 98); q.vol(1, 70); q.vol(2, 74); q.vol(3, 78); q.vol(4, 56);
    q.off(5); q.off(6); q.off(7);
    q.mix({28.5f, -42.8f, 5.4f, -7.3f, 0.0f});
}

// Night Drive 100: eighth-note saw bass, supersaw pad, a pulse lead with delayed vibrato, FM bells, gated chords.
static void m2Synthwave(PB& b)
{
    PB2 q(b);
    b.kit(0, mSAW, {0, 0, 64, 0, 64, 64, 70, 0}); b.amp(0, 0, 0, 40, 60, 20); b.flt2(0, 52, 30, 0, 0, 0, 40, 80);
    b.kit(1, mSUPER, {60, 100, 127, 0, 30, 64, 0, 20}); b.amp(1, 30, 0, 70, 90, 70); b.flt2(1, 80, 20, 0, 20, 0, 50, 64);
    b.kit(2, mPULSE, {50, 40, 30, 0, 0, 0, 0, 0}); b.amp(2, 5, 0, 60, 90, 60); b.fx(2, tpPORT, 24); b.legato(2);
    b.lfo(2, 1, 74, 12, 1, 1, PITCH);
    b.kit(3, mFM, {32, 30, 48, 20, 0, 3, 60, 30}); b.amp(3, 0, 0, 70, 50, 60);
    b.kit(4, mCHORD, {20, shapeV2(csMaj7), 40, 0, 30, 40, 0, 70}); b.amp(4, 0, 0, 58, 36, 40); b.flt2(4, 88, 20, 0, 20, 0, 40, 76);
    b.kit(5, mNOISE, {100, 10, 0, 0, 0, 0, 30, 0}); b.amp(5, 0, 0, 40, 0, 50);
    q.seq(0, 0, mBars({"A1 A1 A1 A1 | A1 A1 A1 A1 | A1 A1 A1 A1 | A1 A1 A2 A1", "F1 F1 F1 F1 | F1 F1 F1 F1 | F1 F1 F1 F1 | F1 F1 F2 F1",
                       "C2 C2 C2 C2 | C2 C2 C2 C2 | C2 C2 C2 C2 | C2 C2 C3 C2", "G1 G1 G1 G1 | G1 G1 G1 G1 | E1 E1 E1 E1 | E1 E1 ~G1 E1"}));
    q.cyc(0, 0, tpVOL, {100, 76, 88, 76});
    q.seq(0, 1, mBars({"C4 - - - | - - - - | - - - - | - - - -", "A3 - - - | - - - - | - - - - | - - - -",
                       "G3 - - - | - - - - | - - - - | - - - -", "B3 - - - | - - - - | G#3 - - - | - - - -"}));
    q.seq(0, 2, mBars({"E4 - - - | - - - - | D4 - - C4 | - - D4 -", "C4 - - - | - - - - | A3 - - - | - - . .",
                       "E4 - - - | G4 - - - | ~A4 - - G4 | - - E4 -", "D4 - - - | - - - - | B3 - - - | - - . ."}));
    q.seq(0, 3, mBars({". . . . | . . . . | . . . . | . . . E5", ". . . . | . . . . | . . . . | . . . C5",
                       ". . . . | . . . . | . . . . | . . . G5", ". . . . | . . . . | . . . . | . . B4 ."}));
    q.seq(0, 4, mBars({". . A3 . | . . A3 . | . . A3 . | . . A3 .", ". . F3 . | . . F3 . | . . F3 . | . . F3 .",
                       ". . C3 . | . . C3 . | . . C3 . | . . C3 .", ". . G3 . | . . G3 . | . . E3 . | . . E3 ."}));
    q.cond(0, 4, {14, 30, 46, 62}, 3);
    q.seq(0, 5, mRep(". . . . | x . . . | . . . . | x . . .", 4));
    b.settings(0, 64, 50);
    q.lfoS(1, 0, ls4Bars, 30, 1, 0, tpCUTOFF);
    q.send(1, 30, 70, 50); q.send(2, 50, 50); q.send(3, 40, 60); q.send(4, 40, 30); q.send(5, 0, 90);
    q.pan(2, 60); q.pan(3, 90); q.pan(4, 40);
    q.echo(d8D, 50, emBbd, 50, 62); q.verb(100, 50, rmPlate, 50, 72, 20);
    b.g(gTEMPO, 100);
    q.vol(0, 96); q.vol(1, 72); q.vol(2, 80); q.vol(3, 64); q.vol(4, 66); q.vol(5, 50);
    q.off(6); q.off(7);
    q.mix({11.6f, -8.9f, -2.2f, 8.7f, 17.5f, 29.8f});
}

// ======================================================================== ELECTRO / FUNK
static void m2ElectroFunk(PB& b)
{
    PB2 q(b);
    b.kit(0, mPULSE, {40, 0, 0, 0, 0, 80, 0, 0}); b.amp(0, 0, 0, 40, 30, 20); b.flt2(0, 50, 60, 0, 0, 0, 40, 100); b.fx(0, tpPORT, 20); b.fx(0, tpDIST, 20); b.legato(0);
    b.kit(1, mSAW, {0, 64, 70, 40, 64, 64, 0, 0}); b.amp(1, 0, 0, 30, 0, 30); b.flt2(1, 62, 60, 0, 20, 0, 30, 100);
    b.kit(2, mVOX, {40, 50, 10, 80, 0, 64, 70, 0}); b.amp(2, 0, 0, 50, 80, 30); b.fx(2, tpDIST, 30); b.fx(2, tpPORT, 30); b.legato(2);
    b.kit(3, mFM, {32, 50, 64, 20, 0, 0, 30, 30}); b.amp(3, 0, 0, 30, 0, 20);
    b.kit(4, mNOISE, {110, 50, 0, 20, 0, 0, 0, 0}); b.amp(4, 0, 0, 25, 0, 20); b.flt2(4, 127, 30, 0, 72, 0, 50, 64);
    q.seq(0, 0, mBars({"E1 . . E1 | . . ~G1 . | E1 . E2 . | ~D2 . B0 .", "E1 . . E1 | . . ~G1 . | A1 . ~B1 . | D2 . B1 .",
                       "C1 . . C1 | . . ~E1 . | C1 . C2 . | ~Bb1 . G0 .", "D1 . . D1 | . . ~F#1 . | A1 . ~D2 . | ~E2 . D2 ~B1"}));
    q.cyc(0, 0, tpFENV, {100, 92, 108, 96});
    q.seq(0, 1, mRep(". . . . | E3 . . . | . . . . | E3 . . E3", 2) + " | . . . . | C3 . . . | . . . . | C3 . . C3 | . . . . | D3 . . . | . . D3 . | . . . .");
    q.cond(0, 1, {15, 31}, 3);
    q.seq(0, 2, mBars({". . . . | . . . . | . . . . | . . . .", "E3 - - G3 | - - E3 - | ~D3 - - - | . . . .",
                       ". . . . | . . . . | . . . . | . . . .", "C3 - - D3 | - - E3 - | ~G3 - - - | E3 - . ."}));
    q.lfoS(2, 0, ls4, 60, 0, 1, 0);
    q.seq(0, 3, mRep(". . . x | . . . . | . . x . | . . . .", 4)); q.cyc(0, 3, 0, {32, 40, 24});
    q.seq(0, 4, mRep("x . x x | x . x . | x . x x | x . x x", 4)); q.cyc(0, 4, tpVOL, {72, 50, 60, 50});
    b.ratch(0, 4, 31, 3); b.ratch(0, 4, 63, 4);
    b.settings(0, 64, 54);
    q.send(1, 40, 10); q.send(2, 50, 30); q.send(3, 30, 30); q.send(4, 0, 10);
    q.pan(1, 40); q.pan(2, 70); q.pan(3, 90); q.pan(4, 54);
    q.echo(d16D, 45, emPingPong, 40, 56); q.verb(50, 60, rmRoom, 50, 50);
    b.g(gCMP_THR, 80); b.g(gCMP_RAT, 30); b.g(gCMP_ATK, 50); b.g(gCMP_REL, 40);   // glue
    b.g(gTEMPO, 118);
    q.vol(0, 100); q.vol(1, 76); q.vol(2, 74); q.vol(3, 66); q.vol(4, 60);
    q.off(5); q.off(6); q.off(7);
    q.mix({-3.0f, 29.2f, -10.8f, 10.0f, 5.8f});
}

static void m2Chiptune(PB& b)
{
    PB2 q(b);
    b.kit(0, mPULSE, {32, 0, 0, 0, 0, 0, 0, 40}); b.amp(0, 0, 0, 40, 90, 10); b.flt2(0, 127, 0, 0, 0, 0, 50, 64);
    b.kit(1, mPULSE, {64, 0, 0, 0, 0, 0, 0, 50}); b.amp(1, 0, 0, 50, 80, 15); b.lfo(1, 1, 80, 12, 1, 1, PITCH); b.fx(1, tpPORT, 10);
    b.kit(2, mPULSE, {16, 0, 0, 0, 0, 0, 0, 50}); b.amp(2, 0, 0, 30, 0, 10); b.arp(2, 1, 1, 0, 1);
    b.kit(3, mNOISE, {127, 0, 0, 60, 0, 0, 0, 0}); b.amp(3, 0, 0, 20, 0, 10);
    q.seq(0, 0, mBars({"C2 . C3 . | C2 . C3 . | C2 . C3 . | C2 . C3 .", "A1 . A2 . | A1 . A2 . | A1 . A2 . | A1 . A2 .",
                       "F1 . F2 . | F1 . F2 . | G1 . G2 . | G1 . G2 .", "C2 . C3 . | C2 . C3 . | G1 . G2 . | B1 . ~D2 ."}));
    q.seq(0, 1, mBars({"E4 - G4 - | C5 - - - | B4 - G4 - | E4 - - -", "A4 - - - | C5 - E5 - | D5 - C5 - | A4 - - -",
                       "F4 - A4 - | C5 - - - | D5 - B4 - | G4 - - -", "E4 - G4 - | C5 - E5 - | D5 - - - | B4 - D5 -"}));
    b.ratch(0, 1, 60, 3);
    q.seq(0, 2, mBars({"C4 - - - | - - - - | - - - - | - - - -", "A3 - - - | - - - - | - - - - | - - - -",
                       "F3 - - - | - - - - | G3 - - - | - - - -", "C4 - - - | - - - - | G3 - - - | - - - -"}));
    q.seq(0, 3, mRep("x . X . | x . X x | x . X . | x x X x", 4)); q.cyc(0, 3, 0, {127, 90, 110, 70});
    b.settings(0, 64, 50);
    q.send(1, 30, 20); q.send(2, 20, 10);
    q.pan(1, 56); q.pan(2, 76);
    q.echo(d8, 30, emDigital, 0, 40); q.verb(30, 60, rmRoom, 30, 30);
    b.g(gTEMPO, 140);
    q.vol(0, 90); q.vol(1, 84); q.vol(2, 68); q.vol(3, 66);
    q.off(4); q.off(5); q.off(6); q.off(7);
    q.mix({6.2f, -3.0f, 9.6f, 9.8f});
}

// ======================================================================== MINIMAL / POLYMETER
// Minimal Plucks 124: three plucked lines of 16, 13 and 11 steps drift against each other, FM bass, a sweeping pad.
static void m2MinimalPlucks(PB& b)
{
    PB2 q(b);
    b.kit(0, mFM, {16, 40, 64, 0, 0, 0, 40, 20}); b.amp(0, 0, 0, 50, 50, 30);
    b.kit(1, mPLUCK, {70, 100, 40, 30, 0, 30, 40, 0}); b.amp(1, 0, 0, 90, 100, 60);
    b.kit(2, mPLUCK, {60, 90, 40, 40, 40, 60, 40, 0}); b.amp(2, 0, 0, 80, 100, 40);
    b.kit(3, mPLUCK, {40, 120, 15, 20, 90, 90, 0, 0}); b.amp(3, 0, 0, 60, 100, 30);
    b.kit(4, mWAVE, {50, 5, 50, 64, 66, 30, 0, 0}); b.amp(4, 80, 0, 100, 100, 100);
    b.kit(5, mNOISE, {127, 70, 0, 0, 0, 0, 0, 0}); b.amp(5, 0, 0, 15, 0, 10); b.flt2(5, 127, 20, 0, 72, 0, 50, 64);
    q.seq(0, 0, mBars({"G1 . . G1 | . . G2 . | . G1 . . | ~Bb1 . . .", "G1 . . G1 | . . G2 . | . G1 . . | ~F1 . . .",
                       "Eb1 . . Eb1 | . . Eb2 . | . Eb1 . . | ~F1 . . .", "G1 . . G1 | . . G2 . | . G1 ~Bb1 . | C2 . ~D2 ."}));
    q.seq(0, 1, "G3 . D4 . Bb3 . . G4 . F4 . D4 . . . .");
    q.seq(0, 2, "D4 . . G4 . . Bb4 . . F4 . . C5");
    q.cyc(0, 2, 0, {50, 70, 60});
    q.seq(0, 3, "G5 . . . D5 . . . Bb4 . .");
    q.cond(0, 3, {4, 8}, 3);
    q.seq(0, 4, mBars({"G3 - - - | - - - - | - - - - | - - - -", "D3 - - - | - - - - | - - - - | - - - -",
                       "Eb3 - - - | - - - - | - - - - | - - - -", "F3 - - - | - - - - | - - - - | - - - -"}));
    q.seq(0, 5, mRep(". . x . | . . x . | . . x . | . x x .", 4));
    b.settings(0, 64, 56);
    q.lfoS(1, 0, ls2Bars, 40, 1, 0, 1);
    q.lfoS(4, 0, ls8Bars, 50, 0, 0, tpCUTOFF); b.flt2(4, 70, 30, 0, 20, 0, 50, 64);
    q.lfoS(2, 0, ls4Bars, 50, 1, 0, tpPAN);
    q.send(1, 70, 30); q.send(2, 50, 40); q.send(3, 60, 50); q.send(4, 0, 90, 30);
    q.pan(1, 40); q.pan(3, 94); q.pan(5, 70);
    q.echo(d16D, 60, emMulti, 50, 60); b.g(gECHO_WIDTH, 120); q.verb(70, 60, rmPlate, 40, 60);
    b.g(gCMP_THR, 80); b.g(gCMP_RAT, 30); b.g(gCMP_ATK, 50); b.g(gCMP_REL, 40);   // glue
    b.g(gTEMPO, 124);
    q.vol(0, 100); q.vol(1, 74); q.vol(2, 68); q.vol(3, 60); q.vol(4, 60); q.vol(5, 58);
    q.off(6); q.off(7);
    q.mix({2.2f, -0.5f, 6.7f, 26.8f, -0.4f, 10.9f});
}

// Chaos Arps 128: two arpeggiators (UP-DOWN and RANDOM) through CHAOS, which repeats and pitches them; bass stays dry.
static void m2ChaosArps(PB& b)
{
    PB2 q(b);
    b.kit(0, mSAW, {0, 30, 70, 50, 32, 64, 70, 0}); b.amp(0, 0, 0, 40, 40, 20); b.flt2(0, 48, 40, 0, 0, 0, 40, 86);
    b.kit(1, mSAW, {0, 64, 72, 64, 96, 50, 0, 0}); b.amp(1, 0, 0, 40, 0, 40); b.flt2(1, 76, 50, 0, 30, 0, 40, 84); b.arp(1, 3, 2, 0, 5);
    b.kit(2, mWAVE, {70, 90, 100, 64, 64, 0, 70, 20}); b.amp(2, 0, 0, 30, 0, 30); b.arp(2, 4, 2, 0, 4);
    b.kit(3, mSUPER, {35, 80, 110, 0, 0, 64, 0, 0}); b.amp(3, 90, 0, 90, 110, 100); b.flt2(3, 64, 20, 0, 20, 0, 50, 64);
    q.seq(0, 0, mBars({"D1 . D2 . | D1 . . D1 | . D1 D2 . | C2 . ~D2 .", "Bb0 . Bb1 . | Bb0 . . Bb0 | . Bb0 Bb1 . | A1 . ~Bb1 .",
                       "G0 . G1 . | G0 . . G0 | . G0 G1 . | F1 . ~G1 .", "A0 . A1 . | A0 . . A0 | . A0 A1 . | C2 . ~A1 ."}));
    q.seq(0, 1, mBars({"D3 - - - | - - - - | - - - - | - - - -", "Bb2 - - - | - - - - | - - - - | - - - -",
                       "G2 - - - | - - - - | - - - - | - - - -", "A2 - - - | - - - - | - - - - | - - - -"}));
    q.seq(0, 2, mBars({". . . . | . . . . | D4 - - - | - - - -", ". . . . | . . . . | F4 - - - | - - - -",
                       ". . . . | . . . . | D4 - - - | - - - -", ". . . . | . . . . | E4 - - - | - - - -"}));
    q.seq(0, 3, mBars({"F3 - - - | - - - - | - - - - | - - - -", "D3 - - - | - - - - | - - - - | - - - -",
                       "Bb2 - - - | - - - - | - - - - | - - - -", "C#3 - - - | - - - - | - - - - | - - - -"}));
    b.settings(0, 64, 52);
    q.lfoS(1, 0, ls2Bars, 50, 0, 0, tpCUTOFF);
    b.g(gGL_CHANCE, 70); b.g(gGL_SIZE, 60); b.g(gGL_LEN, 40); b.g(gGL_PITCH, 90); b.g(gGL_REV, 40); b.g(gGL_MIX, 110);
    for (int t = 0; t < kTracks; ++t) q.chaos(t, 0);
    q.chaos(1, 127); q.chaos(2, 127);
    q.send(1, 60, 30); q.send(2, 60, 40); q.send(3, 0, 90, 40);
    q.pan(1, 34); q.pan(2, 94);
    q.echo(d8D, 55, emPingPong, 40, 60); q.verb(80, 60, rmHall, 40, 66);
    b.g(gCMP_THR, 80); b.g(gCMP_RAT, 30); b.g(gCMP_ATK, 50); b.g(gCMP_REL, 40);   // glue
    b.g(gTEMPO, 128);
    q.vol(0, 98); q.vol(1, 74); q.vol(2, 70); q.vol(3, 62);
    q.off(4); q.off(5); q.off(6); q.off(7);
    q.mix({12.5f, -1.9f, 2.5f, -6.2f});
}

// ======================================================================== AMBIENT / KEYS
static void m2Ambient(PB& b)
{
    PB2 q(b);
    b.kit(0, mWAVE, {60, 8, 60, 64, 66, 60, 0, 0}); b.amp(0, 100, 0, 100, 110, 110); b.flt2(0, 70, 10, 0, 20, 0, 50, 64);
    b.kit(1, mSUPER, {25, 70, 127, 0, 0, 64, 0, 0}); b.amp(1, 110, 0, 110, 110, 115); b.flt2(1, 62, 10, 0, 10, 0, 50, 64);
    b.kit(2, mVOX, {30, 64, 30, 60, 20, 50, 30, 80}); b.amp(2, 100, 0, 100, 110, 110);
    b.kit(3, mFM, {32, 30, 112, 40, 0, 3, 90, 0}); b.amp(3, 0, 0, 90, 0, 100);
    b.kit(4, mPLUCK, {110, 60, 70, 60, 10, 30, 50, 0}); b.amp(4, 0, 0, 100, 110, 100);
    b.kit(5, mNOISE, {50, 30, 0, 0, 30, 0, 60, 0}); b.amp(5, 110, 0, 110, 110, 115);
    b.kit(6, mSAW, {127, 127, 64, 0, 64, 64, 90, 0}); b.amp(6, 60, 0, 90, 100, 100); b.flt2(6, 34, 10, 0, 0, 0, 50, 64);
    q.seq(0, 0, mBars({"D3 - - - | - - - - | - - - - | - - - -", "F3 - - - | - - - - | - - - - | - - - -",
                       "C3 - - - | - - - - | - - - - | - - - -", "A2 - - - | - - - - | - - - - | - - - -"}));
    q.seq(0, 1, mBars({"A3 - - - | - - - - | - - - - | - - - -", "C4 - - - | - - - - | - - - - | - - - -",
                       "G3 - - - | - - - - | - - - - | - - - -", "E3 - - - | - - - - | - - - - | - - - -"}));
    q.seq(0, 2, mBars({". . . . | . . . . | F4 - - - | - - - -", ". . . . | . . . . | A4 - - - | - - - -",
                       ". . . . | . . . . | E4 - - - | - - - -", ". . . . | . . . . | C#4 - - - | - - - -"}));
    q.seq(0, 3, mBars({". . . . | . . A5 . | . . . . | . . . .", ". . . . | . . . . | . . C6 . | . . . .",
                       ". . . . | G5 . . . | . . . . | . . E5 .", ". . . . | . . . . | . . . . | . . . ."}));
    q.cond(0, 3, {6, 26, 36, 46}, 3);
    q.seq(0, 4, mBars({"D5 . . . | . . . . | . . F5 . | . . . .", ". . . . | A4 . . . | . . . . | . . C5 .",
                       ". . E5 . | . . . . | . . . . | G4 . . .", ". . . . | . . . . | E5 . . . | . . . ."}));
    q.cond(0, 4, {0, 10, 20, 30, 34, 44, 56}, 2);
    q.seq(0, 5, "x - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -");
    q.seq(0, 6, mBars({"D1 - - - | - - - - | - - - - | - - - -", "Bb0 - - - | - - - - | - - - - | - - - -",
                       "C1 - - - | - - - - | - - - - | - - - -", "A0 - - - | - - - - | - - - - | - - - -"}));
    b.settings(0, 64, 50);
    q.lfoS(0, 0, ls4Bars, 40, 1, 0, 0); q.lfoS(1, 0, ls8Bars, 30, 1, 0, tpCUTOFF); q.lfoS(2, 0, ls2Bars, 50, 1, 0, 0);
    q.lfoS(5, 0, ls4Bars, 70, 1, 0, 0); q.lfoS(4, 0, ls1Bar, 40, 1, 0, tpPAN);
    q.send(0, 0, 100, 40); q.send(1, 0, 110, 40); q.send(2, 30, 110); q.send(3, 70, 90); q.send(4, 80, 90); q.send(5, 0, 100);
    q.echo(d4, 75, emDiffuse, 70, 66); q.verb(127, 70, rmShimmer, 60, 90, 40);
    b.g(gCHO_MODEL, 50); b.g(gCHO_LEV, 60);
    b.g(gTEMPO, 80);
    q.vol(0, 80); q.vol(1, 70); q.vol(2, 70); q.vol(3, 66); q.vol(4, 70); q.vol(5, 50); q.vol(6, 86);
    q.off(7);
    q.mix({6.0f, -6.9f, 1.7f, 11.9f, 33.9f, 1.7f, 12.4f});
}

// Lo-Fi Keys 84: FM electric piano chords with a tremolo LFO, plucked bass, wobbling (PITCH LFO) organ, vinyl hiss.
static void m2LofiKeys(PB& b)
{
    PB2 q(b);
    b.kit(0, mPLUCK, {30, 50, 70, 70, 0, 40, 70, 30}); b.amp(0, 0, 0, 60, 70, 30); b.flt2(0, 64, 20, 0, 0, 0, 50, 64);
    b.kit(1, mFM, {32, 35, 32, 30, 0, 2, 50, 30}); b.amp(1, 10, 0, 70, 70, 60);
    b.kit(2, mFM, {32, 35, 32, 30, 0, 2, 50, 30}); b.amp(2, 10, 0, 70, 70, 60);
    b.kit(3, mCHORD, {60, shapeV2(csMin9), 30, 0, 0, 20, 10, 60}); q.table(3, 3); b.amp(3, 30, 0, 80, 90, 70); b.flt2(3, 86, 10, 0, 20, 0, 50, 64);
    b.kit(4, mNOISE, {60, 127, 127, 50, 60, 0, 90, 0}); b.amp(4, 0, 0, 127, 127, 60);
    b.kit(5, mPULSE, {50, 30, 20, 0, 0, 0, 0, 40}); b.amp(5, 5, 0, 60, 80, 60); b.fx(5, tpPORT, 30); b.legato(5); b.fx(5, tpSRR, 40);
    q.seq(0, 0, mBars({"D2 . . . | . . . D2 | . . A1 . | . . . .", "G1 . . . | . . . G1 | . . D2 . | . . . .",
                       "C2 . . . | . . . C2 | . . G1 . | . . . .", "A1 . . . | . . . A1 | . ~C2 . . | E2 . . ."}));
    q.seq(0, 1, mBars({"F3 - - - | - - - . | . . . . | . . F3 .", "F3 - - - | - - - . | . . . . | . . . .",
                       "E3 - - - | - - - . | . . . . | . . E3 .", "E3 - - - | - - - . | G3 - - - | - - . ."}));
    q.seq(0, 2, mBars({"C4 - - - | - - - . | . . . . | . . C4 .", "B3 - - - | - - - . | . . . . | . . . .",
                       "B3 - - - | - - - . | . . . . | . . B3 .", "C#4 - - - | - - - . | B3 - - - | - - . ."}));
    q.micro(0, 1, {0, 16, 32, 48}, 2); q.micro(0, 2, {0, 16, 32, 48}, 4);
    q.seq(0, 3, mBars({". . . . | . . . . | . . . . | . . . .", "G3 - - - | - - - - | - - - - | - - - -",
                       ". . . . | . . . . | . . . . | . . . .", "A3 - - - | - - - - | - - - - | - - - -"}));
    q.seq(0, 4, "x - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -");
    q.seq(0, 5, mBars({". . . . | . . . . | . . . . | . . . .", ". . . . | . . . . | . . . . | . . . .",
                       "E4 - - - | G4 - - - | ~A4 - - - | G4 - E4 -", "D4 - - - | - - - - | C4 - - - | - - . ."}));
    b.settings(0, 64, 62);
    q.lfoS(1, 0, ls8, 30, 1, 0, tpVOL); q.lfoS(2, 0, ls8, 30, 1, 0, tpVOL);
    b.lfo(3, 0, 20, 8, 1, 0, PITCH);
    q.send(1, 30, 40, 30); q.send(2, 30, 40, 30); q.send(3, 0, 60, 40); q.send(5, 50, 40);
    q.pan(1, 44); q.pan(2, 84); q.pan(3, 60); q.pan(5, 70);
    q.echo(d8, 40, emLofi, 30, 50); q.verb(50, 80, rmRoom, 50, 50);
    b.g(gCMP_THR, 80); b.g(gCMP_RAT, 30); b.g(gCMP_ATK, 50); b.g(gCMP_REL, 40);   // glue
    b.g(gTEMPO, 84);
    q.vol(0, 96); q.vol(1, 78); q.vol(2, 74); q.vol(3, 64); q.vol(4, 40); q.vol(5, 74);
    q.off(6); q.off(7);
    q.mix({-0.2f, -8.6f, -9.1f, -2.5f, -5.2f, -7.1f});
}

// Full Band 118: all eight tracks: bass, chords, pad, lead, arp, bells, choir and pluck, with a B pattern for the chorus.
static void m2FullBand(PB& b)
{
    PB2 q(b);
    b.kit(0, mSAW, {0, 30, 70, 50, 32, 64, 70, 0}); b.amp(0, 0, 0, 50, 60, 25); b.flt2(0, 45, 40, 0, 0, 0, 45, 90); b.fx(0, tpPORT, 20);
    b.kit(1, mCHORD, {20, shapeV2(csMin7), 40, 0, 20, 30, 10, 80}); b.amp(1, 0, 0, 58, 36, 40); b.flt2(1, 89, 25, 0, 20, 0, 45, 70);
    b.kit(2, mSUPER, {50, 100, 127, 0, 0, 64, 0, 10}); b.amp(2, 80, 0, 90, 110, 100); b.flt2(2, 70, 10, 0, 20, 0, 50, 64);
    b.kit(3, mWAVE, {30, 30, 40, 64, 70, 30, 10, 0}); q.table(3, 4); b.amp(3, 5, 0, 60, 80, 50); b.flt2(3, 90, 20, 0, 20, 0, 50, 64);
    b.fx(3, tpPORT, 25); b.legato(3); b.lfo(3, 1, 72, 10, 1, 0, PITCH);
    b.kit(4, mPLUCK, {65, 100, 40, 40, 0, 40, 30, 0}); b.amp(4, 0, 0, 70, 100, 50); b.arp(4, 1, 2, 0, 3);
    b.kit(5, mFM, {48, 50, 112, 30, 0, 3, 60, 0}); b.amp(5, 0, 0, 70, 0, 80);
    b.kit(6, mVOX, {30, 50, 30, 60, 20, 50, 30, 30}); b.amp(6, 70, 0, 90, 100, 90);
    b.kit(7, mPLUCK, {75, 90, 40, 40, 0, 40, 30, 0}); b.amp(7, 0, 0, 90, 100, 60);
    q.seq(0, 0, mBars({"A1 . A2 A1 | . A1 . G1 | F1 . F2 F1 | . F1 . E1", "C2 . C3 C2 | . C2 . D2 | G1 . G2 G1 | . G1 ~A1 .",
                       "A1 . A2 A1 | . A1 . G1 | F1 . F2 F1 | . F1 . E1", "C2 . C3 C2 | . C2 . D2 | G1 . G2 G1 | ~B1 . ~C2 ~D2"}));
    q.seq(0, 1, mBars({". . A3 . | . . . . | . . F3 . | . . . .", ". . C4 . | . . . . | . . G3 . | . . . .",
                       ". . A3 . | . . . A3 | . . F3 . | . . . F3", ". . C4 . | . . . C4 | . . G3 . | . G3 . ."}));
    q.cyc(0, 1, 1, {shapeV2(csMin7), shapeV2(csMaj7), shapeV2(csMaj9), shapeV2(csDom7)});
    q.seq(0, 2, mBars({"A2 - - - | - - - - | F2 - - - | - - - -", "C3 - - - | - - - - | G2 - - - | - - - -",
                       "A2 - - - | - - - - | F2 - - - | - - - -", "C3 - - - | - - - - | G2 - - - | - - - -"}));
    q.seq(0, 3, mBars({". . . . | . . . . | . . . . | . . . .", "E4 - - G4 | - - A4 - | ~G4 - - E4 | - D4 - -",
                       ". . . . | . . . . | . . . . | . . . .", "E4 - - G4 | - - A4 - | ~C5 - - B4 | - G4 - -"}));
    q.seq(0, 4, mBars({"A3 - - - | - - - - | F3 - - - | - - - -", "C4 - - - | - - - - | G3 - - - | - - - -",
                       "A3 - - - | - - - - | F3 - - - | - - - -", "C4 - - - | - - - - | G3 - - - | - - - -"}));
    q.seq(0, 5, mRep(". . . . | . . . . | . . . . | . . . E5", 3) + " | . . . . | . . . . | . . . . | . . D5 B4");
    q.seq(0, 6, mBars({"E4 - - - | - - - - | C4 - - - | - - - -", "E4 - - - | - - - - | D4 - - - | - - - -",
                       "E4 - - - | - - - - | C4 - - - | - - - -", "E4 - - - | - - - - | D4 - - - | - - - -"}));
    q.seq(0, 7, mRep(". . . . | A4 . . . | . . . . | F4 . . .", 4)); q.cond(0, 7, {4, 36}, 3);
    b.settings(0, 64, 52);
    q.copy(0, 1);
    q.seq(1, 3, mBars({"A4 - - - | G4 - - - | ~A4 - C5 - | - - A4 -", "G4 - - - | E4 - - - | ~D4 - - - | - - - -",
                       "A4 - - - | G4 - - - | ~A4 - C5 - | - - D5 -", "E5 - - - | - - D5 - | ~C5 - - B4 | - - - -"}));
    q.cyc(1, 2, tpCUTOFF, {86});
    b.settings(1, 64, 52);
    q.send(1, 50, 30); q.send(2, 0, 90, 40); q.send(3, 45, 30); q.send(4, 60, 30); q.send(5, 0, 70); q.send(6, 0, 80); q.send(7, 50, 20);
    q.pan(1, 44); q.pan(3, 60); q.pan(4, 26); q.pan(5, 100); q.pan(6, 70); q.pan(7, 84);
    q.echo(d8D, 50, emPingPong, 30, 64); q.verb(90, 60, rmHall, 40, 70);
    q.chain("1 2 0:2 1:2");
    b.g(gTEMPO, 118);
    q.vol(0, 96); q.vol(1, 80); q.vol(2, 64); q.vol(3, 82); q.vol(4, 70); q.vol(5, 64); q.vol(6, 60); q.vol(7, 66);
    q.mix({18.7f, 10.7f, -4.9f, -2.2f, 3.7f, 12.0f, 4.8f, 36.0f});
}

static void m2Demo(PB& b)
{
    PB2 q(b);
    b.kit(0, mSAW, {0, 40, 70, 50, 32, 64, 60, 0}); b.amp(0, 0, 0, 55, 60, 30); b.flt2(0, 58, 40, 0, 0, 0, 50, 90); b.fx(0, tpPORT, 30);
    b.kit(1, mPULSE, {40, 40, 30, 0, 0, 0, 0, 0}); b.amp(1, 0, 0, 40, 0, 40); b.flt2(1, 90, 30, 0, 10, 0, 45, 80);
    b.kit(2, mSUPER, {55, 90, 127, 0, 0, 64, 0, 10}); b.amp(2, 60, 0, 80, 100, 80); b.flt2(2, 82, 10, 0, 0, 0, 50, 64);
    q.seq(0, 0, "C2 . C2 C3 | . C2 ~D#2 . | C2 . G1 . | Bb1 . ~C2 . | C2 . C2 C3 | . C2 ~D#2 . | Ab1 . Ab1 . | Bb1 . ~B1 .");
    q.seq(0, 1, ". . . . | !G3 . . . | . . . . | !G3 . . F3 | . . . . | !G3 . . . | . . . . | !Ab3 . G3 .");
    q.seq(0, 2, "C3 - - - | - - - - | - - - - | - - - - | Ab2 - - - | - - - - | - - - - | - - - -");
    b.settings(0, 32, 54);
    q.send(1, 50, 20); q.send(2, 0, 70);
    q.pan(1, 44);
    b.g(gCMP_THR, 80); b.g(gCMP_RAT, 30); b.g(gCMP_ATK, 50); b.g(gCMP_REL, 40);   // glue
    b.g(gTEMPO, 116);
    q.vol(0, 96); q.vol(1, 80); q.vol(2, 72);
    for (int t = 3; t < kTracks; ++t) q.off(t);
    q.mix({1.0f, 25.7f, -10.9f});
}
