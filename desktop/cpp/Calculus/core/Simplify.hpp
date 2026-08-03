#pragma once
// =============================================================================
// calculus/core/Simplify.h
//
// Algebraic simplification of ExprPtr trees.
//
// Without this, diff(x^2) produces:
//   2 * x^(2 - 1)
// which is mathematically correct but unreadable. Simplify turns it into:
//   2 * x
//
// Rules applied (in order, repeated until stable):
//
//   CONSTANT FOLDING
//     NUMBER op NUMBER  →  evaluate immediately
//     -NUMBER           →  NUMBER (with negated value)
//
//   IDENTITY / ANNIHILATOR
//     x + 0   →  x          0 + x   →  x
//     x - 0   →  x          0 - x   →  -x
//     x * 1   →  x          1 * x   →  x
//     x * 0   →  0          0 * x   →  0
//     x / 1   →  x          0 / x   →  0
//     x ^ 1   →  x          x ^ 0   →  1
//     1 ^ x   →  1          0 ^ x   →  0  (x > 0)
//
//   DOUBLE NEGATION
//     -(-x)   →  x
//
//   LOGARITHM / EXPONENTIAL
//     ln(e^x) →  x          e^(ln x) →  x
//     ln(1)   →  0          ln(e)    →  1
//     log(1)  →  0
//
//   TRIG IDENTITIES
//     sin(0)  →  0          cos(0)   →  1
//     sin(pi) →  0          cos(pi)  →  -1
//
//   POWER RULES
//     (x^a)^b →  x^(a*b)
//     x^1/2   →  sqrt(x)    (display only)
//
//   LIKE TERMS  (one pass — full polynomial collection is not attempted)
//     x + x   →  2*x
//     x - x   →  0
//     2*x + 3*x → 5*x
//
// Design:
//   simplify(expr) walks the tree bottom-up (children first), applies rules
//   at each node, and returns a new tree. It repeats until no rule fires
//   (fixed-point iteration), up to MAX_PASSES to avoid infinite loops.
//
//   simplifyStep(expr) does exactly one pass — useful for testing.
// =============================================================================

#ifndef SIMPLIFY_HPP
#define SIMPLIFY_HPP

#include "Expression.hpp"

namespace Calculus
{

    // Full simplification - repeats until stable (up to MAX_PASSES iterations).
    // This is the function all other modules should call.
    ExprPtr simplify(const ExprPtr &expr);

    // Single pass - applies rules once, botton-up
    // Returns the (possibly unchanged) expression.
    ExprPtr simplifyStep(const ExprPtr &expr);

    // Convenience: simplify, then convert to string
    std::string simplifyToString(const ExprPtr &expr);

    // Convenience: simplify, then convert to LaTeX
    std::string simplifyToLatex(const ExprPtr &expr);

}

#endif // SIMPLIFY_HPP