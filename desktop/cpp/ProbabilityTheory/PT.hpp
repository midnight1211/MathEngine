#pragma once
// PT.hpp — CoreEngine prefix: "prob:<op>|{json}"

#ifndef PT_HPP
#define PT_HPP

#include <string>
#include <vector>

namespace ProbabilityTheory
{

    struct PTResult
    {
        bool ok = true;
        std::string value;
        std::string detail;
        std::string error;
    };

    using Vec = std::vector<double>;

    // ── Moment generating functions ───────────────────────────────────────────────
    PTResult mgfNormal(double mu, double sigma, double t);
    PTResult mgfExponential(double lambda, double t);
    PTResult mgfBinomial(int n, double p, double t);
    PTResult mgfPoisson(double lambda, double t);
    PTResult mgfGamma(double alpha, double beta, double t);
    PTResult mgfUniform(double a, double b, double t);
    PTResult mgfDerive(const std::string &mgf, int moment); // E[X^k] from M^(k)(0)

    // ── Characteristic functions ──────────────────────────────────────────────────
    PTResult charFnNormal(double mu, double sigma, double t);
    PTResult charFnPoisson(double lambda, double t);
    PTResult charFnCauchy(double x0, double gamma, double t);
    PTResult levyContinuity(const Vec &charFnValues, const Vec &tValues); // inversion

    // ── Transforms and moments ────────────────────────────────────────────────────
    PTResult momentsByMGF(const std::string &dist, const Vec &params, int maxMoment);
    PTResult cumulantFromMGF(const std::string &dist, const Vec &params, int k);
    PTResult probabilityGF(const Vec &probs, double z); // G(z) = Σ p_k z^k
    PTResult laplaceStieltjes(const std::string &dist, const Vec &params, double s);

    // ── Limit theorems ────────────────────────────────────────────────────────────
    PTResult cltDemo(const std::string &dist, const Vec &params, int n, int reps);
    PTResult weakLLN(const std::string &dist, const Vec &params, int n);
    PTResult strongLLN(const std::string &dist, const Vec &params, int nMax);
    PTResult berryEsseen(double mu, double sigma, double rho, int n);     // CLT rate
    PTResult cltApproximate(double mu, double sigma, int n, double xVal); // P(S_n ≤ x)

    // ── Concentration inequalities ────────────────────────────────────────────────
    PTResult markovInequality(double mu, double a);                  // P(X≥a) ≤ μ/a
    PTResult chebyshevInequality(double mu, double sigma, double k); // P(|X-μ|≥kσ) ≤ 1/k²
    PTResult chernoffBound(double mu, double delta);                 // Binomial: P(X≥(1+δ)μ)
    PTResult hoeffdingBound(int n, double a, double b, double t);    // bounded rvs
    PTResult azumaHoeffding(int n, double c, double t);              // martingale differences

    // ── Stochastic processes ──────────────────────────────────────────────────────
    PTResult poissonProcess(double lambda, double t, int maxK); // N(t) ∼ Pois(λt)
    PTResult brownianMotion(double t, int steps);               // Wiener process
    PTResult geometricBrownian(double S0, double mu, double sigma, double t, int steps);
    PTResult ornsteinUhlenbeck(double theta, double mu, double sigma, double x0, double t, int steps);
    PTResult randomWalkSymm(int steps, int trials); // symmetric ±1
    // Discrete-time Markov chain: state distribution after n steps + steady state
    PTResult markovChain(const std::string &transition, const std::string &initial, int steps);

    // ── Martingales ───────────────────────────────────────────────────────────────
    PTResult optionalStopping(double mu, double sigma, double a, double b); // gambler's ruin
    PTResult gamblersRuin(double p, int start, int target);                 // absorption probs
    PTResult doobMaximal(double mu, double sigma, double a);                // P(max≥a) ≤ E[X]/a
    PTResult doobDecomp(const Vec &xs);                                     // Doob decomposition signal+noise
    PTResult martingaleConverg(double mu, double sigma, int n);             // convergence check

    // ── Joint distributions ───────────────────────────────────────────────────────
    PTResult jointNormal(double mu1, double mu2, double s1, double s2, double rho,
                         double x1, double x2);
    PTResult conditionalNormal(double mu1, double mu2, double s1, double s2, double rho,
                               double x2Given);
    PTResult covarianceMatrix(const std::vector<Vec> &data);
    PTResult correlationMatrix(const std::vector<Vec> &data);
    PTResult mahalanobis(const Vec &x, const Vec &mu, const std::vector<Vec> &sigma);

    // ── Order statistics ──────────────────────────────────────────────────────────
    PTResult orderStatDist(int n, int k, const std::string &dist, const Vec &params);
    PTResult extremeValue(int n, const std::string &dist, const Vec &params);
    PTResult spacings(int n, double lambda); // exponential spacings

    // ── Simulation ────────────────────────────────────────────────────────────────
    PTResult mcEstimate(const std::string &f, double a, double b, int n); // MC integration
    PTResult importanceSampling(double mu, double sigma, double threshold, int n);

    // ── Dispatch ──────────────────────────────────────────────────────────────────
    PTResult dispatch(const std::string &op, const std::string &json);

} // namespace ProbabilityTheory
#endif
