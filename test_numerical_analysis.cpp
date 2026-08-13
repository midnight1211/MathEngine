// NumericalAnalysis/tests/test_numerical_analysis.cpp
// ============================================================================
// Runtime correctness tests for NumericalAnalysis::dispatch(). See
// Calculus/tests/, Linear_Algebra/tests/, Statistics/tests/,
// NumberTheory/tests/ for the sister harnesses.
//
//     g++ -std=c++20 -Wall -I../.. ../NA.cpp test_numerical_analysis.cpp -o test_na && ./test_na
//
// (run from NumericalAnalysis/tests/)
// ============================================================================

#include <iostream>
#include <string>
#include <cstdlib>
#include "../NA.hpp"

static int g_checks = 0;
static int g_failures = 0;

static void check_near(const std::string& op, const std::string& json,
                        double expected, double tol, int line) {
    ++g_checks;
    auto r = NumericalAnalysis::dispatch(op, json);
    if (!r.ok) {
        ++g_failures;
        std::cerr << "FAIL line " << line << "  op=" << op << " json=" << json
                  << "  expected ok, got error=\"" << r.error << "\"\n";
        return;
    }
    try {
        double got = std::stod(r.value);
        if (std::abs(got - expected) > tol) {
            ++g_failures;
            std::cerr << "FAIL line " << line << "  op=" << op << " json=" << json
                      << "  expected ~" << expected << ", got " << got << "\n";
        }
    } catch (...) {
        ++g_failures;
        std::cerr << "FAIL line " << line << "  op=" << op << " json=" << json
                  << "  could not parse a number from value=\"" << r.value << "\"\n";
    }
}
#define CHECK_NEAR(op, json, expected, tol) check_near(op, json, expected, tol, __LINE__)

int main() {
    // ── Root finding ────────────────────────────────────────────────────
    // root of x^2 - 2 on [0,2] is sqrt(2)
    CHECK_NEAR("bisection", R"JSON({"f":"x^2-2","x":"x","a":0,"b":2,"tol":1e-8})JSON", 1.41421356, 1e-4);
    CHECK_NEAR("newton", R"JSON({"f":"x^2-2","x":"x","x0":1.0,"tol":1e-10})JSON", 1.41421356, 1e-6);
    CHECK_NEAR("secant", R"JSON({"f":"x^2-2","x":"x","x0":1.0,"x1":2.0,"tol":1e-8})JSON", 1.41421356, 1e-4);
    CHECK_NEAR("brent", R"JSON({"f":"x^2-2","x":"x","a":0,"b":2,"tol":1e-8})JSON", 1.41421356, 1e-4);

    // ── Interpolation ───────────────────────────────────────────────────
    // Lagrange through (0,0),(1,1),(2,4),(3,9) (i.e. y=x^2) evaluated at x=4 -> 16
    CHECK_NEAR("lagrange", R"JSON({"xs":"[0,1,2,3]","ys":"[0,1,4,9]","xeval":4})JSON", 16.0, 1e-6);
    CHECK_NEAR("neville", R"JSON({"xs":"[0,1,2,3]","ys":"[0,1,4,9]","xeval":4})JSON", 16.0, 1e-6);

    // ── Numerical differentiation ───────────────────────────────────────
    // d/dx[x^3] at x=2 is 3*4=12
    CHECK_NEAR("central_diff", R"JSON({"f":"x^3","x":"x","x0":2.0,"h":1e-4})JSON", 12.0, 1e-3);

    // ── Numerical integration — the exact bug class that broke
    // Calculus/integration/Numerical.cpp's romberg(); this is a
    // SEPARATE, independently-written implementation, verified here
    // rather than assumed correct just because it looks structurally
    // similar.
    CHECK_NEAR("romberg", R"JSON({"f":"x^2","x":"x","a":0,"b":1,"levels":6})JSON", 1.0/3.0, 1e-6);
    CHECK_NEAR("romberg", R"JSON({"f":"sin(x)","x":"x","a":0,"b":3.14159265358979,"levels":8})JSON", 2.0, 1e-4);
    CHECK_NEAR("simpsons", R"JSON({"f":"x^2","x":"x","a":0,"b":1,"n":10})JSON", 1.0/3.0, 1e-6);
    CHECK_NEAR("trapezoidal", R"JSON({"f":"x","x":"x","a":0,"b":2,"n":100})JSON", 2.0, 1e-3);

    // ── Linear systems ──────────────────────────────────────────────────
    // Regression test for a real bug: cu_parseMat/mcParseMat (MathCore.hpp)
    // mis-parsed the FIRST row of any matrix, silently dropping its first
    // element (see MathCore.hpp's mcParseMat comment for the full trace).
    // [[1,1],[0,1]]x=[3,2] has the exact, easy-to-hand-check solution
    // x1=1, x2=2 - the bug turned this into x1=3 (row 0 corrupted to just
    // [1], making A effectively [[1],[0,1]], a ragged/undefined read).
    {
        ++g_checks;
        auto r = NumericalAnalysis::dispatch("gauss_elim",
            R"JSON({"A":[[1,1],[0,1]],"b":[3,2]})JSON");
        if (!r.ok || r.value.find("x_1 = 1\n") == std::string::npos
                  || r.value.find("x_2 = 2") == std::string::npos) {
            ++g_failures;
            std::cerr << "FAIL gauss_elim regression: expected x_1=1, x_2=2, got ok=" << r.ok
                      << " value=\"" << r.value << "\" error=\"" << r.error << "\"\n";
        }
    }

    // Solve [[2,1],[1,3]]x = [3,5]: x = (0.8, 1.4)
    {
        ++g_checks;
        auto r = NumericalAnalysis::dispatch("gauss_elim",
            R"JSON({"A":"[[2,1],[1,3]]","b":"[3,5]"})JSON");
        if (!r.ok || r.value.find("0.8") == std::string::npos) {
            ++g_failures;
            std::cerr << "FAIL gauss_elim: expected solution to mention 0.8, got ok=" << r.ok
                      << " value=\"" << r.value << "\" error=\"" << r.error << "\"\n";
        }
    }

    std::cout << (g_checks - g_failures) << "/" << g_checks << " checks passed";
    if (g_failures) {
        std::cout << "  (" << g_failures << " FAILED)\n";
        return 1;
    }
    std::cout << "\n";
    return 0;
}
