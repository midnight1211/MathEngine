// Statistics_OttLongnecker.cpp
// Ott & Longnecker — "An Introduction to Statistical Methods and Data Analysis" 7th ed.
// All new functions declared in Statistics.h (Ott-Longnecker section).
// Compiled as part of the Statistics module — included via the build system.

#include "Statistics_internal.hpp"
#include "Statistics.hpp"
#include <cmath>
#include <algorithm>
#include <numeric>
#include <sstream>
#include <iomanip>
#include <stdexcept>
#include <map>
#include <set>
#include <functional>

namespace Statistics
{

    // Re-use helpers from Statistics.cpp (defined there as static, redeclare locally)
    static double PI_OL = M_PI;

    // fmtOL defined in Statistics.cpp via Statistics_internal.h
    static StatResult okOL(const std::string &val, const std::string &det = "")
    {
        return {true, val, det, ""};
    }
    static StatResult errOL(const std::string &m) { return {false, "", "", m}; }

// Internal helpers (defined in Statistics.cpp)
#include "Statistics_internal.hpp"

    // =============================================================================
    // FREQUENCY TABLES (Ch. 2)
    // =============================================================================

    std::string FrequencyTable::format() const
    {
        std::ostringstream ss;
        ss << std::left << std::setw(16) << "Class"
           << std::setw(10) << "Freq"
           << std::setw(12) << "Rel.Freq"
           << std::setw(12) << "Cum.Freq" << "\n";
        ss << std::string(50, '-') << "\n";
        for (size_t i = 0; i < classes.size(); ++i)
        {
            ss << std::setw(16) << classes[i]
               << std::setw(10) << frequency[i]
               << std::setw(12) << std::fixed << std::setprecision(4) << relFrequency[i]
               << std::setw(12) << cumFrequency[i] << "\n";
        }
        return ss.str();
    }

    FrequencyTable frequencyTable(const Vec &x, int bins)
    {
        FrequencyTable ft;
        if (x.empty())
        {
            ft.ok = false;
            ft.error = "Empty data";
            return ft;
        }
        Vec s = x;
        std::sort(s.begin(), s.end());
        double xmin = s.front(), xmax = s.back();
        if (bins <= 0)
            bins = (int)std::ceil(std::sqrt(x.size())); // Sturges-ish
        if (bins < 1)
            bins = 1;
        double width = (xmax - xmin) / bins;
        if (width == 0)
            width = 1;

        ft.frequency.resize(bins, 0);
        ft.classes.resize(bins);
        for (int i = 0; i < bins; ++i)
        {
            double lo = xmin + i * width, hi = xmin + (i + 1) * width;
            ft.classes[i] = "[" + fmtOL(lo, 4) + ", " + fmtOL(hi, 4) + (i == bins - 1 ? "]" : ")");
        }
        for (double v : x)
        {
            int b = (int)((v - xmin) / width);
            b = std::max(0, std::min(bins - 1, b));
            ft.frequency[b]++;
        }
        double n = x.size();
        double cum = 0;
        ft.relFrequency.resize(bins);
        ft.cumFrequency.resize(bins);
        for (int i = 0; i < bins; ++i)
        {
            ft.relFrequency[i] = ft.frequency[i] / n;
            cum += ft.relFrequency[i];
            ft.cumFrequency[i] = cum;
        }
        return ft;
    }

    FrequencyTable freqTableCat(const std::vector<std::string> &cats)
    {
        FrequencyTable ft;
        std::map<std::string, double> counts;
        for (auto &c : cats)
            counts[c]++;
        double n = cats.size();
        double cum = 0;
        for (auto &[k, v] : counts)
        {
            ft.classes.push_back(k);
            ft.frequency.push_back(v);
            ft.relFrequency.push_back(v / n);
            cum += v / n;
            ft.cumFrequency.push_back(cum);
        }
        return ft;
    }

    // =============================================================================
    // SAMPLING DISTRIBUTIONS (Ch. 6)
    // =============================================================================

    StatResult standardError(const Vec &x)
    {
        if (x.size() < 2)
            return errOL("Need at least 2 observations");
        double n = x.size();
        double m = std::accumulate(x.begin(), x.end(), 0.0) / n;
        double s2 = 0;
        for (double v : x)
            s2 += (v - m) * (v - m);
        s2 /= (n - 1);
        double se = std::sqrt(s2 / n);
        std::ostringstream ss;
        ss << "SE = s/√n = " << fmtOL(std::sqrt(s2)) << "/√" << (int)n
           << " = " << fmtOL(se);
        return okOL(fmtOL(se), ss.str());
    }

    StatResult cltDemo(const Vec &population, int sampleSize, int reps)
    {
        if (population.size() < 2)
            return errOL("Population too small");
        double popMean = std::accumulate(population.begin(), population.end(), 0.0) / population.size();
        double popVar = 0;
        for (double v : population)
            popVar += (v - popMean) * (v - popMean);
        popVar /= population.size();
        double theoSE = std::sqrt(popVar / sampleSize);

        // Generate sample means
        Vec xbars;
        for (int r = 0; r < reps; ++r)
        {
            double sum = 0;
            for (int i = 0; i < sampleSize; ++i)
                sum += population[rand() % population.size()];
            xbars.push_back(sum / sampleSize);
        }
        double empMean = std::accumulate(xbars.begin(), xbars.end(), 0.0) / reps;
        double empVar = 0;
        for (double v : xbars)
            empVar += (v - empMean) * (v - empMean);
        empVar /= (reps - 1);

        std::ostringstream ss;
        ss << "Central Limit Theorem Demonstration\n\n";
        ss << "Population: μ=" << fmtOL(popMean) << ", σ²=" << fmtOL(popVar) << "\n";
        ss << "Sample size n=" << sampleSize << ", Repetitions=" << reps << "\n\n";
        ss << "Theoretical: E[X̄]=" << fmtOL(popMean) << ", SE=" << fmtOL(theoSE) << "\n";
        ss << "Empirical:   E[X̄]=" << fmtOL(empMean) << ", SE=" << fmtOL(std::sqrt(empVar)) << "\n\n";
        ss << "The sampling distribution of X̄ is approximately N("
           << fmtOL(popMean) << ", " << fmtOL(theoSE * theoSE) << ") by CLT";
        return okOL(ss.str());
    }

    StatResult bootstrapCI(const Vec &x, double alpha, int B)
    {
        if (x.empty())
            return errOL("Empty data");
        int n = x.size();
        double origMean = std::accumulate(x.begin(), x.end(), 0.0) / n;
        Vec bootMeans(B);
        for (int b = 0; b < B; ++b)
        {
            double sum = 0;
            for (int i = 0; i < n; ++i)
                sum += x[rand() % n];
            bootMeans[b] = sum / n;
        }
        std::sort(bootMeans.begin(), bootMeans.end());
        double lo = bootMeans[(int)(alpha / 2 * B)];
        double hi = bootMeans[(int)((1 - alpha / 2) * B)];
        std::ostringstream ss;
        ss << "Bootstrap " << (int)((1 - alpha) * 100) << "% CI: ["
           << fmtOL(lo) << ", " << fmtOL(hi) << "]\n";
        ss << "Point estimate: " << fmtOL(origMean) << "\n";
        ss << "B=" << B << " bootstrap samples";
        return okOL(fmtOL(lo) + " to " + fmtOL(hi), ss.str());
    }

    StatResult samplingDistProp(int n, double p, double alpha)
    {
        double mu = p, sigma = std::sqrt(p * (1 - p) / n);
        double z = norminv(1 - alpha / 2);
        std::ostringstream ss;
        ss << "Sampling distribution of p̂ ~ N(" << fmtOL(mu) << ", " << fmtOL(sigma * sigma) << ")\n";
        ss << "SE(p̂) = √(p(1-p)/n) = " << fmtOL(sigma) << "\n";
        ss << (1 - alpha) * 100 << "% CI: [" << fmtOL(mu - z * sigma) << ", " << fmtOL(mu + z * sigma) << "]";
        return okOL(ss.str());
    }

    // =============================================================================
    // EXPERIMENTAL DESIGN / ANOVA EXTENSIONS (Ch. 12-14)
    // =============================================================================

    StatResult randomisedBlock(const std::vector<Vec> &data, double alpha)
    {
        // data[i][j] = treatment i, block j
        int t = data.size(), b = data[0].size();
        double grandTotal = 0, N = t * b;
        for (auto &row : data)
            for (double v : row)
                grandTotal += v;
        double grandMean = grandTotal / N;

        Vec trtMeans(t, 0), blkMeans(b, 0);
        for (int i = 0; i < t; ++i)
            for (int j = 0; j < b; ++j)
            {
                trtMeans[i] += data[i][j];
                blkMeans[j] += data[i][j];
            }
        for (auto &v : trtMeans)
            v /= b;
        for (auto &v : blkMeans)
            v /= t;

        double SSTrt = 0, SSBlk = 0, SSE = 0, SST = 0;
        for (int i = 0; i < t; ++i)
            for (int j = 0; j < b; ++j)
            {
                SSTrt += b * (trtMeans[i] - grandMean) * (trtMeans[i] - grandMean) / b; // will divide again
                SSBlk += t * (blkMeans[j] - grandMean) * (blkMeans[j] - grandMean) / t;
                SSE += (data[i][j] - trtMeans[i] - blkMeans[j] + grandMean) * (data[i][j] - trtMeans[i] - blkMeans[j] + grandMean);
                SST += (data[i][j] - grandMean) * (data[i][j] - grandMean);
            }
        SSTrt = 0;
        for (int i = 0; i < t; ++i)
            SSTrt += b * (trtMeans[i] - grandMean) * (trtMeans[i] - grandMean);
        SSBlk = 0;
        for (int j = 0; j < b; ++j)
            SSBlk += t * (blkMeans[j] - grandMean) * (blkMeans[j] - grandMean);
        SSE = SST - SSTrt - SSBlk;

        double dfTrt = t - 1, dfBlk = b - 1, dfE = (t - 1) * (b - 1);
        double MSTrt = SSTrt / dfTrt, MSBlk = SSBlk / dfBlk, MSE = SSE / dfE;
        double Ftrt = MSTrt / MSE, Fblk = MSBlk / MSE;
        double pTrt = 1 - fcdf(Ftrt, dfTrt, dfE), pBlk = 1 - fcdf(Fblk, dfBlk, dfE);

        std::ostringstream ss;
        ss << "Randomised Complete Block Design ANOVA\n\n";
        ss << "Source      SS        df   MS        F       p\n";
        ss << std::string(55, '-') << "\n";
        ss << std::left << std::setw(12) << "Treatments"
           << std::setw(10) << fmtOL(SSTrt) << std::setw(5) << (int)dfTrt
           << std::setw(10) << fmtOL(MSTrt) << std::setw(8) << fmtOL(Ftrt)
           << fmtOL(pTrt) << "\n";
        ss << std::setw(12) << "Blocks"
           << std::setw(10) << fmtOL(SSBlk) << std::setw(5) << (int)dfBlk
           << std::setw(10) << fmtOL(MSBlk) << std::setw(8) << fmtOL(Fblk)
           << fmtOL(pBlk) << "\n";
        ss << std::setw(12) << "Error"
           << std::setw(10) << fmtOL(SSE) << std::setw(5) << (int)dfE
           << std::setw(10) << fmtOL(MSE) << "\n";
        ss << std::setw(12) << "Total"
           << std::setw(10) << fmtOL(SST) << std::setw(5) << (int)(N - 1) << "\n\n";
        ss << "Treatment effect: " << (pTrt < alpha ? "significant" : "not significant")
           << "  (p=" << fmtOL(pTrt) << ")\n";
        ss << "Block effect:     " << (pBlk < alpha ? "significant" : "not significant")
           << "  (p=" << fmtOL(pBlk) << ")";
        return okOL(ss.str());
    }

