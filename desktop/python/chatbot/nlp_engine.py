"""
nlp_engine.py
──────────────
The chatbot's brain. NLPChatbot.handle(session_id, message) is the single
entry point used by both cli.py (subprocess protocol) and the test suite.

Pipeline:
  1. Small talk short-circuit (greeting / thanks / help / bye).
  2. Try every Intent's patterns against the message (first match wins;
     INTENTS is ordered roughly most-specific-first).
  3. If nothing matches but the message already looks like a bare math
     expression ("2^8 + sqrt(16)"), pass it straight through — this is
     the same "just type the formula" path InputPanel already supports,
     so arithmetic never needs an intent at all.
  4. Otherwise, return a low-confidence pass-through with a hint reply,
     rather than refusing outright — the engine's own parser will report
     a clear error if the text really isn't valid math.

This module intentionally does NOT talk to the C++ engine. It only
decides *what command to send*. Java (ChatbotBridge/ChatbotPanel) or the
Spring server (ChatController) is responsible for actually calling
MathBridge.compute() / CoreEngine.compute() with the returned
`engine_input`, and can optionally report the result back on the next
turn via the `last_result` field for even richer follow-ups later.
"""

from __future__ import annotations

import os
from dataclasses import dataclass, asdict
from typing import Optional

from intents import INTENTS, EngineCommand, looks_like_plain_expression
from session import SessionStore
import responses
import actions
import knowledge
import validate
import followups
import semantic_router
import analytics


@dataclass
class ChatResponse:
    reply: str
    engine_input: Optional[str]
    precision_flag: int
    intent: str
    confidence: float
    session_id: str
    action: Optional[dict] = None  # Feature 2: structured UI action, if any

    def to_dict(self) -> dict:
        return asdict(self)


