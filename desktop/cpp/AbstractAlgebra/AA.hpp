#pragma once
// AbstractAlgebra.h — CoreEngine prefix: "aa:<op>|{json}"

#ifndef AA_HPP
#define AA_HPP

#include <string>
#include <vector>

namespace AbstractAlgebra
{

    struct AAResult
    {
        bool ok = true;
        std::string value;
        std::string detail;
        std::string error;
    };

    using Vec = std::vector<long long>;
    using Mat = std::vector<Vec>;
    using Poly = std::vector<long long>; // coefficients: poly[i] = coefficient of x^i

    // ── Groups ────────────────────────────────────────────────────────────────────
    AAResult cyclic(int n);                 // Z_n: Cayley table, generators
    AAResult dihedralGroup(int n);          // D_n: symmetries of n-gon
    AAResult symmetricGroup(int n);         // S_n: order, alternating subgroup
    AAResult groupOrder(const Mat &cayley); // order of each element
    AAResult isGroup(const Mat &cayley);    // verify group axioms
    AAResult isAbelian(const Mat &cayley);
    AAResult subgroups(int n);                         // subgroups of Z_n
    AAResult quotientGroup(int n, int k);              // Z_n / Z_k
    AAResult directProduct(int m, int n);              // Z_m × Z_n
    AAResult groupHomomorphism(int m, int n, int phi); // Z_m → Z_n, phi(1)=phi
    AAResult conjugacyClasses(const Mat &cayley);
    AAResult centerOfGroup(const Mat &cayley);
    AAResult commutatorSubgroup(const Mat &cayley);
    AAResult sylow(int p, int groupOrder_); // Sylow p-subgroups
    AAResult lagrangeThm(int subgroupOrder, int groupOrder_);

    // ── Rings ─────────────────────────────────────────────────────────────────────
    AAResult ringZn(int n);                              // Z_n: units, zero divisors, ideals
    AAResult gaussianIntegers(long long a, long long b); // Z[i]: norm, units, associates
    AAResult idealGenerated(int n, int generator);       // (k) in Z_n
    AAResult quotientRing(int n, int ideal);             // Z_n / (ideal)
    AAResult chineseRemainderRing(const Vec &mods);      // Z_{m1} × ... by CRT
    AAResult isIntegralDomain(int n);
    AAResult isField(int n);

    // ── Fields ────────────────────────────────────────────────────────────────────
    AAResult galoisField(int p, int n);                  // GF(p^n): primitive element
    AAResult fieldExtension(int p, const Poly &minPoly); // GF(p)[x]/(minPoly)
    AAResult fieldCharacteristic(int p, int n);
    AAResult multiplicativeOrder(long long a, long long p); // order in GF(p)*
    AAResult primitiveElementGF(int p);                     // generator of GF(p)*

    // ── Polynomial rings ──────────────────────────────────────────────────────────
    AAResult polyAdd(const Poly &A, const Poly &B, long long mod = 0);
    AAResult polySub(const Poly &A, const Poly &B, long long mod = 0);
    AAResult polyMul(const Poly &A, const Poly &B, long long mod = 0);
    AAResult polyDiv(const Poly &A, const Poly &B, long long mod = 0);
    AAResult polyGCD(const Poly &A, const Poly &B, long long mod = 0);
    AAResult polyEval(const Poly &A, long long x, long long mod = 0);
    AAResult polyIrreducible(const Poly &A, long long p); // over GF(p)
    AAResult polyCyclotomic(int n);                       // nth cyclotomic polynomial
    AAResult polyFactor(const Poly &A, long long p);      // factor over GF(p)
    AAResult polyEuclid(const Poly &A, const Poly &B, long long p);

    // ── Permutations (S_n) ────────────────────────────────────────────────────────
    AAResult permCycleNotation(const Vec &perm); // write as cycles
    AAResult permOrder(const Vec &perm);         // lcm of cycle lengths
    AAResult permCompose(const Vec &p, const Vec &q);
    AAResult permInverse(const Vec &perm);
    AAResult permParity(const Vec &perm);               // even or odd
    AAResult permConjugate(const Vec &p, const Vec &q); // q p q^{-1}

    // ── Group actions ─────────────────────────────────────────────────────────────
    AAResult burnside(const std::vector<Vec> &orbits); // count colorings
    AAResult orbitStabiliser(const Mat &cayley, int element);

    // ── Dispatch ──────────────────────────────────────────────────────────────────
    AAResult dispatch(const std::string &op, const std::string &json);

} // namespace AbstractAlgebra
#endif