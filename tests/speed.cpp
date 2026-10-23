// Per-track SPEED: a track with every step on plays mult x 16 trigs per pattern loop, on its own grid; MIDI-free,
// shared by kinetik-kdsp16 and kinetik-kmsp6.   g++ -O2 -std=c++17 tests/speed.cpp -o /tmp/t && /tmp/t
#include "../src/Engine.hpp"
#include "../src/Presets.hpp"
#include <cstdio>
using namespace kn;

int main()
{
    int fails = 0;
    for (int sp = 0; sp < kTrackSpeeds; ++sp) {
        std::vector<float> v(kTotalParams); Bank* b = new Bank(); buildPreset(0, v.data(), *b);   // Init
        Engine* e = new Engine(); e->init(48000);
        for (int t = 0; t < kTracks; t++) for (int p = 0; p < kTrackParams; p++) e->trackParam[t][p] = v[tpIndex(t, p)];
        for (int g = 0; g < gOUT_STEP; g++) e->global[g] = v[kGlobalBase + g];
        Pattern& P = b->patterns[0];
        for (int s = 0; s < 16; ++s) { P.flags[0][s] = sfTrig; P.flags[1][s] = sfTrig; }
        b->kit[0][kfSpeed] = (uint8_t)sp;          // track 1 at this speed, track 2 at 1x
        e->bank = *b;
        e->global[gPLAY] = 1.f; e->global[gTEMPO] = 120.f;
        // 8 bars at 120 BPM = 16 s; count trigs during exactly 8 pattern loops (minus a hair)
        const int frames = (int)(48000 * 16.0) - 64;
        std::vector<float> L(64), R(64);
        int64_t lastFine = -1; bool fineOk = true;
        for (int i = 0; i + 64 <= frames; i += 64) {
            e->process(L.data(), R.data(), 64, Transport(), nullptr, 0);
            if (e->outStep < lastFine && lastFine < 32768 * kFineSteps - 1000) fineOk = false;
            lastFine = e->outStep;
        }
        const double want = 8 * 16 * kTrackSpeedMult[sp];
        const bool ok = std::fabs((double)e->trigCount[0] - want) <= 1.0 && e->trigCount[1] == 128 && fineOk;
        std::printf("%s speed %-5s track trigs %u (want %.0f), 1x track %u\n", ok ? "ok  " : "FAIL", kTrackSpeedNames[sp], e->trigCount[0], want, e->trigCount[1]);
        if (!ok) ++fails;
        delete e; delete b;
    }
    std::printf(fails ? "%d failures\n" : "all passed\n", fails);
    return fails ? 1 : 0;
}
