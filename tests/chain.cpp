// Pattern CHAIN: A02 x2, A03 x1, looped; the chain line round-trips through the bank text; CHAIN off = the chosen
// pattern. Shared by kinetik-kdsp16 and kinetik-kmsp6.   g++ -O2 -std=c++17 tests/chain.cpp -o /tmp/t && /tmp/t
#include "../src/Engine.hpp"
#include "../src/Presets.hpp"
#include <cstdio>
#include <vector>
using namespace kn;

int main()
{
    int fails = 0;
    auto check = [&](bool ok, const char* what) { std::printf("%s %s\n", ok ? "ok  " : "FAIL", what); if (!ok) ++fails; };
    std::vector<float> v(kTotalParams); Bank* b = new Bank(); buildPreset(0, v.data(), *b);   // Init
    applyCommand(*b, "C 1 2 1:2 2:1");
    Bank* rb = new Bank(); rb->initDefault();
    check(deserializeBank(*rb, serializeBank(*b).c_str()) && rb->chain.on && rb->chain.n == 2 && rb->chain.rep[0] == 2 && rb->chain.pat[1] == 2, "chain line round trip");
    Engine* e = new Engine(); e->init(48000);
    for (int t = 0; t < kTracks; t++) for (int p = 0; p < kTrackParams; p++) e->trackParam[t][p] = v[tpIndex(t, p)];
    for (int g = 0; g < gOUT_STEP; g++) e->global[g] = v[kGlobalBase + g];
    e->bank = *b;
    e->global[gPLAY] = 1.f; e->global[gTEMPO] = 120.f;
    std::vector<float> L(64), R(64);
    std::vector<int> seq; std::vector<int> at;
    const int frames = 48000 * 2 * 7;          // 7 bars of 2 s
    for (int i = 0; i + 64 <= frames; i += 64) {
        e->process(L.data(), R.data(), 64, Transport(), nullptr, 0);
        const int p = e->currentPattern();
        if (seq.empty() || seq.back() != p) { seq.push_back(p); at.push_back(i); }
    }
    for (size_t i = 0; i < seq.size(); ++i) std::printf("  A%02d from %.2f s\n", seq[i] + 1, at[i] / 48000.0);
    check(seq.size() >= 4 && seq[0] == 1 && seq[1] == 2 && seq[2] == 1 && seq[3] == 2, "A02 A03 A02 A03 ...");
    check(at.size() >= 3 && std::abs(at[1] - 4 * 48000) < 200 && std::abs(at[2] - 6 * 48000) < 200, "A02 plays two bars, A03 one");
    // chain off: the chosen pattern
    e->bank.chain.on = 0; e->global[gPATTERN] = 5.f;
    for (int i = 0; i < 48000 * 3; i += 64) e->process(L.data(), R.data(), 64, Transport(), nullptr, 0);
    check(e->currentPattern() == 5, "CHAIN off: back to the chosen pattern at the next bar");
    std::printf(fails ? "%d failures\n" : "all passed\n", fails);
    return fails ? 1 : 0;
}
