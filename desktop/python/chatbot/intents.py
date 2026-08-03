"""
intents.py
───────────
The intent registry: each Intent has a name, a list of trigger regexes,
and a `build(match, text, session)` function that returns an
`EngineCommand` (the exact string CoreEngine.compute() expects, plus the
precision flag to use).

This mirrors — and where possible reuses the shorthand already accepted
by Preprocessor.cpp (e.g. "diff[x^2,x]", "d/dx[...]") so the chatbot's
output is never a foreign format grafted onto the engine; it either:
  (a) emits the same "opname[args]" shorthand a human would type into
      InputPanel, which CoreEngine's own preprocessor then expands, or
  (b) for modules Preprocessor.cpp doesn't shorthand (LinearAlgebra,
      Statistics, NumberTheory, Geometry, ComplexAnalysis,
      NumericalAnalysis, AbstractAlgebra, most of DiffEq), emits the raw
      "prefix:op|payload" wire format directly, using the exact key
      names each module's dispatch() reads (verified against the C++
      source, not guessed).

Adding a new intent = adding one Intent(...) entry below. Nothing else
in the pipeline needs to change.
"""

from __future__ import annotations

import re
from dataclasses import dataclass, field
from typing import Callable, List, Optional, Match

from entities import (
    jv, jnum, find_numbers, find_matrix_literal, matrix_literal_to_json_rows,
    find_vector_literal, clean_expression, first_var, extract_between,
    split_top_level, extract_expr_list, extract_var_list,
)


@dataclass
class EngineCommand:
    engine_input: str          # string to feed CoreEngine.compute()
    precision_flag: int = 0    # 0 = symbolic, 1 = numerical
    reply: str = ""            # natural-language framing to show the user
    remembers_expr: Optional[str] = None  # what to store as "last expression"


@dataclass
class Intent:
    name: str
    patterns: List[re.Pattern]
    build: Optional[Callable[[Match, str, "Session"], EngineCommand]] = None
    numerical_hint: bool = False   # True if this intent defaults to numerical mode


def _p(*patterns: str) -> List[re.Pattern]:
    return [re.compile(p, re.IGNORECASE) for p in patterns]


# ─────────────────────────────────────────────────────────────────────────
# CALCULUS  (→ Preprocessor.cpp shorthand: "calc:<op>|{json}")
# ─────────────────────────────────────────────────────────────────────────

def _build_derivative(m: Match, text: str, session) -> EngineCommand:
    expr = session.resolve_pronoun(clean_expression(m.group("expr"))) if "expr" in m.groupdict() and m.group("expr") else session.resolve_pronoun(text)
    var = m.group("var") if "var" in m.groupdict() and m.group("var") else first_var(expr)
    return EngineCommand(f"diff[{expr},{var}]", 0,
                          f"Differentiating {expr} with respect to {var}.", expr)


def _build_partial(m: Match, text: str, session) -> EngineCommand:
    expr = clean_expression(m.group("expr"))
    var = m.group("var")
    return EngineCommand(f"partial[{expr},{var}]", 0,
                          f"Taking the partial derivative of {expr} with respect to {var}.", expr)


def _build_gradient(m: Match, text: str, session) -> EngineCommand:
    expr = clean_expression(m.group("expr"))
    return EngineCommand(f"gradient[{expr},x,y]", 0,
                          f"Computing the gradient of {expr}.", expr)


def _build_integral_indef(m: Match, text: str, session) -> EngineCommand:
    expr = session.resolve_pronoun(clean_expression(m.group("expr"))) if m.group("expr") else session.resolve_pronoun(text)
    return EngineCommand(f"integrate[{expr}]", 0,
                          f"Finding the antiderivative of {expr}.", expr)


def _build_integral_def(m: Match, text: str, session) -> EngineCommand:
    expr = clean_expression(m.group("expr"))
    a, b = m.group("a"), m.group("b")
    return EngineCommand(f"definite_int[{expr},{a},{b}]", 0,
                          f"Evaluating the definite integral of {expr} from {a} to {b}.", expr)


def _build_limit(m: Match, text: str, session) -> EngineCommand:
    expr = clean_expression(m.group("expr"))
    var = m.group("var")
    point = m.group("point").strip()
    return EngineCommand(f"{expr} as {var}->{point}", 0,
                          f"Taking the limit of {expr} as {var} approaches {point}.", expr)


def _build_taylor(m: Match, text: str, session) -> EngineCommand:
    expr = clean_expression(m.group("expr"))
    center = m.group("center") or "0"
    order = m.group("order") or "5"
    return EngineCommand(f"taylor[{expr},{center},{order}]", 0,
                          f"Building the Taylor series of {expr} around {center} (order {order}).", expr)


def _build_optimize(m: Match, text: str, session) -> EngineCommand:
    expr = clean_expression(m.group("expr"))
    return EngineCommand(f"optimize[{expr}]", 1,
                          f"Finding critical points of {expr}.", expr)


# ─────────────────────────────────────────────────────────────────────────
# LINEAR ALGEBRA  (→ raw "la:<op>|matrix[|matrix]" — verified against LA.cpp)
# ─────────────────────────────────────────────────────────────────────────

def _mat(text: str, session) -> str:
    lit = find_matrix_literal(text)
    if lit:
        return matrix_literal_to_json_rows(lit)
    if session.last_matrix:
        return session.last_matrix
    return "[[]]"


def _build_la_unary(op: str, verb: str):
    def build(m: Match, text: str, session) -> EngineCommand:
        mat = _mat(text, session)
        session.last_matrix = mat
        numeric = op not in ("determinant", "rank", "trace")
        return EngineCommand(f"la:{op}|{mat}", 1 if numeric else 0,
                              f"{verb} {mat}.", None)
    return build


def _build_la_solve(m: Match, text: str, session) -> EngineCommand:
    mat_lit = find_matrix_literal(text)
    vec_lit = find_vector_literal(text[text.find(mat_lit) + len(mat_lit):]) if mat_lit else None
    A = matrix_literal_to_json_rows(mat_lit) if mat_lit else (session.last_matrix or "[[]]")
    b = vec_lit if vec_lit else "[]"
    session.last_matrix = A
    return EngineCommand(f"la:solve|{A}|{b}", 1,
                          f"Solving the linear system A x = b for A={A}, b={b}.", None)


def _build_la_multiply(m: Match, text: str, session) -> EngineCommand:
    lits = []
    remaining = text
    for _ in range(2):
        lit = find_matrix_literal(remaining)
        if not lit:
            break
        lits.append(lit)
        remaining = remaining[remaining.find(lit) + len(lit):]
    if len(lits) < 2:
        A = session.last_matrix or "[[]]"
        B = lits[0] if lits else "[[]]"
    else:
        A, B = lits[0], lits[1]
    A = matrix_literal_to_json_rows(A)
    B = matrix_literal_to_json_rows(B)
    session.last_matrix = A
    return EngineCommand(f"la:matrix_multiply|{A}|{B}", 1,
                          "Multiplying the two matrices.", None)


# ─────────────────────────────────────────────────────────────────────────
# STATISTICS  (→ raw "stat:<op>|{json}" — verified against Statistics.cpp)
# ─────────────────────────────────────────────────────────────────────────

def _vec_json(text: str, session, key: str = "x") -> str:
    lit = find_vector_literal(text) or find_matrix_literal(text)
    if lit:
        # Strip outer [] once if doubled, else use as-is
        inner = lit
        session.last_dataset = inner
        return inner
    return session.last_dataset or "[]"


def _build_stat_unary(op: str, verb: str):
    def build(m: Match, text: str, session) -> EngineCommand:
        x = _vec_json(text, session)
        return EngineCommand(f'stat:{op}|{{"x":{x}}}', 1, f"{verb} {x}.", None)
    return build


def _build_stat_two_sample(op: str, verb: str):
    def build(m: Match, text: str, session) -> EngineCommand:
        # Expect two bracketed lists: [1,2,3] and [4,5,6]
        text_after_first = text
        lists = []
        remaining = text
        for _ in range(2):
            lit = find_vector_literal(remaining)
            if not lit:
                break
            lists.append(lit)
            idx = remaining.find(lit)
            remaining = remaining[idx + len(lit):]
        x = lists[0] if len(lists) > 0 else (session.last_dataset or "[]")
        y = lists[1] if len(lists) > 1 else "[]"
        session.last_dataset = x
        return EngineCommand(f'stat:{op}|{{"x":{x},"y":{y}}}', 1, f"{verb}.", None)
    return build


# ─────────────────────────────────────────────────────────────────────────
# NUMBER THEORY  (→ raw "nt:<op>|{json}" — verified against NumberTheory.cpp)
# ─────────────────────────────────────────────────────────────────────────

def _build_nt_two_int(op: str, key_a: str, key_b: str, verb: str):
    def build(m: Match, text: str, session) -> EngineCommand:
        nums = find_numbers(text)
        a = nums[0] if len(nums) > 0 else "0"
        b = nums[1] if len(nums) > 1 else "0"
        return EngineCommand(f'nt:{op}|{{"{key_a}":{jnum(a)},"{key_b}":{jnum(b)}}}', 0,
                              f"{verb} {a} and {b}.", None)
    return build


def _build_nt_one_int(op: str, key: str, verb: str):
    def build(m: Match, text: str, session) -> EngineCommand:
        nums = find_numbers(text)
        n = nums[0] if nums else "0"
        return EngineCommand(f'nt:{op}|{{"{key}":{jnum(n)}}}', 0, f"{verb} {n}.", None)
    return build


# ─────────────────────────────────────────────────────────────────────────
# DISCRETE MATH  (→ Preprocessor shorthand for combinations/permutations;
#                    raw "dm:<op>|{json}" for graph ops)
# ─────────────────────────────────────────────────────────────────────────

def _build_dm_choose(op: str):
    def build(m: Match, text: str, session) -> EngineCommand:
        nums = find_numbers(text)
        n = nums[0] if len(nums) > 0 else "0"
        r = nums[1] if len(nums) > 1 else "0"
        return EngineCommand(f"{op}[{n},{r}]", 0, f"Computing {op}({n}, {r}).", None)
    return build


def _build_dm_dijkstra(m: Match, text: str, session) -> EngineCommand:
    lit = find_matrix_literal(text)
    adj = matrix_literal_to_json_rows(lit) if lit else (session.last_matrix or "[[]]")
    nums = find_numbers(text[text.find(lit) + len(lit):]) if lit else find_numbers(text)
    src = nums[0] if nums else "0"
    session.last_matrix = adj
    return EngineCommand(f'dm:dijkstra|{{"adj":{adj},"src":{jnum(src)}}}', 1,
                          f"Running Dijkstra's algorithm from node {src}.", None)


# ─────────────────────────────────────────────────────────────────────────
# GEOMETRY  (→ raw "geo:<op>|{json}")
# ─────────────────────────────────────────────────────────────────────────

def _build_geo_distance(m: Match, text: str, session) -> EngineCommand:
    nums = find_numbers(text)
    if len(nums) < 4:
        nums = (nums + ["0", "0", "0", "0"])[:4]
    x1, y1, x2, y2 = nums[:4]
    return EngineCommand(
        f'geo:distance_2d|{{"x1":{jnum(x1)},"y1":{jnum(y1)},"x2":{jnum(x2)},"y2":{jnum(y2)}}}',
        1, f"Finding the distance between ({x1}, {y1}) and ({x2}, {y2}).", None)


def _build_geo_triangle_area(m: Match, text: str, session) -> EngineCommand:
    nums = find_numbers(text)
    nums = (nums + ["0"] * 6)[:6]
    x1, y1, x2, y2, x3, y3 = nums
    return EngineCommand(
        f'geo:triangle_area|{{"x1":{jnum(x1)},"y1":{jnum(y1)},"x2":{jnum(x2)},'
        f'"y2":{jnum(y2)},"x3":{jnum(x3)},"y3":{jnum(y3)}}}',
        1, "Computing the triangle's area.", None)


def _build_geo_circle(m: Match, text: str, session) -> EngineCommand:
    nums = find_numbers(text)
    nums = (nums + ["0", "0", "1"])[:3]
    cx, cy, r = nums
    return EngineCommand(f'geo:circle|{{"cx":{jnum(cx)},"cy":{jnum(cy)},"r":{jnum(r)}}}', 0,
                          f"Building the circle equation for center ({cx}, {cy}), radius {r}.", None)


# ─────────────────────────────────────────────────────────────────────────
# COMPLEX ANALYSIS (→ raw "ca:<op>|{json}")
# ─────────────────────────────────────────────────────────────────────────

def _build_ca_roots(m: Match, text: str, session) -> EngineCommand:
    nums = find_numbers(text)
    re_, im = (nums + ["0", "0"])[:2]
    n = m.group("n") if "n" in m.groupdict() and m.group("n") else (nums[2] if len(nums) > 2 else "2")
    return EngineCommand(f'ca:all_roots|{{"re":{jnum(re_)},"im":{jnum(im)},"n":{jnum(n)}}}', 1,
                          f"Finding all {n}th roots of {re_} + {im}i.", None)


# ─────────────────────────────────────────────────────────────────────────
# NUMERICAL ANALYSIS (→ raw "na:<op>|{json}")
# ─────────────────────────────────────────────────────────────────────────

def _build_na_bisection(m: Match, text: str, session) -> EngineCommand:
    expr = clean_expression(m.group("expr"))
    var = first_var(expr)
    nums = find_numbers(m.group("range") if "range" in m.groupdict() and m.group("range") else text)
    a, b = (nums + ["0", "1"])[:2]
    return EngineCommand(
        f'na:bisection|{{"f":{jv(expr)},"x":{jv(var)},"a":{jnum(a)},"b":{jnum(b)},"tol":1e-10,"maxIter":100}}',
        1, f"Finding a root of {expr} on [{a}, {b}] with bisection.", expr)


def _build_na_simpsons(m: Match, text: str, session) -> EngineCommand:
    expr = clean_expression(m.group("expr"))
    var = first_var(expr)
    a, b = m.group("a"), m.group("b")
    return EngineCommand(
        f'na:simpsons|{{"f":{jv(expr)},"x":{jv(var)},"a":{jnum(a)},"b":{jnum(b)},"n":100}}',
        1, f"Numerically integrating {expr} from {a} to {b} (Simpson's rule).", expr)


# ─────────────────────────────────────────────────────────────────────────
# DIFFERENTIAL EQUATIONS  (→ raw "de:<op>|{json}")
# ─────────────────────────────────────────────────────────────────────────

def _build_de_rk4(m: Match, text: str, session) -> EngineCommand:
    expr = clean_expression(m.group("expr"))
    y0 = m.group("y0") or "1"
    t1 = m.group("t1") or "1"
    return EngineCommand(
        f'de:rk4|{{"f":{jv(expr)},"tvar":"t","yvar":"y","t0":0,"t1":{jnum(t1)},"y0":{jnum(y0)},"n":100}}',
        1, f"Solving dy/dt = {expr} numerically (RK4) from y(0)={y0} to t={t1}.", expr)


def _build_de_laplace(m: Match, text: str, session) -> EngineCommand:
    expr = clean_expression(m.group("expr"))
    return EngineCommand(f'de:laplace|{{"f":{jv(expr)},"t":"t","s":"s"}}', 0,
                          f"Taking the Laplace transform of {expr}.", expr)


# ─────────────────────────────────────────────────────────────────────────
# ABSTRACT ALGEBRA (→ raw "aa:<op>|{json}")
# ─────────────────────────────────────────────────────────────────────────

def _build_aa_cyclic(m: Match, text: str, session) -> EngineCommand:
    nums = find_numbers(text)
    n = nums[0] if nums else "1"
    return EngineCommand(f'aa:cyclic|{{"n":{jnum(n)}}}', 0, f"Building the cyclic group of order {n}.", None)


def _build_aa_symmetric(m: Match, text: str, session) -> EngineCommand:
    nums = find_numbers(text)
    n = nums[0] if nums else "1"
    return EngineCommand(f'aa:symmetric|{{"n":{jnum(n)}}}', 0, f"Building the symmetric group S{n}.", None)


# ─────────────────────────────────────────────────────────────────────────
# PROBABILITY (steady-state Markov lives under stat: — verified against
# Statistics.cpp; the "prob:markov" shorthand in Preprocessor.cpp doesn't
# route anywhere in PT.cpp, so it's intentionally not used here)
# ─────────────────────────────────────────────────────────────────────────

def _build_markov_steady(m: Match, text: str, session) -> EngineCommand:
    lit = find_matrix_literal(text)
    T = matrix_literal_to_json_rows(lit) if lit else (session.last_matrix or "[[]]")
    session.last_matrix = T
    return EngineCommand(f'stat:markov_steady|{{"T":{T}}}', 1,
                          "Finding the steady-state distribution of the Markov chain.", None)


def _build_stat_vec_nums(op: str, keys: List[str], verb: str, defaults: Optional[List[str]] = None):
    """x-vector plus trailing scalar params (alpha, window, lag, etc.)."""
    def build(m: Match, text: str, session) -> EngineCommand:
        x = _vec_json(text, session)
        rest = text[text.find(x) + len(x):] if x in text else text
        nums = find_numbers(rest)
        defs = defaults or (["0"] * len(keys))
        vals = list(nums[:len(keys)])
        if len(vals) < len(keys):
            vals += defs[len(vals):len(keys)]
        pairs = ",".join(f'"{k}":{jnum(v)}' for k, v in zip(keys, vals))
        extra = ("," + pairs) if pairs else ""
        return EngineCommand(f'stat:{op}|{{"x":{x}{extra}}}', 1, f"{verb}.", None)
    return build


def _build_stat_two_sample_nums(op: str, keys: List[str], verb: str, defaults: Optional[List[str]] = None):
    """Two vectors x,y plus trailing scalar params."""
    def build(m: Match, text: str, session) -> EngineCommand:
        lists = []
        remaining = text
        for _ in range(2):
            lit = find_vector_literal(remaining)
            if lit:
                lists.append(lit)
                remaining = remaining[remaining.find(lit) + len(lit):]
        x = lists[0] if len(lists) > 0 else (session.last_dataset or "[]")
        y = lists[1] if len(lists) > 1 else "[]"
        nums = find_numbers(remaining)
        defs = defaults or (["0"] * len(keys))
        vals = list(nums[:len(keys)])
        if len(vals) < len(keys):
            vals += defs[len(vals):len(keys)]
        pairs = ",".join(f'"{k}":{jnum(v)}' for k, v in zip(keys, vals))
        extra = ("," + pairs) if pairs else ""
        session.last_dataset = x
        return EngineCommand(f'stat:{op}|{{"x":{x},"y":{y}{extra}}}', 1, f"{verb}.", None)
    return build


def _build_stat_table(op: str, verb: str, key: str = "table"):
    """A single contingency-table matrix literal."""
    def build(m: Match, text: str, session) -> EngineCommand:
        lit = find_matrix_literal(text)
        table = matrix_literal_to_json_rows(lit) if lit else "[[]]"
        return EngineCommand(f'stat:{op}|{{"{key}":{table}}}', 1, f"{verb}.", None)
    return build


def _build_stat_groups(op: str, verb: str):
    """ANOVA-family ops: several groups, given as rows of one matrix
    literal, e.g. "[[5,6,7],[8,9,10],[6,7,8]]" — one row per group."""
    def build(m: Match, text: str, session) -> EngineCommand:
        lit = find_matrix_literal(text)
        groups = matrix_literal_to_json_rows(lit) if lit else "[[]]"
        return EngineCommand(f'stat:{op}|{{"groups":{groups}}}', 1, f"{verb}.", None)
    return build


def _build_stat_reg_diag(op: str, verb: str):
    """Regression-diagnostic ops: a design matrix X and response vector y."""
    def build(m: Match, text: str, session) -> EngineCommand:
        lit = find_matrix_literal(text)
        X = matrix_literal_to_json_rows(lit) if lit else (session.last_matrix or "[[]]")
        remaining = text[text.find(lit) + len(lit):] if lit else text
        y = find_vector_literal(remaining) or "[]"
        return EngineCommand(f'stat:{op}|{{"X":{X},"y":{y}}}', 1, f"{verb}.", None)
    return build


# ─────────────────────────────────────────────────────────────────────────
# ARITHMETIC PASSTHROUGH — no NL keyword matched; if it already looks like
# a plain math expression, hand it straight to the week_4 pipeline.
# ─────────────────────────────────────────────────────────────────────────

PLAIN_EXPR_RE = re.compile(r"^[0-9a-zA-Z_+\-*/^().,\s!%]+$")
_HAS_OPERATOR_OR_DIGIT = re.compile(r"[\d+\-*/^()!%]")


def looks_like_plain_expression(text: str) -> bool:
    """True for things InputPanel would already accept as-is: "2^8 + sqrt(16)",
    "sin(pi/2)", a bare variable. False for ordinary English sentences that
    happen to avoid punctuation, e.g. "please do the thing with the stuff" —
    those have no digits/operators and more than a couple of words, so they
    read as prose rather than a formula."""
    t = text.strip()
    if not t or not PLAIN_EXPR_RE.fullmatch(t):
        return False
    if _HAS_OPERATOR_OR_DIGIT.search(t):
        return True
    return len(t.split()) <= 2


# ─────────────────────────────────────────────────────────────────────────
# EXPANDED COVERAGE (batch 2) — additional operations across every module,
# verified against each module's dispatch() the same way as the first batch.
# ─────────────────────────────────────────────────────────────────────────

# ── Calculus: raw "calc:<op>|{json}" (Preprocessor.cpp doesn't shorthand
#    these, so we build the wire format directly rather than relying on it) ──

def _build_calc_log_diff(m: Match, text: str, session) -> EngineCommand:
    expr = clean_expression(m.group("expr"))
    var = first_var(expr)
    return EngineCommand(f'calc:log_diff|{{"expr":{jv(expr)},"var":{jv(var)}}}', 0,
                          f"Taking the logarithmic derivative of {expr}.", expr)


def _build_calc_implicit_diff(m: Match, text: str, session) -> EngineCommand:
    expr = clean_expression(m.group("expr"))
    indep = m.group("indep") if "indep" in m.groupdict() and m.group("indep") else "x"
    dep = m.group("dep") if "dep" in m.groupdict() and m.group("dep") else "y"
    return EngineCommand(
        f'calc:implicit_diff|{{"expr":{jv(expr)},"indep":{jv(indep)},"dep":{jv(dep)},"order":"1"}}',
        0, f"Implicitly differentiating {expr} (treating {dep} as a function of {indep}).", expr)


def _build_calc_hessian(m: Match, text: str, session) -> EngineCommand:
    expr = clean_expression(m.group("expr"))
    vars_ = extract_var_list(text)
    vars_json = "[" + ",".join(jv(v) for v in vars_) + "]"
    return EngineCommand(f'calc:hessian|{{"expr":{jv(expr)},"vars":{vars_json}}}', 0,
                          f"Computing the Hessian of {expr}.", expr)


def _build_calc_laplacian(m: Match, text: str, session) -> EngineCommand:
    expr = clean_expression(m.group("expr"))
    vars_ = extract_var_list(text)
    vars_json = "[" + ",".join(jv(v) for v in vars_) + "]"
    return EngineCommand(f'calc:laplacian|{{"expr":{jv(expr)},"vars":{vars_json}}}', 0,
                          f"Computing the Laplacian of {expr}.", expr)


def _build_calc_vector_field_op(op: str, verb: str):
    def build(m: Match, text: str, session) -> EngineCommand:
        exprs = extract_expr_list(text) or (session.last_matrix and [session.last_matrix]) or ["0", "0", "0"]
        vars_ = extract_var_list(text)
        exprs_json = "[" + ",".join(jv(e) for e in exprs) + "]"
        vars_json = "[" + ",".join(jv(v) for v in vars_) + "]"
        return EngineCommand(f'calc:{op}|{{"exprs":{exprs_json},"vars":{vars_json}}}', 0,
                              f"{verb} the vector field {exprs}.", None)
    return build


def _build_calc_jacobian(m: Match, text: str, session) -> EngineCommand:
    exprs = extract_expr_list(text) or ["0", "0"]
    vars_ = extract_var_list(text)
    exprs_json = "[" + ",".join(jv(e) for e in exprs) + "]"
    vars_json = "[" + ",".join(jv(v) for v in vars_) + "]"
    return EngineCommand(f'calc:jacobian|{{"exprs":{exprs_json},"vars":{vars_json}}}', 0,
                          "Computing the Jacobian matrix.", None)


def _build_calc_limit_directed(op: str, verb: str):
    def build(m: Match, text: str, session) -> EngineCommand:
        expr = clean_expression(m.group("expr"))
        var = m.group("var") if "var" in m.groupdict() and m.group("var") else first_var(expr)
        return EngineCommand(f'calc:{op}|{{"expr":{jv(expr)},"var":{jv(var)}}}', 0,
                              f"{verb} of {expr}.", expr)
    return build


# ── Linear Algebra: raw "la:<op>|A[|B]" (same pipe-delimited style as the
#    first batch's determinant/inverse/solve) ──

def _build_la_binary(op: str, verb: str, second_is_scalar: bool = False):
    def build(m: Match, text: str, session) -> EngineCommand:
        lits = []
        remaining = text
        for _ in range(2):
            lit = find_matrix_literal(remaining) if not second_is_scalar or not lits else None
            if lit:
                lits.append(lit)
                remaining = remaining[remaining.find(lit) + len(lit):]
        if second_is_scalar:
            mat = lits[0] if lits else (session.last_matrix or "[[]]")
            mat = matrix_literal_to_json_rows(mat)
            nums = find_numbers(text)
            scalar = nums[-1] if nums else "1"
            session.last_matrix = mat
            return EngineCommand(f"la:{op}|{mat}|{scalar}", 1, f"{verb}.", None)
        A = matrix_literal_to_json_rows(lits[0]) if len(lits) > 0 else (session.last_matrix or "[[]]")
        B = matrix_literal_to_json_rows(lits[1]) if len(lits) > 1 else "[[]]"
        session.last_matrix = A
        return EngineCommand(f"la:{op}|{A}|{B}", 1, f"{verb}.", None)
    return build


