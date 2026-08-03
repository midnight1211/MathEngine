#pragma once
// calculus/vectorcalc/Theorems.hpp
// Green's, Stoke's, and Divergence Theorem - verification and application

#ifndef THEOREMS_HPP

#include "LineIntegral.hpp"
#include "SurfaceIntegral.hpp"
#include "VectorOps.hpp"
#include <string>

namespace Calculus {

struct TheoremResult {
	double      lhs = 0.0;
	double      rhs = 0.0;
	bool        verified = false;
	std::string statement;
	bool        ok = true;
	std::string error;
};

// Green's Theorem: ∮_C (P dx + Q dy) = ∬_D (∂Q/∂x - ∂P/∂y) dA
// Verifies both sides numerically.
// boundary: parametric curve [x(t),y(t)], t in [t1,t2], traversed CCW
// region: rectangular [x1,x2]×[y1,y2]
TheoremResult greensTheorem(
	const std::string& P, const std::string& Q,
	const std::vector<std::string>& boundary,   // [x(t), y(t)]
	double t1, double t2,
	double x1, double x2, double y1, double y2);

// Stokes' Theorem: ∮_C F·dr = ∬_S (∇×F)·dS
// Verifies both sides numerically.
TheoremResult stokesTheorem(
	const std::vector<std::string>& F,          // [P,Q,R]
	const std::vector<std::string>& curve,       // [x(t),y(t),z(t)]
	const std::vector<std::string>& surface,     // [x(u,v),y(u,v),z(u,v)]
	const std::vector<std::string>& fieldVars,   // ["x","y","z"]
	double t1, double t2,
	double u1, double u2, double v1, double v2);

// Divergence Theorem: ∬_S F·dS = ∭_V (∇·F) dV
// Verifies both sides numerically over a box volume.
TheoremResult divergenceTheorem(
	const std::vector<std::string>& F,
	const std::vector<std::string>& surface,
	const std::vector<std::string>& fieldVars,
	double u1, double u2, double v1, double v2,
	double x1, double x2, double y1, double y2, double z1, double z2);

} // Calculus

#endif //THEOREMS_HPP