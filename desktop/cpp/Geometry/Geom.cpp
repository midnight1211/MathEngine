// Geometry.cpp

#include "Geom.hpp"
#include "../CommonUtils.hpp"

// ── CommonUtils aliases (replaces previous local duplicates) ──────────────────
static auto getP = [](const std::string &j, const std::string &k, const std::string &d = "")
{ return cu_getStr(j, k, d); };
static auto getN = [](const std::string &j, const std::string &k, double d = 0.0)
{ return cu_getNum(j, k, d); };
static auto parseVec = [](const std::string &s)
{ return cu_parseVecD(s); };
static auto parseMat = [](const std::string &s)
{ return cu_parseMat(s); };
static auto fmt = [](double v, int p = 8)
{ return cu_fmt(v, p); };

#include "../Calculus/Calculus.hpp"
#include <cmath>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <stdexcept>
#include <sstream>

namespace Geometry
{

    static GeoResult ok(const std::string &v, const std::string &d = "") { return {true, v, d, ""}; }
    static GeoResult err(const std::string &m) { return {false, "", "", m}; }
    static const double PI = M_PI;

    static double diff1(const std::string &f, const std::string &var, double val)
    {
        auto e = Calculus::parse(f);
        auto d = Calculus::simplify(Calculus::diff(e, var));
        return Calculus::evaluate(d, {{var, val}});
    }

