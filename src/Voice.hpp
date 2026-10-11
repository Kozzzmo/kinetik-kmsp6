// Kinetik KMSP-6 — monophonic oscillator section ("machines"). Envelopes and filters live in the engine.
#pragma once
#include "Dsp.hpp"
#include "Params.hpp"
#include "Wavetable.hpp"
#include <cstring>
#include <vector>
#include <algorithm>

namespace kn {

struct MonoVoice {
    float sr = 48000.f, isr = 1.f / 48000.f;
    int machine = mOFF;
    Rng rng;
    float x[8] = {};

    // shared oscillator state
    float ph[8] = {}, phSub = 0, phAux = 0, phLfo = 0;
    float crushHold = 0; int crushCnt = 0;
    // FM
    float fmEnv = 0, fmEnvCoef = 0.999f, fmFb1 = 0, fmFb2 = 0;
    // VOX
    Svf form[3]; OnePole glot; float vibPh = 0;
    // PLUCK
    std::vector<float> ks;
    uint32_t ksW = 0; float ksLp = 0, ksApZ = 0, ksExcLeft = 0; OnePole ksExcLp; Svf body;
    // NOISE
    Svf nf; float grainAmp = 1.f;
    // slow automatic level control for the resonant machines (VOX, NOISE)
    float agcEnv = 0.06f, agcA = 0.0003f;
    inline float agc(float s, float target)
    {
        agcEnv += (s * s - agcEnv) * agcA;
        const float g = target / std::sqrt(agcEnv + 1e-7f);
        return s * (g < 0.1f ? 0.1f : (g > 4.f ? 4.f : g));
    }
    const WaveBank* waves = nullptr;

    void init(float sampleRate)
    {
        sr = sampleRate; isr = 1.f / sr;
        agcA = 1.f - std::exp(-1.f / (0.08f * sr));
        ks.assign(8192, 0.f);
        waves = &WaveBank::get();
    }

    void setMachine(int m)
    {
        if (m < 0 || m >= kMachines) m = 0;
        if (m == machine) return;
        machine = m;
        std::fill(ks.begin(), ks.end(), 0.f);
        for (auto& f : form) f.reset();
        nf.reset(); body.reset();
        agcEnv = 0.06f;
    }

    void control(const float* p)
    {
        for (int i = 0; i < 8; ++i) x[i] = clampf(p[i], 0.f, 127.f) / 127.f;
    }

    // called at note start (retrig = envelopes restart; false = legato / slide)
    void noteOn(bool retrig)
    {
        if (!retrig) return;
        if (machine == mFM) { fmEnv = 1.f; }
        if (machine == mSUPER) for (int i = 0; i < 7; ++i) ph[i] = rng.uni();
        if (machine == mPLUCK) { ksExcLeft = 1.f; ksExcLp.y = 0; }
        if (machine == mNOISE) grainAmp = 1.f;
    }

    // --- helpers ---
    static inline float saw(float p, float dt) { return 2.f * p - 1.f - polyBlep(p, dt); }
    static inline float pulse(float p, float dt, float pw)
    {
        float s = p < pw ? 1.f : -1.f;
        s += polyBlep(p, dt);
        float q = p - pw; if (q < 0.f) q += 1.f;
        s -= polyBlep(q, dt);
        return s - (2.f * pw - 1.f); // remove DC
    }
    static inline float tri(float p) { return 1.f - 4.f * std::fabs(p - 0.5f); }
    static inline float shapeOsc(float p, float dt, float shape, float pw)
    {
        if (shape < 0.5f) { const float t = shape * 2.f; return saw(p, dt) * (1.f - t) + pulse(p, dt, pw) * t; }
        const float t = (shape - 0.5f) * 2.f;
        return pulse(p, dt, pw) * (1.f - t) + tri(p) * t;
    }
    static inline float adv(float& p, float dt) { p += dt; if (p >= 1.f) { p -= 1.f; return 1.f; } return 0.f; }
    static inline float octMul(float x) { return std::exp2((float)((int)std::lround(x * 4.f) - 2)); }
    static inline float cents(float x) { return std::exp2((x - 0.5f) * 100.f / 1200.f); }
    inline float crush(float v, float amount)
    {
        if (amount <= 0.001f) return v;
        const int hold = 1 + (int)(amount * amount * 24.f);
        const float bits = std::exp2(12.f - amount * 9.f);
        if (--crushCnt <= 0) { crushCnt = hold; crushHold = std::round(v * bits) / bits; }
        return crushHold;
    }
    float waveRead(float frame, float p, float f) const
    {
        // level with harmonics below Nyquist
        int L = 0;
        const float maxH = 0.5f * sr / std::max(f, 1.f);
        while (L < WaveBank::kLevels - 1 && (float)(1024 >> L) > maxH) ++L;
        const int f0 = std::max(0, std::min((int)frame, WaveBank::kFrames - 1)), f1 = std::min(f0 + 1, WaveBank::kFrames - 1);
        const float ft = clampf(frame - (float)f0, 0.f, 1.f);
        const float pos = p * WaveBank::kSize;
        const int i = (int)pos; const float fr = pos - (float)i;
        const float* a = waves->table(f0, L); const float* b = waves->table(f1, L);
        const float va = a[i] + (a[i + 1] - a[i]) * fr, vb = b[i] + (b[i + 1] - b[i]) * fr;
        return va + (vb - va) * ft;
    }

