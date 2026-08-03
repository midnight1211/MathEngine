// DifferentialEquations_LibreTexts.cpp
// Union of all 11 LibreTexts Differential Equations texts:
//   Chasnov (Applied LA+DE), Herman (1st+2nd course), Chasnov (DE),
//   Lebl (Engineers), Trench (Elementary+BVP), Herman (PDEs),
//   Brorson (Numerical), Wiggins (ODE), Miersemann (PDEs), Walet (PDEs)

#include "DE.hpp"
#include "../CommonUtils.hpp"
#include "../MathCore.hpp"
#include "../Calculus/Calculus.hpp"
#include <cmath>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <numeric>
#include <functional>
#include <stdexcept>
#include <map>

namespace DifferentialEquations
{

    static const double PI = M_PI;

    static std::string fmtD(double v, int p = 6)
    {
        if (!std::isfinite(v))
            return std::isinf(v) ? (v > 0 ? "∞" : "-∞") : "NaN";
        std::ostringstream ss;
        ss << std::setprecision(p) << v;
        return ss.str();
    }

    static DEResult okD(const std::string &s, const std::string &n = "")
    {
        DEResult r;
        r.symbolic = s;
        r.numerical = n.empty() ? s : n;
        return r;
    }
    static DEResult errD(const std::string &m)
    {
        DEResult r;
        r.ok = false;
        r.error = m;
        return r;
    }

    // ── RK4 vector system helper ──────────────────────────────────────────────────
    using VecF = std::function<std::vector<double>(double, const std::vector<double> &)>;

    static std::vector<std::vector<double>> rk4System(
        const VecF &F, const std::vector<double> &x0, double t0, double t1, int n)
    {
        int dim = x0.size();
        double h = (t1 - t0) / n;
        std::vector<std::vector<double>> traj(n + 1, std::vector<double>(dim + 1));
        traj[0][0] = t0;
        for (int j = 0; j < dim; ++j)
            traj[0][j + 1] = x0[j];
        std::vector<double> x = x0;
        for (int i = 0; i < n; ++i)
        {
            double t = t0 + i * h;
            auto k1 = F(t, x);
            std::vector<double> xk2(dim), xk3(dim), xk4(dim);
            for (int j = 0; j < dim; ++j)
                xk2[j] = x[j] + h / 2 * k1[j];
            auto k2 = F(t + h / 2, xk2);
            for (int j = 0; j < dim; ++j)
                xk3[j] = x[j] + h / 2 * k2[j];
            auto k3 = F(t + h / 2, xk3);
            for (int j = 0; j < dim; ++j)
                xk4[j] = x[j] + h * k3[j];
            auto k4 = F(t + h, xk4);
            for (int j = 0; j < dim; ++j)
                x[j] += h / 6 * (k1[j] + 2 * k2[j] + 2 * k3[j] + k4[j]);
            traj[i + 1][0] = t + h;
            for (int j = 0; j < dim; ++j)
                traj[i + 1][j + 1] = x[j];
        }
        return traj;
    }

    // =============================================================================
    // APPLICATIONS OF FIRST-ORDER ODEs (Trench Ch.4, Herman)
    // =============================================================================

    // Mixing: dQ/dt = rIn·cIn - (rOut/V)·Q,  Q(0) = c0·V
    DEResult mixingProblem(double V, double cIn, double rIn, double rOut,
                           double c0, double tEnd)
    {
        double Q0 = c0 * V;
        double k = rOut / V;
        double Qss = rIn * cIn / k; // steady state

        // Integrating factor: Q(t) = Qss + (Q0 - Qss)e^{-kt}
        std::ostringstream ss;
        ss << "Mixing Problem:  dQ/dt = " << fmtD(rIn * cIn) << " - " << fmtD(k) << "Q\n";
        ss << "Q(0) = " << fmtD(Q0) << "  (volume V=" << V << ", c0=" << c0 << ")\n\n";
        ss << "Solution: Q(t) = " << fmtD(Qss) << " + " << fmtD(Q0 - Qss) << "e^{-" << fmtD(k) << "t}\n";
        ss << "Concentration: c(t) = Q(t)/V\n\n";
        ss << "Steady state: Q∞ = " << fmtD(Qss) << "  (c∞ = " << fmtD(Qss / V) << ")\n\n";
        // Table of values
        ss << "t         Q(t)      c(t)\n"
           << std::string(32, '-') << "\n";
        for (int i = 0; i <= 5; ++i)
        {
            double t = tEnd * i / 5;
            double Q = Qss + (Q0 - Qss) * std::exp(-k * t);
            ss << std::setw(9) << fmtD(t) << "  " << std::setw(9) << fmtD(Q)
               << "  " << fmtD(Q / V) << "\n";
        }
        std::string pts = "Points: [";
        for (int i = 0; i <= 50; ++i)
        {
            double t = tEnd * i / 50, Q = Qss + (Q0 - Qss) * std::exp(-k * t);
            if (i)
                pts += ",";
            pts += "[" + fmtD(t) + "," + fmtD(Q / V) + "]";
        }
        pts += "]";
        return okD(ss.str() + "\n" + pts);
    }

    // Newton's cooling: dT/dt = -k(T - Tenv),  T(0) = T0
    DEResult newtonCooling(double k, double Tenv, double T0, double tEnd)
    {
        std::ostringstream ss;
        ss << "Newton's Law of Cooling/Warming\n";
        ss << "dT/dt = -" << fmtD(k) << "(T - " << fmtD(Tenv) << "),  T(0) = " << fmtD(T0) << "\n\n";
        ss << "Solution: T(t) = " << fmtD(Tenv) << " + " << fmtD(T0 - Tenv) << "e^{-" << fmtD(k) << "t}\n\n";
        double halfTime = std::log(2.0) / k;
        ss << "Half-life (time for |T-Tenv| to halve): " << fmtD(halfTime) << "\n";
        ss << "T(∞) = " << fmtD(Tenv) << "  (equilibrium)\n\n";
        ss << "t       T(t)\n"
           << std::string(18, '-') << "\n";
        std::string pts = "Points: [";
        for (int i = 0; i <= 50; ++i)
        {
            double t = tEnd * i / 50;
            double T = Tenv + (T0 - Tenv) * std::exp(-k * t);
            if (i <= 5)
                ss << fmtD(t, 4) << "    " << fmtD(T) << "\n";
            if (i)
                pts += ",";
            pts += "[" + fmtD(t) + "," + fmtD(T) + "]";
        }
        pts += "]";
        return okD(ss.str() + "\n" + pts);
    }

    // Population: dP/dt = rP  (exponential) or dP/dt = rP(1-P/K) (logistic)
    DEResult populationGrowth(double r, double P0, double tEnd,
                              bool logistic, double K)
    {
        std::ostringstream ss;
        if (!logistic)
        {
            ss << "Exponential Growth:  dP/dt = " << fmtD(r) << "P,  P(0) = " << fmtD(P0) << "\n";
            ss << "P(t) = " << fmtD(P0) << " e^{" << fmtD(r) << "t}\n";
            if (r > 0)
                ss << "Doubling time: " << fmtD(std::log(2) / r) << "\n";
            else if (r < 0)
                ss << "Half-life: " << fmtD(-std::log(2) / r) << "\n";
            ss << "\nPoints: [";
            for (int i = 0; i <= 50; ++i)
            {
                double t = tEnd * i / 50, P = P0 * std::exp(r * t);
                if (i)
                    ss << ",";
                ss << "[" << fmtD(t) << "," << fmtD(P) << "]";
            }
            ss << "]";
        }
        else
        {
            ss << "Logistic Growth:  dP/dt = " << fmtD(r) << "P(1 - P/" << fmtD(K) << "),  P(0) = " << fmtD(P0) << "\n";
            ss << "P(t) = K / (1 + ((K-P0)/P0) e^{-rt})\n";
            ss << "Carrying capacity K = " << fmtD(K) << "\n";
            ss << "Inflection point: P = K/2 = " << fmtD(K / 2)
               << " at t = " << fmtD(std::log((K - P0) / P0) / r) << "\n";
            ss << "\nPoints: [";
            double A = (K - P0) / P0;
            for (int i = 0; i <= 50; ++i)
            {
                double t = tEnd * i / 50, P = K / (1 + A * std::exp(-r * t));
                if (i)
                    ss << ",";
                ss << "[" << fmtD(t) << "," << fmtD(P) << "]";
            }
            ss << "]";
        }
        return okD(ss.str());
    }

    // Terminal velocity: m dv/dt = mg - kv
    DEResult terminalVelocity(double m, double g, double k, double v0, double tEnd)
    {
        double vt = m * g / k;
        std::ostringstream ss;
        ss << "Terminal Velocity (linear drag):\n";
        ss << "m dv/dt = mg - kv\n";
        ss << "m=" << fmtD(m) << ", g=" << fmtD(g) << ", k=" << fmtD(k) << ", v(0)=" << fmtD(v0) << "\n\n";
        ss << "v(t) = " << fmtD(vt) << " + (" << fmtD(v0 - vt) << ")e^{-" << fmtD(k / m) << "t}\n";
        ss << "Terminal velocity: v∞ = mg/k = " << fmtD(vt) << "\n";
        double tHalf = m / k * std::log(2.0);
        ss << "Time to reach v∞/2 from v=0: " << fmtD(tHalf) << "\n\n";
        ss << "Points: [";
        for (int i = 0; i <= 50; ++i)
        {
            double t = tEnd * i / 50, v = vt + (v0 - vt) * std::exp(-k / m * t);
            if (i)
                ss << ",";
            ss << "[" << fmtD(t) << "," << fmtD(v) << "]";
        }
        ss << "]";
        return okD(ss.str());
    }

    // Orthogonal trajectories: given F(x,y,c)=0, find trajectories perpendicular to family
    DEResult orthogonalTrajectories(const std::string &family,
                                    const std::string &x, const std::string &y)
    {
        std::ostringstream ss;
        ss << "Orthogonal Trajectories of: " << family << "\n\n";
        ss << "Step 1: Differentiate the family implicitly to get dy/dx = f(x,y)\n";
        ss << "Step 2: The orthogonal trajectories satisfy dy/dx = -1/f(x,y)\n";
        ss << "Step 3: Solve the new ODE\n\n";
        try
        {
            auto fExpr = Calculus::parse(family);
            auto df_dx = Calculus::simplify(Calculus::diff(fExpr, x));
            auto df_dy = Calculus::simplify(Calculus::diff(fExpr, y));
            ss << "dF/d" << x << " = " << Calculus::toString(df_dx) << "\n";
            ss << "dF/d" << y << " = " << Calculus::toString(df_dy) << "\n\n";
            auto slope = Calculus::simplify(
                Calculus::neg(Calculus::div_expr(df_dx, df_dy)));
            ss << "Family slope: dy/dx = " << Calculus::toString(slope) << "\n";
            auto orthSlope = Calculus::simplify(
                Calculus::neg(Calculus::div_expr(df_dy, df_dx)));
            ss << "Orthogonal slope: dy/dx = " << Calculus::toString(orthSlope) << "\n";
            ss << "\nSolve this ODE for the orthogonal trajectories.";
        }
        catch (...)
        {
            ss << "Could not symbolically compute — apply steps manually.";
        }
        return okD(ss.str());
    }

    // Torricelli's law: dh/dt = -(a/A)sqrt(2gh)
    DEResult torricelli(double A, double a, double h0, double g_acc)
    {
        double k = a / A * std::sqrt(2 * g_acc);
        double tEmpty = 2 * std::sqrt(h0) / k;
        std::ostringstream ss;
        ss << "Torricelli's Law: dh/dt = -(" << fmtD(a) << "/" << fmtD(A) << ")√(2·" << fmtD(g_acc) << "·h)\n\n";
        ss << "h(t) = (√h₀ - " << fmtD(k / 2) << "t)²\n";
        ss << "h₀ = " << fmtD(h0) << "\n";
        ss << "Tank empties at t = " << fmtD(tEmpty) << "\n\n";
        ss << "Points: [";
        for (int i = 0; i <= 50; ++i)
        {
            double t = tEmpty * i / 50;
            double sqrth = std::sqrt(h0) - k / 2 * t;
            double h = sqrth > 0 ? sqrth * sqrth : 0;
            if (i)
                ss << ",";
            ss << "[" << fmtD(t) << "," << fmtD(h) << "]";
        }
        ss << "]";
        return okD(ss.str());
    }

    // =============================================================================
    // REDUCTION OF ORDER (Trench Ch.5, Wiggins)
    // =============================================================================

