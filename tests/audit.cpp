// Preset audit (shared by kinetik-kdsp16 and kinetik-kmsp6): renders every factory preset and prints loudness
// (BS.1770 K-weighted, gated), peak, crest, bass / mid / high balance, stereo width and how many tracks are heard.
//   g++ -O2 -std=c++17 -pthread tests/audit.cpp -o /tmp/audit && /tmp/audit [preset]  (also writes out/<n>.wav)
#include "../src/Engine.hpp"
#include "../src/Presets.hpp"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <vector>
using namespace kn;

static constexpr int kSr = 48000, kBlock = 256;

struct ABiquad {
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
    double run(double x) { const double y = b0 * x + z1; z1 = b1 * x - a1 * y + z2; z2 = b2 * x - a2 * y; return y; }
};
// K-weighting at 48 kHz (ITU-R BS.1770 coefficients)
static void kWeight(ABiquad& s, ABiquad& h)
{
    s.b0 = 1.53512485958697; s.b1 = -2.69169618940638; s.b2 = 1.19839281085285; s.a1 = -1.69065929318241; s.a2 = 0.73248077421585;
    h.b0 = 1.0; h.b1 = -2.0; h.b2 = 1.0; h.a1 = -1.99004745483398; h.a2 = 0.99007225036621;
}
static void lp(ABiquad& f, double hz) { const double w = 2 * M_PI * hz / kSr, c = std::cos(w), a = std::sin(w) / (2 * 0.707), n = 1 + a;
    f.b0 = (1 - c) / 2 / n; f.b1 = (1 - c) / n; f.b2 = (1 - c) / 2 / n; f.a1 = -2 * c / n; f.a2 = (1 - a) / n; }
static void hp(ABiquad& f, double hz) { const double w = 2 * M_PI * hz / kSr, c = std::cos(w), a = std::sin(w) / (2 * 0.707), n = 1 + a;
    f.b0 = (1 + c) / 2 / n; f.b1 = -(1 + c) / n; f.b2 = (1 + c) / 2 / n; f.a1 = -2 * c / n; f.a2 = (1 - a) / n; }

struct Result { double lufs, peak, crest, low, mid, high, width; int heard, tracksOn; };

static float gExtraDb = 0.f;   // --fit: extra level trim on top of the preset's own
static void load(int preset, Engine& e, Bank& b, std::vector<float>& v)
{
    v.assign(kTotalParams, 0.f); buildPreset(preset, v.data(), b);
    applyLevelTrim(v.data(), gExtraDb);
    e.init(kSr); e.bank = b;
    for (int t = 0; t < kTracks; t++) for (int p = 0; p < kTrackParams; p++) e.trackParam[t][p] = v[tpIndex(t, p)];
    for (int g = 0; g < gOUT_STEP; g++) e.global[g] = v[kGlobalBase + g];
    e.global[gPLAY] = 1.f;
}

static double gPeak = 0;
static double renderRms(int preset, int seconds, int onlyTrack, std::vector<float>* outL = nullptr, std::vector<float>* outR = nullptr)
{
    gPeak = 0;
    auto e = std::make_unique<Engine>(); auto b = std::make_unique<Bank>(); std::vector<float> v;
    load(preset, *e, *b, v);
    if (onlyTrack >= 0) { for (int t = 0; t < kTracks; ++t) e->bank.mute[t] = t != onlyTrack; }
    std::vector<float> L(kBlock), R(kBlock); double s = 0; long n = 0;
    for (int i = 0; i < kSr * seconds; i += kBlock) {
        e->process(L.data(), R.data(), kBlock, Transport(), nullptr, 0);
        for (int j = 0; j < kBlock; ++j) { s += 0.5 * (L[j] * L[j] + R[j] * R[j]); ++n; gPeak = std::max(gPeak, (double)std::max(std::fabs(L[j]), std::fabs(R[j]))); }
        if (outL) { outL->insert(outL->end(), L.begin(), L.end()); outR->insert(outR->end(), R.begin(), R.end()); }
    }
    return std::sqrt(s / std::max(1L, n));
}

static void writeWav(const std::string& path, const std::vector<float>& L, const std::vector<float>& R)
{
    FILE* f = std::fopen(path.c_str(), "wb"); if (!f) return;
    const uint32_t n = (uint32_t)L.size(), data = n * 4, sz = 36 + data;
    auto w32 = [&](uint32_t x) { std::fwrite(&x, 4, 1, f); }; auto w16 = [&](uint16_t x) { std::fwrite(&x, 2, 1, f); };
    std::fwrite("RIFF", 1, 4, f); w32(sz); std::fwrite("WAVEfmt ", 1, 8, f); w32(16); w16(1); w16(2); w32(kSr); w32(kSr * 4); w16(4); w16(16);
    std::fwrite("data", 1, 4, f); w32(data);
    for (uint32_t i = 0; i < n; ++i) { int16_t a = (int16_t)std::lround(std::max(-1.f, std::min(1.f, L[i])) * 32767); int16_t c = (int16_t)std::lround(std::max(-1.f, std::min(1.f, R[i])) * 32767); w16((uint16_t)a); w16((uint16_t)c); }
    std::fclose(f);
}

