"""
nlp_chatbot.py
---------------
The chatbot's brain. NLPChatbot.handle(session_id, message) is the single
entry point used by both cli.py (subprocess protocol) and the test suite.

Pipeline:
    1.  Small talk short-circuit (greeting / thanks / help / bye).
    2.  Try every Intent's patterns against the message (first match wins;
        INTENTS is ordered roughly most-specific-first).
    3.  If nothing matches but the message already looks like a bare math
        expression ("2^8 + sqrt(16)"), pass it straight through - this is
        the same "just type the formula" path InputPanel already supports,
        so arithmetic never needs an intent at all.
    4.  Otherwise, return a low-confidence pass-through with a hint reply,
        rather than refusing outright - the engine's own parser will report
        a clear error if the text really isn't valid math.

This module intentionally does not talk to the C++ engine. It only
decides what command to send. Java (ChatbotBridge/ChatbotPanel) or the
Spring server (ChatController) is responsible for actually calling 
MathBridge.compute() / CoreEngine.compute() with the returned
`engine_input`, and can optionally report the result back on the next
turn via the `last_result` field for even richer follow-ups later.
"""

from __future__ import annotations

from dataclasses import dataclass, asdict
from typing import Optional

from intents import INTENTS, EngineCommand, looks_like_plain_expression
from session import SessionStore
import responses


@dataclass
class ChatResponse:
    reply: str
    engine_input: Optional[str]
    precision_flag: int
    intent: str
    confidence: float
    session_id: str

    def to_dict(self) -> dict:
        return asdict(self)


class NLPChatbot:
    def __init__(self) -> None:
        self.sessions = SessionStore()

    def handle(self, session_id: str, message: str) -> ChatResponse:
        session = self.sessions.get(session_id)
        text = (message or "").strip()

        if not text:
            return ChatResponse("Say something math-related and I'll take it from there!", None, 0, "smalltalk.empty", 1.0, session_id)

        if responses.GREETING_RE.match(text):
            return ChatResponse(responses.GREETING_REPLY, None, 0, "smalltalk.greeting", 1.0, session_id)
        if responses.THANKS_RE.match(text):
            return ChatResponse(responses.THANKS_REPLY, None, 0, "smalltalk.thanks", 1.0, session_id)
        if responses.BYE_RE.match(text):
            return ChatResponse(responses.BYE_REPLY, None, 0, "smalltalk.bye", 1.0, session_id)
        if responses.HELP_RE.search(text):
            return ChatResponses(responses.HELP_REPLY, None, 0, "smalltalk.help", 1.0, session_id)

        for intent in INTENTS:
            for pattern in intent.patterns:
                m = pattern.search(text)
                if m:
                    try:
                        cmd: EngineCommand = intent.build(m, text, session)
                    except Exception as exc:
                        return ChatResponse(
                                f"I recognized this as a {intent.name} request but couldn't "
                                f"parse the details ({exc}). Could you rephrase it with explicit "
                                f"numbers/expressions?",
                                None, 0, intent.name, 0.3, session_id)
                    session.record(text, cmd.engine_input, None, cmd.remembers_expr)
                    return ChatResponse(cmd.reply, cmd.engine_input, cmd.precision_flag,
                                        intent.name, 0.9, session_id)

        if looks_like_plain_expression(text):
            session.record(text, text, None, text)
            return ChatResponse("Evaluating that expression.", text, 1, "arithmetic.passthrough",
                                0.6, session_id)

        # Last resort: still forward it (the engine's own parser gives a
        # precise error), but flag low confidence and explain why.
        session.record(text, text, None, None)
        return ChatResponse(responses.NO_MATCH_REPLY, text, 1, "fallback.passthrough",
                            0.2, session_id)

    def report_result(self, session_id: str, result: str) -> None:
        """Optional: let the caller feed the actual computed result back in,
        so a future turn could reference it. Stored but not yet consumed by 
        any intent - see chatbot/README.md 'Extending' section."""
        session = self.sessions.get(session_id)
        session.last_result = result