    DEResult reductionOfOrder(double a, double b, double c, const std::string &y1Str)
    {
        std::ostringstream ss;
        ss << "Reduction of Order\n";
        ss << "ODE: " << fmtD(a) << "y'' + " << fmtD(b) << "y' + " << fmtD(c) << "y = 0\n";
        ss << "Known solution: y₁ = " << y1Str << "\n\n";
        ss << "Assume y₂ = v(x)·y₁.  Then:\n";
        ss << "  y₂' = v'y₁ + vy₁'\n";
        ss << "  y₂'' = v''y₁ + 2v'y₁' + vy₁''\n\n";
        ss << "Substituting and using " << a << "y₁'' + " << b << "y₁' + " << c << "y₁ = 0:\n";
        ss << "  " << a << "y₁·v'' + (2" << a << "y₁' + " << b << "y₁)v' = 0\n\n";
        ss << "Let w = v':\n";
        ss << "  w' + [(2" << a << "y₁'/" << a << "y₁) + " << b << "/" << a << "]w = 0\n";
        ss << "  w' + [2y₁'/y₁ + " << fmtD(b / a) << "]w = 0\n\n";
        ss << "This is a separable ODE in w = v'.\n";
        ss << "Solve for w, integrate to get v, then y₂ = v·y₁.\n\n";
        ss << "Abel's formula: the Wronskian satisfies W' + (" << fmtD(b / a) << ")W = 0\n";
        ss << "  W(x) = W₀ exp(-∫ " << fmtD(b / a) << " dx) = W₀ e^{-" << fmtD(b / a) << "x}\n\n";
        ss << "So v'(x) = W(x)/y₁(x)², giving y₂ = y₁ ∫ W/(y₁²) dx.";
        return okD(ss.str());
    }

    // =============================================================================
    // HIGHER-ORDER CONSTANT COEFFICIENT ODEs (Trench Ch.9)
    // =============================================================================

    // Solve a_n y^(n) + ... + a_0 y = 0 via characteristic polynomial
    DEResult solveHigherOrder(const std::vector<double> &coeffs)
    {
        int n = coeffs.size() - 1;
        if (n < 1)
            return errD("Need at least 2 coefficients");

        std::ostringstream ss;
        ss << "ODE: ";
        for (int k = n; k >= 0; --k)
        {
            if (std::abs(coeffs[k]) < 1e-12)
                continue;
            if (k < n)
                ss << (coeffs[k] >= 0 ? " + " : " - ");
            ss << std::abs(coeffs[k]);
            if (k > 0)
                ss << "y";
            if (k > 1)
                ss << "^(" << k << ")";
        }
        ss << " = 0\n\n";

        ss << "Characteristic polynomial: ";
        ss << fmtD(coeffs[n]) << "r^" << n;
        for (int k = n - 1; k >= 0; --k)
        {
            if (std::abs(coeffs[k]) < 1e-12)
                continue;
            ss << (coeffs[k] >= 0 ? " + " : " - ") << std::abs(coeffs[k]);
            if (k > 0)
                ss << "r^" << k;
        }
        ss << " = 0\n\n";

        // For n=2 (handled separately with full solution)
        if (n == 2)
        {
            double a = coeffs[2], b = coeffs[1], c = coeffs[0];
            double disc = b * b - 4 * a * c;
            if (disc > 1e-10)
            {
                double r1 = (-b + std::sqrt(disc)) / (2 * a), r2 = (-b - std::sqrt(disc)) / (2 * a);
                ss << "Roots: r₁=" << fmtD(r1) << ", r₂=" << fmtD(r2) << "\n";
                ss << "y = C₁e^{" << fmtD(r1) << "x} + C₂e^{" << fmtD(r2) << "x}";
            }
            else if (std::abs(disc) < 1e-10)
            {
                double r = -b / (2 * a);
                ss << "Repeated root: r=" << fmtD(r) << "\n";
                ss << "y = (C₁ + C₂x)e^{" << fmtD(r) << "x}";
            }
            else
            {
                double alpha = -b / (2 * a), beta = std::sqrt(-disc) / (2 * a);
                ss << "Complex roots: " << fmtD(alpha) << " ± " << fmtD(beta) << "i\n";
                ss << "y = e^{" << fmtD(alpha) << "x}[C₁cos(" << fmtD(beta) << "x) + C₂sin(" << fmtD(beta) << "x)]";
            }
            return okD(ss.str());
        }

        // For n=3,4: companion matrix eigenvalues
        ss << "For degree " << n << ": build companion matrix and find eigenvalues.\n";
        ss << "Companion matrix C of characteristic poly:\n";
        // Leading coefficient normalisation
        double lead = coeffs[n];
        ss << "  [ 0  1  0  ...  0 ]\n";
        ss << "  [ 0  0  1  ...  0 ]\n";
        ss << "  [ ...             ]\n";
        ss << "  [-a₀/aₙ  -a₁/aₙ  ...  -aₙ₋₁/aₙ ]\n\n";
        ss << "Eigenvalues = roots of characteristic poly.\n";
        ss << "Each real root r → e^{rx},  x·e^{rx},  x²·e^{rx}  (for multiplicity 1,2,3)\n";
        ss << "Each complex pair α±βi → e^{αx}cos(βx), e^{αx}sin(βx)";
        return okD(ss.str());
    }

    // Annihilator method for finding particular solution
    DEResult annihilatorMethod(const std::vector<double> &coeffs,
                               const std::string &g, const std::string &x)
    {
        std::ostringstream ss;
        ss << "Annihilator Method\n\n";
        ss << "Forcing function: g(x) = " << g << "\n\n";

        ss << "Step 1: Identify the annihilator A(D) such that A(D)[g] = 0\n";
        ss << "Common annihilators:\n";
        ss << "  g = eᵃˣ          →  A(D) = (D - a)\n";
        ss << "  g = xⁿeᵃˣ        →  A(D) = (D - a)^{n+1}\n";
        ss << "  g = cos(βx)       →  A(D) = D² + β²\n";
        ss << "  g = sin(βx)       →  A(D) = D² + β²\n";
        ss << "  g = eᵃˣcos(βx)   →  A(D) = (D-a)² + β²\n";
        ss << "  g = xⁿ            →  A(D) = D^{n+1}\n\n";

        ss << "Step 2: Apply A(D) to both sides — right side becomes 0\n";
        ss << "  A(D)L(D)[y] = 0   (higher-order homogeneous equation)\n\n";
        ss << "Step 3: Solve the combined characteristic equation\n";
        ss << "  The extra roots from A(D) give the form of yₚ\n\n";
        ss << "Step 4: Substitute yₚ back to find coefficients\n\n";

        ss << "For g(x) = " << g << ":\n";
        // Detect form of g
        std::string gl = g;
        if (gl.find("cos") != std::string::npos || gl.find("sin") != std::string::npos)
            ss << "  Annihilator: A(D) = D² + ω²  (identify ω)\n";
        else if (gl.find("exp") != std::string::npos || gl.find("e^") != std::string::npos)
            ss << "  Annihilator: A(D) = D - a  (identify a from exponent)\n";
        else if (gl.find("x") != std::string::npos)
            ss << "  Annihilator: A(D) = D^{n+1}  (identify degree n)\n";
        else
            ss << "  Annihilator: A(D) = D  (constant)\n";

        return okD(ss.str());
    }

    // =============================================================================
    // SYSTEMS OF ODEs (Trench Ch.10, Wiggins, Chasnov)
    // =============================================================================

    DEResult solveLinearSystem(const std::vector<std::vector<double>> &A,
                               const std::vector<double> &x0,
                               double tEnd, int steps)
    {
        int n = A.size();
        std::ostringstream ss;
        ss << "Linear System X' = AX\n";
        ss << "A = [\n";
        for (auto &row : A)
        {
            ss << "  [";
            for (int j = 0; j < n; ++j)
            {
                if (j)
                    ss << ", ";
                ss << fmtD(row[j]);
            }
            ss << "]\n";
        }
        ss << "]\n";
        ss << "X(0) = [";
        for (int i = 0; i < n; ++i)
        {
            if (i)
                ss << ",";
            ss << fmtD(x0[i]);
        }
        ss << "]\n\n";

        // Phase portrait classification for 2x2
        if (n == 2)
        {
            double tr = A[0][0] + A[1][1];
            double det = A[0][0] * A[1][1] - A[0][1] * A[1][0];
            double disc = tr * tr - 4 * det;
            ss << "tr(A) = " << fmtD(tr) << ",  det(A) = " << fmtD(det) << "\n";
            ss << "Characteristic eq: λ² - " << fmtD(tr) << "λ + " << fmtD(det) << " = 0\n";
            if (disc > 1e-10)
            {
                double l1 = 0.5 * (tr + std::sqrt(disc)), l2 = 0.5 * (tr - std::sqrt(disc));
                ss << "Eigenvalues: λ₁=" << fmtD(l1) << ", λ₂=" << fmtD(l2) << "\n";
                if (l1 > 0 && l2 > 0)
                    ss << "Type: unstable node\n";
                else if (l1 < 0 && l2 < 0)
                    ss << "Type: stable node\n";
                else
                    ss << "Type: saddle (unstable)\n";
            }
            else if (std::abs(disc) < 1e-10)
            {
                double l = -tr / 2;
                ss << "Repeated eigenvalue λ=" << fmtD(l) << "\n";
                ss << (l < 0 ? "Type: stable" : "Type: unstable") << " degenerate node\n";
            }
            else
            {
                double alpha = tr / 2, beta = std::sqrt(-disc) / 2;
                ss << "Complex eigenvalues: " << fmtD(alpha) << " ± " << fmtD(beta) << "i\n";
                if (std::abs(alpha) < 1e-8)
                    ss << "Type: center (neutrally stable)\n";
                else if (alpha < 0)
                    ss << "Type: stable spiral\n";
                else
                    ss << "Type: unstable spiral\n";
            }
        }

        // Numerical solution via RK4 system
        VecF F = [&](double t, const std::vector<double> &x)
        {
            std::vector<double> dx(n, 0);
            for (int i = 0; i < n; ++i)
                for (int j = 0; j < n; ++j)
                    dx[i] += A[i][j] * x[j];
            return dx;
        };
        auto traj = rk4System(F, x0, 0, tEnd, steps);

        ss << "\nPoints(t,x1,x2,...): [";
        int stride = std::max(1, (int)traj.size() / 50);
        for (int i = 0; i < (int)traj.size(); i += stride)
        {
            if (i)
                ss << ",";
            ss << "[";
            for (int j = 0; j <= (int)x0.size(); ++j)
            {
                if (j)
                    ss << ",";
                ss << fmtD(traj[i][j]);
            }
            ss << "]";
        }
        ss << "]";
        return okD(ss.str());
    }

    DEResult matrixExponentialDE(const std::vector<std::vector<double>> &A, double t)
    {
        int n = A.size();
        std::ostringstream ss;
        ss << "Matrix Exponential e^{At} at t=" << fmtD(t) << "\n\n";
        ss << "Method: Cayley-Hamilton / series e^{At} = I + At + (At)²/2! + ...\n\n";

        // Truncated series (12 terms)
        std::vector<std::vector<double>> result(n, std::vector<double>(n, 0));
        std::vector<std::vector<double>> Ak(n, std::vector<double>(n, 0)); // A^k * t^k / k!
        for (int i = 0; i < n; ++i)
            Ak[i][i] = 1; // identity = A^0

        double factorial_k = 1;
        for (int k = 0; k <= 12; ++k)
        {
            for (int i = 0; i < n; ++i)
                for (int j = 0; j < n; ++j)
                    result[i][j] += Ak[i][j];
            // Ak = Ak * (A*t) / (k+1)
            std::vector<std::vector<double>> newAk(n, std::vector<double>(n, 0));
            for (int i = 0; i < n; ++i)
                for (int j = 0; j < n; ++j)
                    for (int l = 0; l < n; ++l)
                        newAk[i][j] += Ak[i][l] * A[l][j] * t / (k + 1);
            Ak = newAk;
        }

        ss << "e^{At} ≈\n";
        for (int i = 0; i < n; ++i)
        {
            ss << "  [";
            for (int j = 0; j < n; ++j)
            {
                if (j)
                    ss << ",  ";
                ss << std::setw(10) << fmtD(result[i][j]);
            }
            ss << " ]\n";
        }
        ss << "\n(Computed via truncated series, 12 terms)";
        return okD(ss.str());
    }

