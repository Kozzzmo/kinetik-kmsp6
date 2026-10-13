// Kinetik KMSP-6 — randomizers (sound / sequence / pattern / everything)
#pragma once
#include "Bank.hpp"
#include <cmath>

namespace kn {

struct RandGen {
    uint32_t s;
    explicit RandGen(uint32_t seed) : s(seed ? seed : 0x1234567u) {}
    uint32_t next() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return s; }
    float uni() { return (next() & 0xFFFFFF) / 16777216.f; }
    int range(int lo, int hi) { return lo + (int)(uni() * (hi - lo + 1)); } // inclusive
    bool chance(float p) { return uni() < p; }
};

inline float& tparam(float* values, int t, int p) { return values[t * kTrackParams + p]; }

// scales used to keep random melodies musical
struct RndScale { int n; int s[7]; };
static const RndScale kRndScales[] = {
    {5, {0, 3, 5, 7, 10}},          // minor pentatonic
    {7, {0, 2, 3, 5, 7, 8, 10}},    // natural minor
    {7, {0, 2, 3, 5, 7, 9, 10}},    // dorian
    {7, {0, 1, 3, 5, 7, 8, 10}},    // phrygian
    {5, {0, 2, 4, 7, 9}},           // major pentatonic
    {7, {0, 2, 4, 5, 7, 9, 11}},    // major
};
static constexpr int kNumRndScales = (int)(sizeof(kRndScales) / sizeof(kRndScales[0]));

// role of a track: 0 = bass, 1 = lead / arp, 2 = pad / chord
inline int trackRole(int t, int m)
{
    if (m == mVOX || (m == mSUPER && t > 1)) return 2;
    return t == 0 ? 0 : 1;
}

// ---- sound of one track (machine kept). LEVEL, PAN and the DELAY / REVERB sends are left as they are:
// randomizing a sound must not change the mix.
inline void randomizeSound(float* values, int t, int m, RandGen& r)
{
    const int role = trackRole(t, m);
    for (int p = 0; p < 8; ++p) {
        const int def = kMachineDefs[m].def[p];
        const int v = r.chance(0.35f) ? r.range(0, 127) : clampi(def + r.range(-40, 40), 0, 127);
        tparam(values, t, p) = (float)v;
    }
    if (m == mSUPER) tparam(values, t, 2) = (float)r.range(60, 127);
    if (m == mFM) tparam(values, t, 4) = (float)r.range(0, 40);
    // amp
    if (role == 2) {
        tparam(values, t, tpATK) = (float)r.range(20, 80); tparam(values, t, tpDEC) = (float)r.range(50, 100);
        tparam(values, t, tpSUS) = (float)r.range(70, 120); tparam(values, t, tpREL) = (float)r.range(50, 100);
    } else {
        tparam(values, t, tpATK) = r.chance(0.8f) ? 0.f : (float)r.range(5, 40);
        tparam(values, t, tpDEC) = (float)r.range(20, 80);
        tparam(values, t, tpSUS) = (float)r.range(0, 110);
        tparam(values, t, tpREL) = (float)r.range(15, 60);
    }
    tparam(values, t, tpHOLD) = 0.f;
    tparam(values, t, tpPORT) = r.chance(0.4f) ? (float)r.range(10, 50) : 0.f;
    // filter
    tparam(values, t, tpCUTOFF) = (float)r.range(role == 0 ? 35 : 50, 115);
    tparam(values, t, tpRESO) = (float)r.range(0, role == 0 ? 100 : 70);
    tparam(values, t, tpFTYPE) = (float)filterTypeValue(r.chance(0.75f) ? (r.chance(0.5f) ? 0 : 1) : r.range(2, 4));
    tparam(values, t, tpLOWCUT) = r.chance(0.7f) ? 0.f : (float)r.range(0, 40);
    tparam(values, t, tpFATK) = r.chance(0.8f) ? 0.f : (float)r.range(10, 70);
    tparam(values, t, tpFDEC) = (float)r.range(20, 90);
    tparam(values, t, tpFENV) = (float)clampi(64 + r.range(-10, 50), 0, 127);
    tparam(values, t, tpKEYTRK) = (float)r.range(32, 96);
    // effects
    tparam(values, t, tpAMD) = r.chance(0.1f) ? (float)r.range(10, 60) : 0.f;
    tparam(values, t, tpAMF) = (float)r.range(20, 110);
    tparam(values, t, tpEQF) = (float)r.range(20, 110);
    tparam(values, t, tpEQG) = (float)clampi(64 + r.range(-12, 12), 0, 127);
    tparam(values, t, tpSRR) = r.chance(0.1f) ? (float)r.range(10, 60) : 0.f;
    tparam(values, t, tpDIST) = r.chance(0.3f) ? (float)r.range(5, 50) : 0.f;
    // LFOs: LFO 1 sometimes on the filter, LFO 2 sometimes vibrato, LFO 3 rarely anything
    for (int l = 0; l < 3; ++l) {
        const int b = kLfoBase[l];
        for (int k = 0; k < 8; ++k) tparam(values, t, lfoParam(l, k)) = kFxDefaults[lfoParam(l, k) - 8];
        const float pr = l == 0 ? 0.35f : (l == 1 ? 0.2f : 0.1f);
        if (!r.chance(pr)) continue;
        tparam(values, t, b) = (float)r.range(10, 100);
        tparam(values, t, b + 1) = (float)r.range(8, 50);
        tparam(values, t, b + 2) = (float)r.range(0, 127);
        tparam(values, t, b + 3) = (float)r.range(0, 127);
        const int dest = l == 0 ? tpCUTOFF : (l == 1 ? kLfoDests : r.range(0, kLfoDests - 1));
        tparam(values, t, b + 4) = (float)lfoDestValue(dest);
        if (l == 1) { tparam(values, t, b) = (float)r.range(60, 85); tparam(values, t, b + 1) = (float)r.range(5, 20); tparam(values, t, b + 2) = 25.f; }
    }
}

