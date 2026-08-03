// NumericalAnalysis.cpp

#include "NA.hpp"
#include "../CommonUtils.hpp"

// ── CommonUtils aliases (replaces previous local duplicates) ──────────────────
static auto getP = [](const std::string &j, const std::string &k, const std::string &d = "")
{ return cu_getStr(j, k, d); };
static auto getN = [](const std::string &j, const std::string &k, double d = 0.0)
{ return cu_getNum(j, k, d); };
static auto parseVec = [](const std::string &s)
{ return cu_parseVecD(s); };
static auto parseMat = [](const std::string &s)
{ return cu_parseMat(s); };
static auto fmt = [](double v, int p = 8)
{ return cu_fmt(v, p); };
static auto &str_ll = cu_str;
#include "../MathCore.hpp"

#include "../Calculus/Calculus.hpp"
#include <cmath>
#include <complex>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <numeric>
#include <stdexcept>

namespace NumericalAnalysis
{

    static NAResult ok(const std::string &v, const std::string &d = "",
                       int iters = 0, double res = 0)
    {
        return {true, v, d, "", iters, res};
    }
    static NAResult err(const std::string &m) { return {false, "", "", m}; }

    static double evalF(const std::string &f, const std::string &x, double val)
    {
        auto expr = Calculus::parse(f);
        return Calculus::evaluate(expr, {{x, val}});
    }

    static double evalDeriv(const std::string &f, const std::string &x, double val)
    {
        auto expr = Calculus::parse(f);
        auto d = Calculus::simplify(Calculus::diff(expr, x));
        return Calculus::evaluate(d, {{x, val}});
    }

    // =============================================================================
    // ROOT FINDING
    // =============================================================================

    NAResult bisection(const std::string &f, const std::string &x,
                       double a, double b, double tol, int maxIter)
    {
        double fa = evalF(f, x, a), fb = evalF(f, x, b);
        if (fa * fb > 0)
            return err("f(a) and f(b) must have opposite signs");
        std::ostringstream ss;
        ss << "Bisection Method\nf(x) = " << f << ",  [" << fmt(a, 6) << "," << fmt(b, 6) << "]\n\n";
        ss << std::setw(5) << "n" << std::setw(14) << "a"
           << std::setw(14) << "b" << std::setw(14) << "c"
           << std::setw(14) << "f(c)" << "\n";
        ss << std::string(57, '-') << "\n";
        double c = a;
        int n = 0;
        for (; n < maxIter; ++n)
        {
            c = (a + b) / 2;
            double fc = evalF(f, x, c);
            if (n < 8)
                ss << std::setw(5) << n << std::setw(14) << fmt(a, 8)
                   << std::setw(14) << fmt(b, 8) << std::setw(14) << fmt(c, 8)
                   << std::setw(14) << fmt(fc, 6) << "\n";
            if (std::abs(fc) < tol || (b - a) / 2 < tol)
            {
                ++n;
                break;
            }
            if (fa * fc < 0)
            {
                b = c;
                fb = fc;
            }
            else
            {
                a = c;
                fa = fc;
            }
        }
        ss << "\nRoot ≈ " << fmt(c) << "\nf(root) = " << fmt(evalF(f, x, c))
           << "\nIterations: " << n;
        return ok(fmt(c), ss.str(), n, std::abs(evalF(f, x, c)));
    }

    NAResult newtonRaphson(const std::string &f, const std::string &x,
                           double x0, double tol, int maxIter)
    {
        std::ostringstream ss;
        ss << "Newton-Raphson Method\nf(x) = " << f << ",  x₀ = " << x0 << "\n\n";
        ss << std::setw(5) << "n" << std::setw(16) << "xₙ"
           << std::setw(14) << "f(xₙ)" << std::setw(14) << "f'(xₙ)" << "\n";
        ss << std::string(49, '-') << "\n";
        double xi = x0;
        int n = 0;
        for (; n < maxIter; ++n)
        {
            double fv = evalF(f, x, xi), dfv = evalDeriv(f, x, xi);
            if (n < 10)
                ss << std::setw(5) << n << std::setw(16) << fmt(xi)
                   << std::setw(14) << fmt(fv, 6) << std::setw(14) << fmt(dfv, 6) << "\n";
            if (std::abs(dfv) < 1e-14)
                return err("Derivative near zero at x=" + fmt(xi));
            double xnew = xi - fv / dfv;
            if (std::abs(xnew - xi) < tol)
            {
                xi = xnew;
                ++n;
                break;
            }
            xi = xnew;
        }
        ss << "\nRoot ≈ " << fmt(xi) << "\nf(root) = " << fmt(evalF(f, x, xi))
           << "\nIterations: " << n << "  (quadratic convergence)";
        return ok(fmt(xi), ss.str(), n, std::abs(evalF(f, x, xi)));
    }

    NAResult secantMethod(const std::string &f, const std::string &x,
                          double x0, double x1, double tol, int maxIter)
    {
        std::ostringstream ss;
        ss << "Secant Method\nf(x) = " << f << "\n\n";
        ss << std::setw(5) << "n" << std::setw(16) << "xₙ" << std::setw(14) << "f(xₙ)" << "\n";
        ss << std::string(35, '-') << "\n";
        double xa = x0, xb = x1;
        int n = 0;
        for (; n < maxIter; ++n)
        {
            double fa = evalF(f, x, xa), fb = evalF(f, x, xb);
            if (n < 10)
                ss << std::setw(5) << n << std::setw(16) << fmt(xb) << std::setw(14) << fmt(fb, 6) << "\n";
            if (std::abs(fb - fa) < 1e-14)
                return err("Near-division by zero");
            double xnew = xb - fb * (xb - xa) / (fb - fa);
            if (std::abs(xnew - xb) < tol)
            {
                xb = xnew;
                ++n;
                break;
            }
            xa = xb;
            xb = xnew;
        }
        ss << "\nRoot ≈ " << fmt(xb) << "\nIterations: " << n;
        return ok(fmt(xb), ss.str(), n, std::abs(evalF(f, x, xb)));
    }

    NAResult regulaFalsi(const std::string &f, const std::string &x,
                         double a, double b, double tol, int maxIter)
    {
        double fa = evalF(f, x, a), fb = evalF(f, x, b);
        if (fa * fb > 0)
            return err("f(a) and f(b) must have opposite signs");
        std::ostringstream ss;
        ss << "Regula Falsi (False Position)\nf(x) = " << f << "\n\n";
        double c = a;
        int n = 0;
        for (; n < maxIter; ++n)
        {
            c = a - fa * (b - a) / (fb - fa);
            double fc = evalF(f, x, c);
            if (n < 6)
                ss << "n=" << n << ": c=" << fmt(c, 10) << "  f(c)=" << fmt(fc, 6) << "\n";
            if (std::abs(fc) < tol)
            {
                ++n;
                break;
            }
            if (fa * fc < 0)
            {
                b = c;
                fb = fc;
            }
            else
            {
                a = c;
                fa = fc;
            }
        }
        ss << "\nRoot ≈ " << fmt(c) << "\nIterations: " << n;
        return ok(fmt(c), ss.str(), n, std::abs(evalF(f, x, c)));
    }

    NAResult fixedPoint(const std::string &g, const std::string &x,
                        double x0, double tol, int maxIter)
    {
        std::ostringstream ss;
        ss << "Fixed-Point Iteration:  x = g(x) = " << g << "\nx₀ = " << x0 << "\n\n";
        double xi = x0;
        int n = 0;
        for (; n < maxIter; ++n)
        {
            double xnew = evalF(g, x, xi);
            if (n < 8)
                ss << "x_" << n + 1 << " = " << fmt(xnew, 10) << "\n";
            if (std::abs(xnew - xi) < tol)
            {
                xi = xnew;
                ++n;
                break;
            }
            xi = xnew;
            if (!std::isfinite(xi))
                return err("Iteration diverged");
        }
        ss << "\nFixed point ≈ " << fmt(xi)
           << "\nIterations: " << n
           << "\n\nConvergence requires |g'(x*)| < 1";
        return ok(fmt(xi), ss.str(), n, std::abs(evalF(g, x, xi) - xi));
    }

