// Kinetik KMSP-6 — user interface (NanoVG)
#include "DistrhoUI.hpp"
#include "DistrhoPlugin.hpp"
#include "Bank.hpp"
#include "Dsp.hpp"
#include "Presets.hpp"
#include "Help.hpp"
#include "Skins.hpp"
#include "Fonts.hpp"
#include "MonoShared.hpp"
#include <cstdlib>
#include <algorithm>
#include <dirent.h>
#include <sys/stat.h>
#ifdef _WIN32
#include <direct.h>
#endif
#include <vector>
#include <cmath>
#include <string>
#include <memory>

START_NAMESPACE_DISTRHO

using namespace kn;
using DGL_NAMESPACE::Color;

static constexpr float W = 1180.f, H = 800.f;

struct R {
    float x, y, w, h;
    bool in(float px, float py) const { return px >= x && px < x + w && py >= y && py < y + h; }
    float cx() const { return x + w * 0.5f; }
    float cy() const { return y + h * 0.5f; }
};

enum Page { pgSynth, pgAmp, pgFilter, pgEfx, pgLfo1, pgLfo2, pgLfo3, pgTrack, pgArp, kPages };
static const char* const kPageNames[kPages] = {"SYNTH", "AMP", "FILTER", "EFX", "LFO 1", "LFO 2", "LFO 3", "TRACK", "ARP"};
enum MPage { mpEcho, mpReverb, mpMaster };
enum Mode { mdTrig, mdAccent, mdSlide, mdNote, mdLen, mdRatch, mdCond, mdTime, kModes };
static const char* const kModeNames[kModes] = {"TRIG", "ACCENT", "SLIDE", "NOTE", "LEN", "RATCH", "COND", "TIME"};

enum DragKind { dkNone, dkTrackKnob, dkMasterKnob, dkTempo, dkSwing, dkAccent, dkKit, dkStepNote, dkStepLen, dkKey };
enum Overlay { ovNone, ovPicker, ovPresets, ovHelp, ovSkins, ovSave };
enum DragExtra { dkResize = 100 };

static const char* const kMachineShort[kMachines] = {
    "no sound", "dual saw / pulse / triangle", "PWM, sync, ring, crusher", "up to 7 detuned saws",
    "3-operator FM, 4 algorithms", "32-wave wavetable, scan, fold", "formant vowels A-E-I-O-U",
    "physical string model", "pitched / coloured noise"};

class MonoUI : public UI
{
public:
    MonoUI()
        : UI(W, H)
    {
        loadSharedResources();
        const double s = getScaleFactor();
        if (s > 1.01) setSize((uint)(W * s), (uint)(H * s));
        setGeometryConstraints((uint)(W * 0.6 * (s > 1.01 ? s : 1.0)), (uint)(H * 0.6 * (s > 1.01 ? s : 1.0)), true, false);
        for (int i = 0; i < kTotalParams; ++i) values[i] = 0.f;
        for (int t = 0; t < kTracks; ++t)
            for (int p = 0; p < kTrackParams; ++p) values[t * kTrackParams + p] = trackParamDefault(t, p);
        for (int i = 0; i < 20; ++i) values[kGlobalBase + gECHO_TIME + i] = kMasterDefaults[i];
        values[kGlobalBase + gTEMPO] = 120;
        values[kGlobalBase + gOUT_STEP] = -1;
        bank.reset(new Bank());
        bank->initDefault();
        fonts[fnSerif] = createFontFromMemory("kn-serif", kFontSerif, sizeof(kFontSerif), false);
        fonts[fnSerifItalic] = createFontFromMemory("kn-serif-i", kFontSerifItalic, sizeof(kFontSerifItalic), false);
        fonts[fnMono] = createFontFromMemory("kn-mono", kFontMono, sizeof(kFontMono), false);
        fonts[fnBlack] = createFontFromMemory("kn-black", kFontBlack, sizeof(kFontBlack), false);
        int sk, zm;
        readDefaultView(sk, zm);
        setSkin(sk, false);
        if (zm != 100) setSize((uint)std::lround(W * baseScale() * zm / 100.0), (uint)std::lround(H * baseScale() * zm / 100.0));
        viewDirty = false;
        if (void* p = getPluginInstancePointer()) direct = dynamic_cast<MonoDirect*>(static_cast<Plugin*>(p));
        if (direct) { direct->setSelectedTrack(0); direct->setRecording(false); }
    }

    ~MonoUI() override
    {
        if (direct) {
            if (keyNote >= 0) direct->playNote(keyTrack, keyNote, 0, false);
            for (int n : pcHeld) if (n >= 0) direct->playNote(selTrack, n, 0, false);
            direct->setRecording(false);
        }
    }

    // ------------------------------------------------------------------ skins
    static std::string viewFile()
    {
       #ifdef _WIN32
        const char* base = std::getenv("APPDATA");
        return base ? std::string(base) + "\\KinetikKMSP6-view.txt" : std::string();
       #else
        const char* base = std::getenv("HOME");
       #ifdef __APPLE__
        return base ? std::string(base) + "/Library/Preferences/KinetikKMSP6-view.txt" : std::string();
       #else
        return base ? std::string(base) + "/.config/KinetikKMSP6-view.txt" : std::string();
       #endif
       #endif
    }
    static void readDefaultView(int& skinOut, int& zoomOut)
    {
        skinOut = kDefaultSkin; zoomOut = 100;
        const std::string f = viewFile();
        if (f.empty()) return;
        if (FILE* fp = std::fopen(f.c_str(), "r")) {
            int a = 0, b = 100; const int n = std::fscanf(fp, "%d %d", &a, &b); std::fclose(fp);
            if (n >= 1 && a >= 0 && a < kNumThemes) skinOut = a;
            if (n >= 2 && b >= 60 && b <= 300) zoomOut = b;
        }
    }
    static void writeDefaultView(int s, int z)
    {
        const std::string f = viewFile();
        if (f.empty()) return;
        if (FILE* fp = std::fopen(f.c_str(), "w")) { std::fprintf(fp, "%d %d\n", s, z); std::fclose(fp); }
    }
    void setSkin(int i, bool save)
    {
        skin = clampi(i, 0, kNumThemes - 1);
        th = &kThemes[skin];
        if (save) saveView();
        repaint();
    }

protected:
    // ------------------------------------------------------------------ DSP -> UI
    void parameterChanged(uint32_t index, float value) override
    {
        if (index >= (uint32_t)kTotalParams) return;
        if (values[index] == value) return;
        values[index] = value;
        if (index == (uint32_t)(kGlobalBase + gOUT_STEP) && recording() && drag == dkTrackKnob && recDirty)
            recordLock(pageParam(page, dragIndex), (int)std::lround(recVal));
        repaint();
    }

    void stateChanged(const char* key, const char* value) override
    {
        if (std::strcmp(key, "bank") == 0) { deserializeBank(*bank, value); repaint(); }
        if (std::strcmp(key, "skin") == 0 && value && *value) {
            int sk = kDefaultSkin, zm = 0;
            const int n = std::sscanf(value, "%d %d", &sk, &zm);
            if (n >= 1) setSkin(sk, false);
            if (n >= 2 && zm >= 60 && zm <= 300 && std::abs(zm - zoomPercent()) > 1) setZoom(zm / 100.0);
            viewDirty = false;
        }
    }

    // ------------------------------------------------------------------ helpers
    float& tp(int t, int p) { return values[t * kTrackParams + p]; }
    float gp(int g) const { return values[kGlobalBase + g]; }
    int editPattern() const { return clampi((int)std::lround(gp(gPATTERN)), 0, kPatterns - 1); }
    int machineOf(int t) const { return clampi(bank->machine[t], 0, kMachines - 1); }
    int64_t absStep() const { return (int64_t)std::lround(gp(gOUT_STEP)); } // -1 when stopped
    int trackStepNow(int t) { const int64_t k = absStep(); return k < 0 ? -1 : pat().trackStep(t, k); }
    int tlen(int t) { return pat().trackLen[t] ? pat().trackLen[t] : 16; }
    Pattern& pat() { return bank->patterns[editPattern()]; }

    void setParam(uint32_t idx, float v)
    {
        values[idx] = v;
        setParameterValue(idx, v);
    }
    void setParamGesture(uint32_t idx, float v)
    {
        editParameter(idx, true);
        setParam(idx, v);
        editParameter(idx, false);
    }

    void sendCmd(const std::string& c)
    {
        applyCommand(*bank, c.c_str());
        setState("cmd", c.c_str());
    }
    void sendLines(const std::string& c)
    {
        size_t st = 0;
        while (st < c.size()) {
            size_t e = c.find('\n', st); if (e == std::string::npos) e = c.size();
            applyCommand(*bank, c.substr(st, e - st).c_str());
            st = e + 1;
        }
        setState("cmd", c.c_str());
    }
    void sendPatternSettings()
    {
        const Pattern& p = pat();
        char b[64];
        std::snprintf(b, sizeof(b), "P %d %d %d %d %d", editPattern(), p.length, p.scale, p.swing, p.accent);
        sendCmd(b);
    }

    // notes to the engine (on-screen keys, computer keyboard, track audition)
    void playNote(int t, int n, int vel, bool on)
    {
        if (direct) { direct->playNote(t, n, vel, on); return; }
        char b[48]; std::snprintf(b, sizeof(b), "%s %d %d %d", on ? "non" : "noff", t, n, vel);
        setState("cmd", b);
    }
    void selectTrack(int t)
    {
        if (t == selTrack) return;
        // release whatever the keyboard holds on the previous track
        if (keyNote >= 0) { playNote(keyTrack, keyNote, 0, false); keyNote = -1; if (drag == dkKey) drag = dkNone; }
        for (int& n : pcHeld) if (n >= 0) { playNote(selTrack, n, 0, false); n = -1; }
        selTrack = t; noteStep = -1;
        if (direct) direct->setSelectedTrack(t);
        else { char b[16]; std::snprintf(b, sizeof(b), "sel %d", t); setState("cmd", b); }
    }
    void setRec(bool on)
    {
        rec = on;
        if (direct) direct->setRecording(on);
        else setState("cmd", on ? "rec 1" : "rec 0");
    }

    int pageParam(int pg, int k) const
    {
        switch (pg) {
        case pgSynth: return k;
        case pgAmp: return 8 + k;
        case pgFilter: return 16 + k;
        case pgEfx: return 24 + k;
        case pgLfo1: return k < 6 ? tpLFO1 + k : -1;
        case pgLfo2: return k < 6 ? tpLFO2 + k : -1;
        case pgLfo3: return k < 6 ? tpLFO3 + k : -1;
        default: return -1; // TRACK / ARP pages: kit settings, see kitField()
        }
    }
    int kitField(int k) const
    {
        if (page == pgTrack) return k < 5 ? k : -1;
        if (page == pgArp) return k < 4 ? kfArpMode + k : -1;
        return -1;
    }
    int masterParam(int mp, int k) const
    {
        if (mp == mpEcho) return k < 6 ? gECHO_TIME + k : -1;
        if (mp == mpReverb) return k < 6 ? gREV_SIZE + k : -1;
        return gEQ_LOW + k;
    }

    std::string valueText(int t, int p, float v) const
    {
        char b[32];
        const int iv = (int)std::lround(v);
        if (p == tpPAN) {
            if (iv == 64) return "C";
            std::snprintf(b, sizeof(b), "%c%d", iv < 64 ? 'L' : 'R', std::abs(iv - 64)); return b;
        }
        if (p == tpEQG || p == tpBOF || p == tpWOF) { std::snprintf(b, sizeof(b), "%+d", iv - 64); return b; }
        if (p == tpPORT && iv == 0) return "OFF";
        if (p >= tpLFO1 && p < tpLFO1 + 18) {
            const int l = (p - tpLFO1) / 6, k = (p - tpLFO1) % 6;
            if (k == 2) return kLfoShapes[lfoShapeIndex(iv)];
            if (k == 3) return kLfoTypes[lfoTypeIndex(iv)];
            if (k == 5) {
                const int tt = lfoTrackIndex(iv);
                if (tt == 0) return "SELF";
                std::snprintf(b, sizeof(b), "T%d", tt); return b;
            }
            if (k == 4) {
                const int d = lfoDestIndex(iv);
                if (d >= kLfoDests) return "PITCH";
                const int trk = lfoTrackIndex((int)std::lround(values[t * kTrackParams + kLfoBase[l] + 5]));
                const int tt = trk == 0 ? t : trk - 1;
                return trackParamName(machineOf(tt), d);
            }
        }
        if (p < 8 && machineOf(t) == mSAW && p == 4) { std::snprintf(b, sizeof(b), "%+d", (int)std::lround(iv / 127.f * 4.f) - 2); return b; }
        if (p < 8 && machineOf(t) == mSUPER && p == 5) { std::snprintf(b, sizeof(b), "%+d", (int)std::lround(iv / 127.f * 4.f) - 2); return b; }
        if (p < 8 && machineOf(t) == mWAVE && p == 3) { std::snprintf(b, sizeof(b), "%+d", (int)std::lround(iv / 127.f * 4.f) - 2); return b; }
        if (p < 8 && machineOf(t) == mSUPER && p == 2) { std::snprintf(b, sizeof(b), "%d", 3 + (int)std::lround(iv / 127.f * 4.f)); return b; }
        if (p < 8 && machineOf(t) == mFM && p == 5) { std::snprintf(b, sizeof(b), "ALG%d", 1 + std::min(3, (int)(iv / 127.f * 4.f))); return b; }
        if (p < 8 && machineOf(t) == mFM && (p == 0 || p == 2)) {
            static const char* R[16] = {"0.5", "1", "1.5", "2", "2.5", "3", "3.5", "4", "5", "6", "7", "8", "9", "10", "11", "12"};
            return R[std::min(15, (int)(iv / 127.f * 16.f))];
        }
        std::snprintf(b, sizeof(b), "%d", iv); return b;
    }
    std::string masterText(int g, float v) const
    {
        char b[32];
        const int iv = (int)std::lround(v);
        if (g == gECHO_TIME) return kEchoNames[echoDivIndex(iv)];
        if (g == gREV_GATE && iv == 0) return "OFF";
        if (g == gEQ_LOW || g == gEQ_HIGH) { std::snprintf(b, sizeof(b), "%+d", iv - 64); return b; }
        std::snprintf(b, sizeof(b), "%d", iv); return b;
    }

