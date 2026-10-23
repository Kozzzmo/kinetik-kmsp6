// Kinetik — morph scenes (identical in kinetik-kdsp16 and kinetik-kmsp6).
// Two sound snapshots (A and B) saved with the bank. The MORPH fader (global gMORPH, host-automatable), the per-step
// MORPH lane of the pattern and any LFO set to the MORPH destination blend the sound from A to B in the DSP:
// for every parameter that differs between the two scenes the engine plays a + (b - a) * morph; switch-like
// parameters jump at 50 %. An empty scene means "the knobs as they are", so one scene alone morphs knobs -> scene.
// Tracks whose machine differs between the scenes keep their synth knobs (0-7).
#pragma once
#include "Params.hpp"
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <string>
#include <cmath>
#include <algorithm>

namespace kn {

static constexpr int kSceneValues = kGlobalBase + gOUT_STEP;   // every saved parameter

// parameters a scene stores (not transport, tempo, pattern or the morph itself)
inline bool sceneParam(int i)
{
    if (i < 0 || i >= kSceneValues) return false;
    int t, p;
    if (tpOf(i, t, p)) return true;
    const int g = i - kGlobalBase;
    return g != gPATTERN && g != gPLAY && g != gTEMPO && g != gMORPH;
}

// parameters that jump at 50 % instead of blending (types, waves, destinations, models, divisions)
inline bool morphSwitch(int i)
{
    int t, p;
    if (tpOf(i, t, p)) {
        if (p == tpFTYPE) return true;
        constexpr int nl = (int)(sizeof(kLfoBase) / sizeof(kLfoBase[0]));
        for (int l = 0; l < nl; ++l)
            for (int k = 2; k <= 6; ++k) if (p == lfoParam(l, k)) return true;   // WAVE MODE DEST TARGET SYNC
        return false;
    }
    const int g = i - kGlobalBase;
    return g == gECHO_TIME || g == gECHO_MODEL || g == gREV_MODEL || g == gCHO_MODEL || g == gGL_SIZE || g == gGL_LEN || g == gMF_ON;
}

struct Scenes {
    uint8_t set[2] = {0, 0};
    uint8_t mach[2][kTracks] = {};
    uint8_t val[2][kSceneValues] = {};
    void clear() { std::memset(this, 0, sizeof(*this)); }
    bool active() const { return set[0] || set[1]; }
};

// bank text line of one scene: "Z a <machines hex> <values hex>" (values rounded to 0..127)
inline std::string sceneLine(const Scenes& s, int a)
{
    static const char* hx = "0123456789abcdef";
    std::string o = "Z " + std::to_string(a) + " ";
    for (int t = 0; t < kTracks; ++t) { o += hx[s.mach[a][t] >> 4]; o += hx[s.mach[a][t] & 15]; }
    o += ' ';
    for (int i = 0; i < kSceneValues; ++i) { o += hx[s.val[a][i] >> 4]; o += hx[s.val[a][i] & 15]; }
    return o;
}
inline int hexNibble(char c) { return c >= '0' && c <= '9' ? c - '0' : (c >= 'a' && c <= 'f' ? c - 'a' + 10 : (c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1)); }
// parses the part after "Z "; false (scene unchanged) if it is malformed
inline bool sceneFromLine(Scenes& s, const char* rest, int maxMachine)
{
    int a = -1, n = 0;
    if (std::sscanf(rest, "%d %n", &a, &n) < 1 || a < 0 || a > 1) return false;
    const char* p = rest + n;
    uint8_t mach[kTracks], val[kSceneValues];
    auto byte = [&](uint8_t& out) {
        const int h = hexNibble(p[0]); if (h < 0) return false;
        const int l = hexNibble(p[1]); if (l < 0) return false;
        out = (uint8_t)(h * 16 + l); p += 2; return true;
    };
    // machines: one byte per track (a scene saved with fewer tracks has fewer; the others stay OFF)
    std::memset(mach, 0, sizeof(mach));
    for (int t = 0; t < kTracks && *p != ' '; ++t) { if (!byte(mach[t])) return false; if (mach[t] > maxMachine) mach[t] = 0; }
    if (*p != ' ') return false;
    ++p;
    // a scene saved by an older version has fewer values (parameters added since): those take their default
    int got = 0;
    for (; got < kSceneValues; ++got) {
        if (hexNibble(p[0]) < 0) break;
        if (!byte(val[got])) return false;
        if (val[got] > 127) val[got] = 127;
    }
    if (got < kGlobalBase + gECHO_TIME) return false;   // not even the track parameters and the transport
    for (int i = got; i < kSceneValues; ++i) val[i] = (uint8_t)std::lround(std::min(127.f, globalDefault(i - kGlobalBase)));
    std::memcpy(s.mach[a], mach, sizeof(mach));
    std::memcpy(s.val[a], val, sizeof(val));
    s.set[a] = 1;
    return true;
}

} // namespace kn
