#pragma once
// calculus/integration/Multivariable.h
// regions and with coordinate system changes.
// Methods:
//   ITERATED INTEGRALS (rectangular regions)
//     doubleIntegral   ∫∫_R f(x,y) dA   over [ax,bx] × [ay,by]
//   VARIABLE-BOUND INTEGRALS
//     — inner bounds are expressions in the outer variable
//   COORDINATE SYSTEM CHANGES
//     polarIntegral       ∫∫ f(r,θ) r dr dθ       (polar)
//     cylindricalIntegral ∫∫∫ f(r,θ,z) r dr dθ dz (cylindrical)
// variables would require a CAS far beyond this scope.

#ifndef MULTIVARIABLE_HPP
#define MULTIVARIABLE_HPP

#include "../core/Expression.hpp"
#include "Numerical.hpp"
#include <string>
#include <functional>

namespace Calculus
{

    using Func2D = std::function<double(double, double)>;
    using Func3D = std::function<double(double, double, double)>;

    // Result

    struct MultiIntResult
    {
        double value = 0.0;
        std::string method;
        bool ok = true;
        std::string error;
    };

    // Double integral over rectangular region  [ax, bx] × [ay, by]

    MultiIntResult doubleIntegral(const Func2D &f,
                                  double ax, double bx,
                                  double ay, double by,
                                  int nx = 50, int ny = 50);

    MultiIntResult tripleIntegral(const Func3D &f,
                                  double ax, double bx,
                                  double ay, double by,
                                  double az, double bz,
                                  int nx = 20, int ny = 20, int nz = 20);

    // Double integral with variable inner bounds
    // ∫_{ax}^{bx} [ ∫_{ayFunc(x)}^{byFunc(x)} f(x,y) dy ] dx

    MultiIntResult doubleIntegralVarBounds(
        const Func2D &f,
        double ax, double bx,
        const std::function<double(double)> &ayFunc,
        const std::function<double(double)> &byFunc,
        int nx = 50, int ny = 50);

    // Polar coordinates: ∫∫ f(r,θ) r dr dθ
    // r ∈ [r1, r2],  θ ∈ [t1, t2]

    MultiIntResult polarIntegral(const Func2D &f_r_theta,
                                 double r1, double r2,
                                 double t1, double t2,
                                 int nr = 50, int nt = 50);

    // Cylindrical: ∫∫∫ f(r,θ,z) r dr dθ dz

    MultiIntResult cylindricalIntegral(const Func3D &f_r_theta_z,
                                       double r1, double r2,
                                       double t1, double t2,
                                       double z1, double z2,
                                       int nr = 20, int nt = 20, int nz = 20);

    // Spherical: ∫∫∫ f(ρ,θ,φ) ρ²sin(φ) dρ dθ dφ
    //   ρ ∈ [rho1, rho2]
    //   θ ∈ [theta1, theta2]   (azimuthal)
    //   φ ∈ [phi1, phi2]       (polar, 0 to π)

    MultiIntResult sphericalIntegral(const Func3D &f_rho_theta_phi,
                                     double rho1, double rho2,
                                     double theta1, double theta2,
                                     double phi1, double phi2,
                                     int nr = 20, int nt = 20, int np = 20);

    // Formatted entry points (string expressions in, number out)

    // Double integral: expr in x and y, over [ax,bx]×[ay,by]
    MultiIntResult computeDoubleIntegral(const std::string &exprStr,
                                         const std::string &varX,
                                         const std::string &varY,
                                         double ax, double bx,
                                         double ay, double by);

    // Double integral with variable bounds:
    // inner lower/upper bounds are expressions in varX
    MultiIntResult computeDoubleIntegralVar(const std::string &exprStr,
                                            const std::string &varX,
                                            const std::string &varY,
                                            double ax, double bx,
                                            const std::string &ayExpr,
                                            const std::string &byExpr);

    // Triple integral
    MultiIntResult computeTripleIntegral(const std::string &exprStr,
                                         const std::string &varX,
                                         const std::string &varY,
                                         const std::string &varZ,
                                         double ax, double bx,
                                         double ay, double by,
                                         double az, double bz);

    // Polar integral: expr in r and theta
    MultiIntResult computePolarIntegral(const std::string &exprStr,
                                        const std::string &varR,
                                        const std::string &varTheta,
                                        double r1, double r2,
                                        double t1, double t2);

    // Cylindrical integral
    MultiIntResult computeCylindricalIntegral(const std::string &exprStr,
                                              const std::string &varR,
                                              const std::string &varTheta,
                                              const std::string &varZ,
                                              double r1, double r2,
                                              double t1, double t2,
                                              double z1, double z2);

    // Spherical integral
    MultiIntResult computeSphericalIntegral(const std::string &exprStr,
                                            const std::string &varRho,
                                            const std::string &varTheta,
                                            const std::string &varPhi,
                                            double rho1, double rho2,
                                            double theta1, double theta2,
                                            double phi1, double phi2);

}

#endif // MULTIVARIABLE_HPP