    // ------------------------------------------------------------------ layout
    R trackBtn(int t) const { return {20.f + t * 190.f, 66.f, 184.f, 64.f}; }
    R muteBtn(int t) const { R r = trackBtn(t); return {r.x + r.w - 30.f, r.y + r.h - 28.f, 30.f, 28.f}; }
    R lcd() const { return {20.f, 146.f, 680.f, 150.f}; }
    R machPrev() const { return {150.f, 156.f, 26.f, 26.f}; }
    R machName() const { return {178.f, 156.f, 130.f, 26.f}; }
    R machNext() const { return {310.f, 156.f, 26.f, 26.f}; }
    R lockClear() const { return {596.f, 156.f, 94.f, 24.f}; }
    R pageTab(int i) const { return {20.f + i * 75.5f, 304.f, 72.f, 28.f}; }
    R knob(int k) const { return {20.f + k * 85.f, 340.f, 85.f, 120.f}; }
    R mTab(int i) const { return {720.f + i * 148.f, 146.f, 144.f, 28.f}; }
    R mKnob(int k) const { return {720.f + (k % 4) * 110.f, 178.f + (k / 4) * 102.f, 110.f, 100.f}; }
    R patBtn(int i) const { return {720.f + (i % 8) * 55.f, 432.f + (i / 8) * 30.f, 51.f, 26.f}; }
    R patCopy() const { return {946.f, 400.f, 68.f, 24.f}; }
    R patPaste() const { return {1018.f, 400.f, 68.f, 24.f}; }
    R patClear() const { return {1090.f, 400.f, 68.f, 24.f}; }
    R modeBtn(int i) const { return {20.f + i * 55.f, 504.f, 52.f, 26.f}; }
    R stepPage(int i) const { return {464.f + i * 50.f, 504.f, 47.f, 26.f}; }
    R lenMinus() const { return {666.f, 504.f, 22.f, 26.f}; }
    R lenBox() const { return {690.f, 504.f, 56.f, 26.f}; }
    R lenPlus() const { return {748.f, 504.f, 22.f, 26.f}; }
    R fillBtn() const { return {778.f, 504.f, 52.f, 26.f}; }
    R trkCopy() const { return {838.f, 504.f, 62.f, 26.f}; }
    R trkPaste() const { return {904.f, 504.f, 62.f, 26.f}; }
    R trkClear() const { return {970.f, 504.f, 62.f, 26.f}; }
    R lockClrAll() const { return {1036.f, 504.f, 66.f, 26.f}; }
    R stepBtn(int i) const { return {20.f + i * 71.25f, 538.f, 66.f, 76.f}; }
    // keyboard
    R octDown() const { return {20.f, 624.f, 34.f, 66.f}; }
    R octBox() const { return {56.f, 624.f, 60.f, 66.f}; }
    R octUp() const { return {118.f, 624.f, 34.f, 66.f}; }
    static constexpr float kKeyX = 162.f, kKeyY = 624.f, kKeyW = 29.f, kKeyH = 66.f, kBlackW = 19.f, kBlackH = 40.f;
    static constexpr int kKbOctaves = 3;
    R pcKeysBtn() const { return {812.f, 624.f, 120.f, 31.f}; }
    R panicBtn() const { return {812.f, 659.f, 120.f, 31.f}; }
    // header
    R patLenBox() const { return {396.f, 9.f, 70.f, 40.f}; }
    R scaleBox() const { return {470.f, 9.f, 56.f, 40.f}; }
    R swingBox() const { return {530.f, 9.f, 78.f, 40.f}; }
    R accBox() const { return {612.f, 9.f, 80.f, 40.f}; }
    R helpBtn() const { return {340.f, 156.f, 26.f, 26.f}; }
    R playBtn() const { return {840.f, 9.f, 84.f, 40.f}; }
    R tempoBox() const { return {932.f, 9.f, 110.f, 40.f}; }
    R skinBtn() const { return {1050.f, 9.f, 116.f, 40.f}; }
    R viewBox() const { return {240.f, 130.f, 700.f, 520.f}; }
    R skinRow(int i) const { return {264.f + (i % 2) * 330.f, 184.f + (i / 2) * 148.f, 316.f, 136.f}; }
    R zoomBtn(int i) const { return {264.f + i * 109.f, 520.f, 101.f, 36.f}; }
    R gripR() const { return {W - 22.f, H - 22.f, 22.f, 22.f}; }
    // bottom bar
    R recBtn() const { return {20.f, 712.f, 96.f, 40.f}; }
    R rndGroup() const { return {124.f, 706.f, 574.f, 52.f}; }
    R rndBtn(int i) const { return {250.f + i * 111.f, 712.f, 105.f, 40.f}; }
    R undoBtn() const { return {712.f, 712.f, 86.f, 40.f}; }
    R presetBtn() const { return {1010.f, 704.f, 150.f, 34.f}; }
    R saveBtn() const { return {1010.f, 742.f, 150.f, 34.f}; }
    // presets window
    R presetBox() const { return {80.f, 80.f, 1020.f, 640.f}; }
    R presetTab(int i) const { return {300.f + i * 134.f, 96.f, 128.f, 32.f}; }
    R presetRow(int i) const { return {100.f + (i % 2) * 490.f, 140.f + (i / 2) * 43.f, 480.f, 40.f}; }
    R presetSaveNew() const { return {100.f, 668.f, 190.f, 36.f}; }
    R presetDelete() const { return {300.f, 668.f, 110.f, 36.f}; }
    R presetLoadAll() const { return {560.f, 668.f, 250.f, 36.f}; }
    R presetLoadKit() const { return {820.f, 668.f, 170.f, 36.f}; }
    R presetCancel() const { return {1000.f, 668.f, 80.f, 36.f}; }
    // save window
    R saveBox() const { return {290.f, 240.f, 600.f, 270.f}; }
    R saveField() const { return {320.f, 330.f, 540.f, 52.f}; }
    R saveOk() const { return {620.f, 440.f, 240.f, 40.f}; }
    R saveCancel() const { return {320.f, 440.f, 140.f, 40.f}; }
    // machine picker
    R pickerBox() const { return {170.f, 170.f, 840.f, 420.f}; }
    R pickerCell(int m) const
    {
        const int i = m == mOFF ? 8 : m - 1; // OFF last
        return {196.f + (i % 3) * 266.f, 240.f + (i / 3) * 106.f, 254.f, 94.f};
    }

    // ------------------------------------------------------------------ keyboard geometry
    int kbLow() const { return kbOct * 12 + 24; }  // lowest C on the keyboard (kbOct 0 = C0 = 24 ... Ableton naming)
    static bool isBlack(int pc) { return pc == 1 || pc == 3 || pc == 6 || pc == 8 || pc == 10; }
    R keyRect(int n) const
    {
        const int rel = n - kbLow();
        const int oct = rel / 12, pc = rel % 12;
        static const int whiteIdx[12] = {0, 0, 1, 1, 2, 3, 3, 4, 4, 5, 5, 6};
        const float xw = kKeyX + (oct * 7 + whiteIdx[pc]) * kKeyW;
        if (!isBlack(pc)) return {xw, kKeyY, kKeyW - 2.f, kKeyH};
        return {xw + kKeyW - kBlackW * 0.5f - 1.f, kKeyY, kBlackW, kBlackH};
    }
    int keyAt(float x, float y) const
    {
        const int lo = kbLow(), hi = lo + 12 * kKbOctaves; // inclusive top C
        for (int n = lo; n <= hi; ++n) if (isBlack(n % 12) && keyRect(n).in(x, y)) return n;
        for (int n = lo; n <= hi; ++n) { if (isBlack(n % 12)) continue; R r = keyRect(n); r.w += 2.f; if (r.in(x, y)) return n; }
        return -1;
    }

    // ------------------------------------------------------------------ drawing primitives (themed)
    void useFont(int f)
    {
        switch (f) {
        case fnSerif: fontFaceId(fonts[fnSerif]); break;
        case fnSerifItalic: fontFaceId(fonts[fnSerifItalic]); break;
        case fnMono: fontFaceId(fonts[fnMono]); break;
        case fnBlack: fontFaceId(fonts[fnBlack]); break;
        default: fontFace(NANOVG_DEJAVU_SANS_TTF); break;
        }
    }
    void box(const R& r, const Color& fillC, float rad = -1.f)
    {
        beginPath(); roundedRect(r.x, r.y, r.w, r.h, rad < 0 ? th->radius : rad); fillColor(fillC); fill();
    }
    void frame(const R& r, const Color& c, float rad = -1.f, float wdt = 1.f)
    {
        beginPath(); roundedRect(r.x + 0.5f, r.y + 0.5f, r.w - 1, r.h - 1, rad < 0 ? th->radius : rad); strokeColor(c); strokeWidth(wdt); stroke();
    }
    // readable label size: every small label is enlarged, with a floor so nothing is tiny
    static float textSize(float size, int font)
    {
        const float s = std::max(size, 9.5f);
        return font == fnMono ? s * 1.13f : s * 1.08f;
    }
    // mono / label text (letter-spaced in the retro skins)
    void label(float x, float y, const char* s, float size, const Color& c, int align = ALIGN_CENTER | ALIGN_MIDDLE, int font = -1)
    {
        if (s == nullptr || *s == '\0') return;
        const int f = font < 0 ? th->fontLabel : font;
        useFont(f);
        fontSize(textSize(size, f));
        textLetterSpacing(f == fnMono ? th->labelSpacing * (size / 12.f) : 0.f);
        fillColor(c); textAlign(align); text(x, y, s, nullptr);
        textLetterSpacing(0.f);
    }
    enum Icon { icNone, icPlay, icStop, icRec, icNext };
    // icon (drawn as a shape, independent of the font) followed by a label, centred on (cx, cy)
    void iconLabel(float cx, float cy, Icon ic, const char* s, float size, const Color& c, int font = -1, int align = ALIGN_CENTER)
    {
        const int f = font < 0 ? th->fontButton : font;
        useFont(f);
        fontSize(textSize(size, f));
        textLetterSpacing(f == fnMono ? th->labelSpacing * (size / 12.f) : 0.f);
        DGL_NAMESPACE::Rectangle<float> bnd; textBounds(0, 0, s, nullptr, bnd);
        textLetterSpacing(0.f);
        const float isz = size * 0.62f, gap = size * 0.55f;
        const float total = isz + gap + bnd.getWidth();
        float x = align == ALIGN_RIGHT ? cx - total : (align == ALIGN_LEFT ? cx : cx - total * 0.5f);
        beginPath();
        switch (ic) {
        case icPlay: case icNext:
            moveTo(x, cy - isz * 0.55f); lineTo(x + isz * 0.9f, cy); lineTo(x, cy + isz * 0.55f); closePath(); break;
        case icStop: rect(x, cy - isz * 0.45f, isz * 0.9f, isz * 0.9f); break;
        case icRec: circle(x + isz * 0.45f, cy, isz * 0.48f); break;
        default: break;
        }
        fillColor(c); fill();
        label(x + isz + gap, cy, s, size, c, ALIGN_LEFT | ALIGN_MIDDLE, f);
    }

    // big numbers / names (serif, black grotesk or sans depending on the skin), optional glow
    void number(float x, float y, const char* s, float size, const Color& c, int align = ALIGN_CENTER | ALIGN_MIDDLE, int font = -1)
    {
        if (s == nullptr || *s == '\0') return;
        const int f = font < 0 ? th->fontNum : font;
        useFont(f);
        const float sz = f == fnSerif || f == fnSerifItalic ? size * 1.25f : (f == fnBlack ? size * 0.95f : size);
        fontSize(sz); textAlign(align);
        if (th->glow) {
            fontBlur(sz * 0.18f); fillColor(Color(c, Color(0, 0, 0, 0.f), 0.35f)); text(x, y, s, nullptr);
            fontBlur(0.f);
        }
        fillColor(c); text(x, y, s, nullptr);
    }
    void dotted(const R& r, const Color& c)
    {
        if (!dotImage.isValid()) { box(r, c, 1.f); return; }
        Paint p = imagePattern(r.x, r.y, 4.f, 4.f, 0.f, dotImage, 1.f);
        p.innerColor = c; p.outerColor = c;
        beginPath(); rect(r.x, r.y, r.w, r.h); fillPaint(p); fill();
    }
    void panelBox(const R& r)
    {
        box(r, th->panel, th->radius + 2.f);
        frame(r, th->panelEdge, th->radius + 2.f, th->panelEdgeWidth);
    }
    void button(const R& r, const char* s, bool on, float size = 11.5f)
    {
        if (on) box(r, th->selFill);
        else { box(r, th->btnFill); frame(r, th->btnEdge, -1.f, 1.f); }
        label(r.cx(), r.cy() + 1, s, size, on ? th->selText : th->btnText, ALIGN_CENTER | ALIGN_MIDDLE, th->fontButton);
    }
    void drawKnob(const R& r, float v01, const char* name, const std::string& val, bool locked, bool highlight, bool enabled)
    {
        const float cx = r.cx(), cy = r.y + 44.f, rad = 26.f;
        const float a0 = kPi * 0.75f, a1 = kPi * 2.25f;
        v01 = clampf(v01, 0.f, 1.f);
        const Color hi = locked ? th->lock : th->knobArc;
        if (!enabled) {
            beginPath(); circle(cx, cy, rad - 6); fillColor(Color(th->knobBody, th->panel, 0.6f)); fill();
            for (int i = 0; i < 24; ++i) {
                const float a = kTwoPi * i / 24.f;
                beginPath(); circle(cx + std::cos(a) * (rad - 6), cy + std::sin(a) * (rad - 6), 0.8f); fillColor(th->knobTrack); fill();
            }
            label(cx, r.y + 90.f, "-", 12.f, th->faint);
            return;
        }
        const float a = a0 + (a1 - a0) * v01;
        switch (th->knob) {
        case ksTicks: {
            // ring of tick dots around a dark knob (lit up to the value)
            const int n = 25;
            for (int i = 0; i < n; ++i) {
                const float t = (float)i / (n - 1);
                const float ang = a0 + (a1 - a0) * t;
                const bool lit = t <= v01 + 1e-4f;
                beginPath(); circle(cx + std::cos(ang) * (rad + 3), cy + std::sin(ang) * (rad + 3), lit ? 1.6f : 1.1f);
                fillColor(lit ? hi : th->knobTrack); fill();
            }
            if (th->glow) {
                beginPath(); circle(cx, cy, rad - 3);
                fillPaint(radialGradient(cx, cy, rad - 8, rad + 8, Color(hi, Color(0, 0, 0, 0.f), 0.75f), Color(0, 0, 0, 0.f))); fill();
            }
            beginPath(); circle(cx, cy, rad - 5); fillColor(th->knobBody); fill();
            strokeColor(locked ? th->lock : th->knobRing); strokeWidth(locked ? 2.f : 1.2f); stroke();
            beginPath(); moveTo(cx + std::cos(a) * 4, cy + std::sin(a) * 4); lineTo(cx + std::cos(a) * (rad - 9), cy + std::sin(a) * (rad - 9));
            strokeColor(locked ? th->lock : th->knobPointer); strokeWidth(2.4f); lineCap(ROUND); stroke();
            break;
        }
        case ksArcThick: {
            beginPath(); arc(cx, cy, rad, a0, a1, CW); strokeColor(th->knobTrack); strokeWidth(6.f); lineCap(BUTT); stroke();
            beginPath(); arc(cx, cy, rad, a0, a0 + (a1 - a0) * std::max(0.001f, v01), CW); strokeColor(hi); strokeWidth(6.f); stroke();
            beginPath(); circle(cx, cy, rad - 6); fillColor(locked ? th->lock : th->knobBody); fill();
            strokeColor(th->knobRing); strokeWidth(1.2f); stroke();
            beginPath(); moveTo(cx + std::cos(a) * 6, cy + std::sin(a) * 6); lineTo(cx + std::cos(a) * (rad - 9), cy + std::sin(a) * (rad - 9));
            strokeColor(th->knobPointer); strokeWidth(2.4f); lineCap(ROUND); stroke();
            break;
        }
        default: {
            beginPath(); circle(cx, cy, rad + 4); fillColor(th->inner); fill();
            beginPath(); arc(cx, cy, rad, a0, a1, CW); strokeColor(th->knobTrack); strokeWidth(5.f); stroke();
            beginPath(); arc(cx, cy, rad, a0, a0 + (a1 - a0) * std::max(0.001f, v01), CW); strokeColor(hi); strokeWidth(5.f); stroke();
            beginPath(); circle(cx, cy, rad - 7); fillColor(highlight ? Color(th->knobBody, Color(255, 255, 255), 0.1f) : th->knobBody); fill();
            beginPath(); moveTo(cx + std::cos(a) * (rad - 18), cy + std::sin(a) * (rad - 18));
            lineTo(cx + std::cos(a) * (rad - 8), cy + std::sin(a) * (rad - 8));
            strokeColor(th->knobPointer); strokeWidth(3.f); stroke();
            break;
        }
        }
        label(cx, r.y + 88.f, name, 12.f, locked ? th->lock : th->text);
        number(cx, r.y + 106.f, val.c_str(), 14.f, locked ? th->lock : (th->fontNum == fnSans ? th->dim : th->text));
    }