    // 2D
    GeoResult distance2D(double x1, double y1, double x2, double y2)
    {
        double d = std::sqrt((x2 - x1) * (x2 - x1) + (y2 - y1) * (y2 - y1));
        return ok(fmt(d));
    }
    GeoResult midpoint2D(double x1, double y1, double x2, double y2) { return ok("(" + fmt((x1 + x2) / 2) + ", " + fmt((y1 + y2) / 2) + ")"); }
    GeoResult slopeLine(double x1, double y1, double x2, double y2)
    {
        if (std::abs(x2 - x1) < 1e-12)
            return ok("undefined");
        double m = (y2 - y1) / (x2 - x1);
        return ok(fmt(m));
    }
    GeoResult lineEquation(double x1, double y1, double x2, double y2)
    {
        if (std::abs(x2 - x1) < 1e-12)
            return ok("x=" + fmt(x1));
        double m = (y2 - y1) / (x2 - x1), b = y1 - m * x1;
        std::ostringstream ss;
        ss << "y=" << fmt(m) << "x" << (b >= 0 ? "+" : "") << fmt(b);
        return ok(ss.str());
    }
    GeoResult lineIntersection(double a1, double b1, double c1, double a2, double b2, double c2)
    {
        double det = a1 * b2 - a2 * b1;
        if (std::abs(det) < 1e-12)
            return ok("parallel or identical");
        double x = (c1 * b2 - c2 * b1) / det, y = (a1 * c2 - a2 * c1) / det;
        return ok("(" + fmt(x) + "," + fmt(y) + ")");
    }
    GeoResult perpendicular(double a, double b, double c, double px, double py)
    {
        double t = -(a * px + b * py + c) / (a * a + b * b);
        double qx = px + a * t, qy = py + b * t;
        return ok("(" + fmt(qx) + "," + fmt(qy) + ")", "Foot of perpendicular");
    }
    GeoResult pointLineDistance(double a, double b, double c, double px, double py)
    {
        return ok(fmt(std::abs(a * px + b * py + c) / std::sqrt(a * a + b * b)));
    }
    GeoResult angleLines(double m1, double m2)
    {
        double theta = std::atan(std::abs((m2 - m1) / (1 + m1 * m2))) * 180 / PI;
        return ok(fmt(theta) + "deg");
    }
    GeoResult areaTriangle2D(double x1, double y1, double x2, double y2, double x3, double y3)
    {
        return ok(fmt(std::abs((x2 - x1) * (y3 - y1) - (x3 - x1) * (y2 - y1)) / 2));
    }
    GeoResult centroid(const std::vector<P2> &pts)
    {
        double cx = 0, cy = 0;
        for (auto &p : pts)
        {
            cx += p.first;
            cy += p.second;
        }
        cx /= pts.size();
        cy /= pts.size();
        return ok("(" + fmt(cx) + "," + fmt(cy) + ")");
    }
    GeoResult polygonArea(const std::vector<P2> &v)
    {
        double A = 0;
        int n = v.size();
        for (int i = 0; i < n; ++i)
        {
            auto &p = v[i];
            auto &q = v[(i + 1) % n];
            A += p.first * q.second - q.first * p.second;
        }
        return ok(fmt(std::abs(A) / 2));
    }
    GeoResult polygonPerimeter(const std::vector<P2> &v)
    {
        double P = 0;
        int n = v.size();
        for (int i = 0; i < n; ++i)
        {
            auto &a = v[i];
            auto &b = v[(i + 1) % n];
            P += std::sqrt((b.first - a.first) * (b.first - a.first) + (b.second - a.second) * (b.second - a.second));
        }
        return ok(fmt(P));
    }
    GeoResult circumcircle(double x1, double y1, double x2, double y2, double x3, double y3)
    {
        double D = 2 * (x1 * (y2 - y3) + x2 * (y3 - y1) + x3 * (y1 - y2));
        if (std::abs(D) < 1e-12)
            return err("Collinear");
        double ux = ((x1 * x1 + y1 * y1) * (y2 - y3) + (x2 * x2 + y2 * y2) * (y3 - y1) + (x3 * x3 + y3 * y3) * (y1 - y2)) / D;
        double uy = ((x1 * x1 + y1 * y1) * (x3 - x2) + (x2 * x2 + y2 * y2) * (x1 - x3) + (x3 * x3 + y3 * y3) * (x2 - x1)) / D;
        double r = std::sqrt((x1 - ux) * (x1 - ux) + (y1 - uy) * (y1 - uy));
        return ok("centre=(" + fmt(ux) + "," + fmt(uy) + ") r=" + fmt(r));
    }
    GeoResult incircle(double x1, double y1, double x2, double y2, double x3, double y3)
    {
        double a = std::sqrt((x2 - x3) * (x2 - x3) + (y2 - y3) * (y2 - y3));
        double b = std::sqrt((x1 - x3) * (x1 - x3) + (y1 - y3) * (y1 - y3));
        double c = std::sqrt((x1 - x2) * (x1 - x2) + (y1 - y2) * (y1 - y2));
        double s = a + b + c, ix = (a * x1 + b * x2 + c * x3) / s, iy = (a * y1 + b * y2 + c * y3) / s;
        double A = std::abs((x2 - x1) * (y3 - y1) - (x3 - x1) * (y2 - y1)) / 2, r = 2 * A / s;
        return ok("centre=(" + fmt(ix) + "," + fmt(iy) + ") r=" + fmt(r));
    }
    GeoResult isConvex(const std::vector<P2> &v)
    {
        int n = v.size();
        bool pos = false, neg = false;
        for (int i = 0; i < n; ++i)
        {
            auto &a = v[i];
            auto &b = v[(i + 1) % n];
            auto &c = v[(i + 2) % n];
            double cross = (b.first - a.first) * (c.second - b.second) - (b.second - a.second) * (c.first - b.first);
            if (cross > 0)
                pos = true;
            if (cross < 0)
                neg = true;
        }
        return ok((!pos || !neg) ? "true" : "false");
    }
    GeoResult convexHull(const std::vector<P2> &pts)
    {
	if (pts.size() < 3)
		return err("Convex hull requires at least 3 points (format: x1,y1;x2,y2;x3,y3)");
        auto hull = pts;
        std::sort(hull.begin(), hull.end());
	hull.erase(std::unique(hull.begin(), hull.end()), hull.end());
	if (hull.size() < 3)
		return err("Convex hull requires at least 3 distinct points");		
        auto cross = [](P2 O, P2 A, P2 B)
        { return (A.first - O.first) * (B.second - O.second) - (A.second - O.second) * (B.first - O.first); };
        std::vector<P2> H;
        for (int i = 0; i < (int)hull.size(); ++i)
        {
            while (H.size() >= 2 && cross(H[H.size() - 2], H.back(), hull[i]) <= 0)
                H.pop_back();
            H.push_back(hull[i]);
        }
        for (int i = (int)hull.size() - 2, t = H.size() + 1; i >= 0; --i)
        {
            while ((int)H.size() >= t && cross(H[H.size() - 2], H.back(), hull[i]) <= 0)
                H.pop_back();
            H.push_back(hull[i]);
        }
        H.pop_back();
        std::ostringstream ss;
        for (auto &p : H)
            ss << "(" << fmt(p.first, 4) << "," << fmt(p.second, 4) << ") ";
        return ok(ss.str());
    }
    GeoResult pointInPolygon(const std::vector<P2> &v, double px, double py)
    {
        int n = v.size(), c = 0;
        for (int i = 0; i < n; ++i)
        {
            auto &a = v[i];
            auto &b = v[(i + 1) % n];
            if (((a.second <= py && py < b.second) || (b.second <= py && py < a.second)) &&
                px < (b.first - a.first) * (py - a.second) / (b.second - a.second) + a.first)
                c++;
        }
        return ok(c % 2 ? "inside" : "outside");
    }

