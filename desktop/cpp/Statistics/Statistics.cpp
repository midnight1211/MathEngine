// Statistics.cpp

#include "Statistics.hpp"
#include "Statistics_internal.hpp"
#include <cmath>
#include <algorithm>
#include <numeric>
#include <sstream>
#include <iomanip>
#include <stdexcept>
#include <map>

namespace Statistics
{

    static const double PI = M_PI;

    static std::string fmt(double v, int prec = 6)
    {
        std::ostringstream ss;
        ss << std::setprecision(prec) << v;
        return ss.str();
    }

    static StatResult ok(const std::string &val, const std::string &detail = "")
    {
        return {true, val, detail, ""};
    }
    static std::vector<Vec> parseMatrix(const std::string &s)
    {
        std::vector<Vec> M;
        size_t pos = 0;
        while ((pos = s.find('[', pos)) != std::string::npos)
        {
            ++pos;
            size_t end = s.find(']', pos);
            if (end == std::string::npos)
                break;
            Vec row;
            std::istringstream rs(s.substr(pos, end - pos));
            std::string tok;
            while (std::getline(rs, tok, ','))
            {
                try
                {
                    row.push_back(std::stod(tok));
                }
                catch (...)
                {
                }
            }
            if (!row.empty())
                M.push_back(row);
            pos = end + 1;
        }
        return M;
    }

    static StatResult err(const std::string &msg)
    {
        return {false, "", "", msg};
    }

    // =============================================================================
    // Special functions
    // =============================================================================

    // Regularised incomplete gamma (series expansion)
    double gammainc(double a, double x)
    {
        if (x < 0)
            return 0;
        if (x == 0)
            return 0;
        double sum = 1.0 / a, term = 1.0 / a;
        for (int k = 1; k < 200; ++k)
        {
            term *= x / (a + k);
            sum += term;
            if (std::abs(term) < 1e-12 * std::abs(sum))
                break;
        }
        return sum * std::exp(-x + a * std::log(x) - lgamma_(a));
    }

    // Regularised incomplete beta (continued fraction)
    double betainc(double x, double a, double b)
    {
        if (x <= 0)
            return 0;
        if (x >= 1)
            return 1;
        double lbeta = lgamma_(a) + lgamma_(b) - lgamma_(a + b);
        double front = std::exp(std::log(x) * a + std::log(1 - x) * b - lbeta) / a;
        // Lentz's continued fraction
        double qab = a + b, qap = a + 1, qam = a - 1;
        double c = 1, d = 1 - qab * x / qap;
        if (std::abs(d) < 1e-30)
            d = 1e-30;
        d = 1 / d;
        double cf = d;
        for (int m = 1; m <= 200; ++m)
        {
            int m2 = 2 * m;
            double aa = m * (b - m) * x / ((qam + m2) * (a + m2));
            d = 1 + aa * d;
            if (std::abs(d) < 1e-30)
                d = 1e-30;
            d = 1 / d;
            c = 1 + aa / c;
            if (std::abs(c) < 1e-30)
                c = 1e-30;
            cf *= c * d;
            aa = -(a + m) * (qab + m) * x / ((a + m2) * (qap + m2));
            d = 1 + aa * d;
            if (std::abs(d) < 1e-30)
                d = 1e-30;
            d = 1 / d;
            c = 1 + aa / c;
            if (std::abs(c) < 1e-30)
                c = 1e-30;
            double del = c * d;
            cf *= del;
            if (std::abs(del - 1) < 1e-10)
                break;
        }
        return front * cf;
    }

    // Standard normal CDF
    double normcdf(double x)
    {
        return 0.5 * std::erfc(-x / std::sqrt(2.0));
    }

    // Standard normal quantile (rational approximation)
    double norminv(double p)
    {
        if (p <= 0)
            return -std::numeric_limits<double>::infinity();
        if (p >= 1)
            return std::numeric_limits<double>::infinity();
        if (p < 0.02425)
        {
            double q = std::sqrt(-2 * std::log(p));
            return -(((((2.515517 + 0.802853 * q + 0.010328 * q * q) / (1 + 1.432788 * q + 0.189269 * q * q + 0.001308 * q * q * q)) - q)));
        }
        if (p <= 0.97575)
        {
            double q = p - 0.5, r = q * q;
            return q * (((((2.506628274631 + r * (-18.61500062529 + r * (41.39119773534 + r * (-25.44106049637 + r * (4.97191438954 + r * (-0.99999751057))))))) /
                          ((1 + r * (-3.96568827558 + r * (5.32934898541 + r * (-2.38357955154 + r * 1.05982936498))))))));
        }
        double q = std::sqrt(-2 * std::log(1 - p));
        return (((((2.515517 + 0.802853 * q + 0.010328 * q * q) / (1 + 1.432788 * q + 0.189269 * q * q + 0.001308 * q * q * q)) - q)));
    }

    // Student t CDF via incomplete beta
    double tcdf(double t, double df)
    {
        double x = df / (df + t * t);
        double ibeta = betainc(x, df / 2, 0.5);
        return t >= 0 ? 1.0 - ibeta / 2 : ibeta / 2;
    }

    // Chi-squared CDF
    double chi2cdf(double x, double df)
    {
        return gammainc(df / 2, x / 2);
    }

    // F-distribution CDF
    double fcdf(double x, double d1, double d2)
    {
        double u = d1 * x / (d1 * x + d2);
        return betainc(u, d1 / 2, d2 / 2);
    }

    // =============================================================================
    // Descriptive statistics
    // =============================================================================

    StatResult mean(const Vec &x)
    {
        if (x.empty())
            return err("Empty data");
        double m = std::accumulate(x.begin(), x.end(), 0.0) / x.size();
        return ok(fmt(m), "n=" + std::to_string(x.size()));
    }

    StatResult median(Vec x)
    {
        if (x.empty())
            return err("Empty data");
        std::sort(x.begin(), x.end());
        int n = x.size();
        double m = (n % 2 == 0) ? (x[n / 2 - 1] + x[n / 2]) / 2.0 : x[n / 2];
        return ok(fmt(m));
    }

    StatResult mode(const Vec &x)
    {
        if (x.empty())
            return err("Empty data");
        std::map<double, int> freq;
        for (double v : x)
            freq[v]++;
        auto it = std::max_element(freq.begin(), freq.end(),
                                   [](const auto &a, const auto &b)
                                   { return a.second < b.second; });
        return ok(fmt(it->first), "frequency=" + std::to_string(it->second));
    }

    StatResult variance(const Vec &x, bool pop)
    {
        if (x.size() < 2)
            return err("Need at least 2 observations");
        double m = std::accumulate(x.begin(), x.end(), 0.0) / x.size();
        double s2 = 0;
        for (double v : x)
            s2 += (v - m) * (v - m);
        s2 /= (pop ? x.size() : x.size() - 1);
        return ok(fmt(s2), pop ? "population" : "sample");
    }

    StatResult stddev(const Vec &x, bool pop)
    {
        auto v = variance(x, pop);
        if (!v.ok)
            return v;
        return ok(fmt(std::sqrt(std::stod(v.value))), v.detail);
    }

    StatResult skewness(const Vec &x)
    {
        if (x.size() < 3)
            return err("Need at least 3 observations");
        double n = x.size();
        double m = std::accumulate(x.begin(), x.end(), 0.0) / n;
        double s = 0, m3 = 0;
        for (double v : x)
        {
            s += (v - m) * (v - m);
            m3 += (v - m) * (v - m) * (v - m);
        }
        s = std::sqrt(s / (n - 1));
        double skew = (n / ((n - 1) * (n - 2))) * m3 / (s * s * s);
        return ok(fmt(skew), std::abs(skew) < 0.5 ? "approx symmetric" : skew > 0 ? "right-skewed"
                                                                                  : "left-skewed");
    }

    StatResult kurtosis(const Vec &x)
    {
        if (x.size() < 4)
            return err("Need at least 4 observations");
        double n = x.size();
        double m = std::accumulate(x.begin(), x.end(), 0.0) / n;
        double s2 = 0, m4 = 0;
        for (double v : x)
        {
            s2 += (v - m) * (v - m);
            m4 += std::pow(v - m, 4);
        }
        s2 /= (n - 1);
        double kurt = (m4 / n) / (s2 * s2) - 3; // excess kurtosis
        return ok(fmt(kurt), kurt > 0 ? "leptokurtic (heavy tails)" : kurt < 0 ? "platykurtic (light tails)"
                                                                               : "mesokurtic (normal)");
    }

