#pragma once
// DifferentialEquations.h — ODEs, systems, Laplace, series solutions, numerical methods, PDEs.

#ifndef DIFFERENTIALEQUATIONS_H
#define DIFFERENTIALEQUATIONS_H

#include <string>
#include <vector>
#include <functional>

namespace DifferentialEquations
{

    struct DEResult
    {
        bool ok = true;
        std::string symbolic;
        std::string numerical;
        std::string method;
        std::string error;

        std::string format(bool exactMode) const
        {
            if (!ok)
                return "ERROR: " + error;
            if (exactMode && !numerical.empty())
                return numerical;
            if (!numerical.empty() && numerical != symbolic)
                return symbolic + "  ~  " + numerical;
            return symbolic;
        }
    };

    using Func = std::function<double(double, double)>; // f(t, y)
    using FuncN = std::function<std::vector<double>(    // f(t, y[])
        double, const std::vector<double> &)>;

    // ── First-order ODEs ──────────────────────────────────────────────────────────
    // All take the ODE as a string "f(x,y)" representing dy/dx = f(x,y)
    // and return the general solution as a string.

    DEResult solveSeparable(const std::string &f, const std::string &x, const std::string &y);
    DEResult solveLinearFirstOrder(const std::string &P, const std::string &Q,
                                   const std::string &x, const std::string &y); // y' + P(x)y = Q(x)
    DEResult solveExact(const std::string &M, const std::string &N,
                        const std::string &x, const std::string &y); // M dx + N dy = 0
    DEResult solveBernoulli(const std::string &P, const std::string &Q,
                            const std::string &n, const std::string &x); // y' + Py = Qy^n

    // ── Second-order ODEs ─────────────────────────────────────────────────────────

    // Constant coefficient: ay'' + by' + cy = 0
    DEResult solveHomogeneous2nd(double a, double b, double c);
    // With forcing: ay'' + by' + cy = g(x)
    DEResult solveUndeterminedCoeff(double a, double b, double c, const std::string &g,
                                    const std::string &x);
    DEResult solveVariationOfParams(double a, double b, double c, const std::string &g,
                                    const std::string &x);
    // Cauchy-Euler: ax²y'' + bxy' + cy = 0
    DEResult solveCauchyEuler(double a, double b, double c);

    // ── Numerical methods ─────────────────────────────────────────────────────────

    struct NumericalDEResult
    {
        std::vector<double> t, y; // solution points
        std::string method;
        bool ok = true;
        std::string error;
        std::string summary; // formatted last value
    };

    // y' = f(t,y),  y(t0) = y0,  integrate to t1 with n steps
    NumericalDEResult eulerMethod(const Func &f, double t0, double y0, double t1, int n);
    NumericalDEResult improvedEuler(const Func &f, double t0, double y0, double t1, int n);
    NumericalDEResult rk2(const Func &f, double t0, double y0, double t1, int n);
    NumericalDEResult rk4(const Func &f, double t0, double y0, double t1, int n);
    NumericalDEResult rk45(const Func &f, double t0, double y0, double t1,
                           double tol = 1e-6); // adaptive step
    NumericalDEResult adamsBashforth(const Func &f, double t0, double y0, double t1, int n);

    // ── Laplace transforms ────────────────────────────────────────────────────────

    DEResult laplaceTransform(const std::string &f, const std::string &t, const std::string &s);
    DEResult inverseLaplace(const std::string &F, const std::string &s, const std::string &t);
    DEResult solveIVPLaplace(double a, double b, double c,
                             const std::string &g, const std::string &t,
                             double y0, double dy0);

    // ── Series solutions ──────────────────────────────────────────────────────────

    DEResult powerSeriesSolution(double p, double q, double r, int terms); // y'' + py' + qy = r
    DEResult frobeniusSolution(double p0, double q0, int terms);           // near regular singular point

    // ── PDE (separation of variables) ────────────────────────────────────────────

    // Heat:  u_t = alpha * u_xx,  u(0,t)=u(L,t)=0,  u(x,0)=f(x)
    DEResult heatEquation(double alpha, double L,
                          const std::string &initialCondition,
                          int terms);
    // Wave:  u_tt = c² u_xx
    DEResult waveEquation(double c, double L,
                          const std::string &initialDisplacement,
                          const std::string &initialVelocity,
                          int terms);
    // Laplace: u_xx + u_yy = 0 (rectangle, Dirichlet BCs)
    DEResult laplaceEquation(double Lx, double Ly,
                             const std::string &boundaryTop,
                             int terms);

