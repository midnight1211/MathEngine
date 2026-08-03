#pragma once
// =============================================================================
// calculus/differentiation/Derivative.h
//
// Symbolic differentiation of ExprPtr trees.
//
// The core function is diff(expr, var) which returns d(expr)/d(var) as a new
// ExprPtr tree. The result is automatically simplified before being returned.
//
// Rules implemented:
//   Constants:       d/dx [c]       = 0
//   Variable:        d/dx [x]       = 1,   d/dx [y] = 0
//   Sum:             d/dx [f+g]     = f' + g'
//   Difference:      d/dx [f-g]     = f' - g'
//   Product:         d/dx [f*g]     = f'g + fg'
//   Quotient:        d/dx [f/g]     = (f'g - fg') / g^2
//   Power (const):   d/dx [f^n]     = n * f^(n-1) * f'
//   Power (const base): d/dx [a^g]  = a^g * ln(a) * g'
//   Power (general): d/dx [f^g]     = f^g * (g'*ln(f) + g*f'/f)
//   Chain rule is applied automatically in every case above via f'.
//
//   Trig:            d/dx [sin f]   = cos(f) * f'
//                    d/dx [cos f]   = -sin(f) * f'
//                    d/dx [tan f]   = sec^2(f) * f'
//                    d/dx [csc f]   = -csc(f)*cot(f) * f'
//                    d/dx [sec f]   = sec(f)*tan(f) * f'
//                    d/dx [cot f]   = -csc^2(f) * f'
//
//   Inverse trig:    d/dx [asin f]  = f' / sqrt(1 - f^2)
//                    d/dx [acos f]  = -f' / sqrt(1 - f^2)
//                    d/dx [atan f]  = f' / (1 + f^2)
//
//   Hyperbolic:      d/dx [sinh f]  = cosh(f) * f'
//                    d/dx [cosh f]  = sinh(f) * f'
//                    d/dx [tanh f]  = sech^2(f) * f'   = f' / cosh^2(f)
//                    d/dx [asinh f] = f' / sqrt(f^2 + 1)
//                    d/dx [acosh f] = f' / sqrt(f^2 - 1)
//                    d/dx [atanh f] = f' / (1 - f^2)
//
//   Exponential:     d/dx [e^f]     = e^f * f'
//                    d/dx [exp(f)]  = exp(f) * f'
//   Logarithm:       d/dx [ln f]    = f' / f
//                    d/dx [log10 f] = f' / (f * ln(10))
//                    d/dx [log2 f]  = f' / (f * ln(2))
//                    d/dx [log_b f] = f' / (f * ln(b))
//
//   Roots:           d/dx [sqrt f]  = f' / (2*sqrt(f))
//                    d/dx [cbrt f]  = f' / (3 * f^(2/3))
//
//   Absolute value:  d/dx [|f|]     = f * f' / |f|    (sign(f) * f')
// =============================================================================

#ifndef DERIVATIVE_HPP
#define DERIVATIVE_HPP

#include "../core/Expression.hpp"
#include "../core/Simplify.hpp"
#include <string>

namespace Calculus
{

	// =============================================================================
	// Core differentiation
	// =============================================================================

	// Differentiate expr with respect to variable var.
	// Result is simplified before returning.
	// Throws std::runtime_error if expr contains an unknown FUNC node.
	ExprPtr diff(const ExprPtr &expr, const std::string &var);

	// nth derivative: apply diff n times
	ExprPtr diffN(const ExprPtr &expr, const std::string &var, int n);

	// =============================================================================
	// Formatted result (for direct use by CoreEngine / REST API)
	// =============================================================================

	struct DiffResult
	{
		std::string symbolic;  // simplified infix string
		std::string latex;	   // LaTeX string
		std::string numerical; // numeric value if expr has no free variables after diff
		bool ok = true;
		std::string error;
	};

	// Differentiate and return a formatted result.
	// InputExpr and varName are strings; parsing uses Week4Bridge.
	DiffResult differentiate(const std::string &inputExpr, const std::string &varName, int order = 1);
}

#endif // DERIVATIVE_HPP