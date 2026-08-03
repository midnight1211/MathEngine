#pragma once
// utils/HashMap.hpp
// =============================================================================
// HashMap<K,V> — open-addressing, robin-hood probing hash map.
// Replaces std::unordered_map throughout the codebase.
//
// Supported operations:
//   insert(key, value)          — O(1) amortized
//   operator[](key)             — insert-or-lookup (like std::unordered_map)
//   find(key)  → pointer or nullptr
//   contains(key)
//   erase(key)
//   size(), empty(), clear()
//   begin() / end()  (forward iteration over occupied slots)
//   Keys must be copy-constructible, equality-comparable, and hashable.
//
// Default hash: uses std::hash<K>. Specialise utils::Hash<K> for custom types.
// Load factor is kept ≤ 0.7; table size is always a power of two.
// =============================================================================

#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <functional>
#include <string>
#include <utility>
#include <initializer_list>

namespace utils {

// ── Customisation point: Hash<K> ─────────────────────────────────────────────
template<typename K>
struct Hash {
    std::size_t operator()(const K& k) const noexcept
    { return std::hash<K>{}(k); }
};

// Specialise for std::pair<A,B> (used in discrete-math multiplicity maps)
template<typename A, typename B>
struct Hash<std::pair<A,B>> {
    std::size_t operator()(const std::pair<A,B>& p) const noexcept
    {
        std::size_t h1 = Hash<A>{}(p.first);
        std::size_t h2 = Hash<B>{}(p.second);
        return h1 ^ (h2 * 0x9e3779b97f4a7c15ULL + (h1 << 6) + (h1 >> 2));
    }
};

// ── HashMap ──────────────────────────────────────────────────────────────────
template<typename K, typename V, typename H = Hash<K>>
class HashMap {
    static constexpr std::size_t MIN_CAP = 8;
    static constexpr float       MAX_LOAD = 0.7f;

    enum class State : std::uint8_t { EMPTY, OCCUPIED, DELETED };

    struct Slot {
        K     key;
        V     val;
        State state = State::EMPTY;
    };

public:
    // ── Types ─────────────────────────────────────────────────────────────────
    using key_type    = K;
    using mapped_type = V;
    using value_type  = std::pair<const K, V>;
    using size_type   = std::size_t;

    // ── Forward iterator ──────────────────────────────────────────────────────
    class iterator {
    public:
        using difference_type   = std::ptrdiff_t;
        using value_type        = std::pair<const K&, V&>;

        iterator(Slot* p, Slot* end) : cur_(p), end_(end) { advance(); }

        std::pair<const K&, V&> operator*()  const { return {cur_->key, cur_->val}; }
        iterator& operator++() { ++cur_; advance(); return *this; }
        iterator  operator++(int) { auto tmp = *this; ++*this; return tmp; }
        bool operator==(const iterator& o) const { return cur_ == o.cur_; }
        bool operator!=(const iterator& o) const { return cur_ != o.cur_; }

        const K& key() const { return cur_->key; }
        V&       val() const { return cur_->val; }

    private:
        Slot *cur_, *end_;
        void advance() { while (cur_ != end_ && cur_->state != State::OCCUPIED) ++cur_; }
    };

    class const_iterator {
    public:
        const_iterator(const Slot* p, const Slot* end) : cur_(p), end_(end) { advance(); }

        std::pair<const K&, const V&> operator*()  const { return {cur_->key, cur_->val}; }
        const_iterator& operator++() { ++cur_; advance(); return *this; }
        const_iterator  operator++(int) { auto t = *this; ++*this; return t; }
        bool operator==(const const_iterator& o) const { return cur_ == o.cur_; }
        bool operator!=(const const_iterator& o) const { return cur_ != o.cur_; }

        const K& key() const { return cur_->key; }
        const V& val() const { return cur_->val; }

    private:
        const Slot *cur_, *end_;
        void advance() { while (cur_ != end_ && cur_->state != State::OCCUPIED) ++cur_; }
    };

    // ── Constructors ──────────────────────────────────────────────────────────
    HashMap() : slots_(new Slot[MIN_CAP]), cap_(MIN_CAP), size_(0) {}

    explicit HashMap(size_type hint)
        : cap_(next_pow2(static_cast<size_type>(hint / MAX_LOAD) + 1)),
          slots_(new Slot[cap_]), size_(0)
    {}

    HashMap(std::initializer_list<std::pair<K, V>> il) : HashMap()
    { for (auto& [k, v] : il) insert(k, v); }

    ~HashMap() { delete[] slots_; }

    HashMap(const HashMap& o) : cap_(o.cap_), size_(0), slots_(new Slot[o.cap_])
    { for (size_type i = 0; i < cap_; ++i) slots_[i] = o.slots_[i]; size_ = o.size_; }

    HashMap& operator=(const HashMap& o)
    {
        if (this == &o) return *this;
        HashMap tmp(o);
        std::swap(slots_, tmp.slots_);
        std::swap(cap_,   tmp.cap_);
        std::swap(size_,  tmp.size_);
        return *this;
    }

