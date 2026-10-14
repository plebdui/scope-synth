// The window: the CRT tube on the left showing what you hear, the two
// oscillators (each with a little CRT TV showing its shape), the ENV and LFO,
// voice settings, and a keyboard along the bottom.
// Linking like Serum: drag the ENV or LFO handle onto a knob. The knob grows
// a coloured ring and a tag under it; drag the tag up/down to set how much,
// right-click the tag to unlink.
#include "DistrhoUI.hpp"
#include "ScopeSynthShared.hpp"
#include "CrtTube.hpp"
#include "OpenGL.hpp"

#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

START_NAMESPACE_DISTRHO

using namespace ss;

// layout, in window units (multiplied by the host's scale factor)
static constexpr float kW = 960, kH = 640;
static constexpr float kTube = 440;          // the tube, top-left, square
static constexpr float kRowH = 110;          // oscillator / ENV / LFO rows on the right
static constexpr float kStripY = 440, kStripH = 90;
static constexpr float kKeysY = 530, kKeysH = 110;
static constexpr int kWhiteKeys = 29;        // four octaves and a top C

static const Color kBg(22, 23, 26), kPanel(30, 32, 36), kEdge(52, 54, 60), kLabel(120, 128, 124), kOff(80, 84, 82);
static const Color kAccent(70, 250, 110);    // P1 green, like the tube
static const Color kSrcCol[kNumSources] = {Color(255, 160, 50), Color(70, 190, 255)};  // ENV orange, LFO blue
static const char* const kSrcName[kNumSources] = {"ENV", "LFO"};

struct Ctl {
    int param;
    float x, y, w, h;
};

struct Rect {
    float x, y, w, h;
    bool has(float px, float py) const { return px >= x && px < x + w && py >= y && py < y + h; }
};

static const Rect kHandle[kNumSources] = {{866, 2 * kRowH + 26, 86, 76}, {866, 3 * kRowH + 26, 86, 76}};
static const Rect kOctDown = {kW - 150, kStripY + 30, 34, 34}, kOctUp = {kW - 46, kStripY + 30, 34, 34};

class ScopeSynthUI : public UI {
public:
    ScopeSynthUI()
        : UI((uint)kW, (uint)kH)
    {
        loadSharedResources();
        for (int p = 0; p < kParamCount; ++p)
            fValues[p] = paramDefault(p);
        // oscillators: six knobs each, right of their TV
        for (int o = 0; o < 2; ++o) {
            const int b = o * (kBShape - kAShape);
            for (int i = 0; i < 6; ++i)
                fCtls.push_back({kAShape + b + i, 572 + i * 64.0f, o * kRowH + 18, 64, 92});
        }
        for (int i = 0; i < 4; ++i)
            fCtls.push_back({kAttack + i, 446 + i * 64.0f, 2 * kRowH + 18, 64, 92});
        fCtls.push_back({kLfoRate, 446, 3 * kRowH + 18, 64, 92});
        fCtls.push_back({kLfoWave, 512, 3 * kRowH + 18, 84, 92});
        fCtls.push_back({kPoly, 8, kStripY, 90, kStripH});
        fCtls.push_back({kLegato, 100, kStripY, 90, kStripH});
        fCtls.push_back({kGlide, 192, kStripY, 80, kStripH});
        fCtls.push_back({kVolume, 274, kStripY, 80, kStripH});

        fTube.mode = crt::kXY;
        fTube.phosphor = 1;  // P1 green
        fTube.flaws = 1;
        fTube.autoGain = true;
        fLast = std::chrono::steady_clock::now();
        if (const char* p = std::getenv("SCOPESYNTH_DUMP"))  // testing: save frames to this file
            fDumpPath = p;
    }

protected:
    // ---- host -> window
    void parameterChanged(uint32_t index, float value) override
    {
        if (index < (uint32_t)kParamCount) {
            fValues[index] = value;
            repaint();
        }
    }

    // ---- called by the host many times a second: pull in new sound, move the beam
    void uiIdle() override
    {
        if (ScopeSynthPlugin* plugin = static_cast<ScopeSynthPlugin*>(getPluginInstancePointer())) {
            AudioRing& ring = plugin->ring;
            const uint32_t written = ring.written.load(std::memory_order_acquire);
            uint32_t avail = written - fRead;
            if (avail > AudioRing::kSize / 2) {  // fell far behind (window was hidden): skip ahead
                fRead = written - AudioRing::kSize / 4;
                avail = AudioRing::kSize / 4;
            }
            float l[1024], r[1024];
            while (avail > 0) {
                const uint32_t n = std::min<uint32_t>(avail, 1024);
                for (uint32_t i = 0; i < n; ++i) {
                    l[i] = ring.left[(fRead + i) & AudioRing::kMask];
                    r[i] = ring.right[(fRead + i) & AudioRing::kMask];
                }
                fTube.feed(l, r, (int)n, (float)getSampleRate());
                fRead += n;
                avail -= n;
            }
            const LiveState& lv = plugin->live;
            fEnv = lv.env.load(std::memory_order_relaxed);
            fLfo = lv.lfo.load(std::memory_order_relaxed);
            fNote = lv.note.load(std::memory_order_relaxed);
            fPitch = lv.pitch.load(std::memory_order_relaxed);
            fVoices = lv.voices.load(std::memory_order_relaxed);
            fAnim = lv.anim.load(std::memory_order_relaxed);
            fKeys[0] = lv.keys[0].load(std::memory_order_relaxed);
            fKeys[1] = lv.keys[1].load(std::memory_order_relaxed);
        }
        const auto now = std::chrono::steady_clock::now();
        const float dt = std::chrono::duration<float>(now - fLast).count();
        fLast = now;
        fTube.step(dt);
        repaint();
    }