def _build_la_vector_binary(op: str, verb: str):
    def build(m: Match, text: str, session) -> EngineCommand:
        vecs = []
        remaining = text
        for _ in range(2):
            lit = find_vector_literal(remaining)
            if lit:
                vecs.append(lit)
                remaining = remaining[remaining.find(lit) + len(lit):]
        u = vecs[0] if len(vecs) > 0 else "[]"
        v = vecs[1] if len(vecs) > 1 else "[]"
        return EngineCommand(f"la:{op}|{u}|{v}", 1, f"{verb} {u} and {v}.", None)
    return build


def _build_la_vector_norm(m: Match, text: str, session) -> EngineCommand:
    vec = find_vector_literal(text) or "[]"
    nums = find_numbers(text[text.find(vec) + len(vec):]) if vec != "[]" else []
    p = nums[0] if nums else "2"
    return EngineCommand(f"la:vector_norm|{vec}|{p}", 1, f"Computing the p={p} norm of {vec}.", None)


def _build_la_positional(op: str, arg_types: List[str], verb: str):
    """Generic LA builder: pulls len(arg_types) arguments (each 'matrix',
    'vector', or 'scalar') out of the message in order and joins them with
    '|', matching LA.cpp's dispatch(op, input) which just does
    input.split('|') positionally — this covers the ~50 LA ops that don't
    need a bespoke extractor."""
    def build(m: Match, text: str, session) -> EngineCommand:
        args = []
        remaining = text
        for t in arg_types:
            if t == "matrix":
                lit = find_matrix_literal(remaining)
                if lit:
                    args.append(matrix_literal_to_json_rows(lit))
                    remaining = remaining[remaining.find(lit) + len(lit):]
                else:
                    args.append(session.last_matrix or "[[]]")
            elif t == "vector":
                lit = find_vector_literal(remaining)
                if lit:
                    args.append(lit)
                    remaining = remaining[remaining.find(lit) + len(lit):]
                else:
                    args.append("[]")
            else:  # scalar
                nums = find_numbers(remaining)
                if nums:
                    val = nums[0]
                    idx = remaining.find(val)
                    args.append(val)
                    remaining = remaining[idx + len(val):] if idx >= 0 else remaining
                else:
                    args.append("1")
        if arg_types and arg_types[0] == "matrix":
            session.last_matrix = args[0]
        return EngineCommand(f"la:{op}|" + "|".join(args), 1, f"{verb}.", None)
    return build


# ── Statistics: raw "stat:<op>|{json}" ──

def _build_stat_percentile(m: Match, text: str, session) -> EngineCommand:
    x = _vec_json(text, session)
    nums = find_numbers(text[text.find(x) + len(x):]) if x in text else find_numbers(text)
    p = nums[-1] if nums else "50"
    return EngineCommand(f'stat:percentile|{{"x":{x},"p":{jnum(p)}}}', 1,
                          f"Finding the {p}th percentile of {x}.", None)


def _build_stat_z_test(m: Match, text: str, session) -> EngineCommand:
    x = _vec_json(text, session)
    nums = find_numbers(text)
    mu0 = nums[-1] if nums else "0"
    return EngineCommand(f'stat:z_test|{{"x":{x},"mu0":{jnum(mu0)},"sigma":1,"alpha":0.05}}', 1,
                          f"Running a one-sample z-test on {x} against mu0={mu0}.", None)


def _build_stat_t_test_one(m: Match, text: str, session) -> EngineCommand:
    x = _vec_json(text, session)
    nums = find_numbers(text)
    mu0 = nums[-1] if nums else "0"
    return EngineCommand(f'stat:t_test_one|{{"x":{x},"mu0":{jnum(mu0)},"alpha":0.05}}', 1,
                          f"Running a one-sample t-test on {x} against mu0={mu0}.", None)


def _build_stat_normal_cdf(m: Match, text: str, session) -> EngineCommand:
    nums = find_numbers(text)
    nums = (nums + ["0", "0", "1"])[:3]
    x, mu, sigma = nums
    return EngineCommand(f'stat:normal_cdf|{{"x":{jnum(x)},"mu":{jnum(mu)},"sigma":{jnum(sigma)}}}', 1,
                          f"Computing the normal CDF at x={x} (mu={mu}, sigma={sigma}).", None)


def _build_stat_binomial_pmf(m: Match, text: str, session) -> EngineCommand:
    nums = find_numbers(text)
    nums = (nums + ["0", "0", "0.5"])[:3]
    k, n, p = nums
    return EngineCommand(f'stat:binomial_pmf|{{"k":{jnum(k)},"n":{jnum(n)},"p":{jnum(p)}}}', 1,
                          f"Computing P(X={k}) for Binomial(n={n}, p={p}).", None)


def _build_stat_poisson_pmf(m: Match, text: str, session) -> EngineCommand:
    nums = find_numbers(text)
    nums = (nums + ["0", "1"])[:2]
    k, lam = nums
    return EngineCommand(f'stat:poisson_pmf|{{"k":{jnum(k)},"lambda":{jnum(lam)}}}', 1,
                          f"Computing P(X={k}) for Poisson(lambda={lam}).", None)


# ── Number Theory: raw "nt:<op>|{json}" ──

def _build_nt_crt_style_two_int(op: str, key_a: str, key_b: str, verb: str):
    return _build_nt_two_int(op, key_a, key_b, verb)  # alias for clarity below


# ── Discrete Math: raw "dm:<op>|{json}" ──

def _build_dm_graph_unary(op: str, verb: str):
    def build(m: Match, text: str, session) -> EngineCommand:
        lit = find_matrix_literal(text)
        adj = matrix_literal_to_json_rows(lit) if lit else (session.last_matrix or "[[]]")
        session.last_matrix = adj
        return EngineCommand(f'dm:{op}|{{"adj":{adj}}}', 1, f"{verb}.", None)
    return build


def _build_dm_bfs(m: Match, text: str, session) -> EngineCommand:
    lit = find_matrix_literal(text)
    adj = matrix_literal_to_json_rows(lit) if lit else (session.last_matrix or "[[]]")
    nums = find_numbers(text[text.find(lit) + len(lit):]) if lit else find_numbers(text)
    start = nums[0] if nums else "0"
    session.last_matrix = adj
    return EngineCommand(f'dm:bfs|{{"adj":{adj},"start":{jnum(start)}}}', 1,
                          f"Running BFS from node {start}.", None)


def _build_dm_graph_start(op: str, verb: str):
    """Graph op needing adj matrix + a start node, e.g. dfs."""
    def build(m: Match, text: str, session) -> EngineCommand:
        lit = find_matrix_literal(text)
        adj = matrix_literal_to_json_rows(lit) if lit else (session.last_matrix or "[[]]")
        rest = text[text.find(lit) + len(lit):] if lit else text
        nums = find_numbers(rest)
        start = nums[0] if nums else "0"
        session.last_matrix = adj
        return EngineCommand(f'dm:{op}|{{"adj":{adj},"start":{jnum(start)}}}', 1,
                              f"{verb} from node {start}.", None)
    return build


# ── Geometry: raw "geo:<op>|{json}" ──

def _build_geo_distance_3d(m: Match, text: str, session) -> EngineCommand:
    nums = find_numbers(text)
    nums = (nums + ["0"] * 6)[:6]
    x1, y1, z1, x2, y2, z2 = nums
    return EngineCommand(
        f'geo:distance_3d|{{"x1":{jnum(x1)},"y1":{jnum(y1)},"z1":{jnum(z1)},'
        f'"x2":{jnum(x2)},"y2":{jnum(y2)},"z2":{jnum(z2)}}}',
        1, f"Finding the distance between ({x1},{y1},{z1}) and ({x2},{y2},{z2}).", None)


def _build_geo_midpoint(m: Match, text: str, session) -> EngineCommand:
    nums = find_numbers(text)
    nums = (nums + ["0"] * 4)[:4]
    x1, y1, x2, y2 = nums
    return EngineCommand(
        f'geo:midpoint_2d|{{"x1":{jnum(x1)},"y1":{jnum(y1)},"x2":{jnum(x2)},"y2":{jnum(y2)}}}',
        1, f"Finding the midpoint of ({x1},{y1}) and ({x2},{y2}).", None)


def _build_geo_slope(m: Match, text: str, session) -> EngineCommand:
    nums = find_numbers(text)
    nums = (nums + ["0"] * 4)[:4]
    x1, y1, x2, y2 = nums
    return EngineCommand(
        f'geo:slope|{{"x1":{jnum(x1)},"y1":{jnum(y1)},"x2":{jnum(x2)},"y2":{jnum(y2)}}}',
        1, f"Finding the slope between ({x1},{y1}) and ({x2},{y2}).", None)


def _build_geo_sphere(m: Match, text: str, session) -> EngineCommand:
    nums = find_numbers(text)
    nums = (nums + ["0", "0", "0", "1"])[:4]
    cx, cy, cz, r = nums
    return EngineCommand(
        f'geo:sphere|{{"cx":{jnum(cx)},"cy":{jnum(cy)},"cz":{jnum(cz)},"r":{jnum(r)}}}',
        0, f"Building the sphere equation for center ({cx},{cy},{cz}), radius {r}.", None)


# ── Complex Analysis: raw "ca:<op>|{json}" ──

def _build_ca_binary_re_im(op: str, verb: str):
    def build(m: Match, text: str, session) -> EngineCommand:
        nums = find_numbers(text)
        re_, im = (nums + ["0", "0"])[:2]
        return EngineCommand(f'ca:{op}|{{"re":{jnum(re_)},"im":{jnum(im)}}}', 1,
                              f"{verb} {re_} + {im}i.", None)
    return build


def _build_ca_complex_pow(m: Match, text: str, session) -> EngineCommand:
    nums = find_numbers(text)
    nums = (nums + ["0", "0", "2"])[:3]
    re_, im, n = nums
    return EngineCommand(f'ca:complex_pow|{{"re":{jnum(re_)},"im":{jnum(im)},"n":{jnum(n)}}}', 1,
                          f"Raising {re_} + {im}i to the power {n}.", None)


def _build_ca_residue(m: Match, text: str, session) -> EngineCommand:
    expr = clean_expression(m.group("expr"))
    nums = find_numbers(text[text.find(m.group("expr")) + len(m.group("expr")):])
    re_, im = (nums + ["0", "0"])[:2]
    return EngineCommand(f'ca:residue|{{"f":{jv(expr)},"re":{jnum(re_)},"im":{jnum(im)},"order":1}}', 1,
                          f"Finding the residue of {expr} at {re_} + {im}i.", expr)


# ── Numerical Analysis: raw "na:<op>|{json}" ──

def _build_na_newton(m: Match, text: str, session) -> EngineCommand:
    expr = clean_expression(m.group("expr"))
    var = first_var(expr)
    x0 = m.group("x0") if "x0" in m.groupdict() and m.group("x0") else "1"
    return EngineCommand(
        f'na:newton|{{"f":{jv(expr)},"x":{jv(var)},"x0":{jnum(x0)},"tol":1e-10,"maxIter":100}}',
        1, f"Running Newton's method on {expr} starting from x0={x0}.", expr)


def _build_na_trapezoidal(m: Match, text: str, session) -> EngineCommand:
    expr = clean_expression(m.group("expr"))
    var = first_var(expr)
    a, b = m.group("a"), m.group("b")
    return EngineCommand(
        f'na:trapezoidal|{{"f":{jv(expr)},"x":{jv(var)},"a":{jnum(a)},"b":{jnum(b)},"n":100}}',
        1, f"Numerically integrating {expr} from {a} to {b} (trapezoidal rule).", expr)


# ── Differential Equations: rk4 builder factory reused for euler/rk2 ──

def _build_de_numeric_method(op: str, label: str):
    def build(m: Match, text: str, session) -> EngineCommand:
        expr = clean_expression(m.group("expr"))
        y0 = m.group("y0") or "1"
        t1 = m.group("t1") or "1"
        return EngineCommand(
            f'de:{op}|{{"f":{jv(expr)},"tvar":"t","yvar":"y","t0":0,"t1":{jnum(t1)},"y0":{jnum(y0)},"n":100}}',
            1, f"Solving dy/dt = {expr} numerically ({label}) from y(0)={y0} to t={t1}.", expr)
    return build


# ── Abstract Algebra: raw "aa:<op>|{json}" ──

def _build_aa_ring_zn(m: Match, text: str, session) -> EngineCommand:
    nums = find_numbers(text)
    n = nums[0] if nums else "1"
    return EngineCommand(f'aa:ring_zn|{{"n":{jnum(n)}}}', 0, f"Building the ring Z/{n}Z.", None)


def _build_aa_perm_unary(op: str, verb: str):
    def build(m: Match, text: str, session) -> EngineCommand:
        vec = find_vector_literal(text) or "[]"
        return EngineCommand(f'aa:{op}|{{"perm":{vec}}}', 0, f"{verb} the permutation {vec}.", None)
    return build


# ─────────────────────────────────────────────────────────────────────────
# EXPANDED COVERAGE (batch 3) — generic numeric-params factory + a handful
# of graph/vector ops that don't fit that shape.
# ─────────────────────────────────────────────────────────────────────────

def _build_nums(prefix: str, op: str, keys: List[str], verb: str, defaults: Optional[List[str]] = None):
    """Generic builder: pulls len(keys) numbers from the message in order
    and maps them positionally onto the given JSON key names. Missing
    trailing values are filled from `defaults` at the matching position
    (not just appended), so partial input still lines up with the right key."""
    def build(m: Match, text: str, session) -> EngineCommand:
        nums = find_numbers(text)
        defs = defaults or (["0"] * len(keys))
        vals = list(nums[:len(keys)])
        if len(vals) < len(keys):
            vals += defs[len(vals):len(keys)]
        pairs = ",".join(f'"{k}":{jnum(v)}' for k, v in zip(keys, vals))
        return EngineCommand(f'{prefix}:{op}|{{{pairs}}}', 0, f"{verb}.", None)
    return build


def _build_dm_cartesian(m: Match, text: str, session) -> EngineCommand:
    vecs = []
    remaining = text
    for _ in range(2):
        lit = find_vector_literal(remaining)
        if lit:
            vecs.append(lit)
            remaining = remaining[remaining.find(lit) + len(lit):]
    A = vecs[0] if len(vecs) > 0 else "[]"
    B = vecs[1] if len(vecs) > 1 else "[]"
    return EngineCommand(f'dm:cartesian|{{"A":{A},"B":{B}}}', 0, f"Computing the Cartesian product of {A} and {B}.", None)


def _build_dm_set_binary(op: str, verb: str):
    def build(m: Match, text: str, session) -> EngineCommand:
        vecs = []
        remaining = text
        for _ in range(2):
            lit = find_vector_literal(remaining)
            if lit:
                vecs.append(lit)
                remaining = remaining[remaining.find(lit) + len(lit):]
        A = vecs[0] if len(vecs) > 0 else "[]"
        B = vecs[1] if len(vecs) > 1 else "[]"
        return EngineCommand(f'dm:{op}|{{"A":{A},"B":{B}}}', 0, f"{verb} {A} and {B}.", None)
    return build


def _build_dm_set_unary(op: str, verb: str):
    def build(m: Match, text: str, session) -> EngineCommand:
        A = find_vector_literal(text) or "[]"
        return EngineCommand(f'dm:{op}|{{"A":{A}}}', 0, f"{verb} {A}.", None)
    return build


def _build_dm_recurrence(m: Match, text: str, session) -> EngineCommand:
    coeffs = find_vector_literal(text) or "[]"
    return EngineCommand(f'dm:recurrence|{{"coeffs":{coeffs}}}', 0,
                          f"Solving the linear recurrence with coefficients {coeffs}.", None)


def _build_dm_coin_change(m: Match, text: str, session) -> EngineCommand:
    denoms = find_vector_literal(text) or "[]"
    rest = text.replace(denoms, " ", 1) if denoms != "[]" else text
    nums = find_numbers(rest)
    target = nums[0] if nums else "0"
    return EngineCommand(f'dm:coin_change|{{"denoms":{denoms},"target":{jnum(target)}}}', 0,
                          f"Solving the coin-change problem for target {target} with denominations {denoms}.", None)


def _build_de_linear_first(m: Match, text: str, session) -> EngineCommand:
    P, Q = clean_expression(m.group("P")), clean_expression(m.group("Q"))
    return EngineCommand(
        f'de:linear_first|{{"P":{jv(P)},"Q":{jv(Q)},"x":"x","y":"y"}}',
        0, f"Solving the linear first-order ODE dy/dx + ({P})y = {Q}.", None)


def _build_de_bernoulli(m: Match, text: str, session) -> EngineCommand:
    P, Q, n = clean_expression(m.group("P")), clean_expression(m.group("Q")), m.group("n")
    return EngineCommand(
        f'de:bernoulli|{{"P":{jv(P)},"Q":{jv(Q)},"n":{jv(n)},"x":"x"}}',
        0, f"Solving the Bernoulli ODE dy/dx + ({P})y = ({Q})y^{n}.", None)





E = r"(?P<expr>.+?)"
V = r"(?P<var>[a-zA-Z](?:_[a-zA-Z0-9]+)?)"
NUMLIKE = r"[-+]?\d+(?:\.\d+)?"

