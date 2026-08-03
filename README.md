# MathEngine

A full-stack desktop mathematics application built as a senior capstone project at Youngstown State University. MathEngine combines a high-performance C++23 computation engine (exposed via JNI), a JavaFX 21 desktop client, a Spring Boot 3 REST server, and a Python-powered natural-language chatbot into a single cohesive system.

---

## Table of Contents

1. [Architecture Overview](#architecture-overview)
2. [Project Directory](#project-directory)
3. [Math Modules](#math-modules)
4. [Expression Syntax](#expression-syntax)
5. [Chatbot](#chatbot)
6. [REST API](#rest-api)
7. [Custom Data Structures (`utils/`)](#custom-data-structures-utils)
8. [Building and Running](#building-and-running)
9. [Bug Fixes Applied](#bug-fixes-applied)
10. [Known Limitations](#known-limitations)

---

## Architecture Overview

```
┌─────────────────────────────────────────────────────────────┐
│                    JavaFX Desktop Client                    │
│  MathEngineApp → MainLayout → {Compute, Graph, Chat} tabs   │
│  InputPanel · OutputPanel · GraphPanel · ChatbotPanel       │
│  QuickFunctionsPanel · OperationPanel · MathKeyboard        │
└───────────┬──────────────────────────┬──────────────────────┘
            │ JNI (mathengine.dll)     │ HTTP/JSON (port 8080)
            ▼                          ▼
┌───────────────────────┐   ┌──────────────────────────────────┐
│    C++ Math Engine    │   │   Spring Boot 3 REST Server      │
│  CoreEngine.cpp       │   │   /api/compute  /api/history     │
│  Preprocessor.cpp     │   │   /api/register /api/login       │
│  12 Math Modules      │   │   /api/preferences               │
│  utils/ (custom DSA)  │   │   SQLite · JWT auth (72 h)       │
└───────────────────────┘   └──────────────────────────────────┘
                                          ▲
                                          │ HTTP/JSON (stdin/stdout JSON)
                             ┌────────────┴─────────────┐
                             │   chatbot.py (Python 3)  │
                             │   NL intent parsing      │
                             │   Domain validation      │
                             │   Conversational memory  │
                             │   Step-by-step formatting│
                             └──────────────────────────┘
```

**Data flow for a user expression:**
1. User types (or selects a quick function) in `InputPanel`
2. `MathBridge.java` calls the native `CoreEngine::compute(expression)` via JNI
3. `Preprocessor` converts natural bracket-notation (e.g. `integrate[x^2,0,5]`) to a structured engine prefix (e.g. `calc:integral|{...}`)
4. `CoreEngine` routes to the appropriate math module
5. The result (symbolic + numeric) is returned and rendered in `OutputPanel` (LaTeX where possible)

---

## Project Directory

```
MathEngine/
│
├── build.ps1                        # Top-level build shortcut (forwards to scripts/)
├── CMakeLists.txt                   # CMake build for the C++ shared library
├── pom.xml                          # Maven build for the JavaFX client
├── chatbot.py                       # Python chatbot backend (run by ChatbotPanel.java)
├── MathCore.hpp                     # Shared scalar/vector/matrix type aliases
├── CommonUtils.hpp / .cpp           # Shared JSON helpers, parse utilities
├── CoreEngine.hpp / .cpp            # Top-level expression router
├── Preprocessor.cpp                 # Natural syntax → structured prefix converter
├── MathBridgeJNI.cpp                # JNI glue between Java and C++
│
├── utils/                           # Custom OO data structures (replaces STL containers)
│   ├── Utils.hpp                    # ← Single umbrella include for all of utils/
│   ├── Vec.hpp                      # Dynamic array  (replaces std::vector)
│   ├── Mat.hpp                      # 2-D matrix     (replaces vector<vector<T>>)
│   ├── Complex.hpp                  # Complex number (replaces std::complex<double>)
│   ├── HashMap.hpp                  # Hash map       (replaces std::unordered_map)
│   ├── OrderedMap.hpp               # Sorted map     (replaces std::map, LLRB BST)
│   ├── Stack.hpp                    # LIFO stack     (replaces std::stack)
│   ├── Queue.hpp                    # FIFO queue     (replaces std::queue / deque)
│   ├── Pair.hpp                     # Value pair     (replaces std::pair)
│   ├── Tuple3.hpp                   # Value triple   (replaces std::tuple<A,B,C>)
│   ├── Optional.hpp                 # Optional value (replaces std::optional)
│   └── Function.hpp                 # Callable       (replaces std::function, SBO)
│
├── AbstractAlgebra/
│   ├── AA.hpp
│   └── AA.cpp                       # Groups, rings, fields, permutations (45 ops)
│
├── AppliedMath/
│   ├── AM.hpp
│   └── AM.cpp                       # Laplace/inverse, FFT, optimization, signals (65 ops)
│
├── Calculus/
│   ├── Calculus.hpp / .cpp          # Top-level Calculus dispatcher (39 ops)
│   ├── core/
│   │   ├── Expression.hpp / .cpp    # Expression tree: parse, evaluate, toString
│   │   ├── Simplify.hpp / .cpp      # Symbolic simplification rules
│   │   └── Week4Bridge.hpp / .cpp   # Bridge to week-4 lexer/parser/evaluator
│   ├── differentiation/
│   │   ├── Derivative.hpp / .cpp    # Symbolic differentiation (chain/product/quotient)
│   │   ├── Implicit.hpp / .cpp      # Implicit differentiation
│   │   └── Partial.hpp / .cpp       # Partial and higher-order derivatives
│   ├── integration/
│   │   ├── Symbolic.hpp / .cpp      # Antiderivatives, definite integrals
│   │   ├── Numerical.hpp / .cpp     # Romberg, Simpson, Gauss-Legendre
│   │   └── Multivariable.hpp / .cpp # Double/triple/polar/cylindrical/spherical integrals
│   ├── limits/
│   │   └── Limits.hpp / .cpp        # Limit evaluation (L'Hôpital, squeeze)
│   ├── optimization/
│   │   └── Optimize.hpp / .cpp      # Critical points, saddle points, Lagrange multipliers
│   └── series/
│       ├── Series.hpp / .cpp        # Taylor, Maclaurin, Fourier series
│       └── Convergence.hpp / .cpp   # Ratio/root/integral convergence tests
│   └── vectorcalc/
│       ├── VectorOps.hpp / .cpp     # Gradient, curl, divergence, Jacobian, Hessian
│       ├── LineIntegral.hpp / .cpp  # Parametric line integrals
│       ├── SurfaceIntegral.hpp/.cpp # Surface integrals with varZ substitution
│       └── Theorems.hpp / .cpp      # Green's, Stokes', Divergence theorems
│
├── ComplexAnalysis/
│   ├── CA.hpp
│   └── CA.cpp                       # Complex functions, residues, contour integrals (26 ops)
│
├── DIscreteMath/
│   ├── DM.hpp
│   └── DM.cpp                       # Graphs, combinatorics, logic, automata (39 ops)
│
├── DiffEq/
│   ├── DE.hpp / .cpp                # ODE solvers: Euler, RK4, Heun, Adams-Bashforth (68 ops)
│   └── DEAdv.cpp                    # PDE methods, Laplace-transform solving
│
├── Geometry/
│   ├── Geom.hpp
│   └── Geom.cpp                     # 2D/3D shapes, dot/cross products, convex hull (50 ops)
│
├── Linear_Algebra/
│   ├── LA.hpp
│   └── LA.cpp                       # Full linear algebra suite (89 ops)
│                                    # det, inverse, RREF, rank, nullity, null space
│                                    # LU, QR, SVD, Jordan, Cholesky decompositions
│                                    # eigenvalues/vectors, characteristic polynomial
│                                    # least-squares, Gram-Schmidt, projections
│                                    # matrix exponential, Cayley-Hamilton
│
├── NumberTheory/
│   ├── NumberTheory.hpp
│   └── NumberTheory.cpp             # Primes, GCD/LCM, factorization, modular arithmetic (44 ops)
│
├── NumericalAnalysis/
│   ├── NA.hpp
│   └── NA.cpp                       # Root-finding, interpolation, numerical diff/int (45 ops)
│                                    # Newton, bisection, secant, regula falsi, Brent
│                                    # Lagrange/Hermite/Newton DD interpolation
│                                    # Cubic/linear spline interpolation
│                                    # Gaussian quadrature (Legendre/Chebyshev/Hermite/Laguerre)
│                                    # FFT / iFFT (iterative radix-2 Cooley-Tukey)
│                                    # Linear/iterative system solvers (Gauss-Seidel, SOR, CG)
│                                    # QR algorithm (numeric eigenvalues)
│
├── ProbabilityTheory/
│   ├── PT.hpp
│   └── PT.cpp                       # Distributions, Markov chains, inequalities (44 ops)
│                                    # Normal, binomial, Poisson, t, chi-square, F
│                                    # Markov chain simulation + steady-state convergence
│                                    # Markov, Chebyshev, Chernoff, Hoeffding inequalities
│                                    # Random walks, martingales, Doob decomposition
│
├── Statistics/
│   ├── Statistics.hpp
│   ├── Statistics.cpp               # Descriptive, inferential, regression (114 ops)
│   ├── Statistics_internal.hpp
│   └── Advanced_Statistics.cpp     # ANOVA, multiple regression, logistic regression
│
├── parser/                          # Week-4 lexer/parser/evaluator (arithmetic fallback)
│   ├── include_week_four/
│   │   ├── lexer.hpp
│   │   ├── parser.hpp
│   │   ├── evaluator.hpp
│   │   └── value.hpp
│   └── src_week_four/
│       ├── lexer.cpp
│       ├── parser.cpp
│       └── evaluator.cpp
│
├── java/com/mathengine/
│   ├── jni/
│   │   └── MathBridge.java          # JNI declarations (native compute/evalExact/getVersion)
│   ├── model/
│   │   └── PrecisionMode.java       # EXACT / APPROXIMATE enum
│   └── ui/
│       ├── MathEngineApp.java        # Application entry point, stage lifecycle
│       ├── Launcher.java             # JavaFX launcher shim
│       ├── MainLayout.java           # Root layout: Compute · Graph · Chat tabs
│       ├── InputPanel.java           # Expression input, operation selector, precision toggle
│       ├── OutputPanel.java          # Result rendering (LaTeX + plain text)
│       ├── GraphPanel.java           # 2D/3D function plotter
│       ├── ChatbotPanel.java         # Chat UI; spawns chatbot.py subprocess
│       ├── OperationPanel.java       # Math-domain tab bar (Calculus, LA, Stats, …)
│       ├── QuickFunctionsPanel.java  # One-click example expressions per operation
│       ├── MathKeyboard.java         # On-screen symbol keyboard
│       ├── LatexRenderer.java        # JLaTeXMath rendering → ImageView
│       ├── CalcExpressionBuilder.java# Converts UI selections to engine expressions
│       ├── DatasetImportPanel.java   # CSV/TSV import for statistics operations
│       ├── LoginScreen.java          # JWT login / register / guest entry
│       ├── AuthService.java          # Token storage and REST auth calls
│       ├── SettingsPanel.java        # Theme, font size, preferences
│       ├── ThemeManager.java         # Light / Dark / High-Contrast / System theme
│       ├── AccessibilityToolbar.java # Font-size controls
│       └── regUser.java              # Register-user form
│
├── server/                          # Spring Boot 3 REST server
│   ├── pom.xml
│   ├── mathengine.db                # SQLite database (auto-created on first run)
│   └── src/main/
│       ├── java/com/mathengine/com/mathengine/server/
│       │   ├── MathEngineServer.java
│       │   ├── api/
│       │   │   ├── AuthController.java      # POST /register, POST /login, GET /me
│       │   │   ├── ComputeController.java   # POST /compute, GET /engine/status
│       │   │   └── HistoryController.java   # GET/DELETE /history
│       │   ├── auth/
│       │   │   ├── JwtService.java          # HS256 token generation/validation
│       │   │   ├── JwtFilter.java           # Spring Security filter
│       │   │   ├── SecurityConfig.java
│       │   │   └── SecurityUtils.java
│       │   ├── db/
│       │   │   └── DatabaseManager.java     # SQLite JDBC connection pool
│       │   ├── engine/
│       │   │   └── ServerEngineService.java # Calls native lib via JNI from server side
│       │   └── model/
│       │       └── Models.java              # Request/response POJOs
│       └── resources/
│           ├── application.properties       # Port 8080, JWT secret, CORS, SQLite path
│           └── static/
│               ├── index.html               # Mobile browser interface
│               ├── mobile.css
│               └── mobile.js
│
├── scripts/
│   ├── build_and_run.ps1            # Full build: CMake → MSVC → Maven → launch
│   ├── Linker_Fixer.ps1             # Resolves MSVC JNI linker path issues
│   ├── update_cmake.ps1             # Regenerates CMakeLists source list
│   └── project_health_report.ps1   # Prints module status and compile diagnostics
│
├── resources/css/
│   ├── base.css                     # Shared styles + chatbot bubble styles
│   ├── dark.css                     # Dark theme overrides
│   ├── light.css                    # Light theme overrides
│   ├── high-contrast.css
│   ├── font-normal.css
│   ├── font-large.css
│   └── font-x-large.css
│
└── include/
    └── com_mathengine_jni_MathBridge.h   # Auto-generated JNI header
```

---

## Math Modules

| Module | Prefix | Ops | Highlights |
|---|---|---|---|
| **Calculus** | `calc:` | 39 | Symbolic/numeric integrals, derivatives, limits, Taylor/Fourier series, vector calculus, Green's/Stokes'/Divergence theorems |
| **Linear Algebra** | `la:` | 89 | LU/QR/SVD/Jordan/Cholesky, eigenvalues, null space, RREF, matrix exponential, Cayley-Hamilton, least squares |
| **Statistics** | `stat:` | 114 | Descriptive stats, hypothesis testing (t/F/chi²/ANOVA), linear/polynomial/logistic/multiple regression, correlation |
| **Differential Equations** | `de:` | 68 | Euler, RK4, Heun, Adams-Bashforth, Adams-Moulton, BDF, Runge-Kutta-Fehlberg; PDE methods |
| **Applied Math** | `am:` | 65 | Laplace / inverse-Laplace, optimization, signal processing |
| **Number Theory** | `nt:` | 44 | Primality testing, factorization, GCD/LCM, Euler's totient, Chinese remainder theorem, modular exponentiation |
| **Numerical Analysis** | `na:` | 45 | Root-finding (Newton/bisection/secant/Brent), interpolation (Lagrange/Hermite/spline), Gaussian quadrature, FFT/iFFT |
| **Probability Theory** | `prob:` | 44 | Normal/Binomial/Poisson/t/chi²/F distributions, Markov chains, concentration inequalities |
| **Geometry** | `geo:` | 50 | 2D/3D shapes, area/volume, dot/cross products, convex hull, projections |
| **Discrete Math** | `dm:` | 39 | Graph algorithms (BFS/DFS/Dijkstra/Kruskal/Prim), combinatorics, logic, FSM |
| **Complex Analysis** | `ca:` | 26 | Complex arithmetic, residues, Laurent series, contour integration |
| **Abstract Algebra** | `aa:` | 45 | Groups, rings, fields, permutation groups, homomorphisms |

**Total: ~668 named operations** across 12 modules.

---

## Expression Syntax

The `Preprocessor` accepts two notation styles:

**Bracket notation** (natural, recommended):
```
integrate[x^2, 0, 5]
derivative of sin(x)*cos(x)
eigenvalues of [[4,1],[2,3]]
spline[0,1,2,3|0,1,4,9|1.5]
fft[1,1,0,-1,-1,-1,0,1]
taylor[e^x, 0, 6]
markov[0.7,0.3;0.4,0.6|0.5,0.5|10]
```

**Prefix notation** (direct engine access):
```
calc:integral   |{"expr":"x^2","var":"x","a":0,"b":5}
la:eigen        |[[4,1],[2,3]]
stat:linear_reg |{"x":"[1,2,3,4]","y":"[2,4,5,8]"}
na:fft          |{"data":"1,1,0,-1,-1,-1,0,1"}
na:bisection    |{"f":"x^2-2","x":"x","a":0,"b":2,"tol":1e-10}
prob:normal_cdf |{"x":1.5,"mu":0,"sigma":1}
```

**Module prefixes:** `calc:` · `la:` · `stat:` · `de:` · `am:` · `nt:` · `na:` · `prob:` · `geo:` · `dm:` · `ca:` · `aa:`

---

## Chatbot

`chatbot.py` is a Python 3 backend launched as a subprocess by `ChatbotPanel.java`. It communicates over stdin/stdout using newline-delimited JSON.

**Features:**

**A — Natural Language Math Processing**
- Translates 40+ intent patterns to engine expressions (integrals, derivatives, limits, eigenvalues, regression, root-finding, distributions, equation solving, systems of equations, and more)
- Asks targeted clarifying questions for incomplete prompts (e.g. "What are the lower and upper bounds?")

**B — Step-by-Step Educational Unpacking**
- Reformats engine output into labelled sections (symbolic result / numeric value / method)
- Newton-Raphson, bisection, and secant produce convergence tables (`iter / x_n / f(x_n) / error`)
- Distributions and regression results include context headers

**C — Conversational Memory & Context**
- Named variable store: `save this as Velocity` → `$Velocity` in future expressions
- Pronoun resolution: "it", "that result", "the previous matrix", "the answer"
- Correction handling: "no, I meant 5 not 3" patches the last intent in place
- Chained requests: "integrate x² from 0 to 3 then take its derivative"
- Graph push: "graph that" sends the last expression to the Graph tab

**D — Smart Safety & Guardrails**
- Division by zero, log/sqrt of invalid arguments, tan at π/2
- Non-square matrices for ops requiring square matrices; singular matrix inversion
- Matrix dimension mismatch for multiplication
- Binomial k > n, negative Poisson λ, probability p outside [0,1]
- Regression x/y length mismatch; empty FFT input

**E — Concept Explanations**
- "Why does Newton-Raphson work?" / "Explain eigenvalues" pulls from a 14-topic knowledge base (integral, derivative, limit, eigenvalues, inverse, determinant, Taylor series, Newton-Raphson, GCD, Fourier, Markov chains, rank, null space, regression, bisection, normal distribution, binomial distribution, FFT)

**F — Unit & Format Conversion**
- Radians ↔ degrees (including symbolic `pi`, `2pi`, `pi/2`)
- Decimal ↔ fraction (`0.75 as a fraction` → `3/4`)
- Base conversion (`255 to binary` → `0b11111111`)

---

## REST API

The Spring Boot server runs on `http://localhost:8080` by default.

| Method | Path | Auth | Description |
|---|---|---|---|
| `POST` | `/api/register` | — | Create account (`{username, password}`) |
| `POST` | `/api/login` | — | Get JWT token (`{username, password}`) |
| `GET` | `/api/me` | JWT | Current user info |
| `POST` | `/api/compute` | JWT | Evaluate an expression (`{expression, precisionFlag}`) |
| `GET` | `/api/engine/status` | — | Native library health check |
| `GET` | `/api/history` | JWT | Paginated computation history |
| `DELETE` | `/api/history` | JWT | Clear all history |
| `DELETE` | `/api/history/{id}` | JWT | Delete one history entry |
| `GET` | `/api/preferences` | JWT | Load saved theme/font preferences |
| `PUT` | `/api/preferences` | JWT | Save theme/font preferences |

JWT tokens expire after 72 hours. The server also serves a minimal mobile browser interface at `http://<host>:8080`.

---

## Custom Data Structures (`utils/`)

All STL containers have been replaced with purpose-built implementations in `utils/`. Include a single header to access all of them:

```cpp
#include "utils/Utils.hpp"
using namespace utils;

// Drop-in replacements — existing code is unchanged:
DVec  data = {1.5, 2.5, 3.5};        // was std::vector<double>
DMat  A(3, 3, 0.0);                   // was std::vector<std::vector<double>>
Complex z(3.0, 4.0);                  // was std::complex<double>
HashMap<std::string, int> freq;       // was std::unordered_map
OrderedMap<int, std::string> labels;  // was std::map
Stack<int> stk;                       // was std::stack
Queue<double> q;                      // was std::queue
Optional<double> result;              // was std::optional
Function<double(double)> fn;          // was std::function (with SBO ≤ 32 bytes)
```

| Class | Replaces | Implementation |
|---|---|---|
| `Vec<T>` | `std::vector<T>` | Heap-allocated contiguous storage, doubling growth |
| `Mat<T>` | `vector<vector<T>>` | Row-major with `rows()`, `cols()`, `transpose()`, `identity()` |
| `Complex` | `std::complex<double>` | Full arithmetic + `sqrt`, `exp`, `log`, `sin`, `cos`, `pow` |
| `HashMap<K,V>` | `std::unordered_map` | Open-addressing, robin-hood probing, 0.7 load factor |
| `OrderedMap<K,V>` | `std::map` | Left-leaning red-black BST, O(log n) |
| `Stack<T>` | `std::stack` | Singly-linked list, O(1) push/pop |
| `Queue<T>` | `std::queue` / `deque` | Circular ring buffer, O(1) amortized |
| `Pair<A,B>` | `std::pair` | C++17 structured-binding support |
| `Tuple3<A,B,C>` | `std::tuple<A,B,C>` | Fixed-arity triple; `GraphEdge` alias predeclared |
| `Optional<T>` | `std::optional` | Aligned in-place storage, no heap allocation |
| `Function<R(Args...)>` | `std::function` | SBO (32-byte threshold); large closures heap-allocated via vtable |

---

## Building and Running

### Prerequisites

| Tool | Version | Purpose |
|---|---|---|
| MSVC (Visual Studio 2022) | 17.x+ | Compile C++ engine (`/std:c++23`) |
| CMake | 3.20+ | Generate MSVC project / Makefile |
| JDK | 21 | Compile and run JavaFX client and Spring Boot server |
| Maven | 3.8+ | Manage Java dependencies |
| Python | 3.10+ | Run chatbot backend |

> The C++ engine targets **Windows only** (MSVC, JNI `.dll`). The Java client and Spring Boot server run cross-platform.

### Quick start

```powershell
# Clone and enter the project
git clone <repo-url>
cd MathEngine

# Full build and launch (compiles C++, builds Java, starts JavaFX app)
.\build.ps1

# Start the REST server separately (needed for history, auth, and chatbot)
.\build.ps1 -StartServer

# Skip C++ recompile (faster iteration on Java changes)
.\build.ps1 -SkipCppBuild
```

### Manual steps

```powershell
# 1. Build C++ engine
cmake -S . -B build -G "Visual Studio 17 2022"
cmake --build build --config Release

# 2. Build and run JavaFX client
mvn -f pom.xml javafx:run

# 3. Build and run Spring Boot server
mvn -f server/pom.xml spring-boot:run
```

### Chatbot

`chatbot.py` is launched automatically by the desktop app when you open the Chat tab. Python 3 must be on your `PATH`. No packages beyond the standard library are required — the chatbot calls the running Spring Boot server at `localhost:8080` for computation.

---

## Bug Fixes Applied

The following issues from the original codebase were resolved:

| ID | Component | Root cause | Fix |
|---|---|---|---|
| ER00001 | `CoreEngine.cpp` | `splitOp(expression,…)` used raw input instead of preprocessed `expr` in `calc:` block → curl/jacobian fell through to week-4 parser | Changed all `splitOp` calls to use `expr` |
| ER00002 | `CoreEngine.cpp` | Same as ER00001 (jacobian) | Same fix |
| ER00003 | `CoreEngine.cpp` | Same as ER00001 (romberg) | Same fix |
| ER00004 | `CoreEngine.cpp` | Same as ER00001 (numerical_int) | Same fix |
| ER00005 | `Calculus.cpp` | `line_integral` never received `paramY`; y was unsubstituted | Added `paramY` to Preprocessor + token-substitution in handler |
| ER00006 | `Calculus.cpp` | `surface_int` ignored `varZ`; z threw "Undefined variable" | Added `varZ` → `(varX+varY)` substitution before double integral |
| ER00007 | `CoreEngine.cpp` | Same root cause as ER00001 (greens) | Same fix |
| ER00008 | `CoreEngine.cpp` | Same root cause as ER00001 (stokes) | Same fix |
| ER00009 | `Preprocessor.cpp` | `saddle:expr` colon syntax not handled | Added colon-syntax handler routing to `calc:optimize_nd` |
| ER00010 | `Preprocessor.cpp` | `gradient_descent:expr` same as ER00009 | Same fix |
| ER00011 | `ProbabilityTheory/PT.cpp` | Markov chain operation not implemented | Added `markovChain()` with step simulation and steady-state convergence |
| ER00012 | `Preprocessor.cpp` | `spline[…]` had no preprocessor handler | Added pipe-argument splitting handler routing to `na:cubic_spline` |
| ER00013 | `Linear_Algebra/LA.cpp` | `la:null` → "Unknown operation: null" | Added `null` alias dispatching to `nullSpace()` |
| ER00014 | `Linear_Algebra/LA.cpp` | `la:qr` → "Unknown operation: qr" | Added `qr` alias |
| ER00015 | `Linear_Algebra/LA.cpp` | `la:lu` → "Unknown operation: lu" | Added `lu` alias |
| ER00016 | `Linear_Algebra/LA.cpp` | `la:svd` → "Unknown operation: svd" | Added `svd` alias |
| ER00017 | `Linear_Algebra/LA.cpp` | `la:eigen` → "Unknown operation: eigen" | Added `eigen` alias |
| ER00018 | `Linear_Algebra/LA.cpp` | `la:inv` → "Unknown operation: inv" | Added `inv` alias |
| B00001 | `QuickFunctionsPanel.java` | "Convex hull" quick button sent `circle[0,0,5]`; FFT sent `romberg[…]` | Fixed both expressions; added `fft[…]` preprocessor handler |
| B00002 | `Linear_Algebra/LA.cpp` | `LAResult::format(exactMode)` returned `numerical` in exact mode and `symbolic` in approximate mode — inverted | Swapped: exact mode now returns `symbolic`, approximate returns both |
| B00003 | `OutputPanel.java` | Plain-text output (e.g. Markov chain state vectors) passed to `LatexRenderer` and displayed incorrectly | Added `looksLikePlainText()` guard before LaTeX rendering |

**Additional fix (discovered during `utils/` compile verification):**
- `decompJordan()` in `LA.cpp` called `computeEigenvalues()` and `computeGeometricMultiplicity()` — neither existed anywhere in the codebase. Rewrote using the existing `eigenQR()` function and a new `numericRank()` helper for geometric multiplicity.

---

## Known Limitations

- **Windows only** for the C++ engine. The JNI `.dll` is compiled with MSVC; Linux/macOS builds are not supported without replacing the CMake toolchain and JNI glue.
- **`decompJordan`** produces correct output for diagonalizable matrices and simple Jordan structures, but block size distribution for defective matrices with higher-order chains is approximate.
- **FFT** operates on real-valued input only (imaginary parts are zero-padded). Complex-input FFT is not yet exposed through the preprocessor.
- **Chatbot** requires Python 3 on `PATH` and the Spring Boot server running at `localhost:8080`. If either is absent, the Compute and Graph tabs remain fully functional.
- **Surface integrals** with a third variable substitute `z = x + y` as the default surface equation when no explicit `z(x,y)` is provided.
