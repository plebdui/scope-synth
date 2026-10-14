// The factory presets: each one only lists what it changes from the defaults
// (Params.hpp). Built-in shapes only, so they work in every project.
// Shape numbers: 0 ROSE, 1 HEART, 2 BUTTERFLY, 3 SPIROGRAPH, 4 LISSAJOUS KNOT,
// 5 INFINITY, 6 SPINNING LISSAJOUS, 7 MORPHING ROSE, 8 BREATHING SPIROGRAPH,
// 9 BEATING HEART, 10 ROTATING CUBE. Filter types: 0 LP12, 1 LP24, 2 HP, 3 BP, 4 NOTCH.
#pragma once

#include "Params.hpp"
#include <utility>
#include <vector>

namespace ss {

struct Preset {
    const char* category;
    const char* name;
    std::vector<std::pair<int, float>> set;

    // every parameter's value for this preset
    void values(float* out) const
    {
        for (int p = 0; p < kParamCount; ++p)
            out[p] = paramDefault(p);
        for (const auto& kv : set)
            out[kv.first] = kv.second;
    }
};

inline int env(int p) { return modParam(kSrcEnv, destIndex(p)); }
inline int lfo(int p) { return modParam(kSrcLfo, destIndex(p)); }

inline const std::vector<Preset>& presets()
{
    static const std::vector<Preset> list = {
        {"", "INIT", {}},

        {"BASS", "SUB SCOPE", {{kASlot0, 5}, {kAOct, -1}, {kFilterType, 0}, {kCutoff, 600}, {kRes, 0.2f},
                               {kAttack, 0.002f}, {kDecay, 0.3f}, {kSustain, 0.8f}, {kRelease, 0.12f},
                               {kPoly, 0}, {kLegato, 1}, {kGlide, 0.05f}, {kVolume, 0.75f}}},
        {"BASS", "CUBE GROWL", {{kASlot0, 10}, {kAOct, -1}, {kAVoices, 3}, {kBSlot0, 3}, {kBOct, -2}, {kBLevel, 0.5f},
                                {kFilterType, 1}, {kCutoff, 300}, {kRes, 0.45f}, {env(kCutoff), 0.55f},
                                {kAttack, 0.001f}, {kDecay, 0.35f}, {kSustain, 0.3f}, {kRelease, 0.15f}, {kPoly, 0}}},

        {"LEAD", "PHOSPHOR LEAD", {{kASlot0, 1}, {kAVoices, 4}, {kBSlot0, 0}, {kBSemi, 7}, {kBLevel, 0.4f},
                                   {kCutoff, 4000}, {kRes, 0.3f}, {kLfoRate, 5.5f}, {lfo(kAFine), 0.08f},
                                   {kAttack, 0.01f}, {kDecay, 0.5f}, {kSustain, 0.8f}, {kRelease, 0.25f},
                                   {kPoly, 0}, {kLegato, 1}, {kGlide, 0.08f}}},
        {"LEAD", "SPIRAL SAW", {{kASlot0, 3}, {kAVoices, 6}, {kFilterType, 1}, {kCutoff, 2500}, {kRes, 0.5f},
                                {env(kCutoff), 0.3f}, {kDecay, 0.6f}, {kSustain, 0.7f}}},

        {"SYNTH", "SCOPE BRASS", {{kASlot0, 2}, {kAVoices, 3}, {kBSlot0, 1}, {kBOct, -1}, {kBLevel, 0.5f},
                                  {kFilterType, 1}, {kCutoff, 800}, {kRes, 0.2f}, {env(kCutoff), 0.45f},
                                  {kAttack, 0.06f}, {kDecay, 0.6f}, {kSustain, 0.7f}, {kRelease, 0.3f}}},
        {"SYNTH", "MORPH WAVE", {{kASlots, 3}, {kASlot0, 0}, {kASlot0 + 1, 1}, {kASlot0 + 2, 10}, {kAShape, 0.5f},
                                 {kLfoRate, 0.3f}, {kLfoWave, 1}, {lfo(kAShape), 0.5f}, {kCutoff, 6000}}},

        {"PLUCK", "GLASS PLUCK", {{kASlot0, 0}, {kBSlot0, 4}, {kBSemi, 12}, {kBLevel, 0.4f}, {kCutoff, 600},
                                  {kRes, 0.35f}, {env(kCutoff), 0.6f}, {kAttack, 0.001f}, {kDecay, 0.25f},
                                  {kSustain, 0}, {kRelease, 0.25f}}},
        {"PLUCK", "WIRE PLUCK", {{kASlot0, 10}, {kAVoices, 2}, {kFilterType, 1}, {kCutoff, 1500}, {env(kCutoff), 0.4f},
                                 {kAttack, 0.001f}, {kDecay, 0.15f}, {kSustain, 0}, {kRelease, 0.15f}}},

        {"PAD", "AURORA PAD", {{kASlot0, 8}, {kAVoices, 6}, {kBSlot0, 7}, {kBOct, 1}, {kBLevel, 0.5f}, {kBVoices, 4},
                               {kAttack, 1.2f}, {kDecay, 2}, {kSustain, 0.8f}, {kRelease, 2.5f}, {kCutoff, 1800},
                               {kRes, 0.15f}, {kLfoRate, 0.2f}, {lfo(kARot), 0.3f}, {lfo(kCutoff), 0.15f}, {kVolume, 0.4f}}},
        {"PAD", "SLOW ORBIT", {{kASlots, 2}, {kASlot0, 5}, {kASlot0 + 1, 6}, {kAShape, 0.5f}, {kAVoices, 4},
                               {kLfoRate, 0.1f}, {kLfoWave, 1}, {lfo(kAShape), 0.5f}, {kFilterType, 1}, {kCutoff, 3000},
                               {kAttack, 0.8f}, {kDecay, 1.5f}, {kSustain, 0.9f}, {kRelease, 3}, {kVolume, 0.5f}}},

        {"FX", "RADAR SWEEP", {{kASlot0, 6}, {kAttack, 0.6f}, {kDecay, 3}, {kSustain, 0.6f}, {kRelease, 4},
                               {env(kASemi), 0.5f}, {kLfoRate, 8}, {kLfoWave, 4}, {lfo(kCutoff), 0.4f},
                               {kFilterType, 3}, {kCutoff, 1500}, {kRes, 0.5f}}},
        {"FX", "TUBE NOISE", {{kASlot0, 10}, {kLfoRate, 12}, {kLfoWave, 4}, {lfo(kASemi), 0.6f}, {lfo(kARot), 1},
                              {kFilterType, 2}, {kCutoff, 400}, {kRes, 0.3f}, {kAttack, 0.01f}, {kDecay, 1},
                              {kSustain, 0.5f}, {kRelease, 1.5f}}},

        {"ARP", "ARP BLIP", {{kASlot0, 4}, {kAOct, 1}, {kCutoff, 3000}, {kRes, 0.4f}, {env(kCutoff), 0.3f},
                             {kAttack, 0.001f}, {kDecay, 0.12f}, {kSustain, 0}, {kRelease, 0.08f}}},
        {"ARP", "ARP SPARKLE", {{kASlot0, 0}, {kBSlot0, 1}, {kBOct, 1}, {kBSemi, 7}, {kBLevel, 0.4f},
                                {kDecay, 0.18f}, {kSustain, 0}, {kRelease, 0.2f}, {kLfoRate, 4}, {kLfoWave, 3},
                                {lfo(kARot), 0.25f}}},

        {"KEYS", "SCOPE EP", {{kASlot0, 1}, {kBSlot0, 0}, {kBOct, 1}, {kBLevel, 0.3f}, {kCutoff, 2500},
                              {env(kCutoff), 0.3f}, {kAttack, 0.002f}, {kDecay, 1.2f}, {kSustain, 0.35f},
                              {kRelease, 0.4f}, {kLfoRate, 4.5f}, {lfo(kVolume), 0.1f}, {kVolume, 0.5f}}},
        {"KEYS", "TERMINAL ORGAN", {{kASlot0, 5}, {kBSlot0, 5}, {kBOct, 1}, {kBLevel, 0.6f}, {kCutoff, 5000},
                                    {kAttack, 0.005f}, {kDecay, 0.1f}, {kSustain, 1}, {kRelease, 0.08f}, {kVolume, 0.35f}}},

        {"BELLS", "CRT BELL", {{kASlot0, 0}, {kBSlot0, 3}, {kBOct, 2}, {kBSemi, 4}, {kBLevel, 0.5f}, {kCutoff, 8000},
                               {kAttack, 0.001f}, {kDecay, 2.5f}, {kSustain, 0}, {kRelease, 2}, {kVolume, 0.55f}}},
        {"BELLS", "GLASS CHIME", {{kASlot0, 2}, {kAOct, 1}, {kBSlot0, 0}, {kBOct, 2}, {kBSemi, 7}, {kBLevel, 0.4f},
                                  {kFilterType, 2}, {kCutoff, 300}, {kAttack, 0.001f}, {kDecay, 1.8f},
                                  {kSustain, 0}, {kRelease, 1.8f}}},
    };
    return list;
}

} // namespace ss