    void onNanoDisplay() override
    {
        const float s = (float)getScaleFactor();
        const uint tw = (uint)std::lround(kTube * s);
        fTube.resize((int)tw, (int)tw);
        const uint8_t* pixels = fTube.render();
        if (!fImage.isValid() || fImageW != tw) {
            fImage = createImageFromRGBA(tw, tw, pixels, 0);
            fImageW = tw;
        } else {
            fImage.update(pixels);
        }
        beginPath();
        rect(0, 0, getWidth(), getHeight());
        fillColor(kBg);
        fill();
        beginPath();
        rect(0, 0, tw, tw);
        fillPaint(imagePattern(0, 0, tw, tw, 0, fImage, 1.0f));
        fill();

        save();
        scale(s, s);
        fontFace(NANOVG_DEJAVU_SANS_TTF);
        drawTubeLabels();
        for (int o = 0; o < 2; ++o)
            drawOscPanel(o);
        drawEnvPanel();
        drawLfoPanel();
        drawStrip();
        drawKeyboard();
        for (const Ctl& c : fCtls)
            drawCtl(c);
        if (fLinkSrc >= 0) {  // dragging a link: a line from the handle to the pointer
            const Rect& h = kHandle[fLinkSrc];
            beginPath();
            moveTo(h.x + h.w / 2, h.y + h.h / 2);
            lineTo(fMouseX, fMouseY);
            strokeColor(kSrcCol[fLinkSrc]);
            strokeWidth(2);
            stroke();
            beginPath();
            circle(fMouseX, fMouseY, 5);
            fillColor(kSrcCol[fLinkSrc]);
            fill();
        }
        restore();

        // testing hook: snapshot the window 3s, 7s, 13s and 19.5s after opening
        if (!fDumpPath.empty() && fDumps < 4) {
            static const float when[4] = {3.0f, 7.0f, 13.0f, 19.5f};
            const float age = std::chrono::duration<float>(std::chrono::steady_clock::now() - fOpened).count();
            if (age > when[fDumps]) {
                static_cast<NanoVG*>(this)->endFrame();
                dumpFrontBuffer(fDumpPath + ".full" + std::to_string(fDumps + 1) + ".ppm");
                static_cast<NanoVG*>(this)->beginFrame(getWidth(), getHeight());
                ++fDumps;
            }
        }
    }

    bool onMouse(const MouseEvent& ev) override
    {
        const float s = (float)getScaleFactor();
        const float x = (float)ev.pos.getX() / s, y = (float)ev.pos.getY() / s;
        fMouseX = x;
        fMouseY = y;
        if (!ev.press) {
            if (fKbNote >= 0) {
                playNote(fKbNote, 0);
                fKbNote = -1;
            }
            if (fLinkSrc >= 0) {  // dropped a link on a knob
                const Ctl* c = ctlAt(x, y);
                const int d = c ? destIndex(c->param) : -1;
                if (d >= 0 && fValues[modParam(fLinkSrc, d)] == 0)
                    setValue(modParam(fLinkSrc, d), 0.5f, true);
                fLinkSrc = -1;
                repaint();
            }
            if (fDragParam >= 0) {
                editParameter(fDragParam, false);
                fDragParam = -1;
            }
            return true;
        }
        for (int src = 0; src < kNumSources; ++src)
            if (ev.button == 1 && kHandle[src].has(x, y)) {
                fLinkSrc = src;
                return true;
            }
        if (y >= kKeysY) {
            if (ev.button != 1) return false;
            fKbNote = keyAt(x, y);
            if (fKbNote >= 0) playNote(fKbNote, velocityAt(y));
            return true;
        }
        if (ev.button == 1 && (kOctDown.has(x, y) || kOctUp.has(x, y))) {
            fKbLow = std::clamp(fKbLow + (kOctUp.has(x, y) ? 12 : -12), 12, 72);
            repaint();
            return true;
        }
        // a link's tag: drag to set how much, right-click to unlink
        for (const Ctl& c : fCtls)
            for (int src = 0; src < kNumSources; ++src) {
                Rect t{};
                if (!tagRect(c, src, t) || !t.has(x, y)) continue;
                const int mp = modParam(src, destIndex(c.param));
                if (ev.button == 3) {
                    setValue(mp, 0, true);
                } else if (ev.button == 1) {
                    startDrag(mp, y);
                }
                return true;
            }
        const Ctl* c = ctlAt(x, y);
        if (!c) return false;
        const Kind kind = kInfo[c->param].kind;
        if (kind == kSwitch) {
            setValue(c->param, fValues[c->param] > 0.5f ? 0.0f : 1.0f, true);
        } else if (kind == kChoice) {
            const int n = (int)paramMax(c->param) + 1;
            setValue(c->param, (float)(((int)std::lround(fValues[c->param]) + (ev.button == 3 ? -1 : 1) + n) % n), true);
        } else if (ev.button == 3) {  // right-click a knob: back to its default
            setValue(c->param, paramDefault(c->param), true);
        } else if (ev.button == 1) {
            startDrag(c->param, y);
        }
        return true;
    }