    HashMap(HashMap&& o) noexcept
        : slots_(o.slots_), cap_(o.cap_), size_(o.size_)
    { o.slots_ = nullptr; o.cap_ = o.size_ = 0; }

    HashMap& operator=(HashMap&& o) noexcept
    {
        if (this == &o) return *this;
        delete[] slots_;
        slots_ = o.slots_; cap_ = o.cap_; size_ = o.size_;
        o.slots_ = nullptr; o.cap_ = o.size_ = 0;
        return *this;
    }

    // ── Capacity ──────────────────────────────────────────────────────────────
    size_type size()  const noexcept { return size_; }
    bool      empty() const noexcept { return size_ == 0; }

    // ── Lookup ────────────────────────────────────────────────────────────────
    V* find(const K& key)
    {
        size_type idx = probe(key);
        if (slots_[idx].state == State::OCCUPIED) return &slots_[idx].val;
        return nullptr;
    }
    const V* find(const K& key) const
    {
        size_type idx = probe(key);
        if (slots_[idx].state == State::OCCUPIED) return &slots_[idx].val;
        return nullptr;
    }

    bool contains(const K& key) const { return find(key) != nullptr; }

    // operator[] — insert default value if absent (like std::unordered_map)
    V& operator[](const K& key)
    {
        maybe_rehash();
        size_type idx = probe(key);
        if (slots_[idx].state != State::OCCUPIED) {
            slots_[idx].key   = key;
            slots_[idx].val   = V();
            slots_[idx].state = State::OCCUPIED;
            ++size_;
        }
        return slots_[idx].val;
    }

    V& at(const K& key)
    {
        V* p = find(key);
        if (!p) throw std::out_of_range("HashMap::at: key not found");
        return *p;
    }
    const V& at(const K& key) const
    {
        const V* p = find(key);
        if (!p) throw std::out_of_range("HashMap::at: key not found");
        return *p;
    }

    // ── Insert / emplace ──────────────────────────────────────────────────────
    // Returns {reference to value, inserted?}
    std::pair<V*, bool> insert(const K& key, const V& val)
    {
        maybe_rehash();
        size_type idx = probe(key);
        if (slots_[idx].state == State::OCCUPIED)
            return {&slots_[idx].val, false};
        slots_[idx] = {key, val, State::OCCUPIED};
        ++size_;
        return {&slots_[idx].val, true};
    }

    std::pair<V*, bool> insert_or_assign(const K& key, const V& val)
    {
        maybe_rehash();
        size_type idx = probe(key);
        bool existed  = (slots_[idx].state == State::OCCUPIED);
        slots_[idx]   = {key, val, State::OCCUPIED};
        if (!existed) ++size_;
        return {&slots_[idx].val, !existed};
    }

    // ── Erase ─────────────────────────────────────────────────────────────────
    bool erase(const K& key)
    {
        size_type idx = probe(key);
        if (slots_[idx].state != State::OCCUPIED) return false;
        slots_[idx].state = State::DELETED;
        --size_;
        return true;
    }

    void clear()
    {
        for (size_type i = 0; i < cap_; ++i) slots_[i].state = State::EMPTY;
        size_ = 0;
    }

    // ── Iteration ─────────────────────────────────────────────────────────────
    iterator       begin()        { return {slots_, slots_ + cap_}; }
    iterator       end()          { return {slots_ + cap_, slots_ + cap_}; }
    const_iterator begin()  const { return {slots_, slots_ + cap_}; }
    const_iterator end()    const { return {slots_ + cap_, slots_ + cap_}; }
    const_iterator cbegin() const { return begin(); }
    const_iterator cend()   const { return end(); }

private:
    Slot*     slots_;
    size_type cap_;
    size_type size_;
    H         hasher_;

    static size_type next_pow2(size_type n) noexcept
    {
        if (n <= MIN_CAP) return MIN_CAP;
        --n;
        n |= n >> 1; n |= n >> 2; n |= n >> 4;
        n |= n >> 8; n |= n >> 16; n |= n >> 32;
        return ++n;
    }

    size_type probe(const K& key) const
    {
        size_type idx = hasher_(key) & (cap_ - 1);
        while (true) {
            if (slots_[idx].state == State::EMPTY) return idx;
            if (slots_[idx].state == State::OCCUPIED && slots_[idx].key == key) return idx;
            idx = (idx + 1) & (cap_ - 1);
        }
    }

    void maybe_rehash()
    {
        if (static_cast<float>(size_ + 1) / cap_ <= MAX_LOAD) return;
        rehash(cap_ * 2);
    }

    void rehash(size_type newcap)
    {
        Slot* old    = slots_;
        size_type oc = cap_;
        slots_       = new Slot[newcap];
        cap_         = newcap;
        size_        = 0;
        for (size_type i = 0; i < oc; ++i)
            if (old[i].state == State::OCCUPIED)
                insert(old[i].key, old[i].val);
        delete[] old;
    }
};

// Convenience alias used in the codebase for symbol tables
using SymbolTable = HashMap<std::string, double>;

} // namespace utils
