#pragma once
// =============================================================================
// LinearAlgebra.h
// Week 1 — Linear Algebra Module
//
// All computation lives in C++. CoreEngine.cpp routes here based on operation
// type. Results are returned as std::string for JNI transport; the format is:
//
//   Symbolic mode  (exactMode = false):  exact fractions, sqrt(), pi where possible
//   Numerical mode (exactMode = true):   floating-point, 10 significant figures
//
// Matrix input format (used throughout):
//   "[[a,b,c],[d,e,f],[g,h,i]]"   — row-major, comma-separated rows
//
// Vector input format:
//   "[a,b,c]"
// =============================================================================

#ifndef LINEARALGEBRA_HPP
#define LINEARALGEBRA_HPP

#include <string>
#include <vector>
#include <complex>

namespace LinearAlgebra
{

    // ── Internal types ────────────────────────────────────────────────────────────

    using Real = double;
    using Complex = std::complex<double>;
    using Matrix = std::vector<std::vector<Real>>;
    using CMatrix = std::vector<std::vector<Complex>>;
    using Vector = std::vector<Real>;
    using CVector = std::vector<Complex>;

    // ── Result struct returned by every operation ─────────────────────────────────

    struct LAResult
    {
        bool ok = true;
        std::string symbolic;  // exact / simplified form
        std::string numerical; // floating-point form
        std::string error;     // non-empty on failure

        // Convenience: format for JNI transport
        // Returns "symbolic  ~  numerical" or just one side, or "ERROR: ..."
        std::string format(bool exactMode) const;
    };

    // =============================================================================
    // SECTION 1 — Parsing and formatting
    // =============================================================================

    // Parse "[[1,2],[3,4]]" → Matrix
    Matrix parseMatrix(const std::string &s);

    // Parse "[1,2,3]" → Vector
    Vector parseVector(const std::string &s);

    // Format Matrix → "[[1,2],[3,4]]"
    std::string formatMatrix(const Matrix &M, bool exact);

    // Format Vector → "[1,2,3]"
    std::string formatVector(const Vector &v, bool exact);

    // Format a single real number (exact fraction if possible, else decimal)
    std::string formatReal(Real x, bool exact);

    // Format a complex number
    std::string formatComplex(Complex z, bool exact);

    // =============================================================================
    // SECTION 2 — Basic matrix operations
    // =============================================================================

    LAResult add(const std::string &A, const std::string &B, bool exact);
    LAResult subtract(const std::string &A, const std::string &B, bool exact);
    LAResult scalarMultiply(const std::string &A, const std::string &scalar, bool exact);
    LAResult multiply(const std::string &A, const std::string &B, bool exact);
    LAResult multiplyStrassen(const std::string &A, const std::string &B, bool exact);
    LAResult hadamard(const std::string &A, const std::string &B, bool exact);
    LAResult transpose(const std::string &A, bool exact);
    LAResult conjugateTranspose(const std::string &A, bool exact);
    LAResult power(const std::string &A, int n, bool exact);
    LAResult trace(const std::string &A, bool exact);

    // =============================================================================
    // SECTION 3 — Determinant (every method)
    // =============================================================================

    LAResult determinantCofactor(const std::string &A, bool exact); // cofactor/Laplace expansion
    LAResult determinantLU(const std::string &A, bool exact);       // via LU decomposition
    LAResult determinantBareiss(const std::string &A, bool exact);  // exact integer arithmetic
    LAResult determinant(const std::string &A, bool exact);         // best method auto-selected

    // =============================================================================
    // SECTION 4 — Inverse
    // =============================================================================

    LAResult inverseGaussJordan(const std::string &A, bool exact); // augmented [A|I] row reduction
    LAResult inverseAdjugate(const std::string &A, bool exact);    // (1/det) * adj(A)
    LAResult inverse(const std::string &A, bool exact);            // auto-selected

    // =============================================================================
    // SECTION 5 — Rank, nullity, and row operations
    // =============================================================================

