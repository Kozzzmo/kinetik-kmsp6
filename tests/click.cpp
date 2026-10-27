// KMSP-6 metronome: clicks on beats only while recording (and when enabled).
//   g++ -O2 -std=c++17 tests/click.cpp -o /tmp/t && /tmp/t
#include "../src/Engine.hpp"
#include "../src/Presets.hpp"
#include <cstdio>
#include <cmath>
#include <memory>
using namespace kn;

int main()
{
    int fails = 0;
    auto check = [&](bool ok, const char* what) { std::printf("%s %s\n", ok ? "ok  " : "FAIL", what); if (!ok) ++fails; };
    std::vector<float> L(64), R(64);
    auto run = [&](bool rec, bool enabled, double& bar, double& beat, double& off) {
        std::vector<float> v(kTotalParams); std::unique_ptr<Bank> b(new Bank()); buildPreset(0, v.data(), *b);   // Init: empty
        std::unique_ptr<Engine> e(new Engine()); e->init(48000);
        for (int t = 0; t < kTracks; t++) for (int p = 0; p < kTrackParams; p++) e->trackParam[t][p] = v[tpIndex(t, p)];
        for (int g = 0; g < gOUT_STEP; g++) e->global[g] = v[kGlobalBase + g];
        e->bank = *b; e->global[gPLAY] = 1.f; e->global[gTEMPO] = 120.f;
        e->recording = rec; e->click.enabled = enabled;
        bar = beat = off = 0;
        for (int pos = 0; pos < 48000 * 2; pos += 64) {
            e->process(L.data(), R.data(), 64, Transport(), nullptr, 0);
            for (int i = 0; i < 64; ++i) {
                const int f = pos + i, in = f % 24000; const double a = std::fabs(L[i]);
                if (in < 2400) { if (f < 24000) bar = std::max(bar, a); else beat = std::max(beat, a); }
                else if (in > 9000) off = std::max(off, a);
            }
        }
    };
    double a, b, c;
    run(true, true, a, b, c);   check(a > 0.2 && b > 0.1 && a > b && c < 1e-3, "click on each beat while recording, accented on beat 1");
    run(true, false, a, b, c);  check(a < 1e-6 && b < 1e-6, "CLICK OFF: silent");
    run(false, true, a, b, c);  check(a < 1e-6 && b < 1e-6, "not recording: silent");
    std::printf(fails ? "%d failures\n" : "all passed\n", fails);
    return fails ? 1 : 0;
}
