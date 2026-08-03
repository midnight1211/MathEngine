#pragma once
// calculus/integration/Symbolic.hpp
//   DIRECT RULES
//     power rule        ∫x^n dx = x^(n+1)/(n+1)  (n ≠ -1)
//     1/x               ∫1/x dx = ln|x|
//     exponential       ∫e^x dx = e^x
//     trig functions    ∫sin, ∫cos, ∫tan, ∫csc, ∫sec, ∫cot
//     inverse trig      ∫1/sqrt(1-x²), ∫1/(1+x²), etc.
//     hyperbolic        ∫sinh, ∫cosh, ∫tanh, etc.
//     logarithm         ∫ln(x) = x*ln(x) - x
//   U-SUBSTITUTION
//     Detects f(g(x)) * g'(x) pattern.
//     Sets u = g(x), rewrites in u, integrates, substitutes back.
//   INTEGRATION BY PARTS
//     ∫u dv = uv - ∫v du
//     Uses LIATE heuristic to choose u:
//       Logarithm > Inverse trig > Algebraic > Trig > Exponential
//     Applies recursively (handles repeated IBP).
//   PARTIAL FRACTIONS
//     Decomposes P(x)/Q(x) where deg(P) < deg(Q).
//     Handles:
//       linear factors       A/(x-a)
//       repeated linear      A/(x-a) + B/(x-a)²
//       irreducible quadratic (Ax+B)/(x²+bx+c)
//   TRIG SUBSTITUTION
//     sqrt(a²-x²)  →  x = a*sin(θ)
//     sqrt(a²+x²)  →  x = a*tan(θ)
//     sqrt(x²-a²)  →  x = a*sec(θ)
//   TRIG REDUCTION IDENTITIES
//     sin²(x) = (1 - cos(2x))/2
//     cos²(x) = (1 + cos(2x))/2
//     sin(x)cos(x) = sin(2x)/2
//     Powers of sin/cos via reduction formulae
//   FALLBACK
//     indefinite integrals.

#ifndef MATHENGINE_SYMBOLIC_HPP
#define MATHENGINE_SYMBOLIC_HPP

#include "../core/Expression.hpp"
#include "../core/Simplify.hpp"
#include "../differentiation/Derivative.hpp"
#include "Numerical.hpp"
#include <string>
#include <optional>

namespace Calculus
{

    // Core symbolic integration

    // Try to find the antiderivative F(x) of expr w.r.t. var.
    // Returns nuppopt if no symbolic method succeeds.
    std::optional<ExprPtr> integrate(const ExprPtr &expr, const std::string &var);

    // Definite integral ∫_a^b expr dx.
    // Returns a double (numerical value).
    double definiteIntegral(const ExprPtr &expr, const std::string &var, double a, double b);

    // Individual strategies (exposed for testing and step display)

    std::optional<ExprPtr> integrateDirectRule(const ExprPtr &expr,
                                               const std::string &var);

    // U-substitution
    std::optional<ExprPtr> integrateUSub(const ExprPtr &expr,
                                         const std::string &var);

    // Integration by parts (IBP)
    std::optional<ExprPtr> integrateByParts(const ExprPtr &expr,
                                            const std::string &var);

    // Partial fractions (for rational expressions)
    std::optional<ExprPtr> integratePartialFractions(const ExprPtr &expr,
                                                     const std::string &var);

    // Trig substitution
    std::optional<ExprPtr> integrateTrigSub(const ExprPtr &expr,
                                            const std::string &var);

    // Trig identities (powers of sin/cos)
    std::optional<ExprPtr> integrateTrigIdentity(const ExprPtr &expr,
                                                 const std::string &var);

    // Verification

    bool verifyAntiderivative(const ExprPtr &antideriv,
                              const ExprPtr &original,
                              const std::string &var);

    // Formatted entry points

    struct SymbolicIntegralResult
    {
        std::string antiderivative; // symbolic F(x) + C
        std::string antiderivative_latex;
        std::string method;          // which strategy succeeded
        bool symbolic = true;        // false if fell back to numerical
        double numericalValue = 0.0; // only for definite integrals
        std::string numericalStr;
        bool ok = true;
        std::string error;
    };

    // Indefinite integral: ∫ expr dx
    SymbolicIntegralResult computeIndefinite(const std::string &exprStr,
                                             const std::string &var);

    // Definite integral: ∫_a^b expr dx
    // aStr, bStr accept "pi", "inf", "1/2", etc.
    SymbolicIntegralResult computeDefinite(const std::string &exprStr,
                                           const std::string &var,
                                           const std::string &aStr,
                                           const std::string &bStr);

}

#endif // MATHENGINE_SYMBOLIC_HPP