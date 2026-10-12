// Kinetik — master effects: echo (5 models), reverb (5 models), EQ, compressor, drive, safety limiter.
// Shared by KDSP-16 and KMSP-6: keep this file identical in both repositories.
// Uses the global parameter layout of Params.hpp (gECHO_TIME ... gREV_MOD), identical in both products.
#pragma once
#include "Params.hpp"
#include "Dsp.hpp"
#include <vector>
#include <algorithm>
#include <cstring>

namespace kn {

inline int clampMI(int v, int n) { return v < 0 ? 0 : (v >= n ? n - 1 : v); }

enum EchoModel { emDigital, emTape, emBbd, emPingPong, emLofi, kEchoModels };
static const char* const kEchoModelNames[kEchoModels] = {"DIGITAL", "TAPE", "BBD", "PING-PONG", "LO-FI"};
enum ReverbModel { rmRoom, rmPlate, rmHall, rmSpring, rmShimmer, kReverbModels };
static const char* const kReverbModelNames[kReverbModels] = {"ROOM", "PLATE", "HALL", "SPRING", "SHIMMER"};
inline int echoModelIndex(int v) { return clampMI(v * kEchoModels / 128, kEchoModels); }
inline int reverbModelIndex(int v) { return clampMI(v * kReverbModels / 128, kReverbModels); }
inline int echoModelValue(int i) { return (i * 128 + kEchoModels - 1) / kEchoModels; }
inline int reverbModelValue(int i) { return (i * 128 + kReverbModels - 1) / kReverbModels; }

// return level of the echo / reverb: 0 .. about +10 dB above the old maximum (square law, 64 = unity-ish)
inline float fxReturnGain(float v127) { const float x = clampf(v127 / 127.f, 0.f, 1.f); return 3.2f * x * x; }

// one-line description of each model (help texts, LCD)
static const char* const kEchoModelInfo[kEchoModels] = {
    "clean digital repeats",
    "tape machine: wow, flutter, warm saturation",
    "bucket-brigade: darker and grainier as the time grows",
    "repeats bounce between left and right",
    "crunchy 8-bit repeats at a low sample rate",
};
static const char* const kReverbModelInfo[kReverbModels] = {
    "small room, short and close",
    "metal plate: dense and bright",
    "large hall, smooth and wide",
    "spring tank: boingy and metallic",
    "hall with octave-up shimmer",
};

class MasterFx {
public:
    void init(float sampleRate)
    {
        sr = sampleRate;
        const size_t echoLen = (size_t)(sr * 4.4f) + 8;
        echoL.assign(echoLen, 0.f); echoR.assign(echoLen, 0.f);
        for (int i = 0; i < 8; ++i) {
            const int maxLen = (int)(kRevMs[i] * 0.001f * sr * 1.6f) + 64;
            revLine[i].assign((size_t)maxLen, 0.f);
        }
        for (int i = 0; i < kDiff; ++i) diff[i].assign((size_t)(kDiffMs[i] * 0.001f * sr * 1.2f) + 4, 0.f);
        for (int i = 0; i < kSpring; ++i) spring[i].assign((size_t)(kSpringMs[i] * 0.001f * sr) + 4, 0.f);
        shim.assign(4096, 0.f);
        preBuf.assign((size_t)(sr * 0.25f) + 4, 0.f);
        revModel = -1;
        reset();
    }

    void reset()
    {
        std::fill(echoL.begin(), echoL.end(), 0.f); std::fill(echoR.begin(), echoR.end(), 0.f);
        clearReverb();
        std::fill(preBuf.begin(), preBuf.end(), 0.f);
        echoW = 0; preW = 0; cmpEnv = 0.f; gateEnv = 0.f; gateGain = 1.f; gateHold = 0;
        echoLpL = echoLpR = echoHpL = echoHpR = echoLp2L = echoLp2R = 0.f;
        holdL = holdR = 0.f; holdCnt = 0; wowPh = flutPh = modPh = 0.f;
    }

