"""
actions.py
----------
Feature 2 (Shared State Modification / Chat-Driven Actions): detects
requests that should change what the desktop UI is showing - right now
just "plot/graph/show a graph of ..." - and returns a structured action
dict instead of (or alongside) a computation.

Java's ChatbotPanel is responsible for actually executing the action
(switching tabs, calling GraphPanel.plotExpression); this module only 
decide that an action should happen and what it is, mirroring how
intents.py decides engine commands without executing them.

Action shape (also documented in chatbot/README.md):
    {
        "type": "SWITCH_TAB",
        "target": "Graph",
        "payload": {"equation": "sin(x)*cos(y)", "range": [-6.283185, 6.283185]}
    }
"""

from __future__ import annotations

import re
from typing import Optional

from entities import clean_expression, find_numbers

PLOT_RE = re.compile(
    r"\b(?:plot|graph|show (?:me )?(?:a )?(?:graph|plot) of|visuali[sz]e)\b\s*"
    r"(?:a )?(?:3d )?(?:surface (?:plot )?of\s+)?"
    r"(?:the (?:function|graph|equation|curve)\s+)?"
    r"(?:of\s+)?"
    r"(?P<expr>.+?)"
    r"(?:\s+(?:from|between)\s+(?P<a>[-+]?\d+(?:\.\d+)?(?:\s*\*?\s*pi)?)\s+"
    r"(?:to|and)\s+(?P<b>[-+]?\d+(?:\.\d+)?(?:\s*\*?\s*pi)?))?"
    r"\s*$",
    re.IGNORECASE,
)

_PI = 3.14159265358979


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


def detect_plot_action(text: str) -> Optional[str]:
    """Returns a SWITCH_TAB action dict for the Graph tab, or None if the
    message isn't a plot request. GraphPanel is a 2D function plotter
    (single expr in x), so a "3D surface z = f(x,y)" request is still
    routed to it as best-effort - the reply notes the limitation rather
    than silently pretending 3D rendering happened."""
    m = PLOT_RE.search(text)
    if not m:
        return None
    expr = clean_expression(m.group("expr"))
    if not expr:
        return None
    a = _parse_bound(m.group("a"), -10.0)
    b = _parse_bound(m.group("b"), 10.0)
    is_3d = bool(re.search(r"\by\b", expr)) and "3d" in text.lower() or "surface" in text.lower()
    return {
            "type": "SWITCH_TAB",
            "target": "Graph",
            "payload": {"equation": expr, "range": [a, b], "is3d": is_3d},
    }
