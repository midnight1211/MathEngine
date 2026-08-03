
// ProbabilityTheory.cpp

#include "PT.hpp"
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

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <numeric>
#include <random>
#include <sstream>
#include "../Calculus/Calculus.hpp"

namespace ProbabilityTheory
{
	static PTResult ok(const std::string &v, const std::string &d = "") { return {true, v, d, ""}; }
	static PTResult err(const std::string &m) { return {false, "", "", m}; }

	// =============================================================================
	// MOMENT GENERATING FUNCTIONS
	// =============================================================================

	PTResult mgfNormal(const double mu, const double sigma, const double t)
	{
		// M(t) = exp(μt + σ²t²/2)
		const double M = std::exp(mu * t + 0.5 * sigma * sigma * t * t);
		std::ostringstream ss;
		ss << "MGF of X ~ N(" << mu << "," << sigma << "²)\n";
		ss << "M_X(t) = exp(μt + σ²t²/2) = exp(" << mu << "t + " << 0.5 * sigma * sigma << "t²)\n\n";
		ss << "M_X(" << t << ") = " << fmt(M) << "\n\n";
		ss << "Moments (from M^(k)(0)):\n";
		ss << "  E[X]   = μ = " << mu << "\n";
		ss << "  E[X²]  = μ²+σ² = " << mu * mu + sigma * sigma << "\n";
		ss << "  Var(X) = σ² = " << sigma * sigma << "\n";
		ss << "  E[X³]  = μ³+3μσ² = " << mu * mu * mu + 3 * mu * sigma * sigma << "\n";
		ss << "  E[X⁴]  = μ⁴+6μ²σ²+3σ⁴ = " << mu * mu * mu * mu + 6 * mu * mu * sigma * sigma + 3 * std::pow(sigma, 4);
		return ok(fmt(M), ss.str());
	}

	PTResult mgfExponential(double lambda, double t)
	{
		if (t >= lambda)
			return err("t must be < λ for MGF to exist");
		double M = lambda / (lambda - t);
		std::ostringstream ss;
		ss << "MGF of X ~ Exp(" << lambda << ")\n";
		ss << "M_X(t) = λ/(λ-t) = " << lambda << "/(" << lambda << "-t),  t < " << lambda << "\n\n";
		ss << "M_X(" << t << ") = " << fmt(M) << "\n\n";
		ss << "Moments:\n  E[X] = 1/λ = " << 1.0 / lambda << "\n";
		ss << "  E[X²] = 2/λ² = " << 2.0 / (lambda * lambda) << "\n";
		ss << "  E[Xⁿ] = n!/λⁿ\n";
		ss << "  Var(X) = 1/λ² = " << 1.0 / (lambda * lambda);
		return ok(fmt(M), ss.str());
	}

	PTResult mgfBinomial(const int n, const double p, const double t)
	{
		// M(t) = (1-p+pe^t)^n
		double const M = std::pow(1 - p + p * std::exp(t), n);
		std::ostringstream ss;
		ss << "MGF of X ~ Bin(" << n << "," << p << ")\n";
		ss << "M_X(t) = (1-p+pe^t)^n = (1-" << p << "+" << p << "e^t)^" << n << "\n\n";
		ss << "M_X(" << t << ") = " << fmt(M) << "\n\n";
		ss << "Moments:\n  E[X] = np = " << n * p << "\n";
		ss << "  Var(X) = np(1-p) = " << n * p * (1 - p) << "\n";
		ss << "  E[X²] = np(1-p) + (np)² = " << n * p * (1 - p) + n * n * p * p;
		return ok(fmt(M), ss.str());
	}

	PTResult mgfPoisson(const double lambda, const double t)
	{
		const double M = std::exp(lambda * (std::exp(t) - 1));
		std::ostringstream ss;
		ss << "MGF of X ~ Pois(" << lambda << ")\n";
		ss << "M_X(t) = exp(λ(e^t - 1))\n\n";
		ss << "M_X(" << t << ") = " << fmt(M) << "\n\n";
		ss << "All moments: M^(k)(0) = λ (for all k, first moment)\n";
		ss << "E[X] = λ = " << lambda << "\n";
		ss << "Var(X) = λ = " << lambda << "\n";
		ss << "E[X²] = λ+λ² = " << lambda + lambda * lambda << "\n";
		ss << "Cumulants: κ_k = λ for all k";
		return ok(fmt(M), ss.str());
	}

	PTResult mgfGamma(const double alpha, const double beta, const double t)
	{
		if (t >= 1.0 / beta)
			return err("t must be < 1/β");
		const double M = std::pow(1 - beta * t, -alpha);
		std::ostringstream ss;
		ss << "MGF of X ~ Gamma(" << alpha << "," << beta << ")\n";
		ss << "M_X(t) = (1-βt)^{-α},  t < 1/β = " << 1.0 / beta << "\n\n";
		ss << "M_X(" << t << ") = " << fmt(M) << "\n\n";
		ss << "E[X] = αβ = " << alpha * beta << "\n";
		ss << "Var(X) = αβ² = " << alpha * beta * beta << "\n";
		ss << "E[X^k] = β^k Γ(α+k)/Γ(α)";
		return ok(fmt(M), ss.str());
	}

	PTResult mgfUniform(const double a, const double b, const double t)
	{
		double M;
		std::ostringstream ss;
		ss << "MGF of X ~ Uniform(" << a << "," << b << ")\n";
		if (std::abs(t) < 1e-10)
		{
			M = 1;
			ss << "M_X(0) = 1\n";
		}
		else
		{
			M = (std::exp(b * t) - std::exp(a * t)) / (t * (b - a));
			ss << "M_X(t) = (e^{bt}-e^{at})/(t(b-a))\n";
		}
		ss << "M_X(" << t << ") = " << fmt(M) << "\n\n";
		ss << "E[X] = (a+b)/2 = " << (a + b) / 2 << "\n";
		ss << "Var(X) = (b-a)²/12 = " << (b - a) * (b - a) / 12;
		return ok(fmt(M), ss.str());
	}

	PTResult momentsByMGF(const std::string &dist, const Vec &params, const int maxMoment)
	{
		std::ostringstream ss;
		ss << "Moments via MGF for " << dist << "\n\n";
		// Dispatch to appropriate MGF and compute derivatives numerically
		auto getMGF = [&](const double t) -> double
		{
			if (dist == "normal")
				return std::exp(params[0] * t + 0.5 * params[1] * params[1] * t * t);
			if (dist == "exponential")
				return params[0] / (params[0] - t);
			if (dist == "poisson")
				return std::exp(params[0] * (std::exp(t) - 1));
			return 0;
		};
		for (int k = 1; k <= maxMoment; ++k)
		{
			constexpr double h = 1e-4;
			// Numerical derivative M^(k)(0) via central differences
			double dk = 0;
			// kth derivative using finite differences
			std::vector<double> coeffs(k + 1);
			for (int j = 0; j <= k; ++j)
			{
				long long binom = 1;
				for (int i = 0; i < j; ++i)
					binom = binom * (k - i) / (i + 1);
				coeffs[j] = (j % 2 == 0 ? 1 : -1) * binom * getMGF((k / 2 - j) * h);
			}
			double sum = 0;
			for (const auto c : coeffs)
				sum += c;
			dk = sum / std::pow(h, k);
			ss << "E[X^" << k << "] = M^(" << k << ")(0) ≈ " << fmt(dk) << "\n";
		}
		return ok(ss.str());
	}

	PTResult cumulantFromMGF(const std::string &dist, const Vec &params, const int k)
	{
		std::ostringstream ss;
		ss << "Cumulant κ_" << k << " for " << dist << "\n\n";
		ss << "κ_k = k-th derivative of log M(t) at t=0\n\n";
		// K(t) = log M(t) — cumulant generating function
		if (dist == "normal")
		{
			if (k == 1)
				ss << "κ_1 = μ = " << params[0];
			else if (k == 2)
				ss << "κ_2 = σ² = " << params[1] * params[1];
			else
				ss << "κ_k = 0 for k > 2 (normal distribution)";
		}
		else if (dist == "poisson")
		{
			ss << "κ_k = λ = " << params[0] << " for all k";
		}
		else if (dist == "gamma")
		{
			ss << "κ_k = (k-1)! α β^k\n";
			double fac = 1;
			for (int i = 1; i < k; ++i)
				fac *= i;
			ss << "κ_" << k << " = " << fac * params[0] * std::pow(params[1], k);
		}
		else
		{
			ss << "(Compute d^k/dt^k [log M(t)] evaluated at t=0)";
		}
		return ok(ss.str());
	}