INTENTS: List[Intent] = [


    # ─Calculus (batch 2) ───────────────────────────────────────────────
    Intent("calculus.log_diff", _p(
        rf"\blogarithmic derivative of\s+{E}$",
    ), _build_calc_log_diff),
    Intent("calculus.implicit_diff", _p(
        rf"\bimplicit(?:ly)? differentiat\w*\s+{E}$",
    ), _build_calc_implicit_diff),
    Intent("calculus.hessian", _p(
        rf"\bhessian of\s+{E}(?:\s+with respect to\s+.+)?$",
    ), _build_calc_hessian),
    Intent("calculus.laplacian", _p(
        rf"\blaplacian of\s+{E}(?:\s+with respect to\s+.+)?$",
    ), _build_calc_laplacian),
    Intent("calculus.jacobian", _p(
        rf"\bjacobian of\b",
    ), _build_calc_jacobian),
    Intent("calculus.curl", _p(r"\bcurl of\b"), _build_calc_vector_field_op("curl", "Computing the curl of")),
    Intent("calculus.div", _p(r"\bdivergence of\b"), _build_calc_vector_field_op("div", "Computing the divergence of")),
    Intent("calculus.limit_left", _p(
        rf"\blimit of\s+{E}\s+as\s+{V}\s*(?:approaches|->|goes to|→)\s*.+\s+from the left$",
    ), _build_calc_limit_directed("limit_left", "Taking the left-hand limit")),
    Intent("calculus.limit_right", _p(
        rf"\blimit of\s+{E}\s+as\s+{V}\s*(?:approaches|->|goes to|→)\s*.+\s+from the right$",
    ), _build_calc_limit_directed("limit_right", "Taking the right-hand limit")),

    # ── Linear Algebra (batch 2) ───────────────────────────────â─
    Intent("la.add", _p(r"\badd\b.*\[\[")),
    Intent("la.subtract", _p(r"\bsubtract\b.*\[\[")),
    Intent("la.power", _p(r"\bmatrix\b.*\braised to\b", r"\bmatrix power\b")),
    Intent("la.vector_norm", _p(r"\b(?:norm|magnitude) of\b.*\[")),
    Intent("la.dot_product", _p(r"\bdot product\b")),
    Intent("la.cross_product", _p(r"\bcross product\b")),
    Intent("la.eigenvalues", _p(r"\beigenvalues of\b")),
    Intent("la.eigenvectors", _p(r"\beigenvectors of\b")),
    Intent("la.is_symmetric", _p(r"\bis\b.*\bsymmetric\b")),
    Intent("la.is_orthogonal", _p(r"\bis\b.*\borthogonal\b")),
    Intent("la.null_space", _p(r"\bnull space of\b", r"\bkernel of\b")),
    Intent("la.condition_number", _p(r"\bcondition number of\b")),
    Intent("la.pseudoinverse", _p(r"\bpseudoinverse of\b", r"\bmoore.penrose\b")),
    Intent("la.decomp_lu", _p(r"\blu decomposition\b")),
    Intent("la.decomp_qr", _p(r"\bqr decomposition\b")),

    # ── Statistics (batch 2) ────────────────────────────────────────────
    Intent("stat.skewness", _p(r"\bskewness of\b")),
    Intent("stat.kurtosis", _p(r"\bkurtosis of\b")),
    Intent("stat.percentile", _p(r"\bpercentile of\b", r"\d+(?:st|nd|rd|th) percentile\b")),
    Intent("stat.standard_error", _p(r"\bstandard error of\b")),
    Intent("stat.five_number", _p(r"\bfive.number summary\b")),
    Intent("stat.iqr", _p(r"\b(?:iqr|interquartile range) of\b")),
    Intent("stat.z_test", _p(r"\bz.test\b")),
    Intent("stat.t_test_one", _p(r"\bone.sample t.test\b")),
    Intent("stat.f_test", _p(r"\bf.test\b")),
    Intent("stat.normal_cdf", _p(r"\bnormal cdf\b", r"\bnormal distribution cdf\b")),
    Intent("stat.binomial_pmf", _p(r"\bbinomial (?:pmf|probability)\b")),
    Intent("stat.poisson_pmf", _p(r"\bpoisson (?:pmf|probability)\b")),
    # ── Number Theory (batch 2) ──────────────────────────────────────────
    Intent("nt.extended_gcd", _p(r"\bextended (?:gcd|euclidean)\b")),
    Intent("nt.divisors", _p(r"\bdivisors of\b", r"\bfactors of\b")),
    Intent("nt.num_divisors", _p(r"\bnumber of divisors of\b", r"\bhow many divisors\b")),
    Intent("nt.sum_divisors", _p(r"\bsum of divisors of\b")),
    Intent("nt.is_perfect", _p(r"\bis\s+\d+\s+a?\s*perfect number\b", r"\bperfect number\?")),
    Intent("nt.mod_inverse", _p(r"\bmodular inverse\b", r"\bmod inverse\b")),
    Intent("nt.next_prime", _p(r"\bnext prime after\b")),
    Intent("nt.nth_prime", _p(r"\bnth prime\b", r"\b\d+(?:st|nd|rd|th) prime\b")),
    Intent("nt.primes_upto", _p(r"\bprimes up to\b", r"\bprimes less than\b", r"\bprimes below\b")),
    Intent("nt.catalan", _p(r"\bcatalan number\b")),
    Intent("nt.bell", _p(r"\bbell number\b")),
    Intent("nt.stirling1", _p(r"\bstirling number of the first kind\b")),
    Intent("nt.stirling2", _p(r"\bstirling number of the second kind\b")),
 
    # ── Discrete Math (batch 2) ──────────────────────────────────────────
    Intent("dm.derangements", _p(r"\bderangements? of\b")),
    Intent("dm.bfs", _p(r"\bbfs\b", r"\bbreadth.first search\b")),
    Intent("dm.components", _p(r"\bconnected components\b")),
    Intent("dm.bipartite", _p(r"\bis\b.*\bbipartite\b")),
    Intent("dm.floyd_warshall", _p(r"\ball.pairs shortest path\b", r"\bfloyd.warshall\b")),
    Intent("dm.prim_mst", _p(r"\bminimum spanning tree\b", r"\bprim'?s? mst\b")),
 
    # ── Geometry (batch 2) ───────────────────────────────────────────────
    Intent("geo.distance_3d", _p(
        r"\bdistance between\b.*\(\s*[-\d.]+\s*,\s*[-\d.]+\s*,\s*[-\d.]+\s*\)"
    ), _build_geo_distance_3d),
    Intent("geo.midpoint", _p(r"\bmidpoint of\b", r"\bmidpoint between\b"), _build_geo_midpoint),
    Intent("geo.slope", _p(r"\bslope (?:of|between)\b"), _build_geo_slope),
    Intent("geo.polygon_perim", _p(r"\bperimeter of\b.*\bpolygon\b")),
    Intent("geo.sphere", _p(r"\bequation of\b.*\bsphere\b", r"\bsphere with center\b"), _build_geo_sphere),
 
    # ── Complex Analysis (batch 2) ───────────────────────────────────────
    Intent("ca.complex_pow", _p(r"\braise\b.*\bcomplex\b.*\bto\b", r"\bcomplex number\b.*\bpower\b"), _build_ca_complex_pow),
    Intent("ca.complex_exp", _p(r"\bexponential of\b.*\bcomplex\b", r"\be\^.*i\b")),
    Intent("ca.complex_log", _p(r"\b(?:complex|logarithm of a complex) log\b")),
    Intent("ca.complex_sqrt", _p(r"\bsquare root of\b.*\bcomplex\b")),
    Intent("ca.polar", _p(r"\bpolar form of\b")),
    Intent("ca.residue", _p(
        rf"\bresidue of\s+{E}\s+at\b",
    ), _build_ca_residue),
 
    # ── Numerical Analysis (batch 2) ─────────────────────────────────────
    Intent("na.newton", _p(
        rf"\bnewton'?s? method\b(?:\s+(?:on|for))?\s+{E}\s+(?:starting (?:at|near)|near|from)\s+(?:x0\s*=\s*)?(?P<x0>{NUMLIKE})$",
    ), _build_na_newton),
    Intent("na.trapezoidal", _p(
        rf"\btrapezoidal rule\b\s+(?:of\s+)?{E}\s+from\s+(?P<a>{NUMLIKE})\s+to\s+(?P<b>{NUMLIKE})$",
        rf"\btrapezoidal(?:ly)? integrate\s+{E}\s+from\s+(?P<a>{NUMLIKE})\s+to\s+(?P<b>{NUMLIKE})$",
    ), _build_na_trapezoidal),
 
    # ── DiffEq (batch 2) ─────────────────────────────────────────────────
    Intent("de.euler", _p(
        rf"\beuler'?s? method\b.*?dy/dt\s*=\s*{E}\s+(?:with\s+)?y\(0\)\s*=\s*(?P<y0>{NUMLIKE})(?:.*?\bto\s+t\s*=\s*(?P<t1>{NUMLIKE}))?$",
    ), _build_de_numeric_method("euler", "Euler's method")),
    Intent("de.rk2", _p(
        rf"\brk2\b.*?dy/dt\s*=\s*{E}\s+(?:with\s+)?y\(0\)\s*=\s*(?P<y0>{NUMLIKE})(?:.*?\bto\s+t\s*=\s*(?P<t1>{NUMLIKE}))?$",
    ), _build_de_numeric_method("rk2", "RK2")),
 
    # ── Abstract Algebra (batch 2) ───────────────────────────────────────
    Intent("aa.ring_zn", _p(r"\bring\s+z/?\d+z?\b", r"\bring of integers mod\b"), _build_aa_ring_zn),
    Intent("aa.perm_order", _p(r"\border of\b.*\bpermutation\b")),
    Intent("aa.perm_inverse", _p(r"\binverse of\b.*\bpermutation\b")),
 
    # ── Batch 4 ───────────────────────────────────────────────────────────
    Intent("am.logistic_growth", _p(r"\blogistic growth\b")),
    Intent("am.sir", _p(r"\bsir (?:model|epidemic)\b", r"\bepidemic model\b")),
    Intent("am.lotka_volterra", _p(r"\blotka.volterra\b", r"\bpredator.prey\b")),
    Intent("am.random_walk", _p(r"\brandom walk\b")),
 
    Intent("prob.mgf_normal", _p(r"\bmgf\b.*\bnormal\b", r"\bmoment generating function\b.*\bnormal\b")),
    Intent("prob.mgf_binomial", _p(r"\bmgf\b.*\bbinomial\b")),
    Intent("prob.mgf_poisson", _p(r"\bmgf\b.*\bpoisson\b")),
    Intent("prob.gamblers_ruin", _p(r"\bgambler'?s? ruin\b")),
    Intent("prob.chebyshev_ineq", _p(r"\bchebyshev'?s? inequality\b")),
 
    Intent("de.linear_first", _p(
        rf"\blinear first.order (?:ode|equation)\b.*?P\s*=\s*(?P<P>.+?)\s*,\s*Q\s*=\s*(?P<Q>.+?)$",
        rf"\bdy/dx\s*\+\s*\((?P<P>.+?)\)\s*y\s*=\s*(?P<Q>.+?)$",
    ), _build_de_linear_first),
    Intent("de.bernoulli", _p(
        rf"\bbernoulli (?:ode|equation)\b.*?P\s*=\s*(?P<P>.+?)\s*,\s*Q\s*=\s*(?P<Q>.+?)\s*,\s*n\s*=\s*(?P<n>{NUMLIKE})$",
    ), _build_de_bernoulli),
    Intent("dm.pigeonhole", _p(r"\bpigeonhole\b")),
    Intent("dm.cartesian", _p(r"\bcartesian product\b"), _build_dm_cartesian),
    Intent("dm.topo_sort", _p(r"\btopological sort\b", r"\btopo sort\b")),
    Intent("dm.chromatic", _p(r"\bchromatic number\b")),
 
    Intent("nt.prime_pi", _p(r"\bprime.counting\b")),
    Intent("nt.mobius", _p(r"\bmobius function\b", r"\bmöbius function\b")),
    Intent("nt.legendre", _p(r"\blegendre symbol\b")),
    Intent("nt.jacobi_symbol", _p(r"\bjacobi symbol\b")),
    Intent("nt.partition", _p(r"\bpartition function\b", r"\bnumber of partitions of\b")),
 
    Intent("aa.dihedral", _p(r"\bdihedral group\b")),
    Intent("aa.quotient_group", _p(r"\bquotient group\b")),
    Intent("aa.gaussian_int", _p(r"\bgaussian integer\b")),
 
    Intent("stat.normal_pdf", _p(r"\bnormal pdf\b", r"\bnormal density\b")),
    Intent("stat.normal_qf", _p(r"\bnormal quantile\b", r"\binverse normal cdf\b")),
    Intent("stat.binomial_cdf", _p(r"\bbinomial cdf\b")),
    Intent("stat.poisson_cdf", _p(r"\bpoisson cdf\b")),
    Intent("stat.geometric_pmf", _p(r"\bgeometric (?:pmf|probability)\b")),
 
    # ── Calculus ──────────────────────────────────────────────────────────
    Intent("calculus.derivative", _p(
        rf"\b(?:derivative|differentiate)\s+of\s+{E}(?:\s+with respect to\s+{V})?$",
        rf"\bd/d(?P<var2>[a-zA-Z])\s*\[?{E}\]?$",
        rf"\bdifferentiate\s+{E}$",
        rf"^{E}\s*'\s*$",
    ), _build_derivative),
 
    Intent("calculus.partial", _p(
        rf"\bpartial derivative of\s+{E}\s+with respect to\s+{V}$",
        rf"\b∂/∂{V}\s*\[?{E}\]?$",
    ), _build_partial),
 
    Intent("calculus.gradient", _p(
        rf"\bgradient of\s+{E}$",
        rf"\bgrad\s*\[?{E}\]?$",
    ), _build_gradient),
 
    Intent("calculus.integral_definite", _p(
        rf"\bintegrate\s+{E}\s+from\s+(?P<a>{NUMLIKE})\s+to\s+(?P<b>{NUMLIKE})$",
        rf"\bintegral of\s+{E}\s+from\s+(?P<a>{NUMLIKE})\s+to\s+(?P<b>{NUMLIKE})$",
        rf"∫.*?{E}.*?from\s+(?P<a>{NUMLIKE})\s+to\s+(?P<b>{NUMLIKE})$",
    ), _build_integral_def),
 
    Intent("calculus.integral_indefinite", _p(
        rf"\b(?:integrate|antiderivative of|integral of)\s+{E}$",
    ), _build_integral_indef),
 
    Intent("calculus.limit", _p(
        rf"\blimit of\s+{E}\s+as\s+{V}\s*(?:approaches|->|goes to|→)\s*(?P<point>.+)$",
    ), _build_limit),
 
    Intent("calculus.taylor", _p(
        rf"\btaylor series of\s+{E}(?:\s+(?:around|at)\s+(?P<center>{NUMLIKE}))?(?:\s+(?:order|degree)\s+(?P<order>\d+))?$",
        rf"\bmaclaurin series of\s+{E}(?:\s+(?:order|degree)\s+(?P<order>\d+))?$",
    ), _build_taylor),
 
    Intent("calculus.optimize", _p(
        rf"\b(?:maximum and minimum|critical points|maxima and minima) of\s+{E}$",
        rf"\boptimi[sz]e\s+{E}$",
    ), _build_optimize),
 
    # ── Linear Algebra ───────────────────────────────────────────────────
    Intent("la.determinant", _p(r"\bdeterminant of\b", r"\bdet\(?\s*\[")),
    Intent("la.inverse", _p(r"\binverse\b", r"\binvert\b")),
    Intent("la.transpose", _p(r"\btranspose of\b", r"\btranspose\b")),
    Intent("la.rank", _p(r"\brank of\b")),
    Intent("la.trace", _p(r"\btrace of\b")),
    Intent("la.rref", _p(r"\brow reduce\b", r"\breduced row echelon\b", r"\brref\b")),
    Intent("la.eigen", _p(r"\beigenvalues?\b", r"\beigenvectors?\b")),
    Intent("la.solve", _p(r"\bsolve\b.*\bsystem\b", r"\bsolve\b.*\bAx\s*=\s*b\b")),
    Intent("la.multiply", _p(r"\bmultiply\b.*\bmatri", r"\bmatrix multiplication\b")),
 
    # ── Statistics ────────────────────────────────────────────────────────
    Intent("stat.mean", _p(r"\b(?:mean|average) of\b")),
    Intent("stat.median", _p(r"\bmedian of\b")),
    Intent("stat.mode", _p(r"\bmode of\b")),
    Intent("stat.stddev", _p(r"\b(?:standard deviation|std ?dev) of\b")),
    Intent("stat.variance", _p(r"\bvariance of\b")),
    Intent("stat.pearson", _p(r"\b(?:correlation|pearson)\b.*\bbetween\b", r"\bcorrelate\b")),
    Intent("stat.linear_reg", _p(r"\blinear regression\b", r"\bline of best fit\b", r"\bregress\b")),
    Intent("stat.covariance", _p(r"\bcovariance\b")),
    Intent("stat.markov_steady", _p(r"\bsteady.state\b.*\bmarkov\b", r"\bmarkov chain\b.*\bsteady\b")),
 
    # ── Number Theory ─────────────────────────────────────────────────────
    Intent("nt.gcd", _p(r"\b(?:gcd|greatest common divisor|greatest common factor)\b")),
    Intent("nt.lcm", _p(r"\b(?:lcm|least common multiple)\b")),
    Intent("nt.is_prime", _p(r"\bis\s+\d+\s+prime\b", r"\bprime\?\s*$", r"\bcheck if\b.*\bprime\b")),
    Intent("nt.prime_factors", _p(r"\bprime factors? of\b", r"\bfactori[sz]e\b")),
    Intent("nt.fibonacci", _p(r"\bfibonacci\b")),
    Intent("nt.mod_pow", _p(r"\b\d+\s*\^\s*\d+\s*mod\s*\d+\b", r"\bmodular exponent")),
    Intent("nt.euler_phi", _p(r"\beuler'?s? (?:totient|phi)\b")),
 
    # ── Discrete Math ─────────────────────────────────────────────────────
    Intent("dm.combinations", _p(r"\bcombinations?\b", r"\bchoose\b", r"\bnCr\b")),
    Intent("dm.permutations", _p(r"\bpermutations?\b", r"\bnPr\b")),
    Intent("dm.dijkstra", _p(r"\bshortest path\b", r"\bdijkstra\b")),
 
    # ── Geometry ──────────────────────────────────────────────────────────
    Intent("geo.distance", _p(r"\bdistance between\b.*\bpoints?\b", r"\bdistance between\s*\(")),
    Intent("geo.triangle_area", _p(r"\barea of\b.*\btriangle\b")),
    Intent("geo.circle", _p(r"\bequation of\b.*\bcircle\b", r"\bcircle with center\b")),
 
    # ── Complex Analysis ─────────────────────────────────────────────────
    Intent("ca.roots", _p(r"\b(?P<n>\d+)(?:th|rd|st|nd)?\s+roots? of\b", r"\ball roots of\b")),
 
    # ── Numerical Analysis ───────────────────────────────────────────────
    Intent("na.bisection", _p(
        rf"\b(?:root of|solve|zero of)\s+{E}\s+(?:on|in|between)\s*(?P<range>[\d,.\-\s\[\]to]+)$",
        rf"\bbisection\b.*?{E}$",
    ), _build_na_bisection),
    Intent("na.simpsons", _p(
        rf"\bnumerically integrate\s+{E}\s+from\s+(?P<a>{NUMLIKE})\s+to\s+(?P<b>{NUMLIKE})$",
        rf"\bsimpson'?s? rule\b.*?{E}.*from\s+(?P<a>{NUMLIKE})\s+to\s+(?P<b>{NUMLIKE})$",
    ), _build_na_simpsons),
 
    # ── DiffEq ────────────────────────────────────────────────────────────
    Intent("de.rk4", _p(
        rf"\bsolve\s+dy/dt\s*=\s*{E}\s+(?:with\s+)?y\(0\)\s*=\s*(?P<y0>{NUMLIKE})(?:.*?\bto\s+t\s*=\s*(?P<t1>{NUMLIKE}))?$",
    ), _build_de_rk4),
    Intent("de.laplace", _p(
        rf"\blaplace transform of\s+{E}$",
    ), _build_de_laplace),
 
    # ── Abstract Algebra ─────────────────────────────────────────────────
    Intent("aa.cyclic", _p(r"\bcyclic group of order\b")),
    Intent("aa.symmetric", _p(r"\bsymmetric group\b")),
]
 
# Fill in unary/simple builders defined via factories (kept out of the
# declarative list above only because they're generated, not because
# they're special-cased).
_INTENT_BUILDERS = {
    "la.determinant": _build_la_unary("determinant", "Computing the determinant of"),
    "la.inverse": _build_la_unary("inverse", "Computing the inverse of"),
    "la.transpose": _build_la_unary("matrix_transpose", "Transposing"),
    "la.rank": _build_la_unary("rank", "Computing the rank of"),
    "la.trace": _build_la_unary("trace", "Computing the trace of"),
    "la.rref": _build_la_unary("rref", "Row-reducing"),
    "la.eigen": _build_la_unary("eigen_full", "Computing eigenvalues/eigenvectors of"),
    "la.solve": _build_la_solve,
    "la.multiply": _build_la_multiply,
 
    "stat.mean": _build_stat_unary("mean", "Computing the mean of"),
    "stat.median": _build_stat_unary("median", "Computing the median of"),
    "stat.mode": _build_stat_unary("mode", "Computing the mode of"),
    "stat.stddev": _build_stat_unary("stddev", "Computing the standard deviation of"),
    "stat.variance": _build_stat_unary("variance", "Computing the variance of"),
    "stat.pearson": _build_stat_two_sample("pearson", "Computing the Pearson correlation"),
    "stat.linear_reg": _build_stat_two_sample("linear_reg", "Fitting a linear regression"),
    "stat.covariance": _build_stat_two_sample("covariance", "Computing the covariance"),
    "stat.markov_steady": _build_markov_steady,
 
    "nt.gcd": _build_nt_two_int("gcd", "a", "b", "Finding the GCD of"),
    "nt.lcm": _build_nt_two_int("lcm", "a", "b", "Finding the LCM of"),
    "nt.is_prime": _build_nt_one_int("is_prime", "n", "Checking whether this is prime:"),
    "nt.prime_factors": _build_nt_one_int("prime_factors", "n", "Factoring"),
    "nt.fibonacci": _build_nt_one_int("fibonacci", "n", "Computing the Fibonacci number for"),
    "nt.euler_phi": _build_nt_one_int("euler_phi", "n", "Computing Euler's totient of"),
    "nt.mod_pow": lambda m, text, session: EngineCommand(
        'nt:mod_pow|{"base":%s,"exp":%s,"mod":%s}' % tuple(
            jnum(x) for x in (find_numbers(text) + ["0", "0", "0"])[:3]
        ), 0, "Computing modular exponentiation.", None),
 
    "dm.combinations": _build_dm_choose("combinations"),
    "dm.permutations": _build_dm_choose("permutations"),
    "dm.dijkstra": _build_dm_dijkstra,
 
    "geo.distance": _build_geo_distance,
    "geo.triangle_area": _build_geo_triangle_area,
    "geo.circle": _build_geo_circle,
 
    "ca.roots": _build_ca_roots,
 
    "aa.cyclic": _build_aa_cyclic,
    "aa.symmetric": _build_aa_symmetric,
 
    "la.add": _build_la_binary("matrix_add", "Adding the two matrices"),
    "la.subtract": _build_la_binary("matrix_subtract", "Subtracting the two matrices"),
    "la.power": _build_la_binary("matrix_power", "Raising the matrix to a power", second_is_scalar=True),
    "la.vector_norm": _build_la_vector_norm,
    "la.dot_product": _build_la_vector_binary("dot_product", "Computing the dot product of"),
    "la.cross_product": _build_la_vector_binary("cross_product", "Computing the cross product of"),
    "la.eigenvalues": _build_la_unary("eigenvalues", "Computing the eigenvalues of"),
    "la.eigenvectors": _build_la_unary("eigenvectors", "Computing the eigenvectors of"),
    "la.is_symmetric": _build_la_unary("is_symmetric", "Checking whether this matrix is symmetric:"),
    "la.is_orthogonal": _build_la_unary("is_orthogonal", "Checking whether this matrix is orthogonal:"),
    "la.null_space": _build_la_unary("null_space", "Computing the null space of"),
    "la.condition_number": _build_la_unary("condition_number", "Computing the condition number of"),
    "la.pseudoinverse": _build_la_unary("pseudoinverse", "Computing the pseudoinverse of"),
    "la.decomp_lu": _build_la_unary("decomp_lu", "Computing the LU decomposition of"),
    "la.decomp_qr": _build_la_unary("decomp_qr_gs", "Computing the QR decomposition of"),
 
    "stat.skewness": _build_stat_unary("skewness", "Computing the skewness of"),
    "stat.kurtosis": _build_stat_unary("kurtosis", "Computing the kurtosis of"),
    "stat.percentile": _build_stat_percentile,
    "stat.standard_error": _build_stat_unary("standard_error", "Computing the standard error of"),
    "stat.five_number": _build_stat_unary("five_number", "Computing the five-number summary of"),
    "stat.iqr": _build_stat_unary("iqr", "Computing the interquartile range of"),
    "stat.z_test": _build_stat_z_test,
    "stat.t_test_one": _build_stat_t_test_one,
    "stat.f_test": _build_stat_two_sample("f_test", "Running an F-test"),
    "stat.normal_cdf": _build_stat_normal_cdf,
    "stat.binomial_pmf": _build_stat_binomial_pmf,
    "stat.poisson_pmf": _build_stat_poisson_pmf,
 
    "nt.extended_gcd": _build_nt_two_int("extended_gcd", "a", "b", "Running the extended Euclidean algorithm on"),
    "nt.divisors": _build_nt_one_int("divisors", "n", "Finding the divisors of"),
    "nt.num_divisors": _build_nt_one_int("num_divisors", "n", "Counting the divisors of"),
    "nt.sum_divisors": _build_nt_one_int("sum_divisors", "n", "Summing the divisors of"),
    "nt.is_perfect": _build_nt_one_int("is_perfect", "n", "Checking whether this is a perfect number:"),
    "nt.mod_inverse": _build_nt_two_int("mod_inverse", "a", "mod", "Finding the modular inverse of"),
    "nt.next_prime": _build_nt_one_int("next_prime", "n", "Finding the next prime after"),
    "nt.nth_prime": _build_nt_one_int("nth_prime", "n", "Finding prime number"),
    "nt.primes_upto": _build_nt_one_int("primes_upto", "n", "Listing primes up to"),
    "nt.catalan": _build_nt_one_int("catalan", "n", "Computing the Catalan number for"),
    "nt.bell": _build_nt_one_int("bell", "n", "Computing the Bell number for"),
    "nt.stirling1": _build_nt_two_int("stirling1", "n", "k", "Computing the Stirling number of the first kind for"),
    "nt.stirling2": _build_nt_two_int("stirling2", "n", "k", "Computing the Stirling number of the second kind for"),
 
    "dm.derangements": lambda m, text, session: EngineCommand(
        f'dm:derangements|{{"n":{jnum((find_numbers(text) + ["0"])[0])}}}',
        0, "Counting derangements of that many items.", None),
    "dm.bfs": _build_dm_bfs,
    "dm.components": _build_dm_graph_unary("components", "Finding connected components"),
    "dm.bipartite": _build_dm_graph_unary("bipartite", "Checking whether this graph is bipartite"),
    "dm.floyd_warshall": _build_dm_graph_unary("floyd_warshall", "Running Floyd-Warshall (all-pairs shortest paths)"),
    "dm.prim_mst": _build_dm_graph_unary("prim_mst", "Finding the minimum spanning tree"),
 
    "geo.polygon_perim": lambda m, text, session: EngineCommand(
        f'geo:polygon_perim|{{"vertices":{matrix_literal_to_json_rows(find_matrix_literal(text) or "[[0,0]]")}}}',
        1, "Computing the polygon's perimeter.", None),
 
    "ca.complex_exp": _build_ca_binary_re_im("complex_exp", "Computing the complex exponential of"),
    "ca.complex_log": _build_ca_binary_re_im("complex_log", "Computing the complex logarithm of"),
    "ca.complex_sqrt": _build_ca_binary_re_im("complex_sqrt", "Computing the complex square root of"),
    "ca.polar": _build_ca_binary_re_im("polar", "Converting to polar form:"),
 
    "aa.perm_order": _build_aa_perm_unary("perm_order", "Computing the order of"),
    "aa.perm_inverse": _build_aa_perm_unary("perm_inverse", "Computing the inverse of"),
 
    "dm.pigeonhole": _build_nums("dm", "pigeonhole", ["items", "holes"], "Applying the pigeonhole principle"),
    "dm.topo_sort": _build_dm_graph_unary("topo_sort", "Finding a topological ordering"),
    "dm.chromatic": _build_dm_graph_unary("chromatic", "Computing the chromatic number"),
 
    "nt.prime_pi": _build_nt_one_int("prime_pi", "n", "Counting primes up to"),
    "nt.mobius": _build_nt_one_int("mobius", "n", "Evaluating the Mobius function at"),
    "nt.legendre": _build_nums("nt", "legendre", ["a", "p"], "Computing the Legendre symbol"),
    "nt.jacobi_symbol": _build_nums("nt", "jacobi", ["a", "n"], "Computing the Jacobi symbol"),
    "nt.partition": _build_nt_one_int("partition", "n", "Computing the partition function of"),
 
    "aa.dihedral": _build_nums("aa", "dihedral", ["n"], "Building the dihedral group"),
    "aa.quotient_group": _build_nums("aa", "quotient_group", ["n", "k"], "Building the quotient group"),
    "aa.gaussian_int": _build_nums("aa", "gaussian_int", ["a", "b"], "Working with the Gaussian integer"),
 
    "stat.normal_pdf": _build_nums("stat", "normal_pdf", ["x", "mu", "sigma"], "Computing the normal PDF", defaults=["0", "0", "1"]),
    "stat.normal_qf": _build_nums("stat", "normal_qf", ["p", "mu", "sigma"], "Computing the normal quantile", defaults=["0.5", "0", "1"]),
    "stat.binomial_cdf": _build_nums("stat", "binomial_cdf", ["k", "n", "p"], "Computing the binomial CDF", defaults=["0", "0", "0.5"]),
    "stat.poisson_cdf": _build_nums("stat", "poisson_cdf", ["k", "lambda"], "Computing the Poisson CDF", defaults=["0", "1"]),
    "stat.geometric_pmf": _build_nums("stat", "geometric_pmf", ["k", "p"], "Computing the geometric PMF", defaults=["0", "0.5"]),
 
    "am.logistic_growth": _build_nums("am", "logistic_growth", ["r", "K", "x0", "T"], "Modeling logistic growth", defaults=["2", "100", "10", "50"]),
    "am.sir": _build_nums("am", "sir", ["beta", "gamma", "S0", "I0", "R0"], "Running the SIR epidemic model", defaults=["0.3", "0.1", "990", "10", "0"]),
    "am.lotka_volterra": _build_nums("am", "lotka_volterra", ["alpha", "beta", "delta", "gamma", "x0", "y0"], "Simulating the Lotka-Volterra predator-prey model", defaults=["1", "1", "1", "1", "10", "5"]),
    "am.random_walk": _build_nums("am", "random_walk", ["p", "steps", "trials"], "Simulating a random walk", defaults=["0.5", "100", "1000"]),
 
    "prob.mgf_normal": _build_nums("prob", "mgf_normal", ["mu", "sigma", "t"], "Computing the normal MGF", defaults=["0", "1", "0"]),
    "prob.mgf_binomial": _build_nums("prob", "mgf_binomial", ["n", "p", "t"], "Computing the binomial MGF", defaults=["10", "0.5", "0"]),
    "prob.mgf_poisson": _build_nums("prob", "mgf_poisson", ["lambda", "t"], "Computing the Poisson MGF", defaults=["1", "0"]),
    "prob.gamblers_ruin": _build_nums("prob", "gamblers_ruin", ["p", "start", "target"], "Solving the gambler's ruin problem", defaults=["0.5", "5", "10"]),
    "prob.chebyshev_ineq": _build_nums("prob", "chebyshev_ineq", ["mu", "sigma", "k"], "Applying Chebyshev's inequality", defaults=["0", "1", "2"]),
}
 
# ─────────────────────────────────────────────────────────────────────────
# EXPANDED COVERAGE (batch 5) — Linear Algebra's remaining ~50 operations,
# covered via the generic _build_la_positional factory since LA.cpp's
# dispatch() just splits the payload on '|' positionally regardless of op.
# Declared as data (name, patterns, op, arg_types, verb) to keep this
# compact; registered with INTENTS[0:0] = ... so these more-specific
# phrasings (e.g. "inverse via Gauss-Jordan") are checked before the
# broader batch-1 patterns (e.g. bare "inverse") they'd otherwise collide
# with.
# ─────────────────────────────────────────────────────────────────────────
 
