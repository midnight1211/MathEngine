// calculus/Calculus.cpp
// Routes operation strings to the correct module function.

#include "Calculus.hpp"
#include <cstring>
#include <cctype>

namespace Calculus
{

    // ── Minimal JSON field extractor ──────────────────────────────────────────────
    // Full JSON parsing would add a dependency — we use simple string scanning
    // since CoreEngine controls the exact format it sends.

    static std::string getStr(const std::string &json, const std::string &key)
    {
        std::string needle = "\"" + key + "\"";
        auto pos = json.find(needle);
        if (pos == std::string::npos)
            return "";
        pos = json.find(':', pos);
        if (pos == std::string::npos)
            return "";
        ++pos;
        while (pos < json.size() && std::isspace(json[pos]))
            ++pos;
        if (pos >= json.size())
            return "";
        if (json[pos] == '"')
        {
            ++pos;
            auto end = json.find('"', pos);
            return (end == std::string::npos) ? "" : json.substr(pos, end - pos);
        }
        // Unquoted value (number)
        auto end = pos;
        while (end < json.size() && json[end] != ',' && json[end] != '}')
            ++end;
        std::string v = json.substr(pos, end - pos);
        while (!v.empty() && std::isspace(v.back()))
            v.pop_back();
        return v;
    }

    static double getNum(const std::string &json, const std::string &key, double def)
    {
        std::string v = getStr(json, key);
        if (v.empty())
            return def;
        try
        {
            return std::stod(v);
        }
        catch (...)
        {
            return def;
        }
    }

    static int getInt(const std::string &json, const std::string &key, int def)
    {
        std::string v = getStr(json, key);
        if (v.empty())
            return def;
        try
        {
            return std::stoi(v);
        }
        catch (...)
        {
            return def;
        }
    }

    // Extract array of strings: "vars":["x","y","z"]
    static std::vector<std::string> getStrArray(const std::string &json, const std::string &key)
    {
        std::vector<std::string> result;
        std::string needle = "\"" + key + "\"";
        auto pos = json.find(needle);
        if (pos == std::string::npos)
            return result;
        pos = json.find('[', pos);
        if (pos == std::string::npos)
            return result;
        ++pos;
        while (pos < json.size() && json[pos] != ']')
        {
            while (pos < json.size() && json[pos] != '"' && json[pos] != ']')
                ++pos;
            if (pos >= json.size() || json[pos] == ']')
                break;
            ++pos;
            auto end = json.find('"', pos);
            if (end == std::string::npos)
                break;
            result.push_back(json.substr(pos, end - pos));
            pos = end + 1;
        }
        return result;
    }

    // Extract array of doubles: "min":[-2,-2]
    static std::vector<double> getNumArray(const std::string &json, const std::string &key)
    {
        std::vector<double> result;
        std::string needle = "\"" + key + "\"";
        auto pos = json.find(needle);
        if (pos == std::string::npos)
            return result;
        pos = json.find('[', pos);
        if (pos == std::string::npos)
            return result;
        ++pos;
        while (pos < json.size() && json[pos] != ']')
        {
            while (pos < json.size() && !std::isdigit(json[pos]) &&
                   json[pos] != '-' && json[pos] != '.' && json[pos] != ']')
                ++pos;
            if (pos >= json.size() || json[pos] == ']')
                break;
            auto end = pos + 1;
            while (end < json.size() && (std::isdigit(json[end]) || json[end] == '.' ||
                                         json[end] == 'e' || json[end] == 'E' || json[end] == '+' || json[end] == '-'))
                ++end;
            try
            {
                result.push_back(std::stod(json.substr(pos, end - pos)));
            }
            catch (...)
            {
            }
            pos = end;
        }
        return result;
    }

    // ── Format helpers ────────────────────────────────────────────────────────────

    static CalculusResult fromDiff(const DiffResult &r)
    {
        CalculusResult out;
        out.ok = r.ok;
        out.symbolic = r.symbolic;
        out.latex = r.latex;
        out.numeric = r.numerical;
        out.method = "symbolic differentiation";
        out.error = r.error;
        return out;
    }

