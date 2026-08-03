// DiscreteMath.cpp

#include "DM.hpp"
#include "../CommonUtils.hpp"

// ── CommonUtils aliases ────────────────────────────────────────────────────────
static auto &getP = cu_getStr;
static auto getN = [](const std::string &j, const std::string &k, double d = 0.0)
{ return cu_getNum(j, k, d); };
static auto getInt_ = [](const std::string &j, const std::string &k, long long d = 0)
{ return (long long)cu_getNum(j, k, (double)d); };
static auto parseVec = [](const std::string &s)
{ return cu_parseVecD(s); };
static auto parseMat = [](const std::string &s)
{ return cu_parseMat(s); };
static auto fmt = [](double v, int p = 8)
{ return cu_fmt(v, p); };
// str(): convert numeric types to string — used throughout this file
static auto str = [](auto v)
{ return std::to_string(v); };
static auto &str_ll = cu_str;
static auto parseVecD = parseVec; // alias used in dispatch section

#include <cmath>
#include <sstream>
#include <algorithm>
#include <numeric>
#include <queue>
#include <stack>
#include <set>
#include <map>
#include <tuple>
#include <functional>
#include <stdexcept>
#include <limits>

namespace DiscreteMath
{

    static DMResult ok(const std::string &v, const std::string &d = "") { return {true, v, d, ""}; }
    static DMResult err(const std::string &m) { return {false, "", "", m}; }
    static const long long INF = std::numeric_limits<long long>::max() / 2;

    static double logFactorial(int n)
    {
        double s = 0;
        for (int i = 2; i <= n; ++i)
            s += std::log(i);
        return s;
    }
    static long long C(int n, int r)
    {
        if (r < 0 || r > n)
            return 0;
        if (r == 0 || r == n)
            return 1;
        double lc = logFactorial(n) - logFactorial(r) - logFactorial(n - r);
        return (long long)std::round(std::exp(lc));
    }

    // =============================================================================
    // COMBINATORICS
    // =============================================================================

    DMResult combinations(int n, int r)
    {
        if (r < 0 || r > n)
            return ok("0");
        long long c = C(n, r);
        std::ostringstream ss;
        ss << "C(" << n << "," << r << ") = " << n << "!/(" << r << "!(" << n - r << ")!) = " << c;
        return ok(str(c), ss.str());
    }

    DMResult permutations(int n, int r)
    {
        if (r < 0 || r > n)
            return ok("0");
        long long p = 1;
        for (int i = n; i > n - r; --i)
            p *= i;
        return ok(str(p), "P(" + str(n) + "," + str(r) + ") = " + str(p));
    }

    DMResult multisetCoeff(int n, int r)
    {
        long long c = C(n + r - 1, r);
        return ok(str(c), "⟨⟨" + str(n) + "," + str(r) + "⟩⟩ = C(" + str(n + r - 1) + "," + str(r) + ") = " + str(c));
    }

    DMResult derangements(int n)
    {
        if (n == 0)
            return ok("1");
        if (n == 1)
            return ok("0");
        long long d = 1, prev = 0, cur = 1;
        for (int i = 2; i <= n; ++i)
        {
            d = (long long)(i - 1) * (prev + cur);
            prev = cur;
            cur = d;
        }
        double prob = 1.0 / std::exp(1.0);
        return ok(str(d), "D(" + str(n) + ")=" + str(d) + ", approx n!/e ≈ " + std::to_string(prob * C(n, 0) * 1));
    }

    DMResult inclusionExclusion(int n, const Vec &sizes)
    {
        // |A1 ∪ A2 ∪ ...| = Σ|Ai| - Σ|Ai∩Aj| + ...
        // Given: sizes[0]=|A1|, sizes[1]=|A2|,..., sizes[k]=|each pair|, etc.
        // Simplified: just explain the principle with given values
        long long total = 0;
        int k = sizes.size();
        std::ostringstream ss;
        ss << "Inclusion-Exclusion Principle\n|A₁∪...∪Aₖ| = Σ|Aᵢ| - Σ|Aᵢ∩Aⱼ| + ...\n\n";
        for (int i = 0; i < k; ++i)
        {
            ss << "|A" << i + 1 << "|=" << sizes[i] << "\n";
            total += sizes[i];
        }
        ss << "\nSum of individual sets: " << total;
        ss << "\n(Subtract intersections to get exact count)";
        return ok(str(total), ss.str());
    }

    DMResult pigeonhole(int items, int holes)
    {
        if (holes <= 0)
            return err("Holes must be > 0");
        int guarantee = (items - 1) / holes + 1;
        std::ostringstream ss;
        ss << "Pigeonhole Principle\n"
           << items << " items, " << holes << " holes\n";
        ss << "At least one hole contains ≥ ⌈" << items << "/" << holes << "⌉ = " << guarantee << " items";
        return ok(str(guarantee), ss.str());
    }

    DMResult ramseyBound(int s, int t)
    {
        // R(s,t) bounds: R(s,t) ≤ C(s+t-2,s-1)
        long long upper = C(s + t - 2, s - 1);
        std::ostringstream ss;
        ss << "Ramsey Number R(" << s << "," << t << ")\n";
        ss << "Upper bound: R(" << s << "," << t << ") ≤ C(" << s + t - 2 << "," << s - 1 << ") = " << upper << "\n";
        // Known values
        std::map<std::pair<int, int>, int> known = {
            {{1, 1}, 1}, {{2, 2}, 2}, {{3, 3}, 6}, {{4, 4}, 18}, {{2, 3}, 3}, {{3, 2}, 3}, {{2, 4}, 4}, {{4, 2}, 4}, {{3, 4}, 9}, {{4, 3}, 9}, {{3, 5}, 14}, {{5, 3}, 14}};
        auto key = std::make_pair(s, t);
        if (known.count(key))
            ss << "Exact value: R(" << s << "," << t << ") = " << known[key];
        return ok(str(upper), ss.str());
    }

