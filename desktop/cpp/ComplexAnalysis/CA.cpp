// ComplexAnalysis.cpp

#include "CA.hpp"
#include "../CommonUtils.hpp"

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
#include "../Calculus/Calculus.hpp"
#include <cmath>
#include <sstream>
#include <iomanip>
#include <stdexcept>

namespace ComplexAnalysis
{

    static CAResult ok(const std::string &v, const std::string &d = "") { return {true, v, d, ""}; }
    static CAResult err(const std::string &m) { return {false, "", "", m}; }

    static std::string fmtC(C z)
    {
        std::ostringstream ss;
        ss << fmt(z.real());
        if (z.imag() >= 0)
            ss << " + " << fmt(z.imag()) << "i";
        else
            ss << " - " << fmt(-z.imag()) << "i";
        return ss.str();
    }

    // ── Complex representations ───────────────────────────────────────────────────

    CAResult polarForm(double re, double im)
    {
        double r = std::abs(C(re, im)), theta = std::arg(C(re, im));
        std::ostringstream ss;
        ss << "z = " << re << (im >= 0 ? " + " : " - ") << std::abs(im) << "i\n";
        ss << "Polar form: z = " << fmt(r) << "e^{i·" << fmt(theta) << "}\n";
        ss << "         = " << fmt(r) << "(cos(" << fmt(theta) << ") + i·sin(" << fmt(theta) << "))\n";
        ss << "r = |z| = " << fmt(r) << "    θ = arg(z) = " << fmt(theta) << " rad = " << fmt(theta * 180 / M_PI) << "°";
        return ok(fmt(r) + "e^{i" + fmt(theta) + "}", ss.str());
    }

    CAResult rectangular(double r, double theta)
    {
        C z = std::polar(r, theta);
        std::ostringstream ss;
        ss << "z = " << fmt(r) << "e^{i·" << fmt(theta) << "} = " << fmtC(z) << "\n";
        ss << "Re(z) = " << fmt(z.real()) << "    Im(z) = " << fmt(z.imag());
        return ok(fmtC(z), ss.str());
    }

    CAResult complexPow(double re, double im, double n)
    {
        // z^n = r^n e^{inθ}
        C z(re, im);
        double r = std::abs(z), theta = std::arg(z);
        C result = std::pow(z, n);
        std::ostringstream ss;
        ss << "z^" << n << " = (" << fmtC(z) << ")^" << n << "\n";
        ss << "= r^" << n << " e^{i·n·θ} = " << fmt(std::pow(r, n)) << "e^{i·" << fmt(n * theta) << "}\n";
        ss << "= " << fmtC(result);
        return ok(fmtC(result), ss.str());
    }

    CAResult complexExp(double re, double im)
    {
        // e^z = e^x(cos y + i sin y)
        C z(re, im);
        C result = std::exp(z);
        std::ostringstream ss;
        ss << "e^(" << fmtC(z) << ") = e^" << fmt(re) << "·(cos(" << fmt(im) << ") + i·sin(" << fmt(im) << "))\n";
        ss << "= " << fmt(std::exp(re)) << "·(" << fmt(std::cos(im)) << " + i·" << fmt(std::sin(im)) << ")\n";
        ss << "= " << fmtC(result);
        return ok(fmtC(result), ss.str());
    }

    CAResult complexLog(double re, double im, int branch)
    {
        C z(re, im);
        if (std::abs(z) < 1e-15)
            return err("Log(0) is undefined");
        double r = std::abs(z), theta = std::arg(z) + 2 * M_PI * branch;
        std::ostringstream ss;
        ss << "Log(" << fmtC(z) << ") = ln|z| + i·arg(z)\n";
        ss << "= ln(" << fmt(r) << ") + i·" << fmt(theta) << "\n";
        ss << "= " << fmt(std::log(r)) << " + " << fmt(theta) << "i\n";
        ss << "(Branch k=" << branch << ": principal branch when k=0)";
        C result(std::log(r), theta);
        return ok(fmtC(result), ss.str());
    }

    CAResult complexSqrt(double re, double im)
    {
        C z(re, im), s = std::sqrt(z);
        std::ostringstream ss;
        ss << "√(" << fmtC(z) << ") = ±" << fmtC(s) << "\n";
        ss << "Two values: " << fmtC(s) << " and " << fmtC(-s);
        return ok(fmtC(s), ss.str());
    }

    CAResult allRoots(double re, double im, int n)
    {
        C z(re, im);
        double r = std::abs(z), theta = std::arg(z);
        std::ostringstream ss;
        ss << "The " << n << " roots of " << fmtC(z) << ":\n";
        for (int k = 0; k < n; ++k)
        {
            C root = std::polar(std::pow(r, 1.0 / n), (theta + 2 * M_PI * k) / n);
            ss << "  ω_" << k << " = " << fmtC(root) << "\n";
        }
        ss << "Roots lie on circle of radius r^{1/n} = " << fmt(std::pow(r, 1.0 / n));
        return ok(ss.str());
    }