_LA_BATCH5 = [
    ("la.adjugate", [r"\badjugate of\b"], "adjugate", ["matrix"], "Computing the adjugate of"),
    ("la.cholesky", [r"\bcholesky decomposition\b"], "decomp_cholesky", ["matrix"], "Computing the Cholesky decomposition of"),
    ("la.svd", [r"\bsvd\b", r"\bsingular value decomposition\b"], "decomp_svd", ["matrix"], "Computing the SVD of"),
    ("la.qr_householder", [r"\bhouseholder qr\b", r"\bqr decomposition\b.*\bhouseholder\b"], "decomp_qr_hh", ["matrix"], "Computing the QR decomposition (Householder) of"),
    ("la.qr_givens", [r"\bgivens qr\b", r"\bqr decomposition\b.*\bgivens\b"], "decomp_qr_givens", ["matrix"], "Computing the QR decomposition (Givens) of"),
    ("la.schur", [r"\bschur decomposition\b"], "decomp_schur", ["matrix"], "Computing the Schur decomposition of"),
    ("la.diagonalize", [r"\bdiagonalize\b"], "diagonalize", ["matrix"], "Diagonalizing"),
    ("la.char_poly", [r"\bcharacteristic polynomial of\b"], "char_poly", ["matrix"], "Computing the characteristic polynomial of"),
    ("la.matrix_exp", [r"\bmatrix exponential of\b"], "matrix_exp", ["matrix"], "Computing the matrix exponential of"),
    ("la.matrix_log", [r"\bmatrix logarithm of\b"], "matrix_log", ["matrix"], "Computing the matrix logarithm of"),
    ("la.matrix_sqrt", [r"\bmatrix square root of\b"], "matrix_sqrt", ["matrix"], "Computing the matrix square root of"),
    ("la.hadamard", [r"\bhadamard product\b"], "matrix_hadamard", ["matrix", "matrix"], "Computing the Hadamard product of"),
    ("la.scalar_multiply", [r"\bmultiply the matrix\b.*\bby\b", r"\bscalar multipl\w* the matrix\b"], "scalar_multiply", ["matrix", "scalar"], "Scaling the matrix"),
    ("la.frobenius_norm", [r"\bfrobenius norm\b"], "frobenius_norm", ["matrix"], "Computing the Frobenius norm of"),
    ("la.spectral_norm", [r"\bspectral norm\b"], "spectral_norm", ["matrix"], "Computing the spectral norm of"),
    ("la.matrix_norm_p", [r"\binduced\b.*\bnorm\b", r"\bp.norm of\b.*\bmatrix\b"], "matrix_norm_p", ["matrix", "scalar"], "Computing the induced p-norm of"),
    ("la.is_posdef", [r"\bpositive semi.definite\b", r"\bis\b.*\bpositive definite\b"], "is_posdef", ["matrix"], "Checking whether this matrix is positive definite"),
    ("la.is_diagonalizable", [r"\bis\b.*\bdiagonalizable\b"], "is_diagonalizable", ["matrix"], "Checking whether this matrix is diagonalizable"),
    ("la.rank_nullity", [r"\brank.nullity\b"], "rank_nullity", ["matrix"], "Applying the rank-nullity theorem to"),
    ("la.nullity", [r"\bnullity of\b"], "nullity", ["matrix"], "Computing the nullity of"),
    ("la.row_space", [r"\brow space of\b"], "row_space", ["matrix"], "Computing the row space of"),
    ("la.column_space", [r"\bcolumn space of\b"], "column_space", ["matrix"], "Computing the column space of"),
    ("la.left_null_space", [r"\bleft null space of\b"], "left_null_space", ["matrix"], "Computing the left null space of"),
    ("la.orthonormal_basis", [r"\borthonormal basis\b"], "orthonormal_basis", ["matrix"], "Computing an orthonormal basis for"),
    ("la.projection", [r"\bproject\b.*\bonto\b"], "projection", ["vector", "vector"], "Projecting"),
    ("la.projection_matrix", [r"\bprojection matrix\b"], "projection_matrix", ["matrix"], "Computing the projection matrix onto the column space of"),
    ("la.gram_matrix", [r"\bgram matrix\b"], "gram_matrix", ["matrix"], "Computing the Gram matrix of"),
    ("la.quadratic_form", [r"\bquadratic form\b"], "quadratic_form", ["matrix", "vector"], "Evaluating the quadratic form of"),
    ("la.pivot_positions", [r"\bpivot positions\b", r"\bpivot columns\b"], "pivot_positions", ["matrix"], "Finding the pivot positions of"),
    ("la.solve_cramer", [r"\bcramer'?s? rule\b"], "solve_cramer", ["matrix", "vector"], "Solving via Cramer's rule"),
    ("la.solve_gaussian", [r"\bgaussian elimination\b"], "solve_gaussian", ["matrix", "vector"], "Solving via Gaussian elimination"),
    ("la.solve_gaussjordan", [r"\bsolve\b.*\bgauss.jordan\b"], "solve_gaussjordan", ["matrix", "vector"], "Solving via Gauss-Jordan elimination"),
    ("la.solve_leastsquares", [r"\bleast squares\b"], "solve_leastsquares", ["matrix", "vector"], "Solving via least squares"),
    ("la.solve_jacobi", [r"\bjacobi (?:iteration|method)\b"], "solve_jacobi", ["matrix", "vector"], "Solving via the Jacobi iterative method"),
    ("la.solve_gaussseidel", [r"\bgauss.seidel\b"], "solve_gaussseidel", ["matrix", "vector"], "Solving via Gauss-Seidel iteration"),
    ("la.determinant_cofactor", [r"\bdeterminant\b.*\bcofactor expansion\b"], "determinant_cofactor", ["matrix"], "Computing the determinant via cofactor expansion of"),
    ("la.determinant_lu", [r"\bdeterminant\b.*\blu decomposition\b"], "determinant_lu", ["matrix"], "Computing the determinant via LU decomposition of"),
    ("la.determinant_bareiss", [r"\bbareiss algorithm\b"], "determinant_bareiss", ["matrix"], "Computing the determinant via the Bareiss algorithm for"),
    ("la.inverse_gaussjordan", [r"\binverse\b.*\bgauss.jordan\b"], "inverse_gaussjordan", ["matrix"], "Computing the inverse via Gauss-Jordan elimination for"),
    ("la.inverse_adjugate", [r"\binverse\b.*\badjugate\b"], "inverse_adjugate", ["matrix"], "Computing the inverse via the adjugate for"),
    ("la.change_of_basis", [r"\bchange of basis\b"], "change_of_basis", ["matrix", "matrix", "matrix"], "Computing the change-of-basis matrix"),
    ("la.span_check", [r"\bdoes\b.*\bspan\b", r"\bspan check\b"], "span_check", ["matrix", "vector"], "Checking whether the vectors span"),
    ("la.sylvesters_law", [r"\bsylvester'?s? law\b"], "sylvesters_law", ["matrix"], "Applying Sylvester's law of inertia to"),
    ("la.cayley_hamilton", [r"\bcayley.hamilton\b"], "cayley_hamilton", ["matrix"], "Verifying the Cayley-Hamilton theorem for"),
    ("la.make_identity", [r"\bidentity matrix\b"], "make_identity", ["scalar"], "Building the identity matrix"),
    ("la.make_zero_matrix", [r"\bzero matrix\b"], "make_zero", ["scalar", "scalar"], "Building the zero matrix"),
    ("la.make_hilbert", [r"\bhilbert matrix\b"], "make_hilbert", ["scalar"], "Building the Hilbert matrix"),
    ("la.make_vandermonde", [r"\bvandermonde matrix\b"], "make_vandermonde", ["vector"], "Building the Vandermonde matrix"),
    ("la.make_companion", [r"\bcompanion matrix\b"], "make_companion", ["vector"], "Building the companion matrix"),
    ("la.matrix_strassen", [r"\bstrassen'?s? algorithm\b"], "matrix_strassen", ["matrix", "matrix"], "Multiplying via Strassen's algorithm"),
    ("la.ref", [r"\brow echelon form\b(?!.*reduced)", r"\bref of\b"], "ref", ["matrix"], "Reducing to row echelon form"),
    ("la.decomp_jordan", [r"\bjordan (?:canonical|normal) form\b", r"\bjordan decomposition\b"], "decomp_jordan", ["matrix"], "Computing the Jordan canonical form of"),
    ("la.spectral_decomp", [r"\bspectral decomposition\b"], "spectral", ["matrix"], "Computing the spectral decomposition of"),
    ("la.alg_mult", [r"\balgebraic multiplicity\b"], "alg_mult", ["matrix"], "Computing the algebraic multiplicity of eigenvalues of"),
    ("la.geom_mult", [r"\bgeometric multiplicity\b"], "geom_mult", ["matrix"], "Computing the geometric multiplicity of eigenvalues of"),
    ("la.classify_matrix", [r"\bclassify\b.*\bmatrix\b"], "classify", ["matrix"], "Classifying the matrix"),
    ("la.is_independent", [r"\blinearly independent\b"], "is_independent", ["matrix"], "Checking whether these vectors are linearly independent"),
    ("la.vector_angle", [r"\bangle between\b.*\[.*\].*\[.*\]"], "vector_angle", ["vector", "vector"], "Computing the angle between"),
    ("la.vector_distance", [r"\bdistance between\b.*\[.*\].*\[.*\]"], "vector_distance", ["vector", "vector"], "Computing the distance between"),
]
 
INTENTS[0:0] = [
    Intent(_name, _p(*_pats), _build_la_positional(_op, _argtypes, _verb))
    for (_name, _pats, _op, _argtypes, _verb) in _LA_BATCH5
]
 
# ─────────────────────────────────────────────────────────────────────────
# EXPANDED COVERAGE (batch 6) — Statistics' remaining ~85 operations.
# Four shapes cover nearly all of them: vector+scalars, two-vectors+scalars,
# pure scalars (a,b,c,d contingency cells), and a single table/groups
# matrix. Declared as data for the same reason as LA batch 5.
# ─────────────────────────────────────────────────────────────────────────
 
_STAT_NUMS = [  # (name, patterns, op, keys, verb, defaults) -- vector x + trailing scalars
    ("stat.ci_mean_t", [r"\bconfidence interval\b.*\bt\b.*\bmean\b", r"\bt.based confidence interval\b"], "ci_mean_t", ["alpha"], "Computing a t-based confidence interval for the mean", ["0.05"]),
    ("stat.ci_mean_z", [r"\bconfidence interval\b.*\bz\b.*\bmean\b", r"\bz.based confidence interval\b"], "ci_mean_z", ["alpha"], "Computing a z-based confidence interval for the mean", ["0.05"]),
    ("stat.bootstrap_ci", [r"\bbootstrap\b.*\bconfidence interval\b", r"\bbootstrap ci\b"], "bootstrap_ci", ["alpha", "B"], "Computing a bootstrap confidence interval", ["0.05", "2000"]),
    ("stat.sign_test", [r"\bsign test\b"], "sign_test", ["mu0", "alpha"], "Running the sign test", ["0", "0.05"]),
    ("stat.runs_test", [r"\bruns test\b"], "runs_test", ["alpha"], "Running the runs test for randomness", ["0.05"]),
    ("stat.anderson_darling", [r"\banderson.darling\b"], "anderson_darling", ["alpha"], "Running the Anderson-Darling normality test", ["0.05"]),
    ("stat.shapiro_wilk", [r"\bshapiro.wilk\b"], "shapiro_wilk", ["alpha"], "Running the Shapiro-Wilk normality test", ["0.05"]),
    ("stat.wilcoxon", [r"\bwilcoxon\b"], "wilcoxon", ["mu0", "alpha"], "Running the Wilcoxon signed-rank test", ["0", "0.05"]),
    ("stat.moving_avg", [r"\bmoving average\b"], "moving_avg", ["window"], "Computing the moving average", ["3"]),
    ("stat.exp_smooth", [r"\bexponential smoothing\b(?!.*double)"], "exp_smooth", ["alpha"], "Applying exponential smoothing", ["0.2"]),
    ("stat.double_exp", [r"\bdouble exponential smoothing\b", r"\bholt'?s? method\b"], "double_exp", ["alpha", "beta"], "Applying double exponential smoothing", ["0.2", "0.1"]),
    ("stat.acf", [r"\bautocorrelation\b", r"\bacf of\b"], "acf", ["maxlag"], "Computing the autocorrelation function", ["20"]),
    ("stat.pacf", [r"\bpartial autocorrelation\b", r"\bpacf of\b"], "pacf", ["maxlag"], "Computing the partial autocorrelation function", ["20"]),
    ("stat.ljung_box", [r"\bljung.box\b"], "ljung_box", ["lag"], "Running the Ljung-Box test", ["10"]),
    ("stat.differencing", [r"\bdifferenc(?:e|ing) the series\b", r"\btime series differencing\b"], "differencing", ["d"], "Differencing the time series", ["1"]),
    ("stat.forecast_ses", [r"\bforecast\b.*\bsimple exponential smoothing\b", r"\bses forecast\b"], "forecast_ses", ["alpha", "h"], "Forecasting via simple exponential smoothing", ["0.2", "5"]),
    ("stat.cusum", [r"\bcusum\b"], "cusum", ["k", "h"], "Running a CUSUM control chart", ["0.5", "4"]),
    ("stat.clt_demo", [r"\bcentral limit theorem\b"], "clt_demo", ["n", "reps"], "Demonstrating the central limit theorem", ["30", "1000"]),
]
 
_STAT_SCALAR = [  # (name, patterns, op, keys, verb, defaults) -- pure scalars, no data vector
    ("stat.t_pdf", [r"\bt.pdf\b", r"\bt distribution pdf\b"], "t_pdf", ["x", "df"], "Computing the t-distribution PDF", ["0", "10"]),
    ("stat.t_cdf", [r"\bt.cdf\b", r"\bt distribution cdf\b"], "t_cdf", ["x", "df"], "Computing the t-distribution CDF", ["0", "10"]),
    ("stat.t_qf", [r"\bt.quantile\b", r"\bt distribution quantile\b"], "t_qf", ["p", "df"], "Computing the t-distribution quantile", ["0.5", "10"]),
    ("stat.chisq_pdf", [r"\bchi.square(?:d)? pdf\b"], "chisq_pdf", ["x", "df"], "Computing the chi-square PDF", ["1", "1"]),
    ("stat.chisq_cdf", [r"\bchi.square(?:d)? cdf\b"], "chisq_cdf", ["x", "df"], "Computing the chi-square CDF", ["1", "1"]),
    ("stat.chisq_qf", [r"\bchi.square(?:d)? quantile\b"], "chisq_qf", ["p", "df"], "Computing the chi-square quantile", ["0.5", "1"]),
    ("stat.f_pdf", [r"\bf.pdf\b", r"\bf distribution pdf\b"], "f_pdf", ["x", "d1", "d2"], "Computing the F-distribution PDF", ["1", "1", "1"]),
    ("stat.f_cdf", [r"\bf.cdf\b", r"\bf distribution cdf\b"], "f_cdf", ["x", "d1", "d2"], "Computing the F-distribution CDF", ["1", "1", "1"]),
    ("stat.exp_pdf", [r"\bexponential pdf\b"], "exp_pdf", ["x", "lambda"], "Computing the exponential PDF", ["1", "1"]),
    ("stat.exp_cdf", [r"\bexponential cdf\b"], "exp_cdf", ["x", "lambda"], "Computing the exponential CDF", ["1", "1"]),
    ("stat.bayes", [r"\bbayes'? theorem\b"], "bayes", ["prior", "likelihood", "marginal"], "Applying Bayes' theorem", ["0.5", "0.5", "1"]),
    ("stat.cond_prob", [r"\bconditional probability\b"], "cond_prob", ["pAB", "pB"], "Computing the conditional probability", ["0.5", "1"]),
    ("stat.ci_prop", [r"\bconfidence interval\b.*\bproportion\b"], "ci_prop", ["k", "n", "alpha"], "Computing a confidence interval for the proportion", ["1", "1", "0.05"]),
    ("stat.sampling_prop", [r"\bsampling distribution\b.*\bproportion\b"], "sampling_prop", ["n", "p", "alpha"], "Computing the sampling distribution of a proportion", ["30", "0.5", "0.05"]),
]
 
_STAT_TWO_NUMS = [
    ("stat.mann_whitney", [r"\bmann.whitney\b"], "mann_whitney", ["alpha"], "Running the Mann-Whitney U test", ["0.05"]),
    ("stat.ks_test", [r"\bkolmogorov.smirnov\b", r"\bks test\b"], "ks_test", ["alpha"], "Running the Kolmogorov-Smirnov test", ["0.05"]),
    ("stat.kl_div", [r"\bkl divergence\b", r"\bkullback.leibler\b"], "kl_div", [], "Computing the KL divergence", []),
]
 
_STAT_4INT = [  # 2x2 contingency table cells a,b,c,d
    ("stat.fisher_exact", [r"\bfisher'?s? exact test\b"], "fisher_exact", ["a", "b", "c", "d", "alpha"], "Running Fisher's exact test", ["1", "1", "1", "1", "0.05"]),
    ("stat.mcnemar", [r"\bmcnemar'?s? test\b"], "mcnemar", ["a", "b", "c", "d", "alpha"], "Running McNemar's test", ["1", "1", "1", "1", "0.05"]),
    ("stat.odds_ratio", [r"\bodds ratio\b"], "odds_ratio", ["a", "b", "c", "d"], "Computing the odds ratio", ["1", "1", "1", "1"]),
    ("stat.relative_risk", [r"\brelative risk\b"], "relative_risk", ["a", "b", "c", "d"], "Computing the relative risk", ["1", "1", "1", "1"]),
    ("stat.risk_diff", [r"\brisk difference\b"], "risk_diff", ["a", "b", "c", "d"], "Computing the risk difference", ["1", "1", "1", "1"]),
    ("stat.nnt", [r"\bnumber needed to treat\b", r"\bnnt\b"], "nnt", ["a", "b", "c", "d"], "Computing the number needed to treat", ["1", "1", "1", "1"]),
    ("stat.phi_coeff", [r"\bphi coefficient\b"], "phi", ["a", "b", "c", "d"], "Computing the phi coefficient", ["1", "1", "1", "1"]),
]
 
_STAT_TABLE = [
    ("stat.chi_sq_indep", [r"\bchi.square(?:d)? test\b.*\bindependence\b", r"\bchi.squared? test of independence\b"], "chi_sq_indep", "Running a chi-square test of independence"),
    ("stat.cramer_v", [r"\bcramer'?s? v\b"], "cramer_v", "Computing Cramer's V"),
    ("stat.factorial_2x2", [r"\bfactorial (?:2x2|design)\b"], "factorial_2x2", "Analyzing the 2x2 factorial design"),
]
 
_STAT_GROUPS = [
    ("stat.anova_one", [r"\bone.way anova\b"], "anova_one", "Running a one-way ANOVA"),
    ("stat.anova_two", [r"\btwo.way anova\b"], "anova_two", "Running a two-way ANOVA"),
    ("stat.tukey", [r"\btukey'?s? hsd\b", r"\btukey test\b"], "tukey", "Running Tukey's HSD post-hoc test"),
    ("stat.scheffe", [r"\bscheffe'?s? test\b"], "scheffe", "Running Scheffe's post-hoc test"),
    ("stat.kruskal_wallis2", [r"\bkruskal.wallis\b"], "kruskal_wallis", "Running the Kruskal-Wallis test"),
    ("stat.friedman", [r"\bfriedman test\b"], "friedman", "Running the Friedman test"),
    ("stat.rand_block", [r"\brandomi[sz]ed block\b"], "rand_block", "Analyzing the randomized block design"),
]
 
_STAT_REG_DIAG = [
    ("stat.multiple_reg", [r"\bmultiple regression\b"], "multiple_reg", "Fitting a multiple regression"),
    ("stat.reg_diagnostics", [r"\bregression diagnostics\b"], "reg_diagnostics", "Running regression diagnostics"),
    ("stat.vif", [r"\bvariance inflation factor\b", r"\bvif\b"], "vif", "Computing variance inflation factors"),
    ("stat.cooks_distance", [r"\bcook'?s? distance\b"], "cooks_distance", "Computing Cook's distance"),
    ("stat.stepwise", [r"\bstepwise regression\b"], "stepwise", "Running stepwise regression"),
]
 
INTENTS[0:0] = [Intent(n, _p(*p), _build_stat_vec_nums(op, k, v, d)) for (n, p, op, k, v, d) in _STAT_NUMS]
INTENTS[0:0] = [Intent(n, _p(*p), _build_nums("stat", op, k, v, d)) for (n, p, op, k, v, d) in _STAT_SCALAR]
INTENTS[0:0] = [Intent(
    "stat.multinomial", _p(r"\bmultinomial (?:test|probability)\b"),
    lambda m, text, session: EngineCommand(
        f'stat:multinomial|{{"counts":{find_vector_literal(text) or "[]"}}}',
        1, "Running the multinomial test.", None))]
INTENTS[0:0] = [Intent(n, _p(*p), _build_stat_two_sample_nums(op, k, v, d)) for (n, p, op, k, v, d) in _STAT_TWO_NUMS]
INTENTS[0:0] = [Intent(n, _p(*p), _build_nums("stat", op, k, v, d)) for (n, p, op, k, v, d) in _STAT_4INT]
INTENTS[0:0] = [Intent(n, _p(*p), _build_stat_table(op, v)) for (n, p, op, v) in _STAT_TABLE]
INTENTS[0:0] = [Intent(n, _p(*p), _build_stat_groups(op, v)) for (n, p, op, v) in _STAT_GROUPS]
INTENTS[0:0] = [Intent(n, _p(*p), _build_stat_reg_diag(op, v)) for (n, p, op, v) in _STAT_REG_DIAG]
 
def _build_stat_poly_reg(m: Match, text: str, session) -> EngineCommand:
 
    lists = []
    remaining = text
    for _ in range(2):
        lit = find_vector_literal(remaining)
        if lit:
            lists.append(lit)
            remaining = remaining[remaining.find(lit) + len(lit):]
    x = lists[0] if len(lists) > 0 else "[]"
    y = lists[1] if len(lists) > 1 else "[]"
    nums = find_numbers(remaining)
    degree = nums[0] if nums else "2"
    return EngineCommand(f'stat:poly_reg|{{"x":{x},"y":{y},"degree":{jnum(degree)}}}', 1,
                          f"Fitting a degree-{degree} polynomial regression.", None)
 
 
INTENTS[0:0] = [
    Intent("stat.poly_reg", _p(r"\bpolynomial regression\b"), _build_stat_poly_reg),
    Intent("stat.entropy", _p(r"\bentropy of\b(?!.*joint)"), _build_stat_unary("entropy", "Computing the entropy of")),
    Intent("stat.mutual_info", _p(r"\bmutual information\b"), _build_stat_table("mutual_info", "Computing the mutual information", key="joint")),
    Intent("stat.freq_table", _p(r"\bfrequency table\b"), _build_stat_unary("freq_table", "Building a frequency table for")),
    Intent("stat.summarize", _p(r"\bsummari[sz]e\b.*\[", r"\bsummary statistics\b"), _build_stat_unary("summarize", "Summarizing")),
]
 
for _intent in INTENTS:
    if _intent.name in _INTENT_BUILDERS:
        _intent.build = _INTENT_BUILDERS[_intent.name]
 
# ─────────────────────────────────────────────────────────────────────────
# EXPANDED COVERAGE (batch  — Number Theory's remaining 17 operations,
# all pure-integer params, so the generic _build_nums factory covers them.
# ─────────────────────────────────────────────────────────────────────────
 
_NT_BATCH8 = [
    ("nt.carmichael", [r"\bcarmichael function\b", r"\bcarmichael lambda\b"], "carmichael", ["n"], "Computing the Carmichael function of", ["1"]),
    ("nt.liouville", [r"\bliouville function\b"], "liouville", ["n"], "Evaluating the Liouville function at", ["1"]),
    ("nt.quad_residue", [r"\bquadratic residue\b"], "quad_residue", ["a", "p"], "Checking the quadratic residue", ["1", "2"]),
    ("nt.primitive_root", [r"\bprimitive root\b"], "primitive_root", ["p"], "Finding a primitive root modulo", ["2"]),
    ("nt.discrete_log", [r"\bdiscrete log(?:arithm)?\b"], "discrete_log", ["base", "target", "mod"], "Solving the discrete logarithm", ["2", "1", "5"]),
    ("nt.linear_cong", [r"\blinear congruence\b"], "linear_cong", ["a", "b", "m"], "Solving the linear congruence", ["1", "0", "1"]),
    ("nt.quad_cong", [r"\bquadratic congruence\b"], "quad_cong", ["a", "b", "c", "m"], "Solving the quadratic congruence", ["1", "0", "0", "1"]),
    ("nt.lucas", [r"\blucas number\b"], "lucas", ["n"], "Computing the Lucas number for", ["1"]),
    ("nt.linear_dioph", [r"\blinear diophantine\b"], "linear_dioph", ["a", "b", "c"], "Solving the linear Diophantine equation", ["1", "1", "1"]),
    ("nt.pyth_triples", [r"\bpythagorean triples\b"], "pyth_triples", ["limit"], "Finding Pythagorean triples up to", ["100"]),
    ("nt.sum_two_sq", [r"\bsum of two squares\b"], "sum_two_sq", ["n"], "Expressing as a sum of two squares:", ["1"]),
    ("nt.sum_four_sq", [r"\bsum of four squares\b"], "sum_four_sq", ["n"], "Expressing as a sum of four squares:", ["1"]),
    ("nt.miller_rabin", [r"\bmiller.rabin\b"], "miller_rabin", ["n", "rounds"], "Running the Miller-Rabin primality test on", ["2", "20"]),
    ("nt.pollard_rho", [r"\bpollard'?s? rho\b"], "pollard_rho", ["n"], "Factoring via Pollard's rho algorithm:", ["1"]),
    ("nt.rsa_keygen", [r"\brsa key\b", r"\brsa keygen\b"], "rsa_keygen", ["p", "q"], "Generating an RSA keypair from primes", ["61", "53"]),
    ("nt.el_gamal", [r"\belgamal\b", r"\bel gamal\b"], "el_gamal", ["p", "g", "x"], "Generating an ElGamal keypair", ["23", "5", "6"]),
]
 
INTENTS[0:0] = [Intent(n, _p(*p), _build_nums("nt", op, k, v, d)) for (n, p, op, k, v, d) in _NT_BATCH8]
 
# ─────────────────────────────────────────────────────────────────────────
# EXPANDED COVERAGE (batch 9) — Discrete Math's remaining 25 operations.
# ─────────────────────────────────────────────────────────────────────────
 
