// Kinetik KMSP-6 — band-limited wavetables, generated at startup (original spectra, no samples)
#pragma once
#include <cmath>
#include <complex>
#include <vector>
#include <mutex>
#include <memory>

namespace kn {

struct WaveBank {
    static constexpr int kFrames = 32;
    static constexpr int kSize = 2048;
    static constexpr int kLevels = 11;        // level L keeps harmonics up to 1024 >> L
    std::vector<float> data;                  // [frame][level][kSize + 1]

    const float* table(int frame, int level) const
    {
        return &data[((size_t)frame * kLevels + level) * (kSize + 1)];
    }

    static void fft(std::vector<std::complex<double>>& a, bool inverse)
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
            const std::complex<double> wl(std::cos(ang), std::sin(ang));
            for (size_t i = 0; i < n; i += len) {
                std::complex<double> w(1.0, 0.0);
                for (size_t j = 0; j < len / 2; ++j) {
                    const auto u = a[i + j], v = a[i + j + len / 2] * w;
                    a[i + j] = u + v; a[i + j + len / 2] = u - v;
                    w *= wl;
                }
            }
        }
    }

    // amplitude of harmonic h (1-based) for frame f
    static double harmonic(int f, int h)
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

    void build()
    {
        data.assign((size_t)kFrames * kLevels * (kSize + 1), 0.f);
        std::vector<std::complex<double>> spec(kSize);
        for (int f = 0; f < kFrames; ++f) {
            double norm = 1.0;
            for (int L = 0; L < kLevels; ++L) {
                const int maxH = (kSize / 2) >> L;
                std::fill(spec.begin(), spec.end(), std::complex<double>(0, 0));
                for (int h = 1; h < maxH && h < kSize / 2; ++h) {
                    const double a = harmonic(f, h) * kSize / 2.0;
                    spec[h] = std::complex<double>(0, -a);
                    spec[kSize - h] = std::complex<double>(0, a);
                }
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
    }

    static const WaveBank& get()
    {
        static std::once_flag once;
        static std::unique_ptr<WaveBank> bank;
        std::call_once(once, [] { bank.reset(new WaveBank()); bank->build(); });
        return *bank;
    }
};

} // namespace kn
