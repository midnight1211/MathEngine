// calculus/series/Series.cpp

#include "Series.hpp"
#include "../core/Week4Bridge.hpp"
#include <cmath>
#include <sstream>
#include <iomanip>

namespace Calculus
{

    static double factorial(int n)
    {
        double f = 1.0;
        for (int i = 2; i <= n; ++i)
            f *= i;
        return f;
    }

    static std::string fmtCoeff(double c, bool exact)
    {
        if (std::abs(c) < 1e-12)
            return "0";
        if (exact)
        {
            // Try to express as a simple fraction
            for (int d = 1; d <= 120; ++d)
            {
                if (const double n = c * d; std::abs(n - std::round(n)) < 1e-9)
                {
                    const long long ni = static_cast<long long>(std::round(n));
                    if (d == 1)
                        return std::to_string(ni);
                    return std::to_string(ni) + "/" + std::to_string(d);
                }
            }
        }
        std::ostringstream ss;
        ss << std::setprecision(6) << c;
        return ss.str();
    }

    // Build a readable polynomial string from terms
    static std::string buildPolynomial(const std::vector<SeriesTerm> &terms,
                                       const std::string &var,
                                       double center)
    {
        std::ostringstream ss;
        bool first = true;
        for (const auto &t : terms)
        {
            if (std::abs(t.coefficient) < 1e-12)
                continue;
            double c = t.coefficient;
            if (!first)
            {
                ss << (c >= 0 ? " + " : " - ");
                c = std::abs(c);
            }
            else if (c < 0)
            {
                ss << "-";
                c = std::abs(c);
            }
            first = false;

            std::string base = (center == 0.0) ? var
                                               : ("(" + var + " - " + fmtCoeff(center, true) + ")");

            if (t.power == 0)
            {
                ss << fmtCoeff(c, true);
            }
            else if (t.power == 1)
            {
                if (std::abs(c - 1.0) > 1e-12)
                    ss << fmtCoeff(c, true) << "*";
                ss << base;
            }
            else
            {
                if (std::abs(c - 1.0) > 1e-12)
                    ss << fmtCoeff(c, true) << "*";
                ss << base << "^" << t.power;
            }
        }
        if (first)
            return "0";
        return ss.str();
    }

    static std::string buildLatex(const std::vector<SeriesTerm> &terms,
                                  const std::string &var,
                                  double center)
    {
        std::ostringstream ss;
        bool first = true;
        for (const auto &t : terms)
        {
            if (std::abs(t.coefficient) < 1e-12)
                continue;
            double c = t.coefficient;
            if (!first)
            {
                ss << (c >= 0 ? " + " : " - ");
                c = std::abs(c);
            }
            else if (c < 0)
            {
                ss << "-";
                c = std::abs(c);
            }
            first = false;

            std::string base = (center == 0.0)
                                   ? var
                                   : ("(" + var + " - " + fmtCoeff(center, true) + ")");

            if (t.power == 0)
            {
                ss << fmtCoeff(c, true);
            }
            else if (t.power == 1)
            {
                if (std::abs(c - 1.0) > 1e-12)
                    ss << fmtCoeff(c, true);
                ss << base;
            }
            else
            {
                if (std::abs(c - 1.0) > 1e-12)
                    ss << fmtCoeff(c, true);
                ss << base << "^{" << t.power << "}";
            }
        }
        if (first)
            return "0";
        return ss.str();
    }

    // ── Taylor series ─────────────────────────────────────────────────────────────

