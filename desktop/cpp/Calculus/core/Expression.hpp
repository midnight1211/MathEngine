#pragma once
// =============================================================================
// calculus/core/Expression.h
//
// The symbolic expression tree.
// Every other file in the calculus module depends on this one.
//
// An expression like  sin(x^2 + 3*x)  is stored as a tree:
//
//         SIN
//          |
//         ADD
//        /   \
//      POW   MUL
//     /  \  /   \
//    x   2  3    x
//
// Each node holds:
//   - its type  (what kind of expression it is)
//   - its value (only meaningful for NUMBER nodes)
//   - its name  (only meaningful for VARIABLE, CONSTANT, FUNC nodes)
//   - its children (sub-expressions)
//
// Design rules:
//   - Nodes are immutable after construction. Every operation (diff,
//     simplify, substitute) returns a NEW tree. The original is never
//     modified. This makes it safe to share subtrees.
//   - ExprPtr is a shared_ptr so subtrees can be reused without copying.
//   - The parser lives here because it is the only way to create trees
//     from user input, and it is tightly coupled to the node types.
// =============================================================================

#ifndef EXPRESSION_H
#define EXPRESSION_H

#include <memory>
#include <string>
#include <vector>
#include <unordered_map>

namespace Calculus
{

    // ── Forward declaration ───────────────────────────────────────────────────────

    struct Expr;
    using ExprPtr = std::shared_ptr<Expr>;

    // ── Every kind of node the tree can hold ─────────────────────────────────────

    enum class ExprType
    {

        // ── Leaves (no children) ─────────────────────────────────
        NUMBER,   // a literal double:    3.14,  -7,  0.5
        VARIABLE, // a named variable:    x,  y,  t
        CONSTANT, // a named constant:    pi,  e,  inf

        // ── Binary operators ─────────────────────────────────────
        ADD, // f + g
        SUB, // f - g
        MUL, // f * g
        DIV, // f / g
        POW, // f ^ g  (base ^ exponent)

        // ── Unary operators ──────────────────────────────────────
        NEG, // -f

        // ── Trigonometric ────────────────────────────────────────
        SIN, // sin(f)
        COS, // cos(f)
        TAN, // tan(f)
        CSC, // csc(f)  = 1/sin(f)
        SEC, // sec(f)  = 1/cos(f)
        COT, // cot(f)  = 1/tan(f)

        // ── Inverse trigonometric ────────────────────────────────
        ASIN,  // arcsin(f)
        ACOS,  // arccos(f)
        ATAN,  // arctan(f)
        ATAN2, // arctan(f, g)  — two-argument form

        // ── Hyperbolic ───────────────────────────────────────────
        SINH,  // sinh(f)
        COSH,  // cosh(f)
        TANH,  // tanh(f)
        ASINH, // arcsinh(f)
        ACOSH, // arccosh(f)
        ATANH, // arctanh(f)

        // ── Exponential and logarithmic ──────────────────────────
        EXP,   // e^f          (same as POW with base=e, but common enough to special-case)
        LOG,   // ln(f)        natural log
        LOG2,  // log2(f)
        LOG10, // log10(f)
        LOGB,  // log_b(f)     log base b: children = {b, f}

        // ── Roots and absolute value ─────────────────────────────
        SQRT, // sqrt(f)      = f^(1/2)
        CBRT, // cbrt(f)      = f^(1/3)
        ABS,  // |f|

        // ── Special functions ────────────────────────────────────
        SIGN,      // sign(f)      = -1, 0, or 1
        FLOOR,     // floor(f)
        CEIL,      // ceil(f)
        FACTORIAL, // f!           (only valid for non-negative integers)

        // ── User-defined / unknown function ─────────────────────
        // Used when the parser encounters a name it doesn't recognise,
        // e.g.  f(x)  or  g(x, y).  Stored with name = "f" and children = args.
        FUNC,
    };

    // ── The node itself ───────────────────────────────────────────────────────────

    struct Expr
    {

        ExprType type;
        double value;                  // Used only by NUMBER
        std::string name;              // Used by VARIABLE, CONSTANT, FUNC
        std::vector<ExprPtr> children; // Sub-expressions (0 for leaves, 1+ for operators)

        // ── Constructors (use the factory functions below, not these directly) ──

        // Leaf: NUMBER
        explicit Expr(const double v)
            : type(ExprType::NUMBER), value(v) {}

        // Leaf: VARIABLE or CONSTANT or FUNC (name only, no children)
        Expr(ExprType t, std::string n)
            : type(t), value(0.0), name(std::move(n)) {}

        // Unary: one child (NEG, SIN, COS, …)
        Expr(ExprType t, ExprPtr child)
            : type(t), value(0.0), children({std::move(child)}) {}

        // Binary: two children (ADD, MUL, POW, …)
        Expr(ExprType t, ExprPtr left, ExprPtr right)
            : type(t), value(0.0), children({std::move(left), std::move(right)}) {}