    DMResult necklaceCount(int n, int k)
    {
        // Burnside's lemma: (1/n) Σ k^gcd(n,d) over d|n
        long long total = 0;
        for (int d = 1; d <= n; ++d)
        {
            if (n % d != 0)
                continue;
            // Euler phi of n/d cycles each with k^d colorings
            long long pow_k = 1;
            for (int i = 0; i < d; ++i)
                pow_k *= k;
            // phi(n/d) necklaces from d-cycle
            int q = n / d;
            long long phi_q = q;
            for (int p = 2; p * p <= q; ++p)
                if (q % p == 0)
                {
                    while (q % p == 0)
                        q /= p;
                    phi_q -= phi_q / p;
                }
            if (q > 1)
                phi_q -= phi_q / q;
            total += phi_q * pow_k;
        }
        total /= n;
        return ok(str(total), "Necklaces: " + str(total) + " (Burnside/Polya, " + str(n) + " beads, " + str(k) + " colours)");
    }

    // =============================================================================
    // GENERATING FUNCTIONS
    // =============================================================================

    DMResult ordinaryGF(const Vec &coeffs, int terms)
    {
        std::ostringstream ss;
        ss << "Ordinary Generating Function\nf(x) = ";
        for (int i = 0; i < (int)coeffs.size() && i < terms; ++i)
        {
            if (i)
                ss << (coeffs[i] >= 0 ? " + " : " - ");
            ss << std::abs(coeffs[i]);
            if (i == 1)
                ss << "x";
            else if (i > 1)
                ss << "x^" << i;
        }
        if ((int)coeffs.size() > terms)
            ss << " + ...";
        ss << "\n\nCoefficients: [";
        for (int i = 0; i < (int)coeffs.size(); ++i)
        {
            if (i)
                ss << ",";
            ss << coeffs[i];
        }
        ss << "]";
        return ok(ss.str());
    }

    DMResult exponentialGF(const Vec &coeffs, int terms)
    {
        std::ostringstream ss;
        ss << "Exponential Generating Function\nf(x) = ";
        double fac = 1;
        for (int i = 0; i < (int)coeffs.size() && i < terms; ++i)
        {
            if (i > 1)
                fac *= i;
            double c = coeffs[i] / fac;
            if (i)
                ss << (c >= 0 ? " + " : " - ");
            ss << std::abs(c);
            if (i == 1)
                ss << "x";
            else if (i > 1)
                ss << "x^" << i << "/" << (long long)fac << "!";
        }
        ss << "\n\nUsed when sequence counts labelled structures.";
        return ok(ss.str());
    }

    DMResult partitionGF(int terms)
    {
        // P(x) = ∏_{k=1}^∞ 1/(1-x^k)
        std::vector<long long> p(terms + 1, 0);
        p[0] = 1;
        for (int k = 1; k <= terms; ++k)
            for (int j = k; j <= terms; ++j)
                p[j] += p[j - k];
        std::ostringstream ss;
        ss << "Partition Generating Function\nP(x) = ∏_{k≥1} 1/(1-x^k)\n\nCoefficients p(n):\n";
        for (int i = 0; i <= std::min(terms, 20); ++i)
            ss << "  p(" << i << ")=" << p[i] << "\n";
        return ok(ss.str());
    }

    DMResult frobeniusCoin(const Vec &denoms, long long target)
    {
        // Count ways to make change (unbounded knapsack variant)
        std::vector<long long> dp(target + 1, 0);
        dp[0] = 1;
        for (long long d : denoms)
            for (long long j = d; j <= target; ++j)
                dp[j] += dp[j - d];
        std::ostringstream ss;
        ss << "Coin/Frobenius problem: make " << target << " from {";
        for (size_t i = 0; i < denoms.size(); ++i)
        {
            if (i)
                ss << ",";
            ss << denoms[i];
        }
        ss << "}\nNumber of ways: " << dp[target];
        return ok(str(dp[target]), ss.str());
    }

    // =============================================================================
    // RECURRENCE RELATIONS
    // =============================================================================

    DMResult charPolyRecurrence(const VecD &coeffs)
    {
        int k = coeffs.size();
        std::ostringstream ss;
        ss << "Characteristic polynomial of recurrence:\n";
        ss << "a(n) = " << coeffs[0] << "a(n-1)";
        for (int i = 1; i < k; ++i)
            ss << " + " << coeffs[i] << "a(n-" << i + 1 << ")";
        ss << "\n\nr^" << k;
        for (int i = 0; i < k; ++i)
            ss << " - " << coeffs[i] << "r^" << k - 1 - i;
        ss << " = 0";
        return ok(ss.str());
    }

    DMResult solveLinearRecurrence(const VecD &coeffs, const VecD &initial, int n)
    {
        int k = coeffs.size();
        if ((int)initial.size() < k)
            return err("Need " + str(k) + " initial values");
        // Just iterate the recurrence
        std::vector<double> a(initial.begin(), initial.end());
        while ((int)a.size() <= n)
        {
            double next = 0;
            for (int i = 0; i < k && (int)a.size() - 1 - i >= 0; ++i)
                next += coeffs[i] * a[a.size() - 1 - i];
            a.push_back(next);
        }
        std::ostringstream ss;
        ss << "Recurrence: a(n) = " << coeffs[0] << "a(n-1)";
        for (int i = 1; i < k; ++i)
            ss << " + " << coeffs[i] << "a(n-" << i + 1 << ")";
        ss << "\nInitial values: a(0)=" << initial[0];
        for (int i = 1; i < (int)initial.size(); ++i)
            ss << ", a(" << i << ")=" << initial[i];
        ss << "\na(" << n << ") = " << a[n];
        ss << "\n\nFirst " << std::min(n + 1, 20) << " terms: ";
        for (int i = 0; i <= std::min(n, 19); ++i)
            ss << a[i] << " ";
        return ok(std::to_string(a[n]), ss.str());
    }