    LAResult rank(const std::string &A, bool exact);
    LAResult nullity(const std::string &A, bool exact);
    LAResult rankNullity(const std::string &A, bool exact);       // full theorem statement
    LAResult rowEchelon(const std::string &A, bool exact);        // REF
    LAResult reducedRowEchelon(const std::string &A, bool exact); // RREF
    LAResult pivotPositions(const std::string &A, bool exact);

    // =============================================================================
    // SECTION 6 — Solving linear systems  Ax = b
    // =============================================================================

    // A is an augmented matrix [A|b] or pass A and b separately
    LAResult solveGaussian(const std::string &A, const std::string &b, bool exact);
    LAResult solveGaussJordan(const std::string &A, const std::string &b, bool exact);
    LAResult solveCramer(const std::string &A, const std::string &b, bool exact);
    LAResult solveLeastSquares(const std::string &A, const std::string &b, bool exact);
    LAResult solveIterativeJacobi(const std::string &A, const std::string &b, bool exact);
    LAResult solveIterativeGaussSeidel(const std::string &A, const std::string &b, bool exact);
    LAResult solve(const std::string &A, const std::string &b, bool exact); // auto

    // =============================================================================
    // SECTION 7 — Matrix decompositions
    // =============================================================================

    struct LUResult
    {
        Matrix L, U, P; // PA = LU
        std::vector<int> pivots;
        int permutationSign; // +1 or -1
    };

    struct QRResult
    {
        Matrix Q, R;
    };

    struct SVDResult
    {
        Matrix U;  // left singular vectors (m x m)
        Vector S;  // singular values (descending)
        Matrix Vt; // V^T  (n x n)
    };

    struct EigenResult
    {
        CVector values;
        CMatrix vectors; // columns are eigenvectors
    };

    struct CholeskyResult
    {
        Matrix L; // A = L * L^T
    };

    struct SchurResult
    {
        CMatrix Q, T; // A = Q * T * Q^H,  T quasi-upper-triangular
    };

    // Decompositions — each returns formatted string result via LAResult
    LAResult decompLU(const std::string &A, bool exact);            // LUP decomposition
    LAResult decompQRGramSchmidt(const std::string &A, bool exact); // classical Gram-Schmidt
    LAResult decompQRHouseholder(const std::string &A, bool exact); // numerically stable
    LAResult decompQRGivens(const std::string &A, bool exact);      // rotation-based
    LAResult decompCholesky(const std::string &A, bool exact);      // symmetric positive definite
    LAResult decompSVD(const std::string &A, bool exact);           // full SVD
    LAResult decompSchur(const std::string &A, bool exact);         // Schur decomposition
    LAResult decompJordan(const std::string &A, bool exact);        // Jordan normal form

    // =============================================================================
    // SECTION 8 — Eigenvalues and eigenvectors
    // =============================================================================

    LAResult characteristicPolynomial(const std::string &A, bool exact);
    LAResult eigenvaluesPowerIteration(const std::string &A, bool exact);
    LAResult eigenvaluesQR(const std::string &A, bool exact);
    LAResult eigenvectors(const std::string &A, bool exact);
    LAResult eigenFull(const std::string &A, bool exact); // values+vectors+multiplicities
    LAResult algebraicMultiplicity(const std::string &A, bool exact);
    LAResult geometricMultiplicity(const std::string &A, bool exact);
    LAResult spectralDecomposition(const std::string &A, bool exact); // A = Q*D*Q^-1
    LAResult diagonalize(const std::string &A, bool exact);           // P, D such that A=PDP^-1
    LAResult isSymmetric(const std::string &A, bool exact);
    LAResult isPositiveDefinite(const std::string &A, bool exact);
    LAResult isOrthogonal(const std::string &A, bool exact);
    LAResult isDiagonalizable(const std::string &A, bool exact);

    // =============================================================================
    // SECTION 9 — Vector space operations
    // =============================================================================