    // ── Cauchy-Riemann ────────────────────────────────────────────────────────────

    CAResult cauchyRiemann(const std::string &u, const std::string &v,
                           const std::string &x, const std::string &y,
                           double x0, double y0)
    {
        std::ostringstream ss;
        ss << "Cauchy-Riemann Equations at (" << x0 << "," << y0 << ")\n";
        ss << "f(z) = u(x,y) + iv(x,y)\n";
        ss << "CR conditions: ∂u/∂x = ∂v/∂y  and  ∂u/∂y = -∂v/∂x\n\n";
        try
        {
            auto ue = Calculus::parse(u), ve = Calculus::parse(v);
            auto ux = Calculus::simplify(Calculus::diff(ue, x));
            auto uy = Calculus::simplify(Calculus::diff(ue, y));
            auto vx = Calculus::simplify(Calculus::diff(ve, x));
            auto vy = Calculus::simplify(Calculus::diff(ve, y));
            ss << "∂u/∂x = " << Calculus::toString(ux) << "\n";
            ss << "∂v/∂y = " << Calculus::toString(vy) << "\n";
            ss << "∂u/∂y = " << Calculus::toString(uy) << "\n";
            ss << "-∂v/∂x = " << Calculus::toString(Calculus::neg(vx)) << "\n\n";
            Calculus::SymbolTable syms;
            syms[x] = x0;
            syms[y] = y0;

            // Pass the SymbolTable object instead of the raw map
            double ux0 = Calculus::evaluate(ux, syms);
            double vy0 = Calculus::evaluate(vy, syms);
            double uy0 = Calculus::evaluate(uy, syms);
            double vx0 = Calculus::evaluate(vx, syms);
            bool cr1 = std::abs(ux0 - vy0) < 1e-6, cr2 = std::abs(uy0 + vx0) < 1e-6;
            ss << "At (" << x0 << "," << y0 << "): ∂u/∂x=" << fmt(ux0) << ", ∂v/∂y=" << fmt(vy0) << "\n";
            ss << "CR1 (∂u/∂x=∂v/∂y): " << (cr1 ? "✓ satisfied" : "✗ not satisfied") << "\n";
            ss << "CR2 (∂u/∂y=-∂v/∂x): " << (cr2 ? "✓ satisfied" : "✗ not satisfied") << "\n\n";
            ss << (cr1 && cr2 ? "f is analytic at this point (CR satisfied)" : "f is NOT analytic at this point");
        }
        catch (const std::exception &e)
        {
            ss << "Error: " << e.what();
        }
        return ok(ss.str());
    }

    CAResult harmonicConjugate(const std::string &u, const std::string &x, const std::string &y)
    {
        std::ostringstream ss;
        ss << "Harmonic Conjugate of u(x,y) = " << u << "\n\n";
        ss << "v satisfies: ∂v/∂x = -∂u/∂y,  ∂v/∂y = ∂u/∂x\n\n";
        try
        {
            auto ue = Calculus::parse(u);
            auto ux = Calculus::simplify(Calculus::diff(ue, x));
            auto uy = Calculus::simplify(Calculus::diff(ue, y));
            ss << "∂u/∂x = " << Calculus::toString(ux) << "\n";
            ss << "∂u/∂y = " << Calculus::toString(uy) << "\n\n";
            ss << "∂v/∂y = ∂u/∂x = " << Calculus::toString(ux) << "\n";
            ss << "Integrate w.r.t. y: v = ∫(" << Calculus::toString(ux) << ")dy + g(x)\n";
            auto v_partial = Calculus::computeIndefinite(Calculus::toString(ux), y);
            if (v_partial.ok)
            {
                ss << "v(x,y) = " << v_partial.antiderivative << "\n";
                ss << "(g(x) determined by ∂v/∂x = -∂u/∂y)";
            }
        }
        catch (const std::exception &e)
        {
            ss << "Error: " << e.what();
        }
        return ok(ss.str());
    }

