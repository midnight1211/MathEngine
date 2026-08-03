# MathEngine Chatbot

A stdlib-only Python NLP layer that turns natural-language math requests
into the exact command strings `CoreEngine.cpp` already understands, so
it plugs into the existing engine instead of becoming a second one.

```
 User types free text
        │
        ▼
 ChatbotPanel.java (desktop) ──┐
 ChatController.java (server) ─┼──►  chatbot/cli.py  (this module)
                                │        │
                                │        ▼
                                │   engine_input string
                                │   e.g. "diff[x^2,x]"
                                │   e.g. "la:determinant|[[1,2],[3,4]]"
                                │
                                ▼
                    MathBridge.compute() / ServerEngineService.compute()
                                │
                                ▼
                         CoreEngine.cpp (unchanged)
```

The chatbot **never talks to the C++ engine directly**. It only decides
*what command to send*; Java is responsible for actually running it,
which means a chat-originated computation goes through the same JNI
call, the same history logging, and the same auth pipeline as anything
typed into `InputPanel` by hand.

## Why no spaCy/nltk/etc.

The rest of this project builds with a PowerShell script, MSVC, and
Maven — no Python environment is otherwise part of the toolchain. Adding
a pip dependency would make the chatbot the one piece that can't be
built the same offline way as everything else. Math-command phrasing is
also a far more constrained grammar than open-domain text, so
regex + light heuristics (see `entities.py`) cover it well without an ML
dependency.

## Files

| File               | Purpose |
|--------------------|---------|
| `entities.py`      | Regex helpers: pull numbers, expressions, matrix/vector literals out of a sentence; JSON-string escaping. |
| `intents.py`       | The intent registry — pattern → `EngineCommand` builder, one entry per supported phrasing. **This is where you add new capabilities.** |
| `session.py`       | Per-conversation memory (`Session`), so "now integrate **that**" resolves to the last expression discussed. |
| `responses.py`     | Small talk text (greeting/help/thanks/bye) and reply templates. |
| `nlp_engine.py`    | `NLPChatbot.handle(session_id, message)` — the classification pipeline tying the above together. |
| `cli.py`           | Subprocess entry point: line-delimited JSON on stdin/stdout, run by Java via `ProcessBuilder`. |
| `tests/`           | `unittest`-based tests, no dependencies. Run with `python tests/test_chatbot.py` from this directory. |

## Wire protocol

One JSON object per line in, one per line out:

```jsonc
// → stdin
{"session_id": "abc123", "message": "derivative of x^2 + 3x"}

// ← stdout
{
  "session_id": "abc123",
  "reply": "Differentiating x^2 + 3x with respect to x.",
  "engine_input": "diff[x^2 + 3x,x]",
  "precision_flag": 0,
  "intent": "calculus.derivative",
  "confidence": 0.9
}
```

`engine_input` is `null` for small talk (greetings, help, thanks) — the
Java side should just display `reply` and skip calling the engine.

Send `"__exit__"` on its own line to end the subprocess cleanly.

Optionally include `"result": "<previous engine result>"` in a request
to feed the last computed value back in — stored on the session for
future intents to use (see "Extending" below).

## How each side calls it

- **Desktop (JavaFX):** `com.mathengine.chatbot.ChatbotBridge` starts and
  owns the subprocess (hand-rolled JSON reader/writer — the desktop
  module has no JSON library on its classpath). `ChatbotPanel` is a new
  "Chat" tab in `MainLayout` that calls `ChatbotBridge.classify()`, then
  `MathBridge.compute()` for the resulting command, on background
  `Task`s — the same pattern `MainLayout.dispatchSolve()` already uses
  for typed input.
- **Server (Spring Boot):** `com.mathengine.server.chatbot.ChatbotService`
  is the equivalent subprocess manager, using Jackson's `ObjectMapper`
  (already on the classpath via `spring-boot-starter-web`) instead of a
  hand-rolled parser. `ChatController` exposes `POST /api/chat`,
  mirroring `ComputeController`'s structure — same
  `ServerEngineService.compute()` call, same `DatabaseManager.saveHistory()`
  call, same JWT-optional auth handling.

Both are independent Maven modules (the server has no compile-time
dependency on the desktop module), so the subprocess-management code is
intentionally duplicated in parallel rather than shared — see the two
files' headers for the exact mapping between them.

## Command format reference

Two shapes exist, both already understood by `CoreEngine.cpp`:

1. **Preprocessor shorthand** (`Preprocessor.cpp` expands it) — used for
   Calculus, some AppliedMath/Probability/DiscreteMath ops:
   `diff[x^2,x]`, `integrate[sin(x)]`, `definite_int[x^2,0,3]`,
   `taylor[cos(x),0,6]`, `combinations[10,3]`.

