// The sound: each note traces the oscillators' shapes the way oscilloscope
// music works (left = x, right = y, one trip around the shape per cycle, so
// the key you play sets how fast the beam goes round). Two oscillators (each
// up to 8 slightly detuned copies: VOICES), an
// envelope per note, one LFO, and ENV/LFO links to the knobs. Plain C++ with
// no plugin code, so it can be tested on its own (tests/engine_test.cpp).
#pragma once

#include "Equations.hpp"
#include "Params.hpp"
#include "Shapes.hpp"
#include <cstdint>
#include <cstring>

namespace ss {

// Every curve traced once into a table. Animated curves are traced again
// whenever a note uses them and the table is more than a few ms old.
class ShapeTables {
public:
    static constexpr int kT = kTableN;

    ShapeTables()
    {
        for (int id = 0; id < crt::kNumEquations; ++id) {
            fill(id, 0);
            stamp[id] = -1;
        }
    }
    void ensure(int id, double u, int64_t tick)
    {
        if (crt::kEquationInfo[id].animated && stamp[id] != tick) {
            fill(id, u);
            stamp[id] = tick;
        }
    }
    const float* x(int id) const { return X[id]; }
    const float* y(int id) const { return Y[id]; }

private:
    // How many times a curve goes round the same outline before it closes.
    // The butterfly laps 6 times (drifting a hair each lap): a note plays one
    // lap, or it would sound 6x too high, out of key.
    static int laps(int id) { return id == 2 ? 6 : 1; }

    // centred on 0, so rotating turns it in place and nothing leaks out as DC
    void fill(int id, double u)
    {
        float mx = 0, my = 0;
        for (int i = 0; i < kT; ++i) {
            crt::equation(id, i / (double)(kT * laps(id)), u, X[id][i], Y[id][i]);
            mx += X[id][i];
            my += Y[id][i];
        }
        mx /= kT;
        my /= kT;
        for (int i = 0; i < kT; ++i) {
            X[id][i] -= mx;
            Y[id][i] -= my;
        }
    }
    float X[crt::kNumEquations][kT], Y[crt::kNumEquations][kT];
    int64_t stamp[crt::kNumEquations];
};

class Engine {
public:
    static constexpr int kVoices = 16;

    explicit Engine(const float* params) : P(params) {}

    void setSampleRate(float rate) { sr = rate > 0 ? rate : 48000; }

    // ---- notes (MIDI, or the keys in the window)
    void noteOn(int note, int velocity)
    {
        if (velocity <= 0) {
            noteOff(note);
            return;
        }
        note &= 127;
        keys[note >> 6] |= 1ull << (note & 63);
        const float vel = 0.25f + 0.75f * std::min(velocity, 127) / 127.0f;
        const float glide = P[kGlide];
        if (poly()) {
            Voice* v = nullptr;
            for (Voice& w : voices)  // the same key again: play it on the same voice
                if (w.active && w.note == note) v = &w;
            if (!v) v = freeVoice();
            if (!v->active) start(*v);
            v->stage = 0;
            v->held = true;
            v->note = note;
            v->vel = vel;
            v->target = (float)note;
            v->pitch = glide > 0 && lastNote >= 0 ? (float)lastNote : (float)note;
            v->order = ++counter;
        } else {
            removeHeld(note);
            const bool overlapping = numHeld > 0;
            if (numHeld < kMaxHeld) held[numHeld++] = note;
            Voice& v = voices[0];
            const bool legato = P[kLegato] > 0.5f;
            const bool slide = glide > 0 && (overlapping || (!legato && lastNote >= 0));
            if (!v.active) {
                start(v);
                v.pitch = slide ? (float)lastNote : (float)note;
            } else if (!slide) {
                v.pitch = (float)note;
            }
            if (!(legato && overlapping && v.active && v.stage != 2))
                v.stage = 0;  // start the envelope again (legato: only for a fresh press)
            v.held = true;
            v.note = note;
            v.vel = vel;
            v.target = (float)note;
            v.order = ++counter;
        }
        lastNote = note;
    }

