// utils/tests/test_utils.cpp
// ============================================================================
// Standalone runtime tests for the utils/ STL-replacement headers (Vec, Mat,
// HashMap, OrderedMap, Optional, Queue, Stack, Pair, Tuple3, Complex,
// Function). These headers are used throughout every math module (see the
// header comments in Vec.hpp/OrderedMap.hpp/etc.), so a bug here is a bug
// everywhere - and simply compiling them (see the *.hpp fixes made
// alongside this file) only catches parse errors, not logic bugs that only
// surface once a template is actually instantiated and exercised.
//
// Deliberately NOT wired into the project's CMake build: this environment
// has no cmake available, so this is a plain-g++-compilable, dependency-free
// harness a developer can run directly:
//
//     g++ -std=c++20 -Wall -Wextra -I.. test_utils.cpp -o test_utils && ./test_utils
//
// (run from utils/tests/, hence the -I.. to find the headers one level up)
//
// No test framework dependency (no GoogleTest/Catch2 - matching the "no
// external deps" philosophy the rest of the C++ engine follows). Just a
// small CHECK macro that reports failures with file:line and keeps going
// instead of aborting on the first one, so a single run surfaces everything
// broken rather than one bug at a time.
// ============================================================================

#include <iostream>
#include <string>
#include <sstream>

#include "../Vec.hpp"
#include "../Mat.hpp"
#include "../HashMap.hpp"
#include "../OrderedMap.hpp"
#include "../Optional.hpp"
#include "../Queue.hpp"
#include "../Stack.hpp"
#include "../Pair.hpp"
#include "../Tuple3.hpp"
#include "../Complex.hpp"
#include "../Function.hpp"

static int g_checks = 0;
static int g_failures = 0;

#define CHECK(cond) \
    do { \
        ++g_checks; \
        if (!(cond)) { \
            ++g_failures; \
            std::cerr << "FAIL " << __FILE__ << ":" << __LINE__ << "  " << #cond << "\n"; \
        } \
    } while (0)

#define CHECK_THROWS(expr, ExcType) \
    do { \
        ++g_checks; \
        bool threw = false; \
        try { expr; } catch (const ExcType&) { threw = true; } catch (...) {} \
        if (!threw) { \
            ++g_failures; \
            std::cerr << "FAIL " << __FILE__ << ":" << __LINE__ \
                      << "  expected " #ExcType " from: " #expr "\n"; \
        } \
    } while (0)

// ── Vec<T> ──────────────────────────────────────────────────────────────
static void test_vec() {
    utils::Vec<int> v;
    CHECK(v.empty());
    v.push_back(1);
    v.push_back(2);
    v.push_back(3);
    CHECK(v.size() == 3);
    CHECK(v[0] == 1 && v[1] == 2 && v[2] == 3);
    CHECK(v.front() == 1 && v.back() == 3);

    v.insert(v.begin() + 1, 99);
    CHECK(v.size() == 4);
    CHECK(v[1] == 99);

    v.erase(v.begin() + 1);
    CHECK(v.size() == 3);
    CHECK(v[1] == 2);

    utils::Vec<int> copy = v;
    copy.push_back(4);
    CHECK(copy.size() == 4 && v.size() == 3);  // deep copy, not aliased

    int sum = 0;
    for (int x : v) sum += x;  // range-for
    CHECK(sum == 1 + 2 + 3);

    utils::Vec<int> moved = std::move(copy);
    CHECK(moved.size() == 4);

    CHECK_THROWS(v.at(999), std::out_of_range);
    v.pop_back();
    v.pop_back();
    v.pop_back();
    CHECK_THROWS(v.pop_back(), std::underflow_error);

    // Resize grows/shrinks and default/value-initializes correctly
    utils::Vec<int> r(3, 7);
    CHECK(r.size() == 3 && r[0] == 7 && r[2] == 7);  // was ambiguous with the
                                                       // (InputIt,InputIt) ctor
                                                       // before Vec.hpp's SFINAE fix
    r.resize(5, 9);
    CHECK(r.size() == 5 && r[4] == 9);
    r.resize(1);
    CHECK(r.size() == 1 && r[0] == 7);
}

// ── Mat<T> ──────────────────────────────────────────────────────────────
static void test_mat() {
    utils::Mat<double> m{{1, 2}, {3, 4}};
    CHECK(m.rows() == 2 && m.cols() == 2);
    CHECK(m.at(0, 0) == 1 && m.at(0, 1) == 2 && m.at(1, 0) == 3 && m.at(1, 1) == 4);

    auto t = m.transpose();
    CHECK(t.at(0, 1) == 3 && t.at(1, 0) == 2);

    auto id = utils::Mat<double>::identity(3);
    CHECK(id.rows() == 3 && id.cols() == 3);
    CHECK(id.at(0, 0) == 1 && id.at(0, 1) == 0 && id.at(1, 1) == 1);
}

// ── Complex ─────────────────────────────────────────────────────────────
static void test_complex() {
    utils::Complex a(3, 4);
    CHECK(a.abs() == 5.0);
    utils::Complex b(1, 2);
    auto sum = a + b;
    CHECK(sum.real() == 4 && sum.imag() == 6);
    auto prod = a * b;
    // (3+4i)(1+2i) = 3 + 6i + 4i + 8i^2 = 3 - 8 + 10i = -5 + 10i
    CHECK(prod.real() == -5 && prod.imag() == 10);
    auto c = a.conj();
    CHECK(c.real() == 3 && c.imag() == -4);
}