class NLPChatbot:
    def __init__(self) -> None:
        self.sessions = SessionStore(ttl_seconds=_session_ttl_from_env())
        self.analytics = analytics.new_analytics()

    def handle(self, session_id: str, message: str, workspace: Optional[dict] = None) -> ChatResponse:
        """Public entry point: dispatches the message, then records the
        outcome for analytics.summary()/"stats" exactly once per user-
        visible turn. The confirm-and-run recursion below calls
        _dispatch() directly (not handle()) so confirming a suggestion
        doesn't get double-counted as two turns."""
        response = self._dispatch(session_id, message, workspace)
        try:
            self.analytics.record(response.intent, response.confidence, message)
        except Exception:  # noqa: BLE001 - analytics must never break a chat turn
            pass
        return response

    def _dispatch(self, session_id: str, message: str, workspace: Optional[dict] = None) -> ChatResponse:
        session = self.sessions.get(session_id)
        session.update_workspace(workspace)  # Feature 1: Workspace Sync
        text = (message or "").strip()

        if not text:
            return ChatResponse("Say something math-related and I'll take it from there!",
                                 None, 0, "smalltalk.empty", 1.0, session_id)

        # Feature: semantic-router confirmation follow-up. A strong,
        # unambiguous suggestion from the fallback branch below leaves a
        # pending_suggestion on the session; if the very next message is a
        # short "yes"-shaped reply, run that suggestion instead of trying
        # (and failing) to parse "yes" as its own request. Cleared either
        # way after one turn, so a stale suggestion from several messages
        # ago can never be accidentally confirmed later, and a "no"/
        # unrelated reply just falls through to being handled normally.
        if session.pending_suggestion:
            suggestion = session.pending_suggestion
            session.pending_suggestion = None
            if followups.is_affirmative(text):
                self.analytics.record_suggestions_confirmed()
                resolved = self._dispatch(session_id, suggestion, None)
                resolved.reply = f'Got it — running "{suggestion}". {resolved.reply}'
                return resolved

        if responses.GREETING_RE.match(text):
            return ChatResponse(responses.GREETING_REPLY, None, 0, "smalltalk.greeting", 1.0, session_id)
        if responses.THANKS_RE.match(text):
            return ChatResponse(responses.THANKS_REPLY, None, 0, "smalltalk.thanks", 1.0, session_id)
        if responses.BYE_RE.match(text):
            return ChatResponse(responses.BYE_REPLY, None, 0, "smalltalk.bye", 1.0, session_id)
        if responses.HELP_RE.search(text):
            return ChatResponse(responses.HELP_REPLY, None, 0, "smalltalk.help", 1.0, session_id)
        # Feature: usage analytics — "stats"/"usage stats"/"how am I doing?"
        # reports this session's intent-hit breakdown and confidence rate
        # (see analytics.py). Checked alongside the other smalltalk
        # commands since it's about the conversation itself, not math.
        if responses.STATS_RE.match(text):
            return ChatResponse(self.analytics.summary(), None, 0, "smalltalk.stats", 1.0, session_id)
        # Feature: "history"/"recap" — a readable log of what's actually
        # been computed this session (see Session.format_history). Distinct
        # from "stats": stats is aggregate/numeric, history is the actual
        # sequence of requests, useful before a "now do the opposite for
        # #2" kind of follow-up a person might type.
        if responses.HISTORY_RE.match(text):
            return ChatResponse(session.format_history(), None, 0, "smalltalk.history", 1.0, session_id)

        # Feature 4: knowledge-base lookup ("what is a derivative?") answers
        # from docs_kb/knowledge_base.json instead of running a computation.
        kb_query = knowledge.extract_query(text)
        if kb_query:
            kb_answer = knowledge.answer(kb_query)
            if kb_answer:
                return ChatResponse(kb_answer, None, 0, "knowledge.lookup", 0.9, session_id)

        # Feature: "explain that" / "why does that matter?" — the pronoun
        # counterpart to a direct knowledge lookup. knowledge.extract_query
        # deliberately returns None for pronoun-referencing text (it's
        # ambiguous what "that" means out of context), so this is the
        # branch that actually resolves it, using whichever intent last
        # ran in this session.
        if followups.is_explain_that(text):
            kb_id = followups.kb_id_for_intent(session.last_intent)
            if kb_id:
                entry_answer = knowledge.answer_by_id(kb_id)
                if entry_answer:
                    return ChatResponse(entry_answer, None, 0, "knowledge.explain_that", 0.85, session_id)
            return ChatResponse(
                "I'm not sure which concept you mean yet — try asking about it by name, "
                "like \"what is a determinant\", or compute something first and I can "
                "explain that.",
                None, 0, "knowledge.explain_that.unknown", 0.3, session_id)

        # Feature 2: chat-driven UI actions ("plot sin(x) from -10 to 10",
        # "plot sin(x) and cos(x)", "clear the graph") short-circuit
        # computation entirely — the action itself is the answer.
        clear_action = actions.detect_clear_graph_action(text)
        if clear_action:
            return ChatResponse("Clearing the graph.", None, 0, "action.clear_graph",
                                 0.9, session_id, action=clear_action)

        plot_action = actions.detect_plot_action(text, session)
        if plot_action:
            eqs = plot_action["payload"]["equations"]
            eq_str = " and ".join(eqs)
            note = (" (plotted as a 2D slice — full 3D surface rendering "
                    "isn't available in the Graph tab yet)" if plot_action["payload"]["is3d"] else "")
            return ChatResponse(f"Switching to the Graph tab and plotting {eq_str}.{note}",
                                 None, 0, "action.plot", 0.9, session_id, action=plot_action)

        for intent in INTENTS:
            for pattern in intent.patterns:
                m = pattern.search(text)
                if m:
                    try:
                        cmd: EngineCommand = intent.build(m, text, session)
                    except Exception as exc:  # noqa: BLE001 - never crash the subprocess
                        return ChatResponse(
                            f"I recognized this as a {intent.name} request but couldn't "
                            f"parse the details ({exc}). Could you rephrase it with explicit "
                            f"numbers/expressions?",
                            None, 0, intent.name, 0.3, session_id)

                    # Feature 3: local pre-flight syntax check before handing
                    # the command to the engine. Only meaningful for the
                    # shorthand/expression forms with an embedded formula;
                    # raw JSON payloads are already well-formed by construction.
                    check = validate.preflight(cmd.engine_input)
                    if not check.ok and _looks_like_bare_formula(cmd.engine_input):
                        return ChatResponse(validate.format_error_reply(cmd.engine_input, check),
                                             None, 0, f"{intent.name}.syntax_error", 0.4, session_id)

                    session.record(text, cmd.engine_input, None, cmd.remembers_expr,
                                    cmd.precision_flag, intent.name)
                    return ChatResponse(cmd.reply, cmd.engine_input, cmd.precision_flag,
                                        intent.name, 0.9, session_id)

        # Feature: precision toggle — "give me that as a decimal" / "show
        # the exact value instead" re-runs the *same* last engine command
        # with the opposite precision flag. Checked only after the main
        # intent registry has already failed to match anything, so it can
        # never shadow a real new computation request that happens to
        # mention "numerically" as part of its own phrasing (several
        # intents already use that word intentionally, e.g. na.bisection).
        toggled_flag = followups.detect_precision_toggle(text)
        if toggled_flag is not None and session.last_engine_input:
            mode_word = "numerically" if toggled_flag == 1 else "symbolically"
            session.record(text, session.last_engine_input, None, None, toggled_flag, session.last_intent)
            return ChatResponse(f"Recomputing that {mode_word}.", session.last_engine_input,
                                 toggled_flag, "followup.precision_toggle", 0.8, session_id)

        if looks_like_plain_expression(text):
            check = validate.preflight(text)
            if not check.ok:
                return ChatResponse(validate.format_error_reply(text, check),
                                     None, 0, "arithmetic.syntax_error", 0.4, session_id)
            session.record(text, text, None, text, 1, "arithmetic.passthrough")
            return ChatResponse("Evaluating that expression.", text, 1, "arithmetic.passthrough",
                                 0.6, session_id)

        # Last resort: still forward it (the engine's own parser gives a
        # precise error), but flag low confidence and explain why. Feature:
        # semantic router — rank every known intent's canonical phrase
        # against the user's text with TF-IDF + cosine similarity (order-
        # and synonym-tolerant, unlike difflib's character-sequence
        # comparison), and reply with graduated confidence instead of
        # either guessing or staying silent:
        #   - one strong, unambiguous match  -> name it directly, and
        #     remember it as a pending_suggestion so a plain "yes" next
        #     turn actually runs it (see the top of this method)
        #   - a few plausible matches        -> offer them as options
        #     (deliberately NOT set as pending_suggestion - guessing which
        #     of several candidates "yes" meant would be worse than asking)
        #   - nothing close                  -> fall back to the generic hint
        session.record(text, text, None, None)
        try:
            matches = semantic_router.route(text)
        except Exception:  # noqa: BLE001 - never let a suggestion feature crash the subprocess
            matches = []
        hint, confirmable_phrase = _format_router_hint(matches)
        if confirmable_phrase:
            session.pending_suggestion = confirmable_phrase
            self.analytics.record_suggestion_offered()
        return ChatResponse(responses.NO_MATCH_REPLY + hint, text, 1, "fallback.passthrough",
                             0.2, session_id)

    def report_result(self, session_id: str, result: str) -> None:
        """Optional: let the caller feed the actual computed result back in,
        so a future turn could reference it. Stored but not yet consumed by
        any intent — see chatbot/README.md 'Extending' section."""
        session = self.sessions.get(session_id)
        session.last_result = result


