// The window, styled like a terminal: the CRT scope on the left showing what
// you hear, then the two oscillators, the ENV and LFO, voice settings, and a
// keyboard along the bottom.
// Controls are knobs, or faders for the envelope and volume: drag up/down to
// change, scroll to nudge,
// right-click for the default.
// Linking like Serum: drag the ENV or LFO handle onto a control. It gets a
// coloured range line and a tag; drag the tag up/down to set how much,
// right-click the tag to unlink.
// Shapes: each oscillator has a playlist (starts with one shape). < > pick
// the selected slot's shape, + adds a slot, MORPH blends through the slots in
// order. NEW f(t) types a shape as x(t), y(t); it joins the shape list.
#include "DistrhoUI.hpp"
#include "ScopeSynthShared.hpp"
#include "CrtTube.hpp"
#include "MonoFont.hpp"
#include "OpenGL.hpp"

#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

START_NAMESPACE_DISTRHO

using namespace ss;

// layout, in window units (multiplied by the host's scale factor)
static constexpr float kW = 1020, kH = 700;
static constexpr float kTube = 500;          // the scope, top-left, square
static constexpr float kR = kTube;           // the column of panels right of it starts here
static constexpr float kOscY[2] = {0, 140}, kOscH = 140;
static constexpr float kEnvY = 280, kLfoY = 390, kRowH = 110;
static constexpr float kStripY = 500, kStripH = 90;
static constexpr float kKeysY = 590, kKeysH = 110;
static constexpr int kWhiteKeys = 50;        // seven octaves and a top C

// terminal colours: green phosphor on black
static const Color kBg(5, 8, 6), kFg(90, 255, 120), kBright(200, 255, 210), kDim(45, 130, 65), kFaint(22, 52, 30);
static const Color kErr(255, 95, 80);
static const Color kSrcCol[2] = {Color(255, 180, 40), Color(80, 220, 255)};  // ENV amber, LFO cyan
static const char* const kSrcName[2] = {"ENV", "LFO"};

struct Ctl {
    int param;
    float x, y, w, h;
};

struct Box {
    float x, y, w, h;
    bool has(float px, float py) const { return px >= x && px < x + w && py >= y && py < y + h; }
};

static const Box kHandle[2] = {{kR + 424, kEnvY + 22, 86, 78}, {kR + 424, kLfoY + 22, 86, 78}};
static const Box kOctDown = {kW - 150, kStripY + 34, 34, 28}, kOctUp = {kW - 46, kStripY + 34, 34, 28};

// an oscillator's playlist bar, under its preview and controls
static constexpr float kBarDY = 112, kBarH = 20, kChip = 22;
static Box barPrev(int o) { return {kR + 12, kOscY[o] + kBarDY, 18, kBarH}; }
static Box barName(int o) { return {kR + 32, kOscY[o] + kBarDY, 104, kBarH}; }
static Box barNext(int o) { return {kR + 138, kOscY[o] + kBarDY, 18, kBarH}; }
static Box barChip(int o, int i) { return {kR + 164 + i * kChip, kOscY[o] + kBarDY, kChip - 2, kBarH}; }
static Box barNew(int o) { return {kR + 362, kOscY[o] + kBarDY, 70, kBarH}; }
static Box barEdit(int o) { return {kR + 436, kOscY[o] + kBarDY, 72, kBarH}; }

// the equation editor, over the scope
static const Box kEdPreview = {50, 48, 400, 222}, kEdField[2] = {{86, 288, 380, 24}, {86, 320, 380, 24}};
static const Box kEdSave = {50, 452, 120, 28}, kEdCancel = {190, 452, 120, 28}, kEdDelete = {330, 452, 120, 28};

