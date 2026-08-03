// calculus/integration/Numerical.cpp

#include "Numerical.hpp"
#include "../core/Week4Bridge.hpp"
#include <cmath>
#include <sstream>
#include <iomanip>
#include <algorithm>

namespace Calculus {

// makeEvaluator

Func1D makeEvaluator(const ExprPtr& expr, const std::string& varName) {
    return [expr, varName](double x) -> double {
        return evaluate(expr, {{varName, x}});
    };
}

// Riemann sums

NumericalResult riemannLeft(const Func1D& f, double a, double b, int n) {
    if (n <= 0) return {0, 0, 0, "Riemann Left", false, "n must be > 0"};
    double h   = (b - a) / n;
    double sum = 0.0;
    for (int i = 0; i < n; ++i) sum += f(a + i * h);
    return {sum * h, std::abs(h), n, "Riemann Left", true, ""};
}

NumericalResult riemannRight(const Func1D& f, double a, double b, int n) {
    if (n <= 0) return {0, 0, 0, "Riemann Right", false, "n must be > 0"};
    double h   = (b - a) / n;
    double sum = 0.0;
    for (int i = 1; i <= n; ++i) sum += f(a + i * h);
    return {sum * h, std::abs(h), n, "Riemann Right", true, ""};
}

NumericalResult riemannMid(const Func1D& f, double a, double b, int n) {
    if (n <= 0) return {0, 0, 0, "Riemann Midpoint", false, "n must be > 0"};
    double h   = (b - a) / n;
    double sum = 0.0;
    for (int i = 0; i < n; ++i) sum += f(a + (i + 0.5) * h);
    return {sum * h, h * h / 24.0, n, "Riemann Midpoint", true, ""};
}

// Trapezoidal rule
//   Error: O(h²) globally

NumericalResult trapezoidal(const Func1D& f, double a, double b, int n) {
    if (n <= 0) return {0, 0, 0, "Trapezoidal", false, "n must be > 0"};
    double h   = (b - a) / n;
    double sum = f(a) + f(b);
    for (int i = 1; i < n; ++i) sum += 2.0 * f(a + i * h);
    double result = sum * h / 2.0;
    return {result, (b - a) * h * h / 12.0, n + 1, "Trapezoidal", true, ""};
}

// Simpson's 1/3 rule
//   Requires even n.  Error: O(h⁴) globally

NumericalResult simpson13(const Func1D& f, double a, double b, int n) {
    if (n <= 0 || n % 2 != 0) {
        // Auto-correct: round up to nearest even
        if (n % 2 != 0) n += 1;
        if (n <= 0) return {0, 0, 0, "Simpson 1/3", false, "n must be positive even"};
    }
    double h   = (b - a) / n;
    double sum = f(a) + f(b);
    for (int i = 1; i < n; ++i) {
        double coeff = (i % 2 == 0) ? 2.0 : 4.0;
        sum += coeff * f(a + i * h);
    }
    double result = sum * h / 3.0;
    double errEst = (b - a) * std::pow(h, 4) / 180.0;
    return {result, errEst, n + 1, "Simpson 1/3", true, ""};
}

// Simpson's 3/8 rule
//   Requires n divisible by 3.  Error: O(h⁴) globally

NumericalResult simpson38(const Func1D& f, double a, double b, int n) {
    if (n <= 0 || n % 3 != 0) {
        while (n % 3 != 0) n++;
        if (n <= 0) return {0, 0, 0, "Simpson 3/8", false, "n must be divisible by 3"};
    }
    double h   = (b - a) / n;
    double sum = f(a) + f(b);
    for (int i = 1; i < n; ++i) {
        double coeff = (i % 3 == 0) ? 2.0 : 3.0;
        sum += coeff * f(a + i * h);
    }
    double result = 3.0 * sum * h / 8.0;
    double errEst = (b - a) * std::pow(h, 4) / 80.0;
    return {result, errEst, n + 1, "Simpson 3/8", true, ""};
}

// Boole's rule
//   Requires n divisible by 4.  Error: O(h⁶) globally

NumericalResult boole(const Func1D& f, double a, double b, int n) {
    if (n <= 0 || n % 4 != 0) {
        while (n % 4 != 0) n++;
        if (n <= 0) return {0, 0, 0, "Boole", false, "n must be divisible by 4"};
    }
    double h   = (b - a) / n;
    static constexpr double coeff[5] = {7.0, 32.0, 12.0, 32.0, 7.0};
    double sum = 0.0;
    for (int i = 0; i < n; i += 4) {
        for (int j = 0; j < 5; ++j) {\
            // At internal shared endpoints we'd double-count, so handle:
            if (i > 0 && j == 0) {
                // already counted in previous block's j==4
            } else {
                sum += coeff[j] * f(a + (i + j) * h);
            }
        }
    }
    // Re-do cleanly to avoid overlap confusion:
    sum = 0.0;
    for (int block = 0; block < n / 4; ++block) {
        int base = block * 4;
        sum += 7.0  * f(a + (base + 0) * h);
        sum += 32.0 * f(a + (base + 1) * h);
        sum += 12.0 * f(a + (base + 2) * h);
        sum += 32.0 * f(a + (base + 3) * h);
        sum += 7.0  * f(a + (base + 4) * h);
        if (block + 1 < n / 4) {
            // subtract the endpoint that will be added again
            sum -= 7.0 * f(a + (base + 4) * h);
        }
    }
    double result = 2.0 * h / 45.0 * sum;
    double errEst = (b - a) * std::pow(h, 6) / 945.0;
    return {result, errEst, n + 1, "Boole", true, ""};
}

// Adaptive Simpson

static double adaptiveSimpsonHelper(const Func1D& f,
                                    double a, double b,
                                    double fa, double fm, double fb,
                                    double whole,
                                    double tol, int depth,
                                    int& evals) {
    double m1 = (a + (a + b) / 2.0) / 2.0;
    double m2 = ((a + b) / 2.0 + b) / 2.0;
    double fm1 = f(m1), fm2 = f(m2);
    evals += 2;

    double left  = (b - a) / 12.0 * (fa  + 4.0 * fm1 + fm );
    double right = (b - a) / 12.0 * (fm  + 4.0 * fm2 + fb );
    double delta = left + right - whole;

    if (depth <= 0 || std::abs(delta) <= 15.0 * tol)
        return left + right + delta / 15.0;  // Richardson extrapolation

    double mid = (a + b) / 2.0;
    return adaptiveSimpsonHelper(f, a,   mid, fa,  fm1, fm,  left,  tol / 2.0, depth - 1, evals)
         + adaptiveSimpsonHelper(f, mid, b,   fm,  fm2, fb,  right, tol / 2.0, depth - 1, evals);
}

NumericalResult adaptiveSimpson(const Func1D& f, double a, double b,
                                double tol, int maxDepth) {
    double fa = f(a), fm = f((a + b) / 2.0), fb = f(b);
    double whole = (b - a) / 6.0 * (fa + 4.0 * fm + fb);
    int evals = 3;
    double result = adaptiveSimpsonHelper(f, a, b, fa, fm, fb,
                                          whole, tol, maxDepth, evals);
    return {result, tol, evals, "Adaptive Simpson", true, ""};
}

// Romberg integration
// Builds a triangular table R[i][j] where:
//   R[0][0] = trapezoidal with n=1
//   R[i][0] = trapezoidal with n=2^i

NumericalResult romberg(const Func1D& f, double a, double b, int maxRows) {
    if (maxRows < 1) maxRows = 1;
    if (maxRows > 12) maxRows = 12;

    int m = maxRows;
    std::vector<std::vector<double>> R(m, std::vector<double>(m, 0.0));
    int totalEvals = 0;

    double h = b - a;
    R[0][0] = 0.5 * h * (f(a) + f(b));
    totalEvals += 2;

    for (int i = 1; i < m; ++i) {
        h /= 2.0;
        double sum = 0.0;
        int pts = 1 << (i - 1);   // 2^(i-1) new points
        for (int k = 1; k <= pts; k += 2) {
            sum += f(a + k * h);
        }
        totalEvals += pts / 2 + 1;
        R[i][0] = 0.5 * R[i-1][0] + h * sum;
    }

    // Richardson extrapolation to fill the table
    for (int j = 1; j < m; ++j) {
        double factor = std::pow(4.0, j);
        for (int i = j; i < m; ++i) {
            R[i][j] = (factor * R[i][j-1] - R[i-1][j-1]) / (factor - 1.0);
        }
    }

    double result = R[m-1][m-1];
    double errEst = std::abs(R[m-1][m-1] - R[m-2][m-2]);
    return {result, errEst, totalEvals, "Romberg", true, ""};
}

// Gauss-Legendre quadrature
// Nodes and weights for n = 2, 3, 4, 5 on [-1, 1].
// Transform [a,b] → [-1,1] via  x = (b-a)/2 * t + (a+b)/2

NumericalResult gaussLegendre(const Func1D& f, double a, double b, int n) {
    // Nodes and weights on [-1, 1]
    // Source: Abramowitz & Stegun, Table 25.4
    struct GLP { double node, weight; };

    static constexpr GLP gl2[] = {{-0.5773502692, 1.0},
                               { 0.5773502692, 1.0}};
    static constexpr GLP gl3[] = {{-0.7745966692, 0.5555555556},
                               { 0.0,          0.8888888889},
                               { 0.7745966692, 0.5555555556}};
    static constexpr GLP gl4[] = {{-0.8611363116, 0.3478548451},
                               {-0.3399810436, 0.6521451549},
                               { 0.3399810436, 0.6521451549},
                               { 0.8611363116, 0.3478548451}};
    static const GLP gl5[] = {{-0.9061798459, 0.2369268851},
                               {-0.5384693101, 0.4786286705},
                               { 0.0,          0.5688888889},
                               { 0.5384693101, 0.4786286705},
                               { 0.9061798459, 0.2369268851}};

    const GLP* pts = nullptr;
    if      (n == 2) pts = gl2;
    else if (n == 3) pts = gl3;
    else if (n == 4) pts = gl4;
    else             { n = 5; pts = gl5; }

    double scale  = (b - a) / 2.0;
    double offset = (b + a) / 2.0;
    double sum = 0.0;
    for (int i = 0; i < n; ++i) {
        double x = scale * pts[i].node + offset;
        sum += pts[i].weight * f(x);
    }
    double result = scale * sum;
    return {result, 0.0, n, "Gauss-Legendre n=" + std::to_string(n), true, ""};
}

// Improper integrals
//   ∫_-∞^b f(x)dx →  substitute x = b - t/(1-t)
//   ∫_-∞^∞ f(x)dx →  substitute x = t/(1-t²),  split at 0
// Singularities at t→1 are handled by stopping before 1.

NumericalResult improperInfinite(const Func1D& f, double a) {
    // x = a + t/(1-t),  dx = 1/(1-t)^2
    auto g = [&](double t) -> double {
        if (t >= 1.0 - 1e-10) return 0.0;
        double x   = a + t / (1.0 - t);
        double jac = 1.0 / ((1.0 - t) * (1.0 - t));
        double val = f(x) * jac;
        return std::isfinite(val) ? val : 0.0;
    };
    return romberg(g, 0.0, 1.0 - 1e-6, 10);
}

NumericalResult improperNegInfinite(const Func1D& f, double b) {
    // x = b - t/(1-t)
    auto g = [&](double t) -> double {
        if (t >= 1.0 - 1e-10) return 0.0;
        double x   = b - t / (1.0 - t);
        double jac = 1.0 / ((1.0 - t) * (1.0 - t));
        double val = f(x) * jac;
        return std::isfinite(val) ? val : 0.0;
    };
    return romberg(g, 0.0, 1.0 - 1e-6, 10);
}

NumericalResult improperBothInfinite(const Func1D& f) {
    // Split at 0: ∫_-∞^0 + ∫_0^∞
    auto left  = improperNegInfinite(f, 0.0);
    auto right = improperInfinite(f, 0.0);
    NumericalResult res;
    res.value   = left.value + right.value;
    res.errorEst = left.errorEst + right.errorEst;
    res.evals   = left.evals + right.evals;
    res.method  = "Improper (-∞, +∞)";
    return res;
}

// computeNumerical — formatted entry point

IntegralResult computeNumerical(const std::string& exprStr,
                                const std::string& var,
                                double a, double b,
                                const std::string& method,
                                int n) {
    IntegralResult out;
    try {
        ExprPtr   expr = parse(exprStr);
        Func1D    f    = makeEvaluator(expr, var);
        NumericalResult res;

        bool aInf = !std::isfinite(a);
        bool bInf = !std::isfinite(b);

        if (aInf && bInf) {
            res = improperBothInfinite(f);
        } else if (bInf) {
            res = improperInfinite(f, a);
        } else if (aInf) {
            res = improperNegInfinite(f, b);
        } else {
            const std::string& m = method;
            if (m == "auto")           res = romberg(f, a, b, 8);
            else if (m == "trapezoid") res = trapezoidal(f, a, b, n);
            else if (m == "simpson")   res = simpson13(f, a, b, n % 2 == 0 ? n : n + 1);
            else if (m == "simpson38") res = simpson38(f, a, b, n);
            else if (m == "boole")     res = boole(f, a, b, n);
            else if (m == "romberg")   res = romberg(f, a, b, 8);
            else if (m == "gauss")     res = gaussLegendre(f, a, b, 5);
            else if (m == "adaptive")  res = adaptiveSimpson(f, a, b, 1e-8);
            else if (m == "midpoint")  res = riemannMid(f, a, b, n);
            else if (m == "left")      res = riemannLeft(f, a, b, n);
            else if (m == "right")     res = riemannRight(f, a, b, n);
            else res = romberg(f, a, b, 8);
        }

        if (!res.ok) {
            out.ok       = false;
            out.errorMsg = res.error;
            return out;
        }

        out.raw   = res.value;
        out.error = res.errorEst;
        out.method = res.method;

        std::ostringstream ss;
        ss << std::setprecision(10) << res.value;
        out.value = ss.str();

    } catch (const std::exception& ex) {
        out.ok       = false;
        out.errorMsg = ex.what();
    }
    return out;
}

} // namespace Calculus