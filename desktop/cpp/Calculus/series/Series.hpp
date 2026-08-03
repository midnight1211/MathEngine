#pragma once
// calculus/series/Series.hpp
// Taylor, Maclaurin, Laurent, and power series

#ifndef SERIES_HPP
#define SERIES_HPP

#include "../core/Expression.hpp"
#include "../core/Simplify.hpp"
#include "../differentiation/Derivative.hpp"
#include <vector>
#include <string>
#include <optional>

namespace Calculus
{

	// One term of a series: coefficient * (x - center)^power
	struct SeriesTerm
	{
		double coefficient;
		int power;
		std::string symbolic; // formatted term string
	};

	struct SeriesResult
	{
		std::vector<SeriesTerm> terms;
		std::string polynomial;
		std::string polynomial_latex;
		std::string remainder;
		double radiusOfConv;
		bool ok = true;
		std::string error;
	};

	// Taylor series of f around x=center to order n
	// f(x) = sum_{k=0}^{n} f^{k}(center)/k! * (x - center)^k
	SeriesResult taylorSeries(const ExprPtr &f, const std::string &var, double center, int n);

	// Maclaurin = Taylor at center=0
	SeriesResult maclaurinSeries(const ExprPtr &f, const std::string &var, int n);

	// Radius of convergence via ratio test on coefficients
	double radiusOfConvergence(const std::vector<double> &coefficients);

	// Power series: given coefficients a_n, return sum string and radius
	SeriesResult powerSeries(const std::vector<double> &coefficients, const std::string &var, double center = 0.0);

	// Formatted entry points
	SeriesResult computeTaylor(const std::string &exprStr, const std::string &var, double center, int order);

	SeriesResult computeMaclaurin(const std::string &exprStr, const std::string &var, int order);

}

#endif // SERIES_HPP