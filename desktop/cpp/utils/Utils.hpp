#pragma once
// utils/Utils.hpp
// =============================================================================
// Umbrella header — include this single file to replace every STL container
// and data structure used in the MathEngine C++ codebase.
//
// Drop-in replacement mapping:
//
//   std::vector<T>            →  utils::Vec<T>
//   std::vector<std::vector<T>> →  utils::Mat<T>   (or Vec<Vec<T>>)
//   std::unordered_map<K,V>   →  utils::HashMap<K,V>
//   std::map<K,V>             →  utils::OrderedMap<K,V>
//   std::stack<T>             →  utils::Stack<T>
//   std::queue<T> / deque<T>  →  utils::Queue<T>
//   std::pair<A,B>            →  utils::Pair<A,B>
//   std::tuple<A,B,C>         →  utils::Tuple3<A,B,C>
//   std::optional<T>          →  utils::Optional<T>
//   std::complex<double>      →  utils::Complex
//   std::function<R(Args...)> →  utils::Function<R(Args...)>
//   std::set<T>               →  utils::Set<T>
//   std::sort/min/max/swap/accumulate/iota/max_element/min_element/
//   upper_bound/unique/reverse/all_of (algorithm+numeric) → utils:: free
//   functions of the same name (Algorithm.hpp), same call signature.
//
// Existing type aliases (DVec, DMat, IVec, SVec, …) are preserved and
// re-declared in the utils namespace so every module that does
//   #include "utils/Utils.hpp"
//   using namespace utils;
// needs zero further changes to its existing code.
// =============================================================================

#include "Vec.hpp"
#include "Mat.hpp"
#include "Complex.hpp"
#include "HashMap.hpp"
#include "OrderedMap.hpp"
#include "Stack.hpp"
#include "Queue.hpp"
#include "Pair.hpp"
#include "Tuple3.hpp"
#include "Optional.hpp"
#include "Function.hpp"
#include "Set.hpp"
#include "Algorithm.hpp"
#include "PriorityQueue.hpp"

namespace utils {

// ── Scalar type aliases (unchanged from MathCore.hpp / CommonUtils.hpp) ───────
using Real = double;
using Int  = long long;

// ── Vec aliases ───────────────────────────────────────────────────────────────
// DVec / IVec / SVec already declared in Vec.hpp
using CVec  = Vec<Complex>;
using RVec  = Vec<Real>;

// ── Mat aliases ───────────────────────────────────────────────────────────────
// DMat / IMat already declared in Mat.hpp
using CMat  = Mat<Complex>;
using RMat  = Mat<Real>;

// The codebase also uses "Matrix" as an alias for DMat in Linear_Algebra
using Matrix  = DMat;
using CMatrix = CMat;

// ── SymbolTable (used by the expression evaluator) ───────────────────────────
// Already declared in HashMap.hpp as HashMap<string,double>

// ── GraphEdge (used by DiscreteMath graph algorithms) ────────────────────────
// Already declared in Tuple3.hpp as Tuple3<long long,int,int>

} // namespace utils

// ── Migration guide ───────────────────────────────────────────────────────────
// To migrate an existing module (e.g. Statistics.cpp):
//
//   BEFORE:
//     #include <vector>
//     #include <unordered_map>
//     #include <map>
//     #include <stack>
//     #include <queue>
//     #include <optional>
//     #include <complex>
//     #include <functional>
//     using DVec = std::vector<double>;
//
//   AFTER:
//     #include "utils/Utils.hpp"
//     using namespace utils;
//
// All the aliases (DVec, DMat, Matrix, SymbolTable, …) are already defined
// in the utils namespace, so every Vec<double>, Mat<double>, HashMap<…>, etc.
// is immediately available and existing code compiles unchanged.