    StatResult percentile(Vec x, double p)
    {
        if (x.empty())
            return err("Empty data");
        std::sort(x.begin(), x.end());
        double pos = p / 100.0 * (x.size() - 1);
        int lo = (int)pos;
        double frac = pos - lo;
        double val = (lo + 1 < (int)x.size()) ? x[lo] + frac * (x[lo + 1] - x[lo]) : x[lo];
        return ok(fmt(val), "P" + fmt(p));
    }

    StatResult iqr(Vec x)
    {
        auto q1 = percentile(x, 25), q3 = percentile(x, 75);
        if (!q1.ok || !q3.ok)
            return err("Cannot compute IQR");
        double iq = std::stod(q3.value) - std::stod(q1.value);
        return ok(fmt(iq), "Q1=" + q1.value + ", Q3=" + q3.value);
    }

    StatResult fiveNumber(Vec x)
    {
        if (x.empty())
            return err("Empty data");
        std::sort(x.begin(), x.end());
        std::ostringstream ss;
        ss << "Min=" << fmt(x.front())
           << ", Q1=" << percentile(x, 25).value
           << ", Median=" << percentile(x, 50).value
           << ", Q3=" << percentile(x, 75).value
           << ", Max=" << fmt(x.back());
        return ok(ss.str());
    }

    StatResult summarize(Vec x)
    {
        if (x.empty())
            return err("Empty data");
        std::ostringstream ss;
        ss << "n       = " << x.size() << "\n";
        ss << "mean    = " << mean(x).value << "\n";
        ss << "median  = " << median(x).value << "\n";
        ss << "std dev = " << stddev(x).value << "\n";
        ss << "variance= " << variance(x).value << "\n";
        ss << "skewness= " << skewness(x).value << "\n";
        ss << "kurtosis= " << kurtosis(x).value << "\n";
        ss << fiveNumber(x).value;
        return ok(ss.str());
    }

    // =============================================================================
    // Distributions
    // =============================================================================

    StatResult normalPDF(double x, double mu, double sigma)
    {
        double v = std::exp(-0.5 * ((x - mu) / sigma) * ((x - mu) / sigma)) / (sigma * std::sqrt(2 * PI));
        return ok(fmt(v), "N(" + fmt(mu) + "," + fmt(sigma) + ")");
    }
    StatResult normalCDF(double x, double mu, double sigma)
    {
        return ok(fmt(normcdf((x - mu) / sigma)));
    }
    StatResult normalQF(double p, double mu, double sigma)
    {
        return ok(fmt(mu + sigma * norminv(p)));
    }

    StatResult tPDF(double x, double df)
    {
        double v = std::exp(lgamma_((df + 1) / 2) - lgamma_(df / 2)) /
                   (std::sqrt(df * PI) * std::pow(1 + x * x / df, (df + 1) / 2));
        return ok(fmt(v), "t(" + fmt(df) + ")");
    }
    StatResult tCDF(double x, double df) { return ok(fmt(tcdf(x, df))); }
    StatResult tQF(double p, double df)
    {
        // Bisection on tCDF
        double lo = -100, hi = 100;
        for (int i = 0; i < 100; ++i)
        {
            double mid = (lo + hi) / 2;
            if (tcdf(mid, df) < p)
                lo = mid;
            else
                hi = mid;
        }
        return ok(fmt((lo + hi) / 2));
    }

    StatResult chiSqPDF(double x, double df)
    {
        if (x <= 0)
            return ok("0");
        double v = std::exp((df / 2 - 1) * std::log(x) - x / 2 - (df / 2) * std::log(2) - lgamma_(df / 2));
        return ok(fmt(v));
    }
    StatResult chiSqCDF(double x, double df)
    {
        return ok(fmt(chi2cdf(x, df)));
    }
    StatResult chiSqQF(double p, double df)
    {
        double lo = 0, hi = df + 10 * std::sqrt(2 * df);
        for (int i = 0; i < 100; ++i)
        {
            double mid = (lo + hi) / 2;
            if (chi2cdf(mid, df) < p)
                lo = mid;
            else
                hi = mid;
        }
        return ok(fmt((lo + hi) / 2));
    }

    StatResult fDistPDF(double x, double d1, double d2)
    {
        if (x <= 0)
            return ok("0");
        double lv = (d1 / 2) * std::log(d1 / d2) + (d1 / 2 - 1) * std::log(x) - ((d1 + d2) / 2) * std::log(1 + d1 * x / d2) + lgamma_((d1 + d2) / 2) - lgamma_(d1 / 2) - lgamma_(d2 / 2);
        return ok(fmt(std::exp(lv)));
    }
    StatResult fDistCDF(double x, double d1, double d2)
    {
        return ok(fmt(fcdf(x, d1, d2)));
    }

    StatResult expPDF(double x, double lambda)
    {
        if (x < 0)
            return ok("0");
        return ok(fmt(lambda * std::exp(-lambda * x)));
    }
    StatResult expCDF(double x, double lambda)
    {
        if (x < 0)
            return ok("0");
        return ok(fmt(1 - std::exp(-lambda * x)));
    }

    StatResult uniformPDF(double x, double a, double b)
    {
        return ok((x >= a && x <= b) ? fmt(1.0 / (b - a)) : "0");
    }
    StatResult uniformCDF(double x, double a, double b)
    {
        if (x < a)
            return ok("0");
        if (x > b)
            return ok("1");
        return ok(fmt((x - a) / (b - a)));
    }

    StatResult gammaPDF(double x, double alpha, double beta)
    {
        if (x <= 0)
            return ok("0");
        double v = std::exp((alpha - 1) * std::log(x) - x / beta - alpha * std::log(beta) - lgamma_(alpha));
        return ok(fmt(v));
    }
    StatResult gammaCDF(double x, double alpha, double beta)
    {
        return ok(fmt(gammainc(alpha, x / beta)));
    }

    StatResult betaPDF(double x, double a, double b)
    {
        if (x <= 0 || x >= 1)
            return ok("0");
        double v = std::exp((a - 1) * std::log(x) + (b - 1) * std::log(1 - x) + lgamma_(a + b) - lgamma_(a) - lgamma_(b));
        return ok(fmt(v));
    }
    StatResult betaCDF(double x, double a, double b)
    {
        return ok(fmt(betainc(x, a, b)));
    }

    // Discrete
    StatResult binomialPMF(int k, int n, double p)
    {
        if (k < 0 || k > n)
            return ok("0");
        double lv = lgamma_(n + 1) - lgamma_(k + 1) - lgamma_(n - k + 1) + k * std::log(p) + (n - k) * std::log(1 - p);
        return ok(fmt(std::exp(lv)));
    }
    StatResult binomialCDF(int k, int n, double p)
    {
        double s = 0;
        for (int i = 0; i <= k; ++i)
            s += std::stod(binomialPMF(i, n, p).value);
        return ok(fmt(s));
    }
    StatResult poissonPMF(int k, double lambda)
    {
        double v = std::exp(-lambda + k * std::log(lambda) - lgamma_(k + 1));
        return ok(fmt(v));
    }
    StatResult poissonCDF(int k, double lambda)
    {
        double s = 0;
        for (int i = 0; i <= k; ++i)
            s += std::stod(poissonPMF(i, lambda).value);
        return ok(fmt(s));
    }
    StatResult geometricPMF(int k, double p)
    {
        return ok(fmt(std::pow(1 - p, k - 1) * p));
    }
    StatResult hypergeoPMF(int k, int N, int K, int n)
    {
        auto lC = [](int a, int b)
        {
            return lgamma_(a + 1) - lgamma_(b + 1) - lgamma_(a - b + 1);
        };
        double v = std::exp(lC(K, k) + lC(N - K, n - k) - lC(N, n));
        return ok(fmt(v));
    }
    StatResult negativeBinPMF(int k, int r, double p)
    {
        double v = std::exp(lgamma_(k + r) - lgamma_(r) - lgamma_(k + 1) + r * std::log(p) + k * std::log(1 - p));
        return ok(fmt(v));
    }

    // =============================================================================
    // Confidence intervals
    // =============================================================================

    StatResult ciMeanZ(const Vec &x, double alpha)
    {
        if (x.empty())
            return err("Empty data");
        double n = x.size();
        double m = std::stod(mean(x).value);
        double s = std::stod(stddev(x).value);
        double z = norminv(1 - alpha / 2);
        double me = z * s / std::sqrt(n);
        std::ostringstream ss;
        ss << fmt(m - me) << " to " << fmt(m + me)
           << "  (z=" << fmt(z) << ", ME=" << fmt(me) << ")";
        return ok(ss.str(), std::to_string((int)((1 - alpha) * 100)) + "% CI");
    }

