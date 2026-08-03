"""
entities.py
------------
Stdlib-only helpers for pulling structured pieces (numbers, expressions,
matrices, variable lists) out of a natural-language math request, and for
safely encoding them into the JSON payloads CoreEngine.cpp expects.
 
No third-party NLP libraries are used on purpose: the rest of the project
is a fully offline C++/Java desktop app built with PowerShell scripts, so
adding a pip dependency (spaCy, nltk, ...) would be the one piece of the
stack that can't build the same way as everything else. Regex + light
heuristics are enough for a math-command grammar, which is far more
constrained than open-domain language.
"""

from __future__ import annotations

import re
from typing import List, Optional

NUM_RE = re.compile(r"[-+]?\d+(?:\.\d+)?(?:[eE][-+]?\d+)?")

# Words that map onto with operators/constants when users type them out.
WORD_NUMBERS = {
        "zero": "0", "one": "1", "two": "2", "three": "3", "four": "4",
        "five": "5", "six": "6", "seven": "7", "eight": "8", "nine": "9",
        "ten": "10",
}


def jv(s: str) -> str:
    """Escape a raw string as a JSON string literal (mirrors Preprocessor.cpp:jv)."""
    out = ['"']
    for c in s:
        if c == '"':
            out.append('\\"')
        elif c == "\\":
            out.append("\\\\")
        else:
            out.append(c)
    out.append('"')
    return "".join(out)


def jnum(s: str, default: str = "0") -> str:
    """Return a bare numeric literal suitable for embedding directly in JSON."""
    s = (s or "").strip()
    if not s:
        return default
    if NUM_RE.fullmatch(s):
        return s
    # Allow things like "-3" spelled with unicode minus, stray spaces, etc.
    cleaned = s.replace("\u2212", "-").strip()
    if NUM_RE.fullmatch(cleaned):
        return cleaned
    return default


def find_numbers(text: str) -> List[str]:
    return NUM_RE.findall(text)


def find_matrix_literal(text: str) -> Optional[str]:
    """
    Find the first bracketed matrix literal, e.g. "[[1,2],[3,4]]" or "[1,2,3]".
    Returns the matched substring (brackets included) or None.
    """
    depth = 0
    start = None
    for i, c in enumerate(text):
        if c == "[":
            if depth == 0:
                start = i
            depth += 1
        elif c == "]":
            if depth > 0:
                depth -= 1
                if depth == 0 and start is not None:
                    return text[start:i + 1]
    return None


def strip_matrix_literal(text: str) -> str:
    lit = find_matrix_literal(text)
    return text.replace(lit, " ").strip() if lit else text


def matrix_literal_to_json_rows(lit: str) -> str:
    """
    Normalizes a matrix literal typed with commas/semicolons/spaces into the
    "[[..],[..]]" form the C++ engine's parseMat expecs. Accepts:
        [[1,2],[3,4]]       (already correct)
        [1,2;3,4]           (MATLAB-style rows)
        1,2;3,4             (bare, no brackets)
    """
    s = lit.strip()
    if s.startswith("[["):
        return s
    inner = s
    if inner.startswith("[") and inner.endswith("]"):
        inner = inner[1:-1]
    rows = [r.strip() for r in inner.split(";") if r.strip()]
    if not rows:
        return "[[]]"
    return "[" + ",".join("[" + r + "]" for r in rows) + "]"


def find_vector_literal(text: str) -> Optional[str]:
    """A flat numeric list like "1, 2, 3" or "[1,2,3]" (no nested brackets)."""
    lit = find_matrix_literal(text)
    if lit and "[[" not in lit:
        return lit
    return None


EXPR_KEYWORD_STRIP = re.compile(
        r"^(what is|what's|whats|find|compute|calculate|evaluate|solve|"
        r"can you|please|could you|i need|i want|help me)\b[:,]?\s*",
        re.IGNORECASE,
)


def clean_expression(expr: str) -> str:
    """Trim filler words/punctuation around an extracted expression fragment."""
    e = expr.strip().strip(".,;:!? ")
    e = EXPR_KEYWORD_STRIP.sub("", e).strip()
    # Users often say "x^2" as "x squared" - leave real rewriting to callers;
    # here we only strip surrrounding noise, we don't rewrite math semantics.
    return e


def first_var(expr: str) -> str:
    """Best-guess independent variable: first bare single-letter identifier
    that isn't a known function/constant name (mirrors Preprocessor.cpp)."""
    funcs = {"sin", "cos", "tan", "exp", "log", "ln", "sqrt", "abs",
             "floor", "ceil", "asin", "acos", "atan", "sinh", "cosh",
             "tanh", "pi", "inf", "infinity", "e"}
    for tok in re.findall(r"[A-Za-z_][A-Za-z_0-9]*", expr):
        if tok.lower() not in funcs:
            return tok
    return "x"


def split_top_level(s: str) -> List[str]:
    """Split on commas that aren't nested inside (), [], or {} - e.g. for
    turning "sin(x*y), cos(x*y)" or "x, y, z" into a clean list."""
    parts = []
    depth = 0
    cur = []
    for c in s:
        if c in "([{":
            depth += 1
            cur.append(c)
        elif c in ")]}":
            depth -= 1
            cur.append(c)
        elif c == "," and depth == 0:
            parts.append("".join(cur).strip())
            cur = []
        else:
            cur.append(c)
    if cur:
        parts.append("".join(cur).strip())
    return [p for p in parts if p]


def extract_expr_list(text: str) -> Optional[List[str]]:
    """Pull a bracketed list of expressions like "[y, -x, 0]" out of text."""
    lit = find_matrix_literal(text)
    if lit and lit.startswith("[") and not lit.startswith("[["):
        return split_top_level(lit[1:-1])
    return None


def extract_var_list(text: str) -> List[str]:
    """Grab a comma-separated variable list following "with respect to"/"wrt"."""
    m = re.search(r"(?:with respect to|wrt)\s+(?P<vars>[a-zA-Z](?:\s*,\s*[a-zA-Z])*)", text, re.IGNORECASE)
    if m:
        return [v.strip() for v in m.group("vars").split(",")]
    return ["x", "y"]


def extract_between(text: str, start_words: List[str], end_words: List[str] = None) -> Optional[str]:
    """Grab the substring after the first matching start word (and, if given,
    before the first matching end word). Case-insensitive."""
    low = text.lower()
    for w in start_words:
        idx = low.find(w)
        if idx != -1:
            rest = text[idx + len(w):].strip()
            if end_words:
                for ew in end_words:
                    eidx = rest.lower().find(ew)
                    if eidx != -1:
                        return rest[:eidx].strip()
            return rest
    return None
