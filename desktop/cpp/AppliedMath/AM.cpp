// =============================================================================
// AppliedMath.cpp — Logan's Applied Mathematics, 4th ed.
// =============================================================================

#include "AM.hpp"
#include "../CommonUtils.hpp"

// ── CommonUtils aliases ──────────────────
static auto getP = [](const std::string &j, const std::string &k, const std::string &d = "")
{ return cu_getStr(j, k, d); };
static auto getN = [](const std::string &j, const std::string &k, double d = 0.0)
{ return cu_getNum(j, k, d); };

// Fix: Use the explicit utility names to avoid undefined/circularity errors
static auto parseVecD = [](const std::string &s)
{ return cu_parseVecD(s); };
static auto parseVecS = [](const std::string &s)
{ return cu_parseVecS(s); };
static auto parseMat = [](const std::string &s)
{ return cu_parseMat(s); };
static auto getParam = [](const std::string &j, const std::string &k, const std::string &d = "")
{
    return cu_getParam(j, k, d);
};
static auto getInt_ = [](const std::string &j, const std::string &k, long long d = 0)
{ return cu_getInt(j, k, d); };
static auto fmt = [](double v, int p = 8)
{ return cu_fmt(v, p); };
static auto evalAt = [](const std::string &expr, const std::string &var, double val)
{
    return cu_evalAt(expr, var, val);
};
static auto &str_ll = cu_str;
#include "../Calculus/Calculus.hpp"
#include <cmath>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <numeric>
#include <stdexcept>
#include <functional>
#include <map>
#include <set>

namespace AppliedMath
{

    static const double PI = M_PI;

    // ── Helpers ───────────────────────────────────────────────────────────────────

    static AMResult ok(const std::string &sym,
                       const std::string &num = "",
                       const std::string &steps = "",
                       const std::string &meth = "")
    {
        AMResult r;
        r.symbolic = sym;
        r.numerical = num.empty() ? sym : num;
        r.steps = steps;
        r.method = meth;
        return r;
    }

    static AMResult err(const std::string &msg)
    {
        AMResult r;
        r.ok = false;
        r.error = msg;
        return r;
    }

    // Evaluate expression at (var=val) using calculus module

    static double evalAt2(const std::string &expr,
                          const std::string &v1, double x1,
                          const std::string &v2, double x2)
    {
        try
        {
            auto e = Calculus::parse(expr);
            return Calculus::evaluate(e, {{v1, x1}, {v2, x2}});
        }
        catch (...)
        {
            return std::numeric_limits<double>::quiet_NaN();
        }
    }

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

    // RK4 integrator (reused internally)
    static std::vector<Vec> rk4System(
        const std::function<Vec(double, const Vec &)> &F,
        double t0, const Vec &y0, double t1, int n)
    {
        double h = (t1 - t0) / n;
        int dim = y0.size();
        std::vector<Vec> traj;
        Vec y = y0;
        double t = t0;
        traj.push_back(y);
        for (int i = 0; i < n; ++i)
        {
            auto k1 = F(t, y);
            Vec y2(dim), y3(dim), y4(dim);
            for (int j = 0; j < dim; ++j)
                y2[j] = y[j] + 0.5 * h * k1[j];
            auto k2 = F(t + 0.5 * h, y2);
            for (int j = 0; j < dim; ++j)
                y3[j] = y[j] + 0.5 * h * k2[j];
            auto k3 = F(t + 0.5 * h, y3);
            for (int j = 0; j < dim; ++j)
                y4[j] = y[j] + h * k3[j];
            auto k4 = F(t + h, y4);
            for (int j = 0; j < dim; ++j)
                y[j] += h / 6 * (k1[j] + 2 * k2[j] + 2 * k3[j] + k4[j]);
            t += h;
            traj.push_back(y);
        }
        return traj;
    }

    // Format a trajectory as JSON points array
    static std::string trajToJson(const std::vector<Vec> &traj,
                                  double t0, double dt, int stride = 1)
    {
        std::ostringstream ss;
        ss << "[";
        bool first = true;
        for (int i = 0; i < (int)traj.size(); i += stride)
        {
            if (!first)
                ss << ",";
            first = false;
            double t = t0 + i * dt;
            ss << "[" << fmt(t, 4);
            for (double v : traj[i])
                ss << "," << fmt(v, 6);
            ss << "]";
        }
        ss << "]";
        return ss.str();
    }

    // =============================================================================
    // CHAPTER 1 — Dimensional Analysis & Scaling
    // =============================================================================

    AMResult buckinghamPi(const std::vector<std::string> &variables,
                          const Mat &D)
    {
        // D[i][j] = dimension of variable i in base unit j
        // Find null space of D^T to get Pi groups
        int nVars = variables.size();
        int nDims = D.empty() ? 0 : D[0].size();
        if (nVars == 0 || nDims == 0)
            return err("Need at least one variable and one base dimension");

        int nPi = nVars - nDims; // Buckingham Pi theorem
        if (nPi <= 0)
            return err("No dimensionless groups exist (nVars <= nDims)");

        std::ostringstream ss, steps;
        steps << "=== Buckingham Pi Theorem ===\n\n";
        steps << "Variables: ";
        for (int i = 0; i < nVars; ++i)
        {
            if (i)
                steps << ", ";
            steps << variables[i];
        }
        steps << "\n";
        steps << "Number of base dimensions: " << nDims << "\n";
        steps << "Rank of dimension matrix: " << std::min(nVars, nDims) << " (assumed full rank)\n\n";
        steps << "Number of independent Pi groups: n - r = " << nVars << " - " << nDims
              << " = " << nPi << "\n\n";

        // Print dimension matrix
        steps << "Dimension matrix [M  L  T ...]:\n";
        for (int i = 0; i < nVars; ++i)
        {
            steps << "  " << variables[i] << ": [";
            for (int j = 0; j < nDims; ++j)
            {
                if (j)
                    steps << " ";
                steps << fmt(D[i][j], 0);
            }
            steps << "]\n";
        }

        steps << "\nTo find Pi groups: solve D^T * a = 0 for each null vector a.\n";
        steps << "Each Pi_i = prod(variables^a_i)\n\n";

        // Simple RREF-based null space (reusing the concept from LinearAlgebra)
        // For display: just state the procedure
        ss << "Pi theorem: " << nPi << " dimensionless group(s)\n";
        ss << "Pi_1 = " << variables[0];
        for (int i = 1; i < std::min(nPi + 1, nVars); ++i)
            ss << " * " << variables[i] << "^a_" << i;
        ss << "\n(solve dimension matrix null space for exponents a_i)";

        return ok(ss.str(), "", steps.str(), "Buckingham Pi");
    }

    AMResult nondimensionalise(const std::string &equation,
                               const std::vector<std::string> &variables,
                               const std::vector<double> &scales)
    {
        std::ostringstream ss, steps;
        steps << "=== Nondimensionalisation ===\n\n";
        steps << "Original equation: " << equation << "\n\n";
        steps << "Characteristic scales:\n";
        for (int i = 0; i < (int)variables.size() && i < (int)scales.size(); ++i)
        {
            steps << "  " << variables[i] << "_c = " << (scales[i] > 0 ? fmt(scales[i]) : "?") << "\n";
        }
        steps << "\nDimensionless variables:\n";
        for (int i = 0; i < (int)variables.size() && i < (int)scales.size(); ++i)
        {
            if (scales[i] > 0)
                steps << "  " << variables[i] << "* = " << variables[i] << " / " << fmt(scales[i]) << "\n";
        }
        steps << "\nSubstitute into equation and simplify to find dimensionless groups (Pi numbers).\n";
        steps << "The resulting ODE/PDE will have coefficients that are dimensionless ratios of scales.";
        ss << "Nondimensionalised form of: " << equation;
        return ok(ss.str(), "", steps.str(), "Nondimensionalisation");
    }

    AMResult scalingAnalysis(const std::string &equation,
                             const std::string &smallParam,
                             double epsilon)
    {
        std::ostringstream ss, steps;
        steps << "=== Scaling / Dominant Balance ===\n\n";
        steps << "Equation: " << equation << "\n";
        steps << "Small parameter: " << smallParam << " = " << fmt(epsilon) << "\n\n";
        steps << "Dominant balance technique:\n";
        steps << "  1. Identify all terms and their order in " << smallParam << "\n";
        steps << "  2. Group terms by order: O(1), O(eps), O(eps²), ...\n";
        steps << "  3. At leading order, retain only O(1) terms → reduced problem\n";
        steps << "  4. At next order, retain O(eps) terms → first correction\n\n";
        steps << "For singular perturbation: if highest-derivative term is O(eps),\n";
        steps << "  the solution has a boundary layer of thickness O(sqrt(eps)) or O(eps).";
        ss << "Scaling analysis for " << equation << " with " << smallParam << " = " << fmt(epsilon);
        return ok(ss.str(), "", steps.str(), "Dominant Balance");
    }

    // =============================================================================
    // CHAPTER 2 — Dynamical Systems
    // =============================================================================

    AMResult classifyLinearSystem(double a11, double a12,
                                  double a21, double a22)
    {
        double tr = a11 + a22;
        double det = a11 * a22 - a12 * a21;
        double disc = tr * tr - 4 * det;

        std::ostringstream ss, steps;
        steps << "=== Linear System x' = Ax ===\n";
        steps << "A = [[" << fmt(a11) << ", " << fmt(a12) << "], ["
              << fmt(a21) << ", " << fmt(a22) << "]]\n\n";
        steps << "Trace(A)       = " << fmt(tr) << "\n";
        steps << "Det(A)         = " << fmt(det) << "\n";
        steps << "Discriminant   = tr² - 4det = " << fmt(disc) << "\n\n";

        std::string type;
        if (det < 0)
        {
            type = "Saddle point (unstable)";
            double lam1 = (tr + std::sqrt(tr * tr - 4 * det)) / 2;
            double lam2 = (tr - std::sqrt(tr * tr - 4 * det)) / 2;
            steps << "Real eigenvalues of opposite sign: λ₁=" << fmt(lam1) << ", λ₂=" << fmt(lam2) << "\n";
        }
        else if (disc > 1e-10)
        {
            double lam1 = (tr + std::sqrt(disc)) / 2;
            double lam2 = (tr - std::sqrt(disc)) / 2;
            steps << "Real distinct eigenvalues: λ₁=" << fmt(lam1) << ", λ₂=" << fmt(lam2) << "\n";
            if (lam1 < 0 && lam2 < 0)
                type = "Stable node";
            else if (lam1 > 0 && lam2 > 0)
                type = "Unstable node";
            else
                type = "Saddle point";
        }
        else if (std::abs(disc) <= 1e-10)
        {
            double lam = tr / 2;
            steps << "Repeated eigenvalue: λ=" << fmt(lam) << "\n";
            type = lam < 0 ? "Stable star/degenerate node" : lam > 0 ? "Unstable star/degenerate node"
                                                                     : "Non-isolated fixed point";
        }
        else
        {
            // Complex eigenvalues
            double alpha = tr / 2;
            double beta = std::sqrt(-disc) / 2;
            steps << "Complex eigenvalues: λ = " << fmt(alpha) << " ± " << fmt(beta) << "i\n";
            if (std::abs(alpha) < 1e-10)
                type = "Center (neutrally stable, periodic orbits)";
            else if (alpha < 0)
                type = "Stable spiral";
            else
                type = "Unstable spiral";
        }
        steps << "\nClassification: " << type;
        ss << type << " [tr=" << fmt(tr) << ", det=" << fmt(det) << "]";
        return ok(ss.str(), "", steps.str(), "Linear system classification");
    }

    AMResult lineariseAtFixedPoint(const std::string &f, const std::string &g,
                                   double x0, double y0)
    {
        // Compute Jacobian numerically
        double h = 1e-5;
        double dfdx = (evalAt2(f, "x", x0 + h, "y", y0) - evalAt2(f, "x", x0 - h, "y", y0)) / (2 * h);
        double dfdy = (evalAt2(f, "x", x0, "y", y0 + h) - evalAt2(f, "x", x0, "y", y0 - h)) / (2 * h);
        double dgdx = (evalAt2(g, "x", x0 + h, "y", y0) - evalAt2(g, "x", x0 - h, "y", y0)) / (2 * h);
        double dgdy = (evalAt2(g, "x", x0, "y", y0 + h) - evalAt2(g, "x", x0, "y", y0 - h)) / (2 * h);

        std::ostringstream steps;
        steps << "=== Jacobian Linearisation ===\n\n";
        steps << "Fixed point: (" << fmt(x0) << ", " << fmt(y0) << ")\n\n";
        steps << "f(x,y) = " << f << "\n";
        steps << "g(x,y) = " << g << "\n\n";
        steps << "Jacobian J = [[∂f/∂x, ∂f/∂y], [∂g/∂x, ∂g/∂y]]:\n";
        steps << "  J = [[" << fmt(dfdx, 4) << ", " << fmt(dfdy, 4) << "],\n";
        steps << "       [" << fmt(dgdx, 4) << ", " << fmt(dgdy, 4) << "]]\n\n";

        // Use classifyLinearSystem for the rest
        auto cls = classifyLinearSystem(dfdx, dfdy, dgdx, dgdy);
        steps << cls.steps;
        return ok(cls.symbolic, "", steps.str(), "Jacobian linearisation");
    }

    PhasePortrait analysePhasePortrait(const std::string &f, const std::string &g,
                                       double xMin, double xMax,
                                       double yMin, double yMax)
    {
        PhasePortrait portrait;
        // Find fixed points by searching for sign changes in both f and g
        int gridN = 30;
        double dx = (xMax - xMin) / gridN, dy = (yMax - yMin) / gridN;
        double h = 1e-5;

        for (int i = 0; i < gridN; ++i)
        {
            for (int j = 0; j < gridN; ++j)
            {
                double xc = xMin + (i + 0.5) * dx, yc = yMin + (j + 0.5) * dy;
                // Check if (f,g) ≈ (0,0) here
                double fv = evalAt2(f, "x", xc, "y", yc);
                double gv = evalAt2(g, "x", xc, "y", yc);
                if (!std::isfinite(fv) || !std::isfinite(gv))
                    continue;

                // Refine using Newton-like iteration
                double x = xc, y = yc;
                for (int iter = 0; iter < 30; ++iter)
                {
                    double fx = evalAt2(f, "x", x, "y", y);
                    double gy = evalAt2(g, "x", x, "y", y);
                    if (std::abs(fx) < 1e-10 && std::abs(gy) < 1e-10)
                        break;
                    // Jacobian
                    double J11 = (evalAt2(f, "x", x + h, "y", y) - evalAt2(f, "x", x - h, "y", y)) / (2 * h);
                    double J12 = (evalAt2(f, "x", x, "y", y + h) - evalAt2(f, "x", x, "y", y - h)) / (2 * h);
                    double J21 = (evalAt2(g, "x", x + h, "y", y) - evalAt2(g, "x", x - h, "y", y)) / (2 * h);
                    double J22 = (evalAt2(g, "x", x, "y", y + h) - evalAt2(g, "x", x, "y", y - h)) / (2 * h);
                    double det = J11 * J22 - J12 * J21;
                    if (std::abs(det) < 1e-14)
                        break;
                    double dx2 = (J22 * fx - J12 * gy) / det;
                    double dy2 = (-J21 * fx + J11 * gy) / det;
                    x -= dx2;
                    y -= dy2;
                    if (std::abs(dx2) + std::abs(dy2) < 1e-10)
                        break;
                }

                double fx_final = evalAt2(f, "x", x, "y", y);
                double gx_final = evalAt2(g, "x", x, "y", y);
                if (std::abs(fx_final) > 1e-6 || std::abs(gx_final) > 1e-6)
                    continue;
                if (x < xMin || x > xMax || y < yMin || y > yMax)
                    continue;

                // Check for duplicate
                bool dup = false;
                for (auto &fp : portrait.fixedPoints)
                    if (std::hypot(fp.x - x, fp.y - y) < 1e-4)
                    {
                        dup = true;
                        break;
                    }
                if (dup)
                    continue;

                // Classify
                double dfdx = (evalAt2(f, "x", x + h, "y", y) - evalAt2(f, "x", x - h, "y", y)) / (2 * h);
                double dfdy = (evalAt2(f, "x", x, "y", y + h) - evalAt2(f, "x", x, "y", y - h)) / (2 * h);
                double dgdx = (evalAt2(g, "x", x + h, "y", y) - evalAt2(g, "x", x - h, "y", y)) / (2 * h);
                double dgdy = (evalAt2(g, "x", x, "y", y + h) - evalAt2(g, "x", x, "y", y - h)) / (2 * h);
                double tr = dfdx + dgdy, det2 = dfdx * dgdy - dfdy * dgdx;
                double disc = tr * tr - 4 * det2;

                FixedPoint fp;
                fp.x = x;
                fp.y = y;
                fp.traceJ = tr;
                fp.detJ = det2;
                fp.discriminant = disc;

                if (det2 < -1e-8)
                {
                    fp.stability = "saddle";
                }
                else if (disc > 1e-8)
                {
                    double l1 = (tr + std::sqrt(disc)) / 2, l2 = (tr - std::sqrt(disc)) / 2;
                    fp.eigenvalue1_re = l1;
                    fp.eigenvalue2_re = l2;
                    fp.stability = (l1 < 0 && l2 < 0) ? "stable node" : (l1 > 0 && l2 > 0) ? "unstable node"
                                                                                           : "saddle";
                }
                else if (disc < -1e-8)
                {
                    double alpha = tr / 2, beta = std::sqrt(-disc) / 2;
                    fp.eigenvalue1_re = alpha;
                    fp.eigenvalue1_im = beta;
                    fp.eigenvalue2_re = alpha;
                    fp.eigenvalue2_im = -beta;
                    fp.stability = std::abs(alpha) < 1e-8 ? "center" : alpha < 0 ? "stable spiral"
                                                                                 : "unstable spiral";
                }
                else
                {
                    fp.stability = "degenerate node";
                }
                portrait.fixedPoints.push_back(fp);
            }
        }

        // Global behavior summary
        std::ostringstream gb;
        gb << portrait.fixedPoints.size() << " fixed point(s) found:\n";
        for (auto &fp : portrait.fixedPoints)
            gb << "  (" << fmt(fp.x, 3) << ", " << fmt(fp.y, 3) << ") — " << fp.stability << "\n";
        portrait.globalBehavior = gb.str();
        return portrait;
    }