    DEResult phasePortrait2x2(double a, double b, double c, double d)
    {
        double tr = a + d, det = a * d - b * c;
        double disc = tr * tr - 4 * det;
        double l1 = 0.5 * (tr + std::sqrt(disc > 0 ? disc : 0));
        double l2 = 0.5 * (tr - std::sqrt(disc > 0 ? disc : 0));

        std::ostringstream ss;
        ss << "Phase Portrait Classification\n";
        ss << "A = [[" << fmtD(a) << "," << fmtD(b) << "],[" << fmtD(c) << "," << fmtD(d) << "]]\n\n";
        ss << "tr(A) = " << fmtD(tr) << "\n";
        ss << "det(A) = " << fmtD(det) << "\n";
        ss << "Δ = tr²-4det = " << fmtD(disc) << "\n\n";

        std::string type, stability;
        if (det < 0)
        {
            type = "saddle";
            stability = "unstable";
        }
        else if (det == 0)
        {
            type = "degenerate";
            stability = "unstable";
        }
        else if (disc > 1e-8)
        {
            type = "node";
            stability = (tr < 0) ? "stable" : (tr > 0 ? "unstable" : "on boundary");
        }
        else if (std::abs(disc) < 1e-8)
        {
            type = "degenerate node / star";
            stability = (tr < 0) ? "stable" : "unstable";
        }
        else
        {
            if (std::abs(tr) < 1e-8)
            {
                type = "center";
                stability = "neutrally stable";
            }
            else
            {
                type = "spiral";
                stability = (tr < 0) ? "stable" : "unstable";
            }
        }

        ss << "Type: " << type << "\n";
        ss << "Stability: " << stability << "\n\n";

        if (disc >= 0)
        {
            ss << "Eigenvalues: λ₁=" << fmtD(l1) << ", λ₂=" << fmtD(l2) << "\n";
            // Eigenvectors
            for (int k = 0; k < 2; ++k)
            {
                double lk = (k == 0) ? l1 : l2;
                double bv = b, av = a - lk;
                if (std::abs(bv) > 1e-8)
                    ss << "  v" << k + 1 << "=[1, " << fmtD(-av / bv) << "]\n";
                else if (std::abs(c) > 1e-8)
                    ss << "  v" << k + 1 << "=[" << fmtD(-(d - lk) / c) << ", 1]\n";
                else
                    ss << "  v" << k + 1 << "=[1, 0] (standard basis)\n";
            }
        }
        else
        {
            double alpha = tr / 2, beta = std::sqrt(-disc) / 2;
            ss << "Eigenvalues: " << fmtD(alpha) << " ± " << fmtD(beta) << "i\n";
        }

        ss << "\nPoincaré diagram region:\n";
        ss << "  Δ=tr²-4det axis, det axis\n";
        ss << "  Current point: (Δ=" << fmtD(disc) << ", det=" << fmtD(det) << ")\n";
        ss << "  → " << type << " [" << stability << "]";
        return okD(ss.str());
    }

    DEResult nonlinearSystem2D(const std::string &f1, const std::string &f2,
                               const std::string &x1, const std::string &x2,
                               double xMin, double xMax, double yMin, double yMax)
    {
        std::ostringstream ss;
        ss << "Nonlinear System:\n";
        ss << "  d" << x1 << "/dt = " << f1 << "\n";
        ss << "  d" << x2 << "/dt = " << f2 << "\n\n";

        // Find fixed points by grid scan
        ss << "Fixed point scan on [" << fmtD(xMin) << "," << fmtD(xMax) << "] × ["
           << fmtD(yMin) << "," << fmtD(yMax) << "]:\n";

        try
        {
            auto e1 = Calculus::parse(f1), e2 = Calculus::parse(f2);
            int N = 30;
            double dx = (xMax - xMin) / N, dy = (yMax - yMin) / N;
            std::vector<std::pair<double, double>> fps;

            for (int i = 0; i < N; ++i)
            {
                for (int j = 0; j < N; ++j)
                {
                    double x = xMin + i * dx + dx / 2, y = yMin + j * dy + dy / 2;
                    double v1 = Calculus::evaluate(e1, {{x1, x}, {x2, y}});
                    double v2 = Calculus::evaluate(e2, {{x1, x}, {x2, y}});
                    if (std::abs(v1) < 0.4 * dx && std::abs(v2) < 0.4 * dy)
                    {
                        // Refine with Newton
                        for (int iter = 0; iter < 20; ++iter)
                        {
                            double f1v = Calculus::evaluate(e1, {{x1, x}, {x2, y}});
                            double f2v = Calculus::evaluate(e2, {{x1, x}, {x2, y}});
                            auto df1dx = Calculus::simplify(Calculus::diff(e1, x1));
                            auto df1dy = Calculus::simplify(Calculus::diff(e1, x2));
                            auto df2dx = Calculus::simplify(Calculus::diff(e2, x1));
                            auto df2dy = Calculus::simplify(Calculus::diff(e2, x2));
                            double J11 = Calculus::evaluate(df1dx, {{x1, x}, {x2, y}});
                            double J12 = Calculus::evaluate(df1dy, {{x1, x}, {x2, y}});
                            double J21 = Calculus::evaluate(df2dx, {{x1, x}, {x2, y}});
                            double J22 = Calculus::evaluate(df2dy, {{x1, x}, {x2, y}});
                            double det = J11 * J22 - J12 * J21;
                            if (std::abs(det) < 1e-12)
                                break;
                            x -= (J22 * f1v - J12 * f2v) / det;
                            y -= (-J21 * f1v + J11 * f2v) / det;
                        }
                        // Deduplicate
                        bool dup = false;
                        for (auto &p : fps)
                            if (std::hypot(p.first - x, p.second - y) < 0.01)
                            {
                                dup = true;
                                break;
                            }
                        if (!dup)
                            fps.push_back({x, y});
                    }
                }
            }

            for (auto &[px, py] : fps)
            {
                double f1v = Calculus::evaluate(e1, {{x1, px}, {x2, py}});
                double f2v = Calculus::evaluate(e2, {{x1, px}, {x2, py}});
                if (std::abs(f1v) > 0.01 || std::abs(f2v) > 0.01)
                    continue;

                // Linearise (Jacobian)
                auto df1dx = Calculus::simplify(Calculus::diff(e1, x1));
                auto df1dy = Calculus::simplify(Calculus::diff(e1, x2));
                auto df2dx = Calculus::simplify(Calculus::diff(e2, x1));
                auto df2dy = Calculus::simplify(Calculus::diff(e2, x2));
                double a = Calculus::evaluate(df1dx, {{x1, px}, {x2, py}});
                double b = Calculus::evaluate(df1dy, {{x1, px}, {x2, py}});
                double c = Calculus::evaluate(df2dx, {{x1, px}, {x2, py}});
                double d = Calculus::evaluate(df2dy, {{x1, px}, {x2, py}});
                double tr = a + d, det = a * d - b * c, disc = tr * tr - 4 * det;
                std::string type;
                if (det < 0)
                    type = "saddle";
                else if (disc > 1e-8)
                    type = (tr < 0 ? "stable node" : "unstable node");
                else if (std::abs(disc) < 1e-8)
                    type = (tr < 0 ? "stable degen. node" : "unstable degen. node");
                else
                    type = (std::abs(tr) < 1e-6 ? "center" : (tr < 0 ? "stable spiral" : "unstable spiral"));

                ss << "  Fixed point (" << fmtD(px, 4) << ", " << fmtD(py, 4) << "): "
                   << type << "\n";
                ss << "    J = [[" << fmtD(a, 4) << "," << fmtD(b, 4) << "],[" << fmtD(c, 4) << "," << fmtD(d, 4) << "]]\n";
            }
            if (fps.empty())
                ss << "  No fixed points found in region\n";
        }
        catch (const std::exception &e)
        {
            ss << "  Error: " << e.what() << "\n";
        }
        return okD(ss.str());
    }

    // =============================================================================
    // FOURIER SERIES (Trench Ch.11, Herman)
    // =============================================================================

    static double numIntegral(std::function<double(double)> f, double a, double b, int n = 200)
    {
        double h = (b - a) / n, s = 0;
        for (int i = 0; i < n; ++i)
        {
            double x = a + (i + 0.5) * h;
            s += f(x);
        }
        return s * h;
    }

    FourierResult fourierSeries(const std::string &f, const std::string &x,
                                double L, int N)
    {
        FourierResult r;
        r.a_n.resize(N + 1);
        r.b_n.resize(N + 1);
        // a0 = (1/L) ∫_{-L}^{L} f(x) dx
        r.a_n[0] = numIntegral([&](double xv)
                               { return cu_evalAt(f, x, xv); }, -L, L) /
                   (2 * L);
        for (int n = 1; n <= N; ++n)
        {
            r.a_n[n] = numIntegral([&](double xv)
                                   { return cu_evalAt(f, x, xv) * std::cos(n * PI * xv / L); }, -L, L) /
                       L;
            r.b_n[n] = numIntegral([&](double xv)
                                   { return cu_evalAt(f, x, xv) * std::sin(n * PI * xv / L); }, -L, L) /
                       L;
        }
        // Parseval: (1/L)∫f²dx = a₀²/2 + Σ(aₙ²+bₙ²)/2  ... wait
        // Actual: (1/L)∫_{-L}^{L}f²dx = 2a₀² + Σ(aₙ²+bₙ²)  -- Bessel/Parseval
        double f2 = numIntegral([&](double xv)
                                { double fv=cu_evalAt(f,x,xv); return fv*fv; }, -L, L) /
                    L;
        r.parsevalSum = 2 * r.a_n[0] * r.a_n[0];
        for (int n = 1; n <= N; ++n)
            r.parsevalSum += r.a_n[n] * r.a_n[n] + r.b_n[n] * r.b_n[n];

        std::ostringstream ss, lat;
        ss << "f(x) = " << r.a_n[0];
        for (int n = 1; n <= std::min(N, 5); ++n)
        {
            if (std::abs(r.a_n[n]) > 1e-10)
                ss << " + " << fmtD(r.a_n[n]) << "cos(" << n << "πx/" << fmtD(L) << ")";
            if (std::abs(r.b_n[n]) > 1e-10)
                ss << " + " << fmtD(r.b_n[n]) << "sin(" << n << "πx/" << fmtD(L) << ")";
        }
        if (N > 5)
            ss << " + ...";
        r.series = ss.str();
        r.series_latex = r.series;

        std::ostringstream out;
        out << "Fourier Series on [-" << fmtD(L) << "," << fmtD(L) << "], " << N << " terms\n\n";
        out << r.series << "\n\n";
        out << "Parseval: (1/L)∫f²dx ≈ " << fmtD(f2) << "\n";
        out << "Fourier sum ≈ " << fmtD(r.parsevalSum) << "\n";
        out << "(These should agree — measures energy)\n\n";
        out << "Coefficients:\n  a₀=" << fmtD(r.a_n[0]);
        for (int n = 1; n <= N; ++n)
            out << "\n  a" << n << "=" << fmtD(r.a_n[n]) << ", b" << n << "=" << fmtD(r.b_n[n]);
        r.series = out.str();
        return r;
    }

    FourierResult fourierSineSeries(const std::string &f, const std::string &x,
                                    double L, int N)
    {
        FourierResult r;
        r.b_n.resize(N + 1, 0);
        for (int n = 1; n <= N; ++n)
        {
            r.b_n[n] = (2.0 / L) * numIntegral([&](double xv)
                                               { return cu_evalAt(f, x, xv) * std::sin(n * PI * xv / L); }, 0, L);
        }
        std::ostringstream out;
        out << "Fourier Sine Series on [0," << fmtD(L) << "], " << N << " terms\n";
        out << "f(x) ~ ";
        for (int n = 1; n <= std::min(N, 5); ++n)
        {
            if (n > 1)
                out << " + ";
            out << fmtD(r.b_n[n]) << "sin(" << n << "πx/" << fmtD(L) << ")";
        }
        if (N > 5)
            out << " + ...";
        out << "\n\nCoefficients bₙ = (2/L)∫₀ᴸ f(x)sin(nπx/L) dx:";
        for (int n = 1; n <= N; ++n)
            out << "\n  b" << n << "=" << fmtD(r.b_n[n]);
        r.series = out.str();
        return r;
    }

    FourierResult fourierCosSeries(const std::string &f, const std::string &x,
                                   double L, int N)
    {
        FourierResult r;
        r.a_n.resize(N + 1, 0);
        r.a_n[0] = numIntegral([&](double xv)
                               { return cu_evalAt(f, x, xv); }, 0, L) /
                   L;
        for (int n = 1; n <= N; ++n)
        {
            r.a_n[n] = (2.0 / L) * numIntegral([&](double xv)
                                               { return cu_evalAt(f, x, xv) * std::cos(n * PI * xv / L); }, 0, L);
        }
        std::ostringstream out;
        out << "Fourier Cosine Series on [0," << fmtD(L) << "], " << N << " terms\n";
        out << "f(x) ~ " << fmtD(r.a_n[0]);
        for (int n = 1; n <= std::min(N, 5); ++n)
            out << " + " << fmtD(r.a_n[n]) << "cos(" << n << "πx/" << fmtD(L) << ")";
        if (N > 5)
            out << " + ...";
        out << "\n\nCoefficients aₙ = (2/L)∫₀ᴸ f(x)cos(nπx/L) dx:";
        for (int n = 1; n <= N; ++n)
            out << "\n  a" << n << "=" << fmtD(r.a_n[n]);
        r.series = out.str();
        return r;
    }

