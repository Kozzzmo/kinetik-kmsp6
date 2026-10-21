// Kinetik — Standard MIDI File writer (type 1), used to export patterns (identical in every Kinetik DPF repository).
#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

namespace kn {
namespace midi {

struct Note {
    int track = 0;       // index into the track names given to write()
    int channel = 0;     // 0..15
    int note = 60;       // 0..127
    int velocity = 100;  // 1..127
    double start = 0.0;  // in beats (quarter notes)
    double length = 0.25;
};

static constexpr int kPpq = 960;

inline void put32(std::string& s, uint32_t v) { for (int i = 3; i >= 0; --i) s.push_back((char)((v >> (8 * i)) & 0xFF)); }
inline void putVar(std::string& s, uint32_t v)
{
    char b[5]; int n = 0;
    b[n++] = (char)(v & 0x7F);
    while ((v >>= 7) != 0) b[n++] = (char)(0x80 | (v & 0x7F));
    while (n > 0) s.push_back(b[--n]);
}
inline void putChunk(std::string& out, const char* id, const std::string& body)
{
    out.append(id, 4); put32(out, (uint32_t)body.size()); out += body;
}
inline void putMeta(std::string& s, uint32_t delta, uint8_t type, const std::string& data)
{
    putVar(s, delta); s.push_back((char)0xFF); s.push_back((char)type); putVar(s, (uint32_t)data.size()); s += data;
}

// Builds a type-1 file: a tempo track (tempo, 4/4, clip length) then one track per name with its notes.
// lengthBeats = clip length (an end-of-track marker is placed there so the DAW clip loops correctly).
inline std::string build(const std::string& title, double bpm, double lengthBeats,
                         const std::vector<std::string>& trackNames, const std::vector<Note>& notes)
{
    auto tick = [](double beats) { return (uint32_t)std::max(0.0, std::floor(beats * kPpq + 0.5)); };
    const uint32_t endTick = std::max<uint32_t>(1, tick(lengthBeats));
    std::string file;
    std::string hd;
    hd.push_back(0); hd.push_back(1);                                    // format 1
    const int nTracks = 1 + (int)trackNames.size();
    hd.push_back((char)(nTracks >> 8)); hd.push_back((char)(nTracks & 0xFF));
    hd.push_back((char)(kPpq >> 8)); hd.push_back((char)(kPpq & 0xFF));
    putChunk(file, "MThd", hd);

    std::string t0;
    putMeta(t0, 0, 0x03, title);
    const uint32_t us = (uint32_t)std::lround(60000000.0 / std::max(1.0, bpm));
    putMeta(t0, 0, 0x51, std::string{(char)((us >> 16) & 0xFF), (char)((us >> 8) & 0xFF), (char)(us & 0xFF)});
    putMeta(t0, 0, 0x58, std::string{4, 2, 24, 8});
    putMeta(t0, endTick, 0x2F, std::string());
    putChunk(file, "MTrk", t0);

    struct Ev { uint32_t t; int on; int ch, note, vel; };
    for (int tr = 0; tr < (int)trackNames.size(); ++tr) {
        struct Span { uint32_t a, b; int ch, nt, v; };
        std::vector<Span> sp;
        for (const Note& n : notes) {
            if (n.track != tr) continue;
            const uint32_t a = std::min(tick(n.start), endTick - 1);
            const uint32_t b = std::min(std::max(a + 1, tick(n.start + n.length)), endTick);
            sp.push_back({a, b, std::clamp(n.channel, 0, 15), std::clamp(n.note, 0, 127), std::clamp(n.velocity, 1, 127)});
        }
        std::stable_sort(sp.begin(), sp.end(), [](const Span& x, const Span& y) { return x.a < y.a; });
        // a note never overlaps the next one of the same pitch (it would be cut or left hanging by the host)
        for (size_t i = 0; i < sp.size(); ++i)
            for (size_t j = i + 1; j < sp.size(); ++j)
                if (sp[j].ch == sp[i].ch && sp[j].nt == sp[i].nt) {
                    if (sp[i].b > sp[j].a) sp[i].b = sp[j].a;   // same start: the duplicate is dropped below
                    break;
                }
        std::vector<Ev> ev;
        for (const Span& x : sp) {
            if (x.b <= x.a) continue;
            ev.push_back({x.a, 1, x.ch, x.nt, x.v});
            ev.push_back({x.b, 0, x.ch, x.nt, 0});
        }
        // note-offs before note-ons at the same tick, so repeated notes retrigger
        std::stable_sort(ev.begin(), ev.end(), [](const Ev& x, const Ev& y) { return x.t != y.t ? x.t < y.t : x.on < y.on; });
        std::string s;
        putMeta(s, 0, 0x03, trackNames[tr]);
        uint32_t last = 0;
        for (const Ev& e : ev) {
            putVar(s, e.t - last); last = e.t;
            s.push_back((char)((e.on ? 0x90 : 0x80) | e.ch)); s.push_back((char)e.note); s.push_back((char)(e.on ? e.vel : 64));
        }
        putMeta(s, endTick - std::min(last, endTick), 0x2F, std::string());
        putChunk(file, "MTrk", s);
    }
    return file;
}

} // namespace midi
} // namespace kn