    StatResult ciMeanT(const Vec &x, double alpha)
    {
        if (x.size() < 2)
            return err("Need at least 2 observations");
        double n = x.size();
        double m = std::stod(mean(x).value);
        double s = std::stod(stddev(x).value);
        double t = std::stod(tQF(1 - alpha / 2, n - 1).value);
        double me = t * s / std::sqrt(n);
        std::ostringstream ss;
        ss << fmt(m - me) << " to " << fmt(m + me)
           << "  (t=" << fmt(t) << ", df=" << n - 1 << ", ME=" << fmt(me) << ")";
        return ok(ss.str());
    }

    StatResult ciProportion(int k, int n, double alpha)
    {
        double p = (double)k / n;
        double z = norminv(1 - alpha / 2);
        double me = z * std::sqrt(p * (1 - p) / n);
        std::ostringstream ss;
        ss << fmt(p - me) << " to " << fmt(p + me) << "  (p̂=" << fmt(p) << ")";
        return ok(ss.str());
    }

    // =============================================================================
    // Hypothesis tests
    // =============================================================================

    StatResult zTest(const Vec &x, double mu0, double sigma, double alpha)
    {
        if (x.empty())
            return err("Empty data");
        double n = x.size();
        double m = std::stod(mean(x).value);
        double z = (m - mu0) / (sigma / std::sqrt(n));
        double pval = 2 * (1 - normcdf(std::abs(z)));
        std::ostringstream ss;
        ss << "z = " << fmt(z) << ",  p-value = " << fmt(pval)
           << "\nConclusion: " << (pval < alpha ? "Reject H₀" : "Fail to reject H₀")
           << "  (α=" << fmt(alpha) << ")";
        return ok(ss.str());
    }

    StatResult tTestOne(const Vec &x, double mu0, double alpha)
    {
        if (x.size() < 2)
            return err("Need at least 2 observations");
        double n = x.size();
        double m = std::stod(mean(x).value);
        double s = std::stod(stddev(x).value);
        double t = (m - mu0) / (s / std::sqrt(n));
        double df = n - 1;
        double pval = 2 * (1 - tcdf(std::abs(t), df));
        std::ostringstream ss;
        ss << "t = " << fmt(t) << ",  df = " << df << ",  p-value = " << fmt(pval)
           << "\nConclusion: " << (pval < alpha ? "Reject H₀" : "Fail to reject H₀");
        return ok(ss.str());
    }

    StatResult tTestTwo(const Vec &x, const Vec &y, double alpha, bool equalVar)
    {
        if (x.size() < 2 || y.size() < 2)
            return err("Need at least 2 observations per group");
        double nx = x.size(), ny = y.size();
        double mx = std::stod(mean(x).value), my = std::stod(mean(y).value);
        double sx = std::stod(stddev(x).value), sy = std::stod(stddev(y).value);
        double t, df;
        if (equalVar)
        {
            double sp2 = ((nx - 1) * sx * sx + (ny - 1) * sy * sy) / (nx + ny - 2);
            t = (mx - my) / std::sqrt(sp2 * (1 / nx + 1 / ny));
            df = nx + ny - 2;
        }
        else
        {
            double se2 = sx * sx / nx + sy * sy / ny;
            t = (mx - my) / std::sqrt(se2);
            double num = se2 * se2;
            double den = (sx * sx / nx) * (sx * sx / nx) / (nx - 1) + (sy * sy / ny) * (sy * sy / ny) / (ny - 1);
            df = num / den;
        }
        double pval = 2 * (1 - tcdf(std::abs(t), df));
        std::ostringstream ss;
        ss << "t = " << fmt(t) << ",  df = " << fmt(df) << ",  p-value = " << fmt(pval)
           << "\nConclusion: " << (pval < alpha ? "Reject H₀" : "Fail to reject H₀");
        return ok(ss.str());
    }

    StatResult tTestPaired(const Vec &x, const Vec &y, double alpha)
    {
        if (x.size() != y.size())
            return err("Paired test requires equal sample sizes");
        Vec diff(x.size());
        for (size_t i = 0; i < x.size(); ++i)
            diff[i] = x[i] - y[i];
        return tTestOne(diff, 0, alpha);
    }

    StatResult chiSqTest(const Vec &obs, const Vec &exp_, double alpha)
    {
        if (obs.size() != exp_.size())
            return err("Observed and expected must have same length");
        double chi2 = 0;
        for (size_t i = 0; i < obs.size(); ++i)
        {
            if (exp_[i] < 1e-10)
                return err("Expected frequency must be > 0");
            chi2 += (obs[i] - exp_[i]) * (obs[i] - exp_[i]) / exp_[i];
        }
        double df = obs.size() - 1;
        double pval = 1 - chi2cdf(chi2, df);
        std::ostringstream ss;
        ss << "χ² = " << fmt(chi2) << ",  df = " << df << ",  p-value = " << fmt(pval)
           << "\nConclusion: " << (pval < alpha ? "Reject H₀" : "Fail to reject H₀");
        return ok(ss.str());
    }

    StatResult fTest(const Vec &x, const Vec &y, double alpha)
    {
        if (x.size() < 2 || y.size() < 2)
            return err("Need at least 2 obs per group");
        double sx = std::stod(stddev(x).value), sy = std::stod(stddev(y).value);
        double F = (sx * sx) / (sy * sy);
        double d1 = x.size() - 1, d2 = y.size() - 1;
        double pval = 2 * std::min(fcdf(F, d1, d2), 1 - fcdf(F, d1, d2));
        std::ostringstream ss;
        ss << "F = " << fmt(F) << ",  df1=" << d1 << ",  df2=" << d2
           << ",  p-value = " << fmt(pval)
           << "\nConclusion: " << (pval < alpha ? "Reject H₀ (unequal variances)" : "Fail to reject H₀");
        return ok(ss.str());
    }

    StatResult anovaOne(const std::vector<Vec> &groups, double alpha)
    {
        if (groups.size() < 2)
            return err("Need at least 2 groups");
        int k = groups.size();
        double N = 0, grandSum = 0;
        for (auto &g : groups)
        {
            N += g.size();
            for (double v : g)
                grandSum += v;
        }
        double grandMean = grandSum / N;
        double SSB = 0, SSW = 0;
        for (auto &g : groups)
        {
            double gm = std::stod(mean(g).value);
            SSB += g.size() * (gm - grandMean) * (gm - grandMean);
            for (double v : g)
                SSW += (v - gm) * (v - gm);
        }
        double dfB = k - 1, dfW = N - k;
        double MSB = SSB / dfB, MSW = SSW / dfW;
        double F = MSB / MSW;
        double pval = 1 - fcdf(F, dfB, dfW);
        std::ostringstream ss;
        ss << "One-way ANOVA:\n"
           << "  SSB=" << fmt(SSB) << ", SSW=" << fmt(SSW) << "\n"
           << "  dfB=" << dfB << ", dfW=" << dfW << "\n"
           << "  MSB=" << fmt(MSB) << ", MSW=" << fmt(MSW) << "\n"
           << "  F=" << fmt(F) << ",  p-value=" << fmt(pval) << "\n"
           << "Conclusion: " << (pval < alpha ? "Reject H₀ (means differ)" : "Fail to reject H₀");
        return ok(ss.str());
    }

    // =============================================================================
    // Non-parametric
    // =============================================================================

    StatResult spearman(const Vec &x, const Vec &y)
    {
        if (x.size() != y.size())
            return err("Vectors must have equal length");
        int n = x.size();
        auto rank = [&](const Vec &v)
        {
            Vec r(n);
            std::vector<int> idx(n);
            std::iota(idx.begin(), idx.end(), 0);
            std::sort(idx.begin(), idx.end(), [&](int a, int b)
                      { return v[a] < v[b]; });
            for (int i = 0; i < n; ++i)
                r[idx[i]] = i + 1;
            return r;
        };
        Vec rx = rank(x), ry = rank(y);
        double d2 = 0;
        for (int i = 0; i < n; ++i)
            d2 += (rx[i] - ry[i]) * (rx[i] - ry[i]);
        double rs = 1 - 6 * d2 / (n * (n * n - 1));
        return ok(fmt(rs), "Spearman rank correlation");
    }

