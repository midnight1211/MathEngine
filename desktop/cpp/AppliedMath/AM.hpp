#pragma once
// =============================================================================
// AppliedMath.h
// Logan, David J. "Applied Mathematics" 4th ed.
//
// Modules:
//   1.  Dimensional Analysis & Scaling
//   2.  Dynamical Systems (2D phase plane)
//   3.  Perturbation Methods & Asymptotic Expansions
//   4.  Calculus of Variations
//   5.  Boundary Value Problems & Integral Equations
//   6.  PDEs (conservation laws, characteristics, stability)
//   7.  Wave Phenomena
//   8.  Continuum Mechanics
//   9.  Discrete Models
//
// All dispatch via:
//   AMResult dispatch(const std::string& op, const std::string& json, bool exact)
//
// CoreEngine prefix: "am:<op>|{json}"
// =============================================================================

#ifndef APPLIEDMATH_H
#define APPLIEDMATH_H

#include <string>
#include <vector>
#include <functional>
#include <map>

namespace AppliedMath
{

    // ── Universal result type ─────────────────────────────────────────────────────

    struct AMResult
    {
        bool ok = true;
        std::string symbolic;
        std::string numerical;
        std::string steps; // intermediate working, for display
        std::string method;
        std::string error;

        std::string format(bool exact) const
        {
            if (!ok)
                return "ERROR: " + error;
            std::string out = symbolic;
            if (!numerical.empty() && numerical != symbolic)
                out += "  ~  " + numerical;
            if (!steps.empty())
                out = steps + "\n\nResult: " + out;
            return out;
        }
    };

    // ── Shared types ──────────────────────────────────────────────────────────────

    using Vec = std::vector<double>;
    using Mat = std::vector<std::vector<double>>;
    using Func1 = std::function<double(double)>;
    using Func2 = std::function<double(double, double)>;

    // =============================================================================
    // CHAPTER 1 — Dimensional Analysis & Scaling
    // =============================================================================

    // Buckingham Pi theorem
    // variables: list of variable names (e.g. {"F","m","L","T"})
    // dimensions: dimension vectors for each variable in [M,L,T,...] basis
    // Returns: independent dimensionless Pi groups
    AMResult buckinghamPi(const std::vector<std::string> &variables,
                          const Mat &dimensionMatrix); // rows = variables, cols = base dims

    // Nondimensionalise an ODE by identifying characteristic scales
    // equation: string description, variables: dependent/independent
    // scales: characteristic scale for each variable (0 = to be determined)
    AMResult nondimensionalise(const std::string &equation,
                               const std::vector<std::string> &variables,
                               const std::vector<double> &scales);

    // Scaling analysis: given a model, identify dominant terms
    AMResult scalingAnalysis(const std::string &equation,
                             const std::string &smallParam,
                             double epsilon);

    // =============================================================================
    // CHAPTER 2 — Two-Dimensional Dynamical Systems
    // =============================================================================

    // ── Fixed point analysis ──────────────────────────────────────────────────────

    struct FixedPoint
    {
        double x, y;
        std::string stability; // "stable node", "unstable node", "saddle",
                               // "stable spiral", "unstable spiral", "center"
        double eigenvalue1_re, eigenvalue1_im;
        double eigenvalue2_re, eigenvalue2_im;
        double traceJ, detJ, discriminant;
    };

    struct PhasePortrait
    {
        std::vector<FixedPoint> fixedPoints;
        std::string globalBehavior; // text description
        bool ok = true;
        std::string error;
    };

    // Analyse the system x' = f(x,y), y' = g(x,y)
    // Find fixed points numerically on a grid, classify via Jacobian
    PhasePortrait analysePhasePortrait(const std::string &f,
                                       const std::string &g,
                                       double xMin, double xMax,
                                       double yMin, double yMax);

    // Classify a 2x2 linear system x' = Ax via trace/det of A
    AMResult classifyLinearSystem(double a11, double a12,
                                  double a21, double a22);

    // Jacobian linearisation at a given fixed point
    AMResult lineariseAtFixedPoint(const std::string &f, const std::string &g,
                                   double x0, double y0);