// ---- sequence of one track ----
inline void randomizeSequence(Pattern& pat, int t, int machine, RandGen& r, int root = -1, int scale = -1)
{
    pat.clearTrack(t);
    if (machine == mOFF) return;
    const int role = trackRole(t, machine);
    if (root < 0) root = r.range(0, 11);
    if (scale < 0) scale = r.range(0, kNumRndScales - 1);
    const RndScale& sc = kRndScales[scale];
    const int base = (role == 0 ? 36 : (role == 2 ? 60 : 60)) + root - (root > 6 ? 12 : 0);
    const int len = pat.trackLen[t] ? pat.trackLen[t] : 16;
    // a short motif repeated with variations sounds better than pure noise
    const int motifLen = r.chance(0.5f) ? 8 : (r.chance(0.5f) ? 4 : 16);
    int motif[16]; uint8_t mflags[16]; uint8_t mgate[16];
    const float density = role == 0 ? 0.6f : (role == 2 ? 0.2f : 0.45f);
    int deg = 0;
    for (int s = 0; s < motifLen; ++s) {
        mflags[s] = 0;
        float p = density;
        if (s % 4 == 0) p += 0.25f;
        if (role == 2 && s % 8 != 0) p *= 0.4f;
        if (r.chance(p)) {
            mflags[s] = sfTrig;
            if (r.chance(0.18f)) mflags[s] |= sfAccent;
            if (role != 2 && r.chance(0.15f)) mflags[s] |= sfSlide;
        }
        deg = clampi(deg + (r.chance(0.5f) ? r.range(-2, 2) : 0), -sc.n, sc.n * 2);
        if (r.chance(0.15f)) deg = 0;
        const int oct = deg >= 0 ? deg / sc.n : -((-deg + sc.n - 1) / sc.n);
        const int d = deg - oct * sc.n;
        motif[s] = base + oct * 12 + sc.s[d];
        if (role == 0 && r.chance(0.2f)) motif[s] = base + (r.chance(0.5f) ? 12 : 0);
        mgate[s] = (uint8_t)(role == 2 ? r.range(6, 9) : (r.chance(0.7f) ? r.range(2, 4) : r.range(5, 6)));
    }
    for (int s = 0; s < len; ++s) {
        const int ms = s % motifLen;
        uint8_t f = mflags[ms];
        int n = motif[ms];
        if (s >= motifLen && r.chance(0.12f)) { f = f ? 0 : sfTrig; }
        if (s >= motifLen && r.chance(0.1f)) n += sc.s[r.range(0, sc.n - 1)] - 12 * (r.chance(0.5f) ? 1 : 0);
        pat.flags[t][s] = f;
        pat.note[t][s] = (uint8_t)clampi(n, 12, 115);
        pat.gate[t][s] = mgate[ms];
        if (!f) continue;
        if (r.chance(0.04f)) pat.ratch[t][s] = (uint8_t)kRatchValues[r.range(1, 2)];
        if (r.chance(0.06f)) pat.cond[t][s] = (uint8_t)r.range(1, 6);
        if (r.chance(0.12f)) pat.locks[t][s][tpCUTOFF] = (uint8_t)r.range(40, 120);
    }
}

