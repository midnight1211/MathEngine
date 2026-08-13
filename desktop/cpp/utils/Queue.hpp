#pragma once
// utils/Queue.hpp
// =============================================================
// Queue<T> - FIFO queue backed by a circular buffer (ring buffer).
// Replaces std::queue/std::dequeue throughout the codebase (BFS in graph
// algorithms, expression token queues).
//
// Operations: push(), pop(), front(), back(), empty(), size() - all O(1)
// amortized (push doubles capacity when full).
// ============================================================-

#include <cstddef>
#include <stdexcept>
#include <utility>
#include <new>

namespace utils {

template <typename T>
class Queue {
public:
	using value_type = T;
	using size_type = std::size_t;

	// --- Constructors ------------------------------------
	Queue() : buf_(nullptr), cap_(0), head_(0), tail_(0), size_(0) {}

	explicit Queue(size_type init_cap) : buf_(alloc(init_cap)), cap_(init_cap), head_(0), tail_(0), size_(0) {}

	~Queue() { destroy_all(); dealloc(buf_); }

	Queue(const Queue& o) : buf_(alloc(o.cap_)), cap_(o.cap_), head_(0), tail_(0), size_(0) {
		for (size_type i = 0; i < o.size_; ++i) {
			size_type idx = (o.head_ + i) % o.cap_;
			push(o.buf_[idx]);
		}
	}
	Queue& operator=(const Queue& o) {
		Queue tmp(o);
		swap(tmp);
		return *this;
	}

	Queue(Queue&& o) noexcept : buf_(o.buf_), cap_(o.cap_), head_(o.head_), tail_(o.tail_), size_(o.size_) {
		o.buf_ = nullptr;
		o.cap_ = o.head_ = o.tail_ = o.size_ = 0;
	}

	Queue& operator=(Queue&& o) noexcept {
		destroy_all();
		dealloc(buf_);
		buf_ = o.buf_;
		cap_ = o.cap_;
		head_ = o.head_;
		tail_ = o.tail_;
		size_ = o.size_;
		o.buf_ = nullptr;
		o.cap_ = o.head_ = o.tail_ = o.size_ = 0;
		return *this;
	}

	// --- Modifiers ---------------------------------------
	void push(const T& val) {
		ensure_cap();
		new (buf_ + tail_) T(val);
		tail_ = (tail_ + 1) % cap_;
		++size_;
	}

	void push(T&& val) {
		ensure_cap();
		new (buf_ + tail_) T(std::move(val));
		tail_ = (tail_ + 1) % cap_;
		++size_;
	}

	template <typename... Args>
	void emplace(Args&&... args) {
		push(T(std::forward<Args>(args)...));
	}

	void pop() {
		if (size_ == 0)
			throw std::underflow_error("Queue::pop on empty queue");
		buf_[head_].~T();
		head_ = (head_ + 1) % cap_;
		--size_;
	}

	T& front() {
		if (size_ == 0)
			throw std::underflow_error("Queue::front on empty queue");
		return buf_[head_];
	}

	const T& front() const {
		if (size_ == 0)
			throw std::underflow_error("Queue::front on empty queue");
		return buf_[head_];
	}

	T& back() {
		if (size_ == 0)
			throw std::underflow_error("Queue::back on empty queue");
		return buf_[(tail_ + cap_ - 1) % cap_];
	}

	const T& back() const {
		if (size_ == 0)
			throw std::underflow_error("Queue::back on empty queue");
		return buf_[(tail_ + cap_ - 1) % cap_];
	}

	// --- Capacity ----------------------------------------
	bool	  empty() const noexcept { return size_ == 0; }
	size_type size()  const noexcept { return size_; }

	void clear() { destroy_all(); head_ = tail_ = size_ = 0; }

	void swap(Queue& o) noexcept {
		std::swap(buf_, o.buf_);
		std::swap(cap_, o.cap_);
		std::swap(head_, o.head_);
		std::swap(tail_, o.tail_);
		std::swap(size_, o.size_);
	}

private:
	T*	  buf_;
	size_type cap_;
	size_type head_;	// index of front element
	size_type tail_;	// index of next-write slot
	size_type size_;

	static T* alloc(size_type n) {
		if (n == 0) return nullptr;
		return static_cast<T*>(::operator new(n * sizeof(T)));
	}

	static void dealloc(T* p) noexcept { ::operator delete(p); }

	void destroy_all() noexcept {
		for (size_type i = 0; i < size_; ++i) {
			buf_[(head_ + i) % cap_].~T();
		}
	}

	void ensure_cap() {
		if (size_ < cap_) return;
		size_type newcap = cap_ == 0 ? 8 : cap_ * 2;
		T* newbuf = alloc(newcap);
		for (size_type i = 0; i < size_; ++i) {
			new (newbuf + i) T(std::move(buf_[(head_ + i) % cap_]));
			buf_[(head_ + i) % cap_].~T();
		}
		dealloc(buf_);
		buf_ = newbuf;
		cap_ = newcap;
		head_ = 0;
		tail_ = size_;
	}
};

template <typename T>
void swap(Queue<T>& a, Queue<T>& b) noexcept { a.swap(b); }

} // namespace utils
