// calculus/integration/Multivariable.cpp

#include "Multivariable.hpp"
#include "../core/Week4Bridge.hpp"
#include <cmath>
#include <iomanip>

namespace Calculus
{

    // Internal: 1D Romberg shorthand (8 rows, high accuracy)

    static double romberg1D(const Func1D &f, double a, double b)
    {
        if (!std::isfinite(a) || !std::isfinite(b))
            return 0.0; // skip degenerate bounds
        if (std::abs(b - a) < 1e-14)
            return 0.0;
        return romberg(f, a, b, 8).value;
    }

    // doubleIntegral — ∫∫ f(x,y) dA  over [ax,bx]×[ay,by]
    // Implementation: outer integral over x using Romberg;
    // for each x, inner integral over y using Romberg.

    MultiIntResult doubleIntegral(const Func2D &f, const double ax, const double bx, const double ay, double by, int nx, int ny)
    {
        MultiIntResult out;
        try
        {
            const Func1D outer = [&](const double x) -> double
            {
                const Func1D inner = [&](const double y) -> double
                {
                    return f(x, y);
                };
                return romberg1D(inner, ay, by);
            };
            out.value = romberg1D(outer, ax, bx);
            out.method = "Iterated Romberg (rectangular)";
        }
        catch (const std::exception &ex)
        {
            out.ok = false;
            out.error = ex.what();
        }
        return out;
    }

    // tripleIntegral - ∫∫∫ f(x,y,z) dV  over box

    MultiIntResult tripleIntegral(const Func3D &f, double ax, double bx, double ay, double by, double az, double bz, int nx, int ny, int nz)
    {
        MultiIntResult out;
        try
        {
            Func1D outer = [&](double x) -> double
            {
                Func1D mid = [&](double y) -> double
                {
                    Func1D inner = [&](double z) -> double
                    {
                        return f(x, y, z);
                    };
                    return romberg1D(inner, az, bz); // fixed: was (ax,bz)
                };
                return romberg1D(mid, ay, by);
            };
            out.value = romberg1D(outer, ax, bx);
            out.method = "Iterated Romberg (box)";
        }
        catch (const std::exception &ex)
        {
            out.ok = false;
            out.error = ex.what();
        }
        return out;
    }

    // doubleIntegralVarBounds
    // ∫_{ax}^{bx} [ ∫_{ayFunc(x)}^{byFunc(x)} f(x,y) dy ] dx

    MultiIntResult doubleIntegralVarBounds(
        const Func2D &f,
        double ax, double bx,
        const std::function<double(double)> &ayFunc,
        const std::function<double(double)> &byFunc,
        int, int)
    {

        MultiIntResult out;
        try
        {
            Func1D outer = [&](double x) -> double
            {
                double lo = ayFunc(x), hi = byFunc(x);
                if (hi <= lo)
                    return 0.0;
                Func1D inner = [&](double y)
                { return f(x, y); };
                return romberg1D(inner, lo, hi);
            };
            out.value = romberg1D(outer, ax, bx);
            out.method = "Iterated Romberg (variable bounds)";
        }
        catch (const std::exception &ex)
        {
            out.ok = false;
            out.error = ex.what();
        }
        return out;
    }

    // polarIntegral — ∫∫ f(r,θ) · r  dr dθ
    // (not pre-multiplied).

    MultiIntResult polarIntegral(const Func2D &f_r_theta,
                                 double r1, double r2,
                                 double t1, double t2,
                                 int, int)
    {
        MultiIntResult out;
        try
        {
            // Include Jacobian r
            Func2D g = [&](double r, double theta) -> double
            {
                return r * f_r_theta(r, theta);
            };
            Func1D outer = [&](double theta) -> double
            {
                Func1D inner = [&](double r)
                { return g(r, theta); };
                return romberg1D(inner, r1, r2);
            };
            out.value = romberg1D(outer, t1, t2);
            out.method = "Polar (Jacobian r included)";
        }
        catch (const std::exception &ex)
        {
            out.ok = false;
            out.error = ex.what();
        }
        return out;
    }

    // cylindricalIntegral — ∫∫∫ f(r,θ,z) · r  dr dθ dz

    MultiIntResult cylindricalIntegral(const Func3D &f_r_theta_z,
                                       double r1, double r2,
                                       double t1, double t2,
                                       double z1, double z2,
                                       int, int, int)
    {
        MultiIntResult out;
        try
        {
            Func1D outerZ = [&](double z) -> double
            {
                Func1D outerT = [&](double theta) -> double
                {
                    Func1D innerR = [&](double r) -> double
                    {
                        return r * f_r_theta_z(r, theta, z);
                    };
                    return romberg1D(innerR, r1, r2);
                };
                return romberg1D(outerT, t1, t2);
            };
            out.value = romberg1D(outerZ, z1, z2);
            out.method = "Cylindrical (Jacobian r included)";
        }
        catch (const std::exception &ex)
        {
            out.ok = false;
            out.error = ex.what();
        }
        return out;
    }

    // sphericalIntegral — ∫∫∫ f(ρ,θ,φ) · ρ²sin(φ)  dρ dθ dφ

