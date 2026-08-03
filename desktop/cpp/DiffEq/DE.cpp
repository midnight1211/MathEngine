// DifferentialEquations.cpp

#include "DE.hpp"
#include "../CommonUtils.hpp"

// ── CommonUtils aliases (replaces previous local duplicates) ──────────────────
static auto getP = [](const std::string &j, const std::string &k, const std::string &d = "")
{
    return cu_getStr(j, k, d);
};
static auto getParam = [](const std::string &j, const std::string &k, const std::string &d = "")
{
    return cu_getParam(j, k, d);
};
static auto getN = [](const std::string &j, const std::string &k, double d = 0.0)
{ return cu_getNum(j, k, d); };
static auto getInt_ = [](const std::string &j, const std::string &k, long long d = 0)
{ return cu_getInt(j, k, d); };
static auto parseVec = [](const std::string &s)
{ return cu_parseVecD(s); };
static auto parseMat = [](const std::string &s)
{ return cu_parseMat(s); };
static auto fmt = [](double v, int p = 8)
{ return cu_fmt(v, p); };
static auto evalAt = [](const std::string &expr, const std::string &var, double val)
{
    return cu_evalAt(expr, var, val);
};
static auto &str_ll = cu_str;
#include "../MathCore.hpp"
#include "../Calculus/Calculus.hpp"
#include <cmath>
#include <sstream>
#include <iomanip>
#include <stdexcept>
#include <algorithm>
#include <numeric>

namespace DifferentialEquations
{

    static const double PI = M_PI;

    static DEResult ok(const std::string &sym, const std::string &num = "")
    {
        DEResult r;
        r.symbolic = sym;
        r.numerical = num.empty() ? sym : num;
        return r;
    }
    static DEResult err(const std::string &msg)
    {
        DEResult r;
        r.ok = false;
        r.error = msg;
        return r;
    }

    // ── Parse a simple expression using the calculus bridge ──────────────────────

    // =============================================================================
    // FIRST-ORDER ODEs
    // =============================================================================

    // Separable: dy/dx = f(x)*g(y)  → ∫1/g(y) dy = ∫f(x) dx
    // We attempt symbolic integration via the calculus module.
    DEResult solveSeparable(const std::string &fStr,
                            const std::string &x, const std::string &y)
    {
        try
        {
            // Try to detect separable form by evaluating partial derivatives
            // If ∂f/∂x / f doesn't depend on y, it's separable
            // For now: attempt to split f = g(x)*h(y) heuristically
            std::ostringstream ss;
            ss << "dy/" << y << " = f(" << x << "," << y << ") implies separable form.\n";
            ss << "Separate: ∫ 1/h(" << y << ") d" << y << " = ∫ g(" << x << ") d" << x << " + C\n";
            ss << "Expression: " << fStr;
            return ok(ss.str());
        }
        catch (const std::exception &e)
        {
            return err(e.what());
        }
    }

    // Linear first-order: y' + P(x)y = Q(x)
    // Integrating factor: μ(x) = e^∫P(x)dx
    // Solution: y = (1/μ) ∫ μQ dx + C/μ
    DEResult solveLinearFirstOrder(const std::string &Pstr, const std::string &Qstr,
                                   const std::string &x, const std::string & /*y*/)
    {
        try
        {
            // Compute ∫P dx symbolically
            auto intResult = Calculus::computeIndefinite(Pstr, x);
            std::string intP = intResult.ok ? intResult.antiderivative : "∫P(x)dx";
            // Remove " + C"
            if (intP.size() > 4 && intP.substr(intP.size() - 4) == " + C")
                intP = intP.substr(0, intP.size() - 4);

            std::ostringstream ss;
            ss << "y' + P(x)y = Q(x)  where  P = " << Pstr << ",  Q = " << Qstr << "\n\n";
            ss << "Integrating factor:  μ(x) = e^(∫P dx) = e^(" << intP << ")\n\n";
            ss << "General solution:\n";
            ss << "  y = (1/μ(x)) * [ ∫ μ(x)·Q(x) dx + C ]\n";
            ss << "  y = e^(-(" << intP << ")) * [ ∫ e^(" << intP << ")·(" << Qstr << ") dx + C ]";
            return ok(ss.str());
        }
        catch (const std::exception &e)
        {
            return err(e.what());
        }
    }

    // Exact equation: M(x,y)dx + N(x,y)dy = 0
    // Exact if ∂M/∂y = ∂N/∂x
    // Solution: F(x,y) = ∫M dx + g(y)  where g'(y) = N - ∂/∂y(∫M dx)
    DEResult solveExact(const std::string &Mstr, const std::string &Nstr,
                        const std::string &x, const std::string &y)
    {
        try
        {
            // Check exactness numerically at a few points
            auto dMdy = Calculus::differentiate(Mstr, y, 1);
            auto dNdx = Calculus::differentiate(Nstr, x, 1);

            bool isExact = true;
            for (double xv : {0.5, 1.0, 1.5})
            {
                for (double yv : {0.5, 1.0, 1.5})
                {
                    double dm = 0, dn = 0;
                    try
                    {
                        auto eM = Calculus::parse(dMdy.symbolic);
                        auto eN = Calculus::parse(dNdx.symbolic);
                        dm = Calculus::evaluate(eM, {{x, xv}, {y, yv}});
                        dn = Calculus::evaluate(eN, {{x, xv}, {y, yv}});
                    }
                    catch (...)
                    {
                    }
                    if (std::abs(dm - dn) > 1e-6)
                    {
                        isExact = false;
                        break;
                    }
                }
                if (!isExact)
                    break;
            }

            std::ostringstream ss;
            ss << "M(x,y) = " << Mstr << ",  N(x,y) = " << Nstr << "\n\n";
            ss << "∂M/∂y = " << (dMdy.ok ? dMdy.symbolic : "?") << "\n";
            ss << "∂N/∂x = " << (dNdx.ok ? dNdx.symbolic : "?") << "\n";
            ss << "Exactness check: " << (isExact ? "EXACT" : "NOT EXACT") << "\n\n";
            if (isExact)
            {
                auto Fxy = Calculus::computeIndefinite(Mstr, x);
                ss << "F(x,y) = ∫M dx = " << (Fxy.ok ? Fxy.antiderivative : "∫M dx") << "\n";
                ss << "General solution: F(x,y) = C";
            }
            else
            {
                ss << "Equation is not exact. Consider an integrating factor.";
            }
            return ok(ss.str());
        }
        catch (const std::exception &e)
        {
            return err(e.what());
        }
    }

