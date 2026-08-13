#pragma once
// =============================================================================
// CommonUtils.h  —  shared utilities used across every MathEngine math module
//
// Every module that previously redefined these locally now does:
//
//   #include "CommonUtils.h"
//
// This file consolidates:
//   • Common type aliases   (DVec, DMat, IVec, IMat, Func1, Func2)
//   • JSON dispatch helpers (cu_getStr, cu_getNum, cu_getInt, cu_getParam)
//   • Vector / matrix I/O  (cu_parseVecD, cu_parseVecI, cu_parseMat,
//                            cu_parseMatI)
//   • Formatting            (cu_fmt)
//   • Math primitives       (cu_evalAt, cu_str)
//
// All names are prefixed "cu_" so they coexist cleanly with any existing
// module-local wrappers during migration.  Modules may add local aliases:
//
//   static auto& getP    = cu_getStr;
//   static auto  fmt     = [](double v, int p=8){ return cu_fmt(v,p); };
//
// =============================================================================
#ifndef COMMONUTILS_HPP
#define COMMONUTILS_HPP

#include <string>
#include <vector>
#include <functional>
#include <sstream>
#include <iomanip>
#include <cmath>
#include <algorithm>
#include <numeric>
#include <stdexcept>

// Pull in the shared math primitives (mcGcd, mcNormcdf, mcParseVecD, etc.)
#include "MathCore.hpp"

// =============================================================================
// 1. COMMON TYPE ALIASES
// =============================================================================

// Double-precision containers  (replaces local  using Vec = std::vector<double>
//                                               using Mat = std::vector<Vec>    )
using DVec = std::vector<double>;
using DMat = std::vector<DVec>;

// Integer containers  (replaces local  using Vec = std::vector<long long>
//                                      using Mat = std::vector<std::vector<long long>> )
using IVec = std::vector<long long>;
using IMat = std::vector<IVec>;

using SVec = std::vector<std::string>;

// Single-argument math function  (replaces per-module  using Func = std::function<double(double)> )
using Func1 = std::function<double(double)>;

// Two-argument math function   (used in DiffEq, NumericalAnalysis: f(x, y))
using Func2 = std::function<double(double, double)>;

// =============================================================================
// 2. JSON DISPATCH HELPERS
// =============================================================================
// These replace the identical static getP / getN / getNum / getInt / getParam
// bodies that were copy-pasted into AbstractAlgebra, ComplexAnalysis, DiscreteMath,
// Geometry, NumberTheory, NumericalAnalysis, ProbabilityTheory, DifferentialEquations,
// AppliedMath, and Statistics.
//
// They are thin wrappers around the mc* functions in MathCore.h, given
// shorter names so existing call sites require minimal edits.

// String value for key k  (returns def if key absent)
inline std::string cu_getStr(const std::string &json,
                             const std::string &key,
                             const std::string &def = "")
{
    return mcGetStr(json, key, def);
}

// Double value for key k  (returns def if absent or non-numeric)
inline double cu_getNum(const std::string &json,
                        const std::string &key,
                        double def = 0.0)
{
    return mcGetNum(json, key, def);
}

// Integer (long long) value for key k
inline long long cu_getInt(const std::string &json,
                           const std::string &key,
                           long long def = 0)
{
    return mcGetInt(json, key, def);
}

// Alias kept for backward compat with DifferentialEquations / AppliedMath / Statistics
// which used getParam(json, key) instead of getP(json, key)
inline std::string cu_getParam(const std::string &json,
                               const std::string &key,
                               const std::string &def = "")
{
    return mcGetStr(json, key, def);
}

// =============================================================================
// 3. VECTOR / MATRIX PARSERS
// =============================================================================
// Replaces per-module parseVec, parseMat (double) and parseVec / parseMat
// (long long) that were copy-pasted across 7–8 files.

// Parse "[1.0, 2.5, 3.14]" → DVec
inline DVec cu_parseVecD(const std::string &s)
{
    return mcParseVecD(s);
}

inline SVec cu_parseVecS(const std::string &s)
{
    return mcParseVecS(s);
}

// Parse "[1, 2, 3]" → IVec  (integer elements)
inline IVec cu_parseVecI(const std::string &s)
{
    return mcParseVecI(s);
}

