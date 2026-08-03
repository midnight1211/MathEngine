"""
session.py
-----------
Per-conversation memory. One session per chat session_id, kept alive for
the life of the subprocess (see cli.py). Lets the chatbot resolve
follow-ups like:

    > differentiate x^2 + 3x
    < 2x + 3
    > now integrate that
    < ... integrates "x^2 + 3x" again ...

    > determinant of [[1,2],[3,4]]
    < -2
    > now find its inverse
    < ... inverse of [[1,2],[3,4]] ...
"""

from __future__ import annotations

import re
from dataclasses import dataclass, field
from typing import Dict, List, Optional

PRONOUN_RE = re.compile(r"\b(?:that|it|the (?:tlast|previous) (?:result|expression|answer))\b", re.IGNORECASE)


@dataclass
class Turn:
    message: str
    engine_input: str
    result: Optional[str] = None


@dataclass
class Session:
    session_id: str
    last_expression: Optional[str] = None
    last_matrix: Optional[str] = None
    last_dataset: Optional[str] = None
    last_result: Optional[str] = None
    history: List[Turn] = field(default_factory=list)

    def resolve_pronoun(self, text: str) -> str:
        """If the user said "that"/"it" instead of a real expression, swap
        in the last remembered expression. Otherwise return text unchanged
        (callers are expected to have already tried to extract a real
        expression before falling back to this)."""
        if PRONOUN_RE.search(text) and self.last_expression:
            return self.last_expression
        return text.strip()

    def record(self, message: str, engine_input: str, result: Optional[str], remembers_expr: Optional[str]) -> None:
        if remembers_expr:
            self.last_expression = remembers_expr
            self.last_result = result
            self.history.append(Turn(message, engine_input, result))
            # Keep memory bounded - this is a chat session, not a database.
            if len(self.history) > 50:
                self.history.pop(0)

class SessionStore:
    """In-process registry of sessions, keyed by session_id. The Java/Spring
    side is expected to keep one long-lived chatbot subprocess per app
    instance (see chatbot/README.md), so this only needs to live as long
    as that process does."""

    def __init__(self) -> None:
        self._sessions: Dict[str, Session] = {}

    def get(self, session_id: str) -> Session:
        if session_id not in self._sessions:
            self._sessions[session_id] = Session(session_id=session_id)
        return self._sessions[session_id]