    LAResult columnSpace(const std::string &A, bool exact); // basis vectors
    LAResult nullSpace(const std::string &A, bool exact);   // basis for ker(A)
    LAResult rowSpace(const std::string &A, bool exact);
    LAResult leftNullSpace(const std::string &A, bool exact);          // ker(A^T)
    LAResult orthonormalBasis(const std::string &vectors, bool exact); // Gram-Schmidt
    LAResult isLinearlyIndependent(const std::string &vectors, bool exact);
    LAResult spanCheck(const std::string &vectors,
                       const std::string &target, bool exact); // is target in span?
    LAResult changeOfBasis(const std::string &v,
                           const std::string &fromBasis,
                           const std::string &toBasis, bool exact);
    LAResult projection(const std::string &v,
                        const std::string &onto, bool exact);    // proj_u(v)
    LAResult projectionMatrix(const std::string &A, bool exact); // P = A(A^TA)^-1 A^T

    // =============================================================================
    // SECTION 10 — Norms and distances
    // =============================================================================

    LAResult vectorNorm(const std::string &v,
                        const std::string &p, bool exact); // p="1","2","inf"
    LAResult matrixNormFrobenius(const std::string &A, bool exact);
    LAResult matrixNormSpectral(const std::string &A, bool exact); // largest singular value
    LAResult matrixNormInduced(const std::string &A,
                               const std::string &p, bool exact);
    LAResult conditionNumber(const std::string &A, bool exact); // ||A|| * ||A^-1||
    LAResult dotProduct(const std::string &u,
                        const std::string &v, bool exact);
    LAResult crossProduct(const std::string &u,
                          const std::string &v, bool exact); // 3D only
    LAResult angle(const std::string &u,
                   const std::string &v, bool exact); // angle between vectors
    LAResult distance(const std::string &u,
                      const std::string &v, bool exact); // Euclidean distance

    // =============================================================================
    // SECTION 11 — Special matrix properties and constructors
    // =============================================================================

    LAResult adjugate(const std::string &A, bool exact);
    LAResult pseudoinverse(const std::string &A, bool exact);     // Moore-Penrose via SVD
    LAResult matrixExponential(const std::string &A, bool exact); // e^A via Pade / Jordan
    LAResult matrixLogarithm(const std::string &A, bool exact);   // log(A) for invertible A
    LAResult squareRoot(const std::string &A, bool exact);        // X s.t. X^2 = A

    // Detect type
    LAResult classifyMatrix(const std::string &A, bool exact); // full property report

    // Construct special matrices (n as string e.g. "3")
    LAResult makeIdentity(const std::string &n, bool exact);
    LAResult makeZero(const std::string &rows,
                      const std::string &cols, bool exact);
    LAResult makeHilbert(const std::string &n, bool exact); // H_ij = 1/(i+j-1)
    LAResult makeVandermonde(const std::string &v, bool exact);
    LAResult makeCompanion(const std::string &poly, bool exact); // companion matrix of polynomial

    // =============================================================================
    // SECTION 12 — Quadratic forms and inner products
    // =============================================================================

    LAResult quadraticForm(const std::string &A,
                           const std::string &x, bool exact);    // x^T A x
    LAResult sylvestersLaw(const std::string &A, bool exact);    // inertia (n+, n-, n0)
    LAResult gramMatrix(const std::string &vectors, bool exact); // G_ij = <v_i, v_j>

    // =============================================================================
    // SECTION 13 — Main dispatch (called by CoreEngine)
    // =============================================================================

    // operation examples:
    //   "matrix_add", "matrix_multiply", "determinant", "eigenvalues",
    //   "decomp_svd", "solve_gaussian", "null_space", "vector_norm_2", ...
    LAResult dispatch(const std::string &operation,
                      const std::string &input,
                      bool exactMode);

} // namespace LinearAlgebra

#endif // LINEARALGEBRA_H