    DMResult masterTheorem(double a, double b, double p)
    {
        // T(n) = aT(n/b) + n^p
        double log_b_a = std::log(a) / std::log(b);
        std::ostringstream ss;
        ss << "Master Theorem: T(n) = " << a << "T(n/" << b << ") + n^" << p << "\n";
        ss << "log_b(a) = log_" << b << "(" << a << ") = " << log_b_a << "\n\n";
        if (std::abs(p - log_b_a) < 1e-8)
        {
            ss << "Case 2: p = log_b(a) = " << log_b_a << "\nT(n) = Θ(n^" << p << " log n)";
        }
        else if (p < log_b_a)
        {
            ss << "Case 1: p < log_b(a)\nT(n) = Θ(n^" << log_b_a << ")";
        }
        else
        {
            ss << "Case 3: p > log_b(a)\nT(n) = Θ(n^" << p << ")";
        }
        return ok(ss.str());
    }

    DMResult akraBaszi(double a, double b, double p, double f_power)
    {
        // Akra-Bazzi method (generalisation of master theorem)
        std::ostringstream ss;
        ss << "Akra-Bazzi Method\nT(n) = aT(n/" << b << ") + n^" << f_power << "\n";
        // Find t such that a/b^t = 1
        double t = std::log(a) / std::log(b);
        ss << "Characteristic exponent t: " << a << "/(" << b << ")^t=1  →  t=" << t << "\n";
        if (std::abs(f_power - t) < 1e-8)
            ss << "T(n) = Θ(n^t · log n) = Θ(n^" << t << " log n)";
        else if (f_power < t)
            ss << "T(n) = Θ(n^t) = Θ(n^" << t << ")";
        else
            ss << "T(n) = Θ(n^" << f_power << ")";
        return ok(ss.str());
    }

    // =============================================================================
    // GRAPH THEORY
    // =============================================================================

    DMResult graphDFS(const Mat &adj, int start)
    {
        int n = adj.size();
        if (start >= n)
            return err("Start node out of range");
        std::vector<bool> visited(n, false);
        std::vector<int> order;
        std::function<void(int)> dfs = [&](int v)
        {
            visited[v] = true;
            order.push_back(v);
            for (int u = 0; u < n; ++u)
                if (adj[v][u] && !visited[u])
                    dfs(u);
        };
        dfs(start);
        std::ostringstream ss;
        ss << "DFS from node " << start << ": ";
        for (int i = 0; i < (int)order.size(); ++i)
        {
            if (i)
                ss << "→";
            ss << order[i];
        }
        return ok(ss.str());
    }

    DMResult graphBFS(const Mat &adj, int start)
    {
        int n = adj.size();
        std::vector<bool> visited(n, false);
        std::vector<int> order;
        std::queue<int> q;
        visited[start] = true;
        q.push(start);
        while (!q.empty())
        {
            int v = q.front();
            q.pop();
            order.push_back(v);
            for (int u = 0; u < n; ++u)
                if (adj[v][u] && !visited[u])
                {
                    visited[u] = true;
                    q.push(u);
                }
        }
        std::ostringstream ss;
        ss << "BFS from node " << start << ": ";
        for (int i = 0; i < (int)order.size(); ++i)
        {
            if (i)
                ss << "→";
            ss << order[i];
        }
        return ok(ss.str());
    }

    DMResult dijkstra(const Mat &w, int src)
    {
        int n = w.size();
        std::vector<long long> dist(n, INF);
        dist[src] = 0;
        std::priority_queue<std::pair<long long, int>,
                            std::vector<std::pair<long long, int>>, std::greater<>>
            pq;
        pq.push({0, src});
        std::vector<int> prev(n, -1);
        while (!pq.empty())
        {
            auto [d, u] = pq.top();
            pq.pop();
            if (d > dist[u])
                continue;
            for (int v = 0; v < n; ++v)
                if (w[u][v] > 0 && w[u][v] < INF && dist[u] + w[u][v] < dist[v])
                {
                    dist[v] = dist[u] + w[u][v];
                    prev[v] = u;
                    pq.push({dist[v], v});
                }
        }
        std::ostringstream ss;
        ss << "Dijkstra from node " << src << ":\n";
        for (int i = 0; i < n; ++i)
        {
            ss << "  to " << i << ": dist=";
            if (dist[i] == INF)
                ss << "∞";
            else
                ss << dist[i];
            if (prev[i] >= 0)
            {
                ss << " (path: ";
                std::vector<int> path;
                int cur = i;
                while (cur >= 0)
                {
                    path.push_back(cur);
                    cur = prev[cur];
                }
                std::reverse(path.begin(), path.end());
                for (int j = 0; j < (int)path.size(); ++j)
                {
                    if (j)
                        ss << "→";
                    ss << path[j];
                }
                ss << ")";
            }
            ss << "\n";
        }
        return ok(ss.str());
    }

    DMResult floydWarshall(const Mat &w)
    {
        int n = w.size();
        std::vector<std::vector<long long>> d(n, std::vector<long long>(n, INF));
        for (int i = 0; i < n; ++i)
            for (int j = 0; j < n; ++j)
                if (w[i][j])
                    d[i][j] = w[i][j];
        for (int i = 0; i < n; ++i)
            d[i][i] = 0;
        for (int k = 0; k < n; ++k)
            for (int i = 0; i < n; ++i)
                for (int j = 0; j < n; ++j)
                    if (d[i][k] < INF && d[k][j] < INF)
                        d[i][j] = std::min(d[i][j], d[i][k] + d[k][j]);
        std::ostringstream ss;
        ss << "Floyd-Warshall All-Pairs Shortest Paths:\n";
        for (int i = 0; i < n; ++i)
        {
            ss << "  from " << i << ": [";
            for (int j = 0; j < n; ++j)
            {
                if (j)
                    ss << ",";
                if (d[i][j] == INF)
                    ss << "∞";
                else
                    ss << d[i][j];
            }
            ss << "]\n";
        }
        return ok(ss.str());
    }

