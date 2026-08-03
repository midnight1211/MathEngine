#pragma once
// Geometry.h — CoreEngine prefix: "geo:<op>|{json}"

#ifndef GEOMETRY_HPP
#define GEOMETRY_HPP

#include <string>
#include <vector>
#include <tuple>

namespace Geometry
{

    struct GeoResult
    {
        bool ok = true;
        std::string value;
        std::string detail;
        std::string error;
    };

    using P2 = std::pair<double, double>;
    using P3 = std::tuple<double, double, double>;
    using Vec = std::vector<double>;

    // ── 2D Analytic geometry ──────────────────────────────────────────────────────
    GeoResult distance2D(double x1, double y1, double x2, double y2);
    GeoResult midpoint2D(double x1, double y1, double x2, double y2);
    GeoResult slopeLine(double x1, double y1, double x2, double y2);
    GeoResult lineEquation(double x1, double y1, double x2, double y2); // ax+by+c=0
    GeoResult lineIntersection(double a1, double b1, double c1,
                               double a2, double b2, double c2);
    GeoResult perpendicular(double a, double b, double c, double px, double py);
    GeoResult pointLineDistance(double a, double b, double c, double px, double py);
    GeoResult angleLines(double m1, double m2);
    GeoResult areaTriangle2D(double x1, double y1, double x2, double y2,
                             double x3, double y3);
    GeoResult centroid(const std::vector<P2> &points);
    GeoResult circumcircle(double x1, double y1, double x2, double y2,
                           double x3, double y3);
    GeoResult incircle(double x1, double y1, double x2, double y2,
                       double x3, double y3);
    GeoResult polygonArea(const std::vector<P2> &vertices); // shoelace
    GeoResult polygonPerimeter(const std::vector<P2> &vertices);
    GeoResult isConvex(const std::vector<P2> &vertices);
    GeoResult convexHull(const std::vector<P2> &points); // Graham scan
    GeoResult pointInPolygon(const std::vector<P2> &vertices, double px, double py);

    // ── Conic sections ────────────────────────────────────────────────────────────
    // General form: Ax² + Bxy + Cy² + Dx + Ey + F = 0
    GeoResult classifyConic(double A, double B, double C, double D, double E, double F);
    GeoResult circleEquation(double cx, double cy, double r);            // (x-h)²+(y-k)²=r²
    GeoResult circleFromGeneral(double A, double D, double E, double F); // x²+y²+Dx+Ey+F=0
    GeoResult ellipseEquation(double cx, double cy, double a, double b, double theta);
    GeoResult ellipseFromGeneral(double A, double B, double C, double D, double E, double F);
    GeoResult hyperbolaEquation(double cx, double cy, double a, double b, bool horizontal);
    GeoResult parabolaEquation(double h, double k, double p, bool vertical);
    GeoResult conicFocus(double A, double B, double C, double D, double E, double F);
    GeoResult eccentricity(double A, double B, double C, double D, double E, double F);

    // ── 3D Geometry ───────────────────────────────────────────────────────────────
    GeoResult distance3D(double x1, double y1, double z1,
                         double x2, double y2, double z2);
    GeoResult midpoint3D(double x1, double y1, double z1,
                         double x2, double y2, double z2);
    GeoResult crossProduct(double ax, double ay, double az,
                           double bx, double by, double bz);
    GeoResult dotProduct3D(double ax, double ay, double az,
                           double bx, double by, double bz);
    GeoResult angleBetween3D(double ax, double ay, double az,
                             double bx, double by, double bz);
    GeoResult planeEquation(double x1, double y1, double z1,
                            double x2, double y2, double z2,
                            double x3, double y3, double z3); // plane through 3 pts
    GeoResult planeFromNormal(double nx, double ny, double nz, double px, double py, double pz);
    GeoResult planeLineIntersect(double a, double b, double c, double d, // ax+by+cz=d
                                 double lx, double ly, double lz,        // point on line
                                 double dx, double dy, double dz);       // direction
    GeoResult pointPlaneDistance(double a, double b, double c, double d,
                                 double px, double py, double pz);
    GeoResult twoPlaneIntersect(double a1, double b1, double c1, double d1,
                                double a2, double b2, double c2, double d2);
    GeoResult skewLines(double x1, double y1, double z1, double dx1, double dy1, double dz1,
                        double x2, double y2, double z2, double dx2, double dy2, double dz2);
    GeoResult sphereEquation(double cx, double cy, double cz, double r);
    GeoResult sphereLintersect(double cx, double cy, double cz, double r,
                               double lx, double ly, double lz,
                               double dx, double dy, double dz);

    // ── Parametric curves ─────────────────────────────────────────────────────────
    GeoResult parametricLength(const std::string &x, const std::string &y,
                               const std::string &t, double t0, double t1);
    GeoResult parametricArea(const std::string &x, const std::string &y,
                             const std::string &t, double t0, double t1);
    GeoResult curvature2D(const std::string &x, const std::string &y,
                          const std::string &t, double t0);
    GeoResult tangentNormal(const std::string &x, const std::string &y,
                            const std::string &t, double t0);
    GeoResult envelopeCurve(const std::string &F, const std::string &x,
                            const std::string &y, const std::string &param);

    // ── Polar coordinates ─────────────────────────────────────────────────────────
    GeoResult polarToRect(double r, double theta);
    GeoResult rectToPolar(double x, double y);
    GeoResult polarArea(const std::string &r, const std::string &theta,
                        double t0, double t1);
    GeoResult polarArcLength(const std::string &r, const std::string &theta,
                             double t0, double t1);

    // ── Transformations ───────────────────────────────────────────────────────────
    GeoResult rotate2D(double px, double py, double cx, double cy, double angle);
    GeoResult scale2D(double px, double py, double cx, double cy, double sx, double sy);
    GeoResult reflect2D(double px, double py, double ax, double ay, double bx, double by);
    GeoResult affineTransform(const std::vector<Vec> &M, double px, double py);           // 2x3 matrix
    GeoResult compositeTransform(const std::vector<Vec> &M1, const std::vector<Vec> &M2); // 3x3

    // ── Dispatch ──────────────────────────────────────────────────────────────────
    GeoResult dispatch(const std::string &op, const std::string &json);

} // namespace Geometry
#endif