    // =============================================================================
    // LIBRETEXTS EXTENSIONS — union of all 11 DE textbooks
    // =============================================================================

    // ── Applications of first-order ODEs (Trench Ch.4, Herman) ───────────────────

    DEResult mixingProblem(double V, double cIn, double rIn, double rOut,
                           double c0, double tEnd);
    DEResult newtonCooling(double k, double Tenv, double T0, double tEnd);
    DEResult populationGrowth(double r, double P0, double tEnd, bool logistic = false,
                              double K = 0); // exponential or logistic
    DEResult terminalVelocity(double m, double g, double k, double v0, double tEnd);
    DEResult orthogonalTrajectories(const std::string &family, const std::string &x,
                                    const std::string &y);
    DEResult torricelli(double A, double a, double h0, double g_acc = 9.81);

    // ── Reduction of order (Trench Ch.5, Wiggins) ────────────────────────────────

    DEResult reductionOfOrder(double a, double b, double c,
                              const std::string &y1); // known solution y1

    // ── Higher-order ODEs (Trench Ch.9) ──────────────────────────────────────────

    // nth-order constant coefficient: a_n y^(n) + ... + a_1 y' + a_0 y = 0
    DEResult solveHigherOrder(const std::vector<double> &coeffs); // coeffs[0]=a_0,...
    DEResult annihilatorMethod(const std::vector<double> &coeffs,
                               const std::string &g,
                               const std::string &x);

    // ── Systems of ODEs (Trench Ch.10, Wiggins, Chasnov) ─────────────────────────

    // Linear system X' = AX, X(0) = X0
    // Returns fundamental matrix, eigenvalues, general solution string
    DEResult solveLinearSystem(const std::vector<std::vector<double>> &A,
                               const std::vector<double> &x0,
                               double tEnd, int steps);

    // Matrix exponential e^{At} via diagonalisation / Cayley-Hamilton
    DEResult matrixExponentialDE(const std::vector<std::vector<double>> &A, double t);

    // Variation of parameters for X' = AX + G(t)
    DEResult varParamsSystem(const std::vector<std::vector<double>> &A,
                             const std::vector<std::string> &G,
                             const std::string &tVar,
                             double t0, double tEnd, int steps);

    // Phase portrait classification for 2x2 system
    DEResult phasePortrait2x2(double a, double b, double c, double d);

    // Nonlinear system: X' = F(X), find and classify fixed points
    DEResult nonlinearSystem2D(const std::string &f1, const std::string &f2,
                               const std::string &x1, const std::string &x2,
                               double xMin, double xMax, double yMin, double yMax);

    // ── Sturm-Liouville / BVPs (Trench Ch.11-13, Herman 2nd) ─────────────────────

    // Compute eigenvalues/eigenfunctions of -(py')' + qy = λwy on [a,b]
    struct SLEigenResult
    {
        std::vector<double> eigenvalues;
        std::vector<std::vector<double>> eigenfunctions; // sampled on grid
        std::vector<double> grid;
        bool ok = true;
        std::string error;
        std::string summary;
    };
    SLEigenResult sturmLiouvilleEigen(const std::string &p, const std::string &q,
                                      const std::string &w,
                                      double a, double b, int N, int gridN = 200);

    DEResult rayleighQuotientDE(const std::string &p, const std::string &q,
                                const std::string &w, const std::string &trial,
                                const std::string &x, double a, double b);

    DEResult greensFunctionDE(double a, double b, const std::string &f,
                              const std::string &bcType); // "dirichlet","neumann","mixed"

    DEResult comparisonTheorem(double q1, double q2, double a, double b);

    // ── Fourier series (Trench Ch.11, Herman) ────────────────────────────────────

    struct FourierResult
    {
        std::vector<double> a_n; // cosine coefficients
        std::vector<double> b_n; // sine coefficients
        std::string series;
        std::string series_latex;
        double parsevalSum;
        bool ok = true;
        std::string error;
    };

