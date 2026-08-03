// NumberTheory.cpp

#include "NumberTheory.hpp"
#include "../CommonUtils.hpp"
#include "../MathCore.hpp"

// ── Helpers ───────────────────────────────────────────────────────────────────
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

// Integer-to-string helper used throughout (mirrors MathCore str())
template <typename T>
static std::string str(T v) { return std::to_string(v); }

// Portable 128-bit multiply-mod: (__int128) is GCC-only; fall back to
// unsigned long long on MSVC (safe for mod < 2^32).
#ifdef __GNUC__
static inline long long mulmod(long long a, long long b, long long m)
{
    return (long long)((__int128)a * b % m);
}
#else
static inline long long mulmod(long long a, long long b, long long m)
{
    // Works correctly when m <= 2^32 (covers all typical use-cases here).
    return (long long)((unsigned long long)(a % m) * (unsigned long long)(b % m) % (unsigned long long)m);
}
#endif

#include <cmath>
#include <sstream>
#include <ostream>
#include <istream>
#include <algorithm>
#include <numeric>
#include <map>
#include <set>
#include <random>
#include <stdexcept>

namespace NumberTheory
{

    static NTResult ok(const std::string &v, const std::string &d = "") { return {true, v, d, ""}; }
    static NTResult err(const std::string &m) { return {false, "", "", m}; }

    // ── Sieve of Eratosthenes ────────────────────────────────────────────────────
    static std::vector<bool> sieve(long long n)
    {
        std::vector<bool> is_prime(n + 1, true);
        is_prime[0] = is_prime[1] = false;
        for (long long i = 2; i * i <= n; ++i)
            if (is_prime[i])
                for (long long j = i * i; j <= n; j += i)
                    is_prime[j] = false;
        return is_prime;
    }

    static bool checkPrime(long long n)
    {
        if (n < 2)
            return false;
        if (n < 4)
            return true;
        if (n % 2 == 0 || n % 3 == 0)
            return false;
        for (long long i = 5; i * i <= n; i += 6)
            if (n % i == 0 || n % (i + 2) == 0)
                return false;
        return true;
    }

    NTResult isPrime(long long n)
    {
        bool p = checkPrime(n);
        return ok(p ? "true" : "false", str(n) + (p ? " is prime" : " is composite"));
    }

    NTResult nextPrime(long long n)
    {
        long long m = n + 1;
        while (!checkPrime(m))
            ++m;
        return ok(str(m));
    }

    NTResult nthPrime(int n)
    {
        if (n < 1)
            return err("n must be >= 1");
        int count = 0;
        long long m = 1;
        while (count < n)
        {
            ++m;
            if (checkPrime(m))
                ++count;
        }
        return ok(str(m), "p_" + str(n) + " = " + str(m));
    }

    NTResult primeFactors(long long n)
    {
        if (n < 1)
            return err("n must be >= 1");
        if (n == 1)
            return ok("1", "1 has no prime factors");
        std::map<long long, int> factors;
        long long m = n;
        for (long long p = 2; p * p <= m; ++p)
            while (m % p == 0)
            {
                factors[p]++;
                m /= p;
            }
        if (m > 1)
            factors[m]++;
        std::ostringstream sym, det;
        sym << n << " = ";
        bool first = true;
        for (auto &[p, e] : factors)
        {
            if (!first)
                sym << " x ";
            sym << p;
            if (e > 1)
                sym << "^" << e;
            det << p << "^" << e << " ";
            first = false;
        }
        return ok(sym.str(), det.str());
    }

    NTResult primesUpTo(long long n)
    {
        if (n < 2)
            return ok("none");
        auto sp = sieve(std::min(n, 100000LL));
        std::ostringstream ss;
        int count = 0;
        for (long long i = 2; i <= n && i < (long long)sp.size(); ++i)
            if (sp[i])
            {
                if (count++)
                    ss << ",";
                ss << i;
            }
        return ok(ss.str(), str(count) + " primes up to " + str(n));
    }

    NTResult primeCountingFn(long long n)
    {
        auto sp = sieve(std::min(n, 1000000LL));
        long long count = 0;
        for (long long i = 2; i <= n && i < (long long)sp.size(); ++i)
            if (sp[i])
                ++count;
        return ok(str(count), "pi(" + str(n) + ") = " + str(count));
    }