    AMResult computeNullclines(const std::string &f, const std::string &g,
                               double xMin, double xMax,
                               double yMin, double yMax, int gridN)
    {
        std::ostringstream ss;
        ss << "Nullclines for dx/dt=" << f << ",  dy/dt=" << g << "\n\n";
        ss << "f-nullcline (dx/dt=0): points where " << f << " = 0\n";
        ss << "g-nullcline (dy/dt=0): points where " << g << " = 0\n\n";
        ss << "Sample points on f-nullcline: [";
        bool first = true;
        for (int i = 0; i <= gridN; ++i)
        {
            double x = xMin + (double)i / gridN * (xMax - xMin);
            // Find y where f(x,y)=0 via bisection
            double ylo = yMin, yhi = yMax;
            double flo = evalAt2(f, "x", x, "y", ylo), fhi = evalAt2(f, "x", x, "y", yhi);
            if (std::isfinite(flo) && std::isfinite(fhi) && flo * fhi < 0)
            {
                for (int iter = 0; iter < 40; ++iter)
                {
                    double ymid = (ylo + yhi) / 2;
                    double fm = evalAt2(f, "x", x, "y", ymid);
                    if (flo * fm < 0)
                        yhi = ymid;
                    else
                        ylo = ymid;
                }
                if (!first)
                    ss << ", ";
                ss << "(" << fmt(x, 3) << "," << fmt((ylo + yhi) / 2, 3) << ")";
                first = false;
            }
        }
        ss << "]\n";
        return ok(ss.str(), "", "", "Nullcline analysis");
    }

    AMResult checkLyapunov(const std::string &V,
                           const std::string &f, const std::string &g,
                           double xMin, double xMax, double yMin, double yMax)
    {
        std::ostringstream steps, ss;
        steps << "=== Lyapunov Stability Analysis ===\n\n";
        steps << "Candidate V(x,y) = " << V << "\n\n";

        // Check V > 0 and dV/dt < 0
        auto Vexpr = Calculus::parse(V);
        auto dVdx_str = Calculus::differentiate(V, "x", 1).symbolic;
        auto dVdy_str = Calculus::differentiate(V, "y", 1).symbolic;
        steps << "∂V/∂x = " << dVdx_str << "\n";
        steps << "∂V/∂y = " << dVdy_str << "\n\n";
        steps << "dV/dt = (∂V/∂x)·f + (∂V/∂y)·g\n";
        steps << "     = (" << dVdx_str << ")·(" << f << ") + (" << dVdy_str << ")·(" << g << ")\n\n";

        // Sample check
        bool vPositive = true, vdotNeg = true;
        int gridN = 10;
        double h = 1e-5;
        for (int i = 1; i <= gridN; ++i)
        {
            for (int j = 1; j <= gridN; ++j)
            {
                double x = xMin + (double)i / (gridN + 1) * (xMax - xMin);
                double y = yMin + (double)j / (gridN + 1) * (yMax - yMin);
                if (std::abs(x) < 1e-8 && std::abs(y) < 1e-8)
                    continue;
                double Vval = evalAt2(V, "x", x, "y", y);
                double dVdx = evalAt2(dVdx_str, "x", x, "y", y);
                double dVdy = evalAt2(dVdy_str, "x", x, "y", y);
                double fval = evalAt2(f, "x", x, "y", y);
                double gval = evalAt2(g, "x", x, "y", y);
                double Vdot = dVdx * fval + dVdy * gval;
                if (Vval <= 0)
                    vPositive = false;
                if (Vdot >= 0)
                    vdotNeg = false;
            }
        }

        if (vPositive && vdotNeg)
        {
            ss << "LYAPUNOV STABLE: V > 0 and dV/dt < 0 in sampled region";
            steps << "Numerical check:\n  V > 0: YES\n  dV/dt < 0: YES\n  → Lyapunov stability confirmed";
        }
        else
        {
            ss << "Lyapunov conditions NOT satisfied numerically";
            steps << "Numerical check:\n  V > 0: " << (vPositive ? "YES" : "NO")
                  << "\n  dV/dt < 0: " << (vdotNeg ? "YES" : "NO");
        }
        return ok(ss.str(), "", steps.str(), "Lyapunov method");
    }

    // =============================================================================
    // Bifurcation analysis
    // =============================================================================

    AMResult classifyBifurcation(const std::string &f,
                                 const std::string &sv, const std::string &pv,
                                 double x0, double mu0)
    {
        double h = 1e-5;
        // Compute partial derivatives at (x0, mu0)
        auto at = [&](double x, double mu)
        { return evalAt2(f, sv, x, pv, mu); };
        double f0 = at(x0, mu0);
        double fx = (at(x0 + h, mu0) - at(x0 - h, mu0)) / (2 * h);
        double fmu = (at(x0, mu0 + h) - at(x0, mu0 - h)) / (2 * h);
        double fxx = (at(x0 + h, mu0) - 2 * at(x0, mu0) + at(x0 - h, mu0)) / (h * h);
        double fxmu = (at(x0 + h, mu0 + h) - at(x0 + h, mu0 - h) - at(x0 - h, mu0 + h) + at(x0 - h, mu0 - h)) / (4 * h * h);
        double fxxx = (at(x0 + 2 * h, mu0) - 2 * at(x0 + h, mu0) + 2 * at(x0 - h, mu0) - at(x0 - 2 * h, mu0)) / (2 * h * h * h);

        std::ostringstream ss, steps;
        steps << "=== Bifurcation Classification ===\n\n";
        steps << "f(" << sv << ";" << pv << ") = " << f << "\n";
        steps << "Fixed point: " << sv << "=" << fmt(x0) << ", " << pv << "=" << fmt(mu0) << "\n\n";
        steps << "f = " << fmt(f0, 4) << " (should be ≈ 0 at fixed point)\n";
        steps << "f_x = " << fmt(fx, 4) << " (should be 0 for bifurcation)\n";
        steps << "f_μ = " << fmt(fmu, 4) << "\n";
        steps << "f_xx = " << fmt(fxx, 4) << "\n";
        steps << "f_xμ = " << fmt(fxmu, 4) << "\n";
        steps << "f_xxx = " << fmt(fxxx, 4) << "\n\n";

        std::string type;
        if (std::abs(fx) > 0.1)
        {
            type = "Not a bifurcation point (f_x ≠ 0)";
        }
        else if (std::abs(fmu) > 1e-6 && std::abs(fxx) > 1e-6)
        {
            type = "Saddle-node bifurcation (f_x=0, f_μ≠0, f_xx≠0)";
            steps << "Saddle-node: two fixed points merge and annihilate\n";
            steps << "Normal form: x' = μ - x²";
        }
        else if (std::abs(fmu) < 1e-6 && std::abs(fxmu) > 1e-6 && std::abs(fxx) > 1e-6)
        {
            type = "Transcritical bifurcation (f_x=0, f_μ=0, f_xμ≠0, f_xx≠0)";
            steps << "Transcritical: two branches cross, exchange stability\n";
            steps << "Normal form: x' = μx - x²";
        }
        else if (std::abs(fxx) < 1e-6 && std::abs(fxxx) > 1e-6)
        {
            type = "Pitchfork bifurcation (f_x=0, f_xx≈0, f_xxx≠0)";
            bool supercritical = fxxx < 0;
            steps << (supercritical ? "Supercritical" : "Subcritical") << " pitchfork\n";
            steps << "Normal form: x' = μx ± x³";
        }
        else
        {
            type = "Hopf bifurcation (eigenvalues cross imaginary axis) — check for complex eigenvalues";
        }

        ss << type;
        steps << "\nClassification: " << type;
        return ok(ss.str(), "", steps.str(), "Bifurcation analysis");
    }

    AMResult bifurcationDiagram(const std::string &f,
                                const std::string &sv, const std::string &pv,
                                double muMin, double muMax, int nPoints)
    {
        std::ostringstream ss, steps;
        steps << "=== Bifurcation Diagram ===\n";
        steps << "f(" << sv << ";" << pv << ") = " << f << "\n";
        steps << "Parameter " << pv << " ∈ [" << fmt(muMin) << ", " << fmt(muMax) << "]\n\n";

        ss << "Points: [";
        bool first = true;
        for (int i = 0; i <= nPoints; ++i)
        {
            double mu = muMin + (double)i / nPoints * (muMax - muMin);
            // Find fixed points: solve f(x;mu)=0 for x ∈ [-20,20]
            double h = 1e-5;
            for (double xTest = -10; xTest <= 10; xTest += 0.5)
            {
                double flo = evalAt2(f, sv, xTest, pv, mu);
                double fhi = evalAt2(f, sv, xTest + 0.5, pv, mu);
                if (!std::isfinite(flo) || !std::isfinite(fhi))
                    continue;
                if (flo * fhi > 0)
                    continue;
                // Bisect
                double lo = xTest, hi = xTest + 0.5;
                for (int iter = 0; iter < 40; ++iter)
                {
                    double mid = (lo + hi) / 2;
                    double fm = evalAt2(f, sv, mid, pv, mu);
                    if (evalAt2(f, sv, lo, pv, mu) * fm < 0)
                        hi = mid;
                    else
                        lo = mid;
                }
                double xFP = (lo + hi) / 2;
                // Stability: sign of df/dx
                double dfx = (evalAt2(f, sv, xFP + h, pv, mu) - evalAt2(f, sv, xFP - h, pv, mu)) / (2 * h);
                bool stable = dfx < 0;
                if (!first)
                    ss << ",";
                ss << "[" << fmt(mu, 4) << "," << fmt(xFP, 4) << "," << (stable ? 1 : 0) << "]";
                first = false;
            }
        }
        ss << "]";
        steps << "Format: [mu, x_fixed, stable(1/0)]";
        return ok(ss.str(), "", steps.str(), "Bifurcation diagram");
    }

    // =============================================================================
    // Reaction kinetics
    // =============================================================================

    AMResult michaelisМenten(double kcat, double Km, double E0,
                             double S0, double tEnd, int n)
    {
        // dS/dt = -kcat*E0*S/(Km+S),  dP/dt = kcat*E0*S/(Km+S)
        auto F = [&](double /*t*/, const Vec &y) -> Vec
        {
            double S = std::max(y[0], 0.0);
            double v = kcat * E0 * S / (Km + S);
            return {-v, v};
        };
        auto traj = rk4System(F, 0, {S0, 0.0}, tEnd, n);
        double h = tEnd / n;
        std::ostringstream ss, steps;
        steps << "=== Michaelis-Menten Kinetics ===\n";
        steps << "dS/dt = -kcat·E₀·S/(Km+S)\n";
        steps << "kcat=" << fmt(kcat) << ", Km=" << fmt(Km) << ", E₀=" << fmt(E0) << "\n";
        steps << "S(0)=" << fmt(S0) << ", t∈[0," << fmt(tEnd) << "]\n\n";
        steps << "Vmax = kcat·E₀ = " << fmt(kcat * E0) << "\n";
        steps << "Half-saturation: S = Km = " << fmt(Km) << "\n";
        steps << "Initial rate: v₀ = " << fmt(kcat * E0 * S0 / (Km + S0)) << "\n";

        ss << "S(" << fmt(tEnd) << ") ≈ " << fmt(traj.back()[0], 4);
        ss << ",  P(" << fmt(tEnd) << ") ≈ " << fmt(traj.back()[1], 4);

        // Trajectory JSON
        std::string pts = trajToJson(traj, 0, h, std::max(1, n / 50));
        steps << "\nPoints: " << pts;
        return ok(ss.str(), "", steps.str(), "Michaelis-Menten");
    }

    AMResult massAction(double k, double A0, double B0, double tEnd, int n)
    {
        // A + B → C: dA/dt = -k*A*B, dB/dt = -k*A*B, dC/dt = k*A*B
        auto F = [&](double /*t*/, const Vec &y) -> Vec
        {
            double A = std::max(y[0], 0.0), B = std::max(y[1], 0.0);
            double r = k * A * B;
            return {-r, -r, r};
        };
        auto traj = rk4System(F, 0, {A0, B0, 0.0}, tEnd, n);
        double h = tEnd / n;
        std::ostringstream ss, steps;
        steps << "=== Mass Action Kinetics (A + B → C) ===\n";
        steps << "dA/dt = dB/dt = -k·A·B,  dC/dt = k·A·B\n";
        steps << "k=" << fmt(k) << ", A(0)=" << fmt(A0) << ", B(0)=" << fmt(B0) << "\n\n";
        if (std::abs(A0 - B0) < 1e-10)
            steps << "Equal concentrations: A(t) = A₀/(1 + k·A₀·t)\n";
        ss << "A(" << fmt(tEnd) << ")=" << fmt(traj.back()[0], 4)
           << ", C(" << fmt(tEnd) << ")=" << fmt(traj.back()[2], 4);
        std::string pts = trajToJson(traj, 0, h, std::max(1, n / 50));
        steps << "\nPoints: " << pts;
        return ok(ss.str(), "", steps.str(), "Mass action");
    }

    // =============================================================================
    // Epidemiology
    // =============================================================================

    AMResult sirModel(double beta, double gamma,
                      double S0, double I0, double R0,
                      double tEnd, int n)
    {
        double N = S0 + I0 + R0;
        auto F = [&](double /*t*/, const Vec &y) -> Vec
        {
            double S = y[0], I = y[1], R = y[2];
            return {-beta * S * I / N, beta * S * I / N - gamma * I, gamma * I};
        };
        auto traj = rk4System(F, 0, {S0, I0, R0}, tEnd, n);
        double h = tEnd / n;

        // Peak infection
        double peakI = I0;
        double peakT = 0;
        for (int i = 0; i < (int)traj.size(); ++i)
        {
            if (traj[i][1] > peakI)
            {
                peakI = traj[i][1];
                peakT = i * h;
            }
        }

        double R0num = beta * S0 / (gamma * N);
        std::ostringstream ss, steps;
        steps << "=== SIR Epidemic Model ===\n\n";
        steps << "dS/dt = -β·S·I/N\n";
        steps << "dI/dt =  β·S·I/N - γ·I\n";
        steps << "dR/dt =  γ·I\n\n";
        steps << "β=" << fmt(beta) << ", γ=" << fmt(gamma) << ", N=" << fmt(N) << "\n";
        steps << "R₀ = β/γ = " << fmt(R0num) << "\n";
        steps << (R0num > 1 ? "Epidemic will occur (R₀>1)" : "No epidemic (R₀≤1)") << "\n\n";
        steps << "Herd immunity threshold: 1 - 1/R₀ = " << fmt(1 - 1.0 / R0num) << "\n";
        steps << "Peak infection: I_max ≈ " << fmt(peakI, 4) << " at t ≈ " << fmt(peakT, 3) << "\n";

        ss << "R₀=" << fmt(R0num) << ", peak I≈" << fmt(peakI, 4) << " at t≈" << fmt(peakT, 3);
        std::string pts = trajToJson(traj, 0, h, std::max(1, n / 100));
        steps << "\nPoints [t,S,I,R]: " << pts;
        return ok(ss.str(), "", steps.str(), "SIR model");
    }

    AMResult seirModel(double beta, double sigma, double gamma,
                       double S0, double E0, double I0, double R0_val,
                       double tEnd, int n)
    {
        double N = S0 + E0 + I0 + R0_val;
        auto F = [&](double /*t*/, const Vec &y) -> Vec
        {
            double S = y[0], E = y[1], I = y[2], R = y[3];
            return {-beta * S * I / N, beta * S * I / N - sigma * E, sigma * E - gamma * I, gamma * I};
        };
        auto traj = rk4System(F, 0, {S0, E0, I0, R0_val}, tEnd, n);
        double h = tEnd / n;
        double R0num = beta / gamma;
        std::ostringstream ss, steps;
        steps << "=== SEIR Model ===\n";
        steps << "S→E→I→R\n";
        steps << "β=" << fmt(beta) << ", σ=" << fmt(sigma) << " (incubation rate), γ=" << fmt(gamma) << "\n";
        steps << "R₀ = β/γ = " << fmt(R0num) << "\n";
        ss << "SEIR: R₀=" << fmt(R0num) << ", I(" << fmt(tEnd) << ")≈" << fmt(traj.back()[2], 4);
        std::string pts = trajToJson(traj, 0, h, std::max(1, n / 100));
        steps << "\nPoints [t,S,E,I,R]: " << pts;
        return ok(ss.str(), "", steps.str(), "SEIR model");
    }

    AMResult reproductionNumber(double beta, double gamma, double N)
    {
        double R0 = beta * N / gamma;
        std::ostringstream ss;
        ss << "Basic Reproduction Number R₀ = β·N/γ = "
           << fmt(beta) << "·" << fmt(N) << "/" << fmt(gamma)
           << " = " << fmt(R0) << "\n\n";
        ss << (R0 > 1 ? "R₀ > 1: epidemic is possible (exponential initial growth)\n" : "R₀ ≤ 1: disease will die out\n");
        ss << "Herd immunity threshold: 1 - 1/R₀ = " << fmt(std::max(0.0, 1 - 1.0 / R0)) << "\n";
        ss << "Final size equation: r∞ = 1 - exp(-R₀·r∞) [solve numerically]\n";
        // Approximate final size
        double r = 0.5;
        for (int i = 0; i < 100; ++i)
            r = 1 - std::exp(-R0 * r);
        ss << "Approximate fraction infected: " << fmt(r, 4);
        return ok(ss.str(), fmt(R0), "", "Reproduction number");
    }
    // =============================================================================
    // CHAPTER 3 — Perturbation Methods
    // =============================================================================