    StatResult anovaTwoWay(const std::vector<std::vector<Vec>> &data, double alpha)
    {
        int a = data.size(), b = data[0].size();
        int n = data[0][0].size(); // reps per cell
        double N = a * b * n;
        double grandTotal = 0;
        for (auto &row : data)
            for (auto &cell : row)
                for (double v : cell)
                    grandTotal += v;
        double grandMean = grandTotal / N;

        Vec aMeans(a, 0), bMeans(b, 0);
        std::vector<Vec> cellMeans(a, Vec(b, 0));
        for (int i = 0; i < a; ++i)
            for (int j = 0; j < b; ++j)
            {
                for (double v : data[i][j])
                {
                    cellMeans[i][j] += v;
                    aMeans[i] += v;
                    bMeans[j] += v;
                }
                cellMeans[i][j] /= n;
            }
        for (auto &v : aMeans)
            v /= (b * n);
        for (auto &v : bMeans)
            v /= (a * n);

        double SSA = 0, SSB = 0, SSAB = 0, SSE = 0, SST = 0;
        for (int i = 0; i < a; ++i)
            SSA += b * n * (aMeans[i] - grandMean) * (aMeans[i] - grandMean);
        for (int j = 0; j < b; ++j)
            SSB += a * n * (bMeans[j] - grandMean) * (bMeans[j] - grandMean);
        for (int i = 0; i < a; ++i)
            for (int j = 0; j < b; ++j)
                SSAB += n * (cellMeans[i][j] - aMeans[i] - bMeans[j] + grandMean) * (cellMeans[i][j] - aMeans[i] - bMeans[j] + grandMean);
        for (int i = 0; i < a; ++i)
            for (int j = 0; j < b; ++j)
                for (double v : data[i][j])
                {
                    SSE += (v - cellMeans[i][j]) * (v - cellMeans[i][j]);
                    SST += (v - grandMean) * (v - grandMean);
                }
        double dfA = a - 1, dfB = b - 1, dfAB = (a - 1) * (b - 1), dfE = a * b * (n - 1);
        double MSA = SSA / dfA, MSB = SSB / dfB, MSAB = SSAB / dfAB, MSE = SSE / dfE;
        double FA = MSA / MSE, FB = MSB / MSE, FAB = MSAB / MSE;
        double pA = 1 - fcdf(FA, dfA, dfE), pB = 1 - fcdf(FB, dfB, dfE), pAB = 1 - fcdf(FAB, dfAB, dfE);

        std::ostringstream ss;
        ss << "Two-Way ANOVA (" << a << "x" << b << " design, n=" << n << " per cell)\n\n";
        ss << "Source       SS        df   MS        F       p\n"
           << std::string(55, '-') << "\n";
        auto row = [&](const std::string &name, double SS, double df, double MS, double F, double p)
        {
            ss << std::left << std::setw(13) << name << std::setw(10) << fmtOL(SS)
               << std::setw(5) << (int)df << std::setw(10) << fmtOL(MS) << std::setw(8) << fmtOL(F) << fmtOL(p) << "\n";
        };
        row("Factor A", SSA, dfA, MSA, FA, pA);
        row("Factor B", SSB, dfB, MSB, FB, pB);
        row("Interaction", SSAB, dfAB, MSAB, FAB, pAB);
        row("Error", SSE, dfE, MSE, 0, 0);
        ss << std::setw(13) << "Total" << std::setw(10) << fmtOL(SST) << (int)(N - 1) << "\n\n";
        ss << "Interaction: " << (pAB < alpha ? "significant" : "not significant") << " (p=" << fmtOL(pAB) << ")\n";
        ss << "Factor A:    " << (pA < alpha ? "significant" : "not significant") << " (p=" << fmtOL(pA) << ")\n";
        ss << "Factor B:    " << (pB < alpha ? "significant" : "not significant") << " (p=" << fmtOL(pB) << ")\n";
        return okOL(ss.str());
    }

    StatResult tukeyHSD(const std::vector<Vec> &groups, double alpha)
    {
        int k = groups.size();
        int N = 0;
        for (auto &g : groups)
            N += g.size();
        // Get MSE from one-way ANOVA
        double grandMean = 0;
        for (auto &g : groups)
            for (double v : g)
                grandMean += v;
        grandMean /= N;
        double SSE = 0;
        for (auto &g : groups)
        {
            double gm = 0;
            for (double v : g)
                gm += v;
            gm /= g.size();
            for (double v : g)
                SSE += (v - gm) * (v - gm);
        }
        double dfE = N - k;
        double MSE = SSE / dfE;

        // Studentised range quantile q_alpha(k, dfE)
        // Approximation using normal theory
        double q = norminv(1 - alpha) * std::sqrt(2.0); // rough approximation
        // Better: use chi-square scaling
        double HSD_base = q * std::sqrt(MSE);

        std::ostringstream ss;
        ss << "Tukey HSD Post-Hoc Test (α=" << alpha << ")\n";
        ss << "MSE=" << fmtOL(MSE) << ", dfE=" << (int)dfE << "\n\n";
        ss << "Pairwise comparisons (|ȳᵢ - ȳⱼ| vs HSD):\n\n";

        Vec gmeans(k);
        for (int i = 0; i < k; ++i)
        {
            for (double v : groups[i])
                gmeans[i] += v;
            gmeans[i] /= groups[i].size();
        }

        bool anyDiff = false;
        for (int i = 0; i < k - 1; ++i)
            for (int j = i + 1; j < k; ++j)
            {
                double diff = std::abs(gmeans[i] - gmeans[j]);
                double ni = groups[i].size(), nj = groups[j].size();
                double hsd = q * std::sqrt(MSE / 2 * (1.0 / ni + 1.0 / nj));
                bool sig = diff > hsd;
                if (sig)
                    anyDiff = true;
                ss << "  Group " << i + 1 << " vs Group " << j + 1
                   << ": |diff|=" << fmtOL(diff)
                   << ", HSD=" << fmtOL(hsd)
                   << (sig ? " *SIGNIFICANT*" : "") << "\n";
            }
        if (!anyDiff)
            ss << "\nNo significant differences found.";
        return okOL(ss.str());
    }

    StatResult bonferroni(const std::vector<Vec> &groups, double alpha)
    {
        int k = groups.size();
        int comparisons = k * (k - 1) / 2;
        double adjAlpha = alpha / comparisons;
        int N = 0;
        for (auto &g : groups)
            N += g.size();
        double SSE = 0;
        double grandMean = 0;
        for (auto &g : groups)
            for (double v : g)
                grandMean += v;
        grandMean /= N;
        for (auto &g : groups)
        {
            double gm = 0;
            for (double v : g)
                gm += v;
            gm /= g.size();
            for (double v : g)
                SSE += (v - gm) * (v - gm);
        }
        double MSE = SSE / (N - k);

        Vec gm(k);
        for (int i = 0; i < k; ++i)
        {
            for (double v : groups[i])
                gm[i] += v;
            gm[i] /= groups[i].size();
        }

        std::ostringstream ss;
        ss << "Bonferroni Correction: " << comparisons << " comparisons, "
           << "adjusted α = " << fmtOL(adjAlpha) << "\n\n";
        for (int i = 0; i < k - 1; ++i)
            for (int j = i + 1; j < k; ++j)
            {
                double diff = gm[i] - gm[j];
                double se = std::sqrt(MSE * (1.0 / groups[i].size() + 1.0 / groups[j].size()));
                double t = diff / se;
                double df = N - k;
                double pval = 2 * (1 - tcdf(std::abs(t), df));
                ss << "  G" << i + 1 << " vs G" << j + 1 << ": diff=" << fmtOL(diff)
                   << ", t=" << fmtOL(t) << ", p=" << fmtOL(pval)
                   << (pval < adjAlpha ? " *" : "") << "\n";
            }
        return okOL(ss.str());
    }

    StatResult scheffe(const std::vector<Vec> &groups, double alpha)
    {
        int k = groups.size(), N = 0;
        for (auto &g : groups)
            N += g.size();
        double SSE = 0;
        Vec gm(k, 0);
        for (int i = 0; i < k; ++i)
        {
            for (double v : groups[i])
                gm[i] += v;
            gm[i] /= groups[i].size();
        }
        for (int i = 0; i < k; ++i)
            for (double v : groups[i])
                SSE += (v - gm[i]) * (v - gm[i]);
        double MSE = SSE / (N - k);
        double Fcrit = (k - 1) * fcdf(alpha, k - 1, N - k); // approximation

        std::ostringstream ss;
        ss << "Scheffé Post-Hoc Test (α=" << alpha << ")\n";
        ss << "Critical F* = (k-1)·F_α(k-1, N-k) ≈ " << fmtOL((k - 1) * norminv(1 - alpha)) << "\n\n";
        for (int i = 0; i < k - 1; ++i)
            for (int j = i + 1; j < k; ++j)
            {
                double diff = gm[i] - gm[j];
                double se = std::sqrt(MSE * (1.0 / groups[i].size() + 1.0 / groups[j].size()));
                double F = (diff * diff) / ((k - 1) * MSE * (1.0 / groups[i].size() + 1.0 / groups[j].size()));
                double pval = 1 - fcdf(F, k - 1, N - k);
                ss << "  G" << i + 1 << " vs G" << j + 1 << ": F=" << fmtOL(F)
                   << ", p=" << fmtOL(pval) << (pval < alpha ? " *" : "") << "\n";
            }
        return okOL(ss.str());
    }

    // =============================================================================
    // REGRESSION DIAGNOSTICS (Ch. 11-12)
    // =============================================================================

