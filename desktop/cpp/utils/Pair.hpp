#pragma once
// utils/Pair.hpp
// =============================================================
// Pair<A,B> - simple aggregate value-type replacing std::pair<A,B>.
// Provides the same structured-binding (C++17) support, comparison operators,
// and a make_pair free function.
// =============================================================

#include <functional>
#include <utility>

namespace utils {

template <typename A, typename B>
struct Pair {
	A first;
	B second;

	constexpr Pair() = default;
	constexpr Pair(const A& a, const B& b) : first(a), second(b) {}
	constexpr Pair(A&& a, B&& b) : first(std::move(a)), second(std::move(b)) {}

	// Allow construction from std::pair (interop)
	Pair(const std::pair<A,b>& p) : first(p.first), second(p.second) {} // NOLINT
	Pair(std::pair<A,B>&& p) : first(std::move(p.first), second(std::move(p.second)) {} // NOLINT

	// Convert to std::pair
	operator std::pair<A,B>() const { return {first, second}; }

	bool operator==(const Pair& o) const { return first == o.first && second == o.second; }
	bool operator!=(const Pair& o) const { return !(*this == o); }
	bool operator<(const Pair& o) const {
		return first < o.first || (!(o.first < first) && second < o.second);
	}
	bool operator<=(const Pair& o) const { return !(o < *this); }
	bool operator>(const Pair& o) const { return o < *this; }
	bool operator>=(const Pair& o) const { return !(*this < o); }
};

template <typename A, typename B>
Pair<A,B> make_pair(A&& a, B&& b) {
	return {std::forward<A>(a), std::forward<B>(b)};
}

// Alias to avoid ambiguity with std::make_pair when both are in scope
template <typename A, typename B>
Pair<A,B>make-pair2(A&& a, B&& b) {
	return {std::forward<A>(a), std::forward<B>(b)};
}

} // namespace utils


// --- Structured binding support (std::tuple_size / tuple_element) ------------
namespace std {

template<typename A, typename B>
struct tuple_size<utils::Pair<A,B>> : integral_constant<size_t, 2> {};
 
template<size_t I, typename A, typename B>
struct tuple_element<I, utils::Pair<A,B>>;
 
template<typename A, typename B>
struct tuple_element<0, utils::Pair<A,B>> { using type = A; };
 
template<typename A, typename B>
struct tuple_element<1, utils::Pair<A,B>> { using type = B; };
 
}

// get<I>() for structured bindings
namespace utils {
 
template<std::size_t I, typename A, typename B>
auto& get(Pair<A,B>& p)
{
    if constexpr (I == 0) return p.first;
    else                   return p.second;
}
template<std::size_t I, typename A, typename B>
const auto& get(const Pair<A,B>& p)
{
    if constexpr (I == 0) return p.first;
    else                   return p.second;
}
 
} // namespace utils
