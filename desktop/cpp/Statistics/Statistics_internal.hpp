#pragma once
// Statistics_internal.h
// Internal math functions shared between Statistics.cpp and Statistics_OttLongnecker.cpp.
// Not part of the public API — do not include from outside the Statistics module.

#include <cmath>
#include <string>
#include <sstream>
#include <iomanip>

namespace Statistics
{

    inline double lgamma_(double x) { return std::lgamma(x); }

    double normcdf(double x);
    double norminv(double p);
    double tcdf(double x, double df);
    double chi2cdf(double x, double df);
    double fcdf(double x, double d1, double d2);
    double betainc(double x, double a, double b);
    double gammainc(double a, double x);

    // Frequency table format helper
    inline std::string fmtOL(double v, int prec = 6)
    {
        if (!std::isfinite(v))
            return std::isinf(v) ? "inf" : "NaN";
        std::ostringstream ss;
        ss << std::setprecision(prec) << v;
        return ss.str();
    }

} // namespace Statistics