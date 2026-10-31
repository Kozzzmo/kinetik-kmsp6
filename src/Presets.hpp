// Kinetik KMSP-6 — factory presets (kit + patterns). All original material.
#pragma once
#include "Random.hpp"
#include "FxPresets.hpp"
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
    constexpr int oldTP = 50, oldGB = kLegacyTracks * oldTP;
    if (idx < oldGB) return tpIndex(idx / oldTP, idx % oldTP);
    return kGlobalBase + (idx - oldGB);
}

struct PB { // preset builder
    float* v; Bank& b;
    PB(float* values, Bank& bank) : v(values), b(bank)
    {
        b.initDefault();
        for (int t = 0; t < kTracks; ++t)
            for (int p = 0; p < kTrackParams; ++p) tparam(v, t, p) = trackParamDefault(t, p);
        for (int g = gECHO_TIME; g < gOUT_STEP; ++g) v[kGlobalBase + g] = globalDefault(g);
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
    static int tpi(int t, int p) { return tpIndex(t, p); }
    static int gpi(int g) { return kGlobalBase + g; }
    void lane(int pat, std::initializer_list<int> vals)
    {
        int st = 0;
        for (int x : vals) { if (st >= kSteps) break; b.patterns[pat].morph[st++] = x < 0 ? kNoLock : (uint8_t)clampi(x, 0, 127); }
    }
    void speed(int t, int sp) { b.kit[t][kfSpeed] = (uint8_t)sp; }   // index into kTrackSpeedMult
};

// gate indices: 0 1/8, 1 1/4, 2 1/2, 3 3/4, 4 1, 5 1.5, 6 2, 7 3, 8 4, 9 6, 10 8, 11 16
// --------------------------------------------------------------------------------------------
static void presetInit(PB& b)
{
    (void)b; // default kit, base parameters, empty sequencer
}

#include "PresetsV2.hpp"

// factory preset: expReso = voiced with the 0.6 RESONANCE curve; cat = style (kPresetCats); trimDb = level trim so every
// preset plays at about the same loudness (-15 LUFS, measured with tests/audit.cpp --fit)
struct PresetDef { const char* name; const char* desc; void (*build)(PB&); bool expReso = false; int cat = 0; float trimDb = 0.f; };
enum PresetCat { pcBasics, pcAcid, pcTechno, pcDub, pcSynthwave, pcAmbient, pcRetro, pcExperimental, pcHouse, kPresetCatCount };
static const char* const kPresetCats[kPresetCatCount] = {"BASICS", "ACID", "TECHNO", "DUB", "SYNTHWAVE", "AMBIENT", "RETRO", "EXPERIMENTAL", "HOUSE"};

static const PresetDef kPresets[] = {
    {"Init", "base parameters, empty sequencer", presetInit, false, pcBasics, 0.f},
    {"Demo Song", "bass, stab and pad, two bars to start from", m2Demo, true, pcBasics, 14.3f},
    {"Full Band", "all eight tracks over 4 bars, chorus pattern in a CHAIN", m2FullBand, true, pcBasics, 0.8f},
    {"Acid Pressure", "64-step acid line with building filter envelope, answer line, B a fourth up", m2Acid, true, pcAcid, -3.6f},
    {"Detroit Strings", "128 BPM: rolling bass, chord stabs changing shape, strings, breakdown", m2Detroit, true, pcTechno, 4.9f},
    {"Hard Groove", "136 BPM: rolling FM bass with ghost notes, pulse stabs, metal hits", m2HardGroove, true, pcTechno, 7.1f},
    {"Berlin Sequence", "16th sequence swept over 4 bars, 12-step counter line, pads, choir", m2Berlin, true, pcTechno, 1.4f},
    {"Minimal Plucks", "plucks of 16, 13 and 11 steps drifting, FM bass, sweeping pad", m2MinimalPlucks, true, pcTechno, 14.1f},
    {"K-House Keys", "124 BPM: FM bass, off-beat organ chords, plucks, vocal hook", m2KHouse, true, pcHouse, 14.4f},
    {"Deep Garage", "130 BPM swung: sliding pulse bass, organ minor 9ths, vocal chop", m2Garage, true, pcHouse, 20.0f},
    {"Dub Chords", "minor 7th stab into a long ducking tape echo, sub, hiss", m2DubChords, true, pcDub, 4.3f},
    {"Trance Gate", "138 BPM: 16th-gated pad, pluck arp, supersaw lead, riser pattern", m2Trance, true, pcSynthwave, 2.2f},
    {"Night Drive", "100 BPM: eighth saw bass, supersaw pad, pulse lead, bells", m2Synthwave, true, pcSynthwave, 0.6f},
    {"Electro Funk", "118 BPM: sliding pulse bass, saw stabs, talking vox, ratcheted hats", m2ElectroFunk, true, pcRetro, 18.2f},
    {"Chiptune", "140 BPM pulse waves: bass, lead, arp and noise drums", m2Chiptune, true, pcRetro, 2.2f},
    {"Lo-Fi Keys", "84 BPM: FM e-piano with tremolo, wobbling organ, plucked bass, hiss", m2LofiKeys, true, pcRetro, 11.9f},
    {"Chaos Arps", "two arpeggiators cut up by CHAOS over a dry bass", m2ChaosArps, true, pcExperimental, 9.6f},
    {"Ambient Drift", "80 BPM: pads, choir, bells and plucks into a shimmer reverb", m2Ambient, true, pcAmbient, -5.2f},
};
static constexpr int kNumPresets = (int)(sizeof(kPresets) / sizeof(kPresets[0]));

inline void convertResoV05(float* values, Bank& bank);
// level trim in dB: master VOLUME first (gain = 1.2 (v / 127)^2), what it cannot give goes to the track levels
inline void applyLevelTrim(float* values, float db, Bank* bank = nullptr)
{
    if (std::fabs(db) < 0.05f) return;
    float& mv = values[kGlobalBase + gVOLUME];
    const float gain = 1.2f * (mv / 127.f) * (mv / 127.f) * std::pow(10.f, db / 20.f);
    float v = 127.f * std::sqrt(gain / 1.2f);
    if (v <= 127.f) { mv = std::round(v); return; }
    mv = 127.f;
    // the rest goes to the track levels (knobs and VOL locks), all by the same factor so the balance stays: at most up
    // to the loudest track (a quiet preset may stay a little under the target rather than lose its mix)
    float top = 1.f;
    for (int t = 0; t < kTracks; ++t) top = std::max(top, values[tpIndex(t, tpVOL)]);
    if (bank) for (const auto& P : bank->patterns) for (int t = 0; t < kTracks; ++t) for (int s = 0; s < kSteps; ++s)
        if (P.locks[t][s][tpVOL] != kNoLock) top = std::max(top, (float)P.locks[t][s][tpVOL]);
    const float extra = std::min(std::sqrt(gain / 1.2f), 127.f / top);   // track gain = (v / 127)^2
    for (int t = 0; t < kTracks; ++t) { float& tv = values[tpIndex(t, tpVOL)]; tv = std::round(std::min(127.f, tv * extra)); }
    if (bank) for (auto& P : bank->patterns) for (int t = 0; t < kTracks; ++t) for (int s = 0; s < kSteps; ++s) {
        uint8_t& l = P.locks[t][s][tpVOL]; if (l != kNoLock) l = (uint8_t)std::min(127.f, std::round(l * extra)); }
}

inline void buildPreset(int i, float* values, Bank& bank)
{
    PB b(values, bank);
    if (i < 0 || i >= kNumPresets) return;
    kPresets[i].build(b);
    if (!kPresets[i].expReso) convertResoV05(values, bank);
    applyLevelTrim(values, kPresets[i].trimDb, &bank);
}
// the presets voiced with the linear RESONANCE curve used before 0.6: their values (knobs and locks) converted to the
// exponential curve (resoQ) so they keep the same resonance
inline void convertResoV05(float* values, Bank& bank)
{
    auto conv = [](float v, float qMin, float span) {
        const float q = qMin + clampf(v, 0.f, 127.f) / 127.f * span;
        const float e = std::log(q / qMin) / std::log((qMin + span) / qMin);
        return std::round(127.f * std::pow(clampf(e, 0.f, 1.f), 0.8f));
    };
    for (int t = 0; t < kTracks; ++t) values[tpIndex(t, tpRESO)] = conv(values[tpIndex(t, tpRESO)], 0.5f, 17.f);
    values[kGlobalBase + gMF_RESO] = conv(values[kGlobalBase + gMF_RESO], 0.707f, 9.f);
    for (auto& pat : bank.patterns)
        for (int t = 0; t < kTracks; ++t)
            for (int st = 0; st < kSteps; ++st)
                if (pat.locks[t][st][tpRESO] != kNoLock) pat.locks[t][st][tpRESO] = (uint8_t)conv(pat.locks[t][st][tpRESO], 0.5f, 17.f);
}

} // namespace kn