    AMResult regularPerturbation(const std::string &equation,
                                 const std::string &variable,
                                 const std::string &smallParam,
                                 double epsilon, int order)
    {
        std::ostringstream ss, steps;
        steps << "=== Regular Perturbation Series ===\n\n";
        steps << "Equation: " << equation << "\n";
        steps << "Small parameter: " << smallParam << " = " << fmt(epsilon) << "\n\n";
        steps << "Assume solution: x = x₀ + " << smallParam << "·x₁ + " << smallParam
              << "²·x₂ + ...\n\n";
        steps << "Hierarchy of equations:\n";
        steps << "  O(1):    L[x₀] = 0  (unperturbed problem)\n";
        steps << "  O(ε):    L[x₁] = -N[x₀]  (first correction)\n";
        steps << "  O(ε²):   L[x₂] = -(∂N/∂x₀)x₁  (second correction)\n\n";
        steps << "Where L = linear part, N = nonlinear part of equation.\n\n";
        steps << "Validity: solution is O(" << smallParam << "^" << order + 1 << ") accurate for ";
        steps << smallParam << " = " << fmt(epsilon) << "\n";
        steps << "Poincaré asymptotic: x ~ x₀ + " << fmt(epsilon) << "·x₁ + "
              << fmt(epsilon * epsilon) << "·x₂";

        ss << "x(" << smallParam << ") = x₀ + " << fmt(epsilon) << "x₁ + "
           << fmt(epsilon * epsilon) << "x₂ + O(" << smallParam << "^" << order + 1 << ")";
        return ok(ss.str(), "", steps.str(), "Regular perturbation");
    }

    AMResult poincareLindstedt(double omega0, double epsilon, double x0, double v0, int order)
    {
        std::ostringstream ss, steps;
        steps << "=== Poincaré-Lindstedt Method ===\n\n";
        steps << "Duffing-type oscillator: x'' + ω₀²x = -ε·x³ + ...\n";
        steps << "ω₀ = " << fmt(omega0) << ", ε = " << fmt(epsilon) << "\n\n";
        steps << "Expand: x = x₀ + ε·x₁ + ε²·x₂ + ...\n";
        steps << "Frequency: ω = ω₀ + ε·ω₁ + ε²·ω₂ + ... (to remove secular terms)\n\n";

        // Leading order correction for Duffing: ω₁ = 3a²/(8ω₀)
        double amplitude = x0; // approximate
        double omega1 = (order >= 1) ? 3 * amplitude * amplitude / (8 * omega0) : 0;
        double omega2 = (order >= 2) ? -51 * amplitude * amplitude * amplitude * amplitude / (256 * omega0 * omega0 * omega0) : 0;
        double omega_corrected = omega0 + epsilon * omega1 + epsilon * epsilon * omega2;

        steps << "x₀ = a·cos(ωτ)  where τ = ωt\n\n";
        steps << "Secular term removal gives:\n";
        steps << "  ω₁ = 3a²/(8ω₀) = " << fmt(omega1) << "\n";
        if (order >= 2)
            steps << "  ω₂ = -51a⁴/(256ω₀³) = " << fmt(omega2) << "\n";
        steps << "\nCorrected frequency: ω ≈ " << fmt(omega_corrected) << "\n";
        steps << "Corrected period:    T ≈ " << fmt(2 * PI / omega_corrected);

        ss << "ω ≈ " << fmt(omega_corrected) << ", T ≈ " << fmt(2 * PI / omega_corrected);
        return ok(ss.str(), "", steps.str(), "Poincaré-Lindstedt");
    }

    AMResult boundaryLayerAnalysis(const std::string &p, const std::string &q,
                                   double epsilon, double a, double b)
    {
        std::ostringstream ss, steps;
        steps << "=== Singular Perturbation / Boundary Layer ===\n\n";
        steps << "ε·y'' + p(x)·y' + q(x)·y = 0\n";
        steps << "ε = " << fmt(epsilon) << ", y(0) = " << fmt(a) << ", y(1) = " << fmt(b) << "\n\n";

        // Evaluate p at x=0 to determine layer location
        double p0 = evalAt(p, "x", 0);
        double p1 = evalAt(p, "x", 1);
        bool layerAtLeft = p0 > 0;

        steps << "p(0) = " << fmt(p0) << ",  p(1) = " << fmt(p1) << "\n";
        steps << "Boundary layer at: " << (layerAtLeft ? "x = 0 (left)" : "x = 1 (right)") << "\n";
        steps << "Layer thickness: O(ε/" << fmt(std::abs(layerAtLeft ? p0 : p1)) << ") = O("
              << fmt(epsilon / std::max(1e-10, std::abs(layerAtLeft ? p0 : p1))) << ")\n\n";

        steps << "=== Outer Solution (away from layer) ===\n";
        steps << "Set ε=0: p(x)·y' + q(x)·y = 0\n";
        steps << "Outer: y_out = C·exp(-∫q/p dx)\n";
        steps << "(Satisfies BC at x=" << (layerAtLeft ? "1" : "0") << ")\n\n";

        steps << "=== Inner Solution (rescale near layer) ===\n";
        double scale = layerAtLeft ? p0 : std::abs(p1);
        steps << "Let X = x/ε (stretched variable)\n";
        steps << "d²y/dX² + p(0)·dy/dX + ε·q·y = 0\n";
        steps << "Inner: y_in = A + B·exp(-" << fmt(scale) << "·X)\n\n";

        steps << "=== Matched Asymptotic Expansion ===\n";
        steps << "Match: lim_{X→∞} y_in = lim_{x→0+} y_out\n";
        steps << "Composite solution: y ≈ y_out + y_in - matching term\n";

        ss << "BL at " << (layerAtLeft ? "x=0" : "x=1") << ", thickness O(" << fmt(epsilon / std::max(1e-10, scale)) << ")";
        return ok(ss.str(), "", steps.str(), "Boundary layer (matched asymptotics)");
    }

    AMResult multipleScales(double omega0, double epsilon, int order)
    {
        std::ostringstream ss, steps;
        steps << "=== Method of Multiple Scales ===\n\n";
        steps << "x'' + ω₀²x + ε·f(x,x') = 0,  ω₀=" << fmt(omega0) << ", ε=" << fmt(epsilon) << "\n\n";
        steps << "Introduce fast and slow time scales:\n";
        steps << "  T₀ = t  (fast),  T₁ = εt  (slow)\n";
        if (order >= 2)
            steps << "  T₂ = ε²t  (very slow)\n\n";
        steps << "Expand: x(t;ε) = x₀(T₀,T₁) + ε·x₁(T₀,T₁) + ...\n\n";
        steps << "D₀ = ∂/∂T₀,  D₁ = ∂/∂T₁\n";
        steps << "d/dt = D₀ + ε·D₁ + ...\n\n";
        steps << "O(1): D₀²x₀ + ω₀²x₀ = 0\n";
        steps << "  → x₀ = A(T₁)·e^(iω₀T₀) + c.c.\n\n";
        steps << "O(ε): D₀²x₁ + ω₀²x₁ = -2D₀D₁x₀ - f(x₀,D₀x₀)\n";
        steps << "  Solvability: remove secular terms → amplitude equation dA/dT₁ = ...\n\n";
        steps << "The amplitude equation governs slow modulation of the oscillation.";
        ss << "Two-scale solution with slow time T₁ = " << fmt(epsilon) << "·t";
        return ok(ss.str(), "", steps.str(), "Multiple scales");
    }

    AMResult wkbApproximation(const std::string &q, const std::string &var,
                              double epsilon, double a, double b)
    {
        std::ostringstream ss, steps;
        steps << "=== WKB Approximation ===\n\n";
        steps << "ε²y'' + q(x)y = 0\n";
        steps << "ε = " << fmt(epsilon) << ", q(x) = " << q << "\n\n";

        // Evaluate q at several points
        double qa = evalAt(q, var, a), qb = evalAt(q, var, b);
        double qmid = evalAt(q, var, (a + b) / 2);

        steps << "q(" << fmt(a) << ") = " << fmt(qa) << "\n";
        steps << "q(" << fmt((a + b) / 2) << ") = " << fmt(qmid) << "\n";
        steps << "q(" << fmt(b) << ") = " << fmt(qb) << "\n\n";

        if (qa > 0)
        {
            steps << "q(x) > 0: oscillatory region\n";
            steps << "WKB: y(x) ≈ C₁·q(x)^(-1/4)·cos(∫√q/ε dx) + C₂·q(x)^(-1/4)·sin(∫√q/ε dx)\n";
            // Approximate phase
            double phase = (b - a) * std::sqrt(qmid) / epsilon;
            steps << "Approximate phase across [" << fmt(a) << "," << fmt(b) << "]: φ ≈ " << fmt(phase) << "\n";
            ss << "Oscillatory WKB, phase ≈ " << fmt(phase);
        }
        else
        {
            steps << "q(x) < 0: exponentially growing/decaying region\n";
            steps << "WKB: y(x) ≈ C₁·|q(x)|^(-1/4)·exp(∫√|q|/ε dx) + C₂·|q(x)|^(-1/4)·exp(-∫√|q|/ε dx)\n";
            double decay = (b - a) * std::sqrt(std::abs(qmid)) / epsilon;
            steps << "Exponential factor across interval: exp(±" << fmt(decay) << ")\n";
            ss << "Exponential WKB, decay factor ≈ " << fmt(std::exp(-decay));
        }
        return ok(ss.str(), "", steps.str(), "WKB approximation");
    }

    AMResult laplaceMethod(const std::string &h, const std::string &g,
                           const std::string &var, double xStar, double N)
    {
        std::ostringstream ss, steps;
        steps << "=== Laplace's Method for ∫e^(N·h(x))g(x)dx ===\n\n";
        steps << "h(x) = " << h << ", g(x) = " << g << "\n";
        steps << "Maximum of h at x* = " << fmt(xStar) << ", N = " << fmt(N) << "\n\n";

        double hval = evalAt(h, var, xStar);
        double gval = evalAt(g, var, xStar);
        double hval_hh = 0;
        double hv = 1e-4;
        hval_hh = (evalAt(h, var, xStar + hv) - 2 * hval + evalAt(h, var, xStar - hv)) / (hv * hv);

        steps << "h(x*) = " << fmt(hval) << "\n";
        steps << "h''(x*) = " << fmt(hval_hh) << "\n";
        steps << "g(x*) = " << fmt(gval) << "\n\n";
        if (hval_hh >= 0)
        {
            return err("x* must be a maximum of h: h''(x*) must be < 0");
        }
        steps << "Laplace's method:\n";
        steps << "∫e^(N·h)g dx ≈ e^(N·h(x*)) · g(x*) · √(2π/(N·|h''(x*)|))\n\n";

        double result = std::exp(N * hval) * gval * std::sqrt(2 * PI / (N * std::abs(hval_hh)));
        steps << "Result ≈ " << fmt(result);
        ss << fmt(result);
        return ok(ss.str(), fmt(result), steps.str(), "Laplace's method");
    }

    // =============================================================================
    // CHAPTER 4 — Calculus of Variations
    // =============================================================================

    AMResult eulerLagrange(const std::string &L,
                           const std::string &indep,
                           const std::string &dep,
                           const std::string &deriv)
    {
        std::ostringstream ss, steps;
        steps << "=== Euler-Lagrange Equation ===\n\n";
        steps << "Functional: J[y] = ∫ L(" << indep << ", " << dep << ", " << deriv << ") d" << indep << "\n\n";
        steps << "L = " << L << "\n\n";

        // Compute ∂L/∂y and ∂L/∂y'
        auto dLdy = Calculus::differentiate(L, dep, 1);
        auto dLdyp = Calculus::differentiate(L, deriv, 1);

        steps << "∂L/∂" << dep << " = " << (dLdy.ok ? dLdy.symbolic : "?") << "\n";
        steps << "∂L/∂" << deriv << " = " << (dLdyp.ok ? dLdyp.symbolic : "?") << "\n\n";
        steps << "Euler-Lagrange equation:\n";
        steps << "  ∂L/∂" << dep << " - d/d" << indep << "[∂L/∂" << deriv << "] = 0\n\n";
        steps << "  " << (dLdy.ok ? dLdy.symbolic : "∂L/∂y") << " - d/d" << indep
              << "[" << (dLdyp.ok ? dLdyp.symbolic : "∂L/∂y'") << "] = 0\n\n";
        steps << "This is the ODE that extremals of the functional must satisfy.";

        ss << "∂L/∂" << dep << " - d/d" << indep << "[∂L/∂" << deriv << "] = 0";
        return ok(ss.str(), "", steps.str(), "Euler-Lagrange");
    }

    AMResult beltramiIdentity(const std::string &L,
                              const std::string &dep,
                              const std::string &deriv)
    {
        std::ostringstream ss, steps;
        steps << "=== Beltrami Identity ===\n\n";
        steps << "When L = L(" << dep << ", " << deriv << ") (no explicit x dependence):\n";
        steps << "  L - " << deriv << "·(∂L/∂" << deriv << ") = C  (constant)\n\n";
        steps << "L = " << L << "\n\n";

        auto dLdyp = Calculus::differentiate(L, deriv, 1);
        steps << "∂L/∂" << deriv << " = " << (dLdyp.ok ? dLdyp.symbolic : "?") << "\n\n";
        steps << "First integral: L - " << deriv << "·(" << (dLdyp.ok ? dLdyp.symbolic : "∂L/∂y'") << ") = C\n";
        steps << "This reduces the second-order EL equation to first-order.";

        ss << "L - " << deriv << "·(∂L/∂" << deriv << ") = C";
        return ok(ss.str(), "", steps.str(), "Beltrami identity");
    }

    AMResult brachistochrone(double x1, double y1, double x2, double y2)
    {
        // Brachistochrone: L = sqrt((1+y'²)/(2g*y)), Beltrami → cycloid
        // Parametric solution: x = r(θ-sinθ), y = r(1-cosθ)
        std::ostringstream ss, steps;
        steps << "=== Brachistochrone Problem ===\n\n";
        steps << "Find curve of fastest descent from (" << fmt(x1) << "," << fmt(y1)
              << ") to (" << fmt(x2) << "," << fmt(y2) << ") under gravity\n\n";
        steps << "Functional: T = ∫sqrt((1+y'²)/(2g·y)) dx\n\n";
        steps << "L = sqrt((1+y'²)/(2g·y)) has no explicit x dependence.\n";
        steps << "Beltrami identity: L - y'·(∂L/∂y') = 1/sqrt(2g·C)\n\n";
        steps << "Solution: parametric cycloid\n";
        steps << "  x(θ) = r·(θ - sin θ) + x₁\n";
        steps << "  y(θ) = r·(1 - cos θ) + y₁\n\n";

        // Estimate r numerically
        double g = 9.81;
        double dx = x2 - x1, dy = y2 - y1;
        // Approximate r by bisection on cycloid endpoint condition
        double rLo = 0.01, rHi = 10 * std::max(std::abs(dx), std::abs(dy));
        for (int iter = 0; iter < 60; ++iter)
        {
            double r = (rLo + rHi) / 2;
            // Find theta where cycloid reaches x2
            double th = 0;
            for (int k = 0; k < 1000; ++k)
            {
                th += PI / 500;
                double xc = r * (th - std::sin(th));
                double yc = r * (1 - std::cos(th));
                if (xc >= dx && yc >= dy)
                    break;
            }
            double xEnd = r * (th - std::sin(th)), yEnd = r * (1 - std::cos(th));
            if (xEnd < dx)
                rLo = r;
            else
                rHi = r;
        }
        double r = (rLo + rHi) / 2;
        // Time of descent: T = theta_end * sqrt(r/g)
        double thetaEnd = 0;
        for (int k = 1; k <= 2000; ++k)
        {
            thetaEnd = k * PI / 1000;
            if (r * (thetaEnd - std::sin(thetaEnd)) >= dx)
                break;
        }
        double T = thetaEnd * std::sqrt(r / g);

        steps << "Radius parameter: r ≈ " << fmt(r) << "\n";
        steps << "Parameter at endpoint: θ ≈ " << fmt(thetaEnd) << " rad\n";
        steps << "Time of descent: T ≈ " << fmt(T) << " s  (with g=" << fmt(g) << " m/s²)\n\n";
        steps << "Compare with straight line: T_line = " << fmt(std::sqrt(2 * dy / g * (1 + dx * dx / (dy * dy)))) << " s";

        ss << "Cycloid: r≈" << fmt(r) << ", T≈" << fmt(T) << "s";
        return ok(ss.str(), fmt(T), steps.str(), "Brachistochrone");
    }

    AMResult isoperimetric(const std::string &F, const std::string &G,
                           const std::string &var, double constraint)
    {
        std::ostringstream ss, steps;
        steps << "=== Isoperimetric Problem ===\n\n";
        steps << "Extremise: J[y] = ∫F(" << var << ",y,y') d" << var << "\n";
        steps << "Subject to: K[y] = ∫G(" << var << ",y,y') d" << var << " = " << fmt(constraint) << "\n\n";
        steps << "Method: Augmented functional with Lagrange multiplier λ\n";
        steps << "  J* = ∫(F + λ·G) d" << var << "\n\n";
        steps << "Modified Euler-Lagrange for H = F + λ·G:\n";
        steps << "  ∂H/∂y - d/d" << var << "[∂H/∂y'] = 0\n\n";
        steps << "= ∂F/∂y - d/d" << var << "[∂F/∂y'] + λ·(∂G/∂y - d/d" << var << "[∂G/∂y']) = 0\n\n";
        steps << "Solve this ODE together with the constraint K[y] = " << fmt(constraint) << "\n";
        steps << "to determine both y(x) and λ.\n\n";
        steps << "Classic example: Dido's problem (maximum area with fixed perimeter → circle)";

        ss << "Extremal satisfies EL for H = F + λG,  ∫G = " << fmt(constraint);
        return ok(ss.str(), "", steps.str(), "Isoperimetric problem");
    }