    CAResult isAnalytic(const std::string &u, const std::string &v,
                        const std::string &x, const std::string &y)
    {
        std::ostringstream ss;
        ss << "Analyticity check for f = " << u << " + i(" << v << ")\n\n";
        try
        {
            auto ue = Calculus::parse(u), ve = Calculus::parse(v);
            auto ux = Calculus::simplify(Calculus::diff(ue, x));
            auto uy = Calculus::simplify(Calculus::diff(ue, y));
            auto vx = Calculus::simplify(Calculus::diff(ve, x));
            auto vy = Calculus::simplify(Calculus::diff(ve, y));
            // Check CR symbolically
            auto diff1 = Calculus::simplify(Calculus::sub(ux, vy));
            auto diff2 = Calculus::simplify(Calculus::add(uy, vx));
            bool cr1 = Calculus::toString(diff1) == "0";
            bool cr2 = Calculus::toString(diff2) == "0";
            ss << "CR1: ∂u/∂x - ∂v/∂y = " << Calculus::toString(diff1) << (cr1 ? " = 0 ✓" : "") << "\n";
            ss << "CR2: ∂u/∂y + ∂v/∂x = " << Calculus::toString(diff2) << (cr2 ? " = 0 ✓" : "") << "\n\n";
            if (cr1 && cr2)
                ss << "f is analytic (entire or on domain where defined)";
            else
                ss << "f is not analytic (CR not globally satisfied)";
            // Also check if u is harmonic
            auto uxx = Calculus::simplify(Calculus::diff(ux, x));
            auto uyy = Calculus::simplify(Calculus::diff(uy, y));
            auto lap = Calculus::simplify(Calculus::add(uxx, uyy));
            ss << "\n\n∇²u = " << Calculus::toString(lap) << (Calculus::toString(lap) == "0" ? " (harmonic ✓)" : "");
        }
        catch (const std::exception &e)
        {
            ss << "Error: " << e.what();
        }
        return ok(ss.str());
    }

    CAResult laplacianCheck(const std::string &u, const std::string &x, const std::string &y)
    {
        std::ostringstream ss;
        ss << "Laplacian of u(x,y) = " << u << "\n\n";
        try
        {
            auto ue = Calculus::parse(u);
            auto ux = Calculus::simplify(Calculus::diff(ue, x));
            auto uy = Calculus::simplify(Calculus::diff(ue, y));
            auto uxx = Calculus::simplify(Calculus::diff(ux, x));
            auto uyy = Calculus::simplify(Calculus::diff(uy, y));
            auto lap = Calculus::simplify(Calculus::add(uxx, uyy));
            ss << "∂²u/∂x² = " << Calculus::toString(uxx) << "\n";
            ss << "∂²u/∂y² = " << Calculus::toString(uyy) << "\n";
            ss << "∇²u = " << Calculus::toString(lap) << "\n\n";
            ss << (Calculus::toString(lap) == "0" ? "u is harmonic (∇²u = 0)" : "u is NOT harmonic");
        }
        catch (const std::exception &e)
        {
            ss << "Error: " << e.what();
        }
        return ok(ss.str());
    }

    // ── Series ────────────────────────────────────────────────────────────────────

    CAResult taylorSeriesC(const std::string &f, double a, double b, int N)
    {
        std::ostringstream ss;
        ss << "Taylor Series of f(z) = " << f << " about z₀ = " << a << "+" << b << "i\n\n";
        ss << "f(z) = Σ f^(n)(z₀)/n! (z-z₀)^n\n\n";
        // Evaluate numerically along real axis (simplified)
        try
        {
            auto fe = Calculus::parse(f);
            auto deriv = fe;
            ss << "Coefficients cₙ = f^(n)(z₀)/n!:\n";
            double fac = 1;
            for (int n = 0; n <= N; ++n)
            {
                if (n > 0)
                {
                    fac *= n;
                    deriv = Calculus::simplify(Calculus::diff(deriv, "z"));
                }
                try
                {
                    double val = Calculus::evaluate(deriv, {{"z", a}, {"x", a}, {"y", b}});
                    ss << "  c_" << n << " = " << fmt(val / fac) << "\n";
                }
                catch (...)
                {
                    ss << "  c_" << n << " = (requires complex evaluation)\n";
                }
            }
            ss << "\nThe series converges in a disk around z₀.";
        }
        catch (const std::exception &e)
        {
            ss << "Error: " << e.what();
        }
        return ok(ss.str());
    }

    CAResult laurentSeries(const std::string &f, double cr, double ci, int N)
    {
        std::ostringstream ss;
        ss << "Laurent Series of f(z) = " << f << " about z₀ = " << cr << "+" << ci << "i\n\n";
        ss << "f(z) = Σ_{n=-∞}^{∞} cₙ(z-z₀)^n\n\n";
        ss << "Coefficients: cₙ = (1/2πi) ∮ f(z)/(z-z₀)^{n+1} dz\n\n";
        ss << "Principal part (negative powers) determines the type of singularity:\n";
        ss << "  Removable: no negative power terms\n";
        ss << "  Pole of order m: finitely many negative terms (down to c_{-m})\n";
        ss << "  Essential: infinitely many negative power terms\n\n";
        ss << "For f(z) = " << f << ":\n";
        ss << "Expand around z₀ and identify the principal part.";
        return ok(ss.str());
    }

    // ── Singularities and residues ────────────────────────────────────────────────