    NAResult mullerMethod(const std::string &f, const std::string &x,
                          double x0, double x1, double x2, double tol, int maxIter)
    {
        std::ostringstream ss;
        ss << "Müller's Method\nf(x) = " << f << "\n\n";
        double xa = x0, xb = x1, xc = x2;
        int n = 0;
        for (; n < maxIter; ++n)
        {
            double fa = evalF(f, x, xa), fb = evalF(f, x, xb), fc = evalF(f, x, xc);
            double h1 = xb - xa, h2 = xc - xb;
            double d1 = (fb - fa) / h1, d2 = (fc - fb) / h2;
            double a_ = (d2 - d1) / (h2 + h1), b_ = a_ * h2 + d2, c_ = fc;
            double disc = b_ * b_ - 4 * a_ * c_;
            if (disc < 0)
                disc = 0;
            double denom = std::abs(b_ + std::sqrt(disc)) > std::abs(b_ - std::sqrt(disc))
                               ? b_ + std::sqrt(disc)
                               : b_ - std::sqrt(disc);
            if (std::abs(denom) < 1e-14)
                break;
            double dx = -2 * c_ / denom;
            double xnew = xc + dx;
            if (n < 6)
                ss << "x_" << n + 3 << " = " << fmt(xnew, 10) << "\n";
            if (std::abs(dx) < tol)
            {
                xc = xnew;
                ++n;
                break;
            }
            xa = xb;
            xb = xc;
            xc = xnew;
        }
        ss << "\nRoot ≈ " << fmt(xc) << "\nIterations: " << n;
        return ok(fmt(xc), ss.str(), n, std::abs(evalF(f, x, xc)));
    }

    NAResult brentsMethod(const std::string &f, const std::string &x, double a, double b, double tol)
    {
        double fa = evalF(f, x, a), fb = evalF(f, x, b);
        if (fa * fb > 0)
            return err("f(a) and f(b) must have opposite signs");
        if (std::abs(fa) < std::abs(fb))
        {
            std::swap(a, b);
            std::swap(fa, fb);
        }
        double c = a, fc = fa, s = 0, d = 0;
        bool mflag = true;
        int n = 0;
        std::ostringstream ss;
        ss << "Brent's Method\nf(x) = " << f << "\n\n";
        for (; n < 100; ++n)
        {
            if (std::abs(b - a) < tol)
                break;
            if (fa != fc && fb != fc)
                s = a * fb * fc / ((fa - fb) * (fa - fc)) + b * fa * fc / ((fb - fa) * (fb - fc)) + c * fa * fb / ((fc - fa) * (fc - fb));
            else
                s = b - fb * (b - a) / (fb - fa);
            bool cond = (s < (3 * a + b) / 4 || s > b) || (mflag && std::abs(s - b) >= std::abs(b - c) / 2) ||
                        (!mflag && std::abs(s - b) >= std::abs(c - d) / 2) ||
                        (mflag && std::abs(b - c) < tol) || (!mflag && std::abs(c - d) < tol);
            if (cond)
            {
                s = (a + b) / 2;
                mflag = true;
            }
            else
                mflag = false;
            double fs = evalF(f, x, s);
            d = c;
            c = b;
            fc = fb;
            if (fa * fs < 0)
            {
                b = s;
                fb = fs;
            }
            else
            {
                a = s;
                fa = fs;
            }
            if (std::abs(fa) < std::abs(fb))
            {
                std::swap(a, b);
                std::swap(fa, fb);
            }
        }
        ss << "Root ≈ " << fmt(b) << "\nf(root) = " << fmt(fb, 6)
           << "\nIterations: " << n << "  (superlinear convergence)";
        return ok(fmt(b), ss.str(), n, std::abs(fb));
    }

    NAResult allRootsInterval(const std::string &f, const std::string &x,
                              double a, double b, int subs)
    {
        std::ostringstream ss;
        ss << "All Roots of f(x) = " << f << " on [" << fmt(a, 6) << "," << fmt(b, 6) << "]\n\n";
        double h = (b - a) / subs;
        std::vector<double> roots;
        for (int i = 0; i < subs; ++i)
        {
            double x1 = a + i * h, x2 = a + (i + 1) * h;
            double f1 = evalF(f, x, x1), f2 = evalF(f, x, x2);
            if (f1 * f2 < 0)
            {
                auto r = bisection(f, x, x1, x2, 1e-10, 50);
                if (r.ok)
                {
                    double rv = std::stod(r.value);
                    bool dup = false;
                    for (double rr : roots)
                        if (std::abs(rr - rv) < 1e-6)
                        {
                            dup = true;
                            break;
                        }
                    if (!dup)
                        roots.push_back(rv);
                }
            }
        }
        if (roots.empty())
            ss << "No roots found (increase subdivisions or check interval)\n";
        else
        {
            ss << roots.size() << " root(s) found:\n";
            for (size_t i = 0; i < roots.size(); ++i)
                ss << "  x_" << i + 1 << " ≈ " << fmt(roots[i]) << "  (f=" << fmt(evalF(f, x, roots[i]), 4) << ")\n";
        }
        return ok(ss.str());
    }

    // =============================================================================
    // INTERPOLATION
    // =============================================================================

    NAResult lagrangeInterp(const Vec &xs, const Vec &ys, double xEval)
    {
        int n = xs.size();
        double result = 0;
        std::ostringstream ss;
        ss << "Lagrange Interpolation\n\n";
        for (int i = 0; i < n; ++i)
        {
            double Li = 1;
            for (int j = 0; j < n; ++j)
                if (j != i)
                    Li *= (xEval - xs[j]) / (xs[i] - xs[j]);
            result += ys[i] * Li;
            if (n <= 6)
                ss << "L_" << i << "(x=" << fmt(xEval, 4) << ") = " << fmt(Li, 8) << "\n";
        }
        ss << "\nP(" << fmt(xEval, 6) << ") = " << fmt(result);
        return ok(fmt(result), ss.str());
    }

    NAResult newtonDivDiff(const Vec &xs, const Vec &ys, double xEval)
    {
        int n = xs.size();
        Vec dd = ys;
        std::ostringstream ss;
        ss << "Newton's Divided Differences\n\n";
        ss << "Divided difference table:\n";
        for (int i = 0; i < n; ++i)
            ss << fmt(dd[i], 8) << " ";
        ss << "\n";
        Vec coeffs = {dd[0]};
        for (int j = 1; j < n; ++j)
        {
            Vec newdd(n - j);
            for (int i = 0; i < n - j; ++i)
                newdd[i] = (dd[i + 1] - dd[i]) / (xs[i + j] - xs[i]);
            coeffs.push_back(newdd[0]);
            for (double v : newdd)
                ss << fmt(v, 8) << " ";
            ss << "\n";
            dd = newdd;
        }
        // Horner evaluation
        double result = coeffs.back();
        for (int i = (int)coeffs.size() - 2; i >= 0; --i)
            result = result * (xEval - xs[i]) + coeffs[i];
        ss << "\nP(" << fmt(xEval, 6) << ") = " << fmt(result);
        return ok(fmt(result), ss.str());
    }

    NAResult neville(const Vec &xs, const Vec &ys, double xEval)
    {
        int n = xs.size();
        Vec Q = ys;
        std::ostringstream ss;
        ss << "Neville's Method\n\n";
        for (int i = 1; i < n; ++i)
        {
            Vec Qnew(n - i);
            for (int j = 0; j < n - i; ++j)
                Qnew[j] = ((xEval - xs[j + i]) * Q[j] + (xs[j] - xEval) * Q[j + 1]) / (xs[j] - xs[j + i]);
            Q = Qnew;
        }
        ss << "P(" << fmt(xEval, 6) << ") = " << fmt(Q[0]);
        return ok(fmt(Q[0]), ss.str());
    }