    MultiIntResult sphericalIntegral(const Func3D &f_rho_theta_phi,
                                     double rho1, double rho2,
                                     double theta1, double theta2,
                                     double phi1, double phi2,
                                     int, int, int)
    {
        MultiIntResult out;
        try
        {
            Func1D outerPhi = [&](double phi) -> double
            {
                Func1D outerTheta = [&](double theta) -> double
                {
                    Func1D innerRho = [&](double rho) -> double
                    {
                        // Jacobian: rho^2 * sin(phi)
                        return rho * rho * std::sin(phi) * f_rho_theta_phi(rho, theta, phi);
                    };
                    return romberg1D(innerRho, rho1, rho2);
                };
                return romberg1D(outerTheta, theta1, theta2);
            };
            out.value = romberg1D(outerPhi, phi1, phi2);
            out.method = "Spherical (Jacobian rho^2 sin(phi) included)";
        }
        catch (const std::exception &ex)
        {
            out.ok = false;
            out.error = ex.what();
        }
        return out;
    }

    // Formatted entry points

    // Helper: build a 2-variable evaluator
    static Func2D make2DEval(const ExprPtr &expr,
                             const std::string &vx,
                             const std::string &vy)
    {
        return [expr, vx, vy](double x, double y) -> double
        {
            return evaluate(expr, {{vx, x}, {vy, y}});
        };
    }

    static Func3D make3DEval(const ExprPtr &expr,
                             const std::string &vx,
                             const std::string &vy,
                             const std::string &vz)
    {
        return [expr, vx, vy, vz](double x, double y, double z) -> double
        {
            return evaluate(expr, {{vx, x}, {vy, y}, {vz, z}});
        };
    }

    MultiIntResult computeDoubleIntegral(const std::string &exprStr,
                                         const std::string &varX,
                                         const std::string &varY,
                                         double ax, double bx,
                                         double ay, double by)
    {
        try
        {
            ExprPtr expr = parse(exprStr);
            return doubleIntegral(make2DEval(expr, varX, varY), ax, bx, ay, by);
        }
        catch (const std::exception &ex)
        {
            return {0.0, "", false, ex.what()};
        }
    }

    MultiIntResult computeDoubleIntegralVar(const std::string &exprStr,
                                            const std::string &varX,
                                            const std::string &varY,
                                            double ax, double bx,
                                            const std::string &ayExpr,
                                            const std::string &byExpr)
    {
        try
        {
            ExprPtr expr = parse(exprStr);
            ExprPtr ayE = parse(ayExpr);
            ExprPtr byE = parse(byExpr);
            Func2D f = make2DEval(expr, varX, varY);
            auto ayFunc = [ayE, varX](double x)
            { return evaluate(ayE, {{varX, x}}); };
            auto byFunc = [byE, varX](double x)
            { return evaluate(byE, {{varX, x}}); };
            return doubleIntegralVarBounds(f, ax, bx, ayFunc, byFunc);
        }
        catch (const std::exception &ex)
        {
            return {0.0, "", false, ex.what()};
        }
    }

    MultiIntResult computeTripleIntegral(const std::string &exprStr,
                                         const std::string &varX,
                                         const std::string &varY,
                                         const std::string &varZ,
                                         double ax, double bx,
                                         double ay, double by,
                                         double az, double bz)
    {
        try
        {
            ExprPtr expr = parse(exprStr);
            return tripleIntegral(make3DEval(expr, varX, varY, varZ),
                                  ax, bx, ay, by, az, bz);
        }
        catch (const std::exception &ex)
        {
            return {0.0, "", false, ex.what()};
        }
    }

    MultiIntResult computePolarIntegral(const std::string &exprStr,
                                        const std::string &varR,
                                        const std::string &varTheta,
                                        double r1, double r2,
                                        double t1, double t2)
    {
        try
        {
            ExprPtr expr = parse(exprStr);
            Func2D f = make2DEval(expr, varR, varTheta);
            return polarIntegral(f, r1, r2, t1, t2);
        }
        catch (const std::exception &ex)
        {
            return {0.0, "", false, ex.what()};
        }
    }

    MultiIntResult computeCylindricalIntegral(const std::string &exprStr,
                                              const std::string &varR,
                                              const std::string &varTheta,
                                              const std::string &varZ,
                                              double r1, double r2,
                                              double t1, double t2,
                                              double z1, double z2)
    {
        try
        {
            ExprPtr expr = parse(exprStr);
            Func3D f = make3DEval(expr, varR, varTheta, varZ);
            return cylindricalIntegral(f, r1, r2, t1, t2, z1, z2);
        }
        catch (const std::exception &ex)
        {
            return {0.0, "", false, ex.what()};
        }
    }

    MultiIntResult computeSphericalIntegral(const std::string &exprStr,
                                            const std::string &varRho,
                                            const std::string &varTheta,
                                            const std::string &varPhi,
                                            double rho1, double rho2,
                                            double theta1, double theta2,
                                            double phi1, double phi2)
    {
        try
        {
            ExprPtr expr = parse(exprStr);
            Func3D f = make3DEval(expr, varRho, varTheta, varPhi);
            return sphericalIntegral(f, rho1, rho2, theta1, theta2, phi1, phi2);
        }
        catch (const std::exception &ex)
        {
            return {0.0, "", false, ex.what()};
        }
    }

}