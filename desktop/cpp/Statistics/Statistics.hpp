#pragma once
// Statistics.h — descriptive stats, distributions, hypothesis testing, regression, Bayesian.

#ifndef STATISTICS_HPP
#define STATISTICS_HPP

#include <string>
#include <vector>
#include <map>

namespace Statistics
{

    struct StatResult
    {
        bool ok = true;
        std::string value;
        std::string detail;
        std::string error;
    };

    using Vec = std::vector<double>;

    // ── Descriptive statistics ────────────────────────────────────────────────────

    StatResult mean(const Vec &x);
    StatResult median(Vec x);
    StatResult mode(const Vec &x);
    StatResult variance(const Vec &x, bool population = false);
    StatResult stddev(const Vec &x, bool population = false);
    StatResult skewness(const Vec &x);
    StatResult kurtosis(const Vec &x);
    StatResult percentile(Vec x, double p);
    StatResult iqr(Vec x);
    StatResult fiveNumber(Vec x); // min, Q1, median, Q3, max
    StatResult summarize(Vec x);  // all of the above

    // ── Distributions — PDF, CDF, quantile, mean, variance ───────────────────────

    // Continuous
    StatResult normalPDF(double x, double mu, double sigma);
    StatResult normalCDF(double x, double mu, double sigma);
    StatResult normalQF(double p, double mu, double sigma); // quantile/inverse CDF
    StatResult tPDF(double x, double df);
    StatResult tCDF(double x, double df);
    StatResult tQF(double p, double df);
    StatResult chiSqPDF(double x, double df);
    StatResult chiSqCDF(double x, double df);
    StatResult chiSqQF(double p, double df);
    StatResult fDistPDF(double x, double d1, double d2);
    StatResult fDistCDF(double x, double d1, double d2);
    StatResult expPDF(double x, double lambda);
    StatResult expCDF(double x, double lambda);
    StatResult uniformPDF(double x, double a, double b);
    StatResult uniformCDF(double x, double a, double b);
    StatResult gammaPDF(double x, double alpha, double beta);
    StatResult gammaCDF(double x, double alpha, double beta);
    StatResult betaPDF(double x, double alpha, double beta);
    StatResult betaCDF(double x, double alpha, double beta);

    // Discrete
    StatResult binomialPMF(int k, int n, double p);
    StatResult binomialCDF(int k, int n, double p);
    StatResult poissonPMF(int k, double lambda);
    StatResult poissonCDF(int k, double lambda);
    StatResult geometricPMF(int k, double p);
    StatResult hypergeoPMF(int k, int N, int K, int n);
    StatResult negativeBinPMF(int k, int r, double p);

    // ── Confidence intervals ──────────────────────────────────────────────────────

    StatResult ciMeanZ(const Vec &x, double alpha); // known sigma (z)
    StatResult ciMeanT(const Vec &x, double alpha); // unknown sigma (t)
    StatResult ciProportion(int successes, int n, double alpha);

    // ── Hypothesis tests ──────────────────────────────────────────────────────────

    StatResult zTest(const Vec &x, double mu0, double sigma, double alpha = 0.05);
    StatResult tTestOne(const Vec &x, double mu0, double alpha = 0.05);
    StatResult tTestTwo(const Vec &x, const Vec &y, double alpha = 0.05, bool equalVar = false);
    StatResult tTestPaired(const Vec &x, const Vec &y, double alpha = 0.05);
    StatResult chiSqTest(const Vec &observed, const Vec &expected, double alpha = 0.05);
    StatResult chiSqIndep(const std::vector<Vec> &table, double alpha = 0.05);
    StatResult fTest(const Vec &x, const Vec &y, double alpha = 0.05);
    StatResult anovaOne(const std::vector<Vec> &groups, double alpha = 0.05);

    // ── Non-parametric tests ──────────────────────────────────────────────────────

    StatResult mannWhitney(const Vec &x, const Vec &y, double alpha = 0.05);
    StatResult wilcoxon(const Vec &x, double mu0 = 0, double alpha = 0.05);
    StatResult kruskalWallis(const std::vector<Vec> &groups, double alpha = 0.05);
    StatResult spearman(const Vec &x, const Vec &y);
    StatResult kendallTau(const Vec &x, const Vec &y);

    // ── Regression ────────────────────────────────────────────────────────────────

