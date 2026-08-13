// Calculus/tests/test_calculus.cpp
// ============================================================================
// Runtime correctness tests for Calculus::dispatch() against independently
// -known correct results. See NumberTheory/tests/, Linear_Algebra/tests/,
// Statistics/tests/ for the sister harnesses.
//
// This is also the file the module's own CMakeLists.txt already anticipated
// (CALCULUS_BUILD_TESTS option -> tests/test_expression.cpp) but that never
// got written; this fills that gap under a clearer name (it tests dispatch()
// end-to-end, not just the Expression class) rather than the placeholder.
//
//     g++ -std=c++20 -Wall -I.. -Icore -I../../parser/include_week_four \
//       ../Calculus.cpp ../core/Expression.cpp ../core/Week4Bridge.cpp ../core/Simplify.cpp \
//       ../differentiation/Derivative.cpp ../differentiation/Partial.cpp ../differentiation/Implicit.cpp \
//       ../limits/Limits.cpp \
//       ../integration/Numerical.cpp ../integration/Symbolic.cpp ../integration/Multivariable.cpp \
//       ../series/Series.cpp ../series/Convergence.cpp \
//       ../vectorcalc/VectorOps.cpp ../vectorcalc/LineIntegral.cpp ../vectorcalc/SurfaceIntegral.cpp ../vectorcalc/Theorems.cpp \
//       ../optimization/Optimize.cpp \
//       ../../parser/src_week_four/lexer.cpp ../../parser/src_week_four/parser.cpp ../../parser/src_week_four/evaluator.cpp \
//       test_calculus.cpp -o test_calc && ./test_calc
//
// (run from Calculus/tests/) - this mirrors exactly the CALCULUS_SOURCES list
// in Calculus/CMakeLists.txt plus the week_four parser sources Week4Bridge.cpp
// needs; that's a lot of moving parts for one module, all wired up here so a
// developer doesn't have to reconstruct it from the CMakeLists by hand.
// ============================================================================

#include <iostream>
#include <string>
#include "../Calculus.hpp"

static int g_checks = 0;
static int g_failures = 0;

static void check_contains(const std::string& op, const std::string& json,
                            const std::string& expectedSubstr, int line) {
    ++g_checks;
    auto r = Calculus::dispatch(op, json);
    std::string got = r.ok ? (r.symbolic.empty() ? r.numeric : r.symbolic) : ("ERROR: " + r.error);
    if (got.find(expectedSubstr) == std::string::npos) {
        ++g_failures;
        std::cerr << "FAIL line " << line << "  op=" << op << " json=" << json
                  << "  expected to contain \"" << expectedSubstr << "\", got \"" << got
                  << "\" (numeric=\"" << r.numeric << "\")\n";
    }
}
#define CHECK_CONTAINS(op, json, expected) check_contains(op, json, expected, __LINE__)

static void check_numeric_near(const std::string& op, const std::string& json,
                                double expected, double tol, int line) {
    ++g_checks;
    auto r = Calculus::dispatch(op, json);
    if (!r.ok) {
        ++g_failures;
        std::cerr << "FAIL line " << line << "  op=" << op << " json=" << json
                  << "  expected ok, got error=\"" << r.error << "\"\n";
        return;
    }
    try {
        double got = std::stod(r.numeric.empty() ? r.symbolic : r.numeric);
        if (std::abs(got - expected) > tol) {
            ++g_failures;
            std::cerr << "FAIL line " << line << "  op=" << op << " json=" << json
                      << "  expected ~" << expected << ", got " << got << "\n";
        }
    } catch (...) {
        ++g_failures;
        std::cerr << "FAIL line " << line << "  op=" << op << " json=" << json
                  << "  could not parse a number from numeric=\"" << r.numeric
                  << "\" symbolic=\"" << r.symbolic << "\"\n";
    }
}
#define CHECK_NUMERIC_NEAR(op, json, expected, tol) check_numeric_near(op, json, expected, tol, __LINE__)

int main() {
    // ── Differentiation ─────────────────────────────────────────────────
    // d/dx[x^2] = 2x
    CHECK_CONTAINS("diff", R"JSON({"expr":"x^2","var":"x","order":1})JSON", "2 * x");
    // d/dx[sin(x)] = cos(x)
    CHECK_CONTAINS("diff", R"JSON({"expr":"sin(x)","var":"x","order":1})JSON", "cos(x)");
    // Second derivative of x^3 is 6x
    CHECK_CONTAINS("diff", R"JSON({"expr":"x^3","var":"x","order":2})JSON", "6 * x");

    // ── Integration ─────────────────────────────────────────────────────
    // integral of x^2 dx = x^3/3 (+C)
    CHECK_CONTAINS("integrate", R"JSON({"expr":"x^2","var":"x"})JSON", "3");
    // definite integral of x^2 from 0 to 1 = 1/3
    CHECK_NUMERIC_NEAR("definite_int", R"JSON({"expr":"x^2","var":"x","a":"0","b":"1"})JSON", 1.0/3.0, 1e-6);
    // definite integral of x from 0 to 2 = 2
    CHECK_NUMERIC_NEAR("definite_int", R"JSON({"expr":"x","var":"x","a":"0","b":"2"})JSON", 2.0, 1e-6);

    // ── Limits ──────────────────────────────────────────────────────────
    // classic: lim x->0 sin(x)/x = 1
    CHECK_NUMERIC_NEAR("limit", R"JSON({"expr":"sin(x)/x","var":"x","point":"0"})JSON", 1.0, 1e-4);

    // ── Numerical integration ───────────────────────────────────────────
    CHECK_NUMERIC_NEAR("numerical_int",
        R"JSON({"expr":"x^2","var":"x","a":0,"b":1,"method":"romberg"})JSON", 1.0/3.0, 1e-4);
    // Regression test for a real bug: romberg()'s new-point sampling loop
    // used the wrong upper bound (see Numerical.cpp's romberg() comment),
    // silently halving the number of new sample points at every refinement
    // row past the first. It was accurate enough on x^2 near a small
    // interval to hide in a quick check, but wildly wrong on anything less
    // trivial - sin(x) from 0 to pi has a known closed-form answer of
    // exactly 2, and the buggy version returned something on the order of
    // 0.5-0.9, not close to 2, regardless of tolerance.
    CHECK_NUMERIC_NEAR("numerical_int",
        R"JSON({"expr":"sin(x)","var":"x","a":0,"b":3.14159265358979,"method":"romberg"})JSON", 2.0, 1e-4);

    // ── Gradient / partial derivatives ──────────────────────────────────
    // partial d/dx[x^2*y] = 2xy
    CHECK_CONTAINS("partial", R"JSON({"expr":"x^2*y","var":"x","order":1})JSON", "2");

    // ── Vector calculus ─────────────────────────────────────────────────
    // curl of [y,-x,0] = (0,0,-2)  (classic rotational field)
    CHECK_CONTAINS("curl", R"JSON({"exprs":["y","-x","0"],"vars":["x","y","z"]})JSON", "2");

    std::cout << (g_checks - g_failures) << "/" << g_checks << " checks passed";
    if (g_failures) {
        std::cout << "  (" << g_failures << " FAILED)\n";
        return 1;
    }
    std::cout << "\n";
    return 0;
}
