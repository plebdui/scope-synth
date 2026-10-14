#include "ScopeSynthShared.hpp"
#include <cstring>

START_NAMESPACE_DISTRHO

ScopeSynthPlugin::ScopeSynthPlugin()
    : Plugin(ss::kParamCount, 0, 2)
{
    fShapesCur.reset(new ss::ShapeSet());
    fShapes.store(fShapesCur.get());
    for (int p = 0; p < ss::kParamCount; ++p)
        fParams[p] = ss::paramDefault(p);
    fEngine.setSampleRate((float)getSampleRate());
}

// saved with the project: the typed-in shapes (Shapes.hpp has the format), and
// the name of the preset last picked (only the window shows it)
void ScopeSynthPlugin::initState(uint32_t index, State& state)
{
    if (index == 1) {
        state.key = "preset";
        state.label = "Preset";
        state.defaultValue = "INIT";
        return;
    }
    state.key = "shapes";
    state.label = "Shapes";
    state.defaultValue = ss::ShapeSet().save().c_str();
}

void ScopeSynthPlugin::setState(const char* key, const char* value)
{
    if (std::strcmp(key, "shapes") != 0)
        return;
    std::unique_ptr<ss::ShapeSet> set(new ss::ShapeSet());
    set->load(value);
    std::lock_guard<std::mutex> lock(fShapesLock);
    fShapes.store(set.get(), std::memory_order_release);
    fShapesOld = std::move(fShapesCur);
    fShapesCur = std::move(set);
}

void ScopeSynthPlugin::initAudioPort(bool input, uint32_t index, AudioPort& port)
{
    port.groupId = kPortGroupStereo;
    Plugin::initAudioPort(input, index, port);
}

void ScopeSynthPlugin::initParameter(uint32_t index, Parameter& p)
{
    using namespace ss;
    p.hints = kParameterIsAutomatable;
    p.ranges.min = paramMin(index);
    p.ranges.max = paramMax(index);
    p.ranges.def = paramDefault(index);
    int src, d;
    if (modOf((int)index, src, d)) {  // ENV / LFO link amounts
        p.name = String(src == kSrcEnv ? "ENV > " : "LFO > ") + kInfo[kDests[d]].name;
        p.symbol = paramSymbol((int)index).c_str();
        return;
    }
    const ParamInfo& in = kInfo[index];
    p.name = in.name;
    p.symbol = in.symbol;
    p.unit = in.unit;
    if (in.curve == kLog) p.hints |= kParameterIsLogarithmic;
    if (in.kind == kInt || in.kind == kChoice) p.hints |= kParameterIsInteger;
    if (in.kind == kSwitch) p.hints |= kParameterIsBoolean | kParameterIsInteger;
    if (index == kLfoWave) {
        static ParameterEnumerationValue v[5];
        for (int i = 0; i < 5; ++i) { v[i].value = (float)i; v[i].label = kLfoWaves[i]; }
        p.enumValues.count = 5; p.enumValues.restrictedMode = true; p.enumValues.values = v;
        p.enumValues.deleteLater = false;
    }
    if (index == kLfoDiv) {
        static ParameterEnumerationValue v[12];
        for (int i = 0; i < 12; ++i) { v[i].value = (float)i; v[i].label = kDivNames[i]; }
        p.enumValues.count = 12; p.enumValues.restrictedMode = true; p.enumValues.values = v;
        p.enumValues.deleteLater = false;
    }
    if (index == kNoiseType) {
        static ParameterEnumerationValue v[4];
        for (int i = 0; i < 4; ++i) { v[i].value = (float)i; v[i].label = kNoiseTypes[i]; }
        p.enumValues.count = 4; p.enumValues.restrictedMode = true; p.enumValues.values = v;
        p.enumValues.deleteLater = false;
    }
    if (index == kSubShape) {  // CIRCLE, then the shapes by name
        static ParameterEnumerationValue v[kMaxShapes + 1];
        static std::string names[kMaxShapes + 1];
        for (int i = 0; i <= kMaxShapes; ++i) {
            names[i] = i == 0 ? "CIRCLE" : i <= kBuiltinShapes ? crt::kEquationInfo[i - 1].name : "MY SHAPE " + std::to_string(i - kBuiltinShapes);
            v[i].value = (float)i;
            v[i].label = names[i].c_str();
        }
        p.enumValues.count = kMaxShapes + 1; p.enumValues.restrictedMode = true; p.enumValues.values = v;
        p.enumValues.deleteLater = false;
    }
    if (index == kFilterType) {
        static ParameterEnumerationValue v[5];
        for (int i = 0; i < 5; ++i) { v[i].value = (float)i; v[i].label = kFilterTypes[i]; }
        p.enumValues.count = 5; p.enumValues.restrictedMode = true; p.enumValues.values = v;
        p.enumValues.deleteLater = false;
    }
    if (index >= (uint32_t)kASlot0 && index < (uint32_t)kModBase && index != (uint32_t)kBSlots) {  // a slot: the shapes by name
        static ParameterEnumerationValue v[kMaxShapes];
        static std::string names[kMaxShapes];
        for (int i = 0; i < kMaxShapes; ++i) {
            names[i] = i < kBuiltinShapes ? crt::kEquationInfo[i].name : "MY SHAPE " + std::to_string(i - kBuiltinShapes + 1);
            v[i].value = (float)i;
            v[i].label = names[i].c_str();
        }
        p.enumValues.count = kMaxShapes; p.enumValues.restrictedMode = true; p.enumValues.values = v;
        p.enumValues.deleteLater = false;
    }
    if (index == kPoly) {
        static ParameterEnumerationValue v[2] = {{0, "Mono"}, {1, "Poly"}};
        p.enumValues.count = 2; p.enumValues.restrictedMode = true; p.enumValues.values = v;
        p.enumValues.deleteLater = false;
    }
}

