// Kinetik KMSP-6 — DPF plugin glue
#include "DistrhoPlugin.hpp"
#include "Engine.hpp"
#include "WaveUser.hpp"
#include "MonoShared.hpp"
#include "License.hpp"
#include <atomic>
#include <cmath>
#include <mutex>
#include <memory>

START_NAMESPACE_DISTRHO

using namespace kn;

static String paramSymbol(int track, int p)
{
    static const char* fx[48] = {"atk", "hold", "dec", "sus", "rel", "vol", "pan", "port",
                                 "cutoff", "reso", "ftype", "lowcut", "fatk", "fdec", "fenv", "keytrk",
                                 "amd", "amf", "eqf", "eqg", "srr", "dist", "del", "rev",
                                 "l1spd", "l1dep", "l1shp", "l1typ", "l1dst", "l1trk",
                                 "l2spd", "l2dep", "l2shp", "l2typ", "l2dst", "l2trk",
                                 "l3spd", "l3dep", "l3shp", "l3typ", "l3dst", "l3trk",
                                 "l1sync", "l1phase", "l2sync", "l2phase", "l3sync", "l3phase"};
    static_assert(sizeof(fx) / sizeof(fx[0]) == kTrackParams - 8, "symbol table must cover every track parameter");
    char buf[32];
    if (p < 8) std::snprintf(buf, sizeof(buf), "t%d_syn%d", track + 1, p + 1);
    else std::snprintf(buf, sizeof(buf), "t%d_%s", track + 1, fx[p - 8]);
    return String(buf);
}

static constexpr uint32_t kKitBit = 1u << 16;

class MonoPlugin : public Plugin, public MonoDirect
{
public:
    MonoPlugin()
        : Plugin(kTotalParams, 0, 5)
    {
        engine.reset(new Engine());
        master.reset(new Bank());
        master->initDefault();
        engine->bank = *master;
        engine->init(getSampleRate());
        for (int i = 0; i < kTotalParams; ++i) { uiVal[i].store(0.f); uiHold[i].store(0); }
        notes.reserve(512);
        pending.reserve(256);
        licensed.store(lic::findInstalled(kProductCode).valid);
    }

    // ---- MonoDirect (UI thread) ----
    bool popRecorded(int& pattern, int& track, int& step, int& note, bool& accent, int& gate) override
    {
        RecEvent e;
        if (!engine->popRec(e)) return false;
        pattern = e.pattern; track = e.track; step = e.step; note = e.note; accent = e.accent; gate = e.gate;
        return true;
    }
    uint32_t heldMask() const override { return engine->outGate; }
    int currentNote(int track) const override { return engine->outNote[track < 0 ? 0 : (track >= kTracks ? kTracks - 1 : track)]; }
    void setRecording(bool on) override { recFlag.store(on); }
    void setClick(bool on) override { clickFlag.store(on); }
    void setSelectedTrack(int t) override { selTrack.store(clampi(t, 0, kTracks - 1)); }
    void playNote(int track, int note, int velocity, bool on) override
    {
        if (track < 0 || track >= kTracks) return;
        std::lock_guard<std::mutex> lk(masterMutex);
        if (pending.size() < 256) pending.push_back({0, track, velocity / 100.f, clampi(note, 0, 127), on});
    }
    void panic() override { panicFlag.store(true); }

protected:
    const char* getLabel() const override { return "KinetikKMSP6"; }
    const char* getDescription() const override { return "8-track monophonic synthesizer with note sequencer, arpeggiator and parameter locks"; }
    const char* getMaker() const override { return "Kinetik"; }
    const char* getLicense() const override { return "Proprietary"; }
    uint32_t getVersion() const override { return d_version(KN_VER_MAJOR, KN_VER_MINOR, KN_VER_MICRO); }

