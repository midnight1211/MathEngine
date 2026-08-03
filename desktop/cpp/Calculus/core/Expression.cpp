// =============================================================================
// calculus/core/Expression.cpp
//
// Implements everything declared in Expression.h:
//   1. defaultSymbols()    — built-in constants
//   2. evaluate()          — tree → double
//   3. parse()             — delegates to Week4Bridge (Lexer → Parser → ExprPtr)
//   4. toString()          — tree → infix string
//   5. toLatex()           — tree → LaTeX string
//   6. substitute()        — replace variable with sub-tree
//   7. clone()             — deep copy
//   8. contains()          — variable presence check
//   9. isConstantExpr()    — no variables anywhere in tree
//  10. equal()             — structural equality
//  11. complexity()        — node count
//
// parse() and evaluate() are thin wrappers around Week4Bridge so the
// week_4 Lexer/Parser/Evaluator is the single source of truth for parsing.
// =============================================================================

#include "Expression.hpp"
#include "Week4Bridge.hpp"
#include <cmath>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cctype>
#include <stdexcept>
#include <functional>

namespace Calculus
{

    // =============================================================================
    // 1. Built-in constants
    // =============================================================================

    SymbolTable defaultSymbols()
    {
        return {
            {"pi", M_PI},
            {"e", M_E},
            {"inf", std::numeric_limits<double>::infinity()},
            {"tau", 2.0 * M_PI},
            {"phi", (1.0 + std::sqrt(5.0)) / 2.0}, // golden ratio
        };
    }

    // =============================================================================
    // 2. Evaluation  (tree → double)
    // =============================================================================

    double evaluate(const ExprPtr &expr, const SymbolTable &syms)
    {
        if (!expr)
            throw std::runtime_error("evaluate: null expression");

        // Merge caller's symbols with defaults (caller takes priority)
        SymbolTable all = defaultSymbols();
        for (auto &[k, v] : syms)
            all[k] = v;

        // Recursive lambda
        std::function<double(const ExprPtr &)> eval = [&](const ExprPtr &e) -> double
        {
            switch (e->type)
            {

            case ExprType::NUMBER:
                return e->value;

            case ExprType::VARIABLE:
            {
                auto it = all.find(e->name);
                if (it == all.end())
                    throw std::runtime_error("Undefined variable: " + e->name);
                return it->second;
            }

            case ExprType::CONSTANT:
            {
                auto it = all.find(e->name);
                if (it == all.end())
                    throw std::runtime_error("Undefined constant: " + e->name);
                return it->second;
            }

            // Binary operators
            case ExprType::ADD:
                return eval(e->left()) + eval(e->right());
            case ExprType::SUB:
                return eval(e->left()) - eval(e->right());
            case ExprType::MUL:
                return eval(e->left()) * eval(e->right());
            case ExprType::DIV:
            {
                double d = eval(e->right());
                if (std::abs(d) < 1e-300)
                    throw std::runtime_error("Division by zero");
                return eval(e->left()) / d;
            }
            case ExprType::POW:
                return std::pow(eval(e->left()), eval(e->right()));

            // Unary minus
            case ExprType::NEG:
                return -eval(e->arg());

            // Trig
            case ExprType::SIN:
                return std::sin(eval(e->arg()));
            case ExprType::COS:
                return std::cos(eval(e->arg()));
            case ExprType::TAN:
                return std::tan(eval(e->arg()));
            case ExprType::CSC:
                return 1.0 / std::sin(eval(e->arg()));
            case ExprType::SEC:
                return 1.0 / std::cos(eval(e->arg()));
            case ExprType::COT:
                return std::cos(eval(e->arg())) / std::sin(eval(e->arg()));

            // Inverse trig
            case ExprType::ASIN:
                return std::asin(eval(e->arg()));
            case ExprType::ACOS:
                return std::acos(eval(e->arg()));
            case ExprType::ATAN:
                return std::atan(eval(e->arg()));
            case ExprType::ATAN2:
                return std::atan2(eval(e->children[0]), eval(e->children[1]));

            // Hyperbolic
            case ExprType::SINH:
                return std::sinh(eval(e->arg()));
            case ExprType::COSH:
                return std::cosh(eval(e->arg()));
            case ExprType::TANH:
                return std::tanh(eval(e->arg()));
            case ExprType::ASINH:
                return std::asinh(eval(e->arg()));
            case ExprType::ACOSH:
                return std::acosh(eval(e->arg()));
            case ExprType::ATANH:
                return std::atanh(eval(e->arg()));

            // Exponential / log
            case ExprType::EXP:
                return std::exp(eval(e->arg()));
            case ExprType::LOG:
                return std::log(eval(e->arg()));
            case ExprType::LOG2:
                return std::log2(eval(e->arg()));
            case ExprType::LOG10:
                return std::log10(eval(e->arg()));
            case ExprType::LOGB:
            {
                double base = eval(e->children[0]);
                double arg = eval(e->children[1]);
                return std::log(arg) / std::log(base);
            }

            // Roots / absolute value
            case ExprType::SQRT:
                return std::sqrt(eval(e->arg()));
            case ExprType::CBRT:
                return std::cbrt(eval(e->arg()));
            case ExprType::ABS:
                return std::abs(eval(e->arg()));

            // Special
            case ExprType::SIGN:
            {
                double v = eval(e->arg());
                return (v > 0) - (v < 0);
            }
            case ExprType::FLOOR:
                return std::floor(eval(e->arg()));
            case ExprType::CEIL:
                return std::ceil(eval(e->arg()));
            case ExprType::FACTORIAL:
            {
                double v = eval(e->arg());
                if (v < 0 || v != std::floor(v))
                    throw std::runtime_error("Factorial requires non-negative integer");
                double result = 1.0;
                for (int i = 2; i <= static_cast<int>(v); ++i)
                    result *= i;
                return result;
            }

            case ExprType::FUNC:
                throw std::runtime_error(
                    "Cannot evaluate unknown function: " + e->name);

            default:
                throw std::runtime_error("evaluate: unhandled expression type");
            }
        };

        return eval(expr);
    }