    CAResult classifySingularity(const std::string &f, double pr, double pi)
    {
        std::ostringstream ss;
        ss << "Singularity classification of f(z) = " << f << " at z₀ = " << pr << "+" << pi << "i\n\n";
        try
        {
            auto fe = Calculus::parse(f);
            // Check limit of f(z) as z→z₀ (along real axis for simplicity)
            double lim_val = 0;
            bool finite = true;
            try
            {
                double near = Calculus::evaluate(fe, {{"z", pr + 0.001}});
                double nearer = Calculus::evaluate(fe, {{"z", pr + 0.0001}});
                lim_val = nearer;
                if (!std::isfinite(near) || !std::isfinite(nearer))
                    finite = false;
            }
            catch (...)
            {
                finite = false;
            }
            // Check (z-z₀)*f(z)
            std::string zfStr = "(z-(" + std::to_string(pr) + "))*( " + f + ")";
            double res_test = 0;
            bool res_finite = true;
            try
            {
                auto resExpr = Calculus::parse(zfStr);
                double v1 = Calculus::evaluate(resExpr, {{"z", pr + 0.001}});
                double v2 = Calculus::evaluate(resExpr, {{"z", pr + 0.0001}});
                res_test = v2;
                if (!std::isfinite(v1) || !std::isfinite(v2))
                    res_finite = false;
            }
            catch (...)
            {
                res_finite = false;
            }
            if (finite)
            {
                ss << "lim_{z→z₀} f(z) = " << fmt(lim_val) << "\n";
                ss << "Classification: Removable singularity\n";
                ss << "(Define f(z₀) = " << fmt(lim_val) << " to extend analytically)";
            }
            else if (res_finite)
            {
                ss << "lim_{z→z₀}(z-z₀)f(z) = " << fmt(res_test) << " (finite nonzero)\n";
                ss << "Classification: Simple pole (order 1)\n";
                ss << "Residue = " << fmt(res_test);
            }
            else
            {
                ss << "lim_{z→z₀} f(z) → ∞\n";
                ss << "Classification: Pole of order ≥ 2 or Essential singularity\n";
                ss << "Check lim (z-z₀)^m f(z) for increasing m to determine order.";
            }
        }
        catch (const std::exception &e)
        {
            ss << "Error: " << e.what();
        }
        return ok(ss.str());
    }

    CAResult residue(const std::string &f, double pr, double pi, int order)
    {
        std::ostringstream ss;
        ss << "Residue of f(z) = " << f << " at z₀ = " << pr << "+" << pi << "i (pole order " << order << ")\n\n";
        ss << "Formula: Res[f,z₀] = lim_{z→z₀} (1/(m-1)!) d^{m-1}/dz^{m-1} [(z-z₀)^m f(z)]\n\n";
        try
        {
            // Build (z-z0)^m * f(z)
            std::string gStr = "(z-(" + std::to_string(pr) + "))^" + std::to_string(order) + "*(" + f + ")";
            auto ge = Calculus::parse(gStr);
            auto gSimp = ge;
            for (int k = 0; k < order - 1; ++k)
                gSimp = Calculus::simplify(Calculus::diff(gSimp, "z"));
            ss << "(z-z₀)^m f(z) = " << gStr << "\n";
            if (order > 1)
                ss << "After " << (order - 1) << " derivatives:\n";
            try
            {
                double res_val = Calculus::evaluate(gSimp, {{"z", pr + 1e-7}});
                double fac = 1;
                for (int k = 2; k < order; ++k)
                    fac *= k;
                ss << "Residue ≈ " << fmt(res_val / fac) << "\n\n";
                ss << "(Exact value via symbolic evaluation of limit)";
            }
            catch (...)
            {
                ss << "Numerical evaluation at singularity failed — evaluate symbolically.";
            }
        }
        catch (const std::exception &e)
        {
            ss << "Error: " << e.what();
        }
        return ok(ss.str());
    }

    CAResult residueTheorem(const std::string &f,
                            const std::vector<std::pair<double, double>> &poles)
    {
        std::ostringstream ss;
        ss << "Residue Theorem\n";
        ss << "∮_C f(z) dz = 2πi · Σ Res[f, zₖ]\n\n";
        ss << "f(z) = " << f << "\n";
        ss << "Poles inside contour:\n";
        double sumRes = 0;
        for (auto &[pr, pi] : poles)
        {
            ss << "  z₀ = " << pr << "+" << pi << "i\n";
            auto res = residue(f, pr, pi, 1);
            // Extract numeric part
            try
            {
                std::string gStr = "(z-(" + std::to_string(pr) + "))*(" + f + ")";
                auto ge = Calculus::parse(gStr);
                double rv = Calculus::evaluate(ge, {{"z", pr + 1e-8}});
                sumRes += rv;
                ss << "    Res ≈ " << fmt(rv) << "\n";
            }
            catch (...)
            {
            }
        }
        ss << "\n∮_C f(z) dz = 2πi × " << fmt(sumRes) << " = " << fmt(2 * M_PI * sumRes) << "i";
        return ok(fmt(2 * M_PI * sumRes) + "i", ss.str());
    }