    // Conics
    GeoResult classifyConic(double A, double B, double C, double D, double E, double F)
    {
        double disc = B * B - 4 * A * C;
        std::string t = (std::abs(disc) < 1e-10) ? "Parabola" : (disc < 0 ? (std::abs(A - C) < 1e-10 && std::abs(B) < 1e-10 ? "Circle" : "Ellipse") : "Hyperbola");
        std::ostringstream ss;
        ss << "B2-4AC=" << disc << "\nType: " << t;
        return ok(t, ss.str());
    }
    GeoResult circleEquation(double cx, double cy, double r)
    {
        std::ostringstream ss;
        ss << "(x-" << cx << ")2+(y-" << cy << ")2=" << r * r << "  r=" << r << " Area=" << fmt(PI * r * r);
        return ok(ss.str());
    }
    GeoResult circleFromGeneral(double A, double D, double E, double F)
    {
        double h = -D / 2, k = -E / 2, r2 = D * D / 4 + E * E / 4 - F;
        if (r2 < 0)
            return err("Imaginary");
        return circleEquation(h, k, std::sqrt(r2));
    }
    GeoResult ellipseEquation(double cx, double cy, double a, double b, double theta)
    {
        double c = std::sqrt(std::abs(a * a - b * b)), e = c / a;
        std::ostringstream ss;
        ss << "(x-" << cx << ")2/" << a * a << "+(y-" << cy << ")2/" << b * b << "=1  e=" << fmt(e) << "  Area=pi*a*b=" << fmt(PI * a * b);
        return ok(ss.str());
    }
    GeoResult ellipseFromGeneral(double A, double B, double C, double D, double E, double F) { return classifyConic(A, B, C, D, E, F); }
    GeoResult hyperbolaEquation(double cx, double cy, double a, double b, bool horiz)
    {
        double c = std::sqrt(a * a + b * b), e = c / a;
        std::ostringstream ss;
        if (horiz)
            ss << "(x-" << cx << ")2/" << a * a << "-(y-" << cy << ")2/" << b * b << "=1";
        else
            ss << "(y-" << cy << ")2/" << a * a << "-(x-" << cx << ")2/" << b * b << "=1";
        ss << "  e=" << fmt(e);
        return ok(ss.str());
    }
    GeoResult parabolaEquation(double h, double k, double p, bool vert)
    {
        std::ostringstream ss;
        if (vert)
            ss << "(x-" << h << ")2=" << 4 * p << "(y-" << k << ")  focus=(" << h << "," << fmt(k + p) << ")";
        else
            ss << "(y-" << k << ")2=" << 4 * p << "(x-" << h << ")  focus=(" << fmt(h + p) << "," << k << ")";
        return ok(ss.str());
    }
    GeoResult conicFocus(double A, double B, double C, double D, double E, double F) { return classifyConic(A, B, C, D, E, F); }
    GeoResult eccentricity(double A, double B, double C, double D, double E, double F)
    {
        double disc = B * B - 4 * A * C;
        if (std::abs(disc) < 1e-10)
            return ok("1", "Parabola");
        return ok(disc < 0 ? "0<e<1" : "e>1", disc < 0 ? "Ellipse" : "Hyperbola");
    }