    NTResult mobiusFunction(long long n)
    {
        if (n < 1)
            return err("n must be >= 1");
        if (n == 1)
            return ok("1");
        long long m = n;
        int primeCount = 0;
        bool squared = false;
        for (long long p = 2; p * p <= m; ++p)
        {
            if (m % p == 0)
            {
                primeCount++;
                m /= p;
                if (m % p == 0)
                {
                    squared = true;
                    break;
                }
            }
        }
        if (m > 1)
            primeCount++;
        int mu = squared ? 0 : (primeCount % 2 == 0 ? 1 : -1);
        return ok(str(mu), "mu(" + str(n) + ") = " + str(mu));
    }

    // ── GCD/LCM ──────────────────────────────────────────────────────────────────

    NTResult gcd(long long a, long long b)
    {
        long long g = mcGcd(a, b);
        return ok(str(g), "gcd(" + str(a) + "," + str(b) + ") = " + str(g));
    }

    NTResult lcm(long long a, long long b)
    {
        long long g = mcGcd(a, b);
        long long l = std::abs(a / g * b);
        return ok(str(l), "lcm(" + str(a) + "," + str(b) + ") = " + str(l));
    }

    NTResult extendedGCD(long long a, long long b)
    {
        long long old_r = a, r = b, old_s = 1, s = 0, old_t = 0, t = 1;
        while (r)
        {
            long long q = old_r / r;
            long long tmp = r;
            r = old_r - q * r;
            old_r = tmp;
            tmp = s;
            s = old_s - q * s;
            old_s = tmp;
            tmp = t;
            t = old_t - q * t;
            old_t = tmp;
        }
        std::ostringstream ss;
        ss << "gcd(" << a << "," << b << ") = " << old_r << "\n";
        ss << "Bezout coefficients: x=" << old_s << ", y=" << old_t << "\n";
        ss << a << "x(" << old_s << ") + " << b << "x(" << old_t << ") = " << old_r;
        return ok(str(old_r), ss.str());
    }

    NTResult divisors(long long n)
    {
        if (n < 1)
            return err("n must be >= 1");
        std::vector<long long> d;
        for (long long i = 1; i * i <= n; ++i)
            if (n % i == 0)
            {
                d.push_back(i);
                if (i != n / i)
                    d.push_back(n / i);
            }
        std::sort(d.begin(), d.end());
        std::ostringstream ss;
        for (size_t i = 0; i < d.size(); ++i)
        {
            if (i)
                ss << ",";
            ss << d[i];
        }
        return ok(ss.str(), str((long long)d.size()) + " divisors");
    }

    NTResult numDivisors(long long n)
    {
        long long m = n, tau = 1;
        for (long long p = 2; p * p <= m; ++p)
        {
            int e = 0;
            while (m % p == 0)
            {
                e++;
                m /= p;
            }
            if (e)
                tau *= (e + 1);
        }
        if (m > 1)
            tau *= 2;
        return ok(str(tau), "tau(" + str(n) + ") = " + str(tau));
    }

    NTResult sumDivisors(long long n)
    {
        long long m = n, sigma = 1;
        for (long long p = 2; p * p <= m; ++p)
        {
            if (m % p == 0)
            {
                long long pk = 1, pterm = 1;
                while (m % p == 0)
                {
                    m /= p;
                    pk *= p;
                    pterm += pk;
                }
                sigma *= pterm;
            }
        }
        if (m > 1)
            sigma *= (1 + m);
        return ok(str(sigma), "sigma(" + str(n) + ") = " + str(sigma));
    }

    NTResult isPerfect(long long n)
    {
        auto s = sumDivisors(n);
        long long sigma = std::stoll(s.value);
        bool perfect = (sigma == 2 * n);
        return ok(perfect ? "true" : "false",
                  str(n) + (perfect ? " is perfect (sigma(n)=2n)" : " is not perfect") +
                      ", sigma(" + str(n) + ")=" + str(sigma));
    }