    // Compute hat matrix diagonal h_ii and other diagnostics
    RegressionDiagnostics regressionDiagnostics(const std::vector<Vec> &X, const Vec &y)
    {
        RegressionDiagnostics d;
        int n = y.size(), p = X[0].size() + 1;
        if (n < p + 1)
        {
            d.ok = false;
            d.error = "Too few observations";
            return d;
        }

        // Build design matrix
        std::vector<Vec> Xd(n, Vec(p));
        for (int i = 0; i < n; ++i)
        {
            Xd[i][0] = 1;
            for (int j = 0; j < p - 1; ++j)
                Xd[i][j + 1] = X[i][j];
        }

        // Compute XtX and its inverse (Cholesky or Gauss-Jordan)
        std::vector<Vec> XtX(p, Vec(p, 0));
        Vec Xty(p, 0);
        for (int i = 0; i < n; ++i)
        {
            for (int j = 0; j < p; ++j)
            {
                Xty[j] += Xd[i][j] * y[i];
                for (int l = 0; l < p; ++l)
                    XtX[j][l] += Xd[i][j] * Xd[i][l];
            }
        }
        // Gauss-Jordan inversion
        std::vector<Vec> aug(p, Vec(2 * p, 0));
        for (int i = 0; i < p; ++i)
        {
            for (int j = 0; j < p; ++j)
                aug[i][j] = XtX[i][j];
            aug[i][p + i] = 1;
        }
        for (int col = 0; col < p; ++col)
        {
            int pivot = col;
            for (int row = col + 1; row < p; ++row)
                if (std::abs(aug[row][col]) > std::abs(aug[pivot][col]))
                    pivot = row;
            std::swap(aug[col], aug[pivot]);
            double dv = aug[col][col];
            if (std::abs(dv) < 1e-12)
            {
                d.ok = false;
                d.error = "Singular X'X";
                return d;
            }
            for (int j = 0; j < 2 * p; ++j)
                aug[col][j] /= dv;
            for (int row = 0; row < p; ++row)
            {
                if (row == col)
                    continue;
                double f = aug[row][col];
                for (int j = 0; j < 2 * p; ++j)
                    aug[row][j] -= f * aug[col][j];
            }
        }
        std::vector<Vec> XtXinv(p, Vec(p, 0));
        for (int i = 0; i < p; ++i)
            for (int j = 0; j < p; ++j)
                XtXinv[i][j] = aug[i][p + j];

        // Coefficients
        Vec beta(p, 0);
        for (int j = 0; j < p; ++j)
            for (int l = 0; l < p; ++l)
                beta[j] += XtXinv[j][l] * Xty[l];

        // Fitted values and residuals
        Vec yhat(n, 0), resid(n);
        for (int i = 0; i < n; ++i)
        {
            for (int j = 0; j < p; ++j)
                yhat[i] += Xd[i][j] * beta[j];
            resid[i] = y[i] - yhat[i];
        }
        double ymean = 0;
        for (double v : y)
            ymean += v;
        ymean /= n;
        double SSE = 0, SST = 0;
        for (int i = 0; i < n; ++i)
        {
            SSE += resid[i] * resid[i];
            SST += (y[i] - ymean) * (y[i] - ymean);
        }
        double MSE = SSE / (n - p);

        // Hat matrix diagonal h_ii = x_i' (X'X)^-1 x_i
        d.leverage.resize(n);
        for (int i = 0; i < n; ++i)
        {
            double h = 0;
            for (int j = 0; j < p; ++j)
                for (int l = 0; l < p; ++l)
                    h += Xd[i][j] * XtXinv[j][l] * Xd[i][l];
            d.leverage[i] = h;
        }

        // Standardised residuals
        d.standardizedResid.resize(n);
        d.studentizedResid.resize(n);
        for (int i = 0; i < n; ++i)
        {
            double se_i = std::sqrt(MSE * (1 - d.leverage[i]));
            d.standardizedResid[i] = resid[i] / std::sqrt(MSE);
            d.studentizedResid[i] = (se_i > 1e-12) ? resid[i] / se_i : 0.0;
        }

        // Cook's distance: D_i = e_i^2 h_i / (p * MSE * (1-h_i)^2)
        d.cooksD.resize(n);
        for (int i = 0; i < n; ++i)
        {
            double hi = d.leverage[i];
            d.cooksD[i] = resid[i] * resid[i] * hi / (p * MSE * (1 - hi) * (1 - hi));
        }

        // DFFITS
        d.dffits.resize(n);
        for (int i = 0; i < n; ++i)
        {
            double hi = d.leverage[i];
            d.dffits[i] = d.studentizedResid[i] * std::sqrt(hi / (1 - hi));
        }

        // Durbin-Watson: d = Σ(e_i-e_{i-1})^2 / Σe_i^2
        double num = 0, den = SSE;
        for (int i = 1; i < n; ++i)
            num += (resid[i] - resid[i - 1]) * (resid[i] - resid[i - 1]);
        d.durbinWatson = num / den;

        // VIF for each predictor (auxiliary regression R^2)
        d.vif.resize(p - 1);
        for (int j = 1; j < p; ++j)
        {
            // Regress Xd[][j] on all other columns
            Vec yj(n);
            for (int i = 0; i < n; ++i)
                yj[i] = Xd[i][j];
            std::vector<Vec> Xothers(n, Vec(p - 1));
            for (int i = 0; i < n; ++i)
            {
                int col = 0;
                for (int l = 0; l < p; ++l)
                {
                    if (l == j)
                        continue;
                    Xothers[i][col++] = Xd[i][l];
                }
            }
            // Compute R^2 for this auxiliary regression
            double ymj = 0;
            for (double v : yj)
                ymj += v;
            ymj /= n;
            double SSTj = 0;
            for (double v : yj)
                SSTj += (v - ymj) * (v - ymj);
            // Quick: sum of cross-products
            double r2 = 0;
            if (SSTj > 1e-12)
            {
                // Simple approximation: use correlation with first predictor
                Vec x1(n);
                for (int i = 0; i < n; ++i)
                    x1[i] = Xd[i][1];
                double mx1 = 0;
                for (double v : x1)
                    mx1 += v;
                mx1 /= n;
                double sx1 = 0, sxj = 0, sxy = 0;
                for (int i = 0; i < n; ++i)
                {
                    sx1 += (x1[i] - mx1) * (x1[i] - mx1);
                    sxj += (yj[i] - ymj) * (yj[i] - ymj);
                    sxy += (x1[i] - mx1) * (yj[i] - ymj);
                }
                if (sx1 > 1e-12 && sxj > 1e-12)
                    r2 = (sxy * sxy) / (sx1 * sxj);
            }
            d.vif[j - 1] = (r2 >= 1 - 1e-12) ? 1e9 : 1.0 / (1 - r2);
        }

        // Build summary
        std::ostringstream ss;
        ss << "Regression Diagnostics\n\n";
        ss << "Durbin-Watson: " << fmtOL(d.durbinWatson)
           << (d.durbinWatson < 1.5 ? " (possible positive autocorrelation)" : d.durbinWatson > 2.5 ? " (possible negative autocorrelation)"
                                                                                                    : " (OK)")
           << "\n\n";
        ss << "VIF (variance inflation factors):\n";
        for (size_t i = 0; i < d.vif.size(); ++i)
            ss << "  X" << (i + 1) << ": VIF=" << fmtOL(d.vif[i])
               << (d.vif[i] > 10 ? " HIGH multicollinearity" : "") << "\n";
        ss << "\nInfluential observations (Cook's D > 4/n = " << fmtOL(4.0 / n) << "):\n";
        bool any = false;
        for (int i = 0; i < n; ++i)
            if (d.cooksD[i] > 4.0 / n)
            {
                ss << "  obs " << i + 1 << ": D=" << fmtOL(d.cooksD[i])
                   << ", leverage=" << fmtOL(d.leverage[i]) << "\n";
                any = true;
            }
        if (!any)
            ss << "  None\n";
        ss << "\nHigh-leverage points (h > " << fmtOL(2.0 * p / n) << "):\n";
        any = false;
        for (int i = 0; i < n; ++i)
            if (d.leverage[i] > 2.0 * p / n)
            {
                ss << "  obs " << i + 1 << ": h=" << fmtOL(d.leverage[i]) << "\n";
                any = true;
            }
        if (!any)
            ss << "  None\n";
        d.summary = ss.str();
        return d;
    }

    StatResult vif(const std::vector<Vec> &X, const Vec &y)
    {
        auto d = regressionDiagnostics(X, y);
        if (!d.ok)
            return errOL(d.error);
        std::ostringstream ss;
        ss << "Variance Inflation Factors:\n";
        for (size_t i = 0; i < d.vif.size(); ++i)
            ss << "  X" << (i + 1) << ": VIF=" << fmtOL(d.vif[i])
               << (d.vif[i] > 10 ? " *** HIGH ***" : d.vif[i] > 5 ? " (moderate)"
                                                                  : "")
               << "\n";
        ss << "\n(VIF = 1: no multicollinearity; VIF > 10: severe)";
        return okOL(ss.str());
    }

    StatResult cooksDistance(const std::vector<Vec> &X, const Vec &y)
    {
        auto d = regressionDiagnostics(X, y);
        if (!d.ok)
            return errOL(d.error);
        double threshold = 4.0 / y.size();
        std::ostringstream ss;
        ss << "Cook's Distance (threshold 4/n = " << fmtOL(threshold) << "):\n";
        for (size_t i = 0; i < d.cooksD.size(); ++i)
            ss << "  obs " << (i + 1) << ": D=" << fmtOL(d.cooksD[i])
               << (d.cooksD[i] > threshold ? " *influential*" : "") << "\n";
        return okOL(ss.str());
    }

    StatResult durbinWatson(const Vec &residuals)
    {
        int n = residuals.size();
        if (n < 3)
            return errOL("Need at least 3 residuals");
        double num = 0, den = 0;
        for (int i = 1; i < n; ++i)
            num += (residuals[i] - residuals[i - 1]) * (residuals[i] - residuals[i - 1]);
        for (int i = 0; i < n; ++i)
            den += residuals[i] * residuals[i];
        double dw = num / den;
        std::ostringstream ss;
        ss << "Durbin-Watson statistic: d = " << fmtOL(dw) << "\n";
        ss << "Interpretation: d ≈ 2 → no autocorrelation\n";
        ss << "                d < 1.5 → positive autocorrelation\n";
        ss << "                d > 2.5 → negative autocorrelation\n";
        ss << "Result: " << (dw < 1.5 ? "positive autocorrelation detected" : dw > 2.5 ? "negative autocorrelation detected"
                                                                                       : "no significant autocorrelation");
        return okOL(fmtOL(dw), ss.str());
    }

    StatResult stepwiseSelection(const std::vector<Vec> &X, const Vec &y, const std::string &method)
    {
        int n = y.size(), p = X[0].size();
        double ymean = 0;
        for (double v : y)
            ymean += v;
        ymean /= n;
        double SST = 0;
        for (double v : y)
            SST += (v - ymean) * (v - ymean);

        std::ostringstream ss;
        ss << "Stepwise variable selection (" << method << ")\n\n";
        std::vector<bool> included(p, method == "backward");
        double bestAIC = 1e300;

        // AIC = n*ln(SSE/n) + 2*k
        auto computeAIC = [&](const std::vector<bool> &inc) -> double
        {
            std::vector<int> idx;
            for (int j = 0; j < p; ++j)
                if (inc[j])
                    idx.push_back(j);
            if (idx.empty())
                return 1e300;
            int k = idx.size() + 1;
            // Build reduced X
            std::vector<Vec> Xr(n, Vec(k));
            for (int i = 0; i < n; ++i)
            {
                Xr[i][0] = 1;
                for (int l = 0; l < (int)idx.size(); ++l)
                    Xr[i][l + 1] = X[i][idx[l]];
            }
            // OLS
            std::vector<Vec> XtX(k, Vec(k, 0));
            Vec Xty(k, 0);
            for (int i = 0; i < n; ++i)
                for (int j = 0; j < k; ++j)
                {
                    Xty[j] += Xr[i][j] * y[i];
                    for (int l = 0; l < k; ++l)
                        XtX[j][l] += Xr[i][j] * Xr[i][l];
                }
            // Solve XtX beta = Xty (Gauss)
            std::vector<Vec> aug(k, Vec(k + 1, 0));
            for (int i = 0; i < k; ++i)
            {
                for (int j = 0; j < k; ++j)
                    aug[i][j] = XtX[i][j];
                aug[i][k] = Xty[i];
            }
            for (int col = 0; col < k; ++col)
            {
                int piv = col;
                for (int r = col + 1; r < k; ++r)
                    if (std::abs(aug[r][col]) > std::abs(aug[piv][col]))
                        piv = r;
                std::swap(aug[col], aug[piv]);
                if (std::abs(aug[col][col]) < 1e-12)
                    return 1e300;
                double dv = aug[col][col];
                for (int j = 0; j <= k; ++j)
                    aug[col][j] /= dv;
                for (int r = 0; r < k; ++r)
                {
                    if (r == col)
                        continue;
                    double f = aug[r][col];
                    for (int j = 0; j <= k; ++j)
                        aug[r][j] -= f * aug[col][j];
                }
            }
            Vec beta(k);
            for (int j = 0; j < k; ++j)
                beta[j] = aug[j][k];
            double SSE = 0;
            for (int i = 0; i < n; ++i)
            {
                double yh = 0;
                for (int j = 0; j < k; ++j)
                    yh += Xr[i][j] * beta[j];
                SSE += (y[i] - yh) * (y[i] - yh);
            }
            return n * std::log(SSE / n) + 2.0 * k;
        };

        ss << "Var | AIC if added/removed\n"
           << std::string(30, '-') << "\n";
        for (int step = 0; step < p; ++step)
        {
            double bestDelta = 0;
            int bestJ = -1;
            for (int j = 0; j < p; ++j)
            {
                auto tryInc = included;
                if (method == "forward" && !included[j])
                    tryInc[j] = true;
                else if (method == "backward" && included[j])
                    tryInc[j] = false;
                else
                {
                    tryInc[j] = !included[j];
                }
                double aic = computeAIC(tryInc);
                if (j == 0 || aic < bestDelta + computeAIC(included) - 1e-9)
                {
                    bestDelta = aic - computeAIC(included);
                    bestJ = j;
                }
            }
            if (bestJ < 0 || bestDelta >= 0)
                break;
            included[bestJ] = (method == "forward") ? true : !included[bestJ];
            double aic = computeAIC(included);
            ss << "  Add/Remove X" << (bestJ + 1) << ": AIC=" << fmtOL(aic) << "\n";
        }
        ss << "\nFinal model includes:";
        for (int j = 0; j < p; ++j)
            if (included[j])
                ss << " X" << (j + 1);
        ss << "\nAIC = " << fmtOL(computeAIC(included));
        return okOL(ss.str());
    }

    // =============================================================================
    // CATEGORICAL DATA ANALYSIS (Ch. 15)
    // =============================================================================

