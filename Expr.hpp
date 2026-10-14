// Typed-in equations: "sin(3t) + 0.5*cos(u)" turned into a short program that
// runs fast and never allocates (so the audio thread can run it). Knows
// t (0..2pi, once around), u (seconds, for animation), pi, e,
// + - * / % ^, implicit multiplication (3t, 2sin(t), (t+1)(t-1)) and
// sin cos tan asin acos atan sinh cosh tanh exp log ln sqrt abs floor ceil
// round sign, and pow min max atan2 mod with two arguments.
#pragma once

#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace ss {

class Expr {
public:
    // "" if it worked, else what's wrong
    std::string compile(const std::string& src)
    {
        code.clear();
        s = src.c_str();
        pos = 0;
        err.clear();
        usesTime = false;
        skip();
        if (!*s)
            return "empty";
        parseSum();
        skip();
        if (err.empty() && s[pos])
            fail(std::string("unexpected '") + s[pos] + "'");
        if (err.empty()) {
            int depth = 0, most = 0;
            for (const Ins& i : code) {
                depth += effect(i.op);
                most = std::max(most, depth);
            }
            if (most > kStack) fail("too long");
        }
        if (!err.empty()) code.clear();
        return err;
    }

    bool ok() const { return !code.empty(); }
    bool animated() const { return usesTime; }

    double eval(double t, double u) const
    {
        double st[kStack];
        int sp = 0;
        for (const Ins& i : code) {
            switch (i.op) {
            case kConst: st[sp++] = i.v; break;
            case kT: st[sp++] = t; break;
            case kU: st[sp++] = u; break;
            case kNeg: st[sp - 1] = -st[sp - 1]; break;
            case kFn1: st[sp - 1] = fn1(i.fn, st[sp - 1]); break;
            default: {
                const double b = st[--sp], a = st[sp - 1];
                st[sp - 1] = fn2(i.op, i.fn, a, b);
                break;
            }
            }
        }
        return sp ? st[0] : 0.0;
    }

private:
    enum Op : uint8_t { kConst, kT, kU, kNeg, kFn1, kAdd, kSub, kMul, kDiv, kMod, kPow, kFn2 };
    struct Ins { Op op; uint8_t fn; double v; };
    static constexpr int kStack = 64;

    std::vector<Ins> code;
    const char* s = "";
    size_t pos = 0;
    std::string err;
    bool usesTime = false;

    static int effect(Op op) { return op <= kU ? 1 : op <= kFn1 ? 0 : -1; }

    static const char* const* names1()
    {
        static const char* const n[] = {"sin", "cos", "tan", "asin", "acos", "atan", "sinh", "cosh", "tanh", "exp",
                                        "log", "ln", "sqrt", "abs", "floor", "ceil", "round", "sign", nullptr};
        return n;
    }
    static const char* const* names2()
    {
        static const char* const n[] = {"pow", "min", "max", "atan2", "mod", nullptr};
        return n;
    }
    static double fn1(int f, double x)
    {
        switch (f) {
        case 0: return std::sin(x);
        case 1: return std::cos(x);
        case 2: return std::tan(x);
        case 3: return std::asin(x);
        case 4: return std::acos(x);
        case 5: return std::atan(x);
        case 6: return std::sinh(x);
        case 7: return std::cosh(x);
        case 8: return std::tanh(x);
        case 9: return std::exp(x);
        case 10: case 11: return std::log(x);
        case 12: return std::sqrt(x);
        case 13: return std::fabs(x);
        case 14: return std::floor(x);
        case 15: return std::ceil(x);
        case 16: return std::round(x);
        default: return (x > 0) - (x < 0);
        }
    }
    static double fn2(Op op, int f, double a, double b)
    {
        switch (op) {
        case kAdd: return a + b;
        case kSub: return a - b;
        case kMul: return a * b;
        case kDiv: return a / b;
        case kMod: return std::fmod(a, b);
        case kPow: return std::pow(a, b);
        default:
            switch (f) {
            case 0: return std::pow(a, b);
            case 1: return std::min(a, b);
            case 2: return std::max(a, b);
            case 3: return std::atan2(a, b);
            default: return std::fmod(a, b);
            }
        }
    }

