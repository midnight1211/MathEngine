// =============================================================================
// calculus/differentiation/Implicit.cpp
// =============================================================================

#include "Implicit.hpp"
#include "../core/Week4Bridge.hpp"
#include <stdexcept>
#include <sstream>

namespace Calculus {

// =============================================================================
// implicitDiff
//
// Given F(x, y) = 0, computes dy/dx via the implicit function theorem:
//
//         dy/dx = -(∂F/∂y) / (∂F/∂y)
//
// This works because differentiating F(x, y(x)) = 0 w.r.t. x gives:
//     ∂F/∂x + ∂F/∂y * dy/dx = 0
//     dy/dx = -(∂F/∂x) / (∂F/∂y)
// =============================================================================

ExprPtr implicitDiff(const ExprPtr& F, const std::string& indepVar, const std::string& depVar) {
	ExprPtr dFdx = simplify(diff(F, indepVar));
	ExprPtr dFdy = simplify(diff(F, depVar));

	// dy/dx = -(∂F/∂x) / (∂F/∂y)
	return simplify(neg(div_expr(dFdx, dFdy)));
}

// =============================================================================
// implicitDiff2
//
// Second-order implicit derivative d²y/dx².
//
// Method:
//   1. Compute p = dy/dx = -(∂F/∂x) / (∂F/∂y)  via implicitDiff
//   2. Differentiate p w.r.t. x, treating y as a function of x.
//      Wherever dy/dx appears in the differentiation, substitute p back in.
//
// This is done by differentiating p w.r.t. x directly (diff handles
// the chain rule through y automatically since y is just a variable),
// then substituting the expression for dy/dx back for any dy/dx terms.
//
// Note: for display purposes the result may still contain y, which is
// standard — implicit derivatives are expressed in terms of both x and y.
// =============================================================================

ExprPtr implicitDiff2(const ExprPtr& F, const std::string& indepVar, const std::string& depVar) {
	// First derivative: dy/dx as an expression in x and y
	ExprPtr dydx = implicitDiff(F, indepVar, depVar);

	// To get d²y/dx², differentiate dy/dx w.r.t. x using the chain rule.
	// Treat depVar (y) as a function of indepVar (x), so when we diff
	// through y we get a dy/dx factor, which we then substitute back.
	//
	// Concretely: d/dx[dydx(x, y)] treating y as y(x)
	// = ∂(dydx)/∂x + ∂(dydx)/∂y * dy/dx

	ExprPtr ddydx_dx = simplify(diff(dydx, indepVar));   // ∂(dy/dx)/∂x
	ExprPtr ddydx_dy = simplify(diff(dydx, depVar));     // ∂(dy/dx)/∂y

	// d²y/dx² = ∂(dy/dx)/∂x + ∂(dy/dx)/∂y * (dy/dx)
	ExprPtr result = simplify(add(ddydx_dx, mul(ddydx_dy, dydx)));

	return result;
}

// =============================================================================
// logDiff
//
// Logarithmic differentiation of y = expr.
//
// Steps:
//     ln(y) = ln(expr)
//     d/dx[ln(y)] = d/dx[ln(expr)]
//     y'/y	= d/dx[ln(expr)]
//	   y' = expr * d/dx[ln(expr)]
//
// d/dx[ln(expr)] = diff(expr, var) / expr  (by the log differentiation rule)
// So:  y' = expr * diff(expr, var) / expr  = diff(expr, var)
//
// The final result is the same as diff(expr, var), but the intermediate
// lnForm step - d/dx[ln(y)] = diff(ln(expr), var)  - shows the method.
// =============================================================================

LogDiffResult logDiff(const ExprPtr& expr, const std::string& var) {
	LogDiffResult out;
	try {
		// d/dx[ln(expr)] - this is the intermediate "logarithmic form"
		ExprPtr lnExpr = log_expr(expr);
		out.lnForm = simplify(diff(lnExpr, var));

		// y' = y * (y'/y) = expr * d/dx[ln(expr)]
		out.result = simplify(mul(expr, out.lnForm));
	} catch (const std::exception& ex) {
		out.ok    = false;
		out.error = ex.what();
	}
	return out;
}

// =============================================================================
// Formatted entry points
// =============================================================================

ImplicitResult computeImplicit(const std::string& Fstr,
								const std::string& indepVar,
								const std::string& depVar,
								bool secondOrder) {
	ImplicitResult out;
	try {
		ExprPtr F    = parse(Fstr);
		ExprPtr dydx = implicitDiff(F, indepVar, depVar);

		out.dydx       = toString(dydx);
		out.dydx_latex = toLatex(dydx);

		if (secondOrder) {
			ExprPtr d2ydx2 = implicitDiff2(F, indepVar, depVar);
			out.d2ydx2       = toString(d2ydx2);
			out.d2ydx2_latex = toLatex(d2ydx2);
		}
	} catch (const std::exception& ex) {
		out.ok    = false;
		out.error = ex.what();
	}
	return out;
}

LogDiffFormatted computeLogDiff(const std::string& exprStr,
	                            const std::string& var) {
	LogDiffFormatted out;
	try {
		ExprPtr expr = parse(exprStr);
		auto    res  = logDiff(expr, var);

		if (!res.ok) {
			out.ok    = false;
			out.error = res.error;
			return out;
		}

		out.lnDerivative       = toString(res.lnForm);
		out.lnDerivative_latex = toLatex(res.lnForm);
		out.result             = toString(res.result);
		out.result_latex       = toLatex(res.result);
	} catch (const std::exception& ex) {
		out.ok    = false;
		out.error = ex.what();
	}
	return out;
}

}