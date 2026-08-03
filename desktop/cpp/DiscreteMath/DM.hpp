#pragma once
// DM.hpp — CoreEngine prefix: "dm:<op>|{json}"

#ifndef DM_HPP
#define DM_HPP

#include <string>
#include <vector>

namespace DiscreteMath
{

    struct DMResult
    {
        bool ok = true;
        std::string value;
        std::string detail;
        std::string error;
    };

    using Vec = std::vector<long long>;
    using VecD = std::vector<double>;
    using Mat = std::vector<std::vector<long long>>;
    using MatD = std::vector<std::vector<double>>;
    // ── Combinatorics ─────────────────────────────────────────────────────────────
    DMResult combinations(int n, int r);
    DMResult permutations(int n, int r);
    DMResult multisetCoeff(int n, int r);                 // C(n+r-1, r)
    DMResult derangements(int n);                         // D(n)
    DMResult inclusionExclusion(int n, const Vec &sizes); // PIE
    DMResult pigeonhole(int items, int holes);
    DMResult ramseyBound(int s, int t);   // R(s,t) bounds
    DMResult necklaceCount(int n, int k); // Burnside / Polya

    // ── Generating functions ──────────────────────────────────────────────────────
    DMResult ordinaryGF(const Vec &coeffs, int terms);
    DMResult exponentialGF(const Vec &coeffs, int terms);
    DMResult partitionGF(int terms); // product formula
    DMResult frobeniusCoin(const Vec &denominations, long long target);

    // ── Recurrence relations ──────────────────────────────────────────────────────
    // a(n) = c1*a(n-1) + c2*a(n-2) + ... + ck*a(n-k)
    DMResult solveLinearRecurrence(const VecD &coeffs,  // c1..ck
                                   const VecD &initial, // a(0)..a(k-1)
                                   int n);
    DMResult charPolyRecurrence(const VecD &coeffs); // characteristic poly
    DMResult akraBaszi(double a, double b, double p, double f_power);
    DMResult masterTheorem(double a, double b, double p); // T(n)=aT(n/b)+n^p

    // ── Graph theory ──────────────────────────────────────────────────────────────
    // All graphs represented as adjacency matrix (flat) or edge list
    DMResult graphDFS(const Mat &adj, int start);
    DMResult graphBFS(const Mat &adj, int start);
    DMResult dijkstra(const Mat &weights, int src); // weights matrix
    DMResult floydWarshall(const Mat &weights);
    DMResult primMST(const Mat &weights);
    DMResult kruskalMST(int n, const std::vector<std::tuple<long long, int, int>> &edges);
    DMResult topologicalSort(const Mat &adj);
    DMResult connectedComponents(const Mat &adj);
    DMResult isBipartite(const Mat &adj);
    DMResult eulerCircuit(const Mat &adj);
    DMResult hamiltonianPath(const Mat &adj);       // backtracking
    DMResult chromaticNumber(const Mat &adj);       // greedy bound
    DMResult graphColorings(const Mat &adj, int k); // check k-coloring
    DMResult adjacencyEigenvalues(const Mat &adj);
    DMResult graphDiameter(const Mat &adj);
    DMResult articulationPoints(const Mat &adj);
    DMResult bridgeEdges(const Mat &adj);
    DMResult maxFlowFordFulkerson(const Mat &capacity, int src, int sink);
    DMResult bipartiteMatching(const Mat &adj); // Hungarian / Hopcroft-Karp

    // ── Boolean algebra & logic ───────────────────────────────────────────────────
    DMResult truthTable(const std::string &expr,
                        const std::vector<std::string> &vars);
    DMResult cnfConvert(const std::string &expr,
                        const std::vector<std::string> &vars);
    DMResult dnfConvert(const std::string &expr,
                        const std::vector<std::string> &vars);
    DMResult quineMcCluskey(const Vec &minterms, int numVars);

    // ── Set theory ────────────────────────────────────────────────────────────────
    DMResult setUnion(const Vec &A, const Vec &B);
    DMResult setIntersection(const Vec &A, const Vec &B);
    DMResult setDifference(const Vec &A, const Vec &B);
    DMResult setSymDiff(const Vec &A, const Vec &B);
    DMResult powerSet(const Vec &A);
    DMResult cartesianProduct(const Vec &A, const Vec &B);
    DMResult isSubset(const Vec &A, const Vec &B);

    // ── Dispatch ──────────────────────────────────────────────────────────────────
    DMResult dispatch(const std::string &op, const std::string &json);

} // namespace DiscreteMath
#endif