    // Bernoulli: y' + P(x)y = Q(x)y^n
    // Substitution v = y^(1-n)  → v' + (1-n)P(x)v = (1-n)Q(x)  (linear)
    DEResult solveBernoulli(const std::string &Pstr, const std::string &Qstr,
                            const std::string &nStr, const std::string &x)
    {
        try
        {
            double n = std::stod(nStr);
            double m = 1.0 - n;
            std::ostringstream ss;
            ss << "Bernoulli ODE: y' + (" << Pstr << ")y = (" << Qstr << ")y^" << nStr << "\n\n";
            ss << "Substitution: v = y^(1-n) = y^" << fmt(m) << "\n";
            ss << "Transformed linear ODE: v' + " << fmt(m) << "·(" << Pstr << ")v = " << fmt(m) << "·(" << Qstr << ")\n\n";
            ss << "Solve via integrating factor method:\n";
            ss << "  μ(x) = e^(∫ " << fmt(m) << "·(" << Pstr << ") dx)\n";
            ss << "  v = (1/μ) ∫ μ · " << fmt(m) << "·(" << Qstr << ") dx + C/μ\n";
            ss << "  y = v^(1/" << fmt(m) << ")";
            return ok(ss.str());
        }
        catch (const std::exception &e)
        {
            return err(e.what());
        }
    }

    // =============================================================================
    // SECOND-ORDER CONSTANT COEFFICIENT ODEs
    // ay'' + by' + cy = 0
    // =============================================================================

    DEResult solveHomogeneous2nd(double a, double b, double c)
    {
        try
        {
            // Characteristic equation: ar² + br + c = 0
            double disc = b * b - 4 * a * c;
            std::ostringstream ss;
            ss << "ODE: " << fmt(a) << "y'' + " << fmt(b) << "y' + " << fmt(c) << "y = 0\n";
            ss << "Characteristic equation: " << fmt(a) << "r² + " << fmt(b) << "r + " << fmt(c) << " = 0\n";
            ss << "Discriminant: " << fmt(disc) << "\n\n";

            if (disc > 1e-10)
            {
                double r1 = (-b + std::sqrt(disc)) / (2 * a);
                double r2 = (-b - std::sqrt(disc)) / (2 * a);
                ss << "Two real roots: r₁ = " << fmt(r1) << ", r₂ = " << fmt(r2) << "\n";
                ss << "General solution: y = C₁e^(" << fmt(r1) << "x) + C₂e^(" << fmt(r2) << "x)";
            }
            else if (std::abs(disc) < 1e-10)
            {
                double r = -b / (2 * a);
                ss << "Repeated root: r = " << fmt(r) << "\n";
                ss << "General solution: y = (C₁ + C₂x)e^(" << fmt(r) << "x)";
            }
            else
            {
                double alpha = -b / (2 * a);
                double beta = std::sqrt(-disc) / (2 * a);
                ss << "Complex roots: r = " << fmt(alpha) << " ± " << fmt(beta) << "i\n";
                ss << "General solution: y = e^(" << fmt(alpha) << "x)[C₁cos(" << fmt(beta) << "x) + C₂sin(" << fmt(beta) << "x)]";
            }
            return ok(ss.str());
        }
        catch (const std::exception &e)
        {
            return err(e.what());
        }
    }

    // ay'' + by' + cy = g(x) — undetermined coefficients
    DEResult solveUndeterminedCoeff(double a, double b, double c,
                                    const std::string &g, const std::string &x)
    {
        try
        {
            auto homog = solveHomogeneous2nd(a, b, c);
            std::ostringstream ss;
            ss << "Non-homogeneous ODE: " << fmt(a) << "y'' + " << fmt(b) << "y' + " << fmt(c) << "y = " << g << "\n\n";
            ss << "=== Homogeneous solution (y_h) ===\n"
               << homog.symbolic << "\n\n";
            ss << "=== Particular solution (y_p) — Undetermined Coefficients ===\n";
            ss << "Guess form based on g(x) = " << g << ":\n";
            // Detect form of g
            std::string gl = g;
            if (gl.find("sin") != std::string::npos || gl.find("cos") != std::string::npos)
                ss << "  y_p = A·cos(ωx) + B·sin(ωx)\n";
            else if (gl.find("^") != std::string::npos || gl.find("x") != std::string::npos)
                ss << "  y_p = A_n·x^n + ... + A₁x + A₀  (polynomial of same degree)\n";
            else if (gl.find("exp") != std::string::npos || gl.find("e^") != std::string::npos)
                ss << "  y_p = Ae^(αx)  (if α not a root of characteristic eq)\n";
            else
                ss << "  y_p = A  (constant)\n";
            ss << "Substitute y_p into ODE and solve for coefficients.\n\n";
            ss << "=== General solution ===\n  y = y_h + y_p";
            return ok(ss.str());
        }
        catch (const std::exception &e)
        {
            return err(e.what());
        }
    }

    // Variation of parameters
    DEResult solveVariationOfParams(double a, double b, double c,
                                    const std::string &g, const std::string &x)
    {
        try
        {
            auto homog = solveHomogeneous2nd(a, b, c);
            double disc = b * b - 4 * a * c;
            std::ostringstream ss;
            ss << "Non-homogeneous ODE: " << fmt(a) << "y'' + " << fmt(b) << "y' + " << fmt(c) << "y = " << g << "\n\n";
            ss << "=== Homogeneous solution ===\n"
               << homog.symbolic << "\n\n";
            ss << "=== Variation of Parameters ===\n";
            ss << "Let y_h = C₁y₁ + C₂y₂.  Assume y_p = u₁(x)y₁ + u₂(x)y₂\n";
            ss << "Wronskian: W = y₁y₂' - y₂y₁'\n";
            ss << "u₁' = -y₂·g(x)/W,  u₂' = y₁·g(x)/W\n";
            ss << "u₁ = -∫y₂·g(x)/W dx,  u₂ = ∫y₁·g(x)/W dx\n\n";
            ss << "With g(x) = " << g << " / " << fmt(a) << " (standard form)\n\n";
            ss << "General solution: y = y_h + u₁y₁ + u₂y₂";
            return ok(ss.str());
        }
        catch (const std::exception &e)
        {
            return err(e.what());
        }
    }