// Parse "[[1,2],[3,4]]" → DMat
inline DMat cu_parseMat(const std::string &s)
{
    return mcParseMat(s);
}

// Parse "[[1,2],[3,4]]" → IMat  (long long elements)
inline IMat cu_parseMatI(const std::string &s)
{
    IMat m;
    // Same outer-bracket-consumption bug as mcParseMat (see MathCore.hpp)
    // was independently duplicated here - strip the matrix's own outer
    // brackets first so the row-finding loop's first find('[', pos) lands
    // on row 0's own bracket instead of the matrix's.
    std::string t = s;
    if (!t.empty() && t.front() == '[' && t.back() == ']')
        t = t.substr(1, t.size() - 2);
    size_t pos = 0;
    while ((pos = t.find('[', pos)) != std::string::npos)
    {
        ++pos;
        size_t end = t.find(']', pos);
        if (end == std::string::npos)
            break;
        auto row = cu_parseVecI("[" + t.substr(pos, end - pos) + "]");
        if (!row.empty())
            m.push_back(row);
        pos = end + 1;
    }
    return m;
}

// Parse "[[re1,im1],[re2,im2],...]" -> vector<pair<double,double>>. Feature:
// shared, tested replacement for the kind of one-off bracket-scanning loop
// ComplexAnalysis's residue_theorem used to hand-roll for its pole list
// (and got the same outer-bracket bug wrong in the process - see CA.cpp).
// Built on cu_parseMat rather than its own loop, so it can't independently
// drift from the one proven implementation.
inline std::vector<std::pair<double,double>> cu_parsePairs(const std::string &s)
{
    std::vector<std::pair<double,double>> out;
    for (const auto &row : cu_parseMat(s))
        if (row.size() >= 2)
            out.push_back({row[0], row[1]});
    return out;
}

// ── Dimension validation ────────────────────────────────────────────────
// Feature: shared pre-flight checks for matrix/vector shape, meant to be
// called at the top of a dispatch() branch, before any real computation.
// The ragged-matrix bug (see mcParseMat above) got as far as an
// out-of-bounds read specifically because nothing upstream rejected a
// malformed shape early - these turn "silently wrong answer or UB" into
// a clear, catchable error instead. Throw std::invalid_argument rather
// than returning a sentinel so a forgotten check fails loudly in testing
// instead of silently passing bad data through; every module's dispatch()
// already runs under a top-level try/catch (see CoreEngine.cpp), so this
// degrades to a clean error reply, not a crash.

inline bool cu_isRectangular(const DMat &m)
{
    if (m.empty())
        return true;
    size_t cols = m[0].size();
    for (const auto &row : m)
        if (row.size() != cols)
            return false;
    return true;
}

inline void cu_requireRectangular(const DMat &m, const std::string &context)
{
    if (!cu_isRectangular(m))
        throw std::invalid_argument(context + ": matrix rows have inconsistent lengths (ragged/malformed input)");
}

inline void cu_requireSquare(const DMat &m, const std::string &context)
{
    cu_requireRectangular(m, context);
    if (m.empty() || m.size() != m[0].size())
        throw std::invalid_argument(context + ": expected a square matrix, got "
            + std::to_string(m.size()) + "x" + std::to_string(m.empty() ? 0 : m[0].size()));
}

// A x B is only defined when A's column count matches B's row count.
inline void cu_requireMultipliable(const DMat &A, const DMat &B, const std::string &context)
{
    cu_requireRectangular(A, context);
    cu_requireRectangular(B, context);
    size_t aCols = A.empty() ? 0 : A[0].size();
    size_t bRows = B.size();
    if (aCols != bRows)
        throw std::invalid_argument(context + ": cannot multiply a " + std::to_string(A.size()) + "x"
            + std::to_string(aCols) + " matrix by a " + std::to_string(bRows) + "x"
            + std::to_string(B.empty() ? 0 : B[0].size()) + " matrix (inner dimensions must match)");
}

// A and B must have identical shape (elementwise add/subtract).
inline void cu_requireSameShape(const DMat &A, const DMat &B, const std::string &context)
{
    cu_requireRectangular(A, context);
    cu_requireRectangular(B, context);
    size_t aCols = A.empty() ? 0 : A[0].size();
    size_t bCols = B.empty() ? 0 : B[0].size();
    if (A.size() != B.size() || aCols != bCols)
        throw std::invalid_argument(context + ": matrix shapes differ (" + std::to_string(A.size()) + "x"
            + std::to_string(aCols) + " vs " + std::to_string(B.size()) + "x" + std::to_string(bCols) + ")");
}