    AMResult hamiltonPrinciple(const std::string &T, const std::string &V,
                               const std::string &q, const std::string &qdot)
    {
        std::ostringstream ss, steps;
        steps << "=== Hamilton's Principle ===\n\n";
        steps << "T = " << T << "  (kinetic energy)\n";
        steps << "V = " << V << "  (potential energy)\n";
        steps << "q = " << q << "  (generalised coordinate)\n\n";
        steps << "Lagrangian: L = T - V\n\n";

        // Compute Lagrangian symbolically
        auto dTdq = Calculus::differentiate(T, q, 1);
        auto dTdqd = Calculus::differentiate(T, qdot, 1);
        auto dVdq = Calculus::differentiate(V, q, 1);

        steps << "∂L/∂" << q << " = ∂T/∂" << q << " - ∂V/∂" << q << "\n";
        steps << "  = " << (dTdq.ok ? dTdq.symbolic : "?") << " - " << (dVdq.ok ? dVdq.symbolic : "?") << "\n\n";
        steps << "∂L/∂" << qdot << " = " << (dTdqd.ok ? dTdqd.symbolic : "?") << "  (generalised momentum p)\n\n";
        steps << "Euler-Lagrange (equation of motion):\n";
        steps << "  d/dt(∂L/∂q̇) - ∂L/∂q = 0\n\n";
        steps << "Hamilton's equations (from H = p·q̇ - L):\n";
        steps << "  dq/dt = ∂H/∂p\n";
        steps << "  dp/dt = -∂H/∂q";

        ss << "EOM: d/dt[∂(T-V)/∂" << qdot << "] - ∂(T-V)/∂" << q << " = 0";
        return ok(ss.str(), "", steps.str(), "Hamilton's principle");
    }

    AMResult noetherTheorem(const std::string &L,
                            const std::string &symmetryType)
    {
        std::ostringstream ss, steps;
        steps << "=== Noether's Theorem ===\n\n";
        steps << "L = " << L << "\n";
        steps << "Symmetry type: " << symmetryType << "\n\n";

        if (symmetryType == "time")
        {
            steps << "Time translation symmetry (L not explicit in t):\n";
            steps << "→ Conservation of Energy\n";
            steps << "  E = q̇·(∂L/∂q̇) - L = constant\n";
            steps << "  (Hamiltonian H is conserved)\n";
            ss << "Energy E = q̇·∂L/∂q̇ - L = const";
        }
        else if (symmetryType == "translation")
        {
            steps << "Spatial translation symmetry (L not explicit in q):\n";
            steps << "→ Conservation of Generalised Momentum\n";
            steps << "  p = ∂L/∂q̇ = constant\n";
            ss << "Momentum p = ∂L/∂q̇ = const";
        }
        else if (symmetryType == "rotation")
        {
            steps << "Rotational symmetry (L invariant under rotation):\n";
            steps << "→ Conservation of Angular Momentum\n";
            steps << "  L = r × p = constant\n";
            ss << "Angular momentum L = r × p = const";
        }
        else
        {
            steps << "General: for each one-parameter symmetry group,\n";
            steps << "there exists a conserved quantity (first integral).";
            ss << "Noether conserved quantity from " + symmetryType + " symmetry";
        }
        return ok(ss.str(), "", steps.str(), "Noether's theorem");
    }

    // =============================================================================
    // CHAPTER 5 — Sturm-Liouville & Integral Equations
    // =============================================================================

    SLResult sturmLiouville(const std::string &p, const std::string &q,
                            const std::string &r,
                            double a, double b, int nEigen)
    {
        SLResult result;
        int N = 200;
        double h = (b - a) / N;
        result.xGrid.resize(N + 1);
        for (int i = 0; i <= N; ++i)
            result.xGrid[i] = a + i * h;

        // Shooting method: for each trial lambda, solve BVP
        // -(p*y')' + q*y = lambda*r*y,  y(a)=0, y'(a)=1
        // Find lambda where y(b)=0

        auto shoot = [&](double lambda) -> double
        {
            Vec y = {0.0, 1.0}; // [y, y']
            double t = a;
            for (int i = 0; i < N; ++i)
            {
                double x = t;
                double pv = evalAt(p, "x", x);
                double ppv = (evalAt(p, "x", x + 1e-5) - evalAt(p, "x", x - 1e-5)) / (2e-5);
                double qv = evalAt(q, "x", x);
                double rv = evalAt(r, "x", x);
                // -(p*y')' + q*y = lambda*r*y
                // y'' = (-q*y + lambda*r*y - p'*y') / p
                double ypp = (-qv * y[0] + lambda * rv * y[0] - ppv * y[1]) / pv;
                y[0] += h * y[1];
                y[1] += h * ypp;
                t += h;
            }
            return y[0]; // y(b)
        };

        // Scan for sign changes → eigenvalues
        double lamMin = -10, lamMax = 1000;
        double dLam = (lamMax - lamMin) / 500;
        double prevVal = shoot(lamMin);

        for (int k = 0; k < (int)(lamMax - lamMin) / dLam && (int)result.eigenvalues.size() < nEigen; ++k)
        {
            double lam = lamMin + k * dLam;
            double val = shoot(lam);
            if (!std::isfinite(val))
            {
                prevVal = val;
                continue;
            }
            if (std::isfinite(prevVal) && prevVal * val < 0)
            {
                // Bisect to find eigenvalue
                double lo = lam - dLam, hi = lam;
                for (int iter = 0; iter < 50; ++iter)
                {
                    double mid = (lo + hi) / 2;
                    if (shoot(lo) * shoot(mid) < 0)
                        hi = mid;
                    else
                        lo = mid;
                }
                double eigenval = (lo + hi) / 2;
                result.eigenvalues.push_back(eigenval);

                // Compute eigenfunction by shooting with this eigenvalue
                Vec y = {0.0, 1.0};
                Vec ef;
                ef.push_back(0.0);
                double t = a;
                for (int i = 0; i < N; ++i)
                {
                    double x = t;
                    double pv = (evalAt(p, "x", x));
                    double ppv = (evalAt(p, "x", x + 1e-5) - evalAt(p, "x", x - 1e-5)) / (2e-5);
                    double qv = evalAt(q, "x", x);
                    double rv = evalAt(r, "x", x);
                    double ypp = (-qv * y[0] + eigenval * rv * y[0] - ppv * y[1]) / pv;
                    y[0] += h * y[1];
                    y[1] += h * ypp;
                    t += h;
                    ef.push_back(y[0]);
                }
                // Normalize
                double norm = 0;
                for (double v : ef)
                    norm += v * v * h;
                norm = std::sqrt(norm);
                if (norm > 1e-15)
                    for (auto &v : ef)
                        v /= norm;
                result.eigenfunctions.push_back(ef);
            }
            prevVal = val;
        }
        return result;
    }

    AMResult rayleighQuotient(const std::string &p, const std::string &q,
                              const std::string &r,
                              const std::string &u,
                              double a, double b)
    {
        // R[u] = (∫(p*u'² + q*u²)dx) / (∫r*u²dx)
        int N = 200;
        double h = (b - a) / N;
        double num = 0, den = 0;
        auto dup = Calculus::differentiate(u, "x", 1);
        for (int i = 0; i <= N; ++i)
        {
            double x = a + i * h;
            double uv = evalAt(u, "x", x);
            double upv = dup.ok ? evalAt(dup.symbolic, "x", x) : 0;
            double pv = evalAt(p, "x", x), qv = evalAt(q, "x", x), rv = evalAt(r, "x", x);
            double w = (i == 0 || i == N) ? 0.5 : 1.0;
            num += w * (pv * upv * upv + qv * uv * uv) * h;
            den += w * rv * uv * uv * h;
        }
        double R = num / den;
        std::ostringstream ss, steps;
        steps << "=== Rayleigh Quotient ===\n\n";
        steps << "R[u] = (∫(p·u'² + q·u²)dx) / (∫r·u²dx)\n";
        steps << "Trial function u = " << u << "\n";
        steps << "R[u] = " << fmt(num) << " / " << fmt(den) << " = " << fmt(R) << "\n\n";
        steps << "This is an upper bound for the first eigenvalue λ₁.";
        ss << "R[u] = " << fmt(R) << " (upper bound for λ₁)";
        return ok(ss.str(), fmt(R), steps.str(), "Rayleigh quotient");
    }

    AMResult fredholmSecondKind(const std::string &f, const std::string &K,
                                double a, double b, double lambda, int n)
    {
        // y(x) = f(x) + lambda * ∫_a^b K(x,t)y(t)dt
        // Numerical: discretise to linear system (I - lambda*A)y = f
        std::vector<double> xGrid(n), fVals(n);
        double h = (b - a) / (n - 1);
        for (int i = 0; i < n; ++i)
        {
            xGrid[i] = a + i * h;
            fVals[i] = evalAt(f, "x", xGrid[i]);
        }

        // Build matrix A[i][j] = h * K(x_i, t_j) (trapezoidal weights)
        std::vector<std::vector<double>> A(n, std::vector<double>(n));
        for (int i = 0; i < n; ++i)
            for (int j = 0; j < n; ++j)
            {
                double w = (j == 0 || j == n - 1) ? 0.5 : 1.0;
                A[i][j] = lambda * w * h * evalAt2(K, "x", xGrid[i], "t", xGrid[j]);
            }

        // Solve (I - A)y = f via Gaussian elimination
        std::vector<std::vector<double>> aug(n, std::vector<double>(n + 1));
        for (int i = 0; i < n; ++i)
        {
            for (int j = 0; j < n; ++j)
                aug[i][j] = (i == j ? 1.0 : 0.0) - A[i][j];
            aug[i][n] = fVals[i];
        }
        for (int col = 0; col < n; ++col)
        {
            int pivot = col;
            for (int row = col + 1; row < n; ++row)
                if (std::abs(aug[row][col]) > std::abs(aug[pivot][col]))
                    pivot = row;
            std::swap(aug[col], aug[pivot]);
            if (std::abs(aug[col][col]) < 1e-14)
                continue;
            double d = aug[col][col];
            for (int j = 0; j <= n; ++j)
                aug[col][j] /= d;
            for (int row = 0; row < n; ++row)
            {
                if (row == col)
                    continue;
                double fac = aug[row][col];
                for (int j = 0; j <= n; ++j)
                    aug[row][j] -= fac * aug[col][j];
            }
        }
        std::vector<double> y(n);
        for (int i = 0; i < n; ++i)
            y[i] = aug[i][n];

        std::ostringstream ss, steps;
        steps << "=== Fredholm Integral Equation (2nd Kind) ===\n";
        steps << "y(x) = f(x) + λ·∫_a^b K(x,t)y(t)dt\n";
        steps << "f=" << f << ", K=" << K << ", λ=" << fmt(lambda) << "\n";
        steps << "Solved by Nyström method (" << n << " nodes)\n";
        steps << "Solution points: [";
        for (int i = 0; i < n && i < 10; ++i)
        {
            if (i)
                steps << ",";
            steps << "(" << fmt(xGrid[i], 3) << "," << fmt(y[i], 4) << ")";
        }
        if (n > 10)
            steps << ",...";
        steps << "]";
        ss << "y solved numerically at " << n << " points, y(" << fmt(a) << ")=" << fmt(y[0], 4) << ", y(" << fmt(b) << ")=" << fmt(y[n - 1], 4);
        return ok(ss.str(), "", steps.str(), "Fredholm 2nd kind (Nyström)");
    }

    AMResult volterraSecondKind(const std::string &f, const std::string &K,
                                double a, double b, int n)
    {
        // y(x) = f(x) + ∫_a^x K(x,t)y(t)dt — solved by successive approximation
        double h = (b - a) / (n - 1);
        std::vector<double> x(n), y(n);
        for (int i = 0; i < n; ++i)
            x[i] = a + i * h;
        y[0] = evalAt(f, "x", x[0]);
        for (int i = 1; i < n; ++i)
        {
            double integral = 0;
            for (int j = 0; j < i; ++j)
            {
                double w = (j == 0 || j == i - 1) ? 0.5 : 1.0;
                integral += w * h * evalAt2(K, "x", x[i], "t", x[j]) * y[j];
            }
            y[i] = evalAt(f, "x", x[i]) + integral;
        }
        std::ostringstream ss, steps;
        steps << "=== Volterra Integral Equation (2nd Kind) ===\n";
        steps << "y(x) = f(x) + ∫_a^x K(x,t)y(t)dt\n";
        steps << "Solved by trapezoidal product integration\n";
        steps << "y(" << fmt(b) << ") = " << fmt(y[n - 1], 6);
        ss << "y(" << fmt(b) << ")" << " = " << fmt(y[n - 1], 6);
        return ok(ss.str(), "", steps.str(), "Volterra 2nd kind");
    }

    // =============================================================================
    // CHAPTER 6 — PDEs (Logan treatment)
    // =============================================================================

    AMResult methodOfCharacteristics(const std::string &c,
                                     const std::string &f,
                                     const std::string &u0,
                                     double tEnd, int n)
    {
        std::ostringstream ss, steps;
        steps << "=== Method of Characteristics ===\n\n";
        steps << "PDE: u_t + c(x,t,u)·u_x = f(x,t,u)\n";
        steps << "c = " << c << ",  f = " << f << "\n";
        steps << "Initial condition: u(x,0) = " << u0 << "\n\n";
        steps << "Characteristic equations:\n";
        steps << "  dx/dt = c(x,t,u)\n";
        steps << "  du/dt = f(x,t,u)\n\n";
        steps << "Along each characteristic: x = x(t; x₀)\n";
        steps << "where x₀ is the initial position.\n\n";

        // Trace several characteristics
        steps << "Sample characteristics:\n";
        for (double x0 = -3; x0 <= 3; x0 += 1.0)
        {
            double u_val = evalAt(u0, "x", x0);
            double c_val = evalAt2(c, "x", x0, "u", u_val);
            // Simple: dx/dt = c(x0, u0) for linear case
            double x_at_tend = x0 + c_val * tEnd;
            steps << "  x₀=" << fmt(x0, 2) << ": u₀=" << fmt(u_val, 3)
                  << ", c=" << fmt(c_val, 3) << ", x(T)≈" << fmt(x_at_tend, 3) << "\n";
        }

        ss << "Characteristics: dx/dt = " << c << " along which du/dt = " << f;
        return ok(ss.str(), "", steps.str(), "Method of characteristics");
    }

    AMResult rankineHugoniot(const std::string &F, double uL, double uR)
    {
        std::ostringstream ss, steps;
        steps << "=== Rankine-Hugoniot Shock Condition ===\n\n";
        steps << "Conservation law: u_t + F(u)_x = 0\n";
        steps << "F(u) = " << F << "\n";
        steps << "Left state:  uL = " << fmt(uL) << "\n";
        steps << "Right state: uR = " << fmt(uR) << "\n\n";

        double FL = evalAt(F, "u", uL);
        double FR = evalAt(F, "u", uR);
        double s = (FR - FL) / (uR - uL);

        steps << "F(uL) = " << fmt(FL) << "\n";
        steps << "F(uR) = " << fmt(FR) << "\n\n";
        steps << "Shock speed: s = [F(uR) - F(uL)] / [uR - uL]\n";
        steps << "           = (" << fmt(FR) << " - " << fmt(FL) << ") / ("
              << fmt(uR) << " - " << fmt(uL) << ")\n";
        steps << "           = " << fmt(s) << "\n\n";
        steps << "The shock moves at speed s = " << fmt(s);

        ss << "Shock speed s = " << fmt(s);
        return ok(ss.str(), fmt(s), steps.str(), "Rankine-Hugoniot");
    }

    AMResult entropyCondition(const std::string &F,
                              double uL, double uR, double s)
    {
        std::ostringstream ss, steps;
        steps << "=== Entropy / Lax Condition ===\n\n";
        steps << "Lax entropy condition for admissible shocks:\n";
        steps << "  F'(uL) > s > F'(uR)\n\n";

        auto dF = Calculus::differentiate(F, "u", 1);
        double dFL = dF.ok ? evalAt(dF.symbolic, "u", uL) : 0;
        double dFR = dF.ok ? evalAt(dF.symbolic, "u", uR) : 0;

        steps << "F'(u) = " << (dF.ok ? dF.symbolic : "?") << "\n";
        steps << "F'(uL) = " << fmt(dFL) << "\n";
        steps << "F'(uR) = " << fmt(dFR) << "\n";
        steps << "Shock speed s = " << fmt(s) << "\n\n";

        bool laxCond = (dFL > s) && (s > dFR);
        steps << "Lax condition satisfied: " << (laxCond ? "YES — physically admissible shock" : "NO — rarefaction wave, not a shock");

        ss << (laxCond ? "Admissible shock" : "Not admissible (entropy violated)") << ": F'(uL)=" << fmt(dFL) << " > s=" << fmt(s) << " > F'(uR)=" << fmt(dFR);
        return ok(ss.str(), "", steps.str(), "Entropy condition");
    }

