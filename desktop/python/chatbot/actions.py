"""
actions.py
───────────
Feature 2 (Shared State Modification / Chat-Driven Actions): detects
requests that should change what the desktop UI is showing — currently
"plot/graph/show a graph of ..." and "clear the graph" — and returns a
structured action dict instead of (or alongside) a computation.

Java's ChatbotPanel is responsible for actually executing the action
(switching tabs, calling GraphPanel.plotExpression/clearPlots); this
module only decides *that* an action should happen and *what* it is,
mirroring how intents.py decides engine commands without executing them.

Action shapes (also documented in chatbot/README.md):
    {
      "type": "SWITCH_TAB",
      "target": "Graph",
      "payload": {"equations": ["sin(x)", "cos(x)"],
                   "range": [-6.283185, 6.283185], "is3d": false}
    }
    {"type": "CLEAR_GRAPH", "target": "Graph", "payload": {}}
"""

from __future__ import annotations

import re
from typing import List, Optional

from entities import clean_expression

PLOT_RE = re.compile(
    r"\b(?:plot|graph|show (?:me )?(?:a )?(?:graph|plot) of|visuali[sz]e)\b\s*"
    r"(?:a )?(?:3d )?(?:surface (?:plot )?of\s+)?"
    r"(?:the (?:function|graph|equation|curve)s?\s+)?"
    r"(?:of\s+)?"
    r"(?P<expr>.+?)"
    r"(?:\s+(?:from|between)\s+(?P<a>[-+]?\d+(?:\.\d+)?(?:\s*\*?\s*pi)?)\s+"
    r"(?:to|and)\s+(?P<b>[-+]?\d+(?:\.\d+)?(?:\s*\*?\s*pi)?))?"
    r"\s*$",
    re.IGNORECASE,
)

CLEAR_GRAPH_RE = re.compile(r"\b(?:clear|reset|wipe)\s+(?:the\s+)?graph\b", re.IGNORECASE)

_PI = 3.14159265358979
_PRONOUN_WORDS = {"that", "it", "this", "the last expression",
                   "the previous expression", "the last result"}


def _parse_bound(s: Optional[str], default: float) -> float:
    if not s:
        return default
    s = s.strip().lower().replace(" ", "")
    if "pi" in s:
        coeff = s.replace("pi", "") or "1"
        if coeff in ("+", ""):
            coeff = "1"
        if coeff == "-":
            coeff = "-1"
        try:
            return float(coeff) * _PI
        except ValueError:
            return default
    try:
        return float(s)
    except ValueError:
        return default


def _split_equations(expr: str) -> List[str]:
    """"sin(x) and cos(x)" / "sin(x), cos(x)" / "sin(x), cos(x), and tan(x)"
    -> ["sin(x)", "cos(x)", "tan(x)"]. Safe to split on "and" here even
    though the bounds clause ("between -5 and 5") also uses the word —
    PLOT_RE has already pulled the bounds out into their own groups by
    the time this runs, so `expr` never contains that "and"."""
    parts = re.split(r"\s*,\s*(?:and\s+)?|\s+and\s+", expr)
    return [p.strip() for p in parts if p.strip()]


def detect_plot_action(text: str, session=None) -> Optional[dict]:
    """Returns a SWITCH_TAB action dict for the Graph tab, or None if the
    message isn't a plot request. GraphPanel is a 2D function plotter
    (single expr in x), so a "3D surface z = f(x,y)" request is still
    routed to it as best-effort — the reply notes the limitation rather
    than silently pretending 3D rendering happened.

    Supports multiple functions in one request ("plot sin(x) and
    cos(x)") and pronoun references ("plot that" after discussing an
    expression), the latter only when `session` is provided."""
    m = PLOT_RE.search(text)
    if not m:
        return None
    raw_expr = clean_expression(m.group("expr"))
    if not raw_expr:
        return None

    if session is not None and raw_expr.strip().lower() in _PRONOUN_WORDS:
        resolved = session.resolve_pronoun(raw_expr)
        if resolved and resolved.strip().lower() not in _PRONOUN_WORDS:
            raw_expr = resolved

    equations = _split_equations(raw_expr)
    if not equations:
        return None

    a = _parse_bound(m.group("a"), -10.0)
    b = _parse_bound(m.group("b"), 10.0)
    text_l = text.lower()
    is_3d = bool(re.search(r"\by\b", raw_expr)) and ("3d" in text_l or "surface" in text_l)
    return {
        "type": "SWITCH_TAB",
        "target": "Graph",
        "payload": {"equations": equations, "range": [a, b], "is3d": is_3d},
    }


def detect_clear_graph_action(text: str) -> Optional[dict]:
    """"clear the graph" / "reset graph" / "wipe the graph" -> clears
    whatever's currently plotted without switching tabs or plotting
    anything new."""
    if CLEAR_GRAPH_RE.search(text):
        return {"type": "CLEAR_GRAPH", "target": "Graph", "payload": {}}
    return None