    NTResult eulerPhi(long long n)
    {
        if (n < 1)
            return err("n must be >= 1");
        long long result = n, m = n;
        for (long long p = 2; p * p <= m; ++p)
        {
            if (m % p == 0)
            {
                while (m % p == 0)
                    m /= p;
                result -= result / p;
            }
        }
        if (m > 1)
            result -= result / m;
        return ok(str(result), "phi(" + str(n) + ") = " + str(result));
    }

    NTResult carmichaelLambda(long long n)
    {
        if (n < 1)
            return err("n must be >= 1");
        long long m = n, lam = 1;
        for (long long p = 2; p * p <= m; ++p)
        {
            if (m % p == 0)
            {
                long long pk = 1;
                int k = 0;
                while (m % p == 0)
                {
                    m /= p;
                    pk *= p;
                    k++;
                }
                long long lamPk;
                if (p == 2 && k >= 3)
                    lamPk = pk / 4;
                else
                {
                    long long phi_pk = pk - pk / p;
                    lamPk = phi_pk;
                }
                lam = lam / mcGcd(lam, lamPk) * lamPk;
            }
        }
        if (m > 1)
        {
            long long lamP = m - 1;
            lam = lam / mcGcd(lam, lamP) * lamP;
        }
        return ok(str(lam), "lambda(" + str(n) + ") = " + str(lam));
    }

    NTResult liouvilleFn(long long n)
    {
        int Omega = 0;
        long long m = n;
        for (long long p = 2; p * p <= m; ++p)
            while (m % p == 0)
            {
                Omega++;
                m /= p;
            }
        if (m > 1)
            Omega++;
        int val = (Omega % 2 == 0) ? 1 : -1;
        return ok(str(val));
    }

    NTResult vonMangoldtFn(long long n)
    {
        long long m = n;
        long long p0 = 0;
        bool isPrimePower = true;
        for (long long p = 2; p * p <= m; ++p)
        {
            if (m % p == 0)
            {
                if (p0 && p0 != p)
                {
                    isPrimePower = false;
                    break;
                }
                p0 = p;
                while (m % p == 0)
                    m /= p;
            }
        }
        if (m > 1)
        {
            if (p0)
                isPrimePower = false;
            else
                p0 = m;
        }
        if (!isPrimePower || n == 1)
            return ok("0");
        return ok(std::to_string(std::log((double)p0)),
                  "Lambda(" + str(n) + ") = ln(" + str(p0) + ")");
    }

    // ── Modular arithmetic ────────────────────────────────────────────────────────

    NTResult modPow(long long base, long long exp_, long long mod)
    {
        if (mod == 1)
            return ok("0");
        long long result = 1;
        base %= mod;
        while (exp_ > 0)
        {
            if (exp_ % 2 == 1)
                result = mulmod(result, base, mod);
            exp_ /= 2;
            base = mulmod(base, base, mod);
        }
        return ok(str(result), str(base) + "^" + str(exp_) + " mod " + str(mod) + " = " + str(result));
    }

    NTResult modInverse(long long a, long long mod)
    {
        long long old_r = a, r = mod, old_s = 1, s = 0;
        while (r)
        {
            long long q = old_r / r, tmp = r;
            r = old_r - q * r;
            old_r = tmp;
            tmp = s;
            s = old_s - q * s;
            old_s = tmp;
        }
        if (old_r != 1)
            return err(str(a) + " has no inverse mod " + str(mod) + " (gcd!=1)");
        return ok(str((old_s % mod + mod) % mod));
    }

    NTResult chineseRemainder(const Vec &r, const Vec &m)
    {
        if (r.size() != m.size() || r.empty())
            return err("Mismatched input");
        long long M = 1;
        for (long long mi : m)
            M *= mi;
        long long x = 0;
        for (size_t i = 0; i < r.size(); ++i)
        {
            long long Mi = M / m[i];
            auto inv = modInverse(Mi, m[i]);
            if (!inv.ok)
                return err("Moduli not pairwise coprime");
            long long yi = std::stoll(inv.value);
            x = (x + mulmod(mulmod((long long)r[i], Mi, M), yi, M)) % M;
        }
        x = (x % M + M) % M;
        std::ostringstream ss;
        for (size_t i = 0; i < r.size(); ++i)
            ss << "x == " << r[i] << " (mod " << m[i] << ")\n";
        ss << "x == " << x << " (mod " << M << ")";
        return ok(str(x), ss.str());
    }

