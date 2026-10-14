#ifndef DISTRHO_PLUGIN_INFO_H_INCLUDED
#define DISTRHO_PLUGIN_INFO_H_INCLUDED

#define DISTRHO_PLUGIN_BRAND   "plebdui"
#define DISTRHO_PLUGIN_NAME    "Scope Synth"
#define DISTRHO_PLUGIN_URI     "https://github.com/plebdui/scope-synth"
#define DISTRHO_PLUGIN_CLAP_ID "io.github.plebdui.scopesynth"

#define DISTRHO_PLUGIN_BRAND_ID  Pleb
#define DISTRHO_PLUGIN_UNIQUE_ID ScSy

#define DISTRHO_PLUGIN_HAS_UI             1
#define DISTRHO_PLUGIN_IS_RT_SAFE         1
#define DISTRHO_PLUGIN_IS_SYNTH           1
#define DISTRHO_PLUGIN_NUM_INPUTS         0
#define DISTRHO_PLUGIN_NUM_OUTPUTS        2
#define DISTRHO_PLUGIN_WANT_DIRECT_ACCESS 1   // the window reads the sound and plays notes straight into the plugin
#define DISTRHO_PLUGIN_WANT_STATE         1   // the shapes: playlists and typed-in equations
#define DISTRHO_UI_USE_NANOVG             1
#define DISTRHO_UI_USER_RESIZABLE         0
#define DISTRHO_PLUGIN_VST3_CATEGORIES    "Instrument|Synth"
#define DISTRHO_PLUGIN_CLAP_FEATURES      "instrument", "synthesizer", "stereo"

#endif
