// =============================================================================
// calculus/differentiation/Partial.cpp
// =============================================================================

#include "Partial.hpp"
#include "../core/Week4Bridge.hpp"
#include <stdexcept>
#include <sstream>
#include <cmath>
#include <iomanip>

namespace Calculus {

// =============================================================================
// Partial derivatives
// =============================================================================

ExprPtr partial(const ExprPtr& f, const std::string& var) {
    // ∂f/∂var is exactly diff(f, var) — all other variables are treated as
    // constants by diff() already, since they don't match var.
    return diff(f, var);
}

ExprPtr partialN(const ExprPtr& f, const std::string& var, int n) {
    return diffN(f, var, n);
}

ExprPtr mixed(const ExprPtr& f,
              const std::string& var1,
              const std::string& var2) {
    // ∂²f/∂var1∂var2 = ∂/∂var2 (∂f/∂var1)
    // Differentiate with respect to var1 first, then var2.
    return simplify(diff(diff(f, var1), var2));
}

// =============================================================================
// Gradient
// =============================================================================

ExprVec gradient(const ExprPtr& f, const std::vector<std::string>& vars) {
    if (vars.empty())
        throw std::invalid_argument("gradient: variable list cannot be empty");

    ExprVec grad;
    grad.reserve(vars.size());
    for (const auto& var : vars)
        grad.push_back(simplify(diff(f, var)));
    return grad;
}

// =============================================================================
// Directional derivative   D_u f = ∇f · û
// =============================================================================

ExprPtr directional(const ExprPtr& f,
                    const std::vector<std::string>& vars,
                    const std::vector<double>& u) {
    if (vars.size() != u.size())
        throw std::invalid_argument(
            "directional: vars and direction vector must have the same length");

    // Normalise u → û
    double mag = 0.0;
    for (double ui : u) mag += ui * ui;
    mag = std::sqrt(mag);
    if (mag < 1e-14)
        throw std::invalid_argument("directional: direction vector is the zero vector");

    // D_u f = Σ (uᵢ/|u|) * ∂f/∂xᵢ
    ExprVec grad = gradient(f, vars);
    ExprPtr result = num(0.0);
    for (size_t i = 0; i < vars.size(); ++i) {
        double uhat = u[i] / mag;
        result = add(result, mul(num(uhat), grad[i]));
    }
    return simplify(result);
}

// =============================================================================
// Jacobian
// =============================================================================

ExprMat jacobian(const ExprVec& F, const std::vector<std::string>& vars) {
    if (F.empty())
        throw std::invalid_argument("jacobian: function vector cannot be empty");
    if (vars.empty())
        throw std::invalid_argument("jacobian: variable list cannot be empty");

    // J is m×n where m = |F|, n = |vars|
    ExprMat J(F.size(), std::vector<ExprPtr>(vars.size()));
    for (size_t i = 0; i < F.size(); ++i)
        for (size_t j = 0; j < vars.size(); ++j)
            J[i][j] = simplify(diff(F[i], vars[j]));
    return J;
}

// =============================================================================
// Hessian
// =============================================================================

ExprMat hessian(const ExprPtr& f, const std::vector<std::string>& vars) {
    if (vars.empty())
        throw std::invalid_argument("hessian: variable list cannot be empty");

    size_t n = vars.size();
    ExprMat H(n, std::vector<ExprPtr>(n));

    // Compute first-order partials once, then differentiate each again
    ExprVec firstOrder(n);
    for (size_t i = 0; i < n; ++i)
        firstOrder[i] = diff(f, vars[i]);

    // H[i][j] = ∂(∂f/∂xᵢ)/∂xⱼ
    for (size_t i = 0; i < n; ++i)
        for (size_t j = 0; j < n; ++j)
            H[i][j] = simplify(diff(firstOrder[i], vars[j]));

    return H;
}

// =============================================================================
// Laplacian   ∇²f = Σ ∂²f/∂xᵢ²
// =============================================================================

ExprPtr laplacian(const ExprPtr& f, const std::vector<std::string>& vars) {
    if (vars.empty())
        throw std::invalid_argument("laplacian: variable list cannot be empty");

    ExprPtr result = num(0.0);
    for (const auto& var : vars)
        result = add(result, diffN(f, var, 2));
    return simplify(result);
}

// =============================================================================
// Formatted entry points
// =============================================================================

// Helper: format an ExprPtr result into a PartialResult
static PartialResult formatPartialResult(const ExprPtr& result) {
    PartialResult out;
    out.symbolic = toString(result);
    out.latex    = toLatex(result);
    if (isConstantExpr(result)) {
        try {
            double val = evaluate(result);
            std::ostringstream ss;
            ss << std::setprecision(10) << val;
            out.numerical = ss.str();
        } catch (...) {}
    }
    return out;
}

// Helper: format an ExprMat into a MatrixResult
static MatrixResult formatMatrixResult(const ExprMat& mat) {
    MatrixResult out;
    out.rows.resize(mat.size());
    out.latex.resize(mat.size());
    for (size_t i = 0; i < mat.size(); ++i) {
        out.rows[i].resize(mat[i].size());
        out.latex[i].resize(mat[i].size());
        for (size_t j = 0; j < mat[i].size(); ++j) {
            out.rows[i][j]  = toString(mat[i][j]);
            out.latex[i][j] = toLatex(mat[i][j]);
        }
    }
    return out;
}

// ── computePartial ────────────────────────────────────────────────────────────

PartialResult computePartial(const std::string& exprStr,
                              const std::string& var,
                              int order) {
    try {
        ExprPtr f      = parse(exprStr);
        ExprPtr result = simplify(partialN(f, var, order));
        return formatPartialResult(result);
    } catch (const std::exception& ex) {
        PartialResult out;
        out.ok    = false;
        out.error = ex.what();
        return out;
    }
}

// ── computeMixed ─────────────────────────────────────────────────────────────

PartialResult computeMixed(const std::string& exprStr,
                            const std::string& var1,
                            const std::string& var2) {
    try {
        ExprPtr f      = parse(exprStr);
        ExprPtr result = mixed(f, var1, var2);
        return formatPartialResult(result);
    } catch (const std::exception& ex) {
        PartialResult out;
        out.ok    = false;
        out.error = ex.what();
        return out;
    }
}

// ── computeGradient ───────────────────────────────────────────────────────────

GradientResult computeGradient(const std::string& exprStr,
                                const std::vector<std::string>& vars) {
    GradientResult out;
    try {
        ExprPtr f    = parse(exprStr);
        ExprVec grad = gradient(f, vars);
        out.components.reserve(grad.size());
        out.latex.reserve(grad.size());
        for (const auto& g : grad) {
            out.components.push_back(toString(g));
            out.latex.push_back(toLatex(g));
        }
    } catch (const std::exception& ex) {
        out.ok    = false;
        out.error = ex.what();
    }
    return out;
}

// ── computeJacobian ──────────────────────────────────────────────────────────

MatrixResult computeJacobian(const std::vector<std::string>& exprStrs,
                              const std::vector<std::string>& vars) {
    try {
        ExprVec F;
        F.reserve(exprStrs.size());
        for (const auto& s : exprStrs)
            F.push_back(parse(s));
        return formatMatrixResult(jacobian(F, vars));
    } catch (const std::exception& ex) {
        MatrixResult out;
        out.ok    = false;
        out.error = ex.what();
        return out;
    }
}

// ── computeHessian ────────────────────────────────────────────────────────────

MatrixResult computeHessian(const std::string& exprStr,
                             const std::vector<std::string>& vars) {
    try {
        ExprPtr f = parse(exprStr);
        return formatMatrixResult(hessian(f, vars));
    } catch (const std::exception& ex) {
        MatrixResult out;
        out.ok    = false;
        out.error = ex.what();
        return out;
    }
}

// ── computeLaplacian ──────────────────────────────────────────────────────────

PartialResult computeLaplacian(const std::string& exprStr,
                                const std::vector<std::string>& vars) {
    try {
        ExprPtr f      = parse(exprStr);
        ExprPtr result = laplacian(f, vars);
        return formatPartialResult(result);
    } catch (const std::exception& ex) {
        PartialResult out;
        out.ok    = false;
        out.error = ex.what();
        return out;
    }
}

} // namespace Calculus