    // 3D
    GeoResult distance3D(double x1, double y1, double z1, double x2, double y2, double z2)
    {
        return ok(fmt(std::sqrt((x2 - x1) * (x2 - x1) + (y2 - y1) * (y2 - y1) + (z2 - z1) * (z2 - z1))));
    }
    GeoResult midpoint3D(double x1, double y1, double z1, double x2, double y2, double z2)
    {
        return ok("(" + fmt((x1 + x2) / 2) + "," + fmt((y1 + y2) / 2) + "," + fmt((z1 + z2) / 2) + ")");
    }
    GeoResult crossProduct(double ax, double ay, double az, double bx, double by, double bz)
    {
        double cx = ay * bz - az * by, cy = az * bx - ax * bz, cz = ax * by - ay * bx;
        return ok("(" + fmt(cx) + "," + fmt(cy) + "," + fmt(cz) + ")");
    }
    GeoResult dotProduct3D(double ax, double ay, double az, double bx, double by, double bz) { return ok(fmt(ax * bx + ay * by + az * bz)); }
    GeoResult angleBetween3D(double ax, double ay, double az, double bx, double by, double bz)
    {
        double d = ax * bx + ay * by + az * bz, ma = std::sqrt(ax * ax + ay * ay + az * az), mb = std::sqrt(bx * bx + by * by + bz * bz);
        return ok(fmt(std::acos(std::max(-1.0, std::min(1.0, d / (ma * mb)))) * 180 / PI) + "deg");
    }
    GeoResult planeEquation(double x1, double y1, double z1, double x2, double y2, double z2, double x3, double y3, double z3)
    {
        double ax = x2 - x1, ay = y2 - y1, az = z2 - z1, bx = x3 - x1, by = y3 - y1, bz = z3 - z1;
        double nx = ay * bz - az * by, ny = az * bx - ax * bz, nz = ax * by - ay * bx, d = nx * x1 + ny * y1 + nz * z1;
        return ok(fmt(nx) + "x+" + fmt(ny) + "y+" + fmt(nz) + "z=" + fmt(d));
    }
    GeoResult planeFromNormal(double nx, double ny, double nz, double px, double py, double pz)
    {
        return ok(fmt(nx) + "x+" + fmt(ny) + "y+" + fmt(nz) + "z=" + fmt(nx * px + ny * py + nz * pz));
    }
    GeoResult pointPlaneDistance(double a, double b, double c, double d, double px, double py, double pz)
    {
        return ok(fmt(std::abs(a * px + b * py + c * pz - d) / std::sqrt(a * a + b * b + c * c)));
    }
    GeoResult planeLineIntersect(double a, double b, double c, double d, double lx, double ly, double lz, double dx, double dy, double dz)
    {
        double den = a * dx + b * dy + c * dz;
        if (std::abs(den) < 1e-12)
            return ok("No intersection");
        double t = (d - a * lx - b * ly - c * lz) / den;
        return ok("(" + fmt(lx + t * dx) + "," + fmt(ly + t * dy) + "," + fmt(lz + t * dz) + ")");
    }
    GeoResult twoPlaneIntersect(double a1, double b1, double c1, double d1, double a2, double b2, double c2, double d2)
    {
        double dx = b1 * c2 - c1 * b2, dy = c1 * a2 - a1 * c2, dz = a1 * b2 - b1 * a2;
        double mag = std::sqrt(dx * dx + dy * dy + dz * dz);
        if (mag < 1e-12)
            return ok("Parallel or identical");
        return ok("Direction: (" + fmt(dx / mag) + "," + fmt(dy / mag) + "," + fmt(dz / mag) + ")");
    }
    GeoResult skewLines(double x1, double y1, double z1, double dx1, double dy1, double dz1,
                        double x2, double y2, double z2, double dx2, double dy2, double dz2)
    {
        double nx = dy1 * dz2 - dz1 * dy2, ny = dz1 * dx2 - dx1 * dz2, nz = dx1 * dy2 - dy1 * dx2;
        double mag = std::sqrt(nx * nx + ny * ny + nz * nz);
        if (mag < 1e-12)
            return ok("0", "Parallel lines");
        double dist = std::abs((x2 - x1) * nx + (y2 - y1) * ny + (z2 - z1) * nz) / mag;
        return ok(fmt(dist));
    }
    GeoResult sphereEquation(double cx, double cy, double cz, double r)
    {
        std::ostringstream ss;
        ss << "(x-" << cx << ")2+(y-" << cy << ")2+(z-" << cz << ")2=" << r * r << "  r=" << r << "  V=" << fmt(4 * PI * r * r * r / 3);
        return ok(ss.str());
    }
    GeoResult sphereLintersect(double cx, double cy, double cz, double r, double lx, double ly, double lz, double dx, double dy, double dz)
    {
        double fx = lx - cx, fy = ly - cy, fz = lz - cz;
        double a = dx * dx + dy * dy + dz * dz, b = 2 * (fx * dx + fy * dy + fz * dz), c_ = fx * fx + fy * fy + fz * fz - r * r;
        double disc = b * b - 4 * a * c_;
        if (disc < 0)
            return ok("No intersection");
        if (std::abs(disc) < 1e-10)
        {
            double t = -b / (2 * a);
            return ok("Tangent at (" + fmt(lx + t * dx) + "," + fmt(ly + t * dy) + "," + fmt(lz + t * dz) + ")");
        }
        double t1 = (-b - std::sqrt(disc)) / (2 * a), t2 = (-b + std::sqrt(disc)) / (2 * a);
        return ok("t1=" + fmt(t1) + " t2=" + fmt(t2));
    }

