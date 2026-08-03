#pragma once
// utils/Tuple3.hpp
// =============================================================================
// Tuple3<A,B,C> — fixed-arity triple value-type, replacing the codebase's
// std::tuple<long long, int, int> edge representation in graph algorithms.
//
// Provides structured binding (C++17), comparison operators, and a
// make_tuple3 free function.
// =============================================================================

#include <utility>
#include <cstddef>

namespace utils {

template<typename A, typename B, typename C>
struct Tuple3 {
    A first;
    B second;
    C third;

    constexpr Tuple3() = default;
    constexpr Tuple3(const A& a, const B& b, const C& c)
        : first(a), second(b), third(c) {}
    constexpr Tuple3(A&& a, B&& b, C&& c)
        : first(std::move(a)), second(std::move(b)), third(std::move(c)) {}

    bool operator==(const Tuple3& o) const
    { return first == o.first && second == o.second && third == o.third; }
    bool operator!=(const Tuple3& o) const { return !(*this == o); }

    // Lexicographic ordering (needed for sorting edges by weight)
    bool operator< (const Tuple3& o) const
    {
        if (first  != o.first)  return first  < o.first;
        if (second != o.second) return second < o.second;
        return third < o.third;
    }
    bool operator<=(const Tuple3& o) const { return !(o < *this); }
    bool operator> (const Tuple3& o) const { return o < *this; }
    bool operator>=(const Tuple3& o) const { return !(*this < o); }
};

template<typename A, typename B, typename C>
Tuple3<A,B,C> make_tuple3(A&& a, B&& b, C&& c)
{ return {std::forward<A>(a), std::forward<B>(b), std::forward<C>(c)}; }

// Common alias used in the codebase for Kruskal edge triples (weight, u, v)
using GraphEdge = Tuple3<long long, int, int>;

} // namespace utils

// ── Structured binding support ─────────────────────────────────────────────────
namespace std {

template<typename A, typename B, typename C>
struct tuple_size<utils::Tuple3<A,B,C>> : integral_constant<size_t, 3> {};

template<size_t I, typename A, typename B, typename C>
struct tuple_element<I, utils::Tuple3<A,B,C>>;

template<typename A, typename B, typename C>
struct tuple_element<0, utils::Tuple3<A,B,C>> { using type = A; };

template<typename A, typename B, typename C>
struct tuple_element<1, utils::Tuple3<A,B,C>> { using type = B; };

template<typename A, typename B, typename C>
struct tuple_element<2, utils::Tuple3<A,B,C>> { using type = C; };

}

namespace utils {

template<std::size_t I, typename A, typename B, typename C>
auto& get(Tuple3<A,B,C>& t)
{
    if constexpr      (I == 0) return t.first;
    else if constexpr (I == 1) return t.second;
    else                        return t.third;
}
template<std::size_t I, typename A, typename B, typename C>
const auto& get(const Tuple3<A,B,C>& t)
{
    if constexpr      (I == 0) return t.first;
    else if constexpr (I == 1) return t.second;
    else                        return t.third;
}

} // namespace utils