// A (n x n) matrix acting on a vector needs a vector of length n.
inline void cu_requireCompatibleVec(const DMat &A, const DVec &v, const std::string &context)
{
    cu_requireRectangular(A, context);
    size_t aCols = A.empty() ? 0 : A[0].size();
    if (aCols != v.size())
        throw std::invalid_argument(context + ": matrix has " + std::to_string(aCols)
            + " columns but vector has " + std::to_string(v.size()) + " entries");
}

// =============================================================================
// 4. FORMATTING
// =============================================================================
// Replaces the 7 distinct fmt(double, int) bodies spread across modules.
// Canonical behaviour:
//   • Infinite values rendered as "∞" / "-∞"
//   • NaN rendered as "NaN"
//   • Otherwise: std::setprecision(p) via ostringstream

inline std::string cu_fmt(double v, int p = 8)
{
    if (!std::isfinite(v))
    {
        if (std::isinf(v))
            return v > 0 ? "∞" : "-∞";
        return "NaN";
    }
    std::ostringstream ss;
    ss << std::setprecision(p) << v;
    return ss.str();
}

// Long-long overload so modules that did  str(v) = std::to_string(v)  can use cu_fmt too
inline std::string cu_fmt(long long v) { return std::to_string(v); }
inline std::string cu_fmt(int v) { return std::to_string(v); }

// Short-name alias used by modules that called str(v) — identical to cu_fmt(long long)
inline std::string cu_str(long long v) { return std::to_string(v); }

// =============================================================================
// 5. SYMBOLIC EVALUATION HELPER
// =============================================================================
// Replaces evalAt(expr, var, val) defined independently in DifferentialEquations,
// DifferentialEquations_LibreTexts, Geometry, and AppliedMath.
//
// Requires Calculus.h to be available in the include path.  Each module that
// previously defined evalAt locally should #include "CommonUtils.h" and drop
// its local copy.

#include "Calculus/Calculus.hpp"

// Evaluate a symbolic expression string at a single variable assignment.
// Returns 0.0 on parse/eval error.
inline double cu_evalAt(const std::string &expr,
                        const std::string &var,
                        double val)
{
    try
    {
        auto e = Calculus::parse(expr);
        return Calculus::evaluate(e, {{var, val}});
    }
    catch (...)
    {
        return 0.0;
    }
}

// Two-variable overload  (used in DE/Applied: f(t, y))
inline double cu_evalAt2(const std::string &expr,
                         const std::string &var1, double val1,
                         const std::string &var2, double val2)
{
    try
    {
        auto e = Calculus::parse(expr);
        return Calculus::evaluate(e, {{var1, val1}, {var2, val2}});
    }
    catch (...)
    {
        return 0.0;
    }
}

// Numerical first derivative of expr w.r.t. var at val (central differences)
inline double cu_diff1(const std::string &expr,
                       const std::string &var,
                       double val,
                       double h = 1e-5)
{
    try
    {
        auto e = Calculus::parse(expr);
        auto d = Calculus::simplify(Calculus::diff(e, var));
        return Calculus::evaluate(d, {{var, val}});
    }
    catch (...)
    {
        // Fallback: central finite difference
        return (cu_evalAt(expr, var, val + h) - cu_evalAt(expr, var, val - h)) / (2.0 * h);
    }
}

// =============================================================================
// 6. CONVENIENCE ALIASES FOR EASY MIGRATION
// =============================================================================
// Modules that already compile with local names (getP, getN, fmt, parseVec, etc.)
// can add these aliases at the top of their .cpp after including CommonUtils.h,
// instead of renaming every call site:
//
//   #include "CommonUtils.h"
//   static auto getP     = cu_getStr;
//   static auto getN     = cu_getNum;
//   static auto fmt      = cu_fmt;
//   static auto parseVec = cu_parseVecD;
//   static auto parseMat = cu_parseMat;
//   static auto evalAt   = cu_evalAt;
//   static auto str      = cu_str;
//
// (Do NOT do this if the module already has a local definition — it will conflict.)

#endif
