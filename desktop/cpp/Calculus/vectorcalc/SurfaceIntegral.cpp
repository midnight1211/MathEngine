// calculus/vectorcalc/SurfaceIntegral.cpp

#include "SurfaceIntegral.hpp"
#include "../differentiation/Derivative.hpp"
#include "../core/Week4Bridge.hpp"
#include <cmath>
#include <array>

#include "../integration/Multivariable.hpp"

namespace Calculus {

// Cross product of two 3-vectors
static std::array<double,3> cross3(const std::array<double,3>& a, const std::array<double,3>& b) {
	return { a[1]*b[2]-a[2]*b[1],
	            a[2]*b[0]-a[0]*b[2],
	            a[0]*b[1]-a[1]*b[0] };
}
static double norm3(const std::array<double,3>& a) {
	return std::sqrt(a[0]*a[0]+a[1]*a[1]+a[2]*a[2]);
}

SurfaceIntResult scalarSurfaceIntegral(const std::string& fStr, const std::vector<std::string>& surfExprs, const std::vector<std::string>& fieldVars, const std::string& u, const std::string& v, double u1, double u2, double v1, double v2) {
	SurfaceIntResult out;
	try {
		// Parse surface and compute partial derivatives
		std::vector<ExprPtr> r, dru, drv;
		for (const auto& s : surfExprs) {
			ExprPtr ri = parse(s);
			r.push_back(ri);
			dru.push_back(simplify(diff(ri, u)));
			drv.push_back(simplify(diff(ri, v)));
		}
		ExprPtr f = parse(fStr);

		Func2D integrand = [&](double uv, double vv) -> double {
			SymbolTable syms = {{u, uv}, {v, vv}};
			std::array<double,3> ru{0,0,0}, rv{0,0,0};
			SymbolTable fSyms = {{u, uv}, {v, vv}};
			for (size_t i = 0; i < r.size() && i < 3; ++i) {
				double rval = evaluate(r[i], syms);
				ru[i] = evaluate(dru[i], syms);
				rv[i] = evaluate(drv[i], syms);
				fSyms[fieldVars[i]] = rval;
			}
			auto n = cross3(ru, rv);
			return evaluate(f, fSyms) * norm3(n);
		};

		out.value  = doubleIntegral(integrand, u1, u2, v1, v2).value;
		out.method = "Scalar surface integral (iterated Romberg)";
	} catch (const std::exception& e) {
		out.ok = false; out.error = e.what();
	}
	return out;
}

SurfaceIntResult fluxIntegral(const std::vector<std::string>& Fstrs, const std::vector<std::string>& surfExprs, const std::vector<std::string>& fieldVars, const std::string& u, const std::string& v, double u1, double u2, double v1, double v2) {
	SurfaceIntResult out;
	try {
		std::vector<ExprPtr> F, r, dru, drv;
		for (size_t i = 0; i < 3; ++i) {
			F.push_back(parse(Fstrs[i]));
			ExprPtr ri = parse(surfExprs[i]);
			r.push_back(ri);
			dru.push_back(simplify(diff(ri, u)));
			drv.push_back(simplify(diff(ri, v)));
		}

		Func2D integrand = [&](double uv, double vv) -> double {
			SymbolTable syms = {{u,uv}, {v,vv}};
			std::array<double,3> ru{0,0,0}, rv{0,0,0};
			SymbolTable fSyms = {{u,uv}, {v, vv}};
			for (int i = 0; i < 3; ++i) {
				fSyms[fieldVars[i]] = evaluate(r[i], syms);
				ru[i] = evaluate(dru[i], syms);
				rv[i] = evaluate(drv[i], syms);
			}
			auto n = cross3(ru, rv);
			double dot = 0.0;
			for (int i = 0; i < 3; ++i)
				dot += evaluate(F[i], fSyms) * n[i];
			return dot;
		};

		out.value  = doubleIntegral(integrand, u1, u2, v1, v2).value;
		out.method = "Flux integral F*dS (iterated Romberg)";
	} catch (const std::exception& e) {
		out.ok = false; out.error = e.what();
	}
	return out;
}

} // Calculus