	PTResult probabilityGF(const Vec &probs, const double z)
	{
		double G = 0;
		std::ostringstream ss;
		ss << "Probability Generating Function G(z) = Σ p_k z^k\n\n";
		ss << "G(z) = ";
		for (size_t k = 0; k < probs.size(); ++k)
		{
			G += probs[k] * std::pow(z, k);
			if (k)
				ss << (probs[k] >= 0 ? " + " : " - ");
			ss << std::abs(probs[k]) << "z^" << k;
		}
		ss << "\n\nG(" << z << ") = " << fmt(G) << "\n\n";
		ss << "Properties:\n  G(1) = 1 (total probability)\n";
		ss << "  G'(1) = E[X]\n  G''(1) = E[X(X-1)]\n";
		if (!probs.empty())
		{
			double EX = 0;
			for (size_t k = 0; k < probs.size(); ++k)
				EX += k * probs[k];
			ss << "  E[X] = G'(1) ≈ " << fmt(EX);
		}
		return ok(fmt(G), ss.str());
	}

	// =============================================================================
	// CHARACTERISTIC FUNCTIONS
	// =============================================================================

	PTResult charFnNormal(const double mu, const double sigma, const double t)
	{
		// φ(t) = exp(iμt - σ²t²/2)  → |φ|=exp(-σ²t²/2)
		double modulus = std::exp(-0.5 * sigma * sigma * t * t);
		std::ostringstream ss;
		ss << "Characteristic Function of X ~ N(" << mu << "," << sigma << "²)\n";
		ss << "φ_X(t) = exp(iμt - σ²t²/2)\n\n";
		ss << "φ_X(" << t << ") = exp(" << mu << "i·" << t << " - " << 0.5 * sigma * sigma * t * t << ")\n";
		ss << "       = " << fmt(modulus) << "·exp(i·" << fmt(mu * t) << ")\n\n";
		ss << "|φ_X(t)| = " << fmt(modulus) << "\n";
		ss << "arg(φ_X(t)) = μt = " << fmt(mu * t) << " rad\n\n";
		ss << "Key property: φ is the Fourier transform of the PDF.\n";
		ss << "φ(t) → 0 as |t| → ∞ (Riemann-Lebesgue lemma)";
		return ok(fmt(modulus), ss.str());
	}

	PTResult charFnPoisson(double lambda, double t)
	{
		double re = std::exp(lambda * (std::cos(t) - 1)), im = lambda * std::sin(t);
		std::ostringstream ss;
		ss << "Characteristic Function of X ~ Pois(" << lambda << ")\n";
		ss << "φ_X(t) = exp(λ(e^{it}-1))\n\n";
		ss << "φ_X(" << t << ") = exp(" << lambda << "(cos(" << t << ")+i·sin(" << t << ")-1))\n";
		ss << "       ≈ " << fmt(re) << " + " << fmt(im) << "i\n\n";
		ss << "(Inversion: f(k) = (1/2π)∫ e^{-itk} φ(t) dt)\n";
		ss << "= λ^k e^{-λ}/k! by uniqueness";
		return ok(ss.str());
	}

	PTResult charFnCauchy(const double x0, const double gamma, const double t)
	{
		// φ(t) = exp(ix₀t - γ|t|)
		double modulus = std::exp(-gamma * std::abs(t));
		std::ostringstream ss;
		ss << "Characteristic Function of Cauchy(" << x0 << "," << gamma << ")\n";
		ss << "φ(t) = exp(ix₀t - γ|t|)\n\n";
		ss << "Note: Cauchy has no MGF (heavy tails), but CF exists\n";
		ss << "|φ(" << t << ")|  = exp(-γ|t|) = " << fmt(modulus) << "\n\n";
		ss << "Cauchy is stable with index α=1; sums scale as n^{1/1}=n";
		return ok(fmt(modulus), ss.str());
	}

	// =============================================================================
	// LIMIT THEOREMS
	// =============================================================================

	PTResult berryEsseen(const double mu, const double sigma, const double rho, const int n)
	{
		// |P(S_n/σ√n ≤ x) - Φ(x)| ≤ C·ρ/(σ³√n)  where C ≈ 0.4748
		const double C = 0.4748;
		const double bound = C * rho / (sigma * sigma * sigma * std::sqrt(n));
		std::ostringstream ss;
		ss << "Berry-Esseen Theorem\n";
		ss << "Quantifies CLT convergence rate\n\n";
		ss << "μ=" << mu << ", σ=" << sigma << ", ρ=E[|X|³]=" << rho << ", n=" << n << "\n\n";
		ss << "Bound: sup_x |F_n(x) - Φ(x)| ≤ C·ρ/(σ³√n)\n";
		ss << "     = " << C << "·" << rho << "/(" << sigma * sigma * sigma << "·" << fmt(std::sqrt(n), 4) << ")\n";
		ss << "     = " << fmt(bound) << "\n\n";
		ss << "This bound is tight (up to the constant C ≈ 0.4748)\n";
		ss << "For n=" << n << ", CLT approximation error ≤ " << fmt(bound);
		return ok(fmt(bound), ss.str());
	}

	PTResult cltApproximate(const double mu, const double sigma, const int n, const double xVal)
	{
		const double Sn_mu = n * mu;
		const double Sn_sigma = sigma * std::sqrt(n);
		const double z = (xVal - Sn_mu) / Sn_sigma;
		const double prob = mcNormcdf(z);
		std::ostringstream ss;
		ss << "CLT Approximation for S_n = X_1+...+X_n\n\n";
		ss << "Individual: E[X]=" << mu << ", SD(X)=" << sigma << "\n";
		ss << "Sum S_n: E[S_n]=" << Sn_mu << ", SD(S_n)=" << fmt(Sn_sigma) << "\n\n";
		ss << "P(S_n ≤ " << xVal << ") ≈ Φ((x - nμ)/(σ√n))\n";
		ss << "= Φ((" << xVal << "-" << Sn_mu << ")/" << fmt(Sn_sigma) << ")\n";
		ss << "= Φ(" << fmt(z) << ")\n";
		ss << "≈ " << fmt(prob) << "\n\n";
		ss << "(CLT valid for n ≥ 30 by rule of thumb)";
		return ok(fmt(prob), ss.str());
	}

	PTResult weakLLN(const std::string &dist, const Vec &params, const int n)
	{
		std::ostringstream ss;
		const double mu = params.empty() ? 0 : params[0];
		const double sigma = params.size() > 1 ? params[1] : 1;
		ss << "Weak Law of Large Numbers\n\n";
		ss << "X̄_n = (X_1+...+X_n)/n → μ in probability\n\n";
		ss << "For " << dist << ": μ = " << mu << "\n\n";
		ss << "Chebyshev bound: P(|X̄_n - μ| ≥ ε) ≤ σ²/(nε²)\n\n";
		for (const double eps : {0.1, 0.05, 0.01})
		{
			double bound = sigma * sigma / (n * eps * eps);
			ss << "  ε=" << eps << ": P(|X̄_" << n << "-" << mu << "| ≥ " << eps << ") ≤ " << fmt(std::min(bound, 1.0))
			   << "\n";
		}
		return ok(ss.str());
	}

	PTResult strongLLN(const std::string &dist, const Vec &params, const int nMax)
	{
		std::ostringstream ss;
		ss << "Strong Law of Large Numbers (SLLN)\n\n";
		ss << "X̄_n → μ almost surely (with probability 1)\n\n";
		ss << "Conditions: E[|X|] < ∞ (Kolmogorov's SLLN)\n\n";
		ss << "Demonstration (simulated running average):\n";
		std::mt19937 rng(42);
		const double mu = params.empty() ? 0 : params[0];
		const double sigma = params.size() > 1 ? params[1] : 1;
		std::normal_distribution<> nd(mu, sigma);
		double sum = 0;
		for (int n = 1; n <= nMax; ++n)
		{
			sum += nd(rng);
			if (n <= 5 || n == nMax / 2 || n == nMax)
				ss << "  X̄_" << n << " = " << fmt(sum / n) << "\n";
		}
		ss << "X̄_" << nMax << " = " << fmt(sum / nMax) << " (true μ = " << mu << ")";
		return ok(ss.str());
	}

	// =============================================================================
	// CONCENTRATION INEQUALITIES
	// =============================================================================

