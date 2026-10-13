// Kinetik KMSP-6 — shared definitions (DSP + UI)
#pragma once
#include <cstdint>
#include <cstdio>
#include <cmath>

namespace kn {

static constexpr int kTracks = 6;
static constexpr int kSteps = 128;
static constexpr int kPatterns = 16;

// ---- per-track parameters (0..127), all of them can be locked per step ----
enum TrackParam {
    tpSyn0 = 0,                                                     // 0..7  machine parameters
    tpATK = 8, tpHOLD, tpDEC, tpSUS, tpREL, tpVOL, tpPAN, tpPORT,          // AMP
    tpCUTOFF = 16, tpRESO, tpFTYPE, tpLOWCUT, tpFATK, tpFDEC, tpFENV, tpKEYTRK, // FILTER
    tpAMD = 24, tpAMF, tpEQF, tpEQG, tpSRR, tpDIST, tpDEL, tpREV,          // FX
    tpLFO1 = 32, tpLFO2 = 38, tpLFO3 = 44,                                  // 3 LFOs x 6 params
    tpLFOX = 50,                                                            // 3 LFOs x (SYNC, PHASE)
    kLockable = 56,
    kTrackParams = 56
};
static constexpr int kLfoBase[3] = {tpLFO1, tpLFO2, tpLFO3};
static constexpr int kLfoDests = 32; // an LFO can move params 0..31 of any track (index 32 = pitch)
// parameter k (0..7) of LFO l: RATE DEPTH WAVE MODE DEST TARGET SYNC PHASE
inline int lfoParam(int l, int k) { return k < 6 ? kLfoBase[l] + k : tpLFOX + l * 2 + (k - 6); }

static const char* const kFxNames[kTrackParams - 8] = {
    "ATTACK", "HOLD", "DECAY", "SUSTAIN", "RELEASE", "LEVEL", "PAN", "GLIDE",
    "CUTOFF", "RESO", "TYPE", "LOW CUT", "ENV ATK", "ENV DEC", "ENV AMT", "KEY TRK",
    "TREMOLO", "TREM RATE", "EQ FREQ", "EQ GAIN", "CRUSH", "DRIVE", "DELAY", "REVERB",
    "RATE", "DEPTH", "WAVE", "MODE", "DEST", "TARGET",
    "RATE", "DEPTH", "WAVE", "MODE", "DEST", "TARGET",
    "RATE", "DEPTH", "WAVE", "MODE", "DEST", "TARGET",
    "SYNC", "PHASE", "SYNC", "PHASE", "SYNC", "PHASE",
};
static const uint8_t kFxDefaults[kTrackParams - 8] = {
    0, 0, 50, 90, 40, 100, 64, 0,
    96, 30, 0, 0, 0, 50, 82, 64,
    0, 64, 64, 64, 0, 0, 0, 0,
    64, 0, 0, 0, 0, 0,
    40, 0, 0, 0, 0, 0,
    20, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0,
};
// filter types (TYPE parameter)
static constexpr int kFilterTypes = 5;
static const char* const kFilterTypeNames[kFilterTypes] = {"LP 12", "LP 24", "BAND", "HIGH", "NOTCH"};
inline int filterTypeIndex(int v) { const int i = v * kFilterTypes / 128; return i < 0 ? 0 : (i >= kFilterTypes ? kFilterTypes - 1 : i); }
inline int filterTypeValue(int i) { return (i * 128 + kFilterTypes - 1) / kFilterTypes; }
// cutoff / low cut value (0..127) -> Hz (20 Hz .. 20.5 kHz, 10 octaves)
inline float cutoffHz(float v) { return 20.f * std::exp2(v / 127.f * 10.f); }

// ---- global parameters ----
enum GlobalParam {
    gPATTERN = 0, gPLAY, gTEMPO,
    gECHO_TIME, gECHO_FB, gECHO_LPF, gECHO_HPF, gECHO_WIDTH, gECHO_LEV,
    gREV_SIZE, gREV_DAMP, gREV_GATE, gREV_HPF, gREV_PRE, gREV_LEV,
    gEQ_LOW, gEQ_HIGH, gCMP_THR, gCMP_RAT, gCMP_ATK, gCMP_REL, gDRIVE, gVOLUME,
    gECHO_MODEL, gECHO_MOD, gREV_MODEL, gREV_MOD,
    gOUT_STEP, gOUT_LEDS, gOUT_PATTERN,
    kGlobalParams
};
static constexpr int kGlobalBase = kTracks * kTrackParams;
static constexpr int kTotalParams = kGlobalBase + kGlobalParams;

// master parameters gECHO_TIME .. gREV_MOD (index g - gECHO_TIME)
static constexpr int kMasterCount = gREV_MOD - gECHO_TIME + 1;
static const char* const kMasterNames[kMasterCount] = {
    "TIME", "FEEDBACK", "HIGH CUT", "LOW CUT", "WIDTH", "LEVEL",
    "DECAY", "DAMPING", "GATE", "LOW CUT", "PRE-DELAY", "LEVEL",
    "LOW", "HIGH", "THRESH", "RATIO", "ATTACK", "RELEASE", "DRIVE", "VOLUME",
    "MODEL", "MOD", "MODEL", "MOD"
};
static const uint8_t kMasterDefaults[kMasterCount] = {
    48, 50, 100, 10, 64, 64,
    60, 64, 0, 20, 10, 64,
    64, 64, 127, 40, 20, 50, 0, 100,
    0, 30, 52, 30
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
    {"SAW",   {"SHAPE 1", "SHAPE 2", "DETUNE", "OSC MIX", "OSC2 OCT", "WIDTH", "SUB", "NOISE"}, {0, 0, 72, 64, 64, 64, 0, 0}},
    {"PULSE", {"WIDTH", "PWM RATE", "PWM DEPTH", "SYNC", "RING MOD", "SUB", "NOISE", "CRUSH"}, {64, 40, 30, 0, 0, 0, 0, 0}},
    {"SUPER", {"DETUNE", "MIX", "VOICES", "SHAPE", "SUB", "OCTAVE", "NOISE", "DRIVE"}, {50, 90, 127, 0, 0, 64, 0, 0}},
    {"FM",    {"RATIO A", "AMOUNT A", "RATIO B", "AMOUNT B", "FEEDBACK", "ROUTING", "MOD DECAY", "MOD SUST"}, {32, 50, 64, 0, 0, 0, 50, 40}},
    {"WAVE",  {"POSITION", "SCAN RATE", "SCAN DEPTH", "OSC2 OCT", "DETUNE", "OSC2 MIX", "FOLD", "CRUSH"}, {0, 40, 0, 64, 70, 0, 0, 0}},
    {"VOX",   {"VOWEL", "FORMANT", "BREATH", "RESONANCE", "VIB DEPTH", "VIB RATE", "BRIGHT", "CHORUS"}, {0, 64, 20, 60, 0, 64, 40, 0}},
    {"PLUCK", {"DECAY", "BRIGHT", "PICK POS", "DAMPING", "STIFFNESS", "NOISE", "BODY", "DRIVE"}, {80, 80, 50, 40, 0, 50, 0, 0}},
    {"NOISE", {"COLOR", "RESONANCE", "TRACKING", "CRUSH", "GRAIN", "TONE", "LP MIX", "DRIVE"}, {64, 60, 100, 0, 0, 0, 40, 0}},
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
// kfKeyTrk is kept for old banks only: key tracking is now the FILTER parameter KEY TRK.
enum KitField { kfOut = 0, kfMidi, kfTrnsp, kfLegato, kfKeyTrk, kfArpMode, kfArpOct, kfArpSpd, kfArpChord, kfScale, kfRoot, kKitFields };
static const char* const kKitNames[kKitFields] = {"OUTPUT", "MIDI CH", "PITCH", "LEGATO", "KEYTRK", "ARP", "OCTAVES", "SPEED", "CHORD", "SCALE", "ROOT"};
static const int kKitMax[kKitFields] = {3, 16, 48, 1, 4, 5, 3, 5, 9, 15, 11};
static const uint8_t kKitDefault[kKitFields] = {0, 0, 24, 0, 2, 0, 0, 1, 0, 0, 0};

// scales: bit n set = semitone n above the root belongs to the scale
static constexpr int kScales = 16;
struct ScaleDef { const char* name; uint16_t mask; };
static const ScaleDef kScaleDefs[kScales] = {
    {"OFF", 0xFFF},          {"MAJOR", 0xAB5},        {"MINOR", 0x5AD},        {"DORIAN", 0x6AD},
    {"PHRYGIAN", 0x5AB},     {"LYDIAN", 0xAD5},       {"MIXOLYD", 0x6B5},      {"LOCRIAN", 0x56B},
    {"HARM MIN", 0x9AD},     {"MELO MIN", 0xAAD},     {"PENTA MAJ", 0x295},    {"PENTA MIN", 0x4A9},
    {"BLUES", 0x4E9},        {"WHOLE", 0x555},        {"HIRAJOSHI", 0x18D},    {"ARABIC", 0x9B3},
};
static const char* const kNoteNames12[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
inline bool inScale(int note, int scale, int root)
{
    if (scale <= 0 || scale >= kScales) return true;
    const int pc = ((note - root) % 12 + 12) % 12;
    return (kScaleDefs[scale].mask >> pc) & 1;
}
// nearest note of the scale (ties go down)
inline int quantizeNote(int note, int scale, int root)
{
    if (scale <= 0 || scale >= kScales) return note;
    for (int d = 0; d < 12; ++d) {
        if (inScale(note - d, scale, root)) return note - d;
        if (inScale(note + d, scale, root)) return note + d;
    }
    return note;
}
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
    case kfTrnsp: if (v == 24) return "0 ST"; std::snprintf(buf, n, "%+d ST", v - 24); return buf;
    case kfScale: return kScaleDefs[v < 0 ? 0 : (v >= kScales ? kScales - 1 : v)].name;
    case kfRoot: return kNoteNames12[v < 0 ? 0 : (v > 11 ? 11 : v)];
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
static const char* const kLfoTypes[3] = {"FREE", "RETRIG", "HOLD"};
// tempo-synced LFO: length of one cycle, in beats
static constexpr int kLfoSyncDivs = 16;
static const float kLfoSyncBeats[kLfoSyncDivs] = {64.f, 32.f, 16.f, 8.f, 4.f, 2.f, 1.5f, 1.f, 2.f / 3.f, 0.75f, 0.5f, 1.f / 3.f, 0.25f, 1.f / 6.f, 0.125f, 0.0625f};
static const char* const kLfoSyncNames[kLfoSyncDivs] = {"16 BARS", "8 BARS", "4 BARS", "2 BARS", "1 BAR", "1/2", "1/4D", "1/4", "1/4T", "1/8D", "1/8", "1/8T", "1/16", "1/16T", "1/32", "1/64"};
inline int lfoSyncDivIndex(int v) { const int i = v * kLfoSyncDivs / 128; return i < 0 ? 0 : (i >= kLfoSyncDivs ? kLfoSyncDivs - 1 : i); }
inline bool lfoSynced(float syncParam) { return syncParam >= 64.f; }
inline float lfoFreeHz(float rate) { return 0.03f * std::pow(1000.f, (rate < 0 ? 0 : (rate > 127 ? 127 : rate)) / 127.f); }
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