    // =============================================================================
    // 3. parse()  —  delegates to Week4Bridge
    //
    // Week4Bridge runs:  string → Lexer → tokens → Parser → ASTNode* → ExprPtr
    // The ASTNode* tree is discarded after conversion; ExprPtr is self-contained.
    // All other operations (toString, toLatex, substitute, diff, etc.) work on
    // ExprPtr directly and never call back into week_4.
    // =============================================================================

    ExprPtr parse(const std::string &input)
    {
        return Week4Bridge::parse(input);
    }

    // =============================================================================
    // 4. toString  (tree → infix string)
    // =============================================================================

    namespace
    {

        // Operator precedence — used to decide when parentheses are needed
        int precedence(ExprType t)
        {
            switch (t)
            {
            case ExprType::ADD:
            case ExprType::SUB:
                return 1;
            case ExprType::MUL:
            case ExprType::DIV:
                return 2;
            case ExprType::POW:
                return 3;
            case ExprType::NEG:
                return 4;
            default:
                return 10;
            }
        }

        bool needsParens(const ExprPtr &child, const ExprPtr &parent, bool isRight)
        {
            int cp = precedence(child->type);
            int pp = precedence(parent->type);
            if (cp < pp)
                return true;
            // For subtraction and division, right operand at same precedence needs parens
            if (cp == pp && isRight &&
                (parent->type == ExprType::SUB || parent->type == ExprType::DIV))
                return true;
            // For right-associative pow: left operand at same precedence needs parens
            if (cp == pp && !isRight && parent->type == ExprType::POW)
                return true;
            return false;
        }

