// Checks the sound engine on its own: g++ -std=c++17 -O2 -I. tests/engine_test.cpp -o engine_test && ./engine_test
#include "SynthEngine.hpp"
#include <chrono>
#include <cstdio>
#include <vector>

using namespace ss;
static float P[kParamCount];
static int fails = 0;
#define EXPECT(c, ...) do { std::printf("%s: ", (c) ? "ok  " : "FAIL"); std::printf(__VA_ARGS__); std::printf("\n"); if (!(c)) ++fails; } while (0)

static void defaults()
{
    for (int p = 0; p < kParamCount; ++p) P[p] = paramDefault(p);
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

static std::vector<float> run(Engine& e, int frames, std::vector<float>* right = nullptr)
{
    std::vector<float> l(frames), r(frames);
    for (int i = 0; i < frames; i += 512) {
        const int m = std::min(512, frames - i);
        e.render(&l[i], &r[i], m);
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
        P[kAShape] = (float)eq;
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
        e.render(l.data(), r.data(), 2400);
        // count rising zero crossings in the first 50 ms vs a plain note
        auto cross = [](const std::vector<float>& x) { int c = 0; for (size_t i = 1; i < x.size(); ++i) c += x[i - 1] < 0 && x[i] >= 0; return c; };
        P[modParam(kSrcEnv, destIndex(kASemi))] = 0;
        Engine plain(P); plain.setSampleRate(sr);
        plain.noteOn(57, 127);
        std::vector<float> l2(2400), r2(2400);
        plain.render(l2.data(), r2.data(), 2400);
        EXPECT(cross(l) > cross(l2) * 1.5, "ENV -> SEMI raises the pitch at the start (%d vs %d crossings)", cross(l), cross(l2));
        P[kSustain] = paramDefault(kSustain); P[kDecay] = paramDefault(kDecay);
    }
    {
        // LFO linked to ROTATE: the shape turns, so left/right energy trades places over time
        P[modParam(kSrcLfo, destIndex(kARot))] = 0.5f; P[kLfoRate] = 1; P[kAShape] = 5;  // infinity: wide, flat
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
        P[modParam(kSrcLfo, destIndex(kARot))] = 0; P[kAShape] = 0;
    }
    {
        // both oscillators, 16 voices of an animated curve: how long it takes
        P[kAShape] = 10; P[kBShape] = 6.5f; P[kBLevel] = 0.6f;
        P[modParam(kSrcLfo, destIndex(kAShape))] = 0.2f;
        Engine e(P); e.setSampleRate(sr);
        for (int n = 40; n < 56; ++n) e.noteOn(n, 100);
        const auto t0 = std::chrono::steady_clock::now();
        auto l = run(e, 48000 * 4);
        const float secs = std::chrono::duration<float>(std::chrono::steady_clock::now() - t0).count();
        EXPECT(secs < 0.4f && peak(l) <= 1.0f, "16 voices x 2 oscillators, 4 s of sound in %.0f ms (%.1f%% of one core)", secs * 1000, secs / 4 * 100);
    }
    std::printf(fails ? "%d FAILED\n" : "ALL OK\n", fails);
    return fails != 0;
}
