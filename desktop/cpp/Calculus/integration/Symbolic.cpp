// calculus/integration/Symbolic.cpp

#include "Symbolic.hpp"
#include "../limits/Limits.hpp"
#include "../core/Week4Bridge.hpp"
#include <cmath>
#include <sstream>
#include <iomanip>
#include <algorithm>

namespace Calculus {

static constexpr double VERIFY_EPS = 1e-6;

// Verification helper

bool verifyAntiderivative(const ExprPtr& antideriv, const ExprPtr& original, const std::string& var) {
	ExprPtr d = simplify(diff(antideriv, var));
	for (double x : {0.5, 1.0, 1.5, 2.0, 2.5}) {
		try {
			double dVal = evaluate(d,        {{var, x}});
			double oVal = evaluate(original, {{var, x}});
			if (std::abs(dVal - oVal) > VERIFY_EPS * (1.0 + std::abs(oVal)))
				return false;
		} catch (...) { continue; }
	}
	return true;
}


// Helper: var_expr wrapping
static ExprPtr var_expr(const std::string& v) { return var(v); }

// Helper to check if a value is a number equal to v
static bool isNum(const ExprPtr& e, double v) {
	return e && e->isNumber() && std::abs(e->value - v) < 1e-12;
}

// DIRECT RULES

std::optional<ExprPtr> integrateDirectRule(const ExprPtr& expr,
                                            const std::string& var) {
    if (!expr) return std::nullopt;

    if (!contains(expr, var)) {
        // ∫c dx = c*x
        return mul(expr, var_expr(var));
    }

    if (expr->isVar(var)) {
        return div_expr(pow_expr(var_expr(var), num(2.0)), num(2.0));
    }

    if (expr->type == ExprType::POW) {
        auto base = expr->left(), exp = expr->right();
        if (base->isVar(var) && isConstantExpr(exp)) {
            double n = evaluate(exp);
            if (std::abs(n + 1.0) < 1e-12) {
                // ∫x^(-1) dx = ln|x|
                return log_expr(abs_expr(var_expr(var)));
            }
            // ∫x^n dx = x^(n+1) / (n+1)
            return div_expr(
                pow_expr(var_expr(var), num(n + 1.0)),
                num(n + 1.0)
            );
        }
    }

    if (expr->type == ExprType::DIV) {
        auto L = expr->left(), R = expr->right();
        if (isNum(L, 1.0) && R->isVar(var)) {
            return log_expr(abs_expr(var_expr(var)));
        }
    }

    if (expr->type == ExprType::EXP && expr->arg()->isVar(var)) {
        return exp_expr(var_expr(var));
    }

    if (expr->type == ExprType::POW) {
        auto base = expr->left(), exp = expr->right();
        if (isConstantExpr(base) && exp->isVar(var)) {
            return div_expr(
                pow_expr(base, var_expr(var)),
                log_expr(base)
            );
        }
    }

    if (expr->arg().get() && expr->children.size() == 1) {
        auto inner = expr->arg();
        if (inner->isVar(var)) {
            switch (expr->type) {
            // ∫sin(x) = -cos(x)
            case ExprType::SIN:
                return neg(cos_expr(var_expr(var)));
            // ∫cos(x) = sin(x)
            case ExprType::COS:
                return sin_expr(var_expr(var));
            // ∫tan(x) = -ln|cos(x)|  = ln|sec(x)|
            case ExprType::TAN:
                return neg(log_expr(abs_expr(cos_expr(var_expr(var)))));
            // ∫cot(x) = ln|sin(x)|
            case ExprType::COT:
                return log_expr(abs_expr(sin_expr(var_expr(var))));
            // ∫sec(x) = ln|sec(x) + tan(x)|
            case ExprType::SEC:
                return log_expr(abs_expr(add(
                    sec_expr(var_expr(var)),
                    tan_expr(var_expr(var)))));
            // ∫csc(x) = -ln|csc(x) + cot(x)|  = ln|csc(x) - cot(x)|
            case ExprType::CSC:
                return neg(log_expr(abs_expr(add(
                    csc_expr(var_expr(var)),
                    cot_expr(var_expr(var))))));
            // ∫sinh(x) = cosh(x)
            case ExprType::SINH:
                return cosh_expr(var_expr(var));
            // ∫cosh(x) = sinh(x)
            case ExprType::COSH:
                return sinh_expr(var_expr(var));
            // ∫tanh(x) = ln(cosh(x))
            case ExprType::TANH:
                return log_expr(cosh_expr(var_expr(var)));
            // ∫ln(x) = x*ln(x) - x
            case ExprType::LOG:
                return sub(
                    mul(var_expr(var), log_expr(var_expr(var))),
                    var_expr(var));
            default: break;
            }
        }
    }

    // ∫1/sqrt(1-x²) dx = asin(x)
    // ∫1/(1+x²) dx    = atan(x)
    // ∫1/sqrt(x²-1) dx= acosh(x)   (for |x|>1)
    if (expr->type == ExprType::DIV) {
        auto num_part = expr->left(), den_part = expr->right();
        if (isConstantExpr(num_part) && std::abs(evaluate(num_part) - 1.0) < 1e-12) {
            // 1 / (1 + x^2) → atan(x)
            if (den_part->type == ExprType::ADD) {
                auto L = den_part->left(), R = den_part->right();
                if (isNum(L, 1.0) && R->type == ExprType::POW
                    && R->left()->isVar(var) && isNum(R->right(), 2.0)) {
                    return atan_expr(var_expr(var));
                }
            }
            // 1 / sqrt(1 - x^2) → asin(x)
            if (den_part->type == ExprType::SQRT) {
                auto inner = den_part->arg();
                if (inner->type == ExprType::SUB) {
                    auto L2 = inner->left(), R2 = inner->right();
                    if (isNum(L2, 1.0) && R2->type == ExprType::POW
                        && R2->left()->isVar(var) && isNum(R2->right(), 2.0)) {
                        return asin_expr(var_expr(var));
                    }
                }
            }
        }
    }

    return std::nullopt;
}

// U-SUBSTITUTION
// Detects: expr = f(g(x)) * g'(x)
// Strategy:
//   2. Try u = inner argument of trig/exp/log/pow functions
//   3. Integrate in u, substitute back

std::optional<ExprPtr> integrateUSub(const ExprPtr& expr,
                                      const std::string& var) {
    // Helper: check if candidate is the derivative of inner
    auto tryU = [&](const ExprPtr& inner,
                    const ExprPtr& outer) -> std::optional<ExprPtr> {
        if (!contains(inner, var)) return std::nullopt;
        ExprPtr du = simplify(diff(inner, var));
        if (isConstantExpr(du)) return std::nullopt;

        ExprPtr ratio = simplify(div_expr(outer, du));
        if (!contains(ratio, var)) {
        }

        // Use a fresh variable 'u' for the substitution
        const std::string u = "_u_";
        ExprPtr ratio_in_u = substitute(ratio, var, inner);
        // We check if ratio is a function of inner only
        // to see if ratio depends only on inner
        bool onlyInner = true;
        for (double t : {0.3, 0.7, 1.2}) {
            try {
                double iv = evaluate(inner, {{var, t}});
                double rv = evaluate(ratio,  {{var, t}});
                // Now evaluate ratio_in_u treating _u_ = iv
                double rv2 = evaluate(ratio, {{var, t + 0.01}});
                double dv1 = evaluate(inner, {{var, t + 0.01}});
                if (std::abs(iv - dv1) < 1e-8 && std::abs(rv - rv2) > 1e-6) {
                    onlyInner = false; break;
                }
            } catch (...) {}
        }
        if (!onlyInner) return std::nullopt;

        // Substitute inner → u and try to integrate in u
        ExprPtr exprInU = substitute(ratio, var, parse("_u_"));
        // Hack: we need to integrate exprInU * du/d_u_=1 in _u_
        auto antiderivInU = integrateDirectRule(exprInU, u);
        if (!antiderivInU) return std::nullopt;

        // Substitute back: _u_ → inner
        ExprPtr result = substitute(*antiderivInU, u, inner);
        return simplify(result);
    };

    // Try u = inner of each composed function
    auto tryFuncInner = [&](const ExprPtr& e) -> std::optional<ExprPtr> {
        if (e->children.size() != 1) return std::nullopt;
        ExprPtr inner = e->arg();
        if (!contains(inner, var) || inner->isVar(var)) return std::nullopt;
        ExprPtr du = simplify(diff(inner, var));
        if (isConstantExpr(du)) return std::nullopt;
        // Reconstruct: expr / du should be the function of inner / du
        ExprPtr quotient = simplify(div_expr(expr, du));
        return tryU(inner, quotient);
    };

    // Walk the tree looking for composed functions
    std::function<std::optional<ExprPtr>(const ExprPtr&)> search = [&](const ExprPtr& e) -> std::optional<ExprPtr> {
	    if (!e) return std::nullopt;
	    // Try this node
	    if (!e->isLeaf() && e->children.size() == 1) {
		    if (auto r = tryFuncInner(e)) return r;
	    }
	    // If it's MUL, try each factor
	    if (e->type == ExprType::MUL) {
		    ExprPtr L = e->left(), R = e->right();
		    if (!L->isLeaf() && L->children.size() == 1) {
			    ExprPtr du = simplify(diff(L->arg(), var));
			    ExprPtr ratio = simplify(div_expr(R, du));
			    if (isConstantExpr(ratio)) {
				    if (auto ai = integrateDirectRule(L, var)) return simplify(mul(ratio, *ai));
			    }
		    }
		    if (!R->isLeaf() && R->children.size() == 1) {
			    ExprPtr du = simplify(diff(R->arg(), var));
			    ExprPtr ratio = simplify(div_expr(L, du));
			    if (isConstantExpr(ratio)) {
				    if (auto ai = integrateDirectRule(R, var)) return simplify(mul(ratio, *ai));
			    }
		    }
	    }
	    return std::nullopt;
    };

    return search(expr);
}

// INTEGRATION BY PARTS
// ∫u dv = u*v - ∫v du
// LIATE priority (higher = chosen as u first):

static int liatePriority(const ExprPtr& expr, const std::string& var) {
    if (!contains(expr, var)) return 0;
    switch (expr->type) {
    case ExprType::LOG: case ExprType::LOG2:
    case ExprType::LOG10: case ExprType::LOGB:       return 5;
    case ExprType::ASIN: case ExprType::ACOS:
    case ExprType::ATAN: case ExprType::ASINH:
    case ExprType::ACOSH: case ExprType::ATANH:      return 4;
    case ExprType::POW:
        if (isConstantExpr(expr->right())) return 3;  // polynomial
        return 1;
    case ExprType::VARIABLE:                          return 3;
    case ExprType::SIN: case ExprType::COS:
    case ExprType::TAN: case ExprType::CSC:
    case ExprType::SEC: case ExprType::COT:          return 2;
    case ExprType::EXP: case ExprType::SINH:
    case ExprType::COSH: case ExprType::TANH:        return 1;
    default:                                          return 1;
    }
}

std::optional<ExprPtr> integrateByParts(const ExprPtr& expr,
                                         const std::string& var) {
    // Only applies when expr is a product
    if (expr->type != ExprType::MUL) {
        // Try: expr itself might be ln(x), asin(x), etc.
        // These are IBP with dv = dx, v = x
        int p = liatePriority(expr, var);
        if (p >= 4) {
            // u = expr, dv = 1 dx → v = x
            const ExprPtr& u  = expr;
            ExprPtr v  = var_expr(var);
            ExprPtr du = simplify(diff(u, var));
            ExprPtr vdu = simplify(mul(v, du));
            auto vdu_int = integrate(vdu, var);
            if (!vdu_int) return std::nullopt;
            return simplify(sub(mul(u, v), *vdu_int));
        }
        return std::nullopt;
    }

    ExprPtr L = expr->left(), R = expr->right();

    // Choose u as the factor with higher LIATE priority
    ExprPtr u_part, dv_part;
    if (liatePriority(L, var) >= liatePriority(R, var)) {
        u_part  = L;
        dv_part = R;
    } else {
        u_part  = R;
        dv_part = L;
    }

    // Find v = ∫dv
    auto v_opt = integrate(dv_part, var);
    if (!v_opt) return std::nullopt;
    const ExprPtr& v = *v_opt;

    // du = d(u)/dx
    ExprPtr du = simplify(diff(u_part, var));

    // ∫v du
    ExprPtr vdu = simplify(mul(v, du));

    if (equal(simplify(vdu), simplify(expr))) return std::nullopt;

    // Recursively integrate v*du (limited depth)
    auto vdu_int = integrate(vdu, var);
    if (!vdu_int) return std::nullopt;

    // IBP result: u*v - ∫v*du
    return simplify(sub(mul(u_part, v), *vdu_int));
}

// PARTIAL FRACTIONS
// Handles linear and repeated linear factors.
// The denominator is matched against known patterns.

std::optional<ExprPtr> integratePartialFractions(const ExprPtr& expr,
                                                   const std::string& var) {
    if (expr->type != ExprType::DIV) return std::nullopt;

    ExprPtr P = expr->left(), Q = expr->right();

    // Must be a polynomial-like expression over var
    if (!contains(Q, var)) return std::nullopt;

    // Pattern: 1 / (x-a)(x-b) → A/(x-a) + B/(x-b)
    // We handle the most common cases symbolically:

    if (Q->type == ExprType::ADD || Q->type == ExprType::SUB) {
        // Try: num=1, den = x^2 + c  → atan(x/sqrt(c)) / sqrt(c)
        if (isNum(P, 1.0) || isConstantExpr(P)) {
            double pVal = evaluate(P);
            // 1 / (x^2 + a^2) → (1/a) * atan(x/a)
            // Match Q = x^2 + a^2
            if (Q->type == ExprType::ADD) {
                ExprPtr QL = Q->left(), QR = Q->right();
                // x^2 + a^2
                bool lIsX2 = QL->type == ExprType::POW
                             && QL->left()->isVar(var)
                             && isNum(QL->right(), 2.0);
                bool rIsConst = isConstantExpr(QR) && !QR->isNumber();
                if (!rIsConst && isConstantExpr(QR) && QR->isNumber()) {
                    rIsConst = QR->value > 0;
                }
                if (lIsX2 && rIsConst) {
                    double a2 = evaluate(QR);
                    if (a2 > 0) {
                        double a = std::sqrt(a2);
                        // ∫ pVal/(x²+a²) dx = (pVal/a) * atan(x/a)
                        return simplify(mul(
                            num(pVal / a),
                            atan_expr(div_expr(var_expr(var), num(a)))
                        ));
                    }
                }
            }
        }
    }

    // Pattern: MUL denominator (x-a)(x-b)
    if (Q->type == ExprType::MUL && isNum(P, 1.0)) {
        ExprPtr A = Q->left(), B = Q->right();
        // (x - a)(x - b): get a and b
        auto getRootOfLinear = [&](const ExprPtr& e) -> std::optional<double> {
            // Matches x-c or x+c
            if (e->type == ExprType::SUB && e->left()->isVar(var)
                && isConstantExpr(e->right()))
                return evaluate(e->right());
            if (e->type == ExprType::ADD && e->left()->isVar(var)
                && isConstantExpr(e->right()))
                return -evaluate(e->right());
            if (e->isVar(var)) return 0.0;
            return std::nullopt;
        };
        auto ra = getRootOfLinear(A), rb = getRootOfLinear(B);
        if (ra && rb && std::abs(*ra - *rb) > 1e-10) {
            double a = *ra, b = *rb;
            // 1/[(x-a)(x-b)] = [1/(a-b)] * [1/(x-a) - 1/(x-b)]
            double coeff = 1.0 / (a - b);
            ExprPtr xa = sub(var_expr(var), num(a));
            ExprPtr xb = sub(var_expr(var), num(b));
            // ∫ = coeff * [ln|x-a| - ln|x-b|]
            return simplify(mul(
                num(coeff),
                sub(log_expr(abs_expr(xa)),
                    log_expr(abs_expr(xb)))
            ));
        }
    }

    return std::nullopt;
}

// TRIG SUBSTITUTION

std::optional<ExprPtr> integrateTrigSub(const ExprPtr& expr,
                                         const std::string& var) {
    // Look for sqrt(a^2 - x^2), sqrt(a^2 + x^2), sqrt(x^2 - a^2)

    std::function<std::optional<ExprPtr>(const ExprPtr&)> findSqrt;
    findSqrt = [&](const ExprPtr& e) -> std::optional<ExprPtr> {
        if (!e) return std::nullopt;
        if (e->type == ExprType::SQRT) return e;
        for (auto& c : e->children) {
	        if (auto r = findSqrt(c)) return r;
        }
        return std::nullopt;
    };

    auto sqrtNode = findSqrt(expr);
    if (!sqrtNode) return std::nullopt;

    ExprPtr inner = (*sqrtNode)->arg();
    if (!contains(inner, var)) return std::nullopt;

    // Pattern: a^2 - x^2
    if (inner->type == ExprType::SUB) {
        ExprPtr L = inner->left(), R = inner->right();
        if (isConstantExpr(L) && R->type == ExprType::POW
            && R->left()->isVar(var) && isNum(R->right(), 2.0)) {
            double a2 = evaluate(L);
            if (a2 > 0) {
                double a = std::sqrt(a2);
                // sqrt(a²-x²): x = a*sin(t), dx = a*cos(t)dt, sqrt = a*cos(t)
                // For ∫sqrt(a²-x²)dx = (a²/2)(t + sin(t)cos(t))
                //   = (x/2)*sqrt(a²-x²) + (a²/2)*asin(x/a)
                ExprPtr xa   = div_expr(var_expr(var), num(a));
                ExprPtr sq   = sqrt_expr(sub(num(a2), pow_expr(var_expr(var), num(2.0))));
                ExprPtr part1 = mul(div_expr(var_expr(var), num(2.0)), sq);
                ExprPtr part2 = mul(num(a2 / 2.0), asin_expr(xa));
                return simplify(add(part1, part2));
            }
        }
    }

    // Pattern: x^2 - a^2
    if (inner->type == ExprType::SUB) {
        ExprPtr L = inner->left(), R = inner->right();
        if (R->type == ExprType::POW && R->right()->isNum(2.0)
            && isConstantExpr(L)) { /* swap */ }
        if (L->type == ExprType::POW && L->left()->isVar(var)
            && isNum(L->right(), 2.0) && isConstantExpr(R)) {
	        if (double a2 = evaluate(R); a2 > 0) {
                ExprPtr sq    = sqrt_expr(sub(pow_expr(var_expr(var), num(2.0)), num(a2)));
                ExprPtr part1 = mul(div_expr(var_expr(var), num(2.0)), sq);
                ExprPtr part2 = mul(num(a2 / 2.0),
                    log_expr(abs_expr(add(var_expr(var), sq))));
                return simplify(sub(part1, part2));
            }
        }
    }

    return std::nullopt;
}

// TRIG IDENTITIES (powers of sin/cos)

std::optional<ExprPtr> integrateTrigIdentity(const ExprPtr& expr,
                                              const std::string& var) {
    // sin²(x) = (1 - cos(2x))/2
    // cos²(x) = (1 + cos(2x))/2
    // sin(x)cos(x) = sin(2x)/2

    // Pattern: sin^2(x) or cos^2(x)
    if (expr->type == ExprType::POW) {
        ExprPtr base = expr->left(), exp = expr->right();
        if (isNum(exp, 2.0)) {
            if (base->type == ExprType::SIN && base->arg()->isVar(var)) {
                // ∫sin²(x)dx = x/2 - sin(2x)/4
                ExprPtr x2 = mul(num(2.0), var_expr(var));
                return simplify(sub(
                    div_expr(var_expr(var), num(2.0)),
                    div_expr(sin_expr(x2), num(4.0))
                ));
            }
            if (base->type == ExprType::COS && base->arg()->isVar(var)) {
                // ∫cos²(x)dx = x/2 + sin(2x)/4
                ExprPtr x2 = mul(num(2.0), var_expr(var));
                return simplify(add(
                    div_expr(var_expr(var), num(2.0)),
                    div_expr(sin_expr(x2), num(4.0))
                ));
            }
        }
        // sin^n and cos^n via reduction formulae
        if (base->type == ExprType::SIN && base->arg()->isVar(var)
            && exp->isNumber() && exp->value > 2.0
            && exp->value == std::floor(exp->value)) {
            int n = static_cast<int>(exp->value);
            ExprPtr sinNm1 = pow_expr(sin_expr(var_expr(var)), num(n - 1));
            ExprPtr cosX   = cos_expr(var_expr(var));
            ExprPtr sinNm2 = pow_expr(sin_expr(var_expr(var)), num(n - 2));
            if (auto subInt  = integrate(sinNm2, var)) {
                return simplify(add(
                    neg(div_expr(mul(sinNm1, cosX), num(n))),
                    mul(num(static_cast<double>(n - 1)/n), *subInt)
                ));
            }
        }
    }

    // sin(x)*cos(x) = sin(2x)/2 → ∫ = -cos(2x)/4
    if (expr->type == ExprType::MUL) {
        ExprPtr L = expr->left(), R = expr->right();
        bool lSin = L->type == ExprType::SIN && L->arg()->isVar(var);
        bool rCos = R->type == ExprType::COS && R->arg()->isVar(var);
        bool lCos = L->type == ExprType::COS && L->arg()->isVar(var);
        bool rSin = R->type == ExprType::SIN && R->arg()->isVar(var);
        if ((lSin && rCos) || (lCos && rSin)) {
            // ∫sin(x)cos(x)dx = -cos(2x)/4
            ExprPtr x2 = mul(num(2.0), var_expr(var));
            return simplify(neg(div_expr(cos_expr(x2), num(4.0))));
        }
    }

    return std::nullopt;
}

// Main integrate() — tries all strategies in order

std::optional<ExprPtr> integrate(const ExprPtr& expr, const std::string& var) {
	if (!expr) return std::nullopt;

	// Simplify first
	ExprPtr e = simplify(expr);

	// 1. Direct rule
	auto r = integrateDirectRule(e, var);
	if (r && verifyAntiderivative(*r, e, var)) return r;

	// 2. Trig identity (before u-sub, catches sin²/cos² patterns)
	r = integrateTrigIdentity(e, var);
	if (r && verifyAntiderivative(*r, e, var)) return r;

	// 3. U-substitution
	r = integrateUSub(e, var);
	if (r && verifyAntiderivative(*r, e, var)) return r;

	// 4. Integration by parts
	r = integrateByParts(e, var);
	if (r && verifyAntiderivative(*r, e, var)) return r;

	// 5. Partial fractions
	r = integratePartialFractions(e, var);
	if (r && verifyAntiderivative(*r, e, var)) return r;

	// 6. Trig substitution
	r = integrateTrigSub(e, var);
	if (r && verifyAntiderivative(*r, e, var)) return r;

	// 7. Linearity: ∫(f+g)dx = ∫f dx + ∫g dx
	if (e->type == ExprType::ADD) {
		auto li = integrate(e->left(),  var);
		auto ri = integrate(e->right(), var);
		if (li && ri) return simplify(add(*li, *ri));
	}
	if (e->type == ExprType::SUB) {
		auto li = integrate(e->left(),  var);
		auto ri = integrate(e->right(), var);
		if (li && ri) return simplify(sub(*li, *ri));
	}

	// 8. Constant multiple: ∫c*f dx = c * ∫f dx
	if (e->type == ExprType::MUL) {
		if (isConstantExpr(e->left())) {
			if (const auto ri = integrate(e->right(), var)) return simplify(mul(e->left(), *ri));
		}
		if (isConstantExpr(e->right())) {
			if (const auto li = integrate(e->left(), var)) return simplify(mul(e->right(), *li));
		}
	}
	if (e->type == ExprType::NEG) {
		if (const auto ai = integrate(e->arg(), var)) return simplify(neg(*ai));
	}

	return std::nullopt;
}

// definiteIntegral

double definiteIntegral(const ExprPtr& expr, const std::string& var, double a, double b) {
	// Try symbolic antiderivative first
	if (const auto F = integrate(expr, var)) {
		try {
			double Fb = evaluate(*F, {{var, b}});
			double Fa = evaluate(*F, {{var, a}});
			if (std::isfinite(Fb) && std::isfinite(Fa))
				return Fb - Fa;
		} catch (...) {}
	}
	// Fall back to Romberg
	Func1D f = makeEvaluator(expr, var);
	bool aInf = !std::isfinite(a), bInf = !std::isfinite(b);
	NumericalResult nr;
	if (aInf && bInf)     nr = improperBothInfinite(f);
	else if (bInf)        nr = improperInfinite(f, a);
	else if (aInf)        nr = improperNegInfinite(f, b);
	else                  nr = romberg(f, a, b, 10);
	return nr.value;
}

// Formatted entry points

SymbolicIntegralResult computeIndefinite(const std::string& exprStr, const std::string& var) {
	SymbolicIntegralResult out;
	try {
		const ExprPtr expr = parse(exprStr);
		if (const auto F = integrate(expr, var)) {
			const ExprPtr result = simplify(*F);
			out.antiderivative       = toString(result) + " + C";
			out.antiderivative_latex = toLatex(result)  + " + C";
			out.method   = "symbolic";
			out.symbolic = true;
		} else {
			out.symbolic             = false;
			out.method               = "no symbolic antiderivative found";
			out.antiderivative       = "cannot integrate symbolically";
			out.antiderivative_latex = "\\text{no closed form}";
		}
	} catch (const std::exception& ex) {
		out.ok    = false;
		out.error = ex.what();
	}
	return out;
}

SymbolicIntegralResult computeDefinite(const std::string& exprStr, const std::string& var,
                                       const std::string& aStr, const std::string& bStr) {
	SymbolicIntegralResult out;
	try {
		ExprPtr expr = parse(exprStr);
		double  a    = parsePoint(aStr);
		double  b    = parsePoint(bStr);

		if (auto F = integrate(expr, var)) {
			ExprPtr result = simplify(*F);
			out.antiderivative       = toString(result) + " + C";
			out.antiderivative_latex = toLatex(result)  + " + C";
			out.symbolic = true;
			out.method = "fundamental theorem of calculus";
			double val;
			try {
				double Fb = evaluate(result, {{var, b}});
				double Fa = evaluate(result, {{var, a}});
				val = Fb - Fa;
			} catch (...) {
				Func1D f = makeEvaluator(expr, var);
				val = romberg(f, a, b, 10).value;
				out.method = "symbolic antiderivative (numerical evaluation)";
			}
			out.numericalValue = val;
			std::ostringstream ss;
			ss << std::setprecision(10) << val;
			out.numericalStr = ss.str();
		} else {
			// Pure numerical fallback
			Func1D f = makeEvaluator(expr, var);
			NumericalResult nr;
			if (!std::isfinite(a) && !std::isfinite(b)) nr = improperBothInfinite(f);
			else if (!std::isfinite(b))                 nr = improperInfinite(f, a);
			else if (!std::isfinite(a))                 nr = improperNegInfinite(f, b);
			else                                        nr = romberg(f, a, b, 10);
			out.symbolic             = false;
			out.numericalValue       = nr.value;
			out.method               = "Romberg (no symbolic form)";
			out.antiderivative       = "no closed form";
			out.antiderivative_latex = "\\text{no closed form}";
			std::ostringstream ss;
			ss << std::setprecision(10) << nr.value;
			out.numericalStr = ss.str();
		}
	} catch (const std::exception& ex) {
		out.ok    = false;
		out.error = ex.what();
	}
	return out;
}

}