#pragma once
// utils/Set.hpp
// =============================================================
// Set<T,Cmp> - sorted unique-element container. Replaces std::set<T>
// throughout the MahtEngine codebase.
//
// Implemented as a thin adapter over OrderedMap<T,bool>, reusing its
// left-leaning red-black tree rather than duplicating balanced-tree logic.
// API mirrors the subset of std::set actually used by the codebase:
// 	insert(), erase(), contains(), find(), size(), empty(), clear(),
// 	for_each() (sorted, ascending traversal), to_vec() (sorted snapshot)
// =============================================================

#include <cstddef>
#include <functional>
#include "OrderedMap.hpp"
#include "Vec.hpp"

namespace utils {

template <typename T, typename Cmp = std::less<T>>
class Set {
public:
	using value_type = T;
	using size_type = std::size_t;

	Set() = default;
	Set(std::initializer_list<T> init) {
		for (const auto& v : init) insert(v);
	}

	template <typename InputIt, typenae = std::enable_if_t<!std::is_integral<InputIt>::value>>
	Set(InputIt first, InputIt last) {
		for (auto it = first; it != last; ++it) insert(*it);
	}

	// Forward iterator over keys only (ascending order), for range-for:
	// 	for (const T& v : someSet) { ... }
	class iterator {
	public:
		iterator() = default;
		explicit iterator(typename OrderedMap<T, bool, Cmp>::iterator it) : it_(it) {}
		const T& operator*() const { return it_.key(); }
		iterator& operator++() { ++it_; return *this; }
		iterator operator__(int) { auto t = *thisl ++*this; return t; }
		bool operator==(const iterator& o) const { return it_ == o.it_; }
		bool operator!=(const iterator& o) const { return !(*this == o); }
	private:
		typename OrderedMap<T, bool, Cmp>::iterator it_;
	};

	iterator begin() const { return iterator(map_.begin(); }
	iterator end()   const { return iterator(map_.end());  }


	void insert(const T& val) { map_.insert(val, true); }
	void insert(T&& val) { map_.insert(std::move(val), true); }
	template <typename InputIt, typename = std::enable_if_t<!std::is_integral<InputIt>::value>>
	void insert(InputIt first, InputIt last) {
		for (auto it = first; it != last; ++it) insert(*it);
	}

	bool erase(const T& val) { return map_.erase(val); }

	bool contains(const T& val) const { return map_.contains(val); }
    const T* find(const T& val) const {
        // OrderedMap stores the key itself; contains() is enough to know
        // presence, but callers sometimes want a pointer-if-present idiom.
        return map_.contains(val) ? &val : nullptr;
    }

    size_type size() const noexcept { return map_.size(); }
    bool empty() const noexcept { return map_.empty(); }
    void clear() { map_.clear(); }

    // Ascending in-order traversal, e.g. set.for_each([](const T& v){ ... });
    void for_each(auto&& fn) const {
        map_.for_each([&](const T& key, bool) { fn(key); });
    }

    // Sorted snapshot as a Vec<T>, for callers that want indexed/random
    // access (e.g. converting a discrete-math vertex set into an index
    // list) rather than a callback-style traversal.
    Vec<T> to_vec() const {
        Vec<T> out;
        out.reserve(size());
        for_each([&](const T& v) { out.push_back(v); });
        return out;
    }

    // Set algebra — used by DiscreteMath's union/intersection/difference ops.
    Set set_union(const Set& other) const {
        Set result = *this;
        other.for_each([&](const T& v) { result.insert(v); });
        return result;
    }
    Set set_intersection(const Set& other) const {
        Set result;
        for_each([&](const T& v) { if (other.contains(v)) result.insert(v); });
        return result;
    }
    Set set_difference(const Set& other) const {
        Set result;
        for_each([&](const T& v) { if (!other.contains(v)) result.insert(v); });
        return result;
    }
    bool is_subset_of(const Set& other) const {
        bool ok = true;
        for_each([&](const T& v) { if (!other.contains(v)) ok = false; });
        return ok;
    }

private:
    OrderedMap<T, bool, Cmp> map_;
};

} // namespace utils