float ScopeSynthPlugin::getParameterValue(uint32_t index) const
{
    return index < (uint32_t)ss::kParamCount ? fParams[index] : 0.0f;
}

void ScopeSynthPlugin::setParameterValue(uint32_t index, float value)
{
    if (index < (uint32_t)ss::kParamCount)
        fParams[index] = value;
}

void ScopeSynthPlugin::activate()
{
    fEngine.setSampleRate((float)getSampleRate());
}

void ScopeSynthPlugin::sampleRateChanged(double newSampleRate)
{
    fEngine.setSampleRate((float)newSampleRate);
}

void ScopeSynthPlugin::handleMidi(const uint8_t* d, uint32_t size)
{
    if (size < 2) return;
    const uint8_t status = d[0] & 0xF0;
    if (status == 0x90 && size >= 3) fEngine.noteOn(d[1], d[2]);
    else if (status == 0x80 && size >= 3) fEngine.noteOff(d[1]);
    else if (status == 0xE0 && size >= 3) fEngine.pitchBend(((d[2] << 7 | d[1]) - 8192) / 8192.0f * 2);  // +-2 semitones
    else if (status == 0xB0 && size >= 3 && (d[1] == 120 || d[1] == 123)) fEngine.allOff(d[1] == 120);
}

void ScopeSynthPlugin::run(const float**, float** outputs, uint32_t frames,
                           const MidiEvent* midiEvents, uint32_t midiEventCount)
{
    float* L = outputs[0];
    float* R = outputs[1];
    uiNotes.drain([this](int note, int vel) { vel ? fEngine.noteOn(note, vel) : fEngine.noteOff(note); });
    const ss::ShapeSet& shapes = *fShapes.load(std::memory_order_acquire);
    const TimePosition& tp = getTimePosition();  // for LFO SYNC
    if (tp.bbt.valid)
        fEngine.setTransport(tp.playing, (tp.bbt.bar - 1) * (double)tp.bbt.beatsPerBar + (tp.bbt.beat - 1) + tp.bbt.tick / tp.bbt.ticksPerBeat,
                             tp.bbt.beatsPerMinute);
    else
        fEngine.setTransport(false, 0, 120);
    uint32_t done = 0;
    for (uint32_t e = 0; e <= midiEventCount; ++e) {  // play up to each note, right on its sample
        const uint32_t at = e < midiEventCount ? std::min(midiEvents[e].frame, frames) : frames;
        if (at > done) {
            fEngine.render(L + done, R + done, at - done, shapes);
            done = at;
        }
        if (e < midiEventCount)
            handleMidi(midiEvents[e].size > MidiEvent::kDataSize ? midiEvents[e].dataExt : midiEvents[e].data,
                       midiEvents[e].size);
    }
    ring.write(L, R, frames);
    live.env.store(fEngine.envNow(), std::memory_order_relaxed);
    live.lfo.store(fEngine.lfoNow(), std::memory_order_relaxed);
    live.lfoPhase.store(fEngine.lfoPhase(), std::memory_order_relaxed);
    live.note.store(fEngine.newestNote(), std::memory_order_relaxed);
    live.pitch.store(fEngine.newestPitch(), std::memory_order_relaxed);
    live.voices.store(fEngine.activeVoices(), std::memory_order_relaxed);
    live.anim.store(fEngine.animTime(), std::memory_order_relaxed);
    uint64_t k[2] = {0, 0};
    for (int n = 0; n < 128; ++n)
        if (fEngine.keyDown(n)) k[n >> 6] |= 1ull << (n & 63);
    live.keys[0].store(k[0], std::memory_order_relaxed);
    live.keys[1].store(k[1], std::memory_order_relaxed);
}

Plugin* createPlugin()
{
    return new ScopeSynthPlugin();
}

END_NAMESPACE_DISTRHO
