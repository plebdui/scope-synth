// Checks the sound engine on its own: g++ -std=c++17 -O2 -I. tests/engine_test.cpp -o engine_test && ./engine_test
#include "SynthEngine.hpp"
#include "Presets.hpp"
#include <chrono>
#include <cstdio>
#include <vector>

using namespace ss;
static float P[kParamCount];
static ShapeSet gSet;
static int fails = 0;
#define EXPECT(c, ...) do { std::printf("%s: ", (c) ? "ok  " : "FAIL"); std::printf(__VA_ARGS__); std::printf("\n"); if (!(c)) ++fails; } while (0)

// an oscillator's shape list, as the slot parameters
static void slotsOf(int o, std::initializer_list<int> ids)
{
    int i = 0;
    for (int id : ids) P[slotParam(o, i++)] = (float)id;
    P[slotsParam(o)] = (float)i;
}

static void defaults()
{
    for (int p = 0; p < kParamCount; ++p) P[p] = paramDefault(p);
    gSet = ShapeSet();
}

// frequency of the left channel from its strongest repeat (autocorrelation)
static float pitchOf(const std::vector<float>& x, float sr)
{
    const int n = 8192, start = (int)x.size() - n - 2000;
    const int lo = (int)(sr / 2000), hi = (int)(sr / 30);
    std::vector<float> r(hi);
    float best = 0;
    for (int t = lo; t < hi; ++t) {
        double s = 0, e = 0;
        for (int i = 0; i < n; ++i) { s += x[start + i] * x[start + i + t]; e += x[start + i + t] * x[start + i + t]; }
        r[t] = (float)(s / std::sqrt(e + 1e-12));
        best = std::max(best, r[t]);
    }
    for (int t = lo + 1; t < hi - 1; ++t)  // the first peak nearly as high as the best (not its multiples)
        if (r[t] >= 0.9f * best && r[t] >= r[t - 1] && r[t] >= r[t + 1]) return sr / t;
    return 0;
}

// pitch of a stretch of sound (both channels), from its first strong repeat
static float pitchAt(const std::vector<float>& l, const std::vector<float>& r, int from, int n, float sr)
{
    std::vector<float> x(n + 1200);
    for (int i = 0; i < n + 1200; ++i) x[i] = l[from + i] + r[from + i];
    const int lo = (int)(sr / 2000), hi = (int)(sr / 50);
    std::vector<float> c(hi);
    float best = 0;
    for (int t = lo; t < hi; ++t) {
        double s = 0, e = 0;
        for (int i = 0; i < n; ++i) { s += x[i] * x[i + t]; e += x[i + t] * x[i + t]; }
        c[t] = (float)(s / std::sqrt(e + 1e-12));
        best = std::max(best, c[t]);
    }
    for (int t = lo + 1; t < hi - 1; ++t)
        if (c[t] >= 0.9f * best && c[t] >= c[t - 1] && c[t] >= c[t + 1]) return sr / t;
    return 0;
}

static std::vector<float> run(Engine& e, int frames, std::vector<float>* right = nullptr)
{
    std::vector<float> l(frames), r(frames);
    for (int i = 0; i < frames; i += 512) {
        const int m = std::min(512, frames - i);
        e.render(&l[i], &r[i], m, gSet);
    }
    if (right) *right = r;
    return l;
}

static float peak(const std::vector<float>& x, int from = 0)
{
    float m = 0;
    for (size_t i = from; i < x.size(); ++i) { if (!std::isfinite(x[i])) return 99; m = std::max(m, std::fabs(x[i])); }
    return m;
}