    // Cauchy-Euler: ax²y'' + bxy' + cy = 0
    // Substitution x = e^t, or try y = x^m → characteristic eq
    DEResult solveCauchyEuler(double a, double b, double c)
    {
        try
        {
            // Characteristic equation: am(m-1) + bm + c = 0 → am² + (b-a)m + c = 0
            double A = a, B = b - a, C = c;
            double disc = B * B - 4 * A * C;
            std::ostringstream ss;
            ss << "Cauchy-Euler: " << fmt(a) << "x²y'' + " << fmt(b) << "xy' + " << fmt(c) << "y = 0\n";
            ss << "Characteristic equation: " << fmt(A) << "m² + " << fmt(B) << "m + " << fmt(C) << " = 0\n";
            ss << "Discriminant: " << fmt(disc) << "\n\n";

            if (disc > 1e-10)
            {
                double m1 = (-B + std::sqrt(disc)) / (2 * A);
                double m2 = (-B - std::sqrt(disc)) / (2 * A);
                ss << "Two real roots: m₁ = " << fmt(m1) << ", m₂ = " << fmt(m2) << "\n";
                ss << "General solution: y = C₁x^" << fmt(m1) << " + C₂x^" << fmt(m2);
            }
            else if (std::abs(disc) < 1e-10)
            {
                double m = -B / (2 * A);
                ss << "Repeated root: m = " << fmt(m) << "\n";
                ss << "General solution: y = x^" << fmt(m) << "(C₁ + C₂ln|x|)";
            }
            else
            {
                double alpha = -B / (2 * A);
                double beta = std::sqrt(-disc) / (2 * A);
                ss << "Complex roots: m = " << fmt(alpha) << " ± " << fmt(beta) << "i\n";
                ss << "General solution: y = x^" << fmt(alpha)
                   << "[C₁cos(" << fmt(beta) << "ln|x|) + C₂sin(" << fmt(beta) << "ln|x|)]";
            }
            return ok(ss.str());
        }
        catch (const std::exception &e)
        {
            return err(e.what());
        }
    }

    // =============================================================================
    // NUMERICAL METHODS
    // =============================================================================

    NumericalDEResult eulerMethod(const Func &f, double t0, double y0, double t1, int n)
    {
        NumericalDEResult r;
        r.method = "Euler";
        double h = (t1 - t0) / n;
        r.t.resize(n + 1);
        r.y.resize(n + 1);
        r.t[0] = t0;
        r.y[0] = y0;
        for (int i = 0; i < n; ++i)
        {
            r.y[i + 1] = r.y[i] + h * f(r.t[i], r.y[i]);
            r.t[i + 1] = r.t[i] + h;
        }
        r.summary = "y(" + fmt(t1) + ") ≈ " + fmt(r.y.back());
        return r;
    }

    NumericalDEResult improvedEuler(const Func &f, double t0, double y0, double t1, int n)
    {
        NumericalDEResult r;
        r.method = "Improved Euler (Heun)";
        double h = (t1 - t0) / n;
        r.t.resize(n + 1);
        r.y.resize(n + 1);
        r.t[0] = t0;
        r.y[0] = y0;
        for (int i = 0; i < n; ++i)
        {
            double k1 = f(r.t[i], r.y[i]);
            double k2 = f(r.t[i] + h, r.y[i] + h * k1);
            r.y[i + 1] = r.y[i] + h / 2 * (k1 + k2);
            r.t[i + 1] = r.t[i] + h;
        }
        r.summary = "y(" + fmt(t1) + ") ≈ " + fmt(r.y.back());
        return r;
    }

    NumericalDEResult rk2(const Func &f, double t0, double y0, double t1, int n)
    {
        NumericalDEResult r;
        r.method = "RK2 (Midpoint)";
        double h = (t1 - t0) / n;
        r.t.resize(n + 1);
        r.y.resize(n + 1);
        r.t[0] = t0;
        r.y[0] = y0;
        for (int i = 0; i < n; ++i)
        {
            double k1 = f(r.t[i], r.y[i]);
            double k2 = f(r.t[i] + h / 2, r.y[i] + h / 2 * k1);
            r.y[i + 1] = r.y[i] + h * k2;
            r.t[i + 1] = r.t[i] + h;
        }
        r.summary = "y(" + fmt(t1) + ") ≈ " + fmt(r.y.back());
        return r;
    }

    NumericalDEResult rk4(const Func &f, double t0, double y0, double t1, int n)
    {
        NumericalDEResult r;
        r.method = "RK4";
        double h = (t1 - t0) / n;
        r.t.resize(n + 1);
        r.y.resize(n + 1);
        r.t[0] = t0;
        r.y[0] = y0;
        for (int i = 0; i < n; ++i)
        {
            double k1 = f(r.t[i], r.y[i]);
            double k2 = f(r.t[i] + h / 2, r.y[i] + h / 2 * k1);
            double k3 = f(r.t[i] + h / 2, r.y[i] + h / 2 * k2);
            double k4 = f(r.t[i] + h, r.y[i] + h * k3);
            r.y[i + 1] = r.y[i] + h / 6 * (k1 + 2 * k2 + 2 * k3 + k4);
            r.t[i + 1] = r.t[i] + h;
        }
        r.summary = "y(" + fmt(t1) + ") ≈ " + fmt(r.y.back());
        return r;
    }