    NAResult splineCubic(const Vec &xs, const Vec &ys, double xEval)
    {
        if (xs.size() < 3)
            return err("Cubic spline requires at least 3 data points");
        if (xs.size() != ys.size())
            return err("xs and ys must have the same length");
        int n = (int)xs.size() - 1;
        Vec h(n), alpha(n), mu(n, 0), z(n + 1, 0), l(n + 1, 0), c(n + 1, 0), b(n), d(n);
        for (int i = 0; i < n; ++i)
            h[i] = xs[i + 1] - xs[i];
        for (int i = 1; i < n; ++i)
            alpha[i] = 3 * (ys[i + 1] - ys[i]) / h[i] - 3 * (ys[i] - ys[i - 1]) / h[i - 1];
        l[0] = 1;
        l[n] = 1;
        for (int i = 1; i < n; ++i)
        {
            l[i] = 2 * (xs[i + 1] - xs[i - 1]) - h[i - 1] * mu[i - 1];
            mu[i] = h[i] / l[i];
            z[i] = (alpha[i] - h[i - 1] * z[i - 1]) / l[i];
        }
        for (int j = n - 1; j >= 0; --j)
        {
            c[j] = z[j] - mu[j] * c[j + 1];
            b[j] = (ys[j + 1] - ys[j]) / h[j] - h[j] * (c[j + 1] + 2 * c[j]) / 3;
            d[j] = (c[j + 1] - c[j]) / (3 * h[j]);
        }
        // Find interval — clamp to valid range so xs[k+1] never goes OOB
        int k = 0;
        for (int i = 0; i < n - 1; ++i)
            if (xEval > xs[i + 1])
                k = i + 1;
        if (k >= n)
            k = n - 1; // clamp: keeps xs[k+1] in bounds
        double t = xEval - xs[k];
        double result = ys[k] + b[k] * t + c[k] * t * t + d[k] * t * t * t;
        std::ostringstream ss;
        ss << "Natural Cubic Spline\nS(" << fmt(xEval, 6) << ") = " << fmt(result)
           << "\n(Using interval [" << fmt(xs[k], 6) << "," << fmt(xs[k + 1], 6) << "])";
        return ok(fmt(result), ss.str());
    }

    NAResult splineLinear(const Vec &xs, const Vec &ys, double xEval)
    {
        if (xs.size() < 2)
            return err("Linear spline requires at least 2 data points");
        if (xs.size() != ys.size())
            return err("xs and ys must have the same length");
        int n = (int)xs.size() - 1;
        int k = 0;
        for (int i = 0; i < n - 1; ++i)
            if (xEval > xs[i + 1])
                k = i + 1;
        double t = (xEval - xs[k]) / (xs[k + 1] - xs[k]);
        double result = (1 - t) * ys[k] + t * ys[k + 1];
        return ok(fmt(result), "Linear spline: S(" + fmt(xEval, 6) + ") = " + fmt(result));
    }

    NAResult hermiteInterp(const Vec &xs, const Vec &ys, const Vec &ders, double xEval)
    {
        int n = xs.size();
        double result = 0;
        std::ostringstream ss;
        ss << "Hermite Interpolation (matches function values AND derivatives)\n\n";
        for (int i = 0; i < n; ++i)
        {
            // H_i and K_i basis polynomials
            double Li = 1, Li_prime = 0;
            for (int j = 0; j < n; ++j)
                if (j != i)
                {
                    Li *= (xEval - xs[j]) / (xs[i] - xs[j]);
                }
            // dL_i/dx at xs[i]
            for (int j = 0; j < n; ++j)
                if (j != i)
                {
                    double term = 1 / (xs[i] - xs[j]);
                    for (int k = 0; k < n; ++k)
                        if (k != i && k != j)
                            term *= (xs[i] - xs[k]) / (xs[i] - xs[k]);
                    Li_prime += term;
                }
            double Hi = (1 - 2 * (xEval - xs[i]) * Li_prime) * Li * Li;
            double Ki = (xEval - xs[i]) * Li * Li;
            result += ys[i] * Hi + ders[i] * Ki;
        }
        ss << "H(" << fmt(xEval, 6) << ") = " << fmt(result);
        return ok(fmt(result), ss.str());
    }

    NAResult chebychevNodes(int n, double a, double b)
    {
        std::ostringstream ss;
        ss << "Chebyshev Nodes on [" << a << "," << b << "], n=" << n << "\n\n";
        ss << "Optimal placement minimises max interpolation error.\n";
        ss << "x_k = (a+b)/2 + (b-a)/2 · cos((2k-1)π/(2n)),  k=1,...,n\n\n";
        ss << "Nodes:\n";
        Vec nodes(n);
        for (int k = 1; k <= n; ++k)
        {
            nodes[k - 1] = (a + b) / 2 + (b - a) / 2 * std::cos((2 * k - 1) * M_PI / (2 * n));
            ss << "  x_" << k << " = " << fmt(nodes[k - 1]) << "\n";
        }
        return ok(ss.str());
    }

    NAResult polynomialFit(const Vec &xs, const Vec &ys, int degree)
    {
        // Least-squares polynomial fit via normal equations
        int n = xs.size(), m = degree + 1;
        if (n < m)
            return err("Need at least degree+1 data points");
        Mat A(m, Vec(m, 0));
        Vec b(m, 0);
        for (int i = 0; i < m; ++i)
            for (int j = 0; j < m; ++j)
                for (int k = 0; k < n; ++k)
                    A[i][j] += std::pow(xs[k], i + j);
        for (int i = 0; i < m; ++i)
            for (int k = 0; k < n; ++k)
                b[i] += ys[k] * std::pow(xs[k], i);
        // Gaussian elimination
        for (int col = 0; col < m; ++col)
        {
            int pivot = col;
            for (int r = col + 1; r < m; ++r)
                if (std::abs(A[r][col]) > std::abs(A[pivot][col]))
                    pivot = r;
            std::swap(A[col], A[pivot]);
            std::swap(b[col], b[pivot]);
            if (std::abs(A[col][col]) < 1e-14)
                return err("Singular system");
            double d = A[col][col];
            for (int j = col; j < m; ++j)
                A[col][j] /= d;
            b[col] /= d;
            for (int r = 0; r < m; ++r)
            {
                if (r == col)
                    continue;
                double f = A[r][col];
                for (int j = col; j < m; ++j)
                    A[r][j] -= f * A[col][j];
                b[r] -= f * b[col];
            }
        }
        std::ostringstream ss;
        ss << "Least-Squares Polynomial Fit (degree " << degree << ")\n\np(x) = ";
        for (int i = 0; i < m; ++i)
        {
            if (i)
                ss << (b[i] >= 0 ? " + " : " - ") << std::abs(b[i]) << "x^" << i;
            else
                ss << b[i];
        }
        ss << "\n\nCoefficients: [";
        for (int i = 0; i < m; ++i)
        {
            if (i)
                ss << ",";
            ss << fmt(b[i]);
        }
        ss << "]";
        return ok(ss.str());
    }

    // =============================================================================
    // NUMERICAL DIFFERENTIATION
    // =============================================================================

    NAResult forwardDiff(const std::string &f, const std::string &x, double x0, double h)
    {
        double fxh = evalF(f, x, x0 + h), fx = evalF(f, x, x0);
        double deriv = (fxh - fx) / h;
        std::ostringstream ss;
        ss << "Forward difference: f'(x) ≈ [f(x+h)-f(x)]/h,  h=" << fmt(h, 3) << "\n";
        ss << "f'(" << x0 << ") ≈ [" << fmt(fxh) << " - " << fmt(fx) << "]/" << h << " = " << fmt(deriv) << "\n";
        ss << "Error O(h)";
        return ok(fmt(deriv), ss.str());
    }