    // ------------------------------------------------------------------
    // f0 -> f1 : frequency ramp across the block (glide, vibrato)
    void render(float* out, int n, float f0, float f1)
    {
        const float df = (f1 - f0) / (float)n;
        float f = f0;
        switch (machine) {
        case mSAW: {
            const float pw = 0.05f + x[5] * 0.9f, mul2 = octMul(x[4]) * cents(x[2]);
            for (int i = 0; i < n; ++i, f += df) {
                const float dt = f * isr, dt2 = dt * mul2;
                const float o1 = shapeOsc(ph[0], dt, x[0], pw), o2 = shapeOsc(ph[1], dt2, x[1], pw);
                const float sub = pulse(phSub, dt * 0.5f, 0.5f);
                adv(ph[0], dt); adv(ph[1], dt2); adv(phSub, dt * 0.5f);
                out[i] = 0.55f * ((1.f - x[3]) * o1 + x[3] * o2) + x[6] * 0.5f * sub + x[7] * 0.4f * rng.next();
            }
            break;
        }
        case mPULSE: {
            const float pwmHz = expmap(x[1], 0.05f, 12.f), syncR = 1.f + x[3] * 7.f;
            const bool doSync = x[3] > 0.02f;
            for (int i = 0; i < n; ++i, f += df) {
                const float dt = f * isr;
                adv(phLfo, pwmHz * isr);
                const float pw = clampf(0.05f + x[0] * 0.9f + x[2] * 0.45f * std::sin(kTwoPi * phLfo), 0.03f, 0.97f);
                float o;
                if (doSync) {
                    const float dts = dt * syncR;
                    o = pulse(ph[1], dts, pw);
                    if (adv(ph[0], dt) > 0.f) ph[1] = ph[0] * syncR - std::floor(ph[0] * syncR); else adv(ph[1], dts);
                } else {
                    o = pulse(ph[0], dt, pw); adv(ph[0], dt);
                }
                if (x[4] > 0.f) { adv(phAux, dt * 1.5f); o = o * (1.f - x[4]) + o * std::sin(kTwoPi * phAux) * x[4] * 1.4f; }
                o += x[5] * 0.6f * pulse(phSub, dt * 0.5f, 0.5f); adv(phSub, dt * 0.5f);
                o += x[6] * 0.4f * rng.next();
                out[i] = crush(o * 0.45f, x[7]);
            }
            break;
        }
        case mSUPER: {
            static const float off[7] = {0.f, -0.0195f, 0.0199f, -0.0629f, 0.0622f, -0.1100f, 0.1075f};
            const int nv = 3 + (int)std::lround(x[2] * 4.f);
            const float d = x[0] * x[0] * 0.7f + x[0] * 0.15f, om = octMul(x[5]);
            const float center = 1.f - 0.6f * x[1], side = x[1];
            const float norm = 0.5f / (center + side * std::sqrt((float)(nv - 1)));
            for (int i = 0; i < n; ++i, f += df) {
                const float fb = f * om;
                float s = 0.f;
                for (int v = 0; v < nv; ++v) {
                    const float dt = fb * (1.f + off[v] * d) * isr;
                    const float o = x[3] < 0.5f ? saw(ph[v], dt) : pulse(ph[v], dt, 0.5f);
                    s += o * (v == 0 ? center : side);
                    adv(ph[v], dt);
                }
                s *= norm * 1.6f;
                s += x[4] * 0.5f * pulse(phSub, fb * 0.5f * isr, 0.5f); adv(phSub, fb * 0.5f * isr);
                s += x[6] * 0.3f * rng.next();
                out[i] = drive(s, x[7]);
            }
            break;
        }
        case mFM: {
            static const float R[16] = {0.5f, 1.f, 1.5f, 2.f, 2.5f, 3.f, 3.5f, 4.f, 5.f, 6.f, 7.f, 8.f, 9.f, 10.f, 11.f, 12.f};
            const float r1 = R[std::min(15, (int)(x[0] * 16.f))], r2 = R[std::min(15, (int)(x[2] * 16.f))];
            const float i1 = x[1] * x[1] * 10.f, i2 = x[3] * x[3] * 10.f, fb = x[4] * 1.3f;
            const int algo = std::min(3, (int)(x[5] * 4.f));
            fmEnvCoef = decayCoef(expmap(x[6], 0.01f, 5.f), sr);
            const float sus = x[7];
            for (int i = 0; i < n; ++i, f += df) {
                const float e = sus + (1.f - sus) * fmEnv; fmEnv *= fmEnvCoef;
                adv(ph[0], f * isr); adv(ph[1], f * r1 * isr); adv(ph[2], f * r2 * isr);
                float o;
                const float fbv = fb * 0.5f * (fmFb1 + fmFb2);
                if (algo == 0) {        // 3 -> 2 -> 1
                    const float m3 = std::sin(kTwoPi * ph[2] + fbv);
                    const float m2 = std::sin(kTwoPi * ph[1] + i2 * e * m3);
                    o = std::sin(kTwoPi * ph[0] + i1 * e * m2);
                    fmFb2 = fmFb1; fmFb1 = m3;
                } else if (algo == 1) { // (2 + 3) -> 1
                    const float m3 = std::sin(kTwoPi * ph[2] + fbv);
                    const float m2 = std::sin(kTwoPi * ph[1]);
                    o = std::sin(kTwoPi * ph[0] + e * (i1 * m2 + i2 * m3));
                    fmFb2 = fmFb1; fmFb1 = m3;
                } else if (algo == 2) { // 2 -> 1, plus 3 as a second carrier
                    const float m2 = std::sin(kTwoPi * ph[1] + fbv);
                    o = 0.7f * std::sin(kTwoPi * ph[0] + i1 * e * m2) + 0.5f * x[3] * std::sin(kTwoPi * ph[2]);
                    fmFb2 = fmFb1; fmFb1 = m2;
                } else {                // 3 -> (1 and 2), two carriers
                    const float m3 = std::sin(kTwoPi * ph[2] + fbv);
                    o = 0.6f * std::sin(kTwoPi * ph[0] + i2 * e * m3) + 0.6f * std::sin(kTwoPi * ph[1] + i1 * e * m3);
                    fmFb2 = fmFb1; fmFb1 = m3;
                }
                out[i] = o * 0.6f;
            }
            break;
        }
        case mWAVE: {
            const float scanHz = expmap(x[1], 0.03f, 8.f), mul2 = octMul(x[3]) * cents(x[4]);
            const float fold = x[6] * 5.f;
            for (int i = 0; i < n; ++i, f += df) {
                adv(phLfo, scanHz * isr);
                const float pos = clampf(x[0] * 31.f + x[2] * 31.f * (0.5f - 0.5f * std::cos(kTwoPi * phLfo)), 0.f, 31.f);
                const float dt = f * isr, dt2 = dt * mul2;
                float s = waveRead(pos, ph[0], f);
                if (x[5] > 0.f) s = s * (1.f - 0.5f * x[5]) + 0.5f * x[5] * waveRead(pos, ph[1], f * mul2);
                adv(ph[0], dt); adv(ph[1], dt2);
                if (fold > 0.01f) s = std::sin(s * (1.f + fold) * 1.5707963f);
                out[i] = crush(s * 0.55f, x[7]);
            }
            break;
        }
        case mVOX: {
            static const float F[5][3] = {{730, 1090, 2440}, {530, 1840, 2480}, {270, 2290, 3010}, {570, 840, 2410}, {300, 870, 2240}};
            static const float G[3] = {1.f, 0.55f, 0.3f};
            const float v = x[0] * 3.999f; const int vi = (int)v; const float vt = v - vi;
            const float shift = std::exp2((x[1] - 0.5f) * 2.f), q = 2.f + x[3] * 18.f;
            for (int k = 0; k < 3; ++k)
                form[k].setup((F[vi][k] + (F[std::min(vi + 1, 4)][k] - F[vi][k]) * vt) * shift, q, sr);
            glot.setLP(expmap(x[6], 400.f, 14000.f), sr);
            const float vibHz = expmap(x[5], 2.f, 9.f), vibDepth = x[4] * 0.04f;
            for (int i = 0; i < n; ++i, f += df) {
                adv(vibPh, vibHz * isr);
                const float fv = f * (1.f + vibDepth * std::sin(kTwoPi * vibPh));
                const float dt = fv * isr;
                float src = saw(ph[0], dt); adv(ph[0], dt);
                if (x[7] > 0.f) { src = src * (1.f - 0.4f * x[7]) + 0.4f * x[7] * saw(ph[1], dt * 1.007f); adv(ph[1], dt * 1.007f); }
                src = glot.lp(src) + x[2] * 0.6f * rng.next();
                float y = 0.f;
                for (int k = 0; k < 3; ++k) { form[k].tick(src); y += form[k].bp * G[k]; }
                out[i] = agc(y, 0.25f);
            }
            break;
        }
        case mPLUCK: {
            const uint32_t mask = (uint32_t)ks.size() - 1;
            const float t60 = expmap(x[0], 0.1f, 20.f);
            const float damp = x[3] * 0.85f, stif = x[4] * 0.6f;
            ksExcLp.setLP(expmap(x[1], 300.f, 16000.f), sr);
            body.setup(400.f, 1.2f, sr);
            for (int i = 0; i < n; ++i, f += df) {
                const float delay = clampf(sr / std::max(f, 20.f), 2.f, (float)ks.size() - 4.f);
                const float gain = std::pow(10.f, -3.f / (t60 * std::max(f, 20.f)));
                float rp = (float)ksW - delay; if (rp < 0.f) rp += (float)ks.size();
                const uint32_t i0 = (uint32_t)rp & mask, i1 = (i0 + 1) & mask; const float fr = rp - std::floor(rp);
                const float y = ks[i0] + (ks[i1] - ks[i0]) * fr;
                ksLp = y + damp * (ksLp - y);
                float z = ksLp;
                if (stif > 0.f) { const float ap = -stif * z + ksApZ; ksApZ = z + stif * ap; z = ap; }
                float in = z * gain;
                if (ksExcLeft > 0.f) {
                    // one period of excitation: noise / triangle mix, pick position comb
                    const float ph01 = 1.f - ksExcLeft;
                    const float trn = 1.f - 2.f * std::fabs(2.f * ph01 - 1.f);
                    float e = x[5] * rng.next() + (1.f - x[5]) * trn;
                    e *= std::sin(kPi * std::min(1.f, ph01 / std::max(0.05f, x[2])));
                    in += ksExcLp.lp(e) * 0.8f;
                    ksExcLeft -= 1.f / delay;
                }
                ks[ksW] = in; ksW = (ksW + 1) & mask;
                float o = y;
                if (x[6] > 0.f) { body.tick(o); o += body.bp * x[6] * 1.5f; }
                out[i] = drive(o * 0.8f, x[7]);
            }
            break;
        }
        case mNOISE: {
            const float colr = std::log2(expmap(x[0], 80.f, 12000.f));
            const float q = 0.5f + x[1] * 29.f;
            float fc = std::exp2(colr + (std::log2(std::max(f * 2.f, 20.f)) - colr) * x[2]);
            nf.setup(fc, q, sr);
            const float grainP = x[4] * 300.f * isr;
            for (int i = 0; i < n; ++i, f += df) {
                float s = rng.next();
                nf.tick(s);
                s = nf.bp * (1.f - x[6]) + nf.lp * x[6];
                s = agc(s, 0.22f);
                if (x[4] > 0.f) {
                    if (rng.uni() < grainP) grainAmp = 0.3f + 0.7f * rng.uni();
                    grainAmp *= 0.9992f; s *= (1.f - x[4]) + x[4] * grainAmp * 1.6f;
                }
                if (x[5] > 0.f) { adv(phAux, f * isr); s = s * (1.f - 0.6f * x[5]) + x[5] * 0.6f * std::sin(kTwoPi * phAux); }
                out[i] = drive(crush(s, x[3]), x[7]);
            }
            break;
        }
        default:
            std::memset(out, 0, sizeof(float) * n);
            break;
        }
    }
};

} // namespace kn