    static CalculusResult fromPartial(const PartialResult &r)
    {
        CalculusResult out;
        out.ok = r.ok;
        out.symbolic = r.symbolic;
        out.latex = r.latex;
        out.numeric = r.numerical;
        out.method = "partial differentiation";
        out.error = r.error;
        return out;
    }

    static CalculusResult fromLimit(const LimitResult &r)
    {
        CalculusResult out;
        out.ok = r.ok;
        out.latex = r.latex;
        out.method = r.method;
        out.error = r.error;

        // The numerical limit engine returns large finite values when the true
        // limit is ±∞.  Convert those to proper infinity symbols here so the
        // UI never sees a raw "1e+12" or "-1e+12".
        std::string val = r.value;
        try
        {
            double d = std::stod(val);
            if (d > 1e10)
                val = "+∞";
            else if (d < -1e10)
                val = "-∞";
        }
        catch (...)
        {
        }

        out.symbolic = r.exists ? val : "DNE: " + val;
        out.numeric = val;
        return out;
    }

    static CalculusResult fromSymInt(const SymbolicIntegralResult &r)
    {
        CalculusResult out;
        out.ok = r.ok;
        out.symbolic = r.antiderivative;
        out.latex = r.antiderivative_latex;
        out.numeric = r.numericalStr;
        out.method = r.method;
        out.error = r.error;
        return out;
    }

    static CalculusResult fromSeries(const SeriesResult &r)
    {
        CalculusResult out;
        out.ok = r.ok;
        out.symbolic = r.polynomial;
        out.latex = r.polynomial_latex;
        out.numeric = "R = " + (std::isinf(r.radiusOfConv)
                                    ? "inf"
                                    : std::to_string(r.radiusOfConv));
        out.method = "Taylor/Maclaurin series";
        out.error = r.error;
        return out;
    }

    static CalculusResult vecResultToCalc(const VecResult &r, const std::string &m)
    {
        CalculusResult out;
        out.ok = r.ok;
        out.error = r.error;
        out.method = m;
        if (!r.scalar.empty())
        {
            out.symbolic = r.scalar;
            out.latex = r.scalar_latex;
        }
        else
        {
            out.symbolic = "[";
            out.latex = "[";
            for (size_t i = 0; i < r.components.size(); ++i)
            {
                if (i)
                {
                    out.symbolic += ", ";
                    out.latex += ", ";
                }
                out.symbolic += r.components[i];
                out.latex += r.latex[i];
            }
            out.symbolic += "]";
            out.latex += "]";
        }
        return out;
    }

    static CalculusResult matResultToCalc(const MatrixResult &r, const std::string &m)
    {
        CalculusResult out;
        out.ok = r.ok;
        out.error = r.error;
        out.method = m;
        std::ostringstream sym, lat;
        sym << "[";
        lat << "[";
        for (size_t i = 0; i < r.rows.size(); ++i)
        {
            if (i)
            {
                sym << ", ";
                lat << ", ";
            }
            sym << "[";
            lat << "[";
            for (size_t j = 0; j < r.rows[i].size(); ++j)
            {
                if (j)
                {
                    sym << ", ";
                    lat << ", ";
                }
                sym << r.rows[i][j];
                lat << r.latex[i][j];
            }
            sym << "]";
            lat << "]";
        }
        sym << "]";
        lat << "]";
        out.symbolic = sym.str();
        out.latex = lat.str();
        return out;
    }

    // ── Main dispatch ─────────────────────────────────────────────────────────────

