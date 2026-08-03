#pragma once
// =============================================================================
// MathCore.h  —  shared foundation for all MathEngine modules
//
// Every module does:  #include "../MathCore.h"   (from modules/ subdirectory)
//                     #include "MathCore.h"       (from src/main/cpp/)
//
// Provides:
//   • Numeric aliases   : Real, Int, Complex, RVec, RMat, IVec, CVec, CMat
//   • Universal result  : MathResult  +  mathOk() / mathErr()
//   • JSON parser       : JsonArgs
//   • Formatting        : fmtReal, fmtVec, fmtMat, fmtComplex
//   • Constants         : MathConst::PI, E, SQRT2, INF, EPS
//   • LA primitives     : matMulR, transposeR, identityR, dotR, normR, gaussianElim
// =============================================================================

#ifndef MATHCORE_HPP
#define MATHCORE_HPP

#include <string>
#include <vector>
#include <complex>
#include <sstream>
#include <iomanip>
#include <cmath>
#include <functional>
#include <map>
#include <stdexcept>
#include <algorithm>
#include <numeric>
#include <limits>

// When compiling with GCC/Clang (Linux/macOS builds), __SIZEOF_INT128__
// is defined and we get the fast single-expression path:
#ifdef __SIZEOF_INT128__
#define MC_MULMOD(a, b, m) (static_cast<__int128>(a) * (b) % (m))
// When compiling with MSVC (Windows, IntelliSense), we fall back to a
// binary-method helper.  It's O(log b) but correct for any m < 2^62:
#else
inline long long mc_mulmod(long long a, long long b, long long m)
{
    long long r = 0;
    a %= m;
    for (; b > 0; b >>= 1)
    {
        if (b & 1)
            r = (r + a) % m;
        a = (a + a) % m;
    }
    return r;
}
#define MC_MULMOD(a, b, m) mc_mulmod(a, b, m)
#endif

// ─────────────────────────────────────────────────────────────────────────────
// Numeric type aliases
// ─────────────────────────────────────────────────────────────────────────────

using Real = double;
using Int = long long;
using Complex = std::complex<double>;

using RVec = std::vector<Real>;
using RMat = std::vector<RVec>;

using IVec = std::vector<Int>;
using IMat = std::vector<IVec>;

using CVec = std::vector<Complex>;
using CMat = std::vector<CVec>;

using RealFn = std::function<Real(Real)>;
using RealFn2 = std::function<Real(Real, Real)>;

// ─────────────────────────────────────────────────────────────────────────────
// Universal result type
// Field names match the pre-existing module result structs so every module
// can typedef or alias MathResult under its own name with zero code change.
// ─────────────────────────────────────────────────────────────────────────────

struct MathResult
{
    bool ok = true;
    std::string value; // primary answer
    std::string steps; // working / extended detail  (alias: detail)
    std::string error;

    // Compatibility alias so modules that use .detail still compile
    const std::string &detail() const { return steps; }

    std::string format(bool showSteps = true) const
    {
        if (!ok)
            return "ERROR: " + error;
        if (showSteps && !steps.empty() && steps != value)
            return value + "\n\n" + steps;
        return value;
    }
};

inline MathResult mathOk(const std::string &val, const std::string &steps = "")
{
    return {true, val, steps, ""};
}
inline MathResult mathErr(const std::string &msg)
{
    return {false, "", "", msg};
}

// ─────────────────────────────────────────────────────────────────────────────
// JSON field extractor — zero external dependencies
// ─────────────────────────────────────────────────────────────────────────────

struct JsonArgs
{
    const std::string &j;
    explicit JsonArgs(const std::string &json) : j(json) {}

