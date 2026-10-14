// Your own presets, as text files you can read, keep and share.
//   <settings>/Presets/<PACK>/<NAME>.ssp   one preset (a pack is a folder)
//   <anything>.sspack                      a whole pack in one file, to share
// Settings are saved by name (a_morph, cutoff, env_cutoff ...), not number, so
// files keep loading as the synth grows. Both kinds look like:
//   # Scope Synth preset pack
//   pack: DUSTY TAPES
//
//   [preset]
//   name: WARM KEYS
//   category: KEYS
//   author: someone
//   param cutoff 1200
//   shape sin(3t) | cos(2t)
// Plain C++, so it can be tested on its own.
#pragma once

#include "Presets.hpp"
#include <cctype>
#include <filesystem>
#include <fstream>
#include <locale>
#include <map>
#include <sstream>

namespace ss {

namespace fs = std::filesystem;

struct UserPreset {
    std::string name = "NEW PRESET", category = "SYNTH", author, pack = "MY PRESETS";
    float values[kParamCount];
    std::vector<std::pair<std::string, std::string>> shapes;  // typed shapes: x, y
    fs::path file;  // where it's kept (presets read from the Presets folder)

    UserPreset()
    {
        for (int p = 0; p < kParamCount; ++p)
            values[p] = paramDefault(p);
    }

