// Echo models and extras (shared by kinetik-kdsp16 and kinetik-kmsp6).
//   g++ -O2 -std=c++17 tests/echo.cpp -o /tmp/t && /tmp/t
#include "../src/Engine.hpp"
#include "../src/Presets.hpp"
#include <cmath>
#include <cstdio>
#include <memory>
using namespace kn;

struct Rig {
    Bank b; Engine e; std::vector<float> v;
    Rig(int model, int steps)
    {
        v.assign(kTotalParams, 0.f); buildPreset(0, v.data(), b);
        e.init(48000);
        for (int t = 0; t < kTracks; t++) for (int p = 0; p < kTrackParams; p++) e.trackParam[t][p] = v[tpIndex(t, p)];
        for (int g = 0; g < gOUT_STEP; g++) e.global[g] = v[kGlobalBase + g];
        for (int t = 0; t < kTracks; ++t) b.machine[t] = mOFF;
        b.machine[0] = mNOISE;
        for (int p = 0; p < 8; ++p) e.trackParam[0][p] = kMachineDefs[mNOISE].def[p];
        e.trackParam[0][tpDEL] = 127; e.trackParam[0][tpREV] = 0;
        for (int s = 0; s < steps; ++s) b.patterns[0].flags[0][s * 4] = sfTrig;
        e.global[gPLAY] = 1.f; e.global[gREV_LEV] = 0; e.global[gECHO_LEV] = 100; e.global[gECHO_FB] = 70;
        e.global[gECHO_MODEL] = (float)echoModelValue(model); e.global[gECHO_MOD] = 64;
        e.bank = b;
    }
    // energy of the next `sec` seconds (and NaN check)
    double run(double sec, bool& bad)
    {
        std::vector<float> L(256), R(256); double s = 0;
        for (int i = 0; i < (int)(48000 * sec / 256); ++i) {
            e.process(L.data(), R.data(), 256, Transport(), nullptr, 0);
            for (int k = 0; k < 256; ++k) { if (!std::isfinite(L[k]) || !std::isfinite(R[k]) || std::fabs(L[k]) > 8.f) bad = true; s += L[k] * L[k] + R[k] * R[k]; }
        }
        return s;
    }
};

int main()
{
    int fails = 0;
    // old saved MODEL values keep their model
    const int oldV[5] = {0, 26, 52, 77, 103}, oldM[5] = {emDigital, emTape, emBbd, emPingPong, emLofi};
    for (int i = 0; i < 5; ++i) if (echoModelIndex(oldV[i]) != oldM[i]) { std::printf("FAIL old value %d -> model %d\n", oldV[i], echoModelIndex(oldV[i])); ++fails; }
    for (int m = 0; m < kEchoModels; ++m) if (echoModelIndex(echoModelValue(m)) != m) { std::printf("FAIL model %d round trip\n", m); ++fails; }
    // every model: a single hit leaves an audible, finite, decaying tail
    for (int m = 0; m < kEchoModels; ++m) {
        auto rp = std::make_unique<Rig>(m, 1); Rig& r = *rp; bool bad = false;
        r.e.bank.patterns[0].length = 64;
        r.run(0.3, bad);
        r.e.global[gPLAY] = 0.f;
        const double t1 = r.run(1.0, bad), t2 = r.run(6.0, bad), t3 = r.run(1.0, bad);
        const bool ok = !bad && t1 > 1e-4 && t3 < t1;
        std::printf("%s %-9s tail %.3g -> %.3g%s\n", ok ? "ok  " : "FAIL", kEchoModelNames[m], t1, t3, bad ? " (NaN / overload)" : "");
        if (!ok) ++fails;
        (void)t2;
    }
    // FREEZE: the loop keeps sounding with no input
    {
        auto rp = std::make_unique<Rig>(emDigital, 1); Rig& r = *rp; bool bad = false;
        r.e.global[gECHO_FB] = 30;
        r.run(0.4, bad);
        r.e.global[gPLAY] = 0.f; r.e.global[gECHO_FREEZE] = 1.f;
        const double a = r.run(1.0, bad), c = r.run(8.0, bad) / 8.0;
        const bool ok = !bad && a > 1e-4 && c > a * 0.3;
        std::printf("%s FREEZE holds the loop (%.3g -> %.3g per second)\n", ok ? "ok  " : "FAIL", a, c);
        if (!ok) ++fails;
    }
    // DUCK lowers the echo while the source plays
    {
        double e[2];
        for (int d = 0; d < 2; ++d) { auto rp = std::make_unique<Rig>(emDigital, 16); Rig& r = *rp; bool bad = false; r.e.global[gECHO_DUCK] = d ? 127.f : 0.f; e[d] = r.run(4.0, bad); if (bad) ++fails; }
        const bool ok = e[1] < e[0] * 0.9;
        std::printf("%s DUCK lowers the level while playing (%.3g -> %.3g)\n", ok ? "ok  " : "FAIL", e[0], e[1]);
        if (!ok) ++fails;
    }
    std::printf(fails ? "%d failures\n" : "all passed\n", fails);
    return fails ? 1 : 0;
}