    struct RegressionResult
    {
        Vec coefficients; // [intercept, b1, b2, ...]
        Vec residuals;
        double r2 = 0, adjR2 = 0, rmse = 0;
        std::string equation;
        std::string summary;
        bool ok = true;
        std::string error;
    };

    RegressionResult linearRegression(const Vec &x, const Vec &y);
    RegressionResult multipleRegression(const std::vector<Vec> &X, const Vec &y);
    RegressionResult polynomialRegression(const Vec &x, const Vec &y, int degree);
    RegressionResult logisticRegression(const std::vector<Vec> &X, const Vec &y, int maxIter = 100);

    // ── Correlation ───────────────────────────────────────────────────────────────

    StatResult pearson(const Vec &x, const Vec &y);
    StatResult covariance(const Vec &x, const Vec &y, bool population = false);

    // ── Bayesian ──────────────────────────────────────────────────────────────────

    StatResult bayesTheorem(double prior, double likelihood, double marginal);
    StatResult naiveBayes(const std::vector<Vec> &features,
                          const std::vector<int> &labels,
                          const Vec &newPoint);

    // ── Markov chains ─────────────────────────────────────────────────────────────

    StatResult markovSteadyState(const std::vector<Vec> &transMatrix);
    StatResult markovAbsorption(const std::vector<Vec> &transMatrix, const std::vector<int> &absorbing);

    // ── Information theory ────────────────────────────────────────────────────────

    StatResult entropy(const Vec &probs);
    StatResult klDivergence(const Vec &P, const Vec &Q);
    StatResult mutualInfo(const std::vector<Vec> &jointProb);

    // =============================================================================
    // OTT-LONGNECKER EXTENSIONS (7th ed.)
    // =============================================================================

    // ── Frequency tables ──────────────────────────────────────────────────────────

    struct FrequencyTable
    {
        std::vector<std::string> classes;
        Vec frequency, relFrequency, cumFrequency;
        bool ok = true;
        std::string error;
        std::string format() const;
    };

    FrequencyTable frequencyTable(const Vec &x, int bins = 0);         // continuous
    FrequencyTable freqTableCat(const std::vector<std::string> &cats); // categorical

    // ── Sampling distributions ────────────────────────────────────────────────────

    StatResult cltDemo(const Vec &population, int sampleSize, int reps);
    StatResult standardError(const Vec &x); // SE = s/sqrt(n)
    StatResult bootstrapCI(const Vec &x, double alpha, int B = 2000);
    StatResult samplingDistProp(int n, double p, double alpha); // for proportions

    // ── Experimental design ───────────────────────────────────────────────────────

    StatResult randomisedBlock(const std::vector<Vec> &data, // [treatment][block]
                               double alpha = 0.05);
    StatResult factorial2x2(const std::vector<Vec> &data, // [cell] = {a1b1,a1b2,a2b1,a2b2}
                            double alpha = 0.05);
    StatResult anovaTwoWay(const std::vector<std::vector<Vec>> &data, // [i][j] = reps
                           double alpha = 0.05);
    StatResult tukeyHSD(const std::vector<Vec> &groups, double alpha = 0.05);
    StatResult bonferroni(const std::vector<Vec> &groups, double alpha = 0.05);
    StatResult scheffe(const std::vector<Vec> &groups, double alpha = 0.05);

    // ── Multiple regression diagnostics ──────────────────────────────────────────

    struct RegressionDiagnostics
    {
        Vec vif;      // Variance Inflation Factors
        Vec leverage; // hat matrix diagonal h_ii
        Vec cooksD;   // Cook's distance
        Vec standardizedResid;
        Vec studentizedResid;
        Vec dffits;
        double durbinWatson; // autocorrelation in residuals
        bool ok = true;
        std::string summary;
        std::string error;
    };

    RegressionDiagnostics regressionDiagnostics(const std::vector<Vec> &X, const Vec &y);
    StatResult vif(const std::vector<Vec> &X, const Vec &y);
    StatResult cooksDistance(const std::vector<Vec> &X, const Vec &y);
    StatResult durbinWatson(const Vec &residuals);
    StatResult stepwiseSelection(const std::vector<Vec> &X, const Vec &y,
                                 const std::string &method = "forward"); // forward/backward/both

    // ── Categorical data analysis ─────────────────────────────────────────────────