    DMResult primMST(const Mat &w)
    {
        int n = w.size();
        std::vector<bool> inMST(n, false);
        std::vector<long long> key(n, INF);
        std::vector<int> parent(n, -1);
        key[0] = 0;
        long long totalWeight = 0;
        std::ostringstream ss;
        ss << "Prim's MST:\n";
        for (int count = 0; count < n; ++count)
        {
            int u = -1;
            for (int v = 0; v < n; ++v)
                if (!inMST[v] && (u == -1 || key[v] < key[u]))
                    u = v;
            inMST[u] = true;
            if (parent[u] >= 0)
            {
                ss << "  Edge " << parent[u] << "-" << u << " weight=" << key[u] << "\n";
                totalWeight += key[u];
            }
            for (int v = 0; v < n; ++v)
                if (w[u][v] > 0 && w[u][v] < INF && !inMST[v] && w[u][v] < key[v])
                {
                    key[v] = w[u][v];
                    parent[v] = u;
                }
        }
        ss << "Total MST weight: " << totalWeight;
        return ok(str(totalWeight), ss.str());
    }

    DMResult topologicalSort(const Mat &adj)
    {
        int n = adj.size();
        std::vector<int> indegree(n, 0);
        for (int u = 0; u < n; ++u)
            for (int v = 0; v < n; ++v)
                if (adj[u][v])
                    indegree[v]++;
        std::queue<int> q;
        for (int i = 0; i < n; ++i)
            if (!indegree[i])
                q.push(i);
        std::vector<int> order;
        while (!q.empty())
        {
            int u = q.front();
            q.pop();
            order.push_back(u);
            for (int v = 0; v < n; ++v)
                if (adj[u][v] && --indegree[v] == 0)
                    q.push(v);
        }
        if ((int)order.size() != n)
            return err("Graph has a cycle — topological sort not possible");
        std::ostringstream ss;
        ss << "Topological order: ";
        for (int i = 0; i < (int)order.size(); ++i)
        {
            if (i)
                ss << "→";
            ss << order[i];
        }
        return ok(ss.str());
    }

    DMResult connectedComponents(const Mat &adj)
    {
        int n = adj.size();
        std::vector<int> comp(n, -1);
        int nc = 0;
        for (int s = 0; s < n; ++s)
        {
            if (comp[s] >= 0)
                continue;
            std::queue<int> q;
            q.push(s);
            comp[s] = nc;
            while (!q.empty())
            {
                int u = q.front();
                q.pop();
                for (int v = 0; v < n; ++v)
                    if (adj[u][v] && comp[v] < 0)
                    {
                        comp[v] = nc;
                        q.push(v);
                    }
            }
            nc++;
        }
        std::ostringstream ss;
        ss << "Connected components: " << nc << "\n";
        for (int c = 0; c < nc; ++c)
        {
            ss << "  C" << c << ": ";
            for (int i = 0; i < n; ++i)
                if (comp[i] == c)
                    ss << i << " ";
            ss << "\n";
        }
        return ok(str(nc), ss.str());
    }

    DMResult isBipartite(const Mat &adj)
    {
        int n = adj.size();
        std::vector<int> color(n, -1);
        bool bip = true;
        for (int s = 0; s < n && bip; ++s)
        {
            if (color[s] >= 0)
                continue;
            std::queue<int> q;
            q.push(s);
            color[s] = 0;
            while (!q.empty() && bip)
            {
                int u = q.front();
                q.pop();
                for (int v = 0; v < n; ++v)
                    if (adj[u][v])
                    {
                        if (color[v] < 0)
                        {
                            color[v] = 1 - color[u];
                            q.push(v);
                        }
                        else if (color[v] == color[u])
                        {
                            bip = false;
                        }
                    }
            }
        }
        return ok(bip ? "true" : "false", bip ? "Graph is bipartite" : "Graph is not bipartite");
    }

    DMResult eulerCircuit(const Mat &adj)
    {
        int n = adj.size();
        // Check: all vertices have even degree
        std::vector<int> deg(n, 0);
        for (int i = 0; i < n; ++i)
            for (int j = 0; j < n; ++j)
                deg[i] += adj[i][j];
        for (int i = 0; i < n; ++i)
            if (deg[i] % 2 != 0)
                return ok("No Euler circuit", "Vertex " + str(i) + " has odd degree " + str(deg[i]));
        // Hierholzer's algorithm
        Mat g = adj;
        std::stack<int> st;
        std::vector<int> circuit;
        st.push(0);
        while (!st.empty())
        {
            int v = st.top();
            bool found = false;
            for (int u = 0; u < n; ++u)
                if (g[v][u] > 0)
                {
                    g[v][u]--;
                    g[u][v]--;
                    st.push(u);
                    found = true;
                    break;
                }
            if (!found)
            {
                circuit.push_back(v);
                st.pop();
            }
        }
        std::ostringstream ss;
        ss << "Euler circuit: ";
        for (int i = 0; i < (int)circuit.size(); ++i)
        {
            if (i)
                ss << "→";
            ss << circuit[i];
        }
        return ok(ss.str());
    }

    DMResult hamiltonianPath(const Mat &adj)
    {
        int n = adj.size();
        std::vector<int> path = {0};
        std::vector<bool> visited(n, false);
        visited[0] = true;
        std::function<bool()> backtrack = [&]() -> bool
        {
            if ((int)path.size() == n)
                return true;
            int last = path.back();
            for (int v = 0; v < n; ++v)
                if (adj[last][v] && !visited[v])
                {
                    visited[v] = true;
                    path.push_back(v);
                    if (backtrack())
                        return true;
                    path.pop_back();
                    visited[v] = false;
                }
            return false;
        };
        if (backtrack())
        {
            std::ostringstream ss;
            ss << "Hamiltonian path: ";
            for (int i = 0; i < (int)path.size(); ++i)
            {
                if (i)
                    ss << "→";
                ss << path[i];
            }
            return ok(ss.str());
        }
        return ok("No Hamiltonian path found from vertex 0");
    }

    DMResult chromaticNumber(const Mat &adj)
    {
        int n = adj.size();
        // Greedy coloring (upper bound)
        std::vector<int> color(n, -1);
        for (int v = 0; v < n; ++v)
        {
            std::set<int> used;
            for (int u = 0; u < n; ++u)
                if (adj[v][u] && color[u] >= 0)
                    used.insert(color[u]);
            int c = 0;
            while (used.count(c))
                c++;
            color[v] = c;
        }
        int chi = *std::max_element(color.begin(), color.end()) + 1;
        std::ostringstream ss;
        ss << "Chromatic number ≤ " << chi << " (greedy upper bound)\n";
        ss << "Coloring: ";
        for (int i = 0; i < n; ++i)
            ss << "v" << i << "=" << color[i] << " ";
        return ok(str(chi), ss.str());
    }