    StatResult kendallTau(const Vec &x, const Vec &y)
    {
        if (x.size() != y.size())
            return err("Vectors must have equal length");
        int n = x.size(), conc = 0, disc = 0;
        for (int i = 0; i < n - 1; ++i)
            for (int j = i + 1; j < n; ++j)
            {
                double sx = (x[j] - x[i]), sy = (y[j] - y[i]);
                if (sx * sy > 0)
                    conc++;
                else if (sx * sy < 0)
                    disc++;
            }
        double tau = (double)(conc - disc) / (0.5 * n * (n - 1));
        return ok(fmt(tau), "Kendall's tau");
    }

    StatResult pearson(const Vec &x, const Vec &y)
    {
        if (x.size() != y.size() || x.size() < 2)
            return err("Need equal-length vectors of size ≥ 2");
        double mx = std::stod(mean(x).value), my = std::stod(mean(y).value);
        double num = 0, dx2 = 0, dy2 = 0;
        for (size_t i = 0; i < x.size(); ++i)
        {
            num += (x[i] - mx) * (y[i] - my);
            dx2 += (x[i] - mx) * (x[i] - mx);
            dy2 += (y[i] - my) * (y[i] - my);
        }
        double r = num / std::sqrt(dx2 * dy2);
        return ok(fmt(r), "Pearson r");
    }

    StatResult covariance(const Vec &x, const Vec &y, bool pop)
    {
        if (x.size() != y.size() || x.size() < 2)
            return err("Need equal-length vectors of size ≥ 2");
        double mx = std::stod(mean(x).value), my = std::stod(mean(y).value);
        double s = 0;
        for (size_t i = 0; i < x.size(); ++i)
            s += (x[i] - mx) * (y[i] - my);
        return ok(fmt(s / (pop ? x.size() : x.size() - 1)));
    }

    StatResult mannWhitney(const Vec &x, const Vec &y, double alpha)
    {
        int nx = x.size(), ny = y.size();
        double U = 0;
        for (int i = 0; i < nx; ++i)
            for (int j = 0; j < ny; ++j)
                if (x[i] > y[j])
                    U++;
                else if (x[i] == y[j])
                    U += 0.5;
        double Umax = nx * ny - U;
        double mu = nx * ny / 2.0, sigma = std::sqrt(nx * ny * (nx + ny + 1) / 12.0);
        double z = (std::min(U, Umax) - mu) / sigma;
        double pval = 2 * (1 - normcdf(std::abs(z)));
        std::ostringstream ss;
        ss << "U=" << fmt(U) << ",  z=" << fmt(z) << ",  p-value=" << fmt(pval)
           << "\n"
           << (pval < alpha ? "Reject H₀" : "Fail to reject H₀");
        return ok(ss.str());
    }

    StatResult wilcoxon(const Vec &x, double mu0, double alpha)
    {
        Vec d;
        for (double v : x)
            if (std::abs(v - mu0) > 1e-12)
                d.push_back(v - mu0);
        int n = d.size();
        std::vector<std::pair<double, double>> ranked;
        for (double v : d)
            ranked.push_back({std::abs(v), v > 0 ? 1 : -1});
        std::sort(ranked.begin(), ranked.end());
        double Wp = 0;
        for (int i = 0; i < n; ++i)
            if (ranked[i].second > 0)
                Wp += i + 1;
        double Wn = n * (n + 1) / 2.0 - Wp;
        double W = std::min(Wp, Wn);
        double mu = n * (n + 1) / 4.0, sig = std::sqrt(n * (n + 1) * (2 * n + 1) / 24.0);
        double z = (W - mu) / sig;
        double pval = 2 * (1 - normcdf(std::abs(z)));
        std::ostringstream ss;
        ss << "W=" << fmt(W) << ",  z=" << fmt(z) << ",  p-value=" << fmt(pval)
           << "\n"
           << (pval < alpha ? "Reject H₀" : "Fail to reject H₀");
        return ok(ss.str());
    }

    StatResult kruskalWallis(const std::vector<Vec> &groups, double alpha)
    {
        int k = groups.size();
        double N = 0;
        for (auto &g : groups)
            N += g.size();
        // Rank all values together
        std::vector<std::pair<double, int>> all;
        for (int i = 0; i < k; ++i)
            for (double v : groups[i])
                all.push_back({v, i});
        std::sort(all.begin(), all.end());
        std::vector<double> Rk(k, 0);
        for (int i = 0; i < (int)all.size(); ++i)
            Rk[all[i].second] += i + 1;
        double H = 12 / (N * (N + 1));
        for (int i = 0; i < k; ++i)
            H += Rk[i] * Rk[i] / groups[i].size();
        H -= 3 * (N + 1);
        double pval = 1 - chi2cdf(H, k - 1);
        std::ostringstream ss;
        ss << "H=" << fmt(H) << ",  df=" << k - 1 << ",  p-value=" << fmt(pval)
           << "\n"
           << (pval < alpha ? "Reject H₀ (medians differ)" : "Fail to reject H₀");
        return ok(ss.str());
    }

    // =============================================================================
    // Regression
    // =============================================================================

    RegressionResult linearRegression(const Vec &x, const Vec &y)
    {
        RegressionResult r;
        if (x.size() != y.size() || x.size() < 2)
        {
            r.ok = false;
            r.error = "Need equal-length vectors";
            return r;
        }
        int n = x.size();
        double mx = std::stod(mean(x).value), my = std::stod(mean(y).value);
        double sxy = 0, sxx = 0;
        for (int i = 0; i < n; ++i)
        {
            sxy += (x[i] - mx) * (y[i] - my);
            sxx += (x[i] - mx) * (x[i] - mx);
        }
        double b1 = sxy / sxx, b0 = my - b1 * mx;
        r.coefficients = {b0, b1};
        double sse = 0, sst = 0;
        r.residuals.resize(n);
        for (int i = 0; i < n; ++i)
        {
            double yhat = b0 + b1 * x[i];
            r.residuals[i] = y[i] - yhat;
            sse += r.residuals[i] * r.residuals[i];
            sst += (y[i] - my) * (y[i] - my);
        }
        r.r2 = 1 - sse / sst;
        r.adjR2 = 1 - (1 - r.r2) * (n - 1) / (n - 2);
        r.rmse = std::sqrt(sse / (n - 2));
        std::ostringstream ss;
        ss << "ŷ = " << fmt(b0) << " + " << fmt(b1) << "x\n";
        ss << "R² = " << fmt(r.r2) << ",  adj R² = " << fmt(r.adjR2) << ",  RMSE = " << fmt(r.rmse);
        r.equation = fmt(b0) + " + " + fmt(b1) + "x";
        r.summary = ss.str();
        return r;
    }

    RegressionResult polynomialRegression(const Vec &x, const Vec &y, int deg)
    {
        RegressionResult r;
        int n = x.size();
        if ((int)y.size() != n || n < deg + 1)
        {
            r.ok = false;
            r.error = "Insufficient data";
            return r;
        }
        // Build Vandermonde-style design matrix, solve via normal equations
        std::vector<Vec> X(n, Vec(deg + 1));
        for (int i = 0; i < n; ++i)
            for (int j = 0; j <= deg; ++j)
                X[i][j] = std::pow(x[i], j);
        // XtX and Xty
        std::vector<Vec> XtX(deg + 1, Vec(deg + 1, 0));
        Vec Xty(deg + 1, 0);
        for (int i = 0; i < n; ++i)
        {
            for (int j = 0; j <= deg; ++j)
            {
                Xty[j] += X[i][j] * y[i];
                for (int k2 = 0; k2 <= deg; ++k2)
                    XtX[j][k2] += X[i][j] * X[i][k2];
            }
        }
        // Gaussian elimination on XtX
        int m = deg + 1;
        std::vector<Vec> aug(m, Vec(m + 1, 0));
        for (int i = 0; i < m; ++i)
        {
            for (int j = 0; j < m; ++j)
                aug[i][j] = XtX[i][j];
            aug[i][m] = Xty[i];
        }
        for (int col = 0; col < m; ++col)
        {
            int pivot = col;
            for (int row = col + 1; row < m; ++row)
                if (std::abs(aug[row][col]) > std::abs(aug[pivot][col]))
                    pivot = row;
            std::swap(aug[col], aug[pivot]);
            if (std::abs(aug[col][col]) < 1e-12)
            {
                r.ok = false;
                r.error = "Singular system";
                return r;
            }
            double d = aug[col][col];
            for (int j = 0; j <= m; ++j)
                aug[col][j] /= d;
            for (int row = 0; row < m; ++row)
            {
                if (row == col)
                    continue;
                double f = aug[row][col];
                for (int j = 0; j <= m; ++j)
                    aug[row][j] -= f * aug[col][j];
            }
        }
        r.coefficients.resize(m);
        for (int i = 0; i < m; ++i)
            r.coefficients[i] = aug[i][m];
        double my = std::stod(mean(y).value), sse = 0, sst = 0;
        r.residuals.resize(n);
        for (int i = 0; i < n; ++i)
        {
            double yhat = 0;
            for (int j = 0; j <= deg; ++j)
                yhat += r.coefficients[j] * std::pow(x[i], j);
            r.residuals[i] = y[i] - yhat;
            sse += r.residuals[i] * r.residuals[i];
            sst += (y[i] - my) * (y[i] - my);
        }
        r.r2 = 1 - sse / sst;
        r.adjR2 = 1 - (1 - r.r2) * (n - 1) / (n - deg - 1);
        r.rmse = std::sqrt(sse / (n - deg - 1));
        std::ostringstream eq;
        eq << fmt(r.coefficients[0]);
        for (int j = 1; j <= deg; ++j)
            eq << " + " << fmt(r.coefficients[j]) << "x^" << j;
        r.equation = eq.str();
        r.summary = "ŷ = " + r.equation + "\nR² = " + fmt(r.r2);
        return r;
    }

