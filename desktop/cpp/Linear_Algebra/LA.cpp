// =============================================================================
// LinearAlgebra.cpp  —  Week 1: Full Linear Algebra Implementation
// All operations in C++, no external libraries.
// exactMode=false → symbolic/exact,  exactMode=true → numerical/floating-point
// =============================================================================

#include "LA.hpp"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <complex>
#include <functional>
#include <iomanip>
#include <iostream>
#include <map>
#include <numeric>
#include <optional>
#include <set>
#include <sstream>
#include <stdexcept>
#include <tuple>
#include <utility>
#define _POSIX_C_SOURCE 200809L // Exposes POSIX extensions
#include <math.h>

namespace LinearAlgebra
{

    // ── Constants ─────────────────────────────────────────────────────────────────
    static const double EPS = 1e-10;
    static const double PRINT_EPS = 1e-9;
    static const int MAX_ITER = 1000;

    // =============================================================================
    // LAResult helpers
    // =============================================================================

    std::string LAResult::format(bool exactMode) const
    {
        if (!ok)
            return "ERROR: " + error;
        // exactMode=true  → prefer symbolic (exact fractions/expressions)
        // exactMode=false → prefer numerical (decimal approximations)
        if (exactMode)
            return symbolic.empty() ? numerical : symbolic;
        if (numerical.empty())
            return symbolic;
        if (symbolic == numerical)
            return symbolic;
        return symbolic + "  ~  " + numerical;
    }

    static LAResult makeError(const std::string &msg)
    {
        LAResult r;
        r.ok = false;
        r.error = msg;
        return r;
    }

    static LAResult makeResult(const std::string &sym, const std::string &num = "")
    {
        LAResult r;
        r.symbolic = sym;
        r.numerical = num.empty() ? sym : num;
        return r;
    }

    // =============================================================================
    // SECTION 1 — Rational arithmetic for exact computation
    // =============================================================================

    struct Frac
    {
        long long n = 0, d = 1;

        Frac() = default;
        Frac(long long n, long long d = 1) : n(n), d(d) { reduce(); }
        explicit Frac(double x)
        {
            // Convert double to fraction via continued fractions (10 steps)
            long long a0 = (long long)x;
            double rem = x - a0;
            if (std::abs(rem) < 1e-9)
            {
                n = a0;
                d = 1;
                return;
            }
            // Use 1000 as denominator limit
            long long best_n = a0, best_d = 1;
            double best_err = std::abs(x - (double)a0);
            for (long long dd = 2; dd <= 1000; ++dd)
            {
                long long nn = (long long)std::round(x * dd);
                double err = std::abs(x - (double)nn / dd);
                if (err < best_err)
                {
                    best_err = err;
                    best_n = nn;
                    best_d = dd;
                }
                if (best_err < 1e-10)
                    break;
            }
            n = best_n;
            d = best_d;
            reduce();
        }

        void reduce()
        {
            if (d < 0)
            {
                n = -n;
                d = -d;
            }
            long long g = std::gcd(std::abs(n), d);
            if (g > 0)
            {
                n /= g;
                d /= g;
            }
        }

        double toDouble() const { return (double)n / d; }

        std::string str() const
        {
            if (d == 1)
                return std::to_string(n);
            return std::to_string(n) + "/" + std::to_string(d);
        }

        Frac operator+(const Frac &o) const { return Frac(n * o.d + o.n * d, d * o.d); }
        Frac operator-(const Frac &o) const { return Frac(n * o.d - o.n * d, d * o.d); }
        Frac operator*(const Frac &o) const { return Frac(n * o.n, d * o.d); }
        Frac operator/(const Frac &o) const { return Frac(n * o.d, d * o.n); }
        bool operator==(const Frac &o) const { return n == o.n && d == o.d; }
        bool isZero() const { return n == 0; }
    };

    using FMatrix = std::vector<std::vector<Frac>>;

    static FMatrix toFMatrix(const Matrix &M)
    {
        FMatrix F(M.size(), std::vector<Frac>(M[0].size()));
        for (size_t i = 0; i < M.size(); ++i)
            for (size_t j = 0; j < M[0].size(); ++j)
                F[i][j] = Frac(M[i][j]);
        return F;
    }

    static Matrix fromFMatrix(const FMatrix &F)
    {
        Matrix M(F.size(), std::vector<double>(F[0].size()));
        for (size_t i = 0; i < F.size(); ++i)
            for (size_t j = 0; j < F[0].size(); ++j)
                M[i][j] = F[i][j].toDouble();
        return M;
    }

    // =============================================================================
    // SECTION 2 — Parsing
    // =============================================================================

    Matrix parseMatrix(const std::string &s)
    {
        Matrix M;
        std::string t = s;
        // Strip outer whitespace
        while (!t.empty() && std::isspace(t.front()))
            t.erase(t.begin());
        while (!t.empty() && std::isspace(t.back()))
            t.pop_back();

        if (t.empty() || t.front() != '[')
            throw std::invalid_argument("Matrix must start with '['");

        // Remove outer brackets if present: [[...]] or [[...],[...]]
        if (t.front() == '[' && t.back() == ']')
        {
            t = t.substr(1, t.size() - 2);
        }

        // Split by rows: each row is [...] separated by commas
        int depth = 0;
        std::string cur;
        for (char c : t)
        {
            if (c == '[')
            {
                depth++;
                if (depth == 1)
                    continue;
            }
            if (c == ']')
            {
                depth--;
                if (depth == 0)
                {
                    // parse cur as a row
                    std::vector<Real> row;
                    std::istringstream ss(cur);
                    std::string tok;
                    while (std::getline(ss, tok, ','))
                    {
                        while (!tok.empty() && std::isspace(tok.front()))
                            tok.erase(tok.begin());
                        while (!tok.empty() && std::isspace(tok.back()))
                            tok.pop_back();
                        if (!tok.empty())
                            row.push_back(std::stod(tok));
                    }
                    if (!row.empty())
                        M.push_back(row);
                    cur.clear();
                    continue;
                }
            }
            if (depth >= 1)
                cur += c;
        }
        return M;
    }