    bool onMotion(const MotionEvent& ev) override
    {
        const float s = (float)getScaleFactor();
        const float x = (float)ev.pos.getX() / s, y = (float)ev.pos.getY() / s;
        fMouseX = x;
        fMouseY = y;
        if (fLinkSrc >= 0) {
            repaint();
            return true;
        }
        if (fKbNote >= 0) {  // slide along the keys
            const int k = y >= kKeysY ? keyAt(x, y) : -1;
            if (k != fKbNote && k >= 0) {
                playNote(fKbNote, 0);
                fKbNote = k;
                playNote(k, velocityAt(y));
            }
            return true;
        }
        if (fDragParam < 0) return false;
        const int p = fDragParam;
        const float dy = (fDragY - y) / 200.0f;
        float v;
        if (p >= kModBase) {
            v = std::clamp(fDragStart + dy * 2, -1.0f, 1.0f);
            if (std::fabs(v) < 0.02f) v = 0.0001f;  // keep the link while passing through 0
        } else {
            v = fromNorm(p, toNorm(p, fDragStart) + dy);
            if (kInfo[p].kind == kInt) v = std::round(v);
        }
        setValue(p, v, false);
        return true;
    }

    bool onScroll(const ScrollEvent& ev) override
    {
        const float s = (float)getScaleFactor();
        const Ctl* c = ctlAt((float)ev.pos.getX() / s, (float)ev.pos.getY() / s);
        if (!c || ev.delta.getY() == 0) return false;
        const int p = c->param;
        const float dir = ev.delta.getY() > 0 ? 1.0f : -1.0f;
        const Kind kind = kInfo[p].kind;
        float v;
        if (kind == kSwitch) v = dir > 0 ? 1.0f : 0.0f;
        else if (kind == kInt || kind == kChoice) v = std::clamp(std::round(fValues[p] + dir), paramMin(p), paramMax(p));
        else v = fromNorm(p, toNorm(p, fValues[p]) + dir * 0.02f);
        setValue(p, v, true);
        return true;
    }

private:
    crt::Tube fTube;
    float fValues[kParamCount];
    std::vector<Ctl> fCtls;
    uint32_t fRead = 0;
    std::chrono::steady_clock::time_point fLast, fOpened = std::chrono::steady_clock::now();
    NanoImage fImage;
    uint fImageW = 0;
    std::string fDumpPath;
    int fDumps = 0;
    // live from the plugin
    float fEnv = 0, fLfo = 0, fPitch = 0;
    int fNote = -1, fVoices = 0;
    double fAnim = 0;
    uint64_t fKeys[2] = {0, 0};
    // mouse
    float fMouseX = 0, fMouseY = 0;
    int fDragParam = -1;
    float fDragY = 0, fDragStart = 0;
    int fLinkSrc = -1;
    int fKbNote = -1, fKbLow = 36;  // keyboard starts at C2

    void startDrag(int p, float y)
    {
        fDragParam = p;
        fDragY = y;
        fDragStart = fValues[p];
        editParameter(p, true);
    }

    // set a value and tell the host (gesture: wrap it in begin/end, for a single click)
    void setValue(int p, float v, bool gesture)
    {
        v = std::clamp(v, paramMin(p), paramMax(p));
        fValues[p] = v;
        if (gesture) editParameter(p, true);
        setParameterValue(p, v);
        if (gesture) editParameter(p, false);
        repaint();
    }

    void playNote(int note, int velocity)
    {
        if (ScopeSynthPlugin* plugin = static_cast<ScopeSynthPlugin*>(getPluginInstancePointer()))
            plugin->uiNotes.push(note, velocity);
        repaint();
    }

    const Ctl* ctlAt(float x, float y) const
    {
        for (const Ctl& c : fCtls)
            if (Rect{c.x, c.y, c.w, c.h}.has(x, y)) return &c;
        return nullptr;
    }

    // ---- keyboard
    static float whiteW() { return kW / kWhiteKeys; }
    static bool hasBlackAfter(int white) { const int i = white % 7; return i != 2 && i != 6; }  // none after E and B
    int whiteNote(int white) const
    {
        static const int st[7] = {0, 2, 4, 5, 7, 9, 11};
        return fKbLow + 12 * (white / 7) + st[white % 7];
    }
    Rect blackRect(int white) const
    {
        const float w = whiteW() * 0.6f;
        return {(white + 1) * whiteW() - w / 2, kKeysY, w, kKeysH * 0.6f};
    }
    int keyAt(float x, float y) const
    {
        for (int i = 0; i < kWhiteKeys - 1; ++i)
            if (hasBlackAfter(i) && blackRect(i).has(x, y)) return whiteNote(i) + 1;
        const int i = std::clamp((int)(x / whiteW()), 0, kWhiteKeys - 1);
        return whiteNote(i);
    }
    static int velocityAt(float y) { return std::clamp((int)(40 + 87 * (y - kKeysY) / kKeysH), 1, 127); }  // lower = harder
    bool lit(int note) const { return note == fKbNote || ((fKeys[(note & 127) >> 6] >> (note & 63)) & 1); }