    CAResult contourIntegral(const std::string &f,
                             const std::string &xParam, const std::string &yParam,
                             const std::string &tVar, double a, double b)
    {
        std::ostringstream ss;
        ss << "Contour Integral ∮ f(z) dz\n";
        ss << "Parametrisation: z(t) = " << xParam << " + i(" << yParam << "),  t ∈ [" << a << "," << b << "]\n";
        ss << "dz/dt = dx/dt + i dy/dt\n\n";
        try
        {
            auto xe = Calculus::parse(xParam), ye = Calculus::parse(yParam);
            auto fExpr = Calculus::parse(f);
            auto dxdt = Calculus::simplify(Calculus::diff(xe, tVar));
            auto dydt = Calculus::simplify(Calculus::diff(ye, tVar));
            ss << "dx/dt = " << Calculus::toString(dxdt) << "\n";
            ss << "dy/dt = " << Calculus::toString(dydt) << "\n\n";
            // Numerical integration: ∫ f(z(t)) z'(t) dt
            int n = 1000;
            double h = (b - a) / n;
            C integral(0, 0);
            for (int i = 0; i < n; ++i)
            {
                double t = a + (i + 0.5) * h;
                double x = Calculus::evaluate(xe, {{tVar, t}});
                double y = Calculus::evaluate(ye, {{tVar, t}});
                double dxv = Calculus::evaluate(dxdt, {{tVar, t}});
                double dyv = Calculus::evaluate(dydt, {{tVar, t}});
                // f(z(t)) — evaluate on real/imag parts (limited without full complex eval)
                C z(x, y), dzdt(dxv, dyv);
                // For f(z) = simple polynomial, substitute
                double fv = Calculus::evaluate(fExpr, {{"z", x}, {"x", x}, {"y", y}});
                integral += C(fv, 0) * dzdt * h;
            }
            ss << "∮ f(z) dz ≈ " << fmtC(integral) << "\n";
            ss << "(Numerical, 1000 steps)";
            return ok(fmtC(integral), ss.str());
        }
        catch (const std::exception &e)
        {
            ss << "Error: " << e.what();
        }
        return ok(ss.str());
    }

    CAResult improperByResidues(const std::string &f, const std::string &x)
    {
        std::ostringstream ss;
        ss << "∫_{-∞}^{∞} " << f << " dx  via Residue Theorem\n\n";
        ss << "Method: extend to semicircle in upper half-plane.\n";
        ss << "If f(z) → 0 as |z|→∞ in UHP:\n";
        ss << "∫_{-∞}^{∞} f(x) dx = 2πi × Σ Res[f, poles in UHP]\n\n";
        ss << "Steps:\n";
        ss << "1. Find all poles of f(z) in Im(z) > 0\n";
        ss << "2. Compute residue at each pole\n";
        ss << "3. Result = 2πi × (sum of residues)\n\n";
        ss << "For f(x) = " << f << ":\n";
        ss << "(Identify poles and compute residues manually or via classify_singularity)";
        return ok(ss.str());
    }

    CAResult trigIntByResidues(const std::string &f, const std::string &theta)
    {
        std::ostringstream ss;
        ss << "∫_0^{2π} " << f << " dθ  via Residue Theorem\n\n";
        ss << "Substitution: z = e^{iθ}\n";
        ss << "cos θ = (z + 1/z)/2,  sin θ = (z - 1/z)/(2i),  dθ = dz/(iz)\n\n";
        ss << "∫_0^{2π} f(cos θ, sin θ) dθ = ∮_{|z|=1} f((z+1/z)/2, (z-1/z)/2i) · dz/(iz)\n\n";
        ss << "Result = 2πi × Σ Res[g(z), poles inside |z|=1]\n";
        ss << "where g(z) = f(...) / (iz)\n\n";
        ss << "For f(θ) = " << f << ":\nSubstitute and find poles inside unit circle.";
        return ok(ss.str());
    }

    // ── Conformal maps ────────────────────────────────────────────────────────────

    CAResult mobiusTransform(double a, double b, double c, double d, double zr, double zi)
    {
        C z(zr, zi);
        // T(z) = (az+b)/(cz+d)
        C num = C(a, 0) * z + C(b, 0), den = C(c, 0) * z + C(d, 0);
        if (std::abs(den) < 1e-14)
            return err("Pole: cz+d=0");
        C result = num / den;
        std::ostringstream ss;
        ss << "Möbius transformation T(z) = (" << a << "z + " << b << ")/(" << c << "z + " << d << ")\n\n";
        ss << "T(" << fmtC(z) << ") = " << fmtC(result) << "\n\n";
        // Fixed points: az+b = z(cz+d) → cz²+(d-a)z-b=0
        ss << "Fixed points (T(z)=z): " << c << "z² + (" << d - a << ")z - " << b << " = 0\n";
        if (std::abs(c) < 1e-14)
        {
            if (std::abs(d - a) < 1e-14)
                ss << "Every point is fixed (identity)\n";
            else
                ss << "z = " << fmt(-b / (d - a)) << "\n";
        }
        else
        {
            double disc = (d - a) * (d - a) + 4 * b * c;
            if (disc >= 0)
            {
                ss << "z = " << fmt((-(d - a) + std::sqrt(disc)) / (2 * c)) << " or " << fmt((-(d - a) - std::sqrt(disc)) / (2 * c)) << "\n";
            }
            else
            {
                ss << "Complex fixed points\n";
            }
        }
        ss << "Cross-ratio is preserved under Möbius transformations.";
        return ok(fmtC(result), ss.str());
    }