        std::string toStringHelper(const ExprPtr &e, const ExprPtr &parent = nullptr, bool isRight = false)
        {
            if (!e)
                return "null";

            switch (e->type)
            {

            case ExprType::NUMBER:
            {
                // Print integers without decimal point
                double v = e->value;
                if (v == std::floor(v) && std::abs(v) < 1e15)
                {
                    std::ostringstream ss;
                    ss << static_cast<long long>(v);
                    return ss.str();
                }
                std::ostringstream ss;
                ss << std::setprecision(10) << v;
                return ss.str();
            }

            case ExprType::VARIABLE:
            case ExprType::CONSTANT:
                return e->name;

            case ExprType::NEG:
            {
                std::string inner = toStringHelper(e->arg(), e, false);
                // Add parens if inner is ADD/SUB
                if (e->arg()->type == ExprType::ADD || e->arg()->type == ExprType::SUB)
                    inner = "(" + inner + ")";
                return "-" + inner;
            }

            case ExprType::ADD:
            case ExprType::SUB:
            case ExprType::MUL:
            case ExprType::DIV:
            case ExprType::POW:
            {
                std::string L = toStringHelper(e->left(), e, false);
                std::string R = toStringHelper(e->right(), e, true);
                if (needsParens(e->left(), e, false))
                    L = "(" + L + ")";
                if (needsParens(e->right(), e, true))
                    R = "(" + R + ")";
                char op = '+';
                switch (e->type)
                {
                case ExprType::ADD:
                    op = '+';
                    break;
                case ExprType::SUB:
                    op = '-';
                    break;
                case ExprType::MUL:
                    op = '*';
                    break;
                case ExprType::DIV:
                    op = '/';
                    break;
                case ExprType::POW:
                    op = '^';
                    break;
                default:
                    break;
                }
                return L + " " + op + " " + R;
            }

            case ExprType::SIN:
                return "sin(" + toStringHelper(e->arg()) + ")";
            case ExprType::COS:
                return "cos(" + toStringHelper(e->arg()) + ")";
            case ExprType::TAN:
                return "tan(" + toStringHelper(e->arg()) + ")";
            case ExprType::CSC:
                return "csc(" + toStringHelper(e->arg()) + ")";
            case ExprType::SEC:
                return "sec(" + toStringHelper(e->arg()) + ")";
            case ExprType::COT:
                return "cot(" + toStringHelper(e->arg()) + ")";
            case ExprType::ASIN:
                return "asin(" + toStringHelper(e->arg()) + ")";
            case ExprType::ACOS:
                return "acos(" + toStringHelper(e->arg()) + ")";
            case ExprType::ATAN:
                return "atan(" + toStringHelper(e->arg()) + ")";
            case ExprType::ATAN2:
                return "atan2(" + toStringHelper(e->children[0]) + ", " + toStringHelper(e->children[1]) + ")";
            case ExprType::SINH:
                return "sinh(" + toStringHelper(e->arg()) + ")";
            case ExprType::COSH:
                return "cosh(" + toStringHelper(e->arg()) + ")";
            case ExprType::TANH:
                return "tanh(" + toStringHelper(e->arg()) + ")";
            case ExprType::ASINH:
                return "asinh(" + toStringHelper(e->arg()) + ")";
            case ExprType::ACOSH:
                return "acosh(" + toStringHelper(e->arg()) + ")";
            case ExprType::ATANH:
                return "atanh(" + toStringHelper(e->arg()) + ")";
            case ExprType::EXP:
                return "exp(" + toStringHelper(e->arg()) + ")";
            case ExprType::LOG:
                return "ln(" + toStringHelper(e->arg()) + ")";
            case ExprType::LOG2:
                return "log2(" + toStringHelper(e->arg()) + ")";
            case ExprType::LOG10:
                return "log10(" + toStringHelper(e->arg()) + ")";
            case ExprType::LOGB:
                return "log(" + toStringHelper(e->children[0]) + ", " + toStringHelper(e->children[1]) + ")";
            case ExprType::SQRT:
                return "sqrt(" + toStringHelper(e->arg()) + ")";
            case ExprType::CBRT:
                return "cbrt(" + toStringHelper(e->arg()) + ")";
            case ExprType::ABS:
                return "abs(" + toStringHelper(e->arg()) + ")";
            case ExprType::SIGN:
                return "sign(" + toStringHelper(e->arg()) + ")";
            case ExprType::FLOOR:
                return "floor(" + toStringHelper(e->arg()) + ")";
            case ExprType::CEIL:
                return "ceil(" + toStringHelper(e->arg()) + ")";
            case ExprType::FACTORIAL:
                return toStringHelper(e->arg()) + "!";

            case ExprType::FUNC:
            {
                std::string result = e->name + "(";
                for (size_t i = 0; i < e->children.size(); ++i)
                {
                    if (i)
                        result += ", ";
                    result += toStringHelper(e->children[i]);
                }
                return result + ")";
            }

            default:
                return "?";
            }
        }

    } // anonymous namespace