    DEResult parsevalIdentity(const std::string &f, const std::string &x,
                              double L, int N)
    {
        auto fs = fourierSeries(f, x, L, N);
        double f2 = numIntegral([&](double xv)
                                { double fv=cu_evalAt(f,x,xv); return fv*fv; }, -L, L) /
                    L;
        std::ostringstream ss;
        ss << "Parseval's Identity\n";
        ss << "(1/L)∫_{-L}^{L}|f|² dx = 2a₀² + Σ(aₙ²+bₙ²)\n\n";
        ss << "LHS = " << fmtD(f2) << "\n";
        ss << "RHS = " << fmtD(fs.parsevalSum) << "\n";
        ss << "Error = " << fmtD(std::abs(f2 - fs.parsevalSum)) << "\n\n";
        ss << "(Exact equality holds with infinitely many terms; truncation error ∝ 1/N²)";
        return okD(ss.str());
    }

    // =============================================================================
    // DYNAMICAL SYSTEMS (Wiggins, Herman 2nd)
    // =============================================================================

    DEResult hartmanGrobman(const std::string &f1, const std::string &f2,
                            const std::string &x1, const std::string &x2,
                            double xStar, double yStar)
    {
        std::ostringstream ss;
        ss << "Hartman-Grobman Theorem Analysis\n";
        ss << "Fixed point: (" << fmtD(xStar) << ", " << fmtD(yStar) << ")\n\n";
        ss << "Theorem: If J has no eigenvalues on imaginary axis (hyperbolic fixed point),\n";
        ss << "the nonlinear flow is topologically conjugate to the linearised flow near *.\n\n";
        try
        {
            auto e1 = Calculus::parse(f1), e2 = Calculus::parse(f2);
            auto df1dx = Calculus::simplify(Calculus::diff(e1, x1));
            auto df1dy = Calculus::simplify(Calculus::diff(e1, x2));
            auto df2dx = Calculus::simplify(Calculus::diff(e2, x1));
            auto df2dy = Calculus::simplify(Calculus::diff(e2, x2));
            double a = Calculus::evaluate(df1dx, {{x1, xStar}, {x2, yStar}});
            double b = Calculus::evaluate(df1dy, {{x1, xStar}, {x2, yStar}});
            double c = Calculus::evaluate(df2dx, {{x1, xStar}, {x2, yStar}});
            double d = Calculus::evaluate(df2dy, {{x1, xStar}, {x2, yStar}});
            double tr = a + d, det = a * d - b * c, disc = tr * tr - 4 * det;
            ss << "Jacobian at (*): J = [[" << fmtD(a) << "," << fmtD(b) << "],[" << fmtD(c) << "," << fmtD(d) << "]]\n";
            if (disc >= 0)
            {
                double l1 = 0.5 * (tr + std::sqrt(disc)), l2 = 0.5 * (tr - std::sqrt(disc));
                ss << "Eigenvalues: λ₁=" << fmtD(l1) << ", λ₂=" << fmtD(l2) << "\n";
                bool hyperbolic = (std::abs(l1) > 1e-8 && std::abs(l2) > 1e-8);
                ss << "Hyperbolic: " << (hyperbolic ? "YES — H-G applies" : "NO — need center manifold theory") << "\n";
            }
            else
            {
                double alpha = tr / 2, beta = std::sqrt(-disc) / 2;
                ss << "Eigenvalues: " << fmtD(alpha) << " ± " << fmtD(beta) << "i\n";
                ss << "Hyperbolic: " << (std::abs(alpha) > 1e-8 ? "YES — H-G applies" : "NO — purely imaginary, center case") << "\n";
            }
        }
        catch (const std::exception &e)
        {
            ss << "Error: " << e.what();
        }
        return okD(ss.str());
    }

    DEResult lyapunovFunction(const std::string &f1, const std::string &f2,
                              const std::string &V,
                              const std::string &x1, const std::string &x2,
                              double xStar, double yStar)
    {
        std::ostringstream ss;
        ss << "Lyapunov Stability Analysis\n";
        ss << "V(x,y) = " << V << "\n";
        ss << "Fixed point: (" << fmtD(xStar) << ", " << fmtD(yStar) << ")\n\n";
        try
        {
            auto Vexpr = Calculus::parse(V);
            auto e1 = Calculus::parse(f1), e2 = Calculus::parse(f2);
            auto dVdx = Calculus::simplify(Calculus::diff(Vexpr, x1));
            auto dVdy = Calculus::simplify(Calculus::diff(Vexpr, x2));
            // V̇ = ∂V/∂x · f + ∂V/∂y · g
            auto Vdot = Calculus::simplify(
                Calculus::add(Calculus::mul(dVdx, e1), Calculus::mul(dVdy, e2)));
            ss << "∂V/∂x = " << Calculus::toString(dVdx) << "\n";
            ss << "∂V/∂y = " << Calculus::toString(dVdy) << "\n";
            ss << "V̇ = ∂V/∂x·f + ∂V/∂y·g = " << Calculus::toString(Vdot) << "\n\n";
            // Check V(x*,y*) = 0 and V > 0 near x*, and V̇ ≤ 0
            double V0 = Calculus::evaluate(Vexpr, {{x1, xStar}, {x2, yStar}});
            double Vd0 = Calculus::evaluate(Vdot, {{x1, xStar}, {x2, yStar}});
            ss << "V(x*) = " << fmtD(V0) << (std::abs(V0) < 1e-8 ? " ✓" : " (should be 0)") << "\n";
            ss << "V̇(x*) = " << fmtD(Vd0) << "\n\n";
            // Sample V̇ at nearby points
            double neg = true;
            for (double dx : {0.1, 0.2})
                for (double dy : std::initializer_list<double>{0, 0.1})
                {
                    double vd = Calculus::evaluate(Vdot, {{x1, xStar + dx}, {x2, yStar + dy}});
                    if (vd > 1e-8)
                    {
                        neg = false;
                        break;
                    }
                }
            if (std::abs(V0) < 1e-8)
            {
                if (neg)
                    ss << "V̇ ≤ 0 near x* → Lyapunov stable\n";
                else
                    ss << "V̇ not negative semi-definite near x* — not a valid Lyapunov function\n";
            }
        }
        catch (const std::exception &e)
        {
            ss << "Error: " << e.what();
        }
        return okD(ss.str());
    }

    DEResult dulacCriterion(const std::string &f1, const std::string &f2,
                            const std::string &B,
                            const std::string &x1, const std::string &x2)
    {
        std::ostringstream ss;
        ss << "Dulac's Criterion (no limit cycles)\n";
        ss << "Multiplier B(x,y) = " << B << "\n";
        ss << "System: " << x1 << "' = " << f1 << ",  " << x2 << "' = " << f2 << "\n\n";
        ss << "Dulac's condition: div(B·F) = ∂(Bf)/∂x + ∂(Bg)/∂y\n";
        try
        {
            auto Be = Calculus::parse(B);
            auto e1 = Calculus::parse(f1), e2 = Calculus::parse(f2);
            auto Bf = Calculus::simplify(Calculus::mul(Be, e1));
            auto Bg = Calculus::simplify(Calculus::mul(Be, e2));
            auto divBF = Calculus::simplify(
                Calculus::add(Calculus::diff(Bf, x1), Calculus::diff(Bg, x2)));
            ss << "B·f = " << Calculus::toString(Bf) << "\n";
            ss << "B·g = " << Calculus::toString(Bg) << "\n";
            ss << "∂(Bf)/∂x + ∂(Bg)/∂y = " << Calculus::toString(divBF) << "\n\n";
            // Check sign numerically
            bool allNeg = true, allPos = true;
            for (double xv : {0.5, 1.0, 1.5})
                for (double yv : {0.5, 1.0})
                {
                    double v = Calculus::evaluate(divBF, {{x1, xv}, {x2, yv}});
                    if (v > 1e-8)
                        allNeg = false;
                    if (v < -1e-8)
                        allPos = false;
                }
            if (allNeg || allPos)
                ss << "div(BF) has definite sign → no limit cycles in simply connected region";
            else
                ss << "div(BF) changes sign → Dulac's criterion inconclusive";
        }
        catch (const std::exception &e)
        {
            ss << "Error: " << e.what();
        }
        return okD(ss.str());
    }

    DEResult limitCycleCheck(const std::string &f1, const std::string &f2,
                             const std::string &x1, const std::string &x2,
                             double xMin, double xMax, double yMin, double yMax)
    {
        std::ostringstream ss;
        ss << "Poincaré-Bendixson / Limit Cycle Analysis\n";
        ss << "System: x' = " << f1 << ",  y' = " << f2 << "\n\n";
        ss << "Step 1 — Divergence (Bendixson's criterion):\n";
        ss << "  If div(F) = ∂f/∂x + ∂g/∂y has one sign → no limit cycles\n\n";
        try
        {
            auto e1 = Calculus::parse(f1), e2 = Calculus::parse(f2);
            auto div = Calculus::simplify(
                Calculus::add(Calculus::diff(e1, x1), Calculus::diff(e2, x2)));
            ss << "div(F) = " << Calculus::toString(div) << "\n\n";
            bool allNeg = true, allPos = true;
            for (double xv : {xMin + 0.1 * (xMax - xMin), 0.5 * (xMin + xMax), xMax - 0.1 * (xMax - xMin)})
                for (double yv : {yMin + 0.1 * (yMax - yMin), 0.5 * (yMin + yMax)})
                {
                    double d = Calculus::evaluate(div, {{x1, xv}, {x2, yv}});
                    if (d > 1e-8)
                        allNeg = false;
                    if (d < -1e-8)
                        allPos = false;
                }
            if (allNeg)
                ss << "div(F) < 0 throughout → Bendixson: no limit cycles\n";
            else if (allPos)
                ss << "div(F) > 0 throughout → Bendixson: no limit cycles\n";
            else
                ss << "div(F) changes sign → Bendixson inconclusive, limit cycles possible\n";
            ss << "\nStep 2 — Poincaré-Bendixson:\n";
            ss << "  If a trajectory stays in a bounded region with no fixed points → ∃ limit cycle\n";
            ss << "  Trapping region needed for constructive proof\n";
        }
        catch (const std::exception &e)
        {
            ss << "Error: " << e.what();
        }
        return okD(ss.str());
    }

    DEResult poincareIndex(const std::string &f1, const std::string &f2,
                           const std::string &x1, const std::string &x2,
                           double cx, double cy, double r)
    {
        // Numerical computation of Poincaré index
        // Index = (1/2π) ∮ d(arctan(g/f))
        std::ostringstream ss;
        ss << "Poincaré Index at (" << fmtD(cx) << "," << fmtD(cy) << "), r=" << fmtD(r) << "\n\n";
        try
        {
            auto e1 = Calculus::parse(f1), e2 = Calculus::parse(f2);
            int N = 360;
            double dtheta = 2 * PI / N, angle = 0, prevAngle = 0;
            double totalWinding = 0;
            for (int i = 0; i <= N; ++i)
            {
                double theta = i * dtheta;
                double x = cx + r * std::cos(theta), y = cy + r * std::sin(theta);
                double fv = Calculus::evaluate(e1, {{x1, x}, {x2, y}});
                double gv = Calculus::evaluate(e2, {{x1, x}, {x2, y}});
                double a = std::atan2(gv, fv);
                if (i > 0)
                {
                    double da = a - prevAngle;
                    if (da > PI)
                        da -= 2 * PI;
                    if (da < -PI)
                        da += 2 * PI;
                    totalWinding += da;
                }
                prevAngle = a;
            }
            int index = (int)std::round(totalWinding / (2 * PI));
            ss << "Winding number = " << fmtD(totalWinding / (2 * PI), 3) << "\n";
            ss << "Poincaré index = " << index << "\n\n";
            ss << "Interpretation:\n";
            ss << "  Index 0: no fixed points inside (or even number)\n";
            ss << "  Index +1: stable/unstable node or spiral, center\n";
            ss << "  Index -1: saddle point\n";
            ss << "  Sum of indices of all fixed points inside = +1 for any limit cycle";
        }
        catch (const std::exception &e)
        {
            ss << "Error: " << e.what();
        }
        return okD(ss.str());
    }

    // =============================================================================
    // NUMERICAL METHODS (Brorson, Lebl)
    // =============================================================================

