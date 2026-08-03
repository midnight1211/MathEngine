// =============================================================================
// calculus/core/Simplify.cpp
// =============================================================================

#include "Simplify.hpp"
#include <cmath>
#include <functional>
#define _POSIX_C_SOURCE 200809L // Exposes POSIX extensions
#include <math.h>

namespace Calculus
{

    static constexpr int MAX_PASSES = 16;
    static constexpr double EPS = 1e-12;

    // ── Internal helpers ──────────────────────────────────────────────────────────

    // True if e is a NUMBER equal to v
    static bool isNum(const ExprPtr &e, double v)
    {
        return e && e->isNumber() && std::abs(e->value - v) < EPS;
    }

    // True if e is the constant "pi" or the number M_PI
    static bool isPi(const ExprPtr &e)
    {
        return (e->type == ExprType::CONSTANT && e->name == "pi") || (e->isNumber() && std::abs(e->value - M_PI) < EPS);
    }

    // True if e is the constant "e" or the number M_E
    static bool isE(const ExprPtr &e)
    {
        return (e->type == ExprType::CONSTANT && e->name == "e") || (e->isNumber() && std::abs(e->value - M_E) < EPS);
    }

    // Extract the numeric coefficient and the "rest" of a MUL node.
    // e.g.,  3*x  -->  {3.0, x}
    //         x   -->  {1.0, x}
    //        -x   -->  {-1.0, x}   (via NEG)
    static std::pair<double, ExprPtr> extractCoeff(const ExprPtr &e)
    {
        if (!e)
            return {1.0, e};
        if (e->type == ExprType::MUL)
        {
            if (e->left()->isNumber())
                return {e->left()->value, e->right()};
            if (e->right()->isNumber())
                return {e->right()->value, e->left()};
        }
        if (e->type == ExprType::NEG)
            return {-1.0, e->arg()};
        return {1.0, e};
    }

    // =============================================================================
    // simplifyStep — one bottom-up pass
    // =============================================================================