    std::string toString(const ExprPtr &expr)
    {
        return toStringHelper(expr);
    }

    // =============================================================================
    // 5. toLatex  (tree → LaTeX)
    // =============================================================================

    namespace
    {

        std::string latexHelper(const ExprPtr &e)
        {
            if (!e)
                return "";
            switch (e->type)
            {

            case ExprType::NUMBER:
            {
                double v = e->value;
                if (v == std::floor(v) && std::abs(v) < 1e15)
                    return std::to_string(static_cast<long long>(v));
                std::ostringstream ss;
                ss << std::setprecision(6) << v;
                return ss.str();
            }
            case ExprType::VARIABLE:
                return e->name;
            case ExprType::CONSTANT:
                if (e->name == "pi")
                    return "\\pi";
                if (e->name == "inf")
                    return "\\infty";
                return e->name;

            case ExprType::ADD:
                return latexHelper(e->left()) + " + " + latexHelper(e->right());
            case ExprType::SUB:
            {
                std::string r = latexHelper(e->right());
                // Wrap right side in parens if it is ADD or SUB
                if (e->right()->type == ExprType::ADD || e->right()->type == ExprType::SUB)
                    r = "\\left(" + r + "\\right)";
                return latexHelper(e->left()) + " - " + r;
            }
            case ExprType::MUL:
            {
                std::string L = latexHelper(e->left());
                std::string R = latexHelper(e->right());
                // Use \cdot only if both sides are non-trivial
                bool needDot = !(e->left()->isNumber() || e->right()->isNumber());
                return L + (needDot ? " \\cdot " : " ") + R;
            }
            case ExprType::DIV:
                return "\\frac{" + latexHelper(e->left()) + "}{" + latexHelper(e->right()) + "}";
            case ExprType::POW:
            {
                std::string base = latexHelper(e->left());
                std::string exp = latexHelper(e->right());
                // Base needs braces if it's more than one character
                if (e->left()->type != ExprType::NUMBER && e->left()->type != ExprType::VARIABLE)
                    base = "\\left(" + base + "\\right)";
                return base + "^{" + exp + "}";
            }
            case ExprType::NEG:
                return "-" + latexHelper(e->arg());
            case ExprType::SIN:
                return "\\sin\\left(" + latexHelper(e->arg()) + "\\right)";
            case ExprType::COS:
                return "\\cos\\left(" + latexHelper(e->arg()) + "\\right)";
            case ExprType::TAN:
                return "\\tan\\left(" + latexHelper(e->arg()) + "\\right)";
            case ExprType::CSC:
                return "\\csc\\left(" + latexHelper(e->arg()) + "\\right)";
            case ExprType::SEC:
                return "\\sec\\left(" + latexHelper(e->arg()) + "\\right)";
            case ExprType::COT:
                return "\\cot\\left(" + latexHelper(e->arg()) + "\\right)";
            case ExprType::ASIN:
                return "\\arcsin\\left(" + latexHelper(e->arg()) + "\\right)";
            case ExprType::ACOS:
                return "\\arccos\\left(" + latexHelper(e->arg()) + "\\right)";
            case ExprType::ATAN:
                return "\\arctan\\left(" + latexHelper(e->arg()) + "\\right)";
            case ExprType::SINH:
                return "\\sinh\\left(" + latexHelper(e->arg()) + "\\right)";
            case ExprType::COSH:
                return "\\cosh\\left(" + latexHelper(e->arg()) + "\\right)";
            case ExprType::TANH:
                return "\\tanh\\left(" + latexHelper(e->arg()) + "\\right)";
            case ExprType::EXP:
                return "e^{" + latexHelper(e->arg()) + "}";
            case ExprType::LOG:
                return "\\ln\\left(" + latexHelper(e->arg()) + "\\right)";
            case ExprType::LOG10:
                return "\\log_{10}\\left(" + latexHelper(e->arg()) + "\\right)";
            case ExprType::LOG2:
                return "\\log_{2}\\left(" + latexHelper(e->arg()) + "\\right)";
            case ExprType::LOGB:
                return "\\log_{" + latexHelper(e->children[0]) + "}\\left(" + latexHelper(e->children[1]) + "\\right)";
            case ExprType::SQRT:
                return "\\sqrt{" + latexHelper(e->arg()) + "}";
            case ExprType::CBRT:
                return "\\sqrt[3]{" + latexHelper(e->arg()) + "}";
            case ExprType::ABS:
                return "\\left|" + latexHelper(e->arg()) + "\\right|";
            case ExprType::FACTORIAL:
                return latexHelper(e->arg()) + "!";
            case ExprType::FUNC:
            {
                std::string result = "\\" + e->name + "\\left(";
                for (size_t i = 0; i < e->children.size(); ++i)
                {
                    if (i)
                        result += ", ";
                    result += latexHelper(e->children[i]);
                }
                return result + "\\right)";
            }
            default:
                return "?";
            }
        }

    } // anonymous namespace

