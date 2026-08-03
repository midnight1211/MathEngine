// calculus/series/Convergence.cpp

#include "Convergence.hpp"
#include "../limits/Limits.hpp"
#include "../integration/Numerical.hpp"
#include "../core/Week4Bridge.hpp"
#include <cmath>
#include <sstream>

namespace Calculus {

static constexpr double INF = std::numeric_limits<double>::infinity();

// Evaluate a_n at large n values to estimate the limit
static double limitAtInfinity(const ExprPtr& expr, const std::string& var) {
    return numericalLimitInf(expr, var, +1);
}

// ── Divergence test ───────────────────────────────────────────────────────────

TestResult divergenceTest(const std::string& aN, const std::string& var) {
    TestResult r;
    r.test = "Divergence Test";
    try {
        ExprPtr a = parse(aN);
        double  L = limitAtInfinity(a, var);
        if (!std::isfinite(L) || std::abs(L) > 1e-8) {
            r.verdict = ConvergenceVerdict::DIVERGES;
            r.reason  = "lim a_n = " + std::to_string(L) + " ≠ 0";
        } else {
            r.verdict = ConvergenceVerdict::INCONCLUSIVE;
            r.reason  = "lim a_n = 0 (test inconclusive)";
        }
    } catch (const std::exception& e) {
        r.ok = false; r.error = e.what();
    }
    return r;
}

// ── Ratio test ────────────────────────────────────────────────────────────────

TestResult ratioTest(const std::string& aN, const std::string& var) {
    TestResult r;
    r.test = "Ratio Test";
    try {
        ExprPtr a  = parse(aN);
        // a_{n+1}: substitute n → n+1
        ExprPtr a1 = substitute(a, var, parse(var + " + 1"));
        ExprPtr ratio = simplify(div_expr(a1, a));

        double L = limitAtInfinity(ratio, var);
        if (!std::isfinite(L)) L = INF;

        if (L < 1.0 - 1e-9) {
            r.verdict = ConvergenceVerdict::CONVERGES;
            r.reason  = "|a_{n+1}/a_n| → " + std::to_string(L) + " < 1";
        } else if (L > 1.0 + 1e-9) {
            r.verdict = ConvergenceVerdict::DIVERGES;
            r.reason  = "|a_{n+1}/a_n| → " + std::to_string(L) + " > 1";
        } else {
            r.verdict = ConvergenceVerdict::INCONCLUSIVE;
            r.reason  = "|a_{n+1}/a_n| → 1 (test inconclusive)";
        }
    } catch (const std::exception& e) {
        r.ok = false; r.error = e.what();
    }
    return r;
}

// ── Root test ─────────────────────────────────────────────────────────────────

TestResult rootTest(const std::string& aN, const std::string& var) {
    TestResult r;
    r.test = "Root Test";
    try {
        ExprPtr a    = parse(aN);
        // |a_n|^(1/n) = exp(ln|a_n|/n)
        ExprPtr root = exp_expr(div_expr(log_expr(abs_expr(a)), parse(var)));
        double  L    = limitAtInfinity(root, var);

        if (L < 1.0 - 1e-9) {
            r.verdict = ConvergenceVerdict::CONVERGES;
            r.reason  = "lim |a_n|^(1/n) = " + std::to_string(L) + " < 1";
        } else if (L > 1.0 + 1e-9) {
            r.verdict = ConvergenceVerdict::DIVERGES;
            r.reason  = "lim |a_n|^(1/n) = " + std::to_string(L) + " > 1";
        } else {
            r.verdict = ConvergenceVerdict::INCONCLUSIVE;
            r.reason  = "lim |a_n|^(1/n) = 1 (test inconclusive)";
        }
    } catch (const std::exception& e) {
        r.ok = false; r.error = e.what();
    }
    return r;
}

// ── Comparison test ───────────────────────────────────────────────────────────

TestResult comparisonTest(const std::string& aN,
                           const std::string& bN,
                           const std::string& var) {
    TestResult r;
    r.test = "Comparison Test";
    try {
        ExprPtr a = parse(aN), b = parse(bN);
        // Evaluate ratio a_n / b_n as n→∞
        double ra = 0.0, rb = 0.0;
        for (double n : {100.0, 1000.0, 10000.0}) {
            try {
                ra = evaluate(a, {{var, n}});
                rb = evaluate(b, {{var, n}});
            } catch (...) {}
        }
        if (std::abs(rb) < 1e-15) {
            r.verdict = ConvergenceVerdict::INCONCLUSIVE;
            r.reason  = "b_n → 0, cannot compare";
            return r;
        }
        bool aLeB = ra <= rb + 1e-9 * std::abs(rb);
        r.verdict = ConvergenceVerdict::INCONCLUSIVE;
        r.reason  = "a_n ≈ " + std::to_string(ra) +
                    ", b_n ≈ " + std::to_string(rb) +
                    (aLeB ? ": if Σb_n converges so does Σa_n"
                          : ": if Σb_n diverges so does Σa_n");
    } catch (const std::exception& e) {
        r.ok = false; r.error = e.what();
    }
    return r;
}

// ── Limit comparison test ─────────────────────────────────────────────────────

TestResult limitComparisonTest(const std::string& aN,
                                const std::string& bN,
                                const std::string& var) {
    TestResult r;
    r.test = "Limit Comparison Test";
    try {
        ExprPtr a = parse(aN), b = parse(bN);
        ExprPtr ratio = simplify(div_expr(a, b));
        double  L     = limitAtInfinity(ratio, var);

        if (std::isfinite(L) && L > 1e-10) {
            r.verdict = ConvergenceVerdict::INCONCLUSIVE;
            r.reason  = "lim a_n/b_n = " + std::to_string(L) +
                        " (finite, nonzero) — Σa_n and Σb_n have same behavior";
        } else if (std::abs(L) < 1e-10) {
            r.verdict = ConvergenceVerdict::INCONCLUSIVE;
            r.reason  = "lim a_n/b_n = 0 — if Σb_n converges so does Σa_n";
        } else {
            r.verdict = ConvergenceVerdict::INCONCLUSIVE;
            r.reason  = "lim a_n/b_n = ∞ — if Σb_n diverges so does Σa_n";
        }
    } catch (const std::exception& e) {
        r.ok = false; r.error = e.what();
    }
    return r;
}

// ── Integral test ─────────────────────────────────────────────────────────────

TestResult integralTest(const std::string& aN, const std::string& var) {
    TestResult r;
    r.test = "Integral Test";
    try {
        ExprPtr a = parse(aN);
        Func1D  f = makeEvaluator(a, var);
        // ∫_1^∞ f(x) dx
        auto res = improperInfinite(f, 1.0);
        if (!res.ok) {
            r.verdict = ConvergenceVerdict::INCONCLUSIVE;
            r.reason  = "integral could not be evaluated";
            return r;
        }
        if (std::isfinite(res.value)) {
            r.verdict = ConvergenceVerdict::CONVERGES;
            r.reason  = "∫_1^∞ a_n dn ≈ " + std::to_string(res.value) + " (finite)";
        } else {
            r.verdict = ConvergenceVerdict::DIVERGES;
            r.reason  = "∫_1^∞ a_n dn diverges";
        }
    } catch (const std::exception& e) {
        r.ok = false; r.error = e.what();
    }
    return r;
}

// ── Alternating series test ───────────────────────────────────────────────────

TestResult alternatingSeriesTest(const std::string& bN, const std::string& var) {
    TestResult r;
    r.test = "Alternating Series Test (Leibniz)";
    try {
        ExprPtr b = parse(bN);
        // Check b_n → 0
        double  L = limitAtInfinity(b, var);
        bool    toZero = std::isfinite(L) && std::abs(L) < 1e-8;
        // Check b_n is decreasing: b(n+1) < b(n) for large n
        bool    decreasing = true;
        for (double n : {10.0, 100.0, 1000.0}) {
            try {
                double bn  = evaluate(b, {{var, n}});
                double bn1 = evaluate(b, {{var, n+1}});
                if (bn1 > bn + 1e-10) { decreasing = false; break; }
            } catch (...) {}
        }
        if (toZero && decreasing) {
            r.verdict = ConvergenceVerdict::CONVERGES;
            r.reason  = "b_n → 0 and b_n is decreasing";
        } else {
            r.verdict = ConvergenceVerdict::INCONCLUSIVE;
            r.reason  = std::string("b_n → 0: ") + (toZero?"yes":"no") +
                        ", decreasing: " + (decreasing?"yes":"no");
        }
    } catch (const std::exception& e) {
        r.ok = false; r.error = e.what();
    }
    return r;
}

// ── p-series ──────────────────────────────────────────────────────────────────

TestResult pSeriesTest(double p) {
    TestResult r;
    r.test = "p-Series Test";
    if (p > 1.0) {
        r.verdict = ConvergenceVerdict::CONVERGES;
        r.reason  = "p = " + std::to_string(p) + " > 1";
    } else {
        r.verdict = ConvergenceVerdict::DIVERGES;
        r.reason  = "p = " + std::to_string(p) + " ≤ 1";
    }
    return r;
}

// ── Geometric series ──────────────────────────────────────────────────────────

TestResult geometricSeriesTest(double r_val) {
    TestResult r;
    r.test = "Geometric Series Test";
    if (std::abs(r_val) < 1.0) {
        r.verdict = ConvergenceVerdict::CONVERGES;
        r.reason  = "|r| = " + std::to_string(std::abs(r_val)) + " < 1, sum = a/(1-r)";
    } else {
        r.verdict = ConvergenceVerdict::DIVERGES;
        r.reason  = "|r| = " + std::to_string(std::abs(r_val)) + " ≥ 1";
    }
    return r;
}

// ── testSeries — tries all applicable tests in priority order ─────────────────

TestResult testSeries(const std::string& aN, const std::string& var) {
    // 1. Divergence test first (cheapest)
    auto dt = divergenceTest(aN, var);
    if (dt.ok && dt.verdict == ConvergenceVerdict::DIVERGES) return dt;

    // 2. Ratio test
    auto rt = ratioTest(aN, var);
    if (rt.ok && rt.verdict != ConvergenceVerdict::INCONCLUSIVE) return rt;

    // 3. Root test
    auto root = rootTest(aN, var);
    if (root.ok && root.verdict != ConvergenceVerdict::INCONCLUSIVE) return root;

    // 4. Integral test
    auto it = integralTest(aN, var);
    if (it.ok && it.verdict != ConvergenceVerdict::INCONCLUSIVE) return it;

    // 5. Return inconclusive with a note
    TestResult out;
    out.test    = "Multiple tests";
    out.verdict = ConvergenceVerdict::INCONCLUSIVE;
    out.reason  = "All applied tests were inconclusive. Try comparison or alternating series test manually.";
    return out;
}

} // namespace Calculus