    void drawSparkle(float cx, float cy, float s, const Color& c)
    {
        beginPath();
        moveTo(cx, cy - s);
        quadTo(cx + s * 0.18f, cy - s * 0.18f, cx + s, cy);
        quadTo(cx + s * 0.18f, cy + s * 0.18f, cx, cy + s);
        quadTo(cx - s * 0.18f, cy + s * 0.18f, cx - s, cy);
        quadTo(cx - s * 0.18f, cy - s * 0.18f, cx, cy - s);
        closePath(); fillColor(c); fill();
    }

    // the same logo in every skin: "Kinetik" in serif italic + a four-point star, in the skin's colours
    void drawLogo()
    {
        useFont(fnSerifItalic); fontSize(54.f); textAlign(ALIGN_LEFT | ALIGN_MIDDLE);
        DGL_NAMESPACE::Rectangle<float> bnd; textBounds(18, 30, "Kinetik", nullptr, bnd);
        const float right = bnd.getX() + bnd.getWidth();
        const Color star = th->logo == lgRiso ? th->lock : (th->logo == lgClassic ? th->knobArc : th->accent);
        if (th->glow) {
            for (int k = 0; k < 2; ++k) { fontBlur(24.f); fillColor(Color(th->accent, Color(0, 0, 0, 0.f), 0.55f)); text(18, 30, "Kinetik", nullptr); }
            fontBlur(6.f); fillColor(Color(th->accent, Color(0, 0, 0, 0.f), 0.7f)); text(18, 30, "Kinetik", nullptr);
            fontBlur(0.f); fillColor(Color(230, 234, 255)); text(18, 30, "Kinetik", nullptr);
            beginPath(); circle(right + 10, 19, 14);
            fillPaint(radialGradient(right + 10, 19, 2, 14, Color(star, Color(0, 0, 0, 0.f), 0.55f), Color(0, 0, 0, 0.f))); fill();
        } else {
            fillColor(th->text); text(18, 30, "Kinetik", nullptr);
        }
        drawSparkle(right + 10, 19, 9.f, star);
        label(right + 30, 21, "KMSP-6", 13.f, th->text, ALIGN_LEFT | ALIGN_MIDDLE, fnMono);
        label(right + 30, 39, "MONO SYNTH PROGRAM", 9.5f, th->dim, ALIGN_LEFT | ALIGN_MIDDLE, fnMono);
    }


    // ------------------------------------------------------------------ main draw
    void onNanoDisplay() override
    {
        sx = getWidth() / W;
        scale(sx, sx);
        if (!dotImage.isValid()) makeTextures();

        beginPath(); rect(0, 0, W, H); fillColor(th->bg); fill();
        if (th->halftone && haloImage.isValid()) {
            Paint p = imagePattern(0, 0, 6.f, 6.f, 0.f, haloImage, 1.f);
            p.innerColor = th->dots; p.outerColor = th->dots;
            beginPath(); rect(0, 0, W, H); fillPaint(p); fill();
        }
        drawHeader();
        drawTracks();
        drawLcdSection();
        drawMaster();
        drawSequencer();
        drawKeyboard();
        drawBottomBar();
        if (overlay == ovPicker) drawPicker();
        if (overlay == ovPresets) drawPresets();
        if (overlay == ovHelp) drawHelp();
        if (overlay == ovSkins) drawSkins();
        if (overlay == ovSave) drawSave();
        drawGrip();
    }

    void makeTextures()
    {
        uint8_t d[4 * 4 * 4] = {};
        for (int i : {5}) { d[i * 4] = 255; d[i * 4 + 1] = 255; d[i * 4 + 2] = 255; d[i * 4 + 3] = 255; }
        dotImage = createImageFromRGBA(4, 4, d, IMAGE_REPEAT_X | IMAGE_REPEAT_Y | (1 << 5));
        uint8_t h[6 * 6 * 4] = {};
        for (int i : {7, 8, 13, 14}) { h[i * 4] = 255; h[i * 4 + 1] = 255; h[i * 4 + 2] = 255; h[i * 4 + 3] = 140; }
        haloImage = createImageFromRGBA(6, 6, h, IMAGE_REPEAT_X | IMAGE_REPEAT_Y | (1 << 5));
    }

    void drawHeader()
    {
        drawLogo();
        drawPatternSettings();
        const int ep = editPattern(), pp = (int)std::lround(gp(gOUT_PATTERN));
        const bool running = gp(gOUT_STEP) >= 0.f;
        char b[48];
        const R pr{700, 9, 130, 40};
        box(pr, th->lcdBg); frame(pr, th->panelEdge, -1.f, th->panelEdgeWidth);
        label(pr.x + 10, pr.cy() + 1, "PAT", 10.f, th->lcdDim, ALIGN_LEFT | ALIGN_MIDDLE);
        if (running && pp != ep) std::snprintf(b, sizeof(b), "A%02d>A%02d", pp + 1, ep + 1);
        else std::snprintf(b, sizeof(b), "A%02d", ep + 1);
        number(pr.x + pr.w - 10, pr.cy() + 1, b, running && pp != ep ? 16.f : 20.f, th->lcdText, ALIGN_RIGHT | ALIGN_MIDDLE);
        const bool play = gp(gPLAY) > 0.5f;
        {
            const R pb = playBtn();
            if (play) box(pb, th->selFill); else { box(pb, th->btnFill); frame(pb, th->btnEdge); }
            iconLabel(pb.cx(), pb.cy() + 1, play ? icStop : icPlay, play ? "STOP" : "PLAY", 12.f, play ? th->selText : th->btnText);
        }
        const R tb = tempoBox();
        box(tb, th->lcdBg); frame(tb, th->panelEdge, -1.f, th->panelEdgeWidth);
        std::snprintf(b, sizeof(b), "%.1f", gp(gTEMPO));
        number(tb.x + 70, tb.cy() + 1, b, 20.f, th->lcdText, ALIGN_RIGHT | ALIGN_MIDDLE);
        label(tb.x + 75, tb.cy() + 2, "BPM", 10.f, th->lcdDim, ALIGN_LEFT | ALIGN_MIDDLE);
        const R sk = skinBtn();
        box(sk, th->btnFill); frame(sk, th->btnEdge);
        label(sk.cx(), sk.y + 12, "SKIN / SIZE", 9.f, th->dim);
        std::snprintf(b, sizeof(b), "%s %d%%", th->name, zoomPercent());
        label(sk.cx(), sk.y + 28, b, 10.5f, th->text);
    }

    void drawTracks()
    {
        const uint32_t leds = (uint32_t)gp(gOUT_LEDS);
        const uint32_t held = direct ? direct->heldMask() : 0u;
        for (int t = 0; t < kTracks; ++t) {
            const R r = trackBtn(t);
            const bool sel = t == selTrack;
            const bool off = machineOf(t) == mOFF;
            const Color fg = sel ? th->selText : (off ? th->faint : th->text);
            if (sel) box(r, th->selFill);
            else { box(r, th->btnFill); frame(r, off ? Color(th->btnEdge, th->bg, 0.5f) : th->btnEdge); }
            char b[16]; std::snprintf(b, sizeof(b), "%d", t + 1);
            number(r.x + 10, r.y + 20, b, 22.f, fg, ALIGN_LEFT | ALIGN_MIDDLE);
            const bool lit = ((leds >> t) & 1) || ((held >> t) & 1);
            beginPath(); circle(r.x + r.w - 13, r.y + 14, 4.5f);
            fillColor(lit ? (sel && th->selFill.red > 0.5f && th->accent.red < 0.2f ? th->selText : th->accent)
                          : Color(fg, Color(0, 0, 0, 0.f), 0.25f)); fill();
            if (lit && th->glow) { beginPath(); circle(r.x + r.w - 13, r.y + 14, 10); fillPaint(radialGradient(r.x + r.w - 13, r.y + 14, 2, 10, Color(th->accent, Color(0, 0, 0, 0.f), 0.5f), Color(0, 0, 0, 0.f))); fill(); }
            label(r.x + 40, r.y + 20, kMachineDefs[machineOf(t)].name, 12.f, fg, ALIGN_LEFT | ALIGN_MIDDLE);
            // what the track is doing: arp / MIDI channel / last note
            std::string info;
            const int ch = bank->kit[t][kfMidi];
            if (ch > 0) { std::snprintf(b, sizeof(b), "CH%d ", ch); info += b; }
            if (bank->kit[t][kfArpMode] > 0) { info += "ARP "; }
            if (bank->kit[t][kfLegato]) info += "LEG ";
            label(r.x + 10, r.y + 47, info.c_str(), 9.5f, Color(fg, Color(0, 0, 0, 0.f), 0.7f), ALIGN_LEFT | ALIGN_MIDDLE);
            if (direct && lit) {
                const int n = direct->currentNote(t);
                if (n >= 0) { char nb[8]; number(r.x + r.w - 40, r.y + 46, noteName(n, nb, sizeof(nb)), 15.f, fg, ALIGN_RIGHT | ALIGN_MIDDLE); }
            }
            const bool muted = bank->mute[t] != 0;
            const R mh = muteBtn(t);
            const R m{mh.x + 6, mh.y + 9, mh.w - 12, mh.h - 14};
            if (muted) box(m, th->lock, 2.f); else frame(m, Color(fg, Color(0, 0, 0, 0.f), 0.6f), 2.f);
            label(m.cx(), m.cy() + 1, "M", 9.f, muted ? th->selText : fg, ALIGN_CENTER | ALIGN_MIDDLE, fnMono);
            if (muted) { beginPath(); rect(r.x + 4, r.y + r.h - 4, r.w - 8, 2); fillColor(th->lock); fill(); }
        }
    }

    void drawLcdSection()
    {
        panelBox({14, 140, 690, 344});
        const R L = lcd();
        box(L, th->lcdBg, th->radius + 1.f);
        const int m = machineOf(selTrack);
        char b[64];
        std::snprintf(b, sizeof(b), "T%d", selTrack + 1);
        number(32, 170, b, 22.f, th->lcdText, ALIGN_LEFT | ALIGN_MIDDLE);
        frame(machPrev(), th->lcdText); label(machPrev().cx(), machPrev().cy() + 1, "\xE2\x97\x80", 10.f, th->lcdText, ALIGN_CENTER | ALIGN_MIDDLE, fnSans);
        frame(machNext(), th->lcdText); label(machNext().cx(), machNext().cy() + 1, "\xE2\x96\xB6", 10.f, th->lcdText, ALIGN_CENTER | ALIGN_MIDDLE, fnSans);
        box(machName(), th->nameFill); label(machName().cx(), machName().cy() + 1, kMachineDefs[m].name, 13.f, th->nameText);
        frame(helpBtn(), th->lcdDim); label(helpBtn().cx(), helpBtn().cy() + 1, "?", 13.f, th->lcdText);
        label(378, 169, kPageNames[page], 11.5f, th->lcdDim, ALIGN_LEFT | ALIGN_MIDDLE);

        if (lockStep >= 0) {
            std::snprintf(b, sizeof(b), "LOCK STEP %02d", lockStep + 1);
            label(586, 169, b, 11.5f, th->lock, ALIGN_RIGHT | ALIGN_MIDDLE);
            frame(lockClear(), th->lock); label(lockClear().cx(), lockClear().cy() + 1, "CLEAR", 10.f, th->lock);
        } else if (rec) {
            iconLabel(688, 169, icRec, recording() ? "REC: play notes / turn knobs" : "REC armed: start playback", 10.f,
                      th->logo == lgRiso ? th->text : th->accent, th->fontLabel, ALIGN_RIGHT);
        } else if (mode == mdNote && noteStep >= 0) {
            std::snprintf(b, sizeof(b), "STEP %02d: play a key", noteStep + 1);
            label(688, 169, b, 10.5f, th->lock, ALIGN_RIGHT | ALIGN_MIDDLE);
        }

        const Pattern& p = pat();
        auto bar = [&](float x, float y, float v01, const Color& c) {
            const R full{x + 4, y + 66, 66, 8};
            frame(full, Color(th->lcdDim, th->lcdBg, 0.4f), 1.f);
            if (th->dottedLeds) {
                dotted({full.x + 1, full.y + 1, (full.w - 2) * v01, full.h - 2}, Color(c, th->lcdBg, 0.25f));
                beginPath(); rect(full.x + 1 + (full.w - 3) * v01, full.y - 1, 2, full.h + 2); fillColor(c); fill();
            } else {
                box({full.x + 1, full.y + 2, std::max(1.f, (full.w - 2) * v01), full.h - 4}, c, 1.f);
            }
        };
        for (int k = 0; k < 8; ++k) {
            const int pi = pageParam(page, k);
            const float x = 32 + k * 83.f, y = 196;
            if (kitField(k) >= 0) {
                const int f = kitField(k), kv = bank->kit[selTrack][f];
                char kb[16];
                label(x, y + 10, kKitNames[f], 11.f, th->lcdDim, ALIGN_LEFT | ALIGN_MIDDLE);
                number(x, y + 40, kitValueText(f, kv, kb, sizeof(kb)), 19.f, th->lcdText, ALIGN_LEFT | ALIGN_MIDDLE);
                bar(x - 4, y, (float)kv / kKitMax[f], th->lcdText);
                continue;
            }
            if (pi < 0) { label(x, y + 30, "--", 12.f, th->lcdDim, ALIGN_LEFT | ALIGN_MIDDLE); continue; }
            const bool locked = lockStep >= 0 && p.locks[selTrack][lockStep][pi] != kNoLock;
            float v = locked ? p.locks[selTrack][lockStep][pi] : tp(selTrack, pi);
            const bool recNow = recording() && drag == dkTrackKnob && dragIndex == k && recDirty;
            if (recNow) v = recVal;
            const Color c = (locked || recNow) ? th->lock : th->lcdText;
            label(x, y + 10, trackParamName(m, pi), 11.f, (locked || recNow) ? th->lock : th->lcdDim, ALIGN_LEFT | ALIGN_MIDDLE);
            const std::string vt = valueText(selTrack, pi, v);
            number(x, y + 40, vt.c_str(), vt.size() > 4 ? 17.f : 24.f, c, ALIGN_LEFT | ALIGN_MIDDLE);
            bar(x - 4, y, v / 127.f, c);
        }

        for (int i = 0; i < kPages; ++i) button(pageTab(i), kPageNames[i], page == i, 11.f);

        for (int k = 0; k < 8; ++k) {
            const int pi = pageParam(page, k);
            if (kitField(k) >= 0) {
                const int f = kitField(k), kv = bank->kit[selTrack][f];
                char kb[16];
                drawKnob(knob(k), (float)kv / kKitMax[f], kKitNames[f], kitValueText(f, kv, kb, sizeof(kb)), false,
                         drag == dkKit && dragIndex == k, true);
                continue;
            }
            if (pi < 0) { drawKnob(knob(k), 0, "", "", false, false, false); continue; }
            const bool locked = lockStep >= 0 && p.locks[selTrack][lockStep][pi] != kNoLock;
            float v = locked ? p.locks[selTrack][lockStep][pi] : tp(selTrack, pi);
            const bool recNow = recording() && drag == dkTrackKnob && dragIndex == k && recDirty;
            if (recNow) v = recVal;
            drawKnob(knob(k), v / 127.f, trackParamName(m, pi), valueText(selTrack, pi, v), locked || recNow,
                     drag == dkTrackKnob && dragIndex == k, true);
        }
    }