    void initParameter(uint32_t index, Parameter& p) override
    {
        p.hints = kParameterIsAutomatable;
        int t = 0, k = 0;
        if (tpOf((int)index, t, k)) {   // track parameters (tracks 7-8 come after the globals)
            char name[48];
            p.symbol = paramSymbol(t, k);
            p.ranges.min = 0; p.ranges.max = 127; p.ranges.def = trackParamDefault(t, k);
            static const char* sec[5] = {"Amp", "Filter", "FX", "", ""};
            if (k < 8) std::snprintf(name, sizeof(name), "T%d Synth %d", t + 1, k + 1);
            else if (k >= tpLFOX) std::snprintf(name, sizeof(name), "T%d LFO%d %s", t + 1, (k - tpLFOX) / 2 + 1, kFxNames[k - 8]);
            else if (k >= tpLFO1) std::snprintf(name, sizeof(name), "T%d LFO%d %s", t + 1, (k - tpLFO1) / 6 + 1, kFxNames[k - 8]);
            else std::snprintf(name, sizeof(name), "T%d %s %s", t + 1, sec[(k - 8) / 8], kFxNames[k - 8]);
            p.name = name;
            p.shortName = name;
            return;
        }
        const int g = (int)index - kGlobalBase;
        switch (g) {
        case gPATTERN:
            p.name = "Pattern"; p.symbol = "pattern"; p.ranges.min = 0; p.ranges.max = kPatterns - 1; p.ranges.def = 0;
            p.hints |= kParameterIsInteger; return;
        case gPLAY:
            p.name = "Internal Play"; p.symbol = "play"; p.ranges.min = 0; p.ranges.max = 1; p.ranges.def = 0;
            p.hints |= kParameterIsBoolean | kParameterIsInteger; return;
        case gTEMPO:
            p.name = "Internal Tempo"; p.symbol = "tempo"; p.ranges.min = 40; p.ranges.max = 240; p.ranges.def = 120;
            p.unit = "BPM"; return;
        case gOUT_STEP:
            p.name = "Play Step"; p.symbol = "out_step"; p.hints = kParameterIsOutput | kParameterIsInteger;
            p.ranges.min = -1; p.ranges.max = 1048575; p.ranges.def = -1; return;
        case gOUT_LEDS:
            p.name = "Trig LEDs"; p.symbol = "out_leds"; p.hints = kParameterIsOutput | kParameterIsInteger;
            p.ranges.min = 0; p.ranges.max = 65535; p.ranges.def = 0; return;
        case gOUT_PATTERN:
            p.name = "Playing Pattern"; p.symbol = "out_pattern"; p.hints = kParameterIsOutput | kParameterIsInteger;
            p.ranges.min = 0; p.ranges.max = kPatterns - 1; p.ranges.def = 0; return;
        default: break;
        }
        if ((g >= gCHA_SEND && g < gCHA_SEND + kLegacyTracks) || (g >= gCHA_SEND2 && g < gCHA_SEND2 + kTracks - kLegacyTracks)) {   // CHAOS send of each track
            static char names[kTracks][32], symbols[kTracks][16];
            const int t = g >= gCHA_SEND2 ? kLegacyTracks + g - gCHA_SEND2 : g - gCHA_SEND;
            std::snprintf(names[t], sizeof(names[t]), "T%02d Chaos Send", t + 1);
            std::snprintf(symbols[t], sizeof(symbols[t]), "t%02d_chaos", t + 1);
            p.name = names[t]; p.symbol = symbols[t];
            p.ranges.min = 0; p.ranges.max = 127; p.ranges.def = 127;
            return;
        }
        if (g == gMF_ON) {
            p.name = "Filter On"; p.symbol = "mfilter_on"; p.ranges.min = 0; p.ranges.max = 1; p.ranges.def = 1;
            p.hints |= kParameterIsBoolean | kParameterIsInteger; return;
        }
        if (g == gMORPH) {
            // the MORPH feature was removed (0.15 / 0.11); the parameter stays so the indices after it do not move
            p.name = "Unused"; p.symbol = "morph"; p.ranges.min = 0; p.ranges.max = 127; p.ranges.def = 0;
            p.hints = kParameterIsHidden; return;
        }
        if (g == gECHO_RTIME) { p.name = "Echo R Time"; p.symbol = "echo_rtime"; p.ranges.min = 0; p.ranges.max = 127; p.ranges.def = 64; return; }
        if (g == gECHO_DRIVE) { p.name = "Echo Drive"; p.symbol = "echo_drive"; p.ranges.min = 0; p.ranges.max = 127; p.ranges.def = 0; return; }
        if (g == gECHO_DUCK) { p.name = "Echo Duck"; p.symbol = "echo_duck"; p.ranges.min = 0; p.ranges.max = 127; p.ranges.def = 0; return; }
        if (g == gECHO_FREEZE) {
            p.name = "Echo Freeze"; p.symbol = "echo_freeze"; p.ranges.min = 0; p.ranges.max = 1; p.ranges.def = 0;
            p.hints |= kParameterIsBoolean | kParameterIsInteger; return;
        }
        if ((g >= gCHO_SEND && g < gCHO_SEND + kLegacyTracks) || (g >= gCHO_SEND2 && g < gCHO_SEND2 + kTracks - kLegacyTracks)) {   // chorus send of each track
            static char names[kTracks][32], symbols[kTracks][16];
            const int t = g >= gCHO_SEND2 ? kLegacyTracks + g - gCHO_SEND2 : g - gCHO_SEND;
            std::snprintf(names[t], sizeof(names[t]), "T%02d Chorus Send", t + 1);
            std::snprintf(symbols[t], sizeof(symbols[t]), "t%02d_cho", t + 1);
            p.name = names[t]; p.symbol = symbols[t];
            p.ranges.min = 0; p.ranges.max = 127; p.ranges.def = 0;
            return;
        }
        static const char* section[] = {"Echo","Echo","Echo","Echo","Echo","Echo","Reverb","Reverb","Reverb","Reverb",
                                          "Reverb","Reverb","Master","Master","Comp","Comp","Comp","Comp","Master","Master",
                                          "Echo","Echo","Reverb","Reverb","Filter","Filter","Filter",
                                          "Chorus","Chorus","Chorus","Chorus","Chorus","Chorus","Chorus","Chorus","Chaos","Chaos","Chaos","Chaos","Chaos","Chaos","Chaos"};
        static const char* syms[] = {"echo_time","echo_fb","echo_lpf","echo_hpf","echo_width","echo_lev",
                                       "rev_decay","rev_damp","rev_gate","rev_hpf","rev_pre","rev_lev",
                                       "eq_low","eq_high","cmp_thr","cmp_ratio","cmp_atk","cmp_rel","drive","volume",
                                       "echo_model","echo_mod","rev_model","rev_mod","mfilter_hp","mfilter_lp","mfilter_reso",
                                       "cho_model","cho_rate","cho_depth","cho_time","cho_fb","cho_width","cho_tone","cho_lev","gl_chance","gl_size","gl_len","gl_pitch","gl_fade","gl_rev","gl_mix"};
        static_assert(sizeof(section) / sizeof(section[0]) == kMasterCount && sizeof(syms) / sizeof(syms[0]) == kMasterCount, "master tables");
        const int i = g - gECHO_TIME;
        if (i >= 0 && i < kMasterCount) {
            char name[48];
            std::snprintf(name, sizeof(name), "%s %s", section[i], kMasterNames[i]);
            p.name = name; p.symbol = syms[i];
            p.ranges.min = 0; p.ranges.max = 127; p.ranges.def = kMasterDefaults[i];
        }
    }