    NTResult legendreSymbol(long long a, long long p)
    {
        if (!checkPrime(p) || p == 2)
            return err("p must be an odd prime");
        long long val = std::stoll(modPow(a, (p - 1) / 2, p).value);
        int ls = (val == 0) ? 0 : (val == 1) ? 1
                                             : -1;
        if (val == p - 1)
            ls = -1;
        return ok(str(ls), "(" + str(a) + "/" + str(p) + ") = " + str(ls));
    }

    NTResult jacobiSymbol(long long a, long long n)
    {
        if (n <= 0 || n % 2 == 0)
            return err("n must be a positive odd integer");
        a %= n;
        if (a < 0)
            a += n;
        int result = 1;
        while (a != 0)
        {
            while (a % 2 == 0)
            {
                a /= 2;
                int r = n % 8;
                if (r == 3 || r == 5)
                    result = -result;
            }
            std::swap(a, n);
            if (a % 4 == 3 && n % 4 == 3)
                result = -result;
            a %= n;
        }
        return ok(str(n == 1 ? result : 0));
    }

    NTResult quadraticResidue(long long a, long long p)
    {
        auto ls = legendreSymbol(a, p);
        if (!ls.ok)
            return ls;
        bool isQR = ls.value == "1";
        return ok(isQR ? "true" : "false",
                  str(a) + (isQR ? " is" : " is not") + " a quadratic residue mod " + str(p));
    }

    NTResult primitiveRoot(long long p)
    {
        if (!checkPrime(p))
            return err("p must be prime");
        auto phi = eulerPhi(p);
        long long phiP = std::stoll(phi.value);
        auto pf = primeFactors(phiP);
        std::vector<long long> factors;
        long long m = phiP;
        for (long long q = 2; q * q <= m; ++q)
            if (m % q == 0)
            {
                factors.push_back(q);
                while (m % q == 0)
                    m /= q;
            }
        if (m > 1)
            factors.push_back(m);
        for (long long g = 2; g < p; ++g)
        {
            bool ok_ = true;
            for (long long q : factors)
            {
                long long pw = std::stoll(modPow(g, phiP / q, p).value);
                if (pw == 1)
                {
                    ok_ = false;
                    break;
                }
            }
            if (ok_)
                return ok(str(g), "Smallest primitive root mod " + str(p) + " is " + str(g));
        }
        return err("No primitive root found");
    }

    NTResult discreteLog(long long base, long long target, long long mod)
    {
        long long m = (long long)std::ceil(std::sqrt((double)mod));
        std::map<long long, long long> table;
        long long val = 1;
        for (long long j = 0; j < m; ++j)
        {
            table[val] = j;
            val = mulmod(val, base, mod);
        }
        long long factor = std::stoll(modPow(base, mod - 1 - m % (mod - 1), mod).value);
        val = target;
        for (long long i = 0; i < m; ++i)
        {
            if (table.count(val))
            {
                long long x = i * m + table[val];
                return ok(str(x), str(base) + "^" + str(x) + "==" + str(target) + " (mod " + str(mod) + ")");
            }
            val = mulmod(val, factor, mod);
        }
        return err("No solution found (may not exist)");
    }

    NTResult solveLinearCongruence(long long a, long long b, long long m)
    {
        long long g = mcGcd(a, m);
        if (b % g != 0)
            return err("No solution: gcd(" + str(a) + "," + str(m) + ")=" + str(g) + " does not divide " + str(b));
        a /= g;
        b /= g;
        long long m2 = m / g;
        auto inv = modInverse(a, m2);
        long long x0 = (mulmod(std::stoll(inv.value), b, m2) + m2) % m2;
        std::ostringstream ss;
        ss << a << "x == " << b << " (mod " << m << ")\n";
        ss << "Solutions: x == " << x0 << " + k*" << m2 << " for k=0,1,...," << g - 1 << "\n";
        ss << "i.e. x == ";
        for (int k = 0; k < g; ++k)
        {
            if (k)
                ss << ", ";
            ss << (x0 + k * m2) % m;
        }
        ss << " (mod " << m << ")";
        return ok(str(x0), ss.str());
    }