    void drawMaster()
    {
        panelBox({710, 140, 460, 344});
        static const char* tabs[3] = {"ECHO", "REVERB", "MASTER"};
        for (int i = 0; i < 3; ++i) button(mTab(i), tabs[i], mpage == i, 11.5f);
        for (int k = 0; k < 8; ++k) {
            const int g = masterParam(mpage, k);
            if (g < 0) { drawKnob(mKnob(k), 0, "", "", false, false, false); continue; }
            const float v = gp(g);
            drawKnob(mKnob(k), v / 127.f, kMasterNames[g - gECHO_TIME], masterText(g, v), false,
                     drag == dkMasterKnob && dragIndex == k, true);
        }
        const R in{716, 394, 448, 88};
        box(in, th->inner, th->radius);
        label(728, 412, "PATTERNS", 10.5f, th->logo == lgRiso ? th->text : th->dim, ALIGN_LEFT | ALIGN_MIDDLE);
        button(patCopy(), patClip >= 0 ? "COPIED" : "COPY", false, 10.5f);
        button(patPaste(), "PASTE", false, 10.5f);
        button(patClear(), "CLEAR", false, 10.5f);
        const int ep = editPattern(), pp = (int)std::lround(gp(gOUT_PATTERN));
        const bool running = gp(gOUT_STEP) >= 0.f;
        for (int i = 0; i < kPatterns; ++i) {
            const R r = patBtn(i);
            char b[8]; std::snprintf(b, sizeof(b), "A%02d", i + 1);
            bool hasContent = false;
            for (int t = 0; t < kTracks && !hasContent; ++t)
                for (int s = 0; s < kSteps; ++s) if (bank->patterns[i].flags[t][s]) { hasContent = true; break; }
            const bool sel = i == ep;
            if (sel) box(r, th->selFill); else { box(r, th->btnFill); frame(r, th->btnEdge); }
            number(r.cx(), r.cy() + 1, b, 14.f, sel ? th->selText : th->btnText);
            if (running && i == pp) { beginPath(); rect(r.x + 4, r.y + r.h - 3, r.w - 8, 2); fillColor(th->accent); fill(); }
            if (hasContent && !sel) { beginPath(); circle(r.x + 5, r.y + 5, 1.8f); fillColor(th->dim); fill(); }
        }
    }

    void drawSequencer()
    {
        panelBox({14, 494, 1156, 204});
        for (int i = 0; i < kModes; ++i) button(modeBtn(i), kModeNames[i], mode == i, 10.f);
        button(fillBtn(), "FILL", fillOn, 10.5f);
        Pattern& p = pat();
        const int len = tlen(selTrack);
        const int pages = (len + 15) / 16;
        if (stepPg >= pages) stepPg = pages - 1;
        const bool samePat = (int)std::lround(gp(gOUT_PATTERN)) == editPattern();
        const bool running = absStep() >= 0;
        const int step = running && samePat ? trackStepNow(selTrack) : -1;
        for (int i = 0; i < 4; ++i) {
            char b[16]; std::snprintf(b, sizeof(b), "%d-%d", i * 16 + 1, i * 16 + 16);
            const R r = stepPage(i);
            if (i < pages) {
                button(r, b, stepPg == i, 10.f);
                if (step >= 0 && step / 16 == i) { beginPath(); rect(r.x + 4, r.y + r.h - 3, r.w - 8, 2); fillColor(th->accent); fill(); }
            } else label(r.cx(), r.cy() + 1, b, 10.f, th->faint);
        }
        char b[32];
        button(lenMinus(), "-", false); button(lenPlus(), "+", false);
        box(lenBox(), th->lcdBg); std::snprintf(b, sizeof(b), "L:%d", len); label(lenBox().cx(), lenBox().cy() + 1, b, 11.f, th->lcdText);
        button(trkCopy(), trackClip.empty() ? "COPY" : "COPIED", false, 10.5f);
        button(trkPaste(), "PASTE", false, 10.5f); button(trkClear(), "CLEAR", false, 10.5f);
        button(lockClrAll(), "-LOCKS", false, 10.5f);

        for (int i = 0; i < 16; ++i) {
            const int s = stepPg * 16 + i;
            const R r = stepBtn(i);
            const bool inLen = s < len;
            const uint8_t f = p.flags[selTrack][s];
            const bool on = (f & sfTrig) && inLen;
            const bool playing = s == step;
            Color bg = inLen ? ((i / 4) % 2 ? th->stepAlt : th->stepFill) : th->stepOff;
            box(r, bg);
            if (playing) frame(r, th->logo == lgRiso ? th->text : th->selFill, -1.f, 2.5f);
            else if (s == lockStep) frame(r, th->lock, -1.f, 2.5f);
            else if (s == noteStep && mode == mdNote) frame(r, th->lock, -1.f, 2.f);
            else frame(r, Color(th->btnEdge, bg, 0.5f));
            // slide into this step: a bridge from the previous step
            if (on && (f & sfSlide)) box({r.x - 6, r.y + 10, 12, 4}, th->lock, 1.f);
            const R led{r.x + 8, r.y + 7, r.w - 16, 9};
            if (on) {
                const Color lc = (f & sfAccent) ? th->lock : th->accent;
                if (th->glow) { beginPath(); rect(led.x - 4, led.y - 4, led.w + 8, led.h + 8); fillPaint(boxGradient(led.x, led.y, led.w, led.h, 2, 8, Color(lc, Color(0, 0, 0, 0.f), 0.5f), Color(0, 0, 0, 0.f))); fill(); }
                box(led, lc, 1.5f);
            } else if (th->dottedLeds) {
                box(led, th->ledOff, 1.5f); dotted({led.x + 1, led.y + 1, led.w - 2, led.h - 2}, Color(th->dots, th->ledOff, inLen ? 0.f : 0.6f));
            } else box(led, inLen ? th->ledOff : Color(th->ledOff, th->bg, 0.5f), 2.f);
            char nb[8]; std::snprintf(nb, sizeof(nb), "%d", s + 1);
            label(r.x + 6, r.y + 24, nb, 9.f, inLen ? th->dim : th->faint, ALIGN_LEFT | ALIGN_MIDDLE);
            if (p.hasLocks(selTrack, s)) { beginPath(); circle(r.x + r.w - 9, r.y + 24, 2.8f); fillColor(th->lock); fill(); }
            if (on) {
                char nn[8];
                if (mode == mdLen) number(r.cx(), r.y + 42, kGateNames[p.gate[selTrack][s] % kGateCount], 16.f, th->stepText);
                else number(r.cx(), r.y + 42, noteName(p.note[selTrack][s], nn, sizeof(nn)), 16.f, th->stepText);
                // gate length bar (one step = full width; longer gates show an arrow)
                const float g = kGateSteps[p.gate[selTrack][s] % kGateCount];
                const float gw = (r.w - 16) * std::min(1.f, g);
                box({r.x + 8, r.y + 56, gw, 3}, Color(th->stepText, bg, 0.45f), 1.f);
                if (g > 1.f) { beginPath(); moveTo(r.x + r.w - 8, r.y + 54); lineTo(r.x + r.w - 3, r.y + 57.5f); lineTo(r.x + r.w - 8, r.y + 61); closePath(); fillColor(Color(th->stepText, bg, 0.45f)); fill(); }
                std::string ex;
                char eb[16];
                if (p.ratch[selTrack][s] > 1) { std::snprintf(eb, sizeof(eb), "x%d ", p.ratch[selTrack][s]); ex += eb; }
                if (p.cond[selTrack][s]) { ex += kConds[p.cond[selTrack][s]].name; ex += " "; }
                if (p.micro[selTrack][s]) { std::snprintf(eb, sizeof(eb), "%+d", p.micro[selTrack][s]); ex += eb; }
                if (!ex.empty()) label(r.cx(), r.y + 68, ex.c_str(), 9.f, th->extra);
            }
        }
    }

    void drawKeyboard()
    {
        // octave selector
        button(octDown(), "-", false, 14.f);
        button(octUp(), "+", false, 14.f);
        const R ob = octBox();
        box(ob, th->lcdBg); frame(ob, th->panelEdge, -1.f, th->panelEdgeWidth);
        label(ob.cx(), ob.y + 16, "OCT", 9.f, th->lcdDim);
        char b[48], nb[8];
        number(ob.cx(), ob.y + 42, noteName(kbLow(), nb, sizeof(nb)), 17.f, th->lcdText);

        const int lo = kbLow(), hi = lo + 12 * kKbOctaves;
        const int playing = direct && ((direct->heldMask() >> selTrack) & 1) ? direct->currentNote(selTrack) : -1;
        const int stepNote = (mode == mdNote && noteStep >= 0 && (pat().flags[selTrack][noteStep] & sfTrig)) ? pat().note[selTrack][noteStep] : -1;
        auto lum = [](const Color& c) { return 0.3f * c.red + 0.59f * c.green + 0.11f * c.blue; };
        const Color whiteKey = lum(th->text) > lum(th->stepFill) ? Color(th->text, th->panel, 0.12f) : th->stepFill;
        const Color blackKey = lum(th->knobBody) < lum(th->text) ? th->knobBody : th->text;
        auto keyColor = [&](int n, bool black) -> Color {
            if (n == keyNote || pcIsHeld(n)) return th->accent;
            if (n == playing) return Color(th->accent, black ? blackKey : whiteKey, 0.35f);
            if (n == stepNote) return th->lock;
            return black ? blackKey : whiteKey;
        };
        for (int n = lo; n <= hi; ++n) {
            if (isBlack(n % 12)) continue;
            const R r = keyRect(n);
            box(r, keyColor(n, false), 3.f);
            if (n % 12 == 0) label(r.cx(), r.y + r.h - 10, noteName(n, nb, sizeof(nb)), 8.5f, Color(blackKey, whiteKey, 0.3f));
        }
        for (int n = lo; n <= hi; ++n) {
            if (!isBlack(n % 12)) continue;
            const R r = keyRect(n);
            box(r, keyColor(n, true), 2.f);
            frame(r, Color(th->bg, blackKey, 0.3f), 2.f);
        }
        // right side: computer keyboard toggle, panic, what the keys play
        button(pcKeysBtn(), pcKeys ? "PC KEYS: ON" : "PC KEYS: OFF", pcKeys, 10.f);
        button(panicBtn(), "ALL NOTES OFF", false, 10.f);
        std::snprintf(b, sizeof(b), "KEYS PLAY TRACK %d", selTrack + 1);
        label(948, 636, b, 10.5f, th->text, ALIGN_LEFT | ALIGN_MIDDLE);
        const int ch = bank->kit[selTrack][kfMidi];
        if (ch > 0) std::snprintf(b, sizeof(b), "MIDI IN: CHANNEL %d", ch);
        else std::snprintf(b, sizeof(b), "MIDI IN: FOLLOWS SELECTION");
        label(948, 656, b, 9.5f, th->dim, ALIGN_LEFT | ALIGN_MIDDLE);
        if (mode == mdNote) label(948, 676, noteStep >= 0 ? "STEP ENTRY ON" : "click a step, then a key", 9.5f, th->lock, ALIGN_LEFT | ALIGN_MIDDLE);
        else if (pcKeys) label(948, 676, "A W S E D F T G Y H U J K   Z/X = octave", 9.f, th->dim, ALIGN_LEFT | ALIGN_MIDDLE);
    }

    void overlayBox(const R& r)
    {
        beginPath(); rect(0, 0, W, H); fillColor(th->overlay); fill();
        box(r, th->panel, th->radius + 3.f); frame(r, th->panelEdge, th->radius + 3.f, th->panelEdgeWidth + 0.5f);
    }

    void drawPicker()
    {
        const R bx = pickerBox();
        overlayBox(bx);
        char b[64]; std::snprintf(b, sizeof(b), "Machine for track %d", selTrack + 1);
        number(bx.x + 26, bx.y + 34, b, 18.f, th->text, ALIGN_LEFT | ALIGN_MIDDLE);
        label(bx.x + bx.w - 26, bx.y + 34, "Esc or click outside = cancel", 10.5f, th->dim, ALIGN_RIGHT | ALIGN_MIDDLE);
        const int cur = machineOf(selTrack);
        for (int m = 0; m < kMachines; ++m) {
            const R r = pickerCell(m);
            const bool sel = m == cur;
            if (sel) box(r, th->selFill); else { box(r, th->btnFill); frame(r, th->btnEdge); }
            number(r.x + 16, r.y + 32, kMachineDefs[m].name, 20.f, sel ? th->selText : th->text, ALIGN_LEFT | ALIGN_MIDDLE);
            label(r.x + 16, r.y + 66, kMachineShort[m], 10.f, sel ? th->selText : th->dim, ALIGN_LEFT | ALIGN_MIDDLE);
        }
    }

