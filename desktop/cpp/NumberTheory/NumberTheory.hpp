#pragma once
// NumberTheory.hpp — CoreEngine prefix: "nt:<op>|{json}"

#ifndef NUMBERTHEORY_HPP
#define NUMBERTHEORY_HPP

#include <string>
#include <vector>

namespace NumberTheory
{

    struct NTResult
    {
        bool ok = true;
        std::string value;
        std::string detail;
        std::string error;
    };

    using Vec = std::vector<long long>;

    // ── Primes ────────────────────────────────────────────────────────────────────
    NTResult isPrime(long long n);
    NTResult nextPrime(long long n);
    NTResult nthPrime(int n);
    NTResult primeFactors(long long n);
    NTResult primesUpTo(long long n);      // sieve of Eratosthenes
    NTResult primeCountingFn(long long n); // π(n)
    NTResult mobiusFunction(long long n);  // μ(n)

    // ── Divisibility ──────────────────────────────────────────────────────────────
    NTResult gcd(long long a, long long b);
    NTResult lcm(long long a, long long b);
    NTResult extendedGCD(long long a, long long b); // Bézout: ax+by=gcd
    NTResult divisors(long long n);
    NTResult numDivisors(long long n); // τ(n)
    NTResult sumDivisors(long long n); // σ(n)
    NTResult isPerfect(long long n);
    NTResult eulerPhi(long long n);         // φ(n)
    NTResult carmichaelLambda(long long n); // λ(n)
    NTResult liouvilleFn(long long n);      // λ(n) = (-1)^Ω(n)
    NTResult vonMangoldtFn(long long n);    // Λ(n)

    // ── Modular arithmetic ────────────────────────────────────────────────────────
    NTResult modPow(long long base, long long exp, long long mod);
    NTResult modInverse(long long a, long long mod);
    NTResult chineseRemainder(const Vec &remainders, const Vec &moduli);
    NTResult legendreSymbol(long long a, long long p);
    NTResult jacobiSymbol(long long a, long long n);
    NTResult quadraticResidue(long long a, long long p); // is a a QR mod p?
    NTResult primitiveRoot(long long p);
    NTResult discreteLog(long long base, long long target, long long mod); // baby-step giant-step

    // ── Congruences ───────────────────────────────────────────────────────────────
    NTResult solveLinearCongruence(long long a, long long b, long long m); // ax ≡ b (mod m)
    NTResult solveQuadraticCongruence(long long a, long long b, long long c, long long m);

    // ── Special functions ─────────────────────────────────────────────────────────
    NTResult fibonacci(int n);
    NTResult lucas(int n);
    NTResult catalanNumber(int n);
    NTResult partitionFn(int n); // p(n) via Euler's pentagonal theorem
    NTResult bellNumber(int n);
    NTResult stirling1st(int n, int k); // [n,k]
    NTResult stirling2nd(int n, int k); // {n,k}
    NTResult bernoulliNumber(int n);    // B_n (rational)

    // ── Diophantine equations ─────────────────────────────────────────────────────
    NTResult linearDiophantine(long long a, long long b, long long c); // ax+by=c
    NTResult pythagoreanTriples(long long limit);
    NTResult sumOfTwoSquares(long long n);
    NTResult sumOfFourSquares(long long n); // Lagrange's theorem

    // ── Cryptographic primitives ──────────────────────────────────────────────────
    NTResult millerRabin(long long n, int rounds = 20);
    NTResult pollardRho(long long n);                        // factorisation
    NTResult rsaKeygen(long long p, long long q);            // toy RSA
    NTResult elGamal(long long p, long long g, long long x); // parameters

    // ── Dispatch ──────────────────────────────────────────────────────────────────
    NTResult dispatch(const std::string &op, const std::string &json);

} // namespace NumberTheory

#endif