"""
suggestions.py
────────────────
Feature: "did you mean...?" — when nothing in the intent registry
matches and the text doesn't look like a bare expression either, this
compares the message against a curated list of example phrasings (one
or two per common operation) and surfaces the closest 1-3 matches
instead of just silently forwarding the raw text to the engine (which
would otherwise fail with an unhelpful low-level parse error for any
natural-language sentence).

This is deliberately a *small, hand-picked* sample of the 600+ engine
operations chatbot/intents.py covers — enough breadth to catch typos
and phrasing near-misses on the most common requests, not an attempt to
have an example for every single op (see intents.py itself, and
chatbot/README.md, for the full list of what's supported).
"""

from __future__ import annotations

import difflib
from typing import List

EXAMPLE_PHRASES: List[str] = [
    # Calculus
    "derivative of x^2 + 3x",
    "integrate x^2 from 0 to 3",
    "limit of 1/x as x approaches infinity",
    "taylor series of cos(x) around 0",
    "gradient of x^2 + y^2",
    # Linear Algebra
    "determinant of [[1,2],[3,4]]",
    "inverse of [[2,1],[1,1]]",
    "eigenvalues of [[2,0],[0,3]]",
    "solve the system [[2,1],[1,3]] with b [5,10]",
    "rank of [[1,2],[2,4]]",
    # Statistics
    "mean of [4,8,15,16,23,42]",
    "standard deviation of [1,2,3,4,5]",
    "correlation between [1,2,3] and [3,2,1]",
    "linear regression of [1,2,3] and [2,4,6]",
    "one-sample t-test on [1,2,3,4,5] against 3",
    # Number Theory
    "gcd of 48 and 18",
    "is 97 prime",
    "prime factors of 60",
    "fibonacci number for 10",
    # Discrete Math
    "10 choose 3",
    "shortest path in [[0,1,0],[1,0,1],[0,1,0]] from 0",
    # Geometry
    "distance between (0,0) and (3,4)",
    "area of a triangle with vertices (0,0) (4,0) (0,3)",
    # Complex Analysis
    "all 4th roots of 1",
    "residue of 1/z at 0",
    # Numerical Analysis
    "newtons method on x^2-2 starting near 1",
    "bisection method for x^2-2 on 0 to 2",
    # Differential Equations
    "solve dy/dt = -2y with y(0) = 5 to t = 4",
    "laplace transform of sin(t)",
    # Abstract Algebra
    "cyclic group of order 6",
    "symmetric group of order 4",
    # Probability / AppliedMath
    "sir model with beta=0.3 gamma=0.1",
    "logistic growth with r=0.5 K=1000 x0=20 T=10",
    # Chat-driven actions / knowledge base
    "plot sin(x) from -10 to 10",
    "what is a determinant",
    # Plain arithmetic
    "2^8 + sqrt(16)",
]


def suggest(text: str, limit: int = 2, cutoff: float = 0.5) -> List[str]:
    """Returns up to `limit` example phrases similar to `text`, best match
    first, or an empty list if nothing clears the similarity `cutoff`.
    Uses stdlib difflib — no extra dependency, consistent with the rest
    of this chatbot."""
    if not text or not text.strip():
        return []
    matches = difflib.get_close_matches(text.strip().lower(),
                                         [p.lower() for p in EXAMPLE_PHRASES],
                                         n=limit, cutoff=cutoff)
    # Map back to original casing.
    lower_to_orig = {p.lower(): p for p in EXAMPLE_PHRASES}
    return [lower_to_orig[m] for m in matches]


def format_suggestions(suggestions: List[str]) -> str:
    if not suggestions:
        return ""
    if len(suggestions) == 1:
        return f' Did you mean something like "{suggestions[0]}"?'
    quoted = ", ".join(f'"{s}"' for s in suggestions)
    return f" Did you mean something like: {quoted}?"