    // Nullclines: f(x,y)=0 and g(x,y)=0 — returns sampled points
    AMResult computeNullclines(const std::string &f, const std::string &g,
                               double xMin, double xMax,
                               double yMin, double yMax, int gridN = 40);

    // Lyapunov function candidate check: V(x,y) > 0 and dV/dt < 0?
    AMResult checkLyapunov(const std::string &V,
                           const std::string &f, const std::string &g,
                           double xMin, double xMax, double yMin, double yMax);

    // ── Bifurcation analysis ──────────────────────────────────────────────────────

    // Classify bifurcation type for dx/dt = f(x; mu) at (x0, mu0)
    // Types detected: saddle-node, transcritical, pitchfork, Hopf
    AMResult classifyBifurcation(const std::string &f,
                                 const std::string &stateVar,
                                 const std::string &paramVar,
                                 double x0, double mu0);

    // Bifurcation diagram: fixed points vs parameter mu over [muMin, muMax]
    AMResult bifurcationDiagram(const std::string &f,
                                const std::string &stateVar,
                                const std::string &paramVar,
                                double muMin, double muMax, int nPoints = 100);

    // ── Reaction kinetics ─────────────────────────────────────────────────────────

    // Michaelis-Menten kinetics: dS/dt = -kcat*E0*S/(Km + S)
    AMResult michaelisМenten(double kcat, double Km, double E0,
                             double S0, double tEnd, int n = 200);

    // Mass-action law for A + B → C with rate k
    AMResult massAction(double k, double A0, double B0, double tEnd, int n = 200);

    // ── Epidemiology ──────────────────────────────────────────────────────────────

    // SIR model: dS/dt = -beta*S*I, dI/dt = beta*S*I - gamma*I, dR/dt = gamma*I
    AMResult sirModel(double beta, double gamma,
                      double S0, double I0, double R0,
                      double tEnd, int n = 500);

    // SEIR model (with exposed compartment)
    AMResult seirModel(double beta, double sigma, double gamma,
                       double S0, double E0, double I0, double R0,
                       double tEnd, int n = 500);

    // Basic reproduction number R0 for SIR
    AMResult reproductionNumber(double beta, double gamma, double N);

    // =============================================================================
    // CHAPTER 3 — Perturbation Methods & Asymptotic Expansions
    // =============================================================================

    // Regular perturbation: x(t; eps) = x0(t) + eps*x1(t) + eps²*x2(t) + ...
    // equation: string "x'' + x + eps*x^3 = 0" style description
    AMResult regularPerturbation(const std::string &equation,
                                 const std::string &variable,
                                 const std::string &smallParam,
                                 double epsilon, int order = 3);

    // Poincaré-Lindstedt method for periodic solutions (removes secular terms)
    AMResult poincareLindstedt(double omega0, double epsilon,
                               double x0, double v0, int order = 2);

    // Boundary layer analysis (singular perturbation)
    // eps*y'' + p(x)*y' + q(x)*y = 0,  y(0)=a, y(1)=b
    AMResult boundaryLayerAnalysis(const std::string &p, const std::string &q,
                                   double epsilon, double a, double b);

    // Method of multiple scales
    AMResult multipleScales(double omega0, double epsilon, int order = 2);

    // WKB approximation for y'' + q(x)/eps² * y = 0
    AMResult wkbApproximation(const std::string &q, const std::string &var,
                              double epsilon, double a, double b);

    // Asymptotic expansion of an integral via Laplace's method
    // ∫ e^(N*h(x)) g(x) dx as N→∞
    AMResult laplaceMethod(const std::string &h, const std::string &g,
                           const std::string &var, double xStar, double N);

    // =============================================================================
    // CHAPTER 4 — Calculus of Variations
    // =============================================================================

    // Euler-Lagrange equation for ∫L(x,y,y') dx
    // Returns the EL equation as a string
    AMResult eulerLagrange(const std::string &L,
                           const std::string &indep,  // x
                           const std::string &dep,    // y
                           const std::string &deriv); // y'

    // First integral (Beltrami identity): when L doesn't depend explicitly on x
    // L - y'*(∂L/∂y') = C
    AMResult beltramiIdentity(const std::string &L,
                              const std::string &dep,
                              const std::string &deriv);