    void drawKeyboard()
    {
        const float ww = whiteW();
        for (int i = 0; i < kWhiteKeys; ++i) {
            const int n = whiteNote(i);
            beginPath();
            roundedRect(i * ww + 1, kKeysY, ww - 2, kKeysH - 2, 3);
            if (lit(n))
                fillPaint(linearGradient(0, kKeysY, 0, kKeysY + kKeysH, Color(150, 255, 170), kAccent));
            else
                fillPaint(linearGradient(0, kKeysY, 0, kKeysY + kKeysH, Color(205, 208, 202), Color(240, 242, 236)));
            fill();
            if (n % 12 == 0) {
                char buf[8];
                std::snprintf(buf, sizeof(buf), "C%d", n / 12 - 1);
                fontSize(9);
                textAlign(ALIGN_CENTER | ALIGN_BOTTOM);
                fillColor(Color(90, 94, 92));
                text(i * ww + ww / 2, kKeysY + kKeysH - 6, buf, nullptr);
            }
        }
        for (int i = 0; i < kWhiteKeys - 1; ++i) {
            if (!hasBlackAfter(i)) continue;
            const Rect r = blackRect(i);
            beginPath();
            roundedRect(r.x, r.y, r.w, r.h, 2);
            if (lit(whiteNote(i) + 1))
                fillPaint(linearGradient(0, r.y, 0, r.y + r.h, Color(40, 150, 70), kAccent));
            else
                fillPaint(linearGradient(0, r.y, 0, r.y + r.h, Color(20, 21, 24), Color(58, 60, 66)));
            fill();
        }
        beginPath();  // shadow along the top edge, under the panel
        rect(0, kKeysY, kW, 5);
        fillPaint(linearGradient(0, kKeysY, 0, kKeysY + 5, Color(0, 0, 0, 0.63f), Color(0, 0, 0, 0.00f)));
        fill();
    }

    // ---- panels
    void panel(float x, float y, float w, float h, const char* title, const char* sub = nullptr)
    {
        beginPath();
        roundedRect(x + 3, y + 3, w - 6, h - 6, 5);
        fillColor(kPanel);
        fill();
        strokeColor(kEdge);
        strokeWidth(1);
        stroke();
        fontSize(10);
        textAlign(ALIGN_LEFT | ALIGN_MIDDLE);
        fillColor(kLabel);
        const float tw = text(x + 10, y + 13, title, nullptr);
        if (sub) {
            fillColor(kAccent);
            text(tw + 8, y + 13, sub, nullptr);
        }
    }

    // the shape an oscillator is playing, as its knobs (and links) set it right now
    void oscShape(int o, std::vector<float>& xs, std::vector<float>& ys, float& level) const
    {
        const int b = o * (kBShape - kAShape);
        const float shape = std::clamp(liveValue(kAShape + b), 0.0f, (float)(crt::kNumEquations - 1));
        const int e0 = std::min((int)shape, crt::kNumEquations - 1), e1 = std::min(e0 + 1, crt::kNumEquations - 1);
        const float m = shape - e0, rot = liveValue(kARot + b) * 0.017453293f;
        const float c = std::cos(rot), s = std::sin(rot);
        level = liveValue(kALevel + b);
        const int n = 600;
        xs.resize(n);
        ys.resize(n);
        for (int i = 0; i < n; ++i) {
            float x0, y0, x1, y1;
            crt::equation(e0, i / (double)(n - 1), fAnim, x0, y0);
            crt::equation(e1, i / (double)(n - 1), fAnim, x1, y1);
            const float x = x0 + (x1 - x0) * m, y = y0 + (y1 - y0) * m;
            xs[i] = x * c - y * s;
            ys[i] = x * s + y * c;
        }
    }

    // a value as the ENV and LFO are moving it right now (newest note)
    float liveValue(int p) const
    {
        const int d = destIndex(p);
        if (d < 0) return fValues[p];
        const float env = fNote >= 0 ? fEnv : 0.0f;
        return fromNorm(p, toNorm(p, fValues[p]) + fValues[modParam(kSrcEnv, d)] * env + fValues[modParam(kSrcLfo, d)] * fLfo);
    }

    void drawOscPanel(int o)
    {
        const float y0 = o * kRowH;
        const int b = o * (kBShape - kAShape);
        const float shape = std::clamp(fValues[kAShape + b], 0.0f, (float)(crt::kNumEquations - 1));
        const int e0 = (int)std::lround(shape);
        char sub[64];
        const float frac = shape - std::floor(shape);
        if (frac > 0.04f && frac < 0.96f)
            std::snprintf(sub, sizeof(sub), "%s > %s", crt::kEquationInfo[(int)shape].name,
                          crt::kEquationInfo[std::min((int)shape + 1, crt::kNumEquations - 1)].name);
        else
            std::snprintf(sub, sizeof(sub), "%s", crt::kEquationInfo[e0].name);
        panel(kTube, y0, kW - kTube, kRowH, o == 0 ? "OSC A" : "OSC B", sub);
        drawTv(o, {448, y0 + 22, 118, 84});
    }

