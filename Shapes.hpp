// The shapes an oscillator can play: the built-in curves, then any typed in as
// x(t) and y(t). Which ones each oscillator plays are parameters (Params.hpp);
// the typed-in ones are saved with the project as plain text, one a line:
//   C sin(3t) | cos(2t)    (x | y), numbered after the built-ins
#pragma once

#include "Equations.hpp"
#include "Expr.hpp"
#include "Params.hpp"
#include <sstream>

namespace ss {

static constexpr int kTableN = 1024;  // points per shape, around once

struct CustomShape {
    std::string xs, ys;
    Expr x, y;
    float scale = 1;            // fits it to -0.8..0.8 like the built-ins
    std::vector<float> X, Y;    // traced once, centred (still shapes; animated ones are traced as they play)

    std::string compile()
    {
        const std::string ex = x.compile(xs), ey = y.compile(ys);
        return !ex.empty() ? "x: " + ex : !ey.empty() ? "y: " + ey : "";
    }
    bool animated() const { return x.animated() || y.animated(); }

    void point(double t01, double u, float& px, float& py) const
    {
        const double t = t01 * 6.283185307179586;
        const double a = x.ok() ? x.eval(t, u) : 0, b = y.ok() ? y.eval(t, u) : 0;
        px = std::isfinite(a) ? (float)(a * scale) : 0.0f;
        py = std::isfinite(b) ? (float)(b * scale) : 0.0f;
    }

    // after compile(): find its size, and trace it if it stands still
    void build()
    {
        scale = 1;
        double m = 1e-9;
        const int frames = animated() ? 48 : 1;
        for (int f = 0; f < frames; ++f)
            for (int i = 0; i < 2000; ++i) {
                float px, py;
                point(i / 2000.0, f * 0.37, px, py);
                m = std::max({m, (double)std::fabs(px), (double)std::fabs(py)});
            }
        scale = (float)std::min(0.8 / m, 1e6);
        X.assign(kTableN, 0);
        Y.assign(kTableN, 0);
        if (!animated())
            trace(X.data(), Y.data(), 0);
    }

    // into a table, centred on 0
    void trace(float* tx, float* ty, double u) const
    {
        float mx = 0, my = 0;
        for (int i = 0; i < kTableN; ++i) {
            point(i / (double)kTableN, u, tx[i], ty[i]);
            mx += tx[i];
            my += ty[i];
        }
        mx /= kTableN;
        my /= kTableN;
        for (int i = 0; i < kTableN; ++i) {
            tx[i] -= mx;
            ty[i] -= my;
        }
    }
};

static_assert(kBuiltinShapes == crt::kNumEquations, "Params.hpp counts the built-in shapes");

struct ShapeSet {
    std::vector<CustomShape> customs;

    int count() const { return crt::kNumEquations + (int)customs.size(); }
    bool isCustom(int id) const { return id >= crt::kNumEquations; }
    const CustomShape& custom(int id) const { return customs[id - crt::kNumEquations]; }

    std::string name(int id) const
    {
        if (!isCustom(id)) return crt::kEquationInfo[std::clamp(id, 0, crt::kNumEquations - 1)].name;
        return "MY SHAPE " + std::to_string(id - crt::kNumEquations + 1);
    }
    bool animated(int id) const { return isCustom(id) ? custom(id).animated() : crt::kEquationInfo[id].animated; }

    // a point on a shape, -0.8..0.8 (not centred)
    void point(int id, double t01, double u, float& x, float& y) const
    {
        if (isCustom(id)) custom(id).point(t01, u, x, y);
        else crt::equation(id, t01, u, x, y);
    }

    std::string save() const
    {
        std::string out;
        for (const CustomShape& c : customs)
            out += "C " + c.xs + " | " + c.ys + "\n";
        return out;
    }

    // read save()'s text
    void load(const std::string& text)
    {
        customs.clear();
        std::istringstream in(text);
        std::string line;
        while (std::getline(in, line)) {
            if (line.size() < 2) continue;
            if (line[0] == 'C' && line[1] == ' ' && (int)customs.size() < kMaxCustom) {
                const size_t bar = line.find('|');
                CustomShape c;
                c.xs = trim(line.substr(2, bar == std::string::npos ? std::string::npos : bar - 2));
                c.ys = bar == std::string::npos ? "0" : trim(line.substr(bar + 1));
                customs.push_back(c);
            }
        }
        for (CustomShape& c : customs) {
            c.compile();
            c.build();
        }
    }

    void removeCustom(int id) { customs.erase(customs.begin() + (id - crt::kNumEquations)); }

    static std::string trim(const std::string& s)
    {
        const size_t a = s.find_first_not_of(" \t\r"), b = s.find_last_not_of(" \t\r");
        return a == std::string::npos ? "" : s.substr(a, b - a + 1);
    }
};

} // namespace ss