    void initState(uint32_t index, State& state) override
    {
        if (index == 0) {
            state.key = "bank";
            state.defaultValue = String(serializeBank(*master).c_str());
            state.label = "Pattern bank";
        } else if (index == 1) {
            state.key = "cmd";
            state.defaultValue = "";
            state.label = "Edit command";
        } else if (index == 2) {
            state.key = "skin";
            state.defaultValue = "";
            state.label = "Interface skin";
        } else if (index == 3) {
            state.key = "waves";         // every track's user wavetable, saved with the project
            state.defaultValue = "";
            state.label = "User wavetables";
        } else {
            state.key = "wav";           // the UI loaded (or removed) one track's wavetable
            state.defaultValue = "";
            state.label = "Wavetable command";
        }
    }

    float getParameterValue(uint32_t index) const override
    {
        { int t, p; if (tpOf((int)index, t, p)) return engine->trackParam[t][p]; }
        const int g = (int)index - kGlobalBase;
        switch (g) {
        case gOUT_STEP: return (float)engine->outStep;
        case gOUT_LEDS: return (float)engine->outLeds;
        case gOUT_PATTERN: return (float)engine->outPattern;
        default: return g < kGlobalParams ? engine->global[g] : 0.f;
        }
    }

    // ---- UI edits win over stale host values
    // The UI sends every parameter it changes twice: as a host parameter and as a "pv <index> <value>" line of the
    // cmd state. Some hosts (Ableton Live) send the processor the value a parameter had before a UI edit, late, or
    // lose part of a bulk edit (randomize, undo, preset): for 1.5 s after a UI edit, host values that disagree with
    // it are ignored, and the "pv" line applies the value even if the host never does.
    std::atomic<float> uiVal[kTotalParams];
    std::atomic<int32_t> uiHold[kTotalParams];   // samples left
    void uiEdit(uint32_t index, float value)
    {
        if (index >= (uint32_t)kTotalParams) return;
        uiVal[index].store(value);
        uiHold[index].store((int32_t)(getSampleRate() * 1.5));
        applyParameter(index, value);
    }
    void setParameterValue(uint32_t index, float value) override
    {
        if (index < (uint32_t)kTotalParams && uiHold[index].load(std::memory_order_relaxed) > 0) {
            if (std::fabs(value - uiVal[index].load(std::memory_order_relaxed)) > 0.01f) return;   // stale host value
            uiHold[index].store(0);                                                                   // host caught up
        }
        applyParameter(index, value);
    }
    void applyParameter(uint32_t index, float value)
    {
        { int t, p; if (tpOf((int)index, t, p)) { engine->trackParam[t][p] = value; return; } }
        const int g = (int)index - kGlobalBase;
        if (g < 0 || g >= gOUT_STEP) return;
        engine->global[g] = value;
    }