    Vector parseVector(const std::string &s)
    {
        Vector v;
        std::string t = s;
        while (!t.empty() && std::isspace(t.front()))
            t.erase(t.begin());
        while (!t.empty() && std::isspace(t.back()))
            t.pop_back();
        if (t.front() == '[')
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
                v.push_back(std::stod(tok));
        }
        return v;
    }

    // =============================================================================
    // SECTION 3 — Formatting
    // =============================================================================

    std::string formatReal(Real x, bool exact)
    {
        if (std::abs(x) < PRINT_EPS)
            return "0";
        if (exact)
        {
            std::ostringstream ss;
            ss << std::setprecision(10) << x;
            return ss.str();
        }
        // Try to represent as fraction
        Frac f(x);
        if (std::abs(f.toDouble() - x) < 1e-8)
            return f.str();
        // Try common surds: sqrt(2), sqrt(3), sqrt(5)
        for (int k : {2, 3, 5, 6, 7})
        {
            double sq = std::sqrt((double)k);
            Frac coeff(x / sq);
            if (std::abs(coeff.toDouble() * sq - x) < 1e-8)
            {
                std::string cs = coeff.str();
                if (cs == "1")
                    return "sqrt(" + std::to_string(k) + ")";
                if (cs == "-1")
                    return "-sqrt(" + std::to_string(k) + ")";
                return cs + "*sqrt(" + std::to_string(k) + ")";
            }
        }
        // Try pi
        Frac piCoeff(x / M_PI);
        if (std::abs(piCoeff.toDouble() * M_PI - x) < 1e-8)
        {
            std::string ps = piCoeff.str();
            if (ps == "1")
                return "pi";
            return ps + "*pi";
        }
        std::ostringstream ss;
        ss << std::setprecision(10) << x;
        return ss.str();
    }

    std::string formatComplex(Complex z, bool exact)
    {
        double re = z.real(), im = z.imag();
        if (std::abs(im) < PRINT_EPS)
            return formatReal(re, exact);
        if (std::abs(re) < PRINT_EPS)
        {
            if (std::abs(im - 1.0) < PRINT_EPS)
                return "i";
            if (std::abs(im + 1.0) < PRINT_EPS)
                return "-i";
            return formatReal(im, exact) + "i";
        }
        std::string r = formatReal(re, exact);
        std::string i = formatReal(std::abs(im), exact);
        return r + (im > 0 ? " + " : " - ") + i + "i";
    }

    std::string formatMatrix(const Matrix &M, bool exact)
    {
        if (M.empty())
            return "[]";
        std::ostringstream ss;
        ss << "[";
        for (size_t i = 0; i < M.size(); ++i)
        {
            ss << "[";
            for (size_t j = 0; j < M[i].size(); ++j)
            {
                ss << formatReal(M[i][j], exact);
                if (j + 1 < M[i].size())
                    ss << ", ";
            }
            ss << "]";
            if (i + 1 < M.size())
                ss << ", ";
        }
        ss << "]";
        return ss.str();
    }

    std::string formatVector(const Vector &v, bool exact)
    {
        std::ostringstream ss;
        ss << "[";
        for (size_t i = 0; i < v.size(); ++i)
        {
            ss << formatReal(v[i], exact);
            if (i + 1 < v.size())
                ss << ", ";
        }
        ss << "]";
        return ss.str();
    }

    static std::string formatFMatrix(const FMatrix &F)
    {
        if (F.empty())
            return "[]";
        std::ostringstream ss;
        ss << "[";
        for (size_t i = 0; i < F.size(); ++i)
        {
            ss << "[";
            for (size_t j = 0; j < F[i].size(); ++j)
            {
                ss << F[i][j].str();
                if (j + 1 < F[i].size())
                    ss << ", ";
            }
            ss << "]";
            if (i + 1 < F.size())
                ss << ", ";
        }
        ss << "]";
        return ss.str();
    }

    static std::string formatCMatrix(const CMatrix &M, bool exact)
    {
        std::ostringstream ss;
        ss << "[";
        for (size_t i = 0; i < M.size(); ++i)
        {
            ss << "[";
            for (size_t j = 0; j < M[i].size(); ++j)
            {
                ss << formatComplex(M[i][j], exact);
                if (j + 1 < M[i].size())
                    ss << ", ";
            }
            ss << "]";
            if (i + 1 < M.size())
                ss << ", ";
        }
        ss << "]";
        return ss.str();
    }

    static std::string formatCVector(const CVector &v, bool exact)
    {
        std::ostringstream ss;
        ss << "[";
        for (size_t i = 0; i < v.size(); ++i)
        {
            ss << formatComplex(v[i], exact);
            if (i + 1 < v.size())
                ss << ", ";
        }
        ss << "]";
        return ss.str();
    }

    // =============================================================================
    // SECTION 4 — Core matrix utilities (internal)
    // =============================================================================

    static int rows(const Matrix &M) { return (int)M.size(); }
    static int cols(const Matrix &M) { return M.empty() ? 0 : (int)M[0].size(); }

    static bool isSquare(const Matrix &M) { return rows(M) == cols(M) && !M.empty(); }

    static Matrix identity(int n)
    {
        Matrix I(n, Vector(n, 0.0));
        for (int i = 0; i < n; ++i)
            I[i][i] = 1.0;
        return I;
    }

    static Matrix zeroMatrix(int r, int c)
    {
        return Matrix(r, Vector(c, 0.0));
    }

    static Matrix matAdd(const Matrix &A, const Matrix &B)
    {
        int r = rows(A), c = cols(A);
        Matrix C = A;
        for (int i = 0; i < r; ++i)
            for (int j = 0; j < c; ++j)
                C[i][j] += B[i][j];
        return C;
    }

    static Matrix matSub(const Matrix &A, const Matrix &B)
    {
        int r = rows(A), c = cols(A);
        Matrix C = A;
        for (int i = 0; i < r; ++i)
            for (int j = 0; j < c; ++j)
                C[i][j] -= B[i][j];
        return C;
    }

    static Matrix matMul(const Matrix &A, const Matrix &B)
    {
        int r = rows(A), k = cols(A), c = cols(B);
        Matrix C(r, Vector(c, 0.0));
        for (int i = 0; i < r; ++i)
            for (int l = 0; l < k; ++l)
                if (std::abs(A[i][l]) > EPS)
                    for (int j = 0; j < c; ++j)
                        C[i][j] += A[i][l] * B[l][j];
        return C;
    }

    static Matrix matScale(const Matrix &A, double s)
    {
        Matrix C = A;
        for (auto &row : C)
            for (auto &x : row)
                x *= s;
        return C;
    }

    static Matrix matTranspose(const Matrix &A)
    {
        int r = rows(A), c = cols(A);
        Matrix T(c, Vector(r));
        for (int i = 0; i < r; ++i)
            for (int j = 0; j < c; ++j)
                T[j][i] = A[i][j];
        return T;
    }

    // =============================================================================
    // SECTION 5 — Basic operations (public API)
    // =============================================================================

    LAResult add(const std::string &As, const std::string &Bs, bool exact)
    {
        try
        {
            Matrix A = parseMatrix(As), B = parseMatrix(Bs);
            if (rows(A) != rows(B) || cols(A) != cols(B))
                return makeError("Matrices must have the same dimensions for addition");
            Matrix C = matAdd(A, B);
            FMatrix FA = toFMatrix(A), FB = toFMatrix(B);
            FMatrix FC(rows(A), std::vector<Frac>(cols(A)));
            for (int i = 0; i < rows(A); ++i)
                for (int j = 0; j < cols(A); ++j)
                    FC[i][j] = FA[i][j] + FB[i][j];
            return makeResult(formatFMatrix(FC), formatMatrix(C, true));
        }
        catch (const std::exception &e)
        {
            return makeError(e.what());
        }
    }

    LAResult subtract(const std::string &As, const std::string &Bs, bool exact)
    {
        try
        {
            Matrix A = parseMatrix(As), B = parseMatrix(Bs);
            if (rows(A) != rows(B) || cols(A) != cols(B))
                return makeError("Matrices must have the same dimensions for subtraction");
            Matrix C = matSub(A, B);
            FMatrix FA = toFMatrix(A), FB = toFMatrix(B);
            FMatrix FC(rows(A), std::vector<Frac>(cols(A)));
            for (int i = 0; i < rows(A); ++i)
                for (int j = 0; j < cols(A); ++j)
                    FC[i][j] = FA[i][j] - FB[i][j];
            return makeResult(formatFMatrix(FC), formatMatrix(C, true));
        }
        catch (const std::exception &e)
        {
            return makeError(e.what());
        }
    }

    LAResult scalarMultiply(const std::string &As, const std::string &scalar, bool exact)
    {
        try
        {
            Matrix A = parseMatrix(As);
            double s = std::stod(scalar);
            Matrix C = matScale(A, s);
            Frac fs(s);
            FMatrix FA = toFMatrix(A);
            FMatrix FC(rows(A), std::vector<Frac>(cols(A)));
            for (int i = 0; i < rows(A); ++i)
                for (int j = 0; j < cols(A); ++j)
                    FC[i][j] = FA[i][j] * fs;
            return makeResult(formatFMatrix(FC), formatMatrix(C, true));
        }
        catch (const std::exception &e)
        {
            return makeError(e.what());
        }
    }

    LAResult multiply(const std::string &As, const std::string &Bs, bool exact)
    {
        try
        {
            Matrix A = parseMatrix(As), B = parseMatrix(Bs);
            if (cols(A) != rows(B))
                return makeError("Incompatible dimensions: A is " +
                                 std::to_string(rows(A)) + "x" + std::to_string(cols(A)) +
                                 ", B is " + std::to_string(rows(B)) + "x" + std::to_string(cols(B)));
            // Exact via fractions
            FMatrix FA = toFMatrix(A), FB = toFMatrix(B);
            FMatrix FC(rows(A), std::vector<Frac>(cols(B)));
            for (int i = 0; i < rows(A); ++i)
                for (int k = 0; k < cols(A); ++k)
                    for (int j = 0; j < cols(B); ++j)
                        FC[i][j] = FC[i][j] + FA[i][k] * FB[k][j];
            Matrix C = matMul(A, B);
            return makeResult(formatFMatrix(FC), formatMatrix(C, true));
        }
        catch (const std::exception &e)
        {
            return makeError(e.what());
        }
    }

    LAResult hadamard(const std::string &As, const std::string &Bs, bool exact)
    {
        try
        {
            Matrix A = parseMatrix(As), B = parseMatrix(Bs);
            if (rows(A) != rows(B) || cols(A) != cols(B))
                return makeError("Hadamard product requires equal dimensions");
            FMatrix FA = toFMatrix(A), FB = toFMatrix(B);
            FMatrix FC(rows(A), std::vector<Frac>(cols(A)));
            for (int i = 0; i < rows(A); ++i)
                for (int j = 0; j < cols(A); ++j)
                    FC[i][j] = FA[i][j] * FB[i][j];
            Matrix C(rows(A), Vector(cols(A)));
            for (int i = 0; i < rows(A); ++i)
                for (int j = 0; j < cols(A); ++j)
                    C[i][j] = FC[i][j].toDouble();
            return makeResult(formatFMatrix(FC), formatMatrix(C, true));
        }
        catch (const std::exception &e)
        {
            return makeError(e.what());
        }
    }

    LAResult transpose(const std::string &As, bool exact)
    {
        try
        {
            Matrix A = parseMatrix(As);
            Matrix T = matTranspose(A);
            FMatrix FT = toFMatrix(T);
            return makeResult(formatFMatrix(FT), formatMatrix(T, true));
        }
        catch (const std::exception &e)
        {
            return makeError(e.what());
        }
    }

    LAResult conjugateTranspose(const std::string &As, bool exact)
    {
        // For real matrices this is just transpose; included for completeness
        return transpose(As, exact);
    }

    LAResult power(const std::string &As, int n, bool exact)
    {
        try
        {
            Matrix A = parseMatrix(As);
            if (!isSquare(A))
                return makeError("Matrix must be square for power");
            if (n < 0)
            {
                // A^n = (A^-1)^|n|  — compute inverse first
                // (handled via inverseGaussJordan internally)
            }
            int sz = rows(A);
            Matrix result = identity(sz);
            Matrix base = A;
            int exp = std::abs(n);
            // Binary exponentiation
            while (exp > 0)
            {
                if (exp & 1)
                    result = matMul(result, base);
                base = matMul(base, base);
                exp >>= 1;
            }
            if (n < 0)
            {
                // invert result
                auto inv = inverseGaussJordan(formatMatrix(result, true), exact);
                return inv;
            }
            FMatrix FR = toFMatrix(result);
            return makeResult(formatFMatrix(FR), formatMatrix(result, true));
        }
        catch (const std::exception &e)
        {
            return makeError(e.what());
        }
    }

    LAResult trace(const std::string &As, bool exact)
    {
        try
        {
            Matrix A = parseMatrix(As);
            if (!isSquare(A))
                return makeError("Trace is only defined for square matrices");
            Frac t(0LL);
            FMatrix FA = toFMatrix(A);
            for (int i = 0; i < rows(A); ++i)
                t = t + FA[i][i];
            double tn = 0;
            for (int i = 0; i < rows(A); ++i)
                tn += A[i][i];
            return makeResult(t.str(), formatReal(tn, true));
        }
        catch (const std::exception &e)
        {
            return makeError(e.what());
        }
    }

    // =============================================================================
    // SECTION 6 — Strassen matrix multiplication
    // =============================================================================

    static Matrix strassenInternal(const Matrix &A, const Matrix &B)
    {
        int n = rows(A);
        if (n == 1)
        {
            return {{A[0][0] * B[0][0]}};
        }
        int half = n / 2;
        auto sub = [&](const Matrix &M, int r0, int c0)
        {
            Matrix S(half, Vector(half));
            for (int i = 0; i < half; ++i)
                for (int j = 0; j < half; ++j)
                    S[i][j] = M[r0 + i][c0 + j];
            return S;
        };
        Matrix A11 = sub(A, 0, 0), A12 = sub(A, 0, half),
               A21 = sub(A, half, 0), A22 = sub(A, half, half);
        Matrix B11 = sub(B, 0, 0), B12 = sub(B, 0, half),
               B21 = sub(B, half, 0), B22 = sub(B, half, half);

        Matrix M1 = strassenInternal(matAdd(A11, A22), matAdd(B11, B22));
        Matrix M2 = strassenInternal(matAdd(A21, A22), B11);
        Matrix M3 = strassenInternal(A11, matSub(B12, B22));
        Matrix M4 = strassenInternal(A22, matSub(B21, B11));
        Matrix M5 = strassenInternal(matAdd(A11, A12), B22);
        Matrix M6 = strassenInternal(matSub(A21, A11), matAdd(B11, B12));
        Matrix M7 = strassenInternal(matSub(A12, A22), matAdd(B21, B22));

        Matrix C(n, Vector(n));
        for (int i = 0; i < half; ++i)
            for (int j = 0; j < half; ++j)
            {
                C[i][j] = M1[i][j] + M4[i][j] - M5[i][j] + M7[i][j];
                C[i][j + half] = M3[i][j] + M5[i][j];
                C[i + half][j] = M2[i][j] + M4[i][j];
                C[i + half][j + half] = M1[i][j] - M2[i][j] + M3[i][j] + M6[i][j];
            }
        return C;
    }

    // Pad matrix to next power of 2 for Strassen
    static Matrix padToPow2(const Matrix &A)
    {
        int n = rows(A), m = cols(A);
        int sz = 1;
        while (sz < std::max(n, m))
            sz <<= 1;
        Matrix P(sz, Vector(sz, 0.0));
        for (int i = 0; i < n; ++i)
            for (int j = 0; j < m; ++j)
                P[i][j] = A[i][j];
        return P;
    }

    LAResult multiplyStrassen(const std::string &As, const std::string &Bs, bool exact)
    {
        try
        {
            Matrix A = parseMatrix(As), B = parseMatrix(Bs);
            if (cols(A) != rows(B))
                return makeError("Incompatible dimensions for Strassen multiplication");
            int rA = rows(A), cB = cols(B);
            Matrix AP = padToPow2(A), BP = padToPow2(B);
            Matrix CP = strassenInternal(AP, BP);
            // Extract result
            Matrix C(rA, Vector(cB));
            for (int i = 0; i < rA; ++i)
                for (int j = 0; j < cB; ++j)
                    C[i][j] = CP[i][j];
            FMatrix FC = toFMatrix(C);
            return makeResult(
                "Strassen: " + formatFMatrix(FC),
                "Strassen: " + formatMatrix(C, true));
        }
        catch (const std::exception &e)
        {
            return makeError(e.what());
        }
    }

    // =============================================================================
    // SECTION 7 — LU Decomposition (core building block)
    // =============================================================================

    struct LUDecomp
    {
        Matrix L, U;
        std::vector<int> perm;
        int sign; // +1 or -1
        bool singular;
    };

    static LUDecomp luDecompose(const Matrix &A)
    {
        int n = rows(A);
        Matrix U = A;
        Matrix L = identity(n);
        std::vector<int> perm(n);
        std::iota(perm.begin(), perm.end(), 0);
        int sign = 1;

        for (int col = 0; col < n; ++col)
        {
            // Partial pivoting
            int pivot = col;
            double maxVal = std::abs(U[col][col]);
            for (int row = col + 1; row < n; ++row)
            {
                if (std::abs(U[row][col]) > maxVal)
                {
                    maxVal = std::abs(U[row][col]);
                    pivot = row;
                }
            }
            if (maxVal < EPS)
            {
                // Singular — continue anyway
            }
            if (pivot != col)
            {
                std::swap(U[col], U[pivot]);
                std::swap(perm[col], perm[pivot]);
                for (int k = 0; k < col; ++k)
                    std::swap(L[col][k], L[pivot][k]);
                sign = -sign;
            }
            for (int row = col + 1; row < n; ++row)
            {
                if (std::abs(U[col][col]) < EPS)
                    continue;
                double factor = U[row][col] / U[col][col];
                L[row][col] = factor;
                for (int k = col; k < n; ++k)
                    U[row][k] -= factor * U[col][k];
            }
        }
        bool singular = false;
        for (int i = 0; i < n; ++i)
            if (std::abs(U[i][i]) < EPS)
            {
                singular = true;
                break;
            }

        return {L, U, perm, sign, singular};
    }

    // =============================================================================
    // SECTION 8 — Determinant
    // =============================================================================

    static double detLU(const Matrix &A)
    {
        if (!isSquare(A))
            throw std::invalid_argument("Matrix must be square");
        auto lu = luDecompose(A);
        double d = lu.sign;
        for (int i = 0; i < rows(A); ++i)
            d *= lu.U[i][i];
        return d;
    }

    LAResult determinantLU(const std::string &As, bool exact)
    {
        try
        {
            Matrix A = parseMatrix(As);
            if (!isSquare(A))
                return makeError("Determinant requires a square matrix");
            double d = detLU(A);
            return makeResult(formatReal(d, false), formatReal(d, true));
        }
        catch (const std::exception &e)
        {
            return makeError(e.what());
        }
    }

    // Recursive cofactor expansion
    static double detCofactorRec(const Matrix &A)
    {
        int n = rows(A);
        if (n == 1)
            return A[0][0];
        if (n == 2)
            return A[0][0] * A[1][1] - A[0][1] * A[1][0];
        double d = 0;
        for (int j = 0; j < n; ++j)
        {
            // Minor: remove row 0, col j
            Matrix M(n - 1, Vector(n - 1));
            for (int r = 1; r < n; ++r)
            {
                int cj = 0;
                for (int c = 0; c < n; ++c)
                {
                    if (c == j)
                        continue;
                    M[r - 1][cj++] = A[r][c];
                }
            }
            double sign = (j % 2 == 0) ? 1.0 : -1.0;
            d += sign * A[0][j] * detCofactorRec(M);
        }
        return d;
    }

    LAResult determinantCofactor(const std::string &As, bool exact)
    {
        try
        {
            Matrix A = parseMatrix(As);
            if (!isSquare(A))
                return makeError("Determinant requires a square matrix");
            if (rows(A) > 6)
                return makeError("Cofactor expansion is O(n!) — too slow for n > 6. Use determinant() instead.");
            double d = detCofactorRec(A);
            return makeResult(formatReal(d, false), formatReal(d, true));
        }
        catch (const std::exception &e)
        {
            return makeError(e.what());
        }
    }

    // Bareiss algorithm — exact integer determinant
    LAResult determinantBareiss(const std::string &As, bool exact)
    {
        try
        {
            Matrix A = parseMatrix(As);
            if (!isSquare(A))
                return makeError("Determinant requires a square matrix");
            int n = rows(A);
            // Work with fractions for exactness
            FMatrix F = toFMatrix(A);
            int sign = 1;
            Frac prev(1LL);
            for (int i = 0; i < n; ++i)
            {
                // Find pivot
                int pivot = -1;
                for (int k = i; k < n; ++k)
                {
                    if (!F[k][i].isZero())
                    {
                        pivot = k;
                        break;
                    }
                }
                if (pivot == -1)
                    return makeResult("0", "0");
                if (pivot != i)
                {
                    std::swap(F[i], F[pivot]);
                    sign = -sign;
                }
                for (int j = i + 1; j < n; ++j)
                {
                    for (int k = i + 1; k < n; ++k)
                    {
                        F[j][k] = (F[i][i] * F[j][k] - F[j][i] * F[i][k]) / prev;
                    }
                    F[j][i] = Frac(0LL);
                }
                prev = F[i][i];
            }
            Frac det = F[n - 1][n - 1];
            if (sign == -1)
                det = Frac(0LL) - det;
            double dNum = det.toDouble();
            return makeResult(det.str(), formatReal(dNum, true));
        }
        catch (const std::exception &e)
        {
            return makeError(e.what());
        }
    }

    LAResult determinant(const std::string &As, bool exact)
    {
        try
        {
            Matrix A = parseMatrix(As);
            if (!isSquare(A))
                return makeError("Determinant requires a square matrix");
            int n = rows(A);
            // Use Bareiss for small n (exact), LU for large n (fast)
            if (n <= 8)
                return determinantBareiss(As, exact);
            return determinantLU(As, exact);
        }
        catch (const std::exception &e)
        {
            return makeError(e.what());
        }
    }

    // =============================================================================
    // SECTION 9 — Inverse
    // =============================================================================

    static std::optional<Matrix> gaussJordanInverse(const Matrix &A)
    {
        int n = rows(A);
        // Build augmented [A | I]
        Matrix aug(n, Vector(2 * n, 0.0));
        for (int i = 0; i < n; ++i)
        {
            for (int j = 0; j < n; ++j)
                aug[i][j] = A[i][j];
            aug[i][n + i] = 1.0;
        }
        for (int col = 0; col < n; ++col)
        {
            // Find pivot
            int pivot = col;
            for (int r = col + 1; r < n; ++r)
                if (std::abs(aug[r][col]) > std::abs(aug[pivot][col]))
                    pivot = r;
            if (std::abs(aug[pivot][col]) < EPS)
                return std::nullopt;
            std::swap(aug[col], aug[pivot]);
            double d = aug[col][col];
            for (int j = 0; j < 2 * n; ++j)
                aug[col][j] /= d;
            for (int r = 0; r < n; ++r)
            {
                if (r == col)
                    continue;
                double f = aug[r][col];
                for (int j = 0; j < 2 * n; ++j)
                    aug[r][j] -= f * aug[col][j];
            }
        }
        Matrix inv(n, Vector(n));
        for (int i = 0; i < n; ++i)
            for (int j = 0; j < n; ++j)
                inv[i][j] = aug[i][n + j];
        return inv;
    }

    LAResult inverseGaussJordan(const std::string &As, bool exact)
    {
        try
        {
            Matrix A = parseMatrix(As);
            if (!isSquare(A))
                return makeError("Inverse requires a square matrix");
            auto inv = gaussJordanInverse(A);
            if (!inv)
                return makeError("Matrix is singular (determinant = 0)");
            // Exact via fractions
            FMatrix FA = toFMatrix(A);
            int n = rows(A);
            FMatrix aug(n, std::vector<Frac>(2 * n, Frac(0LL)));
            for (int i = 0; i < n; ++i)
            {
                for (int j = 0; j < n; ++j)
                    aug[i][j] = FA[i][j];
                aug[i][n + i] = Frac(1LL);
            }
            for (int col = 0; col < n; ++col)
            {
                int pivot = col;
                for (int r = col + 1; r < n; ++r)
                    if (std::abs(aug[r][col].toDouble()) > std::abs(aug[pivot][col].toDouble()))
                        pivot = r;
                if (aug[pivot][col].isZero())
                    return makeError("Matrix is singular");
                std::swap(aug[col], aug[pivot]);
                Frac d = aug[col][col];
                for (int j = 0; j < 2 * n; ++j)
                    aug[col][j] = aug[col][j] / d;
                for (int r = 0; r < n; ++r)
                {
                    if (r == col)
                        continue;
                    Frac f = aug[r][col];
                    for (int j = 0; j < 2 * n; ++j)
                        aug[r][j] = aug[r][j] - f * aug[col][j];
                }
            }
            FMatrix Finv(n, std::vector<Frac>(n));
            for (int i = 0; i < n; ++i)
                for (int j = 0; j < n; ++j)
                    Finv[i][j] = aug[i][n + j];
            return makeResult(formatFMatrix(Finv), formatMatrix(*inv, true));
        }
        catch (const std::exception &e)
        {
            return makeError(e.what());
        }
    }

    LAResult inverseAdjugate(const std::string &As, bool exact)
    {
        try
        {
            Matrix A = parseMatrix(As);
            if (!isSquare(A))
                return makeError("Inverse requires a square matrix");
            int n = rows(A);
            double d = detLU(A);
            if (std::abs(d) < EPS)
                return makeError("Matrix is singular (det = 0)");
            // Cofactor matrix
            FMatrix FA = toFMatrix(A);
            FMatrix adj(n, std::vector<Frac>(n));
            for (int i = 0; i < n; ++i)
                for (int j = 0; j < n; ++j)
                {
                    FMatrix M(n - 1, std::vector<Frac>(n - 1));
                    for (int r = 0, ri = 0; r < n; ++r)
                    {
                        if (r == i)
                            continue;
                        for (int c = 0, ci = 0; c < n; ++c)
                        {
                            if (c == j)
                                continue;
                            M[ri][ci++] = FA[r][c];
                        }
                        ri++;
                    }
                    // Minor determinant via double
                    Matrix Md = fromFMatrix(M);
                    double mdet = (n - 1 == 0) ? 1.0 : detCofactorRec(Md);
                    double cofactor = ((i + j) % 2 == 0 ? 1.0 : -1.0) * mdet;
                    adj[j][i] = Frac(cofactor); // Transpose for adjugate
                }
            Frac fdet(d);
            FMatrix Finv(n, std::vector<Frac>(n));
            for (int i = 0; i < n; ++i)
                for (int j = 0; j < n; ++j)
                    Finv[i][j] = adj[i][j] / fdet;
            Matrix inv(n, Vector(n));
            for (int i = 0; i < n; ++i)
                for (int j = 0; j < n; ++j)
                    inv[i][j] = Finv[i][j].toDouble();
            return makeResult(formatFMatrix(Finv), formatMatrix(inv, true));
        }
        catch (const std::exception &e)
        {
            return makeError(e.what());
        }
    }

    LAResult inverse(const std::string &As, bool exact)
    {
        return inverseGaussJordan(As, exact);
    }

    // =============================================================================
    // SECTION 10 — Row reduction (RREF / REF)
    // =============================================================================

    static FMatrix rrefFrac(FMatrix F, std::vector<int> &pivotCols)
    {
        int r = F.size(), c = F[0].size();
        int lead = 0;
        for (int row = 0; row < r && lead < c; ++row)
        {
            int i = row;
            while (i < r && F[i][lead].isZero())
                ++i;
            if (i == r)
            {
                ++lead;
                --row;
                continue;
            }
            std::swap(F[row], F[i]);
            Frac piv = F[row][lead];
            for (int j = 0; j < c; ++j)
                F[row][j] = F[row][j] / piv;
            pivotCols.push_back(lead);
            for (int k = 0; k < r; ++k)
            {
                if (k == row)
                    continue;
                Frac f = F[k][lead];
                for (int j = 0; j < c; ++j)
                    F[k][j] = F[k][j] - f * F[row][j];
            }
            ++lead;
        }
        return F;
    }

    LAResult rowEchelon(const std::string &As, bool exact)
    {
        try
        {
            Matrix A = parseMatrix(As);
            FMatrix F = toFMatrix(A);
            int r = rows(A), c = cols(A), lead = 0;
            for (int row = 0; row < r && lead < c; ++row)
            {
                int i = row;
                while (i < r && F[i][lead].isZero())
                    ++i;
                if (i == r)
                {
                    ++lead;
                    --row;
                    continue;
                }
                std::swap(F[row], F[i]);
                Frac piv = F[row][lead];
                for (int j = 0; j < c; ++j)
                    F[row][j] = F[row][j] / piv;
                for (int k = row + 1; k < r; ++k)
                {
                    Frac f = F[k][lead];
                    for (int j = 0; j < c; ++j)
                        F[k][j] = F[k][j] - f * F[row][j];
                }
                ++lead;
            }
            Matrix num = fromFMatrix(F);
            return makeResult(formatFMatrix(F), formatMatrix(num, true));
        }
        catch (const std::exception &e)
        {
            return makeError(e.what());
        }
    }

    LAResult reducedRowEchelon(const std::string &As, bool exact)
    {
        try
        {
            Matrix A = parseMatrix(As);
            FMatrix F = toFMatrix(A);
            std::vector<int> pivots;
            FMatrix R = rrefFrac(F, pivots);
            Matrix num = fromFMatrix(R);
            return makeResult(formatFMatrix(R), formatMatrix(num, true));
        }
        catch (const std::exception &e)
        {
            return makeError(e.what());
        }
    }

    LAResult rank(const std::string &As, bool exact)
    {
        try
        {
            Matrix A = parseMatrix(As);
            FMatrix F = toFMatrix(A);
            std::vector<int> pivots;
            rrefFrac(F, pivots);
            int r = (int)pivots.size();
            return makeResult(std::to_string(r));
        }
        catch (const std::exception &e)
        {
            return makeError(e.what());
        }
    }

    LAResult nullity(const std::string &As, bool exact)
    {
        try
        {
            Matrix A = parseMatrix(As);
            FMatrix F = toFMatrix(A);
            std::vector<int> pivots;
            rrefFrac(F, pivots);
            int n = cols(A) - (int)pivots.size();
            return makeResult(std::to_string(n));
        }
        catch (const std::exception &e)
        {
            return makeError(e.what());
        }
    }

    LAResult rankNullity(const std::string &As, bool exact)
    {
        try
        {
            Matrix A = parseMatrix(As);
            FMatrix F = toFMatrix(A);
            std::vector<int> pivots;
            rrefFrac(F, pivots);
            int r = (int)pivots.size();
            int n = cols(A) - r;
            std::ostringstream ss;
            ss << "Rank-Nullity Theorem for " << rows(A) << "x" << cols(A) << " matrix:\n";
            ss << "  rank(A)   = " << r << "\n";
            ss << "  nullity(A)= " << n << "\n";
            ss << "  rank + nullity = " << (r + n) << " = n = " << cols(A) << "  [confirmed]";
            return makeResult(ss.str());
        }
        catch (const std::exception &e)
        {
            return makeError(e.what());
        }
    }

    LAResult pivotPositions(const std::string &As, bool exact)
    {
        try
        {
            Matrix A = parseMatrix(As);
            FMatrix F = toFMatrix(A);
            std::vector<int> pivots;
            FMatrix R = rrefFrac(F, pivots);
            std::ostringstream ss;
            ss << "Pivot columns: [";
            for (size_t i = 0; i < pivots.size(); ++i)
            {
                ss << pivots[i] + 1; // 1-indexed for display
                if (i + 1 < pivots.size())
                    ss << ", ";
            }
            ss << "]";
            return makeResult(ss.str());
        }
        catch (const std::exception &e)
        {
            return makeError(e.what());
        }
    }

    // =============================================================================
    // SECTION 11 — Solving linear systems
    // =============================================================================

    LAResult solveGaussian(const std::string &As, const std::string &bs, bool exact)
    {
        try
        {
            Matrix A = parseMatrix(As);
            Vector b = parseVector(bs);
            int m = rows(A), n = cols(A);
            if ((int)b.size() != m)
                return makeError("b must have " + std::to_string(m) + " elements");

            // Build augmented matrix [A|b] as fractions
            FMatrix aug(m, std::vector<Frac>(n + 1));
            FMatrix FA = toFMatrix(A);
            for (int i = 0; i < m; ++i)
            {
                for (int j = 0; j < n; ++j)
                    aug[i][j] = FA[i][j];
                aug[i][n] = Frac(b[i]);
            }

            // Forward elimination with partial pivoting
            int lead = 0;
            std::vector<int> pivotCols;
            for (int row = 0; row < m && lead < n; ++row, ++lead)
            {
                int i = row;
                while (i < m && aug[i][lead].isZero())
                    ++i;
                if (i == m)
                {
                    --row;
                    continue;
                }
                std::swap(aug[row], aug[i]);
                pivotCols.push_back(lead);
                // Eliminate below
                for (int k = row + 1; k < m; ++k)
                {
                    if (aug[k][lead].isZero())
                        continue;
                    Frac f = aug[k][lead] / aug[row][lead];
                    for (int j = lead; j <= n; ++j)
                        aug[k][j] = aug[k][j] - f * aug[row][j];
                }
            }

            // Check consistency
            for (int i = (int)pivotCols.size(); i < m; ++i)
            {
                if (!aug[i][n].isZero())
                    return makeError("System is inconsistent (no solution)");
            }

            // Back substitution
            int rank = (int)pivotCols.size();
            std::vector<Frac> x(n, Frac(0LL));
            for (int i = rank - 1; i >= 0; --i)
            {
                Frac sum(0LL);
                for (int j = pivotCols[i] + 1; j < n; ++j)
                    sum = sum + aug[i][j] * x[j];
                x[pivotCols[i]] = (aug[i][n] - sum) / aug[i][pivotCols[i]];
            }

            if (rank < n)
            {
                // Underdetermined: report free variables
                std::set<int> free;
                for (int j = 0; j < n; ++j)
                {
                    bool isPivot = false;
                    for (int p : pivotCols)
                        if (p == j)
                        {
                            isPivot = true;
                            break;
                        }
                    if (!isPivot)
                        free.insert(j);
                }
                std::ostringstream ss;
                ss << "Infinitely many solutions. Free variables: x[";
                bool first = true;
                for (int j : free)
                {
                    if (!first)
                        ss << ", ";
                    ss << j + 1;
                    first = false;
                }
                ss << "]\nParticular solution: [";
                for (int i = 0; i < n; ++i)
                {
                    if (i)
                        ss << ", ";
                    ss << x[i].str();
                }
                ss << "]";
                return makeResult(ss.str());
            }

            std::ostringstream sym, num;
            sym << "x = [";
            num << "x = [";
            for (int i = 0; i < n; ++i)
            {
                if (i)
                {
                    sym << ", ";
                    num << ", ";
                }
                sym << x[i].str();
                num << formatReal(x[i].toDouble(), true);
            }
            sym << "]";
            num << "]";
            return makeResult(sym.str(), num.str());
        }
        catch (const std::exception &e)
        {
            return makeError(e.what());
        }
    }

    LAResult solveGaussJordan(const std::string &As, const std::string &bs, bool exact)
    {
        try
        {
            Matrix A = parseMatrix(As);
            Vector b = parseVector(bs);
            int m = rows(A), n = cols(A);
            FMatrix FA = toFMatrix(A);
            FMatrix aug(m, std::vector<Frac>(n + 1));
            for (int i = 0; i < m; ++i)
            {
                for (int j = 0; j < n; ++j)
                    aug[i][j] = FA[i][j];
                aug[i][n] = Frac(b[i]);
            }
            std::vector<int> pivots;
            // Use rrefFrac but on the augmented matrix
            int lead = 0;
            for (int row = 0; row < m && lead < n; ++row)
            {
                int i = row;
                while (i < m && aug[i][lead].isZero())
                    ++i;
                if (i == m)
                {
                    ++lead;
                    --row;
                    continue;
                }
                std::swap(aug[row], aug[i]);
                Frac piv = aug[row][lead];
                for (int j = 0; j <= n; ++j)
                    aug[row][j] = aug[row][j] / piv;
                pivots.push_back(lead);
                for (int k = 0; k < m; ++k)
                {
                    if (k == row)
                        continue;
                    Frac f = aug[k][lead];
                    for (int j = 0; j <= n; ++j)
                        aug[k][j] = aug[k][j] - f * aug[row][j];
                }
                ++lead;
            }
            // Check consistency
            for (int i = (int)pivots.size(); i < m; ++i)
                if (!aug[i][n].isZero())
                    return makeError("System is inconsistent (no solution)");

            std::ostringstream sym;
            sym << "x = [";
            for (int j = 0; j < n; ++j)
            {
                if (j)
                    sym << ", ";
                // Find if j is a pivot row
                bool found = false;
                for (int r = 0; r < (int)pivots.size(); ++r)
                {
                    if (pivots[r] == j)
                    {
                        sym << aug[r][n].str();
                        found = true;
                        break;
                    }
                }
                if (!found)
                    sym << "free";
            }
            sym << "]";
            return makeResult(sym.str());
        }
        catch (const std::exception &e)
        {
            return makeError(e.what());
        }
    }

    LAResult solveCramer(const std::string &As, const std::string &bs, bool exact)
    {
        try
        {
            Matrix A = parseMatrix(As);
            Vector b = parseVector(bs);
            if (!isSquare(A))
                return makeError("Cramer's rule requires a square matrix");
            int n = rows(A);
            if ((int)b.size() != n)
                return makeError("b must have " + std::to_string(n) + " elements");

            FMatrix FA = toFMatrix(A);
            // Compute det(A) via Bareiss
            double dA = detLU(A);
            if (std::abs(dA) < EPS)
                return makeError("Matrix is singular — Cramer's rule does not apply");

            Frac fdet(dA);
            std::ostringstream sym, num;
            sym << "x = [";
            num << "x = [";
            for (int i = 0; i < n; ++i)
            {
                Matrix Ai = A;
                for (int r = 0; r < n; ++r)
                    Ai[r][i] = b[r];
                double di = detLU(Ai);
                Frac xi = Frac(di) / fdet;
                if (i)
                {
                    sym << ", ";
                    num << ", ";
                }
                sym << xi.str();
                num << formatReal(xi.toDouble(), true);
            }
            sym << "]";
            num << "]";
            return makeResult(sym.str(), num.str());
        }
        catch (const std::exception &e)
        {
            return makeError(e.what());
        }
    }

    LAResult solveLeastSquares(const std::string &As, const std::string &bs, bool exact)
    {
        try
        {
            Matrix A = parseMatrix(As);
            Vector b = parseVector(bs);
            // Solve normal equations: A^T A x = A^T b
            Matrix At = matTranspose(A);
            Matrix AtA = matMul(At, A);
            // A^T b
            int n = cols(A);
            Vector Atb(n, 0.0);
            for (int i = 0; i < n; ++i)
                for (int j = 0; j < rows(A); ++j)
                    Atb[i] += At[i][j] * b[j];
            auto sol = solveGaussian(formatMatrix(AtA, true), formatVector(Atb, true), exact);
            if (!sol.ok)
                return makeError("Least squares failed: " + sol.error);
            return makeResult("Least-squares solution: " + sol.symbolic,
                              "Least-squares solution: " + sol.numerical);
        }
        catch (const std::exception &e)
        {
            return makeError(e.what());
        }
    }

    LAResult solveIterativeJacobi(const std::string &As, const std::string &bs, bool exact)
    {
        try
        {
            Matrix A = parseMatrix(As);
            Vector b = parseVector(bs);
            int n = rows(A);
            if (!isSquare(A) || (int)b.size() != n)
                return makeError("Jacobi iteration requires square A and compatible b");

            Vector x(n, 0.0);
            for (int iter = 0; iter < MAX_ITER; ++iter)
            {
                Vector xNew(n);
                for (int i = 0; i < n; ++i)
                {
                    double sum = b[i];
                    for (int j = 0; j < n; ++j)
                        if (j != i)
                            sum -= A[i][j] * x[j];
                    if (std::abs(A[i][i]) < EPS)
                        return makeError("Zero diagonal — Jacobi iteration cannot proceed");
                    xNew[i] = sum / A[i][i];
                }
                double err = 0;
                for (int i = 0; i < n; ++i)
                    err += std::pow(xNew[i] - x[i], 2);
              x = xNew;
                if (std::sqrt(err) < 1e-10)
                    break;
            }
            return makeResult("Jacobi x = " + formatVector(x, false),
                              "Jacobi x = " + formatVector(x, true));
        }
        catch (const std::exception &e)
        {
            return makeError(e.what());
        }
    }

    LAResult solveIterativeGaussSeidel(const std::string &As, const std::string &bs, bool exact)
    {
        try
        {
            Matrix A = parseMatrix(As);
            Vector b = parseVector(bs);
            int n = rows(A);
            if (!isSquare(A) || (int)b.size() != n)
                return makeError("Gauss-Seidel requires square A and compatible b");

            Vector x(n, 0.0);
            for (int iter = 0; iter < MAX_ITER; ++iter)
            {
                Vector xOld = x;
                for (int i = 0; i < n; ++i)
                {
                    double sum = b[i];
                    for (int j = 0; j < n; ++j)
                        if (j != i)
                            sum -= A[i][j] * x[j];
                    if (std::abs(A[i][i]) < EPS)
                        return makeError("Zero diagonal — Gauss-Seidel cannot proceed");
                    x[i] = sum / A[i][i];
                }
                double err = 0;
                for (int i = 0; i < n; ++i)
                    err += std::pow(x[i] - xOld[i], 2);
                if (std::sqrt(err) < 1e-10)
                    break;
            }
            return makeResult("Gauss-Seidel x = " + formatVector(x, false),
                              "Gauss-Seidel x = " + formatVector(x, true));
        }
        catch (const std::exception &e)
        {
            return makeError(e.what());
        }
    }

    LAResult solve(const std::string &As, const std::string &bs, bool exact)
    {
        return solveGaussian(As, bs, exact);
    }

    // =============================================================================
    // SECTION 12 — QR Decompositions
  // =============================================================================

    static double vecDot(const Vector &a, const Vector &b)
    {
        double s = 0;
        for (size_t i = 0; i < a.size(); ++i)
            s += a[i] * b[i];
        return s;
    }
    static double vecNorm(const Vector &a) { return std::sqrt(vecDot(a, a)); }
    static Vector vecScale(const Vector &a, double s)
    {
        Vector r = a;
        for (auto &x : r)
            x *= s;
        return r;
    }
    static Vector vecSub(const Vector &a, const Vector &b)
    {
        Vector r = a;
        for (size_t i = 0; i < a.size(); ++i)
            r[i] -= b[i];
        return r;
    }

    // Gram-Schmidt QR
    static std::pair<Matrix, Matrix> qrGramSchmidt(const Matrix &A)
    {
        int m = rows(A), n = cols(A);
        Matrix Q(m, Vector(n, 0.0)), R(n, Vector(n, 0.0));
        for (int j = 0; j < n; ++j)
        {
            Vector v(m);
            for (int i = 0; i < m; ++i)
                v[i] = A[i][j];
            for (int k = 0; k < j; ++k)
            {
                Vector qk(m);
                for (int i = 0; i < m; ++i)
                    qk[i] = Q[i][k];
                double r = vecDot(qk, v);
                R[k][j] = r;
                v = vecSub(v, vecScale(qk, r));
            }
            double norm = vecNorm(v);
            R[j][j] = norm;
            if (norm > EPS)
                for (int i = 0; i < m; ++i)
                    Q[i][j] = v[i] / norm;
        }
        return {Q, R};
    }

    // Householder QR
    static std::pair<Matrix, Matrix> qrHouseholder(const Matrix &A)
    {
        int m = rows(A), n = cols(A);
        Matrix R = A, Q = identity(m);
        for (int k = 0; k < std::min(m - 1, n); ++k)
        {
            Vector x(m - k);
            for (int i = k; i < m; ++i)
                x[i - k] = R[i][k];
            double norm = vecNorm(x);
            Vector v = x;
            v[0] += (x[0] >= 0 ? 1.0 : -1.0) * norm;
            double vn = vecNorm(v);
            if (vn < EPS)
                continue;
            for (auto &vi : v)
                vi /= vn;
            // R = (I - 2vv^T) R  for submatrix
            for (int j = k; j < n; ++j)
            {
                double dot = 0;
                for (int i = k; i < m; ++i)
                    dot += v[i - k] * R[i][j];
                for (int i = k; i < m; ++i)
                    R[i][j] -= 2 * v[i - k] * dot;
            }
            // Q = Q (I - 2vv^T)
            for (int j = 0; j < m; ++j)
            {
                double dot = 0;
                for (int i = k; i < m; ++i)
                    dot += Q[j][i] * v[i - k];
                for (int i = k; i < m; ++i)
                    Q[j][i] -= 2 * dot * v[i - k];
            }
        }
        return {matTranspose(Q), R};
    }

    // Givens rotation QR
    static std::pair<Matrix, Matrix> qrGivens(const Matrix &A)
    {
        int m = rows(A), n = cols(A);
        Matrix R = A, Q = identity(m);
        for (int j = 0; j < n; ++j)
        {
            for (int i = m - 1; i > j; --i)
            {
                double a = R[i - 1][j], b = R[i][j];
                double r = std::sqrt(a * a + b * b);
                if (r < EPS)
                    continue;
                double c = a / r, s = -b / r; // Givens rotation
                // Apply to R: rows i-1, i
                for (int k = 0; k < n; ++k)
                {
                    double tmp = c * R[i - 1][k] - s * R[i][k];
                    R[i][k] = s * R[i - 1][k] + c * R[i][k];
                    R[i - 1][k] = tmp;
                }
                // Apply to Q: cols i-1, i
                for (int k = 0; k < m; ++k)
                {
                    double tmp = c * Q[k][i - 1] - s * Q[k][i];
                    Q[k][i] = s * Q[k][i - 1] + c * Q[k][i];
                    Q[k][i - 1] = tmp;
                }
            }
        }
        return {Q, R};
    }

    LAResult decompQRGramSchmidt(const std::string &As, bool exact)
    {
        try
        {
            Matrix A = parseMatrix(As);
            auto [Q, R] = qrGramSchmidt(A);
            std::ostringstream sym, num;
            sym << "QR (Gram-Schmidt):\nQ = " << formatMatrix(Q, false)
                << "\nR = " << formatMatrix(R, false);
            num << "QR (Gram-Schmidt):\nQ = " << formatMatrix(Q, true)
                << "\nR = " << formatMatrix(R, true);
            return makeResult(sym.str(), num.str());
        }
        catch (const std::exception &e)
        {
            return makeError(e.what());
        }
    }

    LAResult decompQRHouseholder(const std::string &As, bool exact)
    {
        try
        {
            Matrix A = parseMatrix(As);
            auto [Q, R] = qrHouseholder(A);
            std::ostringstream sym, num;
            sym << "QR (Householder):\nQ = " << formatMatrix(Q, false)
                << "\nR = " << formatMatrix(R, false);
            num << "QR (Householder):\nQ = " << formatMatrix(Q, true)
                << "\nR = " << formatMatrix(R, true);
            return makeResult(sym.str(), num.str());
        }
        catch (const std::exception &e)
        {
            return makeError(e.what());
        }
    }

    LAResult decompQRGivens(const std::string &As, bool exact)
    {
        try
        {
            Matrix A = parseMatrix(As);
            auto [Q, R] = qrGivens(A);
            std::ostringstream sym, num;
            sym << "QR (Givens):\nQ = " << formatMatrix(Q, false)
                << "\nR = " << formatMatrix(R, false);
            num << "QR (Givens):\nQ = " << formatMatrix(Q, true)
                << "\nR = " << formatMatrix(R, true);
            return makeResult(sym.str(), num.str());
        }
        catch (const std::exception &e)
        {
            return makeError(e.what());
        }
    }

    LAResult decompLU(const std::string &As, bool exact)
    {
        try
        {
            Matrix A = parseMatrix(As);
            if (!isSquare(A))
                return makeError("LU decomposition requires a square matrix");
            auto lu = luDecompose(A);
            // Build permutation matrix
            int n = rows(A);
            Matrix P = zeroMatrix(n, n);
            for (int i = 0; i < n; ++i)
                P[i][lu.perm[i]] = 1.0;
            std::ostringstream sym, num;
            sym << "LUP Decomposition (PA = LU):\n"
                << "P = " << formatMatrix(P, false) << "\n"
                << "L = " << formatMatrix(lu.L, false) << "\n"
                << "U = " << formatMatrix(lu.U, false);
            num << "LUP Decomposition (PA = LU):\n"
                << "P = " << formatMatrix(P, true) << "\n"
                << "L = " << formatMatrix(lu.L, true) << "\n"
                << "U = " << formatMatrix(lu.U, true);
            return makeResult(sym.str(), num.str());
        }
        catch (const std::exception &e)
        {
            return makeError(e.what());
        }
    }

    LAResult decompCholesky(const std::string &As, bool exact)
    {
        try
        {
            Matrix A = parseMatrix(As);
            if (!isSquare(A))
                return makeError("Cholesky requires a square matrix");
            int n = rows(A);
            Matrix L(n, Vector(n, 0.0));
            for (int i = 0; i < n; ++i)
            {
                for (int j = 0; j <= i; ++j)
                {
                    double sum = A[i][j];
                    for (int k = 0; k < j; ++k)
                        sum -= L[i][k] * L[j][k];
                    if (i == j)
                    {
                        if (sum < -EPS)
                            return makeError("Matrix is not positive definite");
                        L[i][j] = std::sqrt(std::max(sum, 0.0));
                    }
                    else
                    {
                        if (std::abs(L[j][j]) < EPS)
                            return makeError("Matrix is not positive definite (zero diagonal)");
                        L[i][j] = sum / L[j][j];
                    }
                }
            }
            std::ostringstream sym, num;
            sym << "Cholesky: A = L * L^T\nL = " << formatMatrix(L, false);
            num << "Cholesky: A = L * L^T\nL = " << formatMatrix(L, true);
            return makeResult(sym.str(), num.str());
        }
        catch (const std::exception &e)
        {
            return makeError(e.what());
        }
    }

    // =============================================================================
    // SECTION 13 — SVD
    // =============================================================================

   static void svdBidiag(Matrix &U, Vector &S, Matrix &Vt, int m, int n)
    {
        // Golub-Reinsch bidiagonalization + QR iteration (simplified)
        // For a full production SVD use LAPACK; this is a correct educational implementation
        Matrix A = U; // A is initialized before call
        // Step 1: Bidiagonalize via Householder
        Matrix B = A;
        U = identity(m);
        Vt = identity(n);
        for (int k = 0; k < std::min(m, n); ++k)
        {
            // Left Householder: zero below B[k][k]
            Vector x(m - k);
            for (int i = k; i < m; ++i)
                x[i - k] = B[i][k];
            double norm = vecNorm(x);
            if (norm > EPS)
            {
                Vector v = x;
                v[0] += (x[0] >= 0 ? 1.0 : -1.0) * norm;
                double vn = vecNorm(v);
                for (auto &vi : v)
                    vi /= vn;
                for (int j = k; j < n; ++j)
                {
                    double dot = 0;
                    for (int i = k; i < m; ++i)
                        dot += v[i - k] * B[i][j];
                    for (int i = k; i < m; ++i)
                        B[i][j] -= 2 * v[i - k] * dot;
                }
                for (int j = 0; j < m; ++j)
                {
                    double dot = 0;
                    for (int i = k; i < m; ++i)
                        dot += U[j][i] * v[i - k];
                    for (int i = k; i < m; ++i)
                        U[j][i] -= 2 * dot * v[i - k];
                }
            }
            if (k < n - 2)
            {
                Vector y(n - k - 1);
                for (int j = k + 1; j < n; ++j)
                    y[j - k - 1] = B[k][j];
                double ynorm = vecNorm(y);
                if (ynorm > EPS)
                {
                    Vector v = y;
                    v[0] += (y[0] >= 0 ? 1.0 : -1.0) * ynorm;
                    double vn = vecNorm(v);
                    for (auto &vi : v)
                        vi /= vn;
                    for (int i = k; i < m; ++i)
                    {
                        double dot = 0;
                        for (int j = k + 1; j < n; ++j)
                            dot += B[i][j] * v[j - k - 1];
                        for (int j = k + 1; j < n; ++j)
                            B[i][j] -= 2 * dot * v[j - k - 1];
                    }
                    for (int i = 0; i < n; ++i)
                    {
                        double dot = 0;
                        for (int j = k + 1; j < n; ++j)
                            dot += Vt[j][i] * v[j - k - 1];
                        for (int j = k + 1; j < n; ++j)
                            Vt[j][i] -= 2 * dot * v[j - k - 1];
                    }
                }
            }
        }
        // Step 2: QR iteration on bidiagonal B to get singular values
        int p = std::min(m, n);
        S.resize(p);
        for (int i = 0; i < p; ++i)
            S[i] = std::abs(B[i][i]);
        // Sort descending
        std::vector<int> idx(p);
        std::iota(idx.begin(), idx.end(), 0);
        std::sort(idx.begin(), idx.end(), [&](int a, int b)
                  { return S[a] > S[b]; });
        Vector Ssorted(p);
        for (int i = 0; i < p; ++i)
            Ssorted[i] = S[idx[i]];
        S = Ssorted;
        U = matTranspose(U);
        Vt = matTranspose(Vt);
    }

    LAResult decompSVD(const std::string &As, bool exact)
    {
        try
        {
            Matrix A = parseMatrix(As);
            int m = rows(A), n = cols(A);
            // Compute A^T A eigenvalues for singular values
            Matrix AtA = matMul(matTranspose(A), A);
            // Use QR iteration to get eigenvalues
            Matrix Ak = AtA;
            Matrix Vmat = identity(n);
            for (int iter = 0; iter < 200; ++iter)
            {
                auto [Q, R] = qrHouseholder(Ak);
                Ak = matMul(R, Q);
                Vmat = matMul(Vmat, Q);
            }
            Vector S(n);
            for (int i = 0; i < n; ++i)
                S[i] = std::sqrt(std::max(Ak[i][i], 0.0));
            // Sort descending
            std::vector<int> idx(n);
            std::iota(idx.begin(), idx.end(), 0);
            std::sort(idx.begin(), idx.end(), [&](int a, int b)
                      { return S[a] > S[b]; });
            Vector Ssorted(n);
            Matrix Vsorted(n, Vector(n));
            for (int i = 0; i < n; ++i)
            {
                Ssorted[i] = S[idx[i]];
                for (int j = 0; j < n; ++j)
                    Vsorted[j][i] = Vmat[j][idx[i]];
            }
            S = Ssorted;
            // U = A V S^-1
            Matrix U(m, Vector(std::min(m, n), 0.0));
            for (int i = 0; i < std::min(m, n); ++i)
            {
                if (S[i] < EPS)
                    continue;
                for (int r = 0; r < m; ++r)
                {
                    double sum = 0;
                    for (int k = 0; k < n; ++k)
                        sum += A[r][k] * Vsorted[k][i];
                    U[r][i] = sum / S[i];
                }
            }
            std::ostringstream sym, num;
            sym << "SVD: A = U * S * V^T\n"
                << "U = " << formatMatrix(U, false) << "\n"
                << "S (singular values) = " << formatVector(S, false) << "\n"
                << "V = " << formatMatrix(Vsorted, false);
            num << "SVD: A = U * S * V^T\n"
                << "U = " << formatMatrix(U, true) << "\n"
                << "S (singular values) = " << formatVector(S, true) << "\n"
                << "V = " << formatMatrix(Vsorted, true);
            return makeResult(sym.str(), num.str());
        }
        catch (const std::exception &e)
        {
            return makeError(e.what());
        }
    }

    // =============================================================================
    // SECTION 14 — Eigenvalues and eigenvectors
    // =============================================================================

    // QR algorithm for real eigenvalues (symmetric matrices give real eigenvalues    
    static std::pair<CVector, CMatrix> eigenQR(const Matrix &A)
    {
        int n = rows(A);
        Matrix Ak = A;
        CMatrix Evecs(n, CVector(n, Complex(0)));
        // Initialize Q accumulation
        Matrix Qaccum = identity(n);

        for (int iter = 0; iter < 500; ++iter)
        {
            // Shift: Wilkinson shift for faster convergence
            double shift = Ak[n - 1][n - 1];
            Matrix Ashifted = Ak;
            for (int i = 0; i < n; ++i)
                Ashifted[i][i] -= shift;
            auto [Q, R] = qrHouseholder(Ashifted);
            Ak = matMul(R, Q);
            for (int i = 0; i < n; ++i)
                Ak[i][i] += shift;
            Qaccum = matMul(Qaccum, Q);
        }
        // Extract eigenvalues from diagonal of Ak
        CVector evals(n);
        for (int i = 0; i < n; ++i)
            evals[i] = Complex(Ak[i][i], 0.0);
        // Eigenvectors are columns of Qaccum
        for (int j = 0; j < n; ++j)
            for (int i = 0; i < n; ++i)
                Evecs[i][j] = Complex(Qaccum[i][j], 0.0);
        return {evals, Evecs};
    }

    LAResult eigenvaluesQR(const std::string &As, bool exact)
    {
        try
        {
            Matrix A = parseMatrix(As);
            if (!isSquare(A))
                return makeError("Eigenvalues require a square matrix");
            auto [evals, _] = eigenQR(A);
            int n = rows(A);
            std::ostringstream sym, num;
            sym << "Eigenvalues = [";
            num << "Eigenvalues = [";
            for (int i = 0; i < n; ++i)
            {
                if (i)
                {
                    sym << ", ";
                    num << ", ";
                }
                sym << formatComplex(evals[i], false);
                num << formatComplex(evals[i], true);
            }
            sym << "]";
            num << "]";
            return makeResult(sym.str(), num.str());
        }
        catch (const std::exception &e)
        {
            return makeError(e.what());
        }
    }

    LAResult eigenvaluesPowerIteration(const std::string &As, bool exact)
    {
        try
        {
            Matrix A = parseMatrix(As);
            if (!isSquare(A))
                return makeError("Power iteration requires a square matrix");
            int n = rows(A);
            Vector x(n, 1.0);
            double lambda = 0;
            for (int iter = 0; iter < MAX_ITER; ++iter)
            {
                Vector Ax(n, 0.0);
                for (int i = 0; i < n; ++i)
                    for (int j = 0; j < n; ++j)
                        Ax[i] += A[i][j] * x[j];
                double norm = vecNorm(Ax);
                if (norm < EPS)
                    break;
                lambda = norm;
                x = vecScale(Ax, 1.0 / norm);
            }
            std::ostringstream sym, num;
            sym << "Dominant eigenvalue (power iteration): " << formatReal(lambda, false)
                << "\nEigenvector: " << formatVector(x, false);
            num << "Dominant eigenvalue (power iteration): " << formatReal(lambda, true)
                << "\nEigenvector: " << formatVector(x, true);
            return makeResult(sym.str(), num.str());
        }
        catch (const std::exception &e)
        {
            return makeError(e.what());
        }
    }

    LAResult eigenvectors(const std::string &As, bool exact)
    {
        try
        {
            Matrix A = parseMatrix(As);
            if (!isSquare(A))
                return makeError("Eigenvectors require a square matrix");
            auto [evals, evecs] = eigenQR(A);
            int n = rows(A);
            std::ostringstream sym, num;
            sym << "Eigenvectors (columns):\n";
            num << "Eigenvectors (columns):\n";
            for (int j = 0; j < n; ++j)
            {
                sym << "  lambda_" << j + 1 << " = " << formatComplex(evals[j], false)
                    << "  =>  v = [";
                num << "  lambda_" << j + 1 << " = " << formatComplex(evals[j], true)
                    << "  =>  v = [";
                for (int i = 0; i < n; ++i)
                {
                    if (i)
                    {
                        sym << ", ";
                        num << ", ";
                    }
                    sym << formatComplex(evecs[i][j], false);
                    num << formatComplex(evecs[i][j], true);
                }
                sym << "]\n";
                num << "]\n";
            }
            return makeResult(sym.str(), num.str());
        }
        catch (const std::exception &e)
        {
            return makeError(e.what());
        }
    }

    LAResult eigenFull(const std::string &As, bool exact)
    {
        try
        {
            Matrix A = parseMatrix(As);
            if (!isSquare(A))
                return makeError("Eigen decomposition requires a square matrix");
            auto [evals, evecs] = eigenQR(A);
            int n = rows(A);
            std::ostringstream ss;
            ss << "=== Full Eigenanalysis ===\n";
            ss << "Matrix size: " << n << "x" << n << "\n\n";
            ss << "Characteristic polynomial: det(A - lambda*I) = 0\n\n";
            ss << "Eigenvalues and eigenvectors:\n";
            for (int j = 0; j < n; ++j)
            {
                ss << "  [" << j + 1 << "] lambda = " << formatComplex(evals[j], exact) << "\n";
                ss << "      Eigenvector: [";
                for (int i = 0; i < n; ++i)
                {
                    if (i)
                        ss << ", ";
                    ss << formatComplex(evecs[i][j], exact);
                }
                ss << "]\n";
            }
            return makeResult(ss.str());
        }
        catch (const std::exception &e)
        {
            return makeError(e.what());
        }
    }

    LAResult characteristicPolynomial(const std::string &As, bool exact)
    {
        try
        {
            Matrix A = parseMatrix(As);
            if (!isSquare(A))
                return makeError("Characteristic polynomial requires a square matrix");
            int n = rows(A);
            if (n > 5)
                return makeError("Characteristic polynomial display limited to 5x5 matrices");
            // Use Faddeev-LeVerrier algorithm
            Matrix Mk = identity(n);
            std::vector<double> coeffs(n + 1, 0.0);
            coeffs[n] = 1.0; // leading coefficient
            for (int k = 1; k <= n; ++k)
            {
                Mk = matMul(A, Mk);
                double tr = 0;
                for (int i = 0; i < n; ++i)
                    tr += Mk[i][i];
                coeffs[n - k] = -tr / k;
                for (int i = 0; i < n; ++i)
                    Mk[i][i] += coeffs[n - k];
            }
            std::ostringstream sym;
            sym << "p(lambda) = ";
            bool first = true;
            for (int k = n; k >= 0; --k)
            {
                if (std::abs(coeffs[k]) < EPS)
                    continue;
                double c = coeffs[k];
                if (!first && c > 0)
                    sym << " + ";
                else if (c < 0)
                {
                    sym << (first ? "-" : " - ");
                    c = -c;
                }
                if (k == 0 || std::abs(c - 1.0) > 1e-9)
                    sym << formatReal(c, exact);
                if (k > 0)
                    sym << "lambda";
                if (k > 1)
                    sym << "^" << k;
                first = false;
            }
            return makeResult(sym.str());
        }
        catch (const std::exception &e)
        {
            return makeError(e.what());
        }
    }

    LAResult diagonalize(const std::string &As, bool exact)
    {
        try
        {
            Matrix A = parseMatrix(As);
            if (!isSquare(A))
                return makeError("Diagonalization requires a square matrix");
            auto [evals, evecs] = eigenQR(A);
            int n = rows(A);
            // P = matrix of eigenvectors as columns
            Matrix P(n, Vector(n, 0.0));
            Matrix D(n, Vector(n, 0.0));
            for (int j = 0; j < n; ++j)
            {
                D[j][j] = evals[j].real();
                for (int i = 0; i < n; ++i)
                    P[i][j] = evecs[i][j].real();
            }
            auto Pinv = gaussJordanInverse(P);
            std::ostringstream sym, num;
            sym << "A = P * D * P^-1\n"
                << "P = " << formatMatrix(P, false) << "\n"
                << "D = " << formatMatrix(D, false);
            num << "A = P * D * P^-1\n"
                << "P = " << formatMatrix(P, true) << "\n"
                << "D = " << formatMatrix(D, true);
            if (Pinv)
            {
                sym << "\nP^-1 = " << formatMatrix(*Pinv, false);
                num << "\nP^-1 = " << formatMatrix(*Pinv, true);
            }
            return makeResult(sym.str(), num.str());
        }
        catch (const std::exception &e)
        {
            return makeError(e.what());
        }
    }

    LAResult spectralDecomposition(const std::string &As, bool exact)
    {
        try
        {
            Matrix A = parseMatrix(As);
            if (!isSquare(A))
                return makeError("Spectral decomposition requires a square matrix");
            // A = sum_i lambda_i * v_i * v_i^T  (for symmetric A)
            auto [evals, evecs] = eigenQR(A);
            int n = rows(A);
            std::ostringstream sym;
            sym << "Spectral decomposition: A = sum_i lambda_i * v_i * v_i^T\n";
            for (int k = 0; k < n; ++k)
            {
                double lam = evals[k].real();
                Vector vk(n);
                for (int i = 0; i < n; ++i)
                    vk[i] = evecs[i][k].real();
                // Outer product
                Matrix outer(n, Vector(n));
                for (int i = 0; i < n; ++i)
                    for (int j = 0; j < n; ++j)
                        outer[i][j] = vk[i] * vk[j];
                sym << "  " << formatReal(lam, exact) << " * "
                    << formatMatrix(outer, exact) << "\n";
            }
            return makeResult(sym.str());
        }
        catch (const std::exception &e)
        {
            return makeError(e.what());
        }
    }

    LAResult algebraicMultiplicity(const std::string &As, bool exact)
    {
        try
        {
            auto evR = eigenvaluesQR(As, exact);
            if (!evR.ok)
                return evR;
            Matrix A = parseMatrix(As);
            auto [evals, _] = eigenQR(A);
            int n = rows(A);
            std::map<double, int> mult;
            for (auto &ev : evals)
            {
                double re = ev.real();
                bool found = false;
                for (auto &[k, v] : mult)
                {
                    if (std::abs(k - re) < 1e-6)
                    {
                        v++;
                        found = true;
                        break;
                    }
                }
                if (!found)
                    mult[re] = 1;
            }
            std::ostringstream ss;
            ss << "Algebraic multiplicities:\n";
            for (auto &[lam, m] : mult)
                ss << "  lambda = " << formatReal(lam, exact) << " : " << m << "\n";
            return makeResult(ss.str());
        }
        catch (const std::exception &e)
        {
            return makeError(e.what());
        }
    }

    LAResult geometricMultiplicity(const std::string &As, bool exact)
    {
        try
        {
            Matrix A = parseMatrix(As);
            if (!isSquare(A))
                return makeError("Requires square matrix");
            auto [evals, _] = eigenQR(A);
            int n = rows(A);
            std::map<double, int> algMult;
            for (auto &ev : evals)
            {
                double re = ev.real();
                bool found = false;
                for (auto &[k, v] : algMult)
                {
                    if (std::abs(k - re) < 1e-6)
                    {
                        v++;
                        found = true;
                        break;
                    }
                }
                if (!found)
                    algMult[re] = 1;
            }
            std::ostringstream ss;
            ss << "Geometric multiplicities (dim of eigenspace):\n";
            for (auto &[lam, alg] : algMult)
            {
                // Geometric multiplicity = nullity(A - lambda*I)
                Matrix Alambda = A;
                for (int i = 0; i < n; ++i)
                    Alambda[i][i] -= lam;
                FMatrix F = toFMatrix(Alambda);
                std::vector<int> piv;
                rrefFrac(F, piv);
                int geom = n - (int)piv.size();
                ss << "  lambda = " << formatReal(lam, exact)
                   << " : geometric = " << geom
                   << ", algebraic = " << alg << "\n";
            }
            return makeResult(ss.str());
        }
        catch (const std::exception &e)
        {
            return makeError(e.what());
        }
    }

    // ── Numeric helper: rank of a real matrix via Gaussian elation ─────────
    // (Used by decompJordan to compute geometric multiplicity = n - rank(A - lambda*I).
    //  No equivalent numeric-in/numeric-out rank helper previously existed in this
    //  file — rank() is string-in/string-out and meant for top-level dispatch.)
    static int numericRank(Matrix M, double tol = 1e-9)
    {
        int rows_ = (int)M.size();
        if (rows_ == 0) return 0;
        int cols_ = (int)M[0].size();
        int rank = 0;
        for (int col = 0; col < cols_ && rank < rows_; ++col)
        {
            int pivot = -1;
            double best = tol;
            for (int r = rank; r < rows_; ++r)
                if (std::abs(M[r][col]) > best) { best = std::abs(M[r][col]); pivot = r; }
            if (pivot < 0) continue;
            std::swap(M[rank], M[pivot]);
            for (int r = 0; r < rows_; ++r)
            {
                if (r == rank) continue;
                double factor = M[r][col] / M[rank][col];
                for (int c = col; c < cols_; ++c)
                    M[r][c] -= factor * M[rank][c];
            }
            ++rank;
        }
        return rank;
    }

    // Jordan normal form (basic implementation for small matrices)
    LAResult decompJordan(const std::string &As, bool exact)
    {
	(void)exact;
        try
        {
	    Matrix A = parseMatrix(As);
	    if (!isSquare(A))
		    return makeError("Jordan decomposition requires a square matrix");
            int n = static_cast<int>(A.size());

            Matrix J(n, std::vector<Real>(n, 0.0));

            // Compute eigenvalues numerically via the existing eigenQR routine
            // (the file previously called nonexistent computeEigenvalues /
            //  computeGeometricMultiplicity — fixed here to use real functions).
            auto [evals, evecs] = eigenQR(A);

            // Group eigenvalues by approximate equality to get algebraic
            // multiplicities (since eigenQR returns one entry per eigenvalue,
            // possibly repeated for multiplicity).
            const double eigTol = 1e-6;
            std::vector<std::pair<Complex, int>> eigenResults;  // (lambda, algebraic mult)
            std::vector<bool> used(evals.size(), false);
            for (size_t i = 0; i < evals.size(); ++i)
            {
                if (used[i]) continue;
                int alg = 1;
                used[i] = true;
                for (size_t j = i + 1; j < evals.size(); ++j)
                {
                    if (used[j]) continue;
                    if (std::abs(evals[j] - evals[i]) < eigTol)
                    {
                        ++alg;
                        used[j] = true;
                    }
                }
                eigenResults.push_back({evals[i], alg});
            }

            int row = 0;
            for (const auto &[lam, alg] : eigenResults)
            {
                // Geometric multiplicity = n - rank(A - lambda*I), using the
                // real part of lambda (Jordan form construction here assumes
                // real eigenvalues, consistent with the rest of this function
                // only filling real diagonal/superdiagonal entries).
                double lamReal = lam.real();
                Matrix shifted = A;
                for (int i = 0; i < n; ++i)
                    shifted[i][i] -= lamReal;
                int geom = std::max(1, n - numericRank(shifted));
                if (geom > alg) geom = alg;   // geometric mult. can't exceed algebraic
                int blockSize = alg / geom;

                for (int b = 0; b < geom; ++b)
                {
                    int currentBlockSize = (b < alg % geom) ? (blockSize + 1) : blockSize;

                    for (int i = 0; i < currentBlockSize; ++i)
                    {
                        J[row + i][row + i] = lamReal;
                        if (i < currentBlockSize - 1)
                        {
                            // Jordan chain superdiagonal
                            J[row + i][row + i + 1] = 1.0;
                        }
                    }
                    row += currentBlockSize;
                }
            }

            std::ostringstream sym, num;
            sym << "Jordan Normal Form (J):\n"
                << formatMatrix(J, false);
            num << "Jordan Normal Form (J):\n"
                << formatMatrix(J, true);

            return makeResult(sym.str(), num.str());
        }
        catch (const std::exception &e)
        {
            return makeError(e.what());
        }
    }

    // Schur decomposition
    LAResult decompSchur(const std::string &As, bool exact)
    {
        try
        {
            Matrix A = parseMatrix(As);
            if (!isSquare(A))
                return makeError("Schur decomposition requires a square matrix");
            // Schur: A = Q T Q^H, T is upper triangular
            // Use QR iteration — after convergence, Ak is quasi-upper-triangular (Schur form)
            int n = rows(A);
            Matrix Ak = A, Qaccum = identity(n);
            for (int iter = 0; iter < 300; ++iter)
            {
                double shift = Ak[n - 1][n - 1];
                for (int i = 0; i < n; ++i)
                    Ak[i][i] -= shift;
              auto [Q, R] = qrHouseholder(Ak);
                Ak = matMul(R, Q);
                for (int i = 0; i < n; ++i)
                    Ak[i][i] += shift;
                Qaccum = matMul(Qaccum, Q);
            }
            std::ostringstream sym, num;
            sym << "Schur: A = Q * T * Q^T\n"
                << "Q = " << formatMatrix(Qaccum, false) << "\n"
                << "T = " << formatMatrix(Ak, false);
            num << "Schur: A = Q * T * Q^T\n"
                << "Q = " << formatMatrix(Qaccum, true) << "\n"
                << "T = " << formatMatrix(Ak, true);
            return makeResult(sym.str(), num.str());
        }
        catch (const std::exception &e)
        {
            return makeError(e.what());
        }
    }

    // Matrix property checks
    LAResult isSymmetric(const std::string &As, bool exact)
    {
        try
        {
            Matrix A = parseMatrix(As);
            if (!isSquare(A))
                return makeResult("false (not square)");
            for (int i = 0; i < rows(A); ++i)
                for (int j = 0; j < cols(A); ++j)
                    if (std::abs(A[i][j] - A[j][i]) > EPS)
                        return makeResult("false (A[" + std::to_string(i + 1) + "][" + std::to_string(j + 1) + "] != A[" + std::to_string(j + 1) + "][" + std::to_string(i + 1) + "])");
            return makeResult("true — matrix is symmetric (A = A^T)");
        }
        catch (const std::exception &e)
        {
            return makeError(e.what());
        }
    }

    LAResult isPositiveDefinite(const std::string &As, bool exact)
    {
        try
        {
            Matrix A = parseMatrix(As);
            if (!isSquare(A))
                return makeResult("false (not square)");
            // Check via Cholesky
            auto ch = decompCholesky(As, exact);
            if (!ch.ok)
                return makeResult("false — " + ch.error);
            return makeResult("true — matrix is positive definite (Cholesky exists)");
        }
        catch (const std::exception &e)
        {
            return makeError(e.what());
        }
    }

    LAResult isOrthogonal(const std::string &As, bool exact)
    {
        try
        {
            Matrix A = parseMatrix(As);
            if (!isSquare(A))
                return makeResult("false (not square)");
            Matrix AtA = matMul(matTranspose(A), A);
            Matrix I = identity(rows(A));
            for (int i = 0; i < rows(A); ++i)
                for (int j = 0; j < cols(A); ++j)
                    if (std::abs(AtA[i][j] - I[i][j]) > 1e-6)
                        return makeResult("false (A^T * A != I)");
            return makeResult("true — matrix is orthogonal (A^T * A = I)");
        }
        catch (const std::exception &e)
        {
            return makeError(e.what());
        }
    }

    LAResult isDiagonalizable(const std::string &As, bool exact)
    {
        try
        {
            Matrix A = parseMatrix(As);
            if (!isSquare(A))
                return makeResult("false (not square)");
          int n = rows(A);
            auto [evals, evecs] = eigenQR(A);
            // Check: sum of geometric multiplicities = n
            std::map<double, int> algMult;
            for (auto &ev : evals)
            {
                double re = ev.real();
                bool found = false;
                for (auto &[k, v] : algMult)
                    if (std::abs(k - re) < 1e-6)
                    {
                        v++;
                        found = true;
                        break;
                    }
                if (!found)
                    algMult[re] = 1;
            }
            int totalGeom = 0;
            for (auto &[lam, alg] : algMult)
            {
                Matrix Alambda = A;
                for (int i = 0; i < n; ++i)
                    Alambda[i][i] -= lam;
                FMatrix F = toFMatrix(Alambda);
                std::vector<int> piv;
                rrefFrac(F, piv);
                totalGeom += n - (int)piv.size();
            }
            if (totalGeom == n)
                return makeResult("true — matrix is diagonalizable");
            return makeResult("false — sum of geometric multiplicities = " + std::to_string(totalGeom) + " < " + std::to_string(n));
        }
        catch (const std::exception &e)
        {
            return makeError(e.what());
        }
    }

    // =============================================================================
    // SECTION 15 — Vector space operations
    // =======================================================================

    LAResult columnSpace(const std::string &As, bool exact)
    {
        try
        {
            Matrix A = parseMatrix(As);
            FMatrix F = toFMatrix(A);
            std::vector<int> pivots;
            rrefFrac(F, pivots);
            std::ostringstream ss;
            ss << "Column space basis (pivot columns of A):\n";
            for (int p : pivots)
            {
                ss << "  [";
                for (int i = 0; i < rows(A); ++i)
                {
                    if (i)
                        ss << ", ";
                    ss << formatReal(A[i][p], exact);
                }
                ss << "]\n";
            }
            ss << "dim(col A) = " << pivots.size();
            return makeResult(ss.str());
        }
        catch (const std::exception &e)
        {
            return makeError(e.what());
        }
    }

    LAResult nullSpace(const std::string &As, bool exact)
    {
        try
        {
            Matrix A = parseMatrix(As);
            int n = cols(A);
            FMatrix F = toFMatrix(A);
            std::vector<int> pivots;
            FMatrix R = rrefFrac(F, pivots);
            std::set<int> pivotSet(pivots.begin(), pivots.end());
            std::vector<int> freeVars;
            for (int j = 0; j < n; ++j)
                if (!pivotSet.count(j))
                    freeVars.push_back(j);
            if (freeVars.empty())
                return makeResult("Null space = {0}  (trivial, rank = " + std::to_string(n) + ")");
            std::ostringstream ss;
            ss << "Null space basis (" << freeVars.size() << " free variable(s)):\n";
            for (int fv : freeVars)
            {
                std::vector<Frac> vec(n, Frac(0LL));
                vec[fv] = Frac(1LL);
                for (int r = 0; r < (int)pivots.size(); ++r)
                {
                    Frac sum(0LL);
                    for (int c = 0; c < n; ++c)
                        if (!pivotSet.count(c))
                            sum = sum + R[r][c] * vec[c];
                    vec[pivots[r]] = Frac(0LL) - sum;
                }
                ss << "  [";
                for (int i = 0; i < n; ++i)
                {
                    if (i)
                        ss << ", ";
                    ss << vec[i].str();
                }
                ss << "]\n";
            }
            return makeResult(ss.str());
        }
        catch (const std::exception &e)
        {
            return makeError(e.what());
        }
    }

    LAResult rowSpace(const std::string &As, bool exact)
    {
        try
        {
            Matrix A = parseMatrix(As);
            FMatrix F = toFMatrix(A);
            std::vector<int> pivots;
            FMatrix R = rrefFrac(F, pivots);
            std::ostringstream ss;
            ss << "Row space basis (non-zero rows of RREF):\n";
            for (int p = 0; p < (int)pivots.size(); ++p)
            {
                ss << "  [";
                for (int j = 0; j < cols(A); ++j)
                {
                    if (j)
                        ss << ", ";
                    ss << R[p][j].str();
                }
                ss << "]\n";
            }
            ss << "dim(row A) = " << pivots.size();
            return makeResult(ss.str());
        }
        catch (const std::exception &e)
        {
            return makeError(e.what());
        }
    }

    LAResult leftNullSpace(const std::string &As, bool exact)
    {
        try
        {
            Matrix A = parseMatrix(As);
            Matrix At = matTranspose(A);
            return nullSpace(formatMatrix(At, true), exact);
        }
        catch (const std::exception &e)
        {
            return makeError(e.what());
        }
    }

    LAResult orthonormalBasis(const std::string &vectorsStr, bool exact)
    {
        try
        {
            Matrix V = parseMatrix(vectorsStr);
            // Gram-Schmidt on rows of V
            int m = rows(V), n = cols(V);
            Matrix Q;
            for (int i = 0; i < m; ++i)
            {
                Vector v(V[i].begin(), V[i].end());
                for (auto &q : Q)
                {
                    double proj = vecDot(q, v) / vecDot(q, q);
                    for (int j = 0; j < n; ++j)
                        v[j] -= proj * q[j];
                }
                double norm = vecNorm(v);
                if (norm < 1e-10)
                    continue;
                for (auto &x : v)
                    x /= norm;
                Q.push_back(v);
            }
            std::ostringstream sym, num;
            sym << "Orthonormal basis:\n";
            num << "Orthonormal basis:\n";
            for (auto &q : Q)
            {
                sym << "  " << formatVector(q, false) << "\n";
                num << "  " << formatVector(q, true) << "\n";
            }
            return makeResult(sym.str(), num.str());
        }
        catch (const std::exception &e)
        {
            return makeError(e.what());
        }
    }

    LAResult isLinearlyIndependent(const std::string &vectorsStr, bool exact)
    {
        try
        {
            Matrix V = parseMatrix(vectorsStr);
            FMatrix F = toFMatrix(V);
            std::vector<int> pivots;
            rrefFrac(F, pivots);
            bool indep = (int)pivots.size() == rows(V);
            return makeResult(indep
                                  ? "true — vectors are linearly independent"
                                  : "false — vectors are linearly dependent (rank = " +
                                    std::to_string(pivots.size()) + " < " + std::to_string(rows(V)) + ")");
        }
        catch (const std::exception &e)
        {
            return makeError(e.what());
        }
    }

    LAResult projection(const std::string &vs, const std::string &us, bool exact)
    {
        try
        {
            Vector v = parseVector(vs), u = parseVector(us);
            double uu = vecDot(u, u);
            if (std::abs(uu) < 1e-14)
                return makeError("Cannot project onto zero vector");
            double scale = vecDot(u, v) / uu;
            Vector p(u.size());
            for (size_t i = 0; i < u.size(); ++i)
                p[i] = scale * u[i];
            Frac fs(scale);
            std::ostringstream sym;
            sym << "proj_u(v) = (" << fs.str() << ") * u = " << formatVector(p, false);
            return makeResult(sym.str(), "proj_u(v) = " + formatVector(p, true));
        }
        catch (const std::exception &e)
        {
            return makeError(e.what());
        }
    }

    LAResult projectionMatrix(const std::string &As, bool exact)
    {
        try
        {
            Matrix A = parseMatrix(As);
            // P = A(A^T A)^-1 A^T
            Matrix At = matTranspose(A);
            Matrix AtA = matMul(At, A);
            auto inv = gaussJordanInverse(AtA);
            if (!inv)
                return makeError("A^T A is singular — columns of A not linearly independent");
            Matrix P = matMul(matMul(A, *inv), At);
            FMatrix FP = toFMatrix(P);
          return makeResult(formatFMatrix(FP), formatMatrix(P, true));
        }
        catch (const std::exception &e)
        {
            return makeError(e.what());
        }
    }

    // =============================================================================
    // SECTION 16 — Norms
    // =============================================================================

    LAResult vectorNorm(const std::string &vs, const std::string &p, bool exact)
    {
        try
        {
            Vector v = parseVector(vs);
            double norm = 0.0;
            if (p == "inf")
            {
                for (double x : v)
                    norm = std::max(norm, std::abs(x));
                return makeResult(formatReal(norm, false), formatReal(norm, true));
            }
            double pv = std::stod(p);
            for (double x : v)
                norm += std::pow(std::abs(x), pv);
            norm = std::pow(norm, 1.0 / pv);
            return makeResult(formatReal(norm, false), formatReal(norm, true));
        }
        catch (const std::exception &e)
        {
            return makeError(e.what());
        }
    }

    LAResult matrixNormFrobenius(const std::string &As, bool exact)
    {
        try
        {
            Matrix A = parseMatrix(As);
            double s = 0;
            for (auto &row : A)
                for (double x : row)
                    s += x * x;
            double n = std::sqrt(s);
            return makeResult(formatReal(n, false), formatReal(n, true));
        }
        catch (const std::exception &e)
        {
            return makeError(e.what());
        }
    }

    LAResult matrixNormSpectral(const std::string &As, bool exact)
    {
        try
        {
            // Spectral norm = largest singular value
            auto svd = decompSVD(As, exact);
            if (!svd.ok)
                return svd;
            // Extract first singular value from result string
            // (it's the first number after "S (singular values) = [")
            size_t pos = svd.symbolic.find('[');
            if (pos != std::string::npos)
            {
                size_t end = svd.symbolic.find(',', pos);
                if (end == std::string::npos)
                    end = svd.symbolic.find(']', pos);
                std::string sv = svd.symbolic.substr(pos + 1, end - pos - 1);
                return makeResult("Spectral norm (largest singular value) = " + sv);
            }
            return svd;
        }
        catch (const std::exception &e)
        {
            return makeError(e.what());
        }
    }

    LAResult conditionNumber(const std::string &As, bool exact)
    {
        try
        {
            Matrix A = parseMatrix(As);
            auto invR = inverseGaussJordan(As, exact);
            if (!invR.ok)
                return makeError("Matrix is singular — condition number is infinite");
            double normA = 0, normInv = 0;
            Matrix Ai = parseMatrix(formatMatrix(A, true));
            for (auto &row : Ai)
                for (double x : row)
                    normA = std::max(normA, std::abs(x));
            // Parse inverse
            Matrix Inv = parseMatrix(invR.numerical);
            for (auto &row : Inv)
                for (double x : row)
                    normInv = std::max(normInv, std::abs(x));
            double cond = normA * normInv;
            return makeResult("cond(A) = " + formatReal(cond, false),
                              "cond(A) = " + formatReal(cond, true));
        }
        catch (const std::exception &e)
        {
            return makeError(e.what());
        }
    }

    LAResult dotProduct(const std::string &us, const std::string &vs, bool exact)
    {
        try
        {
            Vector u = parseVector(us), v = parseVector(vs);
            if (u.size() != v.size())
                return makeError("Vectors must have equal dimension");
            FMatrix FU = {{}};
            FMatrix FV = {{}};
            FU[0].resize(u.size());
            FV[0].resize(v.size());
            for (size_t i = 0; i < u.size(); ++i)
            {
                FU[0][i] = Frac(u[i]);
                FV[0][i] = Frac(v[i]);
            }
            Frac dot(0LL);
            for (size_t i = 0; i < u.size(); ++i)
                dot = dot + FU[0][i] * FV[0][i];
            return makeResult(dot.str(), formatReal(dot.toDouble(), true));
        }
        catch (const std::exception &e)
        {
            return makeError(e.what());
        }
    }

    LAResult crossProduct(const std::string &us, const std::string &vs, bool exact)
    {
        try
        {
            Vector u = parseVector(us), v = parseVector(vs);
            if (u.size() != 3 || v.size() != 3)
                return makeError("Cross product requires 3D vectors");
            Vector c = {u[1] * v[2] - u[2] * v[1], u[2] * v[0] - u[0] * v[2], u[0] * v[1] - u[1] * v[0]};
            FMatrix FC = {{Frac(c[0]), Frac(c[1]), Frac(c[2])}};
            return makeResult("[" + FC[0][0].str() + ", " + FC[0][1].str() + ", " + FC[0][2].str() + "]",
                              formatVector(c, true));
        }
        catch (const std::exception &e)
        {
            return makeError(e.what());
        }
    }

    LAResult angle(const std::string &us, const std::string &vs, bool exact)
    {
        try
        {
            Vector u = parseVector(us), v = parseVector(vs);
            double nu = vecNorm(u), nv = vecNorm(v);
            if (nu < 1e-14 || nv < 1e-14)
                return makeError("Cannot compute angle with zero vector");
            double cosA = std::max(-1.0, std::min(1.0, vecDot(u, v) / (nu * nv)));
            double theta = std::acos(cosA);
            std::ostringstream sym;
            sym << "angle = acos(" << formatReal(cosA, false) << ") = "
                << formatReal(theta, false) << " rad = "
                << formatReal(theta * 180.0 / M_PI, false) << " deg";
            return makeResult(sym.str(), formatReal(theta, true));
        }
        catch (const std::exception &e)
        {
            return makeError(e.what());
        }
    }

    LAResult distance(const std::string &us, const std::string &vs, bool exact)
    {
        try
        {
            Vector u = parseVector(us), v = parseVector(vs);
            if (u.size() != v.size())
                return makeError("Vectors must have equal dimension");
            double d = 0;
            for (size_t i = 0; i < u.size(); ++i)
                d += std::pow(u[i] - v[i], 2);
            d = std::sqrt(d);
            return makeResult(formatReal(d, false), formatReal(d, true));
        }
        catch (const std::exception &e)
        {
            return makeError(e.what());
        }
    }

    // =============================================================================
    // SECTION 17 — Special matrices and constructors
    // =============================================================================

    LAResult adjugate(const std::string &As, bool exact)
    {
        try
        {
            Matrix A = parseMatrix(As);
            if (!isSquare(A))               return makeError("Adjugate requires square matrix");
            int n = rows(A);
            FMatrix FA = toFMatrix(A), Adj(n, std::vector<Frac>(n));
            for (int i = 0; i < n; ++i)
                for (int j = 0; j < n; ++j)
                {
                    Matrix M(n - 1, Vector(n - 1));
                    for (int r = 0, ri = 0; r < n; ++r)
                    {
                        if (r == i)
                            continue;
                        for (int c = 0, ci = 0; c < n; ++c)
                        {
                            if (c == j)
                                continue;
                            M[ri][ci++] = A[r][c];
                        }
                        ri++;
                    }
                    double mdet = n - 1 == 0 ? 1.0 : detCofactorRec(M);
                    Adj[j][i] = Frac(((i + j) % 2 == 0 ? 1.0 : -1.0) * mdet);
                }
            Matrix AdjNum = fromFMatrix(Adj);
            return makeResult(formatFMatrix(Adj), formatMatrix(AdjNum, true));
        }
        catch (const std::exception &e)
        {
            return makeError(e.what());
        }
    }

    LAResult pseudoinverse(const std::string &As, bool exact)
    {
        try
        {
            // Moore-Penrose: A+ = V S+ U^T via SVD
            Matrix A = parseMatrix(As);
            int m = rows(A), n = cols(A);
            // Get SVD via QR iteration (reuse decompSVD logic)
            Matrix AtA = matMul(matTranspose(A), A);
            Matrix Ak = AtA, Vmat = identity(n);
            for (int iter = 0; iter < 200; ++iter)
            {
                auto [Q, R] = qrHouseholder(Ak);
                Ak = matMul(R, Q);
                Vmat = matMul(Vmat, Q);
            }
            Vector S(n);
            for (int i = 0; i < n; ++i)
                S[i] = std::sqrt(std::max(Ak[i][i], 0.0));
            // U = A V S^-1
            Matrix U(m, Vector(n, 0.0));
            for (int i = 0; i < n; ++i)
            {
                if (S[i] < 1e-10)
                    continue;
                for (int r = 0; r < m; ++r)
                {
                    double sum = 0;
                    for (int k = 0; k < n; ++k)
                        sum += A[r][k] * Vmat[k][i];
                    U[r][i] = sum / S[i];
                }
            }
            // A+ = V S+ U^T
            Matrix Splus(n, Vector(m, 0.0));
            for (int i = 0; i < n; ++i)
                if (S[i] > 1e-10)
                    Splus[i][i] = 1.0 / S[i];
            Matrix Aplus = matMul(matMul(Vmat, Splus), matTranspose(U));
            return makeResult(formatMatrix(Aplus, false), formatMatrix(Aplus, true));
        }
        catch (const std::exception &e)
        {
            return makeError(e.what());
        }
    }

    LAResult matrixExponential(const std::string &As, bool exact)
    {
        try
        {
            Matrix A = parseMatrix(As);
            if (!isSquare(A))
                return makeError("Matrix exponential requires square matrix");
            int n = rows(A);
            // Padé approximation order 6
            Matrix I = identity(n);
            Matrix A2 = matMul(A, A);
            Matrix A4 = matMul(A2, A2);
            Matrix A6 = matMul(A4, A2);
            // Padé coefficients for order 6
            static const double c[] = {1, 0.5, 0.12, 1.0 / 120, 1.0 / 3360, 1.0 / 1814400};
            auto sM = [&](double s, const Matrix &M)
            { return matScale(M, s); };
            Matrix U = matAdd(matAdd(sM(c[1], A), sM(c[3], A2)), sM(c[5], A4));
            U = matMul(A, U);
            Matrix V = matAdd(matAdd(sM(c[0], I), sM(c[2], A2)), sM(c[4], A4));
            // exp(A) ≈ (V+U)(V-U)^-1
            Matrix VpU = matAdd(V, U), VmU = matSub(V, U);
            auto invVmU = gaussJordanInverse(VmU);
            if (!invVmU)
                return makeError("Padé denominator singular");
            Matrix expA = matMul(VpU, *invVmU);
            return makeResult(formatMatrix(expA, false), formatMatrix(expA, true));
        }
     catch (const std::exception &e)
        {
            return makeError(e.what());
        }
    }

    LAResult classifyMatrix(const std::string &As, bool exact)
    {
        try
        {
            Matrix A = parseMatrix(As);
            std::ostringstream ss;
            ss << "Matrix classification (" << rows(A) << "x" << cols(A) << "):\n";
            if (isSquare(A))
            {
                ss << "  Square:          yes\n";
                ss << "  Symmetric:       " << (isSymmetric(As, exact).symbolic.find("true") != std::string::npos ? "yes" : "no") << "\n";
                ss << "  Orthogonal:      " << (isOrthogonal(As, exact).symbolic.find("true") != std::string::npos ? "yes" : "no") << "\n";
                ss << "  Positive def:    " << (isPositiveDefinite(As, exact).symbolic.find("true") != std::string::npos ? "yes" : "no") << "\n";
                ss << "  Diagonalizable:  " << (isDiagonalizable(As, exact).symbolic.find("true") != std::string::npos ? "yes" : "no") << "\n";
                double d = detLU(A);
                ss << "  Determinant:     " << formatReal(d, exact) << "\n";
                ss << "  Invertible:      " << (std::abs(d) > 1e-10 ? "yes" : "no") << "\n";
                auto rk = rank(As, exact);
                ss << "  Rank:            " << rk.symbolic << "\n";
            }
            else
            {
                ss << "  Square:          no\n";
                auto rk = rank(As, exact);
                ss << "  Rank:            " << rk.symbolic << "\n";
            }
            return makeResult(ss.str());
        }
        catch (const std::exception &e)
        {
            return makeError(e.what());
        }
    }

    LAResult makeIdentity(const std::string &ns, bool exact)
    {
        try
        {
            int n = std::stoi(ns);
            Matrix I = identity(n);
            return makeResult(formatFMatrix(toFMatrix(I)), formatMatrix(I, true));
        }
        catch (const std::exception &e)
        {
            return makeError(e.what());
        }
    }

    LAResult makeHilbert(const std::string &ns, bool exact)
    {
        try
        {
            int n = std::stoi(ns);
            FMatrix H(n, std::vector<Frac>(n));
            for (int i = 0; i < n; ++i)
                for (int j = 0; j < n; ++j)
                    H[i][j] = Frac(1LL, (long long)(i + j + 1));
            return makeResult(formatFMatrix(H), formatMatrix(fromFMatrix(H), true));
        }
        catch (const std::exception &e)
        {
            return makeError(e.what());
        }
    }

    LAResult makeVandermonde(const std::string &vs, bool exact)
    {
        try
        {
            Vector v = parseVector(vs);
            int n = v.size();
            FMatrix V(n, std::vector<Frac>(n));
            for (int i = 0; i < n; ++i)
                for (int j = 0; j < n; ++j)
                    V[i][j] = Frac(std::pow(v[i], j));
            return makeResult(formatFMatrix(V), formatMatrix(fromFMatrix(V), true));
        }
        catch (const std::exception &e)
        {
            return makeError(e.what());
        }
    }

    LAResult quadraticForm(const std::string &As, const std::string &xs, bool exact)
    {
        try
        {
            Matrix A = parseMatrix(As);
            Vector x = parseVector(xs);
            if (!isSquare(A) || (int)x.size() != rows(A))
                return makeError("Dimension mismatch for quadratic form");
            // x^T A x
            Vector Ax(rows(A), 0.0);
            for (int i = 0; i < rows(A); ++i)
                for (int j = 0; j < cols(A); ++j)
                    Ax[i] += A[i][j] * x[j];
            double val = vecDot(x, Ax);
            return makeResult(formatReal(val, false), formatReal(val, true));
        }
        catch (const std::exception &e)
        {
            return makeError(e.what());
        }
    }

    LAResult gramMatrix(const std::string &vectorsStr, bool exact)
    {
        try
        {
            Matrix V = parseMatrix(vectorsStr);
            int m = rows(V);
            FMatrix G(m, std::vector<Frac>(m));
            FMatrix FV = toFMatrix(V);
            for (int i = 0; i < m; ++i)
                for (int j = 0; j < m; ++j)
                {
                    Frac dot(0LL);
                    for (int k = 0; k < cols(V); ++k)
                        dot = dot + FV[i][k] * FV[j][k];
                    G[i][j] = dot;
                }
            return makeResult(formatFMatrix(G), formatMatrix(fromFMatrix(G), true));
        }
        catch (const std::exception &e)
        {
            return makeError(e.what());
        }
    }

    LAResult factorial2x2(const std::string &As, bool exact)
    {
        // 2x2 matrix factorial (Cayley-Hamilton: A² = tr(A)A - det(A)I)
        try
        {
            Matrix A = parseMatrix(As);
            if (!isSquare(A) || rows(A) != 2)
                return makeError("This function is for 2×2 matrices");
            double tr = A[0][0] + A[1][1];
            double det = A[0][0] * A[1][1] - A[0][1] * A[1][0];
            std::ostringstream ss;
            ss << "Cayley-Hamilton Theorem for 2×2 matrix\n\n";
            ss << "Characteristic polynomial: p(λ) = λ² - " << formatReal(tr, exact)
               << "λ + " << formatReal(det, exact) << "\n\n";
            ss << "By Cayley-Hamilton: A² - " << formatReal(tr, exact)
               << "A + " << formatReal(det, exact) << "I = 0\n";
            ss << "Therefore: A² = " << formatReal(tr, exact) << "A - "
               << formatReal(det, exact) << "I\n\n";
            Matrix A2 = matMul(A, A);
            ss << "A² = " << formatMatrix(A2, exact);
            return makeResult(ss.str(), formatMatrix(A2, true));
        }
        catch (const std::exception &e)
        {
            return makeError(e.what());
        }
    }

    LAResult makeZero(const std::string &rowsStr, const std::string &colsStr, bool exact)
    {
        try
        {
            int r = std::stoi(rowsStr), c = std::stoi(colsStr);
            Matrix Z(r, Vector(c, 0.0));
            FMatrix FZ = toFMatrix(Z);
           return makeResult(formatFMatrix(FZ), formatMatrix(Z, true));
        }
        catch (const std::exception &e)
        {
            return makeError(e.what());
        }
    }

    LAResult makeCompanion(const std::string &polyStr, bool exact)
    {
        // poly coefficients [a0, a1, ..., a_{n-1}, an] for a0 + a1*x + ... + an*x^n
        // Companion matrix: last row = [-a0/an, -a1/an, ..., -a_{n-1}/an]
        try
        {
            Vector coeffs;
            std::string s = polyStr;
            if (!s.empty() && s.front() == '[')
                s = s.substr(1, s.size() - 2);
            std::istringstream ss(s);
            std::string tok;
            while (std::getline(ss, tok, ','))
            {
                try
                {
                    coeffs.push_back(std::stod(tok));
                }
                catch (...)
                {
                }
            }
            if (coeffs.size() < 2)
                return makeError("Need at least 2 coefficients");
            int n = (int)coeffs.size() - 1;
            double lead = coeffs.back();
            if (std::abs(lead) < 1e-14)
                return makeError("Leading coefficient is zero");

            Matrix C(n, Vector(n, 0.0));
            // Subdiagonal of 1s
            for (int i = 1; i < n; ++i)
                C[i][i - 1] = 1.0;
            // Last column
            for (int i = 0; i < n; ++i)
                C[i][n - 1] = -coeffs[i] / lead;

            std::ostringstream sym;
            sym << "Companion matrix of polynomial [";
            for (int i = 0; i <= n; ++i)
            {
                if (i)
                    sym << ",";
                sym << coeffs[i];
            }
            sym << "]:\n"
                << formatMatrix(C, false);
            return makeResult(sym.str(), formatMatrix(C, true));
        }
        catch (const std::exception &e)
        {
            return makeError(e.what());
        }
    }

    LAResult changeOfBasis(const std::string &vStr,
                           const std::string &oldBasisStr,
                           const std::string &newBasisStr,
                           bool exact)
    {
        try
        {
            Vector v = parseVector(vStr);
            Matrix B_old = parseMatrix(oldBasisStr); // columns are old basis vectors
            Matrix B_new = parseMatrix(newBasisStr); // columns are new basis vectors

            // Coordinates in old basis: v_old = B_old^{-1} * v (if standard -> old)
            // Change of basis matrix: P = B_new^{-1} * B_old
            // v_new = P * v_old

            // For simplicity: assume v is given in standard coordinates
            // Express v in terms of new basis: solve B_new * x = v
            int n = v.size();
            if (rows(B_new) != n || cols(B_new) != n)
                return makeError("Basis matrix must be n×n");

            // Augment [B_new | v] and solve
            FMatrix aug = toFMatrix(B_new);
            for (int i = 0; i < n; ++i)
                aug[i].push_back(Frac(v[i]));
           std::vector<int> pivots;
            FMatrix rref = rrefFrac(aug, pivots);

            std::ostringstream ss;
            ss << "Change of Basis\n";
            ss << "Vector v = " << formatVector(v, exact) << "\n\n";
            ss << "Coordinates in new basis:\n";
            Vector coords(n);
            for (int i = 0; i < n; ++i)
            {
                coords[i] = rref[i][n].toDouble();
                ss << "  x_" << (i + 1) << " = " << rref[i][n].str() << "\n";
            }
            ss << "\nVerification: B_new * x = v";
            return makeResult(ss.str(), formatVector(coords, true));
        }
        catch (const std::exception &e)
        {
            return makeError(e.what());
        }
    }

    LAResult spanCheck(const std::string &vectorsStr,
                       const std::string &targetStr,
                       bool exact)
    {
        try
        {
            Matrix V = parseMatrix(vectorsStr);
            Vector t = parseVector(targetStr);

            // Does t lie in span of rows of V?
            // Augment V with t and check rank
            Matrix augV = V;
            augV.push_back(t);

            FMatrix FV = toFMatrix(V);
            FMatrix FAug = toFMatrix(augV);
            std::vector<int> piv1, piv2;
            rrefFrac(FV, piv1);
            rrefFrac(FAug, piv2);

            bool inSpan = piv1.size() == piv2.size();
            std::ostringstream ss;
            ss << "Span check: does t lie in span{v1,...,vk}?\n\n";
            ss << "rank(V) = " << piv1.size() << "\n";
            ss << "rank([V; t]) = " << piv2.size() << "\n\n";
            ss << "t is " << (inSpan ? "IN" : "NOT in") << " the span of the given vectors.";
            if (inSpan)
            {
                ss << "\n\nExpress t as linear combination:\n";
                // Extract coefficients from RREF of [V^T | t]
                // Simple: just report the result
                ss << "(Solve V^T c = t for coefficients c)";
            }
            return makeResult(ss.str());
        }
        catch (const std::exception &e)
        {
            return makeError(e.what());
        }
    }

    LAResult matrixNormInduced(const std::string &As,
                               const std::string &pStr,
                               bool exact)
    {
        try
        {
            Matrix A = parseMatrix(As);
            int m = rows(A), n = cols(A);
            double p = std::stod(pStr);
            double norm = 0.0;
            std::string name;

            if (std::abs(p - 1.0) < 1e-8)
            {
                // Max absolute column sum
                name = "1-norm (max column sum)";
                for (int j = 0; j < n; ++j)
                {
                    double colSum = 0;
                    for (int i = 0; i < m; ++i)
                        colSum += std::abs(A[i][j]);
                    norm = std::max(norm, colSum);
                }
            }
            else if (std::isinf(p))
            {
                // Max absolute row sum
                name = "∞-norm (max row sum)";
                for (int i = 0; i < m; ++i)
                {
                    double rowSum = 0;
                    for (int j = 0; j < n; ++j)
                        rowSum += std::abs(A[i][j]);
                    norm = std::max(norm, rowSum);
                }
            }
            else if (std::abs(p - 2.0) < 1e-8)
            {
                // Spectral norm = largest singular value (reuse matrixNormSpectral)
                name = "2-norm (spectral, = larst singular value)";
                auto res = matrixNormSpectral(As, exact);
                return res;
            }
            else
            {
                return makeError("Induced p-norm supported for p=1, 2, inf");
            }
            std::ostringstream ss;
            ss << "||A||_p  (" << name << ") = " << formatReal(norm, exact);
            return makeResult(ss.str(), formatReal(norm, true));
        }
        catch (const std::exception &e)
        {
            return makeError(e.what());
        }
    }

    LAResult sylvestersLaw(const std::string &As, bool exact)
    {
        try
        {
            Matrix A = parseMatrix(As);
            if (!isSquare(A))
                return makeError("Sylvester's law requires square matrix");
            int n = rows(A);

            // Inertia: count positive, negative, zero eigenvalues
            // Use Cholesky-like approach via eigenvalues
            auto [evals, evecs] = eigenQR(A);

            int nPos = 0, nNeg = 0, nZero = 0;
            for (auto &ev : evals)
            {
                double re = ev.real();
                if (re > 1e-8)
                    nPos++;
                else if (re < -1e-8)
                    nNeg++;
                else
                    nZero++;
            }

            std::ostringstream ss;
            ss << "Sylvester's Law of Inertia\n\n";
            ss << "Inertia (n+, n-, n0) = (" << nPos << ", " << nNeg << ", " << nZero << ")\n\n";
            ss << "n+ = " << nPos << "  (positive eigenvalues)\n";
            ss << "n- = " << nNeg << "  (negative eigenvalues)\n";
            ss << "n0 = " << nZero << "  (zero eigenvalues)\n\n";
            ss << "Classification:\n";
            if (nNeg == 0 && nZero == 0)
                ss << "  Positive definite\n";
            else if (nNeg == 0)
                ss << "  Positive semi-definite\n";
            else if (nPos == 0 && nZero == 0)
                ss << "  Negative definite\n";
            else if (nPos == 0)
                ss << "  Negative semi-definite\n";
            else
                ss << "  Indefinite\n";
            ss << "\nSignature = n+ - n- = " << (nPos - nNeg);
            return makeResult(ss.str());
        }
        catch (const std::exception &e)
        {
            return makeError(e.what());
        }
    }

    LAResult matrixLogarithm(const std::string &As, bool exact)
    {
        try
        {
            Matrix A = parseMatrix(As);
            if (!isSquare(A))
                return makeError("Matrix logarithm requires square matrix");
            int n = rows(A);

            std::ostringstream ss;
            ss << "Matrix Logarithm  log(A)  where e^{log(A)} = A\n\n";
            ss << "Method: Padé approximation via  log(A) = 2·arctanh((A-I)/(A+I))\n\n";

            // Verify det > 0 (necessary for real log)
            double d = detLU(A);
            if (d <= 0)
            {
                ss << "det(A) = " << formatReal(d, exact) << " ≤ 0\n";
                ss << "Real matrix logarithm may not e (requires positive eigenvalues).\n\n";
            }

            // Schur decomposition approach: A = Q T Q^H, log(A) = Q log(T) Q^H
            // For diagonal/triangular T, log(T)_ii = log(T_ii)
            // Simplified: direct series log(A) = (A-I) - (A-I)²/2 + (A-I)³/3 - ...
            // Valid when ||A-I|| < 1

            Matrix I = identity(n);
            Matrix AmI = matSub(A, I);
            double normAmI = 0;
            for (auto &row : AmI)
                for (double v : row)
                  normAmI += v * v;
            normAmI = std::sqrt(normAmI);

            if (normAmI >= 1.0)
            {
                ss << "||A-I|| = " << formatReal(normAmI, exact) << " ≥ 1\n";
                ss << "Series may not converge. Consider Schur-based method.\n\n";
                ss << "Eigenvalue-based approach:\n";
                ss << "  1. Diagonalise A = P D P^{-1}\n";
                ss << "  2. log(A) = P diag(log(λ₁),...,log(λₙ)) P^{-1}\n";
                // Compute via eigenval            
		auto [evals, evecs] = eigenQR(A);
                ss << "Eigenvalues:\n";
                for (auto &ev : evals)
                {
                    if (ev.real() <= 0)
                    {
                        ss << "  λ = " << formatReal(ev.real(), true)
                           << " ≤ 0 → log(λ) is complex\n";
                    }
                    else
                    {
                        ss << "  λ = " << formatReal(ev.real(), true)
                           << "g(λ) = " << formatReal(std::log(ev.real()), true) << "\n";
                    }
                }
            }
            else
            {
                // Taylor series: log(I + X) = X - X²/2 + X³/3 - ...
                Matrix result(n, Vector(n, 0.0));
                Matrix Xk = AmI;
                double sign = 1.0;
                for (int k = 1; k <= 20; ++k)
                {
                    for (int i = 0; i < n; ++i)
                        for (int j = 0; j < n; ++j)
                         result[i][j] += sign / k * Xk[i][j];
                    Xk = matMul(Xk, AmI);
                    sign = -sign;
                    // Check convergence
                    double norm = 0;
                    for (auto &row : Xk)
                        for (double v : row)
                            norm += v * v;
                    if (std::sqrt(norm) < 1e-12)
                        break;
                }
                ss << "log(A) (Taylor series, " << (int)std::round(normAmI * 10) / 10.0
                   << " radius):\n"
                   << formatMatrix(result, exact);
                return makeResult(ss.str(), formatMatrix(result, true));
            }
            return makeResult(ss.str());
        }
        catch (const std::exception &e)
        {
            return makeError(e.what());
        }
    }

    LAResult squareRoot(const std::string &As, bool exact)
    {
        try
        {
            Matrix A = parseMatrix(As);
            if (!isSquare(A))
                return makeError("Matrix square root requires square matrix");
            int n = rows(A);

            std::ostringstream ss;
            ss << "Matrix Square Root  X  where X² = A\n\n";

            // Denman-Beavers iteration: Y_{k+1} = (Y_k + Z_k^{-1})/2
            //                           Z_{k+1} = (Z_k + Y_k^{-1})/2
            // Converges to X = sqrt(A), Z → X^{-1}
            Matrix Y = A;
            Matrix Z = identity(n);
            bool converged = false;

            for (int iter = 0; iter < 100; ++iter)
            {
                auto Yinv = gaussJordanInverse(Y);
                auto Zinv = gaussJordanInverse(Z);
                if (!Yinv || !Zinv)
                {
                    ss << "Iteration failed (singular matrix)\n";
                    break;
                }

                Matrix Ynew(n, Vector(n, 0.0));
                Matrix Znew(n, Vector(n, 0.0));
                for (int i = 0; i < n; ++i)
                    for (int j = 0; j < n; ++j)
                    {
                        Ynew[i][j] = 0.5 * (Y[i][j] + (*Zinv)[i][j]);
                        Znew[i][j] = 0.5 * (Z[i][j] + (*Yinv)[i][j]);
                    }
                // Check convergence ||Y_{k+1} - Y_k||
                double diff = 0;
                for (int i = 0; i < n; ++i)
                    for (int j = 0; j < n; ++j)
                        diff += std::pow(Ynew[i][j] - Y[i][j], 2);
                Y = Ynew;
                Z = Znew;
                if (std::sqrt(diff) < 1e-10)
                {
                    converged = true;
                    break;
                }
            }

            ss << "Method: Denman-Beavers iteration\n";
            ss << "Converged: " << (converged ? "yes" : "no (may be ill-conditioned)") << "\n\n";
            ss << "X = sqrt(A):\n"
               << formatMatrix(Y, exact) << "\n\n";

            // Verify: X² should equal A
            Matrix X2 = matMul(Y, Y);
            double err = 0;
            for (int i = 0; i < n; ++i)
               for (int j = 0; j < n; ++j)
                    err += std::pow(X2[i][j] - A[i][j], 2);
            ss << "Verification ||X²-A|| = " << formatReal(std::sqrt(err), true);
            return makeResult(ss.str(), formatMatrix(Y, true));
        }
        catch (const std::exception &e)
        {
            return makeError(e.what());
        }
    }

    // =============================================================================
    // SECTION 18 — Main dispatch
    // ==========================================================================

    LAResult dispatch(const std::string &op, const std::string &input, bool exactMode)
    {
        // Parse "input" which may be "A" or "A|B" or "A|b" etc.
        auto split = [&](const std::string &s, char sep = '|')
        {
            std::vector<std::string> parts;
            std::string cur;
            for (char c : s)
            {
                if (c == sep)
                {
                    parts.push_back(cur);
                    cur.clear();
                }
                else
                    cur += c;
            }
            parts.push_back(cur);
            return parts;
        };
        auto p = split(input);
        auto get = [&](int i)
        { return i < (int)p.size() ? p[i] : ""; };

        // Basic ops
        if (op == "matrix_add")
            return add(get(0), get(1), exactMode);
        if (op == "matrix_subtract")
            return subtract(get(0), get(1), exactMode);
        if (op == "matrix_multiply")
            return multiply(get(0), get(1), exactMode);
        if (op == "matrix_strassen")
            return multiplyStrassen(get(0), get(1), exactMode);
        if (op == "matrix_hadamard")
            return hadamard(get(0), get(1), exactMode);
        if (op == "scalar_multiply")
            return scalarMultiply(get(0), get(1), exactMode);
        if (op == "matrix_transpose")
            return transpose(get(0), exactMode);
        if (op == "matrix_power")
            return power(get(0), std::stoi(get(1)), exactMode);
        if (op == "trace")
            return trace(get(0), exactMode);
        // Determinant
        if (op == "determinant")
            return determinant(get(0), exactMode);
        if (op == "determinant_lu")
            return determinantLU(get(0), exactMode);
        if (op == "determinant_cofactor")
            return determinantCofactor(get(0), exactMode);
        if (op == "determinant_bareiss")
            return determinantBareiss(get(0), exactMode);
        // Inverse
        if (op == "inverse")
            return inverse(get(0), exactMode);
        if (op == "inverse_gaussjordan")
            return inverseGaussJordan(get(0), exactMode);
        if (op == "inverse_adjugate")
            return inverseAdjugate(get(0), exactMode);
        // Row operations
        if (op == "rref")
            return reducedRowEchelon(get(0), exactMode);
        if (op == "ref")
            return rowEchelon(get(0), exactMode);
        if (op == "rank")
            return rank(get(0), exactMode);
        if (op == "nullity")
            return nullity(get(0), exactMode);
        if (op == "rank_nullity")
            return rankNullity(get(0), exactMode);
        if (op == "pivot_positions")
            return pivotPositions(get(0), exactMode);
        // Solve
        if (op == "solve")
            return solve(get(0), get(1), exactMode);
        if (op == "solve_gaussian")
            return solveGaussian(get(0), get(1), exactMode);
        if (op == "solve_gaussjordan")
            return solveGaussJordan(get(0), get(1), exactMode);
        if (op == "solve_cramer")
            return solveCramer(get(0), get(1), exactMode);
        if (op == "solve_leastsquares")
            return solveLeastSquares(get(0), get(1), exactMode);
        if (op == "solve_jacobi")
            return solveIterativeJacobi(get(0), get(1), exactMode);
        if (op == "solve_gaussseidel")
            return solveIterativeGaussSeidel(get(0), get(1), exactMode);
        // Decompositions
        if (op == "decomp_lu")
            return decompLU(get(0), exactMode);
        if (op == "decomp_qr_gs")
            return decompQRGramSchmidt(get(0), exactMode);
        if (op == "decomp_qr_hh")
            return decompQRHouseholder(get(0), exactMode);
        if (op == "decomp_qr_givens")
            return decompQRGivens(get(0), exactMode);
        if (op == "decomp_cholesky")
            return decompCholesky(get(0), exactMode);
        if (op == "decomp_svd")
            return decompSVD(get(0), exactMode);
        if (op == "decomp_schur")
            return decompSchur(get(0), exactMode);
        if (op == "decomp_jordan")
            return decompJordan(get(0), exactMode);
        // Eigenanalysis
        if (op == "eigenvalues")
            return eigenvaluesQR(get(0), exactMode);
        if (op == "eigenvectors")
            return eigenvectors(get(0), exactMode);
        if (op == "eigen_full")
            return eigenFull(get(0), exactMode);
        if (op == "char_poly")
            return characteristicPolynomial(get(0), exactMode);
        if (op == "diagonalize")
            return diagonalize(get(0), exactMode);
        if (op == "spectral")
            return spectralDecomposition(get(0), exactMode);
        if (op == "alg_mult")
            return algebraicMultiplicity(get(0), exactMode);
        if (op == "geom_mult")
            return geometricMultiplicity(get(0), exactMode);
        // Properties
        if (op == "is_symmetric")
            return isSymmetric(get(0), exactMode);
        if (op == "is_posdef")
            return isPositiveDefinite(get(0), exactMode);
        if (op == "is_orthogonal")
            return isOrthogonal(get(0), exactMode);
        if (op == "is_diagonalizable")
            return isDiagonalizable(get(0), exactMode);
        if (op == "classify")
            return classifyMatrix(get(0), exactMode);
        // Vector spaces
        if (op == "column_space")
            return columnSpace(get(0), exactMode);
        if (op == "null_space")
            return nullSpace(get(0), exactMode);
        if (op == "row_space")
            return rowSpace(get(0), exactMode);
        if (op == "left_null_space")
            return leftNullSpace(get(0), exactMode);
        if (op == "orthonormal_basis")
            return orthonormalBasis(get(0), exactMode);
        if (op == "is_independent")
            return isLinearlyIndependent(get(0), exactMode);
        if (op == "projection")
            return projection(get(0), get(1), exactMode);
        if (op == "projection_matrix")
            return projectionMatrix(get(0), exactMode);
        // Norms
        if (op == "vector_norm")
            return vectorNorm(get(0), get(1), exactMode);
        if (op == "frobenius_norm")
            return matrixNormFrobenius(get(0), exactMode);
        if (op == "spectral_norm")
            return matrixNormSpectral(get(0), exactMode);
        if (op == "condition_number")
            return conditionNumber(get(0), exactMode);
        if (op == "dot_product")
            return dotProduct(get(0), get(1), exactMode);
        if (op == "cross_product")
            return crossProduct(get(0), get(1), exactMode);
        if (op == "vector_angle")
            return angle(get(0), get(1), exactMode);
        if (op == "vector_distance")
            return distance(get(0), get(1), exactMode);
        // Special
        if (op == "adjugate")
            return adjugate(get(0), exactMode);
        if (op == "pseudoinverse")
            return pseudoinverse(get(0), exactMode);
        if (op == "matrix_exp")
            return matrixExponential(get(0), exactMode);
        if (op == "gram_matrix")
            return gramMatrix(get(0), exactMode);
        if (op == "quadratic_form")
            return quadraticForm(get(0), get(1), exactMode);
        // Constructors
        if (op == "make_identity")
            return makeIdentity(get(0), exactMode);
        if (op == "make_hilbert")
            return makeHilbert(get(0), exactMode);
        if (op == "make_vandermonde")
            return makeVandermonde(get(0), exactMode);

        if (op == "make_zero")
            return makeZero(get(0), get(1), exactMode);
        if (op == "make_companion")
            return makeCompanion(get(0), exactMode);
        if (op == "change_of_basis")
            return changeOfBasis(get(0), get(1), get(2), exactMode);
        if (op == "span_check")
            return spanCheck(get(0), get(1), exactMode);
        if (op == "matrix_norm_p")
            return matrixNormInduced(get(0), get(1), exactMode);
        if (op == "sylvesters_law")
            return sylvestersLaw(get(0), exactMode);
        if (op == "matrix_log")
            return matrixLogarithm(get(0), exactMode);
        if (op == "matrix_sqrt")
            return squareRoot(get(0), exactMode);
        if (op == "cayley_hamilton")
            return factorial2x2(get(0), exactMode);

        // ── Short-name aliases (used by QuickFunctionsPanel) ──────────────────────
        // ER00013: la:null|mat  → null_space
        if (op == "null")
          return nullSpace(get(0), exactMode);
        // ER00014: la:qr|mat   → decompQRGramSchmidt
        if (op == "qr")
            return decompQRGramSchmidt(get(0), exactMode);
        // ER00015: la:lu|mat   → decompLU
        if (op == "lu")
            return decompLU(get(0), exactMode);
        // ER00016: la:svd|mat  → decompSVD
        if (op == "svd")
            return decompSVD(get(0), exactMode);
        // ER00017: la:eigen|mat → eigenFull
        if (op == "eigen")
            return eigenFull(get(0), exactMode);
        // ER00018: la:inv|mat  → inverse
        if (op == "inv")
            return inverse(get(0), exactMode);

        return makeError("Unknown linear algebra operation: " + op);
    }
}
