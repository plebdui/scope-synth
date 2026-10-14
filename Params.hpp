// Every parameter: its range, default and how a knob turns through it. Shared
// by the sound engine, the plugin (what the host sees) and the window.
// The order is fixed: hosts save parameters by number.
#pragma once

#include <algorithm>
#include <cmath>

namespace ss {

// shapes: the 11 built-in curves, then up to 16 typed in. Each oscillator
// plays a list of up to 8 of them (its slots), which MORPH blends through.
static constexpr int kBuiltinShapes = 11, kMaxCustom = 16, kMaxShapes = kBuiltinShapes + kMaxCustom, kMaxSlots = 8;

enum Param {
    kAShape = 0, kAOct, kASemi, kAFine, kARot, kALevel, kAVoices,  // oscillator A (SHAPE = MORPH through its playlist)
    kBShape, kBOct, kBSemi, kBFine, kBRot, kBLevel, kBVoices,      // oscillator B
    kAttack, kDecay, kSustain, kRelease,                 // envelope (also the volume of each note)
    kLfoRate, kLfoWave,                                  // LFO
    kPoly, kLegato, kGlide, kVolume,                     // voice / master
    kASlots, kASlot0, kBSlots = kASlot0 + kMaxSlots, kBSlot0,  // each oscillator's shape list (parameters, so they can be automated)
    kModBase = kBSlot0 + kMaxSlots,                      // then ENV -> each destination, then LFO -> each
};

// knobs the ENV and LFO can be linked to
static const int kDests[] = {kAShape, kASemi, kAFine, kARot, kALevel,
                             kBShape, kBSemi, kBFine, kBRot, kBLevel, kLfoRate, kVolume};
static constexpr int kNumDests = sizeof(kDests) / sizeof(kDests[0]);
enum Source { kSrcEnv = 0, kSrcLfo = 1, kNumSources = 2 };
static constexpr int kParamCount = kModBase + kNumSources * kNumDests;

// the parameter holding how much `src` moves destination number `d` (-1..1 of the knob's travel)
inline int modParam(int src, int d) { return kModBase + src * kNumDests + d; }
// destination number of a parameter, or -1 if it can't be linked
inline int destIndex(int p)
{
    for (int d = 0; d < kNumDests; ++d)
        if (kDests[d] == p) return d;
    return -1;
}

enum Curve { kLin, kLog, kSquare };
enum Kind { kKnob, kInt, kSwitch, kChoice };

struct ParamInfo {
    const char* name;    // what the host shows
    const char* symbol;
    const char* caption; // on the knob
    const char* unit;
    float min, max, def;
    Curve curve;
    Kind kind;
};

static const ParamInfo kInfo[kModBase] = {
    {"A Morph", "a_morph", "MORPH", "%", 0, 1, 0, kLin, kKnob},
    {"A Octave", "a_oct", "OCT", "", -3, 3, 0, kLin, kInt},
    {"A Semi", "a_semi", "SEMI", "st", -12, 12, 0, kLin, kInt},
    {"A Fine", "a_fine", "FINE", "ct", -100, 100, 0, kLin, kKnob},
    {"A Rotate", "a_rot", "ROTATE", "deg", -180, 180, 0, kLin, kKnob},
    {"A Level", "a_level", "LEVEL", "%", 0, 1, 0.8f, kLin, kKnob},
    {"A Voices", "a_voices", "VOICES", "", 1, 8, 1, kLin, kInt},
    {"B Morph", "b_morph", "MORPH", "%", 0, 1, 0, kLin, kKnob},
    {"B Octave", "b_oct", "OCT", "", -3, 3, 0, kLin, kInt},
    {"B Semi", "b_semi", "SEMI", "st", -12, 12, 0, kLin, kInt},
    {"B Fine", "b_fine", "FINE", "ct", -100, 100, 0, kLin, kKnob},
    {"B Rotate", "b_rot", "ROTATE", "deg", -180, 180, 0, kLin, kKnob},
    {"B Level", "b_level", "LEVEL", "%", 0, 1, 0, kLin, kKnob},
    {"B Voices", "b_voices", "VOICES", "", 1, 8, 1, kLin, kInt},
    {"Attack", "attack", "ATTACK", "s", 0.001f, 10, 0.005f, kLog, kKnob},
    {"Decay", "decay", "DECAY", "s", 0.001f, 10, 0.4f, kLog, kKnob},
    {"Sustain", "sustain", "SUSTAIN", "%", 0, 1, 0.7f, kLin, kKnob},
    {"Release", "release", "RELEASE", "s", 0.001f, 10, 0.3f, kLog, kKnob},
    {"LFO Rate", "lfo_rate", "RATE", "Hz", 0.05f, 20, 2, kLog, kKnob},
    {"LFO Wave", "lfo_wave", "WAVE", "", 0, 4, 0, kLin, kChoice},
    {"Poly", "poly", "POLY/MONO", "", 0, 1, 1, kLin, kSwitch},
    {"Legato", "legato", "LEGATO", "", 0, 1, 0, kLin, kSwitch},
    {"Glide", "glide", "GLIDE", "s", 0, 2, 0, kSquare, kKnob},
    {"Volume", "volume", "VOLUME", "%", 0, 1, 0.7f, kLin, kKnob},
    {"A Shapes", "a_shapes", "SHAPES", "", 1, kMaxSlots, 1, kLin, kInt},
    {"A Slot 1", "a_slot1", "SLOT", "", 0, kMaxShapes - 1, 0, kLin, kChoice},
    {"A Slot 2", "a_slot2", "SLOT", "", 0, kMaxShapes - 1, 0, kLin, kChoice},
    {"A Slot 3", "a_slot3", "SLOT", "", 0, kMaxShapes - 1, 0, kLin, kChoice},
    {"A Slot 4", "a_slot4", "SLOT", "", 0, kMaxShapes - 1, 0, kLin, kChoice},
    {"A Slot 5", "a_slot5", "SLOT", "", 0, kMaxShapes - 1, 0, kLin, kChoice},
    {"A Slot 6", "a_slot6", "SLOT", "", 0, kMaxShapes - 1, 0, kLin, kChoice},
    {"A Slot 7", "a_slot7", "SLOT", "", 0, kMaxShapes - 1, 0, kLin, kChoice},
    {"A Slot 8", "a_slot8", "SLOT", "", 0, kMaxShapes - 1, 0, kLin, kChoice},
    {"B Shapes", "b_shapes", "SHAPES", "", 1, kMaxSlots, 1, kLin, kInt},
    {"B Slot 1", "b_slot1", "SLOT", "", 0, kMaxShapes - 1, 1, kLin, kChoice},
    {"B Slot 2", "b_slot2", "SLOT", "", 0, kMaxShapes - 1, 0, kLin, kChoice},
    {"B Slot 3", "b_slot3", "SLOT", "", 0, kMaxShapes - 1, 0, kLin, kChoice},
    {"B Slot 4", "b_slot4", "SLOT", "", 0, kMaxShapes - 1, 0, kLin, kChoice},
    {"B Slot 5", "b_slot5", "SLOT", "", 0, kMaxShapes - 1, 0, kLin, kChoice},
    {"B Slot 6", "b_slot6", "SLOT", "", 0, kMaxShapes - 1, 0, kLin, kChoice},
    {"B Slot 7", "b_slot7", "SLOT", "", 0, kMaxShapes - 1, 0, kLin, kChoice},
    {"B Slot 8", "b_slot8", "SLOT", "", 0, kMaxShapes - 1, 0, kLin, kChoice},
};

static const char* const kLfoWaves[5] = {"SINE", "TRIANGLE", "SAW", "SQUARE", "RANDOM"};

// an oscillator's shape list, read from the parameters
inline int slotsParam(int o) { return o ? kBSlots : kASlots; }
inline int slotParam(int o, int i) { return (o ? kBSlot0 : kASlot0) + i; }
inline int slotCount(const float* P, int o) { return std::clamp((int)std::lround(P[slotsParam(o)]), 1, kMaxSlots); }
inline int slotShape(const float* P, int o, int i, int shapes)  // shapes: how many exist right now
{
    return std::clamp((int)std::lround(P[slotParam(o, i)]), 0, shapes - 1);
}

inline float paramMin(int p) { return p < kModBase ? kInfo[p].min : -1.0f; }
inline float paramMax(int p) { return p < kModBase ? kInfo[p].max : 1.0f; }
inline float paramDefault(int p) { return p < kModBase ? kInfo[p].def : 0.0f; }

// value <-> position on the knob (0..1)
inline float toNorm(int p, float v)
{
    const float lo = paramMin(p), hi = paramMax(p);
    const Curve c = p < kModBase ? kInfo[p].curve : kLin;
    float n;
    if (c == kLog)
        n = std::log(std::max(v, lo) / lo) / std::log(hi / lo);
    else if (c == kSquare)
        n = std::sqrt(std::max(v - lo, 0.0f) / (hi - lo));
    else
        n = (v - lo) / (hi - lo);
    return std::clamp(n, 0.0f, 1.0f);
}
inline float fromNorm(int p, float n)
{
    n = std::clamp(n, 0.0f, 1.0f);
    const float lo = paramMin(p), hi = paramMax(p);
    const Curve c = p < kModBase ? kInfo[p].curve : kLin;
    if (c == kLog) return lo * std::pow(hi / lo, n);
    if (c == kSquare) return lo + (hi - lo) * n * n;
    return lo + (hi - lo) * n;
}

} // namespace ss