    SeriesResult taylorSeries(const ExprPtr &f,
                              const std::string &var,
                              double center,
                              int n)
    {
        SeriesResult out;
        out.radiusOfConv = std::numeric_limits<double>::infinity();

        ExprPtr deriv = f;
        std::vector<double> coeffs;

        for (int k = 0; k <= n; ++k)
        {
            double val;
            try
            {
                val = evaluate(deriv, {{var, center}});
            }
            catch (...)
            {
                // evaluate() threw — try a central finite-difference estimate instead.
                // This handles: functions undefined at an exact symbolic point,
                // Week4Bridge evaluate() not supporting variable bindings, etc.
                const double h = (std::abs(center) > 0.1) ? std::abs(center) * 0.01 : 0.01;
                try
                {
                    double fp = evaluate(deriv, {{var, center + h}});
                    double fm = evaluate(deriv, {{var, center - h}});
                    val = (fp + fm) / 2.0; // midpoint estimate
                }
                catch (...)
                {
                    // Truly undefined — stop here but keep the terms we have
                    if (k == 0)
                    {
                        out.ok = false;
                        out.error = "Function is undefined or cannot be evaluated at center=" +
                                    std::to_string(center);
                        return out;
                    }
                    break; // partial series is still valid
                }
            }
            double coeff = val / factorial(k);
            coeffs.push_back(coeff);

            SeriesTerm term;
            term.coefficient = coeff;
            term.power = k;
            term.symbolic = fmtCoeff(coeff, true) +
                            (k > 0 ? ("*(x-a)^" + std::to_string(k)) : "");
            out.terms.push_back(term);

            if (k < n)
            {
                try
                {
                    deriv = simplify(diff(deriv, var));
                }
                catch (...)
                {
                    break;
                }
            }
        }

        out.polynomial = buildPolynomial(out.terms, var, center);
        out.polynomial_latex = buildLatex(out.terms, var, center);
        out.remainder = "R_" + std::to_string(n) + "(x) = f^(" +
                        std::to_string(n + 1) + ")(c)/(" +
                        std::to_string(n + 1) + "!) * (x-a)^" +
                        std::to_string(n + 1);
        out.radiusOfConv = radiusOfConvergence(coeffs);
        return out;
    }

    SeriesResult maclaurinSeries(const ExprPtr &f,
                                 const std::string &var,
                                 int n)
    {
        return taylorSeries(f, var, 0.0, n);
    }

    // ── Radius of convergence (ratio test) ───────────────────────────────────────

    double radiusOfConvergence(const std::vector<double> &coefficients)
    {
        // R = lim |a_n / a_{n+1}|  (ratio test)
        // Use average of last few ratios for stability
        double sumRatios = 0.0;
        int count = 0;
        for (int i = static_cast<int>(coefficients.size()) - 2; i >= 0 &&
                                                                i >= static_cast<int>(coefficients.size()) - 6;
             --i)
        {
            double an = std::abs(coefficients[i]);
            double an1 = std::abs(coefficients[i + 1]);
            if (an1 < 1e-15)
                return std::numeric_limits<double>::infinity();
            if (an < 1e-15)
                return 0.0;
            sumRatios += an / an1;
            ++count;
        }
        if (count == 0)
            return std::numeric_limits<double>::infinity();
        return sumRatios / count;
    }

    // ── Power series from raw coefficients ───────────────────────────────────────

    SeriesResult powerSeries(const std::vector<double> &coefficients,
                             const std::string &var,
                             double center)
    {
        SeriesResult out;
        for (int k = 0; k < static_cast<int>(coefficients.size()); ++k)
        {
            SeriesTerm t;
            t.coefficient = coefficients[k];
            t.power = k;
            t.symbolic = fmtCoeff(coefficients[k], true);
            out.terms.push_back(t);
        }
        out.polynomial = buildPolynomial(out.terms, var, center);
        out.polynomial_latex = buildLatex(out.terms, var, center);
        out.radiusOfConv = radiusOfConvergence(coefficients);
        return out;
    }

    // ── Formatted entry points ────────────────────────────────────────────────────

    SeriesResult computeTaylor(const std::string &exprStr,
                               const std::string &var,
                               double center,
                               int order)
    {
        try
        {
            ExprPtr f = parse(exprStr);
            return taylorSeries(f, var, center, order);
        }
        catch (const std::exception &ex)
        {
            SeriesResult r;
            r.ok = false;
            r.error = ex.what();
            return r;
        }
    }

    SeriesResult computeMaclaurin(const std::string &exprStr,
                                  const std::string &var,
                                  int order)
    {
        return computeTaylor(exprStr, var, 0.0, order);
    }

}