    void fail(const std::string& m)
    {
        if (err.empty()) err = m;
    }
    void skip()
    {
        while (s[pos] == ' ' || s[pos] == '\t') ++pos;
    }
    void emit(Op op, uint8_t fn = 0, double v = 0) { code.push_back({op, fn, v}); }

    void parseSum()
    {
        parseProduct();
        for (;;) {
            skip();
            const char c = s[pos];
            if (c != '+' && c != '-') return;
            ++pos;
            parseProduct();
            emit(c == '+' ? kAdd : kSub);
        }
    }

    void parseProduct()
    {
        parseUnary();
        for (;;) {
            if (!err.empty()) return;
            skip();
            const char c = s[pos];
            if (c == '*' || c == '/' || c == '%') {
                ++pos;
                parseUnary();
                emit(c == '*' ? kMul : c == '/' ? kDiv : kMod);
            } else if (std::isalnum((unsigned char)c) || c == '.' || c == '(') {
                parsePower();  // 3t, 2sin(t), (a)(b): multiply
                emit(kMul);
            } else {
                return;
            }
        }
    }

    void parseUnary()
    {
        skip();
        if (s[pos] == '-') {
            ++pos;
            parseUnary();
            emit(kNeg);
        } else if (s[pos] == '+') {
            ++pos;
            parseUnary();
        } else {
            parsePower();
        }
    }

    void parsePower()
    {
        parsePrimary();
        skip();
        if (s[pos] == '^' || (s[pos] == '*' && s[pos + 1] == '*')) {
            pos += s[pos] == '^' ? 1 : 2;
            parseUnary();  // right to left: 2^3^2 = 2^9
            emit(kPow);
        }
    }

    void parsePrimary()
    {
        skip();
        const char c = s[pos];
        if (!err.empty()) return;
        if (std::isdigit((unsigned char)c) || c == '.') {
            char* end = nullptr;
            const double v = std::strtod(s + pos, &end);
            if (end == s + pos) { fail("bad number"); return; }
            pos = end - s;
            emit(kConst, 0, v);
            return;
        }
        if (c == '(') {
            ++pos;
            parseSum();
            skip();
            if (s[pos] != ')') { fail("missing )"); return; }
            ++pos;
            return;
        }
        if (!std::isalpha((unsigned char)c)) {
            fail(c ? std::string("unexpected '") + c + "'" : "ends too soon");
            return;
        }
        std::string id;
        while (std::isalnum((unsigned char)s[pos]) || s[pos] == '_')
            id += (char)std::tolower((unsigned char)s[pos++]);
        // a function name runs into its argument without a bracket? (sint) - check names first
        for (int k = 0; names1()[k]; ++k)
            if (id == names1()[k]) { call(1, k); return; }
        for (int k = 0; names2()[k]; ++k)
            if (id == names2()[k]) { call(2, k); return; }
        if (id == "t" || id == "theta") emit(kT);
        else if (id == "u") { emit(kU); usesTime = true; }
        else if (id == "pi") emit(kConst, 0, 3.14159265358979323846);
        else if (id == "e") emit(kConst, 0, 2.71828182845904523536);
        else fail("unknown '" + id + "'");
    }

    void call(int args, int k)
    {
        skip();
        if (s[pos] != '(') { fail("needs ( after the function name"); return; }
        ++pos;
        parseSum();
        for (int a = 1; a < args; ++a) {
            skip();
            if (s[pos] != ',') { fail("needs 2 numbers, like max(a, b)"); return; }
            ++pos;
            parseSum();
        }
        skip();
        if (s[pos] != ')') { fail("missing )"); return; }
        ++pos;
        if (args == 1) emit(kFn1, (uint8_t)k);
        else emit(kFn2, (uint8_t)k);
    }
};

} // namespace ss