    RegressionResult logisticRegression(const std::vector<Vec> &X, const Vec &y, int maxIter)
    {
        RegressionResult r;
        int n = y.size(), p = X[0].size() + 1;
        Vec beta(p, 0.0);
        auto sigmoid = [](double z)
        { return 1.0 / (1 + std::exp(-z)); };
        for (int iter = 0; iter < maxIter; ++iter)
        {
            Vec grad(p, 0);
            std::vector<Vec> H(p, Vec(p, 0));
            double ll = 0;
            for (int i = 0; i < n; ++i)
            {
                double z = beta[0];
                for (int j = 0; j < p - 1; ++j)
                    z += beta[j + 1] * X[i][j];
                double pi = sigmoid(z);
                ll += y[i] * std::log(pi + 1e-15) + (1 - y[i]) * std::log(1 - pi + 1e-15);
                double w = pi * (1 - pi), e = y[i] - pi;
                grad[0] += e;
                for (int j = 0; j < p - 1; ++j)
                    grad[j + 1] += e * X[i][j];
                H[0][0] += w;
                for (int j = 0; j < p - 1; ++j)
                {
                    H[0][j + 1] += w * X[i][j];
                    H[j + 1][0] += w * X[i][j];
                    for (int k2 = 0; k2 < p - 1; ++k2)
                        H[j + 1][k2 + 1] += w * X[i][j] * X[i][k2];
                }
            }
            // Newton step: beta += H^{-1} grad (simplified: gradient ascent)
            double lr = 0.1;
            for (int j = 0; j < p; ++j)
                beta[j] += lr * grad[j] / (n + 1e-8);
        }
        r.coefficients = beta;
        std::ostringstream ss;
        ss << "Logistic regression (intercept + " << p - 1 << " features):\n";
        ss << "β₀=" << fmt(beta[0]);
        for (int j = 1; j < p; ++j)
            ss << ",  β" << j << "=" << fmt(beta[j]);
        r.summary = ss.str();
        return r;
    }

    RegressionResult multipleRegression(const std::vector<Vec> &X, const Vec &y)
    {
        // Build design matrix with intercept
        int n = y.size(), k = X[0].size();
        std::vector<Vec> Xd(n, Vec(k + 1));
        for (int i = 0; i < n; ++i)
        {
            Xd[i][0] = 1;
            for (int j = 0; j < k; ++j)
                Xd[i][j + 1] = X[i][j];
        }
        // Normal equations (reuse polynomial regression logic)
        Vec allX;
        for (auto &row : Xd)
            for (double v : row)
                allX.push_back(v);
        // Build XtX
        int m = k + 1;
        std::vector<Vec> XtX(m, Vec(m, 0));
        Vec Xty(m, 0);
        for (int i = 0; i < n; ++i)
        {
            for (int j = 0; j < m; ++j)
            {
                Xty[j] += Xd[i][j] * y[i];
                for (int l = 0; l < m; ++l)
                    XtX[j][l] += Xd[i][j] * Xd[i][l];
            }
        }
        std::vector<Vec> aug(m, Vec(m + 1, 0));
        for (int i = 0; i < m; ++i)
        {
            for (int j = 0; j < m; ++j)
                aug[i][j] = XtX[i][j];
            aug[i][m] = Xty[i];
        }
        for (int col = 0; col < m; ++col)
        {
            int pivot = col;
            for (int row = col + 1; row < m; ++row)
                if (std::abs(aug[row][col]) > std::abs(aug[pivot][col]))
                    pivot = row;
            std::swap(aug[col], aug[pivot]);
            if (std::abs(aug[col][col]) < 1e-12)
            {
                RegressionResult r;
                r.ok = false;
                r.error = "Singular (multicollinearity?)";
                return r;
            }
            double d = aug[col][col];
            for (int j = 0; j <= m; ++j)
                aug[col][j] /= d;
            for (int row = 0; row < m; ++row)
            {
                if (row == col)
                    continue;
                double f = aug[row][col];
                for (int j = 0; j <= m; ++j)
                    aug[row][j] -= f * aug[col][j];
            }
        }
        RegressionResult r;
        r.coefficients.resize(m);
        for (int i = 0; i < m; ++i)
            r.coefficients[i] = aug[i][m];
        double my = std::stod(mean(y).value), sse = 0, sst = 0;
        r.residuals.resize(n);
        for (int i = 0; i < n; ++i)
        {
            double yhat = 0;
            for (int j = 0; j < m; ++j)
                yhat += r.coefficients[j] * Xd[i][j];
            r.residuals[i] = y[i] - yhat;
            sse += r.residuals[i] * r.residuals[i];
            sst += (y[i] - my) * (y[i] - my);
        }
        r.r2 = 1 - sse / sst;
        r.adjR2 = 1 - (1 - r.r2) * (n - 1) / (n - m);
        r.rmse = std::sqrt(sse / (n - m));
        std::ostringstream ss;
        ss << "Multiple regression: R²=" << fmt(r.r2) << ",  adj R²=" << fmt(r.adjR2);
        r.summary = ss.str();
        return r;
    }

    // =============================================================================
    // Bayesian
    // =============================================================================

    StatResult bayesTheorem(double prior, double likelihood, double marginal)
    {
        if (std::abs(marginal) < 1e-15)
            return err("Marginal probability cannot be zero");
        double posterior = prior * likelihood / marginal;
        std::ostringstream ss;
        ss << "P(H|E) = P(E|H)·P(H) / P(E)\n"
           << "       = " << fmt(likelihood) << " × " << fmt(prior) << " / " << fmt(marginal)
           << " = " << fmt(posterior);
        return ok(fmt(posterior), ss.str());
    }

    StatResult entropy(const Vec &probs)
    {
        double H = 0;
        for (double p : probs)
            if (p > 1e-15)
                H -= p * std::log2(p);
        return ok(fmt(H), "bits");
    }

    StatResult klDivergence(const Vec &P, const Vec &Q)
    {
        if (P.size() != Q.size())
            return err("P and Q must have equal length");
        double kl = 0;
        for (size_t i = 0; i < P.size(); ++i)
            if (P[i] > 1e-15)
            {
                if (Q[i] < 1e-15)
                    return err("Q has zero where P is nonzero");
                kl += P[i] * std::log(P[i] / Q[i]);
            }
        return ok(fmt(kl), "nats");
    }

    StatResult markovSteadyState(const std::vector<Vec> &T)
    {
        int n = T.size();
        // Power iteration: π = π T
        Vec pi(n, 1.0 / n);
        for (int iter = 0; iter < 1000; ++iter)
        {
            Vec next(n, 0);
            for (int j = 0; j < n; ++j)
                for (int i = 0; i < n; ++i)
                    next[j] += pi[i] * T[i][j];
            pi = next;
        }
        std::ostringstream ss;
        ss << "Steady-state distribution π:\n  [";
        for (int i = 0; i < n; ++i)
        {
            if (i)
                ss << ", ";
            ss << fmt(pi[i]);
        }
        ss << "]";
        return ok(ss.str());
    }

    // =============================================================================
    // Dispatch
    // =============================================================================