        // N-ary: arbitrary children (FUNC with multiple arguments)
        Expr(ExprType t, std::string n, std::vector<ExprPtr> ch)
            : type(t), value(0.0), name(std::move(n)), children(std::move(ch)) {}

        // ── Convenience accessors ───────────────────────────────────────────────

        // For binary nodes: left child
        [[nodiscard]] const ExprPtr &left() const { return children[0]; }
        ExprPtr &left() { return children[0]; }

        // For binary nodes: right child
        [[nodiscard]] const ExprPtr &right() const { return children[1]; }
        ExprPtr &right() { return children[1]; }

        // For unary nodes: the single child
        [[nodiscard]] const ExprPtr &arg() const { return children[0]; }
        ExprPtr &arg() { return children[0]; }

        // ── Type query helpers ──────────────────────────────────────────────────

        [[nodiscard]] bool isNumber() const { return type == ExprType::NUMBER; }
        [[nodiscard]] bool isVariable() const { return type == ExprType::VARIABLE; }
        [[nodiscard]] bool isConstant() const { return type == ExprType::CONSTANT; }

        [[nodiscard]] bool isZero() const { return isNumber() && value == 0.0; }
        [[nodiscard]] bool isOne() const { return isNumber() && value == 1.0; }
        [[nodiscard]] bool isNegOne() const { return isNumber() && value == -1.0; }

        // True for any leaf (NUMBER, VARIABLE, CONSTANT)
        [[nodiscard]] bool isLeaf() const { return children.empty(); }

        // True for a number equal to the given value (within tolerance)
        [[nodiscard]] bool isNum(double v, double eps = 1e-12) const
        {
            return isNumber() && std::abs(value - v) < eps;
        }

        // True for a variable with the given name
        [[nodiscard]] bool isVar(const std::string &n) const
        {
            return isVariable() && name == n;
        }
    };

    // =============================================================================
    // Factory functions — the preferred way to build nodes.
    // These make client code read like the math it represents.
    // =============================================================================

    // ── Leaves ────────────────────────────────────────────────────────────────────

    inline ExprPtr num(double v)
    {
        return std::make_shared<Expr>(v);
    }

    inline ExprPtr var(const std::string &name)
    {
        return std::make_shared<Expr>(ExprType::VARIABLE, name);
    }

    inline ExprPtr con(const std::string &name)
    {
        return std::make_shared<Expr>(ExprType::CONSTANT, name);
    }

    // Common constants
    inline ExprPtr pi_expr() { return con("pi"); }
    inline ExprPtr e_expr() { return con("e"); }
    inline ExprPtr inf_expr() { return con("inf"); }

    // ── Binary operators ──────────────────────────────────────────────────────────

    inline ExprPtr add(ExprPtr l, ExprPtr r)
    {
        return std::make_shared<Expr>(ExprType::ADD, std::move(l), std::move(r));
    }
    inline ExprPtr sub(ExprPtr l, ExprPtr r)
    {
        return std::make_shared<Expr>(ExprType::SUB, std::move(l), std::move(r));
    }
    inline ExprPtr mul(ExprPtr l, ExprPtr r)
    {
        return std::make_shared<Expr>(ExprType::MUL, std::move(l), std::move(r));
    }
    inline ExprPtr div_expr(ExprPtr l, ExprPtr r)
    {
        return std::make_shared<Expr>(ExprType::DIV, std::move(l), std::move(r));
    }
    inline ExprPtr pow_expr(ExprPtr base, ExprPtr exp)
    {
        return std::make_shared<Expr>(ExprType::POW, std::move(base), std::move(exp));
    }

    // ── Unary operators ───────────────────────────────────────────────────────────

    inline ExprPtr neg(ExprPtr x)
    {
        return std::make_shared<Expr>(ExprType::NEG, std::move(x));
    }

    // ── Trig ──────────────────────────────────────────────────────────────────────

    inline ExprPtr sin_expr(ExprPtr x) { return std::make_shared<Expr>(ExprType::SIN, std::move(x)); }
    inline ExprPtr cos_expr(ExprPtr x) { return std::make_shared<Expr>(ExprType::COS, std::move(x)); }
    inline ExprPtr tan_expr(ExprPtr x) { return std::make_shared<Expr>(ExprType::TAN, std::move(x)); }
    inline ExprPtr csc_expr(ExprPtr x) { return std::make_shared<Expr>(ExprType::CSC, std::move(x)); }
    inline ExprPtr sec_expr(ExprPtr x) { return std::make_shared<Expr>(ExprType::SEC, std::move(x)); }
    inline ExprPtr cot_expr(ExprPtr x) { return std::make_shared<Expr>(ExprType::COT, std::move(x)); }
    inline ExprPtr asin_expr(ExprPtr x) { return std::make_shared<Expr>(ExprType::ASIN, std::move(x)); }
    inline ExprPtr acos_expr(ExprPtr x) { return std::make_shared<Expr>(ExprType::ACOS, std::move(x)); }
    inline ExprPtr atan_expr(ExprPtr x) { return std::make_shared<Expr>(ExprType::ATAN, std::move(x)); }