    DMResult graphColorings(const Mat &adj, int k)
    {
        int n = adj.size();
        std::vector<int> color(n, -1);
        int count = 0;
        std::function<void(int)> bt = [&](int v)
        {
            if (v == n)
            {
                count++;
                return;
            }
            for (int c = 0; c < k; ++c)
            {
                bool ok_ = true;
                for (int u = 0; u < v; ++u)
                    if (adj[v][u] && color[u] == c)
                    {
                        ok_ = false;
                        break;
                    }
                if (ok_)
                {
                    color[v] = c;
                    bt(v + 1);
                    color[v] = -1;
                }
            }
        };
        bt(0);
        return ok(str(count), "Number of valid " + str(k) + "-colorings = " + str(count));
    }

    DMResult graphDiameter(const Mat &adj)
    {
        int n = adj.size();
        // BFS from each vertex (unweighted)
        long long diam = 0;
        for (int s = 0; s < n; ++s)
        {
            std::vector<long long> d(n, -1);
            d[s] = 0;
            std::queue<int> q;
            q.push(s);
            while (!q.empty())
            {
                int u = q.front();
                q.pop();
                for (int v = 0; v < n; ++v)
                    if (adj[u][v] && d[v] < 0)
                    {
                        d[v] = d[u] + 1;
                        q.push(v);
                    }
            }
            for (long long di : d)
                if (di > diam)
                    diam = di;
        }
        return ok(str(diam), "Graph diameter = " + str(diam));
    }

    DMResult articulationPoints(const Mat &adj)
    {
        int n = adj.size(), timer = 0;
        std::vector<int> disc(n, -1), low(n, 0), parent(n, -1);
        std::vector<bool> ap(n, false);
        std::function<void(int)> dfs = [&](int u)
        {
            int children = 0;
            disc[u] = low[u] = timer++;
            for (int v = 0; v < n; ++v)
                if (adj[u][v])
                {
                    if (disc[v] < 0)
                    {
                        children++;
                        parent[v] = u;
                        dfs(v);
                        low[u] = std::min(low[u], low[v]);
                        if (parent[u] < 0 && children > 1)
                            ap[u] = true;
                        if (parent[u] >= 0 && low[v] >= disc[u])
                            ap[u] = true;
                    }
                    else if (v != parent[u])
                        low[u] = std::min(low[u], disc[v]);
                }
        };
        for (int i = 0; i < n; ++i)
            if (disc[i] < 0)
                dfs(i);
        std::ostringstream ss;
        ss << "Articulation points: ";
        bool any = false;
        for (int i = 0; i < n; ++i)
            if (ap[i])
            {
                ss << i << " ";
                any = true;
            }
        if (!any)
            ss << "none";
        return ok(ss.str());
    }

    DMResult bridgeEdges(const Mat &adj)
    {
        int n = adj.size(), timer = 0;
        std::vector<int> disc(n, -1), low(n, 0), parent(n, -1);
        std::vector<std::pair<int, int>> bridges;
        std::function<void(int)> dfs = [&](int u)
        {
            disc[u] = low[u] = timer++;
            for (int v = 0; v < n; ++v)
                if (adj[u][v])
                {
                    if (disc[v] < 0)
                    {
                        parent[v] = u;
                        dfs(v);
                        low[u] = std::min(low[u], low[v]);
                        if (low[v] > disc[u])
                            bridges.push_back({u, v});
                    }
                    else if (v != parent[u])
                        low[u] = std::min(low[u], disc[v]);
                }
        };
        for (int i = 0; i < n; ++i)
            if (disc[i] < 0)
                dfs(i);
        std::ostringstream ss;
        ss << "Bridge edges: ";
        for (auto &[u, v] : bridges)
            ss << "(" << u << "-" << v << ") ";
        if (bridges.empty())
            ss << "none";
        return ok(ss.str());
    }

    DMResult maxFlowFordFulkerson(const Mat &capacity, int src, int sink)
    {
        int n = capacity.size();
        Mat flow(n, std::vector<long long>(n, 0));
        long long maxFlow = 0;
        // BFS augmenting path
        auto bfs = [&](std::vector<int> &parent) -> bool
        {
            std::vector<bool> vis(n, false);
            vis[src] = true;
            parent[src] = -1;
            std::queue<int> q;
            q.push(src);
            while (!q.empty())
            {
                int u = q.front();
                q.pop();
                for (int v = 0; v < n; ++v)
                    if (!vis[v] && capacity[u][v] - flow[u][v] > 0)
                    {
                        vis[v] = true;
                        parent[v] = u;
                        if (v == sink)
                            return true;
                        q.push(v);
                    }
            }
            return false;
        };
        std::vector<int> parent(n);
        while (bfs(parent))
        {
            // Find min residual
            long long pathFlow = INF;
            for (int v = sink; v != src; v = parent[v])
                pathFlow = std::min(pathFlow, capacity[parent[v]][v] - flow[parent[v]][v]);
            for (int v = sink; v != src; v = parent[v])
            {
                flow[parent[v]][v] += pathFlow;
                flow[v][parent[v]] -= pathFlow;
            }
            maxFlow += pathFlow;
        }
        return ok(str(maxFlow), "Max flow = " + str(maxFlow));
    }

    DMResult bipartiteMatching(const Mat &adj)
    {
        int n = adj.size();
        std::vector<int> match(n, -1);
        int matched = 0;
        std::function<bool(int, std::vector<bool> &)> augment = [&](int u, std::vector<bool> &vis) -> bool
        {
            for (int v = 0; v < n; ++v)
                if (adj[u][v] && !vis[v])
                {
                    vis[v] = true;
                    if (match[v] < 0 || augment(match[v], vis))
                    {
                        match[v] = u;
                        return true;
                    }
                }
            return false;
        };
        for (int u = 0; u < n / 2; ++u)
        {
            std::vector<bool> vis(n, false);
            if (augment(u, vis))
                matched++;
        }
        std::ostringstream ss;
        ss << "Maximum bipartite matching: " << matched << "\nMatching: ";
        for (int v = 0; v < n; ++v)
            if (match[v] >= 0)
                ss << match[v] << "–" << v << " ";
        return ok(str(matched), ss.str());
    }