    // A little CRT television: plastic bezel, curved glass, the shape bent by
    // the curve of the glass, glow, scanlines and dark corners.
    void drawTv(int o, const Rect& r)
    {
        beginPath();  // bezel
        roundedRect(r.x, r.y, r.w, r.h, 9);
        fillPaint(linearGradient(r.x, r.y, r.x, r.y + r.h, Color(70, 66, 60), Color(34, 32, 30)));
        fill();
        strokeColor(Color(12, 12, 12));
        strokeWidth(1);
        stroke();
        const Rect g = {r.x + 7, r.y + 6, r.w - 14, r.h - 12};
        const float cx = g.x + g.w / 2, cy = g.y + g.h / 2;
        std::vector<float> xs, ys;
        float level;
        oscShape(o, xs, ys, level);
        const bool on = level > 0.001f;
        beginPath();  // the glass, glowing faintly when there's a picture
        roundedRect(g.x, g.y, g.w, g.h, 14);
        fillPaint(radialGradient(cx, cy, 4, g.w * 0.65f, on ? Color(18, 40, 24) : Color(14, 16, 15), Color(4, 6, 5)));
        fill();
        save();
        intersectScissor(g.x, g.y, g.w, g.h);
        if (on) {
            // barrel distortion: the further from the middle, the more it bulges out
            const float sx = g.w * 0.5f / 1.05f, sy = g.h * 0.5f / 1.05f;
            auto warp = [&](float x, float y, float& px, float& py) {
                const float k = 1 + 0.16f * (x * x + y * y);
                px = cx + x * k * sx;
                py = cy - y * k * sy;
            };
            const float bright = std::clamp(0.35f + 0.65f * level / 0.8f, 0.35f, 1.0f);
            for (int pass = 0; pass < 2; ++pass) {  // wide soft glow, then the sharp beam
                beginPath();
                for (size_t i = 0; i < xs.size(); ++i) {
                    float px, py;
                    warp(xs[i], ys[i], px, py);
                    if (i == 0) moveTo(px, py); else lineTo(px, py);
                }
                lineJoin(ROUND);
                strokeColor(pass == 0 ? Color(kAccent.red, kAccent.green, kAccent.blue, 0.22f * bright)
                                      : Color(0.75f, 1.0f, 0.8f, 0.9f * bright));
                strokeWidth(pass == 0 ? 4.5f : 1.1f);
                stroke();
            }
        } else {
            fontSize(9);
            textAlign(ALIGN_CENTER | ALIGN_MIDDLE);
            fillColor(kOff);
            text(cx, cy, "LEVEL 0", nullptr);
        }
        for (float y = g.y + 1; y < g.y + g.h; y += 2.5f) {  // scanlines
            beginPath();
            rect(g.x, y, g.w, 1);
            fillColor(Color(0, 0, 0, 0.35f));
            fill();
        }
        restore();
        beginPath();  // dark corners where the glass curves away
        roundedRect(g.x, g.y, g.w, g.h, 14);
        fillPaint(boxGradient(g.x + 4, g.y + 3, g.w - 8, g.h - 6, 16, 14, Color(0, 0, 0, 0.00f), Color(0, 0, 0, 0.78f)));
        fill();
        beginPath();  // reflection on the glass
        roundedRect(g.x, g.y, g.w, g.h, 14);
        fillPaint(linearGradient(g.x, g.y, g.x + g.w * 0.6f, g.y + g.h, Color(255, 255, 255, 0.12f), Color(255, 255, 255, 0.00f)));
        fill();
        beginPath();  // power lamp
        circle(r.x + r.w - 8, r.y + r.h - 4, 1.6f);
        fillColor(on ? Color(255, 60, 40) : Color(70, 30, 25));
        fill();
    }

    void drawEnvPanel()
    {
        const float y0 = 2 * kRowH;
        panel(kTube, y0, kW - kTube, kRowH, "ENV", "also each note's volume");
        // the envelope's shape (times on a log-ish scale so short and long both show)
        const Rect g = {708, y0 + 26, 150, 74};
        graphBox(g);
        auto tx = [](float t) { return std::sqrt(std::clamp(t, 0.0f, 10.0f) / 10.0f); };
        const float a = tx(fValues[kAttack]), d = tx(fValues[kDecay]), r = tx(fValues[kRelease]);
        const float total = a + d + 0.35f + r, sus = fValues[kSustain];
        auto X = [&](float t) { return g.x + 4 + (g.w - 8) * t / total; };
        auto Y = [&](float v) { return g.y + g.h - 4 - (g.h - 10) * v; };
        beginPath();
        moveTo(X(0), Y(0));
        lineTo(X(a), Y(1));
        lineTo(X(a + d), Y(sus));
        lineTo(X(a + d + 0.35f), Y(sus));
        lineTo(X(total), Y(0));
        strokeColor(kSrcCol[kSrcEnv]);
        strokeWidth(1.6f);
        stroke();
        if (fNote >= 0) {  // where the newest note's envelope is now
            beginPath();
            rect(g.x + 2, Y(fEnv), g.w - 4, 1);
            fillColor(Color(255, 160, 50, 0.47f));
            fill();
        }
        drawHandle(kSrcEnv);
    }

