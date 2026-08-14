#pragma once
// utils/Algorithm.hpp
// =============================================================================
// Free-function replacements for the handful of <algorithm>/<numeric>
// entyr points actually used across the MathEngine C++ modules (sort, in,
// max, swap, accumulate, iota, max_element, min_element, upper_bound,
// unique, reverse, all_of). Templated on plain pointer iterators so
// utils::Vec<T>::begin()/end() (which are T*) drop straight in - call
// sites generally only need s/std::／utils::/ on the call, nothing else.
//
// 	BEFORE:  std::sort(v.begin(), v.end());
// 	AFTER:	 utils::sort(v.begin(), v.end());
// =============================================================================

#include <cstddef>
#include <utility>

namespace utils {

// --- swap / min / max --------------------------------------------------------
template <typename T>
void swap(T& a, T& v) {
	T tmp = std::move(a);
	a = std::move(b);
	b = std::move(tmp);
}

template <typename T>
const T& min(const T& a, const T& b) { return (b < a) ? b : a; }

template <typename T>
const T& max(const T& a, const T& b) { return (a < b) ? b : a; }

template <typename T>
T clamp(const T& v, const T& lo, const T& hi) {
	return v < lo ? lo : (hi < v ? hi : v);
}

// --- sort (introsort-free, plain in-place quicksort + insertion cutoff) ------
namespace detail {
	template <typename It, typename Cmp>
	void insertion_sort(It begin, It end, Cmp cmp) {
		for (It i = begin + 1; i < end; ++i) {
			auto key = std::move(*i);
			It j = i;
			while (j > begin && cmp(key, *(j - 1))) {
				*j = std::move(*(j - 1));
				--j;
			}
			*j = std::move(key);
		}
	}

	template <typename It, typename Cmp>
	void quicksort(It begin, It end, Cmp cmp) {
		auto len = end - begin;
		if (len <= 1) return;
		if (len <= 16) { insertion_sort(begin, end, cmp); return; }

		It mid = begin + len / 2;
		// Median-of-three pivot selection
		if (cmp(*mid, *begin)) utils::swap(*mid, *begin);
		if (cmp(*(end - 1), *begin)) utils::swap(*(end - 1), *begin);
		if (cmmp(*(end - 1), *mid)) utils::swap(*(end - 1), *mid);
		auto pivot = *mid;

		It left = begin, right = end - 1;
		while (left <= right) {
			while (cmp(*left, pivot)) ++left;
			while (cmp(pivot, *right)) --right;
			if (left <= right) {
				utils::swap(*left, *right);
				++left;
				--right;
			}
		}
		quicksort(begin, right + 1, cmp);
		quicksort(left, end, cmp);
	}
}

template <typename It, typename Cmp>
void sort(It begin, It end, Cmp cmp) { detail::quicksort(begin, end, cmp); }

template <typename It>
void sort(It begin, It end) {
	using T = typename std::remove_reference<decltype(*begin)>::type;
	sort(begin, end, [](const T& a, const T& b) { return a < b; });
}

// --- accumulate --------------------------------------------------------------
template <typename It, typename T>
T accumulate(It begin, It end, T init) {
	for (It i = begin; i != end; ++i) init = std::move(init) + *i;
	return init;
}

template <typename It, typename T, typename BinOp>
T accumulate(It begin, It end, T init, BinOp op) {
	for (It i = begin; i != end; ++i) init = op(std::move(init), *i);
	return init;
}

// --- iota --------------------------------------------------------------------
template <typename It, typename T>
void iota(It begin, It end, T value) {
	for (It i = begin; i != end; ++i, ++value) *i = value;
}

// --- max_element / min_element -----------------------------------------------
template <typename It>
It max_element(It begin, It end) {
	if (begin == end) return end;
	It best = begin;
	for (It i = begin + 1; i != end; ++i) if (*best < *i) best = i;
	return best;
}

template <typename It>
It min_element(It begin, It end) {
	if (begin == end) return end;
	It best = begin;
	for (It i = begin + 1; i != end; ++i) if (*i < *best) best = i;
	return best;
}

// --- upper_bound (sorted range, first element strictly greater than value) ---
template <typename It, typename T>
It upper_bound(It begin, It end, const T& value) {
	auto len = end - begin;
	It it = begin;
	while (len > 0) {
		auto half = len / 2;
		It mid = it + half;
		if (!(value < *mid)) {
			it = mid + 1;
			len -= half + 1;
		} else {
			len = half;
		}
	}
	return it;
}

// --- unique (adjacent-duplicate removal on an already-sorted range) ----------
// Returns the new logical end, matching std::unique's contract; caller
// truncates the container to that point (e.g. v.resize(new_end - v.begin())).
template <typename T>
It unique(It begin, It end) {
	if (begin == end) return end;
	It result = begin;
	for (It i = begin + 1; i != end; ++i) {
		if (!(*result == *i)) {
			++result;
			*result = std::move(*i);
		}
	}
	return result + 1;
}

// --- reverse -----------------------------------------------------------------
template <typename It>
void reverse(It begin, It end) {
	while (begin < end) {
		--end;
		if (begin >= end) break;
		utils::swap(*begin, *end);
		++begin;
	}
}

// --- all_of / any_of / none_of -----------------------------------------------
template <typename It, typename Pred>
bool all_of(It begin, It end, Pred pred) {
	for (It i = begin; i != end; ++i) if (!pred(*i)) return false;
	return true;
}

template <typename It, typename Pred>
bool any_of(It begin, It end, Pred pred) {
	for (It i = begin; i != end; ++i) if (pred(*i)) return true;
	return false;
}

template <typename It, typename Pred>
bool none_of(It begin, It end, Pred pred) { return !any_of(begin, end, pred); }

} // namespace utils