    // ------------------------------------------------------------------ bottom bar / overlays
    void drawBottomBar()
    {
        const R rb = recBtn();
        const bool live = recording();
        if (rec) {
            box(rb, th->accent);
            iconLabel(rb.cx(), rb.cy() + 1, icRec, "REC", 13.f, th->accentText);
            if (live && th->glow) { beginPath(); rect(rb.x - 6, rb.y - 6, rb.w + 12, rb.h + 12); fillPaint(boxGradient(rb.x, rb.y, rb.w, rb.h, 4, 10, Color(th->accent, Color(0, 0, 0, 0.f), 0.45f), Color(0, 0, 0, 0.f))); fill(); }
        } else {
            box(rb, th->btnFill); frame(rb, th->btnEdge, -1.f, 1.5f);
            iconLabel(rb.cx(), rb.cy() + 1, icRec, "REC", 13.f, th->logo == lgRiso ? th->text : th->accent);
        }
        const R grp = rndGroup();
        box(grp, th->inner);
        const Color gt = th->logo == lgRiso ? th->text : th->dim;
        label(134, grp.y + 20, "RANDOMIZE", 10.f, gt, ALIGN_LEFT | ALIGN_MIDDLE);
        label(134, grp.y + 37, "undo restores", 9.f, gt, ALIGN_LEFT | ALIGN_MIDDLE);
        static const char* rn[4] = {"SOUND", "SEQUENCE", "PATTERN", "EVERYTHING"};
        static const char* rs[4] = {"this track", "this track", "all tracks", "kit+fx+pattern"};
        for (int i = 0; i < 4; ++i) {
            const R r = rndBtn(i);
            box(r, th->btnFill); frame(r, th->btnEdge);
            label(r.cx(), r.y + 14, rn[i], 10.5f, th->btnText);
            label(r.cx(), r.y + 30, rs[i], 9.f, th->dim);
        }
        const R u = undoBtn();
        if (undoValid) { box(u, th->btnFill); frame(u, th->lock, -1.f, 1.5f); }
        else { box(u, th->btnFill); frame(u, th->btnEdge); }
        label(u.cx(), u.y + 14, "UNDO", 10.5f, undoValid ? th->lock : th->faint);
        label(u.cx(), u.y + 30, undoValid ? undoWhat : "", 9.f, th->dim);
        label(996, 718, "PRESET", 9.5f, th->dim, ALIGN_RIGHT | ALIGN_MIDDLE);
        if (th->fontNum == fnSerif) number(996, 744, presetName.c_str(), 19.f, th->text, ALIGN_RIGHT | ALIGN_MIDDLE, fnSerifItalic);
        else number(996, 744, presetName.c_str(), 16.f, th->text, ALIGN_RIGHT | ALIGN_MIDDLE);
        const R pb = presetBtn();
        box(pb, th->btnFill); frame(pb, th->btnEdge, -1.f, 1.5f);
        label(pb.cx() - 8, pb.cy() + 1, "PRESETS", 11.5f, th->btnText);
        beginPath(); moveTo(pb.x + pb.w - 28, pb.cy() - 5); lineTo(pb.x + pb.w - 21, pb.cy() + 1); lineTo(pb.x + pb.w - 28, pb.cy() + 7);
        closePath(); fillColor(th->btnText); fill();
        const R sv = saveBtn();
        box(sv, th->btnFill); frame(sv, th->btnEdge, -1.f, 1.f);
        label(sv.cx(), sv.cy() + 1, "SAVE AS PRESET", 10.5f, th->btnText);
    }

    int presetCount() const { return presetTabUser ? (int)userPresets.size() : kNumPresets; }

    void drawPresets()
    {
        const R bx = presetBox();
        overlayBox(bx);
        number(100, bx.y + 32, "Presets", 22.f, th->text, ALIGN_LEFT | ALIGN_MIDDLE);
        button(presetTab(0), "FACTORY", !presetTabUser, 11.f);
        char b[64]; std::snprintf(b, sizeof(b), "USER (%d)", (int)userPresets.size());
        button(presetTab(1), b, presetTabUser, 11.f);
        label(1080, bx.y + 32, "double-click = load everything", 10.f, th->dim, ALIGN_RIGHT | ALIGN_MIDDLE);
        const int n = presetCount();
        for (int k = 0; k < 22; ++k) {
            const int i = k + presetScroll * 2;
            if (i >= n) break;
            const R r = presetRow(k);
            const bool sel = i == presetSel;
            if (sel) box(r, th->selFill); else { box(r, th->btnFill); frame(r, th->btnEdge); }
            if (presetTabUser) {
                number(r.x + 14, r.cy() + 1, userPresets[i].c_str(), 15.f, sel ? th->selText : th->text, ALIGN_LEFT | ALIGN_MIDDLE);
            } else {
                number(r.x + 12, r.y + 13, kPresets[i].name, 14.f, sel ? th->selText : th->text, ALIGN_LEFT | ALIGN_MIDDLE);
                label(r.x + 12, r.y + 29, kPresets[i].desc, 9.f, sel ? th->selText : th->dim, ALIGN_LEFT | ALIGN_MIDDLE);
            }
        }
        if (presetTabUser && n == 0) {
            label(bx.cx(), 340, "No user presets yet.", 13.f, th->text);
            label(bx.cx(), 370, "Use SAVE CURRENT AS... (or SAVE AS PRESET on the main panel).", 11.f, th->dim);
        }
        if (n > 22) {
            std::snprintf(b, sizeof(b), "scroll for more (%d-%d of %d)", presetScroll * 2 + 1, std::min(n, presetScroll * 2 + 22), n);
            label(1080, 640, b, 9.5f, th->dim, ALIGN_RIGHT | ALIGN_MIDDLE);
        }
        label(100, 640, presetMsg.c_str(), 10.f, th->lock, ALIGN_LEFT | ALIGN_MIDDLE);
        const bool ok = presetSel >= 0 && presetSel < n;
        button(presetSaveNew(), "SAVE CURRENT AS...", false, 10.5f);
        if (presetTabUser) button(presetDelete(), deleteArmed ? "CONFIRM?" : "DELETE", deleteArmed, 10.5f);
        button(presetLoadAll(), "LOAD SOUND + PATTERNS", ok, 10.5f);
        button(presetLoadKit(), "LOAD SOUND ONLY", false, 10.5f);
        button(presetCancel(), "CLOSE", false, 10.5f);
    }

    void openPresets()
    {
        refreshUserPresets();
        overlay = ovPresets; presetSel = -1; presetScroll = 0; deleteArmed = false; presetMsg.clear();
        repaint();
    }

    void presetClick(float x, float y, bool dbl)
    {
        for (int t = 0; t < 2; ++t)
            if (presetTab(t).in(x, y)) {
                presetTabUser = t == 1; presetSel = -1; presetScroll = 0; deleteArmed = false; presetMsg.clear();
                if (presetTabUser) refreshUserPresets();
                repaint(); return;
            }
        const int n = presetCount();
        for (int k = 0; k < 22; ++k) {
            const int i = k + presetScroll * 2;
            if (i < n && presetRow(k).in(x, y)) {
                deleteArmed = false;
                if (dbl && presetSel == i) { loadAnyPreset(i, true); overlay = ovNone; }
                else presetSel = i;
                repaint(); return;
            }
        }
        if (presetSaveNew().in(x, y)) { openSave(); return; }
        if (presetTabUser && presetDelete().in(x, y) && presetSel >= 0 && presetSel < n) {
            if (!deleteArmed) { deleteArmed = true; repaint(); return; }
            std::remove(userPresetPath(userPresets[presetSel]).c_str());
            presetMsg = "Deleted \"" + userPresets[presetSel] + "\"";
            deleteArmed = false; presetSel = -1; refreshUserPresets(); repaint(); return;
        }
        deleteArmed = false;
        if (presetSel >= 0 && presetSel < n && presetLoadAll().in(x, y)) { loadAnyPreset(presetSel, true); overlay = ovNone; }
        else if (presetSel >= 0 && presetSel < n && presetLoadKit().in(x, y)) { loadAnyPreset(presetSel, false); overlay = ovNone; }
        else if (presetCancel().in(x, y) || !presetBox().in(x, y)) overlay = ovNone;
        repaint();
    }

    void loadAnyPreset(int i, bool withPatterns)
    {
        if (presetTabUser) loadUserPreset(userPresets[i], withPatterns);
        else loadPreset(i, withPatterns);
    }

    // ------------------------------------------------------------------ user presets (files)
    static std::string userDir()
    {
       #ifdef _WIN32
        const char* base = std::getenv("APPDATA");
        if (!base) return std::string();
        std::string d = std::string(base) + "\\Kinetik";
        _mkdir(d.c_str()); d += "\\KMSP-6"; _mkdir(d.c_str()); d += "\\Presets"; _mkdir(d.c_str());
        return d + "\\";
       #else
        const char* base = std::getenv("HOME");
        if (!base) return std::string();
       #ifdef __APPLE__
        std::string d = std::string(base) + "/Library/Application Support";
       #else
        std::string d = std::string(base) + "/.config";
       #endif
        mkdir(d.c_str(), 0755); d += "/Kinetik"; mkdir(d.c_str(), 0755); d += "/KMSP-6"; mkdir(d.c_str(), 0755);
        d += "/Presets"; mkdir(d.c_str(), 0755);
        return d + "/";
       #endif
    }
    static std::string cleanName(const std::string& n)
    {
        std::string o;
        for (char c : n) {
            const bool okc = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == ' ' || c == '-' || c == '_';
            o.push_back(okc ? c : '_');
        }
        while (!o.empty() && o.back() == ' ') o.pop_back();
        while (!o.empty() && o.front() == ' ') o.erase(o.begin());
        return o;
    }
    static std::string userPresetPath(const std::string& name) { return userDir() + name + ".kmsp"; }

    void refreshUserPresets()
    {
        userPresets.clear();
        const std::string d = userDir();
        if (d.empty()) return;
        if (DIR* dir = opendir(d.c_str())) {
            while (dirent* e = readdir(dir)) {
                std::string f = e->d_name;
                if (f.size() > 5 && f.compare(f.size() - 5, 5, ".kmsp") == 0) userPresets.push_back(f.substr(0, f.size() - 5));
            }
            closedir(dir);
        }
        std::sort(userPresets.begin(), userPresets.end(), [](const std::string& a, const std::string& b) {
            std::string la = a, lb = b;
            for (auto& c : la) c = (char)std::tolower((unsigned char)c);
            for (auto& c : lb) c = (char)std::tolower((unsigned char)c);
            return la < lb; });
    }

    bool saveUserPreset(const std::string& name)
    {
        const std::string path = userPresetPath(name);
        FILE* fp = std::fopen(path.c_str(), "wb");
        if (!fp) return false;
        std::fprintf(fp, "KMSP6-PRESET 1\nNAME %s\nPATTERN %d\n", name.c_str(), editPattern());
        for (int i = 0; i < kGlobalBase + gOUT_STEP; ++i) {
            if (i == kGlobalBase + gPLAY || i == kGlobalBase + gPATTERN) continue;
            std::fprintf(fp, "V %d %.4f\n", i, values[i]);
        }
        std::fprintf(fp, "BANK\n%s", serializeBank(*bank).c_str());
        std::fclose(fp);
        return true;
    }

    void loadUserPreset(const std::string& name, bool withPatterns)
    {
        FILE* fp = std::fopen(userPresetPath(name).c_str(), "rb");
        if (!fp) { presetMsg = "Could not open " + name; return; }
        std::string all; char buf[4096]; size_t n;
        while ((n = std::fread(buf, 1, sizeof(buf), fp)) > 0) all.append(buf, n);
        std::fclose(fp);
        if (all.rfind("KMSP6-PRESET", 0) != 0) { presetMsg = "Not a KMSP-6 preset: " + name; return; }
        snapshot("preset");
        std::vector<float> nv(values, values + kTotalParams);
        std::unique_ptr<Bank> nb(new Bank(*bank));
        int patIdx = 0;
        const size_t bankPos = all.find("\nBANK\n");
        const std::string head = all.substr(0, bankPos == std::string::npos ? all.size() : bankPos);
        size_t st = 0;
        while (st < head.size()) {
            size_t e = head.find('\n', st); if (e == std::string::npos) e = head.size();
            const std::string line = head.substr(st, e - st);
            int idx; float v;
            if (std::sscanf(line.c_str(), "V %d %f", &idx, &v) == 2 && idx >= 0 && idx < kGlobalBase + gOUT_STEP) nv[idx] = v;
            std::sscanf(line.c_str(), "PATTERN %d", &patIdx);
            st = e + 1;
        }
        if (bankPos != std::string::npos) {
            std::unique_ptr<Bank> fileBank(new Bank());
            if (deserializeBank(*fileBank, all.c_str() + bankPos + 6)) {
                std::memcpy(nb->machine, fileBank->machine, sizeof(nb->machine));
                std::memcpy(nb->mute, fileBank->mute, sizeof(nb->mute));
                std::memcpy(nb->kit, fileBank->kit, sizeof(nb->kit));
                if (withPatterns) for (int p = 0; p < kPatterns; ++p) nb->patterns[p] = fileBank->patterns[p];
            }
        }
        applyState(nv.data(), nb.get());
        if (withPatterns) { setParamGesture(kGlobalBase + gPATTERN, (float)clampi(patIdx, 0, kPatterns - 1)); stepPg = 0; }
        lockStep = -1; noteStep = -1;
        presetName = name;
    }

    // ------------------------------------------------------------------ save-as window (text entry)
    void openSave()
    {
        refreshUserPresets();
        saveName = cleanName(presetName);
        if (saveName.empty() || saveName == "Init" || saveName == "(random)") {
            int k = 1;
            char b[32];
            do { std::snprintf(b, sizeof(b), "My preset %d", k++); } while (std::find(userPresets.begin(), userPresets.end(), std::string(b)) != userPresets.end());
            saveName = b;
        }
        saveConfirm = false;
        overlay = ovSave; repaint();
    }

    bool saveExists() const { return std::find(userPresets.begin(), userPresets.end(), saveName) != userPresets.end(); }

    void commitSave()
    {
        saveName = cleanName(saveName);
        if (saveName.empty()) return;
        if (saveExists() && !saveConfirm) { saveConfirm = true; repaint(); return; }
        if (saveUserPreset(saveName)) {
            presetName = saveName;
            presetMsg = "Saved \"" + saveName + "\"";
            refreshUserPresets();
            presetTabUser = true;
        } else presetMsg = "Could not write the preset file";
        overlay = ovNone; repaint();
    }