int main()
{
    const float sr = 48000;
    for (int eq = 0; eq < crt::kNumEquations; ++eq) {
        defaults();
        slotsOf(0, {eq});
        Engine e(P);
        e.setSampleRate(sr);
        e.noteOn(57, 100);  // A3 = 220 Hz
        std::vector<float> r;
        auto l = run(e, 48000, &r);
        for (size_t i = 0; i < l.size(); ++i) r[i] += l[i];  // both channels: a 3:2 Lissajous is 3x on one side
        const float f = pitchOf(r, sr);
        const float oct = std::exp2(std::round(std::log2(std::max(f, 1.0f) / 220)));  // an octave up is still in key
        EXPECT(std::fabs(f / 220 / oct - 1) < 0.02f && oct <= 2, "%-20s A3 plays at %.1f Hz, peak %.2f", crt::kEquationInfo[eq].name, f, peak(l));
    }

    defaults();
    {
        Engine e(P); e.setSampleRate(sr);
        P[kASemi] = 7;
        e.noteOn(57, 127);
        auto l = run(e, 48000);
        EXPECT(std::fabs(pitchOf(l, sr) / 329.63f - 1) < 0.02f, "SEMI +7 -> %.1f Hz (E4 329.6)", pitchOf(l, sr));
        P[kASemi] = 0;
    }
    {
        Engine e(P); e.setSampleRate(sr);
        for (int n : {48, 52, 55, 60, 64, 67, 72, 76, 79, 84, 88, 91, 96, 100, 103, 108})
            e.noteOn(n, 127);
        std::vector<float> r;
        auto l = run(e, 48000, &r);
        EXPECT(e.activeVoices() == 16 && peak(l) <= 1.0f && peak(r) <= 1.0f, "16-note chord: %d voices, peaks %.2f %.2f (never over 1)", e.activeVoices(), peak(l), peak(r));
        e.noteOn(40, 127);
        EXPECT(e.activeVoices() == 16 && e.newestNote() == 40, "17th note steals a voice");
        for (int n = 0; n < 128; ++n) e.noteOff(n);
        auto t = run(e, 48000 * 3);
        EXPECT(e.activeVoices() == 0 && peak(t, 48000 * 2) < 1e-3f, "all released -> silent (%d voices left)", e.activeVoices());
    }
    {
        P[kPoly] = 0;
        Engine e(P); e.setSampleRate(sr);
        e.noteOn(60, 100); e.noteOn(64, 100); e.noteOn(67, 100);
        run(e, 4800);
        EXPECT(e.activeVoices() == 1 && e.newestNote() == 67, "MONO: three keys -> one voice playing the last");
        e.noteOff(67);
        run(e, 4800);
        EXPECT(e.activeVoices() == 1 && e.newestNote() == 64, "MONO: let go of the last -> back to the one still held");
        // legato: a key pressed while another is held doesn't restart the envelope
        P[kLegato] = 1; P[kAttack] = 0.5f;
        Engine g(P); g.setSampleRate(sr);
        g.noteOn(60, 100);
        run(g, 48000);  // past the attack
        const float before = g.envNow();
        g.noteOn(62, 100);
        run(g, 4800);
        EXPECT(std::fabs(g.envNow() - before) < 0.02f, "LEGATO: overlapping note keeps the envelope (%.2f -> %.2f)", before, g.envNow());
        P[kLegato] = 0;
        Engine h(P); h.setSampleRate(sr);
        h.noteOn(60, 100);
        run(h, 48000);
        h.noteOn(62, 100);
        run(h, 64);
        const float right = h.envNow();
        run(h, 4800);
        EXPECT(std::fabs(right - before) < 0.02f && h.envNow() > before + 0.1f,
               "no LEGATO: envelope attacks again, from where it was (%.2f -> %.2f -> %.2f), no click to 0", before, right, h.envNow());
        P[kAttack] = paramDefault(kAttack);
        // glide
        P[kGlide] = 0.3f;
        Engine gl(P); gl.setSampleRate(sr);
        gl.noteOn(48, 100); run(gl, 24000); gl.noteOn(60, 100); run(gl, 480);
        const float mid = gl.newestPitch();
        run(gl, 48000);
        EXPECT(mid > 48.5f && mid < 59.5f && std::fabs(gl.newestPitch() - 60) < 0.05f, "GLIDE: 10 ms in at %.2f, settles on %.2f", mid, gl.newestPitch());
        P[kGlide] = 0; P[kPoly] = 1;
    }
    {
        // ENV linked to SEMI: the pitch drops as the envelope falls to sustain 0
        P[modParam(kSrcEnv, destIndex(kASemi))] = 0.5f;  // half the knob = +12 st at full envelope
        P[kSustain] = 0; P[kDecay] = 0.3f;
        Engine e(P); e.setSampleRate(sr);
        e.noteOn(57, 127);
        std::vector<float> l(2400);
        run(e, 0);
        std::vector<float> r(2400);
        e.render(l.data(), r.data(), 2400, gSet);
        // count rising zero crossings in the first 50 ms vs a plain note
        auto cross = [](const std::vector<float>& x) { int c = 0; for (size_t i = 1; i < x.size(); ++i) c += x[i - 1] < 0 && x[i] >= 0; return c; };
        P[modParam(kSrcEnv, destIndex(kASemi))] = 0;
        Engine plain(P); plain.setSampleRate(sr);
        plain.noteOn(57, 127);
        std::vector<float> l2(2400), r2(2400);
        plain.render(l2.data(), r2.data(), 2400, gSet);
        EXPECT(cross(l) > cross(l2) * 1.5, "ENV -> SEMI raises the pitch at the start (%d vs %d crossings)", cross(l), cross(l2));
        P[kSustain] = paramDefault(kSustain); P[kDecay] = paramDefault(kDecay);
    }
    {
        // LFO linked to ROTATE: the shape turns, so left/right energy trades places over time
        P[modParam(kSrcLfo, destIndex(kARot))] = 0.5f; P[kLfoRate] = 1; slotsOf(0, {5});  // infinity: wide, flat
        Engine e(P); e.setSampleRate(sr);
        e.noteOn(57, 127);
        std::vector<float> r;
        auto l = run(e, 48000, &r);
        float lo = 1e9f, hi = 0;
        for (int w = 0; w < 48000; w += 4800) {
            double el = 0, er = 0;
            for (int i = w; i < w + 4800; ++i) { el += l[i] * l[i]; er += r[i] * r[i]; }
            const float ratio = (float)(el / (er + 1e-9));
            lo = std::min(lo, ratio); hi = std::max(hi, ratio);
        }
        EXPECT(hi / lo > 3, "LFO -> ROTATE spins the shape (L/R energy ratio %.2f .. %.2f)", lo, hi);
        P[modParam(kSrcLfo, destIndex(kARot))] = 0; slotsOf(0, {0});
    }
    {
        // both oscillators, 16 voices of an animated curve: how long it takes
        slotsOf(0, {10}); slotsOf(1, {6, 7, 9}); P[kBShape] = 0.3f; P[kBLevel] = 0.6f;
        gSet.customs.resize(1);
        gSet.customs[0].xs = "sin(3t + u)"; gSet.customs[0].ys = "cos(5t)";
        gSet.customs[0].compile(); gSet.customs[0].build();
        slotsOf(0, {10, 11});
        P[modParam(kSrcLfo, destIndex(kAShape))] = 0.2f;
        Engine e(P); e.setSampleRate(sr);
        for (int n = 40; n < 56; ++n) e.noteOn(n, 100);
        const auto t0 = std::chrono::steady_clock::now();
        auto l = run(e, 48000 * 4);
        const float secs = std::chrono::duration<float>(std::chrono::steady_clock::now() - t0).count();
        EXPECT(secs < 0.4f && peak(l) <= 1.0f, "16 voices x 2 oscillators, 4 s of sound in %.0f ms (%.1f%% of one core)", secs * 1000, secs / 4 * 100);
    }
    {
        // VOICES: 8 detuned copies, still in key, about as loud, and the shape drifts
        defaults();
        P[kAVoices] = 8;
        Engine e(P); e.setSampleRate(sr);
        e.noteOn(57, 127);
        std::vector<float> r;
        auto l = run(e, 48000, &r);
        for (size_t i = 0; i < l.size(); ++i) r[i] += l[i];
        const float f = pitchOf(r, sr);
        EXPECT(std::fabs(f / 220 - 1) < 0.02f && peak(l) > 0.1f && peak(l) <= 1.0f, "VOICES 8: A3 at %.1f Hz, peak %.2f", f, peak(l));
        // worst case: 16 notes, both oscillators, 8 voices each
        P[kBLevel] = 0.6f; P[kBVoices] = 8;
        Engine w(P); w.setSampleRate(sr);
        for (int n = 40; n < 56; ++n) w.noteOn(n, 100);
        const auto t0 = std::chrono::steady_clock::now();
        auto big = run(w, 48000 * 2);
        const float secs = std::chrono::duration<float>(std::chrono::steady_clock::now() - t0).count();
        EXPECT(secs < 1.0f && peak(big) <= 1.0f, "16 notes x 2 oscillators x 8 voices: 2 s of sound in %.0f ms (%.0f%% of one core)", secs * 1000, secs / 2 * 100);
    }
    {
        // the filter: how loud the harmonics of a bright shape (the cube) are below 500 Hz and above 2 kHz
        auto harm = [&](const std::vector<float>& x, int from, int len, float f) {  // Goertzel: one frequency's power
            const double w = 2 * M_PI * f / sr, c = 2 * std::cos(w);
            double s1 = 0, s2 = 0;
            for (int i = from; i < from + len; ++i) { const double s0 = x[i] + c * s1 - s2; s2 = s1; s1 = s0; }
            return s1 * s1 + s2 * s2 - c * s1 * s2;
        };
        auto split = [&](const std::vector<float>& x, int from, int len, double& lowE, double& highE) {
            lowE = highE = 0;
            for (int h = 1; h <= 60; ++h) {
                const double e = harm(x, from, len, 110.0f * h);
                if (110 * h < 500) lowE += e;
                else if (110 * h > 2000) highE += e;
            }
        };
        auto bands = [&](double& lowE, double& highE) {
            Engine e(P); e.setSampleRate(sr);
            e.noteOn(45, 127);  // A2, 110 Hz: plenty of harmonics both sides
            std::vector<float> r;
            auto l = run(e, 24000, &r);
            split(l, 4800, 19200, lowE, highE);  // 19200 samples = whole cycles of 110 Hz at 48 kHz (44)
            return peak(l);
        };
        defaults();
        slotsOf(0, {10});
        double open0, open1, lo0, lo1, hi0, hi1, bp0, bp1;
        bands(open0, open1);
        P[kCutoff] = 500;
        float pk = bands(lo0, lo1);
        EXPECT(lo1 / lo0 < 0.1 * open1 / open0, "LP12 at 500 Hz: highs drop (high/low %.4f -> %.4f)", open1 / open0, lo1 / lo0);
        P[kFilterType] = 1;
        double l24a, l24b;
        bands(l24a, l24b);
        EXPECT(l24b / l24a < 0.5 * lo1 / lo0, "LP24 cuts harder than LP12 (%.5f vs %.4f)", l24b / l24a, lo1 / lo0);
        P[kFilterType] = 2; P[kCutoff] = 2000;
        bands(hi0, hi1);
        EXPECT(hi1 / hi0 > 3 * open1 / open0, "HP at 2 kHz: lows drop (high/low %.4f -> %.4f)", open1 / open0, hi1 / hi0);
        P[kFilterType] = 0; P[kCutoff] = 1000; P[kRes] = 1;
        pk = bands(bp0, bp1);
        EXPECT(pk <= 1.0f && std::isfinite(pk), "full resonance stays stable, peak %.2f", pk);
        // ENV linked to cutoff: bright at the start of the note, dark once it decays
        defaults();
        slotsOf(0, {10});
        P[kCutoff] = 200; P[kSustain] = 0; P[kDecay] = 0.2f;
        P[modParam(kSrcEnv, destIndex(kCutoff))] = 0.8f;
        Engine e(P); e.setSampleRate(sr);
        e.noteOn(45, 127);
        std::vector<float> r;
        auto l = run(e, 9600, &r);
        double e0, e1, d0, d1;
        split(l, 0, 1309, e0, e1);   // ~12 cycles at the start
        split(l, 8291, 1309, d0, d1);  // and once it has decayed
        EXPECT(e1 / e0 > 10 * d1 / d0, "ENV -> CUTOFF sweeps it down (high/low %.4f at the start, %.6f later)", e1 / e0, d1 / d0);
    }
    {
        // typed equations
        Expr x;
        EXPECT(x.compile("sin(3t) + 0.5cos(2*t)^2").empty() && std::fabs(x.eval(1, 0) - (std::sin(3) + 0.5 * std::pow(std::cos(2), 2))) < 1e-9,
               "equation: sin(3t) + 0.5cos(2*t)^2 reads right");
        EXPECT(x.compile("-2^2").empty() && x.eval(0, 0) == -4, "equation: -2^2 = -4");
        EXPECT(x.compile("max(t, u) * pi").empty() && x.animated() && std::fabs(x.eval(1, 2) - 2 * M_PI) < 1e-9, "equation: max(t, u) * pi, uses u so it moves");
        const char* bad[] = {"sin(t", "foo(t)", "3 +", "max(t)", "t $ 2", ""};
        for (const char* b : bad) {
            const std::string err = x.compile(b);
            EXPECT(!err.empty(), "equation: \"%s\" refused (%s)", b, err.c_str());
        }
        // a typed shape plays in key
        defaults();
        gSet.customs.resize(1);
        gSet.customs[0].xs = "sin(t) + 0.3sin(3t)"; gSet.customs[0].ys = "cos(2t)";
        EXPECT(gSet.customs[0].compile().empty(), "custom shape compiles");
        gSet.customs[0].build();
        slotsOf(0, {crt::kNumEquations});
        Engine e(P); e.setSampleRate(sr);
        e.noteOn(57, 127);
        std::vector<float> r;
        auto l = run(e, 48000, &r);
        for (size_t i = 0; i < l.size(); ++i) r[i] += l[i];
        EXPECT(std::fabs(pitchOf(r, sr) / 220 - 1) < 0.02f && peak(l) > 0.2f, "MY SHAPE 1 at A3: %.1f Hz, peak %.2f", pitchOf(r, sr), peak(l));
        // save and load
        ShapeSet back;
        back.load(gSet.save());
        EXPECT(back.save() == gSet.save() && back.customs.size() == 1 && back.custom(crt::kNumEquations).X == gSet.custom(crt::kNumEquations).X,
               "typed shapes saved and loaded back: %s", back.save().c_str());
        // MORPH through a playlist: half way between HEART and CUBE is neither
        defaults();
        slotsOf(0, {1, 10});
        auto energy = [&](float morph) {
            P[kAShape] = morph;
            Engine m(P); m.setSampleRate(sr);
            m.noteOn(57, 127);
            std::vector<float> rr;
            auto ll = run(m, 24000, &rr);
            double el = 0;
            for (int i = 12000; i < 24000; ++i) el += ll[i] * rr[i];
            return el / 12000;
        };
        const double h = energy(0), c = energy(1), half = energy(0.5f);
        EXPECT(std::fabs(h - c) > 1e-4 && std::fabs(half - h) > 1e-5 && std::fabs(half - c) > 1e-5,
               "MORPH 0 / 50%% / 100%% through HEART > CUBE all differ (L*R %.4f %.4f %.4f)", h, half, c);
    }
    {
        // DRAW + SYNC: an arp. 120 bpm, 1/16 a step, steps 0 4 7 12 semitones (LFO -> SEMI at +50% = 1 step of 1/12 a semitone)
        defaults();
        P[kLfoDraw] = 1; P[kLfoSync] = 1; P[kLfoDiv] = 4; P[kLfoSteps] = 4;
        const float st[4] = {0, 4, 7, 12};
        for (int i = 0; i < 4; ++i) P[kLfoStep0 + i] = st[i] / 12;
        P[modParam(kSrcLfo, destIndex(kASemi))] = 0.5f;
        P[kSustain] = 1;
        Engine e(P); e.setSampleRate(sr);
        e.setTransport(true, 0, 120);
        e.noteOn(57, 127);
        std::vector<float> r;
        auto l = run(e, 6000 * 8, &r);
        bool ok = true;
        char got[96] = "";
        for (int w = 0; w < 8; ++w) {
            const float want = 220.0f * std::exp2(st[w % 4] / 12), f = pitchAt(l, r, w * 6000 + 1500, 3000, sr);
            ok = ok && std::fabs(f / want - 1) < 0.02f;
            std::snprintf(got + std::strlen(got), sizeof(got) - std::strlen(got), " %.0f", f);
        }
        EXPECT(ok, "arp, synced at 120 bpm, 1/16: A C# E A twice:%s Hz", got);
        // free running (no SYNC): RATE = steps a second, and a new press starts from step 1
        P[kLfoSync] = 0; P[kLfoRate] = 8;
        Engine f(P); f.setSampleRate(sr);
        run(f, 1000);  // the LFO has moved on before the key goes down
        f.noteOn(57, 127);
        auto l2 = run(f, 6000 * 4, &r);
        const float a0 = pitchAt(l2, r, 1000, 3000, sr), a1 = pitchAt(l2, r, 6000 + 1000, 3000, sr);
        EXPECT(std::fabs(a0 / 220 - 1) < 0.02f && std::fabs(a1 / 277.18f - 1) < 0.02f, "arp, free at 8 steps/s: starts on step 1 (%.0f, then %.0f Hz)", a0, a1);
    }
    // every factory preset: its typed shapes compile, a note and a chord make sound,
    // cleanly, it rings out, and no two presets are the same
    {
        std::vector<std::vector<float>> seen;
        int quiet = 0, badShapes = 0, dupes = 0, stuck = 0, loud = 0;
        for (const Preset& pr : presets()) {
            pr.values(P);
            pr.shapesInto(gSet);
            for (const auto& c : gSet.customs) if (!c.x.ok() || !c.y.ok()) { ++badShapes; std::printf("  bad shape in %s\n", pr.name); }
            std::vector<float> v(P, P + kParamCount);
            for (const auto& xy : pr.shapes) v.push_back((float)std::hash<std::string>()(std::string(xy.first) + xy.second));
            for (const auto& o : seen) if (o == v) { ++dupes; std::printf("  %s is the same as another\n", pr.name); }
            seen.push_back(v);
            Engine e(P); e.setSampleRate(sr);
            e.noteOn(48, 110);
            auto one = run(e, (int)(sr * std::clamp(P[kAttack] + 0.5f, 0.5f, 4.5f)));  // slow pads and risers get their attack
            e.noteOn(52, 110); e.noteOn(55, 110); e.noteOn(60, 110);
            auto chord = run(e, 48000);
            for (int n : {48, 52, 55, 60}) e.noteOff(n);
            for (int i = 0; i < 24 && e.activeVoices() > 0; ++i) run(e, 24000);  // up to 12 s to ring out
            double s2 = 0; for (float x : one) s2 += x * x;
            const float rms = (float)std::sqrt(s2 / one.size());
            const bool ok = peak(one) > 0.02f && rms > 0.003f && peak(chord) <= 1.0f && e.activeVoices() == 0;
            if (rms > 0.35f) ++loud;
            if (!ok || rms > 0.35f)
                std::printf("  %-9s %-16s note peak %.2f rms %.3f, chord peak %.2f, %d voices left\n", pr.category, pr.name, peak(one), rms, peak(chord), e.activeVoices());
            quiet += peak(one) <= 0.02f || rms <= 0.003f;
            stuck += e.activeVoices() != 0;
        }
        EXPECT(!quiet && !badShapes && !dupes && !stuck && !loud,
               "%zu presets: all play and ring out (%d too quiet, %d too loud, %d with bad shapes, %d the same, %d stuck)",
               presets().size(), quiet, loud, badShapes, dupes, stuck);
    }
    std::printf(fails ? "%d FAILED\n" : "ALL OK\n", fails);
    return fails != 0;
}