    StatResult fisherExact(int a, int b, int c, int d, double alpha)
    {
        // 2x2 contingency: [[a,b],[c,d]]
        int n = a + b + c + d, r1 = a + b, r2 = c + d, c1 = a + c, c2 = b + d;
        // Exact p-value via hypergeometric
        auto lC = [](int n, int k) -> double
        {
            return std::lgamma(n + 1) - std::lgamma(k + 1) - std::lgamma(n - k + 1);
        };
        double logDenom = lC(n, c1);
        // Sum over all tables with same marginals where a' <= a (lower tail)
        double pLower = 0, pUpper = 0, pExact = 0;
        int aMin = std::max(0, c1 - r2), aMax = std::min(r1, c1);
        double pObs = std::exp(lC(r1, a) + lC(r2, c1 - a) - logDenom);
        for (int ai = aMin; ai <= aMax; ++ai)
        {
            double p = std::exp(lC(r1, ai) + lC(r2, c1 - ai) - logDenom);
            if (p <= pObs + 1e-12)
                pExact += p;
        }
        // Two-sided p-value
        double pTwo = std::min(1.0, pExact);

        std::ostringstream ss;
        ss << "Fisher's Exact Test\n";
        ss << "  |" << a << " " << b << "|\n";
        ss << "  |" << c << " " << d << "|\n\n";
        ss << "Odds ratio: " << fmtOL((double)a * d / (b * c)) << "\n";
        ss << "Two-sided p-value: " << fmtOL(pTwo) << "\n";
        ss << "Conclusion: " << (pTwo < alpha ? "reject H₀" : "fail to reject H₀")
           << " (α=" << alpha << ")";
        return okOL(fmtOL(pTwo), ss.str());
    }

    StatResult mcnemar(int a, int b, int c, int d, double alpha)
    {
        // [[a,b],[c,d]]: a=both+, b=+/-, c=-/+, d=both-
        double chi2 = (double)(b - c) * (b - c) / (b + c);
        double pval = 1 - chi2cdf(chi2, 1);
        std::ostringstream ss;
        ss << "McNemar's Test (paired proportions)\n";
        ss << "Discordant cells: b=" << b << ", c=" << c << "\n";
        ss << "χ² = (b-c)²/(b+c) = " << fmtOL(chi2) << "\n";
        ss << "p-value = " << fmtOL(pval) << "\n";
        ss << "Conclusion: " << (pval < alpha ? "reject H₀" : "fail to reject H₀");
        return okOL(fmtOL(pval), ss.str());
    }

    StatResult oddsRatio(int a, int b, int c, int d)
    {
        double OR = (double)a * d / (b * c);
        double lnOR = std::log(OR);
        double seLn = std::sqrt(1.0 / a + 1.0 / b + 1.0 / c + 1.0 / d);
        double lo = std::exp(lnOR - 1.96 * seLn), hi = std::exp(lnOR + 1.96 * seLn);
        std::ostringstream ss;
        ss << "Odds Ratio = " << fmtOL(OR) << "\n";
        ss << "95% CI: [" << fmtOL(lo) << ", " << fmtOL(hi) << "]\n";
        ss << "ln(OR) = " << fmtOL(lnOR) << ", SE = " << fmtOL(seLn) << "\n";
        ss << "Interpretation: ";
        if (OR > 1)
            ss << "exposure increases odds by factor " << fmtOL(OR);
        else if (OR < 1)
            ss << "exposure reduces odds (protective)";
        else
            ss << "no association";
        return okOL(fmtOL(OR), ss.str());
    }

    StatResult relativeRisk(int a, int b, int c, int d)
    {
        double p1 = (double)a / (a + b), p2 = (double)c / (c + d);
        double RR = p1 / p2;
        double lnRR = std::log(RR);
        double seLn = std::sqrt((1 - p1) / (a) + (1 - p2) / (c));
        double lo = std::exp(lnRR - 1.96 * seLn), hi = std::exp(lnRR + 1.96 * seLn);
        std::ostringstream ss;
        ss << "Relative Risk = " << fmtOL(RR) << "\n";
        ss << "p₁=" << fmtOL(p1) << " (exposed), p₂=" << fmtOL(p2) << " (unexposed)\n";
        ss << "95% CI: [" << fmtOL(lo) << ", " << fmtOL(hi) << "]\n";
        ss << "Interpretation: " << (RR > 1 ? "exposure increases risk by factor " + fmtOL(RR) : RR < 1 ? "exposure reduces risk (protective)"
                                                                                                        : "no association");
        return okOL(fmtOL(RR), ss.str());
    }

    StatResult riskDifference(int a, int b, int c, int d)
    {
        double p1 = (double)a / (a + b), p2 = (double)c / (c + d);
        double RD = p1 - p2;
        double se = std::sqrt(p1 * (1 - p1) / (a + b) + p2 * (1 - p2) / (c + d));
        double lo = RD - 1.96 * se, hi = RD + 1.96 * se;
        std::ostringstream ss;
        ss << "Risk Difference = " << fmtOL(RD) << "\n";
        ss << "95% CI: [" << fmtOL(lo) << ", " << fmtOL(hi) << "]\n";
        ss << "Attributable risk: " << fmtOL(std::abs(RD) * 100) << "%";
        return okOL(fmtOL(RD), ss.str());
    }

    StatResult numberOfNeededToTreat(int a, int b, int c, int d)
    {
        double p1 = (double)a / (a + b), p2 = (double)c / (c + d);
        double ARR = std::abs(p2 - p1);
        double NNT = ARR < 1e-10 ? std::numeric_limits<double>::infinity() : 1.0 / ARR;
        std::ostringstream ss;
        ss << "Number Needed to Treat (NNT) = " << fmtOL(NNT) << "\n";
        ss << "ARR = |p₂-p₁| = " << fmtOL(ARR) << "\n";
        ss << "Interpretation: treat " << (int)std::ceil(NNT)
           << " patients to prevent 1 outcome";
        return okOL(fmtOL(NNT), ss.str());
    }

    StatResult cramerV(const std::vector<Vec> &table)
    {
        int r = table.size(), c = table[0].size();
        double n = 0;
        for (auto &row : table)
            for (double v : row)
                n += v;
        Vec rowT(r, 0), colT(c, 0);
        for (int i = 0; i < r; ++i)
            for (int j = 0; j < c; ++j)
            {
                rowT[i] += table[i][j];
                colT[j] += table[i][j];
            }
        double chi2 = 0;
        for (int i = 0; i < r; ++i)
            for (int j = 0; j < c; ++j)
            {
                double E = rowT[i] * colT[j] / n;
                if (E > 0)
                    chi2 += (table[i][j] - E) * (table[i][j] - E) / E;
            }
        double V = std::sqrt(chi2 / (n * std::min(r - 1, c - 1)));
        std::ostringstream ss;
        ss << "Cramér's V = " << fmtOL(V) << "  (χ²=" << fmtOL(chi2) << ")\n";
        ss << "Association strength: " << (V < 0.1 ? "negligible" : V < 0.3 ? "weak"
                                                                : V < 0.5   ? "moderate"
                                                                            : "strong");
        return okOL(fmtOL(V), ss.str());
    }

    StatResult phiCoefficient(int a, int b, int c, int d)
    {
        double n = a + b + c + d;
        double phi = (a * d - b * c) / std::sqrt((double)(a + b) * (c + d) * (a + c) * (b + d));
        return okOL(fmtOL(phi), "φ = " + fmtOL(phi) +
                                    "\nInterpretation: " + (std::abs(phi) < 0.1 ? "negligible" : std::abs(phi) < 0.3 ? "weak"
                                                                                             : std::abs(phi) < 0.5   ? "moderate"
                                                                                                                     : "strong") +
                                    " association");
    }

    // =============================================================================
    // EXTENDED NON-PARAMETRIC (Ch. 16)
    // =============================================================================

    StatResult signTest(const Vec &x, double mu0, double alpha)
    {
        int nPlus = 0, nMinus = 0;
        for (double v : x)
        {
            if (v > mu0)
                nPlus++;
            else if (v < mu0)
                nMinus++;
        }
        int n = nPlus + nMinus;
        if (n == 0)
            return errOL("All values equal to mu0");
        // Two-sided exact binomial p-value
        int B = std::min(nPlus, nMinus);
        double pval = 0;
        for (int k = 0; k <= B; ++k)
        {
            double lp = std::lgamma(n + 1) - std::lgamma(k + 1) - std::lgamma(n - k + 1) - n * std::log(2);
            pval += std::exp(lp);
        }
        pval *= 2;
        std::ostringstream ss;
        ss << "Sign Test: n+=" << nPlus << ", n-=" << nMinus << ", n=" << n << "\n";
        ss << "Test statistic B = min(n+,n-) = " << B << "\n";
        ss << "Two-sided p-value = " << fmtOL(pval) << "\n";
        ss << (pval < alpha ? "Reject H₀" : "Fail to reject H₀");
        return okOL(fmtOL(pval), ss.str());
    }

    StatResult runsTest(const Vec &x, double alpha)
    {
        if (x.size() < 2)
            return errOL("Need at least 2 observations");
        double med = x[x.size() / 2]; // use median as threshold
        Vec s = x;
        std::sort(s.begin(), s.end());
        med = s[s.size() / 2];
        int n1 = 0, n2 = 0, R = 1;
        char prev = x[0] > med ? '+' : '-';
        for (int i = 0; i < (int)x.size(); ++i)
        {
            char cur = x[i] > med ? '+' : '-';
            if (cur == '+')
                n1++;
            else
                n2++;
            if (i > 0 && cur != prev)
                R++;
            prev = cur;
        }
        int n = n1 + n2;
        double mu_R = 2.0 * n1 * n2 / n + 1;
        double s2_R = 2.0 * n1 * n2 * (2.0 * n1 * n2 - n) / (n * n * (n - 1));
        double z = (R - mu_R) / std::sqrt(s2_R);
        double pval = 2 * (1 - normcdf(std::abs(z)));
        std::ostringstream ss;
        ss << "Runs Test: R=" << R << ", n+=" << n1 << ", n-=" << n2 << "\n";
        ss << "E[R]=" << fmtOL(mu_R) << ", Var[R]=" << fmtOL(s2_R) << "\n";
        ss << "z=" << fmtOL(z) << ", p=" << fmtOL(pval) << "\n";
        ss << (pval < alpha ? "Reject H₀ (non-random)" : "Fail to reject H₀ (random)");
        return okOL(fmtOL(pval), ss.str());
    }

    StatResult friedmanTest(const std::vector<Vec> &blocks, double alpha)
    {
        // blocks[i][j] = block i, treatment j
        int b = blocks.size(), k = blocks[0].size();
        Vec rankSum(k, 0);
        for (int i = 0; i < b; ++i)
        {
            // Rank within block
            Vec r(k);
            std::iota(r.begin(), r.end(), 0);
            std::sort(r.begin(), r.end(), [&](int a, int bb)
                      { return blocks[i][a] < blocks[i][bb]; });
            for (int j = 0; j < k; ++j)
                rankSum[r[j]] += j + 1;
        }
        double chi2 = 12.0 / (b * k * (k + 1));
        for (double rs : rankSum)
            chi2 += rs * rs;
        chi2 *= 12.0 / (b * k * (k + 1));
        chi2 -= 3 * b * (k + 1);
        double pval = 1 - chi2cdf(chi2, k - 1);
        std::ostringstream ss;
        ss << "Friedman Test: b=" << b << " blocks, k=" << k << " treatments\n";
        ss << "Rank sums:";
        for (int j = 0; j < k; ++j)
            ss << " T" << j + 1 << "=" << rankSum[j];
        ss << "\nχ²_r=" << fmtOL(chi2) << ", df=" << k - 1 << ", p=" << fmtOL(pval) << "\n";
        ss << (pval < alpha ? "Reject H₀ (treatment effects differ)" : "Fail to reject H₀");
        return okOL(fmtOL(pval), ss.str());
    }

    StatResult kolmogorovSmirnov(const Vec &x, const Vec &y, double alpha)
    {
        Vec sx = x, sy = y;
        std::sort(sx.begin(), sx.end());
        std::sort(sy.begin(), sy.end());
        int n = x.size(), m = y.size();
        double D = 0;
        for (double v : sx)
        {
            double Fn = (double)(std::upper_bound(sx.begin(), sx.end(), v) - sx.begin()) / n;
            double Gm = (double)(std::upper_bound(sy.begin(), sy.end(), v) - sy.begin()) / m;
            D = std::max(D, std::abs(Fn - Gm));
        }
        for (double v : sy)
        {
            double Fn = (double)(std::upper_bound(sx.begin(), sx.end(), v) - sx.begin()) / n;
            double Gm = (double)(std::upper_bound(sy.begin(), sy.end(), v) - sy.begin()) / m;
            D = std::max(D, std::abs(Fn - Gm));
        }
        // Asymptotic p-value
        double t = D * std::sqrt((double)n * m / (n + m));
        double pval = 2 * std::exp(-2 * t * t);
        std::ostringstream ss;
        ss << "Two-sample K-S Test: D=" << fmtOL(D) << ", n=" << n << ", m=" << m << "\n";
        ss << "p-value ≈ " << fmtOL(pval) << "\n";
        ss << (pval < alpha ? "Reject H₀ (distributions differ)" : "Fail to reject H₀");
        return okOL(fmtOL(D), ss.str());
    }