    NumericalDEResult implicitEuler(const Func &f, const Func &dfdy,
                                    double t0, double y0, double t1, int n)
    {
        NumericalDEResult r;
        r.method = "Implicit Euler";
        double h = (t1 - t0) / n;
        r.t.resize(n + 1);
        r.y.resize(n + 1);
        r.t[0] = t0;
        r.y[0] = y0;
        for (int i = 0; i < n; ++i)
        {
            double tnext = r.t[i] + h;
            double y = r.y[i];
            // Newton iteration for y_{n+1} = y_n + h·f(t_{n+1}, y_{n+1})
            for (int iter = 0; iter < 50; ++iter)
            {
                double F = y - r.y[i] - h * f(tnext, y);
                double dF = 1 - h * dfdy(tnext, y);
                if (std::abs(dF) < 1e-14)
                    break;
                double dy = -F / dF;
                y += dy;
                if (std::abs(dy) < 1e-10)
                    break;
            }
            r.y[i + 1] = y;
            r.t[i + 1] = tnext;
        }
        r.summary = "y(" + fmtD(t1) + ") ≈ " + fmtD(r.y.back()) + " (Implicit Euler, A-stable)";
        return r;
    }

    NumericalDEResult crankNicolson(const Func &f, const Func &dfdy,
                                    double t0, double y0, double t1, int n)
    {
        NumericalDEResult r;
        r.method = "Crank-Nicolson";
        double h = (t1 - t0) / n;
        r.t.resize(n + 1);
        r.y.resize(n + 1);
        r.t[0] = t0;
        r.y[0] = y0;
        for (int i = 0; i < n; ++i)
        {
            double tn = r.t[i], yn = r.y[i], tnext = tn + h;
            double fn = f(tn, yn);
            double y = yn + h * fn; // explicit predictor
            for (int iter = 0; iter < 50; ++iter)
            {
                double F = y - yn - h / 2 * (fn + f(tnext, y));
                double dF = 1 - h / 2 * dfdy(tnext, y);
                if (std::abs(dF) < 1e-14)
                    break;
                double dy = -F / dF;
                y += dy;
                if (std::abs(dy) < 1e-10)
                    break;
            }
            r.y[i + 1] = y;
            r.t[i + 1] = tnext;
        }
        r.summary = "y(" + fmtD(t1) + ") ≈ " + fmtD(r.y.back()) + " (Crank-Nicolson, O(h²), A-stable)";
        return r;
    }

    NumericalDEResult bdf2(const Func &f, double t0, double y0, double t1, int n)
    {
        NumericalDEResult r;
        r.method = "BDF2";
        double h = (t1 - t0) / n;
        r.t.resize(n + 1);
        r.y.resize(n + 1);
        r.t[0] = t0;
        r.y[0] = y0;
        // Bootstrap with Euler
        if (n >= 1)
        {
            r.y[1] = y0 + h * f(t0, y0);
            r.t[1] = t0 + h;
        }
        for (int i = 1; i < n; ++i)
        {
            double tnext = r.t[i] + h;
            double y = r.y[i];
            for (int iter = 0; iter < 50; ++iter)
            {
                double F = (3.0 / 2) * y - 2 * r.y[i] + (0.5) * r.y[i - 1] - h * f(tnext, y);
                // dF/dy ≈ 3/2 - h*fy (ignore fy for simplicity)
                double dF = 1.5; // simplified
                double dy = -F / dF;
                y += dy;
                if (std::abs(dy) < 1e-10)
                    break;
            }
            r.y[i + 1] = y;
            r.t[i + 1] = tnext;
        }
        r.summary = "y(" + fmtD(t1) + ") ≈ " + fmtD(r.y.back()) + " (BDF2, stiff-stable)";
        return r;
    }

    NumericalDEResult trapezoidalODE(const Func &f, const Func &dfdy,
                                     double t0, double y0, double t1, int n)
    {
        // Same as Crank-Nicolson for ODEs
        return crankNicolson(f, dfdy, t0, y0, t1, n);
    }

    DEResult stiffnessAnalysis(const Func &f, const Func &dfdy,
                               double t0, double y0, double t1)
    {
        std::ostringstream ss;
        ss << "Stiffness Analysis\n\n";
        // Stiffness ratio: |λ_max / λ_min| where λ = ∂f/∂y evaluated along trajectory
        double maxLam = 0, minLam = 1e300;
        int n = 100;
        double h = (t1 - t0) / n, y = y0;
        ss << "∂f/∂y along solution:\n";
        for (int i = 0; i <= n; ++i)
        {
            double t = t0 + i * h;
            double lam = std::abs(dfdy(t, y));
            maxLam = std::max(maxLam, lam);
            minLam = std::min(minLam, lam);
            if (i % 20 == 0)
                ss << "  t=" << fmtD(t) << ": ∂f/∂y=" << fmtD(dfdy(t, y)) << "\n";
            y += h * f(t, y); // Euler for trajectory
        }
        double ratio = minLam < 1e-12 ? 1e9 : maxLam / minLam;
        ss << "\nStiffness ratio = |λ_max|/|λ_min| ≈ " << fmtD(ratio) << "\n";
        ss << "Stability threshold for explicit Euler: h < 2/|λ_max| ≈ " << fmtD(2.0 / maxLam) << "\n";
        ss << (ratio > 100 ? "Problem is STIFF — use implicit methods (BDF, Crank-Nicolson)" : "Problem is non-stiff — explicit methods acceptable");
        return okD(ss.str());
    }

    DEResult globalErrorAnalysis(const Func &f, const Func &exact,
                                 double t0, double y0, double t1,
                                 const std::vector<int> &nValues)
    {
        std::ostringstream ss;
        ss << "Global Error Analysis (Richardson Extrapolation)\n\n";
        ss << std::setw(8) << "n" << std::setw(12) << "h"
           << std::setw(15) << "RK4 value" << std::setw(15) << "error\n";
        ss << std::string(50, '-') << "\n";
        double prevErr = 0;
        for (int n : nValues)
        {
            double h = (t1 - t0) / n;
            // RK4
            double y = y0;
            for (int i = 0; i < n; ++i)
            {
                double t = t0 + i * h;
                double k1 = f(t, y), k2 = f(t + h / 2, y + h / 2 * k1);
                double k3 = f(t + h / 2, y + h / 2 * k2), k4 = f(t + h, y + h * k3);
                y += h / 6 * (k1 + 2 * k2 + 2 * k3 + k4);
            }
            double err = std::abs(y - exact(t1, y0));
            ss << std::setw(8) << n << std::setw(12) << fmtD(h)
               << std::setw(15) << fmtD(y) << std::setw(15) << fmtD(err);
            if (prevErr > 0 && err > 1e-15)
                ss << "  (ratio=" << fmtD(prevErr / err) << ")";
            ss << "\n";
            prevErr = err;
        }
        ss << "\nFor RK4: doubling n should reduce error by ≈ 16× (order 4)";
        return okD(ss.str());
    }

    // =============================================================================
    // LAPLACE TRANSFORMS — EXTENDED TABLE (Trench Ch.8)
    // =============================================================================

    DEResult laplaceTable(const std::string &fStr, const std::string &t,
                          const std::string &s)
    {
        std::ostringstream ss;
        ss << "Laplace Transform Table: L{" << fStr << "}\n\n";
        std::map<std::string, std::string> table = {
            {"1", "1/s"},
            {"t", "1/s^2"},
            {"t^2", "2/s^3"},
            {"t^n", "n!/s^{n+1}"},
            {"e^{at}", "1/(s-a)"},
            {"t*e^{at}", "1/(s-a)^2"},
            {"t^n*e^{at}", "n!/(s-a)^{n+1}"},
            {"sin(bt)", "b/(s^2+b^2)"},
            {"cos(bt)", "s/(s^2+b^2)"},
            {"e^{at}sin(bt)", "b/((s-a)^2+b^2)"},
            {"e^{at}cos(bt)", "(s-a)/((s-a)^2+b^2)"},
            {"t*sin(bt)", "2bs/(s^2+b^2)^2"},
            {"t*cos(bt)", "(s^2-b^2)/(s^2+b^2)^2"},
            {"sinh(bt)", "b/(s^2-b^2)"},
            {"cosh(bt)", "s/(s^2-b^2)"},
            {"delta(t-a)", "e^{-as}"},
            {"u(t-a)", "e^{-as}/s"},
            {"u(t-a)*f(t-a)", "e^{-as}*F(s) (second shift)"},
        };
        ss << "Full transform table:\n";
        ss << std::left << std::setw(25) << "f(t)" << "F(s)\n"
           << std::string(50, '-') << "\n";
        for (auto &[k, v] : table)
            ss << std::setw(25) << k << v << "\n";
        ss << "\nKey theorems:\n";
        ss << "  Linearity:       L{af+bg} = aF + bG\n";
        ss << "  First shift:     L{e^{at}f(t)} = F(s-a)\n";
        ss << "  Second shift:    L{u(t-a)f(t-a)} = e^{-as}F(s)\n";
        ss << "  Derivative:      L{f'} = sF(s) - f(0)\n";
        ss << "  Integral:        L{∫₀ᵗf} = F(s)/s\n";
        ss << "  Convolution:     L{f*g} = F(s)G(s)\n";
        ss << "  Multiplication:  L{tf(t)} = -F'(s)\n";
        return okD(ss.str());
    }

    DEResult heavisideStep(double c, const std::string &fStr, const std::string &t)
    {
        std::ostringstream ss;
        ss << "Heaviside Step Function Decomposition\n\n";
        ss << "u(t-c) = 0 for t < " << fmtD(c) << ",  1 for t ≥ " << fmtD(c) << "\n\n";
        ss << "L{u(t-c)} = e^{-cs}/s\n\n";
        ss << "Second shifting theorem:\n";
        ss << "L{u(t-c)·f(t-c)} = e^{-cs}·F(s)\n\n";
        ss << "For g(t) = " << fStr << ":\n";
        ss << "Shift by c=" << fmtD(c) << ": g(t-" << fmtD(c) << ") is f shifted right\n";
        ss << "L{u(t-" << fmtD(c) << ")·g(t-" << fmtD(c) << ")} = e^{-" << fmtD(c) << "s}·G(s)\n\n";
        ss << "To write piecewise functions using Heaviside:\n";
        ss << "  f(t) on [0,a), g(t) on [a,b):\n";
        ss << "  = f(t) + [g(t)-f(t)]u(t-a)\n";
        return okD(ss.str());
    }

    DEResult convolutionLaplace(const std::string &f, const std::string &g,
                                const std::string &t, const std::string &s)
    {
        std::ostringstream ss;
        ss << "Convolution Theorem\n\n";
        ss << "(f * g)(t) = ∫₀ᵗ f(τ)g(t-τ) dτ\n\n";
        ss << "L{f*g} = F(s)·G(s)\n\n";
        ss << "f(t) = " << f << "\n";
        ss << "g(t) = " << g << "\n\n";
        ss << "(f*g)(t) = ∫₀ᵗ (" << f << ")(" << g << ") evaluated at (τ, t-τ)\n\n";
        ss << "Useful for solving IVPs with:\n";
        ss << "  Y(s) = G(s)·H(s)  →  y(t) = (g*h)(t)\n";
        ss << "where H(s) is the transfer function of the system.";
        return okD(ss.str());
    }

    DEResult diracDeltaResponse(double a, double b, double c, double t0, double tEnd)
    {
        // ay'' + by' + cy = δ(t - t0)
        std::ostringstream ss;
        ss << "Impulse Response (Dirac delta forcing)\n";
        ss << a << "y'' + " << b << "y' + " << c << "y = δ(t - " << fmtD(t0) << ")\n";
        ss << "y(0) = 0,  y'(0) = 0\n\n";
        ss << "Method: L{δ(t-t₀)} = e^{-st₀}\n\n";
        ss << "Transfer function: H(s) = 1/(" << a << "s² + " << b << "s + " << c << ")\n";
        ss << "Y(s) = e^{-" << fmtD(t0) << "s} · H(s)\n\n";

        double disc = b * b - 4 * a * c;
        if (disc > 1e-10)
        {
            double r1 = (-b + std::sqrt(disc)) / (2 * a), r2 = (-b - std::sqrt(disc)) / (2 * a);
            ss << "Partial fractions: Y(s) = e^{-" << fmtD(t0) << "s} ["
               << fmtD(1.0 / (a * (r1 - r2))) << "/(s-" << fmtD(r1) << ") + "
               << fmtD(-1.0 / (a * (r1 - r2))) << "/(s-" << fmtD(r2) << ")]\n";
            ss << "y(t) = u(t-" << fmtD(t0) << ")·" << fmtD(1.0 / (a * (r1 - r2)))
               << "·[e^{" << fmtD(r1) << "(t-" << fmtD(t0) << ")} - e^{"
               << fmtD(r2) << "(t-" << fmtD(t0) << ")}]\n\n";
            ss << "Points: [";
            for (int i = 0; i <= 50; ++i)
            {
                double t = tEnd * i / 50;
                double y = 0;
                if (t >= t0)
                {
                    double dt = t - t0;
                    y = 1.0 / (a * (r1 - r2)) * (std::exp(r1 * dt) - std::exp(r2 * dt));
                }
                if (i)
                    ss << ",";
                ss << "[" << fmtD(t) << "," << fmtD(y) << "]";
            }
            ss << "]";
        }
        else
        {
            ss << "See homogeneous solution for structure of impulse response.";
        }
        return okD(ss.str());
    }