	PTResult markovInequality(const double mu, double a)
	{
		if (mu <= 0)
			return err("μ must be > 0 (non-negative rv)");
		if (a <= 0)
			return err("a must be > 0");
		double bound = mu / a;
		std::ostringstream ss;
		ss << "Markov's Inequality\n";
		ss << "For non-negative X with E[X]=μ:\n";
		ss << "P(X ≥ a) ≤ E[X]/a = " << mu << "/" << a << " = " << fmt(std::min(bound, 1.0)) << "\n\n";
		ss << "This is a very loose bound; Chebyshev is tighter when variance is known.";
		return ok(fmt(std::min(bound, 1.0)), ss.str());
	}

	PTResult chebyshevInequality(double mu, double sigma, double k)
	{
		if (k <= 0)
			return err("k must be > 0");
		double bound = 1.0 / (k * k);
		double prob_interval = 1 - bound;
		std::ostringstream ss;
		ss << "Chebyshev's Inequality\n";
		ss << "P(|X-μ| ≥ kσ) ≤ 1/k²\n\n";
		ss << "μ=" << mu << ", σ=" << sigma << ", k=" << k << "\n";
		ss << "Interval: [" << mu - k * sigma << ", " << mu + k * sigma << "]\n\n";
		ss << "P(|X-" << mu << "| ≥ " << k * sigma << ") ≤ 1/" << k * k << " = " << fmt(bound) << "\n";
		ss << "P(X ∈ [" << mu - k * sigma << "," << mu + k * sigma << "]) ≥ " << fmt(prob_interval) << "\n\n";
		ss << "For Normal: empirical rule gives 68/95/99.7 for k=1,2,3\n";
		ss << "Chebyshev's bound at k=2: ≥ 75% (vs 95% for normal)";
		return ok(fmt(bound), ss.str());
	}

	PTResult chernoffBound(double mu, double delta)
	{
		// P(X ≥ (1+δ)μ) ≤ (e^δ/(1+δ)^{1+δ})^μ  for Binomial
		double bound = std::pow(std::exp(delta) / std::pow(1 + delta, 1 + delta), mu);
		std::ostringstream ss;
		ss << "Chernoff Bound (Binomial upper tail)\n\n";
		ss << "P(X ≥ (1+δ)μ) ≤ (eᵟ/(1+δ)^{1+δ})^μ\n\n";
		ss << "μ=" << mu << ", δ=" << delta << "\n";
		ss << "(1+δ)μ = " << (1 + delta) * mu << "\n";
		ss << "Bound = (e^" << delta << "/" << std::pow(1 + delta, 1 + delta) << ")^" << mu << "\n";
		ss << "      = " << fmt(bound) << "\n\n";
		ss << "Simpler form for δ≤1: P(X≥(1+δ)μ) ≤ exp(-μδ²/3) = " << fmt(std::exp(-mu * delta * delta / 3));
		return ok(fmt(bound), ss.str());
	}

	PTResult hoeffdingBound(int n, double a, double b, double t)
	{
		// P(X̄ - E[X̄] ≥ t) ≤ exp(-2n²t²/(Σ(b_i-a_i)²)) = exp(-2nt²/(b-a)²)
		double bound = std::exp(-2 * n * t * t / ((b - a) * (b - a)));
		std::ostringstream ss;
		ss << "Hoeffding's Inequality\n";
		ss << "For bounded rvs a_i ≤ X_i ≤ b_i:\n";
		ss << "P(X̄-E[X̄] ≥ t) ≤ exp(-2n²t²/Σ(b_i-a_i)²)\n\n";
		ss << "n=" << n << ", [a,b]=[" << a << "," << b << "], t=" << t << "\n";
		ss << "Bound = exp(-2·" << n << "·" << t * t << "/(" << b - a << "²))\n";
		ss << "      = " << fmt(bound);
		return ok(fmt(bound), ss.str());
	}

	PTResult azumaHoeffding(int n, double c, double t)
	{
		double bound = std::exp(-2 * t * t / (n * c * c));
		std::ostringstream ss;
		ss << "Azuma-Hoeffding Inequality\n";
		ss << "For martingale differences |X_k - X_{k-1}| ≤ c:\n";
		ss << "P(X_n - X_0 ≥ t) ≤ exp(-t²/(2nc²))\n\n";
		ss << "n=" << n << ", c=" << c << ", t=" << t << "\n";
		ss << "Bound = " << fmt(bound);
		return ok(fmt(bound), ss.str());
	}

	// =============================================================================
	// STOCHASTIC PROCESSES
	// =============================================================================

	PTResult poissonProcess(double lambda, double t, int maxK)
	{
		std::ostringstream ss;
		ss << "Poisson Process N(t) ~ Pois(λt)\n";
		ss << "λ = " << lambda << " events/unit time,  t = " << t << "\n\n";
		double mean = lambda * t;
		ss << "E[N(t)] = λt = " << fmt(mean) << "\n";
		ss << "Var(N(t)) = λt = " << fmt(mean) << "\n\n";
		ss << "P(N(t)=k) for k=0,...," << maxK << ":\n";
		double logFactk = 0;
		for (int k = 0; k <= maxK; ++k)
		{
			if (k > 0)
				logFactk += std::log(k);
			double p = std::exp(-mean + k * std::log(mean) - logFactk);
			ss << "  P(N=" << k << ") = " << fmt(p, 6) << "\n";
		}
		ss << "\nInter-arrival times ~ Exp(λ): E[T] = 1/λ = " << 1.0 / lambda;
		return ok(ss.str());
	}

	PTResult brownianMotion(double t, int steps)
	{
		std::ostringstream ss;
		ss << "Standard Brownian Motion W(t)\n\n";
		ss << "Properties:\n";
		ss << "  W(0) = 0\n";
		ss << "  W(t) ~ N(0, t)\n";
		ss << "  Independent increments\n";
		ss << "  Continuous paths\n\n";
		ss << "At t = " << t << ":\n";
		ss << "  E[W(" << t << ")] = 0\n";
		ss << "  Var(W(" << t << ")) = " << t << "\n";
		ss << "  SD(W(" << t << ")) = " << fmt(std::sqrt(t)) << "\n\n";
		// Simulate one path
		std::mt19937 rng(42);
		std::normal_distribution<double> nd(0, 1);
		double dt = t / steps, W = 0;
		ss << "Simulated path (first 10 steps):\n";
		for (int i = 0; i < std::min(steps, 10); ++i)
		{
			W += nd(rng) * std::sqrt(dt);
			ss << "  t=" << fmt((i + 1) * dt, 4) << ": W=" << fmt(W) << "\n";
		}
		ss << "\nQuadratic variation: ⟨W⟩_t = t (p=2 variation)\n";
		ss << "Hölder continuous with exponent α < 1/2";
		return ok(ss.str());
	}

	PTResult geometricBrownian(double S0, double mu, double sigma, double t, int steps)
	{
		// dS = μS dt + σS dW  →  S(t) = S0 exp((μ-σ²/2)t + σW(t))
		const double drift = (mu - 0.5 * sigma * sigma) * t;
		const double ElogS = std::log(S0) + drift;
		const double VarlogS = sigma * sigma * t;
		const double ES = S0 * std::exp(mu * t);
		std::ostringstream ss;
		ss << "Geometric Brownian Motion (Black-Scholes model)\n";
		ss << "dS = μS dt + σS dW\n\n";
		ss << "S₀=" << S0 << ", μ=" << mu << ", σ=" << sigma << ", T=" << t << "\n\n";
		ss << "Solution: S(T) = S₀ exp((μ-σ²/2)T + σW(T))\n\n";
		ss << "E[S(T)] = S₀ e^{μT} = " << fmt(ES) << "\n";
		ss << "log S(T) ~ N(" << fmt(ElogS) << ", " << fmt(VarlogS) << ")\n";
		ss << "Median S(T) = S₀ exp((μ-σ²/2)T) = " << fmt(S0 * std::exp(drift)) << "\n\n";
		// Simulate
		std::mt19937 rng(42);
		std::normal_distribution<double> nd(0, 1);
		double dt = t / steps, S = S0;
		ss << "Simulated path (5 snapshots):\n";
		for (int i = 0; i < steps; ++i)
		{
			S *= std::exp((mu - 0.5 * sigma * sigma) * dt + sigma * std::sqrt(dt) * nd(rng));
			if (i % (steps / 5) == 0)
				ss << "  t=" << fmt((i + 1) * dt, 4) << ": S=" << fmt(S) << "\n";
		}
		return ok(fmt(ES), ss.str());
	}

