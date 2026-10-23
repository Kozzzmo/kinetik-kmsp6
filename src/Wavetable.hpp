// Kinetik KMSP-6 — band-limited wavetables for the WAVE and CHORD machines.
// 8 factory tables generated at start-up (original spectra and shapes, no samples) + one user table per track,
// imported from a WAV file (frames of 2048 samples, or 256..4096) and built off the audio thread.
// Every table: up to 32 frames x 11 mip levels (level L keeps the harmonics below 1024 >> L) x 2049 samples.
#pragma once
#include <cmath>
#include <complex>
#include <vector>
#include <string>
#include <mutex>
#include <memory>
#include <algorithm>
#include <functional>
#include <atomic>
#include <thread>
#include "Params.hpp"

namespace kn {

// kFactoryTables, kTableUser, kWaveTableNames: Params.hpp

struct WaveSet {
    static constexpr int kMaxFrames = 32;
    static constexpr int kSize = 2048;
    static constexpr int kLevels = 11;
    int frames = 0;
    std::string name;
    std::vector<float> data;                  // [frame][level][kSize + 1]

    const float* table(int frame, int level) const { return &data[((size_t)frame * kLevels + level) * (kSize + 1)]; }

    using cd = std::complex<double>;
    static void fft(std::vector<cd>& a, bool inverse)
    {
        const size_t n = a.size();
        for (size_t i = 1, j = 0; i < n; ++i) {
            size_t bit = n >> 1;
            for (; j & bit; bit >>= 1) j ^= bit;
            j ^= bit;
            if (i < j) std::swap(a[i], a[j]);
        }
        for (size_t len = 2; len <= n; len <<= 1) {
            const double ang = 2.0 * 3.14159265358979323846 / (double)len * (inverse ? 1.0 : -1.0);
            const cd wl(std::cos(ang), std::sin(ang));
            for (size_t i = 0; i < n; i += len) {
                cd w(1.0, 0.0);
                for (size_t j = 0; j < len / 2; ++j) {
                    const auto u = a[i + j], v = a[i + j + len / 2] * w;
                    a[i + j] = u + v; a[i + j + len / 2] = u - v;
                    w *= wl;
                }
            }
        }
    }

    void alloc(int n) { frames = std::max(1, std::min(n, kMaxFrames)); data.assign((size_t)frames * kLevels * (kSize + 1), 0.f); }

    // frame f from its spectrum: S[h] (h = 1 .. kSize/2 - 1) = complex amplitude scaled like the FFT of a kSize-sample
    // frame (a sine of amplitude a = (0, -a * kSize / 2)); every level, normalized to a 0.95 peak at level 0
    void buildFrame(int f, const std::vector<cd>& S)
    {
        std::vector<cd> spec(kSize);
        double norm = 1.0;
        for (int L = 0; L < kLevels; ++L) {
            const int maxH = (kSize / 2) >> L;
            std::fill(spec.begin(), spec.end(), cd(0, 0));
            for (int h = 1; h < maxH && h < kSize / 2; ++h) { spec[h] = S[h]; spec[kSize - h] = std::conj(S[h]); }
            fft(spec, true);
            float* out = &data[((size_t)f * kLevels + L) * (kSize + 1)];
            if (L == 0) {
                double pk = 1e-9;
                for (int i = 0; i < kSize; ++i) pk = std::max(pk, std::fabs(spec[i].real() / kSize));
                norm = 0.95 / pk;
            }
            for (int i = 0; i < kSize; ++i) out[i] = (float)(spec[i].real() / kSize * norm);
            out[kSize] = out[0];
        }
    }
    // spectrum from harmonic amplitudes (sine phase): amp(h), h >= 1
    static std::vector<cd> sineSpectrum(const std::function<double(int)>& amp)
    {
        std::vector<cd> S(kSize / 2);
        for (int h = 1; h < kSize / 2; ++h) S[h] = cd(0, -amp(h) * kSize / 2.0);
        return S;
    }
    // spectrum of one period given as samples (any length >= 16): resampled linearly to N (a power of two, at least
    // kSize, 8x oversampled for drawn shapes), FFT, DC removed, harmonics above kSize/2 dropped (band limit)
    static std::vector<cd> periodSpectrum(const float* x, int len, int N)
    {
        std::vector<cd> a((size_t)N);
        for (int i = 0; i < N; ++i) {
            const double p = (double)i * len / N;
            const int i0 = (int)p % len, i1 = (i0 + 1) % len;
            const double fr = p - std::floor(p);
            a[(size_t)i] = cd(x[i0] + (x[i1] - x[i0]) * fr, 0.0);
        }
        fft(a, false);
        std::vector<cd> S(kSize / 2);
        for (int h = 1; h < kSize / 2 && h < N / 2; ++h) S[h] = a[(size_t)h] * ((double)kSize / N);
        return S;
    }
    // a shape s(t), t in [0, 1): sampled 8x finer than the table, so sharp edges do not alias into the table
    static std::vector<cd> shapeSpectrum(const std::function<double(double)>& s)
    {
        const int N = kSize * 8;
        std::vector<float> x((size_t)N);
        for (int i = 0; i < N; ++i) x[(size_t)i] = (float)s((double)i / N);
        return periodSpectrum(x.data(), N, N);
    }