    FourierResult fourierSeries(const std::string &f, const std::string &x,
                                double L, int N); // full series on [-L,L]
    FourierResult fourierSineSeries(const std::string &f, const std::string &x,
                                    double L, int N); // odd extension
    FourierResult fourierCosSeries(const std::string &f, const std::string &x,
                                   double L, int N); // even extension
    DEResult parsevalIdentity(const std::string &f, const std::string &x,
                              double L, int N);

    // ── Dynamical systems (Wiggins, Herman 2nd course) ────────────────────────────

    DEResult poincareIndex(const std::string &f1, const std::string &f2,
                           const std::string &x1, const std::string &x2,
                           double cx, double cy, double r);

    DEResult dulacCriterion(const std::string &f1, const std::string &f2,
                            const std::string &B,
                            const std::string &x1, const std::string &x2);

    DEResult hartmanGrobman(const std::string &f1, const std::string &f2,
                            const std::string &x1, const std::string &x2,
                            double xStar, double yStar);

    DEResult lyapunovFunction(const std::string &f1, const std::string &f2,
                              const std::string &V,
                              const std::string &x1, const std::string &x2,
                              double xStar, double yStar);

    DEResult limitCycleCheck(const std::string &f1, const std::string &f2,
                             const std::string &x1, const std::string &x2,
                             double xMin, double xMax, double yMin, double yMax);

    // ── Numerical methods (Brorson, Lebl) ────────────────────────────────────────

    NumericalDEResult implicitEuler(const Func &f, const Func &dfdy,
                                    double t0, double y0, double t1, int n);
    NumericalDEResult crankNicolson(const Func &f, const Func &dfdy,
                                    double t0, double y0, double t1, int n);
    NumericalDEResult bdf2(const Func &f, double t0, double y0,
                           double t1, int n);
    NumericalDEResult trapezoidalODE(const Func &f, const Func &dfdy,
                                     double t0, double y0, double t1, int n);

    DEResult stiffnessAnalysis(const Func &f, const Func &dfdy,
                               double t0, double y0, double t1);
    DEResult richardsonExtrapolODE(const Func &f, double t0, double y0,
                                   double t1, int n);
    DEResult globalErrorAnalysis(const Func &f, const Func &exact,
                                 double t0, double y0, double t1,
                                 const std::vector<int> &nValues);

    // ── Laplace transforms (full table, Trench Ch.8) ──────────────────────────────

    DEResult laplaceTable(const std::string &fStr, const std::string &tVar,
                          const std::string &sVar);
    DEResult heavisideStep(double c, const std::string &fStr,
                           const std::string &tVar);
    DEResult convolutionLaplace(const std::string &f, const std::string &g,
                                const std::string &tVar, const std::string &sVar);
    DEResult diracDeltaResponse(double a, double b, double c, double t0,
                                double tEnd);
    DEResult partialFractionsDE(const std::string &num, const std::string &den,
                                const std::string &sVar);

    // ── PDEs extended (Herman, Miersemann, Walet) ─────────────────────────────────

    DEResult fourierTransformPDE(const std::string &pde, const std::string &ic,
                                 const std::string &x, const std::string &t,
                                 double tEnd);
    DEResult characteristics1stPDE(const std::string &a, const std::string &b,
                                   const std::string &c, const std::string &ic,
                                   const std::string &x, const std::string &t);
    DEResult duhamelPrinciple(double a, double b, double c,
                              const std::string &g, const std::string &x,
                              double L, double tEnd, int terms);
    DEResult besselEquation(double nu, double x0);
    DEResult legendreEquation(int n, double x0);
    DEResult associatedLegendre(int l, int m, double x);
    DEResult nonhomogPDE(double alpha, double L,
                         const std::string &source,
                         const std::string &ic, int terms);
    DEResult weakSolution(const std::string &pde,
                          const std::string &testFn,
                          const std::string &x);

    // ── Formatted entry points ────────────────────────────────────────────────────

    struct DEFormatted
    {
        std::string solution;
        std::string method;
        std::string steps; // intermediate steps for display
        bool ok = true;
        std::string error;
    };

    DEFormatted computeODE(const std::string &type, const std::string &params);
    DEFormatted computeIVP(const std::string &type, const std::string &params,
                           double t0, double t1, double y0, int n);
    DEFormatted computePDE(const std::string &type, const std::string &params, int terms);
    DEResult dispatch(const std::string &operation,
                      const std::string &input, bool exactMode);

} // namespace DifferentialEquations

#endif