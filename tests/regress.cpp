// Sound regression test: renders every factory preset and compares a fingerprint (level and brightness every
// half second) with tests/regress-ref.txt. Identical in kinetik-kdsp16 and kinetik-kmsp6.
//   g++ -O2 -std=c++17 tests/regress.cpp -o /tmp/regress && /tmp/regress            compare (exit 1 on change)
//   /tmp/regress --write                                                            store new references
// Run --write only after an intended change of sound, and say so in the commit.
#include "../src/Engine.hpp"
#include "../src/Presets.hpp"
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
using namespace kn;

static constexpr int kSr = 48000, kBlock = 256, kSeconds = 8, kSegs = kSeconds * 2;
static constexpr float kTolDb = 1.0f;   // allowed drift (compilers, CPUs)
static constexpr float kFloorDb = -70.f;

struct Print { float lev[kSegs], bright[kSegs]; };

static Print render(int preset)
{
    std::vector<float> v(kTotalParams); Bank* b = new Bank(); buildPreset(preset, v.data(), *b);
    Engine* e = new Engine(); e->init(kSr); e->bank = *b;
    for (int t = 0; t < kTracks; t++) for (int p = 0; p < kTrackParams; p++) e->trackParam[t][p] = v[t * kTrackParams + p];
    for (int g = 0; g < gOUT_STEP; g++) e->global[g] = v[kGlobalBase + g];
    e->global[gPLAY] = 1.f;
    std::vector<float> L(kBlock), R(kBlock);
    Print pr{};
    double sum[kSegs] = {}, dsum[kSegs] = {}; float prev = 0.f;
    const int segLen = kSr / 2;
    for (int i = 0; i < kSr * kSeconds; i += kBlock) {
        e->process(L.data(), R.data(), kBlock, Transport(), nullptr, 0);
        for (int j = 0; j < kBlock; ++j) {
            const float m = 0.5f * (L[j] + R[j]);
            const int s = (i + j) / segLen;
            if (s < kSegs) { sum[s] += (double)m * m; dsum[s] += (double)(m - prev) * (m - prev); }
            prev = m;
        }
    }
    for (int s = 0; s < kSegs; ++s) {
        const double rms = std::sqrt(sum[s] / segLen), drms = std::sqrt(dsum[s] / segLen);
        pr.lev[s] = (float)std::max<double>(-120.0, 20.0 * std::log10(rms + 1e-12));
        pr.bright[s] = pr.lev[s] > kFloorDb ? (float)(20.0 * std::log10((drms + 1e-12) / (rms + 1e-12))) : 0.f;
    }
    delete e; delete b;
    return pr;
}

static std::string line(int i, const Print& p)
{
    std::string s = std::to_string(i);
    char b[24];
    for (int k = 0; k < kSegs; ++k) { std::snprintf(b, sizeof(b), " %.1f", p.lev[k]); s += b; }
    s += " |";
    for (int k = 0; k < kSegs; ++k) { std::snprintf(b, sizeof(b), " %.1f", p.bright[k]); s += b; }
    return s;
}

int main(int argc, char** argv)
{
    const bool write = argc > 1 && std::strcmp(argv[1], "--write") == 0;
    const char* refPath = "tests/regress-ref.txt";
    if (write) {
        FILE* f = std::fopen(refPath, "w");
        if (!f) { std::printf("cannot write %s (run from the repository root)\n", refPath); return 2; }
        std::fprintf(f, "# preset: level dB per 0.5 s (%d) | brightness dB per 0.5 s (%d)\n", kSegs, kSegs);
        for (int i = 0; i < kNumPresets; ++i) std::fprintf(f, "%s\n", line(i, render(i)).c_str());
        std::fclose(f);
        std::printf("wrote %d references to %s\n", kNumPresets, refPath);
        return 0;
    }
    FILE* f = std::fopen(refPath, "r");
    if (!f) { std::printf("no %s (run from the repository root, or --write)\n", refPath); return 2; }
    std::vector<std::vector<float>> ref(kNumPresets);
    char buf[4096];
    while (std::fgets(buf, sizeof(buf), f)) {
        if (buf[0] == '#') continue;
        int idx = -1, off = 0;
        if (std::sscanf(buf, "%d%n", &idx, &off) != 1 || idx < 0 || idx >= kNumPresets) continue;
        const char* s = buf + off; float x; int n = 0;
        while (*s) {
            while (*s == ' ' || *s == '|') ++s;
            if (std::sscanf(s, "%f%n", &x, &n) != 1) break;
            ref[idx].push_back(x); s += n;
        }
    }
    std::fclose(f);
    int bad = 0;
    for (int i = 0; i < kNumPresets; ++i) {
        const Print p = render(i);
        if (ref[i].size() != (size_t)(2 * kSegs)) { std::printf("%-20s no reference (run --write)\n", kPresets[i].name); ++bad; continue; }
        float worst = 0.f; int where = -1;
        for (int k = 0; k < kSegs; ++k) {
            const float rl = ref[i][k], rb = ref[i][kSegs + k];
            if (rl < kFloorDb && p.lev[k] < kFloorDb) continue;     // silent in both
            const float d = std::max(std::fabs(p.lev[k] - rl), (rl > kFloorDb && p.lev[k] > kFloorDb) ? std::fabs(p.bright[k] - rb) : 0.f);
            if (d > worst) { worst = d; where = k; }
        }
        const bool ok = worst <= kTolDb;
        if (!ok) ++bad;
        std::printf("%s %-20s worst %.2f dB%s\n", ok ? "ok  " : "FAIL", kPresets[i].name, worst,
                    ok || where < 0 ? "" : (" at " + std::to_string(where * 0.5) + " s").c_str());
    }
    if (bad) { std::printf("%d presets sound different from the references\n", bad); return 1; }
    std::printf("all %d presets match\n", kNumPresets);
    return 0;
}