    // Parametric
    GeoResult parametricLength(const std::string &xp, const std::string &yp, const std::string &t, double t0, double t1)
    {
        int n = 1000;
        double h = (t1 - t0) / n, L = 0;
        for (int i = 0; i < n; ++i)
        {
            double ti = t0 + (i + 0.5) * h;
            double xd = diff1(xp, t, ti), yd = diff1(yp, t, ti);
            L += std::sqrt(xd * xd + yd * yd) * h;
        }
        return ok(fmt(L));
    }
    GeoResult parametricArea(const std::string &xp, const std::string &yp, const std::string &t, double t0, double t1)
    {
        int n = 1000;
        double h = (t1 - t0) / n, A = 0;
        for (int i = 0; i < n; ++i)
        {
            double ti = t0 + (i + 0.5) * h;
            A += cu_evalAt(yp, t, ti) * diff1(xp, t, ti) * h;
        }
        return ok(fmt(std::abs(A)));
    }
    GeoResult curvature2D(const std::string &xp, const std::string &yp, const std::string &t, double t0)
    {
        double xd = diff1(xp, t, t0), yd = diff1(yp, t, t0), h = 1e-4;
        double xdd = (diff1(xp, t, t0 + h) - diff1(xp, t, t0 - h)) / (2 * h);
        double ydd = (diff1(yp, t, t0 + h) - diff1(yp, t, t0 - h)) / (2 * h);
        double kappa = std::abs(xd * ydd - yd * xdd) / std::pow(xd * xd + yd * yd, 1.5);
        return ok(fmt(kappa), "kappa=" + fmt(kappa) + " R=" + fmt(1 / kappa));
    }
    GeoResult tangentNormal(const std::string &xp, const std::string &yp, const std::string &t, double t0)
    {
        double xv = cu_evalAt(xp, t, t0), yv = cu_evalAt(yp, t, t0), xd = diff1(xp, t, t0), yd = diff1(yp, t, t0);
        double sp = std::sqrt(xd * xd + yd * yd);
        std::ostringstream ss;
        ss << "Point: (" << fmt(xv) << "," << fmt(yv) << ") T=(" << fmt(xd / sp) << "," << fmt(yd / sp) << ") N=(" << fmt(-yd / sp) << "," << fmt(xd / sp) << ")";
        return ok(ss.str());
    }
    GeoResult envelopeCurve(const std::string &F, const std::string &x, const std::string &y, const std::string &param)
    {
        std::ostringstream ss;
        ss << "Envelope: eliminate " << param << " from F=0 and dF/d" << param << "=0\nF=" << F;
        return ok(ss.str());
    }

    // Polar
    GeoResult polarToRect(double r, double theta)
    {
        return ok("(" + fmt(r * std::cos(theta)) + "," + fmt(r * std::sin(theta)) + ")", "x=r cos t, y=r sin t");
    }
    GeoResult rectToPolar(double x, double y)
    {
        return ok("(" + fmt(std::sqrt(x * x + y * y)) + "," + fmt(std::atan2(y, x)) + "rad)");
    }
    GeoResult polarArea(const std::string &r, const std::string &theta, double t0, double t1)
    {
        int n = 1000;
        double h = (t1 - t0) / n, A = 0;
        for (int i = 0; i < n; ++i)
        {
            double ti = t0 + (i + 0.5) * h;
            double rv = cu_evalAt(r, theta, ti);
            A += 0.5 * rv * rv * h;
        }
        return ok(fmt(A));
    }
    GeoResult polarArcLength(const std::string &r, const std::string &theta, double t0, double t1)
    {
        int n = 1000;
        double h = (t1 - t0) / n, L = 0;
        for (int i = 0; i < n; ++i)
        {
            double ti = t0 + (i + 0.5) * h;
            double rv = cu_evalAt(r, theta, ti), drdth = diff1(r, theta, ti);
            L += std::sqrt(rv * rv + drdth * drdth) * h;
        }
        return ok(fmt(L));
    }