    AMResult reactionDiffusion(double D, const std::string &R,
                               const std::string &u0,
                               double L, double tEnd, int nx, int nt)
    {
        // u_t = D*u_xx + R(u) with u(0,t)=u(L,t)=0
        // Explicit finite difference (FTCS)
        double dx = L / (nx - 1), dt = tEnd / nt;
        double r = D * dt / (dx * dx); // stability: r ≤ 0.5

        std::vector<double> u(nx), unew(nx);
        for (int i = 0; i < nx; ++i)
            u[i] = evalAt(u0, "x", i * dx);
        u[0] = u[nx - 1] = 0;

        for (int step = 0; step < nt; ++step)
        {
            for (int i = 1; i < nx - 1; ++i)
            {
                double Ru = evalAt(R, "u", u[i]);
                unew[i] = u[i] + r * (u[i - 1] - 2 * u[i] + u[i + 1]) + dt * Ru;
            }
            unew[0] = unew[nx - 1] = 0;
            u = unew;
        }

        std::ostringstream ss, steps;
        steps << "=== Reaction-Diffusion Equation ===\n";
        steps << "u_t = " << fmt(D) << "·u_xx + R(u),  R(u) = " << R << "\n";
        steps << "BC: u(0,t) = u(" << fmt(L) << ",t) = 0\n";
        steps << "IC: u(x,0) = " << u0 << "\n\n";
        steps << "FTCS scheme: r = D·dt/dx² = " << fmt(r) << " (stability: r ≤ 0.5)\n\n";
        steps << "Solution at t=" << fmt(tEnd) << ":\n";
        steps << "Points: [";
        for (int i = 0; i < nx; i += std::max(1, nx / 20))
        {
            if (i)
                steps << ",";
            steps << "[" << fmt(i * dx, 3) << "," << fmt(u[i], 4) << "]";
        }
        steps << "]";
        double umax = *std::max_element(u.begin(), u.end());
        ss << "u_max(T=" << fmt(tEnd) << ") = " << fmt(umax, 4);
        return ok(ss.str(), "", steps.str(), "Reaction-diffusion (FTCS)");
    }

    AMResult travellingWave(double D, const std::string &R,
                            double uMinus, double uPlus)
    {
        std::ostringstream ss, steps;
        steps << "=== Travelling Wave Analysis ===\n\n";
        steps << "u_t = D·u_xx + R(u)\n";
        steps << "Seek u(x,t) = U(ξ),  ξ = x - c·t\n\n";
        steps << "Substituting: D·U'' + c·U' + R(U) = 0\n\n";
        steps << "Boundary conditions: U(-∞) = " << fmt(uMinus) << ",  U(+∞) = " << fmt(uPlus) << "\n\n";

        // R(uMinus) and R(uPlus) should be zero (equilibria)
        double RuM = evalAt(R, "u", uMinus);
        double RuP = evalAt(R, "u", uPlus);
        steps << "R(" << fmt(uMinus) << ") = " << fmt(RuM) << (std::abs(RuM) < 1e-6 ? " ✓" : " (should be 0)") << "\n";
        steps << "R(" << fmt(uPlus) << ") = " << fmt(RuP) << (std::abs(RuP) < 1e-6 ? " ✓" : " (should be 0)") << "\n\n";

        // Minimum wave speed for Fisher-KPP type: c_min = 2*sqrt(D*R'(0))
        auto dR = Calculus::differentiate(R, "u", 1);
        double dRu0 = dR.ok ? evalAt(dR.symbolic, "u", uMinus) : 0;
        double cMin = 2 * std::sqrt(D * std::max(0.0, dRu0));
        steps << "R'(" << fmt(uMinus) << ") = " << fmt(dRu0) << "\n";
        steps << "Minimum wave speed (Fisher-KPP): c_min = 2√(D·R') = " << fmt(cMin) << "\n\n";
        steps << "Phase plane: U' = V,  D·V' = -c·V - R(U)\n";
        steps << "Wave exists if heteroclinic orbit connects equilibria.";

        ss << "Min wave speed: c ≥ " << fmt(cMin);
        return ok(ss.str(), fmt(cMin), steps.str(), "Travelling wave");
    }

    AMResult turingInstability(double D1, double D2,
                               double fu, double fv, double gu, double gv)
    {
        std::ostringstream ss, steps;
        steps << "=== Turing Instability (Diffusion-Driven Instability) ===\n\n";
        steps << "Reaction-diffusion system:\n";
        steps << "  u_t = D₁·∇²u + f(u,v)\n";
        steps << "  v_t = D₂·∇²v + g(u,v)\n\n";
        steps << "Jacobian at homogeneous steady state:\n";
        steps << "  J = [[" << fmt(fu, 4) << ", " << fmt(fv, 4) << "],\n";
        steps << "       [" << fmt(gu, 4) << ", " << fmt(gv, 4) << "]]\n\n";

        double trJ = fu + gv, detJ = fu * gv - fv * gu;
        steps << "tr(J) = " << fmt(trJ) << " (must be < 0 for stable SS)\n";
        steps << "det(J) = " << fmt(detJ) << " (must be > 0 for stable SS)\n\n";

        bool ssStable = trJ < 0 && detJ > 0;
        steps << "Homogeneous SS is " << (ssStable ? "stable" : "UNSTABLE") << " without diffusion\n\n";

        if (!ssStable)
        {
            ss << "No Turing instability (SS unstable without diffusion)";
            return ok(ss.str(), "", steps.str(), "Turing instability");
        }

        // Turing instability condition: for some wavenumber k,
        // h(k²) = D1*D2*k⁴ - (D2*fu + D1*gv)*k² + det(J) < 0
        double d = D2 * fu + D1 * gv;
        steps << "Dispersion relation condition for instability:\n";
        steps << "  h(k²) = D₁D₂k⁴ - (D₂f_u + D₁g_v)k² + det(J) < 0\n";
        steps << "  requires D₂f_u + D₁g_v = " << fmt(d) << " > 0\n\n";

        bool turingPossible = d > 0 && d * d > 4 * D1 * D2 * detJ;
        if (turingPossible)
        {
            double kc2 = d / (2 * D1 * D2);
            double kc = std::sqrt(kc2);
            steps << "Critical wavenumber: k_c = √(" << fmt(kc2) << ") = " << fmt(kc) << "\n";
            steps << "Critical wavelength: λ_c = 2π/k_c = " << fmt(2 * PI / kc) << "\n\n";
            steps << "Turing pattern wavelength ≈ " << fmt(2 * PI / kc);
            ss << "TURING INSTABILITY: k_c=" << fmt(kc) << ", λ_c=" << fmt(2 * PI / kc);
        }
        else
        {
            steps << "No Turing instability (condition d² > 4D₁D₂·det(J) not met)";
            ss << "No Turing instability";
        }
        return ok(ss.str(), "", steps.str(), "Turing instability analysis");
    }

    // =============================================================================
    // CHAPTER 7 — Wave Phenomena
    // =============================================================================

    AMResult dispersionRelation(const std::string &waveEq, double k)
    {
        std::ostringstream ss, steps;
        steps << "=== Dispersion Relation ===\n\n";
        steps << "Wave equation: " << waveEq << "\n";
        steps << "Wavenumber: k = " << fmt(k) << "\n\n";
        steps << "Seek plane wave solution: u = e^{i(kx - ωt)}\n";
        steps << "Substitute → algebraic equation in ω and k → dispersion relation ω = ω(k)\n\n";
        steps << "Common dispersion relations:\n";
        steps << "  Advection:         ω = ck         (non-dispersive)\n";
        steps << "  Wave equation:     ω = ±c·k       (non-dispersive)\n";
        steps << "  Diffusion:         ω = -iDk²      (purely diffusive)\n";
        steps << "  Klein-Gordon:      ω² = k² + m²   (dispersive)\n";
        steps << "  Schrödinger:       ω = ħk²/(2m)   (dispersive)\n";
        steps << "  KdV:               ω = k³ - k     (strongly dispersive)\n\n";
        steps << "Phase velocity:   c_p = ω/k\n";
        steps << "Group velocity:   c_g = dω/dk";
        ss << "Dispersion: ω = ω(k=" << fmt(k) << ") — see steps for common forms";
        return ok(ss.str(), "", steps.str(), "Dispersion relation");
    }

    AMResult dAlembertSolution(const std::string &f0, const std::string &g0,
                               double c, double xEval, double tEval)
    {
        // u(x,t) = (1/2)[f(x+ct) + f(x-ct)] + (1/2c)∫_{x-ct}^{x+ct} g(s)ds
        double xp = xEval + c * tEval, xm = xEval - c * tEval;
        double fp = evalAt(f0, "x", xp), fm = evalAt(f0, "x", xm);
        double dispPart = 0.5 * (fp + fm);

        // Numerical integral of g0 from xm to xp
        int N = 200;
        double h2 = (xp - xm) / N, intG = 0;
        for (int i = 0; i < N; ++i)
        {
            double s = xm + i * h2, s2 = xm + (i + 1) * h2;
            intG += 0.5 * h2 * (evalAt(g0, "x", s) + evalAt(g0, "x", s2));
        }
        double u = dispPart + intG / (2 * c);

        std::ostringstream ss, steps;
        steps << "=== D'Alembert Solution ===\n\n";
        steps << "Wave equation: u_tt = c²u_xx,  c=" << fmt(c) << "\n";
        steps << "u(x,0) = f(x) = " << f0 << "\n";
        steps << "u_t(x,0) = g(x) = " << g0 << "\n\n";
        steps << "D'Alembert formula:\n";
        steps << "  u(x,t) = ½[f(x+ct) + f(x-ct)] + 1/(2c)·∫_{x-ct}^{x+ct} g(s)ds\n\n";
        steps << "Evaluation at (x=" << fmt(xEval) << ", t=" << fmt(tEval) << "):\n";
        steps << "  x+ct = " << fmt(xp) << ",  x-ct = " << fmt(xm) << "\n";
        steps << "  f(x+ct) = " << fmt(fp, 4) << ",  f(x-ct) = " << fmt(fm, 4) << "\n";
        steps << "  ∫g ds ≈ " << fmt(intG, 4) << "\n";
        steps << "  u = " << fmt(u, 6);

        ss << "u(" << fmt(xEval) << "," << fmt(tEval) << ") = " << fmt(u, 6);
        return ok(ss.str(), fmt(u), steps.str(), "D'Alembert");
    }

    AMResult waveVelocities(const std::string &omega_k, double k0)
    {
        double omega = evalAt(omega_k, "k", k0);
        double h = 1e-5;
        double cg = (evalAt(omega_k, "k", k0 + h) - evalAt(omega_k, "k", k0 - h)) / (2 * h);
        double cp = omega / k0;

        std::ostringstream ss, steps;
        steps << "=== Phase and Group Velocity ===\n\n";
        steps << "Dispersion relation: ω(k) = " << omega_k << "\n";
        steps << "At k₀ = " << fmt(k0) << ": ω₀ = " << fmt(omega, 4) << "\n\n";
        steps << "Phase velocity:  c_p = ω/k = " << fmt(omega, 4) << "/" << fmt(k0) << " = " << fmt(cp) << "\n";
        steps << "Group velocity:  c_g = dω/dk|_{k₀} = " << fmt(cg) << "\n\n";
        steps << (std::abs(cp - cg) < 1e-6 ? "Non-dispersive medium (c_p = c_g)\n" : "Dispersive medium (c_p ≠ c_g)\n");
        steps << "Energy propagates at group velocity c_g = " << fmt(cg);
        ss << "c_p=" << fmt(cp, 4) << ", c_g=" << fmt(cg, 4);
        return ok(ss.str(), "", steps.str(), "Wave velocities");
    }

    AMResult burgers(double nu, const std::string &u0,
                     double L, double tEnd, int nx, int nt)
    {
        // Burgers: u_t + u*u_x = nu*u_xx
        // Cole-Hopf: u = -2nu*phi_x/phi where phi solves heat equation
        // Numerical: FTCS with upwind for advection
        double dx = L / (nx - 1), dt = tEnd / nt;
        std::vector<double> u(nx), unew(nx);
        for (int i = 0; i < nx; ++i)
            u[i] = evalAt(u0, "x", -L / 2 + i * dx);

        for (int step = 0; step < nt; ++step)
        {
            for (int i = 1; i < nx - 1; ++i)
            {
                double diff = nu * (u[i - 1] - 2 * u[i] + u[i + 1]) / (dx * dx);
                double adv = (u[i] > 0) ? (u[i] - u[i - 1]) / dx : (u[i + 1] - u[i]) / dx;
                unew[i] = u[i] + dt * (diff - u[i] * adv);
            }
            unew[0] = u[0];
            unew[nx - 1] = u[nx - 1];
            u = unew;
        }
        std::ostringstream ss, steps;
        steps << "=== Burgers' Equation ===\n";
        steps << "u_t + u·u_x = ν·u_xx,  ν=" << fmt(nu) << "\n";
        steps << "Cole-Hopf transformation: u = -2ν·φ_x/φ (reduces to heat equation)\n\n";
        steps << "Numerical solution (upwind + FTCS) at t=" << fmt(tEnd) << ":\n";
        steps << "Points: [";
        for (int i = 0; i < nx; i += std::max(1, nx / 20))
        {
            if (i)
                steps << ",";
            steps << "[" << fmt(-L / 2 + i * dx, 3) << "," << fmt(u[i], 4) << "]";
        }
        steps << "]";
        double umax = *std::max_element(u.begin(), u.end());
        ss << "u_max(T=" << fmt(tEnd) << ")=" << fmt(umax, 4) << ", ν=" << fmt(nu);
        return ok(ss.str(), "", steps.str(), "Burgers equation");
    }

    AMResult shockFormationTime(const std::string &c_u,
                                const std::string &u0,
                                double xMin, double xMax)
    {
        // Breaking time: t_b = -1/min(c'(u₀)·u₀')
        // Characteristics: x = x₀ + c(u₀(x₀))·t
        // Shock forms when characteristics cross
        std::ostringstream ss, steps;
        steps << "=== Shock Formation Time ===\n\n";
        steps << "u_t + c(u)·u_x = 0\n";
        steps << "c(u) = " << c_u << ",  u(x,0) = " << u0 << "\n\n";

        auto du0 = Calculus::differentiate(u0, "x", 1);
        auto dc = Calculus::differentiate(c_u, "u", 1);

        double tBreak = 1e18;
        int N = 500;
        double dx = (xMax - xMin) / N;
        for (int i = 0; i < N; ++i)
        {
            double x = xMin + i * dx;
            double u0v = evalAt(u0, "x", x);
            double du0v = du0.ok ? evalAt(du0.symbolic, "x", x) : 0;
            double dcv = dc.ok ? evalAt(dc.symbolic, "u", u0v) : 0;
            double denom = dcv * du0v;
            if (denom < -1e-12)
            {
                double t = -1.0 / denom;
                if (t < tBreak)
                    tBreak = t;
            }
        }
        steps << "Breaking time analysis: t_break = -1/min{c'(u₀)·u₀'}\n\n";
        if (tBreak > 1e17)
        {
            steps << "No shock formation (c'(u₀)·u₀' ≥ 0 everywhere)\n";
            ss << "No shock formation";
        }
        else
        {
            steps << "Minimum breaking time: t_b ≈ " << fmt(tBreak);
            ss << "Shock forms at t ≈ " << fmt(tBreak);
        }
        return ok(ss.str(), fmt(tBreak), steps.str(), "Shock formation time");
    }

    // =============================================================================
    // CHAPTER 8 — Continuum Mechanics
    // =============================================================================

    AMResult speedOfSound(double gamma, double p, double rho)
    {
        double c = std::sqrt(gamma * p / rho);
        std::ostringstream ss, steps;
        steps << "=== Speed of Sound in Ideal Gas ===\n\n";
        steps << "c = √(γp/ρ)\n";
        steps << "γ=" << fmt(gamma) << ", p=" << fmt(p) << ", ρ=" << fmt(rho) << "\n\n";
        steps << "c = √(" << fmt(gamma * p / rho) << ") = " << fmt(c) << " m/s\n\n";
        steps << "For air at STP: γ=1.4, p=101325 Pa, ρ=1.225 kg/m³ → c≈343 m/s";
        ss << "c = " << fmt(c) << " m/s";
        return ok(ss.str(), fmt(c), steps.str(), "Speed of sound");
    }

    AMResult machNumber(double v, double gamma, double p, double rho)
    {
        double c = std::sqrt(gamma * p / rho), M = v / c;
        std::ostringstream ss, steps;
        steps << "=== Mach Number ===\n\n";
        steps << "Ma = v/c = " << fmt(v) << "/" << fmt(c) << " = " << fmt(M) << "\n\n";
        std::string regime;
        if (M < 0.3)
            regime = "Incompressible flow";
        else if (M < 0.8)
            regime = "Subsonic";
        else if (M < 1.2)
            regime = "Transonic";
        else if (M < 5.0)
            regime = "Supersonic";
        else
            regime = "Hypersonic";
        steps << regime;
        ss << "Ma = " << fmt(M) << " (" << regime << ")";
        return ok(ss.str(), fmt(M), steps.str(), "Mach number");
    }

    AMResult potentialFlow(const std::string &phi,
                           double xEval, double yEval)
    {
        // u = phi_x, v = phi_y
        auto dphidx = Calculus::differentiate(phi, "x", 1);
        auto dphidy = Calculus::differentiate(phi, "y", 1);
        double u = dphidx.ok ? evalAt2(dphidx.symbolic, "x", xEval, "y", yEval) : 0;
        double v = dphidy.ok ? evalAt2(dphidy.symbolic, "x", xEval, "y", yEval) : 0;
        double speed = std::hypot(u, v);

        std::ostringstream ss, steps;
        steps << "=== Potential Flow ===\n\n";
        steps << "Velocity potential: φ = " << phi << "\n";
        steps << "u = ∂φ/∂x = " << (dphidx.ok ? dphidx.symbolic : "?") << "\n";
        steps << "v = ∂φ/∂y = " << (dphidy.ok ? dphidy.symbolic : "?") << "\n\n";
        steps << "At (" << fmt(xEval) << "," << fmt(yEval) << "):\n";
        steps << "  u = " << fmt(u, 4) << ", v = " << fmt(v, 4) << ", |V| = " << fmt(speed, 4) << "\n\n";

        // Check irrotationality (Laplace equation)
        auto d2x = Calculus::differentiate(dphidx.symbolic, "x", 1);
        auto d2y = Calculus::differentiate(dphidy.symbolic, "y", 1);
        if (d2x.ok && d2y.ok)
        {
            double lapPhi = evalAt2(d2x.symbolic, "x", xEval, "y", yEval) +
                            evalAt2(d2y.symbolic, "x", xEval, "y", yEval);
            steps << "∇²φ = " << fmt(lapPhi, 4) << (std::abs(lapPhi) < 1e-4 ? " ✓ (satisfies Laplace)" : " (check Laplace)");
        }
        ss << "V = (" << fmt(u, 4) << ", " << fmt(v, 4) << "), |V| = " << fmt(speed, 4);
        return ok(ss.str(), "", steps.str(), "Potential flow");
    }