    std::string toLatex(const ExprPtr &expr)
    {
        return latexHelper(expr);
    }

    // =============================================================================
    // 6. Substitution  (replace variable → sub-tree)
    // =============================================================================

    ExprPtr substitute(const ExprPtr &expr,
                       const std::string &varName,
                       const ExprPtr &replacement)
    {
        if (!expr)
            return nullptr;

        // If this node IS the variable we're replacing, return the replacement
        if (expr->type == ExprType::VARIABLE && expr->name == varName)
            return clone(replacement);

        // Leaf nodes other than the target variable are returned as-is
        if (expr->isLeaf())
            return expr;

        // For everything else: recurse into children and rebuild
        std::vector<ExprPtr> newChildren;
        newChildren.reserve(expr->children.size());
        for (const auto &child : expr->children)
            newChildren.push_back(substitute(child, varName, replacement));

        // Rebuild the node with the same type and name but new children
        auto result = std::make_shared<Expr>(expr->type, expr->name, newChildren);
        result->value = expr->value;
        return result;
    }

    // =============================================================================
    // 7. clone  (deep copy)
    // =============================================================================

    ExprPtr clone(const ExprPtr &expr)
    {
        if (!expr)
            return nullptr;

        if (expr->isLeaf())
        {
            // Leaf nodes are immutable — safe to share, but we deep-copy anyway
            auto c = std::make_shared<Expr>(expr->type, expr->name);
            c->value = expr->value;
            return c;
        }

        std::vector<ExprPtr> newChildren;
        newChildren.reserve(expr->children.size());
        for (const auto &child : expr->children)
            newChildren.push_back(clone(child));

        auto c = std::make_shared<Expr>(expr->type, expr->name, newChildren);
        c->value = expr->value;
        return c;
    }

    // =============================================================================
    // 8. contains  (does this expression use this variable?)
    // =============================================================================

    bool contains(const ExprPtr &expr, const std::string &varName)
    {
        if (!expr)
            return false;
        if (expr->type == ExprType::VARIABLE && expr->name == varName)
            return true;
        for (const auto &child : expr->children)
            if (contains(child, varName))
                return true;
        return false;
    }

    // =============================================================================
    // 9. isConstantExpr  (no variables anywhere)
    // =============================================================================

    bool isConstantExpr(const ExprPtr &expr)
    {
        if (!expr)
            return true;
        if (expr->type == ExprType::VARIABLE)
            return false;
        for (const auto &child : expr->children)
            if (!isConstantExpr(child))
                return false;
        return true;
    }

    // =============================================================================
    // 10. equal  (structural equality)
    // =============================================================================

    bool equal(const ExprPtr &a, const ExprPtr &b)
    {
        if (!a && !b)
            return true;
        if (!a || !b)
            return false;
        if (a->type != b->type)
            return false;
        if (a->type == ExprType::NUMBER)
            return std::abs(a->value - b->value) < 1e-12;
        if (a->type == ExprType::VARIABLE || a->type == ExprType::CONSTANT)
            return a->name == b->name;
        if (a->children.size() != b->children.size())
            return false;
        for (size_t i = 0; i < a->children.size(); ++i)
            if (!equal(a->children[i], b->children[i]))
                return false;
        return true;
    }

    // =============================================================================
    // 11. complexity  (node count — used to choose between algorithms)
    // =============================================================================

    int complexity(const ExprPtr &expr)
    {
        if (!expr)
            return 0;
        int c = 1;
        for (const auto &child : expr->children)
            c += complexity(child);
        return c;
    }

} // namespace Calculus