    void noteOff(int note)
    {
        note &= 127;
        keys[note >> 6] &= ~(1ull << (note & 63));
        if (poly()) {
            for (Voice& v : voices)
                if (v.active && v.held && v.note == note) {
                    v.held = false;
                    v.stage = 2;
                }
            return;
        }
        removeHeld(note);
        Voice& v = voices[0];
        if (!v.active || v.note != note)
            return;
        if (numHeld > 0) {  // back to the key still held down
            const int back = held[numHeld - 1];
            v.note = back;
            v.target = (float)back;
            if (P[kGlide] <= 0) v.pitch = (float)back;
            if (P[kLegato] <= 0.5f) v.stage = 0;
            lastNote = back;
        } else {
            v.held = false;
            v.stage = 2;
        }
    }

    void pitchBend(float semitones) { bend = semitones; }

    // all notes off: let them ring out, or (hard) stop at once
    void allOff(bool hard)
    {
        for (Voice& v : voices) {
            v.held = false;
            v.stage = 2;
            if (hard) v.active = false;
        }
        numHeld = 0;
        keys[0] = keys[1] = 0;
    }

    // ---- sound: typed-in shapes come from `set` (it may change between blocks)
    void render(float* L, float* R, uint32_t n, const ShapeSet& set)
    {
        if (&set != lastSet) {  // new or edited shapes: trace the moving ones afresh
            lastSet = &set;
            for (int64_t& st : customStamp) st = -1;
        }
        shapes = &set;
        if (wasPoly >= 0 && poly() != (wasPoly == 1))  // switched POLY / MONO: let what's playing ring out
            allOff(false);
        wasPoly = poly();
        while (n > 0) {
            const uint32_t m = std::min<uint32_t>(n, kSub);
            renderSub(L, R, m);
            L += m;
            R += m;
            n -= m;
        }
    }

    // what the window shows
    float envNow() const { const Voice* v = newest(); return v ? v->env : 0.0f; }
    float lfoNow() const { return lfoVal; }
    int newestNote() const { const Voice* v = newest(); return v ? v->note : -1; }
    float newestPitch() const { const Voice* v = newest(); return v ? v->pitch + bend : 0.0f; }
    int activeVoices() const { int c = 0; for (const Voice& v : voices) c += v.active; return c; }
    bool keyDown(int note) const { return (keys[(note & 127) >> 6] >> (note & 63)) & 1; }
    double animTime() const { return u; }

    // a parameter as the ENV and LFO have moved it for this note
    float modulated(int p, float env, float lfo) const
    {
        const int d = destIndex(p);
        if (d < 0) return P[p];
        const float n = toNorm(p, P[p]) + P[modParam(kSrcEnv, d)] * env + P[modParam(kSrcLfo, d)] * lfo;
        return fromNorm(p, n);
    }

private:
    static constexpr int kSub = 32;         // modulation is worked out every 32 samples
    static constexpr int kMaxHeld = 32;
    static constexpr int kTableEvery = 256; // animated curves: re-traced this often (samples)
    static constexpr int kMaxUnison = 8;
    static constexpr float kSpreadCents = 20; // VOICES: copies spread over +-10 cents

    struct Voice {
        bool active = false, held = false;
        int note = 60, stage = 0;  // stage: 0 attack, 1 decay/sustain, 2 release
        float vel = 1, env = 0;
        float pitch = 60, target = 60;
        uint64_t order = 0;
        double ph[2][kMaxUnison] = {};
        float lv[2] = {0, 0}, cs[2] = {1, 1}, sn[2] = {0, 0};  // last level and rotation, ramped from
        float f1[2][2] = {}, f2[2][2] = {};  // filter state: [stage][x, y] (two stages for LP24)
    };

