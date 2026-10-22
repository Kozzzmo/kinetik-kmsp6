// Kinetik KMSP-6 — factory presets (kit + patterns). All original material.
#pragma once
#include "Random.hpp"
#include <cstring>
#include <algorithm>
#include <cctype>
#include <initializer_list>

namespace kn {

inline int shapeVal(int i) { return (i * 128 + 5) / 6; }      // LFO shape index -> value
inline int typeVal(int i) { return (i * 128 + 2) / 3; }       // LFO type index -> value
static constexpr int PITCH = kLfoDests;                       // LFO destination: pitch

// Convert the parameters of one track saved with the first filter layout (KMSP-6 0.1):
// old 16..23 = BASE WDTH HPQ LPQ FATK FDEC BOF WOF, key tracking was the kit field KEYTRK (0..4).
inline void migrateFilterV1(float* trackParams, int oldKeyTrk)
{
    float* f = trackParams + 16;
    const float base = f[0], width = f[1], lpq = f[3], fatk = f[4], fdec = f[5], bof = f[6], wof = f[7];
    f[tpCUTOFF - 16] = std::min(127.f, base + width);
    f[tpRESO - 16] = lpq;
    f[tpFTYPE - 16] = 0.f;
    f[tpLOWCUT - 16] = base;
    f[tpFATK - 16] = fatk; f[tpFDEC - 16] = fdec;
    f[tpFENV - 16] = std::max(0.f, std::min(127.f, 64.f + (bof - 64.f) + (wof - 64.f)));
    f[tpKEYTRK - 16] = (float)std::min(127, oldKeyTrk * 32);
}
// parameter index of a "V <index> <value>" line written by a KMSP-6 0.1 preset (50 params per track)
inline int presetIndexV1(int idx)
{
    constexpr int oldTP = 50, oldGB = kTracks * oldTP;
    if (idx < oldGB) return (idx / oldTP) * kTrackParams + idx % oldTP;
    return kGlobalBase + (idx - oldGB);
}

struct PB { // preset builder
    float* v; Bank& b;
    PB(float* values, Bank& bank) : v(values), b(bank)
    {
        b.initDefault();
        for (int t = 0; t < kTracks; ++t)
            for (int p = 0; p < kTrackParams; ++p) tparam(v, t, p) = trackParamDefault(t, p);
        for (int i = 0; i < kMasterCount; ++i) v[kGlobalBase + gECHO_TIME + i] = kMasterDefaults[i];
        v[kGlobalBase + gTEMPO] = 120;
    }
    // machine + synth params (-1 keeps the machine default)
    void kit(int t, int m, std::initializer_list<int> syn = {})
    {
        b.machine[t] = (uint8_t)m;
        for (int p = 0; p < 8; ++p) tparam(v, t, p) = kMachineDefs[m].def[p];
        int p = 0;
        for (int x : syn) { if (x >= 0) tparam(v, t, p) = (float)x; if (++p >= 8) break; }
    }
    void amp(int t, int atk, int hold, int dec, int sus, int rel)
    {
        tparam(v, t, tpATK) = (float)atk; tparam(v, t, tpHOLD) = (float)hold; tparam(v, t, tpDEC) = (float)dec;
        tparam(v, t, tpSUS) = (float)sus; tparam(v, t, tpREL) = (float)rel;
    }
    // presets written for the first filter layout (low edge, width, two resonances, env -> edges),
    // converted to CUTOFF / RESO / TYPE / LOW CUT / ENV ATK / ENV DEC / ENV AMT
    void flt(int t, int base, int width, int /*hpq*/, int lpq, int fatk, int fdec, int bof, int wof)
    {
        flt2(t, clampi(base + width, 0, 127), lpq, 0, base, fatk, fdec, clampi(64 + (bof - 64) + (wof - 64), 0, 127));
    }
    void flt2(int t, int cutoff, int reso, int type, int lowcut, int fatk, int fdec, int envAmt)
    {
        tparam(v, t, tpCUTOFF) = (float)cutoff; tparam(v, t, tpRESO) = (float)reso;
        tparam(v, t, tpFTYPE) = (float)filterTypeValue(type); tparam(v, t, tpLOWCUT) = (float)lowcut;
        tparam(v, t, tpFATK) = (float)fatk; tparam(v, t, tpFDEC) = (float)fdec; tparam(v, t, tpFENV) = (float)envAmt;
    }
    void fx(int t, int p, int val) { tparam(v, t, p) = (float)val; }
    void lfo(int t, int l, int spd, int dep, int shape, int type, int dest, int trk = 0)
    {
        const int base = kLfoBase[l];
        tparam(v, t, base) = (float)spd; tparam(v, t, base + 1) = (float)dep;
        tparam(v, t, base + 2) = (float)shapeVal(shape); tparam(v, t, base + 3) = (float)typeVal(type);
        tparam(v, t, base + 4) = (float)lfoDestValue(dest); tparam(v, t, base + 5) = (float)lfoTrackValue(trk);
    }
    void arp(int t, int mode, int oct, int spd, int chord)
    {
        b.kit[t][kfArpMode] = (uint8_t)mode; b.kit[t][kfArpOct] = (uint8_t)oct;
        b.kit[t][kfArpSpd] = (uint8_t)spd; b.kit[t][kfArpChord] = (uint8_t)chord;
    }
    void legato(int t, bool on = true) { b.kit[t][kfLegato] = on ? 1 : 0; }
    void keytrk(int t, int v4) { tparam(v, t, tpKEYTRK) = (float)clampi(v4 * 32, 0, 127); }
    void g(int gp, int val) { v[kGlobalBase + gp] = (float)val; }