	PTResult ornsteinUhlenbeck(double theta, double mu, double sigma, double x0, double t, int steps)
	{
		// dX = θ(μ-X)dt + σdW  (mean-reverting)
		double EX = mu + (x0 - mu) * std::exp(-theta * t);
		double VarX = sigma * sigma / (2 * theta) * (1 - std::exp(-2 * theta * t));
		std::ostringstream ss;
		ss << "Ornstein-Uhlenbeck Process\n";
		ss << "dX = θ(μ-X)dt + σdW\n\n";
		ss << "θ=" << theta << " (mean-reversion speed), μ=" << mu << " (long-run mean), σ=" << sigma << "\n";
		ss << "X₀=" << x0 << ", T=" << t << "\n\n";
		ss << "E[X(T)] = μ + (X₀-μ)e^{-θT} = " << fmt(EX) << "\n";
		ss << "Var(X(T)) = σ²/(2θ)(1-e^{-2θT}) = " << fmt(VarX) << "\n\n";
		ss << "Stationary distribution: N(μ, σ²/(2θ))\n";
		ss << "Stationary variance: " << fmt(sigma * sigma / (2 * theta)) << "\n\n";
		// Euler-Maruyama simulation
		std::mt19937 rng(42);
		std::normal_distribution<double> nd(0, 1);
		double dt = t / steps, X = x0;
		ss << "Simulated path (5 snapshots):\n";
		for (int i = 0; i < steps; ++i)
		{
			X += theta * (mu - X) * dt + sigma * std::sqrt(dt) * nd(rng);
			if (i % (steps / 5) == 0)
				ss << "  t=" << fmt((i + 1) * dt, 4) << ": X=" << fmt(X) << "\n";
		}
		return ok(fmt(EX), ss.str());
	}

	// Accepts "[[a,b],[c,d]]" or "a,b;c,d"
	static std::vector<Vec> parseRows(const std::string &s)
	{
		if (s.find('[') != std::string::npos)
			return parseMat(s);
		std::vector<Vec> rows;
		std::stringstream ss(s);
		std::string row;
		while (std::getline(ss, row, ';'))
		{
			auto v = parseVec("[" + row + "]");
			if (!v.empty())
				rows.push_back(v);
		}
		return rows;
	}

	PTResult markovChain(const std::string &transition, const std::string &initial, int steps)
	{
		auto P = parseRows(transition);
		if (P.empty())
			return err("Transition matrix required (e.g. 0.7,0.3;0.4,0.6)");
		const size_t n = P.size();
		for (const auto &row : P)
		{
			if (row.size() != n)
				return err("Transition matrix must be square");
			double s = std::accumulate(row.begin(), row.end(), 0.0);
			if (std::abs(s - 1.0) > 1e-6)
				return err("Each transition row must sum to 1 (row sum = " + fmt(s, 6) + ")");
		}

		Vec pi = initial.empty() ? Vec() : parseVec(initial.find('[') != std::string::npos ? initial : "[" + initial + "]");
		if (pi.empty())
			pi.assign(n, 1.0 / static_cast<double>(n));
		if (pi.size() != n)
			return err("Initial distribution must have " + std::to_string(n) + " entries");

		if (steps < 0)
			steps = 0;
		std::ostringstream ss;
		ss << "Discrete-Time Markov Chain (" << n << " states)\n\n";
		ss << "Distribution by step:\n";
		auto printDist = [&](int k, const Vec &d)
		{
			ss << "  n=" << k << ": [";
			for (size_t i = 0; i < d.size(); ++i)
				ss << (i ? ", " : "") << fmt(d[i], 6);
			ss << "]\n";
		};
		printDist(0, pi);
		Vec cur = pi;
		for (int k = 1; k <= steps; ++k)
		{
			Vec next(n, 0.0);
			for (size_t j = 0; j < n; ++j)
				for (size_t i = 0; i < n; ++i)
					next[j] += cur[i] * P[i][j];
			cur = next;
			if (steps <= 20 || k == steps)
				printDist(k, cur);
		}

		// Steady state via power iteration
		Vec sd(n, 1.0 / static_cast<double>(n));
		bool converged = false;
		for (int it = 0; it < 10000 && !converged; ++it)
		{
			Vec next(n, 0.0);
			for (size_t j = 0; j < n; ++j)
				for (size_t i = 0; i < n; ++i)
					next[j] += sd[i] * P[i][j];
			double diff = 0;
			for (size_t i = 0; i < n; ++i)
				diff += std::abs(next[i] - sd[i]);
			sd = next;
			converged = diff < 1e-12;
		}
		ss << "\nSteady-state distribution π (πP = π):\n  [";
		for (size_t i = 0; i < n; ++i)
			ss << (i ? ", " : "") << fmt(sd[i], 6);
		ss << "]\n";
		if (!converged)
			ss << "  (power iteration did not fully converge — chain may be periodic)\n";

		std::ostringstream val;
		val << "[";
		for (size_t i = 0; i < cur.size(); ++i)
			val << (i ? ", " : "") << fmt(cur[i], 6);
		val << "]";
		return ok(val.str(), ss.str());
	}

	PTResult randomWalkSymm(int steps, int trials)
	{
		std::mt19937 rng(42);
		double meanFinal = 0, meanMax = 0, pReturn = 0;
		for (int t = 0; t < trials; ++t)
		{
			int pos = 0, maxPos = 0, returned = 0;
			for (int s = 0; s < steps; ++s)
			{
				pos += (rng() % 2 ? 1 : -1);
				if (std::abs(pos) > maxPos)
					maxPos = std::abs(pos);
				if (pos == 0 && !returned)
				{
					returned = 1;
				}
			}
			meanFinal += std::abs(pos);
			meanMax += maxPos;
			pReturn += returned;
		}
		meanFinal /= trials;
		meanMax /= trials;
		pReturn /= trials;
		std::ostringstream ss;
		ss << "Symmetric Random Walk (±1 steps)\n\n";
		ss << "Steps n=" << steps << ", Trials=" << trials << "\n\n";
		ss << "Theoretical:\n";
		ss << "  E[|S_n|] ≈ √(2n/π) = " << fmt(std::sqrt(2.0 * steps / M_PI)) << "\n";
		ss << "  E[max|S_k|] ≈ √(2n/π) = " << fmt(std::sqrt(2.0 * steps / M_PI)) << "\n";
		ss << "  P(return to origin) = 1 (recurrence in Z^1)\n\n";
		ss << "Simulated (Monte Carlo, " << trials << " paths):\n";
		ss << "  Mean |S_n|: " << fmt(meanFinal) << "\n";
		ss << "  Mean max:   " << fmt(meanMax) << "\n";
		ss << "  Return fraction: " << fmt(pReturn);
		return ok(ss.str());
	}

	// =============================================================================
	// MARTINGALES
	// =============================================================================

	PTResult gamblersRuin(double p, int start, int target)
	{
		// P(ruin) = (q/p)^start - 1 / ((q/p)^target - 1)  if p≠q
		double q = 1 - p;
		std::ostringstream ss;
		ss << "Gambler's Ruin Problem\n";
		ss << "p=" << p << " (win prob), q=" << q << " (lose prob)\n";
		ss << "Start at " << start << ", target " << target << "\n\n";
		double pWin, pRuin;
		if (std::abs(p - 0.5) < 1e-8)
		{
			pWin = (double)start / target;
			pRuin = 1 - pWin;
		}
		else
		{
			double r = q / p;
			double rN = std::pow(r, target);
			double rS = std::pow(r, start);
			pWin = (1 - rS) / (1 - rN);
			pRuin = 1 - pWin;
		}
		double ET; // expected duration
		if (std::abs(p - 0.5) < 1e-8)
			ET = (double)start * (target - start);
		else
			ET = (start / (q - p)) - (target / (q - p)) * pWin;
		ss << "P(reach " << target << ") = " << fmt(pWin) << "\n";
		ss << "P(ruin at 0) = " << fmt(pRuin) << "\n";
		ss << "E[game duration] ≈ " << fmt(ET) << "\n\n";
		ss << "If p<1/2: ruin is certain in infinite time\n";
		ss << "If p=1/2: ruin is certain but E[T]=∞\n";
		ss << "If p>1/2: positive probability of reaching target";
		return ok(fmt(pWin), ss.str());
	}