    NTResult solveQuadraticCongruence(long long a, long long b, long long c, long long m)
    {
        if (m > 10000)
            return err("m too large for brute-force; use special algorithms");
        std::vector<long long> sols;
        for (long long x = 0; x < m; ++x)
        {
            if ((mulmod(mulmod(a, x, m), x, m) + mulmod(b, x, m) + c) % m == 0)
                sols.push_back(x);
        }
        std::ostringstream ss;
        ss << a << "x^2 + " << b << "x + " << c << " == 0 (mod " << m << ")\n";
        if (sols.empty())
            ss << "No solutions\n";
        else
        {
            ss << "Solutions: x == ";
            for (size_t i = 0; i < sols.size(); ++i)
            {
                if (i)
                    ss << ",";
                ss << sols[i];
            }
            ss << " (mod " << m << ")";
        }
        return ok(sols.empty() ? "none" : str(sols[0]), ss.str());
    }

    // ── Special functions ─────────────────────────────────────────────────────────

    NTResult fibonacci(int n)
    {
        if (n < 0)
            return err("n must be >= 0");
        if (n == 0)
            return ok("0");
        if (n == 1)
            return ok("1");
        long long a = 0, b = 1;
        for (int i = 2; i <= n; ++i)
        {
            long long c = a + b;
            a = b;
            b = c;
        }
        return ok(str(b), "F_" + str(n) + " = " + str(b));
    }

    NTResult lucas(int n)
    {
        if (n < 0)
            return err("n must be >= 0");
        if (n == 0)
            return ok("2");
        if (n == 1)
            return ok("1");
        long long a = 2, b = 1;
        for (int i = 2; i <= n; ++i)
        {
            long long c = a + b;
            a = b;
            b = c;
        }
        return ok(str(b));
    }

    NTResult catalanNumber(int n)
    {
        if (n < 0)
            return err("n must be >= 0");
        long long cat = 1;
        for (int i = 0; i < n; ++i)
            cat = cat * 2 * (2 * i + 1) / (i + 2);
        return ok(str(cat), "C_" + str(n) + " = " + str(cat));
    }

    NTResult partitionFn(int n)
    {
        std::vector<long long> p(n + 1, 0);
        p[0] = 1;
        for (int m = 1; m <= n; ++m)
        {
            for (int j = 1;; ++j)
            {
                int g1 = j * (3 * j - 1) / 2, g2 = j * (3 * j + 1) / 2;
                if (g1 > m && g2 > m)
                    break;
                if (g1 <= m)
                    p[m] += (j % 2 == 1 ? 1 : -1) * p[m - g1];
                if (g2 <= m)
                    p[m] += (j % 2 == 1 ? 1 : -1) * p[m - g2];
            }
        }
        return ok(str(p[n]), "p(" + str(n) + ") = " + str(p[n]));
    }

    NTResult bellNumber(int n)
    {
        std::vector<std::vector<long long>> B(n + 1, std::vector<long long>(n + 1, 0));
        B[0][0] = 1;
        for (int i = 1; i <= n; ++i)
        {
            B[i][0] = B[i - 1][i - 1];
            for (int j = 1; j <= i; ++j)
                B[i][j] = B[i][j - 1] + B[i - 1][j - 1];
        }
        return ok(str(B[n][0]), "B_" + str(n) + " = " + str(B[n][0]));
    }

    NTResult stirling1st(int n, int k)
    {
        std::vector<std::vector<long long>> s(n + 1, std::vector<long long>(n + 1, 0));
        s[0][0] = 1;
        for (int i = 1; i <= n; ++i)
            for (int j = 1; j <= i; ++j)
                s[i][j] = (i - 1) * s[i - 1][j] + s[i - 1][j - 1];
        return ok(str(s[n][k]), "[" + str(n) + "," + str(k) + "] = " + str(s[n][k]));
    }

