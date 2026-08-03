#pragma once
// =============================================================================
// calculus/limits/Limits.h
//
// Limit evaluation using four strategies applied in order:
//
//   1. DIRECT SUBSTITUTION
//      Substitute x = a and evaluate. If the result is finite and
//      well-defined (no 0/0, ∞/∞, etc.), return it immediately.
//
//   2. ALGEBRAIC SIMPLIFICATION
//      Simplify the expression (factor, cancel) then try substitution again.
//      Handles cases like (x²-1)/(x-1) → (x+1) → 2 at x=1.
//      Implemented by evaluating near the limit point from both sides
//      and checking that the simplified form matches.
//
//   3. L'HÔPITAL'S RULE
//      If the limit has the indeterminate form 0/0 or ∞/∞:
//        lim f/g = lim f'/g'
//      Applied up to MAX_LHOPITAL times. Requires expr to be a DIV node
//      or reducible to one.
//
//   4. NUMERICAL APPROACH
//      Evaluate at points approaching a from both sides and check for
//      convergence. Used as fallback when symbolic methods fail.
//      Returns numerical value with a note that it's approximate.
//
// One-sided limits:
//   limitLeft (x → a⁻):   approach from x < a
//   limitRight(x → a⁺):   approach from x > a
//
// Limits at infinity:
//   limitInf (x → +∞):    evaluate at large positive x
//   limitNegInf(x → -∞):  evaluate at large negative x
//
// Special indeterminate forms handled:
//   0/0,  ∞/∞  →  L'Hôpital
//   0*∞        →  rewrite as 0/(1/∞) then L'Hôpital
//   ∞-∞        →  rewrite with common denominator
//   0^0, 1^∞, ∞^0 →  rewrite as e^(ln form) then L'Hôpital
// =============================================================================

#ifndef LIMITS_HPP
#define LIMITS_HPP

#include "../core/Expression.hpp"
#include "../core/Simplify.hpp"
#include "../differentiation/Derivative.hpp"
#include <string>
#include <optional>

namespace Calculus
{

	// =============================================================================
	// Core limit functions
	// =============================================================================

	// General two-sided limit:  lim_{var → point} expr
	// point is given as a double (e.g. 0.0, 1.0, M_PI)
	// Returns the limit value, or throws if it cannot be determined.
	double limitAt(const ExprPtr &expr,
				   const std::string &var,
				   double point);

	// One-sided limits
	double limitLeft(const ExprPtr &expr, const std::string &var, double point);
	double limitRight(const ExprPtr &expr, const std::string &var, double point);

	// Limits at infinity
	double limitInf(const ExprPtr &expr, const std::string &var);
	double limitNegInf(const ExprPtr &expr, const std::string &var);

	// =============================================================================
	// Formatted result
	// =============================================================================

	enum class LimitDirection
	{
		BOTH,	 // two-sided (default)
		LEFT,	 // x -> a⁻
		RIGHT,	 // x -> a⁺
		POS_INF, // x -> +∞
		NEG_INF, // x -> -∞
	};

	struct LimitResult
	{
		std::string value;	// symbolic if recognizable, else decimal
		std::string latex;	// LaTeX form of the result
		std::string method; // which strategy succeeded
		bool exists = true; // false if left ≠ right limit
		bool ok = true;
		std::string error;
	};

	// Main entry point: parse expr string, compute limit, format result.
	// pointStr: "0", "1", "pi", "inf", "-inf", etc.
	LimitResult computeLimit(const std::string &exprStr,
							 const std::string &var,
							 const std::string &pointStr,
							 LimitDirection direction = LimitDirection::BOTH);

	// =============================================================================
	// Individual strategies (exposed for testing / step-by-step display)
	// =============================================================================

	// Try direct substitution. Returns empty optional if indeterminate.
	std::optional<double> tryDirectSubstitution(const ExprPtr &expr,
												const std::string &var,
												double point);

	// Try L'Hôpital. Returns empty optional if not applicable or failed.
	// MAX_LHOPITAL = 5 applications.
	std::optional<double> tryHopital(const ExprPtr &expr,
									 const std::string &var,
									 double point);

	// Numerical approach: evaluate approaching from the given side.
	// side = +1 for right, -1 for left.
	double numericalLimit(const ExprPtr &expr,
						  const std::string &var,
						  double point,
						  int side);

	// Numerical limit at infinity.
	// direction = +1 for +∞, -1 for -∞.
	double numericalLimitInf(const ExprPtr &expr,
							 const std::string &var,
							 int direction);

	// =============================================================================
	// Helper: parse a point string to a double
	// Handles "0", "1", "pi", "e", "inf", "-inf", "-pi/2" etc.
	// =============================================================================
	double parsePoint(const std::string &pointStr);

}

#endif // LIMITS_HPP