    NAResult centralDiff(const std::string &f, const std::string &x, double x0, double h)
    {
        double fxh = evalF(f, x, x0 + h), fxmh = evalF(f, x, x0 - h);
        double deriv = (fxh - fxmh) / (2 * h);
        std::ostringstream ss;
        ss << "Central difference: f'(x) ≈ [f(x+h)-f(x-h)]/(2h),  h=" << fmt(h, 3) << "\n";
        ss << "f'(" << x0 << ") ≈ " << fmt(deriv) << "\nError O(h²)";
        return ok(fmt(deriv), ss.str());
    }

    NAResult secondDeriv(const std::string &f, const std::string &x, double x0, double h)
    {
        double fxh = evalF(f, x, x0 + h), fx = evalF(f, x, x0), fxmh = evalF(f, x, x0 - h);
        double d2 = (fxh - 2 * fx + fxmh) / (h * h);
        std::ostringstream ss;
        ss << "Second derivative: f''(x) ≈ [f(x+h)-2f(x)+f(x-h)]/h²\n";
        ss << "f''(" << x0 << ") ≈ " << fmt(d2) << "\nError O(h²)";
        return ok(fmt(d2), ss.str());
    }

    NAResult richardsonDiff(const std::string &f, const std::string &x, double x0, double h)
    {
        // D(h) = [f(x+h)-f(x-h)]/(2h)
        // Richardson: (4D(h/2)-D(h))/3 gives O(h^4)
        auto D = [&](double hh)
        { return (evalF(f, x, x0 + hh) - evalF(f, x, x0 - hh)) / (2 * hh); };
        double Dh = D(h), Dh2 = D(h / 2);
        double rich = (4 * Dh2 - Dh) / 3;
        std::ostringstream ss;
        ss << "Richardson Extrapolation for f'(x)\n";
        ss << "D(h)   = " << fmt(Dh) << "\n";
        ss << "D(h/2) = " << fmt(Dh2) << "\n";
        ss << "R = (4D(h/2)-D(h))/3 = " << fmt(rich) << "\nError O(h⁴)";
        return ok(fmt(rich), ss.str());
    }

    // =============================================================================
    // NUMERICAL INTEGRATION (QUADRATURE)
    // =============================================================================

    NAResult trapezoidalRule(const std::string &f, const std::string &x,
                             double a, double b, int n)
    {
        double h = (b - a) / n, sum = 0;
        sum += evalF(f, x, a) + evalF(f, x, b);
        for (int i = 1; i < n; ++i)
            sum += 2 * evalF(f, x, a + i * h);
        double result = h / 2 * sum;
        double err_est = std::abs((b - a) * h * h / 12 * (evalF(f, x, (a + b) / 2 + 1e-4) - 2 * evalF(f, x, (a + b) / 2) + evalF(f, x, (a + b) / 2 - 1e-4)) / (1e-4 * 1e-4));
        std::ostringstream ss;
        ss << "Trapezoidal Rule, n=" << n << "\n∫f dx ≈ " << fmt(result)
           << "\nError estimate O(h²) ≈ " << fmt(err_est);
        return ok(fmt(result), ss.str());
    }

    NAResult simpsonsRule(const std::string &f, const std::string &x,
                          double a, double b, int n)
    {
        if (n % 2 != 0)
            n++;
        double h = (b - a) / n, sum = evalF(f, x, a) + evalF(f, x, b);
        for (int i = 1; i < n; ++i)
            sum += (i % 2 == 0 ? 2 : 4) * evalF(f, x, a + i * h);
        double result = h / 3 * sum;
        std::ostringstream ss;
        ss << "Simpson's 1/3 Rule, n=" << n << "\n∫f dx ≈ " << fmt(result) << "\nError O(h⁴)";
        return ok(fmt(result), ss.str());
    }

    NAResult simpsons38Rule(const std::string &f, const std::string &x,
                            double a, double b, int n)
    {
        if (n % 3 != 0)
            n += 3 - n % 3;
        double h = (b - a) / n, sum = evalF(f, x, a) + evalF(f, x, b);
        for (int i = 1; i < n; ++i)
            sum += (i % 3 == 0 ? 2 : 3) * evalF(f, x, a + i * h);
        double result = 3 * h / 8 * sum;
        std::ostringstream ss;
        ss << "Simpson's 3/8 Rule, n=" << n << "\n∫f dx ≈ " << fmt(result) << "\nError O(h⁴)";
        return ok(fmt(result), ss.str());
    }

    NAResult gaussLegendre(const std::string &f, const std::string &x,
                           double a, double b, int n)
    {
        // Gauss-Legendre nodes and weights for n=2..5
        static const double nodes5[] = {0, 0.538469, 0.538469, 0.90618, 0.90618};
        static const double wts5[] = {0.568889, 0.478629, 0.478629, 0.236927, 0.236927};
        static const double nodes4[] = {0.339981, 0.339981, 0.861136, 0.861136};
        static const double wts4[] = {0.652145, 0.652145, 0.347855, 0.347855};
        static const double nodes2[] = {-0.577350, 0.577350};
        static const double wts2[] = {1.0, 1.0};
        const double *nd = nodes5;
        const double *wt = wts5;
        int npts = 5;
        if (n == 2)
        {
            nd = nodes2;
            wt = wts2;
            npts = 2;
        }
        else if (n == 4)
        {
            nd = nodes4;
            wt = wts4;
            npts = 4;
        }
        // Map from [-1,1] to [a,b]
        double sum = 0;
        for (int i = 0; i < npts; ++i)
        {
            double xi = (b - a) / 2 * nd[i] + (a + b) / 2;
            sum += wt[i] * evalF(f, x, xi);
        }
        double result = (b - a) / 2 * sum;
        std::ostringstream ss;
        ss << "Gauss-Legendre Quadrature (" << npts << "-point)\n∫f dx ≈ " << fmt(result)
           << "\nExact for polynomials of degree ≤ " << 2 * npts - 1;
        return ok(fmt(result), ss.str());
    }

    NAResult romberg(const std::string &f, const std::string &x,
                     double a, double b, int levels)
    {
        std::vector<Vec> R(levels, Vec(levels, 0));
        // R[0][0] = trapezoidal with 1 interval
        R[0][0] = (b - a) / 2 * (evalF(f, x, a) + evalF(f, x, b));
        std::ostringstream ss;
        ss << "Romberg Integration\n\nRomberg table:\n";
        for (int i = 1; i < levels; ++i)
        {
            int n = 1 << i;
            double h = (b - a) / n;
            double sum = 0;
            for (int k = 1; k <= n; k += 2)
                sum += evalF(f, x, a + k * h);
            R[i][0] = R[i - 1][0] / 2 + h * sum;
            for (int j = 1; j <= i; ++j)
            {
                double fac = std::pow(4, j);
                R[i][j] = (fac * R[i][j - 1] - R[i - 1][j - 1]) / (fac - 1);
            }
            ss << "R[" << i << "] = ";
            for (int j = 0; j <= i; ++j)
                ss << fmt(R[i][j], 10) << "  ";
            ss << "\n";
        }
        double result = R[levels - 1][levels - 1];
        ss << "\nBest estimate: " << fmt(result);
        return ok(fmt(result), ss.str());
    }