    // Find the raw value string for a key (handles quoted and unquoted values)
    std::string raw(const std::string &key, const std::string &def = "") const
    {
        auto pos = j.find("\"" + key + "\"");
        if (pos == std::string::npos)
            return def;
        pos = j.find(':', pos);
        if (pos == std::string::npos)
            return def;
        ++pos;
        while (pos < j.size() && std::isspace((unsigned char)j[pos]))
            ++pos;
        if (pos >= j.size())
            return def;
        if (j[pos] == '"')
        {
            ++pos;
            auto e = j.find('"', pos);
            return e == std::string::npos ? def : j.substr(pos, e - pos);
        }
        if (j[pos] == '[' || j[pos] == '{')
            return def; // array/object — use rvec/rmat
        auto e = pos;
        while (e < j.size() && j[e] != ',' && j[e] != '}' && j[e] != ']')
            ++e;
        auto v = j.substr(pos, e - pos);
        while (!v.empty() && std::isspace((unsigned char)v.back()))
            v.pop_back();
        return v;
    }

    std::string str(const std::string &key, const std::string &def = "") const
    {
        return raw(key, def);
    }
    double num(const std::string &key, double def = 0.0) const
    {
        auto v = raw(key);
        if (v.empty())
            return def;
        try
        {
            return std::stod(v);
        }
        catch (...)
        {
            return def;
        }
    }
    Int i64(const std::string &key, Int def = 0) const
    {
        auto v = raw(key);
        if (v.empty())
            return def;
        try
        {
            return std::stoll(v);
        }
        catch (...)
        {
            return def;
        }
    }
    int i32(const std::string &key, int def = 0) const
    {
        return static_cast<int>(i64(key, def));
    }
    bool flag(const std::string &key, bool def = false) const
    {
        auto v = raw(key);
        if (v.empty())
            return def;
        return v == "true" || v == "1";
    }

    // Array of doubles: "key": [1.0, 2.5, 3.0]
    RVec rvec(const std::string &key) const
    {
        RVec result;
        auto pos = j.find("\"" + key + "\"");
        if (pos == std::string::npos)
            return result;
        pos = j.find('[', pos);
        if (pos == std::string::npos)
            return result;
        ++pos;
        auto end = j.find(']', pos);
        std::istringstream ss(j.substr(pos, end - pos));
        std::string tok;
        while (std::getline(ss, tok, ','))
        {
            while (!tok.empty() && std::isspace((unsigned char)tok.front()))
                tok.erase(tok.begin());
            while (!tok.empty() && std::isspace((unsigned char)tok.back()))
                tok.pop_back();
            if (!tok.empty())
                try
                {
                    result.push_back(std::stod(tok));
                }
                catch (...)
                {
                }
        }
        return result;
    }

    // Array of integers
    IVec ivec(const std::string &key) const
    {
        IVec result;
        auto pos = j.find("\"" + key + "\"");
        if (pos == std::string::npos)
            return result;
        pos = j.find('[', pos);
        if (pos == std::string::npos)
            return result;
        ++pos;
        auto end = j.find(']', pos);
        std::istringstream ss(j.substr(pos, end - pos));
        std::string tok;
        while (std::getline(ss, tok, ','))
        {
            while (!tok.empty() && std::isspace((unsigned char)tok.front()))
                tok.erase(tok.begin());
            while (!tok.empty() && std::isspace((unsigned char)tok.back()))
                tok.pop_back();
            if (!tok.empty())
                try
                {
                    result.push_back(std::stoll(tok));
                }
                catch (...)
                {
                }
        }
        return result;
    }

    // Array of strings: "key": ["a","b","c"]
    std::vector<std::string> svec(const std::string &key) const
    {
        std::vector<std::string> result;
        auto pos = j.find("\"" + key + "\"");
        if (pos == std::string::npos)
            return result;
        pos = j.find('[', pos);
        if (pos == std::string::npos)
            return result;
        auto end_bracket = j.find(']', pos);
        while (pos < end_bracket)
        {
            pos = j.find('"', pos);
            if (pos == std::string::npos || pos >= end_bracket)
                break;
            ++pos;
            auto e = j.find('"', pos);
            if (e == std::string::npos)
                break;
            result.push_back(j.substr(pos, e - pos));
            pos = e + 1;
        }
        return result;
    }