    DMResult adjacencyEigenvalues(const Mat &adj)
    {
        // Power iteration for spectral radius
        int n = adj.size();
        std::vector<double> v(n, 1.0 / std::sqrt(n));
        double lambda = 0;
        for (int iter = 0; iter < 100; ++iter)
        {
            std::vector<double> av(n, 0);
            for (int i = 0; i < n; ++i)
                for (int j = 0; j < n; ++j)
                    av[i] += adj[i][j] * v[j];
            lambda = 0;
            for (double vi : av)
                lambda += vi * vi;
            lambda = std::sqrt(lambda);
            if (lambda > 1e-10)
                for (auto &vi : av)
                    vi /= lambda;
            v = av;
        }
        std::ostringstream ss;
        ss << "Largest eigenvalue (spectral radius) ≈ " << lambda << "\n";
        ss << "(Eigenvector approximation via power iteration)\n";
        ss << "For complete graph K_n: λ₁ = n-1\n";
        ss << "For bipartite: spectrum symmetric about 0";
        return ok(std::to_string(lambda), ss.str());
    }

    DMResult kruskalMST(int n, const std::vector<std::tuple<long long, int, int>> &edges)
    {
        // Union-find
        std::vector<int> parent(n), rank_(n, 0);
        std::iota(parent.begin(), parent.end(), 0);
        std::function<int(int)> find = [&](int x) -> int
        { return parent[x] == x ? x : parent[x] = find(parent[x]); };
        auto unite = [&](int a, int b)
        {a=find(a);b=find(b);if(a==b)return;if(rank_[a]<rank_[b])std::swap(a,b);parent[b]=a;if(rank_[a]==rank_[b])rank_[a]++; };
        auto sorted = edges;
        std::sort(sorted.begin(), sorted.end());
        long long total = 0;
        std::ostringstream ss;
        ss << "Kruskal's MST:\n";
        for (auto &[w, u, v] : sorted)
        {
            if (find(u) != find(v))
            {
                unite(u, v);
                ss << "  Edge " << u << "-" << v << " w=" << w << "\n";
                total += w;
            }
        }
        ss << "Total weight: " << total;
        return ok(str(total), ss.str());
    }

    // =============================================================================
    // BOOLEAN LOGIC
    // =============================================================================

    DMResult truthTable(const std::string &expr, const std::vector<std::string> &vars)
    {
        int n = vars.size();
        std::ostringstream ss;
        // Header
        for (auto &v : vars)
            ss << v << "\t";
        ss << expr << "\n";
        ss << std::string(40, '-') << "\n";
        // Evaluate via simple recursive parser
        // For now: evaluate by substitution for AND/OR/NOT/XOR
        auto eval = [&](const std::string &e, std::map<std::string, bool> &vals) -> bool
        {
            // Very simple evaluator for and/or/not/xor with single-letter vars
            std::function<bool(const std::string &)> ev = [&](const std::string &s) -> bool
            {
                if (s.size() == 1 && vals.count(s))
                    return vals[s];
                if (s.find("NOT ") != std::string::npos)
                {
                    return !ev(s.substr(4));
                }
                // Find outermost OR
                int depth = 0;
                for (int i = s.size() - 1; i >= 0; --i)
                {
                    if (s[i] == ')')
                        depth++;
                    else if (s[i] == '(')
                        depth--;
                    else if (depth == 0 && i + 2 < (int)s.size() && s.substr(i, 3) == "OR ")
                    {
                        return ev(s.substr(0, i)) || ev(s.substr(i + 3));
                    }
                }
                for (int i = s.size() - 1; i >= 0; --i)
                {
                    if (depth == 0 && i + 4 < (int)s.size() && s.substr(i, 4) == "AND ")
                    {
                        return ev(s.substr(0, i)) && ev(s.substr(i + 4));
                    }
                }
                if (!s.empty() && vals.count(std::string(1, s[0])))
                    return vals[std::string(1, s[0])];
                return false;
            };
            return ev(e);
        };
        for (int mask = 0; mask < (1 << n); ++mask)
        {
            std::map<std::string, bool> vals;
            for (int i = 0; i < n; ++i)
            {
                bool v = (mask >> (n - 1 - i)) & 1;
                ss << (v ? "T" : "F") << "\t";
                vals[vars[i]] = v;
            }
            bool result = eval(expr, vals);
            ss << (result ? "T" : "F") << "\n";
        }
        return ok(ss.str());
    }

    DMResult cnfConvert(const std::string &expr, const std::vector<std::string> &vars)
    {
        std::ostringstream ss;
        ss << "CNF (Conjunctive Normal Form) of: " << expr << "\n\n";
        ss << "Steps:\n1. Eliminate biconditionals (p↔q = (p→q)∧(q→p))\n";
        ss << "2. Eliminate implications (p→q = ¬p∨q)\n";
        ss << "3. Push negations in (De Morgan's laws)\n";
        ss << "4. Distribute OR over AND\n\n";
        ss << "Result: (clause₁) ∧ (clause₂) ∧ ...\n";
        ss << "Each clause is a disjunction of literals.\n\n";
        ss << "(Full symbolic CNF conversion requires a SAT preprocessor)\n";
        ss << "For truth table based CNF:\n";
        ss << "  Identify rows where formula is FALSE,\n";
        ss << "  write a clause for each FALSE row.";
        return ok(ss.str());
    }

    DMResult dnfConvert(const std::string &expr, const std::vector<std::string> &vars)
    {
        std::ostringstream ss;
        ss << "DNF (Disjunctive Normal Form) of: " << expr << "\n\n";
        ss << "Identify rows in truth table where formula is TRUE,\n";
        ss << "write a minterm for each TRUE row, connect with OR.\n\n";
        ss << "Steps:\n1. Build truth table\n2. For each TRUE row write: ";
        ss << "x₁ ∧ x₂ ∧ ... (use ¬xᵢ if variable is 0)\n";
        ss << "3. Connect all minterms with ∨\n\n";
        ss << "DNF is always satisfiable if any row is TRUE.";
        return ok(ss.str());
    }