    StatResult shapiroWilk(const Vec &x, double alpha)
    {
        // Approximation using regression on normal order statistics
        int n = x.size();
        if (n < 3 || n > 5000)
            return errOL("n must be between 3 and 5000");
        Vec sx = x;
        std::sort(sx.begin(), sx.end());
        // Expected normal order statistics (approximation)
        Vec m(n);
        for (int i = 0; i < n; ++i)
            m[i] = norminv((i + 1 - 0.375) / (n + 0.25));
        double mnorm = 0;
        for (double v : m)
            mnorm += v;
        mnorm /= n;
        double cm2 = 0;
        for (double v : m)
            cm2 += (v - mnorm) * (v - mnorm);
        double b = 0;
        for (int i = 0; i < n / 2; ++i)
            b += (m[n - 1 - i] - m[i]) * (sx[n - 1 - i] - sx[i]);
        b /= std::sqrt(cm2);
        double xmean = 0;
        for (double v : sx)
            xmean += v;
        xmean /= n;
        double SSx = 0;
        for (double v : sx)
            SSx += (v - xmean) * (v - xmean);
        double W = b * b / SSx;
        // Approximate p-value using log-normal transformation
        double mu = -1.2725 + 1.0521 * std::log((n - 3) / 4.0 + 0.1);
        double sigma = 1.0308 - 0.1980 * std::log(n);
        double z = (std::log(1 - W) - mu) / sigma;
        double pval = 1 - normcdf(z);
        std::ostringstream ss;
        ss << "Shapiro-Wilk Test: W=" << fmtOL(W) << "\n";
        ss << "Approximate p-value=" << fmtOL(pval) << "\n";
        ss << (pval < alpha ? "Reject H₀ (non-normal)" : "Fail to reject H₀ (consistent with normality)");
        return okOL(fmtOL(W), ss.str());
    }

    StatResult andersonDarling(const Vec &x, double alpha)
    {
        Vec sx = x;
        std::sort(sx.begin(), sx.end());
        int n = sx.size();
        double xm = 0, s = 0;
        for (double v : sx)
            xm += v;
        xm /= n;
        for (double v : sx)
            s += (v - xm) * (v - xm);
        s = std::sqrt(s / (n - 1));
        double A2 = 0;
        for (int i = 0; i < n; ++i)
        {
            double z = (sx[i] - xm) / s;
            double Phi = normcdf(z), Phi2 = normcdf((sx[n - 1 - i] - xm) / s);
            if (Phi < 1e-14)
                Phi = 1e-14;
            if (Phi > 1 - 1e-14)
                Phi = 1 - 1e-14;
            if (Phi2 < 1e-14)
                Phi2 = 1e-14;
            if (Phi2 > 1 - 1e-14)
                Phi2 = 1 - 1e-14;
            A2 += (2 * (i + 1) - 1) * (std::log(Phi) + std::log(1 - Phi2));
        }
        A2 = -n - A2 / n;
        // Adjust for estimated parameters
        double A2star = A2 * (1 + 0.75 / n + 2.25 / (n * n));
        double pval;
        if (A2star < 0.2)
            pval = 1 - std::exp(-13.436 + 101.14 * A2star - 223.73 * A2star * A2star);
        else if (A2star < 0.34)
            pval = 1 - std::exp(-8.318 + 42.796 * A2star - 59.938 * A2star * A2star);
        else if (A2star < 0.6)
            pval = std::exp(0.9177 - 4.279 * A2star - 1.38 * A2star * A2star);
        else
            pval = std::exp(1.2937 - 5.709 * A2star + 0.0186 * A2star * A2star);
        pval = std::max(0.0, std::min(1.0, pval));
        std::ostringstream ss;
        ss << "Anderson-Darling Test: A²=" << fmtOL(A2) << ", A²*=" << fmtOL(A2star) << "\n";
        ss << "p-value ≈ " << fmtOL(pval) << "\n";
        ss << (pval < alpha ? "Reject H₀ (non-normal)" : "Fail to reject H₀ (consistent with normality)");
        return okOL(fmtOL(A2star), ss.str());
    }

    // =============================================================================
    // SURVIVAL ANALYSIS (Ch. 17)
    // =============================================================================

    KaplanMeierResult kaplanMeier(const Vec &times, const std::vector<int> &status)
    {
        KaplanMeierResult r;
        if (times.size() != status.size())
        {
            r.ok = false;
            r.error = "times and status must match";
            return r;
        }
        int n = times.size();
        // Sort by time
        std::vector<int> idx(n);
        std::iota(idx.begin(), idx.end(), 0);
        std::sort(idx.begin(), idx.end(), [&](int a, int b)
                  { return times[a] < times[b]; });

        // Get unique event times
        std::vector<double> eventTimes;
        for (int i = 0; i < n; ++i)
            if (status[idx[i]] == 1)
            {
                if (eventTimes.empty() || times[idx[i]] != eventTimes.back())
                    eventTimes.push_back(times[idx[i]]);
            }

        double S = 1.0;
        int nRisk = n;
        double logVarSum = 0;
        std::ostringstream table;
        table << "Time   n.risk  n.event  survival  lower.95  upper.95\n";

        r.times.push_back(0);
        r.survival.push_back(1);
        r.lower.push_back(1);
        r.upper.push_back(1);
        r.nAtRisk.push_back(n);
        r.nEvents.push_back(0);
        r.nCensored.push_back(0);

        size_t ptr = 0;
        for (double t : eventTimes)
        {
            int nEvent = 0, nCensor = 0;
            while (ptr < (size_t)n && times[idx[ptr]] <= t)
            {
                if (status[idx[ptr]] == 1)
                    nEvent++;
                else
                    nCensor++;
                ptr++;
            }
            nRisk -= nCensor; // remove censored before this time
            double km_step = 1.0 - (double)nEvent / nRisk;
            S *= km_step;
            logVarSum += (double)nEvent / (nRisk * (nRisk - nEvent));
            double seLog = std::sqrt(logVarSum) / (std::abs(std::log(S)) + 1e-12);
            double lo = std::max(0.0, S * std::exp(-1.96 * seLog / S));
            double hi = std::min(1.0, S * std::exp(1.96 * seLog / S));
            r.times.push_back(t);
            r.survival.push_back(S);
            r.lower.push_back(lo);
            r.upper.push_back(hi);
            r.nAtRisk.push_back(nRisk);
            r.nEvents.push_back(nEvent);
            r.nCensored.push_back(nCensor);
            table << std::setw(7) << t << std::setw(8) << nRisk << std::setw(9) << nEvent
                  << std::setw(10) << std::setprecision(4) << S
                  << std::setw(10) << lo << std::setw(10) << hi << "\n";
            r.totalEvents += nEvent;
            nRisk -= nEvent;
            // Median survival: first t where S<=0.5
            if (r.medianSurvival == 0 && S <= 0.5)
                r.medianSurvival = t;
        }
        r.table = table.str();
        return r;
    }

    StatResult logRankTest(const Vec &t1, const std::vector<int> &s1,
                           const Vec &t2, const std::vector<int> &s2)
    {
        // Collect all unique event times
        std::vector<double> allTimes;
        for (size_t i = 0; i < t1.size(); ++i)
            if (s1[i] == 1)
                allTimes.push_back(t1[i]);
        for (size_t i = 0; i < t2.size(); ++i)
            if (s2[i] == 1)
                allTimes.push_back(t2[i]);
        std::sort(allTimes.begin(), allTimes.end());
        allTimes.erase(std::unique(allTimes.begin(), allTimes.end()), allTimes.end());

        double O1 = 0, E1 = 0, V = 0;
        int n1 = t1.size(), n2 = t2.size();

        for (double t : allTimes)
        {
            // Count at risk and events at time t
            int r1 = 0, r2 = 0, d1 = 0, d2 = 0;
            for (size_t i = 0; i < t1.size(); ++i)
                if (t1[i] >= t)
                    r1++;
            for (size_t i = 0; i < t2.size(); ++i)
                if (t2[i] >= t)
                    r2++;
            for (size_t i = 0; i < t1.size(); ++i)
                if (t1[i] == t && s1[i] == 1)
                    d1++;
            for (size_t i = 0; i < t2.size(); ++i)
                if (t2[i] == t && s2[i] == 1)
                    d2++;
            int r = r1 + r2, d = d1 + d2;
            if (r < 2)
                continue;
            O1 += d1;
            E1 += (double)d * r1 / r;
            V += (double)d * (r1) * (r2) * (r - d) / (r * r * (r - 1));
        }
        double chi2 = (O1 - E1) * (O1 - E1) / (V + 1e-15);
        double pval = 1 - chi2cdf(chi2, 1);
        std::ostringstream ss;
        ss << "Log-Rank Test\n";
        ss << "Group 1: O=" << fmtOL(O1) << ", E=" << fmtOL(E1) << "\n";
        ss << "Group 2: O=" << fmtOL(O1 + (t2.size() - E1 - (n1 - O1))) << "...\n";
        ss << "χ²=" << fmtOL(chi2) << ", df=1, p=" << fmtOL(pval) << "\n";
        ss << (pval < 0.05 ? "Reject H₀ (survival curves differ)" : "Fail to reject H₀");
        return okOL(fmtOL(pval), ss.str());
    }

    StatResult hazardRate(const Vec &times, const std::vector<int> &status, double bw)
    {
        auto km = kaplanMeier(times, status);
        if (!km.ok)
            return errOL(km.error);
        if (bw <= 0)
            bw = (times.back() - times.front()) / 10.0;
        std::ostringstream ss;
        ss << "Hazard Rate h(t) = -d/dt ln S(t)\n\n";
        ss << "Nelson-Aalen Cumulative Hazard Estimator:\n";
        ss << "  H(t) = Σ_{tᵢ≤t} dᵢ/nᵢ\n\n";
        double H = 0;
        for (size_t i = 1; i < km.times.size(); ++i)
        {
            if (km.nAtRisk[i] > 0 && km.nEvents[i] > 0)
            {
                H += km.nEvents[i] / km.nAtRisk[i];
                ss << "  t=" << km.times[i] << ": H(t)=" << fmtOL(H) << "\n";
            }
        }
        return okOL(ss.str());
    }

    // =============================================================================
    // TIME SERIES (Ch. 18)
    // =============================================================================

    StatResult movingAverage(const Vec &x, int w)
    {
        if (w <= 0 || w > (int)x.size())
            return errOL("Invalid window size");
        Vec ma(x.size() - w + 1);
        for (int i = 0; i < (int)ma.size(); ++i)
        {
            double s = 0;
            for (int j = 0; j < w; ++j)
                s += x[i + j];
            ma[i] = s / w;
        }
        std::ostringstream ss;
        ss << "Moving Average (window=" << w << ")\nSmoothed series: [";
        for (size_t i = 0; i < ma.size(); ++i)
        {
            if (i)
                ss << ",";
            ss << fmtOL(ma[i]);
        }
        ss << "]\nFirst value: " << fmtOL(ma.front()) << "\nLast value: " << fmtOL(ma.back());
        return okOL(ss.str());
    }

    StatResult exponentialSmoothing(const Vec &x, double alpha_es)
    {
        if (x.empty())
            return errOL("Empty series");
        Vec s(x.size());
        s[0] = x[0];
        for (size_t i = 1; i < x.size(); ++i)
            s[i] = alpha_es * x[i] + (1 - alpha_es) * s[i - 1];
        double forecast = alpha_es * x.back() + (1 - alpha_es) * s.back();
        std::ostringstream ss;
        ss << "Simple Exponential Smoothing (α=" << alpha_es << ")\n";
        ss << "Smoothed: [";
        for (size_t i = 0; i < s.size(); ++i)
        {
            if (i)
                ss << ",";
            ss << fmtOL(s[i], 4);
        }
        ss << "]\n1-step forecast: " << fmtOL(forecast);
        return okOL(fmtOL(forecast), ss.str());
    }