	PTResult optionalStopping(double mu, double sigma, double a, double b)
	{
		// For BM with drift: P(reach a before -b)
		std::ostringstream ss;
		ss << "Optional Stopping Theorem\n\n";
		ss << "Brownian motion with drift: X(t) = μt + σW(t)\n";
		ss << "μ=" << mu << ", σ=" << sigma << "\n";
		ss << "Barriers: a=" << a << " (upper), -b=" << -b << " (lower)\n\n";
		if (std::abs(mu) < 1e-10)
		{
			double pUp = b / (a + b);
			ss << "No drift (μ=0, martingale):\n";
			ss << "P(reach a before -b) = b/(a+b) = " << fmt(pUp) << "\n";
			ss << "E[τ] = ab/σ² = " << fmt(a * b / (sigma * sigma)) << "\n";
		}
		else
		{
			// Wald's identity: E[X(τ)] = μ·E[τ]
			ss << "With drift: use Wald's identity\n";
			ss << "E[X(τ)] = μ·E[τ]\n";
			ss << "X(τ) = a·P(up) + (-b)·P(down)\n";
			double pUp = b / (a + b); // approx
			ss << "P(reach a) ≈ " << fmt(pUp) << " (exact depends on μ/σ ratio)\n";
		}
		ss << "\nOptional stopping theorem: E[M(τ)] = E[M(0)] for martingale M\n";
		ss << "Conditions: τ bounded, or M uniformly integrable";
		return ok(ss.str());
	}

	PTResult doobDecomp(const Vec &xs)
	{
		// Doob decomposition: X_n = M_n + A_n (martingale + predictable)
		int n = xs.size();
		std::ostringstream ss;
		ss << "Doob Decomposition X_n = M_n + A_n\n\n";
		ss << "M: martingale component\nA: predictable (non-decreasing if submartingale)\n\n";
		// A_n = Σ E[X_k - X_{k-1} | F_{k-1}] ≈ sample increments for simple case
		Vec A(n, 0), M(n);
		M[0] = xs[0];
		for (int k = 1; k < n; ++k)
		{
			A[k] = A[k - 1] + (xs[k] - xs[k - 1]); // simplified: uses actual increments
			M[k] = xs[k] - A[k];
		}
		ss << "Index | X_n      | A_n      | M_n\n"
		   << std::string(40, '-') << "\n";
		for (int k = 0; k < n && k < 10; ++k)
			ss << std::setw(5) << k << " | " << std::setw(8) << fmt(xs[k], 5) << " | " << std::setw(8) << fmt(A[k], 5)
			   << " | " << fmt(M[k], 5) << "\n";
		return ok(ss.str());
	}

	PTResult martingaleConverg(double mu, double sigma, int n)
	{
		std::ostringstream ss;
		ss << "Martingale Convergence Theorem\n\n";
		ss << "L² bounded martingale (E[M_n²] ≤ C < ∞) converges a.s.\n\n";
		ss << "For X_n = sample mean (n observations):\n";
		ss << "  μ=" << mu << ", σ=" << sigma << "\n";
		ss << "  E[X̄_n²] = μ²+σ²/n → μ² (bounded by μ²+σ²)\n\n";
		ss << "X̄_n is NOT a martingale (it converges to μ by LLN)\n\n";
		ss << "Example of martingale: S_n - n·E[X] for random walk\n";
		ss << "Doob's L² inequality: E[max_{k≤n} M_k²] ≤ 4E[M_n²]";
		return ok(ss.str());
	}

	// =============================================================================
	// JOINT DISTRIBUTIONS
	// =============================================================================

	PTResult jointNormal(double mu1, double mu2, double s1, double s2, double rho,
						 double x1, double x2)
	{
		double z1 = (x1 - mu1) / s1, z2 = (x2 - mu2) / s2;
		double exponent = -(1.0 / (2 * (1 - rho * rho))) * (z1 * z1 - 2 * rho * z1 * z2 + z2 * z2);
		double pdf = std::exp(exponent) / (2 * M_PI * s1 * s2 * std::sqrt(1 - rho * rho));
		std::ostringstream ss;
		ss << "Bivariate Normal Distribution\n";
		ss << "(X₁,X₂) ~ N(μ,Σ)\n\n";
		ss << "μ₁=" << mu1 << ", μ₂=" << mu2 << "\n";
		ss << "σ₁=" << s1 << ", σ₂=" << s2 << ", ρ=" << rho << "\n\n";
		ss << "f(" << x1 << "," << x2 << ") = " << fmt(pdf) << "\n\n";
		ss << "Covariance matrix Σ:\n";
		ss << "  [" << s1 * s1 << "  " << rho * s1 * s2 << "]\n";
		ss << "  [" << rho * s1 * s2 << "  " << s2 * s2 << "]\n\n";
		ss << "Marginals: X₁~N(" << mu1 << "," << s1 * s1 << "), X₂~N(" << mu2 << "," << s2 * s2 << ")\n";
		ss << "Independence iff ρ=0";
		return ok(fmt(pdf), ss.str());
	}

	PTResult conditionalNormal(double mu1, double mu2, double s1, double s2, double rho, double x2Given)
	{
		double muCond = mu1 + rho * (s1 / s2) * (x2Given - mu2);
		double sCond = s1 * std::sqrt(1 - rho * rho);
		std::ostringstream ss;
		ss << "Conditional Distribution X₁|X₂=" << x2Given << "\n\n";
		ss << "X₁|X₂=x₂ ~ N(μ₁|₂, σ₁|₂²)\n\n";
		ss << "μ₁|₂ = μ₁ + ρ(σ₁/σ₂)(x₂-μ₂) = " << fmt(muCond) << "\n";
		ss << "σ₁|₂ = σ₁√(1-ρ²) = " << fmt(sCond) << "\n\n";
		ss << "Conditional variance is constant (doesn't depend on x₂)!\n";
		ss << "This property characterises the bivariate normal.";
		return ok(fmt(muCond) + ", " + fmt(sCond), ss.str());
	}

	PTResult mahalanobis(const Vec &x, const Vec &mu, const std::vector<Vec> &sigma)
	{
		int n = x.size();
		// D² = (x-μ)' Σ^{-1} (x-μ)
		// For diagonal Σ: D² = Σ (x_i-μ_i)²/σ_ii²
		double D2 = 0;
		for (int i = 0; i < n; ++i)
			D2 += (x[i] - mu[i]) * (x[i] - mu[i]) / (sigma[i][i] + 1e-15);
		std::ostringstream ss;
		ss << "Mahalanobis Distance\n";
		ss << "D² = (x-μ)' Σ⁻¹ (x-μ) = " << fmt(D2) << "\n";
		ss << "D  = " << fmt(std::sqrt(D2)) << "\n\n";
		ss << "Under N(μ,Σ): D² ~ χ²(" << n << ")\n";
		ss << "p-value: P(χ²(" << n << ") > " << fmt(D2) << ") ≈ [compute from chi-sq CDF]";
		return ok(fmt(std::sqrt(D2)), ss.str());
	}

	PTResult mcEstimate(const std::string &f, double a, double b, int n)
	{
		std::ostringstream ss;
		ss << "Monte Carlo Integration\n";
		ss << "∫_{" << a << "}^{" << b << "} " << f << " dx\n\n";
		try
		{
			auto fExpr = Calculus::parse(f);
			std::mt19937 rng(42);
			std::uniform_real_distribution<double> ud(a, b);
			double sum = 0, sum2 = 0;
			for (int i = 0; i < n; ++i)
			{
				double x = ud(rng);
				double fv = Calculus::evaluate(fExpr, {{"x", x}});
				sum += fv;
				sum2 += fv * fv;
			}
			double mean = sum / n, var = (sum2 / n - mean * mean);
			double result = (b - a) * mean;
			double se = (b - a) * std::sqrt(var / n);
			ss << "Estimate: " << fmt(result) << "\n";
			ss << "Standard error: " << fmt(se) << "\n";
			ss << "95% CI: [" << fmt(result - 1.96 * se) << ", " << fmt(result + 1.96 * se) << "]\n";
			ss << "n=" << n << " samples";
			return ok(fmt(result), ss.str());
		}
		catch (const std::exception &e)
		{
			return err(e.what());
		}
	}

	// =============================================================================
	// MISSING PROBABILITY THEORY IMPLEMENTATIONS
	// =============================================================================