# Score bands for semantic_router matches, tuned against the corpus's own
# score distribution (TF-IDF cosine similarity over short phrases rarely
# exceeds ~0.85 even for a near-exact rewrite, since distinct content
# words still split the similarity mass).
_ROUTER_CONFIDENT = 0.55
_ROUTER_PLAUSIBLE = 0.30


def _format_router_hint(matches: "list[semantic_router.RouteMatch]") -> "tuple[str, Optional[str]]":
    """Turns ranked semantic_router matches into the trailing sentence(s)
    appended to responses.NO_MATCH_REPLY, with confidence-graduated
    phrasing: a single strong match is named directly, several plausible
    ones are offered as a menu, and nothing close falls back to silence
    (the generic NO_MATCH_REPLY text already covers that case). Returns
    (hint_text, confirmable_phrase) — confirmable_phrase is the phrase to
    store as session.pending_suggestion, or None when the match wasn't
    unambiguous enough to safely auto-run on a bare "yes"."""
    strong = [m for m in matches if m.score >= _ROUTER_CONFIDENT]
    plausible = [m for m in matches if m.score >= _ROUTER_PLAUSIBLE]

    if strong and (len(strong) == 1 or strong[0].score - strong[1].score >= 0.15):
        phrase = strong[0].phrase
        return f' Did you mean something like "{phrase}"? (say "yes" and I\'ll run it)', phrase
    if plausible:
        quoted = ", ".join(f'"{m.phrase}"' for m in plausible[:3])
        return f" Did you mean one of these: {quoted}?", None
    return "", None


def _looks_like_bare_formula(engine_input: str) -> bool:
    """True for plain expressions and calc[...] shorthand (where bracket
    balance is meaningful); false for raw prefix:op|{json} payloads, whose
    braces are already well-formed by construction and shouldn't be
    re-flagged by the bracket checker (JSON objects legitimately nest)."""
    return ":" not in engine_input or "|" not in engine_input


def _session_ttl_from_env(default_seconds: float = 6 * 3600) -> float:
    """MATHENGINE_CHATBOT_SESSION_TTL_SECONDS overrides SessionStore's
    default eviction window (6 hours) - mainly useful for a long-running
    server-side deployment that wants a tighter or looser bound than the
    desktop app's default. Falls back to the default on anything that
    isn't a positive number rather than raising, since a malformed env
    var shouldn't be able to crash chatbot startup."""
    raw = os.environ.get("MATHENGINE_CHATBOT_SESSION_TTL_SECONDS")
    if not raw:
        return default_seconds
    try:
        value = float(raw)
    except ValueError:
        return default_seconds
    return value if value > 0 else default_seconds
