#pragma once
// calculus/vectorcalc/VectorOps.npp
// Vector differential operators: grad, div, curl, laplacian.

#ifndef VECTOROPS_HPP
#define VECTOROPS_HPP

#include "../core/Expression.hpp"
#include "../core/Simplify.hpp"
#include "../differentiation/Partial.hpp"
#include <string>
#include <vector>

namespace Calculus {

struct VecResult {
	std::vector<std::string> components;  // one per dimension
	std::vector<std::string> latex;
	std::string              scalar;      // for scalar results (dic, Laplacian)
	std::string              scalar_latex;
	bool                     ok = true;
	std::string              error;
};

// ∇f — gradient of scalar field f(x,y) or f(x,y,z)
VecResult computeGrad(const std::string& f, const std::vector<std::string>& vars);

// ∇*F - divergence of vector field F = [P,Q] or [P,Q,R]
VecResult computeDiv(const std::vector<std::string>& F, const std::vector<std::string>& vars);

// ∇xF - curl of 3D vector field F = [P,Q,R]
// Returns [∂R/∂y-∂Q/∂z, ∂P/∂z-∂R/∂x, ∂Q/∂x-∂P/∂y]
VecResult computeCurl(const std::vector<std::string>& F, const std::vector<std::string>& vars);

// ∇²f — Laplacian of scalar field
VecResult computeLaplacianVec(const std::string& f, const std::vector<std::string>& vars);

// ∇²F — vector Laplacian (Laplacian applied to each component)
VecResult computeVectorLaplacian(const std::vector<std::string>& F, const std::vector<std::string>& vars);

}

#endif //VECTOROPS_HPP