// ── Optional<T> ─────────────────────────────────────────────────────────
static void test_optional() {
    utils::Optional<int> empty;
    CHECK(!empty.has_value());
    CHECK(empty.value_or(42) == 42);
    CHECK_THROWS(empty.value(), std::bad_optional_access);

    utils::Optional<int> present(7);
    CHECK(present.has_value());
    CHECK(*present == 7);
    CHECK(present.value_or(42) == 7);

    utils::Optional<int> copy = present;
    copy = utils::nullopt;
    CHECK(!copy.has_value());
    CHECK(present.has_value());  // original untouched by copy's reset
}

// ── Pair<A,B> ───────────────────────────────────────────────────────────
static void test_pair() {
    utils::Pair<int, std::string> p(1, "one");
    CHECK(p.first == 1 && p.second == "one");

    std::pair<int, std::string> std_p(2, "two");
    utils::Pair<int, std::string> from_std(std_p);       // copy-from-std::pair ctor
    CHECK(from_std.first == 2 && from_std.second == "two");

    utils::Pair<int, std::string> from_std_move(std::pair<int, std::string>(3, "three"));
    CHECK(from_std_move.first == 3 && from_std_move.second == "three");

    auto made = utils::make_pair2(4, std::string("four"));
    CHECK(made.first == 4 && made.second == "four");

    utils::Pair<int, int> a(1, 2), b(1, 3);
    CHECK(a < b);
    CHECK(a != b);

    // Structured bindings
    auto [f, s] = p;
    CHECK(f == 1 && s == "one");
}

// ── Tuple3<A,B,C> ───────────────────────────────────────────────────────
static void test_tuple3() {
    utils::Tuple3<int, int, double> t(1, 2, 3.5);
    CHECK(t.first == 1 && t.second == 2 && t.third == 3.5);
    utils::Tuple3<int, int, double> t2(1, 2, 4.0);
    CHECK(t < t2);
}

// ── Stack<T> ────────────────────────────────────────────────────────────
static void test_stack() {
    utils::Stack<int> s;
    CHECK(s.empty());
    s.push(1);
    s.push(2);
    s.push(3);
    CHECK(s.top() == 3);
    s.pop();
    CHECK(s.top() == 2);
    CHECK(s.size() == 2);
}

// ── Queue<T> ────────────────────────────────────────────────────────────
static void test_queue() {
    utils::Queue<int> q;
    CHECK(q.empty());
    q.push(1);
    q.push(2);
    q.push(3);
    CHECK(q.front() == 1);
    CHECK(q.back() == 3);
    q.pop();
    CHECK(q.front() == 2);
    CHECK(q.size() == 2);

    // Wrap-around: exercises the head_/tail_ modulo arithmetic that the
    // "head" (missing underscore) typo in pop() had been silently
    // corrupting before this test file's accompanying fix.
    utils::Queue<int> wrap;
    for (int i = 0; i < 4; ++i) wrap.push(i);
    wrap.pop(); wrap.pop();
    wrap.push(4);
    wrap.push(5);
    CHECK(wrap.front() == 2);
    CHECK(wrap.back() == 5);
    CHECK(wrap.size() == 4);
}

// ── HashMap<K,V> ────────────────────────────────────────────────────────
static void test_hashmap() {
    utils::HashMap<std::string, int> h;
    CHECK(h.size() == 0);
    h["a"] = 1;
    h["b"] = 2;
    CHECK(h.size() == 2);
    CHECK(h.contains("a") && !h.contains("z"));
    CHECK(h.at("a") == 1);
    CHECK_THROWS(h.at("z"), std::out_of_range);

    h["a"] = 10;  // overwrite, not duplicate
    CHECK(h.size() == 2);
    CHECK(h.at("a") == 10);

    CHECK(h.erase("a"));
    CHECK(!h.contains("a"));
    CHECK(h.size() == 1);
    CHECK(!h.erase("nonexistent"));
}

// ── OrderedMap<K,V> ─────────────────────────────────────────────────────
static void test_ordered_map() {
    utils::OrderedMap<int, std::string> m;
    m.insert(3, "three");
    m.insert(1, "one");
    m.insert(2, "two");
    CHECK(m.size() == 3);
    CHECK(m.contains(2));
    CHECK(m.at(1) == "one");

    // In-order iteration must come back sorted by key - this is the whole
    // point of the red-black tree, and exactly what the Iterator/iterator
    // name-case bug (fixed alongside this file) prevented from even
    // compiling before.
    std::string order;
    for (auto it = m.begin(); it != m.end(); ++it)
        order += std::to_string(it.key()) + " ";
    CHECK(order == "1 2 3 ");

    m.erase(2);
    CHECK(m.size() == 2 && !m.contains(2));
}

// ── Function<R(Args...)> ────────────────────────────────────────────────
static void test_function() {
    utils::Function<int(int, int)> add = [](int a, int b) { return a + b; };
    CHECK(add(2, 3) == 5);

    utils::Function<int(int, int)> empty_fn;
    CHECK_THROWS(empty_fn(1, 2), std::bad_function_call);

    int captured = 10;
    utils::Function<int()> closure = [captured] { return captured * 2; };
    CHECK(closure() == 20);
}

int main() {
    test_vec();
    test_mat();
    test_complex();
    test_optional();
    test_pair();
    test_tuple3();
    test_stack();
    test_queue();
    test_hashmap();
    test_ordered_map();
    test_function();

    std::cout << (g_checks - g_failures) << "/" << g_checks << " checks passed";
    if (g_failures) {
        std::cout << "  (" << g_failures << " FAILED)\n";
        return 1;
    }
    std::cout << "\n";
    return 0;
}