    ExprPtr simplifyStep(const ExprPtr &expr)
    {
        if (!expr)
            return expr;

        // ── Leaves: nothing to simplify ───────────────────────────────────────────
        if (expr->isLeaf())
            return expr;

        // ── Recurse into children first (bottom-up) ───────────────────────────────
        std::vector<ExprPtr> newChildren;
        newChildren.reserve(expr->children.size());
        bool changed = false;
        for (const auto &c : expr->children)
        {
            auto sc = simplifyStep(c);
            if (sc != c)
                changed = true;
            newChildren.push_back(sc);
        }

        // Rebuild mode with simplified children
        auto e = std::make_shared<Expr>(expr->type, expr->name, newChildren);
        e->value = expr->value;

        // ── Now apply rules at this node ──────────────────────────────────────────

        switch (e->type)
        {

        //  ── NEG ───────────────────────────────────────────────────────────────────
        case ExprType::NEG:
        {
            auto a = e->arg();
            // -NUMBER --> NUMBER with negated value
            if (a->isNumber())
                return num(-a->value);
            // -(-x) --> x
            if (a->type == ExprType::NEG)
                return a->arg();
            // -(x-y) --> y - x  (not always smaller, skip for now)
            return e;
        }

        // ── ADD ───────────────────────────────────────────────────────────────────
        case ExprType::ADD:
        {
            auto L = e->left(), R = e->right();

            // NUMBER + NUMBER → NUMBER
            if (L->isNumber() && R->isNumber())
                return num(L->value + R->value);

            // x + 0 → x,   0 + x → x
            if (isNum(R, 0.0))
                return L;
            if (isNum(L, 0.0))
                return R;

            // x + (-y) → x - y
            if (R->type == ExprType::NEG)
                return sub(L, R->arg());

            // x + x → 2*x
            if (equal(L, R))
                return mul(num(2.0), L);

            // c1*x + c2*x → (c1+c2)*x
            auto [c1, t1] = extractCoeff(L);
            auto [c2, t2] = extractCoeff(R);
            if (equal(t1, t2))
                return mul(num(c1 + c2), t1);

            return e;
        }

        // ── SUB ───────────────────────────────────────────────────────────────────
        case ExprType::SUB:
        {
            auto L = e->left(), R = e->right();

            // NUMBER - NUMBER → NUMBER
            if (L->isNumber() && R->isNumber())
                return num(L->value - R->value);

            // x - 0 → x
            if (isNum(R, 0.0))
                return L;

            // 0 - x → -x
            if (isNum(L, 0.0))
                return neg(R);

            // x - x → 0
            if (equal(L, R))
                return num(0.0);

            // x - (-y) → x + y
            if (R->type == ExprType::NEG)
                return add(L, R->arg());

            // c1*x - c2*x → (c1-c2)*x
            auto [c1, t1] = extractCoeff(L);
            auto [c2, t2] = extractCoeff(R);
            if (equal(t1, t2))
            {
                double diff = c1 - c2;
                if (std::abs(diff) < EPS)
                    return num(0.0);
                return mul(num(diff), t1);
            }

            return e;
        }

        // ── MUL ───────────────────────────────────────────────────────────────────
        case ExprType::MUL:
        {
            auto L = e->left(), R = e->right();

            // NUMBER * NUMBER → NUMBER
            if (L->isNumber() && R->isNumber())
                return num(L->value * R->value);

            // x * 1 → x,   1 * x → x
            if (isNum(R, 1.0))
                return L;
            if (isNum(L, 1.0))
                return R;

            // x * 0 → 0,   0 * x → 0
            if (isNum(R, 0.0) || isNum(L, 0.0))
                return num(0.0);

            // x * (-1) → -x,   (-1) * x → -x
            if (isNum(R, -1.0))
                return neg(L);
            if (isNum(L, -1.0))
                return neg(R);

            // (-x) * (-y) → x * y
            if (L->type == ExprType::NEG && R->type == ExprType::NEG)
                return mul(L->arg(), R->arg());

            // NUMBER * (NUMBER * x) → (N1*N2) * x  (flatten numeric coefficients)
            if (L->isNumber() && R->type == ExprType::MUL && R->left()->isNumber())
                return mul(num(L->value * R->left()->value), R->right());
            if (R->isNumber() && L->type == ExprType::MUL && L->left()->isNumber())
                return mul(num(R->value * L->left()->value), L->right());

            // x * x → x^2
            if (equal(L, R))
                return pow_expr(L, num(2.0));

            // x^a * x^b → x^(a+b)
            if (L->type == ExprType::POW && R->type == ExprType::POW &&
                equal(L->left(), R->left()))
                return pow_expr(L->left(), simplifyStep(add(L->right(), R->right())));

            // x^a * x → x^(a+1)
            if (L->type == ExprType::POW && equal(L->left(), R))
                return pow_expr(L->left(), simplifyStep(add(L->right(), num(1.0))));

            return e;
        }

        // ── DIV ───────────────────────────────────────────────────────────────────
        case ExprType::DIV:
        {
            auto L = e->left(), R = e->right();

            // NUMBER / NUMBER → NUMBER
            if (L->isNumber() && R->isNumber())
            {
                if (std::abs(R->value) < EPS)
                    return e; // leave 0-division for runtime
                return num(L->value / R->value);
            }

            // x / 1 → x
            if (isNum(R, 1.0))
                return L;

            // 0 / x → 0  (when x is not 0)
            if (isNum(L, 0.0))
                return num(0.0);

            // x / x → 1
            if (equal(L, R))
                return num(1.0);

            // x / (-1) → -x
            if (isNum(R, -1.0))
                return neg(L);

            // (a*x) / a → x
            if (L->type == ExprType::MUL && L->left()->isNumber() && R->isNumber())
                if (std::abs(R->value) > EPS)
                    return mul(num(L->left()->value / R->value), L->right());

            return e;
        }

        // ── POW ───────────────────────────────────────────────────────────────────
        case ExprType::POW:
        {
            auto base = e->left(), exp = e->right();

            // NUMBER ^ NUMBER → NUMBER
            if (base->isNumber() && exp->isNumber())
                return num(std::pow(base->value, exp->value));

            // x ^ 1 → x
            if (isNum(exp, 1.0))
                return base;

            // x ^ 0 → 1
            if (isNum(exp, 0.0))
                return num(1.0);

            // 1 ^ x → 1
            if (isNum(base, 1.0))
                return num(1.0);

            // 0 ^ x → 0  (for positive x — leave general case alone)
            if (isNum(base, 0.0) && exp->isNumber() && exp->value > 0)
                return num(0.0);

            // (x^a)^b → x^(a*b)
            if (base->type == ExprType::POW)
                return pow_expr(base->left(),
                                simplifyStep(mul(base->right(), exp)));

            // x^(1/2) → sqrt(x)
            if (exp->isNumber() && std::abs(exp->value - 0.5) < EPS)
                return sqrt_expr(base);

            // sqrt(x)^2 → x  (x >= 0 assumed)
            if (base->type == ExprType::SQRT && isNum(exp, 2.0))
                return base->arg();

            return e;
        }

        // ── EXP ───────────────────────────────────────────────────────────────────
        case ExprType::EXP:
        {
            auto a = e->arg();
            // exp(0) → 1
            if (isNum(a, 0.0))
                return num(1.0);
            // exp(1) → e
            if (isNum(a, 1.0))
                return e_expr();
            // exp(ln(x)) → x
            if (a->type == ExprType::LOG)
                return a->arg();
            // exp(NUMBER) → NUMBER
            if (a->isNumber())
                return num(std::exp(a->value));
            return e;
        }

        // ── LOG (natural log) ─────────────────────────────────────────────────────
        case ExprType::LOG:
        {
            auto a = e->arg();
            // ln(1) → 0
            if (isNum(a, 1.0))
                return num(0.0);
            // ln(e) → 1
            if (isE(a))
                return num(1.0);
            // ln(e^x) → x
            if (a->type == ExprType::EXP)
                return a->arg();
            // ln(x^n) → n * ln(x)
            if (a->type == ExprType::POW)
                return mul(a->right(), log_expr(a->left()));
            // ln(NUMBER) → NUMBER
            if (a->isNumber() && a->value > 0)
                return num(std::log(a->value));
            return e;
        }

        // ── LOG10 ─────────────────────────────────────────────────────────────────
        case ExprType::LOG10:
        {
            auto a = e->arg();
            if (isNum(a, 1.0))
                return num(0.0);
            if (isNum(a, 10.0))
                return num(1.0);
            if (isNum(a, 100.0))
                return num(2.0);
            if (a->isNumber() && a->value > 0)
                return num(std::log10(a->value));
            return e;
        }

        // ── LOG2 ──────────────────────────────────────────────────────────────────
        case ExprType::LOG2:
        {
            auto a = e->arg();
            if (isNum(a, 1.0))
                return num(0.0);
            if (isNum(a, 2.0))
                return num(1.0);
            if (a->isNumber() && a->value > 0)
                return num(std::log2(a->value));
            return e;
        }

        // ── SQRT ──────────────────────────────────────────────────────────────────
        case ExprType::SQRT:
        {
            auto a = e->arg();
            if (isNum(a, 0.0))
                return num(0.0);
            if (isNum(a, 1.0))
                return num(1.0);
            if (isNum(a, 4.0))
                return num(2.0);
            if (isNum(a, 9.0))
                return num(3.0);
            if (isNum(a, 16.0))
                return num(4.0);
            if (isNum(a, 25.0))
                return num(5.0);
            // sqrt(x^2) → x  (x >= 0 assumed)
            if (a->type == ExprType::POW && isNum(a->right(), 2.0))
                return a->left();
            // sqrt(NUMBER) → NUMBER if perfect square
            if (a->isNumber() && a->value > 0)
            {
                double s = std::sqrt(a->value);
                if (std::abs(s - std::round(s)) < EPS)
                    return num(std::round(s));
            }
            return e;
        }

        // ── SIN ───────────────────────────────────────────────────────────────────
        case ExprType::SIN:
        {
            auto a = e->arg();
            if (isNum(a, 0.0))
                return num(0.0);
            if (isPi(a))
                return num(0.0); // sin(pi) = 0
            // sin(NUMBER) → NUMBER
            if (a->isNumber())
                return num(std::sin(a->value));
            return e;
        }

        // ── COS ───────────────────────────────────────────────────────────────────
        case ExprType::COS:
        {
            auto a = e->arg();
            if (isNum(a, 0.0))
                return num(1.0);
            if (isPi(a))
                return num(-1.0); // cos(pi) = -1
            // cos(NUMBER) → NUMBER
            if (a->isNumber())
                return num(std::cos(a->value));
            return e;
        }

        // ── TAN ───────────────────────────────────────────────────────────────────
        case ExprType::TAN:
        {
            auto a = e->arg();
            if (isNum(a, 0.0))
                return num(0.0);
            if (a->isNumber())
                return num(std::tan(a->value));
            return e;
        }

        // ── SINH / COSH / TANH ────────────────────────────────────────────────────
        case ExprType::SINH:
        {
            auto a = e->arg();
            if (isNum(a, 0.0))
                return num(0.0);
            if (a->isNumber())
                return num(std::sinh(a->value));
            return e;
        }
        case ExprType::COSH:
        {
            auto a = e->arg();
            if (isNum(a, 0.0))
                return num(1.0);
            if (a->isNumber())
                return num(std::cosh(a->value));
            return e;
        }
        case ExprType::TANH:
        {
            auto a = e->arg();
            if (isNum(a, 0.0))
                return num(0.0);
            if (a->isNumber())
                return num(std::tanh(a->value));
            return e;
        }

        // ── ABS ───────────────────────────────────────────────────────────────────
        case ExprType::ABS:
        {
            auto a = e->arg();
            if (a->isNumber())
                return num(std::abs(a->value));
            // abs(-x) → abs(x)
            if (a->type == ExprType::NEG)
                return abs_expr(a->arg());
            return e;
        }

        default:
            return e;
        }
    }

    // =============================================================================
    // simplify - fixed-point iteration
    // =============================================================================

    ExprPtr simplify(const ExprPtr &expr)
    {
        ExprPtr current = expr;
        for (int i = 0; i < MAX_PASSES; ++i)
        {
            ExprPtr next = simplifyStep(current);
            // Stop when nothing changed (structural quality check)
            if (equal(next, current))
                return next;
            current = next;
        }
        return current;
    }

    // =============================================================================
    // Convenience wrappers
    // =============================================================================

    std::string simplifyToString(const ExprPtr &expr)
    {
        return toString(simplify(expr));
    }

    std::string simplifyToLatex(const ExprPtr &expr)
    {
        return toLatex(simplify(expr));
    }

}
