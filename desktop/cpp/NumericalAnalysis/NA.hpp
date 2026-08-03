#pragma once
// NumericalAnalysis.h — CoreEngine prefix: "na:<op>|{json}"

#ifndef NUMERICALANALYSIS_HPP
#define NUMERICALANALYSIS_HPP

#include <string>
#include <vector>
#include <functional>

namespace NumericalAnalysis
{

    struct NAResult
    {
        bool ok = true;
        std::string value;
        std::string detail;
        std::string error;
        int iterations = 0;
        double residual = 0.0;
    };

    using Vec = std::vector<double>;
    using Mat = std::vector<Vec>;
    using Func = std::function<double(double)>;

    // ── Root finding ──────────────────────────────────────────────────────────────
    NAResult bisection(const std::string &f, const std::string &x,
                       double a, double b, double tol = 1e-10, int maxIter = 100);
    NAResult newtonRaphson(const std::string &f, const std::string &x,
                           double x0, double tol = 1e-10, int maxIter = 100);
    NAResult secantMethod(const std::string &f, const std::string &x,
                          double x0, double x1, double tol = 1e-10, int maxIter = 100);
    NAResult regulaFalsi(const std::string &f, const std::string &x,
                         double a, double b, double tol = 1e-10, int maxIter = 100);
    NAResult fixedPoint(const std::string &g, const std::string &x,
                        double x0, double tol = 1e-10, int maxIter = 100);
    NAResult mullerMethod(const std::string &f, const std::string &x,
                          double x0, double x1, double x2,
                          double tol = 1e-10, int maxIter = 50);
    NAResult brentsMethod(const std::string &f, const std::string &x,
                          double a, double b, double tol = 1e-10);
    NAResult allRootsInterval(const std::string &f, const std::string &x,
                              double a, double b, int subdivisions = 100);

    // ── Interpolation ─────────────────────────────────────────────────────────────
    NAResult lagrangeInterp(const Vec &xs, const Vec &ys, double xEval);
    NAResult newtonDivDiff(const Vec &xs, const Vec &ys, double xEval); // divided differences
    NAResult neville(const Vec &xs, const Vec &ys, double xEval);
    NAResult splineCubic(const Vec &xs, const Vec &ys, double xEval); // natural cubic spline
    NAResult splineLinear(const Vec &xs, const Vec &ys, double xEval);
    NAResult hermiteInterp(const Vec &xs, const Vec &ys, const Vec &ders, double xEval);
    NAResult chebychevNodes(int n, double a, double b);               // optimal node placement
    NAResult polynomialFit(const Vec &xs, const Vec &ys, int degree); // least squares

    // ── Numerical differentiation ─────────────────────────────────────────────────
    NAResult forwardDiff(const std::string &f, const std::string &x,
                         double x0, double h = 1e-5);
    NAResult centralDiff(const std::string &f, const std::string &x,
                         double x0, double h = 1e-5);
    NAResult secondDeriv(const std::string &f, const std::string &x,
                         double x0, double h = 1e-5);
    NAResult richardsonDiff(const std::string &f, const std::string &x,
                            double x0, double h = 0.1); // extrapolated derivative

    // ── Numerical integration (quadrature) ────────────────────────────────────────
    NAResult trapezoidalRule(const std::string &f, const std::string &x,
                             double a, double b, int n);
    NAResult simpsonsRule(const std::string &f, const std::string &x,
                          double a, double b, int n);
    NAResult simpsons38Rule(const std::string &f, const std::string &x,
                            double a, double b, int n);
    NAResult gaussLegendre(const std::string &f, const std::string &x,
                           double a, double b, int n = 5); // n-point GL
    NAResult gaussChebyshev(const std::string &f, const std::string &x, int n = 5);
    NAResult romberg(const std::string &f, const std::string &x,
                     double a, double b, int levels = 6);
    NAResult adaptiveQuad(const std::string &f, const std::string &x,
                          double a, double b, double tol = 1e-8);
    NAResult gaussLaguerre(const std::string &f, const std::string &x, int n = 5); // ∫_0^∞
    NAResult gaussHermite(const std::string &f, const std::string &x, int n = 5);  // ∫_{-∞}^∞

    // ── Linear systems ────────────────────────────────────────────────────────────
    NAResult gaussianElim(const Mat &A, const Vec &b); // with partial pivoting
    NAResult luDecomp(const Mat &A, const Vec &b);
    NAResult choleskyNum(const Mat &A, const Vec &b); // for SPD matrices
    NAResult jacobiIter(const Mat &A, const Vec &b,
                        double tol = 1e-8, int maxIter = 500);
    NAResult gaussSeidelIter(const Mat &A, const Vec &b,
                             double tol = 1e-8, int maxIter = 500);
    NAResult sorIter(const Mat &A, const Vec &b,
                     double omega, double tol = 1e-8, int maxIter = 500);
    NAResult conjugateGradient(const Mat &A, const Vec &b,
                               double tol = 1e-8, int maxIter = 1000);
    NAResult conditionNumber(const Mat &A);

    // ── Eigenvalue methods ────────────────────────────────────────────────────────
    NAResult powerIteration(const Mat &A, double tol = 1e-8, int maxIter = 1000);
    NAResult inverseIteration(const Mat &A, double shift,
                              double tol = 1e-8, int maxIter = 1000);
    NAResult qrAlgorithmNum(const Mat &A, int maxIter = 100);

    // ── Error analysis ────────────────────────────────────────────────────────────
    NAResult truncationError(const std::string &method, int order, double h);
    NAResult roundoffError(double computedVal, double exactVal);
    NAResult convergenceOrder(const Vec &errors); // estimate convergence rate

    // ── Fast Fourier Transform ───────────────────────────────────────────────────
    NAResult fastFourierTransform(const std::string &dataStr, bool inverse);

    // ── Dispatch ──────────────────────────────────────────────────────────────────
    NAResult dispatch(const std::string &op, const std::string &json);

} // namespace NumericalAnalysis
#endif