    // 2-D array (matrix): "key": [[1,2],[3,4]]
    RMat rmat(const std::string &key) const
    {
        RMat result;
        auto pos = j.find("\"" + key + "\"");
        if (pos == std::string::npos)
            return result;
        pos = j.find('[', pos);
        if (pos == std::string::npos)
            return result;
        ++pos; // skip outer [
        while (pos < j.size())
        {
            pos = j.find('[', pos);
            if (pos == std::string::npos)
                break;
            ++pos;
            auto end = j.find(']', pos);
            RVec row;
            std::istringstream ss(j.substr(pos, end - pos));
            std::string tok;
            while (std::getline(ss, tok, ','))
            {
                while (!tok.empty() && std::isspace((unsigned char)tok.front()))
                    tok.erase(tok.begin());
                while (!tok.empty() && std::isspace((unsigned char)tok.back()))
                    tok.pop_back();
                if (!tok.empty())
                    try
                    {
                        row.push_back(std::stod(tok));
                    }
                    catch (...)
                    {
                    }
            }
            if (!row.empty())
                result.push_back(row);
            pos = end + 1;
        }
        return result;
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// Mathematical constants
// ─────────────────────────────────────────────────────────────────────────────

namespace MathConst
{
    constexpr double PI = 3.14159265358979323846;
    constexpr double E = 2.71828182845904523536;
    constexpr double SQRT2 = 1.41421356237309504880;
    constexpr double LN2 = 0.69314718055994530941;
    constexpr double GOLDEN = 1.61803398874989484820;
    constexpr double INF = std::numeric_limits<double>::infinity();
    constexpr double EPS = 1e-12;
}

// ─────────────────────────────────────────────────────────────────────────────
// Formatting utilities
// ─────────────────────────────────────────────────────────────────────────────

inline std::string fmtReal(double v, int prec = 8)
{
    if (!std::isfinite(v))
        return std::isinf(v) ? (v > 0 ? "inf" : "-inf") : "NaN";
    std::ostringstream ss;
    ss << std::setprecision(prec) << v;
    return ss.str();
}

inline std::string fmtComplex(Complex z, int prec = 6)
{
    std::ostringstream ss;
    ss << std::setprecision(prec) << z.real();
    if (z.imag() >= 0)
        ss << " + " << z.imag() << "i";
    else
        ss << " - " << -z.imag() << "i";
    return ss.str();
}

inline std::string fmtVec(const RVec &v, int prec = 6)
{
    std::ostringstream ss;
    ss << "[";
    for (size_t i = 0; i < v.size(); ++i)
    {
        if (i)
            ss << ", ";
        ss << std::setprecision(prec) << v[i];
    }
    ss << "]";
    return ss.str();
}

inline std::string fmtIVec(const IVec &v)
{
    std::ostringstream ss;
    ss << "[";
    for (size_t i = 0; i < v.size(); ++i)
    {
        if (i)
            ss << ", ";
        ss << v[i];
    }
    ss << "]";
    return ss.str();
}

inline std::string fmtMat(const RMat &M, int prec = 6)
{
    std::ostringstream ss;
    for (const auto &row : M)
    {
        ss << "  [";
        for (size_t j = 0; j < row.size(); ++j)
        {
            if (j)
                ss << ",  ";
            ss << std::setw(prec + 4) << std::setprecision(prec) << row[j];
        }
        ss << "]\n";
    }
    return ss.str();
}

// ─────────────────────────────────────────────────────────────────────────────
// Linear algebra primitives (shared, header-only)
// ─────────────────────────────────────────────────────────────────────────────

inline RMat matMulR(const RMat &A, const RMat &B)
{
    int m = (int)A.size(), k = (int)B.size(), n = k ? (int)B[0].size() : 0;
    RMat C(m, RVec(n, 0.0));
    for (int i = 0; i < m; ++i)
        for (int l = 0; l < k; ++l)
            if (A[i][l] != 0.0)
                for (int j = 0; j < n; ++j)
                    C[i][j] += A[i][l] * B[l][j];
    return C;
}

inline RMat transposeR(const RMat &A)
{
    if (A.empty())
        return {};
    int m = (int)A.size(), n = (int)A[0].size();
    RMat T(n, RVec(m));
    for (int i = 0; i < m; ++i)
        for (int j = 0; j < n; ++j)
            T[j][i] = A[i][j];
    return T;
}

inline RMat identityR(int n)
{
    RMat I(n, RVec(n, 0.0));
    for (int i = 0; i < n; ++i)
        I[i][i] = 1.0;
    return I;
}

inline Real dotR(const RVec &a, const RVec &b)
{
    Real s = 0;
    size_t n = std::min(a.size(), b.size());
    for (size_t i = 0; i < n; ++i)
        s += a[i] * b[i];
    return s;
}

inline Real normR(const RVec &v) { return std::sqrt(dotR(v, v)); }

// Gauss-Jordan elimination in-place → RREF; returns rank
inline int gaussianElim(RMat &A)
{
    int m = (int)A.size(), n = A.empty() ? 0 : (int)A[0].size();
    int row = 0;
    for (int col = 0; col < n && row < m; ++col)
    {
        int pivot = -1;
        for (int r = row; r < m; ++r)
            if (std::abs(A[r][col]) > MathConst::EPS)
            {
                pivot = r;
                break;
            }
        if (pivot == -1)
            continue;
        std::swap(A[row], A[pivot]);
        double sc = A[row][col];
        for (int j = 0; j < n; ++j)
            A[row][j] /= sc;
        for (int r = 0; r < m; ++r)
        {
            if (r == row || std::abs(A[r][col]) < MathConst::EPS)
                continue;
            double f = A[r][col];
            for (int j = 0; j < n; ++j)
                A[r][j] -= f * A[row][j];
        }
        ++row;
    }
    return row;
}

// Solve Ax = b via Gaussian elimination (in-place on augmented matrix [A|b])
// Returns solution vector, or empty if no unique solution
inline RVec solveLinear(RMat A, RVec b)
{
    int n = (int)A.size();
    // Augment
    for (int i = 0; i < n; ++i)
        A[i].push_back(b[i]);
    gaussianElim(A);
    RVec x(n, 0.0);
    for (int i = n - 1; i >= 0; --i)
    {
        if (std::abs(A[i][i]) < MathConst::EPS)
            return {};
        x[i] = A[i][n];
        for (int j = i + 1; j < n; ++j)
            x[i] -= A[i][j] * x[j];
        x[i] /= A[i][i];
    }
    return x;
}

// RMat inversion via Gauss-Jordan
inline RMat invertR(const RMat &A)
{
    int n = (int)A.size();
    RMat aug = A;
    for (int i = 0; i < n; ++i)
    {
        aug[i].resize(2 * n, 0.0);
        aug[i][n + i] = 1.0;
    }
    gaussianElim(aug);
    RMat inv(n, RVec(n));
    for (int i = 0; i < n; ++i)
        for (int j = 0; j < n; ++j)
            inv[i][j] = aug[i][n + j];
    return inv;
}

// =============================================================================
// SHARED DISPATCH UTILITIES  (replaces per-module duplicate helpers)
// =============================================================================

// ── fmt overload (double → string, compatible with fmtD / fmtOL) ─────────────
// fmtReal already exists above; this alias avoids duplicate definitions
inline std::string mcFmt(double v, int p = 8) { return fmtReal(v, p); }

// ── JSON extraction helpers ───────────────────────────────────────────────────
// Every module previously redefined getP / getN.  Use these instead.

inline std::string mcGetStr(const std::string &j, const std::string &k,
                            const std::string &def = "")
{
    auto pos = j.find('"' + k + '"');
    if (pos == std::string::npos)
        return def;
    pos = j.find(':', pos);
    if (pos == std::string::npos)
        return def;
    ++pos;
    while (pos < j.size() && std::isspace(static_cast<unsigned char>(j[pos])))
        ++pos;
    if (pos < j.size() && j[pos] == '"')
    {
        ++pos;
        auto e = j.find('"', pos);
        return e == std::string::npos ? def : j.substr(pos, e - pos);
    }
    // Handle array values [...]
    if (j[pos] == '[')
    {
        int depth = 0;
        auto e = pos;
        while (e < j.size())
        {
            if (j[e] == '[')
                depth++;
            else if (j[e] == ']')
            {
                depth--;
                if (depth == 0)
                {
                    ++e;
                    break;
                }
            }
            ++e;
        }
        return j.substr(pos, e - pos);
    }
    auto e = pos;
    while (e < j.size() && j[e] != ',' && j[e] != '}')
        ++e;
    auto v = j.substr(pos, e - pos);
    while (!v.empty() && std::isspace(static_cast<unsigned char>(v.back())))
        v.pop_back();
    return v.empty() ? def : v;
}

inline double mcGetNum(const std::string &j, const std::string &k, double def = 0.0)
{
    auto v = mcGetStr(j, k);
    if (v.empty())
        return def;
    try
    {
        return std::stod(v);
    }
    catch (...)
    {
        return def;
    }
}

inline long long mcGetInt(const std::string &j, const std::string &k, long long def = 0)
{
    auto v = mcGetStr(j, k);
    if (v.empty())
        return def;
    try
    {
        return std::stoll(v);
    }
    catch (...)
    {
        return def;
    }
}

// ── Vector / matrix parsers ───────────────────────────────────────────────────

inline std::vector<double> mcParseVecD(const std::string &s)
{
    std::vector<double> v;
    std::string t = s;
    if (!t.empty() && t.front() == '[')
        t = t.substr(1, t.size() - 2);
    std::istringstream ss(t);
    std::string tok;
    while (std::getline(ss, tok, ','))
        try
        {
            v.push_back(std::stod(tok));
        }
        catch (...)
        {
        }
    return v;
}

inline std::vector<std::string> mcParseVecS(const std::string &s)
{
    std::vector<std::string> v;
    std::string t = s;
    if (!t.empty() && t.front() == '[')
        t = t.substr(1, t.size() - 2);
    std::istringstream ss(t);
    std::string tok;
    while (std::getline(ss, tok, ','))
        try
        {
            v.push_back(tok);
        }
        catch (...)
        {
        }
    return v;
}

inline std::vector<long long> mcParseVecI(const std::string &s)
{
    std::vector<long long> v;
    std::string t = s;
    if (!t.empty() && t.front() == '[')
        t = t.substr(1, t.size() - 2);
    std::istringstream ss(t);
    std::string tok;
    while (std::getline(ss, tok, ','))
        try
        {
            v.push_back(std::stoll(tok));
        }
        catch (...)
        {
        }
    return v;
}

inline std::vector<std::vector<double>> mcParseMat(const std::string &s)
{
    std::vector<std::vector<double>> m;
    size_t pos = 0;
    while ((pos = s.find('[', pos)) != std::string::npos)
    {
        ++pos;
        size_t end = s.find(']', pos);
        if (end == std::string::npos)
            break;
        auto row = mcParseVecD("[" + s.substr(pos, end - pos) + "]");
        if (!row.empty())
            m.push_back(row);
        pos = end + 1;
    }
    return m;
}

// ── Shared math primitives ────────────────────────────────────────────────────

// GCD (integer) — single canonical implementation
inline long long mcGcd(long long a, long long b)
{
    while (b)
    {
        a %= b;
        std::swap(a, b);
    }
    return std::abs(a);
}
inline long long mcLcm(long long a, long long b)
{
    return std::abs(a / mcGcd(a, b) * b);
}

// Modular exponentiation: base^exp mod m
inline long long mcModPow(long long base, long long exp_, long long m)
{
    if (m == 1)
        return 0;
    long long result = 1;
    base %= m;
    while (exp_ > 0)
    {
        if (exp_ & 1)
            result = MC_MULMOD(result, base, m);
        exp_ >>= 1;
        base = MC_MULMOD(base, base, m);
    }
    return result;
}

// Normal CDF — single canonical implementation (replaces normcdf / Phi)
inline double mcNormcdf(double x)
{
    return 0.5 * std::erfc(-x / std::sqrt(2.0));
}

// Inverse normal CDF (rational approximation)
inline double mcNorminv(double p)
{
    static const double a[] = {-3.969683028665376e+01, 2.209460984245205e+02,
                               -2.759285104469687e+02, 1.383577518672690e+02,
                               -3.066479806614716e+01, 2.506628277459239e+00};
    static const double b[] = {-5.447609879822406e+01, 1.615858368580409e+02,
                               -1.556989798598866e+02, 6.680131188771972e+01,
                               -1.328068155288572e+01};
    static const double c[] = {-7.784894002430293e-03, -3.223964580411365e-01,
                               -2.400758277161838e+00, -2.549732539343734e+00,
                               4.374664141464968e+00, 2.938163982698783e+00};
    static const double d[] = {7.784695709041462e-03, 3.224671290700398e-01,
                               2.445134137142996e+00, 3.754408661907416e+00};
    if (p <= 0)
        return -1e300;
    if (p >= 1)
        return 1e300;
    double q, r;
    if (p < 0.02425)
    {
        q = std::sqrt(-2 * std::log(p));
        return (((((c[0] * q + c[1]) * q + c[2]) * q + c[3]) * q + c[4]) * q + c[5]) /
               ((((d[0] * q + d[1]) * q + d[2]) * q + d[3]) * q + 1);
    }
    if (p < 1 - 0.02425)
    {
        q = p - 0.5;
        r = q * q;
        return (((((a[0] * r + a[1]) * r + a[2]) * r + a[3]) * r + a[4]) * r + a[5]) * q /
               (((((b[0] * r + b[1]) * r + b[2]) * r + b[3]) * r + b[4]) * r + 1);
    }
    q = std::sqrt(-2 * std::log(1 - p));
    return -(((((c[0] * q + c[1]) * q + c[2]) * q + c[3]) * q + c[4]) * q + c[5]) /
           ((((d[0] * q + d[1]) * q + d[2]) * q + d[3]) * q + 1);
}

// Log-gamma (Lanczos)
inline double mcLgamma(double x) { return std::lgamma(x); }

// Combinations C(n,r) avoiding overflow
inline long long mcComb(int n, int r)
{
    if (r < 0 || r > n)
        return 0;
    if (r == 0 || r == n)
        return 1;
    double lc = std::lgamma(n + 1) - std::lgamma(r + 1) - std::lgamma(n - r + 1);
    return static_cast<long long>(std::round(std::exp(lc)));
}

// Factorial (exact up to 20, log for larger)
inline double mcLogFactorial(int n)
{
    double s = 0;
    for (int i = 2; i <= n; ++i)
        s += std::log(i);
    return s;
}

// ── Shared formatting shorthand ───────────────────────────────────────────────
// Thin wrappers so module code can say fmt(v) without local definition
// (Each module still compiles if it has its own fmt; these are the fallbacks.)
#ifndef MC_FMT_DEFINED
#define MC_FMT_DEFINED
namespace mc
{
    inline std::string fmt(double v, int p = 8) { return mcFmt(v, p); }
    inline std::string fmt(long long v) { return std::to_string(v); }
    inline std::string fmt(int v) { return std::to_string(v); }
}
#endif

#endif // MATHCORE_H