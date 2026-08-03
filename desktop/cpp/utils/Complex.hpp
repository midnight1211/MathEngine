#pragma once
// utils/Complex.hpp
// =============================================================================
// Complex — immutable-style complex number class replacing std::complex<double>.
//
// Provides all arithmetic, comparison, and math-function overloads that the
// codebase uses on std::complex<double>:
//   real(), imag(), abs(), arg(), norm(), conj()
//   +, -, *, /,  +=, -=, *=, /=
//   ==, !=
//   sqrt(), exp(), log(), sin(), cos(), tan() (free functions)
//   pow() (free function, integer and double exponents)
//
// Also provides an operator<< for debug output.
// =============================================================================

#include <cmath>
#include <stdexcept>
#include <string>
#include <sstream>
#include <iomanip>

namespace utils {

class Complex {
public:
    // ── Constructors ──────────────────────────────────────────────────────────
    constexpr Complex() noexcept : re_(0.0), im_(0.0) {}
    constexpr Complex(double re, double im = 0.0) noexcept : re_(re), im_(im) {}

    // ── Accessors ─────────────────────────────────────────────────────────────
    constexpr double real() const noexcept { return re_; }
    constexpr double imag() const noexcept { return im_; }

    void real(double v) noexcept { re_ = v; }
    void imag(double v) noexcept { im_ = v; }

    // ── Arithmetic (compound assignment) ──────────────────────────────────────
    Complex& operator+=(const Complex& o) noexcept
    { re_ += o.re_; im_ += o.im_; return *this; }

    Complex& operator-=(const Complex& o) noexcept
    { re_ -= o.re_; im_ -= o.im_; return *this; }

    Complex& operator*=(const Complex& o) noexcept
    {
        double r = re_ * o.re_ - im_ * o.im_;
        double i = re_ * o.im_ + im_ * o.re_;
        re_ = r; im_ = i;
        return *this;
    }

    Complex& operator/=(const Complex& o)
    {
        double denom = o.re_ * o.re_ + o.im_ * o.im_;
        if (denom == 0.0) throw std::domain_error("Complex division by zero");
        double r = (re_ * o.re_ + im_ * o.im_) / denom;
        double i = (im_ * o.re_ - re_ * o.im_) / denom;
        re_ = r; im_ = i;
        return *this;
    }

    // Scalar compound assignment
    Complex& operator+=(double v) noexcept { re_ += v; return *this; }
    Complex& operator-=(double v) noexcept { re_ -= v; return *this; }
    Complex& operator*=(double v) noexcept { re_ *= v; im_ *= v; return *this; }
    Complex& operator/=(double v)
    {
        if (v == 0.0) throw std::domain_error("Complex division by zero scalar");
        re_ /= v; im_ /= v;
        return *this;
    }

    // Unary
    Complex operator+() const noexcept { return *this; }
    Complex operator-() const noexcept { return {-re_, -im_}; }

    // ── Comparison ────────────────────────────────────────────────────────────
    bool operator==(const Complex& o) const noexcept
    { return re_ == o.re_ && im_ == o.im_; }

    bool operator!=(const Complex& o) const noexcept
    { return !(*this == o); }

    // ── Serialisation ─────────────────────────────────────────────────────────
    std::string to_string(int precision = 6) const
    {
        std::ostringstream ss;
        ss << std::fixed << std::setprecision(precision);
        if (im_ == 0.0) {
            ss << re_;
        } else if (re_ == 0.0) {
            ss << im_ << "i";
        } else if (im_ < 0.0) {
            ss << re_ << " - " << (-im_) << "i";
        } else {
            ss << re_ << " + " << im_ << "i";
        }
        return ss.str();
    }