    void drawLfoPanel()
    {
        const float y0 = 3 * kRowH;
        panel(kTube, y0, kW - kTube, kRowH, "LFO", "one for all notes");
        const Rect g = {602, y0 + 26, 256, 74};
        graphBox(g);
        const int wave = (int)std::lround(fValues[kLfoWave]);
        beginPath();
        for (int i = 0; i <= 120; ++i) {
            const float p = i / 120.0f;
            float v;
            switch (wave) {
            case 1: v = 1 - 4 * std::fabs(p - 0.5f); break;
            case 2: v = 2 * p - 1; break;
            case 3: v = p < 0.5f ? 1.0f : -1.0f; break;
            case 4: { static const float st[6] = {0.4f, -0.7f, 0.9f, -0.2f, 0.6f, -0.9f}; v = st[std::min((int)(p * 6), 5)]; break; }
            default: v = std::sin(6.2831853f * p); break;
            }
            const float x = g.x + 4 + (g.w - 8) * p, y = g.y + g.h / 2 - (g.h / 2 - 6) * v;
            if (i == 0) moveTo(x, y); else lineTo(x, y);
        }
        strokeColor(kSrcCol[kSrcLfo]);
        strokeWidth(1.6f);
        stroke();
        beginPath();  // where it is now
        rect(g.x + 2, g.y + g.h / 2 - (g.h / 2 - 6) * fLfo, g.w - 4, 1);
        fillColor(Color(70, 190, 255, 0.47f));
        fill();
        drawHandle(kSrcLfo);
    }

    void graphBox(const Rect& g)
    {
        beginPath();
        roundedRect(g.x, g.y, g.w, g.h, 3);
        fillColor(Color(12, 13, 14));
        fill();
        strokeColor(kEdge);
        strokeWidth(1);
        stroke();
    }

    // the thing you drag onto a knob to link it
    void drawHandle(int src)
    {
        const Rect& h = kHandle[src];
        const Color& c = kSrcCol[src];
        beginPath();
        roundedRect(h.x, h.y, h.w, h.h, 6);
        fillColor(Color(c.red, c.green, c.blue, fLinkSrc == src ? 0.35f : 0.15f));
        fill();
        strokeColor(c);
        strokeWidth(1.2f);
        stroke();
        // four-way arrows
        const float cx = h.x + h.w / 2, cy = h.y + 26;
        beginPath();
        moveTo(cx - 10, cy); lineTo(cx + 10, cy);
        moveTo(cx, cy - 10); lineTo(cx, cy + 10);
        for (int k = 0; k < 4; ++k) {
            const float dx = k == 0 ? 1.0f : k == 1 ? -1.0f : 0.0f, dy = k == 2 ? 1.0f : k == 3 ? -1.0f : 0.0f;
            const float ex = cx + dx * 10, ey = cy + dy * 10;
            moveTo(ex - dx * 4 + dy * 3, ey - dy * 4 + dx * 3);
            lineTo(ex, ey);
            lineTo(ex - dx * 4 - dy * 3, ey - dy * 4 - dx * 3);
        }
        strokeColor(c);
        strokeWidth(1.5f);
        stroke();
        fontSize(11);
        textAlign(ALIGN_CENTER | ALIGN_MIDDLE);
        fillColor(c);
        text(cx, h.y + 50, kSrcName[src], nullptr);
        fontSize(8);
        fillColor(kLabel);
        text(cx, h.y + 64, "drag onto a knob", nullptr);
    }

    void drawStrip()
    {
        beginPath();
        rect(0, kStripY, kW, kStripH);
        fillColor(Color(26, 27, 30));
        fill();
        fontSize(9.5f);
        textAlign(ALIGN_CENTER | ALIGN_MIDDLE);
        fillColor(kLabel);
        const float hx = (362 + kOctDown.x) / 2;
        if (fLinkSrc >= 0) {
            fillColor(kSrcCol[fLinkSrc]);
            text(hx, kStripY + 45, "drop it on a knob to link it", nullptr);
        } else {
            text(hx, kStripY + 30, "drag ENV or LFO onto a knob to link it", nullptr);
            text(hx, kStripY + 46, "drag a link's tag up/down to set how much, right-click it to unlink", nullptr);
            text(hx, kStripY + 62, "right-click a knob for its default", nullptr);
        }
        // keyboard octave
        for (const Rect* r : {&kOctDown, &kOctUp}) {
            beginPath();
            roundedRect(r->x, r->y, r->w, r->h, 4);
            fillColor(kPanel);
            fill();
            strokeColor(kEdge);
            stroke();
            fontSize(14);
            fillColor(kAccent);
            text(r->x + r->w / 2, r->y + r->h / 2, r == &kOctDown ? "<" : ">", nullptr);
        }
        char buf[24];
        std::snprintf(buf, sizeof(buf), "C%d-C%d", fKbLow / 12 - 1, fKbLow / 12 + 3);
        fontSize(11);
        fillColor(kAccent);
        text((kOctDown.x + kOctDown.w + kOctUp.x) / 2, kOctDown.y + kOctDown.h / 2, buf, nullptr);
        fontSize(9);
        fillColor(kLabel);
        text((kOctDown.x + kOctDown.w + kOctUp.x) / 2, kStripY + 15, "KEYS", nullptr);
    }

