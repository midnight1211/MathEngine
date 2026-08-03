#pragma once
// =============================================================================
// calculus/differentiation/Partial.h
//
// Partial differentiation and multivariable differential operators.
//
// Everything here is built on top of diff() from Derivative.h —
// a partial derivative ∂f/∂x is just diff(f, "x") treating all other
// variables as constants, which is exactly what diff() already does.
//
// Operations:
//
//   PARTIAL DERIVATIVES
//     partial(f, x)          ∂f/∂x
//     partialN(f, x, n)      ∂ⁿf/∂xⁿ
//     mixed(f, x, y)         ∂²f/∂x∂y  =  diff(diff(f, x), y)
//
//   GRADIENT
//     gradient(f, vars)      ∇f = [∂f/∂x₁, ∂f/∂x₂, ..., ∂f/∂xₙ]
//
//   DIRECTIONAL DERIVATIVE
//     directional(f, vars, u) D_u f = ∇f · û  (u normalised internally)
//
//   JACOBIAN
//     jacobian(F, vars)      J[i][j] = ∂Fᵢ/∂xⱼ   (m×n matrix)
//     F is a vector of expressions [f₁, f₂, ..., fₘ]
//
//   HESSIAN
//     hessian(f, vars)       H[i][j] = ∂²f/∂xᵢ∂xⱼ   (n×n matrix)
//
//   LAPLACIAN
//     laplacian(f, vars)     ∇²f = ∂²f/∂x₁² + ... + ∂²f/∂xₙ²
// =============================================================================

#ifndef PARTIAL_HPP
#define PARTIAL_HPP

#include "../core/Expression.hpp"
#include "../core/Simplify.hpp"
#include "Derivative.hpp"
#include <vector>
#include <string>

namespace Calculus
{

    // ── Types used throughout ─────────────────────────────────────────────────────

    // A vector of expressions — used for gradient, Jacobian rows, etc.
    using ExprVec = std::vector<ExprPtr>;

    // A matrix of expressions — used for Jacobian, Hessian
    using ExprMat = std::vector<std::vector<ExprPtr>>;

    // Formatted result returned to callers
    struct PartialResult
    {
        std::string symbolic;
        std::string latex;
        std::string numerical; // only set if result is a constant
        bool ok = true;
        std::string error;
    };

    // =============================================================================
    // Partial derivatives
    // =============================================================================

    // ∂f/∂var  — identical to diff(f, var), exposed here for naming clarity
    ExprPtr partial(const ExprPtr &f, const std::string &var);

    // ∂ⁿf/∂varⁿ
    ExprPtr partialN(const ExprPtr &f, const std::string &var, int n);

    // ∂²f/∂x∂y  (mixed partial — Clairaut's theorem: order doesn't matter for
    // smooth functions, but the implementation respects the given order)
    ExprPtr mixed(const ExprPtr &f,
                  const std::string &var1,
                  const std::string &var2);

    // =============================================================================
    // Gradient  ∇f
    // =============================================================================

    // Returns [∂f/∂vars[0], ∂f/∂vars[1], ..., ∂f/∂vars[n-1]]
    ExprVec gradient(const ExprPtr &f, const std::vector<std::string> &vars);

    // =============================================================================
    // Directional derivative
    // =============================================================================

    // D_u f = ∇f · û   where û = u / |u|
    // u is given as a vector of doubles (the direction)
    // Returns a scalar ExprPtr
    ExprPtr directional(const ExprPtr &f,
                        const std::vector<std::string> &vars,
                        const std::vector<double> &u);

    // =============================================================================
    // Jacobian
    // =============================================================================

    // J[i][j] = ∂F[i]/∂vars[j]
    // F is a list of m scalar expressions (the component functions)
    // vars is a list of n variable names
    // Result is an m×n matrix of expressions
    ExprMat jacobian(const ExprVec &F,
                     const std::vector<std::string> &vars);

    // =============================================================================
    // Hessian
    // =============================================================================

    // H[i][j] = ∂²f / ∂vars[i]∂vars[j]
    // Result is an n×n symmetric matrix of expressions
    ExprMat hessian(const ExprPtr &f,
                    const std::vector<std::string> &vars);

    // =============================================================================
    // Laplacian
    // =============================================================================

    // ∇²f = Σ ∂²f/∂xᵢ²
    ExprPtr laplacian(const ExprPtr &f,
                      const std::vector<std::string> &vars);

    // =============================================================================
    // Formatted entry points (string in → string out, for CoreEngine / API)
    // =============================================================================

    // Partial derivative: "x^2*y + y^3", var="x"  →  "2*x*y"
    PartialResult computePartial(const std::string &expr,
                                 const std::string &var,
                                 int order = 1);

    // Mixed partial: expr, var1, var2
    PartialResult computeMixed(const std::string &expr,
                               const std::string &var1,
                               const std::string &var2);

    // Gradient — returns each component as a formatted string
    struct GradientResult
    {
        std::vector<std::string> components; // symbolic strings
        std::vector<std::string> latex;
        bool ok = true;
        std::string error;
    };
    GradientResult computeGradient(const std::string &expr,
                                   const std::vector<std::string> &vars);

    // Jacobian — returns matrix as formatted strings
    struct MatrixResult
    {
        std::vector<std::vector<std::string>> rows; // symbolic
        std::vector<std::vector<std::string>> latex;
        bool ok = true;
        std::string error;
    };
    MatrixResult computeJacobian(const std::vector<std::string> &exprs,
                                 const std::vector<std::string> &vars);

    // Hessian
    MatrixResult computeHessian(const std::string &expr,
                                const std::vector<std::string> &vars);

    // Laplacian
    PartialResult computeLaplacian(const std::string &expr,
                                   const std::vector<std::string> &vars);

} // namespace Calculus

#endif // PARTIAL_HPP