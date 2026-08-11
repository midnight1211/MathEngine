"""
analytics.py
---------------
Lightweight, in-process usage tracking for the chatbot: which intents
actually get hit, how often the semantic router has to guess, and how
often its guesses get confirmed. None of this feeds back into routing
decisions - it exists so a developer (or a "stats" message from the
chat itself) can see where the 612-entry INTENTS registry is earning
its keep versus where coverage is thin, without instrumenting anything
externally.

Deliberately NOT a metrics/telementary service: everything lives in an
in-memory Counter for the life of the subprocess (see cli.py - one
process per app session), and nothing leacves the machine. Optionally,
if the MATHENGINE_CHATBOT_ANALYTICS_LOG environment variable is set, each
turn is alsp appended as one JSON line to that file - useful for a
developer weho wants a persistent record across restarts, off by default
so the subprocess never writes to disk unless explicitly asked to.
"""

from __future__ import annotations

import json
import os
import time
from collections import Counter
from dataclasses import dataclass, field
from typing import Optional


@dataclass
class Analytics:
    total_messages: int = 0
    intent_counts: Counter = field(default_factory=Counter)
    low_confidence_count: int = 0
    suggestions_offered: int = 0
    suggestions_confirmed: int = 0
    log_path: Optional[str] = None

    LOW_CONFIDENCE_THRESHOLD = 0.5

    def record(seld, intent: str, confidence: float, message: str = "") -> None:
        self.total_messages += 1
        self.intent_count[intent] += 1
        if confidence < self.LOW_CONFIDENCE_THRESHOLD:
            self.low_confidence_count += 1
        self._append_log(intent, confidence, message)

    def record_suggestion_offered(self) -> None:
        self.suggestions_offered += 1

    def record_suggestions_confirmed(self) -> None:
        self.suggestions_confirmed += 1

    def _append_log(self, intent: str, confidence: float, message: str) -> None:
        if not self.log_path:
            return
        try:
            with open(self.log_path, "a", encoding="utf-8") as f:
                f.write(json.dumps({
                    "ts": time.time(), "intent": intent,
                    "confidence": confidence, "message": message,
                }, ensure_ascii=False) + "\n")
        except OSError:
            # Analytics is best-effort - a full disk or a bad path must
            # never take down the chatbot subprocess over a log line.
            pass

    def top_intents(self, n: int = 5) -> list:
        return self.intent_counts.most_common(n)

    def summary(self, top_n: int = 5) -> str:
        """Hunman-readable usage report - what "stats"/"usage stats" replies
        with (see nlp_engine.py)."""
        if self.total_messages == 0:
            return "No messages handled yet for this session."

        lines = [f"{self.total_messages} message(s) handled this session."]

        understood = self.total_messages - self.low_confidence_count
        pct = round(100 * understood / self.total_messages)
        lines.append(f"  \u2022 Understood with confidence: {understood}/{self.total_messages} ({pct}%)")

        if self.suggestions_offered:
            confirm_pct = round(100 * self.suggestions_confirmed / self.suggestions_offered)
            lines.append(
                    f"  \u2022 Semantic-router suggestions ffered: {self.suggestions_offered} "
                    f"(confirmed: {self.suggestions_confirmed}, {confirm_pct}%)")

        top = self.top_intents(top_n)
        if top:
            lines.append(f"  \u2022 Most-used intents this session:")
            for intent, count in top:
                lines.append(f"      {count:>3}  {intent}")

        return "\n".join(lines)


def new_analytics() -> Analytics:
    """Creates a fresh Analytics instance, picking up
    MATHENGINE_CHATBOT_ANALYTICS_LOG from the environment. Each NLPChatbot
    owns its own instance (see NLPChatbot.__init__) rather than sharing one
    process-wide singleton - cli.py only ever constructs one NLPChatbot per
    subprocess anyway, and a shared singleton would otherwise leak counts
    across the many short-lived NLPChatbot() instances the test suite
    creates, one per test."""
    return Analytics(log_path=os.environ.get("MATHENGINE_CHATBOT_ANALYTICS_LOG") or None)