    // Natural boundary conditions
    AMResult naturalBC(const std::string &L,
                       const std::string &dep,
                       const std::string &deriv,
                       double xA, double xB);

    // Brachistochrone problem
    AMResult brachistochrone(double x1, double y1, double x2, double y2);

    // Isoperimetric problem: extremise ∫F dx subject to ∫G dx = C
    AMResult isoperimetric(const std::string &F, const std::string &G,
                           const std::string &var, double constraint);

    // Hamilton's principle — action and equations of motion
    AMResult hamiltonPrinciple(const std::string &T,     // kinetic energy
                               const std::string &V,     // potential energy
                               const std::string &q,     // generalised coord
                               const std::string &qdot); // generalised velocity

    // Noether's theorem: symmetry → conservation law
    AMResult noetherTheorem(const std::string &L,
                            const std::string &symmetryType); // "time", "translation", "rotation"

    // =============================================================================
    // CHAPTER 5 — Boundary Value Problems & Integral Equations
    // =============================================================================

    // Sturm-Liouville problem: -(p(x)y')' + q(x)y = lambda*r(x)y
    // Find eigenvalues and eigenfunctions numerically (shooting method)
    struct SLResult
    {
        Vec eigenvalues;
        std::vector<Vec> eigenfunctions; // sampled at grid points
        Vec xGrid;
        bool ok = true;
        std::string error;
    };
    SLResult sturmLiouville(const std::string &p, const std::string &q,
                            const std::string &r,
                            double a, double b,
                            int nEigenvalues = 5);

    // Rayleigh quotient for eigenvalue estimate
    AMResult rayleighQuotient(const std::string &p, const std::string &q,
                              const std::string &r,
                              const std::string &trialFunc,
                              double a, double b);

    // Green's function for Ly = f with homogeneous BCs
    AMResult greensFunction(const std::string &p, const std::string &q,
                            double a, double b,
                            double alpha, double beta, // BC coefficients
                            const std::string &forcing);

    // Fredholm integral equation of the second kind: y(x) = f(x) + lambda*∫K(x,t)y(t)dt
    AMResult fredholmSecondKind(const std::string &f, const std::string &K,
                                double a, double b, double lambda,
                                int n = 50);

    // Volterra integral equation of the second kind: y(x) = f(x) + ∫_a^x K(x,t)y(t)dt
    AMResult volterraSecondKind(const std::string &f, const std::string &K,
                                double a, double b, int n = 100);

    // =============================================================================
    // CHAPTER 6 — PDEs (Logan treatment)
    // =============================================================================

    // Method of characteristics for first-order PDE: u_t + c(x,t,u)*u_x = f
    AMResult methodOfCharacteristics(const std::string &c,
                                     const std::string &f,
                                     const std::string &initialCondition,
                                     double tEnd, int n = 50);

    // Conservation law: u_t + (F(u))_x = 0
    // Rankine-Hugoniot shock speed: s = (F(uR) - F(uL))/(uR - uL)
    AMResult rankineHugoniot(const std::string &F,
                             double uLeft, double uRight);

    // Entropy condition (Lax): check if shock is physically admissible
    AMResult entropyCondition(const std::string &F,
                              double uLeft, double uRight, double shockSpeed);

    // Reaction-diffusion: u_t = D*u_xx + R(u)
    AMResult reactionDiffusion(double D, const std::string &R,
                               const std::string &initialCondition,
                               double L, double tEnd, int nx = 50, int nt = 500);

    // Travelling wave solution: u(x,t) = U(x - ct)
    // Substitute into PDE and find wave speed c and profile U
    AMResult travellingWave(double D, const std::string &R,
                            double uMinus, double uPlus);

    // Stability of constant solution u* to reaction-diffusion (Turing instability)
    AMResult turingInstability(double D1, double D2,
                               double f_u, double f_v,
                               double g_u, double g_v);

    // =============================================================================
    // CHAPTER 7 — Wave Phenomena
    // =============================================================================

    // Dispersion relation: given wave PDE, extract omega(k)
    AMResult dispersionRelation(const std::string &waveEquation,
                                double k);