    StatResult doubleExpSmoothing(const Vec &x, double alpha_es, double beta_es)
    {
        if (x.size() < 2)
            return errOL("Need at least 2 observations");
        Vec S(x.size()), T(x.size());
        S[0] = x[0];
        T[0] = x[1] - x[0];
        for (size_t i = 1; i < x.size(); ++i)
        {
            S[i] = alpha_es * x[i] + (1 - alpha_es) * (S[i - 1] + T[i - 1]);
            T[i] = beta_es * (S[i] - S[i - 1]) + (1 - beta_es) * T[i - 1];
        }
        double f1 = S.back() + T.back(), f2 = S.back() + 2 * T.back(), f5 = S.back() + 5 * T.back();
        std::ostringstream ss;
        ss << "Holt Double Exponential Smoothing (α=" << alpha_es << ", β=" << beta_es << ")\n";
        ss << "Level S[n]=" << fmtOL(S.back()) << ", Trend T[n]=" << fmtOL(T.back()) << "\n";
        ss << "Forecasts: h=1: " << fmtOL(f1) << ", h=2: " << fmtOL(f2) << ", h=5: " << fmtOL(f5);
        return okOL(fmtOL(f1), ss.str());
    }

    StatResult acf(const Vec &x, int maxLag)
    {
        int n = x.size();
        double xm = 0;
        for (double v : x)
            xm += v;
        xm /= n;
        double s0 = 0;
        for (double v : x)
            s0 += (v - xm) * (v - xm);
        s0 /= n;
        std::ostringstream ss;
        ss << "Autocorrelation Function (ACF)\n\n";
        ss << "Lag  ACF      ±1.96/√n\n";
        double ci = 1.96 / std::sqrt(n);
        for (int k = 1; k <= std::min(maxLag, n - 1); ++k)
        {
            double rk = 0;
            for (int t = k; t < n; ++t)
                rk += (x[t] - xm) * (x[t - k] - xm);
            rk /= n * s0;
            ss << "  " << std::setw(3) << k << "  " << std::setw(8) << fmtOL(rk)
               << (std::abs(rk) > ci ? " *" : "") << "\n";
        }
        ss << "\n±" << fmtOL(ci) << " (95% significance bounds)";
        return okOL(ss.str());
    }

    StatResult pacf(const Vec &x, int maxLag)
    {
        int n = x.size();
        double xm = 0;
        for (double v : x)
            xm += v;
        xm /= n;
        double s0 = 0;
        for (double v : x)
            s0 += (v - xm) * (v - xm);
        s0 /= n;
        // Compute ACF values first
        Vec rho(maxLag + 1, 1);
        for (int k = 1; k <= maxLag && k < n; ++k)
        {
            double rk = 0;
            for (int t = k; t < n; ++t)
                rk += (x[t] - xm) * (x[t - k] - xm);
            rho[k] = rk / (n * s0);
        }
        // Durbin-Levinson recursion for PACF
        std::ostringstream ss;
        ss << "Partial Autocorrelation Function (PACF)\n\nLag  PACF\n";
        double ci = 1.96 / std::sqrt(n);
        Vec phi_prev(1, rho[1]);
        ss << "  1  " << fmtOL(rho[1]) << (std::abs(rho[1]) > ci ? " *" : "") << "\n";
        for (int k = 2; k <= std::min(maxLag, n - 1); ++k)
        {
            Vec phi(k);
            double num = rho[k];
            for (int j = 1; j < k; ++j)
                num -= phi_prev[j - 1] * rho[k - j];
            double den = 1;
            for (int j = 1; j < k; ++j)
                den -= phi_prev[j - 1] * rho[j];
            phi[k - 1] = (std::abs(den) > 1e-12) ? num / den : 0;
            for (int j = 1; j < k; ++j)
                phi[j - 1] = phi_prev[j - 1] - phi[k - 1] * phi_prev[k - 1 - j];
            ss << "  " << k << "  " << fmtOL(phi[k - 1]) << (std::abs(phi[k - 1]) > ci ? " *" : "") << "\n";
            phi_prev = phi;
        }
        return okOL(ss.str());
    }

    StatResult ljungBox(const Vec &x, int lag)
    {
        int n = x.size();
        double xm = 0;
        for (double v : x)
            xm += v;
        xm /= n;
        double s0 = 0;
        for (double v : x)
            s0 += (v - xm) * (v - xm);
        s0 /= n;
        double Q = 0;
        for (int k = 1; k <= lag && k < n; ++k)
        {
            double rk = 0;
            for (int t = k; t < n; ++t)
                rk += (x[t] - xm) * (x[t - k] - xm);
            rk /= n * s0;
            Q += rk * rk / (n - k);
        }
        Q *= n * (n + 2);
        double pval = 1 - chi2cdf(Q, lag);
        std::ostringstream ss;
        ss << "Ljung-Box Q-Test: Q=" << fmtOL(Q) << ", df=" << lag << ", p=" << fmtOL(pval) << "\n";
        ss << (pval < 0.05 ? "Series has significant autocorrelation" : "No significant autocorrelation detected");
        return okOL(fmtOL(Q), ss.str());
    }

    StatResult differencing(const Vec &x, int d)
    {
        Vec y = x;
        for (int i = 0; i < d; ++i)
        {
            Vec dy(y.size() - 1);
            for (size_t j = 0; j < dy.size(); ++j)
                dy[j] = y[j + 1] - y[j];
            y = dy;
        }
        std::ostringstream ss;
        ss << d << "-order differenced series (length " << y.size() << "): [";
        for (size_t i = 0; i < y.size() && i < 20; ++i)
        {
            if (i)
                ss << ",";
            ss << fmtOL(y[i]);
        }
        if (y.size() > 20)
            ss << "...";
        ss << "]";
        return okOL(ss.str());
    }

    StatResult forecastSES(const Vec &x, double alpha_es, int h)
    {
        Vec s = x;
        double sv = x[0];
        for (double v : x)
        {
            sv = alpha_es * v + (1 - alpha_es) * sv;
            s.push_back(sv);
        }
        std::ostringstream ss;
        ss << "SES Forecast (α=" << alpha_es << "), h=" << h << " steps ahead\n";
        ss << "Forecasts: ";
        for (int i = 1; i <= h; ++i)
            ss << "ŷ_{n+" << i << "}=" << fmtOL(sv) << " ";
        ss << "\n(SES forecasts are flat beyond h=1)";
        return okOL(fmtOL(sv), ss.str());
    }

    // =============================================================================
    // QUALITY CONTROL (Ch. 19)
    // =============================================================================

    // Control chart constants (from standard tables)
    static double A2(int n)
    { // X-bar chart using R
        static const double a[] = {0, 0, 1.880, 1.023, 0.729, 0.577, 0.483, 0.419, 0.373, 0.337, 0.308};
        return (n >= 2 && n <= 10) ? a[n] : 3.0 / std::sqrt(n);
    }
    static double D3(int n)
    {
        static const double d[] = {0, 0, 0, 0, 0, 0, 0, 0.076, 0.136, 0.184, 0.223};
        return (n >= 2 && n <= 10) ? d[n] : std::max(0.0, 1 - 3 * std::sqrt(2.0 / (n - 1)));
    }
    static double D4(int n)
    {
        static const double d[] = {0, 0, 3.267, 2.574, 2.282, 2.114, 2.004, 1.924, 1.864, 1.816, 1.777};
        return (n >= 2 && n <= 10) ? d[n] : 1 + 3 * std::sqrt(2.0 / (n - 1));
    }
    static double d2(int n)
    { // E[R/sigma]
        static const double d[] = {0, 0, 1.128, 1.693, 2.059, 2.326, 2.534, 2.704, 2.847, 2.970, 3.078};
        return (n >= 2 && n <= 10) ? d[n] : std::sqrt(2 * std::log(n));
    }
    static double c4(int n)
    {
        return std::sqrt(2.0 / (n - 1)) * std::exp(std::lgamma(n / 2.0) - std::lgamma((n - 1) / 2.0));
    }

    ControlChart xbarChart(const std::vector<Vec> &subgroups)
    {
        ControlChart cc;
        cc.chartType = "X-bar";
        int k = subgroups.size(), n = subgroups[0].size();
        Vec xbars(k), Rs(k);
        for (int i = 0; i < k; ++i)
        {
            xbars[i] = std::accumulate(subgroups[i].begin(), subgroups[i].end(), 0.0) / n;
            Rs[i] = *std::max_element(subgroups[i].begin(), subgroups[i].end()) -
                    *std::min_element(subgroups[i].begin(), subgroups[i].end());
        }
        cc.data = xbars;
        double xbarbar = std::accumulate(xbars.begin(), xbars.end(), 0.0) / k;
        double Rbar = std::accumulate(Rs.begin(), Rs.end(), 0.0) / k;
        cc.centreLine = xbarbar;
        cc.UCL = xbarbar + A2(n) * Rbar;
        cc.LCL = xbarbar - A2(n) * Rbar;
        cc.violations.resize(k, false);
        for (int i = 0; i < k; ++i)
            cc.violations[i] = (xbars[i] > cc.UCL || xbars[i] < cc.LCL);
        std::ostringstream ss;
        ss << "X-bar Chart (k=" << k << " subgroups, n=" << n << ")\n";
        ss << "X̄̄=" << fmtOL(xbarbar) << ", R̄=" << fmtOL(Rbar) << "\n";
        ss << "UCL=" << fmtOL(cc.UCL) << ", CL=" << fmtOL(cc.centreLine) << ", LCL=" << fmtOL(cc.LCL) << "\n";
        int vcount = 0;
        for (bool v : cc.violations)
            vcount += v;
        ss << "Out-of-control points: " << vcount;
        cc.summary = ss.str();
        return cc;
    }

    ControlChart rChart(const std::vector<Vec> &subgroups)
    {
        ControlChart cc;
        cc.chartType = "R";
        int k = subgroups.size(), n = subgroups[0].size();
        Vec Rs(k);
        for (int i = 0; i < k; ++i)
            Rs[i] = *std::max_element(subgroups[i].begin(), subgroups[i].end()) -
                    *std::min_element(subgroups[i].begin(), subgroups[i].end());
        cc.data = Rs;
        double Rbar = std::accumulate(Rs.begin(), Rs.end(), 0.0) / k;
        cc.centreLine = Rbar;
        cc.UCL = D4(n) * Rbar;
        cc.LCL = D3(n) * Rbar;
        cc.violations.resize(k, false);
        for (int i = 0; i < k; ++i)
            cc.violations[i] = (Rs[i] > cc.UCL || Rs[i] < cc.LCL);
        std::ostringstream ss;
        ss << "R Chart: R̄=" << fmtOL(Rbar) << ", UCL=" << fmtOL(cc.UCL) << ", LCL=" << fmtOL(cc.LCL);
        cc.summary = ss.str();
        return cc;
    }

    ControlChart sChart(const std::vector<Vec> &subgroups)
    {
        ControlChart cc;
        cc.chartType = "S";
        int k = subgroups.size(), n = subgroups[0].size();
        Vec ss_v(k);
        for (int i = 0; i < k; ++i)
        {
            double m = std::accumulate(subgroups[i].begin(), subgroups[i].end(), 0.0) / n;
            double s2 = 0;
            for (double v : subgroups[i])
                s2 += (v - m) * (v - m);
            ss_v[i] = std::sqrt(s2 / (n - 1));
        }
        cc.data = ss_v;
        double sbar = std::accumulate(ss_v.begin(), ss_v.end(), 0.0) / k;
        double c4v = c4(n);
        cc.centreLine = sbar;
        cc.UCL = sbar + 3 * sbar * std::sqrt(1 - c4v * c4v) / c4v;
        cc.LCL = std::max(0.0, sbar - 3 * sbar * std::sqrt(1 - c4v * c4v) / c4v);
        cc.violations.resize(k, false);
        for (int i = 0; i < k; ++i)
            cc.violations[i] = (ss_v[i] > cc.UCL || ss_v[i] < cc.LCL);
        std::ostringstream ss;
        ss << "S Chart: s̄=" << fmtOL(sbar) << ", UCL=" << fmtOL(cc.UCL) << ", LCL=" << fmtOL(cc.LCL);
        cc.summary = ss.str();
        return cc;
    }