    // ── Magnitude / argument helpers ──────────────────────────────────────────
    double abs()  const noexcept { return std::sqrt(re_*re_ + im_*im_); }
    double norm() const noexcept { return re_*re_ + im_*im_; }
    double arg()  const noexcept { return std::atan2(im_, re_); }
    Complex conj()const noexcept { return {re_, -im_}; }

private:
    double re_, im_;
};

// ── Binary arithmetic (non-member) ────────────────────────────────────────────
inline Complex operator+(Complex a, const Complex& b) noexcept { return a += b; }
inline Complex operator-(Complex a, const Complex& b) noexcept { return a -= b; }
inline Complex operator*(Complex a, const Complex& b) noexcept { return a *= b; }
inline Complex operator/(Complex a, const Complex& b)          { return a /= b; }

inline Complex operator+(Complex a, double b) noexcept { return a += b; }
inline Complex operator-(Complex a, double b) noexcept { return a -= b; }
inline Complex operator*(Complex a, double b) noexcept { return a *= b; }
inline Complex operator/(Complex a, double b)          { return a /= b; }

inline Complex operator+(double a, Complex b) noexcept { return b += a; }
inline Complex operator-(double a, const Complex& b) noexcept
{ return Complex(a - b.real(), -b.imag()); }
inline Complex operator*(double a, Complex b) noexcept { return b *= a; }
inline Complex operator/(double a, const Complex& b)
{ return Complex(a) /= b; }

// ── Free math functions (mirror std::complex overloads) ───────────────────────
inline double abs(const Complex& z)  noexcept { return z.abs(); }
inline double norm(const Complex& z) noexcept { return z.norm(); }
inline double arg(const Complex& z)  noexcept { return z.arg(); }
inline Complex conj(const Complex& z)noexcept { return z.conj(); }

inline Complex sqrt(const Complex& z)
{
    double r  = z.abs();
    double re = std::sqrt((r + z.real()) / 2.0);
    double im = (z.imag() >= 0.0 ? 1.0 : -1.0) * std::sqrt((r - z.real()) / 2.0);
    return {re, im};
}

inline Complex exp(const Complex& z) noexcept
{
    double er = std::exp(z.real());
    return {er * std::cos(z.imag()), er * std::sin(z.imag())};
}

inline Complex log(const Complex& z)
{
    if (z.abs() == 0.0) throw std::domain_error("log(0) undefined for Complex");
    return {std::log(z.abs()), z.arg()};
}

inline Complex sin(const Complex& z) noexcept
{
    return {std::sin(z.real()) * std::cosh(z.imag()),
            std::cos(z.real()) * std::sinh(z.imag())};
}

inline Complex cos(const Complex& z) noexcept
{
    return {std::cos(z.real()) * std::cosh(z.imag()),
           -std::sin(z.real()) * std::sinh(z.imag())};
}

inline Complex tan(const Complex& z)
{
    Complex c = cos(z);
    if (c.norm() == 0.0) throw std::domain_error("tan: cos(z) == 0");
    return sin(z) / c;
}

inline Complex pow(const Complex& base, int n)
{
    if (n == 0) return Complex(1.0);
    if (n < 0)  return Complex(1.0) / pow(base, -n);
    Complex result(1.0);
    Complex b = base;
    int     e = n;
    while (e > 0) {
        if (e & 1) result *= b;
        b *= b;
        e >>= 1;
    }
    return result;
}

inline Complex pow(const Complex& base, double exp_)
{
    if (base.abs() == 0.0) return Complex(0.0);
    return utils::exp(Complex(exp_) * utils::log(base));
}

inline Complex polar(double r, double theta = 0.0) noexcept
{
    return {r * std::cos(theta), r * std::sin(theta)};
}

// ── Stream output ─────────────────────────────────────────────────────────────
inline std::ostream& operator<<(std::ostream& os, const Complex& z)
{
    os << z.to_string();
    return os;
}

// ── Interop: construct from std::complex<double> ─────────────────────────────
#ifdef _COMPLEX_  // already included elsewhere
#include <complex>
inline Complex from_std(const std::complex<double>& z) { return {z.real(), z.imag()}; }
inline std::complex<double> to_std(const Complex& z)   { return {z.real(), z.imag()}; }
#endif

} // namespace utils