    String getState(const char* key) const override
    {
        if (std::strcmp(key, "bank") == 0) {
            std::lock_guard<std::mutex> lk(masterMutex);
            return String(serializeBank(*master).c_str());
        }
        if (std::strcmp(key, "skin") == 0) return skinState;
        if (std::strcmp(key, "waves") == 0) {
            std::lock_guard<std::mutex> lk(masterMutex);
            UserWavePtr cur[kTracks];
            for (int t = 0; t < kTracks; ++t) cur[t] = wavSet[t] ? wavPend[t] : wavLive[t];
            return String(wav::encodeAll(cur, kTracks).c_str());
        }
        return String("");
    }

    void setState(const char* key, const char* value) override
    {
        if (std::strcmp(key, "skin") == 0) { skinState = value ? value : ""; return; }
        if (std::strcmp(key, "waves") == 0 || std::strcmp(key, "wav") == 0) {
            const bool all = key[3] == 'e';
            if (!all && !(value && *value)) return;
            // decode and build the tables here (not on the audio thread), then hand them over in run()
            std::vector<std::pair<int, UserWavePtr>> got;
            wav::decodeAll(value ? value : "", [&](int t, UserWavePtr u) { if (t >= 0 && t < kTracks) got.emplace_back(t, std::move(u)); });
            std::lock_guard<std::mutex> lk(masterMutex);
            for (int t = 0; t < kTracks; ++t) wavGrave[t].reset();   // free what the audio thread released
            if (all) for (int t = 0; t < kTracks; ++t) { wavPend[t].reset(); wavSet[t] = true; }   // a full set replaces everything
            for (auto& g : got) { wavPend[g.first] = std::move(g.second); wavSet[g.first] = true; }
            wavDirty.store(true);
            return;
        }
        if (std::strcmp(key, "bank") == 0) {
            std::lock_guard<std::mutex> lk(masterMutex);
            if (deserializeBank(*master, value)) dirty.store(0xFFFFu | kKitBit);
            return;
        }
        if (std::strcmp(key, "cmd") == 0 && value && *value) {
            std::lock_guard<std::mutex> lk(masterMutex);
            uint32_t d = 0;
            const char* s = value;
            std::string line;
            while (*s) {
                const char* e = std::strchr(s, '\n');
                const size_t len = e ? (size_t)(e - s) : std::strlen(s);
                line.assign(s, len);
                const char* c = line.c_str();
                if (line.rfind("pv ", 0) == 0) {                     // UI parameter edit (see uiEdit)
                    unsigned i = 0; float v = 0.f;
                    if (std::sscanf(line.c_str() + 3, "%u %f", &i, &v) == 2) uiEdit(i, v);
                } else if (line == "lic") licensed.store(lic::findInstalled(kProductCode).valid);  // the UI installed a licence
                else if (line.rfind("fill ", 0) == 0) fillFlag.store(std::atoi(c + 5) != 0);
                else if (line.rfind("gh ", 0) == 0) engine->setGlitchHold(std::atoi(c + 3) != 0);   // glitch HOLD
                else if (line.rfind("sel ", 0) == 0) selTrack.store(clampi(std::atoi(c + 4), 0, kTracks - 1));
                else if (line.rfind("rec ", 0) == 0) recFlag.store(std::atoi(c + 4) != 0);
                else if (line.rfind("click ", 0) == 0) clickFlag.store(std::atoi(c + 6) != 0);   // metronome while recording
                else if (line.rfind("non ", 0) == 0 || line.rfind("noff ", 0) == 0) {
                    const bool on = line[1] == 'o' && line[2] == 'n';
                    int t = 0, n = 60, v = 100;
                    std::sscanf(c + (on ? 4 : 5), "%d %d %d", &t, &n, &v);
                    if (t >= 0 && t < kTracks && pending.size() < 256)
                        pending.push_back({0, t, v / 100.f, clampi(n, 0, 127), on});
                } else if (line == "panic") panicFlag.store(true);
                else {
                    const int p = applyCommand(*master, c);
                    if (p == kKitTouched) d |= kKitBit;
                    else if (p >= 0) d |= 1u << p;
                }
                s += len; if (*s == '\n') ++s;
            }
            if (d) dirty.fetch_or(d);
        }
    }