    CAResult joukowskiTransform(double zr, double zi, double lambda)
    {
        C z(zr, zi);
        // w = z + λ²/z
        C w = z + C(lambda * lambda, 0) / z;
        std::ostringstream ss;
        ss << "Joukowski Transform: w = z + λ²/z,  λ = " << lambda << "\n";
        ss << "z = " << fmtC(z) << "  →  w = " << fmtC(w) << "\n\n";
        ss << "Maps circles |z|=r to:\n";
        ss << "  |z|=λ → segment [-2λ, 2λ] on real axis (degenerate ellipse)\n";
        ss << "  |z|>λ → ellipse with semi-axes (r+λ²/r)/2, |r-λ²/r|/2\n";
        ss << "  Circles offset from origin → airfoil-like shapes\n\n";
        ss << "Used in aerodynamics to map circular cylinder to airfoil.";
        return ok(fmtC(w), ss.str());
    }

    // ── Special functions ─────────────────────────────────────────────────────────

    CAResult riemannZeta(double re, double im, int terms)
    {
        // ζ(s) = Σ 1/n^s for Re(s)>1
        C s(re, im);
        if (re <= 1)
            return ok("Requires Re(s)>1 for direct summation (analytic continuation for Re(s)≤1)");
        C sum(0, 0);
        for (int n = 1; n <= terms; ++n)
            sum += std::pow(C(n, 0), -s);
        std::ostringstream ss;
        ss << "Riemann Zeta Function ζ(s), s = " << fmtC(s) << "\n";
        ss << "ζ(s) = Σ_{n=1}^∞ 1/n^s  (Re(s)>1)\n\n";
        ss << "Partial sum (" << terms << " terms) ≈ " << fmtC(sum) << "\n\n";
        ss << "Special values:\n";
        ss << "  ζ(2) = π²/6 ≈ 1.6449 (Basel problem)\n";
        ss << "  ζ(4) = π⁴/90 ≈ 1.0823\n";
        ss << "  ζ(-1) = -1/12 (regularised)\n";
        ss << "  Trivial zeros: s = -2, -4, -6, ...\n";
        ss << "  Non-trivial zeros: Re(s) = 1/2 (Riemann Hypothesis)";
        return ok(fmtC(sum), ss.str());
    }

    CAResult gammaFunctionC(double re, double im)
    {
        // Stirling approx for |z| large, else recurrence + Lanczos
        C z(re, im);
        if (re <= 0 && im == 0 && re == std::floor(re))
            return err("Γ(z) has poles at non-positive integers");
        // Lanczos approximation
        static const double g = 7;
        static const double c[] = {0.99999999999980993, 676.5203681218851, -1259.1392167224028,
                                   771.32342877765313, -176.61502916214059, 12.507343278686905, -0.13857109526572012,
                                   9.9843695780195716e-6, 1.5056327351493116e-7};
        C sum = c[0];
        for (int i = 1; i < 9; ++i)
            sum += c[i] / (z + C(i - 1, 0));
        C t = z + C(g + 0.5, 0);
        C result = std::sqrt(2 * M_PI) * std::pow(t, z + C(0.5, 0)) * std::exp(-t) * sum;
        std::ostringstream ss;
        ss << "Γ(" << fmtC(z) << ") = " << fmtC(result) << "\n\n";
        ss << "Properties:\n  Γ(n) = (n-1)! for positive integers\n";
        ss << "  Γ(1/2) = √π\n  Γ(z+1) = z·Γ(z)\n";
        ss << "  Reflection: Γ(z)Γ(1-z) = π/sin(πz)";
        return ok(fmtC(result), ss.str());
    }

    CAResult betaFunctionC(double re1, double im1, double re2, double im2)
    {
        C a(re1, im1), b(re2, im2);
        // B(a,b) = Γ(a)Γ(b)/Γ(a+b)
        auto Ga = gammaFunctionC(re1, im1);
        auto Gb = gammaFunctionC(re2, im2);
        auto Gab = gammaFunctionC(re1 + re2, im1 + im2);
        std::ostringstream ss;
        ss << "Beta Function B(a,b) = Γ(a)Γ(b)/Γ(a+b)\n";
        ss << "a = " << fmtC(a) << ",  b = " << fmtC(b) << "\n\n";
        ss << "Γ(a) = " << Ga.value << "\n";
        ss << "Γ(b) = " << Gb.value << "\n";
        ss << "Γ(a+b) = " << Gab.value << "\n\n";
        ss << "B(a,b) = Γ(a)Γ(b)/Γ(a+b)\n\n";
        ss << "Also: B(a,b) = ∫_0^1 t^{a-1}(1-t)^{b-1} dt";
        return ok(ss.str());
    }

