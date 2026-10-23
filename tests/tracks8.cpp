// KMSP-6 0.10: 8 tracks. Host parameter indices of 0.9 projects must not move; tracks 7-8 play.
//   g++ -O2 -std=c++17 -pthread tests/tracks8.cpp -o /tmp/t && /tmp/t
#include "../src/Engine.hpp"
#include "../src/Presets.hpp"
#include <cstdio>
#include <memory>
using namespace kn;

int main()
{
    int fails = 0;
    auto check = [&](bool ok, const char* what) { std::printf("%s %s\n", ok ? "ok  " : "FAIL", what); if (!ok) ++fails; };
    // indices of KMSP-6 0.9 (6 tracks)
    check(kGlobalBase == 336 && gCHO_SEND == 45 && gCHA_SEND == 51 && gMF_ON == 57 && gMORPH == 58 && gECHO_FREEZE == 62,
          "0.9 global parameter indices unchanged");
    bool same = true;
    for (int t = 0; t < 6; ++t) for (int p = 0; p < kTrackParams; ++p) same &= tpIndex(t, p) == t * kTrackParams + p;
    check(same, "tracks 1-6 keep their parameter indices");
    // every index maps back, no overlap with real globals
    bool round = true; std::vector<int> seen(kTotalParams, 0);
    for (int t = 0; t < kTracks; ++t) for (int p = 0; p < kTrackParams; ++p) {
        const int i = tpIndex(t, p); int t2, p2;
        round &= i >= 0 && i < kGlobalBase + gOUT_STEP && tpOf(i, t2, p2) && t2 == t && p2 == p && !seen[i]++;
    }
    for (int g = 0; g < gEXT_TP; ++g) { int t2, p2; round &= !tpOf(kGlobalBase + g, t2, p2); }
    check(round, "tpIndex / tpOf round trip, tracks 7-8 after the globals");
    // LFO TARGET values saved with 6 tracks keep their track
    bool lfo = true;
    for (int t = 0; t <= 6; ++t) lfo &= lfoTrackIndex((t * 128 + 6) / 7) == t;
    for (int t = 0; t <= 8; ++t) lfo &= lfoTrackIndex(lfoTrackValue(t)) == t;
    check(lfo, "LFO TARGET: old values and tracks 7-8");
    // track 8 plays
    {
        auto b = std::make_unique<Bank>(); std::vector<float> v(kTotalParams); buildPreset(0, v.data(), *b);
        auto e = std::make_unique<Engine>(); e->init(48000);
        for (int t = 0; t < kTracks; t++) for (int p = 0; p < kTrackParams; p++) e->trackParam[t][p] = v[tpIndex(t, p)];
        for (int g = 0; g < gOUT_STEP; g++) e->global[g] = v[kGlobalBase + g];
        for (int t = 0; t < kTracks; ++t) b->machine[t] = mOFF;
        b->machine[7] = mSAW;
        for (int s = 0; s < 16; s += 4) b->patterns[0].flags[7][s] = sfTrig;
        e->global[gPLAY] = 1;
        e->bank = *b;
        std::vector<float> L(256), R(256); double sum = 0;
        for (int i = 0; i < 48000 / 256; ++i) { e->process(L.data(), R.data(), 256, Transport(), nullptr, 0); for (float x : L) sum += x * x; }
        check(sum > 1.0 && std::isfinite(sum), "track 8 plays");
    }
    // a 0.9 bank (6 tracks) loads: tracks 7-8 get their defaults
    {
        Bank b6; b6.initDefault();
        std::string txt = serializeBank(b6), cut;
        for (size_t a = 0; a < txt.size();) {   // drop the lines of tracks 6 and 7 (as written by 0.9)
            size_t e = txt.find('\n', a); if (e == std::string::npos) e = txt.size();
            const std::string ln = txt.substr(a, e - a);
            int t = -1; char c;
            if (!(std::sscanf(ln.c_str(), "%c %d", &c, &t) == 2 && (c == 'M' || c == 'U' || c == 'K') && t >= 6)) cut += ln + "\n";
            a = e + 1;
        }
        Bank b; const bool ok = deserializeBank(b, cut.c_str());
        check(ok && b.machine[6] == mVOX && b.machine[7] == mCHORD, "6-track bank loads, tracks 7-8 default");
    }
    std::printf(fails ? "%d failures\n" : "all passed\n", fails);
    return fails ? 1 : 0;
}
