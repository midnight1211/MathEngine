#pragma once
// calculus/vectorcalc/LineIntegral.hpp
// Scalar and vector line integrals along parametric curves.

#ifndef LINEINTEGRAL_HPP
#define LINEINTEGRAL_HPP

#include "../core/Expression.hpp"
#include "../integration/Numerical.hpp"
#include <string>
#include <vector>

namespace Calculus {

struct LineIntResult {
	double      value = 0.0;
	std::string method;
	bool        ok   = true;
	std::string error;
};

// Scalar line integral: ∫_C f(x,y,z) ds
// Curve parametrised as r(t) = [xExpr(t), yExpr(t), zExpr(t)], t ∈ [a,b]
// ds = |r'(t)| dt
// f is a string in x,y,z; curve components are strings in t
LineIntResult scalarLineIntegral(
	const std::string& f,
	const std::vector<std::string>& curveExprs,   // [x(t), y(t)] or [x(t),y(t),z(t)]
	const std::vector<std::string>& fieldVars,    // ["x","y"] or ["x","y","z"]
	const std::string& param,					  // "t"
	double a, double b);

// Vector line integral (work integral): ∫_C F·dr
// F = [P, Q] or [P, Q, R]  (strings in x,y,z)
// ∫_C F·dr = ∫_a^b F(r(t))·r'(t) dt
LineIntResult vectorLineIntegral(
	const std::vector<std::string>& f,
	const std::vector<std::string>& curveExprs,
	const std::vector<std::string>& fieldVars,
	const std::string& param,
	double a, double b);

// Green's theorem check: is F = [P,Q] conservative? (∂P/∂y == ∂Q/∂x)
bool isConservative2D(const std::string& P, const std::string& Q, const std::string& x, const std::string& y);

}

#endif //LINEINTEGRAL_HPP