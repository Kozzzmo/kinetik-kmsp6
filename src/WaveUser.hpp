// Kinetik KMSP-6 — user wavetables (one per track): import from audio, plugin state text, built tables.
// State lines (key "waves" = all tracks, key "wav" = one command from the UI):
//   "w<track> <name>\t<frame length>\t<base64 of the losslessly packed 16-bit PCM (Sample.hpp)>"
//   "w<track>" alone = remove the track's table.
#pragma once
#include "Wavetable.hpp"
#include "Sample.hpp"

namespace kn {

struct UserWave {
    std::string name;
    int frameLen = 2048;
    std::vector<float> pcm;     // frames x frameLen, mono
    WaveSet set;                // built mip levels (fromFrames)
    int frames() const { return frameLen > 0 ? (int)(pcm.size() / (size_t)frameLen) : 0; }
};
using UserWavePtr = std::shared_ptr<const UserWave>;

namespace wav {

static constexpr int kMaxFrameLen = 4096;

// frame length of a WAV used as a wavetable: 2048 if it divides the file (the common format), else 1024 / 4096 / 512 /
// 256; a file shorter than 2048 samples or of another length = one single cycle (any length)
inline int guessFrameLen(size_t n)
{
    for (int L : {2048, 1024, 4096, 512, 256})
        if (n >= (size_t)L && n % (size_t)L == 0) return L;
    return (int)std::min<size_t>(n, (size_t)kMaxFrameLen);
}

// builds a user table from mono PCM; nullptr if unusable. Keeps at most 32 frames of the source (picked evenly).
inline UserWavePtr make(const std::string& name, const float* pcm, size_t n, int frameLen = 0)
{
    if (!pcm || n < 16) return nullptr;
    if (frameLen <= 0) frameLen = guessFrameLen(n);
    if (frameLen < 16 || frameLen > kMaxFrameLen) return nullptr;
    int nFrames = (int)(n / (size_t)frameLen);
    if (nFrames < 1) return nullptr;
    auto u = std::make_shared<UserWave>();
    u->name = smp::cleanSampleName(name);
    u->frameLen = frameLen;
    const int keep = std::min(nFrames, WaveSet::kMaxFrames);
    u->pcm.resize((size_t)keep * frameLen);
    for (int f = 0; f < keep; ++f) {
        const int src = keep == 1 ? 0 : (int)std::lround((double)f * (nFrames - 1) / (keep - 1));
        std::copy(pcm + (size_t)src * frameLen, pcm + (size_t)(src + 1) * frameLen, u->pcm.begin() + (size_t)f * frameLen);
    }
    for (float& v : u->pcm) if (!std::isfinite(v)) v = 0.f;
    if (!u->set.fromFrames(u->pcm.data(), keep, frameLen)) return nullptr;
    u->set.name = u->name;
    return u;
}

inline std::string encodeLine(int t, const UserWave& u)
{
    std::vector<int32_t> x(u.pcm.size());
    for (size_t i = 0; i < u.pcm.size(); ++i) x[i] = smp::toPcm16(u.pcm[i]);
    const std::vector<uint8_t> b = smp::pack(x);
    return "w" + std::to_string(t) + " " + u.name + "\t" + std::to_string(u.frameLen) + "\t" + smp::base64(b.data(), b.size());
}

template <class Arr>
std::string encodeAll(const Arr& w, int tracks)
{
    std::string o;
    for (int t = 0; t < tracks; ++t) if (w[t]) o += encodeLine(t, *w[t]) + "\n";
    return o;
}

// calls fn(track, UserWavePtr) for each "w<track> ..." line (nullptr = remove, or unreadable)
template <class Fn>
void decodeAll(const char* text, Fn fn)
{
    const char* s = text;
    while (s && *s) {
        const char* e = std::strchr(s, '\n');
        const size_t len = e ? (size_t)(e - s) : std::strlen(s);
        if (len > 1 && *s == 'w' && s[1] >= '0' && s[1] <= '9') {
            char* end = nullptr;
            const long t = std::strtol(s + 1, &end, 10);
            if (end && (size_t)(end - s) <= len) {
                const char* rest = end; while (rest < s + len && *rest == ' ') ++rest;
                const size_t rl = len - (size_t)(rest - s);
                UserWavePtr u;
                if (rl > 0) {
                    const char* t1 = (const char*)std::memchr(rest, '\t', rl);
                    const char* t2 = t1 ? (const char*)std::memchr(t1 + 1, '\t', rl - (size_t)(t1 + 1 - rest)) : nullptr;
                    if (t2) {
                        const int fl = std::atoi(std::string(t1 + 1, t2).c_str());
                        const std::vector<uint8_t> b = smp::unbase64(t2 + 1, rl - (size_t)(t2 + 1 - rest));
                        std::vector<int32_t> x;
                        if (fl >= 16 && fl <= kMaxFrameLen && smp::unpack(b.data(), b.size(), x) && x.size() >= (size_t)fl
                            && x.size() <= (size_t)fl * WaveSet::kMaxFrames) {
                            std::vector<float> f(x.size());
                            for (size_t i = 0; i < x.size(); ++i) f[i] = (float)x[i] / 32767.f;
                            u = make(std::string(rest, t1), f.data(), f.size(), fl);
                        }
                    }
                }
                fn((int)t, u);
            }
        }
        s = e ? e + 1 : nullptr;
    }
}

} // namespace wav
} // namespace kn