    ControlChart pChart(const Vec &defectives, const Vec &nSampled)
    {
        ControlChart cc;
        cc.chartType = "p";
        int k = defectives.size();
        Vec p(k);
        double pbar_num = 0, pbar_den = 0;
        for (int i = 0; i < k; ++i)
        {
            p[i] = defectives[i] / nSampled[i];
            pbar_num += defectives[i];
            pbar_den += nSampled[i];
        }
        double pbar = pbar_num / pbar_den;
        cc.data = p;
        cc.centreLine = pbar;
        cc.violations.resize(k, false);
        for (int i = 0; i < k; ++i)
        {
            double ni = nSampled[i];
            double ucl = pbar + 3 * std::sqrt(pbar * (1 - pbar) / ni);
            double lcl = std::max(0.0, pbar - 3 * std::sqrt(pbar * (1 - pbar) / ni));
            cc.violations[i] = (p[i] > ucl || p[i] < lcl);
        }
        cc.UCL = pbar + 3 * std::sqrt(pbar * (1 - pbar) / pbar_den * k);
        cc.LCL = std::max(0.0, pbar - 3 * std::sqrt(pbar * (1 - pbar) / pbar_den * k));
        std::ostringstream ss;
        ss << "p Chart: p̄=" << fmtOL(pbar) << ", UCL≈" << fmtOL(cc.UCL) << ", LCL≈" << fmtOL(cc.LCL);
        cc.summary = ss.str();
        return cc;
    }

    ControlChart cChart(const Vec &counts)
    {
        ControlChart cc;
        cc.chartType = "c";
        double cbar = std::accumulate(counts.begin(), counts.end(), 0.0) / counts.size();
        cc.data = counts;
        cc.centreLine = cbar;
        cc.UCL = cbar + 3 * std::sqrt(cbar);
        cc.LCL = std::max(0.0, cbar - 3 * std::sqrt(cbar));
        cc.violations.resize(counts.size(), false);
        for (size_t i = 0; i < counts.size(); ++i)
            cc.violations[i] = (counts[i] > cc.UCL || counts[i] < cc.LCL);
        std::ostringstream ss;
        ss << "c Chart: c̄=" << fmtOL(cbar) << ", UCL=" << fmtOL(cc.UCL) << ", LCL=" << fmtOL(cc.LCL);
        cc.summary = ss.str();
        return cc;
    }

    ControlChart cusum(const Vec &x, double k_ref, double h_ref)
    {
        ControlChart cc;
        cc.chartType = "CUSUM";
        int n = x.size();
        double xm = std::accumulate(x.begin(), x.end(), 0.0) / n;
        double s2 = 0;
        for (double v : x)
            s2 += (v - xm) * (v - xm);
        double sig = std::sqrt(s2 / (n - 1));
        Vec Cp(n, 0), Cm(n, 0);
        for (int i = 1; i < n; ++i)
        {
            Cp[i] = std::max(0.0, Cp[i - 1] + (x[i] - xm) / sig - k_ref);
            Cm[i] = std::max(0.0, Cm[i - 1] - (x[i] - xm) / sig - k_ref);
        }
        cc.data = Cp;
        cc.centreLine = 0;
        cc.UCL = h_ref;
        cc.LCL = 0;
        cc.violations.resize(n, false);
        for (int i = 0; i < n; ++i)
            cc.violations[i] = (Cp[i] > h_ref || Cm[i] > h_ref);
        std::ostringstream ss;
        ss << "CUSUM Chart (k=" << k_ref << ", h=" << h_ref << ")\n";
        ss << "Reference value k, Decision interval h\n";
        int vc = 0;
        for (bool v : cc.violations)
            vc += v;
        ss << "Out-of-control signals: " << vc;
        cc.summary = ss.str();
        return cc;
    }

    StatResult processCap(const Vec &x, double LSL, double USL)
    {
        int n = x.size();
        if (n < 2)
            return errOL("Need at least 2 obs");
        double xm = 0;
        for (double v : x)
            xm += v;
        xm /= n;
        double s2 = 0;
        for (double v : x)
            s2 += (v - xm) * (v - xm);
        s2 /= (n - 1);
        double sigma = std::sqrt(s2);
        double Cp = (USL - LSL) / (6 * sigma);
        double Cpl = (xm - LSL) / (3 * sigma);
        double Cpu = (USL - xm) / (3 * sigma);
        double Cpk = std::min(Cpl, Cpu);
        // Overall (using sample std dev for Pp/Ppk)
        double Pp = Cp, Ppk = Cpk; // same if using sample std dev
        double ppm_above = normcdf(-(USL - xm) / sigma) * 1e6;
        double ppm_below = normcdf(-(xm - LSL) / sigma) * 1e6;
        std::ostringstream ss;
        ss << "Process Capability Analysis\n";
        ss << "LSL=" << LSL << ", USL=" << USL << "\n";
        ss << "n=" << n << ", x̄=" << fmtOL(xm) << ", s=" << fmtOL(sigma) << "\n\n";
        ss << "Capability Indices:\n";
        ss << "  Cp   = " << fmtOL(Cp) << "  (potential, centered)\n";
        ss << "  Cpk  = " << fmtOL(Cpk) << "  (actual, accounts for centering)\n";
        ss << "  Cpl  = " << fmtOL(Cpl) << "  (lower)\n";
        ss << "  Cpu  = " << fmtOL(Cpu) << "  (upper)\n\n";
        ss << "Estimated defect rate:\n";
        ss << "  Above USL: " << fmtOL(ppm_above) << " ppm\n";
        ss << "  Below LSL: " << fmtOL(ppm_below) << " ppm\n";
        ss << "  Total:     " << fmtOL(ppm_above + ppm_below) << " ppm\n\n";
        ss << "Assessment: ";
        if (Cpk >= 1.67)
            ss << "Excellent (Six Sigma capable)";
        else if (Cpk >= 1.33)
            ss << "Capable";
        else if (Cpk >= 1.0)
            ss << "Marginally capable";
        else
            ss << "NOT capable — process improvement needed";
        return okOL(ss.str());
    }

    // =============================================================================
    // PROBABILITY (Ch. 4-5)
    // =============================================================================

    StatResult conditionalProb(double pAandB, double pB)
    {
        if (std::abs(pB) < 1e-12)
            return errOL("P(B) cannot be zero");
        double pAgivenB = pAandB / pB;
        std::ostringstream ss;
        ss << "P(A|B) = P(A∩B)/P(B) = " << fmtOL(pAandB) << "/" << fmtOL(pB) << " = " << fmtOL(pAgivenB);
        return okOL(fmtOL(pAgivenB), ss.str());
    }

    StatResult totalProbability(const Vec &prior, const Vec &likelihood)
    {
        if (prior.size() != likelihood.size())
            return errOL("Vectors must have equal length");
        double total = 0;
        for (size_t i = 0; i < prior.size(); ++i)
            total += prior[i] * likelihood[i];
        std::ostringstream ss;
        ss << "Law of Total Probability:\nP(B) = Σ P(B|Aᵢ)P(Aᵢ) = ";
        for (size_t i = 0; i < prior.size(); ++i)
        {
            if (i)
                ss << " + ";
            ss << fmtOL(likelihood[i]) << "×" << fmtOL(prior[i]);
        }
        ss << " = " << fmtOL(total);
        return okOL(fmtOL(total), ss.str());
    }

    StatResult bayesExtended(const Vec &prior, const Vec &likelihood, int event)
    {
        if (event < 0 || (size_t)event >= prior.size())
            return errOL("Event index out of range");
        double total = 0;
        for (size_t i = 0; i < prior.size(); ++i)
            total += prior[i] * likelihood[i];
        double posterior = prior[event] * likelihood[event] / total;
        std::ostringstream ss;
        ss << "Bayes' Theorem (extended form):\n";
        ss << "P(A" << event + 1 << "|B) = P(B|A" << event + 1 << ")·P(A" << event + 1 << ") / P(B)\n";
        ss << "             = " << fmtOL(likelihood[event]) << "×" << fmtOL(prior[event]) << "/" << fmtOL(total) << "\n";
        ss << "             = " << fmtOL(posterior);
        return okOL(fmtOL(posterior), ss.str());
    }

    StatResult combinations(int n, int r)
    {
        if (r < 0 || r > n)
            return errOL("Invalid r");
        double lC = std::lgamma(n + 1) - std::lgamma(r + 1) - std::lgamma(n - r + 1);
        double C = std::round(std::exp(lC));
        std::ostringstream ss;
        ss << "C(" << n << "," << r << ") = " << n << "! / (" << r << "! × " << (n - r) << "!) = " << (long long)C;
        return okOL(std::to_string((long long)C), ss.str());
    }

    StatResult permutations(int n, int r)
    {
        if (r < 0 || r > n)
            return errOL("Invalid r");
        double lP = std::lgamma(n + 1) - std::lgamma(n - r + 1);
        double P = std::round(std::exp(lP));
        std::ostringstream ss;
        ss << "P(" << n << "," << r << ") = " << n << "! / " << (n - r) << "! = " << (long long)P;
        return okOL(std::to_string((long long)P), ss.str());
    }

    StatResult multinomialCoeff(int n, const std::vector<int> &counts)
    {
        int sum = 0;
        for (int c : counts)
            sum += c;
        if (sum != n)
            return errOL("Counts must sum to n");
        double lM = std::lgamma(n + 1);
        for (int c : counts)
            lM -= std::lgamma(c + 1);
        double M = std::round(std::exp(lM));
        std::ostringstream ss;
        ss << "M = " << n << "! / (";
        for (size_t i = 0; i < counts.size(); ++i)
        {
            if (i)
                ss << "×";
            ss << counts[i] << "!";
        }
        ss << ") = " << (long long)M;
        return okOL(std::to_string((long long)M), ss.str());
    }

    // =============================================================================
    // REMAINING MISSING STATISTICS IMPLEMENTATIONS
    // =============================================================================

    // chiSqIndep — chi-square test of independence
    StatResult chiSqIndep(const std::vector<Vec> &table, double alpha)
    {
        int r = table.size(), c = table[0].size();
        double n = 0;
        for (auto &row : table)
            for (double v : row)
                n += v;
        Vec rowT(r, 0), colT(c, 0);
        for (int i = 0; i < r; ++i)
            for (int j = 0; j < c; ++j)
            {
                rowT[i] += table[i][j];
                colT[j] += table[i][j];
            }

        double chi2 = 0;
        bool lowExpected = false;
        std::ostringstream ss;
        ss << "Chi-Square Test of Independence\n";
        ss << r << "×" << c << " contingency table\n\n";
        ss << "Expected frequencies:\n";
        for (int i = 0; i < r; ++i)
        {
            ss << "  [";
            for (int j = 0; j < c; ++j)
            {
                double E = rowT[i] * colT[j] / n;
                if (E < 5)
                    lowExpected = true;
                if (j)
                    ss << ", ";
                ss << fmtOL(E, 3);
                if (E > 1e-10)
                    chi2 += std::pow(table[i][j] - E, 2) / E;
            }
            ss << "]\n";
        }
        double df = (r - 1.0) * (c - 1.0);
        double pval = 1 - chi2cdf(chi2, df);
        ss << "\nχ² = " << fmtOL(chi2) << ",  df = " << (int)df << ",  p = " << fmtOL(pval) << "\n";
        if (lowExpected)
            ss << "⚠ Some expected frequencies < 5 (use Fisher's exact test)\n";
        ss << "Conclusion: " << (pval < alpha ? "Reject H₀ (variables are associated)" : "Fail to reject H₀ (variables may be independent)");
        return okOL(ss.str());
    }