    void drawSave()
    {
        const R bx = saveBox();
        overlayBox(bx);
        number(320, bx.y + 36, "Save as preset", 22.f, th->text, ALIGN_LEFT | ALIGN_MIDDLE);
        label(320, bx.y + 70, "Name (type on the keyboard, Enter = save, Esc = cancel)", 10.5f, th->dim, ALIGN_LEFT | ALIGN_MIDDLE);
        const R f = saveField();
        box(f, th->lcdBg); frame(f, th->lcdText, -1.f, 1.5f);
        number(f.x + 16, f.cy() + 1, saveName.c_str(), 20.f, th->lcdText, ALIGN_LEFT | ALIGN_MIDDLE);
        useFont(th->fontNum); fontSize(th->fontNum == fnSerif ? 25.f : (th->fontNum == fnBlack ? 19.f : 20.f));
        DGL_NAMESPACE::Rectangle<float> bnd; float cx = f.x + 16;
        if (!saveName.empty()) { textBounds(f.x + 16, f.cy(), saveName.c_str(), nullptr, bnd); cx = bnd.getX() + bnd.getWidth() + 3; }
        beginPath(); rect(cx, f.y + 12, 2, f.h - 24); fillColor(th->lcdText); fill();
        if (saveExists())
            label(320, bx.y + 166, saveConfirm ? "Click SAVE again to replace it." : "A user preset with this name exists: it will be replaced.", 10.5f, th->lock, ALIGN_LEFT | ALIGN_MIDDLE);
        else
            label(320, bx.y + 166, "Saves the kit, all 16 patterns, effects and tempo.", 10.5f, th->dim, ALIGN_LEFT | ALIGN_MIDDLE);
        button(saveCancel(), "CANCEL", false, 11.f);
        button(saveOk(), saveConfirm ? "REPLACE" : "SAVE", true, 12.f);
    }