    NAResult adaptiveQuad(const std::string &f, const std::string &x,
                          double a, double b, double tol)
    {
        double total = 0;
        int calls = 0;
        std::function<double(double, double, double, double, double, int)> adapt =
            [&](double lo, double hi, double flo, double fhi, double fmid, int depth) -> double
        {
            double mid = (lo + hi) / 2;
            double fm1 = (lo + mid) / 2, fm2 = (mid + hi) / 2;
            double ffm1 = evalF(f, x, fm1), ffm2 = evalF(f, x, fm2);
            calls += 2;
            double s1 = (hi - lo) / 6 * (flo + 4 * fmid + fhi);
            double s2 = (hi - lo) / 12 * (flo + 4 * ffm1 + 2 * fmid + 4 * ffm2 + fhi);
            if (depth > 50 || std::abs(s2 - s1) < 15 * tol * (hi - lo))
                return s2 + (s2 - s1) / 15;
            return adapt(lo, mid, flo, fmid, ffm1, depth + 1) + adapt(mid, hi, fmid, fhi, ffm2, depth + 1);
        };
        double fa = evalF(f, x, a), fb = evalF(f, x, b), fm = evalF(f, x, (a + b) / 2);
        calls += 3;
        total = adapt(a, b, fa, fb, fm, 0);
        std::ostringstream ss;
        ss << "Adaptive Quadrature (Simpson-based)\n∫f dx ≈ " << fmt(total)
           << "\nFunction evaluations: " << calls << "\nTolerance: " << tol;
        return ok(fmt(total), ss.str());
    }

    NAResult gaussLaguerre(const std::string &f, const std::string &x, int n)
    {
        // 5-point Gauss-Laguerre for ∫_0^∞ f(x)e^{-x} dx
        static const double nd[] = {0.263560, 1.413403, 3.596426, 7.085191, 12.640801};
        static const double wt[] = {0.521756, 0.398667, 0.0759424, 0.00361176, 0.0000233700};
        double sum = 0;
        for (int i = 0; i < 5; ++i)
            sum += wt[i] * evalF(f, x, nd[i]) * std::exp(nd[i]); // remove e^{-x} factor
        std::ostringstream ss;
        ss << "Gauss-Laguerre Quadrature (5-point)\n";
        ss << "∫_0^∞ f(x) dx ≈ " << fmt(sum) << "\n";
        ss << "(Weights adjusted for e^{-x} kernel)";
        return ok(fmt(sum), ss.str());
    }

    NAResult gaussHermite(const std::string &f, const std::string &x, int n)
    {
        // 5-point Gauss-Hermite for ∫_{-∞}^∞ f(x)e^{-x²} dx
        static const double nd[] = {-2.020182, -0.958572, 0, 0.958572, 2.020182};
        static const double wt[] = {0.019953, 0.394424, 0.945309, 0.394424, 0.019953};
        double sum = 0;
        for (int i = 0; i < 5; ++i)
            sum += wt[i] * evalF(f, x, nd[i]) * std::exp(nd[i] * nd[i]);
        std::ostringstream ss;
        ss << "Gauss-Hermite Quadrature (5-point)\n";
        ss << "∫_{-∞}^∞ f(x) dx ≈ " << fmt(sum) << "\n";
        ss << "(Weights adjusted for e^{-x²} kernel)";
        return ok(fmt(sum), ss.str());
    }

    NAResult gaussChebyshev(const std::string &f, const std::string &x, int n)
    {
        // n-point Gauss-Chebyshev for ∫_{-1}^1 f(x)/√(1-x²) dx
        double sum = 0;
        for (int k = 1; k <= n; ++k)
        {
            double xk = std::cos((2 * k - 1) * M_PI / (2 * n));
            sum += evalF(f, x, xk);
        }
        double result = M_PI / n * sum;
        std::ostringstream ss;
        ss << "Gauss-Chebyshev Quadrature (" << n << "-point)\n";
        ss << "∫_{-1}^1 f(x)/√(1-x²) dx ≈ " << fmt(result);
        return ok(fmt(result), ss.str());
    }

    // =============================================================================
    // LINEAR SYSTEMS
    // =============================================================================

    NAResult gaussianElim(const Mat &A, const Vec &b)
    {
        int n = A.size();
        Mat Aug(n, Vec(n + 1));
        for (int i = 0; i < n; ++i)
        {
            for (int j = 0; j < n; ++j)
                Aug[i][j] = A[i][j];
            Aug[i][n] = b[i];
        }
        std::ostringstream ss;
        ss << "Gaussian Elimination with Partial Pivoting\n\n";
        for (int col = 0; col < n; ++col)
        {
            int pivot = col;
            for (int r = col + 1; r < n; ++r)
                if (std::abs(Aug[r][col]) > std::abs(Aug[pivot][col]))
                    pivot = r;
            std::swap(Aug[col], Aug[pivot]);
            if (std::abs(Aug[col][col]) < 1e-14)
                return err("Singular or near-singular matrix");
            double d = Aug[col][col];
            for (int j = col; j <= n; ++j)
                Aug[col][j] /= d;
            for (int r = 0; r < n; ++r)
            {
                if (r == col)
                    continue;
                double f = Aug[r][col];
                for (int j = col; j <= n; ++j)
                    Aug[r][j] -= f * Aug[col][j];
            }
        }
        Vec sol(n);
        for (int i = 0; i < n; ++i)
            sol[i] = Aug[i][n];
        ss << "Solution:\n";
        for (int i = 0; i < n; ++i)
            ss << "  x_" << i + 1 << " = " << fmt(sol[i]) << "\n";
        return ok(ss.str());
    }

    NAResult jacobiIter(const Mat &A, const Vec &b, double tol, int maxIter)
    {
        int n = A.size();
        Vec x(n, 0);
        std::ostringstream ss;
        ss << "Jacobi Iteration\n\n";
        int k = 0;
        for (; k < maxIter; ++k)
        {
            Vec xnew(n);
            for (int i = 0; i < n; ++i)
            {
                double sum = b[i];
                for (int j = 0; j < n; ++j)
                    if (j != i)
                        sum -= A[i][j] * x[j];
                if (std::abs(A[i][i]) < 1e-14)
                    return err("Zero diagonal element");
                xnew[i] = sum / A[i][i];
            }
            double res = 0;
            for (int i = 0; i < n; ++i)
                res += std::pow(xnew[i] - x[i], 2);
            if (k < 5)
            {
                ss << "Iter " << k << ": x=[";
                for (int i = 0; i < n; ++i)
                {
                    if (i)
                        ss << ",";
                    ss << fmt(xnew[i], 6);
                }
                ss << "]\n";
            }
            x = xnew;
            if (std::sqrt(res) < tol)
            {
                k++;
                break;
            }
        }
        ss << "\nSolution after " << k << " iterations:\n";
        for (int i = 0; i < n; ++i)
            ss << "  x_" << i + 1 << " = " << fmt(x[i]) << "\n";
        return ok(ss.str(), ss.str(), k);
    }

    NAResult gaussSeidelIter(const Mat &A, const Vec &b, double tol, int maxIter)
    {
        int n = A.size();
        Vec x(n, 0);
        std::ostringstream ss;
        ss << "Gauss-Seidel Iteration\n\n";
        int k = 0;
        for (; k < maxIter; ++k)
        {
            Vec xold = x;
            for (int i = 0; i < n; ++i)
            {
                double sum = b[i];
                for (int j = 0; j < n; ++j)
                    if (j != i)
                        sum -= A[i][j] * x[j];
                if (std::abs(A[i][i]) < 1e-14)
                    return err("Zero diagonal");
                x[i] = sum / A[i][i];
            }
            double res = 0;
            for (int i = 0; i < n; ++i)
                res += std::pow(x[i] - xold[i], 2);
            if (std::sqrt(res) < tol)
            {
                k++;
                break;
            }
        }
        ss << "Solution after " << k << " iterations:\n";
        for (int i = 0; i < n; ++i)
            ss << "  x_" << i + 1 << " = " << fmt(x[i]) << "\n";
        ss << "(Faster than Jacobi when convergent)";
        return ok(ss.str(), ss.str(), k);
    }

