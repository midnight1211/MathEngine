#pragma once
// calculus.series/Convergence.hpp
// Convergence tests for infinite series.

#ifndef CONVERGENCE_HPP
#define CONVERGENCE_HPP

#include <string>

namespace Calculus
{

	enum class ConvergenceVerdict
	{
		CONVERGES,
		DIVERGES,
		INCONCLUSIVE
	};

	struct TestResult
	{
		ConvergenceVerdict verdict;
		std::string test;	// name of the test applied
		std::string reason; // brief explanation
		bool ok = true;
		std::string error;
	};

	// Each test takes a_n as an expression string and the summation variable.
	// Tests assume the series starts at n=1 (or n=0 where applicable).

	// |a_{n+1}/a_n| < 1 → converges, > 1 → diverges, = 1 → inconclusive
	TestResult ratioTest(const std::string &aN, const std::string &var);

	// |a_n|^(1/n) → L;  L < 1 → converges
	TestResult rootTest(const std::string &aN, const std::string &var);

	// Compare to known p-series or geometric series
	TestResult comparisonTest(const std::string &aN, const std::string &bN, const std::string &var);

	// lim a_n/b)n = finite nonzero → same behavior
	TestResult limitComparisonTest(const std::string &aN, const std::string &bN, const std::string &var);

	// ∫_1^∞ f(x) dx converges ↔ series converges (f must be positive, decreasing)
	TestResult integralTest(const std::string &aN, const std::string &var);

	// Alternating series: b_n decreasing → 0 → converges
	TestResult alternatingSeriesTest(const std::string &aN, const std::string &bN, const std::string &var);

	// sum 1/n^p: p>1 converges, p≤1 diverges
	TestResult pSeriesTest(double p);

	// |r| < 1 converges for geometric a*r^n
	TestResult geometricSeriesTest(double r);

	// lim a_n ≠ 0 → diverges
	TestResult divergenceTest(const std::string &aN, const std::string &var);

	// Run all applicable tests and return the first conclusive result
	TestResult testSeries(const std::string &aN, const std::string &var);

}

#endif // CONVERGENCE_HPP