static double lufsOfBuffers(const std::vector<float>& L, const std::vector<float>& R);
static double analyseLufs(int preset)
{
    std::vector<float> L, R;
    renderRms(preset, 12, -1, &L, &R);
    return lufsOfBuffers(L, R);
}
static Result analyse(int preset, bool wav)
{
    std::vector<float> L, R;
    renderRms(preset, 12, -1, &L, &R);
    if (wav) writeWav("out/" + std::to_string(preset) + ".wav", L, R);
    Result r{};
    r.lufs = lufsOfBuffers(L, R);

    double pk = 0, sq = 0, mid2 = 0, side2 = 0;
    ABiquad lo, hiF, b1, b2; lp(lo, 150); hp(hiF, 4000); hp(b1, 150); lp(b2, 4000);
    double el = 0, em = 0, eh = 0;
    for (size_t i = 0; i < L.size(); ++i) {
        pk = std::max(pk, (double)std::max(std::fabs(L[i]), std::fabs(R[i])));
        const double m = 0.5 * (L[i] + R[i]), s = 0.5 * (L[i] - R[i]);
        sq += m * m; mid2 += m * m; side2 += s * s;
        const double a = lo.run(m), c = hiF.run(m), d = b2.run(b1.run(m));
        el += a * a; eh += c * c; em += d * d;
    }
    const double rms = std::sqrt(sq / L.size());
    r.peak = 20 * std::log10(pk + 1e-12); r.crest = r.peak - 20 * std::log10(rms + 1e-12);
    const double tot = el + em + eh + 1e-20;
    r.low = 100 * el / tot; r.mid = 100 * em / tot; r.high = 100 * eh / tot;
    r.width = 10 * std::log10((side2 + 1e-20) / (mid2 + 1e-20));
    // tracks that sound on their own (solo render above -50 dBFS RMS) and tracks with a machine
    auto b = std::make_unique<Bank>(); std::vector<float> v(kTotalParams); buildPreset(preset, v.data(), *b);
    for (int t = 0; t < kTracks; ++t) {
        if (b->machine[t] == 0) continue;
        bool any = false;
        for (int p = 0; p < kPatterns && !any; ++p) for (int s = 0; s < kSteps; ++s) if (b->patterns[p].flags[t][s]) { any = true; break; }
        if (!any) continue;
        ++r.tracksOn;
        const double db = 20 * std::log10(renderRms(preset, 6, t) + 1e-12);
        if (db > -50) ++r.heard;
        if (std::getenv("AUDIT_TRACKS")) std::printf("    track %d machine %d: rms %.1f peak %.1f dBFS\n", t + 1, b->machine[t], db, 20 * std::log10(gPeak + 1e-12));
    }
    return r;
}

static double lufsOfBuffers(const std::vector<float>& L, const std::vector<float>& R)
{
    // K-weighted gated loudness over 400 ms blocks, 75 % overlap
    ABiquad sL, hL, sR, hR; kWeight(sL, hL); kWeight(sR, hR);
    std::vector<double> kw((size_t)L.size());
    for (size_t i = 0; i < L.size(); ++i) { const double a = hL.run(sL.run(L[i])), c = hR.run(sR.run(R[i])); kw[i] = a * a + c * c; }
    const size_t blk = kSr * 4 / 10, hop = blk / 4;
    std::vector<double> z;
    for (size_t i = kSr * 2; i + blk <= kw.size(); i += hop) { double s = 0; for (size_t k = 0; k < blk; ++k) s += kw[i + k]; z.push_back(s / blk); }   // skip the first 2 s
    auto lufsOf = [](double m) { return -0.691 + 10 * std::log10(m + 1e-20); };
    double acc = 0; int cnt = 0;
    for (double m : z) if (lufsOf(m) > -70) { acc += m; ++cnt; }
    const double rel = lufsOf(acc / std::max(1, cnt)) - 10; acc = 0; cnt = 0;
    for (double m : z) if (lufsOf(m) > -70 && lufsOf(m) > rel) { acc += m; ++cnt; }
    return cnt ? lufsOf(acc / cnt) : -99;
}

int main(int argc, char** argv)
{
    if (std::system("mkdir -p out") != 0) return 1;
    if (argc > 2 && std::string(argv[1]) == "--fit") {   // --fit <LUFS>: trim (dB) that brings each preset to that loudness
        const double target = std::atof(argv[2]);
        for (int i = 1; i < kNumPresets; ++i) {
            gExtraDb = 0.f; double l = 0;
            for (int it = 0; it < 4; ++it) {
                l = analyseLufs(i);
                const double d = target - l;
                if (std::fabs(d) < 0.3) break;
                gExtraDb += (float)d;
            }
            std::printf("%d %.1f %.1f %s\n", i, kPresets[i].trimDb + gExtraDb, l, kPresets[i].name);
            std::fflush(stdout);
        }
        return 0;
    }
    const int only = argc > 1 ? std::atoi(argv[1]) : -1;
    std::printf("%-3s %-22s %7s %6s %6s %5s %5s %5s %6s %s\n", "#", "preset", "LUFS", "peak", "crest", "low%", "mid%", "hi%", "width", "tracks heard/used");
    for (int i = 0; i < kNumPresets; ++i) {
        if (only >= 0 && i != only) continue;
        const Result r = analyse(i, only >= 0);
        std::printf("%-3d %-22s %7.1f %6.1f %6.1f %5.0f %5.0f %5.0f %6.1f %d/%d\n", i, kPresets[i].name, r.lufs, r.peak, r.crest, r.low, r.mid, r.high, r.width, r.heard, r.tracksOn);
    }
    return 0;
}