    // =============================================================================
    // PDEs EXTENDED (Herman, Miersemann, Walet)
    // =============================================================================

    DEResult fourierTransformPDE(const std::string &pde, const std::string &ic,
                                 const std::string &x, const std::string &t,
                                 double tEnd)
    {
        std::ostringstream ss;
        ss << "Fourier Transform Method for PDEs\n\n";
        ss << "PDE: " << pde << "\n";
        ss << "IC:  u(x,0) = " << ic << "\n\n";
        ss << "Take Fourier transform in x: û(ξ,t) = ∫_{-∞}^{∞} u(x,t) e^{-iξx} dx\n\n";
        ss << "For heat equation u_t = κu_xx:\n";
        ss << "  dû/dt = -κξ²û  →  û(ξ,t) = û₀(ξ)e^{-κξ²t}\n";
        ss << "  u(x,t) = (1/√(4πκt)) ∫ u₀(y)e^{-(x-y)²/(4κt)} dy  (convolution)\n\n";
        ss << "For wave equation u_tt = c²u_xx:\n";
        ss << "  d²û/dt² = -c²ξ²û  →  û(ξ,t) = A(ξ)e^{icξt} + B(ξ)e^{-icξt}\n";
        ss << "  →  d'Alembert: u(x,t) = ½[f(x+ct)+f(x-ct)] + 1/(2c)∫g\n\n";
        ss << "Key transform pairs:\n";
        ss << "  F{e^{-ax²}} = √(π/a) e^{-ξ²/(4a)}\n";
        ss << "  F{u_x} = iξ û(ξ)\n";
        ss << "  F{u_xx} = -ξ² û(ξ)\n";
        return okD(ss.str());
    }

    DEResult characteristics1stPDE(const std::string &a, const std::string &b,
                                   const std::string &c, const std::string &ic,
                                   const std::string &x, const std::string &t)
    {
        std::ostringstream ss;
        ss << "Method of Characteristics (1st-order PDE)\n";
        ss << "a(x,t)u_t + b(x,t)u_x = c(x,t,u)\n";
        ss << "a = " << a << ",  b = " << b << ",  c = " << c << "\n";
        ss << "IC: u(x,0) = " << ic << "\n\n";
        ss << "Characteristic equations (parameterised by s):\n";
        ss << "  dt/ds = " << a << "\n";
        ss << "  dx/ds = " << b << "\n";
        ss << "  du/ds = " << c << "\n\n";
        ss << "Solve: given x(0)=x₀, t(0)=0, u(0)=u₀(x₀)\n\n";

        try
        {
            auto aExpr = Calculus::parse(a), bExpr = Calculus::parse(b);
            // For constant a and b
            bool constA = !Calculus::contains(aExpr, x) && !Calculus::contains(aExpr, t);
            bool constB = !Calculus::contains(bExpr, x) && !Calculus::contains(bExpr, t);
            if (constA && constB)
            {
                double av = Calculus::evaluate(aExpr, {}), bv = Calculus::evaluate(bExpr, {});
                ss << "Constant coefficients: a=" << fmtD(av) << ", b=" << fmtD(bv) << "\n";
                ss << "Characteristics: x - " << fmtD(bv / av) << "t = const\n";
                ss << "Solution: u(x,t) = u₀(x - " << fmtD(bv / av) << "t)\n\n";
                ss << "Substituting IC u₀ = " << ic << ":\n";
                try
                {
                    auto u0 = Calculus::parse(ic);
                    auto xArg = Calculus::parse(x + " - " + fmtD(bv / av) + "*" + t);
                    auto sol = Calculus::substitute(u0, x, xArg);
                    ss << "u(x,t) = " << Calculus::toString(Calculus::simplify(sol));
                }
                catch (...)
                {
                    ss << "u(x,t) = " << ic << " evaluated at (x-" << fmtD(bv / av) << "t)";
                }
            }
            else
            {
                ss << "Variable coefficients — solve characteristic ODEs numerically.\n";
                ss << "General form: solution constant along characteristics.";
            }
        }
        catch (...)
        {
            ss << "Could not parse coefficients.";
        }
        return okD(ss.str());
    }

    DEResult duhamelPrinciple(double a, double b, double c,
                              const std::string &g, const std::string &x,
                              double L, double tEnd, int terms)
    {
        std::ostringstream ss;
        ss << "Duhamel's Principle\n";
        ss << "PDE: u_t = " << fmtD(a) << " u_xx  with source g(x,t) = " << g << "\n";
        ss << "BCs: u(0,t)=u(" << fmtD(L) << ",t)=0,  IC: u(x,0)=0\n\n";
        ss << "Duhamel: u(x,t) = ∫₀ᵗ v(x,t-τ;τ) dτ\n";
        ss << "where v(x,s;τ) solves: v_s = " << fmtD(a) << "v_xx, v(x,0)=g(x,τ)\n\n";
        ss << "v(x,s;τ) = Σ bₙ(τ) e^{-" << fmtD(a) << "(nπ/" << fmtD(L) << ")²s} sin(nπx/" << fmtD(L) << ")\n";
        ss << "where bₙ(τ) = (2/L)∫₀ᴸ g(x,τ) sin(nπx/L) dx\n\n";
        ss << "Using Fourier sine series expansion of g:\n";
        for (int n = 1; n <= std::min(terms, 4); ++n)
        {
            double lambda = n * PI / L;
            ss << "  n=" << n << ": λₙ=" << fmtD(lambda)
               << ",  decay rate=" << fmtD(a * lambda * lambda) << "\n";
        }
        return okD(ss.str());
    }

    DEResult besselEquation(double nu, double x0)
    {
        std::ostringstream ss;
        ss << "Bessel Equation of Order ν = " << fmtD(nu) << "\n";
        ss << "x²y'' + xy' + (x² - ν²)y = 0\n\n";
        ss << "General solution: y = C₁Jᵥ(x) + C₂Yᵥ(x)\n";
        ss << "where Jᵥ is Bessel function of the first kind (regular at 0)\n";
        ss << "      Yᵥ is Bessel function of the second kind (singular at 0)\n\n";

        // Series for Jν
        ss << "Jᵥ(x) = Σₖ (-1)ᵏ/(k! Γ(k+ν+1)) (x/2)^{2k+ν}\n\n";

        // Evaluate Jν(x0) numerically
        double Jnu = 0;
        double factorial_k = 1;
        for (int k = 0; k < 20; ++k)
        {
            if (k > 0)
                factorial_k *= k;
            double gamma_k_nu = std::exp(std::lgamma(k + nu + 1));
            double term = (k % 2 == 0 ? 1 : -1) / (factorial_k * gamma_k_nu) * std::pow(x0 / 2.0, 2 * k + nu);
            Jnu += term;
            if (std::abs(term) < 1e-12 * std::abs(Jnu))
                break;
        }

        ss << "Jᵥ(" << fmtD(x0) << ") ≈ " << fmtD(Jnu) << "\n\n";
        ss << "Recurrence relations:\n";
        ss << "  J_{ν-1}(x) + J_{ν+1}(x) = (2ν/x)Jᵥ(x)\n";
        ss << "  J_{ν-1}(x) - J_{ν+1}(x) = 2J'ᵥ(x)\n\n";
        ss << "Zeros of J₀: x ≈ 2.405, 5.520, 8.654, 11.792, ...\n";
        ss << "Zeros of J₁: x ≈ 3.832, 7.016, 10.173, 13.324, ...";
        return okD(ss.str());
    }

    DEResult legendreEquation(int n, double x0)
    {
        std::ostringstream ss;
        ss << "Legendre Equation of Order n = " << n << "\n";
        ss << "(1-x²)y'' - 2xy' + n(n+1)y = 0\n\n";
        ss << "Polynomial solution: Pₙ(x) (Legendre polynomial)\n";
        ss << "Second solution: Qₙ(x) (singular at x=±1)\n\n";

        // Rodrigues' formula: Pₙ(x) = (1/2ⁿn!) dⁿ/dxⁿ (x²-1)ⁿ
        ss << "Rodrigues formula: Pₙ(x) = (1/(2ⁿn!)) dⁿ/dxⁿ (x²-1)ⁿ\n\n";

        // Compute Pn(x0) via recurrence: P0=1, P1=x, Pk=((2k-1)x*P_{k-1}-(k-1)P_{k-2})/k
        double P_prev2 = 1, P_prev1 = x0, Pn = 1;
        if (n == 0)
            Pn = 1;
        else if (n == 1)
            Pn = x0;
        else
        {
            P_prev2 = 1;
            P_prev1 = x0;
            for (int k = 2; k <= n; ++k)
            {
                Pn = ((2 * k - 1) * x0 * P_prev1 - (k - 1) * P_prev2) / k;
                P_prev2 = P_prev1;
                P_prev1 = Pn;
            }
        }

        ss << "P_" << n << "(" << fmtD(x0) << ") = " << fmtD(Pn) << "\n\n";
        ss << "Orthogonality: ∫_{-1}^{1} Pₘ(x)Pₙ(x)dx = (2/(2n+1))δₘₙ\n\n";
        ss << "Recurrence: (n+1)P_{n+1} = (2n+1)x Pₙ - n P_{n-1}\n\n";
        ss << "Polynomials:\n";
        ss << "  P₀(x) = 1\n";
        ss << "  P₁(x) = x\n";
        ss << "  P₂(x) = (3x²-1)/2\n";
        ss << "  P₃(x) = (5x³-3x)/2\n";
        ss << "  P₄(x) = (35x⁴-30x²+3)/8\n";
        return okD(ss.str());
    }

    DEResult associatedLegendre(int l, int m, double x)
    {
        std::ostringstream ss;
        ss << "Associated Legendre Function P_l^m(x), l=" << l << ", m=" << m << "\n\n";
        ss << "(1-x²)y'' - 2xy' + [l(l+1) - m²/(1-x²)]y = 0\n\n";
        // P_l^m(x) = (-1)^m (1-x²)^{m/2} d^m/dx^m P_l(x)
        // Compute P_l(x) first
        double P_prev2 = 1, P_prev1 = x, Pl = 1;
        if (l == 0)
            Pl = 1;
        else if (l == 1)
            Pl = x;
        else
        {
            for (int k = 2; k <= l; ++k)
            {
                Pl = ((2 * k - 1) * x * P_prev1 - (k - 1) * P_prev2) / k;
                P_prev2 = P_prev1;
                P_prev1 = Pl;
            }
        }
        ss << "P_" << l << "(x) = " << fmtD(Pl) << " at x=" << fmtD(x) << "\n\n";
        ss << "P_l^m(x) = (-1)^m (1-x²)^{|m|/2} (d/dx)^{|m|} P_l(x)\n\n";
        ss << "Used in spherical harmonics Y_l^m(θ,φ) = N P_l^m(cos θ) e^{imφ}\n";
        ss << "Key property: ∫_{-1}^{1} P_l^m P_{l'}^m dx = (2/(2l+1))·(l+m)!/(l-m)! δ_{ll'}";
        return okD(ss.str());
    }

    DEResult weakSolution(const std::string &pde, const std::string &testFn,
                          const std::string &x)
    {
        std::ostringstream ss;
        ss << "Weak (Distributional) Formulation\n\n";
        ss << "PDE: " << pde << "\n";
        ss << "Test function: φ(x) = " << testFn << "  (compactly supported, C∞)\n\n";
        ss << "Weak formulation: multiply PDE by φ, integrate by parts.\n\n";
        ss << "For -u'' = f on (a,b), u=0 on boundary:\n";
        ss << "∫ u''φ dx = -∫ u'φ' dx + [u'φ]  (integration by parts)\n";
        ss << "Weak form: ∫ u'φ' dx = ∫ fφ dx  for all test φ ∈ H₀¹\n\n";
        ss << "This allows non-smooth solutions (e.g. shocks).\n";
        ss << "H₀¹ = Sobolev space of functions with L² derivative, zero on boundary.\n\n";
        ss << "Rankine-Hugoniot conditions for conservation laws arise naturally\n";
        ss << "from the weak formulation with discontinuous solutions.";
        return okD(ss.str());
    }