    // Transforms
    GeoResult rotate2D(double px, double py, double cx, double cy, double angle)
    {
        double rad = angle * PI / 180, cos_a = std::cos(rad), sin_a = std::sin(rad), tx = px - cx, ty = py - cy;
        return ok("(" + fmt(tx * cos_a - ty * sin_a + cx) + "," + fmt(tx * sin_a + ty * cos_a + cy) + ")");
    }
    GeoResult scale2D(double px, double py, double cx, double cy, double sx, double sy)
    {
        return ok("(" + fmt(cx + sx * (px - cx)) + "," + fmt(cy + sy * (py - cy)) + ")");
    }
    GeoResult reflect2D(double px, double py, double ax, double ay, double bx, double by)
    {
        double dx = bx - ax, dy = by - ay, t = ((px - ax) * dx + (py - ay) * dy) / (dx * dx + dy * dy);
        double fx = ax + t * dx, fy = ay + t * dy;
        return ok("(" + fmt(2 * fx - px) + "," + fmt(2 * fy - py) + ")");
    }
    GeoResult affineTransform(const std::vector<Vec> &M, double px, double py)
    {
        if (M.size() < 2 || M[0].size() < 3)
            return err("Need 2x3 matrix");
        return ok("(" + fmt(M[0][0] * px + M[0][1] * py + M[0][2]) + "," + fmt(M[1][0] * px + M[1][1] * py + M[1][2]) + ")");
    }
    GeoResult compositeTransform(const std::vector<Vec> &M1, const std::vector<Vec> &M2)
    {
        if (M1.size() < 3 || M2.size() < 3)
            return err("Need 3x3 matrices");
        std::vector<Vec> R(3, Vec(3, 0));
        for (int i = 0; i < 3; ++i)
            for (int j = 0; j < 3; ++j)
                for (int k = 0; k < 3; ++k)
                    R[i][j] += M1[i][k] * M2[k][j];
        std::ostringstream ss;
        for (auto &r : R)
        {
            ss << "[";
            for (int i = 0; i < 3; ++i)
            {
                if (i)
                    ss << " ";
                ss << fmt(r[i], 5);
            }
            ss << "]";
        }
        return ok(ss.str());
    }

    // =============================================================================
    // DISPATCH
    // =============================================================================

    static std::vector<P2> parsePoints(const std::string &s)
    {
        std::vector<P2> pts;
        // "[[x1,y1],[x2,y2],…]" — bracketed rows
        if (s.find('[') != std::string::npos)
        {
            auto rows = parseMat(s);
            for (auto &r : rows)
                if (r.size() >= 2)
                    pts.push_back({r[0], r[1]});
            return pts;
        }
        // "x1,y1;x2,y2;…" — semicolon-separated points
        if (s.find(';') != std::string::npos)
        {
            std::stringstream ss(s);
            std::string chunk;
            while (std::getline(ss, chunk, ';'))
            {
                auto v = parseVec("[" + chunk + "]");
                if (v.size() >= 2)
                    pts.push_back({v[0], v[1]});
            }
            return pts;
        }
        // "x1,y1,x2,y2,…" — flat coordinate list
        auto flat = parseVec("[" + s + "]");
        for (size_t i = 0; i + 1 < flat.size(); i += 2)
            pts.push_back({flat[i], flat[i + 1]});
        return pts;
    }