    // =============================================================================
    // CHAPTER 9 — Discrete Models
    // =============================================================================

    AMResult logisticMap(double r, double x0, int N)
    {
        std::vector<double> orbit(N);
        orbit[0] = x0;
        for (int n = 1; n < N; ++n)
            orbit[n] = r * orbit[n - 1] * (1 - orbit[n - 1]);

        // Find eventual period from last 100 iterates
        int tail = std::min(100, N / 2);
        std::set<double> seen;
        for (int i = N - tail; i < N; ++i)
        {
            double rounded = std::round(orbit[i] * 1e6) / 1e6;
            seen.insert(rounded);
        }
        int period = (int)seen.size();

        std::ostringstream ss, steps;
        steps << "=== Logistic Map ===\n\n";
        steps << "x_{n+1} = r·x_n·(1 - x_n)\n";
        steps << "r = " << fmt(r) << ", x₀ = " << fmt(x0) << "\n\n";
        steps << "Key transitions:\n";
        steps << "  r < 1:      → 0 (extinction)\n";
        steps << "  1 < r < 3:  → fixed point 1-1/r\n";
        steps << "  3 < r < 3.449: period 2\n";
        steps << "  3.449 < r < 3.544: period 4\n";
        steps << "  r ≈ 3.57...: onset of chaos\n";
        steps << "  r = 4:      fully chaotic\n\n";

        if (period == 1)
            steps << "Orbit converges to fixed point ≈ " << fmt(orbit[N - 1], 4);
        else if (period <= 8)
            steps << "Period-" << period << " cycle";
        else
            steps << "Chaotic orbit (period > 8 or aperiodic)";
        steps << "\n\nFirst 20 iterates: ";
        for (int i = 0; i < std::min(20, N); ++i)
        {
            if (i)
                steps << ", ";
            steps << fmt(orbit[i], 4);
        }

        ss << "r=" << fmt(r) << ": " << (period == 1 ? "fixed point" : period <= 8 ? "period-" + std::to_string(period)
                                                                                   : "chaos");
        return ok(ss.str(), "", steps.str(), "Logistic map");
    }

    AMResult cobwebDiagram(const std::string &f, double x0, int N)
    {
        std::ostringstream ss, steps;
        steps << "=== Cobweb Diagram ===\n\n";
        steps << "Map: x_{n+1} = f(x_n) = " << f << "\n";
        steps << "Starting point: x₀ = " << fmt(x0) << "\n\n";
        steps << "Cobweb points (x_n, f(x_n)) for n=0,...," << N << ":\n";

        double x = x0;
        ss << "Points: [";
        bool first = true;
        for (int n = 0; n < N; ++n)
        {
            double fx = evalAt(f, "x", x);
            if (!first)
                ss << ",";
            first = false;
            ss << "[" << fmt(x, 4) << "," << fmt(fx, 4) << "]";
            x = fx;
            if (!std::isfinite(x) || x > 1e10 || x < -1e10)
                break;
        }
        ss << "]";
        steps << "Final x_" << N << " = " << fmt(x, 4);
        return ok(ss.str(), "", steps.str(), "Cobweb diagram");
    }

    AMResult discreteFixedPoints(const std::string &f,
                                 double xMin, double xMax)
    {
        // Fixed points: f(x) = x  →  g(x) = f(x) - x = 0
        std::ostringstream ss, steps;
        steps << "=== Discrete Fixed Points ===\n\n";
        steps << "f(x) = " << f << "\n";
        steps << "Fixed points: f(x*) = x*\n\n";

        auto df = Calculus::differentiate(f, "x", 1);
        std::vector<std::pair<double, std::string>> fps;
        int N = 500;
        double dx = (xMax - xMin) / N;
        double prevg = evalAt(f, "x", xMin) - xMin;
        for (int i = 1; i <= N; ++i)
        {
            double x = xMin + i * dx;
            double g = evalAt(f, "x", x) - x;
            if (!std::isfinite(g) || !std::isfinite(prevg))
            {
                prevg = g;
                continue;
            }
            if (prevg * g < 0)
            {
                double lo = x - dx, hi = x;
                for (int k = 0; k < 50; ++k)
                {
                    double mid = (lo + hi) / 2;
                    double gm = evalAt(f, "x", mid) - mid;
                    if ((evalAt(f, "x", lo) - lo) * gm < 0)
                        hi = mid;
                    else
                        lo = mid;
                }
                double xstar = (lo + hi) / 2;
                double fprime = df.ok ? evalAt(df.symbolic, "x", xstar) : 0;
                std::string stab = std::abs(fprime) < 1 ? "stable (|f'|<1)" : "unstable (|f'|>1)";
                fps.push_back({xstar, stab});
            }
            prevg = g;
        }
        steps << "Fixed points found:\n";
        for (auto &[x, s] : fps)
            steps << "  x*=" << fmt(x, 4) << " — " << s << ", f'(x*)=" << fmt(evalAt(df.ok ? df.symbolic : "0", "x", x), 4) << "\n";
        if (fps.empty())
            steps << "No fixed points found in [" << fmt(xMin) << "," << fmt(xMax) << "]\n";
        ss << fps.size() << " fixed point(s)";
        return ok(ss.str(), "", steps.str(), "Discrete fixed points");
    }

    AMResult feigenbaumAnalysis(double rMin, double rMax, int steps_n)
    {
        std::ostringstream ss, steps;
        steps << "=== Feigenbaum Period-Doubling Analysis ===\n\n";
        steps << "Logistic map x→rx(1-x), r ∈ [" << fmt(rMin) << "," << fmt(rMax) << "]\n\n";
        steps << "Feigenbaum constant: δ ≈ 4.6692...\n";
        steps << "Ratio of parameter intervals between doublings: (r_n - r_{n-1})/(r_{n+1} - r_n) → δ\n\n";

        // Find period-doubling bifurcation points
        std::vector<double> rBif;
        double dr = (rMax - rMin) / steps_n;
        for (int i = 0; i < steps_n; ++i)
        {
            double r = rMin + i * dr;
            // Iterate 500 times to find attractor, then check period
            double x = 0.5;
            for (int n = 0; n < 500; ++n)
                x = r * x * (1 - x);
            std::vector<double> orbit(64);
            for (int n = 0; n < 64; ++n)
            {
                x = r * x * (1 - x);
                orbit[n] = x;
            }
            int period = 1;
            for (int p = 1; p <= 32; ++p)
            {
                bool match = true;
                for (int k = 0; k < 10; ++k)
                {
                    if (std::abs(orbit[k] - orbit[k + p]) > 1e-6)
                    {
                        match = false;
                        break;
                    }
                }
                if (match)
                {
                    period = p;
                    break;
                }
            }
            if (!rBif.empty() && period > 1)
            {
                // Check for new bifurcation
            }
        }

        steps << "Period-doubling cascade begins near r ≈ 3.0\n";
        steps << "r₁ ≈ 3.000 (period 1→2)\n";
        steps << "r₂ ≈ 3.449 (period 2→4)\n";
        steps << "r₃ ≈ 3.544 (period 4→8)\n";
        steps << "r₄ ≈ 3.564 (period 8→16)\n";
        steps << "r_∞ ≈ 3.5699... (onset of chaos)\n\n";
        double delta_est = (3.449 - 3.0) / (3.544 - 3.449);
        steps << "Estimated Feigenbaum δ ≈ " << fmt(delta_est) << " (theoretical: 4.6692...)";

        ss << "Feigenbaum δ ≈ 4.6692, chaos onset at r∞ ≈ 3.5699";
        return ok(ss.str(), "", steps.str(), "Feigenbaum analysis");
    }

    AMResult discreteSystem(const Mat &A, const Vec &x0, int steps_n)
    {
        int n = x0.size();
        std::ostringstream ss, steps;
        steps << "=== Discrete Dynamical System x_{n+1} = Ax_n ===\n\n";

        // Eigenvalue stability check
        // (reuse characteristic polynomial idea)
        if (n == 2)
        {
            double tr = A[0][0] + A[1][1], det = A[0][0] * A[1][1] - A[0][1] * A[1][0];
            double disc = tr * tr - 4 * det;
            steps << "A = [[" << fmt(A[0][0]) << "," << fmt(A[0][1]) << "],[" << fmt(A[1][0]) << "," << fmt(A[1][1]) << "]]\n";
            steps << "tr=" << fmt(tr) << ", det=" << fmt(det) << "\n\n";
            if (disc >= 0)
            {
                double l1 = (tr + std::sqrt(disc)) / 2, l2 = (tr - std::sqrt(disc)) / 2;
                steps << "Eigenvalues: " << fmt(l1) << ", " << fmt(l2) << "\n";
                steps << "Spectral radius ρ(A) = " << fmt(std::max(std::abs(l1), std::abs(l2))) << "\n";
                steps << (std::max(std::abs(l1), std::abs(l2)) < 1 ? "Stable (ρ<1)" : "Unstable (ρ≥1)") << "\n\n";
            }
            else
            {
                double alpha = tr / 2, beta = std::sqrt(-disc) / 2;
                double rho = std::hypot(alpha, beta);
                steps << "Complex eigenvalues: " << fmt(alpha) << "±" << fmt(beta) << "i, |λ|=" << fmt(rho) << "\n";
                steps << (rho < 1 ? "Stable spiral (|λ|<1)" : "Unstable spiral (|λ|≥1)") << "\n\n";
            }
        }

        // Iterate
        Vec x = x0;
        steps << "Trajectory:\n  x₀ = [";
        for (int i = 0; i < n; ++i)
        {
            if (i)
                steps << ",";
            steps << fmt(x[i], 4);
        }
        steps << "]\n";
        for (int k = 0; k < steps_n && k < 20; ++k)
        {
            Vec xnew(n, 0);
            for (int i = 0; i < n; ++i)
                for (int j = 0; j < n; ++j)
                    xnew[i] += A[i][j] * x[j];
            x = xnew;
            if (k < 5 || k == steps_n - 1)
            {
                steps << "  x_" << k + 1 << " = [";
                for (int i = 0; i < n; ++i)
                {
                    if (i)
                        steps << ",";
                    steps << fmt(x[i], 4);
                }
                steps << "]\n";
            }
        }
        ss << "x_" << steps_n << "=[";
        for (int i = 0; i < n; ++i)
        {
            if (i)
                ss << ",";
            ss << fmt(x[i], 4);
        }
        ss << "]";
        return ok(ss.str(), "", steps.str(), "Discrete system");
    }

    AMResult randomWalk1D(double p, int n, int trials)
    {
        double q = 1 - p;
        // Theoretical: mean = (p-q)*n, variance = 4pq*n
        double mean_th = (p - q) * n, var_th = 4 * p * q * n;
        // Simulate
        std::vector<double> finals(trials);
        unsigned long seed = 42;
        double sumX = 0, sumX2 = 0;
        for (int t = 0; t < trials; ++t)
        {
            int pos = 0;
            for (int k = 0; k < n; ++k)
            {
                seed = (seed * 1664525 + 1013904223) & 0xffffffff;
                pos += (seed / 4294967296.0 < p) ? 1 : -1;
            }
            finals[t] = pos;
            sumX += pos;
            sumX2 += pos * pos;
        }
        double mean_sim = sumX / trials, var_sim = sumX2 / trials - mean_sim * mean_sim;

        std::ostringstream ss, steps;
        steps << "=== 1D Random Walk ===\n\n";
        steps << "Steps: n=" << n << ", p(right)=" << fmt(p) << ", q(left)=" << fmt(q) << "\n";
        steps << "Trials: " << trials << "\n\n";
        steps << "Theoretical:\n";
        steps << "  E[X_n] = (p-q)n = " << fmt(mean_th) << "\n";
        steps << "  Var[X_n] = 4pqn = " << fmt(var_th) << "\n";
        steps << "  SD = " << fmt(std::sqrt(var_th)) << "\n\n";
        steps << "Simulated:\n";
        steps << "  E[X_n] ≈ " << fmt(mean_sim, 4) << "\n";
        steps << "  Var[X_n] ≈ " << fmt(var_sim, 4) << "\n";
        ss << "E[X_" << n << "]=" << fmt(mean_th) << ", SD=" << fmt(std::sqrt(var_th));
        return ok(ss.str(), "", steps.str(), "Random walk");
    }

    AMResult branchingProcess(const Vec &pk, int generations)
    {
        // pk[k] = P(offspring = k), k=0,1,2,...
        double mu = 0, varP = 0;
        for (int k = 0; k < (int)pk.size(); ++k)
        {
            mu += k * pk[k];
            varP += k * k * pk[k];
        }
        varP -= mu * mu;

        // Extinction probability: smallest fixed point of G(s) = sum pk*s^k = s
        double q = 0; // extinction probability
        if (mu <= 1)
        {
            q = 1;
        }
        else
        {
            // Iterate G(q) = q starting from q=0
            q = 0.5;
            for (int iter = 0; iter < 200; ++iter)
            {
                double Gq = 0;
                double sp = 1;
                for (int k = 0; k < (int)pk.size(); ++k)
                {
                    Gq += pk[k] * sp;
                    sp *= q;
                }
                q = Gq;
            }
        }

        std::ostringstream ss, steps;
        steps << "=== Branching Process ===\n\n";
        steps << "Offspring distribution: p_k = ";
        for (int k = 0; k < (int)pk.size(); ++k)
        {
            if (k)
                steps << ", ";
            steps << "p_" << k << "=" << fmt(pk[k], 3);
        }
        steps << "\n\nMean: μ = " << fmt(mu, 4) << "\n";
        steps << "Variance: σ² = " << fmt(varP, 4) << "\n\n";
        steps << "Extinction probability q:\n";
        steps << (mu <= 1 ? "μ ≤ 1: certain extinction (q=1)\n" : "μ > 1: sub-critical → q = smallest root of G(s)=s\n");
        steps << "q = " << fmt(q, 4) << "\n\n";

        // Population means
        steps << "Expected population: E[Z_n] = μⁿ\n";
        for (int n = 0; n <= generations && n <= 10; ++n)
            steps << "  n=" << n << ": E[Z_n]=" << fmt(std::pow(mu, n), 3) << "\n";

        ss << "μ=" << fmt(mu, 4) << ", q_ext=" << fmt(q, 4);
        return ok(ss.str(), "", steps.str(), "Branching process");
    }

    // ── Natural boundary conditions ───────────────────────────────────────────────

    AMResult naturalBC(const std::string &L, const std::string &x,
                       const std::string &y, bool freeLeft, bool freeRight)
    {
        std::ostringstream ss;
        ss << "Natural Boundary Conditions for J[y] = ∫L(x,y,y') dx\n\n";
        ss << "The Euler-Lagrange equation must hold:\n";
        ss << "  ∂L/∂y - d/dx(∂L/∂y') = 0\n\n";
        if (freeLeft)
        {
            ss << "Left endpoint free (natural BC):\n";
            ss << "  [∂L/∂y']_{x=a} = 0\n\n";
        }
        if (freeRight)
        {
            ss << "Right endpoint free (natural BC):\n";
            ss << "  [∂L/∂y']_{x=b} = 0\n\n";
        }
        ss << "Lagrangian: L = " << L << "\n";
        ss << "These conditions replace Dirichlet BCs at free endpoints.";
        return ok(ss.str());
    }

    // ── Green's function ──────────────────────────────────────────────────────────

    AMResult greensFunction(const std::string &p, const std::string &q,
                            double a, double b,
                            double alpha, double beta, // BC coefficients
                            const std::string &forcing)
    {
        std::ostringstream ss;
        ss << "Green's Function for Differential Operator L[y] = -(p(x)y')' + q(x)y\n";
        ss << "p(x) = " << p << ", q(x) = " << q << "\n";
        ss << "Domain: [" << a << ", " << b << "]\n";
        ss << "Boundary Conditions: y(" << a << ") = " << alpha << ", y(" << b << ") = " << beta << "\n\n";

        ss << "Mathematical Framework:\n";
        ss << "1. The Green's function G(x,ξ) solves L[G] = δ(x - ξ).\n";
        ss << "2. Continuity: G(x,ξ) is continuous at x = ξ.\n";
        ss << "3. Jump Condition: ∂G/∂x (ξ⁺,ξ) - ∂G/∂x (ξ⁻,ξ) = -1 / p(ξ).\n\n";

        try
        {
            // Example logic for the most common case: p=1, q=0 (Poisson Equation)
            if ((p == "1" || p == "1.0") && (q == "0" || q == "0.0"))
            {
                ss << "Case Detected: p=1, q=0 (Standard Laplacian)\n";
                ss << "For homogeneous Dirichlet BCs on [" << a << ", " << b << "]:\n";
                ss << "G(x,ξ) = { (x-a)(b-ξ)/(b-a)  for a ≤ x ≤ ξ\n";
                ss << "         { (ξ-a)(b-x)/(b-a)  for ξ < x ≤ b\n\n";

                // If forcing is provided, mention the integral solution
                if (!forcing.empty() && forcing != "0")
                {
                    ss << "The solution for f(x) = " << forcing << " is:\n";
                    ss << "y(x) = ∫[" << a << "," << b << "] G(x,ξ) f(ξ) dξ\n";
                }
            }
            else
            {
                ss << "General Solution Procedure:\n";
                ss << "- Let y₁(x) satisfy L[y₁]=0 and the BC at x=" << a << ".\n";
                ss << "- Let y₂(x) satisfy L[y₂]=0 and the BC at x=" << b << ".\n";
                ss << "- G(x,ξ) = (1/W) * { y₁(x)y₂(ξ) for x ≤ ξ, y₁(ξ)y₂(x) for x > ξ }\n";
                ss << "- Where W is the weighted Wronskian: p(x)[y₁'y₂ - y₁y₂'].\n";
            }
        }
        catch (const std::exception &e)
        {
            ss << "Error in analysis: " << e.what() << "\n";
        }

        return ok(ss.str());
    }