    StatResult fisherExact(int a, int b, int c, int d, double alpha = 0.05);
    StatResult mcnemar(int a, int b, int c, int d, double alpha = 0.05);
    StatResult oddsRatio(int a, int b, int c, int d);
    StatResult relativeRisk(int a, int b, int c, int d);
    StatResult riskDifference(int a, int b, int c, int d);
    StatResult numberOfNeededToTreat(int a, int b, int c, int d);
    StatResult phiCoefficient(int a, int b, int c, int d);
    StatResult cramerV(const std::vector<Vec> &table);

    // ── Extended non-parametric ───────────────────────────────────────────────────

    StatResult signTest(const Vec &x, double mu0 = 0, double alpha = 0.05);
    StatResult runsTest(const Vec &x, double alpha = 0.05);
    StatResult friedmanTest(const std::vector<Vec> &blocks, double alpha = 0.05);
    StatResult kolmogorovSmirnov(const Vec &x, const Vec &y, double alpha = 0.05); // two-sample
    StatResult andersonDarling(const Vec &x, double alpha = 0.05);                 // normality test
    StatResult shapiroWilk(const Vec &x, double alpha = 0.05);                     // normality test

    // ── Survival analysis ─────────────────────────────────────────────────────────

    struct KaplanMeierResult
    {
        Vec times;
        Vec survival;     // S(t) estimate
        Vec lower, upper; // 95% CI
        Vec nAtRisk, nEvents, nCensored;
        int totalEvents = 0;
        double medianSurvival = 0;
        bool ok = true;
        std::string error;
        std::string table;
    };

    KaplanMeierResult kaplanMeier(const Vec &times,
                                  const std::vector<int> &status); // 1=event, 0=censored
    StatResult logRankTest(const Vec &times1, const std::vector<int> &status1,
                           const Vec &times2, const std::vector<int> &status2);
    StatResult hazardRate(const Vec &times, const std::vector<int> &status,
                          double bandwidth = 0); // kernel-smoothed

    // ── Time series ───────────────────────────────────────────────────────────────

    StatResult movingAverage(const Vec &x, int window);
    StatResult exponentialSmoothing(const Vec &x, double alpha);            // simple
    StatResult doubleExpSmoothing(const Vec &x, double alpha, double beta); // Holt
    StatResult acf(const Vec &x, int maxLag = 20);                          // autocorrelation
    StatResult pacf(const Vec &x, int maxLag = 20);                         // partial autocorrelation
    StatResult ljungBox(const Vec &x, int lag = 10);                        // portmanteau test
    StatResult differencing(const Vec &x, int d = 1);                       // d-th order difference
    StatResult forecastSES(const Vec &x, double alpha, int h = 5);          // h-step ahead

    // ── Quality control (SPC) ─────────────────────────────────────────────────────

    struct ControlChart
    {
        Vec data;
        double centreLine, UCL, LCL;
        std::vector<bool> violations;    // points outside control limits
        std::vector<int> runsViolations; // Western Electric runs rules
        bool ok = true;
        std::string chartType;
        std::string summary;
    };

    ControlChart xbarChart(const std::vector<Vec> &subgroups);       // X-bar chart
    ControlChart rChart(const std::vector<Vec> &subgroups);          // R chart
    ControlChart sChart(const std::vector<Vec> &subgroups);          // S chart
    ControlChart pChart(const Vec &defectives, const Vec &nSampled); // proportion
    ControlChart npChart(const Vec &defectives, int n);              // count
    ControlChart cChart(const Vec &counts);                          // Poisson defects
    ControlChart cusum(const Vec &x, double k, double h);            // CUSUM
    StatResult processCap(const Vec &x, double LSL, double USL);     // Cp, Cpk, Pp, Ppk

    // ── Probability (Ott-Longnecker Ch. 4-5) ─────────────────────────────────────

    StatResult conditionalProb(double pAandB, double pB);
    StatResult totalProbability(const Vec &priorP, const Vec &likelihoodP);
    StatResult bayesExtended(const Vec &priorP, const Vec &likelihoodP, int event);
    StatResult combinations(int n, int r);
    StatResult permutations(int n, int r);
    StatResult multinomialCoeff(int n, const std::vector<int> &counts);

    // ── Dispatch ──────────────────────────────────────────────────────────────────

    StatResult dispatch(const std::string &operation,
                        const std::string &input, bool exactMode);

} // namespace Statistics

#endif