_DM_SET_BINARY = [
    ("dm.set_union", [r"\bunion of\b.*\[.*\].*\[.*\]", r"\bset union\b"], "set_union", "Computing the union of"),
    ("dm.set_intersect", [r"\bintersection of\b.*\[.*\].*\[.*\]", r"\bset intersection\b"], "set_intersect", "Computing the intersection of"),
    ("dm.set_diff", [r"\bset difference\b"], "set_diff", "Computing the set difference of"),
    ("dm.set_sym_diff", [r"\bsymmetric difference\b"], "set_sym_diff", "Computing the symmetric difference of"),
    ("dm.is_subset", [r"\bis\b.*\bsubset of\b"], "is_subset", "Checking whether this is a subset of"),
]
 
 
_DM_SET_UNARY = [
    ("dm.power_set", [r"\bpower set of\b"], "power_set", "Computing the power set of"),
]
 
_DM_GRAPH_UNARY_BATCH9 = [
    ("dm.diameter", [r"\bgraph diameter\b", r"\bdiameter of\b.*\bgraph\b"], "diameter", "Computing the diameter of the graph"),
    ("dm.articulation", [r"\barticulation points\b", r"\bcut vertices\b"], "articulation", "Finding the articulation points of"),
    ("dm.bridges", [r"\bbridge edges\b", r"\bgraph bridges\b"], "bridges", "Finding the bridge edges of"),
    ("dm.euler_circuit", [r"\beuler(?:ian)? circuit\b"], "euler_circuit", "Finding an Eulerian circuit in"),
    ("dm.hamiltonian", [r"\bhamiltonian path\b", r"\bhamiltonian cycle\b"], "hamiltonian", "Finding a Hamiltonian path in"),
    ("dm.spec_radius", [r"\bspectral radius\b.*\bgraph\b", r"\badjacency eigenvalues\b"], "spec_radius", "Computing the spectral radius of the graph"),
]
 
_DM_NUMS = [
    ("dm.multiset", [r"\bmultiset coefficient\b"], "multiset", ["n", "r"], "Computing the multiset coefficient", ["1", "1"]),
    ("dm.ramsey", [r"\bramsey number\b"], "ramsey", ["s", "t"], "Computing the Ramsey number bound", ["3", "3"]),
    ("dm.necklace", [r"\bnecklace count\b", r"\bnecklace combinatorics\b"], "necklace", ["n", "k"], "Counting distinct necklaces", ["4", "2"]),
    ("dm.partition_gf", [r"\bpartition generating function\b"], "partition_gf", ["terms"], "Computing the partition generating function", ["20"]),
    ("dm.master_theorem", [r"\bmaster theorem\b"], "master_theorem", ["a", "b", "p"], "Applying the Master Theorem", ["1", "2", "0"]),
    ("dm.akra_bazzi", [r"\bakra.bazzi\b"], "akra_bazzi", ["a", "b", "p"], "Applying the Akra-Bazzi method", ["1", "2", "0"]),
    ("dm.max_flow", [r"\bmax(?:imum)? flow\b"], "max_flow", ["src", "sink"], "Computing the maximum flow", ["0", "1"]),
]
 
INTENTS[0:0] = [Intent(n, _p(*p), _build_dm_set_binary(op, v)) for (n, p, op, v) in _DM_SET_BINARY]
INTENTS[0:0] = [Intent(n, _p(*p), _build_dm_set_unary(op, v)) for (n, p, op, v) in _DM_SET_UNARY]
INTENTS[0:0] = [Intent(n, _p(*p), _build_dm_graph_unary(op, v)) for (n, p, op, v) in _DM_GRAPH_UNARY_BATCH9]
INTENTS[0:0] = [Intent("dm.dfs", _p(r"\bdfs\b", r"\bdepth.first search\b"), _build_dm_graph_start("dfs", "Running DFS"))]
 
INTENTS[0:0] = [
    Intent(n, _p(*p), _build_nums("dm", op, k, v, d)) for (n, p, op, k, v, d) in _DM_NUMS[:-1]
]
INTENTS[0:0] = [
    # max_flow needs adj matrix + src/sink, not pure scalars -- bespoke builder
    Intent("dm.max_flow", _p(r"\bmax(?:imum)? flow\b"),
           lambda m, text, session: EngineCommand(
               (lambda adj, nums: f'dm:max_flow|{{"adj":{adj},"src":{jnum(nums[0] if nums else "0")},"sink":{jnum(nums[1] if len(nums) > 1 else "1")}}}')(
                   matrix_literal_to_json_rows(find_matrix_literal(text) or "[[]]"),
                   find_numbers(text[text.find(find_matrix_literal(text) or chr(0)) + len(find_matrix_literal(text) or ""):])),
               1, "Computing the maximum flow.", None)),
    Intent("dm.recurrence", _p(r"\blinear recurrence\b"), _build_dm_recurrence),
    Intent("dm.coin_change", _p(r"\bcoin change\b"), _build_dm_coin_change),
    Intent("dm.truth_table", _p(r"\btruth table\b"),
           lambda m, text, session: EngineCommand(
               f'dm:truth_table|{{"vars":{find_vector_literal(text) or "[]"}}}',
               0, "Building the truth table.", None)),
    Intent("dm.inclusion_exclusion", _p(r"\binclusion.exclusion\b"),
           lambda m, text, session: EngineCommand(
               f'dm:inclusion_excl|{{"sizes":{find_vector_literal(text) or "[]"}}}',
               0, "Applying inclusion-exclusion.", None)),
]
 
def _build_stat_vec2_keys(op: str, key1: str, key2: str, verb: str, extra_keys=None, extra_defaults=None):
    """Two vectors under specific (non x/y) JSON key names, e.g. p_chart's
    'defectives'/'n', or kaplan_meier's 't'/'status'."""
    def build(m: Match, text: str, session) -> EngineCommand:
        lists = []
        remaining = text
        for _ in range(2):
            lit = find_vector_literal(remaining)
            if lit:
                lists.append(lit)
                remaining = remaining[remaining.find(lit) + len(lit):]
        a = lists[0] if len(lists) > 0 else "[]"
        b = lists[1] if len(lists) > 1 else "[]"
        pairs = f'"{key1}":{a},"{key2}":{b}'
        if extra_keys:
            nums = find_numbers(remaining)
            defs = extra_defaults or (["0"] * len(extra_keys))
            vals = list(nums[:len(extra_keys)])
            if len(vals) < len(extra_keys):
                vals += defs[len(vals):len(extra_keys)]
            pairs += "," + ",".join(f'"{k}":{jnum(v)}' for k, v in zip(extra_keys, vals))
        return EngineCommand(f'stat:{op}|{{{pairs}}}', 1, f"{verb}.", None)
    return build
 
 
def _build_stat_subgroups(op: str, verb: str):
    def build(m: Match, text: str, session) -> EngineCommand:
        lit = find_matrix_literal(text)
        subgroups = matrix_literal_to_json_rows(lit) if lit else "[[]]"
        return EngineCommand(f'stat:{op}|{{"subgroups":{subgroups}}}', 1, f"{verb}.", None)
    return build
 
 
def _build_stat_log_rank(m: Match, text: str, session) -> EngineCommand:
    """log_rank compares two survival groups: (times1, status1, times2, status2)."""
    lists = []
    remaining = text
    for _ in range(4):
        lit = find_vector_literal(remaining)
        if lit:
            lists.append(lit)
            remaining = remaining[remaining.find(lit) + len(lit):]
    lists = (lists + ["[]"] * 4)[:4]
    t1, s1, t2, s2 = lists
    return EngineCommand(
        f'stat:log_rank|{{"x":{t1},"status":{s1},"t2":{t2},"status2":{s2}}}',
        1, "Running the log-rank test comparing the two survival groups.", None)
 
 
_STAT_BATCH7_VEC = [
    ("stat.c_chart", [r"\bc.chart\b"], "c_chart", [], "Running a c-chart", []),
    ("stat.np_chart", [r"\bnp.chart\b"], "np_chart", ["n"], "Running an np-chart", ["10"]),
    ("stat.durbin_watson", [r"\bdurbin.watson\b"], "durbin_watson", [], "Computing the Durbin-Watson statistic", []),
    ("stat.process_cap", [r"\bprocess capability\b"], "process_cap", ["LSL", "USL"], "Computing process capability", ["0", "1"]),
]
 
_STAT_BATCH7_TWO_NUMS = [
    ("stat.t_test_two2", [r"\btwo.sample t.test\b"], "t_test_two", ["alpha"], "Running a two-sample t-test", ["0.05"]),
    ("stat.t_test_paired", [r"\bpaired t.test\b"], "t_test_paired", ["alpha"], "Running a paired t-test", ["0.05"]),
]
 
_STAT_BATCH7_TABLE = [
    ("stat.chi_sq_indep2b", [r"\bchi.square(?:d)? goodness.of.fit\b(?=.*\[\[)"], "chi_sq_indep2", "Running a second-form chi-square independence test"),
]
 
INTENTS[0:0] = [Intent(n, _p(*p), _build_stat_vec_nums(op, k, v, d)) for (n, p, op, k, v, d) in _STAT_BATCH7_VEC]
INTENTS[0:0] = [Intent(n, _p(*p), _build_stat_two_sample_nums(op, k, v, d)) for (n, p, op, k, v, d) in _STAT_BATCH7_TWO_NUMS]
INTENTS[0:0] = [Intent(n, _p(*p), _build_stat_table(op, v)) for (n, p, op, v) in _STAT_BATCH7_TABLE]
 
INTENTS[0:0] = [
    Intent("stat.spearman", _p(r"\bspearman\b"), _build_stat_two_sample("spearman", "Computing Spearman's rank correlation")),
    Intent("stat.kendall", _p(r"\bkendall'?s? tau\b"), _build_stat_two_sample("kendall", "Computing Kendall's tau")),
    Intent("stat.chi_sq_test", _p(r"\bchi.square(?:d)? goodness.of.fit\b"),
           _build_stat_vec2_keys("chi_sq_test", "x", "expected", "Running a chi-square goodness-of-fit test", extra_keys=["alpha"], extra_defaults=["0.05"])),
    Intent("stat.bonferroni", _p(r"\bbonferroni correction\b"), _build_stat_groups("bonferroni", "Applying the Bonferroni correction")),
    Intent("stat.naive_bayes", _p(r"\bnaive bayes\b"), _build_stat_reg_diag("naive_bayes", "Running naive Bayes classification")),
    Intent("stat.p_chart", _p(r"\bp.chart\b"), _build_stat_vec2_keys("p_chart", "defectives", "n", "Running a p-chart")),
    Intent("stat.xbar_chart", _p(r"\bx.bar chart\b", r"\bxbar chart\b"), _build_stat_subgroups("xbar_chart", "Running an X-bar chart")),
    Intent("stat.r_chart", _p(r"\br.chart\b"), _build_stat_subgroups("r_chart", "Running an R-chart")),
    Intent("stat.s_chart", _p(r"\bs.chart\b"), _build_stat_subgroups("s_chart", "Running an S-chart")),
    Intent("stat.kaplan_meier", _p(r"\bkaplan.meier\b"), _build_stat_vec2_keys("kaplan_meier", "t", "status", "Computing the Kaplan-Meier survival estimate")),
    Intent("stat.hazard_rate", _p(r"\bhazard rate\b"), _build_stat_vec2_keys("hazard_rate", "t", "status", "Computing the hazard rate")),
    Intent("stat.total_prob", _p(r"\btotal probability\b"), _build_stat_vec2_keys("total_prob", "x", "likelihood", "Applying the law of total probability")),
    Intent("stat.bayes_ext", _p(r"\bextended bayes\b", r"\bbayes'? theorem\b.*\bmultiple\b"),
           _build_stat_vec2_keys("bayes_ext", "x", "likelihood", "Applying the extended Bayes' theorem", extra_keys=["event"], extra_defaults=["0"])),
    Intent("stat.markov_absorb", _p(r"\babsorbing markov chain\b"),
           lambda m, text, session: EngineCommand(
               f'stat:markov_absorb|{{"T":{matrix_literal_to_json_rows(find_matrix_literal(text) or "[[]]")},"absorbing":{find_vector_literal(text[text.find(find_matrix_literal(text) or chr(0)) + len(find_matrix_literal(text) or ""):]) or "[]"}}}',
               1, "Analyzing the absorbing Markov chain.", None)),
    Intent("stat.log_rank", _p(r"\blog.rank test\b"), _build_stat_log_rank),
]
 
# ─────────────────────────────────────────────────────────────────────────
# EXPANDED COVERAGE (batch 10) — Geometry's remaining ~40 operations.
# ─────────────────────────────────────────────────────────────────────────
 
def _build_geo_points(op: str, key: str, verb: str, extra_keys=None, extra_defaults=None):
    def build(m: Match, text: str, session) -> EngineCommand:
        lit = find_matrix_literal(text)
        pts = matrix_literal_to_json_rows(lit) if lit else "[[0,0]]"
        pairs = f'"{key}":{pts}'
        if extra_keys:
            rest = text[text.find(lit) + len(lit):] if lit else text
            nums = find_numbers(rest)
            defs = extra_defaults or (["0"] * len(extra_keys))
            vals = list(nums[:len(extra_keys)])
            if len(vals) < len(extra_keys):
                vals += defs[len(vals):len(extra_keys)]
            pairs += "," + ",".join(f'"{k}":{jnum(v)}' for k, v in zip(extra_keys, vals))
        return EngineCommand(f'geo:{op}|{{{pairs}}}', 1, f"{verb}.", None)
    return build
 
 
_GEO_NUMS = [
    ("geo.line_eq", [r"\bequation of\b.*\bline\b"], "line_eq", ["x1", "y1", "x2", "y2"], "Finding the equation of the line", ["0", "0", "1", "1"]),
    ("geo.line_intersect", [r"\bintersection of\b.*\blines\b", r"\bline intersection\b"], "line_intersect", ["a1", "b1", "c1", "a2", "b2", "c2"], "Finding the intersection of the two lines", ["1", "-1", "0", "1", "1", "2"]),
    ("geo.perpendicular", [r"\bperpendicular\b.*\bline\b.*\bpoint\b", r"\bperpendicular from\b"], "perpendicular", ["a", "b", "c", "px", "py"], "Finding the perpendicular through the point", ["1", "1", "0", "0", "0"]),
    ("geo.pt_line_dist", [r"\bdistance from\b.*\bpoint\b.*\bline\b", r"\bpoint.to.line distance\b"], "pt_line_dist", ["a", "b", "c", "px", "py"], "Computing the point-to-line distance", ["1", "1", "0", "0", "0"]),
    ("geo.angle_lines", [r"\bangle between\b.*\blines\b"], "angle_lines", ["m1", "m2"], "Computing the angle between the two lines", ["1", "-1"]),
    ("geo.classify_conic", [r"\bclassify\b.*\bconic\b"], "classify_conic", ["A", "B", "C", "D", "E", "F"], "Classifying the conic", ["1", "0", "1", "0", "0", "-1"]),
    ("geo.circle_gen", [r"\bcircle\b.*\bgeneral form\b"], "circle_gen", ["D", "E", "F"], "Converting the circle from general form", ["0", "0", "-1"]),
    ("geo.ellipse", [r"\bequation of\b.*\bellipse\b", r"\bellipse with center\b"], "ellipse", ["cx", "cy", "a", "b", "theta"], "Building the ellipse equation", ["0", "0", "2", "1", "0"]),
    ("geo.hyperbola", [r"\bequation of\b.*\bhyperbola\b", r"\bhyperbola with center\b"], "hyperbola", ["cx", "cy", "a", "b"], "Building the hyperbola equation", ["0", "0", "1", "1"]),
    ("geo.parabola", [r"\bequation of\b.*\bparabola\b", r"\bparabola with vertex\b"], "parabola", ["h", "k", "p"], "Building the parabola equation", ["0", "0", "1"]),
    ("geo.eccentricity", [r"\beccentricity of\b.*\bconic\b"], "eccentricity", ["A", "B", "C", "D", "E", "F"], "Computing the eccentricity of the conic", ["1", "0", "1", "0", "0", "-1"]),
    ("geo.polar_to_rect", [r"\bpolar to rectangular\b", r"\bpolar to cartesian\b"], "polar_to_rect", ["r", "theta"], "Converting from polar to rectangular coordinates", ["1", "0"]),
    ("geo.rect_to_polar", [r"\brectangular to polar\b", r"\bcartesian to polar\b"], "rect_to_polar", ["x", "y"], "Converting from rectangular to polar coordinates", ["1", "0"]),
    ("geo.rotate_2d", [r"\brotate\b.*\bpoint\b.*\bby\b", r"\brotate the point\b"], "rotate_2d", ["px", "py", "cx", "cy", "angle"], "Rotating the point", ["1", "0", "0", "0", "90"]),
    ("geo.scale_2d", [r"\bscale\b.*\bpoint\b"], "scale_2d", ["px", "py", "cx", "cy", "sx", "sy"], "Scaling the point", ["1", "1", "0", "0", "2", "2"]),
    ("geo.reflect_2d", [r"\breflect\b.*\bpoint\b"], "reflect_2d", ["px", "py", "ax", "ay", "bx", "by"], "Reflecting the point across the line", ["1", "1", "0", "0", "1", "0"]),
    ("geo.angle_3d", [r"\bangle between\b.*\bvectors\b.*\b3d\b", r"\bangle between\b.*\(.*,.*,.*\).*\(.*,.*,.*\)"], "angle_3d", ["ax", "ay", "az", "bx", "by", "bz"], "Computing the 3D angle between the vectors", ["1", "0", "0", "0", "1", "0"]),
    ("geo.dot_3d", [r"\b3d dot product\b", r"\bdot product\b.*\b3d\b"], "dot_3d", ["ax", "ay", "az", "bx", "by", "bz"], "Computing the 3D dot product", ["1", "0", "0", "0", "1", "0"]),
    ("geo.plane_3pts", [r"\bplane through\b.*\bpoints\b", r"\bplane\b.*\bthree points\b"], "plane_3pts", ["x1", "y1", "z1", "x2", "y2", "z2", "x3", "y3", "z3"], "Finding the plane through the three points", ["0", "0", "0", "1", "0", "0", "0", "1", "0"]),
    ("geo.plane_normal", [r"\bplane\b.*\bnormal vector\b"], "plane_normal", ["nx", "ny", "nz", "px", "py", "pz"], "Finding the plane from its normal vector", ["0", "0", "1", "0", "0", "0"]),
    ("geo.pt_plane_dist", [r"\bdistance from\b.*\bpoint\b.*\bplane\b"], "pt_plane_dist", ["a", "b", "c", "d", "px", "py", "pz"], "Computing the point-to-plane distance", ["0", "0", "1", "0", "0", "0", "1"]),
    ("geo.plane_line", [r"\bintersection of\b.*\bplane\b.*\bline\b", r"\bplane.line intersection\b"], "plane_line", ["a", "b", "c", "d", "lx", "ly", "lz", "dx", "dy", "dz"], "Finding the plane-line intersection", ["0", "0", "1", "0", "0", "0", "0", "0", "0", "1"]),
    ("geo.two_planes", [r"\bintersection of\b.*\btwo planes\b"], "two_planes", ["a1", "b1", "c1", "d1", "a2", "b2", "c2", "d2"], "Finding the intersection of the two planes", ["1", "0", "0", "0", "0", "1", "0", "0"]),
    ("geo.skew_lines", [r"\bskew lines\b"], "skew_lines", ["x1", "y1", "z1", "dx1", "dy1", "dz1", "x2", "y2", "z2", "dx2", "dy2", "dz2"], "Checking whether the two lines are skew", ["0", "0", "0", "1", "0", "0", "0", "1", "0", "0", "1", "0"]),
    ("geo.sphere_line", [r"\bintersection of\b.*\bsphere\b.*\bline\b", r"\bsphere.line intersection\b"], "sphere_line", ["cx", "cy", "cz", "r", "lx", "ly", "lz", "dx", "dy", "dz"], "Finding the sphere-line intersection", ["0", "0", "0", "1", "0", "0", "0", "1", "0", "0"]),
    ("geo.param_length", [r"\barc length\b.*\bparametric\b"], "param_length", ["t0", "t1"], "Computing the arc length of the parametric curve", ["0", "6.283185"]),
    ("geo.param_area", [r"\barea\b.*\bparametric\b"], "param_area", ["t0", "t1"], "Computing the area enclosed by the parametric curve", ["0", "6.283185"]),
    ("geo.curvature", [r"\bcurvature\b.*\bparametric\b", r"\bcurvature of\b.*\bcurve\b"], "curvature", ["t0"], "Computing the curvature of the curve", ["0"]),
    ("geo.tangent_normal", [r"\btangent and normal\b"], "tangent_normal", ["t0"], "Finding the tangent and normal vectors", ["0"]),
    ("geo.polar_area", [r"\barea\b.*\bpolar curve\b"], "polar_area", ["t0", "t1"], "Computing the area of the polar curve", ["0", "6.283185"]),
    ("geo.polar_length", [r"\barc length\b.*\bpolar curve\b"], "polar_length", ["t0", "t1"], "Computing the arc length of the polar curve", ["0", "6.283185"]),
]
 
INTENTS[0:0] = [Intent(n, _p(*p), _build_nums("geo", op, k, v, d)) for (n, p, op, k, v, d) in _GEO_NUMS]
 
INTENTS[0:0] = [
    Intent("geo.is_convex", _p(r"\bis\b.*\bpolygon\b.*\bconvex\b", r"\bconvex polygon\b"), _build_geo_points("is_convex", "vertices", "Checking whether this polygon is convex")),
    Intent("geo.convex_hull", _p(r"\bconvex hull\b"), _build_geo_points("convex_hull", "points", "Computing the convex hull")),
    Intent("geo.point_in_poly", _p(r"\bpoint (?:inside|in) (?:the )?polygon\b"), _build_geo_points("point_in_poly", "vertices", "Checking whether the point is inside the polygon", extra_keys=["px", "py"], extra_defaults=["0", "0"])),
    Intent("geo.circumcircle", _p(r"\bcircumcircle\b", r"\bcircumscribed circle\b"), _build_nums("geo", "circumcircle", ["x1", "y1", "x2", "y2", "x3", "y3"], "Finding the circumcircle of the triangle", ["0", "0", "1", "0", "0", "1"])),
    Intent("geo.incircle", _p(r"\bincircle\b", r"\binscribed circle\b"), _build_nums("geo", "incircle", ["x1", "y1", "x2", "y2", "x3", "y3"], "Finding the incircle of the triangle", ["0", "0", "1", "0", "0", "1"])),
    Intent("geo.midpoint_3d", _p(r"\bmidpoint\b.*\(.*,.*,.*\).*\(.*,.*,.*\)"), _build_nums("geo", "midpoint_3d", ["x1", "y1", "z1", "x2", "y2", "z2"], "Finding the 3D midpoint", ["0", "0", "0", "1", "1", "1"])),
    Intent("geo.affine", _p(r"\baffine transform"), lambda m, text, session: EngineCommand(
        f'geo:affine|{{"M":{matrix_literal_to_json_rows(find_matrix_literal(text) or "[[1,0],[0,1]]")},"px":0,"py":0}}',
        1, "Applying the affine transform.", None)),
]
 
def _build_geo_composite_tf(m: Match, text: str, session) -> EngineCommand:
    lit1 = find_matrix_literal(text)
    M1 = matrix_literal_to_json_rows(lit1) if lit1 else "[[1,0],[0,1]]"
    rest = text[text.find(lit1) + len(lit1):] if lit1 else text
    lit2 = find_matrix_literal(rest)
    M2 = matrix_literal_to_json_rows(lit2) if lit2 else "[[1,0],[0,1]]"
    return EngineCommand(f'geo:composite_tf|{{"M1":{M1},"M2":{M2}}}', 1, "Composing the two transforms.", None)
 
 
INTENTS[0:0] = [Intent("geo.composite_tf", _p(r"\bcomposite transform"), _build_geo_composite_tf)]
 
INTENTS[0:0] = [
    Intent("geo.polygon_area", _p(r"\barea of\b.*\bpolygon\b"), _build_geo_points("polygon_area", "vertices", "Computing the polygon's area")),
    Intent("geo.envelope", _p(r"\benvelope of\b.*\bfamily\b.*\bcurves\b", r"\benvelope curve\b"),
           lambda m, text, session: EngineCommand(
               f'geo:envelope|{{"F":{jv(clean_expression(text.split("of",1)[-1].strip()))},"x":"x","y":"y","param":"c"}}',
               0, "Finding the envelope of the family of curves.", None)),
]
 
# ─────────────────────────────────────────────────────────────────────────
# EXPANDED COVERAGE (batch 11) — Complex Analysis's remaining 15 operations
# (residue_theorem and schwarz_christoffel skipped: both need an array of
# poles/prevertices with per-item structure that doesn't have a clean
# singlntence phrasing).
# ─────────────────────────────────────────────────────────────────────────
 
def _build_ca_expr1_nums(op: str, num_keys, verb: str, defaults=None, expr_key="f"):
    def build(m: Match, text: str, session) -> EngineCommand:
        expr = clean_expression(m.group("expr"))
        rest = text[text.find(m.group("expr")) + len(m.group("expr")):]
        nums = find_numbers(rest)
        defs = defaults or (["0"] * len(num_keys))
        vals = list(nums[:len(num_keys)])
        if len(vals) < len(num_keys):
            vals += defs[len(vals):len(num_keys)]
        pairs = ",".join(f'"{k}":{jnum(v)}' for k, v in zip(num_keys, vals))
        return EngineCommand(f'ca:{op}|{{"{expr_key}":{jv(expr)}{"," + pairs if pairs else ""}}}',
                              0, f"{verb} {expr}.", expr)
    return build
 
 
def _build_ca_cauchy_riemann(m: Match, text: str, session) -> EngineCommand:
    u, v = clean_expression(m.group("u")), clean_expression(m.group("v"))
    nums = find_numbers(text[text.find(m.group("v")) + len(m.group("v")):])
    x0, y0 = (nums + ["0", "0"])[:2]
    return EngineCommand(f'ca:cauchy_riemann|{{"u":{jv(u)},"v":{jv(v)},"x0":{jnum(x0)},"y0":{jnum(y0)}}}',
                          0, f"Checking the Cauchy-Riemann equations for u={u}, v={v}.", None)
 
 