    DMResult quineMcCluskey(const Vec &minterms, int numVars)
    {
        std::ostringstream ss;
        ss << "Quine-McCluskey Minimization\n";
        ss << "Minterms: {";
        for (size_t i = 0; i < minterms.size(); ++i)
        {
            if (i)
                ss << ",";
            ss << minterms[i];
        }
        ss << "}\nVariables: " << numVars << "\n\n";
        // Group by number of 1-bits
        auto ones = [](long long n) -> int
        {int c=0;while(n){c+=n&1;n>>=1;}return c; };
        std::map<int, std::vector<long long>> groups;
        for (long long m : minterms)
            groups[ones(m)].push_back(m);
        ss << "Groups by number of 1s:\n";
        for (auto &[k, v] : groups)
        {
            ss << "  " << k << "-group: ";
            for (long long m : v)
            {
                std::string bits;
                for (int i = numVars - 1; i >= 0; --i)
                    bits += (((m >> i) & 1) ? '1' : '0');
                ss << bits << " ";
            }
            ss << "\n";
        }
        ss << "\n(Full algorithm: compare adjacent groups, mark combined terms, extract prime implicants)";
        return ok(ss.str());
    }

    // =============================================================================
    // SET THEORY
    // =============================================================================

    DMResult setUnion(const Vec &A, const Vec &B)
    {
        std::set<long long> s(A.begin(), A.end());
        s.insert(B.begin(), B.end());
        std::ostringstream ss;
        ss << "A ∪ B = {";
        bool f = true;
        for (long long v : s)
        {
            if (!f)
                ss << ",";
            ss << v;
            f = false;
        }
        ss << "}";
        return ok(ss.str());
    }

    DMResult setIntersection(const Vec &A, const Vec &B)
    {
        std::set<long long> sA(A.begin(), A.end());
        std::ostringstream ss;
        ss << "A ∩ B = {";
        bool f = true;
        for (long long v : B)
            if (sA.count(v))
            {
                if (!f)
                    ss << ",";
                ss << v;
                f = false;
            }
        ss << "}";
        return ok(ss.str());
    }

    DMResult setDifference(const Vec &A, const Vec &B)
    {
        std::set<long long> sB(B.begin(), B.end());
        std::ostringstream ss;
        ss << "A \\ B = {";
        bool f = true;
        for (long long v : A)
            if (!sB.count(v))
            {
                if (!f)
                    ss << ",";
                ss << v;
                f = false;
            }
        ss << "}";
        return ok(ss.str());
    }

    DMResult setSymDiff(const Vec &A, const Vec &B)
    {
        std::set<long long> sA(A.begin(), A.end()), sB(B.begin(), B.end());
        std::ostringstream ss;
        ss << "A △ B = {";
        bool f = true;
        for (long long v : A)
            if (!sB.count(v))
            {
                if (!f)
                    ss << ",";
                ss << v;
                f = false;
            }
        for (long long v : B)
            if (!sA.count(v))
            {
                if (!f)
                    ss << ",";
                ss << v;
                f = false;
            }
        ss << "}";
        return ok(ss.str());
    }

    DMResult powerSet(const Vec &A)
    {
        int n = A.size();
        if (n > 20)
            return err("Too large (max 20 elements)");
        std::ostringstream ss;
        ss << "P(A) has 2^" << n << "=" << (1 << n) << " elements:\n";
        for (int mask = 0; mask < (1 << n); ++mask)
        {
            ss << "{";
            bool f = true;
            for (int i = 0; i < n; ++i)
                if ((mask >> i) & 1)
                {
                    if (!f)
                        ss << ",";
                    ss << A[i];
                    f = false;
                }
            ss << "} ";
            if (mask % 8 == 7)
                ss << "\n";
        }
        return ok(str(1 << n), ss.str());
    }

    DMResult cartesianProduct(const Vec &A, const Vec &B)
    {
        std::ostringstream ss;
        ss << "A × B = {";
        bool f = true;
        for (long long a : A)
            for (long long b : B)
            {
                if (!f)
                    ss << ",";
                ss << "(" << a << "," << b << ")";
                f = false;
            }
        ss << "}";
        return ok(str(A.size() * B.size()), ss.str());
    }

    DMResult isSubset(const Vec &A, const Vec &B)
    {
        std::set<long long> sB(B.begin(), B.end());
        bool sub = true;
        for (long long a : A)
            if (!sB.count(a))
            {
                sub = false;
                break;
            }
        return ok(sub ? "true" : "false", sub ? "A ⊆ B" : "A ⊄ B");
    }

    // =============================================================================
    // DISPATCH
    // =============================================================================
    // [Helper functions (getP, getN, parseVec, parseMat) are defined earlier in file]