	PTResult cltDemo(const std::string &dist, const Vec &params, const int n, const int reps)
	{
		std::mt19937 rng(42);
		const double mu = params.empty() ? 0 : params[0];
		const double sigma = params.size() > 1 ? params[1] : 1;

		auto sample = [&]() -> double
		{
			if (dist == "normal")
				return std::normal_distribution<>(mu, sigma)(rng);
			if (dist == "exponential")
				return std::exponential_distribution<>(1.0 / mu)(rng);
			if (dist == "uniform")
				return std::uniform_real_distribution<>(mu, sigma)(rng);
			if (dist == "bernoulli")
				return std::bernoulli_distribution(mu)(rng) ? 1.0 : 0.0;
			return std::normal_distribution<>(mu, sigma)(rng);
		};

		Vec xbars(reps);
		for (int r = 0; r < reps; ++r)
		{
			double s = 0;
			for (int i = 0; i < n; ++i)
				s += sample();
			xbars[r] = s / n;
		}
		const double emp_mean = std::accumulate(xbars.begin(), xbars.end(), 0.0) / reps;
		double emp_var = 0;
		for (const double v : xbars)
			emp_var += (v - emp_mean) * (v - emp_mean);
		emp_var /= (reps - 1);

		std::ostringstream ss;
		ss << "CLT Demonstration: " << dist << " distribution, n=" << n << ", B=" << reps << " ";
		ss << "Population: μ=" << mu << ", σ=" << sigma << " ";
		ss << "Theory: X̄_n ~ N(" << fmt(mu) << ", " << fmt(sigma * sigma / n) << ")";
		ss << "Simulated: ";
		ss << "  E[X̄_n] ≈ " << fmt(emp_mean) << "  (true: " << mu << ")";
		ss << "  Var(X̄_n) ≈ " << fmt(emp_var) << "  (true: " << fmt(sigma * sigma / n) << ")";
		// Histogram (5 bins)
		double lo = xbars[0], hi = xbars[0];
		for (double v : xbars)
		{
			lo = std::min(lo, v);
			hi = std::max(hi, v);
		}
		constexpr int bins = 5;
		Vec counts(bins, 0);
		const double binw = (hi - lo) / bins;
		for (const double v : xbars)
		{
			int b = std::min(static_cast<int>((v - lo) / binw), bins - 1);
			counts[b]++;
		}
		ss << "Histogram of X̄_n: ";
		for (int i = 0; i < bins; ++i)
		{
			ss << "  [" << fmt(lo + i * binw, 4) << "," << fmt(lo + (i + 1) * binw, 4) << "] " << std::string(static_cast<int>(counts[i] * 30 / reps), '#') << " " << static_cast<int>(counts[i]) << " ";
		}
		return ok(fmt(emp_mean), ss.str());
	}

	PTResult covarianceMatrix(const std::vector<Vec> &data)
	{
		const int p = data.size();
		const int n = data[0].size();
		Vec means(p);
		for (int i = 0; i < p; ++i)
		{
			means[i] = std::accumulate(data[i].begin(), data[i].end(), 0.0) / n;
		}
		std::ostringstream ss;
		ss << "Covariance Matrix Σ (" << p << "×" << p << ")";
		for (int i = 0; i < p; ++i)
		{
			ss << "  [";
			for (int j = 0; j < p; ++j)
			{
				double cov = 0;
				for (int k = 0; k < n; ++k)
					cov += (data[i][k] - means[i]) * (data[j][k] - means[j]);
				cov /= (n - 1);
				if (j)
					ss << ", ";
				ss << fmt(cov, 5);
			}
			ss << "]";
		}
		return ok(ss.str());
	}

	PTResult correlationMatrix(const std::vector<Vec> &data)
	{
		const int p = data.size();
		const int n = data[0].size();
		Vec means(p), stds(p);
		for (int i = 0; i < p; ++i)
		{
			means[i] = std::accumulate(data[i].begin(), data[i].end(), 0.0) / n;
			double var = 0;
			for (double v : data[i])
				var += (v - means[i]) * (v - means[i]);
			stds[i] = std::sqrt(var / (n - 1));
		}
		std::ostringstream ss;
		ss << "Correlation Matrix R (" << p << "×" << p << ")";
		for (int i = 0; i < p; ++i)
		{
			ss << "  [";
			for (int j = 0; j < p; ++j)
			{
				double cov = 0;
				for (int k = 0; k < n; ++k)
					cov += (data[i][k] - means[i]) * (data[j][k] - means[j]);
				cov /= (n - 1);
				const double r = (stds[i] * stds[j] > 1e-14) ? cov / (stds[i] * stds[j]) : (i == j ? 1.0 : 0.0);
				if (j)
					ss << ", ";
				ss << fmt(r, 5);
			}
			ss << "]";
		}
		ss << "Diagonal = 1(self - correlation)";
		ss << "Off-diagonal: -1 = perfect negative, 0 = uncorrelated, 1 = perfect positive";
		return ok(ss.str());
	}

	PTResult doobMaximal(const double mu, const double sigma, const double a)
	{
		// Doob's maximal inequality: for L² martingale M,
		// P(max_{0≤k≤n} M_k ≥ a) ≤ E[M_n²] / a²
		// For Brownian motion at time t: E[W_t²] = t
		std::ostringstream ss;
		ss << "Doob's Maximal Inequality";
		ss << "For a non-negative submartingale {M_n}: ";
		ss << "P(max_{0≤k≤n} M_k ≥ a) ≤ E[M_n] / a    (Doob's L¹ maximal)";
		ss << "For an L² martingale: ";
		ss << "P(max_{0≤k≤n} |M_k| ≥ a) ≤ E[M_n²] / a²  (Doob's L² maximal)";
		ss << "Parameters: μ=" << mu << ", σ=" << sigma << ", a=" << a << " ";
		// For BM: P(max_{0≤t≤T} W_t ≥ a) = 2(1 - Φ(a/√T)) (reflection principle)
		// Use σ as √T
		const double T = sigma * sigma;
		const double pMax = 2 * (1 - mcNormcdf(a / std::sqrt(T)));
		const double doobBound = T / (a * a); // E[W_T²]/a² = T/a²
		ss << "For Brownian motion W with T=" << fmt(T) << ": ";
		ss << "  P(max W_t ≥ " << a << ") = 2Φ(-a/√T) = " << fmt(pMax) << "  (exact, reflection principle)";
		ss << "  Doob L² bound = T/a² = " << fmt(doobBound) << " ";
		ss << "The bound " << fmt(doobBound) << " ≥ " << fmt(pMax) << " ✓";
		return ok(fmt(pMax), ss.str());
	}

	PTResult extremeValue(const int n, const std::string &dist, const Vec &params)
	{
		std::ostringstream ss;
		ss << "Extreme Value Theory";
		ss << "Distribution of M_n = max(X_1,...,X_n),  n=" << n << " ";
		ss << "Base distribution: " << dist << " ";
		ss << "Fisher-Tippett-Gnedenko theorem: ";
		ss << "Under appropriate normalization, M_n → one of: ";
		ss << "  Gumbel  (Type I):  F(x) = exp(-exp(-x))       — normal, exponential";
		ss << "  Fréchet (Type II): F(x) = exp(-x^{-α})        — Pareto, Cauchy";
		ss << "  Weibull (Type III):F(x) = exp(-(-x)^α) x<0   — bounded support";
		if (dist == "normal" || dist == "exponential")
		{
			// Gumbel domain
			// Normalizing constants for standard normal: a_n=√(2 ln n), b_n=√(2 ln n)-...
			const double an = std::sqrt(2 * std::log(n));
			const double bn = an - (std::log(std::log(n)) + std::log(4 * M_PI)) / (2 * an);
			ss << "Normal → Gumbel domain";
			ss << "Normalizing: a_n = √(2 ln n) = " << fmt(an) << " ";
			ss << "             b_n ≈ " << fmt(bn) << " ";
			ss << "E[M_n] ≈ b_n + γ/a_n ≈ " << fmt(bn + 0.5772 / an) << "  (γ = Euler-Mascheroni)";
		}
		else
		{
			ss << "Extreme value domain depends on tail behaviour of " << dist << ".";
		}
		return ok(ss.str());
	}