    // user table: nFrames periods of frameLen samples each (more than 32 frames: 32 picked evenly)
    bool fromFrames(const float* pcm, int nFrames, int frameLen)
    {
        if (!pcm || nFrames < 1 || frameLen < 16) return false;
        const int n = std::min(nFrames, kMaxFrames);
        alloc(n);
        int N = kSize; while (N < frameLen) N <<= 1;
        for (int f = 0; f < n; ++f) {
            const int src = n == 1 ? 0 : (int)std::lround((double)f * (nFrames - 1) / (n - 1));
            buildFrame(f, periodSpectrum(pcm + (size_t)src * frameLen, frameLen, N));
        }
        return true;
    }
};

// amplitude of harmonic h (1-based) of frame f of the CLASSIC table (the only table before 0.9)
inline double classicHarmonic(int f, int h)
{
    const double hd = (double)h;
    if (f < 8) {                       // saw -> square
        const double t = f / 7.0;
        return (h % 2 ? 1.0 : 1.0 - t) / hd;
    }
    if (f < 16) {                      // vocal / formant-like peaks moving up the spectrum
        const double c = 2.0 + (f - 8) * 2.6, w = 1.6;
        return std::pow(hd, -0.75) * (0.12 + std::exp(-(hd - c) * (hd - c) / (2.0 * w * w)));
    }
    if (f < 24) {                      // drawbar organs
        static const double bars[8][6] = {
            {1, 0.8, 0.0, 0.6, 0.0, 0.4}, {1, 0.0, 0.8, 0.0, 0.6, 0.0}, {0.6, 1, 0.6, 0.3, 0.2, 0.1},
            {1, 1, 1, 1, 0.0, 0.0},       {0.3, 0.6, 1, 0.6, 0.3, 0.6}, {1, 0.0, 0.0, 0.8, 0.0, 0.8},
            {0.5, 0.2, 0.9, 0.1, 0.7, 0.3}, {1, 0.7, 0.5, 0.35, 0.25, 0.18}};
        static const int hs[6] = {1, 2, 3, 4, 6, 8};
        for (int i = 0; i < 6; ++i) if (hs[i] == h) return bars[f - 16][i];
        return h > 8 && h < 32 ? 0.04 / hd : 0.0;
    }
    const double d = 0.5 - (f - 24) * (0.45 / 7.0); // pulse width sweep 50% -> 5%
    return 2.0 / (3.14159265358979323846 * hd) * std::sin(3.14159265358979323846 * hd * d);
}

struct WaveBank {
    WaveSet set[kFactoryTables];

    static double frac(double x) { return x - std::floor(x); }

    void build()
    {
        constexpr int F = WaveSet::kMaxFrames;
        for (auto& s : set) s.alloc(F);
        for (int t = 0; t < kFactoryTables; ++t) set[t].name = kWaveTableNames[t];
        // frames are independent: several threads share them (start-up time)
        std::atomic<int> next{0};
        auto work = [&] { for (int f; (f = next.fetch_add(1)) < F;) buildFrames(f); };
        const unsigned nt = std::max(1u, std::min(8u, std::thread::hardware_concurrency()));
        std::vector<std::thread> th;
        for (unsigned i = 1; i < nt; ++i) th.emplace_back(work);
        work();
        for (auto& t : th) t.join();
    }