def _build_ca_is_analytic(m: Match, text: str, session) -> EngineCommand:
    u, v = clean_expression(m.group("u")), clean_expression(m.group("v"))
    return EngineCommand(f'ca:is_analytic|{{"u":{jv(u)},"v":{jv(v)}}}', 0,
                          f"Checking whether u={u}, v={v} is analytic.", None)
 
 
_CA_EXPR1 = [
    ("ca.harmonic_conj", [r"\bharmonic conjugate of\s+(?P<expr>.+?)$"], "harmonic_conj", [], "Finding the harmonic conjugate of"),
    ("ca.laplacian_check", [r"\bcheck\b.*\bharmonic\b.*?(?P<expr>.+?)$", r"\bis\s+(?P<expr>.+?)\s+harmonic\b"], "laplacian_check", [], "Checking whether this function is harmonic:"),
    ("ca.improper_res", [r"\bimproper integral\b.*\bresidues?\b.*?(?P<expr>.+?)$"], "improper_res", [], "Evaluating the improper integral by residues:"),
    ("ca.trig_int_res", [r"\btrigonometric integral\b.*\bresidues?\b.*?(?P<expr>.+?)$"], "trig_int_res", [], "Evaluating the trigonometric integral by residues:"),
]
_CA_EXPR1_NUMS = [
    ("ca.taylor_c", [r"\bcomplex taylor series of\s+(?P<expr>.+?)\s+at\b"], "taylor_c", ["re", "im", "N"], "Building the complex Taylor series of", ["0", "0", "6"]),
    ("ca.laurent", [r"\blaurent series of\s+(?P<expr>.+?)\s+at\b"], "laurent", ["re", "im", "N"], "Building the Laurent series of", ["0", "0", "6"]),
    ("ca.classify_sing", [r"\bclassify\b.*\bsingularity\b.*\bof\s+(?P<expr>.+?)\s+at\b"], "classify_sing", ["re", "im"], "Classifying the singularity of", ["0", "0"]),
    ("ca.radius_conv", [r"\bradius of convergence\b.*\bof\s+(?P<expr>.+?)$"], "radius_conv", ["a", "b"], "Finding the radius of convergence of", ["0", "1"]),
]
 
INTENTS[0:0] = [Intent(n, _p(*p), _build_ca_expr1_nums(op, [], v)) for (n, p, op, _nk, v) in _CA_EXPR1]
INTENTS[0:0] = [Intent(n, _p(*p), _build_ca_expr1_nums(op, k, v, d)) for (n, p, op, k, v, d) in _CA_EXPR1_NUMS]
 
INTENTS[0:0] = [
    Intent("ca.cauchy_riemann", _p(r"\bcauchy.riemann\b.*\bu\s*=\s*(?P<u>.+?)\s*,?\s*v\s*=\s*(?P<v>.+?)$"), _build_ca_cauchy_riemann),
    Intent("ca.is_analytic", _p(r"\bis\b.*\bu\s*=\s*(?P<u>.+?)\s*,?\s*v\s*=\s*(?P<v>.+?)\s*analytic\b"), _build_ca_is_analytic),
    Intent("ca.joukowski", _p(r"\bjoukowski transform\b"), _build_nums("ca", "joukowski", ["re", "im", "lambda"], "Applying the Joukowski transform", ["1", "0", "1"])),
    Intent("ca.zeta", _p(r"\briemann zeta\b"), _build_nums("ca", "zeta", ["re", "im", "terms"], "Evaluating the Riemann zeta function", ["2", "0", "1000"])),
    Intent("ca.gamma_c", _p(r"\bcomplex gamma function\b", r"\bgamma function\b.*\bcomplex\b"), _build_nums("ca", "gamma_c", ["re", "im"], "Evaluating the complex Gamma function", ["1", "0"])),
    Intent("ca.beta_c", _p(r"\bcomplex beta function\b"), _build_nums("ca", "beta_c", ["re1", "im1", "re2", "im2"], "Evaluating the complex Beta function", ["1", "0", "1", "0"])),
    Intent("ca.contour_int", _p(r"\bcontour integral\b"),
           lambda m, text, session: EngineCommand(
               (lambda expr, nums: f'ca:contour_int|{{"f":{jv(expr)},"x":"cos(t)","y":"sin(t)","t":"t","a":{jnum(nums[0] if nums else "0")},"b":{jnum(nums[1] if len(nums)>1 else "6.283185")}}}')(
                   clean_expression(extract_between(text, ["contour integral of", "contour integral"]) or "1"),
                   find_numbers(text)),
               0, "Evaluating the contour integral.", None)),
]
 
# ─────────────────────────────────────────────────────────────────────────
# EXPANDED COVERAGE (batch 12) — Numerical Analysis's remaining 35 ops.
# ─────────────────────────────────────────────────────────────────────────
 
def _build_na_root_1x0(op: str, verb: str, extra_keys=None, extra_defaults=None):
    """Root/deriv-finding ops needing f, x, and one seed point x0 (+ maybe more nums)."""
    def build(m: Match, text: str, session) -> EngineCommand:
        expr = clean_expression(m.group("expr"))
        var = first_var(expr)
        rest = text[text.find(m.group("expr")) + len(m.group("expr")):]
        nums = find_numbers(rest)
        keys = ["x0"] + (extra_keys or [])
        defs = ["1"] + (extra_defaults or [])
        vals = list(nums[:len(keys)])
        if len(vals) < len(keys):
            vals += defs[len(vals):len(keys)]
        pairs = ",".join(f'"{k}":{jnum(v)}' for k, v in zip(keys, vals))
        return EngineCommand(f'na:{op}|{{"f":{jv(expr)},"x":{jv(var)},{pairs},"tol":1e-10,"maxIter":100}}',
                              1, f"{verb} {expr}.", expr)
    return build
 
 
def _build_na_interval(op: str, verb: str, extra_json=""):
    def build(m: Match, text: str, session) -> EngineCommand:
        expr = clean_expression(m.group("expr"))
        var = first_var(expr)
        a, b = m.group("a"), m.group("b")
        return EngineCommand(
            f'na:{op}|{{"f":{jv(expr)},"x":{jv(var)},"a":{jnum(a)},"b":{jnum(b)}{extra_json}}}',
            1, f"{verb} {expr} on [{a}, {b}].", expr)
    return build
 
 
def _build_na_interp(op: str, verb: str, has_ders=False):
    def build(m: Match, text: str, session) -> EngineCommand:
        lists = []
        remaining = text
        for _ in range(3 if has_ders else 2):
            lit = find_vector_literal(remaining)
            if lit:
                lists.append(lit)
                remaining = remaining[remaining.find(lit) + len(lit):]
        xs = lists[0] if len(lists) > 0 else "[]"
        ys = lists[1] if len(lists) > 1 else "[]"
        nums = find_numbers(remaining)
        xeval = nums[0] if nums else "0"
        extra = ""
        if has_ders:
            ders = lists[2] if len(lists) > 2 else "[]"
            extra = f',"ders":{ders}'
        return EngineCommand(f'na:{op}|{{"xs":{xs},"ys":{ys}{extra},"xeval":{jnum(xeval)}}}',
                              1, f"{verb} at x={xeval}.", None)
    return build
 
 
def _build_na_linalg(op: str, verb: str, extra_keys=None, extra_defaults=None):
    def build(m: Match, text: str, session) -> EngineCommand:
        lit = find_matrix_literal(text)
        A = matrix_literal_to_json_rows(lit) if lit else (session.last_matrix or "[[]]")
        rest = text[text.find(lit) + len(lit):] if lit else text
        b_lit = find_vector_literal(rest)
        b = b_lit if b_lit else "[]"
        rest2 = rest[rest.find(b_lit) + len(b_lit):] if b_lit else rest
        pairs = f'"A":{A}'
        if b_lit:
            pairs += f',"b":{b}'
        if extra_keys:
            nums = find_numbers(rest2)
            defs = extra_defaults or (["0"] * len(extra_keys))
            vals = list(nums[:len(extra_keys)])
            if len(vals) < len(extra_keys):
                vals += defs[len(vals):len(extra_keys)]
            pairs += "," + ",".join(f'"{k}":{jnum(v)}' for k, v in zip(extra_keys, vals))
        session.last_matrix = A
        return EngineCommand(f'na:{op}|{{{pairs},"tol":1e-10,"maxIter":100}}', 1, f"{verb}.", None)
    return build
 
 
E2 = r"(?P<expr>.+?)"
 
_NA_ROOT1X0 = [
    ("na.secant2", [rf"\bsecant method\b.*?{E2}\s+(?:starting|near|from)\s+(?P<extra>.+?)$"], "secant", "Running the secant method on"),
    ("na.fixed_point", [rf"\bfixed.point iteration\b.*?{E2}$"], "fixed_point", "Running fixed-point iteration on"),
]
 
_NA_INTERVAL = [
    ("na.romberg", [rf"\bromberg integration\b.*?{E2}\s+from\s+(?P<a>{NUMLIKE})\s+to\s+(?P<b>{NUMLIKE})$"], "romberg", "Integrating via Romberg integration"),
    ("na.adaptive_quad", [rf"\badaptive quadrature\b.*?{E2}\s+from\s+(?P<a>{NUMLIKE})\s+to\s+(?P<b>{NUMLIKE})$"], "adaptive_quad", "Integrating via adaptive quadrature"),
    ("na.simpsons38", [rf"\bsimpson'?s? 3/8 rule\b.*?{E2}\s+from\s+(?P<a>{NUMLIKE})\s+to\s+(?P<b>{NUMLIKE})$"], "simpsons38", "Integrating via Simpson's 3/8 rule"),
    ("na.gauss_legendre", [rf"\bgauss.legendre quadrature\b.*?{E2}\s+from\s+(?P<a>{NUMLIKE})\s+to\s+(?P<b>{NUMLIKE})$"], "gauss_legendre", "Integrating via Gauss-Legendre quadrature"),
    ("na.regula_falsi", [rf"\bregula falsi\b.*?{E2}\s+(?:on|from)\s+(?P<a>{NUMLIKE})\s+to\s+(?P<b>{NUMLIKE})$"], "regula_falsi", "Running the method of regula falsi on"),
    ("na.brent", [rf"\bbrent'?s? method\b.*?{E2}\s+(?:on|from)\s+(?P<a>{NUMLIKE})\s+to\s+(?P<b>{NUMLIKE})$"], "brent", "Running Brent's method on"),
]
 
_NA_INTERP = [
    ("na.lagrange", [r"\blagrange interpolation\b"], "lagrange", "Interpolating via Lagrange's method", False),
    ("na.newton_dd", [r"\bnewton'?s? divided differences?\b"], "newton_dd", "Interpolating via Newton's divided differences", False),
    ("na.neville", [r"\bneville'?s? method\b"], "neville", "Interpolating via Neville's method", False),
    ("na.cubic_spline", [r"\bcubic spline interpolation\b"], "cubic_spline", "Interpolating via cubic spline", False),
    ("na.linear_spline", [r"\blinear spline interpolation\b"], "linear_spline", "Interpolating via linear spline", False),
    ("na.hermite", [r"\bhermite interpolation\b"], "hermite", "Interpolating via Hermite interpolation", True),
]
 
INTENTS[0:0] = [Intent(n, _p(*p), _build_na_root_1x0(op, v)) for (n, p, op, v) in _NA_ROOT1X0]
INTENTS[0:0] = [Intent(n, _p(*p), _build_na_interval(op, v)) for (n, p, op, v) in _NA_INTERVAL]
INTENTS[0:0] = [Intent(n, _p(*p), _build_na_interp(op, v, hd)) for (n, p, op, v, hd) in _NA_INTERP]
 
_NA_DIFF = [
    ("na.forward_diff", [r"\bforward difference\b\s+of\s+" + E2 + r"\s+at\s+(?P<extra>{NUMLIKE})$".format(NUMLIKE=NUMLIKE)], "forward_diff"),
    ("na.central_diff", [r"\bcentral difference\b\s+of\s+" + E2 + r"\s+at\s+(?P<extra>{NUMLIKE})$".format(NUMLIKE=NUMLIKE)], "central_diff"),
    ("na.second_deriv", [r"\bsecond derivative\b.*\bnumerical(?:ly)?\b\s+of\s+" + E2 + r"\s+at\s+(?P<extra>{NUMLIKE})$".format(NUMLIKE=NUMLIKE)], "second_deriv"),
    ("na.richardson_diff", [r"\brichardson extrapolation\b\s+of\s+" + E2 + r"\s+at\s+(?P<extra>{NUMLIKE})$".format(NUMLIKE=NUMLIKE)], "richardson_diff"),
]
 
def _build_na_diff(op: str):
    def build(m: Match, text: str, session) -> EngineCommand:
        expr = clean_expression(m.group("expr"))
        var = first_var(expr)
        x0 = m.group("extra") if "extra" in m.groupdict() and m.group("extra") else "0"
        return EngineCommand(f'na:{op}|{{"f":{jv(expr)},"x":{jv(var)},"x0":{jnum(x0)}}}',
                              1, f"Computing the {op.replace('_', ' ')} of {expr} at x={x0}.", expr)
    return build
 
INTENTS[0:0] = [Intent(n, _p(*p), _build_na_diff(op)) for (n, p, op) in _NA_DIFF]
 
def _build_na_muller(m: Match, text: str, session) -> EngineCommand:
    expr = clean_expression(m.group("expr"))
    var = first_var(expr)
    rest = text[text.find(m.group("expr")) + len(m.group("expr")):]
    nums = find_numbers(rest)
    x0, x1, x2 = (nums + ["0", "1", "0.5"])[:3]
    return EngineCommand(
        f'na:muller|{{"f":{jv(expr)},"x":{jv(var)},"x0":{jnum(x0)},"x1":{jnum(x1)},"x2":{jnum(x2)},"tol":1e-10}}',
        1, f"Running Muller's method on {expr}.", expr)
 
 
INTENTS[0:0] = [
    Intent("na.muller", _p(rf"\bmuller'?s? method\b.*?{E2}\s+(?:near|starting|with)\s+.+$"), _build_na_muller),
    Intent("na.poly_fit", _p(r"\bpolynomial fit(?:ting)?\b"), _build_na_interp("poly_fit", "Fitting a polynomial", False)),
    Intent("na.cheb_nodes", _p(r"\bchebyshev nodes\b"), _build_nums("na", "cheb_nodes", ["n", "a", "b"], "Computing Chebyshev nodes", ["5", "-1", "1"])),
    Intent("na.gauss_chebyshev", _p(r"\bgauss.chebyshev quadrature\b.*?" + E2 + r"$"), _build_na_root_1x0("gauss_chebyshev", "Integrating via Gauss-Chebyshev quadrature")),
    Intent("na.gauss_laguerre", _p(r"\bgauss.laguerre quadrature\b.*?" + E2 + r"$"), _build_na_root_1x0("gauss_laguerre", "Integrating via Gauss-Laguerre quadrature")),
    Intent("na.gauss_hermite", _p(r"\bgauss.hermite quadrature\b.*?" + E2 + r"$"), _build_na_root_1x0("gauss_hermite", "Integrating via Gauss-Hermite quadrature")),
    Intent("na.jacobi_iter", _p(r"\bjacobi iteration\b.*\[\["), _build_na_linalg("jacobi_iter", "Solving via Jacobi iteration")),
    Intent("na.gauss_seidel", _p(r"\bgauss.seidel\b.*\[\["), _build_na_linalg("gauss_seidel", "Solving via Gauss-Seidel iteration")),
    Intent("na.sor", _p(r"\bsor iteration\b", r"\bsuccessive over.relaxation\b"), _build_na_linalg("sor", "Solving via SOR iteration", extra_keys=["omega"], extra_defaults=["1.5"])),
    Intent("na.conj_grad", _p(r"\bconjugate gradient\b"), _build_na_linalg("conj_grad", "Solving via the conjugate gradient method")),
    Intent("na.cond_num", _p(r"\bcondition number\b.*\bnumerically\b"), _build_na_linalg("cond_num", "Computing the condition number numerically")),
    Intent("na.power_iter", _p(r"\bpower iteration\b"), _build_na_linalg("power_iter", "Running power iteration")),
    Intent("na.inverse_iter", _p(r"\binverse iteration\b"), _build_na_linalg("inverse_iter", "Running inverse iteration", extra_keys=["shift"], extra_defaults=["0"])),
    Intent("na.qr_num", _p(r"\bqr algorithm\b.*\bnumerical\b", r"\bnumerical qr algorithm\b"), _build_na_linalg("qr_num", "Running the numerical QR algorithm")),
    Intent("na.roundoff", _p(r"\bround.off error\b"), _build_nums("na", "roundoff", ["computed", "exact"], "Computing the round-off error", ["0", "0"])),
    Intent("na.trunc_error", _p(r"\btruncation error\b"), _build_nums("na", "trunc_error", ["order", "h"], "Computing the truncation error", ["2", "0.1"])),
    Intent("na.conv_order", _p(r"\border of convergence\b"), lambda m, text, session: EngineCommand(
        f'na:conv_order|{{"errors":{find_vector_literal(text) or "[]"}}}', 1, "Estimating the order of convergence.", None)),
]
 
# ─────────────────────────────────────────────────────────â───────────
# EXPANDED COVERAGE (batch 13) — Abstract Algebra's remaining ~35 ops.
# ─────────────────────────────────────────────────────────────────────────
 
def _build_aa_table(op: str, verb: str, extra_keys=None, extra_defaults=None):
    def build(m: Match, text: str, session) -> EngineCommand:
        lit = find_matrix_literal(text)
        table = matrix_literal_to_json_rows(lit) if lit else "[[]]"
        pairs = f'"table":{table}'
        if extra_keys:
            rest = text[text.find(lit) + len(lit):] if lit else text
            nums = find_numbers(rest)
            defs = extra_defaults or (["0"] * len(extra_keys))
            vals = list(nums[:len(extra_keys)])
            if len(vals) < len(extra_keys):
                vals += defs[len(vals):len(extra_keys)]
            pairs += "," + ",".join(f'"{k}":{jnum(v)}' for k, v in zip(extra_keys, vals))
        return EngineCommand(f'aa:{op}|{{{pairs}}}', 0, f"{verb}.", None)
    return build
 
 
def _build_aa_poly2(op: str, verb: str, has_mod=True):
    def build(m: Match, text: str, session) -> EngineCommand:
        lists = []
        remaining = text
        for _ in range(2):
            lit = find_vector_literal(remaining)
            if lit:
                lists.append(lit)
                remaining = remaining[remaining.find(lit) + len(lit):]
        A = lists[0] if len(lists) > 0 else "[]"
        B = lists[1] if len(lists) > 1 else "[]"
        pairs = f'"A":{A},"B":{B}'
        if has_mod:
            nums = find_numbers(remaining)
            mod = nums[0] if nums else "0"
            pairs += f',"mod":{jnum(mod)}'
        return EngineCommand(f'aa:{op}|{{{pairs}}}', 0, f"{verb} {A} and {B}.", None)
    return build
 
 
def _build_aa_poly1(op: str, verb: str, num_keys=("p",), num_defaults=("2",)):
    def build(m: Match, text: str, session) -> EngineCommand:
        poly = find_vector_literal(text) or "[]"
        rest = text[text.find(poly) + len(poly):] if poly != "[]" else text
        nums = find_numbers(rest)
        defs = list(num_defaults)
        vals = list(nums[:len(num_keys)])
        if len(vals) < len(num_keys):
            vals += defs[len(vals):len(num_keys)]
        pairs = ",".join(f'"{k}":{jnum(v)}' for k, v in zip(num_keys, vals))
        return EngineCommand(f'aa:{op}|{{"poly":{poly},{pairs}}}', 0, f"{verb} {poly}.", None)
    return build
 
 
_AA_NUMS = [
    ("aa.direct_product", [r"\bdirect product of\b.*\bgroups\b", r"\bgroup direct product\b"], "direct_product", ["m", "n"], "Computing the direct product of the groups", ["2", "3"]),
    ("aa.lagrange_thm", [r"\blagrange'?s? theorem\b"], "lagrange", ["sub", "grp"], "Applying Lagrange's theorem", ["2", "6"]),
    ("aa.sylow", [r"\bsylow\b"], "sylow", ["p", "n"], "Finding the Sylow subgroups", ["2", "12"]),
    ("aa.hom", [r"\bgroup homomorphism\b"], "hom", ["m", "n", "phi"], "Checking the group homomorphism", ["6", "3", "1"]),
    ("aa.is_int_domain", [r"\bis\b.*\bintegral domain\b"], "is_int_domain", ["n"], "Checking whether this is an integral domain:", ["1"]),
    ("aa.is_field_aa", [r"\bis\b.*\ba field\b"], "is_field_aa", ["n"], "Checking whether this is a field:", ["1"]),
    ("aa.cyclotomic", [r"\bcyclotomic polynomial\b"], "cyclotomic", ["n"], "Finding the cyclotomic polynomial for", ["1"]),
    ("aa.galois_field", [r"\bgalois field\b", r"\bgf\(\d"], "galois_field", ["p", "n"], "Constructing the Galois field", ["2", "1"]),
    ("aa.mult_order", [r"\bmultiplicative order\b"], "mult_order", ["a", "p"], "Finding the multiplicative order of", ["1", "2"]),
    ("aa.prim_element", [r"\bprimitive element\b"], "prim_element", ["p"], "Finding a primitive element of", ["2"]),
    ("aa.field_ext_num", [r"\bfield extension\b(?!.*minpoly)"], "field_ext", ["p"], "Building the field extension over", ["2"]),
    ("aa.field_char", [r"\bfield characteristic\b", r"\bcharacteristic of\b.*\bfield\b"], "field_char", ["p", "n"], "Finding the field characteristic", ["2", "1"]),
    ("aa.ideal_gen", [r"\bideal generated by\b"], "ideal_gen", ["n", "gen"], "Finding the ideal generated by", ["12", "3"]),
    ("aa.quotient_ring", [r"\bquotient ring\b"], "quotient_ring", ["n", "ideal"], "Building the quotient ring", ["12", "3"]),
]
 
INTENTS[0:0] = [Intent(n, _p(*p), _build_nums("aa", op, k, v, d)) for (n, p, op, k, v, d) in _AA_NUMS]
 
INTENTS[0:0] = [
    Intent("aa.is_group", _p(r"\bis\b.*\ba group\b"), _build_aa_table("is_group", "Checking whether this is a group")),
    Intent("aa.is_abelian", _p(r"\bis\b.*\babelian\b"), _build_aa_table("is_abelian", "Checking whether this group is abelian")),
    Intent("aa.group_order_table", _p(r"\border of\b.*\bgroup\b.*\[\["), _build_aa_table("group_order", "Computing the order of the group")),
    Intent("aa.conjugacy", _p(r"\bconjugacy classes\b"), _build_aa_table("conjugacy", "Finding the conjugacy classes")),
    Intent("aa.center", _p(r"\bcenter of\b.*\bgroup\b"), _build_aa_table("center", "Finding the center of the group")),
    Intent("aa.commutator", _p(r"\bcommutator subgroup\b"), _build_aa_table("commutator", "Finding the commutator subgroup")),
    Intent("aa.orbit_stab", _p(r"\borbit.stabili[sz]er\b"), _build_aa_table("orbit_stab", "Applying the orbit-stabilizer theorem", extra_keys=["element"], extra_defaults=["0"])),
    Intent("aa.subgroups2", _p(r"\bsubgroups of\b.*\bcyclic\b", r"\ball subgroups of\b"), _build_nums("aa", "subgroups", ["n"], "Finding all subgroups of the cyclic group of order", ["6"])),
 
    Intent("aa.poly_add", _p(r"\badd\b.*\bpolynomials\b"), _build_aa_poly2("poly_add", "Adding the polynomials")),
    Intent("aa.poly_sub", _p(r"\bsubtract\b.*\bpolynomials\b"), _build_aa_poly2("poly_sub", "Subtracting the polynomials")),
    Intent("aa.poly_mul", _p(r"\bmultiply\b.*\bpolynomials\b"), _build_aa_poly2("poly_mul", "Multiplying the polynomials")),
    Intent("aa.poly_div", _p(r"\bdivide\b.*\bpolynomials\b"), _build_aa_poly2("poly_div", "Dividing the polynomials")),
    Intent("aa.poly_gcd", _p(r"\bgcd of\b.*\bpolynomials\b", r"\bpolynomial gcd\b"), _build_aa_poly2("poly_gcd", "Finding the GCD of the polynomials")),
    Intent("aa.poly_euclid", _p(r"\beuclidean algorithm\b.*\bpolynomials\b"), _build_aa_poly2("poly_euclid", "Running the Euclidean algorithm on the polynomials")),
]

def _build_aa_poly_eval(m: Match, text: str, session) -> EngineCommand:
    poly = find_vector_literal(text) or "[]"
    xm = re.search(r"\bat\s+x\s*=\s*(" + NUMLIKE + r")", text, re.IGNORECASE)
    modm = re.search(r"\bmod\s+(" + NUMLIKE + r")", text, re.IGNORECASE)
    x = xm.group(1) if xm else "0"
    mod = modm.group(1) if modm else "2"
    return EngineCommand(f'aa:poly_eval|{{"poly":{poly},"x":{jnum(x)},"mod":{jnum(mod)}}}', 0,
                          f"Evaluating the polynomial {poly}.", None)
 
 
INTENTS[0:0] = [Intent("aa.poly_eval", _p(r"\bevaluate\b.*\bpolynomial\b.*\bmod\b"), _build_aa_poly_eval)]
Intent("aa.poly_irred", _p(r"\bis\b.*\bpolynomial\b.*\birreducible\b", r"\birreducible polynomial\b"), _build_aa_poly1("poly_irred", "Checking whether this polynomial is irreducible")),
Intent("aa.poly_factor", _p(r"\bfactor\b.*\bpolynomial\b"), _build_aa_poly1("poly_factor", "Factoring the polynomial")),
 
