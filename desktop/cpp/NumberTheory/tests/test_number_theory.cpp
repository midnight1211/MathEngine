// NumberTheory/tests/test_number_theory.cpp
// ============================================================================
// Runtime correctness tests for NumberTheory::dispatch() - the same entry
// point the JNI bridge and server call with "nt:<op>|{json}" - checked
// against independently-known correct values, not just "does it compile".
//
// Standalone, no test framework (matches utils/tests/test_utils.cpp):
//
//     g++ -std=c++20 -Wall -I../.. ../NumberTheory.cpp test_number_theory.cpp -o test_nt && ./test_nt
//
// (run from NumberTheory/tests/)
// ============================================================================

#include <iostream>
#include <string>
#include "../NumberTheory.hpp"

static int g_checks = 0;
static int g_failures = 0;

#define CHECK_EQ(op, json, expected) check_eq(op, json, expected, __LINE__)
#define CHECK_OK(op, json) check_ok(op, json, __LINE__)
#define CHECK_ERR(op, json) check_err(op, json, __LINE__)

static void check_eq(const std::string& op, const std::string& json,
                      const std::string& expected, int line) {
    ++g_checks;
    auto r = NumberTheory::dispatch(op, json);
    if (!r.ok || r.value != expected) {
        ++g_failures;
        std::cerr << "FAIL line " << line << "  op=" << op << " json=" << json
                  << "  expected=\"" << expected << "\"  got ok=" << r.ok
                  << " value=\"" << r.value << "\" error=\"" << r.error << "\"\n";
    }
}

static void check_ok(const std::string& op, const std::string& json, int line) {
    ++g_checks;
    auto r = NumberTheory::dispatch(op, json);
    if (!r.ok) {
        ++g_failures;
        std::cerr << "FAIL line " << line << "  op=" << op << " json=" << json
                  << "  expected ok, got error=\"" << r.error << "\"\n";
    }
}

static void check_err(const std::string& op, const std::string& json, int line) {
    ++g_checks;
    auto r = NumberTheory::dispatch(op, json);
    if (r.ok) {
        ++g_failures;
        std::cerr << "FAIL line " << line << "  op=" << op << " json=" << json
                  << "  expected an error, got ok value=\"" << r.value << "\"\n";
    }
}

int main() {
    using namespace std::string_literals;

    // ── Primes ──────────────────────────────────────────────────────────
    CHECK_EQ("is_prime", R"({"n":97})", "true");
    CHECK_EQ("is_prime", R"({"n":100})", "false");
    CHECK_EQ("next_prime", R"({"n":100})", "101");
    CHECK_EQ("nth_prime", R"({"n":10})", "29");          // 10th prime is 29
    CHECK_EQ("prime_pi", R"({"n":100})", "25");          // pi(100) = 25

    // ── Divisibility ────────────────────────────────────────────────────
    CHECK_EQ("gcd", R"({"a":48,"b":18})", "6");
    CHECK_EQ("lcm", R"({"a":4,"b":6})", "12");
    CHECK_EQ("num_divisors", R"({"n":28})", "6");         // 1,2,4,7,14,28
    CHECK_EQ("sum_divisors", R"({"n":28})", "56");        // includes 28 itself
    CHECK_EQ("is_perfect", R"({"n":28})", "true");
    CHECK_EQ("is_perfect", R"({"n":30})", "false");
    CHECK_EQ("euler_phi", R"({"n":36})", "12");

    // ── Modular arithmetic ──────────────────────────────────────────────
    CHECK_EQ("mod_pow", R"({"base":2,"exp":10,"mod":1000})", "24");   // 1024 mod 1000
    CHECK_EQ("mod_inverse", R"({"a":3,"mod":11})", "4");              // 3*4=12=1 mod 11

    // ── Special sequences ───────────────────────────────────────────────
    CHECK_EQ("fibonacci", R"({"n":10})", "55");
    CHECK_EQ("lucas", R"({"n":10})", "123");
    CHECK_EQ("catalan", R"({"n":5})", "42");

    // ── Chinese Remainder Theorem — the bug this test file was written to
    // catch: dispatch("crt", ...) previously ignored its json entirely and
    // always solved for two empty vectors, regardless of what the caller
    // passed. x=23 is the unique solution mod 105 for x=2(mod3),3(mod5),2(mod7).
    CHECK_OK("crt", R"({"remainders":[2,3,2],"moduli":[3,5,7]})");
    {
        auto r = NumberTheory::dispatch("crt", R"({"remainders":[2,3,2],"moduli":[3,5,7]})");
        ++g_checks;
        if (r.value.find("23") == std::string::npos) {
            ++g_failures;
            std::cerr << "FAIL crt: expected solution to mention 23, got \""
                      << r.value << "\"\n";
        }
    }

    // ── Error handling ──────────────────────────────────────────────────
    CHECK_ERR("mod_inverse", R"({"a":2,"mod":4})");  // gcd(2,4)!=1, no inverse exists
    CHECK_ERR("not_a_real_op", R"({})");

    std::cout << (g_checks - g_failures) << "/" << g_checks << " checks passed";
    if (g_failures) {
        std::cout << "  (" << g_failures << " FAILED)\n";
        return 1;
    }
    std::cout << "\n";
    return 0;
}
