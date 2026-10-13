// Kinetik KMSP-6 — pattern bank, serialization and edit commands (shared DSP + UI)
#pragma once
#include "Params.hpp"
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <initializer_list>

namespace kn {

enum StepFlags : uint8_t { sfTrig = 1, sfAccent = 2, sfSlide = 4 };
static constexpr uint8_t kNoLock = 255;

struct Pattern {
    uint8_t length = 16;   // master length 1..kSteps (128): pattern changes happen at its end
    uint8_t scale = 0;     // index into kScaleMult
    uint8_t swing = 50;    // 50..80
    uint8_t accent = 64;   // 0..127
    uint8_t trackLen[kTracks]; // per-track length 1..kSteps (128)
    uint8_t flags[kTracks][kSteps];
    uint8_t locks[kTracks][kSteps][kLockable];
    uint8_t cond[kTracks][kSteps];   // index into kConds (0 = always)
    int8_t  micro[kTracks][kSteps];  // -11..+11 in 1/24 of a step
    uint8_t ratch[kTracks][kSteps];  // number of hits in the step (0/1 = single)
    uint8_t note[kTracks][kSteps];   // MIDI note of the trig (60 = C3)
    uint8_t gate[kTracks][kSteps];   // index into kGateSteps

    Pattern() { clear(); }
    void clear()
    {
        length = 16; scale = 0; swing = 50; accent = 64;
        std::memset(trackLen, 16, sizeof(trackLen));
        std::memset(flags, 0, sizeof(flags));
        std::memset(locks, kNoLock, sizeof(locks));
        std::memset(cond, 0, sizeof(cond)); std::memset(micro, 0, sizeof(micro)); std::memset(ratch, 0, sizeof(ratch));
        std::memset(note, 60, sizeof(note)); std::memset(gate, kDefaultGate, sizeof(gate));
    }
    void clearTrack(int t)
    {
        std::memset(flags[t], 0, sizeof(flags[t]));
        std::memset(locks[t], kNoLock, sizeof(locks[t]));
        std::memset(cond[t], 0, kSteps); std::memset(micro[t], 0, kSteps); std::memset(ratch[t], 0, kSteps);
        std::memset(note[t], 60, kSteps); std::memset(gate[t], kDefaultGate, kSteps);
    }
    void copyStep(int t, int dst, int src)
    {
        flags[t][dst] = flags[t][src]; cond[t][dst] = cond[t][src];
        micro[t][dst] = micro[t][src]; ratch[t][dst] = ratch[t][src];
        note[t][dst] = note[t][src]; gate[t][dst] = gate[t][src];
        std::memcpy(locks[t][dst], locks[t][src], kLockable);
    }
    // step of track t for absolute sequencer step k
    int trackStep(int t, int64_t k) const
    {
        const int L = trackLen[t] ? trackLen[t] : 16;
        return (int)(((k % L) + L) % L);
    }
    bool hasLocks(int t, int s) const
    {
        for (int p = 0; p < kLockable; ++p)
            if (locks[t][s][p] != kNoLock) return true;
        return false;
    }
};

struct Bank {
    Pattern patterns[kPatterns];
    // kit settings (not host parameters, so hosts can never send stale values for them)
    uint8_t machine[kTracks];
    uint8_t mute[kTracks];
    uint8_t kit[kTracks][kKitFields]; // output, MIDI channel, transpose, legato, key tracking, arpeggiator

    Bank() { resetKit(); }
    void resetKit()
    {
        for (int t = 0; t < kTracks; ++t) {
            machine[t] = kDefaultKit[t]; mute[t] = 0;
            for (int f = 0; f < kKitFields; ++f) kit[t][f] = kKitDefault[f];
        }
    }