    NTResult stirling2nd(int n, int k)
    {
        std::vector<std::vector<long long>> S(n + 1, std::vector<long long>(k + 1, 0));
        S[0][0] = 1;
        for (int i = 1; i <= n; ++i)
            for (int j = 1; j <= std::min(i, k); ++j)
                S[i][j] = j * S[i - 1][j] + S[i - 1][j - 1];
        return ok(str(S[n][k]), "{" + str(n) + "," + str(k) + "} = " + str(S[n][k]));
    }

    NTResult bernoulliNumber(int n)
    {
        std::vector<double> a(n + 2);
        for (int i = 0; i <= n + 1; ++i)
            a[i] = 1.0 / (i + 1);
        for (int i = 1; i <= n + 1; ++i)
            for (int j = 0; j <= n - i + 1; ++j)
                a[j] = (j + 1) * (a[j] - a[j + 1]);
        std::ostringstream ss;
        ss << "B_" << n << " ~= " << a[0];
        if (n % 2 == 1 && n > 1)
            ss << " (= 0 for odd n > 1)";
        return ok(std::to_string(a[0]), ss.str());
    }

    // ── Diophantine ───────────────────────────────────────────────────────────────

    NTResult linearDiophantine(long long a, long long b, long long c)
    {
        long long g = mcGcd(a, b);
        if (c % g != 0)
            return err("No integer solutions: gcd(" + str(a) + "," + str(b) + ")=" + str(g) + " does not divide " + str(c));
        long long old_r = a, r = b, old_s = 1, s = 0;
        while (r)
        {
            long long q = old_r / r, tmp = r;
            r = old_r - q * r;
            old_r = tmp;
            tmp = s;
            s = old_s - q * s;
            old_s = tmp;
        }
        long long x0 = old_s * (c / g), y0 = (c - a * x0) / b;
        std::ostringstream out;
        out << a << "x + " << b << "y = " << c << "\n";
        out << "General solution:\n";
        out << "  x = " << x0 << " + " << (b / g) << "t\n";
        out << "  y = " << y0 << " - " << (a / g) << "t\n";
        out << "  (t any integer)";
        return ok(str(x0) + "," + str(y0), out.str());
    }

    NTResult pythagoreanTriples(long long limit)
    {
        std::ostringstream ss;
        ss << "Primitive Pythagorean triples (a^2+b^2=c^2, c<=" << limit << "):\n";
        int count = 0;
        for (long long m = 2; m * m <= limit; ++m)
            for (long long n = 1; n < m; ++n)
            {
                if ((m - n) % 2 == 0 || mcGcd(m, n) != 1)
                    continue;
                long long a = m * m - n * n, b = 2 * m * n, c = m * m + n * n;
                if (a > b)
                    std::swap(a, b);
                if (c > limit)
                    continue;
                ss << "  (" << a << "," << b << "," << c << ")\n";
                count++;
            }
        ss << count << " primitive triples found.";
        return ok(ss.str());
    }

    NTResult sumOfTwoSquares(long long n)
    {
        std::ostringstream ss;
        ss << n << " = a^2 + b^2:\n";
        bool found = false;
        for (long long a = 0; a * a <= n / 2; ++a)
        {
            long long rem = n - a * a;
            long long b = (long long)std::sqrt((double)rem);
            if (b * b == rem)
            {
                ss << "  " << a << "^2 + " << b << "^2 = " << n << "\n";
                found = true;
            }
        }
        if (!found)
            ss << "  No representation as sum of two squares\n";
        return ok(ss.str());
    }

    NTResult sumOfFourSquares(long long n)
    {
        for (long long a = 0; a * a <= n; ++a)
            for (long long b = a; a * a + b * b <= n; ++b)
                for (long long c = b; a * a + b * b + c * c <= n; ++c)
                {
                    long long rem = n - a * a - b * b - c * c;
                    long long d = (long long)std::sqrt((double)rem);
                    if (d * d == rem && d >= c)
                    {
                        std::ostringstream ss;
                        ss << n << " = " << a << "^2+" << b << "^2+" << c << "^2+" << d << "^2";
                        return ok(ss.str());
                    }
                }
        return err("Not found (should not happen by Lagrange's theorem)");
    }

    // ── Miller-Rabin / Pollard ────────────────────────────────────────────────────

