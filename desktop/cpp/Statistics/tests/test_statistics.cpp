// Statistics/tests/test_statistics.cpp
// ============================================================================
// Runtime correctness tests for Statistics::dispatch() against independently
// -known correct values. Statistics is the single largest module by intent
// count (111 of the chatbot's 612 intents route here - see
// desktop/python/chatbot/coverage_report.py), so a silent bug here has the
// widest blast radius of any one module in the engine.
//
//     g++ -std=c++20 -Wall -I../.. ../Statistics.cpp ../Advanced_Statistics.cpp test_statistics.cpp -o test_stat && ./test_stat
//
// (run from Statistics/tests/) - note both .cpp files are required: dispatch()
// in Statistics.cpp calls into functions only implemented in
// Advanced_Statistics.cpp (ANOVA follow-ups, categorical tests, time series,
// control charts, ...), so linking Statistics.cpp alone fails with dozens of
// "undefined reference" errors.
// ============================================================================

#include <iostream>
#include <string>
#include "../Statistics.hpp"

static int g_checks = 0;
static int g_failures = 0;

static void check_contains(const std::string& op, const std::string& json,
                            const std::string& expectedSubstr, int line) {
    ++g_checks;
    auto r = Statistics::dispatch(op, json, true);
    std::string got = r.ok ? r.value : ("ERROR: " + r.error);
    if (got.find(expectedSubstr) == std::string::npos) {
        ++g_failures;
        std::cerr << "FAIL line " << line << "  op=" << op << " json=" << json
                  << "  expected to contain \"" << expectedSubstr << "\", got \"" << got << "\"\n";
    }
}
#define CHECK_CONTAINS(op, json, expected) check_contains(op, json, expected, __LINE__)

int main() {
    // Dataset: [4,8,15,16,23,42]  (n=6, sum=108, mean=18)
    const std::string X = R"({"x":[4,8,15,16,23,42]})";

    CHECK_CONTAINS("mean", X, "18");
    CHECK_CONTAINS("median", X, "15.5");   // (15+16)/2

    // Sample variance = sum((xi-mean)^2)/(n-1)
    // deviations: -14,-10,-3,-2,5,24 -> squares: 196,100,9,4,25,576 = 910
    // sample variance = 910/5 = 182
    CHECK_CONTAINS("variance", X, "182");
    // sample stddev = sqrt(182) ≈ 13.49
    CHECK_CONTAINS("stddev", X, "13.4");

    // Simple, hand-checkable dataset for mode: [1,2,2,3]
    CHECK_CONTAINS("mode", R"({"x":[1,2,2,3]})", "2");

    // IQR of [1..8] using linear-interpolation percentiles (the engine's
    // convention, same as NumPy's default 'linear' method): position for
    // Q1 = 0.25*(n-1) = 1.75 -> interpolate x[1..2] = 2.75;
    // Q3 position = 0.75*(n-1) = 5.25 -> interpolate x[5..6] = 6.25;
    // IQR = 6.25 - 2.75 = 3.5
    CHECK_CONTAINS("iqr", R"({"x":[1,2,3,4,5,6,7,8]})", "3.5");

    // Correlation of a dataset with itself should be exactly 1
    CHECK_CONTAINS("pearson", R"({"x":[1,2,3,4,5],"y":[1,2,3,4,5]})", "1");
    // Perfectly anti-correlated: should be exactly -1
    CHECK_CONTAINS("pearson", R"({"x":[1,2,3,4,5],"y":[5,4,3,2,1]})", "-1");

    // Odds ratio of 2x2 table [[10,5],[5,10]]: (10*10)/(5*5) = 4
    CHECK_CONTAINS("odds_ratio", R"({"a":10,"b":5,"c":5,"d":10})", "4");

    // McNemar's chi-square = (b-c)^2/(b+c); for b=20,c=5: (15)^2/25 = 9
    CHECK_CONTAINS("mcnemar", R"({"a":1,"b":20,"c":5,"d":1})", "9");

    std::cout << (g_checks - g_failures) << "/" << g_checks << " checks passed";
    if (g_failures) {
        std::cout << "  (" << g_failures << " FAILED)\n";
        return 1;
    }
    std::cout << "\n";
    return 0;
}
