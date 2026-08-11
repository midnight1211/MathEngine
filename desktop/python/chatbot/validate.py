"""
validate.py
────────────
Feature 3 (Hybrid Validation Pipeline): a fast, local pre-flight syntax
check the chatbot runs on any expression it's about to send to the C++
engine, before formulating its reply. Catches the class of errors a
human would call "you forgot a closing paren" — unbalanced/mismatched
brackets — and reports the exact character column, the same way a real
lexer would, so the chatbot's natural-language error is precise instead
of vague.

This is NOT a substitute for CoreEngine's own parser (which still runs
via MathBridge.compute() and remains the source of truth for whether an
expression is valid math) — it's a cheap first line of defense that
avoids a wasted round trip to the engine for the most common typos, and
gives a much more specific message than the engine's generic parse
error would.
"""

from __future__ import annotations

from dataclasses import dataclass
from typing import Optional

PAIRS = {")": "(", "]": "[", "}": "{"}
OPENERS = set(PAIRS.values())
CLOSERS = set(PAIRS.keys())


@dataclass
class ValidationResult:
    ok: bool
    message: Optional[str] = None
    column: Optional[int] = None       # 1-based character offset into the checked string
    near: Optional[str] = None         # a short snippet around the error, for quoting back


def check_brackets(expr: str) -> ValidationResult:
    """Balanced-bracket check with a 1-based column offset on failure."""
    stack = []  # list of (char, index)
    for i, c in enumerate(expr):
        if c in OPENERS:
            stack.append((c, i))
        elif c in CLOSERS:
            if not stack:
                return ValidationResult(
                    False,
                    f"unmatched closing '{c}'",
                    i + 1,
                    _snippet(expr, i),
                )
            top_char, _ = stack[-1]
            if top_char != PAIRS[c]:
                return ValidationResult(
                    False,
                    f"'{c}' doesn't match the last opening '{top_char}'",
                    i + 1,
                    _snippet(expr, i),
                )
            stack.pop()
    if stack:
        char, idx = stack[-1]
        return ValidationResult(
            False,
            f"'{char}' is never closed",
            idx + 1,
            _snippet(expr, idx),
        )
    return ValidationResult(True)


def _snippet(expr: str, idx: int, width: int = 6) -> str:
    start = max(0, idx - width)
    end = min(len(expr), idx + width + 1)
    return expr[start:end]


def preflight(expr: str) -> ValidationResult:
    """Run all local checks; currently just bracket balance, but this is
    the extension point for more (e.g. trailing operators, empty
    function calls) without changing callers."""
    if not expr or not expr.strip():
        return ValidationResult(False, "the expression is empty", 1, "")
    return check_brackets(expr)


def format_error_reply(expr: str, result: ValidationResult) -> str:
    col = result.column or 1
    near = f' near "{result.near}"' if result.near else ""
    return (
        f"That doesn't look quite right{near} (character {col}): {result.message}. "
        f"Mind double-checking the parentheses/brackets in \"{expr}\"?"
    )