	PTResult importanceSampling(const double mu, const double sigma, const double threshold, const int n)
	{
		// Estimate P(X > threshold) for X~N(mu,sigma) using importance sampling
		// Shift di to N(threshold, sigma) for better sampling
		std::mt19937 rng(42);
		const double mu_IS = threshold; // proposal mean
		std::normal_distribution<> nd_IS(mu_IS, sigma);

		double sum = 0, sum2 = 0;
		for (int i = 0; i < n; ++i)
		{
			const double x = nd_IS(rng);
			if (x > threshold)
			{
				// Likelihood ratio: f(x)/g(x) = N(x;mu,sigma)/N(x;mu_IS,sigma)
				const double log_ratio = -0.5 * ((x - mu) * (x - mu) - (x - mu_IS) * (x - mu_IS)) / (sigma * sigma);
				const double w = std::exp(log_ratio);
				sum += w;
				sum2 += w * w;
			}
		}
		const double est = sum / n;
		const double var_IS = (sum2 / n - est * est) / n;
		const double se = std::sqrt(var_IS);

		// True value for comparison
		const double true_val = 1 - mcNormcdf((threshold - mu) / sigma);

		std::ostringstream ss;
		ss << "Importance Sampling Estimate";
		ss << "P(X > " << threshold << ") for X~N(" << mu << "," << sigma << "²)";
		ss << "IS proposal: N(" << mu_IS << "," << sigma << "²)  n=" << n << " ";
		ss << "IS estimate: " << fmt(est) << " ";
		ss << "SE:          " << fmt(se) << " ";
		ss << "True value:  " << fmt(true_val) << " ";
		ss << "Relative error: " << fmt(std::abs(est - true_val) / true_val * 100) << "%";
		ss << "Variance reduction vs naive MC: ≈ " << fmt(true_val * (1 - true_val) / (var_IS * n + 1e-15)) << "×";
		return ok(fmt(est), ss.str());
	}

	PTResult laplaceStieltjes(const std::string &dist, const Vec &params, double s)
	{
		// LST: L(s) = E[e^{-sX}] = MGF evaluated at -s
		std::ostringstream ss;
		ss << "Laplace-Sttjes Transform L(s) = E[e^{-sX}]";
		ss << "Distribution: " << dist << ",  s=" << s << " ";
		double L = 0;
		if (dist == "exponential")
		{
			const double lambda = params.empty() ? 1 : params[0];
			if (s < lambda)
			{
				L = lambda / (lambda + s);
				ss << "L(s) = λ/(λ+s) = " << fmt(L);
			}
			else
				ss << "L(s) undefined for s ≥ λ";
		}
		else if (dist == "normal")
		{
			const double mu = params.empty() ? 0 : params[0];
			const double sigma = params.size() > 1 ? params[1] : 1;
			L = std::exp(-mu * s + 0.5 * sigma * sigma * s * s); // M(-s)
			ss << "L(s) = exp(-μs + σ²s²/2) = " << fmt(L);
		}
		else if (dist == "gamma")
		{
			const double alpha = params.empty() ? 1 : params[0];
			const double beta = params.size() > 1 ? params[1] : 1;
			if (s < 1.0 / beta)
			{
				L = std::pow(1 + beta * s, -alpha);
				ss << "L(s)=(1+βs)^{-α}=" << fmt(L);
			}
			else
				ss << "L(s) undefined for s ≥ 1/β";
		}
		else
		{
			ss << "L(s) = MGF_X(-s)";
			ss << "For " << dist << ": substitute t=-s in trmula.";
		}
		ss << "Properties : L(0) = 1 - L'(0) = E[X] L''(0) = E[X²]";
		return ok(L > 0 ? fmt(L) : "see detail", ss.str());
	}

	PTResult levyContinuity(const Vec &charFnValues, const Vec &tValues)
	{
		std::ostringstream ss;
		ss << "Lévy Continuity Theorem";
		ss << "If φ_n(t) → φ(t) pointwise for each t, and φ is continuous at 0,";
		ss << "then the sequence of distributions converges weakly to the ";
		ss << "distribution with characteristic function φ.";
		ss << "Provided characteristic fvalues: ";
		const int n = std::min(charFnValues.size(), tValues.size());
		for (int i = 0; i < n; ++i)
			ss << "  φ(t=" << fmt(tValues[i], 4) << ") = " << fmt(charFnValues[i]) << " ";
		ss << "Continuity check at t = 0 : ";
		if (!charFnValues.empty())
			ss << "  φ(0) = " << fmt(charFnValues[0]) << " (should be 1.0) ";
		ss << "  Application : CLT proof via Lévy — show φ_{S_n}(t) → e ^ {-t² / 2} ";
		ss << "  φ_{X/√n}(t) = φ_X(t/√n) ≈ 1 - t²σ²/(2n) → e^{-σ²t²/2}  as n→∞";
		return ok(ss.str());
	}

	PTResult mgfDerive(const std::string &mgf, const int moment)
	{
		std::ostringstream ss;
		ss << "Computing E[X^" << moment << "] from MGF M(t) = " << mgf << " ";
		ss << "E[X^k] = M^(k)(0) = k-th derivative of M(t) evaluated at t=0";
		try
		{
			const auto e = Calculus::parse(mgf);
			auto d = e;
			for (int k = 1; k <= moment; ++k)
			{
				d = Calculus::simplify(Calculus::diff(d, "t"));
				if (k <= 3)
				{
					ss << "M^(" << k << ")(t) = " << Calculus::toString(d) << " ";
				}
			}
			const double val = Calculus::evaluate(d, {{"t", 0.0}});
			ss << "E[X ^ " << moment << "] = M ^ (" << moment << ")(0) = " << fmt(val);
			return ok(fmt(val), ss.str());
		}
		catch (const std::exception &ex)
		{
			ss << "Symbolic differentiation error: " << ex.what() << " ";
			ss << "Use numerical differentiation: M^(k)(0) ≈ [finite differences at t=0]";
			return ok(ss.str());
		}
	}

	PTResult orderStatDist(const int n, const int k, const std::string &dist, const Vec &params)
	{
		// PDF of k-th order stistic X_(k) from n iid samples
		// f_{(k)}(x) = n!/(k-1)!(n-k)! F(x)^{k-1} (1-F(x))^{n-k} f(x)
		std::ostringstream ss;
		ss << "Order Statistic X_(" << k << ") from n=" << n << " iid " << dist << " samples";
		ss << "PDF: f_{(" << k << ")}(x) = n!/((" << k - 1 << ")!(" << n - k << ")!) · F(x)^{" << k - 1 << "} · (1-F(x))^{" << n - k << "} · f(x)";

		const double mu = params.empty() ? 0 : params[0];
		const double sigma = params.size() > 1 ? params[1] : 1;

		if (dist == "uniform")
		{
			const double a = mu;
			// X~U(a,b): X_(k) ~ Beta(k, n-k+1) scaled to [a,b]
			double b = sigma;
			const double EXk = a + (b - a) * k / (n + 1.0);
			const double VarXk = (b - a) * (b - a) * k * (n - k + 1.0) / ((n + 1.0) * (n + 1.0) * (n + 2.0));
			ss << "For Uniform[" << a << "," << b << "]: ";
			ss << "X_(" << k << ") ~ Beta(" << k << "," << n - k + 1 << ") scaled to [" << a << "," << b << "]";
			ss << "E[X_(" << k << ")] = " << fmt(EXk) << " ";
			ss << "Var(X_(" << k << ")) = " << fmt(VarXk) << " ";
			ss << "Special cases: ";
			ss << "  Minimum X_(1): E = " << fmt(a + (b - a) / (n + 1.0)) << " ";
			ss << "  Maximum X_(" << n << "): E = " << fmt(a + n * (b - a) / (n + 1.0)) << " ";
			ss << "  Median X_(" << (n + 1) / 2 << "): E = " << fmt((a + b) / 2) << " ";
		}
		else if (dist == "exponential")
		{
			const double lambda = mu > 0 ? mu : 1;
			// E[X_(k)] = (1/λ) Σ_{j=n-k+1}^{n} 1/j
			double EXk = 0;
			for (int j = n - k + 1; j <= n; ++j)
				EXk += 1.0 / j;
			EXk /= lambda;
			ss << "For Exponential(λ=" << lambda << "): ";
			ss << "E[X_(" << k << ")] = (1/λ) Σ_{j=" << n - k + 1 << "}^{" << n << "} 1/j = " << fmt(EXk) << " ";
			ss << "Spacings X_(k)-X_(k-1) are independent Exp(λ(n-k+1))";
		}
		else
		{
			ss << "General: evaluate the integral numerically for " << dist << ".";
		}
		return ok(ss.str());
	}

