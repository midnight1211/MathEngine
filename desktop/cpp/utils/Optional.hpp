#pragma once
// utils/Optional.hpp — replaces std::optional<T>
#include <stdexcept>
#include <utility>

namespace utils {

template<typename T>
class Optional {
public:
    Optional() noexcept : has_(false) {}
    Optional(const T& v) : has_(true)  { new (&buf_) T(v); }       // NOLINT
    Optional(T&& v)      : has_(true)  { new (&buf_) T(std::move(v)); } // NOLINT

    Optional(const Optional& o) : has_(o.has_) { if (has_) new (&buf_) T(o.val()); }
    Optional(Optional&& o) noexcept : has_(o.has_)
    { if (has_) { new (&buf_) T(std::move(o.val())); o.reset(); } }

    Optional& operator=(const Optional& o)
    { if (this != &o) { reset(); if (o.has_) { new (&buf_) T(o.val()); has_ = true; } } return *this; }
    Optional& operator=(Optional&& o) noexcept
    { if (this != &o) { reset(); if (o.has_) { new (&buf_) T(std::move(o.val())); has_ = true; o.reset(); } } return *this; }

    ~Optional() { reset(); }

    bool has_value() const noexcept { return has_; }
    explicit operator bool() const noexcept { return has_; }

    T&       value()       { if (!has_) throw std::bad_optional_access{}; return val(); }
    const T& value() const { if (!has_) throw std::bad_optional_access{}; return val(); }
    T& operator*()        noexcept { return val(); }
    const T& operator*()  const noexcept { return val(); }
    T* operator->()       noexcept { return &val(); }
    const T* operator->() const noexcept { return &val(); }

    template<typename U>
    T value_or(U&& def) const { return has_ ? val() : static_cast<T>(std::forward<U>(def)); }

    void reset() noexcept { if (has_) { val().~T(); has_ = false; } }

    template<typename... Args>
    T& emplace(Args&&... args)
    { reset(); new (&buf_) T(std::forward<Args>(args)...); has_ = true; return val(); }

    bool operator==(const Optional& o) const
    { if (has_ != o.has_) return false; return !has_ || val() == o.val(); }
    bool operator!=(const Optional& o) const { return !(*this == o); }

    // Assign nullopt to reset
    struct NullOptTag {};
    Optional& operator=(NullOptTag) noexcept { reset(); return *this; }

private:
    alignas(T) unsigned char buf_[sizeof(T)];
    bool has_;

    T&       val()       noexcept { return *reinterpret_cast<T*>(&buf_); }
    const T& val() const noexcept { return *reinterpret_cast<const T*>(&buf_); }
};

template<typename T>
Optional<T> make_optional(T&& v) { return Optional<T>(std::forward<T>(v)); }

// Sentinel nullopt type
struct NullOpt {};
inline constexpr NullOpt nullopt{};

} // namespace utils