inline void randomizePattern(Pattern& pat, const Bank& kit, RandGen& r)
{
    const int root = r.range(0, 11), scale = r.range(0, kNumRndScales - 1);
    for (int t = 0; t < kTracks; ++t) {
        if (r.chance(t == 0 ? 0.05f : 0.3f)) { pat.clearTrack(t); continue; } // leave some space
        randomizeSequence(pat, t, kit.machine[t], r, root, scale);
    }
    pat.swing = (uint8_t)(r.chance(0.6f) ? 50 : r.range(52, 62));
}

// ---- everything: kit, master effects, arpeggiators, current pattern ----
inline void randomizeKit(float* values, Bank& kit, RandGen& r)
{
    static const int bass[] = {mSAW, mPULSE, mFM, mWAVE, mSAW};
    static const int any[] = {mSAW, mPULSE, mSUPER, mFM, mWAVE, mVOX, mPLUCK, mNOISE, mPLUCK, mFM, mWAVE};
    for (int t = 0; t < kTracks; ++t) {
        const int m = t == 0 ? bass[r.range(0, 4)] : any[r.range(0, (int)(sizeof(any) / sizeof(any[0])) - 1)];
        kit.machine[t] = (uint8_t)m;
        for (int p = 0; p < 8; ++p) tparam(values, t, p) = kMachineDefs[m].def[p];
        randomizeSound(values, t, m, r);
        kit.mute[t] = 0;
        kit.kit[t][kfLegato] = (uint8_t)(r.chance(0.3f) ? 1 : 0);
        kit.kit[t][kfArpMode] = (uint8_t)(t > 0 && r.chance(0.25f) ? r.range(1, 5) : 0);
        kit.kit[t][kfArpOct] = (uint8_t)r.range(0, 2);
        kit.kit[t][kfArpSpd] = (uint8_t)r.range(0, 2);
        kit.kit[t][kfArpChord] = (uint8_t)r.range(0, 9);
    }
    float* g = values + kGlobalBase;
    g[gECHO_TIME] = (float)r.range(30, 100); g[gECHO_FB] = (float)r.range(20, 80);
    g[gECHO_LPF] = (float)r.range(60, 120); g[gECHO_HPF] = (float)r.range(0, 50);
    g[gECHO_WIDTH] = (float)r.range(20, 127); g[gECHO_LEV] = (float)r.range(40, 90);
    g[gREV_SIZE] = (float)r.range(30, 110); g[gREV_DAMP] = (float)r.range(20, 110);
    g[gREV_GATE] = 0.f; g[gREV_HPF] = (float)r.range(0, 60);
    g[gREV_PRE] = (float)r.range(0, 50); g[gREV_LEV] = (float)r.range(40, 90);
}

} // namespace kn
