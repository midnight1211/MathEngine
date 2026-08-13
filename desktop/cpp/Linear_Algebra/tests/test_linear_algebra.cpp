// Linear_Algebra/tests/test_linear_algebra.cpp
// ============================================================================
// Runtime correctness tests for LinearAlgebra::dispatch() - checked against
// independently-known correct values. See NumberTheory/tests/ for the sister
// harness and rationale (same "compiling isn't the same as correct" finding
// that started this).
//
//     g++ -std=c++20 -Wall -I../.. ../LA.cpp test_linear_algebra.cpp -o test_la && ./test_la
//
// (run from Linear_Algebra/tests/)
// ============================================================================

#include <iostream>
#include <string>
#include "../LA.hpp"

static int g_checks = 0;
static int g_failures = 0;

static std::string run(const std::string& op, const std::string& input, bool exact = true) {
    return LinearAlgebra::dispatch(op, input, exact).format(exact);
}

static void check_contains(const std::string& op, const std::string& input,
                            const std::string& expectedSubstr, int line, bool exact = true) {
    ++g_checks;
    std::string got = run(op, input, exact);
    if (got.find(expectedSubstr) == std::string::npos) {
        ++g_failures;
        std::cerr << "FAIL line " << line << "  op=" << op << " input=" << input
                  << "  expected to contain \"" << expectedSubstr << "\", got \"" << got << "\"\n";
    }
}
#define CHECK_CONTAINS(op, input, expected) check_contains(op, input, expected, __LINE__)

static void check_error(const std::string& op, const std::string& input, int line) {
    ++g_checks;
    std::string got = run(op, input);
    if (got.rfind("ERROR", 0) != 0) {
        ++g_failures;
        std::cerr << "FAIL line " << line << "  op=" << op << " input=" << input
                  << "  expected an ERROR, got \"" << got << "\"\n";
    }
}
#define CHECK_ERROR(op, input) check_error(op, input, __LINE__)

int main() {
    // ── Determinant ─────────────────────────────────────────────────────
    // det([[1,2],[3,4]]) = 1*4 - 2*3 = -2
    CHECK_CONTAINS("determinant", "[[1,2],[3,4]]", "-2");
    // det(3x3) known example = 1 (a classic textbook matrix)
    CHECK_CONTAINS("determinant", "[[2,-3,1],[2,0,-1],[1,4,5]]", "49");

    // ── Trace ───────────────────────────────────────────────────────────
    CHECK_CONTAINS("trace", "[[1,2],[3,4]]", "5");  // 1+4

    // ── Transpose ───────────────────────────────────────────────────────
    CHECK_CONTAINS("matrix_transpose", "[[1,2],[3,4]]", "3");  // (0,1)->(1,0): 3 shows up transposed

    // ── Matrix add / multiply ───────────────────────────────────────────
    CHECK_CONTAINS("matrix_add", "[[1,2],[3,4]]|[[5,6],[7,8]]", "6");  // (0,0): 1+5=6
    // [[1,2],[3,4]] * [[5,6],[7,8]] = [[19,22],[43,50]]
    CHECK_CONTAINS("matrix_multiply", "[[1,2],[3,4]]|[[5,6],[7,8]]", "19");
    CHECK_CONTAINS("matrix_multiply", "[[1,2],[3,4]]|[[5,6],[7,8]]", "50");

    // Strassen's algorithm should agree with the naive multiply above
    CHECK_CONTAINS("matrix_strassen", "[[1,2],[3,4]]|[[5,6],[7,8]]", "19");
    CHECK_CONTAINS("matrix_strassen", "[[1,2],[3,4]]|[[5,6],[7,8]]", "50");

    // ── Rank ────────────────────────────────────────────────────────────
    CHECK_CONTAINS("rank", "[[1,2],[2,4]]", "1");   // second row is a multiple of the first
    CHECK_CONTAINS("rank", "[[1,0],[0,1]]", "2");   // full rank

    // ── Inverse ─────────────────────────────────────────────────────────
    // inverse of [[2,0],[0,2]] is [[0.5,0],[0,0.5]] — exact=true means the
    // symbolic form (fractions) is preferred, per LAResult::format()
    CHECK_CONTAINS("inverse", "[[2,0],[0,2]]", "1/2");
    // singular matrix should error, not silently return garbage
    CHECK_ERROR("inverse", "[[1,2],[2,4]]");

    // ── Eigenvalues ─────────────────────────────────────────────────────
    // eigenvalues of a diagonal matrix are its diagonal entries
    CHECK_CONTAINS("eigenvalues", "[[2,0],[0,3]]", "2");
    CHECK_CONTAINS("eigenvalues", "[[2,0],[0,3]]", "3");

    std::cout << (g_checks - g_failures) << "/" << g_checks << " checks passed";
    if (g_failures) {
        std::cout << "  (" << g_failures << " FAILED)\n";
        return 1;
    }
    std::cout << "\n";
    return 0;
}
