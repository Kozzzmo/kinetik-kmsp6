// Scene morph, LFO -> MORPH and CHAOS sends (shared by kinetik-kdsp16 and kinetik-kmsp6).
//   g++ -O2 -std=c++17 tests/morph.cpp -o /tmp/t && /tmp/t
#include "../src/Engine.hpp"
#include "../src/Presets.hpp"
#include <cstdio>
using namespace kn;

// current value of scene slot i (track parameter or global)
static float sceneValueOf(Engine* e, int i)
{
    int t, p;
    return tpOf(i, t, p) ? e->trackParam[t][p] : e->global[i - kGlobalBase];
}

static Engine* make(Bank*& b, std::vector<float>& v)
{
    v.assign(kTotalParams, 0.f); b = new Bank(); buildPreset(0, v.data(), *b);   // Init
    Engine* e = new Engine(); e->init(48000);
    for (int t = 0; t < kTracks; t++) for (int p = 0; p < kTrackParams; p++) e->trackParam[t][p] = v[tpIndex(t, p)];
    for (int g = 0; g < gOUT_STEP; g++) e->global[g] = v[kGlobalBase + g];
    for (int t = 0; t < kTracks; ++t) b->machine[t] = mOFF;
    b->machine[0] = mNOISE;
    for (int p = 0; p < 8; ++p) e->trackParam[0][p] = kMachineDefs[mNOISE].def[p];
    for (int s = 0; s < 16; ++s) b->patterns[0].flags[0][s] = sfTrig;
    e->global[gPLAY] = 1.f; e->global[gREV_LEV] = 0; e->global[gECHO_LEV] = 0;
    return e;
}
// brightness = energy of the first difference / energy (high = bright)
static double bright(Engine* e, int seconds)
{
    std::vector<float> L(256), R(256); double s = 0, d = 0; float prev = 0;
    for (int i = 0; i < 48000 * seconds / 256; ++i) {
        e->process(L.data(), R.data(), 256, Transport(), nullptr, 0);
        for (float x : L) { s += x * x; d += (x - prev) * (x - prev); prev = x; }
    }
    return s > 1e-12 ? d / s : 0.0;
}

int main()
{
    int fails = 0;
    // 1. scenes A (dark) / B (bright): brightness rises with the fader; the knob value is not changed by the morph
    double br[3];
    for (int k = 0; k < 3; ++k) {
        Bank* b; std::vector<float> v; Engine* e = make(b, v);
        e->trackParam[0][tpCUTOFF] = 64;
        auto snap = [&](int a, float cut) {
            b->scenes.set[a] = 1;
            for (int t = 0; t < kTracks; ++t) b->scenes.mach[a][t] = b->machine[t];
            for (int i = 0; i < kSceneValues; ++i)
                b->scenes.val[a][i] = (uint8_t)std::lround(sceneValueOf(e, i));
            b->scenes.val[a][tpCUTOFF] = (uint8_t)cut;
        };
        snap(0, 25); snap(1, 120);
        e->bank = *b; e->global[gMORPH] = k * 63.5f;
        br[k] = bright(e, 2);
        if (e->trackParam[0][tpCUTOFF] != 64.f) { std::printf("FAIL the morph changed the knob value\n"); ++fails; }
        delete e; delete b;
    }
    const bool rise = br[0] < br[1] && br[1] < br[2];
    std::printf("%s scene morph brightness %.3f -> %.3f -> %.3f\n", rise ? "ok  " : "FAIL", br[0], br[1], br[2]);
    if (!rise) ++fails;

    // 2. an LFO sent to MORPH moves the sound with the fader at 0
    {
        Bank* b; std::vector<float> v; Engine* e = make(b, v);
        const int lb = kLfoBase[0];   // the LFO is set before the scene is taken (the scene stores it too)
        e->trackParam[0][lb + 1] = 127; e->trackParam[0][lb + 4] = (float)lfoDestValue(kDestMorph); e->trackParam[0][lb] = 90;
        b->scenes.set[1] = 1;
        for (int t = 0; t < kTracks; ++t) b->scenes.mach[1][t] = b->machine[t];
        for (int i = 0; i < kSceneValues; ++i)
            b->scenes.val[1][i] = (uint8_t)std::lround(sceneValueOf(e, i));
        b->scenes.val[1][tpCUTOFF] = 127;
        e->trackParam[0][tpCUTOFF] = 10;
        e->bank = *b;
        std::vector<float> L(256), R(256); float lo = 1e9f, hi = -1e9f;
        for (int i = 0; i < 48000 * 3 / 256; ++i) { e->process(L.data(), R.data(), 256, Transport(), nullptr, 0); lo = std::min(lo, e->morphAmount()); hi = std::max(hi, e->morphAmount()); }
        const bool ok = hi - lo > 0.5f;
        std::printf("%s LFO -> MORPH swings the morph %.2f .. %.2f\n", ok ? "ok  " : "FAIL", lo, hi);
        if (!ok) ++fails;
        delete e; delete b;
    }

    // 3. CHAOS only repeats the tracks sent to it: track 1 (send 0) plays unchanged under a 100 % chaos
    {
        double diff = 0, ref = 0;
        std::vector<float> out[2];
        for (int run = 0; run < 2; ++run) {
            Bank* b; std::vector<float> v; Engine* e = make(b, v);
            e->global[gCHA_SEND + 0] = 0;
            e->global[gGL_CHANCE] = run ? 127.f : 0.f;
            e->bank = *b;
            std::vector<float> L(256), R(256);
            for (int i = 0; i < 48000 * 4 / 256; ++i) { e->process(L.data(), R.data(), 256, Transport(), nullptr, 0); out[run].insert(out[run].end(), L.begin(), L.end()); }
            delete e; delete b;
        }
        for (size_t i = 0; i < out[0].size(); ++i) { diff += (out[1][i] - out[0][i]) * (out[1][i] - out[0][i]); ref += out[0][i] * out[0][i]; }
        const bool ok = diff < ref * 1e-6;
        std::printf("%s CHAOS send 0 leaves the track untouched (diff %.2e of %.2e)\n", ok ? "ok  " : "FAIL", diff, ref);
        if (!ok) ++fails;
    }
    // 4. a scene saved before parameters were added (shorter line) still loads; the new values take their default
    {
        Scenes sc; sc.clear();
        sc.set[0] = 1;
        for (int i = 0; i < kSceneValues; ++i) sc.val[0][i] = (uint8_t)(i % 100);
        std::string line = sceneLine(sc, 0);
        line.resize(line.size() - 2 * 4);   // 4 values fewer
        Scenes back; back.clear();
        const bool ok = sceneFromLine(back, line.c_str() + 2, 255) && back.set[0]
                        && back.val[0][kSceneValues - 5] == sc.val[0][kSceneValues - 5]
                        && back.val[0][kSceneValues - 1] == (uint8_t)std::lround(std::min(127.f, globalDefault(kSceneValues - 1 - kGlobalBase)));
        std::printf("%s older (shorter) scene line loads, new values = defaults\n", ok ? "ok  " : "FAIL");
        if (!ok) ++fails;
    }
    std::printf(fails ? "%d failures\n" : "all passed\n", fails);
    return fails ? 1 : 0;
}