    // factory state: default kit, empty sequencer
    void initDefault()
    {
        resetKit();
        for (auto& p : patterns) p.clear();
    }

};

// ---------------------------------------------------------------------------
// hex helpers
inline void appendHex(std::string& s, const uint8_t* d, size_t n)
{
    static const char* hx = "0123456789abcdef";
    s.reserve(s.size() + n * 2);
    for (size_t i = 0; i < n; ++i) { s.push_back(hx[d[i] >> 4]); s.push_back(hx[d[i] & 15]); }
}
inline int hexVal(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}
inline bool readHex(const char* s, uint8_t* d, size_t n)
{
    for (size_t i = 0; i < n; ++i) {
        const int a = hexVal(s[2*i]); if (a < 0) return false;
        const int b = hexVal(s[2*i+1]); if (b < 0) return false;
        d[i] = (uint8_t)(a * 16 + b);
    }
    return true;
}

// Track data blob: length + flags + locks + cond + micro + ratch + note + gate
static constexpr size_t kTrackBlob = 1 + kSteps + kSteps * kLockable + 5 * kSteps;

inline std::string trackToHex(const Pattern& p, int t)
{
    std::string s;
    appendHex(s, &p.trackLen[t], 1);
    appendHex(s, p.flags[t], kSteps);
    appendHex(s, &p.locks[t][0][0], kSteps * kLockable);
    appendHex(s, p.cond[t], kSteps);
    appendHex(s, (const uint8_t*)p.micro[t], kSteps);
    appendHex(s, p.ratch[t], kSteps);
    appendHex(s, p.note[t], kSteps);
    appendHex(s, p.gate[t], kSteps);
    return s;
}
inline bool trackFromHex(Pattern& p, int t, const char* h)
{
    if (std::strlen(h) < kTrackBlob * 2) return false;
    uint8_t tmp[kTrackBlob];
    if (!readHex(h, tmp, kTrackBlob)) return false;
    p.trackLen[t] = (uint8_t)(tmp[0] >= 1 && tmp[0] <= kSteps ? tmp[0] : 16);
    std::memcpy(p.flags[t], tmp + 1, kSteps);
    const uint8_t* q = tmp + 1 + kSteps;
    std::memcpy(&p.locks[t][0][0], q, kSteps * kLockable); q += kSteps * kLockable;
    std::memcpy(p.cond[t], q, kSteps); q += kSteps;
    std::memcpy(p.micro[t], q, kSteps); q += kSteps;
    std::memcpy(p.ratch[t], q, kSteps); q += kSteps;
    std::memcpy(p.note[t], q, kSteps); q += kSteps;
    std::memcpy(p.gate[t], q, kSteps);
    for (int s = 0; s < kSteps; ++s) {
        if (p.note[t][s] > 127) p.note[t][s] = 60;
        if (p.gate[t][s] >= kGateCount) p.gate[t][s] = kDefaultGate;
        if (p.cond[t][s] >= kNumConds) p.cond[t][s] = 0;
        if (p.micro[t][s] > kMicroMax || p.micro[t][s] < -kMicroMax) p.micro[t][s] = 0;
    }
    return true;
}

// ---------------------------------------------------------------------------
// Bank serialization (compact, sparse text)
inline std::string serializeBank(const Bank& b)
{
    std::string s = "KMSP2\n";
    char buf[96];
    for (int t = 0; t < kTracks; ++t) {
        std::snprintf(buf, sizeof(buf), "M %d %d\nU %d %d\n", t, b.machine[t], t, b.mute[t]);
        s += buf;
        for (int f = 0; f < kKitFields; ++f)
            if (b.kit[t][f] != kKitDefault[f]) { std::snprintf(buf, sizeof(buf), "K %d %d %d\n", t, f, b.kit[t][f]); s += buf; }
    }
    for (int pi = 0; pi < kPatterns; ++pi) {
        const Pattern& p = b.patterns[pi];
        std::snprintf(buf, sizeof(buf), "P %d %d %d %d %d\n", pi, p.length, p.scale, p.swing, p.accent);
        s += buf;
        for (int t = 0; t < kTracks; ++t)
            if (p.trackLen[t] != 16) { std::snprintf(buf, sizeof(buf), "T %d %d %d\n", pi, t, p.trackLen[t]); s += buf; }
        for (int t = 0; t < kTracks; ++t)
            for (int st = 0; st < kSteps; ++st) {
                if (p.flags[t][st]) {
                    std::snprintf(buf, sizeof(buf), "f %d %d %d %d\n", pi, t, st, p.flags[t][st]);
                    s += buf;
                }
                if (p.cond[t][st]) { std::snprintf(buf, sizeof(buf), "c %d %d %d %d\n", pi, t, st, p.cond[t][st]); s += buf; }
                if (p.micro[t][st]) { std::snprintf(buf, sizeof(buf), "m %d %d %d %d\n", pi, t, st, p.micro[t][st]); s += buf; }
                if (p.ratch[t][st] > 1) { std::snprintf(buf, sizeof(buf), "r %d %d %d %d\n", pi, t, st, p.ratch[t][st]); s += buf; }
                if (p.note[t][st] != 60) { std::snprintf(buf, sizeof(buf), "n %d %d %d %d\n", pi, t, st, p.note[t][st]); s += buf; }
                if (p.gate[t][st] != kDefaultGate) { std::snprintf(buf, sizeof(buf), "g %d %d %d %d\n", pi, t, st, p.gate[t][st]); s += buf; }
                for (int k = 0; k < kLockable; ++k)
                    if (p.locks[t][st][k] != kNoLock) {
                        std::snprintf(buf, sizeof(buf), "l %d %d %d %d %d\n", pi, t, st, k, p.locks[t][st][k]);
                        s += buf;
                    }
            }
    }
    return s;
}

inline int clampi(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }

// Apply one edit command. Commands are idempotent "set" operations so that replaying is harmless.
// Returns the pattern index touched, kKitTouched for kit edits, or -1 if invalid.
static constexpr int kKitTouched = 100;
inline int applyCommand(Bank& b, const char* line)
{
    char op[8] = {0};
    int n = 0;
    if (std::sscanf(line, "%7s%n", op, &n) != 1) return -1;
    const char* rest = line + n;
    int a = 0, c = 0, d = 0, e = 0, f = 0;
    auto validP = [](int p) { return p >= 0 && p < kPatterns; };
    auto validT = [](int t) { return t >= 0 && t < kTracks; };
    auto validS = [](int s) { return s >= 0 && s < kSteps; };

    if (!std::strcmp(op, "P")) { // P pat len scale swing accent
        if (std::sscanf(rest, "%d %d %d %d %d", &a, &c, &d, &e, &f) != 5 || !validP(a)) return -1;
        Pattern& p = b.patterns[a];
        p.length = (uint8_t)clampi(c, 1, kSteps);
        p.scale = (uint8_t)clampi(d, 0, 6);
        p.swing = (uint8_t)clampi(e, 50, 80);
        p.accent = (uint8_t)clampi(f, 0, 127);
        return a;
    }
    if (!std::strcmp(op, "M")) { // M track machine
        if (std::sscanf(rest, "%d %d", &a, &c) != 2 || !validT(a)) return -1;
        b.machine[a] = (uint8_t)clampi(c, 0, kMachines - 1);
        return kKitTouched;
    }
    if (!std::strcmp(op, "U")) { // U track mute(0/1)
        if (std::sscanf(rest, "%d %d", &a, &c) != 2 || !validT(a)) return -1;
        b.mute[a] = c ? 1 : 0;
        return kKitTouched;
    }
    if (!std::strcmp(op, "K")) { // K track field value : kit setting
        if (std::sscanf(rest, "%d %d %d", &a, &c, &d) != 3 || !validT(a) || c < 0 || c >= kKitFields) return -1;
        b.kit[a][c] = (uint8_t)clampi(d, 0, kKitMax[c]);
        return kKitTouched;
    }
    if (!std::strcmp(op, "c") || !std::strcmp(op, "m") || !std::strcmp(op, "r") || !std::strcmp(op, "n") || !std::strcmp(op, "g")) {
        if (std::sscanf(rest, "%d %d %d %d", &a, &c, &d, &e) != 4 || !validP(a) || !validT(c) || !validS(d)) return -1;
        Pattern& p = b.patterns[a];
        if (op[0] == 'c') p.cond[c][d] = (uint8_t)clampi(e, 0, kNumConds - 1);
        else if (op[0] == 'm') p.micro[c][d] = (int8_t)clampi(e, -kMicroMax, kMicroMax);
        else if (op[0] == 'r') p.ratch[c][d] = (uint8_t)clampi(e, 0, 8);
        else if (op[0] == 'n') p.note[c][d] = (uint8_t)clampi(e, 0, 127);
        else p.gate[c][d] = (uint8_t)clampi(e, 0, kGateCount - 1);
        return a;
    }
    if (!std::strcmp(op, "T")) { // T pat track length
        if (std::sscanf(rest, "%d %d %d", &a, &c, &d) != 3 || !validP(a) || !validT(c)) return -1;
        b.patterns[a].trackLen[c] = (uint8_t)clampi(d, 1, kSteps);
        return a;
    }
    if (!std::strcmp(op, "f")) { // f pat track step flags
        if (std::sscanf(rest, "%d %d %d %d", &a, &c, &d, &e) != 4 || !validP(a) || !validT(c) || !validS(d)) return -1;
        b.patterns[a].flags[c][d] = (uint8_t)(e & 7);
        return a;
    }
    if (!std::strcmp(op, "l")) { // l pat track step param value(255 = clear)
        if (std::sscanf(rest, "%d %d %d %d %d", &a, &c, &d, &e, &f) != 5 || !validP(a) || !validT(c) || !validS(d)
            || e < 0 || e >= kLockable) return -1;
        b.patterns[a].locks[c][d][e] = (f == kNoLock) ? kNoLock : (uint8_t)clampi(f, 0, 127);
        return a;
    }
    if (!std::strcmp(op, "cl")) { // cl pat track step : clear all locks of a step
        if (std::sscanf(rest, "%d %d %d", &a, &c, &d) != 3 || !validP(a) || !validT(c) || !validS(d)) return -1;
        std::memset(b.patterns[a].locks[c][d], kNoLock, kLockable);
        return a;
    }
    if (!std::strcmp(op, "ct")) { // ct pat track : clear track
        if (std::sscanf(rest, "%d %d", &a, &c) != 2 || !validP(a) || !validT(c)) return -1;
        b.patterns[a].clearTrack(c);
        return a;
    }
    if (!std::strcmp(op, "cp")) { // cp pat : clear pattern
        if (std::sscanf(rest, "%d", &a) != 1 || !validP(a)) return -1;
        b.patterns[a].clear();
        return a;
    }
    if (!std::strcmp(op, "tr")) { // tr pat track hexblob : set whole track
        if (std::sscanf(rest, "%d %d %n", &a, &c, &d) < 2 || !validP(a) || !validT(c)) return -1;
        if (!trackFromHex(b.patterns[a], c, rest + d)) return -1;
        return a;
    }
    if (!std::strcmp(op, "pc")) { // pc dst src : copy pattern
        if (std::sscanf(rest, "%d %d", &a, &c) != 2 || !validP(a) || !validP(c)) return -1;
        if (a != c) b.patterns[a] = b.patterns[c];
        return a;
    }
    return -1;
}

// Parse a full bank. Returns false if the text isn't a bank.
inline bool deserializeBank(Bank& b, const char* text)
{
    if (!text || (std::strncmp(text, "KMSP1", 5) != 0 && std::strncmp(text, "KMSP2", 5) != 0)) return false;
    const bool v1 = text[4] == '1';   // first filter layout: convert its locks
    for (auto& p : b.patterns) p.clear();
    b.resetKit();
    const char* s = text;
    std::string line;
    while (*s) {
        const char* e = std::strchr(s, '\n');
        const size_t len = e ? (size_t)(e - s) : std::strlen(s);
        line.assign(s, len);
        if (v1 && line.size() > 2 && line[0] == 'l' && line[1] == ' ') {
            // l pat track step param value: old filter params 16..23 = BASE WDTH HPQ LPQ FATK FDEC BOF WOF
            int a = 0, c = 0, d = 0, e2 = 0, f = 0;
            if (std::sscanf(line.c_str() + 2, "%d %d %d %d %d", &a, &c, &d, &e2, &f) == 5 && e2 >= 16 && e2 <= 23) {
                static const int to[8] = {-1, tpCUTOFF, -1, tpRESO, tpFATK, tpFDEC, tpFENV, tpFENV};
                if (to[e2 - 16] >= 0) { char nl[64]; std::snprintf(nl, sizeof(nl), "l %d %d %d %d %d", a, c, d, to[e2 - 16], f); applyCommand(b, nl); }
            } else applyCommand(b, line.c_str());
        } else if (!line.empty() && line.rfind("KMSP", 0) != 0) applyCommand(b, line.c_str());
        s += len;
        if (*s == '\n') ++s;
    }
    return true;
}

} // namespace kn