    static int parseNote(const char*& c)
    {
        static const int semis[7] = {9, 11, 0, 2, 4, 5, 7}; // A B C D E F G
        const char L = (char)std::toupper((unsigned char)*c);
        if (L < 'A' || L > 'G') return -1;
        int n = semis[L - 'A']; ++c;
        if (*c == '#') { ++n; ++c; } else if (*c == 'b') { --n; ++c; }
        int oct = 0, sign = 1;
        if (*c == '-') { sign = -1; ++c; }
        while (*c >= '0' && *c <= '9') { oct = oct * 10 + (*c - '0'); ++c; }
        return clampi(n + 12 * (sign * oct + 2), 0, 127);  // C3 = 60 (Ableton)
    }
    // tokens: note "C2" "D#3" "Bb1" (x / X = hit on C5), prefix '!' accent, '~' slide into it; "." rest; "-" tie (longer gate).
    // "|" is ignored (visual bar separator).
    void seq(int pat, int t, const char* s)
    {
        Pattern& P = b.patterns[pat];
        int st = 0, last = -1, tie = 0;
        auto closeNote = [&]() {
            if (last < 0) return;
            if (tie > 0) {
                const float want = tie + 1.f - 0.15f;  // steps
                int best = 0;
                for (int gi = 1; gi < kGateCount; ++gi)
                    if (std::fabs(kGateSteps[gi] - want) < std::fabs(kGateSteps[best] - want)) best = gi;
                P.gate[t][last] = (uint8_t)best;
            }
            last = -1; tie = 0;
        };
        const char* c = s;
        while (*c && st < kSteps) {
            if (*c == ' ' || *c == '|') { ++c; continue; }
            if (*c == '.') { closeNote(); P.flags[t][st++] = 0; ++c; continue; }
            if (*c == '-') { if (last >= 0) ++tie; P.flags[t][st++] = 0; ++c; continue; }
            uint8_t f = sfTrig;
            while (*c == '!' || *c == '~') { f |= *c == '!' ? sfAccent : sfSlide; ++c; }
            int n;
            if (*c == 'x' || *c == 'X') { if (*c == 'X') f |= sfAccent; n = 84; ++c; } // percussive hit
            else n = parseNote(c);
            if (n < 0) { ++c; continue; }
            closeNote();
            P.flags[t][st] = f; P.note[t][st] = (uint8_t)n; P.gate[t][st] = kDefaultGate;
            last = st++;
        }
        closeNote();
        P.trackLen[t] = (uint8_t)std::max(st, 1);
    }
    void gate(int pat, int t, int gi) // same gate on every trig of a track
    {
        for (int s = 0; s < kSteps; ++s) if (b.patterns[pat].flags[t][s] & sfTrig) b.patterns[pat].gate[t][s] = (uint8_t)gi;
    }
    void lock(int pat, int t, int step, int p, int val) { b.patterns[pat].locks[t][step][p] = (uint8_t)clampi(val, 0, 127); }
    void ratch(int pat, int t, int step, int n) { b.patterns[pat].ratch[t][step] = (uint8_t)n; }
    void cond(int pat, int t, int step, int c) { b.patterns[pat].cond[t][step] = (uint8_t)c; }
    void settings(int pat, int len, int swing = 50, int scale = 0, int accent = 64)
    {
        Pattern& P = b.patterns[pat];
        P.length = (uint8_t)len; P.swing = (uint8_t)swing; P.scale = (uint8_t)scale; P.accent = (uint8_t)accent;
        for (int t = 0; t < kTracks; ++t) {
            bool any = false;
            for (int st = 0; st < kSteps; ++st) any |= P.flags[t][st] != 0;
            if (!any) P.trackLen[t] = (uint8_t)len;
        }
    }
    void copyPattern(int dst, int src) { b.patterns[dst] = b.patterns[src]; }
};

// gate indices: 0 1/8, 1 1/4, 2 1/2, 3 3/4, 4 1, 5 1.5, 6 2, 7 3, 8 4, 9 6, 10 8, 11 16
// --------------------------------------------------------------------------------------------
static void presetInit(PB& b)
{
    (void)b; // default kit, base parameters, empty sequencer
}

static void presetDemo(PB& b)
{
    // bass, stab, pad, bell, wave lead, pluck
    b.kit(0, mSAW, {0, 40, 70, 50, 32, 64, 60, 0}); b.amp(0, 0, 0, 55, 60, 30); b.flt(0, 0, 58, 0, 70, 0, 50, 64, 100); b.fx(0, tpPORT, 30); b.fx(0, tpVOL, 92);
    b.kit(1, mPULSE, {40, 40, 30, 0, 0, 0, 0, 0}); b.amp(1, 0, 0, 40, 0, 40); b.flt(1, 10, 80, 0, 40, 0, 45, 64, 90);
    b.fx(1, tpDEL, 50); b.fx(1, tpPAN, 44);
    b.kit(2, mSUPER, {55, 90, 127, 0, 0, 64, 0, 10}); b.amp(2, 60, 0, 80, 100, 80); b.flt(2, 0, 82, 0, 20, 0, 50, 64, 64);
    b.fx(2, tpREV, 70); b.fx(2, tpVOL, 80); b.lfo(2, 0, 30, 30, 1, 0, tpCUTOFF);
    b.kit(3, mFM, {32, 60, 96, 30, 0, 0, 60, 10}); b.amp(3, 0, 0, 70, 0, 70); b.flt(3, 0, 127, 0, 0, 0, 50, 64, 64);
    b.fx(3, tpREV, 50); b.fx(3, tpDEL, 40); b.fx(3, tpPAN, 84); b.fx(3, tpVOL, 88);
    b.kit(4, mWAVE, {10, 30, 50, 64, 70, 30, 10, 0}); b.amp(4, 5, 0, 60, 80, 50); b.flt(4, 0, 96, 0, 30, 0, 50, 64, 70);
    b.lfo(4, 1, 72, 10, 1, 0, PITCH); b.fx(4, tpDEL, 40); b.fx(4, tpPORT, 25); b.legato(4); b.fx(4, tpVOL, 90);
    b.kit(5, mPLUCK, {75, 90, 40, 40, 0, 40, 30, 0}); b.amp(5, 0, 0, 90, 100, 60); b.fx(5, tpDEL, 60); b.fx(5, tpPAN, 90);
    b.g(gECHO_TIME, 86); b.g(gECHO_FB, 55); b.g(gREV_SIZE, 80);
    b.seq(0, 0, "C2 . C2 C3 | . C2 ~D#2 . | C2 . G1 . | Bb1 . ~C2 .");
    b.seq(0, 1, ". . . . | !G3 . . . | . . . . | !G3 . . F3");
    b.seq(0, 2, "C3 - - - | - - - - | Ab2 - - - | - - - -");
    b.seq(0, 3, ". . . . | . . . . | . . . . | . . . . | . . . . | . . . . | . . . . | . . G4 .");
    b.seq(0, 4, ". . . . | . . . . | . . . . | . . . . | C4 - - Eb4 | - - G4 - | ~F4 - - - | Eb4 - D4 -");
    b.seq(0, 5, "G4 . . C5 | . . D#5 . | . G4 . . | C5 . . .");
    b.settings(0, 32, 54);
    b.g(gTEMPO, 116);
}

static void presetAcid(PB& b)
{
    b.kit(0, mSAW, {0, 0, 64, 0, 64, 64, 0, 0}); b.amp(0, 0, 0, 45, 50, 20); b.flt(0, 0, 44, 0, 108, 0, 52, 64, 112);
    b.fx(0, tpDIST, 40); b.fx(0, tpPORT, 22); b.fx(0, tpDEL, 25); b.fx(0, tpVOL, 92);
    b.kit(1, mPULSE, {64, 0, 0, 0, 0, 30, 0, 0}); b.amp(1, 0, 0, 40, 40, 20); b.flt(1, 0, 48, 0, 100, 0, 46, 64, 108);
    b.fx(1, tpDIST, 30); b.fx(1, tpPORT, 22); b.fx(1, tpDEL, 40); b.fx(1, tpPAN, 80);
    b.lfo(1, 0, 18, 40, 0, 0, tpCUTOFF);
    b.kit(2, mNOISE, {110, 40, 0, 0, 0, 0, 0, 0}); b.amp(2, 0, 0, 25, 0, 20); b.flt(2, 60, 127, 0, 0, 0, 50, 64, 64);
    b.fx(2, tpVOL, 70);
    b.kit(3, mFM, {16, 40, 64, 0, 0, 0, 30, 0}); b.amp(3, 0, 0, 35, 0, 25); b.fx(3, tpREV, 30);
    b.kit(4, mSUPER, {40, 90, 100, 0, 0, 64, 0, 20}); b.amp(4, 40, 0, 70, 90, 70); b.flt(4, 0, 70, 0, 30, 0, 50, 64, 64);
    b.fx(4, tpREV, 70); b.fx(4, tpVOL, 70);
    b.kit(5, mWAVE, {80, 70, 60, 64, 64, 0, 40, 0}); b.amp(5, 0, 0, 50, 0, 40); b.fx(5, tpDEL, 70); b.fx(5, tpPAN, 40);
    b.g(gECHO_TIME, 54); b.g(gECHO_FB, 55); b.g(gREV_SIZE, 50);
    b.seq(0, 0, "!A1 A1 ~A2 A1 | . A1 ~C2 !A1 | G1 . A1 ~E2 | A1 !A2 . ~G2");
    b.lock(0, 0, 7, tpFENV, 120); b.lock(0, 0, 13, tpFENV, 124);
    b.seq(0, 2, ". . X . | . . X . | . . X . | . . X X");
    for (int s = 0; s < 16; ++s) if (b.b.patterns[0].flags[2][s]) b.b.patterns[0].note[2][s] = 84;
    b.settings(0, 16, 56);
    b.copyPattern(1, 0);
    b.seq(1, 1, "A2 ~C3 A2 . | ~E3 . A2 !G3 | . A2 ~A3 . | C3 . !E3 ~D3");
    b.seq(1, 4, "A3 - - - | - - - - | - - - - | - - - -");
    b.settings(1, 16, 56);
    b.g(gTEMPO, 132);
}

static void presetDetroit(PB& b)
{
    b.kit(0, mPULSE, {50, 20, 10, 0, 0, 70, 0, 0}); b.amp(0, 0, 0, 50, 40, 25); b.flt(0, 0, 50, 0, 50, 0, 45, 64, 90);
    b.kit(1, mSAW, {0, 0, 76, 64, 64, 64, 0, 0}); b.amp(1, 0, 0, 55, 30, 40); b.flt(1, 10, 70, 0, 40, 0, 50, 64, 96);
    b.arp(1, 1, 0, 0, 5); b.fx(1, tpDEL, 60); b.fx(1, tpREV, 40); b.fx(1, tpPAN, 50);
    b.lfo(1, 0, 6, 25, 1, 0, tpCUTOFF);
    b.kit(2, mSUPER, {30, 70, 90, 0, 0, 64, 0, 0}); b.amp(2, 70, 0, 80, 100, 90); b.flt(2, 0, 80, 0, 10, 0, 50, 64, 64);
    b.fx(2, tpREV, 80); b.fx(2, tpVOL, 72);
    b.kit(3, mFM, {32, 40, 112, 25, 0, 1, 40, 20}); b.amp(3, 0, 0, 40, 0, 40); b.fx(3, tpDEL, 50); b.fx(3, tpPAN, 84);
    b.kit(4, mWAVE, {44, 20, 30, 64, 70, 40, 0, 0}); b.amp(4, 20, 0, 60, 80, 60); b.fx(4, tpREV, 50);
    b.kit(5, mNOISE, {120, 20, 0, 0, 0, 0, 0, 0}); b.amp(5, 0, 0, 20, 0, 15); b.flt(5, 70, 127, 0, 0, 0, 50, 64, 64); b.fx(5, tpVOL, 64);
    b.g(gECHO_TIME, 86); b.g(gECHO_FB, 60); b.g(gREV_SIZE, 90);
    b.seq(0, 0, "F1 . F1 . | . F2 . F1 | . . F1 . | Ab1 . Bb1 .");
    b.seq(0, 1, ". . F3 . | . . F3 . | . . Ab3 . | . . Eb3 .");
    b.gate(0, 1, 1);
    b.seq(0, 2, "F3 - - - | - - - - | - - - - | - - - - | Db3 - - - | - - - - | Eb3 - - - | - - - -");
    b.seq(0, 5, ". . C5 . | . . C5 . | . . C5 . | . . C5 C5");
    b.settings(0, 32, 54);
    b.g(gTEMPO, 126);
}

static void presetBerlin(PB& b)
{
    b.kit(0, mSAW, {0, 30, 66, 30, 64, 64, 50, 0}); b.amp(0, 0, 0, 50, 40, 40); b.flt(0, 0, 54, 0, 70, 0, 55, 64, 92);
    b.lfo(0, 0, 4, 30, 0, 0, tpCUTOFF); b.fx(0, tpDEL, 30); b.fx(0, tpVOL, 88);
    b.kit(1, mSAW, {0, 64, 70, 50, 96, 64, 0, 0}); b.amp(1, 0, 0, 45, 0, 40); b.flt(1, 0, 70, 0, 60, 0, 40, 64, 90);
    b.fx(1, tpDEL, 70); b.fx(1, tpPAN, 34); b.lfo(1, 0, 3, 35, 1, 0, tpCUTOFF);
    b.kit(2, mSUPER, {40, 80, 127, 64, 0, 64, 0, 0}); b.amp(2, 90, 0, 90, 110, 100); b.flt(2, 0, 76, 0, 20, 0, 50, 64, 64);
    b.fx(2, tpREV, 90); b.fx(2, tpVOL, 70);
    b.kit(3, mWAVE, {60, 10, 100, 64, 64, 0, 0, 0}); b.amp(3, 80, 0, 80, 100, 100); b.fx(3, tpREV, 80); b.fx(3, tpVOL, 70);
    b.kit(4, mVOX, {20, 60, 20, 60, 30, 64, 40, 60}); b.amp(4, 60, 0, 80, 100, 90); b.fx(4, tpREV, 90); b.fx(4, tpVOL, 70);
    b.kit(5, mPLUCK, {60, 70, 50, 40, 0, 50, 40, 0}); b.amp(5, 0, 0, 90, 100, 60); b.fx(5, tpDEL, 70); b.fx(5, tpPAN, 94);
    b.g(gECHO_TIME, 54); b.g(gECHO_FB, 70); b.g(gECHO_LPF, 80); b.g(gREV_SIZE, 110); b.g(gREV_LEV, 80);
    b.seq(0, 0, "E1 E2 B1 E2 | D2 E2 B1 E2 | G1 E2 B1 E2 | A1 E2 D2 B1");
    b.seq(0, 1, "E3 B3 G3 E4 | D4 B3 G3 B3 | E3 B3 G3 E4 | F#4 D4 B3 A3 | E3 B3 G3 E4 | D4 B3 G3 B3 | C4 G3 E3 G3 | B3 F#3 D3 F#3");
    b.seq(0, 2, "E3 - - - | - - - - | - - - - | - - - - | C3 - - - | - - - - | D3 - - - | - - - -");
    b.settings(0, 32, 50);
    b.g(gTEMPO, 112);
}

static void presetSynthwave(PB& b)
{
    b.kit(0, mSAW, {0, 0, 64, 0, 64, 64, 70, 0}); b.amp(0, 0, 0, 40, 60, 20); b.fx(0, tpVOL, 90); b.flt(0, 0, 60, 0, 30, 0, 40, 64, 84);
    b.kit(1, mSUPER, {60, 100, 127, 0, 30, 64, 0, 20}); b.amp(1, 30, 0, 70, 90, 70); b.flt(1, 0, 90, 0, 20, 0, 50, 64, 64);
    b.fx(1, tpREV, 70); b.fx(1, tpDEL, 30); b.fx(1, tpVOL, 78);
    b.kit(2, mSAW, {0, 64, 72, 64, 96, 50, 0, 0}); b.amp(2, 0, 0, 40, 0, 40); b.flt(2, 0, 80, 0, 40, 0, 40, 64, 84);
    b.arp(2, 3, 2, 0, 2); b.fx(2, tpDEL, 70); b.fx(2, tpPAN, 30); b.fx(2, tpVOL, 80);
    b.kit(3, mWAVE, {8, 40, 20, 96, 70, 40, 0, 0}); b.amp(3, 5, 0, 60, 90, 60); b.flt(3, 0, 100, 0, 20, 0, 50, 64, 64);
    b.lfo(3, 1, 74, 12, 1, 1, PITCH); b.fx(3, tpPORT, 30); b.legato(3); b.fx(3, tpDEL, 50); b.fx(3, tpREV, 50);
    b.kit(4, mNOISE, {100, 10, 0, 0, 0, 0, 30, 0}); b.amp(4, 0, 0, 40, 0, 50); b.flt(4, 40, 127, 0, 0, 0, 50, 64, 64);
    b.fx(4, tpREV, 90); b.fx(4, tpVOL, 70);
    b.kit(5, mFM, {32, 30, 48, 20, 0, 3, 60, 30}); b.amp(5, 0, 0, 70, 50, 60); b.fx(5, tpREV, 60); b.fx(5, tpPAN, 94);
    b.g(gECHO_TIME, 86); b.g(gECHO_FB, 50); b.g(gREV_SIZE, 100); b.g(gREV_LEV, 80);
    b.seq(0, 0, "A1 A1 A2 A1 | A1 A1 A2 A1 | F1 F1 F2 F1 | F1 F1 F2 F1 | C2 C2 C3 C2 | C2 C2 C3 C2 | G1 G1 G2 G1 | G1 G1 G2 G1");
    b.seq(0, 1, "A2 - - - | - - - - | F2 - - - | - - - - | C3 - - - | - - - - | G2 - - - | - - - -");
    b.seq(0, 2, "A3 . . . | . . . . | F3 . . . | . . . . | C4 . . . | . . . . | G3 . . . | . . . .");
    b.gate(0, 2, 10);
    b.seq(0, 4, ". . . . | !C3 . . . | . . . . | !C3 . . .");
    b.settings(0, 32, 50);
    b.copyPattern(1, 0);
    b.seq(1, 3, "E4 - - - | ~D4 - C4 - | A3 - - - | - - - - | E4 - - G4 | - ~A4 - - | G4 - E4 - | D4 - - -");
    b.settings(1, 32, 50);
    b.g(gTEMPO, 100);
}

static void presetArpCascade(PB& b)
{
    b.kit(0, mFM, {16, 30, 64, 0, 0, 0, 40, 20}); b.amp(0, 0, 0, 50, 50, 30); b.flt(0, 0, 70, 0, 20, 0, 50, 64, 64);
    b.kit(1, mPLUCK, {70, 100, 40, 30, 0, 30, 40, 0}); b.amp(1, 0, 0, 90, 100, 60); b.arp(1, 1, 2, 0, 4); b.fx(1, tpVOL, 84);
    b.fx(1, tpDEL, 60); b.fx(1, tpPAN, 34); b.fx(1, tpREV, 40);
    b.kit(2, mSAW, {64, 64, 70, 50, 96, 30, 0, 0}); b.amp(2, 0, 0, 35, 0, 30); b.flt(2, 10, 80, 0, 40, 0, 35, 64, 84);
    b.arp(2, 2, 1, 1, 5); b.fx(2, tpDEL, 60); b.fx(2, tpPAN, 94); b.lfo(2, 0, 2, 40, 0, 0, tpCUTOFF);
    b.kit(3, mFM, {32, 40, 112, 50, 0, 3, 50, 10}); b.amp(3, 0, 0, 60, 0, 60); b.arp(3, 4, 2, 0, 1);
    b.fx(3, tpREV, 70); b.fx(3, tpDEL, 50); b.fx(3, tpVOL, 80);
    b.kit(4, mSUPER, {35, 80, 110, 0, 0, 64, 0, 0}); b.amp(4, 90, 0, 90, 110, 100); b.fx(4, tpREV, 80); b.fx(4, tpVOL, 66);
    b.kit(5, mWAVE, {90, 60, 40, 64, 64, 0, 20, 0}); b.amp(5, 40, 0, 60, 80, 80); b.arp(5, 5, 0, 0, 0); b.fx(5, tpREV, 60);
    b.g(gECHO_TIME, 64); b.g(gECHO_FB, 60); b.g(gREV_SIZE, 100);
    b.seq(0, 0, "D2 - . D2 | . . D2 . | Bb1 - . Bb1 | . . Bb1 . | F1 - . F1 | . . F1 . | C2 - . C2 | . . A1 .");
    b.seq(0, 1, "D3 . . . | . . . . | Bb2 . . . | . . . . | F2 . . . | . . . . | C3 . . . | . . . .");
    b.gate(0, 1, 10);
    b.seq(0, 2, ". . . . | . . . . | . . . . | . . . . | D4 . . . | . . . . | . . . . | . . . .");
    b.gate(0, 2, 10);
    b.seq(0, 4, "D3 - - - | - - - - | Bb2 - - - | - - - - | F2 - - - | - - - - | C3 - - - | - - - -");
    b.settings(0, 32, 50);
    b.g(gTEMPO, 124);
}

static void presetDubStabs(PB& b)
{
    b.kit(0, mSAW, {127, 127, 64, 0, 64, 64, 90, 0}); b.amp(0, 0, 0, 70, 90, 40); b.fx(0, tpVOL, 80); b.flt(0, 0, 40, 0, 20, 0, 50, 64, 64);
    b.kit(1, mSAW, {0, 0, 78, 64, 64, 64, 0, 0}); b.amp(1, 0, 0, 40, 0, 50); b.flt(1, 10, 66, 0, 60, 0, 40, 64, 92);
    b.arp(1, 5, 0, 0, 5); b.b.kit[1][kfArpSpd] = 0; b.fx(1, tpDEL, 90); b.fx(1, tpREV, 60); b.lfo(1, 0, 3, 30, 1, 0, tpCUTOFF);
    b.kit(2, mPULSE, {60, 20, 20, 0, 0, 0, 0, 0}); b.amp(2, 0, 0, 30, 0, 40); b.flt(2, 30, 70, 0, 40, 0, 40, 64, 80);
    b.fx(2, tpDEL, 90); b.fx(2, tpPAN, 30);
    b.kit(3, mNOISE, {80, 50, 0, 0, 40, 0, 20, 0}); b.amp(3, 80, 0, 90, 90, 100); b.fx(3, tpREV, 100); b.fx(3, tpVOL, 60);
    b.lfo(3, 0, 10, 50, 1, 0, 0);
    b.kit(4, mVOX, {40, 50, 30, 70, 0, 64, 30, 40}); b.amp(4, 20, 0, 60, 60, 80); b.fx(4, tpDEL, 70); b.fx(4, tpREV, 70);
    b.kit(5, mPLUCK, {50, 50, 60, 60, 20, 60, 60, 0}); b.amp(5, 0, 0, 80, 100, 60); b.fx(5, tpDEL, 80); b.fx(5, tpPAN, 100);
    b.g(gECHO_TIME, 86); b.g(gECHO_FB, 85); b.g(gECHO_LPF, 70); b.g(gECHO_HPF, 30); b.g(gREV_SIZE, 100); b.g(gREV_LEV, 80);
    b.seq(0, 0, "C2 - - - | - - . . | C2 - - - | - - - . | Eb2 - - - | - - . . | F1 - - - | - - G1 -");
    b.seq(0, 1, ". . C3 . | . . . . | . . . . | . . C3 . | . . . . | . . . . | . . . . | . . . .");
    b.gate(0, 1, 1);
    b.seq(0, 2, ". . . . | . . . . | . . . . | . . . . | . . . . | . . . . | . . . . | . . . G4");
    b.settings(0, 32, 54);
    b.g(gTEMPO, 118);
}

static void presetIdm(PB& b)
{
    b.kit(0, mFM, {8, 70, 64, 10, 20, 0, 30, 10}); b.amp(0, 0, 0, 40, 30, 20); b.fx(0, tpDIST, 20);
    b.kit(1, mPULSE, {30, 80, 60, 40, 30, 0, 0, 50}); b.amp(1, 0, 0, 25, 0, 20); b.fx(1, tpDEL, 50); b.fx(1, tpPAN, 30);
    b.lfo(1, 0, 110, 60, 5, 0, 3);
    b.kit(2, mFM, {80, 90, 100, 60, 30, 3, 20, 0}); b.amp(2, 0, 0, 20, 0, 20); b.fx(2, tpPAN, 100); b.fx(2, tpSRR, 30);
    b.kit(3, mWAVE, {40, 100, 90, 64, 64, 0, 60, 30}); b.amp(3, 0, 0, 30, 0, 30); b.arp(3, 4, 2, 0, 9); b.fx(3, tpDEL, 60);
    b.kit(4, mNOISE, {90, 90, 127, 40, 60, 40, 0, 0}); b.amp(4, 0, 0, 20, 0, 20); b.fx(4, tpVOL, 80);
    b.kit(5, mPLUCK, {40, 110, 20, 20, 60, 80, 0, 40}); b.amp(5, 0, 0, 80, 100, 40); b.fx(5, tpDEL, 50); b.fx(5, tpREV, 40);
    b.g(gECHO_TIME, 43); b.g(gECHO_FB, 50); b.g(gREV_SIZE, 50);
    b.seq(0, 0, "C2 . . C2 | . . Eb2 . | . C2 . . | G1 . . . | C2 . ~C3 . | . . Eb2 Eb2 | . . Bb1 . | . . . .");
    b.seq(0, 1, ". G4 . . | D5 . . G4 | . . . . | . . D#5 . | . G4 . . | . . . . | C6 . . G4 | . . . .");
    b.seq(0, 2, ". . . . . . . C5 . . . . . C5 . . . . . . . . . . . . .");
    b.seq(0, 4, "C6 . C6 . C6 C6 . C6 . C6 . . C6 . C6 C6 C6");
    b.ratch(0, 4, 6, 3); b.ratch(0, 4, 15, 4); b.cond(0, 1, 8, 3); b.cond(0, 2, 13, 13);
    b.seq(0, 5, ". . . . . . G3 . . . . . . . . . . . . . . . D#4 . . . . . . . . .");
    b.settings(0, 32, 50);
    b.g(gTEMPO, 138);
}

static void presetAmbient(PB& b)
{
    b.kit(0, mWAVE, {60, 8, 60, 64, 66, 60, 0, 0}); b.amp(0, 100, 0, 100, 110, 110); b.flt(0, 0, 76, 0, 20, 0, 50, 64, 64);
    b.fx(0, tpREV, 100); b.fx(0, tpVOL, 80); b.lfo(0, 0, 4, 40, 1, 0, 0);
    b.kit(1, mSUPER, {25, 70, 127, 0, 0, 64, 0, 0}); b.amp(1, 110, 0, 110, 110, 115); b.flt(1, 0, 70, 0, 10, 0, 50, 64, 64);
    b.fx(1, tpREV, 110); b.fx(1, tpVOL, 70); b.lfo(1, 0, 3, 30, 1, 0, tpCUTOFF);
    b.kit(2, mVOX, {30, 64, 30, 60, 20, 50, 30, 80}); b.amp(2, 100, 0, 100, 110, 110); b.fx(2, tpREV, 110); b.fx(2, tpVOL, 70);
    b.lfo(2, 0, 2, 50, 1, 0, 0);
    b.kit(3, mFM, {32, 30, 112, 40, 0, 3, 90, 0}); b.amp(3, 0, 0, 90, 0, 100); b.fx(3, tpREV, 90); b.fx(3, tpDEL, 70); b.fx(3, tpVOL, 75);
    b.kit(4, mPLUCK, {110, 60, 70, 60, 10, 30, 50, 0}); b.amp(4, 0, 0, 100, 110, 100); b.fx(4, tpDEL, 80); b.fx(4, tpREV, 90);
    b.fx(4, tpPAN, 30);
    b.kit(5, mNOISE, {50, 30, 0, 0, 30, 0, 60, 0}); b.amp(5, 110, 0, 110, 110, 115); b.fx(5, tpREV, 100); b.fx(5, tpVOL, 50);
    b.lfo(5, 0, 2, 70, 1, 0, 0);
    b.g(gECHO_TIME, 96); b.g(gECHO_FB, 75); b.g(gECHO_LPF, 70); b.g(gREV_SIZE, 127); b.g(gREV_DAMP, 90); b.g(gREV_LEV, 95);
    b.seq(0, 0, "D3 - - - | - - - - | - - - - | - - - - | Bb2 - - - | - - - - | - - - - | - - - -");
    b.seq(0, 1, "A3 - - - | - - - - | - - - - | - - - - | F3 - - - | - - - - | - - - - | - - - -");
    b.seq(0, 3, ". . . . | . . A5 . | . . . . | . . . . | . . . . | E5 . . . | . . . . | . . D5 .");
    b.seq(0, 4, ". . . . | . . . . | D4 . . . | . . . . | . . . . | . . . . | A3 . . . | . . . .");
    b.seq(0, 5, "C4 - - - | - - - - | - - - - | - - - - | - - - - | - - - - | - - - - | - - - -");
    b.settings(0, 32, 50, 4);
    b.g(gTEMPO, 90);
}

static void presetFmBells(PB& b)
{
    b.kit(0, mFM, {32, 30, 64, 0, 0, 0, 40, 40}); b.amp(0, 0, 0, 60, 60, 30);
    b.kit(1, mFM, {48, 60, 112, 40, 0, 3, 80, 10}); b.amp(1, 0, 0, 90, 0, 90); b.fx(1, tpREV, 70); b.fx(1, tpDEL, 40);
    b.kit(2, mFM, {120, 50, 72, 30, 0, 1, 70, 0}); b.amp(2, 0, 0, 80, 0, 80); b.fx(2, tpREV, 80); b.fx(2, tpPAN, 30);
    b.kit(3, mFM, {32, 35, 32, 30, 0, 2, 50, 30}); b.amp(3, 10, 0, 70, 70, 60); b.fx(3, tpREV, 50); b.fx(3, tpPAN, 96); // e-piano
    b.lfo(3, 0, 50, 20, 1, 0, tpAMD);
    b.kit(4, mFM, {16, 90, 64, 0, 40, 0, 20, 0}); b.amp(4, 0, 0, 30, 0, 20); b.fx(4, tpDEL, 50);
    b.kit(5, mFM, {104, 70, 120, 50, 0, 3, 100, 0}); b.amp(5, 0, 0, 100, 0, 110); b.fx(5, tpREV, 100); b.fx(5, tpVOL, 70);
    b.g(gECHO_TIME, 86); b.g(gECHO_FB, 50); b.g(gREV_SIZE, 100);
    b.seq(0, 0, "E1 . . E1 | . . B1 . | C2 . . C2 | . . G1 . | A1 . . A1 | . . E2 . | D2 . . D2 | . . B1 .");
    b.seq(0, 1, "E4 . B4 . | G4 . . E5 | . . D5 . | B4 . . . | C5 . G4 . | E4 . . D5 | . . C5 . | B4 . . .");
    b.seq(0, 3, "E3 - - - | - - G3 - | - - - - | . . . . | A3 - - - | - - C4 - | - - - - | . . . .");
    b.seq(0, 5, ". . . . | . . . . | . . . . | . . . . | . . . . | . . . . | . . . . | . . . . | E6 . . . .");
    b.settings(0, 32, 52);
    b.g(gTEMPO, 104);
}

static void presetWaveLead(PB& b)
{
    b.kit(0, mWAVE, {0, 20, 0, 32, 64, 60, 30, 0}); b.amp(0, 0, 0, 40, 70, 20); b.flt(0, 0, 60, 0, 50, 0, 45, 64, 90);
    b.kit(1, mWAVE, {100, 40, 50, 96, 70, 50, 40, 0}); b.amp(1, 10, 0, 70, 90, 50); b.flt(1, 0, 100, 0, 20, 0, 50, 64, 64);
    b.lfo(1, 1, 74, 14, 1, 1, PITCH); b.legato(1); b.fx(1, tpPORT, 35); b.fx(1, tpDEL, 60); b.fx(1, tpREV, 40);
    b.kit(2, mWAVE, {40, 10, 80, 64, 66, 40, 0, 0}); b.amp(2, 80, 0, 90, 100, 90); b.fx(2, tpREV, 90); b.fx(2, tpVOL, 70);
    b.kit(3, mWAVE, {70, 90, 100, 64, 64, 0, 70, 20}); b.amp(3, 0, 0, 30, 0, 30); b.arp(3, 3, 1, 0, 2); b.fx(3, tpDEL, 60);
    b.fx(3, tpPAN, 30); b.fx(3, tpVOL, 80);
    b.kit(4, mNOISE, {110, 30, 0, 0, 0, 0, 0, 0}); b.amp(4, 0, 0, 20, 0, 15); b.flt(4, 70, 127, 0, 0, 0, 50, 64, 64); b.fx(4, tpVOL, 64);
    b.kit(5, mPLUCK); b.fx(5, tpDEL, 50);
    b.g(gECHO_TIME, 86); b.g(gECHO_FB, 55); b.g(gREV_SIZE, 90);
    b.seq(0, 0, "G1 . G2 G1 | . G1 F2 . | G1 . G2 G1 | . Bb1 C2 . | Eb1 . Eb2 Eb1 | . Eb1 D2 . | F1 . F2 F1 | . F1 A1 .");
    b.seq(0, 1, "D4 - - - | - - ~F4 - | ~G4 - - - | - - F4 - | Eb4 - - - | ~D4 - - - | C4 - - - | ~D4 - - -");
    b.seq(0, 2, "G2 - - - | - - - - | - - - - | - - - - | Eb2 - - - | - - - - | F2 - - - | - - - -");
    b.seq(0, 4, ". . X . | . . X . | . . X . | . X X .");
    for (int s = 0; s < 16; ++s) if (b.b.patterns[0].flags[4][s]) b.b.patterns[0].note[4][s] = 84;
    b.settings(0, 32, 52);
    b.g(gTEMPO, 110);
}

static void presetChoir(PB& b)
{
    b.kit(0, mVOX, {80, 40, 10, 50, 10, 60, 30, 30}); b.amp(0, 30, 0, 80, 100, 70); b.fx(0, tpREV, 60);
    b.kit(1, mVOX, {0, 64, 30, 70, 30, 64, 50, 70}); b.amp(1, 90, 0, 100, 110, 100); b.fx(1, tpREV, 100); b.fx(1, tpPAN, 34);
    b.lfo(1, 0, 3, 60, 1, 0, 0);
    b.kit(2, mVOX, {30, 70, 30, 70, 30, 70, 50, 70}); b.amp(2, 90, 0, 100, 110, 100); b.fx(2, tpREV, 100); b.fx(2, tpPAN, 94);
    b.lfo(2, 0, 4, 60, 0, 0, 0);
    b.kit(3, mVOX, {64, 80, 30, 80, 40, 70, 60, 50}); b.amp(3, 80, 0, 100, 110, 100); b.fx(3, tpREV, 110); b.fx(3, tpVOL, 80);
    b.kit(4, mPLUCK, {100, 60, 60, 50, 20, 30, 70, 0}); b.amp(4, 0, 0, 100, 100, 100); b.fx(4, tpREV, 80); b.fx(4, tpDEL, 50);
    b.kit(5, mSUPER, {20, 60, 127, 64, 0, 64, 0, 0}); b.amp(5, 110, 0, 110, 110, 115); b.flt(5, 0, 60, 0, 10, 0, 50, 64, 64);
    b.fx(5, tpREV, 110); b.fx(5, tpVOL, 64);
    b.g(gREV_SIZE, 120); b.g(gREV_LEV, 90); b.g(gECHO_TIME, 96); b.g(gECHO_FB, 60);
    b.seq(0, 1, "C4 - - - | - - - - | - - - - | - - - - | Bb3 - - - | - - - - | - - - - | - - - -");
    b.seq(0, 2, "G4 - - - | - - - - | - - - - | - - - - | F4 - - - | - - - - | - - - - | - - - -");
    b.seq(0, 3, "Eb4 - - - | - - - - | D4 - - - | - - - - | D4 - - - | - - - - | C4 - - - | - - - -");
    b.seq(0, 0, "C2 - - - | - - - - | - - - - | - - - - | Bb1 - - - | - - - - | Ab1 - - - | - - - -");
    b.seq(0, 4, ". . . . | . . G4 . | . . . . | . . C5 . | . . . . | . . F4 . | . . . . | . . D4 .");
    b.settings(0, 32, 50, 4);
    b.g(gTEMPO, 84);
}

static void presetChiptune(PB& b)
{
    b.kit(0, mPULSE, {32, 0, 0, 0, 0, 0, 0, 40}); b.amp(0, 0, 0, 40, 90, 10); b.flt(0, 0, 127, 0, 0, 0, 50, 64, 64);
    b.kit(1, mPULSE, {64, 0, 0, 0, 0, 0, 0, 50}); b.amp(1, 0, 0, 50, 80, 15); b.lfo(1, 1, 80, 12, 1, 1, PITCH);
    b.fx(1, tpPORT, 10); b.fx(1, tpDEL, 30);
    b.kit(2, mPULSE, {16, 0, 0, 0, 0, 0, 0, 50}); b.amp(2, 0, 0, 30, 0, 10); b.arp(2, 1, 0, 0, 2); b.fx(2, tpPAN, 34);
    b.kit(3, mNOISE, {127, 0, 0, 60, 0, 0, 0, 0}); b.amp(3, 0, 0, 20, 0, 10); b.fx(3, tpVOL, 80);
    b.kit(4, mWAVE, {0, 0, 0, 64, 64, 0, 0, 80}); b.amp(4, 0, 0, 40, 90, 10);
    b.kit(5, mPULSE, {64, 0, 0, 0, 0, 0, 0, 30}); b.amp(5, 0, 0, 20, 0, 10);
    b.g(gECHO_LEV, 30); b.g(gREV_LEV, 20);
    b.seq(0, 0, "C2 . C3 . | G1 . G2 . | A1 . A2 . | E1 . E2 . | F1 . F2 . | C2 . C3 . | F1 . F2 . | G1 . G2 .");
    b.seq(0, 1, "E4 . G4 . | C5 . . B4 | . . A4 . | G4 . E4 . | F4 . A4 . | C5 . . E5 | . D5 C5 . | B4 . . .");
    b.seq(0, 2, ". . . . | . . . . | . . . . | . . . . | . . . . | . . . . | . . . . | G3 . G3 .");
    b.gate(0, 2, 4);
    b.seq(0, 3, "C5 . C5 C5 | !C5 . C5 . | C5 . C5 C5 | !C5 . C5 C5");
    b.settings(0, 32, 50);
    b.g(gTEMPO, 140);
}

static void presetTrance(PB& b)
{
    b.kit(0, mSAW, {0, 0, 64, 0, 64, 64, 80, 0}); b.amp(0, 0, 0, 30, 40, 15); b.fx(0, tpVOL, 92); b.flt(0, 0, 54, 0, 40, 0, 35, 64, 80);
    b.kit(1, mSUPER, {70, 110, 127, 0, 20, 64, 10, 20}); b.amp(1, 0, 0, 40, 30, 50); b.flt(1, 0, 80, 0, 30, 0, 50, 64, 90);
    b.fx(1, tpDEL, 60); b.fx(1, tpREV, 70); b.fx(1, tpVOL, 82); b.lfo(1, 0, 1, 40, 3, 0, tpCUTOFF);
    b.kit(2, mSUPER, {50, 100, 127, 0, 0, 64, 0, 10}); b.amp(2, 80, 0, 90, 110, 100); b.fx(2, tpREV, 100); b.fx(2, tpVOL, 66);
    b.kit(3, mPLUCK, {60, 110, 30, 30, 0, 40, 30, 0}); b.amp(3, 0, 0, 80, 100, 50); b.arp(3, 1, 2, 0, 2);
    b.fx(3, tpDEL, 70); b.fx(3, tpPAN, 30);
    b.kit(4, mNOISE, {90, 20, 0, 0, 0, 0, 30, 0}); b.amp(4, 100, 0, 110, 110, 100); b.lfo(4, 0, 1, 80, 3, 1, 0);
    b.fx(4, tpREV, 90); b.fx(4, tpVOL, 60);
    b.kit(5, mFM, {48, 50, 112, 30, 0, 3, 60, 0}); b.amp(5, 0, 0, 70, 0, 80); b.fx(5, tpREV, 70); b.fx(5, tpPAN, 94);
    b.g(gECHO_TIME, 54); b.g(gECHO_FB, 60); b.g(gREV_SIZE, 110); b.g(gREV_LEV, 85);
    b.seq(0, 0, ". A1 A2 A1 | . A1 A2 A1 | . F1 F2 F1 | . F1 F2 F1 | . C2 C3 C2 | . C2 C3 C2 | . G1 G2 G1 | . G1 G2 G1");
    b.seq(0, 1, "A3 . A3 . | C4 . A3 . | E4 . . A3 | . C4 . E4 | F4 . F4 . | E4 . C4 . | D4 . . B3 | . G3 . B3");
    b.gate(0, 1, 2);
    b.seq(0, 2, "A2 - - - | - - - - | F2 - - - | - - - - | C3 - - - | - - - - | G2 - - - | - - - -");
    b.settings(0, 32, 50);
    b.g(gTEMPO, 138);
}

static void presetMinimal(PB& b)
{
    b.kit(0, mPLUCK, {30, 50, 70, 70, 0, 40, 70, 30}); b.amp(0, 0, 0, 60, 70, 30); b.flt(0, 0, 70, 0, 30, 0, 50, 64, 64);
    b.kit(1, mPLUCK, {60, 90, 40, 40, 40, 60, 40, 0}); b.amp(1, 0, 0, 80, 100, 40); b.fx(1, tpDEL, 70); b.fx(1, tpPAN, 34);
    b.lfo(1, 0, 9, 40, 5, 1, 1);
    b.kit(2, mPLUCK, {40, 120, 15, 20, 90, 90, 0, 0}); b.amp(2, 0, 0, 60, 100, 30); b.fx(2, tpDEL, 50); b.fx(2, tpPAN, 94);
    b.kit(3, mNOISE, {127, 70, 0, 0, 0, 0, 0, 0}); b.amp(3, 0, 0, 15, 0, 10); b.flt(3, 80, 127, 0, 0, 0, 50, 64, 64); b.fx(3, tpVOL, 70);
    b.kit(4, mFM, {64, 40, 80, 30, 0, 0, 20, 0}); b.amp(4, 0, 0, 25, 0, 20); b.fx(4, tpDEL, 70); b.fx(4, tpREV, 40);
    b.kit(5, mWAVE, {50, 5, 50, 64, 66, 30, 0, 0}); b.amp(5, 80, 0, 100, 100, 100); b.fx(5, tpREV, 90); b.fx(5, tpVOL, 64);
    b.g(gECHO_TIME, 54); b.g(gECHO_FB, 65); b.g(gECHO_WIDTH, 120); b.g(gREV_SIZE, 70);
    b.seq(0, 0, "A1 . . A1 | . . A2 . | . A1 . . | E2 . . . | A1 . . A1 | . . A2 . | . A1 . . | G2 . D2 .");
    b.seq(0, 1, ". . E4 . | . . . . | . . . C4 . | . . . . . .");
    b.seq(0, 2, ". . . . . . . A5 . . . . . . . . . . . . . . . . . E5 . . . . . . .");
    b.seq(0, 3, ". . C6 . . . C6 . . . C6 . . . C6 C6");
    b.cond(0, 3, 15, 3);
    b.seq(0, 4, ". . . . . . . . . . . . . . . . . . . . . . . . . . . . . . E5 .");
    b.seq(0, 5, "A2 - - - | - - - - | - - - - | - - - - | - - - - | - - - - | - - - - | - - - -");
    b.settings(0, 32, 58);
    b.g(gTEMPO, 122);
}

static void presetElectroFunk(PB& b)
{
    b.kit(0, mPULSE, {40, 0, 0, 0, 0, 80, 0, 0}); b.amp(0, 0, 0, 40, 30, 20); b.flt(0, 0, 50, 0, 80, 0, 40, 64, 100);
    b.fx(0, tpPORT, 20); b.fx(0, tpDIST, 20); b.legato(0);
    b.kit(1, mSAW, {0, 64, 70, 40, 64, 64, 0, 0}); b.amp(1, 0, 0, 30, 0, 30); b.flt(1, 20, 60, 0, 70, 0, 30, 64, 100);
    b.fx(1, tpDEL, 40); b.fx(1, tpPAN, 40);
    b.kit(2, mVOX, {40, 50, 10, 80, 0, 64, 70, 0}); b.amp(2, 0, 0, 50, 80, 30); b.fx(2, tpDIST, 30); b.fx(2, tpDEL, 50);
    b.lfo(2, 0, 30, 60, 0, 1, 0); b.fx(2, tpPORT, 30); b.legato(2);
    b.kit(3, mFM, {32, 50, 64, 20, 0, 0, 30, 30}); b.amp(3, 0, 0, 30, 0, 20); b.fx(3, tpPAN, 90);
    b.kit(4, mNOISE, {110, 50, 0, 20, 0, 0, 0, 0}); b.amp(4, 0, 0, 25, 0, 20); b.fx(4, tpVOL, 80); b.flt(4, 60, 127, 0, 0, 0, 50, 64, 64);
    b.kit(5, mWAVE, {16, 40, 30, 64, 70, 40, 20, 0}); b.amp(5, 20, 0, 60, 80, 60); b.fx(5, tpREV, 60);
    b.g(gECHO_TIME, 54); b.g(gECHO_FB, 45); b.g(gREV_SIZE, 50);
    b.seq(0, 0, "E1 . E2 ~D2 | . E1 . . | G1 ~A1 . E1 | . . ~B1 D2 | E1 . E2 ~D2 | . E1 . . | A1 . G1 . | ~E1 . D2 .");
    b.seq(0, 1, ". . . . | !E3 . . . | . . . . | !E3 . . G3 | . . . . | !E3 . . . | . . . . | !D3 . . .");
    b.seq(0, 2, ". . . . | . . . . | E3 - ~G3 - | ~A3 - . . | . . . . | . . . . | B3 - ~A3 - | ~G3 - ~E3 -");
    b.seq(0, 4, ". . X . . . X . . . X . . X X .");
    for (int s = 0; s < 16; ++s) if (b.b.patterns[0].flags[4][s]) b.b.patterns[0].note[4][s] = 84;
    b.settings(0, 32, 58);
    b.g(gTEMPO, 110);
}

static void presetNoiseScapes(PB& b)
{
    b.kit(0, mNOISE, {30, 100, 127, 0, 0, 30, 100, 20}); b.amp(0, 0, 0, 60, 70, 40); b.flt(0, 0, 80, 0, 0, 0, 50, 64, 64);
    b.kit(1, mNOISE, {80, 110, 127, 0, 30, 0, 0, 0}); b.amp(1, 30, 0, 80, 80, 80); b.fx(1, tpDEL, 70); b.fx(1, tpREV, 70);
    b.fx(1, tpPAN, 30); b.lfo(1, 0, 2, 40, 1, 0, 0);
    b.kit(2, mNOISE, {60, 40, 0, 0, 80, 0, 30, 0}); b.amp(2, 100, 0, 100, 110, 110); b.fx(2, tpREV, 110); b.fx(2, tpVOL, 70);
    b.lfo(2, 0, 3, 90, 1, 0, 0);
    b.kit(3, mNOISE, {127, 80, 0, 50, 0, 0, 0, 0}); b.amp(3, 0, 0, 15, 0, 10); b.fx(3, tpVOL, 70);
    b.kit(4, mWAVE, {80, 30, 80, 64, 64, 0, 90, 60}); b.amp(4, 0, 0, 30, 0, 30); b.arp(4, 4, 2, 0, 9); b.fx(4, tpDEL, 70);
    b.kit(5, mPLUCK, {70, 127, 10, 10, 100, 127, 80, 60}); b.amp(5, 0, 0, 100, 100, 80); b.fx(5, tpREV, 80); b.fx(5, tpPAN, 100);
    b.g(gECHO_TIME, 64); b.g(gECHO_FB, 80); b.g(gREV_SIZE, 120); b.g(gREV_LEV, 80);
    b.seq(0, 0, "D2 . . D2 | . . D3 . | . . A1 . | . . . C2");
    b.seq(0, 1, "A4 - - - | - - - - | . . . . | . . . . | F4 - - - | - - - - | . . . . | . . . .");
    b.seq(0, 2, "D3 - - - | - - - - | - - - - | - - - - | - - - - | - - - - | - - - - | - - - -");
    b.seq(0, 3, "C6 C6 C6 . C6 C6 . C6 C6 . C6 C6 C6");
    b.ratch(0, 3, 7, 3);
    b.seq(0, 5, ". . . . . . . . . . . . . . . . . . . . . . . . . D4 . . . . . .");
    b.settings(0, 32, 50);
    b.g(gTEMPO, 96);
}

// --------------------------------------------------------------------------------------------
struct PresetDef { const char* name; const char* desc; void (*build)(PB&); };

static const PresetDef kPresets[] = {
    {"Init", "base parameters, empty sequencer", presetInit},
    {"Demo Song", "bass, stab, pad, bells, lead and pluck", presetDemo},
    {"Acid Line", "resonant sliding bass, two patterns", presetAcid},
    {"Detroit Chords", "minor 7th arp stabs and strings", presetDetroit},
    {"Berlin School", "sequenced saws, slow pads", presetBerlin},
    {"Synthwave Drive", "octave bass, supersaw, arp, lead", presetSynthwave},
    {"Arp Cascade", "four arpeggiators interlocking", presetArpCascade},
    {"Dub Stabs", "chord stabs drowned in echo", presetDubStabs},
    {"IDM Bleeps", "FM blips, ratchets, conditions", presetIdm},
    {"Ambient Drift", "slow pads, bells and wind", presetAmbient},
    {"FM Bells", "FM bass, bells, e-piano, chimes", presetFmBells},
    {"Wavetable Lead", "gliding wave lead over a wave bass", presetWaveLead},
    {"Vox Choir", "formant voices in harmony", presetChoir},
    {"Chiptune", "pulse waves, crushed and quick", presetChiptune},
    {"Trance Supersaw", "offbeat bass, supersaw riff, pluck arp", presetTrance},
    {"Plucked Minimal", "strings, clicks and space", presetMinimal},
    {"Electro Funk", "sliding pulse bass, talking vox", presetElectroFunk},
    {"Noise Scapes", "pitched noise, grains and textures", presetNoiseScapes},
};
static constexpr int kNumPresets = (int)(sizeof(kPresets) / sizeof(kPresets[0]));

inline void buildPreset(int i, float* values, Bank& bank)
{
    PB b(values, bank);
    if (i >= 0 && i < kNumPresets) kPresets[i].build(b);
    // the presets were voiced with the linear RESONANCE curve used before 0.11: convert their values (knobs and
    // locks) to the exponential curve (resoQ) so they keep the same resonance
    auto conv = [](float v, float qMin, float span) {
        const float q = qMin + clampf(v, 0.f, 127.f) / 127.f * span;
        const float e = std::log(q / qMin) / std::log((qMin + span) / qMin);
        return std::round(127.f * std::pow(clampf(e, 0.f, 1.f), 0.8f));
    };
    for (int t = 0; t < kTracks; ++t) values[t * kTrackParams + tpRESO] = conv(values[t * kTrackParams + tpRESO], 0.5f, 17.f);
    values[kGlobalBase + gMF_RESO] = conv(values[kGlobalBase + gMF_RESO], 0.707f, 9.f);
    for (auto& pat : bank.patterns)
        for (int t = 0; t < kTracks; ++t)
            for (int st = 0; st < kSteps; ++st)
                if (pat.locks[t][st][tpRESO] != kNoLock) pat.locks[t][st][tpRESO] = (uint8_t)conv(pat.locks[t][st][tpRESO], 0.5f, 17.f);
}

} // namespace kn