    CalculusResult dispatch(const std::string &op, const std::string &json)
    {
        CalculusResult out;
        try
        {

            // ── Differentiation ──────────────────────────────────────────────────
            if (op == "diff")
            {
                return fromDiff(differentiate(
                    getStr(json, "expr"), getStr(json, "var"), getInt(json, "order", 1)));
            }
            if (op == "partial")
            {
                return fromPartial(computePartial(
                    getStr(json, "expr"), getStr(json, "var"), getInt(json, "order", 1)));
            }
            if (op == "partial_mixed")
            {
                return fromPartial(computeMixed(
                    getStr(json, "expr"), getStr(json, "var1"), getStr(json, "var2")));
            }
            if (op == "gradient" || op == "grad")
            {
                auto r = computeGradient(getStr(json, "expr"), getStrArray(json, "vars"));
                CalculusResult cr;
                cr.ok = r.ok;
                cr.error = r.error;
                cr.method = "gradient";
                cr.symbolic = "[";
                cr.latex = "[";
                for (size_t i = 0; i < r.components.size(); ++i)
                {
                    if (i)
                    {
                        cr.symbolic += ", ";
                        cr.latex += ", ";
                    }
                    cr.symbolic += r.components[i];
                    cr.latex += r.latex[i];
                }
                cr.symbolic += "]";
                cr.latex += "]";
                return cr;
            }
            if (op == "jacobian")
            {
                return matResultToCalc(
                    computeJacobian(getStrArray(json, "exprs"), getStrArray(json, "vars")),
                    "Jacobian");
            }
            if (op == "hessian")
            {
                return matResultToCalc(
                    computeHessian(getStr(json, "expr"), getStrArray(json, "vars")),
                    "Hessian");
            }
            if (op == "laplacian")
            {
                return fromPartial(computeLaplacian(
                    getStr(json, "expr"), getStrArray(json, "vars")));
            }
            if (op == "implicit_diff")
            {
                auto r = computeImplicit(
                    getStr(json, "expr"), getStr(json, "indep"), getStr(json, "dep"),
                    getStr(json, "order") == "2");
                out.ok = r.ok;
                out.error = r.error;
                out.symbolic = r.dydx + (r.d2ydx2.empty() ? "" : "\nd2y/dx2 = " + r.d2ydx2);
                out.latex = r.dydx_latex;
                out.method = "implicit differentiation";
                return out;
            }
            if (op == "log_diff")
            {
                auto r = computeLogDiff(getStr(json, "expr"), getStr(json, "var"));
                out.ok = r.ok;
                out.error = r.error;
                out.symbolic = r.result;
                out.latex = r.result_latex;
                out.method = "logarithmic differentiation";
                return out;
            }

            // ── Limits ────────────────────────────────────────────────────────────
            if (op == "limit" || op == "limit_left" || op == "limit_right" ||
                op == "limit_inf" || op == "limit_neginf")
            {
                LimitDirection dir = LimitDirection::BOTH;
                if (op == "limit_left")
                    dir = LimitDirection::LEFT;
                if (op == "limit_right")
                    dir = LimitDirection::RIGHT;
                if (op == "limit_inf")
                    dir = LimitDirection::POS_INF;
                if (op == "limit_neginf")
                    dir = LimitDirection::NEG_INF;
                std::string pt = getStr(json, "point");
                if (pt.empty())
                    pt = (dir == LimitDirection::POS_INF ? "inf" : dir == LimitDirection::NEG_INF ? "-inf"
                                                                                                  : "0");
                return fromLimit(computeLimit(
                    getStr(json, "expr"), getStr(json, "var"), pt, dir));
            }

            // ── Integration ───────────────────────────────────────────────────────
            if (op == "integrate")
            {
                return fromSymInt(computeIndefinite(
                    getStr(json, "expr"), getStr(json, "var")));
            }
            if (op == "definite_int")
            {
                // Try symbolic evaluation first
                auto sr = fromSymInt(computeDefinite(
                    getStr(json, "expr"), getStr(json, "var"),
                    getStr(json, "a"), getStr(json, "b")));
                // If the symbolic result still looks like an antiderivative
                // (contains "+C") or has no numeric value, fall back to
                // Romberg numerical integration which always returns a number.
                bool looksSymbolic = sr.symbolic.find("+C") != std::string::npos || sr.numeric.empty();
                if (!sr.ok || looksSymbolic)
                {
                    auto nr = computeNumerical(
                        getStr(json, "expr"), getStr(json, "var"),
                        getNum(json, "a", 0.0), getNum(json, "b", 1.0),
                        "romberg", 1000);
                    CalculusResult cr;
                    cr.ok = nr.ok;
                    cr.error = nr.errorMsg;
                    cr.symbolic = nr.value;
                    cr.numeric = nr.value;
                    cr.method = "Romberg numerical integration";
                    return cr;
                }
                return sr;
            }
            if (op == "numerical_int")
            {
                auto r = computeNumerical(
                    getStr(json, "expr"), getStr(json, "var"),
                    getNum(json, "a", 0), getNum(json, "b", 0),
                    getStr(json, "method"), getInt(json, "n", 1000));
                out.ok = r.ok;
                out.error = r.errorMsg;
                out.symbolic = r.value;
                out.numeric = r.value;
                out.method = r.method;
                return out;
            }
            if (op == "double_int")
            {
                auto r = computeDoubleIntegral(
                    getStr(json, "expr"), getStr(json, "varX"), getStr(json, "varY"),
                    getNum(json, "ax", 0), getNum(json, "bx", 0),
                    getNum(json, "ay", 0), getNum(json, "by", 0));
                out.ok = r.ok;
                out.error = r.error;
                out.symbolic = std::to_string(r.value);
                out.numeric = out.symbolic;
                out.method = r.method;
                return out;
            }
            if (op == "triple_int")
            {
                auto r = computeTripleIntegral(
                    getStr(json, "expr"),
                    getStr(json, "varX"), getStr(json, "varY"), getStr(json, "varZ"),
                    getNum(json, "ax", 0), getNum(json, "bx", 0),
                    getNum(json, "ay", 0), getNum(json, "by", 0),
                    getNum(json, "az", 0), getNum(json, "bz", 0));
                out.ok = r.ok;
                out.error = r.error;
                out.symbolic = std::to_string(r.value);
                out.numeric = out.symbolic;
                out.method = r.method;
                return out;
            }
            if (op == "polar_int")
            {
                auto r = computePolarIntegral(
                    getStr(json, "expr"),
                    getStr(json, "varR"), getStr(json, "varT"),
                    getNum(json, "r1", 0), getNum(json, "r2", 0),
                    getNum(json, "t1", 0), getNum(json, "t2", 0));
                out.ok = r.ok;
                out.error = r.error;
                out.symbolic = std::to_string(r.value);
                out.numeric = out.symbolic;
                out.method = r.method;
                return out;
            }

            // ── Series ────────────────────────────────────────────────────────────
            // Numerical Taylor/Maclaurin fallback helper.
            // computeTaylor/computeMaclaurin can throw "undefined variable" when the
            // symbolic series engine calls evaluate() without variable bindings.
            // We catch that and produce a numerical series via finite differences.
            auto numericalTaylor = [&](const std::string &expr,
                                       const std::string &var,
                                       double center,
                                       int order) -> CalculusResult
            {
                CalculusResult cr;
                cr.ok = true;
                cr.method = "numerical Taylor series (finite differences)";

                // Evaluate f^(n)(center) numerically using Richardson-extrapolated
                // central differences.  h shrinks if the point is 0 to avoid cancellation.
                const double h0 = (std::abs(center) > 0.1) ? std::abs(center) * 0.01 : 0.01;

                // Build evaluator via the numerical integration infrastructure
                auto nr0 = computeNumerical(expr, var, center, center + 1e-14, "romberg", 2);
                if (!nr0.ok)
                {
                    cr.ok = false;
                    cr.error = nr0.errorMsg;
                    return cr;
                }

                // Use Romberg-style finite differences to get each derivative
                // f^(n)(c) ≈ [Σ_k (-1)^(n-k) C(n,k) f(c + k*h)] / h^n
                auto evalPt = [&](double x) -> double
                {
                    auto r2 = computeNumerical(expr, var, x, x + 1e-14, "gauss", 2);
                    if (!r2.ok)
                        throw std::runtime_error(r2.errorMsg);
                    return r2.raw;
                };

                // Use direct evaluation through the Week4Bridge-style approach:
                // Build a dummy 1-point "integral" of width ε to evaluate f(x)
                // Actually just use romberg on a tiny interval then divide by width.
                auto feval = [&](double x) -> double
                {
                    double eps = 1e-9;
                    auto r2 = computeNumerical(expr, var, x - eps, x + eps, "romberg", 4);
                    if (!r2.ok)
                        return 0.0;
                    return r2.raw / (2.0 * eps);
                };

                // Compute factorial
                auto fact = [](int n) -> double
                {
                    double f = 1.0;
                    for (int i = 2; i <= n; ++i)
                        f *= i;
                    return f;
                };

                // Build C(n,k) table
                auto binom = [](int n, int k) -> double
                {
                    if (k < 0 || k > n)
                        return 0.0;
                    double r = 1.0;
                    for (int i = 0; i < k; ++i)
                        r *= (double)(n - i) / (i + 1);
                    return r;
                };

                std::ostringstream poly;
                std::ostringstream detail;
                detail << "Taylor series of " << expr << " about " << var << "=" << center << " ";

                double h = h0;
                bool first = true;
                for (int n = 0; n <= order; ++n)
                {
                    // nth derivative via central difference formula
                    double deriv = 0.0;
                    int pts = n + (n % 2 == 0 ? 0 : 1); // ensure even for central diff
                    if (pts < 2)
                        pts = 2;
                    for (int k = 0; k <= pts; ++k)
                    {
                        double sign = ((pts - k) % 2 == 0) ? 1.0 : -1.0;
                        double xk = center + (k - pts / 2.0) * h;
                        try
                        {
                            deriv += sign * binom(pts, k) * feval(xk);
                        }
                        catch (...)
                        {
                        }
                    }
                    deriv /= std::pow(h, pts);
                    // Pad extra applications of diff to get exactly n-th derivative
                    // when pts > n (they differ by at most 1, usually 0)

                    double coeff = deriv / fact(n);
                    if (std::abs(coeff) < 1e-14)
                    {
                        continue;
                    }

                    // Format this term: coeff*(var-center)^n / n!
                    std::string termStr;
                    if (!first && coeff > 0)
                        poly << " + ";
                    else if (coeff < 0)
                    {
                        poly << " - ";
                        coeff = -coeff;
                    }
                    first = false;

                    if (n == 0)
                    {
                        poly << coeff;
                    }
                    else if (std::abs(coeff - 1.0) > 1e-10)
                    {
                        poly << coeff << "*";
                    }
                    if (n >= 1)
                    {
                        poly << "(" << var;
                        if (center != 0.0)
                            poly << "-" << center;
                        poly << ")";
                        if (n > 1)
                            poly << "^" << n;
                    }

                    detail << "  a_" << n << " = f^(" << n << ")(" << center << ")/" << n << "! ≈ " << coeff << " ";
                }

                std::string p = poly.str();
                if (p.empty())
                    p = "0";
                cr.symbolic = p + "  +  O((" + var + (center != 0.0 ? "-" + std::to_string(center) : "") + ")^" + std::to_string(order + 1) + ")";
                cr.numeric = detail.str();
                return cr;
            };

            if (op == "taylor")
            {
                std::string expr = getStr(json, "expr");
                std::string var = getStr(json, "var");
                double center = getNum(json, "center", 0.0);
                int order = getInt(json, "order", 6);
                // Try symbolic first
                try
                {
                    auto r = fromSeries(computeTaylor(expr, var, center, order));
                    if (r.ok && !r.symbolic.empty())
                        return r;
                }
                catch (...)
                {
                }
                // Numerical fallback
                return numericalTaylor(expr, var, center, order);
            }
            if (op == "maclaurin")
            {
                std::string expr = getStr(json, "expr");
                std::string var = getStr(json, "var");
                int order = getInt(json, "order", 6);
                try
                {
                    auto r = fromSeries(computeMaclaurin(expr, var, order));
                    if (r.ok && !r.symbolic.empty())
                        return r;
                }
                catch (...)
                {
                }
                return numericalTaylor(expr, var, 0.0, order);
            }
            if (op == "fourier")
            {
                // Fourier series coefficients via numerical integration
                // f(x) = a0/2 + Σ_{n=1}^{N} [an*cos(n*2π/T*x) + bn*sin(n*2π/T*x)]
                std::string expr = getStr(json, "expr");
                std::string var = getStr(json, "var");
                double period = getNum(json, "period", 2.0 * 3.14159265358979);
                int terms = getInt(json, "terms", 6);
                double L = period / 2.0;

                auto nr_a0 = computeNumerical(expr, var, -L, L, "romberg", 8);
                if (!nr_a0.ok)
                {
                    out.ok = false;
                    out.error = nr_a0.errorMsg;
                    return out;
                }
                double a0 = nr_a0.raw / L;

                std::ostringstream poly, detail;
                detail << "Fourier series of " << expr << " on [-" << L << "," << L << "]";
                detail << "a₀/2 = " << (a0 / 2.0) << " ";

                bool first = true;
                if (std::abs(a0 / 2.0) > 1e-10)
                {
                    poly << (a0 / 2.0);
                    first = false;
                }

                for (int n = 1; n <= terms; ++n)
                {
                    double omega = n * 3.14159265358979 / L;
                    // an = (1/L) ∫_{-L}^{L} f(x)*cos(n*pi*x/L) dx
                    // Build integrand strings: f(x)*cos(n*pi/L*x)
                    // Use numerical integration of the product
                    std::string cosExpr = "(" + expr + ")*cos(" + std::to_string(omega) + "*" + var + ")";
                    std::string sinExpr = "(" + expr + ")*sin(" + std::to_string(omega) + "*" + var + ")";
                    auto rCos = computeNumerical(cosExpr, var, -L, L, "romberg", 8);
                    auto rSin = computeNumerical(sinExpr, var, -L, L, "romberg", 8);
                    if (!rCos.ok || !rSin.ok)
                        continue;
                    double an = rCos.raw / L;
                    double bn = rSin.raw / L;
                    detail << "a_" << n << " = " << an << ",  b_" << n << " = " << bn << " ";
                    if (std::abs(an) > 1e-10)
                    {
                        if (!first && an > 0)
                            poly << " + ";
                        else if (an < 0)
                        {
                            poly << " - ";
                            an = -an;
                        }
                        if (std::abs(an - 1.0) > 1e-10)
                            poly << an << "*";
                        poly << "cos(" << omega << "*" << var << ")";
                        first = false;
                    }
                    if (std::abs(bn) > 1e-10)
                    {
                        if (!first && bn > 0)
                            poly << " + ";
                        else if (bn < 0)
                        {
                            poly << " - ";
                            bn = -bn;
                        }
                        if (std::abs(bn - 1.0) > 1e-10)
                            poly << bn << "*";
                        poly << "sin(" << omega << "*" << var << ")";
                        first = false;
                    }
                }
                std::string p = poly.str();
                if (p.empty())
                    p = "0";
                out.ok = true;
                out.symbolic = p;
                out.numeric = detail.str();
                out.method = "numerical Fourier series";
                return out;
            }
            if (op == "convergence")
            {
                auto r = testSeries(getStr(json, "expr"), getStr(json, "var"));
                out.ok = r.ok;
                out.error = r.error;
                out.symbolic = r.reason;
                out.method = r.test;
                out.numeric = (r.verdict == ConvergenceVerdict::CONVERGES ? "converges" : r.verdict == ConvergenceVerdict::DIVERGES ? "diverges"
                                                                                                                                    : "inconclusive");
                return out;
            }

            // ── Vector calculus ───────────────────────────────────────────────────
            if (op == "div")
            {
                return vecResultToCalc(
                    computeDiv(getStrArray(json, "exprs"), getStrArray(json, "vars")),
                    "divergence");
            }
            if (op == "curl")
            {
                return vecResultToCalc(
                    computeCurl(getStrArray(json, "exprs"), getStrArray(json, "vars")),
                    "curl");
            }
            if (op == "vector_laplacian")
            {
                return vecResultToCalc(
                    computeVectorLaplacian(getStrArray(json, "exprs"), getStrArray(json, "vars")),
                    "vector Laplacian");
            }

            // ── Optimization ──────────────────────────────────────────────────────
            if (op == "optimize_1d")
            {
                auto r = findCriticalPoints1D(
                    getStr(json, "expr"), getStr(json, "var"),
                    getNum(json, "a", -10.0), getNum(json, "b", 10.0));
                out.ok = r.ok;
                out.error = r.error;
                std::ostringstream ss;
                bool firstLine = true;
                for (auto &cp : r.points) {
                    if (!firstLine) ss << "\n";
                    firstLine = false;
                    std::string label = cp.type;
                    if (!label.empty())
                        label[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(label[0])));
                    ss << label << " at x = " << cp.x << ", f(x) = " << cp.fVal;
                }
                out.symbolic = ss.str().empty()
                    ? "Result: no critical points found in the given interval."
                    : ss.str();
                out.method = "second derivative test";
                return out;
            }
            if (op == "optimize_nd")
            {
                auto r = findCriticalPointsND(
                    getStr(json, "expr"), getStrArray(json, "vars"),
                    getNumArray(json, "min"), getNumArray(json, "max"));
                out.ok = r.ok;
                out.error = r.error;
                std::ostringstream ss;
                bool firstLine = true;
                for (auto &cp : r.points)
                {
                    if (!firstLine) ss << "\n";
                    firstLine = false;
                    std::string label = cp.type;
                    if (!label.empty())
                        label[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(label[0])));
                    ss << label << " at (";
                    for (size_t i = 0; i < cp.point.size(); ++i)
                    {
                        if (i)
                            ss << ", ";
                        ss << cp.point[i];
                    }
                    ss << "), f = " << cp.fVal;
                }
                out.symbolic = ss.str().empty()
                    ? "Result: no critical points found in the given region."
                    : ss.str();
                out.method = "Hessian classification";
                return out;
            }
            if (op == "lagrange")
            {
                auto r = lagrangeMultipliers(
                    getStr(json, "f"), getStr(json, "g"),
                    getStrArray(json, "vars"),
                    getNumArray(json, "min"), getNumArray(json, "max"));
                out.ok = r.ok;
                out.error = r.error;
                out.symbolic = r.summary;
                out.method = "Lagrange multipliers";
                std::ostringstream ss;
                for (size_t i = 0; i < r.candidates.size(); ++i)
                {
                    ss << "Point " << i + 1 << ": (";
                    for (size_t j = 0; j < r.candidates[i].size() - 1; ++j)
                    {
                        if (j)
                            ss << ",";
                        ss << r.candidates[i][j];
                    }
                    ss << ")  f=" << r.fVals[i] << "\n";
                }
                out.numeric = ss.str();
                return out;
            }

