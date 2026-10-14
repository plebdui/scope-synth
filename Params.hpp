// Every parameter: its range, default and how a knob turns through it. Shared
// by the sound engine, the plugin (what the host sees) and the window.
// The order is fixed: hosts save parameters by number.
#pragma once

#include <algorithm>
#include <cmath>
#include <string>

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
    kModBase = kBSlot0 + kMaxSlots,                      // then ENV -> each of the first 12 destinations, then LFO -> each
    kFilterType = kModBase + 2 * 12, kCutoff, kRes,      // added in 1.1 (after the 1.0 ones, so 1.0 projects still load)
    kLfoSync, kLfoDiv, kLfoDraw, kLfoSteps, kLfoStep0,   // LFO: tempo sync, and DRAW mode's steps
    kModBase2 = kLfoStep0 + 16,                          // ENV, LFO -> cutoff and resonance, a pair each
    kSubLevel = kModBase2 + 4, kSubOct, kNoiseLevel,     // added in 1.2: SUB and NOISE
    kModBase3,                                           // ENV, LFO -> sub and noise level, a pair each
};

// knobs the ENV and LFO can be linked to
static const int kDests[] = {kAShape, kASemi, kAFine, kARot, kALevel,
                             kBShape, kBSemi, kBFine, kBRot, kBLevel, kLfoRate, kVolume,
                             kCutoff, kRes,
                             kSubLevel, kNoiseLevel};
static constexpr int kNumDests = sizeof(kDests) / sizeof(kDests[0]), kDests1 = 12, kDests2 = 14;
enum Source { kSrcEnv = 0, kSrcLfo = 1, kNumSources = 2 };
static constexpr int kParamCount = kModBase3 + kNumSources * (kNumDests - kDests2);

// the parameter holding how much `src` moves destination number `d` (-1..1 of the knob's travel)
inline int modParam(int src, int d)
{
    if (d < kDests1) return kModBase + src * kDests1 + d;
    if (d < kDests2) return kModBase2 + (d - kDests1) * 2 + src;
    return kModBase3 + (d - kDests2) * 2 + src;
}
// the other way: is p a link amount, and which
inline bool modOf(int p, int& src, int& d)
{
    if (p >= kModBase && p < kFilterType) { src = (p - kModBase) / kDests1; d = (p - kModBase) % kDests1; return true; }
    if (p >= kModBase2 && p < kSubLevel) { src = (p - kModBase2) % 2; d = kDests1 + (p - kModBase2) / 2; return true; }
    if (p >= kModBase3 && p < kParamCount) { src = (p - kModBase3) % 2; d = kDests2 + (p - kModBase3) / 2; return true; }
    return false;
}
inline bool isMod(int p) { int s, d; return modOf(p, s, d); }
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