    // factorial2x2 — 2×2 factorial design ANOVA (Ott-Longnecker Ch.14)
    StatResult factorial2x2(const std::vector<Vec> &cells, double alpha)
    {
        // cells must have 4 groups: [A1B1, A1B2, A2B1, A2B2]
        if (cells.size() < 4)
            return errOL("Need 4 cells for 2×2 factorial");
        int n = cells[0].size(); // reps per cell
        if (n < 2)
            return errOL("Need at least 2 replicates per cell");

        double ybar[2][2], grand = 0;
        int N = 4 * n;
        for (int i = 0; i < 2; ++i)
            for (int j = 0; j < 2; ++j)
            {
                ybar[i][j] = 0;
                for (double v : cells[i * 2 + j])
                    ybar[i][j] += v;
                ybar[i][j] /= n;
                grand += ybar[i][j] * n;
            }
        grand /= N;

        double Amean[2] = {0, 0}, Bmean[2] = {0, 0};
        for (int i = 0; i < 2; ++i)
        {
            Amean[i] = (ybar[i][0] + ybar[i][1]) / 2;
        }
        for (int j = 0; j < 2; ++j)
        {
            Bmean[j] = (ybar[0][j] + ybar[1][j]) / 2;
        }

        double SSA = 0, SSB = 0, SSAB = 0, SSE = 0, SST = 0;
        for (int i = 0; i < 2; ++i)
            SSA += 2 * n * (Amean[i] - grand) * (Amean[i] - grand);
        for (int j = 0; j < 2; ++j)
            SSB += 2 * n * (Bmean[j] - grand) * (Bmean[j] - grand);
        for (int i = 0; i < 2; ++i)
            for (int j = 0; j < 2; ++j)
                SSAB += n * (ybar[i][j] - Amean[i] - Bmean[j] + grand) * (ybar[i][j] - Amean[i] - Bmean[j] + grand);
        for (int i = 0; i < 2; ++i)
            for (int j = 0; j < 2; ++j)
                for (double v : cells[i * 2 + j])
                {
                    SSE += (v - ybar[i][j]) * (v - ybar[i][j]);
                    SST += (v - grand) * (v - grand);
                }

        double dfA = 1, dfB = 1, dfAB = 1, dfE = 4 * (n - 1);
        double MSA = SSA, MSB = SSB, MSAB = SSAB, MSE = SSE / dfE;
        double FA = MSA / MSE, FB = MSB / MSE, FAB = MSAB / MSE;
        double pA = 1 - fcdf(FA, 1, dfE), pB = 1 - fcdf(FB, 1, dfE), pAB = 1 - fcdf(FAB, 1, dfE);

        std::ostringstream ss;
        ss << "2×2 Factorial ANOVA (n=" << n << " per cell)\n\n";
        ss << "Cell means:\n";
        ss << "  A1B1=" << fmtOL(ybar[0][0]) << "  A1B2=" << fmtOL(ybar[0][1]) << "\n";
        ss << "  A2B1=" << fmtOL(ybar[1][0]) << "  A2B2=" << fmtOL(ybar[1][1]) << "\n\n";
        ss << "Source    SS      df  MS      F      p\n"
           << std::string(45, '-') << "\n";
        ss << std::left << std::setw(9) << "A" << std::setw(8) << fmtOL(SSA) << std::setw(4) << 1
           << std::setw(8) << fmtOL(MSA) << std::setw(7) << fmtOL(FA) << fmtOL(pA) << "\n";
        ss << std::setw(9) << "B" << std::setw(8) << fmtOL(SSB) << std::setw(4) << 1
           << std::setw(8) << fmtOL(MSB) << std::setw(7) << fmtOL(FB) << fmtOL(pB) << "\n";
        ss << std::setw(9) << "A×B" << std::setw(8) << fmtOL(SSAB) << std::setw(4) << 1
           << std::setw(8) << fmtOL(MSAB) << std::setw(7) << fmtOL(FAB) << fmtOL(pAB) << "\n";
        ss << std::setw(9) << "Error" << std::setw(8) << fmtOL(SSE) << (int)dfE << "\n\n";
        ss << "Interaction A×B: " << (pAB < alpha ? "significant" : "not significant") << " (p=" << fmtOL(pAB) << ")\n";
        ss << "Main effect A:   " << (pA < alpha ? "significant" : "not significant") << " (p=" << fmtOL(pA) << ")\n";
        ss << "Main effect B:   " << (pB < alpha ? "significant" : "not significant") << " (p=" << fmtOL(pB) << ")\n";
        return okOL(ss.str());
    }

    // markovAbsorption — absorbing Markov chain fundamental matrix
    StatResult markovAbsorption(const std::vector<Vec> &T,
                                const std::vector<int> &absorbing)
    {
        int n = T.size();
        std::set<int> abs(absorbing.begin(), absorbing.end());
        std::vector<int> trans;
        for (int i = 0; i < n; ++i)
            if (!abs.count(i))
                trans.push_back(i);
        int t = trans.size(), a = absorbing.size();
        if (t == 0)
            return errOL("No transient states");

        // Extract Q (transient→transient) and R (transient→absorbing)
        std::vector<Vec> Q(t, Vec(t, 0)), R(t, Vec(a, 0));
        for (int i = 0; i < t; ++i)
        {
            for (int j = 0; j < t; ++j)
                Q[i][j] = T[trans[i]][trans[j]];
            for (int j = 0; j < a; ++j)
                R[i][j] = T[trans[i]][absorbing[j]];
        }

        // Fundamental matrix N = (I - Q)^{-1}
        std::vector<Vec> IminusQ(t, Vec(t, 0));
        for (int i = 0; i < t; ++i)
        {
            IminusQ[i] = Q[i];
            IminusQ[i][i] -= 1.0;
            for (auto &v : IminusQ[i])
                v = -v;
        }
        // Gauss-Jordan inversion
        std::vector<Vec> aug(t, Vec(2 * t, 0));
        for (int i = 0; i < t; ++i)
        {
            for (int j = 0; j < t; ++j)
                aug[i][j] = IminusQ[i][j];
            aug[i][t + i] = 1;
        }
        for (int col = 0; col < t; ++col)
        {
            int piv = col;
            for (int r = col + 1; r < t; ++r)
                if (std::abs(aug[r][col]) > std::abs(aug[piv][col]))
                    piv = r;
            std::swap(aug[col], aug[piv]);
            if (std::abs(aug[col][col]) < 1e-12)
                return errOL("I-Q is singular");
            double d = aug[col][col];
            for (int j = 0; j < 2 * t; ++j)
                aug[col][j] /= d;
            for (int r = 0; r < t; ++r)
            {
                if (r == col)
                    continue;
                double f = aug[r][col];
                for (int j = 0; j < 2 * t; ++j)
                    aug[r][j] -= f * aug[col][j];
            }
        }
        std::vector<Vec> N(t, Vec(t, 0));
        for (int i = 0; i < t; ++i)
            for (int j = 0; j < t; ++j)
                N[i][j] = aug[i][t + j];

        // Expected absorption times: t_i = sum of row i of N
        std::ostringstream ss;
        ss << "Absorbing Markov Chain Analysis\n\n";
        ss << "Absorbing states: ";
        for (int s : absorbing)
            ss << s << " ";
        ss << "\n";
        ss << "Transient states: ";
        for (int s : trans)
            ss << s << " ";
        ss << "\n\n";
        ss << "Fundamental matrix N = (I-Q)^{-1}:\n";
        for (int i = 0; i < t; ++i)
        {
            ss << "  [";
            for (int j = 0; j < t; ++j)
            {
                if (j)
                    ss << ",";
                ss << fmtOL(N[i][j], 4);
            }
            ss << "]\n";
        }
        ss << "\nExpected steps to absorption from transient states:\n";
        for (int i = 0; i < t; ++i)
        {
            double ti = 0;
            for (int j = 0; j < t; ++j)
                ti += N[i][j];
            ss << "  State " << trans[i] << ": E[T] = " << fmtOL(ti) << "\n";
        }
        // Absorption probabilities B = N R
        ss << "\nAbsorption probability matrix B = NR:\n";
        for (int i = 0; i < t; ++i)
        {
            ss << "  From state " << trans[i] << ": [";
            for (int j = 0; j < a; ++j)
            {
                double b = 0;
                for (int k = 0; k < t; ++k)
                    b += N[i][k] * R[k][j];
                if (j)
                    ss << ", ";
                ss << "→" << absorbing[j] << ":" << fmtOL(b, 4);
            }
            ss << "]\n";
        }
        return okOL(ss.str());
    }

    // naiveBayes — Gaussian Naive Bayes classifier
    StatResult naiveBayes(const std::vector<Vec> &features,
                          const std::vector<int> &labels,
                          const Vec &newPoint)
    {
        if (features.empty() || features.size() != labels.size())
            return errOL("Invalid input");
        int nFeat = features[0].size();

        // Count classes
        std::map<int, int> classCounts;
        for (int l : labels)
            classCounts[l]++;
        int N = labels.size();

        std::ostringstream ss;
        ss << "Gaussian Naive Bayes Classifier\n\n";
        ss << "Training: " << N << " samples, " << nFeat << " features, " << classCounts.size() << " classes\n\n";

        double bestLogProb = -1e300;
        int bestClass = -1;

        for (auto &[cls, count] : classCounts)
        {
            double logP = std::log((double)count / N); // log prior
            // Compute feature stats for this class
            for (int f = 0; f < nFeat; ++f)
            {
                double mu = 0, s2 = 0;
                int n2 = 0;
                for (int i = 0; i < N; ++i)
                    if (labels[i] == cls)
                    {
                        mu += features[i][f];
                        n2++;
                    }
                mu /= n2;
                for (int i = 0; i < N; ++i)
                    if (labels[i] == cls)
                        s2 += (features[i][f] - mu) * (features[i][f] - mu);
                s2 = s2 / n2 + 1e-9; // smoothing
                // Log Gaussian likelihood
                double x = newPoint[f];
                logP += -0.5 * std::log(2 * M_PI * s2) - 0.5 * (x - mu) * (x - mu) / s2;
            }
            if (logP > bestLogProb)
            {
                bestLogProb = logP;
                bestClass = cls;
            }
            ss << "  Class " << cls << ": log P(x|class)*P(class) = " << fmtOL(logP) << "\n";
        }
        ss << "\nPrediction: class " << bestClass << " (highest log posterior probability)";
        return okOL(ss.str());
    }

    // mutualInfo — mutual information I(X;Y)
    StatResult mutualInfo(const std::vector<Vec> &jointProb)
    {
        // jointProb[i][j] = P(X=i, Y=j)
        int r = jointProb.size(), c = jointProb[0].size();
        Vec rowMarg(r, 0), colMarg(c, 0);
        double total = 0;
        for (int i = 0; i < r; ++i)
            for (int j = 0; j < c; ++j)
            {
                rowMarg[i] += jointProb[i][j];
                colMarg[j] += jointProb[i][j];
                total += jointProb[i][j];
            }
        double MI = 0, H_X = 0, H_Y = 0;
        for (int i = 0; i < r; ++i)
            if (rowMarg[i] > 1e-15)
                H_X -= rowMarg[i] * std::log2(rowMarg[i]);
        for (int j = 0; j < c; ++j)
            if (colMarg[j] > 1e-15)
                H_Y -= colMarg[j] * std::log2(colMarg[j]);
        for (int i = 0; i < r; ++i)
            for (int j = 0; j < c; ++j)
            {
                double p = jointProb[i][j];
                if (p > 1e-15 && rowMarg[i] > 1e-15 && colMarg[j] > 1e-15)
                    MI += p * std::log2(p / (rowMarg[i] * colMarg[j]));
            }
        std::ostringstream ss;
        ss << "Mutual Information I(X;Y) = " << fmtOL(MI) << " bits\n\n";
        ss << "H(X) = " << fmtOL(H_X) << " bits\n";
        ss << "H(Y) = " << fmtOL(H_Y) << " bits\n";
        ss << "I(X;Y) = H(X) + H(Y) - H(X,Y)\n";
        ss << "Normalised: I/(min(H_X,H_Y)) = " << fmtOL(MI / std::max(std::min(H_X, H_Y), 1e-10));
        return okOL(fmtOL(MI), ss.str());
    }

    // npChart (count chart, np variant)
    ControlChart npChart(const Vec &defectives, int n)
    {
        ControlChart cc;
        cc.chartType = "np";
        double pbar = 0;
        for (double d : defectives)
            pbar += d;
        pbar /= (defectives.size() * n);
        double np_bar = pbar * n;
        cc.data = defectives;
        cc.centreLine = np_bar;
        cc.UCL = np_bar + 3 * std::sqrt(np_bar * (1 - pbar));
        cc.LCL = std::max(0.0, np_bar - 3 * std::sqrt(np_bar * (1 - pbar)));
        cc.violations.resize(defectives.size(), false);
        for (size_t i = 0; i < defectives.size(); ++i)
            cc.violations[i] = (defectives[i] > cc.UCL || defectives[i] < cc.LCL);
        std::ostringstream ss;
        ss << "np Chart: n̄p=" << fmtOL(np_bar) << ", UCL=" << fmtOL(cc.UCL) << ", LCL=" << fmtOL(cc.LCL);
        cc.summary = ss.str();
        return cc;
    }

} // namespace Statistics