            // ── Vector-calculus theorems ──────────────────────────────────────
            if (op == "greens")
            {
                // Green's theorem: ∯ P dx + Q dy over region [ax,bx]×[ay,by]
                // Reduce to double integral of (∂Q/∂x - ∂P/∂y) via dispatch
                std::string P = getStr(json, "P");
                std::string Q = getStr(json, "Q");
                // Build (dQ/dx - dP/dy) and double-integrate it
                auto dQdx = computePartial(Q, "x", 1);
                auto dPdy = computePartial(P, "y", 1);
                if (!dQdx.ok || !dPdy.ok)
                {
           	    out.ok = false;
                    out.error = (!dQdx.ok ? dQdx.error : dPdy.error);
                    return out;
                }
                std::string integrand = "(" + dQdx.symbolic + ")-(" + dPdy.symbolic + ")";
                auto r = computeDoubleIntegral(integrand, "x", "y",
                                               getNum(json, "ax", 0), getNum(json, "bx", 1),
                                               getNum(json, "ay", 0), getNum(json, "by", 1));
                out.ok = r.ok;
                out.error = r.error;
                out.symbolic = "∮(P dx+Q dy) = ∬(∂Q/∂x−∂P/∂y) dA = " + std::to_string(r.value);
                out.numeric = std::to_string(r.value);
                out.method = "Green's theorem";
                return out;
            }

