#pragma once
// utils/Vec.hpp
// =============================================================================
// Vec<T> — dynamic array (heap-allocated, contiguous storage).
// Replaces std::vector<T> throughout the MathEngine codebase.
//
// Design goals:
//   • Identical iterator/range-for semantics as std::vector so existing code
//     needs no loop rewrites.
//   • All standard capacity/access methods: size, capacity, push_back,
//     pop_back, insert, erase, reserve, resize, clear, front, back, data.
//   • Strong exception guarantee on copy; no exception guarantees on moves
//     (same as std::vector).
//   • Rule-of-five: copy ctor, copy assign, move ctor, move assign, dtor.
// =============================================================================

#include <cstddef>
#include <cstring>
#include <stdexcept>
#include <initializer_list>
#include <utility>
#include <new>
#include <algorithm>
#include <iterator>
#include <type_traits>

namespace utils {

template<typename T>
class Vec {
public:
    // ── Types ─────────────────────────────────────────────────────────────────
    using value_type      = T;
    using size_type       = std::size_t;
    using difference_type = std::ptrdiff_t;
    using reference       = T&;
    using const_reference = const T&;
    using pointer         = T*;
    using const_pointer   = const T*;
    using iterator        = T*;
    using const_iterator  = const T*;

    // ── Constructors ──────────────────────────────────────────────────────────
    Vec() noexcept : data_(nullptr), size_(0), cap_(0) {}

    explicit Vec(size_type n)
        : data_(alloc(n)), size_(n), cap_(n)
    {
        for (size_type i = 0; i < n; ++i)
            new (data_ + i) T();
    }

    Vec(size_type n, const T& val)
        : data_(alloc(n)), size_(n), cap_(n)
    {
        for (size_type i = 0; i < n; ++i)
            new (data_ + i) T(val);
    }

    Vec(std::initializer_list<T> il)
        : data_(alloc(il.size())), size_(il.size()), cap_(il.size())
    {
        size_type i = 0;
        for (const T& v : il)
            new (data_ + i++) T(v);
    }

    template<typename InputIt,
             typename = std::enable_if_t<!std::is_integral<InputIt>::value>>
    Vec(InputIt first, InputIt last)
        : data_(nullptr), size_(0), cap_(0)
    {
        for (auto it = first; it != last; ++it)
            push_back(*it);
    }

    // Copy
    Vec(const Vec& o)
        : data_(alloc(o.size_)), size_(o.size_), cap_(o.size_)
    {
        for (size_type i = 0; i < size_; ++i)
            new (data_ + i) T(o.data_[i]);
    }

    Vec& operator=(const Vec& o)
    {
        if (this == &o) return *this;
        Vec tmp(o);
        swap(tmp);
        return *this;
    }

    // Move
    Vec(Vec&& o) noexcept
        : data_(o.data_), size_(o.size_), cap_(o.cap_)
    {
        o.data_ = nullptr;
        o.size_ = o.cap_ = 0;
    }

    Vec& operator=(Vec&& o) noexcept
    {
        if (this == &o) return *this;
        destroy_all();
        dealloc(data_);
        data_ = o.data_; size_ = o.size_; cap_ = o.cap_;
        o.data_ = nullptr; o.size_ = o.cap_ = 0;
        return *this;
    }

    ~Vec()
    {
        destroy_all();
        dealloc(data_);
    }

    // ── Capacity ──────────────────────────────────────────────────────────────
    size_type size()     const noexcept { return size_; }
    size_type capacity() const noexcept { return cap_;  }
    bool      empty()    const noexcept { return size_ == 0; }

    void reserve(size_type newcap)
    {
        if (newcap <= cap_) return;
        reallocate(newcap);
    }

    void resize(size_type n)
    {
        if (n < size_) {
            for (size_type i = n; i < size_; ++i) data_[i].~T();
            size_ = n;
        } else if (n > size_) {
            ensure_cap(n);
            for (size_type i = size_; i < n; ++i) new (data_ + i) T();
            size_ = n;
        }
    }

    void resize(size_type n, const T& val)
    {
        if (n < size_) {
            for (size_type i = n; i < size_; ++i) data_[i].~T();
            size_ = n;
        } else if (n > size_) {
            ensure_cap(n);
            for (size_type i = size_; i < n; ++i) new (data_ + i) T(val);
            size_ = n;
        }
    }

    void shrink_to_fit()
    {
        if (cap_ > size_) reallocate(size_);
    }

    // ── Element access ────────────────────────────────────────────────────────
    reference       operator[](size_type i)       noexcept { return data_[i]; }
    const_reference operator[](size_type i) const noexcept { return data_[i]; }

    reference at(size_type i)
    {
        if (i >= size_) throw std::out_of_range("Vec::at out of range");
        return data_[i];
    }
    const_reference at(size_type i) const
    {
        if (i >= size_) throw std::out_of_range("Vec::at out of range");
        return data_[i];
    }

    reference       front()       { return data_[0]; }
    const_reference front() const { return data_[0]; }
    reference       back()        { return data_[size_ - 1]; }
    const_reference back()  const { return data_[size_ - 1]; }
    pointer         data()        noexcept { return data_; }
    const_pointer   data()  const noexcept { return data_; }