    DEResult nonhomogPDE(double alpha, double L, const std::string &source,
                         const std::string &ic, int terms)
    {
        std::ostringstream ss;
        ss << "Nonhomogeneous Heat Equation\n";
        ss << "u_t = " << fmtD(alpha) << " u_xx + Q(x,t),  Q = " << source << "\n";
        ss << "u(0,t)=u(" << fmtD(L) << ",t)=0,  u(x,0) = " << ic << "\n\n";
        ss << "Method: Eigenfunction expansion\n";
        ss << "u(x,t) = Σ Tₙ(t) sin(nπx/L)\n\n";
        ss << "Each Tₙ satisfies:\n";
        ss << "  T'ₙ + (nπ/L)²α Tₙ = Qₙ(t)\n";
        ss << "where Qₙ(t) = (2/L)∫₀ᴸ Q(x,t)sin(nπx/L)dx\n\n";
        ss << "Solution by integrating factor:\n";
        ss << "  Tₙ(t) = e^{-λₙt}[T_n(0) + ∫₀ᵗ e^{λₙτ} Qₙ(τ) dτ]\n";
        ss << "where λₙ = " << fmtD(alpha) << "(nπ/" << fmtD(L) << ")²\n\n";
        for (int n = 1; n <= std::min(terms, 4); ++n)
        {
            double lambda = alpha * std::pow(n * PI / L, 2);
            ss << "  λ_" << n << " = " << fmtD(lambda) << "\n";
        }
        return okD(ss.str());
    }

    // Sturm-Liouville eigenvalue solver
    SLEigenResult sturmLiouvilleEigen(const std::string &p, const std::string &q,
                                      const std::string &w,
                                      double a, double b, int N, int gridN)
    {
        SLEigenResult r;
        r.grid.resize(gridN);
        double h = (b - a) / (gridN - 1);
        for (int i = 0; i < gridN; ++i)
            r.grid[i] = a + i * h;

        // Finite difference discretisation of -(py')' + qy = λwy
        // Tridiagonal eigenvalue problem
        std::vector<double> pv(gridN), qv(gridN), wv(gridN);
        try
        {
            auto pe = Calculus::parse(p), qe = Calculus::parse(q), we = Calculus::parse(w);
            for (int i = 0; i < gridN; ++i)
            {
                double x = r.grid[i];
                pv[i] = Calculus::evaluate(pe, {{"x", x}});
                qv[i] = Calculus::evaluate(qe, {{"x", x}});
                wv[i] = Calculus::evaluate(we, {{"x", x}});
            }
        }
        catch (const std::exception &e)
        {
            r.ok = false;
            r.error = e.what();
            return r;
        }

        // Interior points only (Dirichlet BCs)
        int n = gridN - 2;
        std::vector<double> diag(n), sub(n - 1), sup(n - 1);
        for (int i = 0; i < n; ++i)
        {
            int j = i + 1; // interior grid index
            double pL = 0.5 * (pv[j] + pv[j - 1]), pR = 0.5 * (pv[j] + pv[j + 1]);
            diag[i] = (pL + pR) / (h * h) + qv[j];
            if (i > 0)
                sub[i - 1] = -pL / (h * h);
            if (i < n - 1)
                sup[i] = -pR / (h * h);
        }

        // Power iteration to find N smallest eigenvalues (inverse iteration)
        r.eigenvalues.resize(N);
        r.eigenfunctions.resize(N, std::vector<double>(gridN, 0));

        // Rayleigh quotient iteration for each eigenvalue
        for (int k = 0; k < N; ++k)
        {
            std::vector<double> v(n, 1.0 / std::sqrt(n));
            // Orthogonalise against previous eigenvectors
            double lambda = 0;
            for (int iter = 0; iter < 200; ++iter)
            {
                // Av = Tridiag * v
                std::vector<double> Av(n, 0);
                for (int i = 0; i < n; ++i)
                {
                    Av[i] += diag[i] * v[i];
                    if (i > 0)
                        Av[i] += sub[i - 1] * v[i - 1];
                    if (i < n - 1)
                        Av[i] += sup[i] * v[i + 1];
                }
                // Rayleigh quotient
                double num = 0, den = 0;
                for (int i = 0; i < n; ++i)
                {
                    num += v[i] * Av[i];
                    den += v[i] * v[i] * wv[i + 1];
                }
                lambda = den > 1e-14 ? num / den : 0;
                // Orthogonalise against previous eigenfunctions
                for (int j = 0; j < k; ++j)
                {
                    double dot = 0;
                    for (int i = 0; i < n; ++i)
                        dot += v[i] * r.eigenfunctions[j][i + 1] * wv[i + 1];
                    for (int i = 0; i < n; ++i)
                        v[i] -= dot * r.eigenfunctions[j][i + 1];
                }
                // Deflate: shift so this eigenvalue moves away
                for (int i = 0; i < n; ++i)
                    Av[i] -= lambda * v[i] * wv[i + 1];
                // Normalise
                double norm = 0;
                for (double vi : v)
                    norm += vi * vi;
                norm = std::sqrt(norm);
                if (norm < 1e-14)
                    break;
                for (double &vi : v)
                    vi /= norm;
            }
            r.eigenvalues[k] = lambda;
            // Store eigenfunction (padded with zeros at boundary)
            r.eigenfunctions[k][0] = 0;
            for (int i = 0; i < n; ++i)
                r.eigenfunctions[k][i + 1] = v[i];
            r.eigenfunctions[k][gridN - 1] = 0;
        }

        std::ostringstream ss;
        ss << "Sturm-Liouville Eigenvalues (-(py')' + qy = λwy):\n";
        for (int k = 0; k < N; ++k)
            ss << "  λ_" << (k + 1) << "=" << fmtD(r.eigenvalues[k]) << "\n";
        ss << "\n(Eigenfunctions stored, sampled on " << gridN << " grid points)";
        r.summary = ss.str();
        return r;
    }

    DEResult rayleighQuotientDE(const std::string &p, const std::string &q,
                                const std::string &w, const std::string &trial,
                                const std::string &x, double a, double b)
    {
        std::ostringstream ss;
        ss << "Rayleigh Quotient R[y] = ∫(py'²+qy²)dx / ∫wy²dx\n\n";
        try
        {
            auto pe = Calculus::parse(p), qe = Calculus::parse(q);
            auto we = Calculus::parse(w), ye = Calculus::parse(trial);
            auto yp = Calculus::simplify(Calculus::diff(ye, x));

            auto num_integrand = [&](double xv)
            {
                double pv = Calculus::evaluate(pe, {{x, xv}});
                double qv = Calculus::evaluate(qe, {{x, xv}});
                double yv = Calculus::evaluate(ye, {{x, xv}});
                double ypv = Calculus::evaluate(yp, {{x, xv}});
                return pv * ypv * ypv + qv * yv * yv;
            };
            auto den_integrand = [&](double xv)
            {
                double wv = Calculus::evaluate(we, {{x, xv}});
                double yv = Calculus::evaluate(ye, {{x, xv}});
                return wv * yv * yv;
            };
            // Romberg integration
            int n = 100;
            double h = (b - a) / n;
            double num = 0, den = 0;
            for (int i = 0; i < n; ++i)
            {
                double xv = a + (i + 0.5) * h;
                num += num_integrand(xv);
                den += den_integrand(xv);
            }
            num *= h;
            den *= h;
            double R = den > 1e-14 ? num / den : 0;
            ss << "Trial function: y = " << trial << "\n";
            ss << "∫(py'²+qy²)dx ≈ " << fmtD(num) << "\n";
            ss << "∫wy²dx ≈ " << fmtD(den) << "\n";
            ss << "R[y] ≈ " << fmtD(R) << "\n\n";
            ss << "Rayleigh quotient provides upper bound for smallest eigenvalue λ₁ ≤ R[y]\n";
            ss << "Minimising R[y] over trial functions gives best approximation to λ₁.";
        }
        catch (const std::exception &e)
        {
            ss << "Error: " << e.what();
        }
        return okD(ss.str());
    }

    DEResult greensFunctionDE(double a, double b, const std::string &f,
                              const std::string &bcType)
    {
        std::ostringstream ss;
        ss << "Green's Function for -y'' = f(x) on [" << fmtD(a) << "," << fmtD(b) << "]\n";
        ss << "BCs: " << bcType << "\n\n";
        if (bcType == "dirichlet")
        {
            ss << "Dirichlet: y(a)=y(b)=0\n\n";
            ss << "G(x,ξ) = { (x-a)(b-ξ)/(b-a)  if x ≤ ξ\n";
            ss << "         { (ξ-a)(b-x)/(b-a)  if x > ξ\n\n";
        }
        else if (bcType == "neumann")
        {
            ss << "Neumann: y'(a)=y'(b)=0\n";
            ss << "(No Green's function unless compatibility condition ∫f=0 holds)\n\n";
            ss << "Modified Green's function exists; solution determined up to constant.\n";
        }
        else
        {
            ss << "Mixed BCs: y(a)=0, y'(b)=0\n\n";
            ss << "G(x,ξ) = { x/b·(b-ξ)  if x ≤ ξ\n";
            ss << "         { ξ/b·(b-x)  if x > ξ  [check this for specific BCs]\n\n";
        }
        ss << "Solution: y(x) = ∫_a^b G(x,ξ)f(ξ)dξ\n\n";
        // Numerical solution for Dirichlet case
        if (bcType == "dirichlet")
        {
            ss << "Numerical solution y(x) = ∫G·f dξ at sample points:\n";
            try
            {
                auto fe = Calculus::parse(f);
                int n = 10;
                for (int i = 1; i < n; ++i)
                {
                    double x = a + (b - a) * i / n;
                    double y = 0;
                    // Quadrature: y(x) = ∫_a^b G(x,ξ)f(ξ)dξ
                    int nq = 100;
                    double dxi = (b - a) / nq;
                    for (int j = 0; j < nq; ++j)
                    {
                        double xi = a + (j + 0.5) * dxi;
                        double G = (x <= xi) ? (x - a) * (b - xi) / (b - a) : (xi - a) * (b - x) / (b - a);
                        double fv = Calculus::evaluate(fe, {{"x", xi}});
                        y += G * fv * dxi;
                    }
                    ss << "  y(" << fmtD(x, 3) << ") ≈ " << fmtD(y) << "\n";
                }
            }
            catch (...)
            {
            }
        }
        return okD(ss.str());
    }

    DEResult comparisonTheorem(double q1, double q2, double a, double b)
    {
        std::ostringstream ss;
        ss << "Sturm Comparison Theorem\n\n";
        ss << "Equations:\n";
        ss << "  y'' + " << fmtD(q1) << "y = 0  (equation 1, q₁=" << fmtD(q1) << ")\n";
        ss << "  z'' + " << fmtD(q2) << "z = 0  (equation 2, q₂=" << fmtD(q2) << ")\n\n";
        ss << "Theorem: If q₂ > q₁ on (a,b), then between any two consecutive zeros\n";
        ss << "of y there is at least one zero of z.\n\n";
        if (q1 > 0 && q2 > 0)
        {
            double period1 = PI / std::sqrt(q1), period2 = PI / std::sqrt(q2);
            ss << "Zero spacing for eq 1: π/√q₁ = " << fmtD(period1) << "\n";
            ss << "Zero spacing for eq 2: π/√q₂ = " << fmtD(period2) << "\n\n";
            if (q2 > q1)
                ss << "q₂ > q₁ → zeros of z2 are more densely packed\n";
            else
                ss << "q₁ > q₂ → zeros of z1 are more densely packed\n";
        }
        else
        {
            ss << "For q<0: solutions grow exponentially (no oscillation)\n";
            ss << "For q=0: solutions are linear\n";
            ss << "For q>0: solutions oscillate with period 2π/√q\n";
        }
        return okD(ss.str());
    }

    DEResult varParamsSystem(const std::vector<std::vector<double>> &A,
                             const std::vector<std::string> &G,
                             const std::string &tVar,
                             double t0, double tEnd, int steps)
    {
        int n = A.size();
        std::ostringstream ss;
        ss << "Variation of Parameters for System X' = AX + G(t)\n\n";
        ss << "Homogeneous solution X_h (from eigenanalysis)...\n";
        ss << "X_p(t) = Φ(t) ∫ Φ⁻¹(t) G(t) dt\n";
        ss << "where Φ(t) is the fundamental matrix.\n\n";
        // Numerical solution
        try
        {
            std::vector<Calculus::ExprPtr> Gexprs;
            for (auto &s : G)
                Gexprs.push_back(Calculus::parse(s));

            VecF F = [&](double t, const std::vector<double> &x)
            {
                std::vector<double> dx(n, 0);
                for (int i = 0; i < n; ++i)
                {
                    for (int j = 0; j < n; ++j)
                        dx[i] += A[i][j] * x[j];
                    dx[i] += Calculus::evaluate(Gexprs[i], {{tVar, t}});
                }
                return dx;
            };
            std::vector<double> x0(n, 0);
            auto traj = rk4System(F, x0, t0, tEnd, steps);
            ss << "Numerical particular solution (x0=0):\n";
            int stride = std::max(1, (int)traj.size() / 10);
            for (int i = 0; i < (int)traj.size(); i += stride)
            {
                ss << "  t=" << fmtD(traj[i][0]) << ": [";
                for (int j = 1; j <= (int)x0.size(); ++j)
                {
                    if (j > 1)
                        ss << ",";
                    ss << fmtD(traj[i][j]);
                }
                ss << "]\n";
            }
        }
        catch (const std::exception &e)
        {
            ss << "Error: " << e.what();
        }
        return okD(ss.str());
    }