    void drawTubeLabels()
    {
        static const char* names[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
        char buf[64];
        fontSize(12);
        textAlign(ALIGN_LEFT | ALIGN_TOP);
        if (fNote >= 0) {
            const float hz = 440.0f * std::exp2((fPitch - 69) / 12);
            std::snprintf(buf, sizeof(buf), "%s%d  %.1f Hz", names[fNote % 12], fNote / 12 - 1, hz);
            fillColor(Color(kAccent.red, kAccent.green, kAccent.blue, 0.85f));
            text(14, 12, buf, nullptr);
            std::snprintf(buf, sizeof(buf), "%d %s", fVoices, fVoices == 1 ? "VOICE" : "VOICES");
            textAlign(ALIGN_RIGHT | ALIGN_TOP);
            fillColor(kLabel);
            text(kTube - 14, 12, buf, nullptr);
        } else {
            textAlign(ALIGN_CENTER | ALIGN_MIDDLE);
            fillColor(Color(kAccent.red, kAccent.green, kAccent.blue, 0.5f));
            text(kTube / 2, kTube / 2 + 40, "PLAY A NOTE", nullptr);
        }
        textAlign(ALIGN_CENTER | ALIGN_BOTTOM);
        fontSize(9);
        fillColor(kOff);
        text(kTube / 2, kTube - 8, "SCOPE SYNTH", nullptr);
    }

    // ---- knobs, switches, selectors
    void valueText(int p, char* out, size_t n) const
    {
        const float v = fValues[p];
        switch (p) {
        case kAShape: case kBShape: std::snprintf(out, n, "%.2f", v + 1); break;
        case kAOct: case kBOct: case kASemi: case kBSemi: std::snprintf(out, n, "%+d", (int)std::lround(v)); break;
        case kAFine: case kBFine: std::snprintf(out, n, "%+d ct", (int)std::lround(v)); break;
        case kARot: case kBRot: std::snprintf(out, n, "%d\xC2\xB0", (int)std::lround(v)); break;
        case kAttack: case kDecay: case kRelease: case kGlide:
            if (v < 1) std::snprintf(out, n, "%d ms", (int)std::lround(v * 1000));
            else std::snprintf(out, n, "%.2f s", v);
            break;
        case kLfoRate: std::snprintf(out, n, "%.2f Hz", v); break;
        case kLfoWave: std::snprintf(out, n, "%s", kLfoWaves[std::clamp((int)std::lround(v), 0, 4)]); break;
        case kPoly: std::snprintf(out, n, "%s", v > 0.5f ? "POLY" : "MONO"); break;
        case kLegato: std::snprintf(out, n, "%s", v > 0.5f ? "ON" : "OFF"); break;
        default: std::snprintf(out, n, "%d%%", (int)std::lround(v * 100)); break;
        }
    }

    // where a link's tag sits under its knob (false: not linked)
    bool tagRect(const Ctl& c, int src, Rect& r) const
    {
        const int d = destIndex(c.param);
        if (d < 0 || fValues[modParam(src, d)] == 0) return false;
        const bool both = fValues[modParam(0, d)] != 0 && fValues[modParam(1, d)] != 0;
        const float w = 28, cx = c.x + c.w / 2;
        const float x = both ? (src == 0 ? cx - w - 1 : cx + 1) : cx - w / 2;
        r = {x, c.y + c.h - 17, w, 13};
        return true;
    }

    void drawCtl(const Ctl& c)
    {
        const int p = c.param;
        const Kind kind = kInfo[p].kind;
        const float cx = c.x + c.w / 2;
        char buf[48];
        fontSize(9);
        textAlign(ALIGN_CENTER | ALIGN_MIDDLE);
        fillColor(kLabel);
        text(cx, c.y + 8, kInfo[p].caption, nullptr);
        valueText(p, buf, sizeof(buf));
        const int d = destIndex(p);
        if (fLinkSrc >= 0 && d >= 0) {  // dragging a link: show where it can go
            beginPath();
            roundedRect(c.x + 3, c.y, c.w - 6, c.h - 3, 4);
            strokeColor(Color(kSrcCol[fLinkSrc].red, kSrcCol[fLinkSrc].green, kSrcCol[fLinkSrc].blue, 0.6f));
            strokeWidth(1);
            stroke();
        }
        if (kind == kSwitch) {
            drawSwitch(cx, c.y + 34, fValues[p] > 0.5f);
            fontSize(9);
            fillColor(kAccent);
            text(cx, c.y + 62, buf, nullptr);
            return;
        }
        if (kind == kChoice) {
            const float dw = c.w - 12, dh = 22, cy = c.y + 38;
            beginPath();
            roundedRect(cx - dw / 2, cy - dh / 2, dw, dh, 3);
            fillColor(Color(10, 12, 11));
            fill();
            strokeColor(Color(60, 62, 68));
            stroke();
            fontSize(10);
            fillColor(kAccent);
            text(cx, cy, buf, nullptr);
            fontSize(8);
            fillColor(kOff);
            text(cx, c.y + 62, "< click >", nullptr);
            return;
        }
        const float r = 13, cy = c.y + 36;
        const float n = toNorm(p, fValues[p]);
        drawKnob(cx, cy, r, n);
        if (d >= 0) {
            bool linked = false;
            for (int src = 0; src < kNumSources; ++src) {
                const float a = fValues[modParam(src, d)];
                if (a == 0) continue;
                linked = true;
                // the ring: how far this link can move the knob (the LFO swings both ways)
                const float lo = std::clamp(src == kSrcLfo ? n - std::fabs(a) : std::min(n, n + a), 0.0f, 1.0f);
                const float hi = std::clamp(src == kSrcLfo ? n + std::fabs(a) : std::max(n, n + a), 0.0f, 1.0f);
                arcRing(cx, cy, r + 7 + 3 * src, lo, hi, kSrcCol[src]);
                Rect t{};
                tagRect(c, src, t);
                beginPath();
                roundedRect(t.x, t.y, t.w, t.h, 3);
                fillColor(Color(kSrcCol[src].red, kSrcCol[src].green, kSrcCol[src].blue, 0.25f));
                fill();
                strokeColor(kSrcCol[src]);
                strokeWidth(1);
                stroke();
                char tb[16];
                std::snprintf(tb, sizeof(tb), "%c%+d", kSrcName[src][0], (int)std::lround(a * 100));
                fontSize(8);
                fillColor(kSrcCol[src]);
                text(t.x + t.w / 2, t.y + t.h / 2, tb, nullptr);
            }
            if (linked) {  // where the links have it right now
                const float live = toNorm(p, liveValue(p));
                const float ang = 0.75f * 3.14159265f + 1.5f * 3.14159265f * live;
                beginPath();
                circle(cx + std::cos(ang) * (r + 3), cy + std::sin(ang) * (r + 3), 2.2f);
                fillColor(Color(255, 255, 255));
                fill();
            }
        }
        fontSize(8.5f);
        fillColor(kLabel);
        text(cx, c.y + 62, buf, nullptr);
    }

    void arcRing(float cx, float cy, float r, float lo, float hi, const Color& col)
    {
        if (hi - lo < 0.002f) return;
        const float a0 = 0.75f * 3.14159265f, span = 1.5f * 3.14159265f;
        beginPath();
        arc(cx, cy, r, a0 + span * lo, a0 + span * hi, CW);
        strokeColor(col);
        strokeWidth(2);
        lineCap(ROUND);
        stroke();
        lineCap(BUTT);
    }

    // a dial: dark knob, a lit arc showing the setting, a pointer
    void drawKnob(float cx, float cy, float r, float n)
    {
        const float a0 = 0.75f * 3.14159265f, a1 = 2.25f * 3.14159265f;
        const float a = a0 + (a1 - a0) * n;
        lineCap(ROUND);
        beginPath();
        arc(cx, cy, r + 3.5f, a0, a1, CW);
        strokeColor(Color(50, 53, 58));
        strokeWidth(2.5f);
        stroke();
        if (n > 0.002f) {
            beginPath();
            arc(cx, cy, r + 3.5f, a0, a, CW);
            strokeColor(kAccent);
            strokeWidth(2.5f);
            stroke();
        }
        beginPath();
        circle(cx, cy, r);
        fillPaint(radialGradient(cx - r * 0.3f, cy - r * 0.4f, r * 0.1f, r * 1.2f, Color(78, 80, 86), Color(26, 27, 30)));
        fill();
        strokeColor(Color(12, 12, 14));
        strokeWidth(1);
        stroke();
        beginPath();
        moveTo(cx + std::cos(a) * r * 0.25f, cy + std::sin(a) * r * 0.25f);
        lineTo(cx + std::cos(a) * r * 0.85f, cy + std::sin(a) * r * 0.85f);
        strokeColor(Color(225, 228, 222));
        strokeWidth(2);
        stroke();
        lineCap(BUTT);
    }

    // a slide switch: the lever sits right and lights up when on
    void drawSwitch(float cx, float cy, bool on)
    {
        const float w = 34, h = 16;
        beginPath();
        roundedRect(cx - w / 2, cy - h / 2, w, h, h / 2);
        fillColor(on ? Color(kAccent.red, kAccent.green, kAccent.blue, 0.35f) : Color(12, 13, 15));
        fill();
        strokeColor(on ? kAccent : Color(60, 62, 68));
        strokeWidth(1);
        stroke();
        const float kx = on ? cx + w / 2 - h / 2 : cx - w / 2 + h / 2;
        beginPath();
        circle(kx, cy, h / 2 - 2);
        fillPaint(radialGradient(kx - 2, cy - 2, 1, h / 2, Color(235, 236, 232), Color(150, 152, 150)));
        fill();
    }

    // testing hook: the frame just drawn
    void dumpFrontBuffer(const std::string& path)
    {
        const int w = (int)getWidth(), h = (int)getHeight();
        std::vector<uint8_t> px((size_t)w * h * 4);
        glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, px.data());
        if (FILE* f = std::fopen(path.c_str(), "wb")) {
            std::fprintf(f, "P6 %d %d 255\n", w, h);
            for (int y = h - 1; y >= 0; --y)  // OpenGL rows run bottom-up
                for (int x = 0; x < w; ++x)
                    std::fwrite(&px[((size_t)y * w + x) * 4], 1, 3, f);
            std::fclose(f);
        }
    }

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ScopeSynthUI)
};

UI* createUI()
{
    return new ScopeSynthUI();
}

END_NAMESPACE_DISTRHO
