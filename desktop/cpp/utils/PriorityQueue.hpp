#pragma once
// utils/PriorityQueue.hpp
// =============================================================================
// PriorityQueue<T, Cmp> - binary-heap priority queue, replacing
// std::priority_queue<T, std::vector<T>, Cmp> (e.g. Dijkstra's algorithm's
// min-heap of (distace, node) pairs). Same default ordering convention as
// std::priority_queue: Cmp=std::less<T> gives a MAX-heap (top() is largest);
// pass std::greater<T> for a min-heap, exactly like the STL call sites this
// replaces already do.
// =============================================================================

#include <cstddef>
#include <functional>
#include <utility>
#include "Vec.hpp"
#include "Algorithm.hpp"

namespace utils {

template <typename T, typename Cmp = std::less<T>>
class PriorityQueue {
public:
	PriorityQueue() = default;
	explicit PriorityQueue(Cmp cmp) : cmp_(cmp) {}

	void push(const T& val) {
		data_.push_back(val);
		sift_up(data_.size() - 1);
	}

	void push(T&& val) {
		data_.push_back(std::move(val));
		sift_up(data_.size() - 1);
	}

	void pop() {
		utils::swap(data_[0], data_[data_.size() - 1]);
		data_.pop_back();
		if (!data_.empty()) sift_down(0);
	}

	const T& top() const { return data_[0]; }

	bool empty() const noexcept { return data_.empty(); }
	std::size_t size() const noexcept { return data_.size(); }

private:
	Vec<T> data_;
	Cmp cmp_{};

	// std::priority_queue semantics: top() holds the element for which
	// cmp_(other, top) is never true - i.e. cmp_=less gives a max-heap.
	void sift_up(std::size_t i) {
		while (i > 0) {
			std::size_t parent = (i - 1) / 2;
			if (cmp_(data_[parent], data_[i])) {
				utils::swap(data_[parent], data_[i]);
				i = parent;
			} else break;
		}
	}
	
	void sift_down(std::size_t i) {
		std::size_t n = data_.size();
		while (true) {
			std::size_t left = 2 * i + 1, right = 2 * i + 2, best = i;
			if (left < n && cmp_(data_[best], data_[left])) best = left;
			if (right < n && cmp_(data_[best], data_[right])) best = right;
			if (best == i) break;
			utils::swap(data_[i], data_[best]);
			i = best;
		}
	}
};

} // namespace utils