def _build_aa_two_perms(op: str, key1: str, key2: str, verb: str):
    def build(m: Match, text: str, session) -> EngineCommand:
        vecs = []
        remaining = text
        for _ in range(2):
            lit = find_vector_literal(remaining)
            if lit:
                vecs.append(lit)
                remaining = remaining[remaining.find(lit) + len(lit):]
        a = vecs[0] if len(vecs) > 0 else "[]"
        b = vecs[1] if len(vecs) > 1 else "[]"
        return EngineCommand(f'aa:{op}|{{"{key1}":{a},"{key2}":{b}}}', 0, f"{verb}.", None)
    return build
 
 
 
INTENTS[0:0] = [
    Intent("aa.perm_cycle", _p(r"\bcycle notation\b"), _build_aa_perm_unary("perm_cycle", "Writing in cycle notation")),
    Intent("aa.perm_compose", _p(r"\bcompose\b.*\bpermutations\b"), _build_aa_two_perms("perm_compose", "p", "q", "Composing the permutations")),
    Intent("aa.perm_conjugate", _p(r"\bconjugate\b.*\bpermutation\b"), _build_aa_two_perms("perm_conjugate", "p", "q", "Conjugating the permutation")),
 
    Intent("aa.field_ext", _p(r"\bfield extension\b.*\bminpoly\b"), lambda m, text, session: EngineCommand(
        f'aa:field_ext|{{"p":{jnum((find_numbers(text)+["2"])[0])},"minpoly":{find_vector_literal(text) or "[]"}}}',
        0, "Building the field extension.", None)),
    Intent("aa.burnside", _p(r"\bburnside'?s? lemma\b"), lambda m, text, session: EngineCommand(
        f'aa:burnside|{{"orbits":{find_matrix_literal(text) or "[[]]"}}}',
        0, "Applying Burnside's lemma.", None)),
    Intent("aa.crt_ring", _p(r"\bchinese remainder\b.*\bring\b", r"\bcrt ring\b"), lambda m, text, session: EngineCommand(
        f'aa:crt_ring|{{"mods":{find_vector_literal(text) or "[]"}}}',
        0, "Applying the Chinese Remainder Theorem for rings.", None)),
]

INTENTS[0:0] = [Intent("aa.perm_parity", _p(r"\bparity of\b.*\bpermutation\b"), _build_aa_perm_unary("perm_parity", "Computing the parity of"))]

# -------------------------------------------------------------------------
# EXPANDED COVERAGE (batch 14) - Calculus' remaining 15 operations
# -------------------------------------------------------------------------

def _build_calc_expr_nums(op: str, extra_str_keys, num_keys, verb: str, defaults=None):
    """expr -> fixed string keys (e.g. varX/varY defaults) + trailing numeric bounds."""
    def build(m: Match, text: str, session) -> EngineCommand:
        expr = clean_expression(m.group("expr"))
        rest = text[text.find(m.group("expr")) + len(m.group("expr")):]
        nums = find_numbers(rest)
        defs = defaults or (["0"] * len(num_keys))
        vals = list(nums[:len(num_keys)])
        if len(vals) < len(num_keys):
            vals += defs[len(vals):len(num_keys)]
        pairs = ",".join(f'"{k}":{jnoum(v)}' for k, v in zip(num_keys, vals))
        str_pairs = ",".join(f'"{k}":{jv(v)}' for k, v in extra_str_keys.items())
        return EngineCommand(f'calc:{op}|{{"expr":{jv(expr)}{"," + str_pairs if str_pairs else ""}{"," + pairs if pairs else ""}}}', 0, f"{verb} {expr}.", expr)
    return build


_CALC_EXPR_NUMS = [
        ("calc.double_int", [rf"\bdouble integral of\s+{E}$"], "double_int", {"varX": "x", "varY": "y"}, ["ax", "bx", "ay", "by"], "Computing the double integral of", ["0", "1", "0", "1"]),
    ("calc.triple_int", [rf"\btriple integral of\s+{E}$"], "triple_int", {"varX": "x", "varY": "y", "varZ": "z"}, ["ax", "bx", "ay", "by", "az", "bz"], "Computing the triple integral of", ["0", "1", "0", "1", "0", "1"]),
    ("calc.polar_int", [rf"\bpolar (?:double )?integral of\s+{E}$"], "polar_int", {"varR": "r", "varT": "theta"}, ["ar", "br", "at", "bt"], "Computing the polar integral of", ["0", "1", "0", "6.283185"]),
    ("calc.numerical_int", [rf"\bnumerically integrate\s+{E}\s+from\s+(?P<a>{NUMLIKE})\s+to\s+(?P<b>{NUMLIKE})$"], "numerical_int", {"var": "x"}, [], "Numerically integrating", []),
    ("calc.fourier", [rf"\bfourier series of\s+{E}$"], "fourier", {"var": "x"}, ["T", "N"], "Computing the Fourier series of", ["6.283185", "5"]),
    ("calc.optimize_1d", [rf"\boptimi[sz]e\s+{E}\s+on\s*\[?(?P<a>{NUMLIKE}),\s*(?P<b>{NUMLIKE})\]?$"], "optimize_1d", {"var": "x"}, [], "Finding the critical points of", []),
    ("calc.greens", [rf"\bgreen'?s? theorem\b.*?P\s*=\s*(?P<P>.+?)\s*,\s*Q\s*=\s*(?P<Q>.+?)$"], "greens", {}, [], "Applying Green's theorem", []),
    ("calc.line_integral", [rf"\bline integral of\s+{E}$"], "line_integral", {"varX": "x", "varY": "y", "paramX": "cos(t)", "param": "t"}, ["a", "b"], "Computing the line integral of", ["0", "6.283185"]),
    ("calc.surface_int", [rf"\bsurface integral of\s+{E}$"], "surface_int", {"varX": "x", "varY": "y"}, ["ax", "bx", "ay", "by"], "Computing the surface integral of", ["0", "1", "0", "1"]),
    ("calc.partial_mixed", [rf"\bmixed partial (?:derivative )?of\s+{E}\s+with respect to\s+(?P<var1>[a-zA-Z])\s*(?:and|,)\s*(?P<var2>[a-zA-Z])$"], "partial_mixed", None, [], "Computing the mixed partial derivative of", []),
]

def _reg_calc_expr_nums():
    out = []
    for entry in _CALC_EXPR_NUMS:
        name, pats, op, str_keys, num_keys, verb, defs = entry
        if name == "calc.numerical_int":
            def build(m, text, session):
                expr = clean_expression(m.group("expr"))
                a, b = m.group("a"), m.group("b")
                return ngineCommand(f'calc:numerical_int|{{"expr":{jv(expr)},"var":"x","a":{jnum(a)},"b":{jnum(b)}}}',
                                      1, f"Numerically integrating {expr} from {a} to {b}.", expr)
            out.append(Intent(name, _p(*pats), build))
        elif name == "calc.greens":
            def build(m, text, session):
                P, Q = clean_expression(m.group("P")), clean_expression(m.group("Q"))
                nums = find_numbers(text[text.find(m.group("Q")) + len(m.group("Q")):])
                ax, bx, ay, by = (nums + ["0", "1", "0", "1"])[:4]
                return EngineCommand(
                        f'calc:greens|{{"P":{jv(P)},"Q":{jv(Q)},"ax":{jnum(ax)},"bx":{jnum(bx)},"ay":{jnum(ay)},"by":{jnum(by)}}}',
                    0, f"Applying Green's theorem to P={P}, Q={Q}.", None)
            out.append(Intent(name, _p(*pats), build))
        elif name == "calc.partial_mixed":
            def build(m, text, session):
                expr = clean_expression(m.group("expr"))
                v1, v2 = m.group("var1"), m.group("var2")
                return EngineCommand(f'calc:partial_mixed|{{"expr":{jv(expr)},"var1":{jv(v1)},"var2":{jv(v2)}}}',
                                      0, f"Computing the mixed partial of {expr}.", expr)
            out.append(Intent(name, _p(*pats), build))
        else:
            out.append(Intent(name, _p(*pats), _build_calc_expr_nums(op, str_keys, num_keys, verb, defs)))
    return out

INTENTS[0:0] = _reg_calc_expr_nums()

INTENTS[0:0] = [
    Intent("calculus.limit_inf", _p(rf"\blimit of\s+{E}\s+as\s+{V}\s*(?:approaches|->|goes to|→)\s*(?:positive )?infinity$"), _build_calc_limit_directed("limit_inf", "Taking the limit at positive infinity")),
    Intent("calculus.limit_neginf", _p(rf"\blimit of\s+{E}\s+as\s+{V}\s*(?:approaches|->|goes to|→)\s*negative infinity$"), _build_calc_limit_directed("limit_neginf", "Taking the limit at negative infinity")),
    Intent("calculus.vector_laplacian", _p(r"\bvector laplacian of\b"), _build_calc_vector_field_op("vector_laplacian", "Computing the vector Laplacian of")),
    Intent("calculus.stokes", _p(r"\bstokes'? theorem\b"), _build_calc_vector_field_op("stokes", "Applying Stokes' theorem to")),
    Intent("calculus.optimize_nd", _p(r"\boptimi[sz]e\s+" + E + r"\s+over\s+(?P<vars>[a-zA-Z](?:\s*,\s*[a-zA-Z])*)$"),
           lambda m, text, session: EngineCommand(
               (lambda expr, vs: f'calc:optimize_nd|{{"expr":{jv(expr)},"vars":[{",".join(jv(v.strip()) for v in vs.split(","))}],"min":[{",".join(["-10"]*len(vs.split(",")))}],"max":[{",".join(["10"]*len(vs.split(",")))}]}}')(
                   clean_expression(m.group("expr")), m.group("vars")),
               0, "Finding the critical points.", None)),
]
# ─────────────────────────────────────────────────────────────────────────
# EXPANDED COVERAGE (batch 15) — DiffEq's remaining ~53 operations.
# ─────────────────────────────────────────────────────────────────────────
 
def _build_de_expr1_nums(op: str, num_keys, verb: str, defaults=None, expr_key="f"):
    def build(m: Match, text: str, session) -> EngineCommand:
        expr = clean_expression(m.group("expr"))
        rest = text[text.find(m.group("expr")) + len(m.group("expr")):]
        nums = find_numbers(rest)
        defs = defaults or (["0"] * len(num_keys))
        vals = list(nums[:len(num_keys)])
        if len(vals) < len(num_keys):
            vals += defs[len(vals):len(num_keys)]
        pairs = ",".join(f'"{k}":{jnum(v)}' for k, v in zip(num_keys, vals))
        return EngineCommand(f'de:{op}|{{"{expr_key}":{jv(expr)}{"," + pairs if pairs else ""}}}',
                              0, f"{verb} {expr}.", expr)
    return build
 
 
def _build_de_2expr_nums(op: str, num_keys, verb: str, defaults=None, keys=("f1", "f2")):
    def build(m: Match, text: str, session) -> EngineCommand:
        exprs = extract_expr_list(text)
        if exprs and len(exprs) >= 2:
            e1, e2 = exprs[0], exprs[1]
        else:
            e1, e2 = "0", "0"
        rest = text[text.find(find_matrix_literal(text) or chr(0)) + len(find_matrix_literal(text) or ""):]
        nums = find_numbers(rest)
        defs = defaults or (["0"] * len(num_keys))
        vals = list(nums[:len(num_keys)])
        if len(vals) < len(num_keys):
            vals += defs[len(vals):len(num_keys)]
        pairs = ",".join(f'"{k}":{jnum(v)}' for k, v in zip(num_keys, vals))
        return EngineCommand(f'de:{op}|{{"{keys[0]}":{jv(e1)},"{keys[1]}":{jv(e2)}{"," + pairs if pairs else ""}}}',
                              0, f"{verb}.", None)
    return build
 
 
# Group A: numeric ODE methods sharing rk4's exact signature
_DE_NUMERIC_METHODS = [
    ("de.adams_bashforth", "adams.bashforth", "adams_bashforth", "Adams-Bashforth"),
    ("de.rk45", "rk45|runge.kutta.fehlberg", "rk45", "RK45"),
    ("de.improved_euler2", "improved euler", "improved_euler", "improved Euler"),
    ("de.implicit_euler", "implicit euler", "implicit_euler", "implicit Euler"),
    ("de.crank_nicolson", "crank.nicolson", "crank_nicolson", "Crank-Nicolson"),
    ("de.richardson_ode", "richardson extrapolation.*?ode", "richardson_ode", "Richardson extrapolation"),
]
INTENTS[0:0] = [
    Intent(n, _p(
        rf"\b(?:{label_re})\b.*?dy/dt\s*=\s*{E}\s+(?:with\s+)?y\(0\)\s*=\s*(?P<y0>{NUMLIKE})(?:.*?\bto\s+t\s*=\s*(?P<t1>{NUMLIKE}))?$"),
        _build_de_numeric_method(op, label))
    for (n, label_re, op, label) in _DE_NUMERIC_METHODS
]
 
# Group B: pure a,b,c
_DE_ABC = [
    ("de.homogeneous2nd", [r"\bhomogeneous second.order\b", r"\bhomogeneous 2nd order\b"], "homogeneous2nd", ["a", "b", "c"], "Solving the homogeneous 2nd-order ODE", ["1", "3", "2"]),
    ("de.cauchy_euler", [r"\bcauchy.euler\b", r"\beuler equation\b.*\bode\b"], "cauchy_euler", ["a", "b", "c"], "Solving the Cauchy-Euler equation", ["1", "1", "1"]),
]
INTENTS[0:0] = [Intent(n, _p(*p), _build_nums("de", op, k, v, d)) for (n, p, op, k, v, d) in _DE_ABC]
 
# Group C: a,b,c + g(x)
def _build_de_abc_g(op: str, verb: str):
    def build(m: Match, text: str, session) -> EngineCommand:
        nums = find_numbers(text)
        a, b, c = (nums + ["1", "0", "0"])[:3]
        g = clean_expression(m.group("g")) if "g" in m.groupdict() and m.group("g") else "0"
        return EngineCommand(f'de:{op}|{{"a":{jnum(a)},"b":{jnum(b)},"c":{jnum(c)},"g":{jv(g)},"x":"x"}}',
                              0, f"{verb} with g(x)={g}.", None)
    return build
 
INTENTS[0:0] = [
    Intent("de.undetermined_coeff", _p(r"\bundetermined coefficients\b.*\bg\(?x\)?\s*=\s*(?P<g>.+?)$", r"\bundetermined coefficients\b"), _build_de_abc_g("undetermined_coeff", "Solving by undetermined coefficients")),
    Intent("de.variation_params", _p(r"\bvariation of parameters\b.*\bg\(?x\)?\s*=\s*(?P<g>.+?)$", r"\bvariation of parameters\b"), _build_de_abc_g("variation_params", "Solving by variation of parameters")),
]
 
# Group D/E: reduction_of_order, annihilator
INTENTS[0:0] = [
    Intent("de.reduction_of_order", _p(r"\breduction of order\b"), lambda m, text, session: EngineCommand(
        (lambda nums: f'de:reduction_of_order|{{"a":{jnum(nums[0] if len(nums)>0 else "1")},"b":{jnum(nums[1] if len(nums)>1 else "0")},"c":{jnum(nums[2] if len(nums)>2 else "0")},"y1":"exp(x)"}}')(find_numbers(text)),
        0, "Solving by reduction of order.", None)),
    Intent("de.annihilator", _p(r"\bannihilator method\b.*\bg\(?x\)?\s*=\s*(?P<g>.+?)$", r"\bannihilator method\b"), lambda m, text, session: EngineCommand(
        (lambda nums, g: f'de:annihilator|{{"a0":{jnum(nums[0] if len(nums)>0 else "1")},"a1":{jnum(nums[1] if len(nums)>1 else "0")},"a2":{jnum(nums[2] if len(nums)>2 else "1")},"g":{jv(g)},"x":"x"}}')(
            find_numbers(text), clean_expression(m.group("g")) if "g" in m.groupdict() and m.group("g") else "0"),
        0, "Solving via the annihilator method.", None)),
    Intent("de.higher_order", _p(r"\bhigher.order (?:linear )?ode\b"), lambda m, text, session: EngineCommand(
        f'de:higher_order|{{"coeffs":{find_vector_literal(text) or "[]"}}}', 0, "Solving the higher-order linear ODE.", None)),
]
 
# Group G: Laplace-related
INTENTS[0:0] = [
    Intent("de.inverse_laplace", _p(rf"\binverse laplace(?: transform)? of\s+{E}$"), _build_de_expr1_nums("inverse_laplace", [], "Finding the inverse Laplace transform of", expr_key="F")),
    Intent("de.laplace_table", _p(rf"\blaplace transform table\b.*?{E}$", rf"\blook up\b.*\blaplace\b.*?{E}$"), _build_de_expr1_nums("laplace_table", [], "Looking up the Laplace transform of")),
    Intent("de.heaviside", _p(rf"\bheaviside\b.*?{E}$"), _build_de_expr1_nums("heaviside", ["c"], "Applying the Heaviside step function to", ["0"])),
    Intent("de.convolution", _p(r"\bconvolution of\b"), _build_de_2expr_nums("convolution", [], "Computing the convolution")),
    Intent("de.partial_fractions_s", _p(r"\bpartial fractions\b.*\bs.domain\b", r"\bpartial fraction\b.*\blaplace\b"), _build_de_2expr_nums("partial_fractions_s", [], "Computing the partial fraction decomposition", keys=("num", "den"))),
    Intent("de.dirac_response", _p(r"\bdirac delta response\b", r"\bimpulse response\b"), _build_nums("de", "dirac_response", ["a", "b", "c", "t0", "tend"], "Computing the impulse response", ["1", "0", "1", "1", "5"])),
    Intent("de.ivp_laplace", _p(r"\bsolve\b.*\bivp\b.*\blaplace\b"), lambda m, text, session: EngineCommand(
        (lambda nums: f'de:ivp_laplace|{{"a":{jnum(nums[0] if len(nums)>0 else "1")},"b":{jnum(nums[1] if len(nums)>1 else "0")},"c":{jnum(nums[2] if len(nums)>2 else "0")},"g":"0","t":"t","y0":{jnum(nums[3] if len(nums)>3 else "0")},"dy0":{jnum(nums[4] if len(nums)>4 else "0")}}}')(find_numbers(text)),
        0, "Solving the IVP via Laplace transform.", None)),
    Intent("de.duhamel", _p(r"\bduhamel'?s? principle\b"), _build_nums("de", "duhamel", ["alpha", "L", "tend"], "Applying Duhamel's principle", ["1", "3.14159", "1"])),
    Intent("de.parseval", _p(rf"\bparseval'?s? identity\b.*?{E}$", r"\bparseval'?s? identity\b"), _build_de_expr1_nums("parseval", ["L", "N"], "Applying Parseval's identity to", ["3.14159", "10"])),
]
 
# Group H: word problems
_DE_WORD = [
    ("de.mixing", [r"\bmixing problem\b"], "mixing", ["V", "cin", "rin", "rout", "c0", "tend"], "Solving the mixing problem", ["100", "0.5", "2", "2", "0", "100"]),
    ("de.cooling", [r"\bnewton'?s? (?:law of )?cooling\b"], "cooling", ["k", "Tenv", "T0", "tend"], "Applying Newton's law of cooling", ["0.1", "20", "100", "50"]),
    ("de.population", [r"\bpopulation growth\b"], "population", ["r", "P0", "tend"], "Modeling population growth", ["0.1", "100", "50"]),
    ("de.terminal_velocity", [r"\bterminal velocity\b"], "terminal_velocity", ["m", "g", "k", "v0", "tend"], "Computing terminal velocity", ["70", "9.81", "0.2", "0", "30"]),
    ("de.torricelli", [r"\btorricelli'?s? law\b"], "torricelli", ["A", "a", "h0", "g"], "Applying Torricelli's law", ["1", "0.01", "4", "9.81"]),
    ("de.comparison_thm", [r"\bcomparison theorem\b"], "comparison_thm", ["q1", "q2", "a", "b"], "Applying the Sturm comparison theorem", ["1", "4", "0", "3.14159"]),
]
INTENTS[0:0] = [Intent(n, _p(*p), _build_nums("de", op, k, v, d)) for (n, p, op, k, v, d) in _DE_WORD]
 
INTENTS[0:0] = [Intent("de.orthogonal_traj", _p(rf"\borthogonal trajector(?:y|ies) of\s+{E}$"), _build_de_expr1_nums("orthogonal_traj", [], "Finding the orthogonal trajectories of", expr_key="family"))]
 
# Group I: special functions & series
_DE_SPECIAL = [
    ("de.bessel", [r"\bbessel(?:'s)? equation\b"], "bessel", ["nu", "x"], "Solving Bessel's equation", ["0", "1"]),
    ("de.legendre_de", [r"\blegendre(?:'s)? equation\b"], "legendre", ["n", "x"], "Solving Legendre's equation", ["2", "0.5"]),
    ("de.assoc_legendre", [r"\bassociated legendre\b"], "assoc_legendre", ["l", "m", "x"], "Solving the associated Legendre equation", ["1", "0", "0.5"]),
    ("de.power_series", [r"\bpower series solution\b"], "power_series", ["p", "q", "r", "terms"], "Finding the power series solution", ["0", "1", "0", "8"]),
]
INTENTS[0:0] = [Intent(n, _p(*p), _build_nums("de", op, k, v, d)) for (n, p, op, k, v, d) in _DE_SPECIAL]
 
INTENTS[0:0] = [
    Intent("de.fourier_series", _p(rf"\bfourier series of\s+{E}$"), _build_de_expr1_nums("fourier_series", ["L", "N"], "Computing the Fourier series of", ["3.14159", "10"])),
    Intent("de.fourier_sine", _p(rf"\bfourier sine series of\s+{E}$"), _build_de_expr1_nums("fourier_sine", ["L", "N"], "Computing the Fourier sine series of", ["3.14159", "10"])),
    Intent("de.fourier_cos", _p(rf"\bfourier cosine series of\s+{E}$"), _build_de_expr1_nums("fourier_cos", ["L", "N"], "Computing the Fourier cosine series of", ["3.14159", "10"])),
    Intent("de.sl_eigen", _p(r"\bsturm.liouville\b"), _build_nums("de", "sl_eigen", ["a", "b", "N"], "Finding the Sturm-Liouville eigenvalues", ["0", "3.14159", "5"])),
    Intent("de.rayleigh_de", _p(r"\brayleigh quotient\b"), _build_nums("de", "rayleigh_de", ["a", "b"], "Computing the Rayleigh quotient", ["0", "1"])),
    Intent("de.greens_fn_de", _p(r"\bgreen'?s? function\b.*\bode\b", r"\bgreens function for\b.*\bode\b"), _build_nums("de", "greens_fn_de", ["a", "b"], "Constructing the Green's function", ["0", "1"])),
    Intent("de.weak_solution", _p(r"\bweak solution\b"), lambda m, text, session: EngineCommand(
        f'de:weak_solution|{{"pde":"0","phi":"1","x":"x"}}', 0, "Analyzing the weak solution.", None)),
]
 
# Group K: qualitative systems theory
INTENTS[0:0] = [
    Intent("de.phase_portrait", _p(r"\bphase portrait\b"), _build_nums("de", "phase_portrait", ["a", "b", "c", "d"], "Analyzing the phase portrait", ["0", "1", "-1", "0"])),
    Intent("de.linear_system", _p(r"\blinear system of odes\b", r"\bsystem of linear odes\b"), lambda m, text, session: EngineCommand(
        f'de:linear_system|{{"A":{find_vector_literal(text) or "[1,0,0,1]"}}}', 0, "Solving the linear system of ODEs.", None)),
    Intent("de.nonlinear_2d", _p(r"\bnonlinear (?:2d )?system\b"), _build_de_2expr_nums("nonlinear_2d", ["xmin", "xmax", "ymin", "ymax"], "Analyzing the nonlinear 2D system", ["-3", "3", "-3", "3"])),
    Intent("de.hartman_grobman", _p(r"\bhartman.grobman\b"), _build_de_2expr_nums("hartman_grobman", ["xs", "ys"], "Applying the Hartman-Grobman theorem", ["0", "0"])),
    Intent("de.lyapunov_fn", _p(r"\blyapunov function\b"), lambda m, text, session: EngineCommand(
        (lambda es: f'de:lyapunov_fn|{{"f1":{jv(es[0] if len(es)>0 else "0")},"f2":{jv(es[1] if len(es)>1 else "0")},"V":{jv(es[2] if len(es)>2 else "x^2+y^2")},"x1":"x","x2":"y","xs":0,"ys":0}}')(extract_expr_list(text) or []),
        0, "Checking the Lyapunov function.", None)),
    Intent("de.dulac", _p(r"\bdulac'?s? criterion\b"), _build_de_2expr_nums("dulac", [], "Applying Dulac's criterion")),
    Intent("de.limit_cycle", _p(r"\blimit cycle\b"), _build_de_2expr_nums("limit_cycle", ["xmin", "xmax", "ymin", "ymax"], "Checking for a limit cycle", ["-3", "3", "-3", "3"])),
    Intent("de.poincare_index", _p(r"\bpoincare index\b", r"\bpoincar[eé].bendixson\b"), _build_de_2expr_nums("poincare_index", ["cx", "cy", "r"], "Computing the Poincare index", ["0", "0", "1"])),
]
 
