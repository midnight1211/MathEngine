// calculus/vectorcalc/Theorems.cpp

#include "Theorems.hpp"
#include "../integration/Multivariable.hpp"
#include "../core/Week4Bridge.hpp"
#include <cmath>
#include <sstream>

namespace Calculus {

static constexpr double THEOREM_TOL = 1e-4;

TheoremResult greensTheorem(
    const std::string& P, const std::string& Q,
    const std::vector<std::string>& boundary,
    double t1, double t2,
    double x1, double x2, double y1, double y2) {

    TheoremResult out;
    try {
        // LHS: ∮_C P dx + Q dy  (line integral)
        auto line = vectorLineIntegral({P, Q}, boundary, {"x","y"}, "t", t1, t2);
        if (!line.ok) throw std::runtime_error(line.error);
        out.lhs = line.value;

        // RHS: ∬_D (∂Q/∂x - ∂P/∂y) dA
        ExprPtr Pexpr = parse(P), Qexpr = parse(Q);
        ExprPtr curl2D = simplify(sub(diff(Qexpr,"x"), diff(Pexpr,"y")));
        Func2D  f      = [&](double x, double y) {
            return evaluate(curl2D, {{"x",x},{"y",y}});
        };
        out.rhs = doubleIntegral(f, x1, x2, y1, y2).value;

        out.verified  = std::abs(out.lhs - out.rhs) < THEOREM_TOL * (1 + std::abs(out.rhs));
        out.statement = "Green's: ∮_C(P dx+Q dy) = ∬_D(∂Q/∂x-∂P/∂y)dA";
    } catch (const std::exception& e) {
        out.ok = false; out.error = e.what();
    }
    return out;
}

TheoremResult stokesTheorem(
    const std::vector<std::string>& F,
    const std::vector<std::string>& curve,
    const std::vector<std::string>& surface,
    const std::vector<std::string>& fieldVars,
    double t1, double t2,
    double u1, double u2, double v1, double v2) {

    TheoremResult out;
    try {
        // LHS: ∮_C F·dr
        auto line = vectorLineIntegral(F, curve, fieldVars, "t", t1, t2);
        if (!line.ok) throw std::runtime_error(line.error);
        out.lhs = line.value;

        // RHS: ∬_S (∇×F)·dS
        auto curlRes = computeCurl(F, fieldVars);
        if (!curlRes.ok) throw std::runtime_error(curlRes.error);
        auto flux = fluxIntegral(curlRes.components, surface, fieldVars,
                                  "u","v", u1, u2, v1, v2);
        if (!flux.ok) throw std::runtime_error(flux.error);
        out.rhs = flux.value;

        out.verified  = std::abs(out.lhs - out.rhs) < THEOREM_TOL * (1 + std::abs(out.rhs));
        out.statement = "Stokes': ∮_C F·dr = ∬_S (∇×F)·dS";
    } catch (const std::exception& e) {
        out.ok = false; out.error = e.what();
    }
    return out;
}

TheoremResult divergenceTheorem(
    const std::vector<std::string>& F,
    const std::vector<std::string>& surface,
    const std::vector<std::string>& fieldVars,
    double u1, double u2, double v1, double v2,
    double x1, double x2, double y1, double y2, double z1, double z2) {

    TheoremResult out;
    try {
        // LHS: ∬_S F·dS
        auto flux = fluxIntegral(F, surface, fieldVars, "u","v", u1, u2, v1, v2);
        if (!flux.ok) throw std::runtime_error(flux.error);
        out.lhs = flux.value;

        // RHS: ∭_V (∇·F) dV
        auto divRes = computeDiv(F, fieldVars);
        if (!divRes.ok) throw std::runtime_error(divRes.error);
        ExprPtr divF = parse(divRes.scalar);
        Func3D  g    = [&](double x, double y, double z) {
            return evaluate(divF, {{"x",x},{"y",y},{"z",z}});
        };
        out.rhs = tripleIntegral(g, x1, x2, y1, y2, z1, z2).value;

        out.verified  = std::abs(out.lhs - out.rhs) < THEOREM_TOL * (1 + std::abs(out.rhs));
        out.statement = "Divergence: ∬_S F·dS = ∭_V (∇·F) dV";
    } catch (const std::exception& e) {
        out.ok = false; out.error = e.what();
    }
    return out;
}

} // namespace Calculus