static const ParamInfo kInfo[kModBase3] = {
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
    // placeholders for the ENV / LFO amounts of the first 12 destinations (info() gives their range)
#define SS_MOD {"", "", "", "", -1, 1, 0, kLin, kKnob}
    SS_MOD, SS_MOD, SS_MOD, SS_MOD, SS_MOD, SS_MOD, SS_MOD, SS_MOD, SS_MOD, SS_MOD, SS_MOD, SS_MOD,
    SS_MOD, SS_MOD, SS_MOD, SS_MOD, SS_MOD, SS_MOD, SS_MOD, SS_MOD, SS_MOD, SS_MOD, SS_MOD, SS_MOD,
#undef SS_MOD
    {"Filter Type", "filter_type", "TYPE", "", 0, 4, 0, kLin, kChoice},
    {"Cutoff", "cutoff", "CUTOFF", "Hz", 20, 20000, 20000, kLog, kKnob},
    {"Resonance", "res", "RES", "%", 0, 1, 0, kLin, kKnob},
    {"LFO Sync", "lfo_sync", "SYNC", "", 0, 1, 0, kLin, kSwitch},
    {"LFO Division", "lfo_div", "RATE", "", 0, 11, 4, kLin, kChoice},
    {"LFO Draw", "lfo_draw", "DRAW", "", 0, 1, 0, kLin, kSwitch},
    {"LFO Steps", "lfo_steps", "STEPS", "", 1, 16, 8, kLin, kInt},
    {"LFO Step 1", "lfo_step1", "STEP", "", -1, 1, 0, kLin, kKnob},
    {"LFO Step 2", "lfo_step2", "STEP", "", -1, 1, 0, kLin, kKnob},
    {"LFO Step 3", "lfo_step3", "STEP", "", -1, 1, 0, kLin, kKnob},
    {"LFO Step 4", "lfo_step4", "STEP", "", -1, 1, 0, kLin, kKnob},
    {"LFO Step 5", "lfo_step5", "STEP", "", -1, 1, 0, kLin, kKnob},
    {"LFO Step 6", "lfo_step6", "STEP", "", -1, 1, 0, kLin, kKnob},
    {"LFO Step 7", "lfo_step7", "STEP", "", -1, 1, 0, kLin, kKnob},
    {"LFO Step 8", "lfo_step8", "STEP", "", -1, 1, 0, kLin, kKnob},
    {"LFO Step 9", "lfo_step9", "STEP", "", -1, 1, 0, kLin, kKnob},
    {"LFO Step 10", "lfo_step10", "STEP", "", -1, 1, 0, kLin, kKnob},
    {"LFO Step 11", "lfo_step11", "STEP", "", -1, 1, 0, kLin, kKnob},
    {"LFO Step 12", "lfo_step12", "STEP", "", -1, 1, 0, kLin, kKnob},
    {"LFO Step 13", "lfo_step13", "STEP", "", -1, 1, 0, kLin, kKnob},
    {"LFO Step 14", "lfo_step14", "STEP", "", -1, 1, 0, kLin, kKnob},
    {"LFO Step 15", "lfo_step15", "STEP", "", -1, 1, 0, kLin, kKnob},
    {"LFO Step 16", "lfo_step16", "STEP", "", -1, 1, 0, kLin, kKnob},
    {"", "", "", "", -1, 1, 0, kLin, kKnob}, {"", "", "", "", -1, 1, 0, kLin, kKnob},  // ENV / LFO -> cutoff, resonance
    {"", "", "", "", -1, 1, 0, kLin, kKnob}, {"", "", "", "", -1, 1, 0, kLin, kKnob},
    {"Sub Level", "sub_level", "SUB", "%", 0, 1, 0, kLin, kKnob},
    {"Sub Octave", "sub_oct", "SUB OCT", "", -2, -1, -1, kLin, kInt},
    {"Noise Level", "noise_level", "NOISE", "%", 0, 1, 0, kLin, kKnob},
};

static const char* const kLfoWaves[5] = {"SINE", "TRIANGLE", "SAW", "SQUARE", "RANDOM"};
static const char* const kFilterTypes[5] = {"LP12", "LP24", "HP", "BP", "NOTCH"};
// LFO SYNC: note lengths, and how many beats each is
static const char* const kDivNames[12] = {"1/1", "1/2", "1/4", "1/8", "1/16", "1/32", "1/4T", "1/8T", "1/16T", "1/4.", "1/8.", "1/16."};
static const double kDivBeats[12] = {4, 2, 1, 0.5, 0.25, 0.125, 2.0 / 3, 1.0 / 3, 1.0 / 6, 1.5, 0.75, 0.375};

// any parameter's range and kind (link amounts: -1..1)
inline const ParamInfo& info(int p)
{
    static const ParamInfo mod = {"", "", "", "", -1, 1, 0, kLin, kKnob};
    return isMod(p) || p < 0 || p >= kModBase3 ? mod : kInfo[p];
}

// an oscillator's shape list, read from the parameters
inline int slotsParam(int o) { return o ? kBSlots : kASlots; }
inline int slotParam(int o, int i) { return (o ? kBSlot0 : kASlot0) + i; }
inline int slotCount(const float* P, int o) { return std::clamp((int)std::lround(P[slotsParam(o)]), 1, kMaxSlots); }
inline int slotShape(const float* P, int o, int i, int shapes)  // shapes: how many exist right now
{
    return std::clamp((int)std::lround(P[slotParam(o, i)]), 0, shapes - 1);
}

// the name a parameter is saved under (preset files, hosts): stays the same across versions
inline std::string paramSymbol(int p)
{
    int src, d;
    if (modOf(p, src, d)) return std::string(src == kSrcEnv ? "env_" : "lfo_") + kInfo[kDests[d]].symbol;
    return info(p).symbol;
}

inline float paramMin(int p) { return info(p).min; }
inline float paramMax(int p) { return info(p).max; }
inline float paramDefault(int p) { return info(p).def; }

// value <-> position on the knob (0..1)
inline float toNorm(int p, float v)
{
    const float lo = paramMin(p), hi = paramMax(p);
    const Curve c = info(p).curve;
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
    const Curve c = info(p).curve;
    if (c == kLog) return lo * std::pow(hi / lo, n);
    if (c == kSquare) return lo + (hi - lo) * n * n;
    return lo + (hi - lo) * n;
}

} // namespace ss
