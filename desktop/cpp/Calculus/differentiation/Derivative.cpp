// =============================================================================
// calculus/differentiation/Derivative.cpp
// =============================================================================

#include "Derivative.hpp"
#include "../core/Week4Bridge.hpp"
#include <stdexcept>
#include <sstream>
#include <cmath>
#include <iomanip>

namespace Calculus {

// =============================================================================
// diff()
//
// Recursively differentiates expr with respect to var.
// Each case builds a new tree from factory functions, then calls simplify()
// on the result before returning so callers can always get clean output.
//
// The chain rule is not a separate case - it falls out automatically.
// Every rule for a composed function f(g(x)) multiplies by diff(g, var)
// at the end, which is exactly the chain rule: d/dx[f(g)] = f'(g) * g'.
// =============================================================================

ExprPtr diff(const ExprPtr& expr, const std::string& var) {
	if (!expr)
		throw std::runtime_error("diff: null expression");

	switch (expr->type) {
	// ── LEAVES ────────────────────────────────────────────────────────────────

	case ExprType::NUMBER:
	case ExprType::CONSTANT:
		// d/dx [constant] = 0
		return num(0.0);

	case ExprType::VARIABLE:
		// d/dx [x] = 1,  d/dx [y] = 0
		return num(expr->name == var ? 1.0 : 0.0);

		// ── BINARY OPERATORS ──────────────────────────────────────────────────────

	case ExprType::ADD:
		// (f + g)' = f' + g'
		return simplify(add(
			diff(expr->left(), var),
			diff(expr->right(), var)
		));

	case ExprType::SUB:
		// (f - g)' = f' - g'
		return simplify(sub(
			diff(expr->left(), var),
			diff(expr->right(), var)
		));

	case ExprType::MUL:
		{
			// Product rule: (f*g)' = f'*g + f*g'
			auto f  = expr->left(),  g  = expr->right();
			auto fp = diff(f, var), gp = diff(g, var);
			return simplify(add(
				mul(fp, g),
				mul(f,  gp)
			));
		}

	case ExprType::DIV:
		{
			// Quotient rule: (f/g)' = (f'*g - f*g') / g^2
			auto f  = expr->left(),  g = expr->right();
			auto fp = diff(f, var),  gp = diff(g, var);
			return simplify(div_expr(
				sub(mul(fp, g), mul(f, gp)),
				pow_expr(g, num(2.0))
			));
		}

	case ExprType::POW:
		{
			auto base = expr->left();
			auto exp  = expr->right();
			bool baseHasVar  = contains(base, var);
			bool expHasVar   = contains(exp,  var);

			if (!baseHasVar && !expHasVar) {
				// Both constant w.r.t. var = derivative is 0
				return num(0.0);
			}

			if (!expHasVar) {
				// d/dx [f^n] = n * f^(n-1) * f'
				// Works for any constant exponent, not just integers
				auto fp = diff(base, var);
				return simplify(mul(
					mul(exp, pow_expr(base, sub(exp, num(1.0)))),
					fp
				));
			}

			if (!baseHasVar) {
				// d/dx [a^g] = a^g * ln(a) * g'
				auto gp = diff(exp, var);
				return simplify(mul(
					mul(expr, log_expr(base)),
					gp
				));
			}

			// General case: d/dx [f^g] = f^g * (f'*ln(f) + g*f'/f)
			// Derived from: f^g = e^(g*ln(f))
			auto fp = diff(base, var);
			auto gp = diff(exp, var);
			return simplify(mul(
				expr,
				add(
					mul(gp, log_expr(base)),
					mul(exp, div_expr(fp, base))
				)
			));
		}

	// ── UNARY MINUS ───────────────────────────────────────────────────────────

	case ExprType::NEG:
		// (-f)' = -f'
		return simplify(neg(diff(expr->arg(), var)));

	// ── TRIG ──────────────────────────────────────────────────────────────────

	case ExprType::SIN:
		{
			// d/dx [sin(f)] = cos(f) * f'
			auto f = expr->arg();
			auto fp = diff(f, var);
			return simplify(mul(cos_expr(f), fp));
		}

	case ExprType::COS:
		{
			// d/dx [cos(f)] = -sin(f) * f'
			auto f  = expr->arg();
			auto fp = diff(f, var);
			return simplify(neg(mul(sin_expr(f), fp)));
		}

	case ExprType::TAN: {
			// d/dx [tan(f)] = sec^2(f) * f'  =  f' / cos^2(f)
			auto f  = expr->arg();
			auto fp = diff(f, var);
			return simplify(div_expr(fp, pow_expr(cos_expr(f), num(2.0))));
	}

	case ExprType::CSC: {
			// d/dx [csc(f)] = -csc(f)*cot(f) * f'
			auto f  = expr->arg();
			auto fp = diff(f, var);
			return simplify(neg(mul(
				mul(csc_expr(f), cot_expr(f)),
				fp
			)));
	}

	case ExprType::SEC: {
			// d/dx [sec(f)] = sec(f)*tan(f) * f'
			auto f  = expr->arg();
			auto fp = diff(f, var);
			return simplify(mul(
				mul(sec_expr(f), tan_expr(f)),
				fp
			));
	}

	case ExprType::COT: {
			// d/dx [cot(f)] = -csc^2(f) * f'
			auto f  = expr->arg();
			auto fp = diff(f, var);
			return simplify(neg(mul(
				pow_expr(csc_expr(f), num(2.0)),
				fp
			)));
	}

	// ── INVERSE TRIG ─────────────────────────────────────────────────────────

	case ExprType::ASIN: {
			// d/dx [asin(f)] = f' / sqrt(1 - f^2)
			auto f  = expr->arg();
			auto fp = diff(f, var);
			return simplify(div_expr(
				fp,
				sqrt_expr(sub(num(1.0), pow_expr(f, num(2.0))))
			));
	}

	case ExprType::ACOS: {
			// d/dx [acos(f)] = -f' / sqrt(1 - f^2)
			auto f  = expr->arg();
			auto fp = diff(f, var);
			return simplify(neg(div_expr(
				fp,
				sqrt_expr(sub(num(1.0), pow_expr(f, num(2.0))))
			)));
	}

	case ExprType::ATAN: {
			// d/dx [atan(f)] = f' / (1 + f^2)
			auto f  = expr->arg();
			auto fp = diff(f, var);
			return simplify(div_expr(
				fp,
				add(num(1.0), pow_expr(f, num(2.0)))
			));
	}

    // ── HYPERBOLIC ────────────────────────────────────────────────────────────

    case ExprType::SINH: {
        // d/dx [sinh(f)] = cosh(f) * f'
        auto f  = expr->arg();
        auto fp = diff(f, var);
        return simplify(mul(cosh_expr(f), fp));
    }

    case ExprType::COSH: {
        // d/dx [cosh(f)] = sinh(f) * f'
        auto f  = expr->arg();
        auto fp = diff(f, var);
        return simplify(mul(sinh_expr(f), fp));
    }

    case ExprType::TANH: {
        // d/dx [tanh(f)] = f' / cosh^2(f)
        auto f  = expr->arg();
        auto fp = diff(f, var);
        return simplify(div_expr(fp, pow_expr(cosh_expr(f), num(2.0))));
    }

    case ExprType::ASINH: {
        // d/dx [asinh(f)] = f' / sqrt(f^2 + 1)
        auto f  = expr->arg();
        auto fp = diff(f, var);
        return simplify(div_expr(
            fp,
            sqrt_expr(add(pow_expr(f, num(2.0)), num(1.0)))
        ));
    }

    case ExprType::ACOSH: {
        // d/dx [acosh(f)] = f' / sqrt(f^2 - 1)
        auto f  = expr->arg();
        auto fp = diff(f, var);
        return simplify(div_expr(
            fp,
            sqrt_expr(sub(pow_expr(f, num(2.0)), num(1.0)))
        ));
    }

    case ExprType::ATANH: {
        // d/dx [atanh(f)] = f' / (1 - f^2)
        auto f  = expr->arg();
        auto fp = diff(f, var);
        return simplify(div_expr(
            fp,
            sub(num(1.0), pow_expr(f, num(2.0)))
        ));
    }

    // ── EXPONENTIAL ───────────────────────────────────────────────────────────

    case ExprType::EXP: {
        // d/dx [exp(f)] = exp(f) * f'
        auto f  = expr->arg();
        auto fp = diff(f, var);
        return simplify(mul(exp_expr(f), fp));
    }

    // ── LOGARITHMS ────────────────────────────────────────────────────────────

    case ExprType::LOG: {
        // d/dx [ln(f)] = f' / f
        auto f  = expr->arg();
        auto fp = diff(f, var);
        return simplify(div_expr(fp, f));
    }

    case ExprType::LOG10: {
        // d/dx [log10(f)] = f' / (f * ln(10))
        auto f  = expr->arg();
        auto fp = diff(f, var);
        return simplify(div_expr(
            fp,
            mul(f, num(std::log(10.0)))
        ));
    }

    case ExprType::LOG2: {
        // d/dx [log2(f)] = f' / (f * ln(2))
        auto f  = expr->arg();
        auto fp = diff(f, var);
        return simplify(div_expr(
            fp,
            mul(f, num(std::log(2.0)))
        ));
    }

    case ExprType::LOGB: {
        // d/dx [log_b(f)] = f' / (f * ln(b))
        // children[0] = b (base), children[1] = f (argument)
        auto b  = expr->children[0];
        auto f  = expr->children[1];
        auto fp = diff(f, var);
        return simplify(div_expr(
            fp,
            mul(f, log_expr(b))
        ));
    }

    // ── ROOTS ─────────────────────────────────────────────────────────────────

    case ExprType::SQRT: {
        // d/dx [sqrt(f)] = f' / (2 * sqrt(f))
        auto f  = expr->arg();
        auto fp = diff(f, var);
        return simplify(div_expr(
            fp,
            mul(num(2.0), sqrt_expr(f))
        ));
    }

    case ExprType::CBRT: {
        // d/dx [cbrt(f)] = f' / (3 * f^(2/3))
        auto f  = expr->arg();
        auto fp = diff(f, var);
        return simplify(div_expr(
            fp,
            mul(num(3.0), pow_expr(f, num(2.0 / 3.0)))
        ));
    }

    // ── ABSOLUTE VALUE ────────────────────────────────────────────────────────

    case ExprType::ABS: {
        // d/dx [|f|] = sign(f) * f'
        // Technically undefined at f=0, but this is the standard result.
        auto f  = expr->arg();
        auto fp = diff(f, var);
        auto sgn = std::make_shared<Expr>(ExprType::SIGN, f);
        return simplify(mul(sgn, fp));
    }

    // ── UNKNOWN FUNCTION ──────────────────────────────────────────────────────

    case ExprType::FUNC: {
        // Cannot differentiate an unknown function symbolically.
        // Return a placeholder: D[f(x), x]
        if (expr->children.size() == 1) {
            auto fp = diff(expr->children[0], var);
            // Return D(name, x) * inner derivative (chain rule placeholder)
            std::string dname = "D[" + expr->name + "]";
            auto dnode = std::make_shared<Expr>(
                ExprType::FUNC, dname,
                std::vector<ExprPtr>{expr->children[0]});
            return simplify(mul(dnode, fp));
        }
        throw std::runtime_error(
            "Cannot symbolically differentiate unknown function: " + expr->name);
    }

    // ── UNHANDLED ─────────────────────────────────────────────────────────────

    default:
        throw std::runtime_error(
            "diff: unhandled expression type: " +
            std::to_string(static_cast<int>(expr->type)));
    }
}

// =============================================================================
// diffN - nth derivative
// =============================================================================

ExprPtr diffN(const ExprPtr& expr, const std::string& var, int n) {
	if (n < 0)  throw std::invalid_argument("diffN: order must be >= 0");
	if (n == 0) return expr;
	ExprPtr result = expr;
	for (int i = 0; i < n; ++i)
		result = diff(result, var);
	return result;
}

// =============================================================================
// differentiate - parse, diff, format
// =============================================================================

DiffResult differentiate(const std::string& inputExpr, const std::string& varName, int order) {
	DiffResult out;
	try {
		ExprPtr expr   = parse(inputExpr);
		ExprPtr result = diffN(expr, varName, order);
		ExprPtr simple = simplify(result);

		out.symbolic = toString(simple);
		out.latex = toLatex(simple);

		// If the result as no variables, evaluate to a number
		if (isConstantExpr(simple)) {
			double val   = evaluate(simple);
			std::ostringstream ss;
			ss << std::setprecision(10) << val;
			out.numerical = ss.str();
		}

	} catch (const std::exception& ex) {
		out.ok    = false;
		out.error = ex.what();
	}
	return out;
}

}