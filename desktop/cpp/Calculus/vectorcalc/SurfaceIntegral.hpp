#pragma once
// calculus/vectorcalc/SurfaceIntegral.hpp
// Scalar and vector (flux) surface integrals.

#ifndef SURFACEINTEGRAL_HPP
#define SURFACEINTEGRAL_HPP

#include "../core/Expression.hpp"
#include "../integration/Numerical.hpp"
#include <string>
#include <vector>

namespace Calculus {

struct SurfaceIntResult {
	double      value = 0.0;
	std::string method;
	bool        ok    = true;
	std::string error;
};

// Scalar surface integral: ∫∫_S f dS
// Surface parametrised as r(u,v) = [xExpr, yExpr, zExpr], (u,v) ∈ [u1,u2]×[v1,v2]
// dS = |∂r/∂u × ∂r/∂v| du dv
SurfaceIntResult scalarSurfaceIntegral(
	const std::string& f,						   // scalar field in x,y,z
	const std::vector<std::string>& surfaceExprs,  // [x(u,v), y(u,v), z(u,v)]
	const std::vector<std::string>& fieldVars,	   // ["x","y","z"]
	const std::string& u, const std::string& v,
	double u1, double u2,
	double v1, double v2);

// Flux integral: ∫∫_S F·dS = ∫∫_S F·n dS
// F = [P,Q,R] in x,y,z
SurfaceIntResult fluxIntegral(
	const std::vector<std::string>& F,
	const std::vector<std::string>& surfaceExprs,
	const std::vector<std::string>& fieldVars,
	const std::string& u, const std::string& v,
	double u1, double u2,
	double v1, double v2);

} // Calculus

#endif //SURFACEINTEGRAL_HPP