    NAResult sorIter(const Mat &A, const Vec &b, double omega, double tol, int maxIter)
    {
        int n = A.size();
        Vec x(n, 0);
        std::ostringstream ss;
        ss << "Successive Over-Relaxation (ω=" << omega << ")\n\n";
        int k = 0;
        for (; k < maxIter; ++k)
        {
            Vec xold = x;
            for (int i = 0; i < n; ++i)
            {
                double sum = b[i];
                for (int j = 0; j < n; ++j)
                    if (j != i)
                        sum -= A[i][j] * x[j];
                if (std::abs(A[i][i]) < 1e-14)
                    return err("Zero diagonal");
                x[i] = (1 - omega) * x[i] + omega * sum / A[i][i];
            }
            double res = 0;
            for (int i = 0; i < n; ++i)
                res += std::pow(x[i] - xold[i], 2);
            if (std::sqrt(res) < tol)
            {
                k++;
                break;
            }
        }
        ss << "ω=" << omega << " (optimal ω gives fastest convergence)\n";
        ss << "Solution after " << k << " iterations:\n";
        for (int i = 0; i < n; ++i)
            ss << "  x_" << i + 1 << " = " << fmt(x[i]) << "\n";
        return ok(ss.str(), ss.str(), k);
    }

    NAResult conjugateGradient(const Mat &A, const Vec &b, double tol, int maxIter)
    {
        int n = A.size();
        Vec x(n, 0), r = b, p = b;
        double rr = 0;
        for (double ri : r)
            rr += ri * ri;
        std::ostringstream ss;
        ss << "Conjugate Gradient Method\n(for symmetric positive definite A)\n\n";
        int k = 0;
        for (; k < maxIter && std::sqrt(rr) > tol; ++k)
        {
            Vec Ap(n, 0);
            for (int i = 0; i < n; ++i)
                for (int j = 0; j < n; ++j)
                    Ap[i] += A[i][j] * p[j];
            double pAp = 0;
            for (int i = 0; i < n; ++i)
                pAp += p[i] * Ap[i];
            if (std::abs(pAp) < 1e-14)
                break;
            double alpha = rr / pAp;
            for (int i = 0; i < n; ++i)
                x[i] += alpha * p[i];
            for (int i = 0; i < n; ++i)
                r[i] -= alpha * Ap[i];
            double rr_new = 0;
            for (double ri : r)
                rr_new += ri * ri;
            double beta = rr_new / rr;
            rr = rr_new;
            for (int i = 0; i < n; ++i)
                p[i] = r[i] + beta * p[i];
        }
        ss << "Converged in " << k << " iterations (residual=" << fmt(std::sqrt(rr)) << ")\n";
        ss << "Solution:\n";
        for (int i = 0; i < n; ++i)
            ss << "  x_" << i + 1 << " = " << fmt(x[i]) << "\n";
        return ok(ss.str(), ss.str(), k, std::sqrt(rr));
    }

    NAResult conditionNumber(const Mat &A)
    {
        // Estimate condition number using power iteration for max/min eigenvalues
        int n = A.size();
        std::ostringstream ss;
        ss << "Condition Number Estimate\n\n";
        // ||A||_1 = max column sum; ||A^{-1}||_1 estimated
        double norm1 = 0;
        for (int j = 0; j < n; ++j)
        {
            double cs = 0;
            for (int i = 0; i < n; ++i)
                cs += std::abs(A[i][j]);
            norm1 = std::max(norm1, cs);
        }
        double normInf = 0;
        for (int i = 0; i < n; ++i)
        {
            double rs = 0;
            for (int j = 0; j < n; ++j)
                rs += std::abs(A[i][j]);
            normInf = std::max(normInf, rs);
        }
        ss << "||A||_1 = " << fmt(norm1) << "\n";
        ss << "||A||_∞ = " << fmt(normInf) << "\n";
        ss << "Frobenius norm = ";
        double frob = 0;
        for (auto &row : A)
            for (double v : row)
                frob += v * v;
        ss << fmt(std::sqrt(frob)) << "\n\n";
        ss << "κ(A) = ||A||·||A⁻¹||  (use LU decomposition for exact value)\n";
        ss << "Large κ → ill-conditioned; loss of ≈log10(κ) decimal digits in solution";
        return ok(ss.str());
    }

    NAResult powerIteration(const Mat &A, double tol, int maxIter)
    {
        int n = A.size();
        Vec v(n, 1.0 / std::sqrt(n));
        double lambda = 0;
        std::ostringstream ss;
        ss << "Power Iteration\n\n";
        int k = 0;
        for (; k < maxIter; ++k)
        {
            Vec Av(n, 0);
            for (int i = 0; i < n; ++i)
                for (int j = 0; j < n; ++j)
                    Av[i] += A[i][j] * v[j];
            double norm = 0;
            for (double vi : Av)
                norm += vi * vi;
            norm = std::sqrt(norm);
            double newLambda = 0;
            for (int i = 0; i < n; ++i)
                newLambda += Av[i] * v[i];
            for (auto &vi : Av)
                vi /= norm;
            if (std::abs(newLambda - lambda) < tol)
            {
                lambda = newLambda;
                v = Av;
                k++;
                break;
            }
            lambda = newLambda;
            v = Av;
        }
        ss << "Dominant eigenvalue: λ₁ ≈ " << fmt(lambda) << "\n";
        ss << "Eigenvector: [";
        for (int i = 0; i < n; ++i)
        {
            if (i)
                ss << ",";
            ss << fmt(v[i], 6);
        }
        ss << "]\nIterations: " << k;
        return ok(fmt(lambda), ss.str(), k);
    }

    NAResult inverseIteration(const Mat &A, double shift, double tol, int maxIter)
    {
        // Find eigenvalue closest to shift
        int n = A.size();
        std::ostringstream ss;
        ss << "Inverse Iteration (shift μ=" << shift << ")\n";
        ss << "Finds eigenvalue closest to μ\n\n";
        // (A - μI)^{-1} via Gaussian elimination at each step
        Mat Ashift = A;
        for (int i = 0; i < n; ++i)
            Ashift[i][i] -= shift;
        Vec v(n, 1.0 / std::sqrt(n));
        double lambda = shift;
        int k = 0;
        for (; k < maxIter; ++k)
        {
            // Solve (A-μI)w = v
            Mat Aug(n, Vec(n + 1));
            for (int i = 0; i < n; ++i)
        {
                for (int j = 0; j < n; ++j)
                    Aug[i][j] = Ashift[i][j];
                Aug[i][n] = v[i];
            }
            for (int col = 0; col < n; ++col)
            {
                int piv = col;
                for (int r = col + 1; r < n; ++r)
                    if (std::abs(Aug[r][col]) > std::abs(Aug[piv][col]))
                        piv = r;
                std::swap(Aug[col], Aug[piv]);
                if (std::abs(Aug[col][col]) < 1e-14)
                    break;
                double d = Aug[col][col];
                for (int j = col; j <= n; ++j)
                    Aug[col][j] /= d;
                for (int r = 0; r < n; ++r)
                {
                    if (r == col)
                        continue;
                    double f = Aug[r][col];
                    for (int j = col; j <= n; ++j)
                        Aug[r][j] -= f * Aug[col][j];
                }
            }
            Vec w(n);
            for (int i = 0; i < n; ++i)
                w[i] = Aug[i][n];
            double norm = 0;
            for (double wi : w)
                norm += wi * wi;
            norm = std::sqrt(norm);
            double newLambda = shift + norm;
            for (auto &wi : w)
                wi /= norm;
            if (std::abs(newLambda - lambda) < tol)
            {
                lambda = newLambda;
                v = w;
                k++;
                break;
            }
            lambda = newLambda;
            v = w;
        }
        ss << "Eigenvalue ≈ " << fmt(lambda) << "\nIterations: " << k;
        return ok(fmt(lambda), ss.str(), k);
    }