    // RK45 adaptive (Dormand-Prince coefficients)
    NumericalDEResult rk45(const Func &f, double t0, double y0, double t1, double tol)
    {
        NumericalDEResult r;
        r.method = "RK45 (adaptive, Dormand-Prince)";
        double t = t0, y = y0;
        double h = (t1 - t0) / 100.0;
        r.t.push_back(t);
        r.y.push_back(y);

        while (t < t1 - 1e-12)
        {
            if (t + h > t1)
                h = t1 - t;
            // Dormand-Prince coefficients
            double k1 = f(t, y);
            double k2 = f(t + h / 5, y + h / 5 * k1);
            double k3 = f(t + 3 * h / 10, y + h * (3 * k1 / 40 + 9 * k2 / 40));
            double k4 = f(t + 4 * h / 5, y + h * (44 * k1 / 45 - 56 * k2 / 15 + 32 * k3 / 9));
            double k5 = f(t + 8 * h / 9, y + h * (19372 * k1 / 6561 - 25360 * k2 / 2187 + 64448 * k3 / 6561 - 212 * k4 / 729));
            double k6 = f(t + h, y + h * (9017 * k1 / 3168 - 355 * k2 / 33 + 46732 * k3 / 5247 + 49 * k4 / 176 - 5103 * k5 / 18656));
            double y5 = y + h * (35 * k1 / 384 + 71 * k3 / 1520 - 135 * k4 / 760 + 11 * k5 / 80 - 1 * k6 / 160); // unused but needed
            double k7 = f(t + h, y5);
            double y4 = y + h * (5179 * k1 / 57600 + 7571 * k3 / 16695 + 393 * k4 / 640 - 92097 * k5 / 339200 + 187 * k6 / 2100 + 1.0 / 40 * k7);
            double err = std::abs(y5 - y4);
            if (err < tol || h < 1e-12)
            {
                t += h;
                y = y5;
                r.t.push_back(t);
                r.y.push_back(y);
            }
            double scale = err > 1e-15 ? 0.9 * std::pow(tol / err, 0.2) : 2.0;
            h *= std::max(0.1, std::min(scale, 5.0));
        }
        r.summary = "y(" + fmt(t1) + ") ≈ " + fmt(r.y.back()) +
                    " (" + std::to_string(r.t.size()) + " steps)";
        return r;
    }

    NumericalDEResult adamsBashforth(const Func &f, double t0, double y0, double t1, int n)
    {
        NumericalDEResult r;
        r.method = "Adams-Bashforth 4-step";
        double h = (t1 - t0) / n;
        r.t.resize(n + 1);
        r.y.resize(n + 1);
        r.t[0] = t0;
        r.y[0] = y0;
        // Bootstrap with RK4
        auto boot = rk4(f, t0, y0, t0 + 3 * h, 3);
        for (int i = 1; i <= 3 && i <= n; ++i)
        {
            r.t[i] = boot.t[i];
            r.y[i] = boot.y[i];
        }
        // AB4 predictor
        for (int i = 3; i < n; ++i)
        {
            double f0 = f(r.t[i], r.y[i]), f1 = f(r.t[i - 1], r.y[i - 1]);
            double f2 = f(r.t[i - 2], r.y[i - 2]), f3 = f(r.t[i - 3], r.y[i - 3]);
            r.y[i + 1] = r.y[i] + h / 24 * (55 * f0 - 59 * f1 + 37 * f2 - 9 * f3);
            r.t[i + 1] = r.t[i] + h;
        }
        r.summary = "y(" + fmt(t1) + ") ≈ " + fmt(r.y.back());
        return r;
    }

    // =============================================================================
    // LAPLACE TRANSFORMS
    // =============================================================================

    DEResult laplaceTransform(const std::string &fStr,
                              const std::string &t, const std::string &s)
    {
        // Common Laplace pairs
        std::ostringstream ss;
        ss << "L{" << fStr << "}:\n\n";
        ss << "Common transforms (variable " << t << " → " << s << "):\n";

        // Try to recognise the form
        std::string fl = fStr;
        if (fl == "1")
            ss << "  L{1} = 1/s";
        else if (fl == t)
            ss << "  L{t} = 1/s²";
        else if (fl.find("e^") != std::string::npos)
            ss << "  L{e^(at)} = 1/(s-a)  [identify a from exponent]";
        else if (fl.find("sin") != std::string::npos)
            ss << "  L{sin(ωt)} = ω/(s²+ω²)  [identify ω]";
        else if (fl.find("cos") != std::string::npos)
            ss << "  L{cos(ωt)} = s/(s²+ω²)  [identify ω]";
        else if (fl.find("^") != std::string::npos)
            ss << "  L{t^n} = n!/s^(n+1)  [identify n]";
        else
            ss << "  L{f(t)} = ∫₀^∞ f(t)e^(-st) dt";
        ss << "\n\n(Full symbolic Laplace requires a CAS — numerical evaluation available via IVP solver)";
        return ok(ss.str());
    }

    DEResult inverseLaplace(const std::string &Fstr,
                            const std::string &s, const std::string &t)
    {
        std::ostringstream ss;
        ss << "L⁻¹{" << Fstr << "}:\n\n";
        ss << "Partial fraction decomposition of F(s) = " << Fstr << "\n";
        ss << "then apply inverse transform table:\n";
        ss << "  L⁻¹{1/s}        = 1\n";
        ss << "  L⁻¹{1/s²}       = t\n";
        ss << "  L⁻¹{1/(s-a)}    = e^(at)\n";
        ss << "  L⁻¹{ω/(s²+ω²)}  = sin(ωt)\n";
        ss << "  L⁻¹{s/(s²+ω²)}  = cos(ωt)\n";
        ss << "  L⁻¹{n!/s^(n+1)} = t^n";
        return ok(ss.str());
    }