    const float* P;
    float sr = 48000;
    Voice voices[kVoices];
    int held[kMaxHeld] = {}, numHeld = 0;
    int lastNote = -1;
    uint64_t counter = 0, keys[2] = {0, 0}, samples = 0;
    float bend = 0;
    int wasPoly = -1;
    double lfoPh = 0, u = 0;
    float lfoVal = 0, lfoHold = 0;
    uint32_t rng = 12345;
    float dcIn[2] = {0, 0}, dcOut[2] = {0, 0};
    ShapeTables tables;
    const ShapeSet* shapes = nullptr;
    const ShapeSet* lastSet = nullptr;
    float customX[kMaxCustom][kTableN], customY[kMaxCustom][kTableN];
    int64_t customStamp[kMaxCustom] = {};

    // the table for shape `id`, traced afresh if it moves and this one is old
    void table(int id, int64_t tick, const float*& X, const float*& Y)
    {
        if (!shapes->isCustom(id)) {
            tables.ensure(id, u, tick);
            X = tables.x(id);
            Y = tables.y(id);
            return;
        }
        const int c = id - crt::kNumEquations;
        const CustomShape& cs = shapes->customs[c];
        if (!cs.animated()) {
            X = cs.X.data();
            Y = cs.Y.data();
            return;
        }
        if (customStamp[c] != tick) {
            cs.trace(customX[c], customY[c], u);
            customStamp[c] = tick;
        }
        X = customX[c];
        Y = customY[c];
    }

    bool poly() const { return P[kPoly] > 0.5f; }

    const Voice* newest() const
    {
        const Voice* best = nullptr;
        for (const Voice& v : voices)
            if (v.active && (!best || v.order > best->order)) best = &v;
        return best;
    }

    static void start(Voice& v)
    {
        v.active = true;
        v.env = 0;
        for (int o = 0; o < 2; ++o)
            for (int k = 0; k < kMaxUnison; ++k)
                v.ph[o][k] = k * 0.618034 - std::floor(k * 0.618034);  // copies start at different points around
        v.lv[0] = v.lv[1] = 0;
        v.cs[0] = v.cs[1] = 1;
        v.sn[0] = v.sn[1] = 0;
        std::memset(v.f1, 0, sizeof(v.f1));
        std::memset(v.f2, 0, sizeof(v.f2));
    }

    // One state-variable filter step (Cytomic's trapezoidal SVF), for x or y.
    // Returns low, band and high pass outputs.
    static void svf(float in, float g, float k, float& s1, float& s2, float& lp, float& bp, float& hp)
    {
        const float a1 = 1 / (1 + g * (g + k)), a2 = g * a1, a3 = g * a2;
        const float v3 = in - s2;
        const float v1 = a1 * s1 + a2 * v3;
        const float v2 = s2 + a2 * s1 + a3 * v3;
        s1 = 2 * v1 - s1;
        s2 = 2 * v2 - s2;
        lp = v2;
        bp = v1;
        hp = in - k * v1 - v2;
    }

    // a free voice, else the quietest one letting go, else the oldest
    Voice* freeVoice()
    {
        Voice* best = nullptr;
        for (Voice& v : voices)
            if (!v.active) return &v;
        for (Voice& v : voices)
            if (!v.held && (!best || v.env < best->env)) best = &v;
        if (best) return best;
        best = &voices[0];
        for (Voice& v : voices)
            if (v.order < best->order) best = &v;
        return best;
    }

    void removeHeld(int note)
    {
        int k = 0;
        for (int i = 0; i < numHeld; ++i)
            if (held[i] != note) held[k++] = held[i];
        numHeld = k;
    }

    float lfoShape(double p)
    {
        switch ((int)std::lround(P[kLfoWave])) {
        case 1: return (float)(1 - 4 * std::fabs(p - 0.5));
        case 2: return (float)(2 * p - 1);
        case 3: return p < 0.5 ? 1.0f : -1.0f;
        case 4: return lfoHold;
        default: return (float)std::sin(6.283185307179586 * p);
        }
    }