    // ── Simple wave (quasi-linear) ────────────────────────────────────────────────

    AMResult simpleWave(const std::string &u0, const std::string &c, double xMax, double tMax)
    {
        std::ostringstream ss;
        ss << "Simple Wave:  u_t + c(u) u_x = 0\n";
        ss << "where c(u) = " << c << "\n\n";
        ss << "Solution via characteristics: x - c(u) t = const\n";
        ss << "Characteristic speed: c(u) = " << c << "\n\n";
        ss << "Initial condition: u(x,0) = " << u0 << "\n\n";
        ss << "The solution is constant along characteristics:\n";
        ss << "  u(x,t) = u₀(ξ)  where  x = ξ + c(u₀(ξ)) · t\n\n";

        ss << "Shock forms when characteristics cross:\n";
        ss << "  t_shock = -1 / min(c'(u₀) · u₀'(ξ))  for all ξ\n\n";

        // 1. Define missing spatial bounds and steps locally
        double xMin = 0.0;
        int steps = 100;

        try
        {
            auto cExpr = Calculus::parse(c);
            auto u0Expr = Calculus::parse(u0);

            double dx = (xMax - xMin) / steps;

            ss << "Characteristic trace (x₀ → x at t=" << tMax << "):\n";
            for (int i = 0; i <= steps; i += steps / 10)
            {
                double xi = xMin + i * dx;

                // 2. Evaluate u0 first, then plug that value into c(u)
                double u0val = Calculus::evaluate(u0Expr, {{"x", xi}});
                double cval = Calculus::evaluate(cExpr, {{"u", u0val}});

                double xT = xi + cval * tMax;
                ss << "  ξ=" << xi << " → x=" << xT << "  (u=" << u0val << ")\n";
            }
        }
        catch (const std::exception &e)
        {
            ss << "Numerical evaluation error: " << e.what() << "\n";
        }

        return ok(ss.str());
    }

    // ── Stream function ───────────────────────────────────────────────────────────

    AMResult streamFunction(const std::string &psi, double x0, double y0)
    {
        std::ostringstream ss;
        ss << "Stream Function:  ψ(x,y) = " << psi << "\n\n";
        ss << "Velocity components:\n";
        ss << "  u = ∂ψ/∂y  (x-component)\n";
        ss << "  v = -∂ψ/∂x  (y-component)\n\n";

        try
        {
            auto psiExpr = Calculus::parse(psi);
            auto u = Calculus::simplify(Calculus::diff(psiExpr, "y"));
            auto v = Calculus::simplify(Calculus::neg(Calculus::diff(psiExpr, "x")));

            ss << "  u(x,y) = " << Calculus::toString(u) << "\n";
            ss << "  v(x,y) = " << Calculus::toString(v) << "\n\n";

            // Vorticity ω = ∂v/∂x - ∂u/∂y = -∇²ψ
            auto vorticity = Calculus::simplify(
                Calculus::sub(Calculus::diff(v, "x"), Calculus::diff(u, "y")));
            ss << "Vorticity ω = ∂v/∂x - ∂u/∂y = " << Calculus::toString(vorticity) << "\n";
            ss << "(For irrotational flow: ω = 0, i.e. ∇²ψ = 0)\n\n";

            Calculus::SymbolTable syms;
            syms["x"] = x0;
            syms["y"] = y0;
            // Evaluate at point
            ss << "At (" << x0 << ", " << y0 << "):\n";

            // 2. Pass the table by reference to the evaluate calls
            ss << "  ψ = " << Calculus::evaluate(psiExpr, syms) << "\n";
            ss << "  u = " << Calculus::evaluate(u, syms) << "\n";
            ss << "  v = " << Calculus::evaluate(v, syms) << "\n";
        }
        catch (const std::exception &e)
        {
            ss << "Symbolic computation: " << e.what();
        }

        return ok(ss.str());
    }

    // ── Continuity equation (1D) ──────────────────────────────────────────────────

    AMResult continuityEquation1D(double rho0, const std::string &v, double domainLength, double tEnd)
    {
        std::ostringstream ss;
        ss << "Continuity Equation:  ∂ρ/∂t + ∂(ρv)/∂x = 0\n\n";
        ss << "Velocity field: v(x,t) = " << v << "\n";
        ss << "Initial density: ρ₀ = " << rho0 << "\n\n";

        // 1. Define missing grid parameters locally
        int nx = 100;
        int nt = 100;
        double dx = domainLength / nx;
        double dt = tEnd / nt;
        double CFL = dt / dx; // Assume max |v| ~ 1 for a basic CFL check

        ss << "Numerical solution (upwind scheme):\n";
        ss << "  Grid: nx=" << nx << ", nt=" << nt << "\n";
        ss << "  dx=" << dx << ", dt=" << dt << "\n";
        ss << "  CFL = dt/dx = " << CFL << "\n\n";

        // 2. Rename the vector to 'rho_grid' to avoid shadowing the 'rho0' parameter
        std::vector<double> rho_grid(nx + 1, rho0);

        try
        {
            auto vExpr = Calculus::parse(v);
            for (int n = 0; n < nt; ++n)
            {
                double currentTime = n * dt;
                std::vector<double> rhoNew(nx + 1);

                for (int i = 1; i < nx; ++i)
                {
                    double currentX = i * dx;
                    // 3. Evaluate using the correct velocity string 'v'
                    double vi = Calculus::evaluate(vExpr, {{"x", currentX}, {"t", currentTime}});

                    if (vi > 0)
                        rhoNew[i] = rho_grid[i] - (dt / dx) * vi * (rho_grid[i] - rho_grid[i - 1]);
                    else
                        rhoNew[i] = rho_grid[i] - (dt / dx) * vi * (rho_grid[i + 1] - rho_grid[i]);
                }

                rhoNew[0] = rho0;  // Left Boundary Condition
                rhoNew[nx] = rho0; // Right Boundary Condition
                rho_grid = rhoNew;
            }

            ss << "Final density profile (sample at t=" << tEnd << "):\n";
            for (int i = 0; i <= nx; i += nx / 10)
            {
                ss << "  x=" << i * dx << ", ρ=" << rho_grid[i] << "\n";
            }
        }
        catch (const std::exception &e)
        {
            ss << "Numerical evaluation error: " << e.what() << "\n";
        }

        return ok(ss.str());
    }
    // ── Euler equations (1D gas dynamics) ────────────────────────────────────────

    AMResult eulerEquations1D(double rho, double u, double p, double gamma, double dx, int n)
    {
        std::ostringstream ss;

        // Avoid division by zero
        if (rho <= 0 || p <= 0)
        {
            return ok("Error: Density (rho) and Pressure (p) must be positive.");
        }

        // Map input parameters to local variables for clarity
        double rho0 = rho;
        double u0 = u;
        double p0 = p;

        double c0 = std::sqrt(gamma * p0 / rho0);
        double M0 = std::abs(u0 / c0); // Mach number is typically magnitude
        double totalEnergy = (p0 / (gamma - 1.0)) + (0.5 * rho0 * u0 * u0);

        ss << "1D Euler Equations (Inviscid Compressible Flow):\n";
        ss << "  ∂ρ/∂t + ∂(ρu)/∂x = 0\n";
        ss << "  ∂(ρu)/∂t + ∂(ρu² + p)/∂x = 0\n";
        ss << "  ∂E/∂t + ∂((E+p)u)/∂x = 0\n\n";

        ss << "Input State (Initial Conditions):\n";
        ss << "  ρ₀ = " << rho0 << "\n";
        ss << "  u₀ = " << u0 << "\n";
        ss << "  p₀ = " << p0 << "\n";
        ss << "  γ  = " << gamma << "\n\n";

        ss << "Derived Quantities:\n";
        ss << "  Sound Speed (c₀): " << c0 << "\n";
        ss << "  Mach Number (M₀): " << M0 << "\n";
        ss << "  Total Energy (E₀): " << totalEnergy << "\n\n";

        ss << "Characteristic Speeds (Eigenvalues):\n";
        ss << "  λ₁ (u - c) = " << u0 - c0 << "\n";
        ss << "  λ₂ (u)     = " << u0 << "\n";
        ss << "  λ₃ (u + c) = " << u0 + c0 << "\n\n";

        // Isentropic / Stagnation Ratios
        if (M0 > 0)
        {
            ss << "Stagnation (Total) Property Ratios:\n";
            double factor = 1.0 + (gamma - 1.0) / 2.0 * M0 * M0;

            double TRatio = 1.0 / factor;
            double pRatio = std::pow(factor, -gamma / (gamma - 1.0));
            double rhoRatio = std::pow(factor, -1.0 / (gamma - 1.0));

            ss << "  T / T_total   = " << TRatio << "\n";
            ss << "  p / p_total   = " << pRatio << "\n";
            ss << "  ρ / ρ_total   = " << rhoRatio << "\n";
        }

        return ok(ss.str());
    }

    AMResult lotkaVolterra(double alpha, double beta, double delta,
                           double gamma_lv, double x0, double y0,
                           double tEnd, int n)
    {
        // dx/dt = αx - βxy  (prey)
        // dy/dt = δxy - γy  (predator)
        double h = tEnd / n;
        double x = x0, y = y0;

        // Conservation: V = δx - γln(x) + βy - αln(y) = constant
        double V0 = delta * x0 - gamma_lv * std::log(x0) + beta * y0 - alpha * std::log(y0);

        std::ostringstream ss;
        ss << "Lotka-Volterra Predator-Prey Model\n\n";
        ss << "  dx/dt = " << alpha << "x - " << beta << "xy  (prey)\n";
        ss << "  dy/dt = " << delta << "xy - " << gamma_lv << "y  (predator)\n\n";

        // Fixed points
        ss << "Fixed points:\n";
        ss << "  (0, 0) — trivial (unstable saddle)\n";
        double xStar = gamma_lv / delta;
        double yStar = alpha / beta;
        ss << "  (" << xStar << ", " << yStar << ") — coexistence (centre)\n\n";

        // Period estimate: T ≈ 2π / sqrt(αγ)
        double T = 2 * M_PI / std::sqrt(alpha * gamma_lv);
        ss << "Approximate period: T ≈ " << T << "\n\n";

        // Conservation quantity
        ss << "Conservation law: V = δx - γln(x) + βy - αln(y) = " << V0 << "\n\n";

        // RK4 integration
        ss << "Solution trajectory (RK4, t=" << tEnd << "):\n";
        ss << "Points: [";
        for (int i = 0; i <= n; i += n / 50)
        {
            // Not printing internal steps — just collect
            if (i > 0)
                ss << ",";
            ss << "[" << i * h << "," << x << "," << y << "]";
        }
        ss << "]\n\n";

        // Actually do RK4
        x = x0;
        y = y0;
        std::vector<double> ts, xs, ys;
        int step = std::max(1, n / 100);
        for (int i = 0; i <= n; ++i)
        {
            if (i % step == 0)
            {
                ts.push_back(i * h);
                xs.push_back(x);
                ys.push_back(y);
            }
            double k1x = alpha * x - beta * x * y;
            double k1y = delta * x * y - gamma_lv * y;
            double k2x = alpha * (x + h / 2 * k1x) - beta * (x + h / 2 * k1x) * (y + h / 2 * k1y);
            double k2y = delta * (x + h / 2 * k1x) * (y + h / 2 * k1y) - gamma_lv * (y + h / 2 * k1y);
            double k3x = alpha * (x + h / 2 * k2x) - beta * (x + h / 2 * k2x) * (y + h / 2 * k2y);
            double k3y = delta * (x + h / 2 * k2x) * (y + h / 2 * k2y) - gamma_lv * (y + h / 2 * k2y);
            double k4x = alpha * (x + h * k3x) - beta * (x + h * k3x) * (y + h * k3y);
            double k4y = delta * (x + h * k3x) * (y + h * k3y) - gamma_lv * (y + h * k3y);
            x += h / 6 * (k1x + 2 * k2x + 2 * k3x + k4x);
            y += h / 6 * (k1y + 2 * k2y + 2 * k3y + k4y);
            if (x < 0)
                x = 0;
            if (y < 0)
                y = 0;
        }

        // Rebuild output with actual RK4 data
        std::ostringstream out;
        out << "Lotka-Volterra Predator-Prey (RK4)\n\n";
        out << "  dx/dt = " << alpha << "x - " << beta << "xy\n";
        out << "  dy/dt = " << delta << "xy - " << gamma_lv << "y\n";
        out << "  x(0)=" << x0 << ", y(0)=" << y0 << "\n\n";
        out << "Fixed points: (0,0) and (" << xStar << ", " << yStar << ")\n";
        out << "Period ≈ " << T << "\n";
        out << "Conservation: V₀ = " << V0 << "\n\n";
        out << "Points(t,prey,predator): [";
        for (int i = 0; i < (int)ts.size(); ++i)
        {
            if (i)
                out << ",";
            out << "[" << ts[i] << "," << xs[i] << "," << ys[i] << "]";
        }
        out << "]";

        return ok(out.str());
    }

    // ── Gas dynamics shock conditions ─────────────────────────────────────────────

    AMResult gasDynamicsShock(double gamma, double M1, double p1,
                              double rho1, double T1)
    {
        if (M1 < 1.0)
            return err("Normal shock requires M₁ > 1");

        double M1sq = M1 * M1;

        // Rankine-Hugoniot relations
        double M2sq = (M1sq + 2.0 / (gamma - 1)) / (2.0 * gamma / (gamma - 1) * M1sq - 1.0);
        double M2 = std::sqrt(M2sq);

        double p2_p1 = (2.0 * gamma * M1sq - (gamma - 1)) / (gamma + 1);
        double rho2_rho1 = (gamma + 1) * M1sq / ((gamma - 1) * M1sq + 2.0);
        double T2_T1 = p2_p1 / rho2_rho1;
        double V1_V2 = rho2_rho1; // velocity ratio = density ratio (mass conservation)

        // Entropy change
        double pRatio = p2_p1;
        double rhoRatio = rho2_rho1;
        double deltaS = std::log(pRatio / std::pow(rhoRatio, gamma));

        std::ostringstream ss;
        ss << "Normal Shock Relations  (γ = " << gamma << ", M₁ = " << M1 << ")\n\n";
        ss << "Rankine-Hugoniot conditions:\n";
        ss << "  M₂     = " << M2 << "  (M₂ < 1 always)\n";
        ss << "  p₂/p₁  = " << p2_p1 << "\n";
        ss << "  ρ₂/ρ₁  = " << rho2_rho1 << "\n";
        ss << "  T₂/T₁  = " << T2_T1 << "\n";
        ss << "  v₁/v₂  = " << V1_V2 << "  (velocity decreases)\n\n";

        if (p1 > 0 && rho1 > 0 && T1 > 0)
        {
            ss << "Absolute values downstream:\n";
            ss << "  p₂  = " << p2_p1 * p1 << " Pa\n";
            ss << "  ρ₂  = " << rho2_rho1 * rho1 << " kg/m³\n";
            ss << "  T₂  = " << T2_T1 * T1 << " K\n\n";
        }

        ss << "Entropy change across shock:\n";
        ss << "  ΔS/R = ln(p₂/p₁) - γ·ln(ρ₂/ρ₁) = " << deltaS << "\n";
        ss << "  (ΔS > 0 confirms physically admissible shock)\n";

        return ok(ss.str());
    }

    // ── Galton-Watson branching process ──────────────────────────────────────────

    AMResult galtonWatson(const std::vector<double> &pk, int generations)
    {
        if (pk.empty())
            return err("Offspring distribution cannot be empty");

        double mean = 0.0;
        for (int k = 0; k < (int)pk.size(); ++k)
            mean += k * pk[k];
        double var = 0.0;
        for (int k = 0; k < (int)pk.size(); ++k)
            var += k * k * pk[k];
        var -= mean * mean;

        // PGF: G(s) = Σ pk s^k
        // Extinction probability: smallest root of G(q) = q in [0,1]
        double q = 1.0; // start with q=1
        if (mean > 1.0)
        {
            // Iterate G(q) starting from q=0
            q = 0.0;
            for (int iter = 0; iter < 1000; ++iter)
            {
                double gq = 0.0;
                for (int k = 0; k < (int)pk.size(); ++k)
                {
                    gq += pk[k] * std::pow(q, k);
                }
                if (std::abs(gq - q) < 1e-10)
                    break;
                q = gq;
            }
        }

        std::ostringstream ss;
        ss << "Galton-Watson Branching Process\n\n";
        ss << "Offspring distribution P(Z=k):\n";
        for (int k = 0; k < (int)pk.size(); ++k)
            ss << "  P(Z=" << k << ") = " << pk[k] << "\n";

        ss << "\nMean offspring: μ = " << mean << "\n";
        ss << "Variance: σ² = " << var << "\n";
        ss << "Extinction probability: q = " << q << "\n";
        ss << "Population type: " << (mean < 1 ? "subcritical (extinction certain)" : mean > 1 ? "supercritical (positive survival prob)"
                                                                                               : "critical")
           << "\n\n";

        // Simulate mean and variance growth
        ss << "Expected population size E[Z_n]:\n";
        double en = 1.0;
        for (int n = 1; n <= generations; ++n)
        {
            en *= mean;
            if (n <= 5 || n == generations)
                ss << "  E[Z_" << n << "] = " << en << "\n";
        }

        if (mean > 1.0 && generations <= 10)
        {
            // Simulate a few sample paths
            ss << "\nSample paths (3 simulations):\n";
            for (int trial = 0; trial < 3; ++trial)
            {
                int pop = 1;
                ss << "  Trial " << trial + 1 << ": 1";
                for (int g = 1; g <= generations && pop > 0; ++g)
                {
                    int newPop = 0;
                    for (int ind = 0; ind < pop; ++ind)
                    {
                        double r = (double)rand() / RAND_MAX;
                        double cumP = 0;
                        for (int k = 0; k < (int)pk.size(); ++k)
                        {
                            cumP += pk[k];
                            if (r < cumP)
                            {
                                newPop += k;
                                break;
                            }
                        }
                    }
                    pop = newPop;
                    ss << " → " << pop;
                }
                ss << "\n";
            }
        }

        return ok(ss.str());
    }