    NAResult luDecomp(const Mat &A, const Vec &b)
    {
        int n = A.size();
        Mat L(n, Vec(n, 0)), U = A;
        std::vector<int> perm(n);
        std::iota(perm.begin(), perm.end(), 0);
        Vec bPerm = b;
        for (int col = 0; col < n; ++col)
        {
            int piv = col;
            for (int r = col + 1; r < n; ++r)
                if (std::abs(U[r][col]) > std::abs(U[piv][col]))
                    piv = r;
            std::swap(U[col], U[piv]);
            std::swap(L[col], L[piv]);
            std::swap(bPerm[col], bPerm[piv]);
            L[col][col] = 1;
            for (int r = col + 1; r < n; ++r)
            {
                if (std::abs(U[col][col]) < 1e-14)
                    continue;
                L[r][col] = U[r][col] / U[col][col];
                for (int j = col; j < n; ++j)
                    U[r][j] -= L[r][col] * U[col][j];
            }
        }
        // Forward sub Ly=b
        Vec y(n);
        for (int i = 0; i < n; ++i)
        {
            y[i] = bPerm[i];
            for (int j = 0; j < i; ++j)
                y[i] -= L[i][j] * y[j];
        }
        // Back sub Ux=y
        Vec x(n);
        for (int i = n - 1; i >= 0; --i)
        {
            x[i] = y[i];
            for (int j = i + 1; j < n; ++j)
                x[i] -= U[i][j] * x[j];
            if (std::abs(U[i][i]) < 1e-14)
                return err("Singular");
            x[i] /= U[i][i];
        }
        std::ostringstream ss;
        ss << "LU Decomposition (with partial pivoting)\nSolution:\n";
        for (int i = 0; i < n; ++i)
            ss << "  x_" << i + 1 << " = " << fmt(x[i]) << "\n";
        return ok(ss.str());
    }

    NAResult choleskyNum(const Mat &A, const Vec &b)
    {
        int n = A.size();
        Mat L(n, Vec(n, 0));
        for (int i = 0; i < n; ++i)
        {
            for (int j = 0; j <= i; ++j)
            {
                double sum = A[i][j];
                for (int k = 0; k < j; ++k)
                    sum -= L[i][k] * L[j][k];
                if (i == j)
                {
                    if (sum <= 0)
                        return err("Matrix not positive definite");
                    L[i][j] = std::sqrt(sum);
                }
                else
                    L[i][j] = sum / L[j][j];
            }
        }
        // Forward sub
        Vec y(n);
        for (int i = 0; i < n; ++i)
        {
            y[i] = b[i];
            for (int j = 0; j < i; ++j)
                y[i] -= L[i][j] * y[j];
            y[i] /= L[i][i];
        }
        // Back sub L^T x = y
        Vec x(n);
        for (int i = n - 1; i >= 0; --i)
        {
            x[i] = y[i];
            for (int j = i + 1; j < n; ++j)
                x[i] -= L[j][i] * x[j];
            x[i] /= L[i][i];
        }
        std::ostringstream ss;
        ss << "Cholesky Decomposition (A = LL^T, for SPD matrices)\n Solution:\n";
        for (int i = 0; i < n; ++i)
            ss << "  x_" << i + 1 << " = " << fmt(x[i]) << "\n";
        return ok(ss.str());
    }

    NAResult qrAlgorithmNum(const Mat &A, int maxIter)
    {
        // Basic QR algorithm via Householder reflectors (simplified)
        int n = A.size();
        Mat Ak = A;
        std::ostringstream ss;
        ss << "QR Algorithm for Eigenvalues\n\n";
        for (int iter = 0; iter < maxIter; ++iter)
        {
            // QR decomposition via Gram-Schmidt
            Mat Q(n, Vec(n, 0)), R(n, Vec(n, 0));
            for (int j = 0; j < n; ++j)
            {
                Vec v(n);
                for (int i = 0; i < n; ++i)
                    v[i] = Ak[i][j];
                for (int k = 0; k < j; ++k)
                {
                    double dot = 0;
                    for (int i = 0; i < n; ++i)
                        dot += Q[i][k] * v[i];
                    R[k][j] = dot;
                    for (int i = 0; i < n; ++i)
                        v[i] -= dot * Q[i][k];
                }
                double norm = 0;
                for (double vi : v)
                    norm += vi * vi;
                norm = std::sqrt(norm);
                R[j][j] = norm;
                if (norm > 1e-14)
                    for (int i = 0; i < n; ++i)
                        Q[i][j] = v[i] / norm;
            }
            // A_{k+1} = R Q
            Mat newAk(n, Vec(n, 0));
            for (int i = 0; i < n; ++i)
                for (int j = 0; j < n; ++j)
                    for (int k = 0; k < n; ++k)
                        newAk[i][j] += R[i][k] * Q[k][j];
            Ak = newAk;
            // Check convergence (off-diagonal norm)
            double offDiag = 0;
            for (int i = 0; i < n; ++i)
                for (int j = 0; j < n; ++j)
                    if (i != j)
                        offDiag += Ak[i][j] * Ak[i][j];
            if (std::sqrt(offDiag) < 1e-10)
                break;
        }
        ss << "Eigenvalues (diagonal of converged matrix):\n";
        for (int i = 0; i < n; ++i)
            ss << "  λ_" << i + 1 << " ≈ " << fmt(Ak[i][i]) << "\n";
        return ok(ss.str());
    }

    // ── Error analysis ─────────────────────────────────────────────────────────────

    NAResult truncationError(const std::string &method, int order, double h)
    {
        std::ostringstream ss;
        ss << "Truncation Error Analysis Method: " << method << "\nOrder: " << order << "\n h = " << h << "\n\n";
        ss << "Error = O(h^" << order << ") ≈ " << fmt(std::pow(h, order)) << "\n\n";
        ss << "If h is halved:\n";
        ss << "  New error ≈ " << fmt(std::pow(h / 2, order)) << "\n";
        ss << "  Error reduction factor: " << fmt(std::pow(2, order)) << "×\n\n";
        ss << "Global error = O(h^" << order << ") for " << order << "-th order method";
        return ok(fmt(std::pow(h, order)), ss.str());
    }

    NAResult roundoffError(double computed, double exact)
    {
        double abs_err = std::abs(computed - exact);
        double rel_err = std::abs(exact) > 1e-15 ? abs_err / std::abs(exact) : abs_err;
        int sig_digits = (rel_err > 1e-15) ? (int)(-std::log10(rel_err)) : 15;
        std::ostringstream ss;
        ss << "Error Analysis\n";
        ss << "Computed value:  " << fmt(computed) << "\n";
        ss << "Exact value:     " << fmt(exact) << "\n";
        ss << "Absolute error:  " << fmt(abs_err) << "\n";
        ss << "Relative error:  " << fmt(rel_err) << "\n";
        ss << "Significant digits: ≈" << sig_digits << "\n";
        ss << "(Machine epsilon ε_M ≈ 2.2×10⁻¹⁶ for double precision)";
        return ok(fmt(abs_err), ss.str());
    }

    NAResult convergenceOrder(const Vec &errors)
    {
        if (errors.size() < 2)
            return err("Need at least 2 error values");
        std::ostringstream ss;
        ss << "Convergence Order Estimation\n\nError sequence: ";
        for (double e : errors)
            ss << fmt(e, 4) << " ";
        ss << "\n\nRatios e_n/e_{n+1}:\n";
        for (size_t i = 0; i + 1 < errors.size(); ++i)
        {
            double ratio = errors[i] / errors[i + 1];
            double order = std::log(ratio) / std::log(2.0);
            ss << "  " << fmt(errors[i], 4) << "/" << fmt(errors[i + 1], 4) << " = " << fmt(ratio, 4) << "  → order ≈ " << fmt(order, 3) << "\n";
        }
        return ok(ss.str());
    }