    DMResult dispatch(const std::string &op, const std::string &json)
    {
        // ── Fix 1: cu_getStr (aliased as getP) requires a 3rd default-value arg.
        // ── Fix 2: parseVec returns DVec (vector<double>); parseVec returns DMat
        //           (vector<vector<double>>), but all math functions take Vec /
        //           Mat (vector<long long> / vector<vector<long long>>).
        //           These helpers perform the element-wise narrowing cast.
        auto toVec = [](const std::vector<double> &dv) -> Vec
        {
            return Vec(dv.begin(), dv.end());
        };
        auto toMat = [](const std::vector<std::vector<double>> &dm) -> Mat
        {
            Mat m;
            m.reserve(dm.size());
            for (const auto &row : dm)
                m.push_back(Vec(row.begin(), row.end()));
            return m;
        };

        try
        {
            if (op == "combinations")
                return combinations((int)getN(json, "n"), (int)getN(json, "r"));
            if (op == "permutations")
                return permutations((int)getN(json, "n"), (int)getN(json, "r"));
            if (op == "multiset")
                return multisetCoeff((int)getN(json, "n"), (int)getN(json, "r"));
            if (op == "derangements")
                return derangements((int)getN(json, "n"));
            if (op == "pigeonhole")
                return pigeonhole((int)getN(json, "items"), (int)getN(json, "holes"));
            if (op == "ramsey")
                return ramseyBound((int)getN(json, "s"), (int)getN(json, "t"));
            if (op == "necklace")
                return necklaceCount((int)getN(json, "n"), (int)getN(json, "k"));
            if (op == "partition_gf")
                return partitionGF((int)getN(json, "terms", 20));
            if (op == "coin_change")
            {
                // d: DVec → Vec; target is passed as long long
                auto d = toVec(parseVec(getP(json, "denoms", "")));
                return frobeniusCoin(d, (long long)getN(json, "target"));
            }
            if (op == "recurrence")
            {
                // solveLinearRecurrence takes VecD (vector<double>) — no toVec needed
                auto c = parseVecD(getP(json, "coeffs", ""));
                auto i = parseVecD(getP(json, "initial", ""));
                return solveLinearRecurrence(c, i, (int)getN(json, "n", 10));
            }
            if (op == "master_theorem")
                return masterTheorem(std::stod(getP(json, "a", "0")),
                                     std::stod(getP(json, "b", "2")),
                                     std::stod(getP(json, "p", "0")));
            if (op == "akra_bazzi")
                return akraBaszi(std::stod(getP(json, "a", "0")),
                                 std::stod(getP(json, "b", "2")),
                                 0,
                                 std::stod(getP(json, "p", "0")));
            if (op == "dfs")
            {
                auto a = toMat(parseMat(getP(json, "adj", "")));
                return graphDFS(a, (int)getN(json, "start"));
            }
            if (op == "bfs")
            {
                auto a = toMat(parseMat(getP(json, "adj", "")));
                return graphBFS(a, (int)getN(json, "start"));
            }
            if (op == "dijkstra")
            {
                auto a = toMat(parseMat(getP(json, "adj", "")));
                return dijkstra(a, (int)getN(json, "src"));
            }
            if (op == "floyd_warshall")
            {
                auto a = toMat(parseMat(getP(json, "adj", "")));
                return floydWarshall(a);
            }
            if (op == "prim_mst")
            {
                auto a = toMat(parseMat(getP(json, "adj", "")));
                return primMST(a);
            }
            if (op == "topo_sort")
            {
                auto a = toMat(parseMat(getP(json, "adj", "")));
                return topologicalSort(a);
            }
            if (op == "components")
            {
                auto a = toMat(parseMat(getP(json, "adj", "")));
                return connectedComponents(a);
            }
            if (op == "bipartite")
            {
                auto a = toMat(parseMat(getP(json, "adj", "")));
                return isBipartite(a);
            }
            if (op == "euler_circuit")
            {
                auto a = toMat(parseMat(getP(json, "adj", "")));
                return eulerCircuit(a);
            }
            if (op == "hamiltonian")
            {
                auto a = toMat(parseMat(getP(json, "adj", "")));
                return hamiltonianPath(a);
            }
            if (op == "chromatic")
            {
                auto a = toMat(parseMat(getP(json, "adj", "")));
                return chromaticNumber(a);
            }
            if (op == "diameter")
            {
                auto a = toMat(parseMat(getP(json, "adj", "")));
                return graphDiameter(a);
            }
            if (op == "articulation")
            {
                auto a = toMat(parseMat(getP(json, "adj", "")));
                return articulationPoints(a);
            }
            if (op == "bridges")
            {
                auto a = toMat(parseMat(getP(json, "adj", "")));
                return bridgeEdges(a);
            }
            if (op == "max_flow")
            {
                auto a = toMat(parseMat(getP(json, "adj", "")));
                return maxFlowFordFulkerson(a, (int)getN(json, "src"), (int)getN(json, "sink"));
            }
            if (op == "matching")
            {
                auto a = toMat(parseMat(getP(json, "adj", "")));
                return bipartiteMatching(a);
            }
            if (op == "spec_radius")
            {
                auto a = toMat(parseMat(getP(json, "adj", "")));
                return adjacencyEigenvalues(a);
            }
            if (op == "truth_table")
            {
                // vars are char codes stored as doubles — iterate as double, cast to char
                auto v = parseVec(getP(json, "vars", ""));
                std::vector<std::string> sv;
                sv.reserve(v.size());
                for (double x : v)
                    sv.push_back(std::string(1, static_cast<char>(static_cast<int>(x))));
                return truthTable(getP(json, "expr", ""), sv);
            }
            if (op == "set_union")
                return setUnion(toVec(parseVec(getP(json, "A", ""))),
                                toVec(parseVec(getP(json, "B", ""))));
            if (op == "set_intersect")
                return setIntersection(toVec(parseVec(getP(json, "A", ""))),
                                       toVec(parseVec(getP(json, "B", ""))));
            if (op == "set_diff")
                return setDifference(toVec(parseVec(getP(json, "A", ""))),
                                     toVec(parseVec(getP(json, "B", ""))));
            if (op == "set_sym_diff")
                return setSymDiff(toVec(parseVec(getP(json, "A", ""))),
                                  toVec(parseVec(getP(json, "B", ""))));
            if (op == "power_set")
                return powerSet(toVec(parseVec(getP(json, "A", ""))));
            if (op == "cartesian")
                return cartesianProduct(toVec(parseVec(getP(json, "A", ""))),
                                        toVec(parseVec(getP(json, "B", ""))));
            if (op == "is_subset")
                return isSubset(toVec(parseVec(getP(json, "A", ""))),
                                toVec(parseVec(getP(json, "B", ""))));
            if (op == "inclusion_excl" || op == "inclusion_exclusion")
            {
                auto dv = parseVec(getP(json, "sizes", ""));
                Vec sizes(dv.begin(), dv.end());
                return inclusionExclusion((int)sizes.size(), sizes);
            }
            return err("Unknown discrete math operation: " + op);
        }
        catch (const std::exception &e)
        {
            return err(e.what());
        }
    }

} // namespace DiscreteMath