    void buildFrames(int f)
    {
        constexpr double pi = 3.14159265358979323846, tp = 2.0 * pi;
        constexpr int F = WaveSet::kMaxFrames;
        {
            const double u = f / (double)(F - 1);   // 0 .. 1 across the table
            // CLASSIC: saw -> square, formants, drawbars, pulse width (unchanged since 0.1)
            set[0].buildFrame(f, WaveSet::sineSpectrum([f](int h) { return classicHarmonic(f, h); }));
            // BASIC: sine -> triangle -> saw -> square, smooth crossfades
            set[1].buildFrame(f, WaveSet::shapeSpectrum([u, tp, pi](double t) {
                const double s = std::sin(tp * t), tri = 2.0 / pi * std::asin(s), saw = 1.0 - 2.0 * frac(t + 0.5), sq = t < 0.5 ? 1.0 : -1.0;
                const double m = u * 3.0;
                if (m < 1.0) return s + (tri - s) * m;
                if (m < 2.0) return tri + (saw - tri) * (m - 1.0);
                return saw + (sq - saw) * (m - 2.0);
            }));
            // VOCAL: harmonics of a 130 Hz voice through vowel formants A E I O U A
            {
                static const double V[6][3] = {{800, 1150, 2900}, {400, 1700, 2600}, {290, 2250, 3000}, {450, 800, 2830}, {325, 700, 2530}, {800, 1150, 2900}};
                static const double G[3] = {1.0, 0.5, 0.25}, B[3] = {80, 90, 120};
                const double p = u * 5.0; const int a = std::min(4, (int)p); const double w = p - a;
                double Fm[3];
                for (int k = 0; k < 3; ++k) Fm[k] = V[a][k] + (V[a + 1][k] - V[a][k]) * w;
                set[2].buildFrame(f, WaveSet::sineSpectrum([&](int h) {
                    const double hz = 130.0 * h; double g = 0;
                    for (int k = 0; k < 3; ++k) { const double x = (hz - Fm[k]) / B[k]; g += G[k] / (1.0 + x * x); }
                    return (0.04 + g) * std::pow((double)h, -0.6) * (hz < 6000.0 ? 1.0 : 0.0);
                }));
            }
            // ORGAN: drawbar registrations (harmonics 1 2 3 4 5 6 8 10 12 16), from flute to full
            {
                static const int hs[10] = {1, 2, 3, 4, 5, 6, 8, 10, 12, 16};
                double w[10];
                for (int k = 0; k < 10; ++k) {
                    const double c = 0.5 + 0.5 * std::cos(pi * (k * 0.37 + u * (1.0 + k * 0.23)) * 1.7);
                    w[k] = k == 0 ? 1.0 : c * std::min(1.0, u * 1.6 + 0.15) * (k < 4 ? 1.0 : 0.8);
                }
                set[3].buildFrame(f, WaveSet::sineSpectrum([&](int h) {
                    for (int k = 0; k < 10; ++k) if (hs[k] == h) return w[k];
                    return 0.0;
                }));
            }
            // SYNC: a saw hard-synced to the cycle, slave ratio 1 -> 8, softened at the reset
            set[4].buildFrame(f, WaveSet::shapeSpectrum([u](double t) {
                const double r = 1.0 + 7.0 * u;
                return (1.0 - 2.0 * frac(t * r)) * (1.0 - 0.3 * t);
            }));
            // FOLD: a sine through a wavefolder, drive 1 -> 9
            set[5].buildFrame(f, WaveSet::shapeSpectrum([u, tp, pi](double t) {
                const double g = 1.0 + 8.0 * u * u;
                return std::sin(0.5 * pi * g * std::sin(tp * t));
            }));
            // DIGITAL: harmonics switched on and off by bit patterns (stepped, glassy)
            set[6].buildFrame(f, WaveSet::sineSpectrum([f](int h) {
                if (h > 96) return 0.0;
                const unsigned mask = (unsigned)(f * 7 + 3) & 31u;
                if (h == 1) return 1.0;
                return ((unsigned)h & mask) != 0u && ((unsigned)h % (unsigned)(f % 5 + 2)) != 1u ? 0.9 / std::sqrt((double)h) : 0.0;
            }));
            // FM: two-operator FM, modulator ratio 1 -> 8 (steps of four frames), index rising inside each step
            set[7].buildFrame(f, WaveSet::shapeSpectrum([f, tp](double t) {
                const int ratio = 1 + f / 4; const double index = 0.6 + 0.9 * (f % 4);
                return std::sin(tp * t + index * std::sin(tp * ratio * t));
            }));
        }
    }

    const WaveSet& factory(int i) const { return set[i < 0 || i >= kFactoryTables ? 0 : i]; }

    static const WaveBank& get()
    {
        static std::once_flag once;
        static std::unique_ptr<WaveBank> bank;
        std::call_once(once, [] { bank.reset(new WaveBank()); bank->build(); });
        return *bank;
    }
};

using WaveSetPtr = std::shared_ptr<const WaveSet>;

} // namespace kn
