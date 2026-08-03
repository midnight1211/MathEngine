#pragma once
// calculus/integration/Numerical.h
// Methods implemented:
//   RIEMANN SUMS
//     riemannLeft    — left endpoint rule
//     riemannRight   — right endpoint rule
//     riemannMid     — midpoint rule
//   NEWTON-COTES (closed)
//     trapezoidal    — O(h²) per step
//   COMPOSITE ADAPTIVE
//     adaptiveSimpson — recursively subdivides until error < tol
//   GAUSSIAN QUADRATURE (Gauss-Legendre)
//   IMPROPER INTEGRALS
//     improperBothInf   — ∫_-∞^∞
// both parsed ExprPtr trees and raw lambdas.

#ifndef NUMERICAL_HPP
#define NUMERICAL_HPP

#include "../core/Expression.hpp"
#include <functional>
#include <string>

namespace Calculus {

using Func1D = std::function<double(double)>;

// Result type

struct NumericalResult {
	double      value     = 0.0;
	double      errorEst  = 0.0;
	int         evals     = 0;
	std::string method;
	bool        ok        = true;
	std::string error;
};

// Riemann sums

NumericalResult riemannLeft (const Func1D& f, double a, double b, int n);
NumericalResult riemannRight(const Func1D& f, double a, double b, int n);
NumericalResult riemannMid  (const Func1D& f, double a, double b, int n);

// Newton-Cotes

NumericalResult trapezoidal (const Func1D& f, double a, double b, int n);
NumericalResult simpson13   (const Func1D& f, double a, double b, int n);  // n must be even
NumericalResult simpson38   (const Func1D& f, double a, double b, int n);  // n div by 3
NumericalResult boole       (const Func1D& f, double a, double b, int n);  // n div by 4

// Adaptive and extrapolation

NumericalResult adaptiveSimpson(const Func1D& f, double a, double b,
								double tol = 1e-8, int maxDepth = 50);

// Teturns the most accurate estimate (bottom-right of table)
NumericalResult romberg(const Func1D& f, double a, double b, int maxRows = 8);

// Gaussian quadrature (Gauss-Legendre)

// n-point Gauss-Legendre quadrature, n in [2, 5]
NumericalResult gaussLegendre(const Func1D& f, double a, double b, int n = 5);

// Improper integrals

// ∫_a^+∞  f(x) dx   (a must be finite)
NumericalResult improperInfinite(const Func1D& f, double a);

// ∫_-∞^b  f(x) dx   (b must be finite)
NumericalResult improperNegInfinite(const Func1D& f, double b);

// ∫_-∞^+∞  f(x) dx
NumericalResult improperBothInfinite(const Func1D& f);

// Convenience: build a Func1D from an ExprPtr

Func1D makeEvaluator(const ExprPtr& expr, const std::string& varName);

// Formatted entry point

struct IntegralResult {
	std::string value;
	std::string method;
	double      raw   = 0.0;
	double      error = 0.0;
	bool        ok    = true;
	std::string errorMsg;
};

// Compute 	∫_a^b expr dx using the specified method.
IntegralResult computeNumerical(const std::string& exprStr,
	                            const std::string& var,
	                            double a, double b,
	                            const std::string& method = "auto",
	                            int n = 1000);

}

#endif //NUMERICAL_HPP