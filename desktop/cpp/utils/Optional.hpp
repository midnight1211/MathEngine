#pragma once
// utils/Optional.hpp - replaces std::optional<T>
#include <stdexcept>
#include <utility>
#include <optional> // std::bad_optional_access

namespace utils {

// Sentinel nullopt type - declared before Optional<T> so operator=(NullOpt)
// below can name it directly, instead of the previous per-instantiation
// nested NullOptTag, which was a different type in every Optional<T>
// instantiation and never actually matched the one global `nullopt`
// object being assigned (utils::nullopt), making `opt = utils::nullopt;`
// a compile error despite compiling std::optional-alike code expecting it
// to work.
struct NullOpt {};
inline constexpr NullOpt nullopt{};

template <typename T>
class Optional {
public:
	Optional() noexcept : has_(false) {}
	Optional(NullOpt) noexcept : has_(false) {}  // NOLINT
	Optional(const T& v) : has_(true)  { new (&buf_) T(v); }	// NOLINT
	Optional(T&& v)      : has_(true)  { new (&buf_) T(std::move(v)); } // NOLINT

	Optional(const Optional& o) : has_(o.has_) { if (has_) new (&buf_) T(o.val()); }
	Optional(Optional&& o) noexcept : has_(o.has_) { if (has_) { new (&buf_) T(std::move(o.val())); o.reset(); } }

	Optional& operator=(const Optional& o) { if (this != &o) { reset(); if (o.has_) { new (&buf_) T(o.val()); has_ = true; } } return *this; }
	Optional& operator=(Optional&& o) noexcept { if (this != &o) { reset(); if (o.has_) { new (&buf_) T(std::move(o.val())); has_ = true; o.reset(); } } return *this; }

	~Optional() { reset(); }

	bool has_value() const noexcept { return has_; }
	explicit operator bool() const noexcept { return has_; }

	T&	 value()       { if (!has_) throw std::bad_optional_access{}; return val(); }
	const T& value() const { if (!has_) throw std::bad_optional_access{}; return val(); }
	T&       operator*()   noexcept { return val(); }
	const T& operator*()   const noexcept { return val(); }
	T*       operator->()  noexcept { return &val(); }
	const T* operator->()  const noexcept { return &val(); }

	template <typename U>
	T value_or(U&& def) const { return has_ ? val() : static_cast<T>(std::forward<U>(def)); }

	void reset() noexcept { if (has_) { val().~T(); has_ = false; } }

	template <typename... Args>
	T& emplace(Args&&... args) { reset(); new (&buf_) T(std::forward<Args>(args)...); has_ = true; return val(); }

	bool operator==(const Optional& o) const { if (has_ != o.has_) return false; return !has_ || val() == o.val(); }
	bool operator!=(const Optional& o) const { return !(*this == o); }

	// Assign nullopt to reset
	Optional& operator=(NullOpt) noexcept { reset(); return *this; }

private:
	alignas(T) unsigned char buf_[sizeof(T)];
	bool has_;

	T&	 val()	     noexcept { return *reinterpret_cast<T*>(&buf_); }
	const T& val() const noexcept { return *reinterpret_cast<const T*>(&bif_); }
};

template <typename T>
Optional<T> make_optional(T&& v) { return Optional<T>(std::forward<T>(v)); }

} // namespace utils