    NTResult millerRabin(long long n, int rounds)
    {
        if (n < 2)
            return ok("false");
        if (n == 2 || n == 3)
            return ok("true");
        if (n % 2 == 0)
            return ok("false");
        long long d = n - 1;
        int r = 0;
        while (d % 2 == 0)
        {
            d /= 2;
            r++;
        }
        std::mt19937_64 rng(42);
        std::uniform_int_distribution<long long> dist(2, n - 2);
        for (int i = 0; i < rounds; ++i)
        {
            long long a = dist(rng);
            long long x = std::stoll(modPow(a, d, n).value);
            if (x == 1 || x == n - 1)
                continue;
            bool composite = true;
            for (int j = 0; j < r - 1; ++j)
            {
                x = mulmod(x, x, n);
                if (x == n - 1)
                {
                    composite = false;
                    break;
                }
            }
            if (composite)
                return ok("false", "Miller-Rabin: " + str(n) + " is composite");
        }
        return ok("true", "Miller-Rabin: " + str(n) + " is probably prime (" + str(rounds) + " rounds)");
    }

    NTResult pollardRho(long long n)
    {
        if (n % 2 == 0)
            return ok(str(2LL), "2 x " + str(n / 2));
        auto f = [&](long long x) -> long long
        { return (mulmod(x, x, n) + 1) % n; };
        long long x = 2, y = 2, d = 1;
        while (d == 1)
        {
            x = f(x);
            y = f(f(y));
            d = mcGcd(std::abs(x - y), n);
        }
        if (d == n)
            return err("Failed to factor (try different parameters)");
        return ok(str(d), str(n) + " = " + str(d) + " x " + str(n / d));
    }

    NTResult rsaKeygen(long long p, long long q)
    {
        if (!checkPrime(p) || !checkPrime(q))
            return err("p and q must both be prime");
        long long n = p * q, phi = (p - 1) * (q - 1);
        long long e = 65537;
        if (mcGcd(e, phi) != 1)
        {
            e = 3;
            while (mcGcd(e, phi) != 1)
                e += 2;
        }
        auto inv = modInverse(e, phi);
        long long d = std::stoll(inv.value);
        std::ostringstream ss;
        ss << "RSA Key Generation (toy example)\n";
        ss << "p=" << p << ", q=" << q << "\n";
        ss << "n = p*q = " << n << "\n";
        ss << "phi(n) = (p-1)(q-1) = " << phi << "\n";
        ss << "Public key: (e, n) = (" << e << ", " << n << ")\n";
        ss << "Private key: (d, n) = (" << d << ", " << n << ")\n\n";
        ss << "Encrypt: c = m^e mod n\nDecrypt: m = c^d mod n";
        return ok(ss.str());
    }

    NTResult elGamal(long long p, long long g, long long x)
    {
        long long y = std::stoll(modPow(g, x, p).value);
        std::ostringstream ss;
        ss << "ElGamal Parameters\np=" << p << " (prime), g=" << g << " (generator), x=" << x << " (private)\n";
        ss << "Public key: y = g^x mod p = " << y << "\n\n";
        ss << "Encrypt(m, k): (c1,c2) = (g^k mod p, m*y^k mod p)\n";
        ss << "Decrypt(c1,c2): m = c2 * c1^{p-1-x} mod p";
        return ok(str(y), ss.str());
    }

    // ── Dispatch ──────────────────────────────────────────────────────────────────

    static long long getLL(const std::string &j, const std::string &k, long long d = 0)
    {
        auto v = getP(j, k);
        if (v.empty())
            return d;
        try
        {
            return std::stoll(v);
        }
        catch (...)
        {
            return d;
        }
    }

