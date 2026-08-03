#pragma once
// calculus/optimization/Optimize.hpp
// Critical points, classification, and constrained optimization

#ifndef OPTIMIZE_HPP
#define OPTIMIZE_HPP

#include "../core/Expression.hpp"
#include "../differentiation/Partial.hpp"
#include "../integration/Numerical.hpp"
#include <string>
#include <vector>

namespace Calculus
{

	// ── Single-variable ───────────────────────────────────────────────────────────

	struct CriticalPoint1D
	{
		double x;
		double fVal;
		std::string type; // "local min", "local max", "saddle/inflection", "unknown"
	};

	struct OptResult1D
	{
		std::vector<CriticalPoint1D> points;
		bool ok = true;
		std::string error;
	};

	// Find critical points of f(x) on [a, b] and classify them.
	OptResult1D findCriticalPoints1D(const std::string &f, const std::string &var, double a, double b);

	// ── Multivariable ─────────────────────────────────────────────────────────────

	struct CriticalPointND
	{
		std::vector<double> point;
		double fVal;
		std::string type;
		double det;
	};

	struct OptResultND
	{
		std::vector<CriticalPointND> points;
		bool ok = true;
		std::string error;
	};

	// Find and classify critical points of f(vars) by solving ∇f = 0 numerically.
	OptResultND findCriticalPointsND(const std::string &f, const std::vector<std::string> &vars,
									 const std::vector<double> &searchMin, const std::vector<double> &searchMax);

	// ── Lagrange multipliers ──────────────────────────────────────────────────────

	struct LagrangeResult
	{
		std::vector<std::vector<double>> candidates; // (x, y, ..., lambda)
		std::vector<double> fVals;
		std::string summary;
		bool ok = true;
		std::string error;
	};

	// Optimize f(x,y) subject to g(x,y) = 0 via Lagrange multipliers.
	// Solves: ∇f = λ∇g, g = 0  numerically.
	LagrangeResult lagrangeMultipliers(
		const std::string &f,
		const std::string &g,
		const std::vector<std::string> &vars,
		const std::vector<double> &searchMin,
		const std::vector<double> &searchMax);

}

#endif // OPTIMIZE_HPP