2. **Raw wire format** (`prefix:op|payload`) — used for everything else
   (LinearAlgebra, Statistics, NumberTheory, Geometry, ComplexAnalysis,
   NumericalAnalysis, AbstractAlgebra, most of DiffEq), because
   `Preprocessor.cpp` doesn't shorthand these modules:
   - `la:determinant|[[1,2],[3,4]]` — LA takes matrix literals directly,
     pipe-separated for binary ops (`la:solve|[[2,1],[1,3]]|[3,5]`).
   - `stat:mean|{"x":[1,2,3,4]}` — JSON payload, one key per parameter.
   - `nt:gcd|{"a":12,"b":18}`, `geo:distance_2d|{"x1":0,"y1":0,"x2":3,"y2":4}`,
     `de:rk4|{"f":"t+y","tvar":"t","yvar":"y","t0":0,"t1":2,"y0":1,"n":100}`,
     etc.

Every op/param name in `intents.py`'s builders was checked directly
against the corresponding module's `dispatch()` function in the C++
source (not guessed), so these are exact, not approximate.

## Coverage

This ships with **147 intents** spanning every engine module, including a
light touch of AppliedMath (logistic growth, SIR epidemic model,
Lotka-Volterra predator-prey) and Probability Theory (moment generating
functions, gambler's ruin, Chebyshev's inequality) alongside the core
Calculus/Linear Algebra/Statistics/Number Theory/Discrete Math/Geometry/
Complex Analysis/Numerical Analysis/DiffEq/Abstract Algebra coverage
described below:

- **Calculus** — derivative/partial/gradient/integral (definite & indefinite)/
  limit (incl. left/right-handed)/Taylor/optimize/log-derivative/implicit
  differentiation/Hessian/Laplacian/Jacobian/curl/divergence
- **Linear Algebra** — determinant/inverse/transpose/rank/trace/rref/
  eigenvalues/eigenvectors/solve/multiply/add/subtract/power/vector norm/
  dot & cross product/is-symmetric/is-orthogonal/null space/condition
  number/pseudoinverse/LU & QR decomposition
- **Statistics** — mean/median/mode/stddev/variance/pearson/linear
  regression/covariance/Markov steady-state/skewness/kurtosis/percentile/
  standard error/five-number summary/IQR/z-test/one-sample t-test/F-test/
  normal CDF/binomial & Poisson PMF
- **Number Theory** — gcd/lcm/is-prime/prime factors/Fibonacci/mod-pow/
  Euler's totient/extended GCD/divisors/number & sum of divisors/is-perfect/
  modular inverse/next & nth prime/primes up to N/Catalan/Bell/Stirling
  numbers (1st & 2nd kind)
- **Discrete Math** — combinations/permutations/Dijkstra/derangements/BFS/
  connected components/bipartite check/Floyd-Warshall/Prim's MST
- **Geometry** — 2D & 3D distance/triangle area/circle/sphere/midpoint/
  slope/polygon perimeter
- **Complex Analysis** — all roots/complex power, exp, log, sqrt/polar
  form/residue
- **Numerical Analysis** — bisection/Simpson's rule/Newton's method/
  trapezoidal rule
- **Differential Equations** — RK4/Euler's method/RK2/Laplace transform
- **Abstract Algebra** — cyclic & symmetric groups/ring Z/nZ/permutation
  order & inverse
- **Arithmetic passthrough** for anything already in plain expression form

The C++ engine exposes several hundred operations in total across its
modules; this is deliberately breadth-first coverage of the most common
phrasings in each domain, not an exhaustive 1:1 mapping. **Adding
support for another operation is always a small, local change** — see
below.

## Extending

To add a new intent:

1. Confirm the op's exact parameter names by checking the module's
   `dispatch()` function in the corresponding `.cpp` file — don't guess.
2. Add a `build(match, text, session)` function near the other builders
   for that module in `intents.py` (or reuse one of the `_build_*_unary`
   /`_build_*_two_*` factories if the shape matches).
3. Add an `Intent("module.opname", _p(r"...regex..."), build_fn)` entry
   to `INTENTS`.
4. Add a test case to `tests/test_chatbot.py`.

Nothing else in the pipeline needs to change — `nlp_engine.py` iterates
`INTENTS` generically.

Two documented-but-not-yet-wired extension points:

- `Session.last_result` / `NLPChatbot.report_result()` — the plumbing
  to let a future intent reference the *actual computed value* (not
  just the expression) of the previous turn already exists; no intent
  currently reads it.
- `prob:markov` — `Preprocessor.cpp` has shorthand for this, but it
  doesn't route to anything in `PT.cpp` (steady-state Markov chains
  actually live under `stat:markov_steady` in `Statistics.cpp`, which is
  what this chatbot uses instead). Worth fixing in the preprocessor
  itself at some point.

## Running the tests

```bash
cd chatbot
python tests/test_chatbot.py -v
```

No `pip install` required.