    // ── Modifiers ─────────────────────────────────────────────────────────────
    void push_back(const T& val)
    {
        ensure_cap(size_ + 1);
        new (data_ + size_++) T(val);
    }

    void push_back(T&& val)
    {
        ensure_cap(size_ + 1);
        new (data_ + size_++) T(std::move(val));
    }

    template<typename... Args>
    reference emplace_back(Args&&... args)
    {
        ensure_cap(size_ + 1);
        new (data_ + size_) T(std::forward<Args>(args)...);
        return data_[size_++];
    }

    void pop_back()
    {
        if (size_ == 0) throw std::underflow_error("Vec::pop_back on empty");
        data_[--size_].~T();
    }

    void clear()
    {
        destroy_all();
        size_ = 0;
    }

    // Insert before position pos (returns iterator to inserted element)
    iterator insert(const_iterator pos, const T& val)
    {
        size_type idx = pos - data_;
        ensure_cap(size_ + 1);
        // Shift right
        for (size_type i = size_; i > idx; --i) {
            new (data_ + i) T(std::move(data_[i - 1]));
            data_[i - 1].~T();
        }
        new (data_ + idx) T(val);
        ++size_;
        return data_ + idx;
    }

    // Erase element at pos (returns iterator to next element)
    iterator erase(const_iterator pos)
    {
        size_type idx = pos - data_;
        data_[idx].~T();
        for (size_type i = idx; i + 1 < size_; ++i) {
            new (data_ + i) T(std::move(data_[i + 1]));
            data_[i + 1].~T();
        }
        --size_;
        return data_ + idx;
    }

    iterator erase(const_iterator first, const_iterator last)
    {
        size_type fidx = first - data_;
        size_type lidx = last  - data_;
        size_type count = lidx - fidx;
        for (size_type i = fidx; i < lidx; ++i) data_[i].~T();
        for (size_type i = lidx; i < size_; ++i) {
            new (data_ + i - count) T(std::move(data_[i]));
            data_[i].~T();
        }
        size_ -= count;
        return data_ + fidx;
    }

    void swap(Vec& o) noexcept
    {
        std::swap(data_, o.data_);
        std::swap(size_, o.size_);
        std::swap(cap_,  o.cap_);
    }

    // ── Iterators ─────────────────────────────────────────────────────────────
    iterator       begin()        noexcept { return data_; }
    const_iterator begin()  const noexcept { return data_; }
    const_iterator cbegin() const noexcept { return data_; }
    iterator       end()          noexcept { return data_ + size_; }
    const_iterator end()    const noexcept { return data_ + size_; }
    const_iterator cend()   const noexcept { return data_ + size_; }

    // Reverse iterators
    using reverse_iterator       = std::reverse_iterator<iterator>;
    using const_reverse_iterator = std::reverse_iterator<const_iterator>;
    reverse_iterator       rbegin()        noexcept { return reverse_iterator(end()); }
    const_reverse_iterator rbegin()  const noexcept { return const_reverse_iterator(end()); }
    reverse_iterator       rend()          noexcept { return reverse_iterator(begin()); }
    const_reverse_iterator rend()    const noexcept { return const_reverse_iterator(begin()); }

    // ── Comparison ────────────────────────────────────────────────────────────
    bool operator==(const Vec& o) const
    {
        if (size_ != o.size_) return false;
        for (size_type i = 0; i < size_; ++i)
            if (!(data_[i] == o.data_[i])) return false;
        return true;
    }
    bool operator!=(const Vec& o) const { return !(*this == o); }

    bool operator<(const Vec& o) const
    {
        return std::lexicographical_compare(begin(), end(), o.begin(), o.end());
    }

private:
    T*        data_;
    size_type size_;
    size_type cap_;

    static T* alloc(size_type n)
    {
        if (n == 0) return nullptr;
        void* p = ::operator new(n * sizeof(T));
        return static_cast<T*>(p);
    }

    static void dealloc(T* p) noexcept
    {
        ::operator delete(p);
    }

    void destroy_all() noexcept
    {
        for (size_type i = 0; i < size_; ++i)
            data_[i].~T();
    }

    void ensure_cap(size_type needed)
    {
        if (needed <= cap_) return;
        reallocate(std::max(needed, cap_ == 0 ? size_type(4) : cap_ * 2));
    }

    void reallocate(size_type newcap)
    {
        T* newdata = alloc(newcap);
        for (size_type i = 0; i < size_; ++i) {
            new (newdata + i) T(std::move(data_[i]));
            data_[i].~T();
        }
        dealloc(data_);
        data_ = newdata;
        cap_  = newcap;
    }
};

// Non-member swap
template<typename T>
void swap(Vec<T>& a, Vec<T>& b) noexcept { a.swap(b); }

// Convenience aliases (mirror the codebase's existing names)
using DVec = Vec<double>;
using IVec = Vec<long long>;
using SVec = Vec<std::string>;

} // namespace utils
