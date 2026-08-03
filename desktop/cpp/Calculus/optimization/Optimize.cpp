// calculus/optimization/Optimize.cpp

#include "Optimize.hpp"
#include "../core/Week4Bridge.hpp"
#include <cmath>
#include <sstream>
#include <algorithm>

namespace Calculus {

// ── Single-variable critical points ──────────────────────────────────────────

OptResult1D findCriticalPoints1D(const std::string& fStr, const std::string& var, double a, double b) {
	OptResult1D out;
	try {
		const ExprPtr f    = parse(fStr);
		const ExprPtr fp   = simplify(diff(f, var));
		const ExprPtr fpp  = simplify(diff(fp, var));

		const Func1D fpFunc = makeEvaluator(fp, var);

		// Scan for sign changes in f' to bracket critical points
		constexpr int N  = 1000;
		const double h = (b - a) / N;
		std::vector<double> candidates;

		double prev = fpFunc(a);
		for (int i = 1; i <= N; i++) {
			const double x   = a + i * h;
			const double cur = fpFunc(x);
			if (!std::isfinite(cur))  { prev = cur; continue; }
			if (!std::isfinite(prev)) { prev = cur; continue; }

			if (prev * cur <= 0.0) {
				// Bisect to find root
				double lo  = x - h, hi = x;
				for (int j = 0; j < 50; ++j) {
					const double mid  = 0.5 * (lo + hi);
					if (const double fmid = fpFunc(mid); fpFunc(lo) * fmid <= 0) hi = mid; else lo = mid;
					if (hi - lo < 1e-10) break;
				}
				candidates.push_back(0.5 * (lo + hi));
			}
			prev = cur;
		}

		for (double xc : candidates) {
			CriticalPoint1D cp;
			cp.x      = xc;
			cp.fVal   = evaluate(f, {{var, xc}});
			if (const double d2 = evaluate(fpp, {{var, xc}}); d2 > 1e-8)       cp.type = "local min";
			else if (d2 < -1e-8) cp.type = "local max";
			else                 cp.type = "saddle/inflection";
			out.points.push_back(cp);
		}

	} catch (const std::exception& ex) {
		out.ok    = false;
		out.error = ex.what();
	}
	return out;
}

// ── Multivariable critical points ─────────────────────────────────────────────

OptResultND findCriticalPointsND(const std::string& fStr, const std::vector<std::string>& vars,
	                             const std::vector<double>& searchMin, const std::vector<double>& searchMax) {
	OptResultND out;
	try {
		size_t n  = vars.size();
		ExprPtr f = parse(fStr);

		// Compute gradient
		std::vector<ExprPtr> grad(n);
		for (size_t i = 0; i < n; ++i)
			grad[i] = simplify(diff(f, vars[i]));

		// Compute Hessian
		ExprMat H = hessian(f, vars);

		// Grid search for approximate critical points (|∇f| = 0)
		int gridN = (n <= 2) ? 30 : 10;
		std::vector<std::vector<double>> candidates;

		std::function<void(int, std::vector<double>&)> gridSearch;
		gridSearch = [&](int dim, std::vector<double>& pt) {
			if (dim == static_cast<int>(n)) {
				// Evaluate |∇f|
				SymbolTable syms;
				for (size_t i = 0; i < n; ++i) syms[vars[i]] = pt[i];
				double gradNorm = 0.0;
				for (size_t i = 0; i < n; ++i) {
					try {
						double g = evaluate(grad[i], syms);
						gradNorm += g*g;
					} catch (...) { return; }
				}
				if (std::sqrt(gradNorm) < 0.5) candidates.push_back(pt);
				return;
			}
			double step = (searchMax[dim] - searchMin[dim]) / gridN;
			for (int k = 0; k <= gridN; ++k) {
				pt[dim] = searchMin[dim] + k * step;
				gridSearch(dim + 1, pt);
			}
		};

		std::vector<double> pt(n);
		gridSearch(0, pt);

		// Refine each candidate with gradient descent toward ∇f=0
		for (auto& c : candidates) {
			// Simple Newton step: x -= J^{-1} * ∇f  (for small n)
			// We just use the gradient magnitude to filter
			SymbolTable syms;
			for (size_t i = 0; i < n; ++i) syms[vars[i]] = c[i];

			double gradNorm = 0.0;
			std::vector<double> gv(n);
			for (size_t i = 0; i < n; ++i) {
				try { gv[i] = evaluate(grad[i], syms); }
				catch (...) { gv[i] = 0.0; }
				gradNorm += gv[i]*gv[i];
			}
			if (std::sqrt(gradNorm) > 0.1) continue;

			// Classify via Hessian
			CriticalPointND cp;
			cp.point = c;
			cp.fVal  = evaluate(f, syms);

			// For 2D: D = H[0][0]*H[1][1] - H[0][1]^2
			if (n == 2) {
				double h00 = evaluate(H[0][0], syms);
				double h01 = evaluate(H[0][1], syms);
				double h11 = evaluate(H[1][1], syms);
				double D   = h00*h11 - h01*h01;
				cp.det = D;
				if (D > 0 && h00 > 0)       cp.type = "local min";
				else if (D > 0 && h00 < 0)   cp.type = "local max";
				else if (D < 0)               cp.type = "saddle";
				else                          cp.type = "unknown (D=0)";
			} else {
				cp.det  = 0.0;
				cp.type = "critical point (classify via Hessian eigenvalues)";
			}
			out.points.push_back(cp);
		}

		// Remove duplicates (within 0.01 of each other)
		std::vector<CriticalPointND> unique;
		for (auto& cp : out.points) {
			bool dup = false;
			for (auto& u : unique) {
				double d = 0.0;
				for (size_t i = 0; i < n; ++i)
					d += std::pow(cp.point[i]-u.point[i], 2);
				if (std::sqrt(d) < 0.01) { dup = true; break; }
			}
			if (!dup) unique.push_back(cp);
		}
		out.points = unique;

	} catch (const std::exception& e) {
		out.ok = false; out.error = e.what();
	}
	return out;
}

// ── Lagrange multipliers ──────────────────────────────────────────────────────
// Solve ∇f = λ∇g, g=0  via grid search + refinement

LagrangeResult lagrangeMultipliers(
    const std::string& fStr,
    const std::string& gStr,
    const std::vector<std::string>& vars,
    const std::vector<double>& searchMin,
    const std::vector<double>& searchMax) {

    LagrangeResult out;
    try {
        size_t n = vars.size();
        ExprPtr f = parse(fStr), g = parse(gStr);

        std::vector<ExprPtr> gf(n), gg(n);
        for (size_t i = 0; i < n; ++i) {
            gf[i] = simplify(diff(f, vars[i]));
            gg[i] = simplify(diff(g, vars[i]));
        }

        int gridN = (n <= 2) ? 40 : 8;
        std::vector<std::vector<double>> candidates;

        std::function<void(int, std::vector<double>&)> gridSearch;
        gridSearch = [&](int dim, std::vector<double>& pt) {
            if (dim == static_cast<int>(n)) {
                SymbolTable syms;
                for (size_t i = 0; i < n; ++i) syms[vars[i]] = pt[i];
                try {
                    double gVal = evaluate(g, syms);
                    if (std::abs(gVal) > 0.3) return;  // not near constraint

                    // Check ∇f = λ∇g: estimate λ from first non-zero ∇g component
                    double lambda = 0.0;
                    for (size_t i = 0; i < n; ++i) {
                        double ggi = evaluate(gg[i], syms);
                        if (std::abs(ggi) > 1e-8) {
                            lambda = evaluate(gf[i], syms) / ggi;
                            break;
                        }
                    }
                    // Check all components
                    double residual = 0.0;
                    for (size_t i = 0; i < n; ++i) {
                        double r = evaluate(gf[i], syms) - lambda * evaluate(gg[i], syms);
                        residual += r*r;
                    }
                    residual += gVal*gVal;
                    if (std::sqrt(residual) < 0.5) {
                        auto c = pt;
                        c.push_back(lambda);
                        candidates.push_back(c);
                    }
                } catch (...) {}
                return;
            }
            double step = (searchMax[dim] - searchMin[dim]) / gridN;
            for (int k = 0; k <= gridN; ++k) {
                pt[dim] = searchMin[dim] + k * step;
                gridSearch(dim+1, pt);
            }
        };

        std::vector<double> pt(n);
        gridSearch(0, pt);

        // Deduplicate and store
        std::vector<std::vector<double>> unique;
        for (auto& c : candidates) {
            bool dup = false;
            for (auto& u : unique) {
                double d = 0.0;
                for (size_t i = 0; i < n; ++i) d += std::pow(c[i]-u[i],2);
                if (std::sqrt(d) < 0.05) { dup = true; break; }
            }
            if (!dup) unique.push_back(c);
        }

        for (auto& c : unique) {
            out.candidates.push_back(c);
            SymbolTable syms;
            for (size_t i = 0; i < n; ++i) syms[vars[i]] = c[i];
            out.fVals.push_back(evaluate(f, syms));
        }

        if (out.candidates.empty()) {
            out.summary = "No candidates found in search region. Try wider bounds.";
        } else {
            std::ostringstream ss;
            ss << out.candidates.size() << " candidate(s) found.";
            auto it = std::max_element(out.fVals.begin(), out.fVals.end());
            auto it2= std::min_element(out.fVals.begin(), out.fVals.end());
            ss << " Max f ≈ " << *it << ", Min f ≈ " << *it2;
            out.summary = ss.str();
        }

    } catch (const std::exception& e) {
        out.ok = false; out.error = e.what();
    }
    return out;
}

}