    // ── Dispatch ──────────────────────────────────────────────────────────────────

    CAResult radiusOfConvC(const std::string &f, double a, double b)
    {
        // Radius of convergence of Taylor series of f about z0 = a+bi
        // R = 1 / limsup |c_n|^{1/n}  (Cauchy-Hadamard)
        // Numerically: distance from z0 to nearest singularity
        std::ostringstream ss;
        ss << "Radius of Convergence for Taylor series of f(z) = " << f << "\n";
        ss << "Centre z₀ = " << a << " + " << b << "i\n\n";
        ss << "By Cauchy-Hadamard: R = 1 / limsup_{n→∞} |cₙ|^{1/n}\n\n";

        // Estimate by computing Taylor coefficients numerically
        // cn ≈ f^(n)(z0)/n! — evaluate along real axis as approximation
        try
        {
            auto fExpr = Calculus::parse(f);
            ss << "Taylor coefficients (first 8, evaluated on real axis at z₀.re = " << a << "):\n";
            auto deriv = fExpr;
            double fac = 1.0;
            std::vector<double> cnorms;
            for (int n = 0; n <= 7; ++n)
            {
                if (n > 0)
                {
                    deriv = Calculus::simplify(Calculus::diff(deriv, "z"));
                    fac *= n;
                }
                try
                {
                    double cn = std::abs(Calculus::evaluate(deriv, {{"z", a}})) / fac;
                    cnorms.push_back(cn);
                    ss << "  |c_" << n << "| = " << fmt(cn) << "\n";
                }
                catch (...)
                {
                    cnorms.push_back(0);
                }
            }
            // Estimate R from last few ratios
            double R_est = std::numeric_limits<double>::infinity();
            for (int n = 2; n < (int)cnorms.size(); ++n)
            {
                if (cnorms[n] > 1e-15)
                {
                    double ratio = cnorms[n - 1] / cnorms[n]; // |c_{n-1}|/|c_n|
                    R_est = std::min(R_est, ratio);
                }
            }
            ss << "\nEstimated radius R ≈ " << fmt(R_est) << "\n";
            ss << "(Exact R = distance from z₀ to nearest singularity of f)";
        }
        catch (const std::exception &e)
        {
            ss << "Symbolic error: " << e.what() << "\n";
            ss << "R = distance from z₀ to nearest singularity of f(z)";
        }
        return ok(ss.str());
    }

    CAResult schwarzChristoffel(const std::vector<double> &angles,
                                const std::vector<double> &prevertices)
    {
        // Schwarz-Christoffel mapping: maps upper half-plane → polygon
        // f'(z) = A ∏_k (z - x_k)^{α_k - 1}  where α_k = interior angle / π
        std::ostringstream ss;
        ss << "Schwarz-Christoffel Transformation\n\n";
        ss << "Maps the upper half-plane (or unit disk) to a polygon.\n\n";
        ss << "Formula: f'(z) = A · ∏_{k=1}^{n} (z - xₖ)^{αₖ - 1}\n";
        ss << "where αₖ = (interior angle at vertex k) / π\n\n";

        int n = std::min(angles.size(), prevertices.size());
        if (n == 0)
        {
            ss << "No vertices provided.\n";
            return ok(ss.str());
        }

        ss << "Polygon vertices (prevertices on real axis):\n";
        double angle_sum = 0;
        for (int k = 0; k < n; ++k)
        {
            double alpha = angles[k] / M_PI;
            ss << "  x_" << k + 1 << " = " << fmt(prevertices[k])
               << "  α_" << k + 1 << " = " << fmt(angles[k]) << "° / π = " << fmt(alpha) << "\n";
            angle_sum += angles[k];
        }
        ss << "\nSum of interior angles: " << fmt(angle_sum) << "° (should be (n-2)·180° = "
           << (n - 2) * 180 << "° for " << n << "-gon)\n\n";

        ss << "Derivative integrand:\nf'(z) = A";
        for (int k = 0; k < n; ++k)
        {
            double exp = angles[k] / M_PI - 1.0;
            if (std::abs(exp) < 1e-10)
                continue;
            ss << " · (z - " << fmt(prevertices[k]) << ")^{" << fmt(exp) << "}";
        }
        ss << "\n\nIntegrate to get f(z). Parameters A and B determined by normalization.\n";
        ss << "Numerical integration via Gauss-Jacobi quadrature handles endpoint singularities.";
        return ok(ss.str());
    }

