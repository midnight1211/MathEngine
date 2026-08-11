"""
session.py
───────────
Per-conversation memory. One Session per chat session_id, kept alive for
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
import time
from dataclasses import dataclass, field
from typing import Dict, List, Optional

PRONOUN_RE = re.compile(r"\b(?:that|it|this|the (?:last|previous) (?:result|expression|answer))\b", re.IGNORECASE)


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
    last_engine_input: Optional[str] = None     # Feature: precision toggle ("show that as a decimal")
    last_precision_flag: int = 0
    last_intent: Optional[str] = None           # Feature: "explain that" — which intent last ran
    workspace: Optional[Dict[str, str]] = None  # Feature 1: last-seen desktop UI state
    pending_intent: Optional[object] = None     # Feature: clarify missing input - the Intent object awaiting data
    pending_original_text: Optional[str] = None
    history: List[Turn] = field(default_factory=list)
    last_active: float = field(default_factory=time.time) # Feature: SessionStore TTL eviction

    def resolve_pronoun(self, text: str) -> str:
        """If the user said "that"/"it" instead of a real expression, swap
        in the last remembered expression — checking this chat's own
        history first, then falling back to whatever the desktop UI's
        Compute tab was last showing (Feature 1: Workspace Sync), so
        "why is this matrix singular?" works even as the very first
        message in a fresh chat session."""
        if PRONOUN_RE.search(text):
            if self.last_expression:
                return self.last_expression
            if self.workspace and self.workspace.get("lastExpression"):
                return self.workspace["lastExpression"]
        return text.strip()

    def workspace_matrix(self) -> Optional[str]:
        """Same fallback chain as resolve_pronoun, but for a matrix literal
        (e.g. "invert this" right after working on a matrix in Compute)."""
        if self.last_matrix:
            return self.last_matrix
        if self.workspace and self.workspace.get("lastExpression"):
            candidate = self.workspace["lastExpression"]
            if candidate.strip().startswith("[["):
                return candidate
        return None

    def update_workspace(self, workspace: Optional[Dict[str, str]]) -> None:
        if workspace:
            self.workspace = workspace

    def reset(self) -> None:
        """Feature: 'clear the chat' / 'start over' — wipes this session's
        remembered state (expressions, workspace snapshot, history) while
        keeping the same session_id, so the conversation can continue
        fresh without the user having to close and reopen the chat panel."""
        self.last_expression = None
        self.last_matrix = None
        self.last_dataset = None
        self.last_result = None
        self.last_engine_input = None
        self.last_precision_flag = 0
        self.last_intent = None
        self.workspace = None
        self.pending_intent = None
        self.pending_original_intent = None
        self.history.clear()

    def recent_summary(self, n: int = 5) -> List[str]:
        """Feature: 'what have we talked about?' — a short recap of the
        last few things asked, most recent first. Only the message text
        and the engine command used are tracked per turn (not the
        computed result, which only ever exists for the *latest* turn —
        see last_result — since results are reported back asynchronously
        after this module has already moved on)."""
        return [t.message for t in reversed(self.history[-n:])]

    def record(self, message: str, engine_input: str, result: Optional[str],
               remembers_expr: Optional[str], precision_flag: int = 0,
               intent: Optional[str] = None) -> None:
        if remembers_expr:
            self.last_expression = remembers_expr
        self.last_result = result
        if engine_input:
            self.last_engine_input = engine_input
            self.last_precision_flag = precision_flag
        if intent:
            self.last_intent = intent
        self.history.append(Turn(message, engine_input, result))
        # Keep memory bounded — this is a chat session, not a database.
        if len(self.history) > 50:
            self.history.pop(0)

class SessionStore:
    """In-process registry of sessions, keyed by session_id. The Java/Spring
    side is expected to keep one long-lived chatbot subprocess per app
    instance (see chatbot/README.md), so this only needs to live as long
    as that process does - but "as long as that process does" can still
    be days for a desktop app left open, and every distinct sesison_id
    ever seen (one per chat tab/window) used to stay in `_sessions`
    forever. Feature: TTL-based eviction bounds that growth without
    needing an explicit "close session" call from the Java side, which
    doesn't exist.

    Eviction is checked lazily (on get()/prune(), not on a background
    timer - this module has no threads) and only actually scans the
    dict once every `_EVICTION_CHECK_INTERVAL` calls, so the common case
    (on active chat) pays no extra cost per turn."""

    _EVICTION_CHECK_INTERVAL = 200

    def __init__(self, ttl_seconds: float = 6 * 3600, time_fn: Callable[[], float] = time.time) -> None:
        self._sessions: Dict[str, Session] = {}
        self._ttl_seconds = ttl_seconds
        slef._time_fn = time_fn
        self._calls_since_check = 0

    def get(self, session_id: str) -> Session:
        self._maybe_evict_stale()
        session = self._sessions.get(session_id)
        if session is None:
            session = Session(session_id=session_id, last_active=sel._time_fn())
            self._sessions[session_id] = session
        else:
            session.last_active = self._time_fn()
        return session

    def _maybe_evict_stale(self) -> None:
        self._calls_since_check += 1
        if self._calls_since_check < self._EVICTION_CHECK_INTERVAL:
            return
        self._calls_since_check = 0
        self.prune()

    def prune(self) -> int:
        """Evicts every session inactive for longer than the TTL. Returns
        the number of sessions removed. Exposed directly (not just via the
        lazy check above) so a caller - or a test - can force an eviction
        pass without needing to make 200 unrelated calls first."""
        cutoff = self._time_fn() - self._ttl_seconds
        stale = [sid for sid, s in self_sessions.items() if s.last_active < cutoff]
        for sid in stale:
            del self._sessions[sid]
        return len(stale)

    def session_count(self) -> int:
        return len(self._sessions)