    // D'Alembert solution: u(x,t) = f(x+ct) + g(x-ct)
    AMResult dAlembertSolution(const std::string &f0, // initial displacement u(x,0)
                               const std::string &g0, // initial velocity u_t(x,0)
                               double c,
                               double xEval, double tEval);

    // Group velocity and phase velocity from dispersion relation omega(k)
    AMResult waveVelocities(const std::string &omega_k, double k0);

    // Burgers' equation: u_t + u*u_x = nu*u_xx  (Cole-Hopf transformation)
    AMResult burgers(double nu,
                     const std::string &initialCondition,
                     double L, double tEnd, int nx = 100, int nt = 1000);

    // Nonlinear wave: simple wave solution and breaking time
    AMResult simpleWave(const std::string &c_u, // characteristic speed c(u)
                        const std::string &u0,  // initial condition u(x,0)
                        double xMin, double xMax);

    // Shock formation time for u_t + c(u)*u_x = 0
    AMResult shockFormationTime(const std::string &c_u,
                                const std::string &u0,
                                double xMin, double xMax);

    // =============================================================================
    // CHAPTER 8 — Mathematical Models of Continua
    // =============================================================================

    // Continuity equation: ∂ρ/∂t + ∇·(ρv) = 0
    // 1D: ρ_t + (ρu)_x = 0
    AMResult continuityEquation1D(double rho0, const std::string &u,
                                  double L, double tEnd);

    // 1D Euler equations (compressible gas dynamics)
    // ρ_t + (ρu)_x = 0,  (ρu)_t + (ρu² + p)_x = 0,  e_t + ((e+p)u)_x = 0
    AMResult eulerEquations1D(double gamma,
                              double rho0, double u0, double p0,
                              double tEnd, int n = 100);

    // Speed of sound in ideal gas: c = sqrt(gamma*p/rho)
    AMResult speedOfSound(double gamma, double p, double rho);

    // Mach number and flow regime classification
    AMResult machNumber(double velocity, double gamma, double p, double rho);

    // Rankine-Hugoniot for gas dynamics (shock relations)
    AMResult gasDynamicsShock(double gamma,
                              double rho1, double u1, double p1,
                              double machShock);

    // Potential flow: phi satisfies Laplace, u = ∇phi
    // Given potential phi(x,y), compute velocity field
    AMResult potentialFlow(const std::string &phi,
                           double xEval, double yEval);

    // Stream function and circulation
    AMResult streamFunction(const std::string &psi,
                            double xEval, double yEval);

    // =============================================================================
    // CHAPTER 9 — Discrete Models
    // =============================================================================

    // Logistic map: x_{n+1} = r*x_n*(1 - x_n)
    // Returns orbit and eventual period (or "chaos" label)
    AMResult logisticMap(double r, double x0, int iterations = 200);

    // Cobweb diagram data: x_n vs f(x_n) for iteration
    AMResult cobwebDiagram(const std::string &f, double x0,
                           int iterations = 20);

    // Fixed points and stability for x_{n+1} = f(x_n)
    AMResult discreteFixedPoints(const std::string &f,
                                 double xMin, double xMax);

    // Period-doubling cascade and Feigenbaum constant estimate
    AMResult feigenbaumAnalysis(double rMin, double rMax, int steps = 500);

    // System of difference equations x_{n+1} = A*x_n
    // Stability via eigenvalues of A
    AMResult discreteSystem(const Mat &A, const Vec &x0, int steps = 50);

    // Random walk: 1D symmetric/asymmetric
    // Returns mean, variance, and sample path
    AMResult randomWalk1D(double p, int steps, int trials = 1000);

    // Branching process: offspring distribution {p_k}
    // Returns extinction probability and mean
    AMResult branchingProcess(const Vec &offspringDist, int generations = 10);

    // Galton-Watson process extinction probability
    AMResult galtonWatson(double mean, double variance, int maxIter = 100);

    // =============================================================================
    // DISPATCH
    // =============================================================================

    AMResult dispatch(const std::string &operation,
                      const std::string &json,
                      bool exactMode);

} // namespace AppliedMath

#endif // APPLIEDMATH_H