    // Solve IVP ay'' + by' + cy = g(t), y(0)=y0, y'(0)=dy0 via Laplace
    DEResult solveIVPLaplace(double a, double b, double c,
                             const std::string &g, const std::string &t,
                             double y0, double dy0)
    {
        std::ostringstream ss;
        ss << "IVP via Laplace Transform:\n";
        ss << fmt(a) << "y'' + " << fmt(b) << "y' + " << fmt(c) << "y = " << g << "\n";
        ss << "y(0) = " << fmt(y0) << ",  y'(0) = " << fmt(dy0) << "\n\n";
        ss << "Apply L{·}:\n";
        ss << "  " << fmt(a) << "[s²Y - s·" << fmt(y0) << " - " << fmt(dy0) << "]\n";
        ss << "  + " << fmt(b) << "[sY - " << fmt(y0) << "]\n";
        ss << "  + " << fmt(c) << "Y = G(s)\n\n";
        ss << "Solve for Y(s):\n";
        double ic_term = a * (0.0 * y0 + dy0) + b * y0; // s-coefficient of ICs at s=0 approx
        ss << "  Y(s) = [G(s) + " << fmt(a) << "·s·" << fmt(y0)
           << " + " << fmt(a * dy0 + b * y0) << "] / [" << fmt(a) << "s² + " << fmt(b) << "s + " << fmt(c) << "]\n\n";
        ss << "Then: y(t) = L⁻¹{Y(s)}  (partial fractions + inverse table)";
        return ok(ss.str());
    }

    // =============================================================================
    // PDE — Separation of Variables
    // =============================================================================

    // Heat equation: u_t = α u_xx, u(0,t)=u(L,t)=0, u(x,0)=f(x)
    // Solution: u(x,t) = Σ Bₙ sin(nπx/L) exp(-α(nπ/L)²t)
    DEResult heatEquation(double alpha, double L,
                          const std::string &ic, int terms)
    {
        std::ostringstream ss;
        ss << "Heat Equation: u_t = " << fmt(alpha) << " u_xx\n";
        ss << "BC: u(0,t) = u(" << fmt(L) << ",t) = 0\n";
        ss << "IC: u(x,0) = " << ic << "\n\n";
        ss << "Solution (Fourier sine series):\n";
        ss << "u(x,t) = Σₙ Bₙ sin(nπx/" << fmt(L) << ") exp(-" << fmt(alpha)
           << "(nπ/" << fmt(L) << ")²t)\n\n";
        ss << "Fourier coefficients:\n";
        ss << "  Bₙ = (2/L) ∫₀^L f(x) sin(nπx/L) dx\n\n";
        ss << "First " << terms << " terms:\n";
        for (int n = 1; n <= terms; ++n)
        {
            double lambda = n * PI / L;
            ss << "  n=" << n << ": λₙ = " << fmt(lambda)
               << ",  exp coefficient = " << fmt(-alpha * lambda * lambda) << "\n";
        }
        return ok(ss.str());
    }

    // Wave equation: u_tt = c² u_xx
    DEResult waveEquation(double c, double L,
                          const std::string &f0, const std::string &g0, int terms)
    {
        std::ostringstream ss;
        ss << "Wave Equation: u_tt = " << fmt(c * c) << " u_xx\n";
        ss << "BC: u(0,t) = u(" << fmt(L) << ",t) = 0\n";
        ss << "IC: u(x,0) = " << f0 << ",  u_t(x,0) = " << g0 << "\n\n";
        ss << "Solution:\n";
        ss << "u(x,t) = Σₙ sin(nπx/L)[Aₙcos(nπct/L) + Bₙsin(nπct/L)]\n\n";
        ss << "  Aₙ = (2/L) ∫₀^L f(x) sin(nπx/L) dx\n";
        ss << "  Bₙ = (2/nπc) ∫₀^L g(x) sin(nπx/L) dx\n\n";
        ss << "Wave speed: c = " << fmt(c) << ",  L = " << fmt(L) << "\n";
        ss << "Natural frequencies: ωₙ = nπc/L\n";
        for (int n = 1; n <= terms; ++n)
            ss << "  ω_" << n << " = " << fmt(n * PI * c / L) << " rad/s\n";
        return ok(ss.str());
    }

    // Laplace equation: u_xx + u_yy = 0
    DEResult laplaceEquation(double Lx, double Ly,
                             const std::string &topBC, int terms)
    {
        std::ostringstream ss;
        ss << "Laplace Equation: ∇²u = 0 on [0," << fmt(Lx) << "]×[0," << fmt(Ly) << "]\n";
        ss << "BC: u=0 on sides, u(x," << fmt(Ly) << ") = " << topBC << "\n\n";
        ss << "Solution:\n";
        ss << "u(x,y) = Σₙ Cₙ sin(nπx/Lx) sinh(nπy/Lx) / sinh(nπLy/Lx)\n\n";
        ss << "  Cₙ = (2/Lx) ∫₀^Lx f(x) sin(nπx/Lx) dx\n\n";
        for (int n = 1; n <= terms; ++n)
        {
            double kn = n * PI / Lx;
            ss << "  n=" << n << ": kₙ = " << fmt(kn)
               << ",  sinh(kₙ·Ly)/sinh(kₙ·Lx) = "
               << fmt(std::sinh(kn * Ly) / std::sinh(kn * Lx)) << "\n";
        }
        return ok(ss.str());
    }

    // =============================================================================
    // Formatted entry points
    // =============================================================================

    static double getNum(const std::string &j, const std::string &k, double d = 0.0)
    {
        auto v = getParam(j, k);
        if (v.empty())
            return d;
        try
        {
            return std::stod(v);
        }
        catch (...)
        {
            return d;
        }
    }
    static int getInt(const std::string &j, const std::string &k, int d = 1)
    {
        auto v = getParam(j, k);
        if (v.empty())
            return d;
        try
        {
            return std::stoi(v);
        }
        catch (...)
        {
            return d;
        }
    }

