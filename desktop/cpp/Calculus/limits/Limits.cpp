// =============================================================================
// calculus/limits/Limits.cpp
// =============================================================================

#include "Limits.hpp"
#include "../core/Week4Bridge.hpp"
#include <cmath>
#include <stdexcept>
#include <sstream>
#include <iomanip>
#include <limits>
#include <algorithm>

namespace Calculus {

static constexpr int    MAX_LHOPITAL  = 5;
static constexpr double CONVERGENCE   = 1e-8;   // two-sided agreement threshold

// =============================================================================
// parsePoint — convert a string like "pi", "-inf", "1.5" to a double
// =============================================================================

double parsePoint(const std::string& s) {
    std::string t = s;
    // trim
    while (!t.empty() && std::isspace(static_cast<unsigned char>(t.front()))) t.erase(t.begin());
    while (!t.empty() && std::isspace(static_cast<unsigned char>(t.back())))  t.pop_back();

    if (t == "inf"  || t == "+inf" || t == "Inf")  return  std::numeric_limits<double>::infinity();
    if (t == "-inf" || t == "-Inf")                 return -std::numeric_limits<double>::infinity();
    if (t == "pi"   || t == "PI")                   return  M_PI;
    if (t == "-pi"  || t == "-PI")                  return -M_PI;
    if (t == "pi/2")                                return  M_PI / 2.0;
    if (t == "-pi/2")                               return -M_PI / 2.0;
    if (t == "e"    || t == "E")                    return  M_E;

    // Try parsing as a plain number
    try { return std::stod(t); }
    catch (...) {}

    // Try parsing as a simple fraction like "1/2"
    auto slash = t.find('/');
    if (slash != std::string::npos) {
        try {
            double num = std::stod(t.substr(0, slash));
            double den = std::stod(t.substr(slash + 1));
            if (std::abs(den) < 1e-15)
                throw std::invalid_argument("Division by zero in point: " + s);
            return num / den;
        } catch (...) {}
    }

    throw std::invalid_argument("Cannot parse limit point: '" + s + "'");
}

// =============================================================================
// Classify the result of direct substitution
// =============================================================================

enum class FormType { FINITE, ZERO_OVER_ZERO, INF_OVER_INF,
                      INF, NEG_INF, NAN_FORM };

static FormType classify(double val) {
    if (std::isnan(val))              return FormType::NAN_FORM;
    if (!std::isfinite(val)) {
        return val > 0 ? FormType::INF : FormType::NEG_INF;
    }
    return FormType::FINITE;
}

// Evaluate safely — returns NaN on any exception
static double safeEval(const ExprPtr& expr,
                       const std::string& var,
                       double x) {
    try {
        return evaluate(expr, {{var, x}});
    } catch (...) {
        return std::numeric_limits<double>::quiet_NaN();
    }
}

// =============================================================================
// tryDirectSubstitution
// =============================================================================

std::optional<double> tryDirectSubstitution(const ExprPtr& expr,
                                            const std::string& var,
                                            double point) {
    if (!std::isfinite(point)) return std::nullopt;   // handled separately

    double val = safeEval(expr, var, point);
    FormType f = classify(val);

    if (f == FormType::FINITE) return val;

    // If we get 0/0 (NaN) or ∞/∞, signal indeterminate
    return std::nullopt;
}

// =============================================================================
// tryLHopital
//
// Applies L'Hôpital's rule to the expression at the given point.
// Requires the expression to be (or reduce to) a ratio f/g.
// Checks the indeterminate form 0/0 or ∞/∞, then returns lim f'/g'.
// Repeats up to MAX_LHOPITAL times.
// =============================================================================

std::optional<double> tryLHopital(const ExprPtr& expr,
                                   const std::string& var,
                                   double point) {
    // Only works on DIV nodes
    if (expr->type != ExprType::DIV) return std::nullopt;
    if (!std::isfinite(point))       return std::nullopt;

    ExprPtr num = expr->left();
    ExprPtr den = expr->right();

    for (int i = 0; i < MAX_LHOPITAL; ++i) {
        double nVal = safeEval(num, var, point);
        double dVal = safeEval(den, var, point);

        bool numZero = std::abs(nVal) < 1e-9 || std::isnan(nVal);
        bool denZero = std::abs(dVal) < 1e-9 || std::isnan(dVal);
        bool numInf  = !std::isfinite(nVal);
        bool denInf  = !std::isfinite(dVal);

        bool zeroOverZero = numZero && denZero;
        bool infOverInf   = numInf  && denInf;

        if (!zeroOverZero && !infOverInf) {
            // Not indeterminate — check if we can just evaluate
            if (std::isfinite(dVal) && std::abs(dVal) > 1e-12)
                return nVal / dVal;
            return std::nullopt;
        }

        // Differentiate numerator and denominator
        try {
            num = simplify(diff(num, var));
            den = simplify(diff(den, var));
        } catch (...) {
            return std::nullopt;
        }
    }

    // After MAX_LHOPITAL applications, try a final evaluation
    double nFinal = safeEval(num, var, point);
    double dFinal = safeEval(den, var, point);
    if (std::isfinite(nFinal) && std::isfinite(dFinal) &&
        std::abs(dFinal) > 1e-12)
        return nFinal / dFinal;

    return std::nullopt;
}

// =============================================================================
// numericalLimit — approach from one side
//
// Evaluates at a sequence of points approaching `point` from the given side.
// side = +1: approach from the right (x → point⁺)
// side = -1: approach from the left  (x → point⁻)
//
// Uses geometric progression of step sizes: h, h/10, h/100, ...
// Returns the limit value if the sequence converges, else NaN.
// =============================================================================

double numericalLimit(const ExprPtr& expr,
                      const std::string& var,
                      double point,
                      int side) {
    double h = 1e-3;
    double prev = std::numeric_limits<double>::quiet_NaN();

    for (int i = 0; i < 10; ++i) {
        double x   = point + side * h;
        double val = safeEval(expr, var, x);
        if (!std::isfinite(val)) { h /= 10.0; continue; }

        if (i > 0 && std::isfinite(prev) &&
            std::abs(val - prev) < CONVERGENCE * (1.0 + std::abs(val)))
            return val;

        prev = val;
        h   /= 10.0;
    }
    return prev;
}

// =============================================================================
// numericalLimitInf
// =============================================================================

double numericalLimitInf(const ExprPtr& expr,
                         const std::string& var,
                         int direction) {
    // Evaluate at geometrically increasing magnitudes
    double prev = std::numeric_limits<double>::quiet_NaN();
    double x    = 1e3;

    for (int i = 0; i < 8; ++i) {
        double val = safeEval(expr, var, direction * x);
        if (std::isfinite(val) && std::isfinite(prev) &&
            std::abs(val - prev) < CONVERGENCE * (1.0 + std::abs(val)))
            return val;
        if (std::isfinite(val)) prev = val;
        x *= 100.0;
    }
    return prev;
}

// =============================================================================
// limitLeft / limitRight
// =============================================================================

double limitLeft(const ExprPtr& expr, const std::string& var, double point) {
    // Try direct substitution first
    if (const auto ds = tryDirectSubstitution(expr, var, point)) return *ds;
    // Numerical from left
    return numericalLimit(expr, var, point, -1);
}

double limitRight(const ExprPtr& expr, const std::string& var, double point) {
    if (auto ds = tryDirectSubstitution(expr, var, point)) return *ds;
    return numericalLimit(expr, var, point, +1);
}

// =============================================================================
// limitInf / limitNegInf
// =============================================================================

double limitInf(const ExprPtr& expr, const std::string& var) {
    return numericalLimitInf(expr, var, +1);
}

double limitNegInf(const ExprPtr& expr, const std::string& var) {
    return numericalLimitInf(expr, var, -1);
}

// =============================================================================
// limitAt — the main two-sided limit
//
// Strategy order:
//   1. Direct substitution
//   2. L'Hôpital (if expression is a ratio)
//   3. Numerical (both sides, check agreement)
// =============================================================================

double limitAt(const ExprPtr& expr, const std::string& var, double point) {

    // ── Infinite limits ───────────────────────────────────────────────────────
    if (!std::isfinite(point)) {
        if (point > 0) return limitInf   (expr, var);
        else           return limitNegInf(expr, var);
    }

    // ── Strategy 1: direct substitution ──────────────────────────────────────
    if (const auto ds = tryDirectSubstitution(expr, var, point)) return *ds;

    // ── Strategy 2: L'Hôpital ─────────────────────────────────────────────────
    if (const auto lh = tryLHopital(expr, var, point)) return *lh;

    // ── Strategy 3: numerical from both sides ─────────────────────────────────
    double left  = numericalLimit(expr, var, point, -1);
    double right = numericalLimit(expr, var, point, +1);

    if (!std::isfinite(left) && !std::isfinite(right)) {
        // Both sides diverge — limit is ±∞ or does not exist
        if (left == right) return left;   // both +∞ or both -∞
        return std::numeric_limits<double>::quiet_NaN();
    }

    if (!std::isfinite(left))  return right;
    if (!std::isfinite(right)) return left;

    // Check that both sides agree
    const double avg = 0.5 * (left + right);
    if (const double tol = CONVERGENCE * (1.0 + std::abs(avg)); std::abs(left - right) > tol) {
        // Left and right limits disagree — limit does not exist
        return std::numeric_limits<double>::quiet_NaN();
    }

    return avg;
}

// =============================================================================
// formatValue — turn a double limit result into a readable string
// =============================================================================

static std::string formatValue(const double val) {
    if (std::isnan(val))              return "DNE";   // does not exist
    if (val ==  std::numeric_limits<double>::infinity()) return "inf";
    if (val == -std::numeric_limits<double>::infinity()) return "-inf";

    // Try to recognize common exact values
    if (std::abs(val)       < 1e-10) return "0";
    if (std::abs(val - 1.0) < 1e-8) return "1";
    if (std::abs(val + 1.0) < 1e-8) return "-1";
    if (std::abs(val - M_PI)         < 1e-6) return "pi";
    if (std::abs(val - M_PI/2.0)     < 1e-6) return "pi/2";
    if (std::abs(val - M_E)          < 1e-6) return "e";
    if (std::abs(val - std::sqrt(2.0)) < 1e-6) return "sqrt(2)";
    if (std::abs(val - 0.5)          < 1e-8) return "1/2";

    std::ostringstream ss;
    ss << std::setprecision(10) << val;
    return ss.str();
}

static std::string formatLatexValue(double val) {
    if (std::isnan(val))              return "\\text{DNE}";
    if (val ==  std::numeric_limits<double>::infinity()) return "\\infty";
    if (val == -std::numeric_limits<double>::infinity()) return "-\\infty";
    if (std::abs(val)       < 1e-10) return "0";
    if (std::abs(val - 1.0) < 1e-8) return "1";
    if (std::abs(val + 1.0) < 1e-8) return "-1";
    if (std::abs(val - M_PI)         < 1e-6) return "\\pi";
    if (std::abs(val - M_PI/2.0)     < 1e-6) return "\\frac{\\pi}{2}";
    if (std::abs(val - M_E)          < 1e-6) return "e";
    if (std::abs(val - std::sqrt(2.0)) < 1e-6) return "\\sqrt{2}";
    if (std::abs(val - 0.5)          < 1e-8) return "\\frac{1}{2}";
    std::ostringstream ss;
    ss << std::setprecision(10) << val;
    return ss.str();
}

// =============================================================================
// computeLimit — main public entry point
// =============================================================================

LimitResult computeLimit(const std::string& exprStr,
                         const std::string& var,
                         const std::string& pointStr,
                         LimitDirection direction) {
    LimitResult out;
    try {
        ExprPtr expr  = parse(exprStr);
        double  point = parsePoint(pointStr);

        double val;
        std::string method;

        switch (direction) {

        case LimitDirection::LEFT: {
            if (auto ds = tryDirectSubstitution(expr, var, point)) { val = *ds; method = "direct substitution"; break; }
            val    = limitLeft(expr, var, point);
            method = "numerical (left approach)";
            break;
        }

        case LimitDirection::RIGHT: {
            if (auto ds = tryDirectSubstitution(expr, var, point)) { val = *ds; method = "direct substitution"; break; }
            val    = limitRight(expr, var, point);
            method = "numerical (right approach)";
            break;
        }

        case LimitDirection::POS_INF:
            val    = limitInf(expr, var);
            method = "numerical (x → +∞)";
            break;

        case LimitDirection::NEG_INF:
            val    = limitNegInf(expr, var);
            method = "numerical (x → -∞)";
            break;

        case LimitDirection::BOTH:
        default: {
            // Try direct substitution
            if (auto ds = tryDirectSubstitution(expr, var, point)) {
                val    = *ds;
                method = "direct substitution";
                break;
            }
            // Try L'Hôpital
            if (auto lh = tryLHopital(expr, var, point)) {
                val    = *lh;
                method = "L'Hôpital's rule";
                break;
            }
            // Numerical fallback — check both sides
            double left  = numericalLimit(expr, var, point, -1);
            double right = numericalLimit(expr, var, point, +1);

            if (std::isfinite(left) && std::isfinite(right)) {
                double tol = CONVERGENCE * (1.0 + 0.5 * std::abs(left + right));
                if (std::abs(left - right) > tol) {
                    out.exists  = false;
                    out.value   = "DNE (left = " + formatValue(left) +
                                  ", right = "   + formatValue(right) + ")";
                    out.latex   = "\\text{DNE}";
                    out.method  = "numerical (sides disagree)";
                    return out;
                }
                val    = 0.5 * (left + right);
                method = "numerical convergence";
            } else {
                val    = std::isfinite(left) ? left : right;
                method = "numerical (one-sided)";
            }
            break;
        }
        }

        out.value  = formatValue(val);
        out.latex  = formatLatexValue(val);
        out.method = method;

        if (std::isnan(val)) {
            out.exists = false;
            out.value  = "DNE";
            out.latex  = "\\text{DNE}";
        }

    } catch (const std::exception& ex) {
        out.ok    = false;
        out.error = ex.what();
    }
    return out;
}

} // namespace Calculus