// KMSP-6 wavetables: factory tables, user tables (codec, damaged data), TABLE kit field in the engine.
//   g++ -O2 -std=c++17 tests/wavetable.cpp -o /tmp/t && /tmp/t
#include "../src/Engine.hpp"
#include "../src/Presets.hpp"
#include "../src/WaveUser.hpp"
#include <cstdio>
#include <memory>
#include <random>
using namespace kn;

// energy of a table level above harmonic `limit` (should be ~0: band-limited)
static double aboveLimit(const float* x, int limit)
{
    std::vector<std::complex<double>> a(WaveSet::kSize);
    for (int i = 0; i < WaveSet::kSize; ++i) a[i] = x[i];
    WaveSet::fft(a, false);
    double hi = 0, all = 1e-12;
    for (int h = 1; h < WaveSet::kSize / 2; ++h) { const double e = std::norm(a[h]); all += e; if (h >= limit) hi += e; }
    return hi / all;
}

int main()
{
    int fails = 0;
    const WaveBank& wb = WaveBank::get();
    for (int t = 0; t < kFactoryTables; ++t) {
        const WaveSet& s = wb.set[t];
        bool ok = s.frames == 32;
        double worst = 0, rms0 = 0;
        for (int f = 0; f < s.frames && ok; ++f)
            for (int L = 0; L < WaveSet::kLevels; ++L) {
                const float* x = s.table(f, L);
                for (int i = 0; i <= WaveSet::kSize; ++i) if (!std::isfinite(x[i]) || std::fabs(x[i]) > 1.5f) ok = false;
                if (L >= 2) worst = std::max(worst, aboveLimit(x, (WaveSet::kSize / 2) >> L));
                if (L == 0) for (int i = 0; i < WaveSet::kSize; ++i) rms0 += x[i] * x[i];
            }
        rms0 = std::sqrt(rms0 / (32.0 * WaveSet::kSize));
        ok = ok && worst < 1e-9 && rms0 > 0.15;
        std::printf("%s %-8s band limit %.1e, rms %.2f\n", ok ? "ok  " : "FAIL", kWaveTableNames[t], worst, rms0);
        if (!ok) ++fails;
    }
    // user table: 8 frames of 2048 (saw -> sine), codec round trip
    {
        std::vector<float> pcm(8 * 2048);
        for (int f = 0; f < 8; ++f) for (int i = 0; i < 2048; ++i) {
            const double t = i / 2048.0, m = f / 7.0;
            pcm[f * 2048 + i] = (float)((1 - m) * (2 * t - 1) + m * std::sin(6.283185307 * t)) * 0.8f;
        }
        UserWavePtr u = wav::make("Test Saw", pcm.data(), pcm.size());
        bool ok = u && u->set.frames == 8 && u->frameLen == 2048;
        const std::string line = u ? wav::encodeLine(2, *u) : "";
        UserWavePtr back; int bt = -1;
        wav::decodeAll(line.c_str(), [&](int t, UserWavePtr w) { bt = t; back = std::move(w); });
        ok = ok && back && bt == 2 && back->name == "Test Saw" && back->set.frames == 8;
        double d = 0;
        if (ok) for (size_t i = 0; i < u->pcm.size(); ++i) d = std::max(d, (double)std::fabs(u->pcm[i] - back->pcm[i]));
        ok = ok && d < 1e-4;
        // a single cycle of odd length, and more than 32 frames
        UserWavePtr one = wav::make("cycle", pcm.data(), 600);
        std::vector<float> big(40 * 2048, 0.3f);
        for (size_t i = 0; i < big.size(); ++i) big[i] = (float)std::sin(i * 0.05);
        UserWavePtr many = wav::make("many", big.data(), big.size());
        ok = ok && one && one->set.frames == 1 && many && many->set.frames == 32 && many->frameLen == 2048;
        std::printf("%s user table: make, encode / decode (max error %.1e), single cycle, 40 -> 32 frames\n", ok ? "ok  " : "FAIL", d);
        if (!ok) ++fails;
        // damaged lines never crash and give no table
        std::mt19937 rng(7); int bad = 0;
        for (int it = 0; it < 300; ++it) {
            std::string s = line;
            for (int k = 0; k < 6; ++k) s[rng() % s.size()] = "w0123456789\t \nAZaz+/="[rng() % 21];
            if (rng() % 3 == 0) s.resize(rng() % s.size());
            wav::decodeAll(s.c_str(), [&](int t, UserWavePtr w) { if (w && (w->set.frames < 1 || w->set.frames > 32)) ++bad; (void)t; });
        }
        std::printf("%s damaged user table lines\n", bad ? "FAIL" : "ok  ");
        if (bad) ++fails;
    }
    // engine: a WAVE track sounds different on each table, finite
    {
        double ref = 0; int same = 0, nan = 0;
        for (int tab = 0; tab <= kTableUser; ++tab) {
            auto b = std::make_unique<Bank>(); std::vector<float> v(kTotalParams); buildPreset(0, v.data(), *b);
            auto e = std::make_unique<Engine>(); e->init(48000);
            for (int t = 0; t < kTracks; t++) for (int p = 0; p < kTrackParams; p++) e->trackParam[t][p] = v[t * kTrackParams + p];
            for (int g = 0; g < gOUT_STEP; g++) e->global[g] = v[kGlobalBase + g];
            for (int t = 0; t < kTracks; ++t) b->machine[t] = mOFF;
            b->machine[0] = mWAVE; b->kit[0][kfTable] = (uint8_t)tab;
            for (int p = 0; p < 8; ++p) e->trackParam[0][p] = kMachineDefs[mWAVE].def[p];
            e->trackParam[0][0] = 70;
            for (int s = 0; s < 16; ++s) b->patterns[0].flags[0][s] = sfTrig;
            e->global[gPLAY] = 1; e->global[gREV_LEV] = 0; e->global[gECHO_LEV] = 0;
            e->bank = *b;
            std::vector<float> L(256), R(256); double sum = 0;
            for (int i = 0; i < 48000 * 2 / 256; ++i) {
                e->process(L.data(), R.data(), 256, Transport(), nullptr, 0);
                for (int k = 0; k < 256; ++k) { if (!std::isfinite(L[k])) ++nan; sum += L[k] * (k % 7 - 3); }
            }
            if (tab == 0) ref = sum; else if (tab != kTableUser && std::fabs(sum - ref) < 1e-6 * (std::fabs(ref) + 1)) ++same;
            if (tab == kTableUser && std::fabs(sum - ref) > 1e-6 * (std::fabs(ref) + 1)) ++same;   // empty USER = CLASSIC
        }
        const bool ok = same == 0 && nan == 0;
        std::printf("%s engine: every table sounds different, empty USER plays CLASSIC\n", ok ? "ok  " : "FAIL");
        if (!ok) ++fails;
    }
    std::printf(fails ? "%d failures\n" : "all passed\n", fails);
    return fails ? 1 : 0;
}