    void renderSub(float* L, float* R, uint32_t m)
    {
        std::fill(L, L + m, 0.0f);
        std::fill(R, R + m, 0.0f);
        const Voice* nv = newest();
        const float envN = nv ? nv->env : 0.0f;

        // the LFO: one for all notes, free running
        lfoVal = lfoShape(lfoPh);
        const float rate = modulated(kLfoRate, envN, lfoVal);
        lfoPh += rate * m / sr;
        if (lfoPh >= 1) {
            lfoPh -= std::floor(lfoPh);
            rng = rng * 1664525u + 1013904223u;
            lfoHold = (rng >> 8) / 8388608.0f - 1;  // new random step each cycle
        }

        const int64_t tick = (int64_t)(samples / kTableEvery);
        const float aInc = 1.0f / (std::max(P[kAttack], 0.001f) * sr);
        const float dCoef = std::exp(-6.9f / (std::max(P[kDecay], 0.001f) * sr));
        const float rCoef = std::exp(-6.9f / (std::max(P[kRelease], 0.001f) * sr));
        const float sus = std::clamp(P[kSustain], 0.0f, 1.0f);
        const float glide = P[kGlide];
        const float glideK = glide > 0.0005f ? 1.0f - std::exp(-(float)m * 4.0f / (glide * sr)) : 1.0f;

        for (Voice& v : voices) {
            if (!v.active) continue;
            v.pitch += (v.target - v.pitch) * glideK;
            const float env = v.env;
            const float vol = modulated(kVolume, env, lfoVal) * v.vel;

            const float* tx[2][2];
            const float* ty[2][2];
            float morph[2], inc[2][kMaxUnison], lv[2], cs[2], sn[2];
            int uni[2];
            for (int o = 0; o < 2; ++o) {
                const int b = o * (kBShape - kAShape);
                // MORPH: where along the playlist, blending the two slots either side
                const int slots = slotCount(P, o);
                const float pos = std::clamp(modulated(kAShape + b, env, lfoVal), 0.0f, 1.0f) * (slots - 1);
                const int s0 = std::min((int)pos, slots - 1), s1 = std::min(s0 + 1, slots - 1);
                morph[o] = pos - s0;
                table(slotShape(P, o, s0, shapes->count()), tick, tx[o][0], ty[o][0]);
                table(slotShape(P, o, s1, shapes->count()), tick, tx[o][1], ty[o][1]);
                const float semis = v.pitch + bend + 12 * std::lround(P[kAOct + b]) + modulated(kASemi + b, env, lfoVal)
                                  + modulated(kAFine + b, env, lfoVal) / 100 - 69;
                uni[o] = std::clamp((int)std::lround(P[kAVoices + b]), 1, kMaxUnison);
                for (int k = 0; k < uni[o]; ++k) {
                    const float cents = uni[o] > 1 ? kSpreadCents * ((float)k / (uni[o] - 1) - 0.5f) : 0.0f;
                    inc[o][k] = std::min(440.0f * std::exp2(semis / 12 + cents / 1200) / sr, 0.45f);
                }
                // copies add up about as loud as one
                lv[o] = std::max(modulated(kALevel + b, env, lfoVal), 0.0f) * vol / std::sqrt((float)uni[o]);
                const float rot = modulated(kARot + b, env, lfoVal) * 0.017453293f;
                cs[o] = std::cos(rot);
                sn[o] = std::sin(rot);
            }

            // the filter for this note: cutoff and resonance as the links set them now
            const int ftype = std::clamp((int)std::lround(P[kFilterType]), 0, 4);
            const float cutoff = std::min(modulated(kCutoff, env, lfoVal), 0.45f * sr);
            const float res = std::clamp(modulated(kRes, env, lfoVal), 0.0f, 1.0f);
            const float fg = std::tan(3.14159265f * cutoff / sr);
            const float fk = 1.414f - 1.38f * res;  // 0 = gentle, 1 = nearly singing

            for (uint32_t j = 0; j < m; ++j) {
                // the envelope, every sample
                if (v.stage == 0) {
                    v.env += aInc;
                    if (v.env >= 1) { v.env = 1; v.stage = 1; }
                } else if (v.stage == 1) {
                    v.env = sus + (v.env - sus) * dCoef;
                } else {
                    v.env *= rCoef;
                }
                const float k = (j + 1) / (float)m;
                float sx = 0, sy = 0;
                for (int o = 0; o < 2; ++o) {
                    const float level = v.lv[o] + (lv[o] - v.lv[o]) * k;
                    float x = 0, y = 0;
                    for (int u2 = 0; u2 < uni[o]; ++u2) {
                        double& ph = v.ph[o][u2];
                        ph += inc[o][u2];
                        if (ph >= 1) ph -= 1;
                        if (level == 0) continue;
                        const float p = (float)(ph * ShapeTables::kT);
                        const int i0 = std::min((int)p, ShapeTables::kT - 1), i1 = (i0 + 1) & (ShapeTables::kT - 1);
                        const float f = p - i0;
                        const float xa = tx[o][0][i0] + (tx[o][0][i1] - tx[o][0][i0]) * f;
                        const float ya = ty[o][0][i0] + (ty[o][0][i1] - ty[o][0][i0]) * f;
                        const float xb = tx[o][1][i0] + (tx[o][1][i1] - tx[o][1][i0]) * f;
                        const float yb = ty[o][1][i0] + (ty[o][1][i1] - ty[o][1][i0]) * f;
                        x += xa + (xb - xa) * morph[o];
                        y += ya + (yb - ya) * morph[o];
                    }
                    if (level == 0) continue;
                    const float c = v.cs[o] + (cs[o] - v.cs[o]) * k, s = v.sn[o] + (sn[o] - v.sn[o]) * k;
                    sx += (x * c - y * s) * level;
                    sy += (x * s + y * c) * level;
                }
                {
                    float* in[2] = {&sx, &sy};
                    for (int ch = 0; ch < 2; ++ch) {
                        float lp, bp, hp;
                        svf(*in[ch], fg, fk, v.f1[0][ch], v.f1[1][ch], lp, bp, hp);
                        float out;
                        switch (ftype) {
                        case 1: {  // LP24: a second, plain low-pass after the resonant one
                            float lp2, bp2, hp2;
                            svf(lp, fg, 1.414f, v.f2[0][ch], v.f2[1][ch], lp2, bp2, hp2);
                            out = lp2;
                            break;
                        }
                        case 2: out = hp; break;
                        case 3: out = bp * fk; break;  // band-pass, peak at 0 dB
                        case 4: out = lp + hp; break;  // notch
                        default: out = lp; break;
                        }
                        *in[ch] = out;
                    }
                }
                L[j] += sx * v.env;
                R[j] += sy * v.env;
            }
            for (int o = 0; o < 2; ++o) {
                v.lv[o] = lv[o];
                v.cs[o] = cs[o];
                v.sn[o] = sn[o];
            }
            if (v.stage == 2 && v.env < 1e-5f) v.active = false;
        }

        // no DC to the speakers; peaks eased in over 0.8 so a big chord never clips
        const float dc = std::exp(-6.2831853f * 10.0f / sr);
        float* out[2] = {L, R};
        for (int c = 0; c < 2; ++c)
            for (uint32_t j = 0; j < m; ++j) {
                const float in = out[c][j];
                float o = in - dcIn[c] + dc * dcOut[c];
                dcIn[c] = in;
                if (std::fabs(o) < 1e-20f) o = 0;  // no denormals in long silences
                dcOut[c] = o;
                const float a = std::fabs(o);
                if (a > 0.8f) o = std::copysign(0.8f + 0.2f * std::tanh((a - 0.8f) / 0.2f), o);
                out[c][j] = o;
            }
        samples += m;
        u += m / (double)sr;
        if (u > 3600) u -= 3600;
    }
};

} // namespace ss
