#pragma once
// utils/Stack.hpp
// =============================================================================
// Stack<T> — LIFO stack backed by a singly-linked list.
// Replaces std::stack throughout the codebase (graph DFS, expression parsers).
//
// Operations: push(), pop(), top(), empty(), size() — all O(1).
// No random access. Rule-of-five provided.
// =============================================================================

#include <cstddef>
#include <stdexcept>
#include <utility>
#include <initializer_list>

namespace utils {

template<typename T>
class Stack {
    struct Node {
        T     val;
        Node* next = nullptr;
        explicit Node(const T& v) : val(v) {}
        explicit Node(T&& v) : val(std::move(v)) {}
    };

public:
    using value_type = T;
    using size_type  = std::size_t;

    Stack() noexcept : head_(nullptr), size_(0) {}

    Stack(std::initializer_list<T> il) : Stack()
    { for (const T& v : il) push(v); }

    ~Stack() { while (head_) pop(); }

    Stack(const Stack& o) : Stack()
    {
        // Copy in reverse so the top of o is still the top of *this
        Node* arr[o.size_];   // VLA-style — bounded by user
        Node* cur = o.head_;
        std::size_t n = 0;
        while (cur) { arr[n++] = cur; cur = cur->next; }
        for (std::size_t i = n; i-- > 0;) push(arr[i]->val);
    }

    Stack& operator=(const Stack& o)
    { Stack tmp(o); swap(tmp); return *this; }

    Stack(Stack&& o) noexcept : head_(o.head_), size_(o.size_)
    { o.head_ = nullptr; o.size_ = 0; }

    Stack& operator=(Stack&& o) noexcept
    {
        while (head_) pop();
        head_ = o.head_; size_ = o.size_;
        o.head_ = nullptr; o.size_ = 0;
        return *this;
    }

    // ── Operations ────────────────────────────────────────────────────────────
    void push(const T& val)
    {
        Node* n = new Node(val);
        n->next = head_;
        head_   = n;
        ++size_;
    }
    void push(T&& val)
    {
        Node* n = new Node(std::move(val));
        n->next = head_;
        head_   = n;
        ++size_;
    }

    template<typename... Args>
    void emplace(Args&&... args)
    { push(T(std::forward<Args>(args)...)); }

    void pop()
    {
        if (!head_) throw std::underflow_error("Stack::pop on empty stack");
        Node* tmp = head_;
        head_     = head_->next;
        delete tmp;
        --size_;
    }

    T& top()
    {
        if (!head_) throw std::underflow_error("Stack::top on empty stack");
        return head_->val;
    }
    const T& top() const
    {
        if (!head_) throw std::underflow_error("Stack::top on empty stack");
        return head_->val;
    }

    bool      empty() const noexcept { return size_ == 0; }
    size_type size()  const noexcept { return size_; }

    void swap(Stack& o) noexcept
    {
        std::swap(head_, o.head_);
        std::swap(size_, o.size_);
    }

private:
    Node*     head_;
    size_type size_;
};

template<typename T>
void swap(Stack<T>& a, Stack<T>& b) noexcept { a.swap(b); }

} // namespace utils