    void initAudioPort(bool input, uint32_t index, AudioPort& port) override
    {
        if (input) return;
        static const char* names[8] = {"Main L", "Main R", "Out 2 L", "Out 2 R", "Out 3 L", "Out 3 R", "Out 4 L", "Out 4 R"};
        static const char* syms[8] = {"main_l", "main_r", "out2_l", "out2_r", "out3_l", "out3_r", "out4_l", "out4_r"};
        port.name = names[index];
        port.symbol = syms[index];
        port.groupId = index < 2 ? kPortGroupStereo : 1 + (index - 2) / 2;
    }

    void initPortGroup(uint32_t groupId, PortGroup& group) override
    {
        static const char* names[4] = {"", "Out 2", "Out 3", "Out 4"};
        static const char* syms[4] = {"", "out2", "out3", "out4"};
        if (groupId >= 1 && groupId <= 3) { group.name = names[groupId]; group.symbol = syms[groupId]; }
    }

    void activate() override { engine->reset(); }

    void sampleRateChanged(double sr) override { engine->init(sr); }

    void run(const float**, float** outputs, uint32_t frames, const MidiEvent* midi, uint32_t midiCount) override
    {
        for (int i = 0; i < kTotalParams; ++i) {
            const int32_t h = uiHold[i].load(std::memory_order_relaxed);
            if (h > 0) uiHold[i].store(h > (int32_t)frames ? h - (int32_t)frames : 0, std::memory_order_relaxed);
        }
        notes.clear();
        // pull pattern edits and on-screen keyboard notes without ever blocking
        {
            std::unique_lock<std::mutex> lk(masterMutex, std::try_to_lock);
            if (lk.owns_lock()) {
                if (dirty.load(std::memory_order_acquire)) {
                    const uint32_t d = dirty.exchange(0);
                    for (int p = 0; p < kPatterns; ++p)
                        if (d & (1u << p)) engine->bank.patterns[p] = master->patterns[p];
                    if (d & kKitBit) {
                        std::memcpy(engine->bank.machine, master->machine, sizeof(master->machine));
                        std::memcpy(engine->bank.mute, master->mute, sizeof(master->mute)); std::memcpy(engine->bank.solo, master->solo, sizeof(master->solo));
                        std::memcpy(engine->bank.kit, master->kit, sizeof(master->kit));
                    engine->bank.scenes = master->scenes; engine->bank.chain = master->chain;
                        engine->bank.glob = master->glob;
                    }
                }
                for (const auto& n : pending) notes.push_back(n);
                pending.clear();
            }
        }
        // install new user wavetables (the old ones are freed later, outside the audio thread)
        if (wavDirty.load(std::memory_order_acquire)) {
            std::unique_lock<std::mutex> lk(masterMutex, std::try_to_lock);
            if (lk.owns_lock()) {
                bool left = false;
                for (int t = 0; t < kTracks; ++t) {
                    if (!wavSet[t]) continue;
                    if (wavGrave[t]) { left = true; continue; }        // wait until the previous one is freed
                    wavGrave[t] = std::move(wavLive[t]);
                    wavLive[t] = std::move(wavPend[t]);
                    wavSet[t] = false;
                    engine->userWaves[t] = wavLive[t] ? &wavLive[t]->set : nullptr;
                }
                wavDirty.store(left);
            }
        }
        engine->fill = fillFlag.load();
        engine->recording = recFlag.load();
        engine->click.enabled = clickFlag.load();
        if (panicFlag.exchange(false)) { engine->allNotesOff(); engine->resetExpression(); }

        Transport tp;
        const TimePosition& pos = getTimePosition();
        if (pos.bbt.valid) {
            tp.hostValid = true;
            tp.hostPlaying = pos.playing;
            const double bt = pos.bbt.beatType > 0 ? pos.bbt.beatType : 4.0;
            const double beatsFromStart = (pos.bbt.bar - 1) * (double)pos.bbt.beatsPerBar + (pos.bbt.beat - 1)
                                        + (pos.bbt.ticksPerBeat > 0 ? pos.bbt.tick / pos.bbt.ticksPerBeat : 0.0);
            tp.beat = beatsFromStart * (4.0 / bt);
            tp.bpm = pos.bbt.beatsPerMinute > 0 ? pos.bbt.beatsPerMinute : 120.0;
        }

        // MIDI: tracks listening to a channel get its notes; otherwise notes go to the selected track
        const int sel = selTrack.load();
        for (uint32_t i = 0; i < midiCount && notes.size() < 500; ++i) {
            const MidiEvent& ev = midi[i];
            if (ev.size < 2) continue;
            const uint8_t st = ev.data[0] & 0xF0;
            const int ch = (ev.data[0] & 0x0F) + 1;
            if (st == 0xB0 && ev.size >= 3 && (ev.data[1] == 123 || ev.data[1] == 120)) { engine->allNotesOff(); continue; }
            if (st == 0xB0 && ev.size >= 3 && ev.data[1] == 121) { engine->resetExpression(); continue; }
            // expression: pitch bend (+-2 semitones), mod wheel (vibrato), channel pressure (filter)
            if (st == 0xE0 || st == 0xD0 || (st == 0xB0 && ev.size >= 3 && ev.data[1] == 1)) {
                float* dst = st == 0xE0 ? engine->midiBend : (st == 0xD0 ? engine->midiPress : engine->midiMod);
                float v;
                if (st == 0xE0) { if (ev.size < 3) continue; v = (float)(((int)ev.data[2] << 7 | ev.data[1]) - 8192) / 8192.f * 2.f; }
                else v = (st == 0xD0 ? ev.data[1] : ev.data[2]) / 127.f;
                bool routed = false;
                for (int t = 0; t < kTracks; ++t) if (engine->bank.kit[t][kfMidi] == ch) { dst[t] = v; routed = true; }
                if (!routed) dst[sel] = v;
                continue;
            }
            if (ev.size < 3 || (st != 0x90 && st != 0x80)) continue;
            const bool on = st == 0x90 && ev.data[2] > 0;
            bool routed = false;
            for (int t = 0; t < kTracks; ++t)
                if (engine->bank.kit[t][kfMidi] == ch) {
                    notes.push_back({ev.frame, t, ev.data[2] / 100.f, (int)ev.data[1], on});
                    routed = true;
                }
            if (!routed) notes.push_back({ev.frame, sel, ev.data[2] / 100.f, (int)ev.data[1], on});
        }
        engine->process(outputs, frames, tp, notes.data(), (uint32_t)notes.size());
        if (lic::kDemoModeEnabled && !licensed.load(std::memory_order_relaxed)) demoGate(outputs, frames);
    }