    NTResult dispatch(const std::string &op, const std::string &json)
    {
        try
        {
            if (op == "is_prime")
                return isPrime(getLL(json, "n"));
            if (op == "next_prime")
                return nextPrime(getLL(json, "n"));
            if (op == "nth_prime")
                return nthPrime((int)getLL(json, "n", 1));
            if (op == "prime_factors")
                return primeFactors(getLL(json, "n"));
            if (op == "primes_upto")
                return primesUpTo(getLL(json, "n", 100));
            if (op == "prime_pi")
                return primeCountingFn(getLL(json, "n"));
            if (op == "mobius")
                return mobiusFunction(getLL(json, "n"));
            if (op == "gcd")
                return gcd(getLL(json, "a"), getLL(json, "b"));
            if (op == "lcm")
                return lcm(getLL(json, "a"), getLL(json, "b"));
            if (op == "extended_gcd")
                return extendedGCD(getLL(json, "a"), getLL(json, "b"));
            if (op == "divisors")
                return divisors(getLL(json, "n"));
            if (op == "num_divisors")
                return numDivisors(getLL(json, "n"));
            if (op == "sum_divisors")
                return sumDivisors(getLL(json, "n"));
            if (op == "is_perfect")
                return isPerfect(getLL(json, "n"));
            if (op == "euler_phi")
                return eulerPhi(getLL(json, "n"));
            if (op == "carmichael")
                return carmichaelLambda(getLL(json, "n"));
            if (op == "liouville")
                return liouvilleFn(getLL(json, "n"));
            if (op == "von_mangoldt")
                return vonMangoldtFn(getLL(json, "n"));
            if (op == "mod_pow")
                return modPow(getLL(json, "base"), getLL(json, "exp"), getLL(json, "mod"));
            if (op == "mod_inverse")
                return modInverse(getLL(json, "a"), getLL(json, "mod"));
            if (op == "legendre")
                return legendreSymbol(getLL(json, "a"), getLL(json, "p"));
            if (op == "jacobi")
                return jacobiSymbol(getLL(json, "a"), getLL(json, "n"));
            if (op == "quad_residue")
                return quadraticResidue(getLL(json, "a"), getLL(json, "p"));
            if (op == "primitive_root")
                return primitiveRoot(getLL(json, "p"));
            if (op == "discrete_log")
                return discreteLog(getLL(json, "base"), getLL(json, "target"), getLL(json, "mod"));
            if (op == "linear_cong")
                return solveLinearCongruence(getLL(json, "a"), getLL(json, "b"), getLL(json, "m"));
            if (op == "quad_cong")
                return solveQuadraticCongruence(getLL(json, "a"), getLL(json, "b"), getLL(json, "c"), getLL(json, "m"));
            if (op == "fibonacci")
                return fibonacci((int)getLL(json, "n"));
            if (op == "lucas")
                return lucas((int)getLL(json, "n"));
            if (op == "catalan")
                return catalanNumber((int)getLL(json, "n"));
            if (op == "partition")
                return partitionFn((int)getLL(json, "n"));
            if (op == "bell")
                return bellNumber((int)getLL(json, "n"));
            if (op == "stirling1")
                return stirling1st((int)getLL(json, "n"), (int)getLL(json, "k", 1));
            if (op == "stirling2")
                return stirling2nd((int)getLL(json, "n"), (int)getLL(json, "k", 1));
            if (op == "bernoulli")
                return bernoulliNumber((int)getLL(json, "n"));
            if (op == "linear_dioph")
                return linearDiophantine(getLL(json, "a"), getLL(json, "b"), getLL(json, "c"));
            if (op == "pyth_triples")
                return pythagoreanTriples(getLL(json, "limit", 100));
            if (op == "sum_two_sq")
                return sumOfTwoSquares(getLL(json, "n"));
            if (op == "sum_four_sq")
                return sumOfFourSquares(getLL(json, "n"));
            if (op == "miller_rabin")
                return millerRabin(getLL(json, "n"), (int)getLL(json, "rounds", 20));
            if (op == "pollard_rho")
                return pollardRho(getLL(json, "n"));
            if (op == "rsa_keygen")
                return rsaKeygen(getLL(json, "p"), getLL(json, "q"));
            if (op == "el_gamal")
                return elGamal(getLL(json, "p"), getLL(json, "g"), getLL(json, "x"));
            if (op == "crt")
            {
                Vec rv;
                Vec mv;
                return chineseRemainder(rv, mv);
            }

            return err("Unknown number theory operation: " + op);
        }
        catch (const std::exception &e)
        {
            return err(e.what());
        }
    }

} // namespace NumberTheory