"""
followups.py
─────────────
Two conversational features that reference what *just happened* rather
than parsing a fresh math request from scratch:

- Precision toggle: "give me that as a decimal" / "show the exact value
  instead" re-runs the *same* last engine command with the opposite
  precision flag, so the user doesn't have to retype the whole request
  just to see it the other way.

- Explain that: "what does that mean?" / "why does that matter?" pulls
  the knowledge-base entry (see knowledge.py) for whatever concept the
  last computation was actually about, using the intent name that
  matched — so "why does that matter?" right after a determinant
  computation explains determinants, without the user having to name
  the term themselves.

Both are deliberately conservative about when they fire: the precision
toggle only applies once nothing in the main intent registry matched
(see nlp_engine.py's ordering), so it can never shadow a real new
computation request that happens to mention "numerically" or similar.
"""

from __future__ import annotations

import re
from typing import Optional

NUMERIC_RE = re.compile(
    r"\b(?:as a decimal|numerically|as a number|in decimal form|"
    r"a decimal (?:answer|value)|give me (?:the |a )?decimal)\b",
    re.IGNORECASE,
)
SYMBOLIC_RE = re.compile(
    r"\b(?:symbolically|as an exact (?:value|answer|form)|in exact form|"
    r"the exact (?:value|answer)|as a fraction|exactly instead)\b",
    re.IGNORECASE,
)

EXPLAIN_RE = re.compile(
    r"^(?:so\s+)?what (?:is|does) (?:that|this)(?: mean)?\??$"
    r"|^explain (?:that|this)\.?$"
    r"|^why (?:does |is )?(?:that|this)(?: matter| important| useful| work)\??$"
    r"|^tell me more(?: about (?:that|this))?\.?$",
    re.IGNORECASE,
)

# Maps an intent name to the docs_kb/knowledge_base.json entry id it's
# about, for the subset of intents that have a matching KB entry.
# Deliberately not exhaustive — this only needs to cover the concepts
# common enough that "why does that matter?" is a natural follow-up;
# see docs_kb/knowledge_base.json for the full definition list.
INTENT_TO_KB = {
    "calculus.derivative": "derivative", "calculus.partial": "derivative",
    "calculus.log_diff": "derivative", "calculus.implicit_diff": "derivative",
    "calculus.integral_definite": "integral", "calculus.integral_indefinite": "integral",
    "calculus.numerical_int": "integral", "calculus.double_int": "integral",
    "calculus.triple_int": "integral",
    "calculus.taylor": "taylor_series",
    "calculus.limit": "limit", "calculus.limit_inf": "limit",
    "calculus.limit_neginf": "limit", "calculus.limit_left": "limit",
    "calculus.limit_right": "limit",
    "la.determinant": "determinant", "la.determinant_cofactor": "determinant",
    "la.determinant_lu": "determinant", "la.determinant_bareiss": "determinant",
    "la.inverse": "matrix_inverse", "la.inverse_gaussjordan": "matrix_inverse",
    "la.inverse_adjugate": "matrix_inverse", "la.pseudoinverse": "matrix_inverse",
    "la.eigen": "eigenvalues", "la.eigenvalues": "eigenvalues", "la.eigenvectors": "eigenvalues",
    "la.rank": "singular_matrix", "la.is_singular": "singular_matrix",
    "stat.mean": "mean",
    "stat.stddev": "standard_deviation", "stat.variance": "standard_deviation",
    "stat.pearson": "correlation",
    "stat.linear_reg": "linear_regression", "stat.poly_reg": "linear_regression",
    "stat.t_test_one": "hypothesis_test", "stat.t_test_two2": "hypothesis_test",
    "stat.t_test_paired": "hypothesis_test", "stat.z_test": "hypothesis_test",
    "stat.chi_sq_test": "hypothesis_test", "stat.f_test": "hypothesis_test",
    "stat.bayes": "bayes_theorem", "stat.bayes_ext": "bayes_theorem",
    "stat.cond_prob": "bayes_theorem",
    "stat.markov_steady": "markov_chain", "stat.markov_absorb": "markov_chain",
    "stat.clt_demo": "central_limit_theorem",
    "nt.gcd": "gcd", "nt.extended_gcd": "gcd", "nt.lcm": "gcd",
    "nt.prime_factors": "prime_factorization", "nt.is_prime": "prime_factorization",
    "nt.mod_pow": "modular_arithmetic", "nt.mod_inverse": "modular_arithmetic",
    "nt.linear_cong": "modular_arithmetic", "nt.quad_cong": "modular_arithmetic",
    "dm.combinations": "combinations_permutations", "dm.permutations": "combinations_permutations",
    "dm.dijkstra": "graph_shortest_path", "dm.bfs": "graph_shortest_path",
    "dm.floyd_warshall": "graph_shortest_path", "dm.prim_mst": "graph_shortest_path",
    "geo.distance": "distance_formula", "geo.distance_3d": "distance_formula",
    "geo.midpoint": "distance_formula",
    "ca.roots": "complex_roots", "ca.residue": "residue",
    "na.newton": "newtons_method", "na.bisection": "newtons_method",
    "na.secant2": "newtons_method", "na.brent": "newtons_method", "na.muller": "newtons_method",
    "de.rk4": "ode_rk4", "de.euler": "ode_rk4", "de.rk2": "ode_rk4",
    "de.rk45": "ode_rk4", "de.adams_bashforth": "ode_rk4",
    "de.laplace": "laplace_transform", "de.inverse_laplace": "laplace_transform",
    "de.ivp_laplace": "laplace_transform",
    "aa.cyclic": "group_theory", "aa.symmetric": "group_theory",
    "aa.dihedral": "group_theory", "aa.is_abelian": "group_theory", "aa.is_group": "group_theory",
    "am.logistic_growth": "logistic_growth",
    "am.sir": "sir_model", "am.seir": "sir_model", "am.reproduction_number": "sir_model",
    "arithmetic.passthrough": "expression_syntax",
}


def detect_precision_toggle(text: str) -> Optional[int]:
    """Returns 1 (switch to numerical), 0 (switch to symbolic), or None if
    this message isn't a precision-toggle follow-up at all."""
    t = text.strip()
    if NUMERIC_RE.search(t):
        return 1
    if SYMBOLIC_RE.search(t):
        return 0
    return None


def is_explain_that(text: str) -> bool:
    return bool(EXPLAIN_RE.match(text.strip()))


def kb_id_for_intent(intent_name: Optional[str]) -> Optional[str]:
    if not intent_name:
        return None
    return INTENT_TO_KB.get(intent_name)