    GeoResult dispatch(const std::string &op, const std::string &json)
    {
        try
        {
            // 2D
            if (op == "distance_2d")
                return distance2D(getN(json, "x1"), getN(json, "y1"), getN(json, "x2"), getN(json, "y2"));
            if (op == "midpoint_2d")
                return midpoint2D(getN(json, "x1"), getN(json, "y1"), getN(json, "x2"), getN(json, "y2"));
            if (op == "slope")
                return slopeLine(getN(json, "x1"), getN(json, "y1"), getN(json, "x2"), getN(json, "y2"));
            if (op == "line_eq")
                return lineEquation(getN(json, "x1"), getN(json, "y1"), getN(json, "x2"), getN(json, "y2"));
            if (op == "line_intersect")
                return lineIntersection(getN(json, "a1"), getN(json, "b1"), getN(json, "c1"), getN(json, "a2"), getN(json, "b2"), getN(json, "c2"));
            if (op == "perpendicular")
                return perpendicular(getN(json, "a"), getN(json, "b"), getN(json, "c"), getN(json, "px"), getN(json, "py"));
            if (op == "pt_line_dist")
                return pointLineDistance(getN(json, "a"), getN(json, "b"), getN(json, "c"), getN(json, "px"), getN(json, "py"));
            if (op == "angle_lines")
                return angleLines(getN(json, "m1"), getN(json, "m2"));
            if (op == "triangle_area")
                return areaTriangle2D(getN(json, "x1"), getN(json, "y1"), getN(json, "x2"), getN(json, "y2"), getN(json, "x3"), getN(json, "y3"));
            if (op == "polygon_area")
                return polygonArea(parsePoints(getP(json, "vertices")));
            if (op == "polygon_perim")
                return polygonPerimeter(parsePoints(getP(json, "vertices")));
            if (op == "is_convex")
                return isConvex(parsePoints(getP(json, "vertices")));
            if (op == "convex_hull")
                return convexHull(parsePoints(getP(json, "points")));
            if (op == "point_in_poly")
                return pointInPolygon(parsePoints(getP(json, "vertices")), getN(json, "px"), getN(json, "py"));
            if (op == "circumcircle")
                return circumcircle(getN(json, "x1"), getN(json, "y1"), getN(json, "x2"), getN(json, "y2"), getN(json, "x3"), getN(json, "y3"));
            if (op == "incircle")
                return incircle(getN(json, "x1"), getN(json, "y1"), getN(json, "x2"), getN(json, "y2"), getN(json, "x3"), getN(json, "y3"));
            // Conics
            if (op == "classify_conic")
                return classifyConic(getN(json, "A"), getN(json, "B"), getN(json, "C"), getN(json, "D"), getN(json, "E"), getN(json, "F"));
            if (op == "circle")
                return circleEquation(getN(json, "cx"), getN(json, "cy"), getN(json, "r"));
            if (op == "circle_gen")
                return circleFromGeneral(getN(json, "A", 1), getN(json, "D"), getN(json, "E"), getN(json, "F"));
            if (op == "ellipse")
                return ellipseEquation(getN(json, "cx"), getN(json, "cy"), getN(json, "a"), getN(json, "b"), getN(json, "theta"));
            if (op == "hyperbola")
                return hyperbolaEquation(getN(json, "cx"), getN(json, "cy"), getN(json, "a"), getN(json, "b"), getP(json, "dir") != "v");
            if (op == "parabola")
                return parabolaEquation(getN(json, "h"), getN(json, "k"), getN(json, "p"), getP(json, "dir") != "h");
            if (op == "eccentricity")
                return eccentricity(getN(json, "A"), getN(json, "B"), getN(json, "C"), getN(json, "D"), getN(json, "E"), getN(json, "F"));
            // 3D
            if (op == "distance_3d")
                return distance3D(getN(json, "x1"), getN(json, "y1"), getN(json, "z1"), getN(json, "x2"), getN(json, "y2"), getN(json, "z2"));
            if (op == "midpoint_3d")
                return midpoint3D(getN(json, "x1"), getN(json, "y1"), getN(json, "z1"), getN(json, "x2"), getN(json, "y2"), getN(json, "z2"));
            if (op == "cross_product")
                return crossProduct(getN(json, "ax"), getN(json, "ay"), getN(json, "az"), getN(json, "bx"), getN(json, "by"), getN(json, "bz"));
            if (op == "dot_3d")
                return dotProduct3D(getN(json, "ax"), getN(json, "ay"), getN(json, "az"), getN(json, "bx"), getN(json, "by"), getN(json, "bz"));
            if (op == "angle_3d")
                return angleBetween3D(getN(json, "ax"), getN(json, "ay"), getN(json, "az"), getN(json, "bx"), getN(json, "by"), getN(json, "bz"));
            if (op == "plane_3pts")
                return planeEquation(getN(json, "x1"), getN(json, "y1"), getN(json, "z1"), getN(json, "x2"), getN(json, "y2"), getN(json, "z2"), getN(json, "x3"), getN(json, "y3"), getN(json, "z3"));
            if (op == "plane_normal")
                return planeFromNormal(getN(json, "nx"), getN(json, "ny"), getN(json, "nz"), getN(json, "px"), getN(json, "py"), getN(json, "pz"));
            if (op == "pt_plane_dist")
                return pointPlaneDistance(getN(json, "a"), getN(json, "b"), getN(json, "c"), getN(json, "d"), getN(json, "px"), getN(json, "py"), getN(json, "pz"));
            if (op == "plane_line")
                return planeLineIntersect(getN(json, "a"), getN(json, "b"), getN(json, "c"), getN(json, "d"), getN(json, "lx"), getN(json, "ly"), getN(json, "lz"), getN(json, "dx"), getN(json, "dy"), getN(json, "dz"));
            if (op == "two_planes")
                return twoPlaneIntersect(getN(json, "a1"), getN(json, "b1"), getN(json, "c1"), getN(json, "d1"), getN(json, "a2"), getN(json, "b2"), getN(json, "c2"), getN(json, "d2"));
            if (op == "skew_lines")
                return skewLines(getN(json, "x1"), getN(json, "y1"), getN(json, "z1"), getN(json, "dx1"), getN(json, "dy1"), getN(json, "dz1"), getN(json, "x2"), getN(json, "y2"), getN(json, "z2"), getN(json, "dx2"), getN(json, "dy2"), getN(json, "dz2"));
            if (op == "sphere")
                return sphereEquation(getN(json, "cx"), getN(json, "cy"), getN(json, "cz"), getN(json, "r"));
            if (op == "sphere_line")
                return sphereLintersect(getN(json, "cx"), getN(json, "cy"), getN(json, "cz"), getN(json, "r"), getN(json, "lx"), getN(json, "ly"), getN(json, "lz"), getN(json, "dx"), getN(json, "dy"), getN(json, "dz"));
            // Parametric
            if (op == "param_length")
                return parametricLength(getP(json, "x"), getP(json, "y"), getP(json, "t", "t"), getN(json, "t0"), getN(json, "t1", 2 * M_PI));
            if (op == "param_area")
                return parametricArea(getP(json, "x"), getP(json, "y"), getP(json, "t", "t"), getN(json, "t0"), getN(json, "t1", 2 * M_PI));
            if (op == "curvature")
                return curvature2D(getP(json, "x"), getP(json, "y"), getP(json, "t", "t"), getN(json, "t0"));
            if (op == "tangent_normal")
                return tangentNormal(getP(json, "x"), getP(json, "y"), getP(json, "t", "t"), getN(json, "t0"));
            if (op == "envelope")
                return envelopeCurve(getP(json, "F"), getP(json, "x", "x"), getP(json, "y", "y"), getP(json, "param", "c"));
            // Polar
            if (op == "polar_to_rect")
                return polarToRect(getN(json, "r"), getN(json, "theta"));
            if (op == "rect_to_polar")
                return rectToPolar(getN(json, "x"), getN(json, "y"));
            if (op == "polar_area")
                return polarArea(getP(json, "r"), getP(json, "theta", "theta"), getN(json, "t0"), getN(json, "t1", 2 * M_PI));
            if (op == "polar_length")
                return polarArcLength(getP(json, "r"), getP(json, "theta", "theta"), getN(json, "t0"), getN(json, "t1", 2 * M_PI));
            // Transforms
            if (op == "rotate_2d")
                return rotate2D(getN(json, "px"), getN(json, "py"), getN(json, "cx"), getN(json, "cy"), getN(json, "angle"));
            if (op == "scale_2d")
                return scale2D(getN(json, "px"), getN(json, "py"), getN(json, "cx"), getN(json, "cy"), getN(json, "sx", 1), getN(json, "sy", 1));
            if (op == "reflect_2d")
                return reflect2D(getN(json, "px"), getN(json, "py"), getN(json, "ax"), getN(json, "ay"), getN(json, "bx"), getN(json, "by"));
            if (op == "affine")
                return affineTransform(parseMat(getP(json, "M")), getN(json, "px"), getN(json, "py"));
            if (op == "composite_tf")
                return compositeTransform(parseMat(getP(json, "M1")), parseMat(getP(json, "M2")));
            return err("Unknown geometry operation: " + op);
        }
        catch (const std::exception &e)
        {
            return err(e.what());
        }
    }

} // namespace Geometry