    // g: global parameters; main/send buffers are mixed into out (main bus + echo + reverb, then master chain)
    void process(const float* g, float bpm, const float* mainL, const float* mainR,
                 const float* sendDL, const float* sendDR, const float* sendRL, const float* sendRR,
                 float* outL, float* outR, uint32_t n)
    {
        // ---- echo ----
        const int em = echoModelIndex((int)g[gECHO_MODEL]);
        const int div = echoDivIndex((int)g[gECHO_TIME]);
        float targetDelay = (float)(kEchoBeats[div] * 60.0 / std::max(20.f, bpm) * sr);
        targetDelay = clampf(targetDelay, 8.f, (float)echoL.size() - 64.f);
        const float fb = std::min(1.02f, g[gECHO_FB] / 127.f * 1.05f);
        float lpHz = expmap(g[gECHO_LPF] / 127.f, 500.f, 20000.f);
        if (em == emTape) lpHz = std::min(lpHz, 9000.f);
        if (em == emBbd) lpHz = std::min(lpHz, clampf(4096.f / (2.f * echoDelay / sr) / 2.4f, 1200.f, 16000.f));
        if (em == emLofi) lpHz = std::min(lpHz, 6000.f);
        const float lpA = std::exp(-kTwoPi * lpHz / sr);
        const float hpA = std::exp(-kTwoPi * expmap(g[gECHO_HPF] / 127.f, 20.f, 3000.f) / sr);
        const float w = em == emPingPong ? 1.f : g[gECHO_WIDTH] / 127.f;
        const float eLev = fxReturnGain(g[gECHO_LEV]);
        const float emod = g[gECHO_MOD] / 127.f;
        const float wowInc = 0.55f / sr, flutInc = (em == emTape ? 6.3f : 0.45f) / sr;
        const float wowDepth = em == emTape ? targetDelay * 0.004f * (0.25f + emod) + 6.f * emod
                             : em == emBbd ? 40.f * emod + targetDelay * 0.0015f * emod
                             : em == emDigital ? 12.f * emod : 4.f * emod;
        const float flutDepth = em == emTape ? targetDelay * 0.0006f * (0.3f + emod) : 0.f;
        const float sat = em == emTape ? 1.8f : (em == emBbd ? 1.3f : 1.f);
        const int holdN = em == emLofi ? 3 + (int)(emod * 5.f) : 1;
        const float bits = em == emLofi ? 64.f - 48.f * emod : 0.f;

        // ---- reverb ----
        const int rm = reverbModelIndex((int)g[gREV_MODEL]);
        if (rm != revModel) setReverbModel(rm);
        const float rmod = g[gREV_MOD] / 127.f;
        const float rt = expmap(g[gREV_SIZE] / 127.f, 0.2f, 10.f) * kRevRt[rm];
        float damp = 0.05f + g[gREV_DAMP] / 127.f * 0.85f;
        if (rm == rmPlate) damp *= 0.55f;
        if (rm == rmSpring) damp = 0.25f + damp * 0.6f;
        const float rHpA = std::exp(-kTwoPi * expmap(g[gREV_HPF] / 127.f, 20.f, 2000.f) / sr);
        const uint32_t psz = (uint32_t)preBuf.size();
        const uint32_t pre = std::min<uint32_t>(psz - 1, (uint32_t)(g[gREV_PRE] / 127.f * 0.2f * sr));
        const float rLev = fxReturnGain(g[gREV_LEV]) * kRevOut[rm];
        const bool gate = g[gREV_GATE] > 0.5f;
        const uint32_t gateHoldS = (uint32_t)(expmap(g[gREV_GATE] / 127.f, 0.03f, 1.0f) * sr);
        float revG[8];
        for (int i = 0; i < 8; ++i) revG[i] = std::pow(10.f, -3.f * (float)revLen[i] / (rt * sr));
        const float lineMod = (rm == rmHall || rm == rmShimmer ? 10.f : (rm == rmPlate ? 4.f : (rm == rmSpring ? 2.f : 3.f))) * (0.3f + rmod) * sr / 48000.f;
        const float shimMix = rm == rmShimmer ? 0.18f + 0.3f * rmod : 0.f;
        const float springLp = std::exp(-kTwoPi * 4200.f / sr), springHp = std::exp(-kTwoPi * 180.f / sr);

        // ---- master ----
        eqLowL.setShelf(false, 120.f, (g[gEQ_LOW] - 64.f) / 64.f * 15.f, sr); copyCoef(eqLowR, eqLowL);
        eqHighL.setShelf(true, 7000.f, (g[gEQ_HIGH] - 64.f) / 64.f * 15.f, sr); copyCoef(eqHighR, eqHighL);
        const float thrDb = -40.f + g[gCMP_THR] / 127.f * 40.f;
        const float ratio = 1.f + g[gCMP_RAT] / 127.f * 19.f;
        const float atk = std::exp(-1.f / (expmap(g[gCMP_ATK] / 127.f, 0.0001f, 0.08f) * sr));
        const float rel = std::exp(-1.f / (expmap(g[gCMP_REL] / 127.f, 0.02f, 1.0f) * sr));
        const float makeup = std::pow(10.f, (-thrDb * (1.f - 1.f / ratio) * 0.4f) / 20.f);
        const float drv = g[gDRIVE] / 127.f;
        const float mv = g[gVOLUME] / 127.f; const float vol = mv * mv * 1.2f;
        const uint32_t esz = (uint32_t)echoL.size();

        for (uint32_t i = 0; i < n; ++i) {
            // ---------------- echo
            echoDelay += (targetDelay - echoDelay) * 0.0005f;
            wowPh += wowInc; if (wowPh >= 1.f) wowPh -= 1.f;
            flutPh += flutInc; if (flutPh >= 1.f) flutPh -= 1.f;
            const float d = echoDelay + wowDepth * (0.5f + 0.5f * std::sin(kTwoPi * wowPh)) + flutDepth * std::sin(kTwoPi * flutPh);
            const float dR = em == emBbd ? echoDelay + wowDepth * (0.5f + 0.5f * std::cos(kTwoPi * wowPh)) : d;
            const float dl = readFrac(echoL, esz, (float)echoW - d);
            const float dr = readFrac(echoR, esz, (float)echoW - dR);
            float inL, inR;
            if (em == emPingPong) {
                inL = 0.5f * (sendDL[i] + sendDR[i]) + fb * dr;   // the input enters on the left only
                inR = fb * dl;
            } else {
                const float sm = 0.5f * (sendDL[i] + sendDR[i]);
                inL = sendDL[i] * (1.f - w) + sm * w + fb * (w * dr + (1.f - w) * dl);
                inR = sendDR[i] * (1.f - w) + fb * (w * dl + (1.f - w) * dr);
            }
            echoLpL = inL + lpA * (echoLpL - inL); echoLpR = inR + lpA * (echoLpR - inR);
            float yl = echoLpL, yr = echoLpR;
            if (em == emBbd || em == emTape) { // second pole: steeper roll-off
                echoLp2L = yl + lpA * (echoLp2L - yl); echoLp2R = yr + lpA * (echoLp2R - yr);
                yl = echoLp2L; yr = echoLp2R;
            }
            echoHpL = yl + hpA * (echoHpL - yl); echoHpR = yr + hpA * (echoHpR - yr);
            yl -= echoHpL; yr -= echoHpR;
            if (holdN > 1) {
                if (--holdCnt <= 0) { holdCnt = holdN; holdL = yl; holdR = yr; }
                yl = std::round(holdL * bits) / bits; yr = std::round(holdR * bits) / bits;
            }
            echoL[echoW] = std::tanh(yl * sat) / sat; echoR[echoW] = std::tanh(yr * sat) / sat;
            echoW = (echoW + 1) % esz;

            // ---------------- reverb (8-line FDN, Householder feedback)
            float rin = 0.5f * (sendRL[i] + sendRR[i]);
            revHpZ = rin + rHpA * (revHpZ - rin); rin = rin - revHpZ;
            preBuf[preW] = rin;
            float x = preBuf[(preW + psz - pre) % psz];
            preW = (preW + 1) % psz;
            if (rm == rmShimmer) x += shimOut * shimMix;
            // input diffusion (plate: 4 allpasses, others 2, room 1)
            for (int k = 0; k < diffCount; ++k) x = allpass(diff[k], diffPos[k], diffLen[k], x, diffG);
            if (rm == rmSpring) {
                for (int k = 0; k < kSpring; ++k) x = allpass(spring[k], springPos[k], springLen[k], x, 0.62f);
                springLpZ = x + springLp * (springLpZ - x); x = springLpZ;
                springHpZ = x + springHp * (springHpZ - x); x -= springHpZ;
            }
            modPh += 0.37f / sr; if (modPh >= 1.f) modPh -= 1.f;
            float o[8], sum = 0.f;
            for (int k = 0; k < 8; ++k) {
                const float m = lineMod * std::sin(kTwoPi * (modPh + k * 0.125f));
                o[k] = readFrac(revLine[k], (uint32_t)revLine[k].size(), (float)revW[k] - (float)revLen[k] - m);
                sum += o[k];
            }
            const float hh = sum * 0.25f;
            float outRL = 0.f, outRR = 0.f;
            for (int k = 0; k < 8; ++k) {
                float v = (o[k] - hh) * revG[k];
                revLp[k] = v + damp * (revLp[k] - v);
                v = revLp[k] + x * ((k & 1) ? 0.5f : -0.5f);
                revLine[k][revW[k]] = v;
                if (++revW[k] >= (uint32_t)revLine[k].size()) revW[k] = 0;
                if (k < 4) outRL += o[k] * ((k & 1) ? -1.f : 1.f); else outRR += o[k] * ((k & 1) ? 1.f : -1.f);
            }
            if (rm == rmShimmer) shimOut = pitchUp(0.5f * (outRL + outRR));
            if (gate) {
                const float a = std::fabs(rin);
                gateEnv = std::max(a, gateEnv * 0.9995f);
                if (gateEnv > 0.003f) gateHold = gateHoldS; else if (gateHold > 0) --gateHold;
                const float tgt = gateHold > 0 ? 1.f : 0.f;
                gateGain += (tgt - gateGain) * (tgt > gateGain ? 0.02f : 0.004f);
            } else gateGain = 1.f;

            float l = mainL[i] + dl * eLev + outRL * 0.35f * rLev * gateGain;
            float r = mainR[i] + dr * eLev + outRR * 0.35f * rLev * gateGain;
            l = eqHighL.tick(eqLowL.tick(l)); r = eqHighR.tick(eqLowR.tick(r));
            // compressor (linked peak)
            const float pk = std::max(std::fabs(l), std::fabs(r));
            cmpEnv = pk > cmpEnv ? pk + atk * (cmpEnv - pk) : pk + rel * (cmpEnv - pk);
            if (ratio > 1.01f && thrDb < -0.1f) {
                const float lv = 20.f * std::log10(cmpEnv + 1e-9f);
                const float over = lv - thrDb;
                float gain = makeup;
                if (over > 0.f) gain *= std::pow(10.f, -over * (1.f - 1.f / ratio) / 20.f);
                l *= gain; r *= gain;
            }
            l = drive(l, drv) * vol; r = drive(r, drv) * vol;
            outL[i] = safety(l); outR[i] = safety(r);
        }
    }