    static Vec parseVec(const std::string &s)
    {
        Vec v;
        std::string t = s;
        if (!t.empty() && t.front() == '[')
            t = t.substr(1, t.size() - 2);
        std::istringstream ss(t);
        std::string tok;
        while (std::getline(ss, tok, ','))
        {
            while (!tok.empty() && std::isspace(tok.front()))
                tok.erase(tok.begin());
            while (!tok.empty() && std::isspace(tok.back()))
                tok.pop_back();
            if (!tok.empty())
                try
                {
                    v.push_back(std::stod(tok));
                }
                catch (...)
                {
                }
        }
        return v;
    }

    static std::string getParam(const std::string &json, const std::string &key)
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
        if (json[pos] == '"')
        {
            ++pos;
            auto e = json.find('"', pos);
            return e == std::string::npos ? "" : json.substr(pos, e - pos);
        }
        // Handle array values: scan to matching ']'
        if (json[pos] == '[')
        {
            int depth = 0;
            auto e = pos;
            while (e < json.size())
            {
                if (json[e] == '[')
                    depth++;
                else if (json[e] == ']')
                {
                    depth--;
                    if (depth == 0)
                    {
                        ++e;
                        break;
                    }
                }
                ++e;
            }
            return json.substr(pos, e - pos);
        }
        auto e = pos;
        while (e < json.size() && json[e] != ',' && json[e] != '}')
            ++e;
        auto v = json.substr(pos, e - pos);
        while (!v.empty() && std::isspace(v.back()))
            v.pop_back();
        return v;
    }
    static double getNum(const std::string &j, const std::string &k, double d = 0.0)
    {
        auto v = getParam(j, k);
        if (v.empty())
            return d;
        try
        {
            return std::stod(v);
        }
        catch (...)
        {
            return d;
        }
    }
    static int getInt(const std::string &j, const std::string &k, int d = 1)
    {
        auto v = getParam(j, k);
        if (v.empty())
            return d;
        try
        {
            return std::stoi(v);
        }
        catch (...)
        {
            return d;
        }
    }

    StatResult dispatch(const std::string &op, const std::string &json, bool /*exact*/)
    {
        try
        {
            Vec x = parseVec(getParam(json, "x"));
            Vec y = parseVec(getParam(json, "y"));

            if (op == "mean")
                return mean(x);
            if (op == "median")
                return median(x);
            if (op == "mode")
                return mode(x);
            if (op == "variance")
                return variance(x);
            if (op == "stddev")
                return stddev(x);
            if (op == "skewness")
                return skewness(x);
            if (op == "kurtosis")
                return kurtosis(x);
            if (op == "iqr")
                return iqr(x);
            if (op == "five_number")
                return fiveNumber(x);
            if (op == "summarize")
                return summarize(x);
            if (op == "percentile")
                return percentile(x, getNum(json, "p", 50));

            if (op == "normal_pdf")
                return normalPDF(getNum(json, "x"), getNum(json, "mu"), getNum(json, "sigma", 1));
            if (op == "normal_cdf")
                return normalCDF(getNum(json, "x"), getNum(json, "mu"), getNum(json, "sigma", 1));
            if (op == "normal_qf")
                return normalQF(getNum(json, "p"), getNum(json, "mu"), getNum(json, "sigma", 1));
            if (op == "t_pdf")
                return tPDF(getNum(json, "x"), getNum(json, "df"));
            if (op == "t_cdf")
                return tCDF(getNum(json, "x"), getNum(json, "df"));
            if (op == "t_qf")
                return tQF(getNum(json, "p"), getNum(json, "df"));
            if (op == "chisq_pdf")
                return chiSqPDF(getNum(json, "x"), getNum(json, "df"));
            if (op == "chisq_cdf")
                return chiSqCDF(getNum(json, "x"), getNum(json, "df"));
            if (op == "chisq_qf")
                return chiSqQF(getNum(json, "p"), getNum(json, "df"));
            if (op == "f_pdf")
                return fDistPDF(getNum(json, "x"), getNum(json, "d1"), getNum(json, "d2"));
            if (op == "f_cdf")
                return fDistCDF(getNum(json, "x"), getNum(json, "d1"), getNum(json, "d2"));
            if (op == "exp_pdf")
                return expPDF(getNum(json, "x"), getNum(json, "lambda", 1));
            if (op == "exp_cdf")
                return expCDF(getNum(json, "x"), getNum(json, "lambda", 1));
            if (op == "binomial_pmf")
                return binomialPMF(getInt(json, "k"), getInt(json, "n"), getNum(json, "p"));
            if (op == "binomial_cdf")
                return binomialCDF(getInt(json, "k"), getInt(json, "n"), getNum(json, "p"));
            if (op == "poisson_pmf")
                return poissonPMF(getInt(json, "k"), getNum(json, "lambda", 1));
            if (op == "poisson_cdf")
                return poissonCDF(getInt(json, "k"), getNum(json, "lambda", 1));
            if (op == "geometric_pmf")
                return geometricPMF(getInt(json, "k"), getNum(json, "p"));

            if (op == "ci_mean_t")
                return ciMeanT(x, getNum(json, "alpha", 0.05));
            if (op == "ci_mean_z")
                return ciMeanZ(x, getNum(json, "alpha", 0.05));
            if (op == "ci_prop")
                return ciProportion(getInt(json, "k"), getInt(json, "n"), getNum(json, "alpha", 0.05));

            if (op == "z_test")
                return zTest(x, getNum(json, "mu0"), getNum(json, "sigma", 1), getNum(json, "alpha", 0.05));
            if (op == "t_test_one")
                return tTestOne(x, getNum(json, "mu0"), getNum(json, "alpha", 0.05));
            if (op == "t_test_two")
                return tTestTwo(x, y, getNum(json, "alpha", 0.05));
            if (op == "t_test_paired")
                return tTestPaired(x, y, getNum(json, "alpha", 0.05));
            if (op == "chi_sq_test")
            {
                Vec exp_ = parseVec(getParam(json, "expected"));
                return chiSqTest(x, exp_, getNum(json, "alpha", 0.05));
            }
            if (op == "f_test")
                return fTest(x, y, getNum(json, "alpha", 0.05));
            if (op == "pearson")
                return pearson(x, y);
            if (op == "spearman")
                return spearman(x, y);
            if (op == "kendall")
                return kendallTau(x, y);
            if (op == "covariance")
                return covariance(x, y);
            if (op == "mann_whitney")
                return mannWhitney(x, y, getNum(json, "alpha", 0.05));
            if (op == "wilcoxon")
                return wilcoxon(x, getNum(json, "mu0"), getNum(json, "alpha", 0.05));

            if (op == "linear_reg")
            {
                auto r = linearRegression(x, y);
                return {r.ok, r.summary, "", r.error};
            }
            if (op == "poly_reg")
            {
                auto r = polynomialRegression(x, y, getInt(json, "degree", 2));
                return {r.ok, r.summary, "", r.error};
            }

            if (op == "entropy")
                return entropy(x);
            if (op == "kl_div")
                return klDivergence(x, y);
            if (op == "bayes")
                return bayesTheorem(getNum(json, "prior"), getNum(json, "likelihood"), getNum(json, "marginal", 1));

            if (op == "chi_sq_indep")
            {
                std::vector<Vec> table;
                // Parse rows separated by ";" e.g. "[[1,2,3],[4,5,6]]"
                std::string raw = getParam(json, "table");
                // Simple row parser
                size_t pos = 0;
                while ((pos = raw.find('[', pos)) != std::string::npos)
                {
                    ++pos;
                    size_t end = raw.find(']', pos);
                    if (end == std::string::npos)
                        break;
                    Vec row;
                    std::istringstream rs(raw.substr(pos, end - pos));
                    std::string tok;
                    while (std::getline(rs, tok, ','))
                    {
                        try
                        {
                            row.push_back(std::stod(tok));
                        }
                        catch (...)
                        {
                        }
                    }
                    if (!row.empty())
                        table.push_back(row);
                    pos = end + 1;
                }
                if (table.empty())
                    return err("Provide table as [[r1c1,r1c2,...],[r2c1,...]]");
                return chiSqIndep(table, getNum(json, "alpha", 0.05));
            }
            if (op == "anova_one")
            {
                std::vector<Vec> groups;
                std::string raw = getParam(json, "groups");
                size_t pos = 0;
                while ((pos = raw.find('[', pos)) != std::string::npos)
                {
                    ++pos;
                    size_t end = raw.find(']', pos);
                    if (end == std::string::npos)
                        break;
                    Vec g;
                    std::istringstream rs(raw.substr(pos, end - pos));
                    std::string tok;
                    while (std::getline(rs, tok, ','))
                    {
                        try
                        {
                            g.push_back(std::stod(tok));
                        }
                        catch (...)
                        {
                        }
                    }
                    if (!g.empty())
                        groups.push_back(g);
                    pos = end + 1;
                }
                if (groups.empty())
                    return err("Provide groups as [[g1...],[g2...],...] ");
                return anovaOne(groups, getNum(json, "alpha", 0.05));
            }
            if (op == "kruskal_wallis")
            {
                std::vector<Vec> groups;
                std::string raw = getParam(json, "groups");
                size_t pos = 0;
                while ((pos = raw.find('[', pos)) != std::string::npos)
                {
                    ++pos;
                    size_t end = raw.find(']', pos);
                    if (end == std::string::npos)
                        break;
                    Vec g;
                    std::istringstream rs(raw.substr(pos, end - pos));
                    std::string tok;
                    while (std::getline(rs, tok, ','))
                    {
                        try
                        {
                            g.push_back(std::stod(tok));
                        }
                        catch (...)
                        {
                        }
                    }
                    if (!g.empty())
                        groups.push_back(g);
                    pos = end + 1;
                }
                return kruskalWallis(groups, getNum(json, "alpha", 0.05));
            }
            if (op == "markov_steady")
                return markovSteadyState(parseMatrix(getParam(json, "T")));
            if (op == "mutual_info")
                return mutualInfo(parseMatrix(getParam(json, "joint")));
            if (op == "multiple_reg")
            {
                // X columns separated by "|", y is "y" field
                Vec y = parseVec(getParam(json, "y"));
                std::vector<Vec> X;
                std::string raw = getParam(json, "X");
                size_t pos = 0;
                while ((pos = raw.find('[', pos)) != std::string::npos)
                {
                    ++pos;
                    size_t end = raw.find(']', pos);
                    if (end == std::string::npos)
                        break;
                    Vec col;
                    std::istringstream rs(raw.substr(pos, end - pos));
                    std::string tok;
                    while (std::getline(rs, tok, ','))
                    {
                        try
                        {
                            col.push_back(std::stod(tok));
                        }
                        catch (...)
                        {
                        }
                    }
                    if (!col.empty())
                        X.push_back(col);
                    pos = end + 1;
                }
                if (X.empty() || y.empty())
                    return err("Provide X as [[col1...],[col2...]] and y as [...]");
                // Transpose X from column-major to row-major
                int n = y.size(), k = X.size();
                std::vector<Vec> Xrows(n, Vec(k));
                for (int j = 0; j < k; ++j)
                    for (int i = 0; i < n; ++i)
                        if (i < (int)X[j].size())
                            Xrows[i][j] = X[j][i];
                auto r = multipleRegression(Xrows, y);
                return {r.ok, r.summary, r.summary, r.error};
            }

            // ── Ott-Longnecker extensions ──────────────────────────────────────────────

            // Sampling distributions
            if (op == "standard_error")
                return standardError(x);
            if (op == "bootstrap_ci")
                return bootstrapCI(x, getNum(json, "alpha", 0.05), getInt(json, "B", 2000));
            if (op == "sampling_prop")
                return samplingDistProp(getInt(json, "n", 30), getNum(json, "p", 0.5), getNum(json, "alpha", 0.05));
            if (op == "clt_demo")
                return cltDemo(x, getInt(json, "n", 30), getInt(json, "reps", 1000));

            // Experimental design
            if (op == "rand_block")
            {
                auto g = parseMatrix(getParam(json, "data"));
                return g.empty() ? err("No data") : randomisedBlock(g, getNum(json, "alpha", 0.05));
            }
            if (op == "anova_two")
            {
                // Parse 3D data: [[[cell reps]]] format is complex; use flattened
                auto g = parseMatrix(getParam(json, "data"));
                if (g.size() < 4)
                    return err("Two-way ANOVA needs 4+ cells");
                // Assume 2x2 design with g rows = cells, each row = reps
                std::vector<std::vector<Vec>> d3(2, std::vector<Vec>(2));
                d3[0][0] = g[0];
                d3[0][1] = g[1];
                d3[1][0] = g[2];
                d3[1][1] = g[3];
                return anovaTwoWay(d3, getNum(json, "alpha", 0.05));
            }
            if (op == "tukey")
            {
                auto g = parseMatrix(getParam(json, "groups"));
                return tukeyHSD(g, getNum(json, "alpha", 0.05));
            }
            if (op == "bonferroni")
            {
                auto g = parseMatrix(getParam(json, "groups"));
                return bonferroni(g, getNum(json, "alpha", 0.05));
            }
            if (op == "scheffe")
            {
                auto g = parseMatrix(getParam(json, "groups"));
                return scheffe(g, getNum(json, "alpha", 0.05));
            }

            // Regression diagnostics
            if (op == "reg_diagnostics")
            {
                auto Xm = parseMatrix(getParam(json, "X"));
                int n2 = y.size(), k2 = Xm.size();
                std::vector<Vec> Xrows(n2, Vec(k2));
                for (int j = 0; j < k2; ++j)
                    for (int i = 0; i < n2 && i < (int)Xm[j].size(); ++i)
                        Xrows[i][j] = Xm[j][i];
                auto d = regressionDiagnostics(Xrows, y);
                return {d.ok, d.summary, d.summary, d.error};
            }
            if (op == "vif")
            {
                auto Xm = parseMatrix(getParam(json, "X"));
                int n2 = y.size(), k2 = Xm.size();
                std::vector<Vec> Xrows(n2, Vec(k2));
                for (int j = 0; j < k2; ++j)
                    for (int i = 0; i < n2 && i < (int)Xm[j].size(); ++i)
                        Xrows[i][j] = Xm[j][i];
                return vif(Xrows, y);
            }
            if (op == "cooks_distance")
            {
                auto Xm = parseMatrix(getParam(json, "X"));
                int n2 = y.size(), k2 = Xm.size();
                std::vector<Vec> Xrows(n2, Vec(k2));
                for (int j = 0; j < k2; ++j)
                    for (int i = 0; i < n2 && i < (int)Xm[j].size(); ++i)
                        Xrows[i][j] = Xm[j][i];
                return cooksDistance(Xrows, y);
            }
            if (op == "durbin_watson")
                return durbinWatson(x);
            if (op == "stepwise")
            {
                auto Xm = parseMatrix(getParam(json, "X"));
                int n2 = y.size(), k2 = Xm.size();
                std::vector<Vec> Xrows(n2, Vec(k2));
                for (int j = 0; j < k2; ++j)
                    for (int i = 0; i < n2 && i < (int)Xm[j].size(); ++i)
                        Xrows[i][j] = Xm[j][i];
                return stepwiseSelection(Xrows, y, getParam(json, "method"));
            }

            // Categorical
            if (op == "fisher_exact")
                return fisherExact(getInt(json, "a"), getInt(json, "b"), getInt(json, "c"), getInt(json, "d"), getNum(json, "alpha", 0.05));
            if (op == "mcnemar")
                return mcnemar(getInt(json, "a"), getInt(json, "b"), getInt(json, "c"), getInt(json, "d"), getNum(json, "alpha", 0.05));
            if (op == "odds_ratio")
                return oddsRatio(getInt(json, "a"), getInt(json, "b"), getInt(json, "c"), getInt(json, "d"));
            if (op == "relative_risk")
                return relativeRisk(getInt(json, "a"), getInt(json, "b"), getInt(json, "c"), getInt(json, "d"));
            if (op == "risk_diff")
                return riskDifference(getInt(json, "a"), getInt(json, "b"), getInt(json, "c"), getInt(json, "d"));
            if (op == "nnt")
                return numberOfNeededToTreat(getInt(json, "a"), getInt(json, "b"), getInt(json, "c"), getInt(json, "d"));
            if (op == "phi")
                return phiCoefficient(getInt(json, "a"), getInt(json, "b"), getInt(json, "c"), getInt(json, "d"));
            if (op == "cramer_v")
            {
                auto t = parseMatrix(getParam(json, "table"));
                return cramerV(t);
            }

            // Extended non-parametric
            if (op == "sign_test")
                return signTest(x, getNum(json, "mu0", 0), getNum(json, "alpha", 0.05));
            if (op == "runs_test")
                return runsTest(x, getNum(json, "alpha", 0.05));
            if (op == "friedman")
            {
                auto g = parseMatrix(getParam(json, "blocks"));
                return friedmanTest(g, getNum(json, "alpha", 0.05));
            }
            if (op == "ks_test")
                return kolmogorovSmirnov(x, y, getNum(json, "alpha", 0.05));
            if (op == "anderson_darling")
                return andersonDarling(x, getNum(json, "alpha", 0.05));
            if (op == "shapiro_wilk")
                return shapiroWilk(x, getNum(json, "alpha", 0.05));

            // Survival
            if (op == "kaplan_meier")
            {
                std::vector<int> status;
                std::string sraw = getParam(json, "status");
                if (!sraw.empty() && sraw.front() == '[')
                    sraw = sraw.substr(1, sraw.size() - 2);
                std::istringstream ss2(sraw);
                std::string tok;
                while (std::getline(ss2, tok, ','))
                {
                    try
                    {
                        status.push_back(std::stoi(tok));
                    }
                    catch (...)
                    {
                    }
                }
                auto km = kaplanMeier(x, status);
                if (!km.ok)
                    return err(km.error);
                std::ostringstream out;
                out << "Kaplan-Meier Survival Estimate" << km.table;
                out << "Median survival: " << fmtOL(km.medianSurvival) << " ";
                out << "Total events: " << km.totalEvents;
                return {true, out.str(), out.str(), ""};
            }
            if (op == "log_rank")
            {
                Vec t2 = parseVec(getParam(json, "t2"));
                std::vector<int> s1, s2;
                {
                    std::string r = getParam(json, "status");
                    if (!r.empty() && r.front() == '[')
                        r = r.substr(1, r.size() - 2);
                    std::istringstream ss2(r);
                    std::string t;
                    while (std::getline(ss2, t, ','))
                    {
                        try
                        {
                            s1.push_back(std::stoi(t));
                        }
                        catch (...)
                        {
                        }
                    }
                }
                {
                    std::string r = getParam(json, "status2");
                    if (!r.empty() && r.front() == '[')
                        r = r.substr(1, r.size() - 2);
                    std::istringstream ss2(r);
                    std::string t;
                    while (std::getline(ss2, t, ','))
                    {
                        try
                        {
                            s2.push_back(std::stoi(t));
                        }
                        catch (...)
                        {
                        }
                    }
                }
                return logRankTest(x, s1, t2, s2);
            }
            if (op == "hazard_rate")
            {
                std::vector<int> status;
                std::string sraw = getParam(json, "status");
                if (!sraw.empty() && sraw.front() == '[')
                    sraw = sraw.substr(1, sraw.size() - 2);
                std::istringstream ss2(sraw);
                std::string tok;
                while (std::getline(ss2, tok, ','))
                {
                    try
                    {
                        status.push_back(std::stoi(tok));
                    }
                    catch (...)
                    {
                    }
                }
                return hazardRate(x, status, getNum(json, "bw", 0));
            }

            // Time series
            if (op == "moving_avg")
                return movingAverage(x, getInt(json, "window", 3));
            if (op == "exp_smooth")
                return exponentialSmoothing(x, getNum(json, "alpha", 0.2));
            if (op == "double_exp")
                return doubleExpSmoothing(x, getNum(json, "alpha", 0.2), getNum(json, "beta", 0.1));
            if (op == "acf")
                return acf(x, getInt(json, "maxlag", 20));
            if (op == "pacf")
                return pacf(x, getInt(json, "maxlag", 20));
            if (op == "ljung_box")
                return ljungBox(x, getInt(json, "lag", 10));
            if (op == "differencing")
                return differencing(x, getInt(json, "d", 1));
            if (op == "forecast_ses")
                return forecastSES(x, getNum(json, "alpha", 0.2), getInt(json, "h", 5));

            // Quality control
            if (op == "xbar_chart")
            {
                auto g = parseMatrix(getParam(json, "subgroups"));
                auto c = xbarChart(g);
                return {c.ok, c.summary, c.summary, ""};
            }
            if (op == "r_chart")
            {
                auto g = parseMatrix(getParam(json, "subgroups"));
                auto c = rChart(g);
                return {c.ok, c.summary, c.summary, ""};
            }
            if (op == "s_chart")
            {
                auto g = parseMatrix(getParam(json, "subgroups"));
                auto c = sChart(g);
                return {c.ok, c.summary, c.summary, ""};
            }
            if (op == "p_chart")
            {
                Vec def = parseVec(getParam(json, "defectives"));
                Vec ns = parseVec(getParam(json, "n"));
                auto c = pChart(def, ns);
                return {c.ok, c.summary, c.summary, ""};
            }
            if (op == "c_chart")
            {
                auto c = cChart(x);
                return {c.ok, c.summary, c.summary, ""};
            }
            if (op == "cusum")
            {
                auto c = cusum(x, getNum(json, "k", 0.5), getNum(json, "h", 4));
                return {c.ok, c.summary, c.summary, ""};
            }
            if (op == "process_cap")
                return processCap(x, getNum(json, "LSL"), getNum(json, "USL"));

            // Probability
            if (op == "cond_prob")
                return conditionalProb(getNum(json, "pAB"), getNum(json, "pB", 1));
            if (op == "total_prob")
            {
                Vec lk = parseVec(getParam(json, "likelihood"));
                return totalProbability(x, lk);
            }
            if (op == "bayes_ext")
            {
                Vec lk = parseVec(getParam(json, "likelihood"));
                return bayesExtended(x, lk, getInt(json, "event", 0));
            }
            if (op == "combinations")
                return combinations(getInt(json, "n"), getInt(json, "r"));
            if (op == "permutations")
                return permutations(getInt(json, "n"), getInt(json, "r"));
            if (op == "multinomial")
            {
                std::vector<int> counts;
                std::string r = getParam(json, "counts");
                if (!r.empty() && r.front() == '[')
                    r = r.substr(1, r.size() - 2);
                std::istringstream ss2(r);
                std::string tok;
                while (std::getline(ss2, tok, ','))
                {
                    try
                    {
                        counts.push_back(std::stoi(tok));
                    }
                    catch (...)
                    {
                    }
                }
                return multinomialCoeff(getInt(json, "n"), counts);
            }

            // Frequency table
            if (op == "freq_table")
            {
                auto ft = frequencyTable(x, getInt(json, "bins", 0));
                return {ft.ok, ft.format(), ft.format(), ft.error};
            }

            if (op == "np_chart")
            {
                auto c = npChart(x, getInt(json, "n", 10));
                return {c.ok, c.summary, c.summary, ""};
            }
            if (op == "chi_sq_indep2")
            {
                auto t = parseMatrix(getParam(json, "table"));
                if (t.empty())
                    return err("Provide table=[[r1c1,...],[r2c1,...]]");
                return chiSqIndep(t, getNum(json, "alpha", 0.05));
            }
            if (op == "factorial_2x2")
            {
                auto g = parseMatrix(getParam(json, "cells"));
                return factorial2x2(g, getNum(json, "alpha", 0.05));
            }
            if (op == "markov_absorb")
            {
                auto T = parseMatrix(getParam(json, "T"));
                std::vector<int> abs;
                std::string r = getParam(json, "absorbing");
                if (!r.empty() && r.front() == '[')
                    r = r.substr(1, r.size() - 2);
                std::istringstream ss2(r);
                std::string tok;
                while (std::getline(ss2, tok, ','))
                    try
                    {
                        abs.push_back(std::stoi(tok));
                    }
                    catch (...)
                    {
                    }
                return markovAbsorption(T, abs);
            }
            if (op == "naive_bayes")
            {
                auto Xm = parseMatrix(getParam(json, "X"));
                int nn = y.size(), k2 = Xm.size();
                std::vector<Vec> Xrows(nn, Vec(k2));
                for (int j = 0; j < k2; ++j)
                    for (int i = 0; i < nn && i < (int)Xm[j].size(); ++i)
                        Xrows[i][j] = Xm[j][i];
                std::vector<int> labs;
                std::string lr = getParam(json, "labels");
                if (!lr.empty() && lr.front() == '[')
                    lr = lr.substr(1, lr.size() - 2);
                std::istringstream ss2(lr);
                std::string tok;
                while (std::getline(ss2, tok, ','))
                    try
                    {
                        labs.push_back(std::stoi(tok));
                    }
                    catch (...)
                    {
                    }
                Vec newpt = parseVec(getParam(json, "point"));
                return naiveBayes(Xrows, labs, newpt);
            }
            if (op == "mutual_info")
                return mutualInfo(parseMatrix(getParam(json, "joint")));

            return err("Unknown statistics operation: " + op);
        }
        catch (const std::exception &e)
        {
            return err(e.what());
        }
    }

} // namespace Statistics