    DEResult dispatch(const std::string &op, const std::string &json, bool exactMode)
    {
        try
        {
            if (op == "homogeneous2nd")
                return solveHomogeneous2nd(getNum(json, "a", 1), getNum(json, "b", 0), getNum(json, "c", 0));
            if (op == "undetermined_coeff")
                return solveUndeterminedCoeff(getNum(json, "a", 1), getNum(json, "b", 0), getNum(json, "c", 0),
                                              getParam(json, "g"), getParam(json, "x", "x"));
            if (op == "variation_params")
                return solveVariationOfParams(getNum(json, "a", 1), getNum(json, "b", 0), getNum(json, "c", 0),
                                              getParam(json, "g"), getParam(json, "x", "x"));
            if (op == "cauchy_euler")
                return solveCauchyEuler(getNum(json, "a", 1), getNum(json, "b", 0), getNum(json, "c", 0));
            if (op == "linear_first")
                return solveLinearFirstOrder(getParam(json, "P"), getParam(json, "Q"),
                                             getParam(json, "x", "x"), getParam(json, "y", "y"));
            if (op == "bernoulli")
                return solveBernoulli(getParam(json, "P"), getParam(json, "Q"),
                                      getParam(json, "n"), getParam(json, "x", "x"));
            if (op == "exact")
                return solveExact(getParam(json, "M"), getParam(json, "N"),
                                  getParam(json, "x", "x"), getParam(json, "y", "y"));
            if (op == "laplace")
                return laplaceTransform(getParam(json, "f"), getParam(json, "t", "t"), getParam(json, "s", "s"));
            if (op == "inverse_laplace")
                return inverseLaplace(getParam(json, "F"), getParam(json, "s", "s"), getParam(json, "t", "t"));
            if (op == "ivp_laplace")
                return solveIVPLaplace(getNum(json, "a", 1), getNum(json, "b", 0), getNum(json, "c", 0),
                                       getParam(json, "g"), getParam(json, "t", "t"),
                                       getNum(json, "y0"), getNum(json, "dy0"));
            if (op == "heat_pde")
                return heatEquation(getNum(json, "alpha", 1), getNum(json, "L", 1),
                                    getParam(json, "ic"), getInt(json, "terms", 5));
            if (op == "wave_pde")
                return waveEquation(getNum(json, "c", 1), getNum(json, "L", 1),
                                    getParam(json, "f0"), getParam(json, "g0"), getInt(json, "terms", 5));
            if (op == "laplace_pde")
                return laplaceEquation(getNum(json, "Lx", 1), getNum(json, "Ly", 1),
                                       getParam(json, "topBC"), getInt(json, "terms", 5));

            // Numerical IVP — build a Func from expression string
            if (op == "euler" || op == "rk4" || op == "rk45" || op == "improved_euler" || op == "rk2" || op == "adams_bashforth")
            {
                std::string fStr = getParam(json, "f");
                std::string tVar = getParam(json, "tvar");
                if (tVar.empty())
                    tVar = "t";
                std::string yVar = getParam(json, "yvar");
                if (yVar.empty())
                    yVar = "y";
                double t0 = getNum(json, "t0", 0), t1 = getNum(json, "t1", 1);
                double y0 = getNum(json, "y0", 1);
                int n = getInt(json, "n", 100);

                auto fExpr = Calculus::parse(fStr);
                Func func = [fExpr, tVar, yVar](double t, double y)
                {
                    return Calculus::evaluate(fExpr, {{tVar, t}, {yVar, y}});
                };

                NumericalDEResult nr;
                if (op == "euler")
                    nr = eulerMethod(func, t0, y0, t1, n);
                else if (op == "improved_euler")
                    nr = improvedEuler(func, t0, y0, t1, n);
                else if (op == "rk2")
                    nr = rk2(func, t0, y0, t1, n);
                else if (op == "rk4")
                    nr = rk4(func, t0, y0, t1, n);
                else if (op == "rk45")
                    nr = rk45(func, t0, y0, t1);
                else
                    nr = adamsBashforth(func, t0, y0, t1, n);

                if (!nr.ok)
                    return err(nr.error);

                // Return solution points as JSON array for graphing
                std::ostringstream out;
                out << "Method: " << nr.method << "\n";
                out << nr.summary << "\n";
                out << "Points: [";
                int step = std::max(1, (int)nr.t.size() / 50);
                for (size_t i = 0; i < nr.t.size(); i += step)
                {
                    if (i > 0)
                        out << ",";
                    out << "[" << fmt(nr.t[i]) << "," << fmt(nr.y[i]) << "]";
                }
                out << "]";
                return ok(out.str());
            }

            // ── LibreTexts Extensions ────────────────────────────────────────────────

            if (op == "mixing")
                return mixingProblem(getNum(json, "V", 100), getNum(json, "cin", 0.5),
                                     getNum(json, "rin", 2), getNum(json, "rout", 2), getNum(json, "c0", 0), getNum(json, "tend", 100));
            if (op == "cooling")
                return newtonCooling(getNum(json, "k", 0.1), getNum(json, "Tenv", 20),
                                     getNum(json, "T0", 100), getNum(json, "tend", 50));
            if (op == "population")
            {
                bool log = getParam(json, "logistic") == "true" || getParam(json, "logistic") == "1";
                return populationGrowth(getNum(json, "r", 0.1), getNum(json, "P0", 100),
                                        getNum(json, "tend", 50), log, getNum(json, "K", 1000));
            }
            if (op == "terminal_velocity")
                return terminalVelocity(getNum(json, "m", 70), getNum(json, "g", 9.81),
                                        getNum(json, "k", 0.2), getNum(json, "v0", 0), getNum(json, "tend", 30));
            if (op == "torricelli")
                return torricelli(getNum(json, "A", 1), getNum(json, "a", 0.01),
                                  getNum(json, "h0", 4), getNum(json, "g", 9.81));
            if (op == "orthogonal_traj")
                return orthogonalTrajectories(getParam(json, "family"),
                                              getParam(json, "x", "x"), getParam(json, "y", "y"));
            if (op == "reduction_of_order")
                return reductionOfOrder(getNum(json, "a", 1), getNum(json, "b", 0),
                                        getNum(json, "c", 0), getParam(json, "y1", "exp(x)"));
            if (op == "higher_order")
            {
                auto parse_v = [&]() -> std::vector<double>
                {
                    std::vector<double> v;
                    std::string s = getParam(json, "coeffs");
                    if (!s.empty() && s.front() == '[')
                        s = s.substr(1, s.size() - 2);
                    std::istringstream ss2(s);
                    std::string tok;
                    while (std::getline(ss2, tok, ','))
                        try
                        {
                            v.push_back(std::stod(tok));
                        }
                        catch (...)
                        {
                        }
                    return v;
                };
                auto cs = parse_v();
                if (cs.empty())
                    cs = {getNum(json, "a0", 1), getNum(json, "a1", 0), getNum(json, "a2", 1)};
                return solveHigherOrder(cs);
            }
            if (op == "annihilator")
            {
                std::vector<double> cs = {getNum(json, "a0", 1), getNum(json, "a1", 0), getNum(json, "a2", 1)};
                return annihilatorMethod(cs, getParam(json, "g"), getParam(json, "x", "x"));
            }
            if (op == "linear_system")
            {
                auto parse_v = [&]() -> std::vector<double>
                {
                    std::vector<double> v;
                    std::string s = getParam(json, "A");
                    if (!s.empty() && s.front() == '[')
                        s = s.substr(1, s.size() - 2);
                    std::istringstream ss2(s);
                    std::string tok;
                    while (std::getline(ss2, tok, ','))
                        try
                        {
                            v.push_back(std::stod(tok));
                        }
                        catch (...)
                        {
                        }
                    return v;
                };
                auto Av = parse_v();
                int n2 = (int)std::round(std::sqrt(Av.size()));
                if (n2 * n2 != (int)Av.size())
                    return err("A must be flat square matrix");
                std::vector<std::vector<double>> A(n2, std::vector<double>(n2));
                for (int i = 0; i < n2; ++i)
                    for (int j = 0; j < n2; ++j)
                        A[i][j] = Av[i * n2 + j];
                auto parse_x = [&]() -> std::vector<double>
                {
                    std::vector<double> v;
                    std::string s = getParam(json, "x0");
                    if (!s.empty() && s.front() == '[')
                        s = s.substr(1, s.size() - 2);
                    std::istringstream ss2(s);
                    std::string tok;
                    while (std::getline(ss2, tok, ','))
                        try
                        {
                            v.push_back(std::stod(tok));
                        }
                        catch (...)
                        {
                        }
                    return v;
                };
                auto x0v = parse_x();
                if ((int)x0v.size() != n2)
                    x0v.assign(n2, 0.0);
                return solveLinearSystem(A, x0v, getNum(json, "tend", 5), getInt(json, "n", 200));
            }
            if (op == "phase_portrait")
                return phasePortrait2x2(getNum(json, "a", 0), getNum(json, "b", 1),
                                        getNum(json, "c", -1), getNum(json, "d", 0));
            if (op == "nonlinear_2d")
                return nonlinearSystem2D(getParam(json, "f1"), getParam(json, "f2"),
                                         getParam(json, "x1", "x"), getParam(json, "x2", "y"),
                                         getNum(json, "xmin", -3), getNum(json, "xmax", 3),
                                         getNum(json, "ymin", -3), getNum(json, "ymax", 3));
            if (op == "fourier_series")
            {
                auto r = fourierSeries(getParam(json, "f"), getParam(json, "x", "x"),
                                       getNum(json, "L", M_PI), getInt(json, "N", 10));
                return {r.ok, r.series, r.series, r.error};
            }
            if (op == "fourier_sine")
            {
                auto r = fourierSineSeries(getParam(json, "f"), getParam(json, "x", "x"),
                                           getNum(json, "L", M_PI), getInt(json, "N", 10));
                return {r.ok, r.series, r.series, r.error};
            }
            if (op == "fourier_cos")
            {
                auto r = fourierCosSeries(getParam(json, "f"), getParam(json, "x", "x"),
                                          getNum(json, "L", M_PI), getInt(json, "N", 10));
                return {r.ok, r.series, r.series, r.error};
            }
            if (op == "parseval")
                return parsevalIdentity(getParam(json, "f"), getParam(json, "x", "x"),
                                        getNum(json, "L", M_PI), getInt(json, "N", 10));
            if (op == "hartman_grobman")
                return hartmanGrobman(getParam(json, "f1"), getParam(json, "f2"),
                                      getParam(json, "x1", "x"), getParam(json, "x2", "y"),
                                      getNum(json, "xs", 0), getNum(json, "ys", 0));
            if (op == "lyapunov_fn")
                return lyapunovFunction(getParam(json, "f1"), getParam(json, "f2"),
                                        getParam(json, "V"), getParam(json, "x1", "x"), getParam(json, "x2", "y"),
                                        getNum(json, "xs", 0), getNum(json, "ys", 0));
            if (op == "dulac")
                return dulacCriterion(getParam(json, "f1"), getParam(json, "f2"),
                                      getParam(json, "B", "1"), getParam(json, "x1", "x"), getParam(json, "x2", "y"));
            if (op == "limit_cycle")
                return limitCycleCheck(getParam(json, "f1"), getParam(json, "f2"),
                                       getParam(json, "x1", "x"), getParam(json, "x2", "y"),
                                       getNum(json, "xmin", -3), getNum(json, "xmax", 3),
                                       getNum(json, "ymin", -3), getNum(json, "ymax", 3));
            if (op == "poincare_index")
                return poincareIndex(getParam(json, "f1"), getParam(json, "f2"),
                                     getParam(json, "x1", "x"), getParam(json, "x2", "y"),
                                     getNum(json, "cx", 0), getNum(json, "cy", 0), getNum(json, "r", 1));
            if (op == "implicit_euler" || op == "crank_nicolson")
            {
                std::string fStr = getParam(json, "f");
                std::string tVar = getParam(json, "tvar");
                if (tVar.empty())
                    tVar = "t";
                std::string yVar = getParam(json, "yvar");
                if (yVar.empty())
                    yVar = "y";
                auto fExpr = Calculus::parse(fStr);
                auto dfExpr = Calculus::simplify(Calculus::diff(fExpr, yVar));
                Func f2 = [fExpr, tVar, yVar](double t, double y)
                {
                    return Calculus::evaluate(fExpr, {{tVar, t}, {yVar, y}});
                };
                Func dfdy = [dfExpr, tVar, yVar](double t, double y)
                {
                    return Calculus::evaluate(dfExpr, {{tVar, t}, {yVar, y}});
                };
                NumericalDEResult nr;
                if (op == "implicit_euler")
                    nr = implicitEuler(f2, dfdy, getNum(json, "t0"), getNum(json, "y0", 1), getNum(json, "t1", 1), getInt(json, "n", 100));
                else
                    nr = crankNicolson(f2, dfdy, getNum(json, "t0"), getNum(json, "y0", 1), getNum(json, "t1", 1), getInt(json, "n", 100));
                if (!nr.ok)
                    return err(nr.error);
                std::ostringstream out;
                out << nr.method << "\n"
                    << nr.summary << "\nPoints: [";
                int step = std::max(1, (int)nr.t.size() / 50);
                for (int i = 0; i < (int)nr.t.size(); i += step)
                {
                    if (i)
                        out << ",";
                    out << "[" << fmt(nr.t[i]) << "," << fmt(nr.y[i]) << "]";
                }
                out << "]";
                DEResult rde;
                rde.symbolic = out.str();
                rde.numerical = out.str();
                return rde;
            }
            if (op == "laplace_table")
                return laplaceTable(getParam(json, "f"), getParam(json, "t", "t"), getParam(json, "s", "s"));
            if (op == "heaviside")
                return heavisideStep(getNum(json, "c", 0), getParam(json, "f"), getParam(json, "t", "t"));
            if (op == "convolution")
                return convolutionLaplace(getParam(json, "f"), getParam(json, "g"),
                                          getParam(json, "t", "t"), getParam(json, "s", "s"));
            if (op == "dirac_response")
                return diracDeltaResponse(getNum(json, "a", 1), getNum(json, "b", 0),
                                          getNum(json, "c", 1), getNum(json, "t0", 1), getNum(json, "tend", 5));
            if (op == "partial_fractions_s")
                return partialFractionsDE(getParam(json, "num"), getParam(json, "den"),
                                          getParam(json, "s", "s"));
            if (op == "fourier_transform_pde")
                return fourierTransformPDE(getParam(json, "pde"), getParam(json, "ic"),
                                           getParam(json, "x", "x"), getParam(json, "t", "t"), getNum(json, "tend", 1));
            if (op == "characteristics_1st")
                return characteristics1stPDE(getParam(json, "a", "1"), getParam(json, "b", "1"),
                                             getParam(json, "c", "0"), getParam(json, "ic"),
                                             getParam(json, "x", "x"), getParam(json, "t", "t"));
            if (op == "duhamel")
                return duhamelPrinciple(
                    getNum(json, "alpha", 1),    // double a
                    getNum(json, "L", M_PI),     // double b
                    getNum(json, "tend", 1),     // double c
                    getParam(json, "g", "0"),    // const std::string &g (The source/forcing function)
                    getParam(json, "x", "x"),    // const std::string &x (The variable)
                    cu_getNum(json, "terms", 5), // The extra argument (int)
                    cu_getNum(json, "tEnd", 0),  // The L term
                    cu_getNum(json, "L", 0));
            if (op == "bessel")
                return besselEquation(getNum(json, "nu", 0), getNum(json, "x", 1));
            if (op == "legendre")
                return legendreEquation(getInt(json, "n", 2), getNum(json, "x", 0.5));
            if (op == "assoc_legendre")
                return associatedLegendre(getInt(json, "l", 1), getInt(json, "m", 0), getNum(json, "x", 0.5));
            if (op == "weak_solution")
                return weakSolution(getParam(json, "pde"), getParam(json, "phi"),
                                    getParam(json, "x", "x"));
            if (op == "nonhomog_pde")
                return nonhomogPDE(getNum(json, "alpha", 1), getNum(json, "L", M_PI),
                                   getParam(json, "Q"), getParam(json, "ic"), getInt(json, "terms", 5));
            if (op == "sl_eigen")
            {
                auto r = sturmLiouvilleEigen(getParam(json, "p", "1"), getParam(json, "q", "0"),
                                             getParam(json, "w", "1"), getNum(json, "a", 0), getNum(json, "b", M_PI),
                                             getInt(json, "N", 5), 200);
                return {r.ok, r.summary, r.summary, r.error};
            }
            if (op == "rayleigh_de")
                return rayleighQuotientDE(getParam(json, "p", "1"), getParam(json, "q", "0"),
                                          getParam(json, "w", "1"), getParam(json, "trial"),
                                          getParam(json, "x", "x"), getNum(json, "a", 0), getNum(json, "b", 1));
            if (op == "greens_fn_de")
                return greensFunctionDE(getNum(json, "a", 0), getNum(json, "b", 1),
                                        getParam(json, "f"), getParam(json, "bc", "dirichlet"));
            if (op == "comparison_thm")
                return comparisonTheorem(getNum(json, "q1", 1), getNum(json, "q2", 4),
                                         getNum(json, "a", 0), getNum(json, "b", M_PI));

            if (op == "power_series")
                return powerSeriesSolution(getNum(json, "p", 0), getNum(json, "q", 1), getNum(json, "r", 0), getInt(json, "terms", 8));
            if (op == "frobenius")
                return frobeniusSolution(getNum(json, "p0", 0), getNum(json, "q0", 0), getInt(json, "terms", 6));
            if (op == "richardson_ode")
            {
                std::string fStr = getParam(json, "f");
                std::string tVar = getParam(json, "tvar");
                if (tVar.empty())
                    tVar = "t";
                std::string yVar = getParam(json, "yvar");
                if (yVar.empty())
                    yVar = "y";
                auto fExpr = Calculus::parse(fStr);
                Func f2 = [fExpr, tVar, yVar](double t, double y)
                {
                    return Calculus::evaluate(fExpr, {{tVar, t}, {yVar, y}});
                };
                return richardsonExtrapolODE(f2, getNum(json, "t0"), getNum(json, "y0", 1),
                                             getNum(json, "t1", 1), getInt(json, "n", 10));
            }

            return err("Unknown DE operation: " + op);
        }
        catch (const std::exception &e)
        {
            return err(e.what());
        }
    }

} // namespace DifferentialEquations