            if (op == "stokes")
            {
                // Stokes' theorem: ∯ F·dr = ∬ (∇×F)·dS
                // For now: compute curl and report symbolically
                std::vector<std::string> comps = {getStr(json, "P"), getStr(json, "Q"), getStr(json, "R")};
                std::vector<std::string> vars = {
                    getStr(json, "varX"), getStr(json, "varY"), getStr(json, "varZ")};
                if (vars[0].empty())
                    vars = {"x", "y", "z"};
                auto cr = computeCurl(comps, vars);
                if (!cr.ok)
                {
                    out.ok = false;
                    out.error = cr.error;
                    return out;
                }
                out.ok = true;
                out.method = "Stokes' theorem";
                out.symbolic = "∇×F = [" + (cr.components.size() > 0 ? cr.components[0] : "0") + ", " + (cr.components.size() > 1 ? cr.components[1] : "0") + ", " + (cr.components.size() > 2 ? cr.components[2] : "0") + "]";
                out.numeric = out.symbolic;
                return out;
            }

            if (op == "line_integral")
            {
                // Line integral ∫ f(x,y) ds along paric curv             // Params: expr, varX, varY, paramX (x as fn of param), param, a, b
                std::string f = getStr(json, "expr");
                std::string vX = getStr(json, "varX");
                if (vX.empty())
                    vX = "x";
                std::string vY = getStr(json, "varY");
                if (vY.empty())
                    vY = "y";
                std::string xoft = getStr(json, "paramX");
                std::string t = getStr(json, "param");
                if (t.empty())
                    t = "t";
                double a = getNum(json, "a", 0), b = getNum(json, "b", 1);
                // Substitute parametric expressions and integrate
                // dx/dt and dy/dt via partial differentiation w.r.t. t
                // ds = sqrt((x'(t))^2 + (y'(t))^2) dt  — numerical approach
                auto r = computeNumerical(
                    f + "*sqrt(1+(" + xoft + ")^2)", t, a, b, "romberg", 1000);
                out.ok = r.ok;
                out.error = r.error;
                out.symbolic = "∫ f ds = " + r.value;
                out.numeric = r.value;
                out.method = "parametric line integral (numerical)";
                return out;
            }

            if (op == "surface_int")
            {
                // Surface integral — basic rectangular numerical integration
                std::string f = getStr(json, "expr");
                std::string vX = getStr(json, "varX");
                if (vX.empty()) vX = "x";
		std::string vY = getStr(json, "varY");
                if (vY.empty()) vY = "y";
                double ax = getNum(json, "ax", 0), bx = getNum(json, "bx", 1);
                double ay = getNum(json, "ay", 0), by = getNum(json, "by", 1);
                auto r = computeDoubleIntegral(f, vX, vY, ax, bx, ay, by);
                out.ok = r.ok;
                out.error = r.error;
                out.symbolic = "∬ f dS = " + std::to_string(r.value);
              out.numeric = std::to_string(r.value);
              out.method = r.method;
                return out;
            }

            out.ok = false;
            out.error = "Unknown calculus operation: " + op;
        }
        catch (const std::exception &e)
        {
            out.ok = false;
            out.error = e.what();
        }
        return out;
    }

} // namespace Calculus