# Group L: PDEs
_DE_PDE = [
    ("de.heat_pde", [r"\bheat equation\b"], "heat_pde", {"ic": "sin(pi*x)"}, ["alpha", "L", "terms"], "Solving the heat equation", ["1", "1", "5"]),
    ("de.wave_pde", [r"\bwave equation\b"], "wave_pde", {"f0": "sin(pi*x)", "g0": "0"}, ["c", "L", "terms"], "Solving the wave equation", ["1", "1", "5"]),
    ("de.laplace_pde", [r"\blaplace'?s? equation\b.*\bpde\b", r"\bpde\b.*\blaplace'?s? equation\b"], "laplace_pde", {"topBC": "sin(pi*x)"}, ["Lx", "Ly", "terms"], "Solving Laplace's equation", ["1", "1", "5"]),
]
def _reg_de_pde():
    out = []
    for name, pats, op, str_keys, num_keys, verb, defs in _DE_PDE:
        def build(m, text, session, op=op, str_keys=str_keys, num_keys=num_keys, verb=verb, defs=defs):
            nums = find_numbers(text)
            vals = list(nums[:len(num_keys)])
            if len(vals) < len(num_keys):
                vals += defs[len(vals):len(num_keys)]
            pairs = ",".join(f'"{k}":{jnum(v)}' for k, v in zip(num_keys, vals))
            str_pairs = ",".join(f'"{k}":{jv(v)}' for k, v in str_keys.items())
            return EngineCommand(f'de:{op}|{{{str_pairs}{"," + pairs if pairs else ""}}}', 0, f"{verb}.", None)
        out.append(Intent(name, _p(*pats), build))
    return out
INTENTS[0:0] = _reg_de_pde()
 
INTENTS[0:0] = [
    Intent("de.nonhomog_pde", _p(r"\bnonhomogeneous (?:heat )?pde\b"), _build_nums("de", "nonhomog_pde", ["alpha", "L", "terms"], "Solving the nonhomogeneous PDE", ["1", "3.14159", "5"])),
    Intent("de.fourier_transform_pde", _p(r"\bfourier transform\b.*\bpde\b"), _build_nums("de", "fourier_transform_pde", ["tend"], "Solving the PDE via Fourier transform", ["1"])),
    Intent("de.characteristics_1st", _p(r"\bmethod of characteristics\b"), _build_nums("de", "characteristics_1st", [], "Solving via the method of characteristics", [])),
]
 
# ─────────────────────────────────────────────────────────────────────────
# EXPANDED COVERAGE (batch 16) — Probability Theory's remaining 36 ops.
# ─────────────────────────────────────────────────────────────────────────
 
def _detect_dist(text: str) -> str:
    for d in ("normal", "exponential", "poisson", "uniform", "gamma", "binomial"):
        if d in text.lower():
            return d
    return "normal"
 
 
_PT_NUMS = [
    ("prob.mgf_exp", [r"\bmgf\b.*\bexponential\b"], "mgf_exp", ["lambda", "t"], "Computing the exponential MGF", ["1", "0"]),
    ("prob.mgf_gamma", [r"\bmgf\b.*\bgamma\b"], "mgf_gamma", ["alpha", "beta", "t"], "Computing the gamma MGF", ["2", "1", "0"]),
    ("prob.mgf_uniform", [r"\bmgf\b.*\buniform\b"], "mgf_uniform", ["a", "b", "t"], "Computing the uniform MGF", ["0", "1", "0"]),
    ("prob.pgf", [r"\bprobability generating function\b"], "pgf", ["z"], "Evaluating the probability generating function", ["1"]),
    ("prob.char_normal", [r"\bcharacteristic function\b.*\bnormal\b"], "char_normal", ["mu", "sigma", "t"], "Computing the normal characteristic function", ["0", "1", "0"]),
    ("prob.char_poisson", [r"\bcharacteristic function\b.*\bpoisson\b"], "char_poisson", ["lambda", "t"], "Computing the Poisson characteristic function", ["1", "0"]),
    ("prob.char_cauchy", [r"\bcharacteristic function\b.*\bcauchy\b"], "char_cauchy", ["x0", "gamma", "t"], "Computing the Cauchy characteristic function", ["0", "1", "0"]),
    ("prob.berry_esseen", [r"\bberry.esseen\b"], "berry_esseen", ["mu", "sigma", "rho", "n"], "Applying the Berry-Esseen bound", ["0", "1", "1", "100"]),
    ("prob.clt_approx", [r"\bclt approximation\b"], "clt_approx", ["mu", "sigma", "n", "x"], "Approximating via the CLT", ["0", "1", "30", "0"]),
    ("prob.markov_ineq2", [r"\bmarkov'?s? inequality\b"], "markov_ineq", ["mu", "a"], "Applying Markov's inequality", ["1", "2"]),
    ("prob.chernoff", [r"\bchernoff bound\b"], "chernoff", ["mu", "delta"], "Applying the Chernoff bound", ["1", "0.5"]),
    ("prob.hoeffding", [r"\bhoeffding'?s? (?:bound|inequality)\b"], "hoeffding", ["n", "a", "b", "t"], "Applying Hoeffding's bound", ["30", "0", "1", "0.1"]),
    ("prob.azuma", [r"\bazuma\b"], "azuma", ["n", "c", "t"], "Applying the Azuma-Hoeffding inequality", ["30", "1", "1"]),
    ("prob.poisson_proc", [r"\bpoisson process\b"], "poisson_proc", ["lambda", "t", "k"], "Analyzing the Poisson process", ["1", "1", "10"]),
    ("prob.gbm", [r"\bgeometric brownian motion\b"], "gbm", ["S0", "mu", "sigma", "t", "steps"], "Simulating geometric Brownian motion", ["100", "0.05", "0.2", "1", "252"]),
    ("prob.brownian", [r"(?<!geometric )\bbrownian motion\b"], "brownian", ["t", "steps"], "Simulating Brownian motion", ["1", "100"]),
    ("prob.opt_stopping", [r"\boptional stopping\b"], "opt_stopping", ["mu", "sigma", "a", "b"], "Applying the optional stopping theorem", ["0", "1", "-1", "1"]),
    ("prob.martingale_conv", [r"\bmartingale convergence\b"], "martingale_conv", ["mu", "sigma", "n"], "Applying the martingale convergence theorem", ["0", "1", "100"]),
    ("prob.joint_normal", [r"\bjoint normal\b", r"\bbivariate normal\b"], "joint_normal", ["mu1", "mu2", "s1", "s2", "rho", "x1", "x2"], "Evaluating the joint normal distribution", ["0", "0", "1", "1", "0", "0", "0"]),
    ("prob.cond_normal", [r"\bconditional normal\b"], "cond_normal", ["mu1", "mu2", "s1", "s2", "rho", "x2"], "Computing the conditional normal distribution", ["0", "0", "1", "1", "0", "0"]),
    ("prob.doob_maximal", [r"\bdoob'?s? maximal\b"], "doob_maximal", ["mu", "sigma", "a"], "Applying Doob's maximal inequality", ["0", "1", "2"]),
    ("prob.importance_samp", [r"\bimportance sampling\b"], "importance_samp", ["mu", "sigma", "threshold", "n"], "Running importance sampling", ["0", "1", "2", "10000"]),
    ("prob.spacings", [r"\border statistics spacings\b", r"\bexponential spacings\b"], "spacings", ["n", "lambda"], "Computing the order-statistic spacings", ["10", "1"]),
]
INTENTS[0:0] = [Intent(n, _p(*p), _build_nums("prob", op, k, v, d)) for (n, p, op, k, v, d) in _PT_NUMS]
 
INTENTS[0:0] = [
    Intent("prob.mc_integrate", _p(rf"\bmonte carlo integrat\w*\s+{E}\s+from\s+(?P<a>{NUMLIKE})\s+to\s+(?P<b>{NUMLIKE})$"),
           lambda m, text, session: EngineCommand(
               f'prob:mc_integrate|{{"f":{jv(clean_expression(m.group("expr")))},"a":{jnum(m.group("a"))},"b":{jnum(m.group("b"))},"n":100000}}',
               1, "Estimating the integral via Monte Carlo.", None)),
    Intent("prob.mgf_derive", _p(rf"\bderive\b.*\bmoments\b.*\bmgf\b.*?{E}$", r"\bmgf derivative\b"),
           lambda m, text, session: EngineCommand(
               f'prob:mgf_derive|{{"mgf":{jv(clean_expression(m.group("expr")) if m.groupdict().get("expr") else "exp(t^2/2)")},"k":1}}',
               0, "Deriving moments from the MGF.", None)),
    Intent("prob.doob_decomp", _p(r"\bdoob decomposition\b"), lambda m, text, session: EngineCommand(
        f'prob:doob_decomp|{{"xs":{find_vector_literal(text) or "[]"}}}', 0, "Computing the Doob decomposition.", None)),
    Intent("prob.levy_cont", _p(r"\blevy continuity\b"), lambda m, text, session: EngineCommand(
        f'prob:levy_cont|{{"values":{find_vector_literal(text) or "[]"},"t":[1]}}', 0,
        "Checking Levy continuity.", None)),
    Intent("prob.cov_matrix", _p(r"\bcovariance matrix\b"), lambda m, text, session: EngineCommand(
        f'prob:cov_matrix|{{"data":{matrix_literal_to_json_rows(find_matrix_literal(text) or "[[]]")}}}', 1,
        "Computing the covariance matrix.", None)),
    Intent("prob.corr_matrix", _p(r"\bcorrelation matrix\b"), lambda m, text, session: EngineCommand(
        f'prob:corr_matrix|{{"data":{matrix_literal_to_json_rows(find_matrix_literal(text) or "[[]]")}}}', 1,
        "Computing the correlation matrix.", None)),
    Intent("prob.wlln", _p(r"\bweak law of large numbers\b"), lambda m, text, session: EngineCommand(
        f'prob:wlln|{{"dist":{jv(_detect_dist(text))},"params":[0,1],"n":100}}', 1,
        "Demonstrating the weak law of large numbers.", None)),
    Intent("prob.slln", _p(r"\bstrong law of large numbers\b"), lambda m, text, session: EngineCommand(
        f'prob:slln|{{"dist":{jv(_detect_dist(text))},"params":[0,1],"n":1000}}', 1,
        "Demonstrating the strong law of large numbers.", None)),
    Intent("prob.extreme_value", _p(r"\bextreme value distribution\b"), lambda m, text, session: EngineCommand(
        (lambda nums: f'prob:extreme_value|{{"n":{jnum(nums[0] if nums else "30")},"dist":{jv(_detect_dist(text))},"params":[0,1]}}')(find_numbers(text)),
        1, "Analyzing the extreme value distribution.", None)),
    Intent("prob.order_stat", _p(r"\border statistic distribution\b"), lambda m, text, session: EngineCommand(
        (lambda nums: f'prob:order_stat|{{"n":{jnum(nums[0] if len(nums)>0 else "10")},"k":{jnum(nums[1] if len(nums)>1 else "1")},"dist":{jv(_detect_dist(text))},"params":[0,1]}}')(find_numbers(text)),
        1, "Finding the order statistic distribution.", None)),
    Intent("prob.moments_mgf", _p(r"\bmoments\b.*\bmgf\b.*\bdistribution\b", r"\bmoments via mgf\b"), lambda m, text, session: EngineCommand(
        f'prob:moments_mgf|{{"dist":{jv(_detect_dist(text))},"params":[0,1],"k":4}}', 1,
        "Computing moments via the MGF.", None)),
    Intent("prob.cumulant", _p(r"\bcumulant\b"), lambda m, text, session: EngineCommand(
        f'prob:cumulant|{{"dist":{jv(_detect_dist(text))},"params":[0,1],"k":1}}', 1,
        "Computing the cumulant.", None)),
    Intent("prob.lst", _p(r"\blaplace.stieltjes\b"), lambda m, text, session: EngineCommand(
        (lambda nums: f'prob:lst|{{"dist":{jv(_detect_dist(text))},"params":[0,1],"s":{jnum(nums[0] if nums else "1")}}}')(find_numbers(text)),
        1, "Computing the Laplace-Stieltjes transform.", None)),
]
 
# ─────────────────────────────────────────────────────────────────────────
# EXPANDED COVERAGE (batch 17) — AppliedMath's remaining ~53 operations
# (the module richest in generous C++-side defaults, so even terse
# phrasings like "wkb approximation" produce a valid, runnable command).
# ─────────────────────────────────────────────────────────────────────────
 
_AM_NUMS = [
    ("am.classify_linear", [r"\bclassify\b.*\blinear system\b"], "classify_linear", ["a11", "a12", "a21", "a22"], "Classifying the linear system", ["0", "1", "-1", "0"]),
    ("am.michaelis", [r"\bmichaelis.menten\b"], "michaelis", ["kcat", "Km", "E0", "S0", "T", "n"], "Simulating Michaelis-Menten kinetics", ["1", "1", "1", "10", "10", "100"]),
    ("am.seir", [r"\bseir model\b"], "seir", ["beta", "sigma", "gamma", "S0", "E0", "I0", "R0", "T"], "Running the SEIR epidemic model", ["0.3", "0.2", "0.1", "990", "0", "10", "0", "100"]),
    ("am.reproduction_number", [r"\breproduction number\b", r"\br.?nought\b", r"\br0\b(?!\d)"], "reproduction_number", ["beta", "gamma", "N"], "Computing the basic reproduction number", ["0.3", "0.1", "1000"]),
    ("am.logistic_map", [r"\blogistic map\b"], "logistic_map", ["r", "x0", "n"], "Iterating the logistic map", ["3.5", "0.5", "200"]),
    ("am.mass_action", [r"\bmass action kinetics\b"], "mass_action", ["k", "A0", "B0", "T", "n"], "Simulating mass-action kinetics", ["1", "10", "10", "10", "200"]),
    ("am.turing", [r"\bturing instability\b", r"\bturing pattern\b"], "turing", ["D1", "D2", "fu", "fv", "gu", "gv"], "Checking for Turing instability", ["1", "10", "1", "1", "-1", "1"]),
    ("am.feigenbaum", [r"\bfeigenbaum\b"], "feigenbaum", ["rmin", "rmax", "n"], "Running the Feigenbaum bifurcation analysis", ["2.5", "4.0", "500"]),
    ("am.speed_of_sound", [r"\bspeed of sound\b"], "speed_of_sound", ["gamma", "p", "rho"], "Computing the speed of sound", ["1.4", "101325", "1.225"]),
    ("am.mach", [r"\bmach number\b"], "mach", ["v", "gamma", "p", "rho"], "Computing the Mach number", ["100", "1.4", "101325", "1.225"]),
    ("am.gas_shock", [r"\bgas.dynamic shock\b", r"\bshock wave\b.*\bgas\b"], "gas_shock", ["gamma", "rho1", "u1", "p1", "M1"], "Analyzing the gas-dynamics shock", ["1.4", "1.225", "0", "101325", "2"]),
    ("am.euler_1d", [r"\b1d euler equations\b", r"\beuler equations\b.*\bgas\b"], "euler_1d", ["gamma", "rho", "u", "p", "T", "n"], "Solving the 1D Euler equations", ["1.4", "1.225", "0", "101325", "1", "100"]),
    ("am.poincare_lindstedt", [r"\bpoincare.lindstedt\b"], "poincare_lindstedt", ["omega0", "eps", "x0", "v0", "order"], "Applying the Poincare-Lindstedt method", ["1", "0.1", "1", "0", "2"]),
    ("am.multiple_scales", [r"\bmethod of multiple scales\b"], "multiple_scales", ["omega0", "eps", "order"], "Applying the method of multiple scales", ["1", "0.1", "2"]),
    ("am.brachistochrone", [r"\bbrachistochrone\b"], "brachistochrone", ["x0", "y0", "x1", "y1"], "Solving the brachistochrone problem", ["0", "0", "1", "-1"]),
]
INTENTS[0:0] = [Intent(n, _p(*p), _build_nums("am", op, k, v, d)) for (n, p, op, k, v, d) in _AM_NUMS]
 
 
def _build_am_expr_nums(op: str, expr_defaults: dict, num_keys, verb: str, defaults=None):
    """1+ named expression params (with defaults) plus trailing numeric params.
    expr_defaults maps JSON key -> default expr string; the first message
    of a bracketed list "[e1,e2]" (if present) overrides them in order,
    else a single bare expression (if the pattern captured one) fills the
    first key."""
    def build(m: Match, text: str, session) -> EngineCommand:
        keys = list(expr_defaults.keys())
        vals = dict(expr_defaults)
        exprs = extract_expr_list(text)
        matched_expr = m.group("expr") if "expr" in m.groupdict() and m.group("expr") else None
        if exprs:
            for k, e in zip(keys, exprs):
                vals[k] = e
            lit = find_matrix_literal(text)
            rest = text[text.find(lit) + len(lit):] if lit else text
        elif matched_expr:
            vals[keys[0]] = clean_expression(matched_expr)
            rest = text[text.find(matched_expr) + len(matched_expr):]
        else:
            rest = text
        nums = find_numbers(rest)
        defs = defaults or (["0"] * len(num_keys))
        nvals = list(nums[:len(num_keys)])
        if len(nvals) < len(num_keys):
            nvals += defs[len(nvals):len(num_keys)]
        str_pairs = ",".join(f'"{k}":{jv(v)}' for k, v in vals.items())
        num_pairs = ",".join(f'"{k}":{jnum(v)}' for k, v in zip(num_keys, nvals))
        return EngineCommand(f'am:{op}|{{{str_pairs}{"," + num_pairs if num_pairs else ""}}}', 0, f"{verb}.", None)
    return build
 
 
_AM_EXPR = [
    ("am.linearise", [r"\blineari[sz]e\b.*\bfixed point\b"], "linearise", {"f": "y", "g": "-x"}, ["x0", "y0"], "Linearizing about the fixed point", ["0", "0"]),
    ("am.euler_lagrange", [rf"\beuler.lagrange\b\s+(?:for|of)\s+{E}$", r"\beuler.lagrange equation\b"], "euler_lagrange", {"L": "yp^2"}, [], "Deriving the Euler-Lagrange equation for"),
    ("am.sturm_liouville_am", [r"\bsturm.liouville problem\b"], "sturm_liouville", {"p": "1", "q": "0", "r": "1"}, ["a", "b", "n"], "Solving the Sturm-Liouville problem", ["0", "3.14159", "5"]),
    ("am.greens_function", [r"\bgreen'?s? function\b.*\bboundary\b", r"\bgreens function\b.*\bode\b"], "greens_function", {"p": "1", "q": "0", "f": "0"}, ["a", "b", "alpha", "beta"], "Constructing the Green's function", ["0", "1", "1", "0"]),
    ("am.bifurcation", [rf"\bbifurcation diagram\b\s+of\s+{E}$", r"\bbifurcation diagram\b"], "bifurcation", {"f": "mu*x-x^3"}, ["mumin", "mumax", "n"], "Building the bifurcation diagram", ["-3", "3", "100"]),
    ("am.classify_bifurcation", [r"\bclassify\b.*\bbifurcation\b"], "classify_bifurcation", {"f": "mu*x-x^3"}, ["x0", "mu0"], "Classifying the bifurcation", ["0", "0"]),
    ("am.nullclines", [r"\bnullclines\b"], "nullclines", {"f": "x-y", "g": "x+y"}, ["xmin", "xmax", "ymin", "ymax"], "Computing the nullclines", ["-5", "5", "-5", "5"]),
    ("am.perturbation", [rf"\b(?:regular )?perturbation (?:series|expansion) of\s+{E}$", r"\bregular perturbation\b"], "perturbation", {"eq": "x+eps*x^2-1"}, ["epsilon", "order"], "Computing the regular perturbation expansion", ["0.1", "3"]),
    ("am.boundary_layer", [r"\bboundary layer analysis\b"], "boundary_layer", {"p": "1", "q": "0"}, ["eps", "a", "b"], "Running the boundary layer analysis", ["0.01", "0", "0"]),
    ("am.wkb", [r"\bwkb approximation\b"], "wkb", {"q": "1"}, ["eps", "a", "b"], "Applying the WKB approximation", ["0.1", "0", "1"]),
    ("am.isoperimetric", [r"\bisoperimetric problem\b"], "isoperimetric", {"F": "yp^2", "G": "y"}, ["C"], "Solving the isoperimetric problem", ["1"]),
    ("am.noether", [r"\bnoether'?s? theorem\b"], "noether", {"L": "yp^2"}, [], "Applying Noether's theorem"),
    ("am.cobweb", [rf"\bcobweb diagram\b\s+of\s+{E}$", r"\bcobweb diagram\b"], "cobweb", {"f": "r*x*(1-x)"}, ["x0", "n"], "Building the cobweb diagram", ["0.5", "20"]),
    ("am.discrete_fixed", [r"\bdiscrete fixed points\b"], "discrete_fixed", {"f": "r*x*(1-x)"}, ["xmin", "xmax"], "Finding the discrete fixed points", ["-5", "5"]),
    ("am.fredholm", [r"\bfredholm (?:integral )?equation\b"], "fredholm", {"f": "1", "K": "x*y"}, ["a", "b", "lambda", "n"], "Solving the Fredholm integral equation", ["0", "1", "1", "50"]),
    ("am.beltrami", [r"\bbeltrami identity\b"], "beltrami", {"L": "yp^2"}, [], "Applying the Beltrami identity"),
    ("am.scaling", [r"\bscaling analysis\b"], "scaling", {"eq": "x+eps*x^2"}, ["eps"], "Running scaling analysis", ["0.1"]),
    ("am.simple_wave", [r"\bsimple wave\b"], "simple_wave", {"c": "1", "u0": "sin(x)"}, ["xmin", "xmax"], "Solving the simple wave equation", ["-5", "5"]),
    ("am.dalembert", [r"\bd'?alembert'?s? solution\b"], "dalembert", {"f0": "sin(x)", "g0": "0"}, ["c", "x", "t"], "Applying d'Alembert's solution", ["1", "0", "1"]),
    ("am.burgers", [r"\bburgers'? equation\b"], "burgers", {"u0": "sin(x)"}, ["nu", "L", "T", "nx", "nt"], "Solving Burgers' equation", ["0.1", "10", "1", "100", "1000"]),
    ("am.rankine_hugoniot", [r"\brankine.hugoniot\b"], "rankine_hugoniot", {"F": "u^2/2"}, ["uL", "uR"], "Applying the Rankine-Hugoniot condition", ["1", "0"]),
    ("am.entropy_condition", [r"\bentropy condition\b"], "entropy_condition", {"F": "u^2/2"}, ["uL", "uR", "s"], "Checking the entropy condition", ["1", "0", "0.5"]),
    ("am.travelling_wave", [r"\btravelling wave\b", r"\btraveling wave\b"], "travelling_wave", {"R": "u*(1-u)"}, ["D", "uminus", "uplus"], "Finding the travelling wave solution", ["1", "0", "1"]),
    ("am.dispersion", [r"\bdispersion relation\b"], "dispersion", {"eq": "omega-k^2"}, ["k"], "Computing the dispersion relation", ["1"]),
    ("am.wave_velocities", [r"\bwave velocities\b", r"\bphase and group velocity\b"], "wave_velocities", {"omega": "k^2"}, ["k0"], "Computing the wave velocities", ["1"]),
    ("am.potential_flow", [r"\bpotential flow\b"], "potential_flow", {"phi": "x^2-y^2"}, ["x", "y"], "Analyzing the potential flow", ["0", "0"]),
    ("am.stream_function", [r"\bstream function\b"], "stream_function", {"psi": "x*y"}, ["x", "y"], "Analyzing the stream function", ["0", "0"]),
    ("am.shock_time", [r"\bshock formation time\b"], "shock_time", {"c": "1+u0", "u0": "sin(x)"}, ["xmin", "xmax"], "Computing the shock formation time", ["-5", "5"]),
    ("am.continuity_1d", [r"\b1d continuity equation\b", r"\bcontinuity equation\b.*\bfluid\b"], "continuity_1d", {"u": "1"}, ["rho", "L", "T"], "Solving the 1D continuity equation", ["1", "10", "1"]),
]
 
def _reg_am_expr():
    out = []
    for entry in _AM_EXPR:
        name, pats, op, edefs, nk, verb = entry[0], entry[1], entry[2], entry[3], entry[4], entry[5]
        defs = entry[6] if len(entry) > 6 else None
        out.append(Intent(name, _p(*pats), _build_am_expr_nums(op, edefs, nk, verb, defs)))
    return out
 
INTENTS[0:0] = _reg_am_expr()
 
INTENTS[0:0] = [
    Intent("am.buckingham", _p(r"\bbuckingham pi\b"), lambda m, text, session: EngineCommand(
        f'am:buckingham|{{"vars":["m","l","t"],"D":[[1,0,0],[0,1,0],[0,0,1]]}}', 0,
        "Applying the Buckingham Pi theorem.", None)),
    Intent("am.nondim", _p(r"\bnondimensionali[sz]e\b"), lambda m, text, session: EngineCommand(
        f'am:nondim|{{"eq":{jv(clean_expression(extract_between(text, ["nondimensionalize", "nondimensionalise"]) or "x+eps*x^2"))},"vars":["x"],"scales":[1]}}',
        0, "Nondimensionalizing the equation.", None)),
]
 
INTENTS[0:0] = [
    Intent("am.galton_watson", _p(r"\bgalton.watson\b"), lambda m, text, session: EngineCommand(
        f'am:galton_watson|{{"pk":{find_vector_literal(text) or "[0.2,0.5,0.3]"},"gen":{jnum((find_numbers(text)+["10"])[-1])}}}',
        1, "Simulating the Galton-Watson branching process.", None)),
    Intent("am.laplace_method", _p(r"\blaplace'?s? method\b.*\basymptotic\b", r"\blaplace method\b"), _build_am_expr_nums(
        "laplace_method", {"h": "-x^2", "g": "1"}, ["xstar", "N"], "Applying Laplace's method", ["0", "100"])),
    Intent("am.reaction_diffusion", _p(r"\breaction.diffusion\b"), _build_am_expr_nums(
        "reaction_diffusion", {"R": "u*(1-u)", "u0": "exp(-x^2)"}, ["D", "L", "T", "nx", "nt"],
        "Solving the reaction-diffusion equation", ["1", "10", "1", "50", "500"])),
]
