// calculus/vectorcalc/LineIntegral.cpp

#include "LineIntegral.hpp"
#include "../differentiation/Derivative.hpp"
#include "../core/Week4Bridge.hpp"
#include <cmath>
#include <sstream>

namespace Calculus {

LineIntResult scalarLineIntegral(
    const std::string& fStr,
    const std::vector<std::string>& curveExprs,
    const std::vector<std::string>& fieldVars,
    const std::string& param,
    double a, double b) {

    LineIntResult out;
    try {
        size_t dim = curveExprs.size();
        if (dim != fieldVars.size())
            throw std::invalid_argument("curve and field variable counts must match");

        // Parse curve components and their derivatives
        std::vector<ExprPtr> r, dr;
        for (const auto& s : curveExprs) {
            ExprPtr ri = parse(s);
            r.push_back(ri);
            dr.push_back(simplify(diff(ri, param)));
        }

        // Parse f
        ExprPtr f = parse(fStr);

        // Integrand: f(r(t)) * |r'(t)|
        Func1D integrand = [&](double t) -> double {
            // Build substitution map: fieldVar[i] = r[i](t)
            SymbolTable syms = {{param, t}};
            std::vector<double> rVals(dim), drVals(dim);
            for (size_t i = 0; i < dim; ++i) {
                rVals[i]  = evaluate(r[i],  syms);
                drVals[i] = evaluate(dr[i], syms);
            }
            // |r'(t)|
            double speed = 0.0;
            for (double dri : drVals) speed += dri * dri;
            speed = std::sqrt(speed);

            // f(x,y,z) at r(t)
            SymbolTable fSyms = {{param, t}};
            for (size_t i = 0; i < dim; ++i)
                fSyms[fieldVars[i]] = rVals[i];
            double fVal = evaluate(f, fSyms);
            return fVal * speed;
        };

        out.value  = romberg(integrand, a, b, 8).value;
        out.method = "Scalar line integral (Romberg)";
    } catch (const std::exception& e) {
        out.ok = false; out.error = e.what();
    }
    return out;
}

LineIntResult vectorLineIntegral(
    const std::vector<std::string>& Fstrs,
    const std::vector<std::string>& curveExprs,
    const std::vector<std::string>& fieldVars,
    const std::string& param,
    double a, double b) {

    LineIntResult out;
    try {
        size_t dim = Fstrs.size();
        if (dim != curveExprs.size())
            throw std::invalid_argument("F and curve must have same dimension");

        std::vector<ExprPtr> F, r, dr;
        for (size_t i = 0; i < dim; ++i) {
            F.push_back(parse(Fstrs[i]));
            ExprPtr ri = parse(curveExprs[i]);
            r.push_back(ri);
            dr.push_back(simplify(diff(ri, param)));
        }

        // ∫ F(r(t)) · r'(t) dt
        Func1D integrand = [&](double t) -> double {
            SymbolTable syms = {{param, t}};
            std::vector<double> rVals(dim), drVals(dim);
            for (size_t i = 0; i < dim; ++i) {
                rVals[i]  = evaluate(r[i],  syms);
                drVals[i] = evaluate(dr[i], syms);
            }
            SymbolTable fSyms = {{param, t}};
            for (size_t i = 0; i < dim; ++i)
                fSyms[fieldVars[i]] = rVals[i];

            double dot = 0.0;
            for (size_t i = 0; i < dim; ++i)
                dot += evaluate(F[i], fSyms) * drVals[i];
            return dot;
        };

        out.value  = romberg(integrand, a, b, 8).value;
        out.method = "Vector line integral F·dr (Romberg)";
    } catch (const std::exception& e) {
        out.ok = false; out.error = e.what();
    }
    return out;
}

bool isConservative2D(const std::string& P, const std::string& Q,
                       const std::string& x, const std::string& y) {
    try {
        ExprPtr dPdy = simplify(diff(parse(P), y));
        ExprPtr dQdx = simplify(diff(parse(Q), x));
        // Compare numerically at a few points
        for (double xi : {0.5, 1.0, 1.5}) {
            for (double yi : {0.5, 1.0, 1.5}) {
                double a = evaluate(dPdy, {{x, xi}, {y, yi}});
                double b = evaluate(dQdx, {{x, xi}, {y, yi}});
                if (std::abs(a - b) > 1e-6) return false;
            }
        }
        return true;
    } catch (...) { return false; }
}

} // namespace Calculus