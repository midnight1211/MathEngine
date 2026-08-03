#pragma once
// =============================================================================
// calculus/differentiation/Implicit.h
//
// Implicit differentiation and logarithmic differentiation.
//
// IMPLICIT DIFFERENTIATION
//   Given F(x, y) = 0, find dy/dx without solving for y.
//   Method: differentiate both sides w.r.t. x treating y as a function of x,
//   then solve for dy/dx algebraically.
//
//   By the implicit function theorem:
//     dy/dx = -(∂F/∂x) / (∂F/∂y)
//
//   Example:  F(x,y) = x^2 + y^2 - 1 = 0  (unit circle)
//     ∂F/∂x = 2x,  ∂F/∂y = 2y
//     dy/dx = -2x / 2y = -x/y
//
//   Higher-order implicit derivatives are obtained by differentiating
//   the first derivative result again w.r.t. x (substituting dy/dx back in).
//
// LOGARITHMIC DIFFERENTIATION
//   For expressions of the form y = f(x)^g(x) or products/quotients of
//   many factors, taking ln of both sides before differentiating simplifies
//   the algebra considerably.
//
//   Steps:
//     1. Take ln of both sides:  ln(y) = g(x)*ln(f(x))
//     2. Differentiate:          y'/y  = [g(x)*ln(f(x))]'
//     3. Multiply through by y:  y'    = y * [g(x)*ln(f(x))]'
//     4. Substitute y = f(x)^g(x) back in
//
//   This module applies those steps symbolically.
// =============================================================================

#ifndef IMPLICIT_HPP
#define IMPLICIT_HPP

#include "../core/Expression.hpp"
#include "../core/Simplify.hpp"
#include "Derivative.hpp"
#include "Partial.hpp"
#include <string>

namespace Calculus
{

    // =============================================================================
    // Implicit differentiation
    // =============================================================================

    // Given F(x, y) = 0, compute dy/dx.
    // F is the expression on the left-hand side (right-hand side assumed 0).
    // depVar is the dependent variable (y), indepVar is the independent variable (x).
    //
    // Returns: -(∂F/∂x) / (∂F/∂y)
    ExprPtr implicitDiff(const ExprPtr &F, const std::string &indepVar, const std::string &depVar);

    // Second-order implicit derivative d²y/dx².
    // Differentiates the first-order result again w.r.t. indepVar,
    // substituting the expression for dy/dx wherever it appears.
    ExprPtr implicitDiff2(const ExprPtr &F,
                          const std::string &indepVar,
                          const std::string &depVar);

    // =============================================================================
    // Logarithmic differentiation
    // =============================================================================

    // Differentiate y = expr using logarithmic differentiation.
    // Returns dy/dx = expr * d/dx[ln(expr)]
    //               = expr * [diff(expr, var) / expr]
    //               = diff(expr, var)
    //
    // The value of this function is not that it gives a different answer —
    // it gives the same answer as diff() — but that it shows the intermediate
    // steps symbolically, which is what the "logarithmic differentiation" method
    // requires for display purposes.
    //
    // Returns both the final result and the intermediate ln-form derivative.
    struct LogDiffResult
    {
        ExprPtr lnForm; // d/dx[ln(y)] = d/dx[ln(expr)]  (intermediate)
        ExprPtr result; // dy/dx = expr * lnForm
        bool ok = true;
        std::string error;
    };

    LogDiffResult logDiff(const ExprPtr &expr, const std::string &var);

    // =============================================================================
    // Formatted entry points
    // =============================================================================

    struct ImplicitResult
    {
        std::string dydx; // symbolic dy/dx
        std::string dydx_latex;
        std::string d2ydx2; // symbolic d²y/dx² (empty if not requested)
        std::string d2ydx2_latex;
        bool ok = true;
        std::string error;
    };

    // Implicit: F(x,y)=0, find dy/dx  (and optionally d²y/dx²)
    ImplicitResult computeImplicit(const std::string &F, const std::string &indepVar, const std::string &depVar, bool secondOrder = false);

    struct LogDiffFormatted
    {
        std::string lnDerivative; // d/dx[ln(y)]
        std::string lnDerivative_latex;
        std::string result; // dy/dx
        std::string result_latex;
        bool ok = true;
        std::string error;
    };

    // Logarithmic differentiation of y = expr
    LogDiffFormatted computeLogDiff(const std::string &expr, const std::string &var);

}

#endif // IMPLICIT_HPP