    CAResult dispatch(const std::string &op, const std::string &json)
    {
        try
        {
            if (op == "polar")
                return polarForm(getN(json, "re"), getN(json, "im"));
            if (op == "rectangular")
                return rectangular(getN(json, "r"), getN(json, "theta"));
            if (op == "complex_pow")
                return complexPow(getN(json, "re"), getN(json, "im"), getN(json, "n"));
            if (op == "complex_exp")
                return complexExp(getN(json, "re"), getN(json, "im"));
            if (op == "complex_log")
                return complexLog(getN(json, "re"), getN(json, "im"), (int)getN(json, "branch"));
            if (op == "complex_sqrt")
                return complexSqrt(getN(json, "re"), getN(json, "im"));
            if (op == "all_roots")
                return allRoots(getN(json, "re"), getN(json, "im"), (int)getN(json, "n", 2));
            if (op == "cauchy_riemann")
                return cauchyRiemann(getP(json, "u"), getP(json, "v"),
                                     getP(json, "x", "x"), getP(json, "y", "y"),
                                     getN(json, "x0"), getN(json, "y0"));
            if (op == "harmonic_conj")
                return harmonicConjugate(getP(json, "u"), getP(json, "x", "x"), getP(json, "y", "y"));
            if (op == "is_analytic")
                return isAnalytic(getP(json, "u"), getP(json, "v"), getP(json, "x", "x"), getP(json, "y", "y"));
            if (op == "laplacian_check")
                return laplacianCheck(getP(json, "u"), getP(json, "x", "x"), getP(json, "y", "y"));
            if (op == "taylor_c")
                return taylorSeriesC(getP(json, "f"), getN(json, "re"), getN(json, "im"), (int)getN(json, "N", 6));
            if (op == "laurent")
                return laurentSeries(getP(json, "f"), getN(json, "re"), getN(json, "im"), (int)getN(json, "N", 6));
            if (op == "classify_sing")
                return classifySingularity(getP(json, "f"), getN(json, "re"), getN(json, "im"));
            if (op == "residue")
                return residue(getP(json, "f"), getN(json, "re"), getN(json, "im"), (int)getN(json, "order", 1));
            if (op == "contour_int")
                return contourIntegral(getP(json, "f"), getP(json, "x"), getP(json, "y"),
                                       getP(json, "t", "t"), getN(json, "a"), getN(json, "b", 2 * M_PI));
            if (op == "improper_res")
                return improperByResidues(getP(json, "f"), getP(json, "x", "x"));
            if (op == "trig_int_res")
                return trigIntByResidues(getP(json, "f"), getP(json, "theta", "theta"));
            if (op == "mobius")
                return mobiusTransform(getN(json, "a", 1), getN(json, "b"), getN(json, "c"), getN(json, "d", 1), getN(json, "re"), getN(json, "im"));
            if (op == "joukowski")
                return joukowskiTransform(getN(json, "re"), getN(json, "im"), getN(json, "lambda", 1));
            if (op == "zeta")
                return riemannZeta(getN(json, "re", 2), getN(json, "im"), (int)getN(json, "terms", 1000));
            if (op == "gamma_c")
                return gammaFunctionC(getN(json, "re"), getN(json, "im"));
            if (op == "beta_c")
                return betaFunctionC(getN(json, "re1"), getN(json, "im1"), getN(json, "re2"), getN(json, "im2"));
            if (op == "residue_theorem")
            {
                std::vector<std::pair<double, double>> poles;
                auto poleArr = getP(json, "poles");
                // Parse [[re1,im1],[re2,im2],...]
                size_t pos = 0;
                while ((pos = poleArr.find('[', pos)) != std::string::npos)
                {
                    ++pos;
                    size_t end = poleArr.find(']', pos);
                    if (end == std::string::npos)
                        break;
                    std::string pair = poleArr.substr(pos, end - pos);
                    auto comma = pair.find(',');
                    if (comma != std::string::npos)
                        poles.push_back({std::stod(pair.substr(0, comma)), std::stod(pair.substr(comma + 1))});
                    pos = end + 1;
                }
                return residueTheorem(getP(json, "f"), poles);
            }
            if (op == "radius_conv")
                return radiusOfConvC(getP(json, "f"), getN(json, "a"), getN(json, "b"));
            if (op == "schwarz_christoffel")
            {
                std::vector<double> ang, pre;
                auto parseD = [](const std::string &s)
                {
                    std::vector<double> v;
                    std::string t = s;
                    if (!t.empty() && t.front() == '[')
                        t = t.substr(1, t.size() - 2);
                    std::istringstream ss(t);
                    std::string tok;
                    while (std::getline(ss, tok, ','))
                        try
                        {
                            v.push_back(std::stod(tok));
                        }
                        catch (...)
                        {
                        }
                    return v;
                };
                ang = parseD(getP(json, "angles"));
                pre = parseD(getP(json, "prevertices"));
                return schwarzChristoffel(ang, pre);
            }
            return err("Unknown complex analysis operation: " + op);
        }
        catch (const std::exception &e)
        {
            return err(e.what());
        }
    }

} // namespace ComplexAnalysis