// calculus/vectorcalc/VectorOps.cpp

#include "VectorOps.hpp"
#include "../core/Week4Bridge.hpp"

namespace Calculus {

static VecResult makeError(const std::string& msg) {
	VecResult r; r.ok = false; r.error = msg; return r;
}

VecResult computeGrad(const std::string& fStr, const std::vector<std::string>& vars) {
	VecResult out;
	try {
		ExprPtr f = parse(fStr);
		for (const auto& v : vars) {
			ExprPtr d = simplify(diff(f, v));
			out.components.push_back(toString(d));
			out.latex.push_back(toLatex(d));
		}
	} catch (const std::exception& e) { return makeError(e.what()); }
	return out;
}

VecResult computeDiv(const std::vector<std::string>& Fstrs, const std::vector<std::string>& vars) {
	VecResult out;
	if (Fstrs.size() != vars.size())
		return makeError("Number of components must match number of variables");
	try {
		ExprPtr sum = num(0.0);
		for (size_t i = 0; i < vars.size(); ++i) {
			ExprPtr fi = parse(Fstrs[i]);
			sum = add(sum, diff(fi, vars[i]));
		}
		ExprPtr result   = simplify(sum);
		out.scalar       = toString(result);
		out.scalar_latex = toLatex(result);
	} catch (const std::exception& e) { return makeError(e.what()); }
	return out;
}

VecResult computeCurl(const std::vector<std::string>& Fstrs,
					   const std::vector<std::string>& vars) {
	if (Fstrs.size() != 3 || vars.size() != 3)
		return makeError("Curl requires a 3D vector field [P,Q,R] and vars [x,y,z]");
	VecResult out;
	try {
		ExprPtr P = parse(Fstrs[0]), Q = parse(Fstrs[1]), R = parse(Fstrs[2]);
		const auto& x = vars[0]; const auto& y = vars[1]; const auto& z = vars[2];
		// [∂R/∂y - ∂Q/∂z,  ∂P/∂z - ∂R/∂x,  ∂Q/∂x - ∂P/∂y]
		std::vector<ExprPtr> components = {
			simplify(sub(diff(R,y), diff(Q,z))),
			simplify(sub(diff(P,z), diff(R,x))),
			simplify(sub(diff(Q,x), diff(P,y)))
		};
		for (const auto& c : components) {
			out.components.push_back(toString(c));
			out.latex.push_back(toLatex(c));
		}
	} catch (const std::exception& e) { return makeError(e.what()); }
	return out;
}

VecResult computeLaplacianVec(const std::string& fStr,
							   const std::vector<std::string>& vars) {
	VecResult out;
	try {
		ExprPtr f      = parse(fStr);
		ExprPtr result = laplacian(f, vars);
		out.scalar       = toString(result);
		out.scalar_latex = toLatex(result);
	} catch (const std::exception& e) { return makeError(e.what()); }
	return out;
}

VecResult computeVectorLaplacian(const std::vector<std::string>& Fstrs,
								  const std::vector<std::string>& vars) {
	VecResult out;
	try {
		for (const auto& fStr : Fstrs) {
			ExprPtr f      = parse(fStr);
			ExprPtr result = laplacian(f, vars);
			out.components.push_back(toString(result));
			out.latex.push_back(toLatex(result));
		}
	} catch (const std::exception& e) { return makeError(e.what()); }
	return out;
}

}