    // Demo mode (no licence installed): the sound drops out for 3 seconds every 45 seconds.
    void demoGate(float** outputs, uint32_t frames)
    {
        const double sr = getSampleRate();
        const uint64_t period = (uint64_t)(45.0 * sr), mute = (uint64_t)(3.0 * sr);
        const float coef = 1.f - std::exp(-1.f / (0.01f * (float)sr));
        for (uint32_t i = 0; i < frames; ++i) {
            const float target = (demoPos % period) >= period - mute ? 0.f : 1.f;
            demoGain += (target - demoGain) * coef;
            ++demoPos;
            if (demoGain < 0.9999f)
                for (uint32_t c = 0; c < DISTRHO_PLUGIN_NUM_OUTPUTS; ++c) outputs[c][i] *= demoGain;
        }
    }

private:
    std::unique_ptr<Engine> engine;
    std::unique_ptr<Bank> master;
    mutable std::mutex masterMutex;
    std::atomic<uint32_t> dirty{0};
    UserWavePtr wavPend[kTracks], wavLive[kTracks], wavGrave[kTracks];
    bool wavSet[kTracks] = {};
    std::atomic<bool> wavDirty{false};
    std::atomic<bool> fillFlag{false}, recFlag{false}, panicFlag{false}, clickFlag{true};
    std::atomic<int> selTrack{0};
    std::atomic<bool> licensed{false};
    uint64_t demoPos = 0; float demoGain = 1.f;
    static constexpr const char* kProductCode = "KMSP-6";
    String skinState;
    std::vector<NoteEvent> notes, pending;

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MonoPlugin)
};

Plugin* createPlugin() { return new MonoPlugin(); }

END_NAMESPACE_DISTRHO