	PTResult spacings(const int n, const double lambda)
	{
		// Exponential spacings: if X_(1) ≤ ... ≤ X_(n) from Exp(λ),
		// define D_k = (n-k+1)(X_(k) - X_(k-1))  (with X_(0)=0)
		// Then D_1,...,iid Exp(λ)
		std::ostringstream ss;
		ss << "Exponential Spacings (Rényi's Theorem)";
		ss << "Order statistics from X_1,...,X_n ~ Exp(λ=" << lambda << ")";
		ss << "Define normalised spacings: ";
		ss << "  D_k = (n-k+1)(X_(k) - X_(k-1)),  k=1,...,n  (X_(0)=0)  ";
		ss << "Rényi's theorem: D_1,...,D_n are iid Exp(λ)";
		ss << "Properties: ";
		ss << "  E[D_k] = 1/λ = " << fmt(1.0 / lambda) << " ";
		ss << "  E[X_(k)] = (1/λ) Σ_{j=n-k+1}^{n} 1/j";
		ss << "Order statistic moments: ";
		for (int k = 1; k <= std::min(n, 5); ++k)
		{
			double EXk = 0;
			for (int j = n - k + 1; j <= n; ++j)
				EXk += 1.0 / j;
			EXk /= lambda;
			ss << "  E[X_(" << k << ")] = " << fmt(EXk) << " ";
		}
		ss << "Application: test for exponentiality using spacing statistics.";
		return ok(ss.str());
	}

	// [parseMat2 removed — use MathCore.h]

	PTResult dispatch(const std::string &op, const std::string &json)
	{
		try
		{
			if (op == "mgf_normal")
				return mgfNormal(getN(json, "mu"), getN(json, "sigma"), getN(json, "t"));
	if (op == "mgf_exp")
				return mgfExponential(getN(json, "lambda"), getN(json, "t"));
			if (op == "mgf_binomial")
				return mgfBinomial((int)getN(json, "n"), getN(json, "p"), getN(json, "t"));
			if (op == "mgf_poisson")
				return mgfPoisson(getN(json, "lambda"), getN(json, "t"));
			if (op == "mgf_gamma")
				return mgfGamma(getN(json, "alpha"), getN(json, "beta"), getN(json, "t"));
			if (op == "mgf_uniform")
				return mgfUniform(getN(json, "a"), getN(json, "b"), getN(json, "t"));
			if (op == "moments_mgf")
				return momentsByMGF(getP(json, "dist"), parseVec(getP(json, "params")), (int)getN(json, "k", 4));
			if (op == "cumulant")
				return cumulantFromMGF(getP(json, "dist"), parseVec(getP(json, "params")), (int)getN(json, "k", 1));
			if (op == "pgf")
				return probabilityGF(parseVec(getP(json, "probs")), getN(json, "z", 1));
			if (op == "char_normal")
				return charFnNormal(getN(json, "mu"), getN(json, "sigma"), getN(json, "t"));
			if (op == "char_poisson")
				return charFnPoisson(getN(json, "lambda"), getN(json, "t"));
			if (op == "char_cauchy")
				return charFnCauchy(getN(json, "x0"), getN(json, "gamma"), getN(json, "t"));
			if (op == "berry_esseen")
				return berryEsseen(getN(json, "mu"), getN(json, "sigma"), getN(json, "rho"), (int)getN(json, "n", 100));
			if (op == "clt_approx")
				return cltApproximate(getN(json, "mu"), getN(json, "sigma"), (int)getN(json, "n"), getN(json, "x"));
			if (op == "wlln")
				return weakLLN(getP(json, "dist"), parseVec(getP(json, "params")), (int)getN(json, "n", 100));
			if (op == "slln")
				return strongLLN(getP(json, "dist"), parseVec(getP(json, "params")), (int)getN(json, "n", 1000));
			if (op == "markov_ineq")
				return markovInequality(getN(json, "mu"), getN(json, "a"));
			if (op == "chebyshev_ineq")
				return chebyshevInequality(getN(json, "mu"), getN(json, "sigma"), getN(json, "k", 2));
			if (op == "chernoff")
				return chernoffBound(getN(json, "mu"), getN(json, "delta", 0.5));
			if (op == "hoeffding")
				return hoeffdingBound((int)getN(json, "n"), getN(json, "a"), getN(json, "b"), getN(json, "t", 0.1));
			if (op == "azuma")
				return azumaHoeffding((int)getN(json, "n"), getN(json, "c"), getN(json, "t"));
			if (op == "poisson_proc")
				return poissonProcess(getN(json, "lambda"), getN(json, "t"), (int)getN(json, "k", 10));
			if (op == "brownian")
				return brownianMotion(getN(json, "t", 1), (int)getN(json, "steps", 100));
			if (op == "gbm")
				return geometricBrownian(getN(json, "S0", 100), getN(json, "mu", 0.05), getN(json, "sigma", 0.2),
										 getN(json, "t", 1), (int)getN(json, "steps", 252));
			if (op == "ou")
				return ornsteinUhlenbeck(getN(json, "theta", 1), getN(json, "mu", 0), getN(json, "sigma", 1),
										 getN(json, "x0"), getN(json, "t", 1), (int)getN(json, "steps", 100));
			if (op == "random_walk")
				return randomWalkSymm((int)getN(json, "steps", 100), (int)getN(json, "trials", 1000));
			if (op == "gamblers_ruin")
				return gamblersRuin(getN(json, "p", 0.5), (int)getN(json, "start"), (int)getN(json, "target"));
			if (op == "opt_stopping")
				return optionalStopping(getN(json, "mu"), getN(json, "sigma"), getN(json, "a"), getN(json, "b"));
			if (op == "doob_decomp")
				return doobDecomp(parseVec(getP(json, "xs")));
			if (op == "martingale_conv")
				return martingaleConverg(getN(json, "mu"), getN(json, "sigma"), (int)getN(json, "n", 100));
			if (op == "joint_normal")
				return jointNormal(getN(json, "mu1"), getN(json, "mu2"), getN(json, "s1"), getN(json, "s2"),
								   getN(json, "rho"), getN(json, "x1"), getN(json, "x2"));
			if (op == "cond_normal")
				return conditionalNormal(getN(json, "mu1"), getN(json, "mu2"), getN(json, "s1"), getN(json, "s2"),
										 getN(json, "rho"), getN(json, "x2"));
			if (op == "mc_integrate")
				return mcEstimate(getP(json, "f"), getN(json, "a"), getN(json, "b"),
								  static_cast<int>(getN(json, "n", 100000)));
			if (op == "clt_demo")
				return cltDemo(getP(json, "dist"), parseVec(getP(json, "params")),
							   static_cast<int>(getN(json, "n", 30)),
							   static_cast<int>(getN(json, "reps", 1000)));
			if (op == "cov_matrix")
			{
				auto d = parseMat(getP(json, "data"));
				return covarianceMatrix(d);
			}
			if (op == "corr_matrix")
			{
				auto d = parseMat(getP(json, "data"));
				return correlationMatrix(d);
			}
			if (op == "doob_maximal")
				return doobMaximal(getN(json, "mu"), getN(json, "sigma"), getN(json, "a"));
			if (op == "extreme_value")
				return extremeValue((int)getN(json, "n"), getP(json, "dist"), parseVec(getP(json, "params")));
			if (op == "importance_samp")
				return importanceSampling(getN(json, "mu"), getN(json, "sigma"), getN(json, "threshold"),
										  static_cast<int>(getN(json, "n", 10000)));
			if (op == "lst")
				return laplaceStieltjes(getP(json, "dist"), parseVec(getP(json, "params")), getN(json, "s"));
			if (op == "levy_cont")
				return levyContinuity(parseVec(getP(json, "values")), parseVec(getP(json, "t")));
			if (op == "mgf_derive")
				return mgfDerive(getP(json, "mgf"), static_cast<int>(getN(json, "k", 1)));
			if (op == "order_stat")
				return orderStatDist(static_cast<int>(getN(json, "n")), static_cast<int>(getN(json, "k", 1)),
									 getP(json, "dist"),
									 parseVec(getP(json, "params")));
			if (op == "spacings")
				return spacings(static_cast<int>(getN(json, "n")), getN(json, "lambda", 1));

			return err("Unknown probability theory operation: " + op);
		}
		catch (const std::exception &e)
		{
			return err(e.what());
		}
	}
} // namespace ProbabilityTheory