    // its typed shapes, in place of the ones there were (like a factory preset)
    void shapesInto(ShapeSet& set) const
    {
        set.customs.clear();
        for (const auto& xy : shapes) {
            CustomShape c;
            c.xs = xy.first;
            c.ys = xy.second;
            c.compile();
            c.build();
            set.customs.push_back(c);
        }
    }
};

// numbers always with a dot, whatever the language the computer is set to
inline std::string numText(float v)
{
    std::ostringstream o;
    o.imbue(std::locale::classic());
    o.precision(9);
    o << v;
    return o.str();
}

inline std::string upper(std::string s)
{
    for (char& c : s) c = (char)std::toupper((unsigned char)c);
    return s;
}

// one preset's block
inline std::string presetText(const UserPreset& p)
{
    std::string t = "[preset]\nname: " + p.name + "\ncategory: " + p.category + "\n";
    if (!p.author.empty()) t += "author: " + p.author + "\n";
    for (int i = 0; i < kParamCount; ++i)
        t += "param " + paramSymbol(i) + " " + numText(p.values[i]) + "\n";
    for (const auto& xy : p.shapes)
        t += "shape " + xy.first + " | " + xy.second + "\n";
    return t;
}

inline std::string fileText(const UserPreset& p)
{
    return "# Scope Synth preset\npack: " + p.pack + "\n\n" + presetText(p);
}

inline std::string packText(const std::string& pack, const std::vector<UserPreset>& list)
{
    std::string t = "# Scope Synth preset pack\npack: " + pack + "\n";
    for (const UserPreset& p : list) t += "\n" + presetText(p);
    return t;
}

// every preset in a .ssp or .sspack's text; anything it doesn't know is skipped
inline std::vector<UserPreset> parsePresets(const std::string& text, const std::string& fallbackPack)
{
    static const std::map<std::string, int> bySymbol = [] {
        std::map<std::string, int> m;
        for (int p = 0; p < kParamCount; ++p) m[paramSymbol(p)] = p;
        return m;
    }();
    auto trim = [](const std::string& s) { return ShapeSet::trim(s); };
    std::vector<UserPreset> out;
    std::string pack = fallbackPack;
    bool open = false;
    std::istringstream in(text);
    std::string line;
    auto start = [&] { out.emplace_back(); out.back().pack = pack; open = true; };
    while (std::getline(in, line)) {
        line = trim(line);
        if (line.empty() || line[0] == '#') continue;
        if (line == "[preset]") { start(); continue; }
        const size_t colon = line.find(':');
        const std::string key = colon == std::string::npos ? "" : trim(line.substr(0, colon));
        const std::string val = colon == std::string::npos ? "" : trim(line.substr(colon + 1));
        if (key == "pack") {
            pack = upper(val);
            if (open) out.back().pack = pack;
            continue;
        }
        if (key == "name" && !open) start();  // a file without [preset]
        if (!open) continue;
        UserPreset& p = out.back();
        if (key == "name") p.name = upper(val);
        else if (key == "category") p.category = upper(val);
        else if (key == "author") p.author = val;
        else if (line.rfind("param ", 0) == 0) {
            std::istringstream ps(line.substr(6));
            ps.imbue(std::locale::classic());
            std::string sym;
            float v;
            if (ps >> sym >> v) {
                const auto it = bySymbol.find(sym);
                if (it != bySymbol.end() && std::isfinite(v))
                    p.values[it->second] = std::clamp(v, paramMin(it->second), paramMax(it->second));
            }
        } else if (line.rfind("shape ", 0) == 0 && (int)p.shapes.size() < kMaxCustom) {
            const std::string xy = line.substr(6);
            const size_t bar = xy.find('|');
            p.shapes.push_back({trim(xy.substr(0, bar)), bar == std::string::npos ? "0" : trim(xy.substr(bar + 1))});
        }
    }
    return out;
}

inline UserPreset fromFactory(const Preset& f)
{
    UserPreset p;
    f.values(p.values);
    for (const auto& xy : f.shapes) p.shapes.push_back({xy.first, xy.second});
    p.name = f.name;
    p.category = *f.category ? f.category : "SYNTH";
    return p;
}

// a safe file name: letters, digits, space, - and _
inline std::string fileName(const std::string& s)
{
    std::string out;
    for (char c : s) out += std::isalnum((unsigned char)c) || c == ' ' || c == '-' || c == '_' ? c : '_';
    out = ShapeSet::trim(out);
    return out.empty() ? "UNTITLED" : out;
}

inline std::string readFile(const fs::path& f)
{
    std::ifstream in(f, std::ios::binary);
    std::ostringstream s;
    s << in.rdbuf();
    return s.str();
}

// the packs in the Presets folder, sorted, each with its presets sorted by name
struct Packs {
    std::vector<std::string> names;
    std::map<std::string, std::vector<UserPreset>> presets;
};
inline Packs scanPacks(const fs::path& dir)
{
    Packs packs;
    std::error_code ec;
    for (const auto& d : fs::directory_iterator(dir, ec)) {
        if (!d.is_directory(ec)) continue;
        const std::string pack = upper(d.path().filename().string());
        std::vector<UserPreset> list;
        for (const auto& f : fs::directory_iterator(d.path(), ec)) {
            if (f.path().extension() != ".ssp") continue;
            auto ps = parsePresets(readFile(f.path()), pack);
            if (ps.empty()) continue;
            ps[0].pack = pack;  // the folder it's in wins
            ps[0].file = f.path();
            list.push_back(ps[0]);
        }
        if (list.empty()) continue;
        std::sort(list.begin(), list.end(), [](const UserPreset& a, const UserPreset& b) { return a.name < b.name; });
        packs.names.push_back(pack);
        packs.presets[pack] = list;
    }
    std::sort(packs.names.begin(), packs.names.end());
    return packs;
}

// into <dir>/<PACK>/<NAME>.ssp (replacing one with the same name)
inline bool savePreset(const fs::path& dir, UserPreset& p)
{
    std::error_code ec;
    const fs::path folder = dir / fileName(p.pack);
    fs::create_directories(folder, ec);
    p.file = folder / (fileName(p.name) + ".ssp");
    std::ofstream out(p.file, std::ios::binary);
    out << fileText(p);
    return (bool)out;
}

// a .ssp or .sspack from someone: its presets join the Presets folder. How many came in.
inline int importFile(const fs::path& dir, const fs::path& file)
{
    int n = 0;
    for (UserPreset& p : parsePresets(readFile(file), upper(file.stem().string())))
        n += savePreset(dir, p);
    return n;
}

inline bool exportPack(const fs::path& dir, const std::string& pack, const fs::path& file)
{
    const Packs packs = scanPacks(dir);
    const auto it = packs.presets.find(pack);
    if (it == packs.presets.end()) return false;
    std::ofstream out(file, std::ios::binary);
    out << packText(pack, it->second);
    return (bool)out;
}

} // namespace ss