    bool onCharacterInput(const CharacterInputEvent& ev) override
    {
        if (overlay != ovSave) return pcKeys && overlay == ovNone;
        const uint c = ev.character;
        if (c >= 32 && c < 127 && saveName.size() < 28) {
            const char ch = (char)c;
            const bool okc = (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9') || ch == ' ' || ch == '-' || ch == '_';
            if (okc) { saveName.push_back(ch); saveConfirm = false; repaint(); }
        }
        return true;
    }

    // ------------------------------------------------------------------ view (skin + size)
    double baseScale() const { const double s = getScaleFactor(); return s > 0.1 ? s : 1.0; }
    int zoomPercent() const { return (int)std::lround(getWidth() / (W * baseScale()) * 100.0); }
    void setZoom(double z)
    {
        z = std::max(0.6, std::min(3.0, z));
        const uint w = (uint)std::lround(W * baseScale() * z), h = (uint)std::lround(H * baseScale() * z);
        if (std::abs((int)w - (int)getWidth()) > 1) setSize(w, h);
        viewDirty = true;
        repaint();
    }
    void saveView()
    {
        char b[32]; std::snprintf(b, sizeof(b), "%d %d", skin, zoomPercent());
        setState("skin", b);      // saved with the project
        writeDefaultView(skin, zoomPercent()); // and used by new instances
        viewDirty = false;
    }
    void onResize(const ResizeEvent& ev) override
    {
        UI::onResize(ev);
        viewDirty = true; viewDirtyTicks = 0;
    }
    void viewIdle()
    {
        // persist the window size a moment after the user stops resizing
        if (viewDirty && drag != (DragKind)dkResize && ++viewDirtyTicks > 30) saveView();
    }

    void drawSkins()
    {
        const R vb = viewBox();
        overlayBox(vb);
        number(264, 158, "Skin and size", 22.f, th->text, ALIGN_LEFT | ALIGN_MIDDLE);
        label(916, 158, "saved with the project and for new instances", 10.f, th->dim, ALIGN_RIGHT | ALIGN_MIDDLE);
        for (int i = 0; i < kNumThemes; ++i) {
            const R r = skinRow(i);
            const Theme& t = kThemes[i];
            box(r, t.bg, 4.f); frame(r, i == skin ? th->accent : th->btnEdge, 4.f, i == skin ? 3.f : 1.f);
            const Theme* keep = th; th = &t;
            number(r.x + 14, r.y + 22, t.name, 16.f, t.text, ALIGN_LEFT | ALIGN_MIDDLE);
            // miniature: an LCD value, a knob and a few steps drawn with the skin itself
            box({r.x + 14, r.y + 40, 120, 52}, t.lcdBg);
            label(r.x + 22, r.y + 52, "DEC", 8.f, t.lcdDim, ALIGN_LEFT | ALIGN_MIDDLE);
            number(r.x + 22, r.y + 74, "75", 20.f, t.lcdText, ALIGN_LEFT | ALIGN_MIDDLE);
            drawKnob({r.x + 140, r.y + 26, 80, 110}, 0.6f, "", "", false, false, true);
            for (int k = 0; k < 4; ++k) {
                const R st{r.x + 224 + (k % 2) * 40.f, r.y + 40 + (k / 2) * 44.f, 36, 40};
                box(st, t.stepFill);
                box({st.x + 5, st.y + 6, st.w - 10, 7}, k == 0 || k == 3 ? t.accent : t.ledOff, 1.f);
                char nb[4]; std::snprintf(nb, sizeof(nb), "%d", k + 1);
                number(st.cx(), st.y + 27, nb, 12.f, t.stepText);
            }
            th = keep;
        }
        label(264, 502, "WINDOW SIZE", 11.f, th->dim, ALIGN_LEFT | ALIGN_MIDDLE);
        static const int zooms[6] = {80, 100, 120, 140, 160, 200};
        const int zp = zoomPercent();
        for (int i = 0; i < 6; ++i) {
            char b[16]; std::snprintf(b, sizeof(b), "%d%%", zooms[i]);
            button(zoomBtn(i), b, std::abs(zp - zooms[i]) <= 2, 12.f);
        }
        label(264, 580, "You can also drag the corner at the bottom right of the window to any size.", 10.5f, th->dim, ALIGN_LEFT | ALIGN_MIDDLE);
        label(264, 606, "Right-click the SKIN / SIZE button to switch skin directly.", 10.5f, th->dim, ALIGN_LEFT | ALIGN_MIDDLE);
    }

    void drawGrip()
    {
        const R g = gripR();
        const Color c = drag == (DragKind)dkResize ? th->accent : Color(th->dim, th->bg, 0.2f);
        for (int i = 0; i < 3; ++i) {
            const float o = 6.f + i * 5.f;
            beginPath(); moveTo(W - 3, H - o); lineTo(W - o, H - 3);
            strokeColor(c); strokeWidth(1.6f); lineCap(ROUND); stroke();
        }
        (void)g;
    }

    void loadPreset(int i, bool withPatterns)
    {
        snapshot("preset");
        std::vector<float> nv(values, values + kTotalParams);
        std::unique_ptr<Bank> nb(new Bank());
        buildPreset(i, nv.data(), *nb);
        if (!withPatterns)
            for (int p = 0; p < kPatterns; ++p) nb->patterns[p] = bank->patterns[p];
        applyState(nv.data(), nb.get());
        if (withPatterns) { setParamGesture(kGlobalBase + gPATTERN, 0.f); stepPg = 0; }
        lockStep = -1; noteStep = -1;
        presetName = kPresets[i].name;
    }

    // ------------------------------------------------------------------ whole-state helpers (presets, random, undo)
    void applyState(const float* nv, const Bank* nb)
    {
        for (int i = 0; i < kGlobalBase + gOUT_STEP; ++i) {
            if (i == kGlobalBase + gPLAY || i == kGlobalBase + gPATTERN) continue;
            if (std::fabs(values[i] - nv[i]) > 1e-4f) setParamGesture((uint32_t)i, nv[i]);
        }
        if (nb) {
            *bank = *nb;
            setState("bank", serializeBank(*bank).c_str());
        }
        repaint();
    }

    void snapshot(const char* what)
    {
        undoValues.assign(values, values + kTotalParams);
        undoBank = serializeBank(*bank);
        undoValid = true; undoWhat = what;
    }

    void undo()
    {
        if (!undoValid) return;
        std::unique_ptr<Bank> b(new Bank());
        deserializeBank(*b, undoBank.c_str());
        std::vector<float> v = undoValues;
        undoValid = false;
        applyState(v.data(), b.get());
    }

    void randomize(int what)
    {
        RandGen r(++rndCounter * 2654435761u ^ lastClickTime);
        static const char* names[4] = {"sound", "sequence", "pattern", "everything"};
        snapshot(names[what]);
        std::vector<float> nv(values, values + kTotalParams);
        if (what == 0) {
            randomizeSound(nv.data(), selTrack, machineOf(selTrack), r);
            applyState(nv.data(), nullptr);
        } else if (what == 1) {
            std::unique_ptr<Pattern> tmp(new Pattern(pat()));
            randomizeSequence(*tmp, selTrack, machineOf(selTrack), r);
            sendCmd("tr " + std::to_string(editPattern()) + " " + std::to_string(selTrack) + " " + trackToHex(*tmp, selTrack));
        } else {
            std::unique_ptr<Bank> nb(new Bank(*bank));
            if (what == 3) randomizeKit(nv.data(), *nb, r);
            randomizePattern(nb->patterns[editPattern()], *nb, r);
            applyState(nv.data(), nb.get());
        }
        if (what >= 2) { lockStep = -1; noteStep = -1; }
        presetName = what == 3 ? "(random)" : presetName;
        repaint();
    }

    // ------------------------------------------------------------------ live recording (REC + playing)
    bool recording() const
    {
        return rec && gp(gOUT_STEP) >= 0.f && (int)std::lround(gp(gOUT_PATTERN)) == editPattern();
    }
    void recordLock(int pi, int v)
    {
        const int s = trackStepNow(selTrack);
        if (pi < 0 || s < 0 || s >= kSteps) return;
        Pattern& p = pat();
        if (!(p.flags[selTrack][s] & sfTrig)) return; // locks are recorded onto trigs
        if (p.locks[selTrack][s][pi] == v) return;
        char b[64];
        std::snprintf(b, sizeof(b), "l %d %d %d %d %d", editPattern(), selTrack, s, pi, clampi(v, 0, 127));
        sendCmd(b);
    }
    // notes recorded by the engine (MIDI keyboard, on-screen keys) are written into the pattern here
    void pollRecorded()
    {
        if (!direct) return;
        std::string c;
        int pt, t, s, n, g; bool acc;
        char b[64];
        while (direct->popRecorded(pt, t, s, n, acc, g)) {
            if (pt < 0 || pt >= kPatterns || t < 0 || t >= kTracks || s < 0 || s >= kSteps) continue;
            if (g < 0) {
                const uint8_t f = (uint8_t)(sfTrig | (acc ? sfAccent : 0) | (bank->patterns[pt].flags[t][s] & sfSlide));
                std::snprintf(b, sizeof(b), "f %d %d %d %d\nn %d %d %d %d\n", pt, t, s, f, pt, t, s, n); c += b;
            } else {
                std::snprintf(b, sizeof(b), "g %d %d %d %d\n", pt, t, s, g); c += b;
            }
        }
        if (!c.empty()) { c.pop_back(); sendLines(c); repaint(); }
    }

    void setKit(int t, int f, int v)
    {
        char b[48]; std::snprintf(b, sizeof(b), "K %d %d %d", t, f, clampi(v, 0, kKitMax[f])); sendCmd(b); repaint();
    }

    // make sure a step has a trig (used by NOTE / LEN / RATCH / COND / TIME modes)
    void ensureTrig(std::string& c, int s)
    {
        if (pat().flags[selTrack][s] & sfTrig) return;
        char b[96];
        std::snprintf(b, sizeof(b), "f %d %d %d %d\nn %d %d %d %d\n", editPattern(), selTrack, s, sfTrig, editPattern(), selTrack, s, lastNote);
        c += b;
    }
    void setStepNote(int s, int n)
    {
        std::string c;
        ensureTrig(c, s);
        char b[48]; std::snprintf(b, sizeof(b), "n %d %d %d %d", editPattern(), selTrack, s, clampi(n, 0, 127));
        c += b; sendLines(c);
    }
    void setStepGate(int s, int g)
    {
        std::string c;
        ensureTrig(c, s);
        char b[48]; std::snprintf(b, sizeof(b), "g %d %d %d %d", editPattern(), selTrack, s, clampi(g, 0, kGateCount - 1));
        c += b; sendLines(c);
    }

    // RATCH / COND / TIME modes: left click = next value, right click = previous, double-click = reset
    void stepExtra(int s, bool right, bool dbl, bool shift)
    {
        Pattern& p = pat();
        const int pi = editPattern();
        std::string c;
        char b[64];
        ensureTrig(c, s);
        if (mode == mdRatch) {
            int idx = 0;
            for (int i = 0; i < kRatchCount; ++i) if (kRatchValues[i] == std::max<int>(1, p.ratch[selTrack][s])) idx = i;
            idx = dbl ? 0 : (idx + (right ? kRatchCount - 1 : 1)) % kRatchCount;
            std::snprintf(b, sizeof(b), "r %d %d %d %d", pi, selTrack, s, kRatchValues[idx]);
        } else if (mode == mdCond) {
            const int v = dbl ? 0 : (p.cond[selTrack][s] + (right ? kNumConds - 1 : 1)) % kNumConds;
            std::snprintf(b, sizeof(b), "c %d %d %d %d", pi, selTrack, s, v);
        } else {
            const int st = shift ? 4 : 1;
            const int v = dbl ? 0 : clampi(p.micro[selTrack][s] + (right ? -st : st), -kMicroMax, kMicroMax);
            std::snprintf(b, sizeof(b), "m %d %d %d %d", pi, selTrack, s, v);
        }
        c += b;
        sendLines(c);
    }

    void drawPatternSettings()
    {
        Pattern& p = pat();
        char b[32];
        auto cell = [&](const R& r, const char* top, const char* val) {
            box(r, th->lcdBg); frame(r, th->panelEdge, -1.f, th->panelEdgeWidth);
            label(r.cx(), r.y + 11, top, 9.f, th->lcdDim); number(r.cx(), r.y + 28, val, 15.f, th->lcdText);
        };
        std::snprintf(b, sizeof(b), "%d", p.length); cell(patLenBox(), "PAT LEN", b);
        cell(scaleBox(), "SPEED", kScaleNames[p.scale % 7]);
        std::snprintf(b, sizeof(b), "%d%%", p.swing); cell(swingBox(), "SWING", b);
        std::snprintf(b, sizeof(b), "%d", p.accent); cell(accBox(), "ACCENT", b);
    }

    // ------------------------------------------------------------------ per-track length
    void setTrackLen(int t, int newLen, bool extend, bool allTracks)
    {
        newLen = clampi(newLen, 1, kSteps);
        std::string cmds;
        for (int tt = 0; tt < kTracks; ++tt) {
            if (!allTracks && tt != t) continue;
            std::unique_ptr<Pattern> tmp(new Pattern(pat()));
            const int old = tmp->trackLen[tt] ? tmp->trackLen[tt] : 16;
            if (extend && newLen > old)
                for (int s = old; s < newLen; ++s) tmp->copyStep(tt, s, s % old);
            tmp->trackLen[tt] = (uint8_t)newLen;
            if (!cmds.empty()) cmds += "\n";
            cmds += "tr " + std::to_string(editPattern()) + " " + std::to_string(tt) + " " + trackToHex(*tmp, tt);
        }
        int maxLen = 1;
        for (int tt = 0; tt < kTracks; ++tt) maxLen = std::max(maxLen, allTracks || tt == t ? newLen : tlen(tt));
        if (maxLen > pat().length || allTracks) {
            const Pattern& p = pat();
            char b[64]; std::snprintf(b, sizeof(b), "\nP %d %d %d %d %d", editPattern(), allTracks ? newLen : maxLen, p.scale, p.swing, p.accent);
            cmds += b;
        }
        sendLines(cmds);
        if (lockStep >= newLen) lockStep = -1;
        if (noteStep >= newLen) noteStep = -1;
    }

    void drawHelp()
    {
        const R box0{80, 80, 1020, 640};
        overlayBox(box0);
        const int m = machineOf(selTrack);
        char b[96];
        std::snprintf(b, sizeof(b), "%s  -  track %d  -  %s page", kMachineDefs[m].name, selTrack + 1, kPageNames[page]);
        number(104, 110, b, 19.f, th->text, ALIGN_LEFT | ALIGN_MIDDLE);
        label(1076, 110, "click anywhere to close", 10.5f, th->dim, ALIGN_RIGHT | ALIGN_MIDDLE);
        const char* intro = page == pgSynth ? kMachineHelp[m].summary
                          : page == pgTrack ? "Track settings, saved with the kit (not automatable, not lockable)."
                          : page == pgArp ? "Arpeggiator. With the sequencer it plays the CHORD on each note; with MIDI or the keys it plays the notes you hold."
                          : page >= pgLfo1 ? "Each track has three LFOs. They can move any parameter of this track or of another track (TRK), or the pitch."
                          : page == pgAmp ? "Amplitude envelope (attack, hold, decay, sustain, release) and output. The gate length of each step (LEN mode) decides when the release starts."
                          : page == pgFilter ? "A high-pass and a low-pass filter in series with their own envelope. All of these can be locked per step."
                          : "Per-track effects, plus the sends to the master echo and reverb.";
        fontFace(NANOVG_DEJAVU_SANS_TTF); fontSize(14.f); fillColor(th->lock); textAlign(ALIGN_LEFT | ALIGN_TOP);
        textBox(104, 130, 970, intro, nullptr);
        for (int k = 0; k < 8; ++k) {
            const int pi = pageParam(page, k);
            const float y = 190 + k * 54.f;
            if (pi < 0 && kitField(k) < 0) continue;
            const char* nm = kitField(k) >= 0 ? kKitNames[kitField(k)] : trackParamName(m, pi);
            const char* tx = kitField(k) >= 0 ? kKitHelp[kitField(k)] : (pi < 8 ? kMachineHelp[m].p[pi] : kFxHelp[pi - 8]);
            box({104, y, 74, 42}, th->lcdBg);
            label(141, y + 22, nm, 11.5f, th->lcdText);
            fontFace(NANOVG_DEJAVU_SANS_TTF); fontSize(14.f); fillColor(th->text); textAlign(ALIGN_LEFT | ALIGN_MIDDLE);
            textBox(194, y + 12, 870, tx, nullptr);
            std::snprintf(b, sizeof(b), "KNOB %c", 'A' + k);
            label(194, y + 36, b, 9.f, th->dim, ALIGN_LEFT | ALIGN_MIDDLE);
        }
        label(104, 662, "NOTE mode: click a step, drag up/down or play a key (step entry). LEN mode: click = longer, right-click = shorter.",
              10.f, th->dim, ALIGN_LEFT | ALIGN_MIDDLE);
        label(104, 686, "RATCH / COND / TIME: left click = next, right click = previous, double-click = reset. Right-click a step in TRIG mode = parameter locks.",
              10.f, th->dim, ALIGN_LEFT | ALIGN_MIDDLE);
    }

    // ------------------------------------------------------------------ interaction
    void setMachine(int t, int m)
    {
        m = clampi(m, 0, kMachines - 1);
        char b[32]; std::snprintf(b, sizeof(b), "M %d %d", t, m); sendCmd(b);
        for (int k = 0; k < 8; ++k) setParamGesture(t * kTrackParams + k, kMachineDefs[m].def[k]);
        repaint();
    }

    void writeLock(int pi, int v)
    {
        char b[64];
        Pattern& p = pat();
        if (!(p.flags[selTrack][lockStep] & sfTrig) && v != kNoLock) {
            std::string c; ensureTrig(c, lockStep); c.pop_back(); sendLines(c);
        }
        std::snprintf(b, sizeof(b), "l %d %d %d %d %d", editPattern(), selTrack, lockStep, pi, v);
        sendCmd(b);
    }

    float knobValue(int k)
    {
        const int pi = pageParam(page, k);
        if (pi < 0) return 0;
        if (lockStep >= 0) {
            const uint8_t l = pat().locks[selTrack][lockStep][pi];
            if (l != kNoLock) return l;
        }
        return tp(selTrack, pi);
    }

    void setKnob(int k, float v)
    {
        const int pi = pageParam(page, k);
        if (pi < 0) return;
        v = clampf(v, 0.f, 127.f);
        if (lockStep < 0 && recording()) {
            recVal = v; recDirty = true;
            recordLock(pi, (int)std::lround(v));
        } else if (lockStep >= 0) {
            const int iv = (int)std::lround(v);
            if (pat().locks[selTrack][lockStep][pi] != iv) writeLock(pi, iv);
        } else {
            setParam(selTrack * kTrackParams + pi, v);
        }
        repaint();
    }

    void setMaster(int k, float v, bool gesture = false)
    {
        const int g = masterParam(mpage, k);
        if (g < 0) return;
        v = clampf(v, 0.f, 127.f);
        if (gesture) setParamGesture(kGlobalBase + g, v); else setParam(kGlobalBase + g, v);
        repaint();
    }

    // ------------------------------------------------------------------ playing notes from the UI
    bool pcIsHeld(int n) const { for (int h : pcHeld) if (h == n) return true; return false; }

    void keyPress(int n, int vel)
    {
        n = clampi(n, 0, 127);
        lastNote = n;
        playNote(selTrack, n, vel, true);
        if (mode == mdNote && noteStep >= 0) {
            // step entry: write the note and move to the next step
            setStepNote(noteStep, n);
            noteStep = (noteStep + 1) % tlen(selTrack);
            if (noteStep / 16 != stepPg) stepPg = noteStep / 16;
        }
        repaint();
    }

    bool onKeyboard(const KeyboardEvent& ev) override
    {
        if (overlay == ovSave) {
            if (ev.press) {
                if (ev.key == kKeyEscape) { overlay = ovNone; repaint(); }
                else if (ev.key == kKeyEnter || ev.key == '\n') commitSave();
                else if (ev.key == kKeyBackspace || ev.key == kKeyDelete) {
                    if (!saveName.empty()) { saveName.pop_back(); saveConfirm = false; repaint(); }
                }
            }
            return true;
        }
        if (ev.press && ev.key == kKeyEscape) {
            if (overlay != ovNone) overlay = ovNone; else { lockStep = -1; noteStep = -1; }
            repaint(); return true;
        }
        if (pcKeys && overlay == ovNone) {
            static const char map[] = "awsedftgyhujkolp";
            const uint k = ev.key < 128 ? (uint)std::tolower((int)ev.key) : ev.key;
            if (ev.press && (k == 'z' || k == 'x')) { kbOct = clampi(kbOct + (k == 'x' ? 1 : -1), 0, 6); repaint(); return true; }
            for (int i = 0; map[i]; ++i) {
                if ((uint)map[i] != k) continue;
                const int n = kbLow() + 12 + i;
                if (ev.press) {
                    if (pcHeld[i] >= 0) return true; // key repeat
                    pcHeld[i] = n; keyPress(n, (ev.mod & kModifierShift) ? 127 : 100);
                } else if (pcHeld[i] >= 0) {
                    playNote(selTrack, pcHeld[i], 0, false); pcHeld[i] = -1; repaint();
                }
                return true;
            }
        }
        return false;
    }

    bool onMouse(const MouseEvent& ev) override
    {
        const float x = ev.pos.getX() / sx, y = ev.pos.getY() / sx;
        if (!ev.press) {
            if (keyNote >= 0) { playNote(keyTrack, keyNote, 0, false); keyNote = -1; }
            if (drag == dkTrackKnob && lockStep < 0 && !recording()) editParameter(selTrack * kTrackParams + pageParam(page, dragIndex), false);
            recDirty = false;
            if (drag == dkMasterKnob) editParameter(kGlobalBase + masterParam(mpage, dragIndex), false);
            if (drag == dkTempo) editParameter(kGlobalBase + gTEMPO, false);
            if (drag == dkSwing || drag == dkAccent) sendPatternSettings();
            if (drag == (DragKind)dkResize) saveView();
            drag = dkNone; repaint();
            return true;
        }
        const bool dbl = (ev.time - lastClickTime) < 350 && std::fabs(x - lastX) < 4 && std::fabs(y - lastY) < 4;
        lastClickTime = ev.time; lastX = x; lastY = y;
        const bool right = ev.button == kMouseButtonRight;
        const bool shift = (ev.mod & kModifierShift) != 0;

        if (overlay == ovPicker) {
            for (int m = 0; m < kMachines; ++m)
                if (pickerCell(m).in(x, y)) { setMachine(selTrack, m); break; }
            overlay = ovNone; repaint(); return true;
        }
        if (gripR().in(x, y) && overlay == ovNone) {
            drag = (DragKind)dkResize; resizeStartW = getWidth();
            grabX = ev.pos.getX(); grabY = ev.pos.getY();
            repaint(); return true;
        }
        if (overlay == ovPresets) { presetClick(x, y, dbl); return true; }
        if (overlay == ovHelp) { overlay = ovNone; repaint(); return true; }
        if (overlay == ovSave) {
            if (saveOk().in(x, y)) commitSave();
            else if (saveCancel().in(x, y) || !saveBox().in(x, y)) { overlay = ovNone; repaint(); }
            return true;
        }
        if (overlay == ovSkins) {
            static const int zooms[6] = {80, 100, 120, 140, 160, 200};
            for (int i = 0; i < kNumThemes; ++i) if (skinRow(i).in(x, y)) { setSkin(i, true); repaint(); return true; }
            for (int i = 0; i < 6; ++i) if (zoomBtn(i).in(x, y)) { setZoom(zooms[i] / 100.0); saveView(); return true; }
            if (!viewBox().in(x, y)) { overlay = ovNone; repaint(); }
            return true;
        }
        if (skinBtn().in(x, y)) {
            if (right) setSkin((skin + 1) % kNumThemes, true); else overlay = ovSkins;
            repaint(); return true;
        }
        if (helpBtn().in(x, y)) { overlay = ovHelp; repaint(); return true; }

        // keyboard
        {
            const int n = keyAt(x, y);
            if (n >= 0) {
                keyTrack = selTrack; keyNote = n; drag = dkKey;
                keyPress(n, shift ? 127 : 100);
                return true;
            }
        }
        if (octDown().in(x, y)) { kbOct = clampi(kbOct - 1, 0, 6); repaint(); return true; }
        if (octUp().in(x, y)) { kbOct = clampi(kbOct + 1, 0, 6); repaint(); return true; }
        if (octBox().in(x, y)) { kbOct = 2; repaint(); return true; }
        if (pcKeysBtn().in(x, y)) {
            pcKeys = !pcKeys;
            for (int& n : pcHeld) if (n >= 0) { playNote(selTrack, n, 0, false); n = -1; }
            repaint(); return true;
        }
        if (panicBtn().in(x, y)) {
            if (direct) direct->panic(); else setState("cmd", "panic");
            for (int& n : pcHeld) n = -1;
            repaint(); return true;
        }

        for (int t = 0; t < kTracks; ++t) {
            if (muteBtn(t).in(x, y)) {
                char b[32]; std::snprintf(b, sizeof(b), "U %d %d", t, bank->mute[t] ? 0 : 1); sendCmd(b);
                repaint(); return true;
            }
            if (trackBtn(t).in(x, y)) {
                selectTrack(t);
                if (right) { keyTrack = t; keyNote = lastNote; playNote(t, lastNote, 100, true); } // audition
                repaint(); return true;
            }
        }
        if (recBtn().in(x, y)) { setRec(!rec); repaint(); return true; }
        for (int i = 0; i < 4; ++i) if (rndBtn(i).in(x, y)) { randomize(i); return true; }
        if (undoBtn().in(x, y)) { undo(); return true; }
        if (presetBtn().in(x, y)) { openPresets(); return true; }
        if (saveBtn().in(x, y)) { openSave(); return true; }
        if (machPrev().in(x, y)) { setMachine(selTrack, machineOf(selTrack) - 1); return true; }
        if (machNext().in(x, y)) { setMachine(selTrack, machineOf(selTrack) + 1); return true; }
        if (machName().in(x, y)) { overlay = ovPicker; repaint(); return true; }
        if (lockStep >= 0 && lockClear().in(x, y)) {
            char b[48]; std::snprintf(b, sizeof(b), "cl %d %d %d", editPattern(), selTrack, lockStep); sendCmd(b);
            repaint(); return true;
        }
        for (int i = 0; i < kPages; ++i) if (pageTab(i).in(x, y)) { page = i; repaint(); return true; }
        for (int i = 0; i < 3; ++i) if (mTab(i).in(x, y)) { mpage = i; repaint(); return true; }

        for (int k = 0; k < 8; ++k) {
            if (!knob(k).in(x, y) || kitField(k) < 0) continue;
            if (dbl) { setKit(selTrack, kitField(k), kKitDefault[kitField(k)]); return true; }
            drag = dkKit; dragIndex = k; dragY = y; dragStart = bank->kit[selTrack][kitField(k)];
            repaint(); return true;
        }
        for (int k = 0; k < 8; ++k) {
            if (!knob(k).in(x, y) || pageParam(page, k) < 0) continue;
            const int pi = pageParam(page, k);
            if (dbl) {
                if (lockStep >= 0) writeLock(pi, kNoLock);
                else setParamGesture(selTrack * kTrackParams + pi, pi < 8 ? kMachineDefs[machineOf(selTrack)].def[pi] : kFxDefaults[pi - 8]);
                repaint(); return true;
            }
            drag = dkTrackKnob; dragIndex = k; dragY = y; dragStart = knobValue(k); recDirty = false;
            if (lockStep < 0 && !recording()) editParameter(selTrack * kTrackParams + pi, true);
            else if (lockStep >= 0 && pat().locks[selTrack][lockStep][pi] == kNoLock) setKnob(k, dragStart);
            repaint(); return true;
        }
        for (int k = 0; k < 8; ++k) {
            const int g = masterParam(mpage, k);
            if (!mKnob(k).in(x, y) || g < 0) continue;
            if (dbl) { setMaster(k, kMasterDefaults[g - gECHO_TIME], true); return true; }
            drag = dkMasterKnob; dragIndex = k; dragY = y; dragStart = gp(g);
            editParameter(kGlobalBase + g, true);
            repaint(); return true;
        }
        if (playBtn().in(x, y)) { setParamGesture(kGlobalBase + gPLAY, gp(gPLAY) > 0.5f ? 0.f : 1.f); repaint(); return true; }
        if (tempoBox().in(x, y)) {
            if (dbl) { setParamGesture(kGlobalBase + gTEMPO, 120.f); repaint(); return true; }
            drag = dkTempo; dragY = y; dragStart = gp(gTEMPO); editParameter(kGlobalBase + gTEMPO, true); return true;
        }
        for (int i = 0; i < kPatterns; ++i)
            if (patBtn(i).in(x, y)) { setParamGesture(kGlobalBase + gPATTERN, (float)i); lockStep = -1; noteStep = -1; stepPg = 0; repaint(); return true; }
        if (patCopy().in(x, y)) { patClip = editPattern(); repaint(); return true; }
        if (patPaste().in(x, y) && patClip >= 0) {
            char b[32]; std::snprintf(b, sizeof(b), "pc %d %d", editPattern(), patClip); sendCmd(b); repaint(); return true;
        }
        if (patClear().in(x, y)) { char b[32]; std::snprintf(b, sizeof(b), "cp %d", editPattern()); sendCmd(b); repaint(); return true; }

        for (int i = 0; i < kModes; ++i) if (modeBtn(i).in(x, y)) { mode = i; if (mode != mdNote) noteStep = -1; repaint(); return true; }
        if (fillBtn().in(x, y)) { fillOn = !fillOn; setState("cmd", fillOn ? "fill 1" : "fill 0"); repaint(); return true; }
        Pattern& p = pat();
        for (int i = 0; i < 4; ++i)
            if (stepPage(i).in(x, y)) {
                const int want = (i + 1) * 16;
                if (dbl) setTrackLen(selTrack, want, false, shift);
                else if (want > tlen(selTrack)) setTrackLen(selTrack, want, true, shift);
                stepPg = i; repaint(); return true;
            }
        if (lenMinus().in(x, y) || lenPlus().in(x, y)) {
            setTrackLen(selTrack, tlen(selTrack) + (lenPlus().in(x, y) ? 1 : -1), false, shift);
            repaint(); return true;
        }
        if (lenBox().in(x, y)) {
            setTrackLen(selTrack, ((tlen(selTrack) - 1) / 16 + 1) % 4 * 16 + 16, true, shift);
            repaint(); return true;
        }
        if (patLenBox().in(x, y)) {
            p.length = (uint8_t)(((p.length - 1) / 16 + 1) % 4 * 16 + 16);
            sendPatternSettings(); repaint(); return true;
        }
        if (scaleBox().in(x, y)) { p.scale = (uint8_t)((p.scale + (right ? 6 : 1)) % 7); sendPatternSettings(); repaint(); return true; }
        if (swingBox().in(x, y)) {
            if (dbl) { p.swing = 50; sendPatternSettings(); repaint(); return true; }
            drag = dkSwing; dragY = y; dragStart = p.swing; return true;
        }
        if (accBox().in(x, y)) {
            if (dbl) { p.accent = 64; sendPatternSettings(); repaint(); return true; }
            drag = dkAccent; dragY = y; dragStart = p.accent; return true;
        }
        if (trkCopy().in(x, y)) { trackClip = trackToHex(p, selTrack); repaint(); return true; }
        if (trkPaste().in(x, y) && !trackClip.empty()) {
            sendCmd("tr " + std::to_string(editPattern()) + " " + std::to_string(selTrack) + " " + trackClip); repaint(); return true;
        }
        if (trkClear().in(x, y)) { char b[32]; std::snprintf(b, sizeof(b), "ct %d %d", editPattern(), selTrack); sendCmd(b); repaint(); return true; }
        if (lockClrAll().in(x, y)) {
            std::string c;
            for (int s = 0; s < kSteps; ++s)
                if (p.hasLocks(selTrack, s)) { char b[48]; std::snprintf(b, sizeof(b), "cl %d %d %d\n", editPattern(), selTrack, s); c += b; }
            if (!c.empty()) { c.pop_back(); sendLines(c); }
            repaint(); return true;
        }

        for (int i = 0; i < 16; ++i) {
            if (!stepBtn(i).in(x, y)) continue;
            const int s = stepPg * 16 + i;
            if (s >= tlen(selTrack)) return true;
            if (mode >= mdRatch) { stepExtra(s, right, dbl, shift); repaint(); return true; }
            if (mode == mdNote && !right) {
                if (dbl) {
                    char b[48]; std::snprintf(b, sizeof(b), "f %d %d %d 0", editPattern(), selTrack, s); sendCmd(b);
                    repaint(); return true;
                }
                noteStep = s;
                if (!(p.flags[selTrack][s] & sfTrig)) setStepNote(s, lastNote);
                const int n = p.note[selTrack][s];
                lastNote = n;
                keyTrack = selTrack; keyNote = n; playNote(selTrack, n, 100, true); // preview
                drag = dkStepNote; dragIndex = s; dragY = y; dragStart = (float)n;
                repaint(); return true;
            }
            if (mode == mdLen) {
                if (right && !(p.flags[selTrack][s] & sfTrig)) return true;
                const int g = p.gate[selTrack][s];
                if (!(p.flags[selTrack][s] & sfTrig)) setStepGate(s, g);
                else setStepGate(s, dbl ? kDefaultGate : clampi(g + (right ? -1 : 1), 0, kGateCount - 1));
                drag = right ? dkNone : dkStepLen; dragIndex = s; dragY = y; dragStart = (float)p.gate[selTrack][s];
                repaint(); return true;
            }
            if (right) { lockStep = (lockStep == s) ? -1 : s; repaint(); return true; }
            uint8_t f = p.flags[selTrack][s];
            std::string c;
            if (mode == mdTrig) f = (f & sfTrig) ? 0 : sfTrig;
            else if (mode == mdAccent) f = (f & sfAccent) ? (uint8_t)(f & ~sfAccent) : (uint8_t)(f | sfAccent | sfTrig);
            else f = (f & sfSlide) ? (uint8_t)(f & ~sfSlide) : (uint8_t)(f | sfSlide | sfTrig);
            char b[96];
            if ((f & sfTrig) && !(p.flags[selTrack][s] & sfTrig)) {
                // new trig: it takes the last note played or entered
                std::snprintf(b, sizeof(b), "n %d %d %d %d\n", editPattern(), selTrack, s, lastNote); c += b;
            }
            std::snprintf(b, sizeof(b), "f %d %d %d %d", editPattern(), selTrack, s, f);
            c += b;
            sendLines(c);
            repaint(); return true;
        }
        return true;
    }

    bool onMotion(const MotionEvent& ev) override
    {
        if (drag == dkNone) return false;
        if (drag == (DragKind)dkResize) {
            const double dx = ev.pos.getX() - grabX, dy = (ev.pos.getY() - grabY) * (double)W / H;
            const double nw = resizeStartW + (dx + dy) * 0.5;
            const double z = std::max(0.6, std::min(3.0, nw / (W * baseScale())));
            const uint w = (uint)std::lround(W * baseScale() * z), h = (uint)std::lround(H * baseScale() * z);
            if (std::abs((int)w - (int)getWidth()) >= 3) setSize(w, h);
            return true;
        }
        const float x = ev.pos.getX() / sx, y = ev.pos.getY() / sx;
        const float sens = (ev.mod & kModifierShift) ? 0.12f : 0.6f;
        const float v = dragStart + (dragY - y) * sens;
        switch (drag) {
        case dkKey: { // glide across the keys
            const int n = keyAt(x, y);
            if (n >= 0 && n != keyNote) {
                if (keyNote >= 0) playNote(keyTrack, keyNote, 0, false);
                keyNote = n; lastNote = n; playNote(keyTrack, n, 100, true); repaint();
            }
            break;
        }
        case dkStepNote: {
            const int n = clampi((int)std::lround(dragStart + (dragY - y) / 6.f), 0, 127);
            if (n != pat().note[selTrack][dragIndex]) {
                setStepNote(dragIndex, n); lastNote = n;
                if (keyNote >= 0) playNote(keyTrack, keyNote, 0, false);
                keyTrack = selTrack; keyNote = n; playNote(selTrack, n, 100, true);
                repaint();
            }
            break;
        }
        case dkStepLen: {
            const int g = clampi((int)std::lround(dragStart + (dragY - y) / 10.f), 0, kGateCount - 1);
            if (g != pat().gate[selTrack][dragIndex]) { setStepGate(dragIndex, g); repaint(); }
            break;
        }
        case dkTrackKnob: setKnob(dragIndex, v); break;
        case dkKit: {
            const int f = kitField(dragIndex);
            if (f >= 0) {
                const int nv = clampi((int)std::lround(dragStart + (dragY - y) * 0.05f), 0, kKitMax[f]);
                if (nv != bank->kit[selTrack][f]) setKit(selTrack, f, nv);
            }
            break;
        }
        case dkMasterKnob: setMaster(dragIndex, v); break;
        case dkTempo: {
            const float tv = clampf(std::round((dragStart + (dragY - y) * ((ev.mod & kModifierShift) ? 0.05f : 0.5f)) * 10.f) / 10.f, 40.f, 240.f);
            setParam(kGlobalBase + gTEMPO, tv); repaint(); break;
        }
        case dkSwing: pat().swing = (uint8_t)clampi((int)std::lround(dragStart + (dragY - y) * 0.2f), 50, 80); repaint(); break;
        case dkAccent: pat().accent = (uint8_t)clampi((int)std::lround(v), 0, 127); repaint(); break;
        default: break;
        }
        return true;
    }

    bool onScroll(const ScrollEvent& ev) override
    {
        const float x = ev.pos.getX() / sx, y = ev.pos.getY() / sx;
        const float d = ev.delta.getY() > 0 ? 1.f : (ev.delta.getY() < 0 ? -1.f : 0.f);
        if (overlay == ovPresets && d != 0.f) {
            const int rows = (presetCount() + 1) / 2;
            presetScroll = clampi(presetScroll - (int)d, 0, std::max(0, rows - 11));
            repaint(); return true;
        }
        if (d == 0.f || overlay != ovNone) return false;
        for (int k = 0; k < 8; ++k) {
            if (knob(k).in(x, y) && kitField(k) >= 0) {
                const int f = kitField(k);
                setKit(selTrack, f, clampi(bank->kit[selTrack][f] + (int)d, 0, kKitMax[f])); return true;
            }
            if (knob(k).in(x, y) && pageParam(page, k) >= 0) {
                const uint32_t idx = selTrack * kTrackParams + pageParam(page, k);
                const bool gesture = lockStep < 0 && !recording();
                if (gesture) editParameter(idx, true);
                setKnob(k, knobValue(k) + d);
                if (gesture) editParameter(idx, false);
                recDirty = false;
                return true;
            }
            if (mKnob(k).in(x, y) && masterParam(mpage, k) >= 0) {
                setMaster(k, gp(masterParam(mpage, k)) + d, true); return true;
            }
        }
        for (int i = 0; i < 16; ++i) {
            if (!stepBtn(i).in(x, y)) continue;
            const int s = stepPg * 16 + i;
            if (s >= tlen(selTrack) || !(pat().flags[selTrack][s] & sfTrig)) return true;
            if (mode == mdLen) setStepGate(s, pat().gate[selTrack][s] + (int)d);
            else { setStepNote(s, pat().note[selTrack][s] + (int)d); lastNote = pat().note[selTrack][s]; }
            repaint(); return true;
        }
        if (keyAt(x, y) >= 0 || octBox().in(x, y)) { kbOct = clampi(kbOct + (int)d, 0, 6); repaint(); return true; }
        if (tempoBox().in(x, y)) { setParamGesture(kGlobalBase + gTEMPO, clampf(gp(gTEMPO) + d, 40.f, 240.f)); repaint(); return true; }
        if (swingBox().in(x, y)) { pat().swing = (uint8_t)clampi(pat().swing + (int)d, 50, 80); sendPatternSettings(); repaint(); return true; }
        if (lenBox().in(x, y)) { setTrackLen(selTrack, tlen(selTrack) + (int)d, false, false); repaint(); return true; }
        if (patLenBox().in(x, y)) { pat().length = (uint8_t)clampi(pat().length + (int)d, 1, kSteps); sendPatternSettings(); repaint(); return true; }
        if (accBox().in(x, y)) { pat().accent = (uint8_t)clampi(pat().accent + (int)d * 4, 0, 127); sendPatternSettings(); repaint(); return true; }
        return false;
    }

    void uiIdle() override
    {
        viewIdle();
        pollRecorded();
        // keep the playing-note display current
        if (direct) {
            uint32_t sig = direct->heldMask();
            for (int t = 0; t < kTracks; ++t) sig = sig * 131u + (uint32_t)(direct->currentNote(t) + 1);
            if (sig != lastLiveSig) { lastLiveSig = sig; repaint(); }
        }
    }

private:
    float values[kTotalParams];
    std::unique_ptr<Bank> bank;
    MonoDirect* direct = nullptr;
    float sx = 1.f;
    int selTrack = 0, page = pgSynth, mpage = mpEcho, mode = mdTrig, stepPg = 0, lockStep = -1, noteStep = -1;
    Overlay overlay = ovNone;
    DragKind drag = dkNone; int dragIndex = 0; float dragY = 0, dragStart = 0;
    uint lastClickTime = 0; float lastX = -100, lastY = -100;
    int patClip = -1;
    std::string trackClip;
    bool rec = false, recDirty = false; float recVal = 0;
    bool fillOn = false;
    // keyboard
    int kbOct = 2;                 // lowest key = C(kbOct) in Ableton naming: 2 -> C2 (48)
    int keyNote = -1, keyTrack = 0, lastNote = 60;
    bool pcKeys = false; int pcHeld[16] = {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1};
    uint32_t lastLiveSig = 0;
    int skin = kDefaultSkin;
    const Theme* th = &kThemes[kDefaultSkin];
    FontId fonts[kFontCount] = {};
    NanoImage dotImage, haloImage;
    int presetSel = -1, presetScroll = 0;
    bool presetTabUser = false, deleteArmed = false, saveConfirm = false;
    std::vector<std::string> userPresets;
    std::string presetMsg, saveName;
    bool viewDirty = false; int viewDirtyTicks = 0;
    double resizeStartW = 0, grabX = 0, grabY = 0;
    std::string presetName = "Init";
    uint32_t rndCounter = 0x51ED;
    bool undoValid = false; std::vector<float> undoValues; std::string undoBank; const char* undoWhat = "";

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MonoUI)
};

UI* createUI() { return new MonoUI(); }

END_NAMESPACE_DISTRHO