    // transparent below 0.9, smooth ceiling at 1.0
    static inline float safety(float x)
    {
        const float a = std::fabs(x);
        if (a <= 0.9f) return x;
        const float y = 0.9f + 0.1f * std::tanh((a - 0.9f) * 10.f);
        return x < 0.f ? -y : y;
    }

private:
    static constexpr int kDiff = 4, kSpring = 6;
    static constexpr float kRevMs[8] = {29.7f, 37.1f, 41.1f, 43.7f, 53.3f, 59.9f, 67.7f, 73.1f};
    static constexpr float kDiffMs[kDiff] = {4.77f, 3.59f, 12.73f, 9.31f};
    static constexpr float kSpringMs[kSpring] = {1.13f, 1.71f, 2.39f, 3.07f, 3.83f, 4.91f};
    // per model: line length scale, decay scale, output gain
    static constexpr float kRevScale[kReverbModels] = {0.45f, 0.8f, 1.3f, 0.38f, 1.4f};
    static constexpr float kRevRt[kReverbModels] = {0.45f, 0.9f, 1.f, 0.7f, 1.25f};
    static constexpr float kRevOut[kReverbModels] = {1.25f, 1.f, 1.f, 1.2f, 0.9f};

    static inline float readFrac(const std::vector<float>& b, uint32_t size, float pos)
    {
        while (pos < 0.f) pos += (float)size;
        const uint32_t i0 = (uint32_t)pos % size, i1 = (i0 + 1) % size;
        const float fr = pos - std::floor(pos);
        return b[i0] + (b[i1] - b[i0]) * fr;
    }
    static inline float allpass(std::vector<float>& b, uint32_t& pos, uint32_t len, float x, float gcoef)
    {
        const float d = b[pos];
        const float v = x + gcoef * d;
        b[pos] = v;
        if (++pos >= len) pos = 0;
        return d - gcoef * v;
    }
    // octave up: two crossfaded taps sweeping a 2048-sample window
    float pitchUp(float x)
    {
        const uint32_t sz = (uint32_t)shim.size();
        shim[shimW] = x;
        shimW = (shimW + 1) % sz;
        constexpr float D = 2048.f;
        shimD -= 1.f; if (shimD < 0.f) shimD += D;           // ratio 2: the delay shrinks one sample per sample
        float d2 = shimD + D * 0.5f; if (d2 >= D) d2 -= D;
        const float a = readFrac(shim, sz, (float)shimW - 1.f - shimD), b = readFrac(shim, sz, (float)shimW - 1.f - d2);
        const float wa = 1.f - std::fabs(shimD / D * 2.f - 1.f), wb = 1.f - std::fabs(d2 / D * 2.f - 1.f);
        return a * wa + b * wb;
    }
    void setReverbModel(int rm)
    {
        revModel = rm;
        for (int i = 0; i < 8; ++i)
            revLen[i] = std::min<uint32_t>((uint32_t)revLine[i].size() - 32, (uint32_t)(kRevMs[i] * 0.001f * sr * kRevScale[rm]));
        diffCount = rm == rmPlate ? 4 : (rm == rmRoom ? 1 : 2);
        diffG = rm == rmPlate ? 0.7f : 0.6f;
        const float ds = rm == rmRoom ? 0.6f : 1.f;
        for (int i = 0; i < kDiff; ++i) diffLen[i] = std::max<uint32_t>(2, (uint32_t)(kDiffMs[i] * 0.001f * sr * ds));
        for (int i = 0; i < kSpring; ++i) springLen[i] = std::max<uint32_t>(2, (uint32_t)(kSpringMs[i] * 0.001f * sr));
        clearReverb();
    }
    void clearReverb()
    {
        for (int i = 0; i < 8; ++i) { std::fill(revLine[i].begin(), revLine[i].end(), 0.f); revW[i] = 0; revLp[i] = 0.f; }
        for (int i = 0; i < kDiff; ++i) { std::fill(diff[i].begin(), diff[i].end(), 0.f); diffPos[i] = 0; }
        for (int i = 0; i < kSpring; ++i) { std::fill(spring[i].begin(), spring[i].end(), 0.f); springPos[i] = 0; }
        std::fill(shim.begin(), shim.end(), 0.f); shimW = 0; shimD = 0.f; shimOut = 0.f;
        revHpZ = springLpZ = springHpZ = 0.f;
    }
    static void copyCoef(Biquad& dst, const Biquad& src)
    {
        dst.b0 = src.b0; dst.b1 = src.b1; dst.b2 = src.b2; dst.a1 = src.a1; dst.a2 = src.a2;
    }

