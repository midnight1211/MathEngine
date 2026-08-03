#pragma once
// ComplexAnalysis.h — CoreEngine prefix: "ca:<op>|{json}"

#ifndef CA_HPP
#define CA_HPP

#include <string>
#include <vector>
#include <complex>

namespace ComplexAnalysis
{

    using C = std::complex<double>;

    struct CAResult
    {
        bool ok = true;
        std::string value;
        std::string detail;
        std::string error;
    };

    // Complex arithmetic / representations
    CAResult polarForm(double re, double im);
    CAResult rectangular(double r, double theta);
    CAResult complexPow(double re, double im, double n);
    CAResult complexExp(double re, double im);
    CAResult complexLog(double re, double im, int branch = 0);
    CAResult complexSqrt(double re, double im);
    CAResult allRoots(double re, double im, int n); // n-th roots

    // Cauchy-Riemann equations check
    // f(z) = u(x,y) + iv(x,y): check ∂u/∂x=∂v/∂y, ∂u/∂y=-∂v/∂x
    CAResult cauchyRiemann(const std::string &u, const std::string &v,
                           const std::string &x, const std::string &y,
                           double x0, double y0);

    // Analytic / harmonic functions
    CAResult harmonicConjugate(const std::string &u,
                               const std::string &x, const std::string &y);
    CAResult isAnalytic(const std::string &u, const std::string &v,
                        const std::string &x, const std::string &y);
    CAResult laplacianCheck(const std::string &u,
                            const std::string &x, const std::string &y);

    // Taylor / Laurent series
    CAResult taylorSeriesC(const std::string &f, double a, double b, int N);
    CAResult laurentSeries(const std::string &f,
                           double centerRe, double centerIm, int N);
    CAResult radiusOfConvC(const std::string &f, double a, double b);

    // Singularities and residues
    CAResult classifySingularity(const std::string &f, double poleRe, double poleIm);
    CAResult residue(const std::string &f, double poleRe, double poleIm, int order);
    CAResult residueTheorem(const std::string &f,
                            const std::vector<std::pair<double, double>> &poles);

    // Contour integrals (numerical)
    // Contour: parametric z(t)=x(t)+iy(t), t in [a,b]
    CAResult contourIntegral(const std::string &f,
                             const std::string &xParam,
                             const std::string &yParam,
                             const std::string &tVar,
                             double a, double b);

    // Classical real integrals via residues
    CAResult improperByResidues(const std::string &f, const std::string &x); // ∫_{-∞}^∞ f dx
    CAResult trigIntByResidues(const std::string &f,
                               const std::string &theta); // ∫_0^{2π} f dθ

    // Conformal maps
    CAResult mobiusTransform(double a, double b, double c, double d,
                             double zRe, double zIm);
    CAResult joukowskiTransform(double zRe, double zIm, double lambda);
    CAResult schwarzChristoffel(const std::vector<double> &angles,
                                const std::vector<double> &prevertices);

    // Specific complex functions
    CAResult riemannZeta(double re, double im, int terms); // ζ(s)
    CAResult gammaFunctionC(double re, double im);         // Γ(z)
    CAResult betaFunctionC(double re1, double im1, double re2, double im2);

    CAResult dispatch(const std::string &op, const std::string &json);

} // namespace ComplexAnalysis
#endif