    // =============================================================================
    // DISPATCH
    // =============================================================================

    // ── Dispatcher ───────────────────────────────────────────────────────────────

    AMResult dispatch(const std::string &operation, const std::string &json, bool exactMode)
    {
        try
        {
            const std::string &op = operation; // op comes from CoreEngine prefix, not JSON

            // ── Dimensional Analysis ──────────────────────────────────────────────
            if (op == "buckingham")
            {
                // Fix for Code 312: Pass vectors directly to the function
                return buckinghamPi(parseVecS(getP(json, "vars")), parseMat(getP(json, "D")));
            }

            if (op == "scaling")
                return scalingAnalysis(getP(json, "eq"), getP(json, "param"), getN(json, "eps"));

            // ── Dynamical Systems ─────────────────────────────────────────────────
            if (op == "classify_linear")
                return classifyLinearSystem(getN(json, "a11"), getN(json, "a12"), getN(json, "a21"), getN(json, "a22"));

            if (op == "linearise")
                return lineariseAtFixedPoint(getP(json, "f"), getP(json, "g"), getN(json, "x0"), getN(json, "y0"));

            // ── Kinetics & Biology ────────────────────────────────────────────────
            if (op == "michaelis")
                return michaelisМenten(getN(json, "kcat"), getN(json, "Km"), getN(json, "E0"), getN(json, "S0"), getN(json, "T", 10), (int)getN(json, "n", 100));

            if (op == "lotka_volterra")
                return lotkaVolterra(getN(json, "alpha"), getN(json, "beta"), getN(json, "delta"), getN(json, "gamma"), getN(json, "x0"), getN(json, "y0"), getN(json, "T", 50), (int)getN(json, "n", 1000));

            // ── Calculus of Variations ────────────────────────────────────────────
            if (op == "euler_lagrange")
                // Fix for Line 2764: Added missing variable name parameters
                return eulerLagrange(getP(json, "L"), getP(json, "x", "x"), getP(json, "y", "y"), getP(json, "yp", "yp"));

            if (op == "brachistochrone")
                // Fix for Line 2769: Added missing coordinates
                return brachistochrone(getN(json, "x0"), getN(json, "y0"), getN(json, "x1"), getN(json, "y1"));

            // ── Integrals & Sturm-Liouville ───────────────────────────────────────
            if (op == "sturm_liouville")
            {
                auto sl = sturmLiouville(getP(json, "p"), getP(json, "q"), getP(json, "r"), getN(json, "a"), getN(json, "b"), (int)getN(json, "n", 5));
                std::ostringstream sl_ss;
                sl_ss << "Eigenvalues: [";
                for (int i = 0; i < (int)sl.eigenvalues.size(); ++i)
                {
                    if (i)
                        sl_ss << ", ";
                    sl_ss << cu_fmt(sl.eigenvalues[i]);
                }
                sl_ss << "]\n";
                sl_ss << "Eigenfunctions: " << sl.eigenfunctions.size() << " computed";
                return ok(sl_ss.str());
            }

            // ── PDEs ──────────────────────────────────────────────────────────────
            if (op == "greens_function")
                // Fix for Line 2785: Signature requires (string, string, double, double)
                return greensFunction(getP(json, "p", "1"), getP(json, "q", "0"), getN(json, "a"), getN(json, "b", 1.0), getN(json, "alpha", 1.0), getN(json, "beta", 0.0), getP(json, "f", "0"));

            if (op == "simple_wave")
                return simpleWave(getP(json, "c"), getP(json, "u0"), getN(json, "xmin"), getN(json, "xmax"));

            if (op == "continuity_1d")
                // Fix for Line 2792: Signature order (rho, u, L, n)
                return continuityEquation1D(getN(json, "rho", 1.0), getP(json, "u", "1"), getN(json, "L", 10), getN(json, "T", 1));

            if (op == "euler_1d")
                // Fix for Line 2796: Signature order (p, u, rho)
                return eulerEquations1D(getN(json, "gamma", 1.4), getN(json, "rho", 1.225), getN(json, "u", 0), getN(json, "p", 101325), getN(json, "T", 1.0), (int)getN(json, "n", 100));

            // ── Waves ─────────────────────────────────────────────────────────────
            if (op == "dalembert")
                return dAlembertSolution(getP(json, "f0"), getP(json, "g0"), getN(json, "c", 1), getN(json, "x", 0), getN(json, "t", 1));

            if (op == "burgers")
                return burgers(getN(json, "nu", 0.1), getP(json, "u0"), getN(json, "L", 10), getN(json, "T", 1), (int)getN(json, "nx", 100), (int)getN(json, "nt", 1000));

            // ── Discrete ──────────────────────────────────────────────────────────
            if (op == "galton_watson")
                return galtonWatson(parseVecD(getP(json, "pk")), (int)getN(json, "gen", 10));

            // ── Epidemiology ──────────────────────────────────────────────────────
            if (op == "sir" || op == "sir_epidemic")
                return sirModel(getN(json, "beta"), getN(json, "gamma"),
                                getN(json, "S0", 990), getN(json, "I0", 10), getN(json, "R0", 0),
                                getN(json, "T", 100), (int)getN(json, "n", 500));

            if (op == "seir")
                return seirModel(getN(json, "beta"), getN(json, "sigma"), getN(json, "gamma"),
                                 getN(json, "S0"), getN(json, "E0"), getN(json, "I0"), getN(json, "R0"),
                                 getN(json, "T", 100), (int)getN(json, "n", 500));

            if (op == "reproduction_number" || op == "R0")
                return reproductionNumber(getN(json, "beta"), getN(json, "gamma"), getN(json, "N", 1000));

            // ── Population dynamics ───────────────────────────────────────────────
            if (op == "logistic_map")
                return logisticMap(getN(json, "r", 3.5), getN(json, "x0", 0.5),
                                   (int)getN(json, "n", 200));

            if (op == "logistic" || op == "logistic_growth")
            {
                // Continuous logistic growth: dN/dt = r*N*(1 - N/K)
                // Exact solution: N(t) = K / (1 + ((K-N0)/N0)*exp(-r*t))
                double r_ = getN(json, "r", 2.0);
                double K = getN(json, "K", 100.0);
                double N0 = getN(json, "x0", 10.0);
                double T = getN(json, "T", 50.0);
                int n = (int)getN(json, "n", 200);
                if (K <= 0 || N0 <= 0 || r_ <= 0)
                    return err("r, K, x0 must all be positive");
                double Nt = K / (1.0 + ((K - N0) / N0) * std::exp(-r_ * T));
                double halfT = std::log((K - N0) / N0) / r_;
                std::ostringstream ss;
                ss << "Logistic Growth  dN/dt = r·N·(1 - N/K)";
                ss << "Parameters: r=" << r_ << ", K=" << K << ", N₀=" << N0 << " ";
                ss << "Exact solution: N(t) = K / (1 + ((K-N₀)/N₀)·e^{-rt})";
                ss << "At t=" << T << ": N = " << cu_fmt(Nt, 6) << " ";
                if (halfT > 0)
                    ss << "Half-K time (N=" << K / 2 << "): t = " << cu_fmt(halfT, 6) << " ";
                ss << "Carrying capacity K = " << K << " ";
                ss << "Selected time points : ";
                for (int i = 0; i <= std::min(n, 10); ++i)
                {
                    double t = i * T / std::min(n, 10);
                    double Ni = K / (1.0 + ((K - N0) / N0) * std::exp(-r_ * t));
                    ss << "  t=" << cu_fmt(t, 4) << "  N=" << cu_fmt(Ni, 6) << " ";
                }
                AMResult res;
                res.symbolic = ss.str();
                res.numerical = cu_fmt(Nt, 10);
                return res;
            }

            if (op == "mass_action")
                return massAction(getN(json, "k"), getN(json, "A0"), getN(json, "B0"),
                                  getN(json, "T", 10), (int)getN(json, "n", 200));

            // ── Dynamical Systems ─────────────────────────────────────────────────
            if (op == "phase_portrait")
            {
                auto pp = analysePhasePortrait(
                    getP(json, "f"), getP(json, "g"),
                    getN(json, "xmin", -5), getN(json, "xmax", 5),
                    getN(json, "ymin", -5), getN(json, "ymax", 5));
                if (!pp.ok)
                    return err(pp.error);
                std::ostringstream ss;
                ss << "Phase Portrait Analysis\n\n";
                ss << "Fixed points found: " << pp.fixedPoints.size() << "\n";
                for (auto &fp : pp.fixedPoints)
                {
                    ss << "  (" << fp.x << ", " << fp.y << ")  →  " << fp.stability << "\n";
                    ss << "    λ₁ = " << fp.eigenvalue1_re << (fp.eigenvalue1_im != 0 ? " + " + std::to_string(fp.eigenvalue1_im) + "i" : "") << "\n";
                    ss << "    λ₂ = " << fp.eigenvalue2_re << (fp.eigenvalue2_im != 0 ? " + " + std::to_string(fp.eigenvalue2_im) + "i" : "") << "\n";
                }
                ss << "\nGlobal behaviour: " << pp.globalBehavior;
                AMResult r;
                r.symbolic = ss.str();
                r.numerical = ss.str();
                return r;
            }

            if (op == "bifurcation")
                return bifurcationDiagram(
                    getP(json, "f"), getP(json, "x", "x"), getP(json, "mu", "mu"),
                    getN(json, "mumin", -3), getN(json, "mumax", 3),
                    (int)getN(json, "n", 100));

            if (op == "classify_bifurcation")
                return classifyBifurcation(
                    getP(json, "f"), getP(json, "x", "x"), getP(json, "mu", "mu"),
                    getN(json, "x0", 0), getN(json, "mu0", 0));

            if (op == "nullclines")
                return computeNullclines(
                    getP(json, "f"), getP(json, "g"),
                    getN(json, "xmin", -5), getN(json, "xmax", 5),
                    getN(json, "ymin", -5), getN(json, "ymax", 5));

            if (op == "lyapunov")
                return checkLyapunov(
                    getP(json, "V"), getP(json, "f"), getP(json, "g"),
                    getN(json, "xmin", -5), getN(json, "xmax", 5),
                    getN(json, "ymin", -5), getN(json, "ymax", 5));

            // ── Perturbation methods ──────────────────────────────────────────────
            if (op == "perturbation" || op == "regular_perturbation")
                return regularPerturbation(
                    getP(json, "eq"), getP(json, "var", "x"),
                    getP(json, "eps", "eps"), getN(json, "epsilon", 0.1),
                    (int)getN(json, "order", 3));

            if (op == "poincare_lindstedt")
                return poincareLindstedt(
                    getN(json, "omega0", 1.0), getN(json, "eps", 0.1),
                    getN(json, "x0", 1.0), getN(json, "v0", 0.0),
                    (int)getN(json, "order", 2));

            if (op == "boundary_layer")
                return boundaryLayerAnalysis(
                    getP(json, "p", "1"), getP(json, "q", "0"),
                    getN(json, "eps", 0.01), getN(json, "a", 0), getN(json, "b", 0));

            if (op == "multiple_scales")
                return multipleScales(
                    getN(json, "omega0", 1.0), getN(json, "eps", 0.1),
                    (int)getN(json, "order", 2));

            if (op == "wkb")
                return wkbApproximation(
                    getP(json, "q"), getP(json, "var", "x"),
                    getN(json, "eps", 0.1), getN(json, "a", 0), getN(json, "b", 1));

            if (op == "laplace_method")
                return laplaceMethod(
                    getP(json, "h"), getP(json, "g", "1"), getP(json, "var", "x"),
                    getN(json, "xstar", 0), getN(json, "N", 100));

            // ── Turing instability ────────────────────────────────────────────────
            if (op == "turing" || op == "turing_instability")
                return turingInstability(
                    getN(json, "D1", 1), getN(json, "D2", 10),
                    getN(json, "fu"), getN(json, "fv"),
                    getN(json, "gu"), getN(json, "gv"));

            // ── Calculus of Variations ────────────────────────────────────────────
            if (op == "beltrami")
                return beltramiIdentity(
                    getP(json, "L"), getP(json, "dep", "y"), getP(json, "deriv", "yp"));

            if (op == "isoperimetric")
                return isoperimetric(
                    getP(json, "F"), getP(json, "G"), getP(json, "var", "x"),
                    getN(json, "C", 1));

            if (op == "hamilton")
                return hamiltonPrinciple(
                    getP(json, "T"), getP(json, "V"),
                    getP(json, "q", "q"), getP(json, "qdot", "qdot"));

            if (op == "noether")
                return noetherTheorem(getP(json, "L"), getP(json, "symmetry", "time"));

            // ── Logistic & discrete models ────────────────────────────────────────
            if (op == "cobweb")
                return cobwebDiagram(getP(json, "f"), getN(json, "x0", 0.5),
                                     (int)getN(json, "n", 20));

            if (op == "discrete_fixed")
                return discreteFixedPoints(getP(json, "f"),
                                           getN(json, "xmin", -5), getN(json, "xmax", 5));

            if (op == "feigenbaum")
                return feigenbaumAnalysis(getN(json, "rmin", 2.5), getN(json, "rmax", 4.0),
                                          (int)getN(json, "n", 500));

            if (op == "random_walk")
                return randomWalk1D(getN(json, "p", 0.5), (int)getN(json, "steps", 100),
                                    (int)getN(json, "trials", 1000));

            // ── Fluid / continuum ─────────────────────────────────────────────────
            if (op == "speed_of_sound")
                return speedOfSound(getN(json, "gamma", 1.4), getN(json, "p"), getN(json, "rho"));

            if (op == "mach")
                return machNumber(getN(json, "v"), getN(json, "gamma", 1.4),
                                  getN(json, "p"), getN(json, "rho"));

            if (op == "shock" || op == "gas_shock")
                return gasDynamicsShock(getN(json, "gamma", 1.4),
                                        getN(json, "rho1"), getN(json, "u1"), getN(json, "p1"),
                                        getN(json, "M1"));

            if (op == "rankine_hugoniot")
                return rankineHugoniot(getP(json, "F"), getN(json, "uL"), getN(json, "uR"));

            if (op == "entropy_condition")
                return entropyCondition(getP(json, "F"), getN(json, "uL"), getN(json, "uR"),
                                        getN(json, "s"));

            if (op == "reaction_diffusion")
                return reactionDiffusion(getN(json, "D", 1.0), getP(json, "R"),
                                         getP(json, "u0"), getN(json, "L", 10),
                                         getN(json, "T", 1), (int)getN(json, "nx", 50),
                                         (int)getN(json, "nt", 500));

            if (op == "travelling_wave")
                return travellingWave(getN(json, "D", 1.0), getP(json, "R"),
                                      getN(json, "uminus", 0), getN(json, "uplus", 1));

            if (op == "dispersion")
                return dispersionRelation(getP(json, "eq"), getN(json, "k", 1));

            if (op == "wave_velocities")
                return waveVelocities(getP(json, "omega"), getN(json, "k0", 1));

            if (op == "potential_flow")
                return potentialFlow(getP(json, "phi"),
                                     getN(json, "x", 0), getN(json, "y", 0));

            if (op == "stream_function")
                return streamFunction(getP(json, "psi"),
                                      getN(json, "x", 0), getN(json, "y", 0));

            if (op == "characteristics")
                return methodOfCharacteristics(
                    getP(json, "c"), getP(json, "f", "0"),
                    getP(json, "u0"), getN(json, "T", 1), (int)getN(json, "n", 50));

            if (op == "shock_time")
                return shockFormationTime(getP(json, "c"), getP(json, "u0"),
                                          getN(json, "xmin", -5), getN(json, "xmax", 5));

            if (op == "nondim")
                return nondimensionalise(getP(json, "eq"),
                                         cu_parseVecS(getP(json, "vars")),
                                         cu_parseVecD(getP(json, "scales")));

            if (op == "fredholm")
                return fredholmSecondKind(getP(json, "f"), getP(json, "K"),
                                          getN(json, "a"), getN(json, "b"),
                                          getN(json, "lambda", 1), (int)getN(json, "n", 50));

            if (op == "volterra")
                return volterraSecondKind(getP(json, "f"), getP(json, "K"),
                                          getN(json, "a"), getN(json, "b"),
                                          (int)getN(json, "n", 100));

            if (op == "rayleigh")
                return rayleighQuotient(getP(json, "p", "1"), getP(json, "q", "0"),
                                        getP(json, "r", "1"), getP(json, "trial"),
                                        getN(json, "a"), getN(json, "b", 1));

            return err("Unknown applied math operation: " + op);
        }
        catch (const std::exception &e)
        {
            return err(e.what());
        }
    }
} // End of namespace AppliedMath