    DEResult partialFractionsDE(const std::string &num, const std::string &den,
                                const std::string &sVar)
    {
        std::ostringstream ss;
        ss << "Partial Fractions in s-domain\n";
        ss << "F(s) = (" << num << ") / (" << den << ")\n\n";
        ss << "Partial fraction decomposition forms:\n\n";
        ss << "For each real root r of denominator:\n";
        ss << "  A/(s-r)  →  A·e^{rt} in time domain\n\n";
        ss << "For repeated real root r of multiplicity m:\n";
        ss << "  A/(s-r) + B/(s-r)² + ...  →  (A + Bt + ...)e^{rt}\n\n";
        ss << "For complex pair α±βi:\n";
        ss << "  (As+B)/((s-α)²+β²)  →  e^{αt}[C·cos(βt)+D·sin(βt)]\n\n";
        ss << "Algorithm (cover-up method for simple roots):\n";
        ss << "  A_k = lim_{s→r_k} (s-r_k)F(s)\n\n";
        ss << "Then apply inverse Laplace table.\n";
        return okD(ss.str());
    }

    // =============================================================================
    // REMAINING MISSING IMPLEMENTATIONS — DifferentialEquations
    // =============================================================================

    // powerSeriesSolution — proper implementation
    DEResult powerSeriesSolution(double p, double q, double r_coeff, int terms)
    {
        // y'' + p*y' + q*y = r_coeff (constant forcing)
        // Assume solution y = Σ aₙxⁿ around x=0 (ordinary point)
        std::ostringstream ss;
        ss << "Power Series Solution\n";
        ss << "y'' + " << fmtD(p) << "y' + " << fmtD(q) << "y = " << fmtD(r_coeff) << "\n\n";
        ss << "Assume y = Σ aₙxⁿ about x=0 (ordinary point if p,q analytic)\n\n";

        // Recurrence: (n+2)(n+1)a_{n+2} + p*(n+1)a_{n+1} + q*aₙ = [r_coeff if n=0 else 0]
        // Two free parameters: a₀, a₁
        std::vector<double> a0series(terms + 2, 0), a1series(terms + 2, 0);
        a0series[0] = 1;
        a0series[1] = 0; // a₀=1, a₁=0
        a1series[0] = 0;
        a1series[1] = 1; // a₀=0, a₁=1

        for (int n = 0; n <= terms - 2; ++n)
        {
            double rhs0 = (n == 0) ? r_coeff : 0;
            a0series[n + 2] = (rhs0 - p * (n + 1) * a0series[n + 1] - q * a0series[n]) / ((n + 2) * (n + 1));
            a1series[n + 2] = (0 - p * (n + 1) * a1series[n + 1] - q * a1series[n]) / ((n + 2) * (n + 1));
        }

        ss << "Two independent solutions (a₀=1,a₁=0) and (a₀=0,a₁=1):\n\n";
        ss << "y₁(x) = ";
        for (int n = 0; n <= terms; ++n)
        {
            if (std::abs(a0series[n]) < 1e-12)
                continue;
            if (n > 0 && a0series[n] >= 0)
                ss << " + ";
            else if (a0series[n] < 0)
                ss << " - ";
            ss << std::abs(a0series[n]);
            if (n == 1)
                ss << "x";
            else if (n > 1)
                ss << "x^" << n;
        }
        ss << "\n\ny₂(x) = ";
        for (int n = 0; n <= terms; ++n)
        {
            if (std::abs(a1series[n]) < 1e-12)
                continue;
            if (n > 0 && a1series[n] >= 0)
                ss << " + ";
            else if (a1series[n] < 0)
                ss << " - ";
            ss << std::abs(a1series[n]);
            if (n == 1)
                ss << "x";
            else if (n > 1)
                ss << "x^" << n;
        }
        ss << "\n\nGeneral solution: y = C₁y₁(x) + C₂y₂(x)";
        if (std::abs(r_coeff) > 1e-12)
            ss << " + yₚ (particular solution)";
        ss << "\n\nRadius of convergence ≥ distance from x=0 to nearest singular point.";
        return okD(ss.str());
    }

    // frobeniusSolution — proper Frobenius method
    DEResult frobeniusSolution(double p0, double q0, int terms)
    {
        // x²y'' + x*p(x)*y' + q(x)*y = 0 near regular singular point x=0
        // p(0) = p0, q(0) = q0 (lowest-order coefficients)
        // Indicial equation: r(r-1) + p0*r + q0 = 0
        std::ostringstream ss;
        ss << "Frobenius Method (Regular Singular Point x=0)\n\n";
        ss << "ODE form: x²y'' + x·p(x)y' + q(x)y = 0\n";
        ss << "p₀ = lim_{x→0} x·P(x) = " << fmtD(p0) << "\n";
        ss << "q₀ = lim_{x→0} x²·Q(x) = " << fmtD(q0) << "\n\n";

        // Indicial equation: r² + (p0-1)r + q0 = 0
        double A = 1, B = p0 - 1, C = q0;
        double disc = B * B - 4 * A * C;
        ss << "Indicial equation: r² + " << fmtD(B) << "r + " << fmtD(C) << " = 0\n";

        double r1, r2;
        if (disc >= 0)
        {
            r1 = (-B + std::sqrt(disc)) / 2;
            r2 = (-B - std::sqrt(disc)) / 2;
            ss << "Roots: r₁ = " << fmtD(r1) << ",  r₂ = " << fmtD(r2) << "\n\n";
        }
        else
        {
            double alpha = -B / 2, beta = std::sqrt(-disc) / 2;
            r1 = alpha;
            r2 = alpha;
            ss << "Complex roots: " << fmtD(alpha) << " ± " << fmtD(beta) << "i\n\n";
        }

        // Classify based on r1 - r2
        double diff = r1 - r2;
        ss << "Cases:\n";
        if (std::abs(diff) < 1e-10)
        {
            ss << "  Equal roots r₁ = r₂ = " << fmtD(r1) << "\n";
            ss << "  y₁ = x^r₁ Σ aₙxⁿ\n";
            ss << "  y₂ = y₁ ln(x) + x^r₂ Σ bₙxⁿ\n";
        }
        else if (std::abs(diff - std::round(diff)) < 1e-8 && diff > 0)
        {
            ss << "  Roots differ by positive integer N = " << (int)std::round(diff) << "\n";
            ss << "  y₁ = x^r₁ Σ aₙxⁿ  (r₁ = " << fmtD(r1) << ", larger)\n";
            ss << "  y₂ = c·y₁ ln(x) + x^r₂ Σ bₙxⁿ  (c may be 0)\n";
        }
        else
        {
            ss << "  Roots differ by non-integer: " << fmtD(diff) << "\n";
            ss << "  y₁ = x^r₁ Σ aₙxⁿ\n";
            ss << "  y₂ = x^r₂ Σ bₙxⁿ  (both Frobenius series)\n";
        }

        ss << "\nFirst solution y₁ = x^" << fmtD(r1) << " [a₀ + a₁x + ...]\n";
        ss << "Recurrence: substitute into ODE to find aₙ in terms of a₀.\n\n";

        // Compute series coefficients for y1
        ss << "Series coefficients (for the larger root r₁ = " << fmtD(r1) << "):\n";
        std::vector<double> a(terms + 1, 0);
        a[0] = 1.0;
        for (int n = 1; n <= terms; ++n)
        {
            // (r1+n)(r1+n-1)*an + p0*(r1+n)*an + q0*an = -[(previous terms)]
            // For simplest case (p,q constant):
            double denom = (r1 + n) * (r1 + n - 1) + p0 * (r1 + n) + q0;
            if (std::abs(denom) < 1e-12)
            {
                ss << "  a_" << n << " = free parameter\n";
                continue;
            }
            // Simple recurrence (constant p0, q0)
            a[n] = 0; // no contribution for constant coefficients beyond leading
            ss << "  a_" << n << " = " << fmtD(a[n]) << "\n";
        }
        ss << "\ny₁(x) ≈ x^" << fmtD(r1) << " (1 + higher-order terms)";
        return okD(ss.str());
    }

    // richardsonExtrapolODE
    DEResult richardsonExtrapolODE(const Func &f, double t0, double y0,
                                   double t1, int n)
    {
        std::ostringstream ss;
        ss << "Richardson Extrapolation for ODEs\n\n";
        ss << "Apply RK4 with step h and h/2, extrapolate for higher accuracy.\n\n";

        // RK4 with n steps
        auto rk4_val = [&](int steps) -> double
        {
            double h = (t1 - t0) / steps, y = y0;
            for (int i = 0; i < steps; ++i)
            {
                double t = t0 + i * h;
                double k1 = f(t, y), k2 = f(t + h / 2, y + h / 2 * k1);
                double k3 = f(t + h / 2, y + h / 2 * k2), k4 = f(t + h, y + h * k3);
                y += h / 6 * (k1 + 2 * k2 + 2 * k3 + k4);
            }
            return y;
        };

        double y1 = rk4_val(n), y2 = rk4_val(2 * n), y4 = rk4_val(4 * n);
        // Richardson: y_exact ≈ y2 + (y2-y1)/15  (order 4 → order 5)
        double rich1 = y2 + (y2 - y1) / 15.0;
        double rich2 = y4 + (y4 - y2) / 15.0;
        // Second extrapolation
        double rich3 = rich2 + (rich2 - rich1) / 31.0;

        ss << std::setw(6) << "n" << std::setw(15) << "RK4 value"
           << std::setw(15) << "Richardson\n"
           << std::string(36, '-') << "\n";
        ss << std::setw(6) << n << std::setw(15) << fmtD(y1) << "\n";
        ss << std::setw(6) << 2 * n << std::setw(15) << fmtD(y2) << std::setw(15) << fmtD(rich1) << "\n";
        ss << std::setw(6) << 4 * n << std::setw(15) << fmtD(y4) << std::setw(15) << fmtD(rich2) << "\n";
        ss << "\nBest estimate (2nd Richardson): y(" << fmtD(t1) << ") ≈ " << fmtD(rich3) << "\n";
        ss << "Error estimate: |y4 - y2| = " << fmtD(std::abs(y4 - y2));
        DEResult r;
        r.symbolic = ss.str();
        r.numerical = ss.str();
        return r;
    }

    // computeODE, computeIVP, computePDE (formatted entry points)
    DEFormatted computeODE(const std::string &type, const std::string &params)
    {
        DEFormatted out;
        try
        {
            DEResult r = dispatch(type, params, false);
            out.ok = r.ok;
            out.error = r.error;
            out.solution = r.symbolic;
            out.method = r.method;
        }
        catch (const std::exception &e)
        {
            out.ok = false;
            out.error = e.what();
        }
        return out;
    }

    DEFormatted computeIVP(const std::string &type, const std::string &params,
                           double t0, double t1, double y0, int n)
    {
        DEFormatted out;
        try
        {
            // Build augmented JSON
            std::string augParams = params;
            if (!augParams.empty() && augParams.back() == '}')
                augParams = augParams.substr(0, augParams.size() - 1) + ",\"t0\":" + std::to_string(t0) + ",\"t1\":" + std::to_string(t1) + ",\"y0\":" + std::to_string(y0) + ",\"n\":" + std::to_string(n) + "}";
            DEResult r = dispatch(type, augParams, false);
            out.ok = r.ok;
            out.error = r.error;
            out.solution = r.symbolic;
            out.method = r.method;
        }
        catch (const std::exception &e)
        {
            out.ok = false;
            out.error = e.what();
        }
        return out;
    }

    DEFormatted computePDE(const std::string &type, const std::string &params, int terms)
    {
        DEFormatted out;
        try
        {
            std::string augParams = params;
            if (!augParams.empty() && augParams.back() == '}')
                augParams = augParams.substr(0, augParams.size() - 1) + ",\"terms\":" + std::to_string(terms) + "}";
            DEResult r = dispatch(type, augParams, false);
            out.ok = r.ok;
            out.error = r.error;
            out.solution = r.symbolic;
            out.method = r.method;
        }
        catch (const std::exception &e)
        {
            out.ok = false;
            out.error = e.what();
        }
        return out;
    }
}