    float sr = 48000.f;
    std::vector<float> echoL, echoR; uint32_t echoW = 0; float echoDelay = 1000.f;
    float echoLpL = 0, echoLpR = 0, echoHpL = 0, echoHpR = 0, echoLp2L = 0, echoLp2R = 0;
    float holdL = 0, holdR = 0; int holdCnt = 0; float wowPh = 0, flutPh = 0, modPh = 0;
    std::vector<float> revLine[8]; uint32_t revLen[8] = {}; uint32_t revW[8] = {}; float revLp[8] = {};
    std::vector<float> diff[kDiff]; uint32_t diffLen[kDiff] = {}, diffPos[kDiff] = {}; int diffCount = 2; float diffG = 0.6f;
    std::vector<float> spring[kSpring]; uint32_t springLen[kSpring] = {}, springPos[kSpring] = {};
    float springLpZ = 0, springHpZ = 0;
    std::vector<float> shim; uint32_t shimW = 0; float shimD = 0, shimOut = 0;
    std::vector<float> preBuf; uint32_t preW = 0; float revHpZ = 0, gateEnv = 0, gateGain = 1; uint32_t gateHold = 0;
    int revModel = -1;
    Biquad eqLowL, eqLowR, eqHighL, eqHighR;
    float cmpEnv = 0;
};

} // namespace kn