    // ── Fast Fourier Transform ───────────────â───────────────────────────
    // Iterative, in-place, radix-2 Cooley-Tukey FFT. Input is zero-padded to the
    // next power of 2 if needed. Operates on complex<double> in-place.
    static void fftInPlace(std::vector<std::complex<double>> &a, bool inverse)
    {
        size_t n = a.size();
        if (n <= 1) return;

        // Bit-reversal permutation
        for (size_t i = 1, j = 0; i < n; ++i)
        {
            size_t bit = n >> 1;
            for (; j & bit; bit >>= 1)
                j ^= bit;
            j ^= bit;
            if (i < j)
                std::swap(a[i], a[j]);
        }

        for (size_t len = 2; len <= n; len <<= 1)
        {
            double ang = 2 * M_PI / (double)len * (inverse ? 1 : -1);
            std::complex<double> wlen(std::cos(ang), std::sin(ang));
            for (size_t i = 0; i < n; i += len)
            {
                std::complex<double> w(1);
                for (size_t k = 0; k < len / 2; ++k)
                {
                    std::complex<double> u = a[i + k];
                    std::complex<double> v = a[i + k + len / 2] * w;
                    a[i + k]           = u + v;
                    a[i + k + len / 2] = u - v;
                    w *= wlen;
                }
            }
        }

        if (inverse)
            for (auto &x : a)
                x /= (double)n;
    }

    static size_t nextPow2(size_t n)
    {
        size_t p = 1;
        while (p < n) p <<= 1;
        return p == 0 ? 1 : p;
    }

    NAResult fastFourierTransform(const std::string &dataStr, bool inverse)
    {
        Vec real = parseVec(dataStr);
        if (real.empty())
            return err("FFT requires at least one input value (e.g. fft[1,1,0,-1,-1,-1,0,1])");

        size_t n0 = real.size();
        size_t n  = nextPow2(n0);

        std::vector<std::complex<double>> a(n, std::complex<double>(0, 0));
        for (size_t i = 0; i < n0; ++i)
            a[i] = std::complex<double>(real[i], 0.0);

        fftInPlace(a, inverse);

        std::ostringstream ss;
        ss << std::fixed << std::setprecision(6);
        if (n0 != n)
            ss << "(zero-padded " << n0 << " -> " << n << " samples)\n";
        ss << (inverse ? "Inverse FFT" : "FFT") << " (" << n << "-point):\n";
        ss << "  k    Re          Im          |X[k]|      phase(rad)\n";
        for (size_t k = 0; k < n; ++k)
        {
            double re  = a[k].real();
            double im  = a[k].imag();
            double mag = std::sqrt(re * re + im * im);
            double ph  = std::atan2(im, re);
            ss << "  " << std::setw(2) << k << "   "
               << std::setw(10) << re << "  "
               << std::setw(10) << im << "  "
               << std::setw(10) << mag << "  "
               << std::setw(10) << ph << "\n";
        }

        NAResult r;
        r.ok    = true;
        r.value = ss.str();
        return r;
    }

    // ── Dispatch ───────────────────────────â──────────────────────────

    NAResult dispatch(const std::string &op, const std::string &json)
    {
        try
        {
            std::string f = getP(json, "f"), x = getP(json, "x");
            if (x.empty())
                x = "x";
            Vec xs = parseVec(getP(json, "xs")), ys = parseVec(getP(json, "ys"));
            Mat A = parseMat(getP(json, "A"));
            Vec b = parseVec(getP(json, "b"));
            double a = getN(json, "a"), bv = getN(json, "b"), tol = getN(json, "tol", 1e-10);
            int n = (int)getN(json, "n", 5), maxIter = (int)getN(json, "maxIter", 100);

            if (op == "bisection")
                return bisection(f, x, a, bv, tol, maxIter);
            if (op == "newton")
                return newtonRaphson(f, x, getN(json, "x0"), tol, maxIter);
            if (op == "secant")
                return secantMethod(f, x, getN(json, "x0"), getN(json, "x1"), tol, maxIter);
            if (op == "regula_falsi")
                return regulaFalsi(f, x, a, bv, tol, maxIter);
            if (op == "fixed_point")
                return fixedPoint(f, x, getN(json, "x0"), tol, maxIter);
            if (op == "muller")
                return mullerMethod(f, x, getN(json, "x0"), getN(json, "x1"), getN(json, "x2"), tol, 50);
            if (op == "brent")
                return brentsMethod(f, x, a, bv, tol);
            if (op == "all_roots")
                return allRootsInterval(f, x, a, bv, (int)getN(json, "subs", 100));
            if (op == "lagrange")
                return lagrangeInterp(xs, ys, getN(json, "xeval"));
            if (op == "newton_dd")
                return newtonDivDiff(xs, ys, getN(json, "xeval"));
            if (op == "neville")
                return neville(xs, ys, getN(json, "xeval"));
            if (op == "cubic_spline")
                return splineCubic(xs, ys, getN(json, "xeval"));
            if (op == "linear_spline")
                return splineLinear(xs, ys, getN(json, "xeval"));
            if (op == "hermite")
                return hermiteInterp(xs, ys, parseVec(getP(json, "ders")), getN(json, "xeval"));
            if (op == "cheb_nodes")
                return chebychevNodes(n, a, bv);
            if (op == "poly_fit")
                return polynomialFit(xs, ys, (int)getN(json, "degree", 2));
            if (op == "forward_diff")
                return forwardDiff(f, x, getN(json, "x0"), getN(json, "h", 1e-5));
            if (op == "central_diff")
                return centralDiff(f, x, getN(json, "x0"), getN(json, "h", 1e-5));
            if (op == "second_deriv")
                return secondDeriv(f, x, getN(json, "x0"), getN(json, "h", 1e-5));
            if (op == "richardson_diff")
                return richardsonDiff(f, x, getN(json, "x0"), getN(json, "h", 0.1));
            if (op == "trapezoidal")
                return trapezoidalRule(f, x, a, bv, n);
            if (op == "simpsons")
                return simpsonsRule(f, x, a, bv, n);
            if (op == "simpsons38")
                return simpsons38Rule(f, x, a, bv, n);
            if (op == "gauss_legendre")
                return gaussLegendre(f, x, a, bv, n);
            if (op == "gauss_chebyshev")
                return gaussChebyshev(f, x, n);
            if (op == "gauss_laguerre")
                return gaussLaguerre(f, x, n);
            if (op == "gauss_hermite")
                return gaussHermite(f, x, n);
            if (op == "romberg")
                return romberg(f, x, a, bv, (int)getN(json, "levels", 6));
            if (op == "adaptive_quad")
                return adaptiveQuad(f, x, a, bv, getN(json, "tol", 1e-8));
            if (op == "gauss_elim")
                return gaussianElim(A, b);
            if (op == "lu")
                return luDecomp(A, b);
            if (op == "cholesky")
                return choleskyNum(A, b);
            if (op == "jacobi_iter")
                return jacobiIter(A, b, tol, maxIter);
            if (op == "gauss_seidel")
                return gaussSeidelIter(A, b, tol, maxIter);
            if (op == "sor")
                return sorIter(A, b, getN(json, "omega", 1.5), tol, maxIter);
            if (op == "conj_grad")
                return conjugateGradient(A, b, tol, maxIter);
            if (op == "cond_num")
                return conditionNumber(A);
            if (op == "power_iter")
                return powerIteration(A, tol, maxIter);
            if (op == "inverse_iter")
                return inverseIteration(A, getN(json, "shift"), tol, maxIter);
            if (op == "qr_num")
                return qrAlgorithmNum(A, maxIter);
            if (op == "trunc_error")
                return truncationError(getP(json, "method"),
                                       (int)getN(json, "order", 2), getN(json, "h", 0.1));
            if (op == "roundoff")
                return roundoffError(getN(json, "computed"), getN(json, "exact"));
            if (op == "conv_order")
                return convergenceOrder(parseVec(getP(json, "errors")));
            if (op == "fft")
                return fastFourierTransform(getP(json, "data"), false);
            if (op == "ifft")
                return fastFourierTransform(getP(json, "data"), true);
            return err("Unknown numerical analysis operation: " + op);
        }
        catch (const std::exception &e)
        {
            return err(e.what());
        }
    }

} // namespace NumericalAnalysis