class ScopeSynthUI : public UI {
public:
    ScopeSynthUI()
        : UI((uint)kW, (uint)kH)
    {
        createFontFromMemory("mono", kMonoFont, kMonoFontSize, false);
        for (int p = 0; p < kParamCount; ++p)
            fValues[p] = paramDefault(p);
        for (int o = 0; o < 2; ++o) {  // oscillators: seven controls each, right of the preview
            const int b = o * (kBShape - kAShape);
            for (int i = 0; i < 7; ++i)
                fCtls.push_back({kAShape + b + i, kR + 116 + i * 56.0f, kOscY[o] + 16, 56, 94});
        }
        for (int i = 0; i < 4; ++i)
            fCtls.push_back({kAttack + i, kR + 12 + i * 60.0f, kEnvY + 16, 60, 92});
        fCtls.push_back({kLfoRate, kR + 12, kLfoY + 16, 60, 92});
        fCtls.push_back({kLfoWave, kR + 74, kLfoY + 16, 84, 92});
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

    // a project opened (or the plugin's own saved shapes came back)
    void stateChanged(const char* key, const char* value) override
    {
        if (std::strcmp(key, "shapes") != 0 || !value) return;
        fSet.load(value);
        repaint();
    }

    // ---- typing in the equation editor
    bool onKeyboard(const KeyboardEvent& ev) override
    {
        if (!fEdit) return false;
        if (!ev.press) return true;
        std::string& f = fField[fFocus];
        switch (ev.key) {
        case kKeyBackspace: if (!f.empty()) f.pop_back(); editChanged(); break;
        case kKeyTab: fFocus ^= 1; break;
        case kKeyEnter: editSave(); break;
        case kKeyEscape: fEdit = false; break;
        case kKeyDelete: f.clear(); editChanged(); break;
        default: break;
        }
        repaint();
        return true;
    }

    bool onCharacterInput(const CharacterInputEvent& ev) override
    {
        if (!fEdit) return false;
        const uint c = ev.character;
        if (c >= 32 && c < 127 && c != '|' && fField[fFocus].size() < 120) {  // | separates x and y when saved
            fField[fFocus] += (char)c;
            editChanged();
            repaint();
        }
        return true;
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
        fBlink += std::clamp(dt, 0.0f, 0.25f);
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
        fontFace("mono");
        drawScopeLabels();
        if (fEdit) drawEditor();
        for (int o = 0; o < 2; ++o)
            drawOscPanel(o);
        drawEnvPanel();
        drawLfoPanel();
        drawStrip();
        drawKeyboard();
        for (const Ctl& c : fCtls)
            drawCtl(c);
        if (fLinkSrc >= 0) {  // dragging a link: a line from the handle to the pointer
            const Box& h = kHandle[fLinkSrc];
            beginPath();
            moveTo(h.x + h.w / 2, h.y + h.h / 2);
            lineTo(fMouseX, fMouseY);
            strokeColor(kSrcCol[fLinkSrc]);
            strokeWidth(1.5f);
            stroke();
            beginPath();
            rect(fMouseX - 3, fMouseY - 3, 6, 6);
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
            if (fLinkSrc >= 0) {  // dropped a link on a control
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
        if (fEdit && x < kTube && y < kTube) {
            if (ev.button != 1) return true;
            if (kEdSave.has(x, y)) editSave();
            else if (kEdCancel.has(x, y)) fEdit = false;
            else if (fEditId >= 0 && kEdDelete.has(x, y)) editDelete();
            else if (kEdField[0].has(x, y)) fFocus = 0;
            else if (kEdField[1].has(x, y)) fFocus = 1;
            repaint();
            return true;
        }
        if (playlistClick(ev.button, x, y)) return true;
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
            fKbLow = std::clamp(fKbLow + (kOctUp.has(x, y) ? 12 : -12), 0, 36);
            repaint();
            return true;
        }
        // a link's tag: drag to set how much, right-click to unlink
        for (const Ctl& c : fCtls)
            for (int src = 0; src < kNumSources; ++src) {
                Box t;
                if (!tagBox(c, src, t) || !t.has(x, y)) continue;
                const int mp = modParam(src, destIndex(c.param));
                if (ev.button == 3) setValue(mp, 0, true);
                else if (ev.button == 1) startDrag(mp, y);
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
        } else if (ev.button == 3) {  // right-click: back to the default
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
    float fBlink = 0;
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
    int fKbNote = -1, fKbLow = 24;  // keyboard starts at C1
    // shapes: the same set the plugin plays, kept in step through the "shapes" state
    ShapeSet fSet;
    int fSel[2] = {0, 0};  // the slot the arrows change
    // equation editor
    bool fEdit = false;
    int fEditOsc = 0, fEditId = -1, fFocus = 0;  // fEditId: the typed shape being edited, -1 = a new one
    std::string fField[2], fEditErr;
    CustomShape fPreview;

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
            if (Box{c.x, c.y, c.w, c.h}.has(x, y)) return &c;
        return nullptr;
    }

    // ---- shapes and playlists
    void sendShapes()
    {
        setState("shapes", fSet.save().c_str());
        repaint();
    }

    // an oscillator's shape list (from its parameters)
    int slots(int o) const { return slotCount(fValues, o); }
    int slotId(int o, int i) const { return slotShape(fValues, o, i, fSet.count()); }

    bool playlistClick(int button, float x, float y)
    {
        for (int o = 0; o < 2; ++o) {
            const int n = slots(o), total = fSet.count();
            int& sel = fSel[o];
            sel = std::min(sel, n - 1);
            if (barPrev(o).has(x, y) || barNext(o).has(x, y) || barName(o).has(x, y)) {
                // < back, > next; on the name: click next, right-click back
                const bool back = barPrev(o).has(x, y) || (barName(o).has(x, y) && button == 3);
                setValue(slotParam(o, sel), (float)((slotId(o, sel) + (back ? -1 : 1) + total) % total), true);
                return true;
            }
            for (int i = 0; i < n; ++i)
                if (barChip(o, i).has(x, y)) {
                    if (button == 3 && n > 1) {  // right-click: take the slot out, the ones after move down
                        for (int j = i; j < n - 1; ++j)
                            setValue(slotParam(o, j), (float)slotId(o, j + 1), true);
                        setValue(slotsParam(o), (float)(n - 1), true);
                        sel = std::min(sel, n - 2);
                    } else if (button == 1) {
                        sel = i;
                        repaint();
                    }
                    return true;
                }
            if (n < kMaxSlots && barChip(o, n).has(x, y)) {  // +: a new slot, the next shape along
                if (button == 1) {
                    setValue(slotParam(o, n), (float)((slotId(o, n - 1) + 1) % total), true);
                    setValue(slotsParam(o), (float)(n + 1), true);
                    sel = n;
                }
                return true;
            }
            if (barNew(o).has(x, y)) {
                if (button == 1) openEditor(o, -1);
                return true;
            }
            if (fSet.isCustom(slotId(o, sel)) && barEdit(o).has(x, y)) {
                if (button == 1) openEditor(o, slotId(o, sel));
                return true;
            }
        }
        return false;
    }

    void openEditor(int o, int id)
    {
        if (id < 0 && (int)fSet.customs.size() >= kMaxCustom)
            id = fSet.count() - 1;  // full: edit the last one instead
        fEdit = true;
        fEditOsc = o;
        fEditId = id;
        fFocus = 0;
        if (id >= 0) {
            fField[0] = fSet.custom(id).xs;
            fField[1] = fSet.custom(id).ys;
        } else {
            fField[0].clear();
            fField[1].clear();
        }
        editChanged();
        repaint();
    }

    void editChanged()
    {
        fPreview.xs = fField[0];
        fPreview.ys = fField[1];
        fEditErr = fPreview.compile();
        if (fEditErr.empty()) fPreview.build();
    }

    void editSave()
    {
        if (!fEditErr.empty()) return;
        CustomShape c;
        c.xs = fField[0];
        c.ys = fField[1];
        c.compile();
        c.build();
        int id = fEditId;
        if (id >= 0) {
            fSet.customs[id - crt::kNumEquations] = c;
        } else {
            fSet.customs.push_back(c);
            id = fSet.count() - 1;
        }
        fEdit = false;
        sendShapes();
        setValue(slotParam(fEditOsc, std::min(fSel[fEditOsc], slots(fEditOsc) - 1)), (float)id, true);  // and play it in the selected slot
    }

    // slots playing a deleted shape go back to ROSE; the typed shapes after it move down one
    void editDelete()
    {
        for (int o = 0; o < 2; ++o)
            for (int i = 0; i < kMaxSlots; ++i) {
                const int id = (int)std::lround(fValues[slotParam(o, i)]);
                if (id >= fEditId) setValue(slotParam(o, i), id == fEditId ? 0.0f : (float)(id - 1), true);
            }
        fSet.removeCustom(fEditId);
        fEdit = false;
        sendShapes();
    }

    // a value as the ENV and LFO are moving it right now (newest note)
    float liveValue(int p) const
    {
        const int d = destIndex(p);
        if (d < 0) return fValues[p];
        const float env = fNote >= 0 ? fEnv : 0.0f;
        return fromNorm(p, toNorm(p, fValues[p]) + fValues[modParam(kSrcEnv, d)] * env + fValues[modParam(kSrcLfo, d)] * fLfo);
    }

    // ---- drawing helpers
    void label(float x, float y, const char* s, const Color& c, float size = 10, int align = ALIGN_LEFT | ALIGN_MIDDLE)
    {
        fontSize(size);
        textAlign(align);
        fillColor(c);
        text(x, y, s, nullptr);
    }

    void outline(const Box& b, const Color& c, float width = 1)
    {
        beginPath();
        rect(b.x + 0.5f, b.y + 0.5f, b.w - 1, b.h - 1);
        strokeColor(c);
        strokeWidth(width);
        stroke();
    }

    void fillBox(const Box& b, const Color& c)
    {
        beginPath();
        rect(b.x, b.y, b.w, b.h);
        fillColor(c);
        fill();
    }

    // a box with its title set into the top line: --[ OSC A ] ROSE------
    void panel(const Box& b, const char* title, const char* sub = nullptr)
    {
        outline({b.x + 4, b.y + 6, b.w - 8, b.h - 10}, kDim);
        char buf[128];
        std::snprintf(buf, sizeof(buf), "[ %s ]", title);
        fontSize(11);
        textAlign(ALIGN_LEFT | ALIGN_MIDDLE);
        Rectangle<float> r;
        textBounds(0, 0, buf, nullptr, r);
        const float tw = r.getWidth();
        float sw = 0;
        if (sub) {
            textBounds(0, 0, sub, nullptr, r);
            sw = r.getWidth();
        }
        fillBox({b.x + 12, b.y, tw + (sub ? sw + 10 : 0) + 8, 12}, kBg);
        label(b.x + 16, b.y + 6.5f, buf, kFg, 11);
        if (sub) label(b.x + 16 + tw + 8, b.y + 6.5f, sub, kBright, 11);
    }

    // a text button
    void button(const Box& b, const char* s, const Color& c, bool lit = false)
    {
        if (lit) fillBox(b, Color(c.red, c.green, c.blue, 0.2f));
        outline(b, lit ? c : Color(c.red, c.green, c.blue, 0.55f));
        label(b.x + b.w / 2, b.y + b.h / 2 + 0.5f, s, c, 10, ALIGN_CENTER | ALIGN_MIDDLE);
    }

    // ---- keyboard
    static float whiteW() { return kW / kWhiteKeys; }
    static bool hasBlackAfter(int white) { const int i = white % 7; return i != 2 && i != 6; }  // none after E and B
    int whiteNote(int white) const
    {
        static const int st[7] = {0, 2, 4, 5, 7, 9, 11};
        return fKbLow + 12 * (white / 7) + st[white % 7];
    }
    Box blackBox(int white) const
    {
        const float w = whiteW() * 0.6f;
        return {(white + 1) * whiteW() - w / 2, kKeysY, w, kKeysH * 0.6f};
    }
    int keyAt(float x, float y) const
    {
        for (int i = 0; i < kWhiteKeys - 1; ++i)
            if (hasBlackAfter(i) && blackBox(i).has(x, y)) return whiteNote(i) + 1;
        return whiteNote(std::clamp((int)(x / whiteW()), 0, kWhiteKeys - 1));
    }
    static int velocityAt(float y) { return std::clamp((int)(40 + 87 * (y - kKeysY) / kKeysH), 1, 127); }  // lower = harder
    bool lit(int note) const { return note == fKbNote || ((fKeys[(note & 127) >> 6] >> (note & 63)) & 1); }

    void drawKeyboard()
    {
        const float ww = whiteW();
        for (int i = 0; i < kWhiteKeys; ++i) {
            const int n = whiteNote(i);
            const Box k = {i * ww + 0.5f, kKeysY + 2, ww - 1, kKeysH - 4};
            fillBox(k, lit(n) ? kFg : Color(232, 234, 228));
            if (n % 12 == 0) {
                char buf[8];
                std::snprintf(buf, sizeof(buf), "C%d", n / 12 - 1);
                label(k.x + k.w / 2, k.y + k.h - 8, buf, Color(90, 96, 92), 8, ALIGN_CENTER | ALIGN_MIDDLE);
            }
        }
        for (int i = 0; i < kWhiteKeys - 1; ++i) {
            if (!hasBlackAfter(i)) continue;
            const Box k = blackBox(i);
            fillBox({k.x, k.y + 2, k.w, k.h}, lit(whiteNote(i) + 1) ? kFg : Color(12, 12, 12));
        }
    }

    // ---- panels
    // the shape an oscillator is playing, as its controls (and links) set it right now
    void oscShape(int o, std::vector<float>& xs, std::vector<float>& ys, float& level) const
    {
        const int b = o * (kBShape - kAShape);
        const int n = slots(o);
        const float pos = std::clamp(liveValue(kAShape + b), 0.0f, 1.0f) * (n - 1);
        const int s0 = std::min((int)pos, n - 1), s1 = std::min(s0 + 1, n - 1);
        const float m = pos - s0, rot = liveValue(kARot + b) * 0.017453293f;
        const float c = std::cos(rot), s = std::sin(rot);
        level = liveValue(kALevel + b);
        const int pts = 600;
        xs.resize(pts);
        ys.resize(pts);
        for (int i = 0; i < pts; ++i) {
            float x0, y0, x1, y1;
            fSet.point(slotId(o, s0), i / (double)(pts - 1), fAnim, x0, y0);
            fSet.point(slotId(o, s1), i / (double)(pts - 1), fAnim, x1, y1);
            const float x = x0 + (x1 - x0) * m, y = y0 + (y1 - y0) * m;
            xs[i] = x * c - y * s;
            ys[i] = x * s + y * c;
        }
    }

    void drawOscPanel(int o)
    {
        const float y0 = kOscY[o];
        const int b = o * (kBShape - kAShape);
        const int n = slots(o), sel = std::min(fSel[o], n - 1);
        // the title says what's playing: one shape, or the two MORPH is between
        char sub[96];
        const float pos = std::clamp(fValues[kAShape + b], 0.0f, 1.0f) * (n - 1);
        const int s0 = std::min((int)pos, n - 1), s1 = std::min(s0 + 1, n - 1);
        const float frac = pos - s0;
        if (frac > 0.04f && frac < 0.96f)
            std::snprintf(sub, sizeof(sub), "%s > %s", fSet.name(slotId(o, s0)).c_str(), fSet.name(slotId(o, s1)).c_str());
        else
            std::snprintf(sub, sizeof(sub), "%s", fSet.name(slotId(o, frac >= 0.96f ? s1 : s0)).c_str());
        panel({kR, y0, kW - kR, kOscH}, o == 0 ? "OSC A" : "OSC B", sub);

        // what it looks like, as a plain line drawing
        const Box g = {kR + 12, y0 + 18, 100, 88};
        outline(g, kFaint);
        std::vector<float> xs, ys;
        float level;
        oscShape(o, xs, ys, level);
        if (level > 0.001f) {
            const float cx = g.x + g.w / 2, cy = g.y + g.h / 2, sc = std::min(g.w, g.h) / 2 / 0.9f;
            beginPath();
            for (size_t i = 0; i < xs.size(); ++i)
                if (i == 0) moveTo(cx + xs[i] * sc, cy - ys[i] * sc); else lineTo(cx + xs[i] * sc, cy - ys[i] * sc);
            lineJoin(ROUND);
            strokeColor(Color(kFg.red, kFg.green, kFg.blue, 0.4f + 0.6f * std::min(level / 0.8f, 1.0f)));
            strokeWidth(1.1f);
            stroke();
        } else {
            label(g.x + g.w / 2, g.y + g.h / 2, "-- OFF --", kDim, 10, ALIGN_CENTER | ALIGN_MIDDLE);
        }

        // playlist bar: < NAME > for the selected slot, the slots, +, NEW f(t), EDIT f(t)
        button(barPrev(o), "<", kFg);
        button(barNext(o), ">", kFg);
        const Box nm = barName(o);
        outline(nm, kDim);
        save();
        intersectScissor(nm.x + 2, nm.y, nm.w - 4, nm.h);
        label(nm.x + nm.w / 2, nm.y + nm.h / 2 + 0.5f, fSet.name(slotId(o, sel)).c_str(), kBright, 10, ALIGN_CENTER | ALIGN_MIDDLE);
        restore();
        // the slots: the selected one outlined bright, the ones MORPH is playing lit by how much
        const float live = std::clamp(liveValue(kAShape + b), 0.0f, 1.0f) * (n - 1);
        for (int i = 0; i < n; ++i) {
            const Box r = barChip(o, i);
            const float w = n > 1 ? std::max(0.0f, 1 - std::fabs(live - i)) : 0.0f;
            fillBox(r, Color(kFg.red, kFg.green, kFg.blue, 0.6f * w));
            outline(r, i == sel ? kBright : kDim, i == sel ? 1.5f : 1);
            char num[4];
            std::snprintf(num, sizeof(num), "%d", i + 1);
            label(r.x + r.w / 2, r.y + r.h / 2 + 0.5f, num, w > 0.5f ? kBg : i == sel ? kBright : kFg, 10, ALIGN_CENTER | ALIGN_MIDDLE);
        }
        if (n < kMaxSlots) button(barChip(o, n), "+", kFg);
        button(barNew(o), "NEW f(t)", kSrcCol[kSrcEnv]);
        if (fSet.isCustom(slotId(o, sel))) button(barEdit(o), "EDIT f(t)", kSrcCol[kSrcEnv]);
    }

    // $ new shape: x(t) and y(t), a live preview above
    void drawEditor()
    {
        fillBox({0, 0, kTube, kTube}, Color(kBg.red, kBg.green, kBg.blue, 0.97f));
        outline({6, 6, kTube - 12, kTube - 12}, kDim);
        char title[96];
        std::snprintf(title, sizeof(title), "$ %s  # osc %c, slot %d", fEditId >= 0 ? "edit shape" : "new shape",
                      'a' + fEditOsc, fSel[fEditOsc] + 1);
        label(20, 26, title, kFg, 12);
        const Box& g = kEdPreview;
        outline(g, kFaint);
        if (fEditErr.empty()) {
            const float sc = std::min(g.w, g.h) / 2 / 0.9f, cx = g.x + g.w / 2, cy = g.y + g.h / 2;
            beginPath();
            for (int i = 0; i <= 800; ++i) {
                float px, py;
                fPreview.point(i / 800.0, fBlink, px, py);
                if (i == 0) moveTo(cx + px * sc, cy - py * sc); else lineTo(cx + px * sc, cy - py * sc);
            }
            lineJoin(ROUND);
            strokeColor(kFg);
            strokeWidth(1.3f);
            stroke();
        }
        static const char* names[2] = {"x =", "y ="};
        const bool blink = std::fmod(fBlink, 1.0f) < 0.6f;
        for (int f = 0; f < 2; ++f) {
            const Box& r = kEdField[f];
            label(r.x - 10, r.y + r.h / 2, names[f], f == fFocus ? kFg : kDim, 12, ALIGN_RIGHT | ALIGN_MIDDLE);
            outline(r, f == fFocus ? kFg : kDim);
            save();
            intersectScissor(r.x + 2, r.y, r.w - 4, r.h);
            fontSize(12);
            textAlign(ALIGN_LEFT | ALIGN_MIDDLE);
            Rectangle<float> tb;
            if (!fField[f].empty()) textBounds(0, 0, fField[f].c_str(), nullptr, tb);
            const float tw = fField[f].empty() ? 0.0f : tb.getWidth();
            const float tx = std::min(r.x + 8, r.x + r.w - 16 - tw);  // keep the end in view
            if (fField[f].empty())  // a faint example until you type
                label(r.x + 16, r.y + r.h / 2 + 0.5f, f == 0 ? "sin(3t)" : "cos(2t)", kFaint, 12);
            else
                label(tx, r.y + r.h / 2 + 0.5f, fField[f].c_str(), kBright, 12);
            if (f == fFocus && blink) fillBox({tx + tw + 1, r.y + 5, 7, r.h - 10}, kFg);  // block cursor
            restore();
        }
        if (!fField[0].empty() || !fField[1].empty()) {
            if (!fEditErr.empty())
                label(kTube / 2, 364, ("error: " + fEditErr).c_str(), kErr, 11, ALIGN_CENTER | ALIGN_MIDDLE);
            else
                label(kTube / 2, 364, "ok", kFg, 11, ALIGN_CENTER | ALIGN_MIDDLE);
        }
        button(kEdSave, "[ SAVE ]", fEditErr.empty() ? kFg : kDim);
        button(kEdCancel, "[ CANCEL ]", kDim);
        if (fEditId >= 0) button(kEdDelete, "[ DELETE ]", kErr);
    }

    void drawEnvPanel()
    {
        const float y0 = kEnvY;
        panel({kR, y0, kW - kR, kRowH}, "ENV");
        // its shape (times on a square-root scale so short and long both show)
        const Box g = {kR + 256, y0 + 22, 160, 78};
        outline(g, kFaint);
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
        strokeWidth(1.3f);
        stroke();
        if (fNote >= 0)  // where the newest note's envelope is now
            fillBox({g.x + 2, Y(fEnv), g.w - 4, 1}, Color(255, 180, 40, 0.45f));
        drawHandle(kSrcEnv);
    }

    void drawLfoPanel()
    {
        const float y0 = kLfoY;
        panel({kR, y0, kW - kR, kRowH}, "LFO");
        const Box g = {kR + 162, y0 + 22, 254, 78};
        outline(g, kFaint);
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
        strokeWidth(1.3f);
        stroke();
        fillBox({g.x + 2, g.y + g.h / 2 - (g.h / 2 - 6) * fLfo, g.w - 4, 1}, Color(80, 220, 255, 0.45f));
        drawHandle(kSrcLfo);
    }

    // the thing you drag onto a control to link it
    void drawHandle(int src)
    {
        const Box& h = kHandle[src];
        const Color& c = kSrcCol[src];
        if (fLinkSrc == src) fillBox(h, Color(c.red, c.green, c.blue, 0.2f));
        outline(h, c);
        label(h.x + h.w / 2, h.y + 28, "<-+->", c, 13, ALIGN_CENTER | ALIGN_MIDDLE);
        label(h.x + h.w / 2, h.y + 52, kSrcName[src], c, 13, ALIGN_CENTER | ALIGN_MIDDLE);
    }

    void drawStrip()
    {
        outline({4, kStripY + 2, kW - 8, kStripH - 6}, kFaint);
        // keyboard octave
        button(kOctDown, "<", kFg);
        button(kOctUp, ">", kFg);
        char buf[24];
        std::snprintf(buf, sizeof(buf), "C%d-C%d", fKbLow / 12 - 1, fKbLow / 12 + 6);
        const float mid = (kOctDown.x + kOctDown.w + kOctUp.x) / 2;
        label(mid, kOctDown.y + kOctDown.h / 2, buf, kBright, 11, ALIGN_CENTER | ALIGN_MIDDLE);
        label(mid, kStripY + 20, "KEYS", kDim, 10, ALIGN_CENTER | ALIGN_MIDDLE);
    }

    void drawScopeLabels()
    {
        static const char* names[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
        char buf[64];
        if (fNote >= 0) {
            const float hz = 440.0f * std::exp2((fPitch - 69) / 12);
            std::snprintf(buf, sizeof(buf), "%s%d  %.1f Hz", names[fNote % 12], fNote / 12 - 1, hz);
            label(14, 16, buf, kFg, 12);
            std::snprintf(buf, sizeof(buf), "%d %s", fVoices, fVoices == 1 ? "note" : "notes");
            label(kTube - 14, 16, buf, kDim, 11, ALIGN_RIGHT | ALIGN_MIDDLE);
        }
        label(kTube / 2, kTube - 12, "scope-synth", kDim, 10, ALIGN_CENTER | ALIGN_MIDDLE);
    }

    // ---- controls: knobs, faders, [x] switches, < choices >
    void valueText(int p, char* out, size_t n) const
    {
        const float v = fValues[p];
        switch (p) {
        case kAShape: case kBShape: {  // where along the playlist: slot 1.0 .. slot n
            const int count = slots(p == kBShape);
            if (count < 2) std::snprintf(out, n, "-");
            else std::snprintf(out, n, "%.1f", 1 + v * (count - 1));
            break;
        }
        case kAOct: case kBOct: case kASemi: case kBSemi: std::snprintf(out, n, "%+d", (int)std::lround(v)); break;
        case kAVoices: case kBVoices: std::snprintf(out, n, "%d", (int)std::lround(v)); break;
        case kAFine: case kBFine: std::snprintf(out, n, "%+dct", (int)std::lround(v)); break;
        case kARot: case kBRot: std::snprintf(out, n, "%ddeg", (int)std::lround(v)); break;
        case kAttack: case kDecay: case kRelease: case kGlide:
            if (v < 1) std::snprintf(out, n, "%dms", (int)std::lround(v * 1000));
            else std::snprintf(out, n, "%.2fs", v);
            break;
        case kLfoRate: std::snprintf(out, n, "%.2fHz", v); break;
        case kLfoWave: std::snprintf(out, n, "%s", kLfoWaves[std::clamp((int)std::lround(v), 0, 4)]); break;
        case kPoly: std::snprintf(out, n, "%s", v > 0.5f ? "POLY" : "MONO"); break;
        case kLegato: std::snprintf(out, n, "%s", v > 0.5f ? "ON" : "OFF"); break;
        default: std::snprintf(out, n, "%d%%", (int)std::lround(v * 100)); break;
        }
    }

    // where a link's tag sits under its control (false: not linked)
    bool tagBox(const Ctl& c, int src, Box& r) const
    {
        const int d = destIndex(c.param);
        if (d < 0 || fValues[modParam(src, d)] == 0) return false;
        const bool both = fValues[modParam(0, d)] != 0 && fValues[modParam(1, d)] != 0;
        const float w = 26, cx = c.x + c.w / 2;
        r = {both ? (src == 0 ? cx - w - 1 : cx + 1) : cx - w / 2, c.y + c.h - 18, w, 13};
        return true;
    }

    void drawCtl(const Ctl& c)
    {
        const int p = c.param;
        const Kind kind = kInfo[p].kind;
        const float cx = c.x + c.w / 2;
        char buf[48];
        valueText(p, buf, sizeof(buf));
        const int d = destIndex(p);
        if (fLinkSrc >= 0 && d >= 0)  // dragging a link: show where it can go
            outline({c.x + 2, c.y + 2, c.w - 4, c.h - 4}, Color(kSrcCol[fLinkSrc].red, kSrcCol[fLinkSrc].green, kSrcCol[fLinkSrc].blue, 0.7f));
        const bool dim = (p == kAShape || p == kBShape) && slots(p == kBShape) < 2;  // one shape: nothing to morph
        label(cx, c.y + 12, kInfo[p].caption, dim ? kFaint : kDim, 9.5f, ALIGN_CENTER | ALIGN_MIDDLE);
        if (kind == kSwitch) {
            const bool on = fValues[p] > 0.5f;
            label(cx, c.y + 38, on ? "[x]" : "[ ]", kFg, 14, ALIGN_CENTER | ALIGN_MIDDLE);
            label(cx, c.y + 62, buf, on ? kBright : kDim, 10, ALIGN_CENTER | ALIGN_MIDDLE);
            return;
        }
        if (kind == kChoice) {
            label(cx, c.y + 38, (std::string("< ") + buf + " >").c_str(), kBright, 10, ALIGN_CENTER | ALIGN_MIDDLE);
            return;
        }
        const float n = toNorm(p, fValues[p]);
        const Color& col = dim ? kDim : kFg;
        // a link's range (how far it can move the control; the LFO swings both ways) and its tag
        auto links = [&](auto drawRange) {
            bool linked = false;
            for (int src = 0; d >= 0 && src < kNumSources; ++src) {
                const float a = fValues[modParam(src, d)];
                if (a == 0) continue;
                linked = true;
                const float lo = std::clamp(src == kSrcLfo ? n - std::fabs(a) : std::min(n, n + a), 0.0f, 1.0f);
                const float hi = std::clamp(src == kSrcLfo ? n + std::fabs(a) : std::max(n, n + a), 0.0f, 1.0f);
                drawRange(src, lo, hi);
                Box t;
                tagBox(c, src, t);
                outline(t, kSrcCol[src]);
                char tb[16];
                std::snprintf(tb, sizeof(tb), "%c%+d", kSrcName[src][0], (int)std::lround(a * 100));
                label(t.x + t.w / 2, t.y + t.h / 2 + 0.5f, tb, kSrcCol[src], 8.5f, ALIGN_CENTER | ALIGN_MIDDLE);
            }
            return linked;
        };
        if (isFader(p)) {
            // a vertical fader: a track, lit from the bottom up to a thumb
            const float top = c.y + 20, bottom = c.y + 52, ty = bottom - (bottom - top) * n;
            fillBox({cx - 1, top, 2, bottom - top}, kFaint);
            fillBox({cx - 1, ty, 2, bottom - ty}, col);
            const bool linked = links([&](int src, float lo, float hi) {
                fillBox({cx + 6 + 3 * src, bottom - (bottom - top) * hi, 2, std::max((bottom - top) * (hi - lo), 1.0f)}, kSrcCol[src]);
            });
            fillBox({cx - 9, ty - 2.5f, 18, 5}, kBg);
            outline({cx - 9, ty - 2.5f, 18, 5}, col);
            if (linked)  // where the links have it right now
                fillBox({cx - 12, bottom - (bottom - top) * toNorm(p, liveValue(p)) - 1, 24, 2}, kBright);
            label(cx, c.y + 62, buf, dim ? kFaint : kBright, 10, ALIGN_CENTER | ALIGN_MIDDLE);
            return;
        }
        // a knob: a 270-degree track, lit up to the pointer
        const float r = 12, ky = c.y + 34;
        const float a0 = 0.75f * 3.14159265f, span = 1.5f * 3.14159265f, a = a0 + span * n;
        lineCap(BUTT);
        beginPath();
        arc(cx, ky, r + 3, a0, a0 + span, CW);
        strokeColor(kFaint);
        strokeWidth(2);
        stroke();
        if (n > 0.002f) {
            beginPath();
            arc(cx, ky, r + 3, a0, a, CW);
            strokeColor(col);
            strokeWidth(2);
            stroke();
        }
        const bool linked = links([&](int src, float lo, float hi) {
            if (hi - lo < 0.002f) return;
            beginPath();
            arc(cx, ky, r + 6 + 3 * src, a0 + span * lo, a0 + span * hi, CW);
            strokeColor(kSrcCol[src]);
            strokeWidth(1.5f);
            stroke();
        });
        beginPath();
        circle(cx, ky, r - 2);
        fillColor(kBg);
        fill();
        strokeColor(dim ? kFaint : kDim);
        strokeWidth(1);
        stroke();
        beginPath();
        moveTo(cx + std::cos(a) * 2, ky + std::sin(a) * 2);
        lineTo(cx + std::cos(a) * (r - 3), ky + std::sin(a) * (r - 3));
        strokeColor(dim ? kDim : kBright);
        strokeWidth(1.5f);
        stroke();
        if (linked) {  // where the links have it right now
            const float la = a0 + span * toNorm(p, liveValue(p));
            beginPath();
            circle(cx + std::cos(la) * (r + 3), ky + std::sin(la) * (r + 3), 2);
            fillColor(kBright);
            fill();
        }
        label(cx, c.y + 62, buf, dim ? kFaint : kBright, 10, ALIGN_CENTER | ALIGN_MIDDLE);
    }

    // the envelope and the main volume are faders; everything else that turns is a knob
    static bool isFader(int p) { return (p >= kAttack && p <= kRelease) || p == kVolume; }

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