    // ── Hyperbolic ────────────────────────────────────────────────────────────────

    inline ExprPtr sinh_expr(ExprPtr x) { return std::make_shared<Expr>(ExprType::SINH, std::move(x)); }
    inline ExprPtr cosh_expr(ExprPtr x) { return std::make_shared<Expr>(ExprType::COSH, std::move(x)); }
    inline ExprPtr tanh_expr(ExprPtr x) { return std::make_shared<Expr>(ExprType::TANH, std::move(x)); }
    inline ExprPtr asinh_expr(ExprPtr x) { return std::make_shared<Expr>(ExprType::ASINH, std::move(x)); }
    inline ExprPtr acosh_expr(ExprPtr x) { return std::make_shared<Expr>(ExprType::ACOSH, std::move(x)); }
    inline ExprPtr atanh_expr(ExprPtr x) { return std::make_shared<Expr>(ExprType::ATANH, std::move(x)); }

    // ── Exponential and log ───────────────────────────────────────────────────────

    inline ExprPtr exp_expr(ExprPtr x) { return std::make_shared<Expr>(ExprType::EXP, std::move(x)); }
    inline ExprPtr log_expr(ExprPtr x) { return std::make_shared<Expr>(ExprType::LOG, std::move(x)); }
    inline ExprPtr log2_expr(ExprPtr x) { return std::make_shared<Expr>(ExprType::LOG2, std::move(x)); }
    inline ExprPtr log10_expr(ExprPtr x) { return std::make_shared<Expr>(ExprType::LOG10, std::move(x)); }
    inline ExprPtr logb_expr(ExprPtr b, ExprPtr x)
    {
        return std::make_shared<Expr>(ExprType::LOGB, "log", std::vector<ExprPtr>{std::move(b), std::move(x)});
    }

    // ── Roots ─────────────────────────────────────────────────────────────────────

    inline ExprPtr sqrt_expr(ExprPtr x) { return std::make_shared<Expr>(ExprType::SQRT, std::move(x)); }
    inline ExprPtr cbrt_expr(ExprPtr x) { return std::make_shared<Expr>(ExprType::CBRT, std::move(x)); }
    inline ExprPtr abs_expr(ExprPtr x) { return std::make_shared<Expr>(ExprType::ABS, std::move(x)); }

    // =============================================================================
    // SymbolTable — variable and constant bindings for evaluation
    // =============================================================================

    using SymbolTable = std::unordered_map<std::string, double>;

    // Built-in constants: pi, e, inf
    SymbolTable defaultSymbols();

    // =============================================================================
    // Core operations declared here, implemented in Expression.cpp
    // =============================================================================

    // ── Parsing ───────────────────────────────────────────────────────────────────

    // Parse a string into an expression tree.
    // Throws std::invalid_argument on syntax error.
    // Examples:
    //   parse("x^2 + 3*x - 1")
    //   parse("sin(pi/2)")
    //   parse("e^(-x^2)")
    ExprPtr parse(const std::string &input);

    // ── Evaluation ───────────────────────────────────────────────────────────────

    // Evaluate the expression to a double, given variable bindings.
    // Throws std::runtime_error if a variable has no binding.
    // Example:
    //   evaluate(parse("x^2 + 1"), {{"x", 3.0}})  → 10.0
    double evaluate(const ExprPtr &expr, const SymbolTable &syms = {});

    // ── Printing ─────────────────────────────────────────────────────────────────

    // Convert the tree back to a human-readable infix string.
    // Example:  toString(parse("sin(x^2)"))  →  "sin(x^2)"
    std::string toString(const ExprPtr &expr);

    // Convert to LaTeX string.
    // Example:  toLatex(parse("x^2/sin(x)"))  →  "\frac{x^{2}}{\sin(x)}"
    std::string toLatex(const ExprPtr &expr);

    // ── Substitution ─────────────────────────────────────────────────────────────

    // Replace every occurrence of variable `varName` with `replacement`.
    // Returns a NEW tree. The original is not modified.
    // Example:
    //   substitute(parse("x^2 + y"), "x", parse("t+1"))
    //   → parse("(t+1)^2 + y")
    ExprPtr substitute(const ExprPtr &expr,
                       const std::string &varName,
                       const ExprPtr &replacement);

    // ── Tree utilities ────────────────────────────────────────────────────────────

    // Deep copy — returns a completely independent tree.
    ExprPtr clone(const ExprPtr &expr);

    // True if the expression contains the given variable.
    bool contains(const ExprPtr &expr, const std::string &varName);

    // True if the expression is numerically constant (no variables).
    bool isConstantExpr(const ExprPtr &expr);

    // True if two expression trees are structurally identical.
    bool equal(const ExprPtr &a, const ExprPtr &b);

    // Count nodes in the tree (useful for choosing between algorithms).
    int complexity(const ExprPtr &expr);

} // namespace Calculus

#endif // EXPRESSION_H