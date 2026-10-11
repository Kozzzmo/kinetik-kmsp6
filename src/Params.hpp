// Kinetik KMSP-6 — shared definitions (DSP + UI)
#pragma once
#include <cstdint>
#include <cstdio>

namespace kn {

static constexpr int kTracks = 6;
static constexpr int kSteps = 64;
static constexpr int kPatterns = 16;

// ---- per-track parameters (0..127), all of them can be locked per step ----
enum TrackParam {
    tpSyn0 = 0,                                                     // 0..7  machine parameters
    tpATK = 8, tpHOLD, tpDEC, tpSUS, tpREL, tpVOL, tpPAN, tpPORT,   // AMP
    tpBASE = 16, tpWDTH, tpHPQ, tpLPQ, tpFATK, tpFDEC, tpBOF, tpWOF, // FILTER
    tpAMD = 24, tpAMF, tpEQF, tpEQG, tpSRR, tpDIST, tpDEL, tpREV,   // EFX
    tpLFO1 = 32, tpLFO2 = 38, tpLFO3 = 44,                           // 3 LFOs x 6 params
    kLockable = 50,
    kTrackParams = 50
};
static constexpr int kLfoBase[3] = {tpLFO1, tpLFO2, tpLFO3};
static constexpr int kLfoDests = 32; // an LFO can move params 0..31 of any track

static const char* const kFxNames[42] = {
    "ATK", "HOLD", "DEC", "SUS", "REL", "VOL", "PAN", "PORT",
    "BASE", "WDTH", "HPQ", "LPQ", "ATK", "DEC", "BOF", "WOF",
    "AMD", "AMF", "EQF", "EQG", "SRR", "DIST", "DEL", "REV",
    "SPD", "DEP", "SHP", "TYP", "DEST", "TRK",
    "SPD", "DEP", "SHP", "TYP", "DEST", "TRK",
    "SPD", "DEP", "SHP", "TYP", "DEST", "TRK",
};
static const uint8_t kFxDefaults[42] = {
    0, 0, 50, 90, 40, 100, 64, 0,
    0, 96, 0, 30, 0, 50, 64, 82,
    0, 64, 64, 64, 0, 0, 0, 0,
    64, 0, 0, 0, 0, 0,
    40, 0, 0, 0, 0, 0,
    20, 0, 0, 0, 0, 0,
};

// ---- global parameters ----
enum GlobalParam {
    gPATTERN = 0, gPLAY, gTEMPO,
    gECHO_TIME, gECHO_FB, gECHO_LPF, gECHO_HPF, gECHO_WIDTH, gECHO_LEV,
    gREV_SIZE, gREV_DAMP, gREV_GATE, gREV_HPF, gREV_PRE, gREV_LEV,
    gEQ_LOW, gEQ_HIGH, gCMP_THR, gCMP_RAT, gCMP_ATK, gCMP_REL, gDRIVE, gVOLUME,
    gOUT_STEP, gOUT_LEDS, gOUT_PATTERN,
    kGlobalParams
};
static constexpr int kGlobalBase = kTracks * kTrackParams;
static constexpr int kTotalParams = kGlobalBase + kGlobalParams;

static const char* const kMasterNames[20] = {
    "TIME", "FB", "LPF", "HPF", "WIDTH", "LEV",
    "DECAY", "DAMP", "GATE", "HPF", "PRE", "LEV",
    "LOW", "HIGH", "THRS", "RATIO", "ATK", "REL", "DRIVE", "VOL"
};
static const uint8_t kMasterDefaults[20] = {
    48, 50, 100, 10, 64, 64,
    60, 64, 0, 20, 10, 64,
    64, 64, 127, 40, 20, 50, 0, 100
};

// ---- machines ----
enum MachineId { mOFF, mSAW, mPULSE, mSUPER, mFM, mWAVE, mVOX, mPLUCK, mNOISE, kMachines };

struct MachineDef {
    const char* name;
    const char* p[8];
    uint8_t def[8];
};

static const MachineDef kMachineDefs[kMachines] = {
    {"OFF",   {"-", "-", "-", "-", "-", "-", "-", "-"},                    {0, 0, 0, 0, 0, 0, 0, 0}},
    {"SAW",   {"SHP1", "SHP2", "DTUN", "MIX", "OCT2", "PW", "SUB", "NOIS"}, {0, 0, 72, 64, 64, 64, 0, 0}},
    {"PULSE", {"PW", "PWMS", "PWMD", "SYNC", "RING", "SUB", "NOIS", "CRSH"}, {64, 40, 30, 0, 0, 0, 0, 0}},
    {"SUPER", {"DTUN", "MIX", "VOIC", "SHP", "SUB", "OCT", "NOIS", "DRIV"}, {50, 90, 127, 0, 0, 64, 0, 0}},
    {"FM",    {"RAT1", "IDX1", "RAT2", "IDX2", "FDBK", "ALGO", "MDEC", "MSUS"}, {32, 50, 64, 0, 0, 0, 50, 40}},
    {"WAVE",  {"POS", "SCAN", "SDEP", "OCT2", "DTUN", "MIX2", "FOLD", "CRSH"}, {0, 40, 0, 64, 70, 0, 0, 0}},
    {"VOX",   {"VOWL", "FORM", "BRTH", "RES", "VIBD", "VIBR", "GLOT", "CHOR"}, {0, 64, 20, 60, 0, 64, 40, 0}},
    {"PLUCK", {"DEC", "BRIT", "PICK", "DAMP", "STIF", "NOIS", "BODY", "DRIV"}, {80, 80, 50, 40, 0, 50, 0, 0}},
    {"NOISE", {"COLR", "RES", "TRCK", "CRSH", "GRAN", "TONE", "WDTH", "DRIV"}, {64, 60, 100, 0, 0, 0, 40, 0}},
};

static const uint8_t kDefaultKit[kTracks] = {mSAW, mPULSE, mSUPER, mFM, mWAVE, mPLUCK};

inline uint8_t trackParamDefault(int track, int p)
{
    const int m = kDefaultKit[track];
    if (p < 8) return kMachineDefs[m].def[p];
    if (p < kTrackParams) return kFxDefaults[p - 8];
    return 0;
}
inline const char* trackParamName(int machine, int p)
{
    if (p < 8) return kMachineDefs[machine].p[p];
    if (p < kTrackParams) return kFxNames[p - 8];
    return "?";
}

// ---- kit settings (TRACK page + arpeggiator), stored with the kit, not host parameters ----
enum KitField { kfOut = 0, kfMidi, kfTrnsp, kfLegato, kfKeyTrk, kfArpMode, kfArpOct, kfArpSpd, kfArpChord, kKitFields };
static const char* const kKitNames[kKitFields] = {"OUT", "MIDI", "TRNSP", "LEGATO", "KEYTRK", "ARP", "OCT", "SPEED", "CHORD"};
static const int kKitMax[kKitFields] = {3, 16, 48, 1, 4, 5, 3, 5, 9};
static const uint8_t kKitDefault[kKitFields] = {0, 0, 24, 0, 2, 0, 0, 1, 0};
static const char* const kArpModes[6] = {"OFF", "UP", "DOWN", "UP-DN", "RAND", "ORDER"};
static const float kArpSpeeds[6] = {0.25f, 0.5f, 1.f, 2.f, 3.f, 4.f};  // length of one arp note, in steps
static const char* const kArpSpeedNames[6] = {"1/4", "1/2", "1", "2", "3", "4"}; // length of one arp note, in steps
struct ChordDef { const char* name; int n; int notes[4]; };
static const ChordDef kChords[10] = {
    {"NOTE", 1, {0}}, {"MAJ", 3, {0, 4, 7}}, {"MIN", 3, {0, 3, 7}}, {"SUS4", 3, {0, 5, 7}},
    {"MAJ7", 4, {0, 4, 7, 11}}, {"MIN7", 4, {0, 3, 7, 10}}, {"DOM7", 4, {0, 4, 7, 10}},
    {"5TH", 2, {0, 7}}, {"OCTV", 2, {0, 12}}, {"DIM", 3, {0, 3, 6}},
};
inline const char* kitValueText(int field, int v, char* buf, int n)
{
    static const char* out[4] = {"MAIN", "OUT 2", "OUT 3", "OUT 4"};
    static const char* kt[5] = {"0%", "25%", "50%", "75%", "100%"};
    switch (field) {
    case kfOut: return out[v < 0 ? 0 : (v > 3 ? 3 : v)];
    case kfMidi: if (v <= 0) return "AUTO"; std::snprintf(buf, n, "CH %d", v); return buf;
    case kfTrnsp: std::snprintf(buf, n, "%+d", v - 24); return buf;
    case kfLegato: return v ? "ON" : "OFF";
    case kfKeyTrk: return kt[v < 0 ? 0 : (v > 4 ? 4 : v)];
    case kfArpMode: return kArpModes[v < 0 ? 0 : (v > 5 ? 5 : v)];
    case kfArpOct: std::snprintf(buf, n, "%d OCT", v + 1); return buf;
    case kfArpSpd: return kArpSpeedNames[v < 0 ? 0 : (v > 5 ? 5 : v)];
    default: return kChords[v < 0 ? 0 : (v > 9 ? 9 : v)].name;
    }
}

// ---- LFO ----
static const char* const kLfoShapes[6] = {"TRI", "SINE", "SQR", "SAW+", "SAW-", "RND"};
static const char* const kLfoTypes[3] = {"FREE", "TRIG", "HOLD"};
inline int lfoShapeIndex(int v) { return v * 6 / 128; }
inline int lfoTypeIndex(int v) { return v * 3 / 128; }
inline int lfoDestIndex(int v) { return v * (kLfoDests + 1) / 128; }               // 0..31 params, 32 = PITCH
inline int lfoDestValue(int p) { return (p * 128 + kLfoDests) / (kLfoDests + 1); }
inline int lfoTrackIndex(int v) { return v * (kTracks + 1) / 128; } // 0 = this track, 1..6
inline int lfoTrackValue(int t) { return (t * 128 + kTracks) / (kTracks + 1); }

// ---- sequencer ----
static const float kScaleMult[7] = {1.f, 2.f, 0.75f, 1.5f, 0.5f, 0.25f, 0.125f};
static const char* const kScaleNames[7] = {"1x", "2x", "3/4x", "3/2x", "1/2x", "1/4x", "1/8x"};
static constexpr int kGateCount = 12;
static const float kGateSteps[kGateCount] = {0.125f, 0.25f, 0.5f, 0.75f, 1.f, 1.5f, 2.f, 3.f, 4.f, 6.f, 8.f, 16.f};
static const char* const kGateNames[kGateCount] = {"1/8", "1/4", "1/2", "3/4", "1", "1.5", "2", "3", "4", "6", "8", "16"};
static constexpr int kDefaultGate = 3; // 3/4 step
static constexpr int kRatchCount = 6;
static const uint8_t kRatchValues[kRatchCount] = {1, 2, 3, 4, 6, 8};
enum CondKind { ckAlways, ckProb, ckFill, ckNotFill, ckPre, ckNotPre, ckFirst, ckNotFirst, ckRatio };
struct CondDef { const char* name; CondKind kind; int a, b; };
static const CondDef kConds[] = {
    {"", ckAlways, 0, 0},
    {"90%", ckProb, 90, 0}, {"75%", ckProb, 75, 0}, {"50%", ckProb, 50, 0}, {"33%", ckProb, 33, 0},
    {"25%", ckProb, 25, 0}, {"10%", ckProb, 10, 0},
    {"FILL", ckFill, 0, 0}, {"!FILL", ckNotFill, 0, 0}, {"PRE", ckPre, 0, 0}, {"!PRE", ckNotPre, 0, 0},
    {"1ST", ckFirst, 0, 0}, {"!1ST", ckNotFirst, 0, 0},
    {"1:2", ckRatio, 1, 2}, {"2:2", ckRatio, 2, 2}, {"1:3", ckRatio, 1, 3}, {"2:3", ckRatio, 2, 3},
    {"3:3", ckRatio, 3, 3}, {"1:4", ckRatio, 1, 4}, {"2:4", ckRatio, 2, 4}, {"3:4", ckRatio, 3, 4},
    {"4:4", ckRatio, 4, 4},
};
static constexpr int kNumConds = (int)(sizeof(kConds) / sizeof(kConds[0]));
static constexpr int kMicroMax = 11;

// note names, Ableton convention: MIDI 60 = C3
inline const char* noteName(int n, char* buf, int len)
{
    static const char* names[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
    n = n < 0 ? 0 : (n > 127 ? 127 : n);
    std::snprintf(buf, len, "%s%d", names[n % 12], n / 12 - 2);
    return buf;
}

static constexpr int kEchoDivs = 12;
static const float kEchoBeats[kEchoDivs] = {
    1.f/16, 1.f/8, 1.f/6, 1.f/4, 1.f/3, 3.f/8, 1.f/2, 2.f/3, 3.f/4, 1.f, 1.5f, 2.f
};
static const char* const kEchoNames[kEchoDivs] = {
    "1/64", "1/32", "1/16T", "1/16", "1/8T", "1/16D", "1/8", "1/4T", "1/8D", "1/4", "1/4D", "1/2"
};
inline int echoDivIndex(int v) { return v * kEchoDivs / 128; }

} // namespace kn
