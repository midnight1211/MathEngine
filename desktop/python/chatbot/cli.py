#!/usr/bin/env python3
"""
cli.py
───────
Entry point run by Java (ChatbotBridge.java) via ProcessBuilder, and by
the Spring server (ChatController.java) the same way. Stdlib only —
no pip install step, matching the rest of this project's zero-setup-
beyond-a-compiler build story.

Protocol: one JSON object per line on stdin, one JSON object per line
on stdout (line-delimited JSON, i.e. JSONL) so a single subprocess can
stay alive for a whole app session instead of paying process-startup
cost per message.

Request:
    {"session_id": "abc123", "message": "derivative of x^2"}

    Optional fields:
      "result": "<string>"   — the engine's result for the *previous*
                                turn, if the caller wants it remembered
                                (see NLPChatbot.report_result).
      "workspace": {...}     — Feature 1 (Workspace Sync): a snapshot of
                                what the desktop UI's other tabs are
                                currently showing, e.g.
                                {"activeTab": "Compute",
                                 "lastExpression": "[[1,2],[2,4]]",
                                 "lastResult": "-2", "lastMode": "Symbolic"}.
                                Lets a fresh chat reference "this matrix"/
                                "that expression" without it having been
                                typed into the chat first.

Response:
    {
      "session_id": "abc123",
      "reply": "Differentiating x^2 with respect to x.",
      "engine_input": "diff[x^2,x]",
      "precision_flag": 0,
      "intent": "calculus.derivative",
      "confidence": 0.9,
      "action": null
    }

    "action" (Feature 2, Chat-Driven Actions) is non-null when the message
    should change what the desktop UI is showing rather than compute a
    result, e.g.:
      {"type": "SWITCH_TAB", "target": "Graph",
       "payload": {"equation": "sin(x)", "range": [-10, 10], "is3d": false}}
    When present, "engine_input" is null — there's nothing to compute,
    only a UI action to carry out.

    On malformed input:
    {"error": "<message>"}

Also supports one-shot mode for quick manual testing:
    python cli.py --once "derivative of x^2"
"""

import json
import sys
import traceback

from nlp_engine import NLPChatbot


def _handle_line(bot: NLPChatbot, line: str) -> dict:
    try:
        req = json.loads(line)
    except json.JSONDecodeError as exc:
        return {"error": f"invalid JSON: {exc}"}

    session_id = str(req.get("session_id") or "default")
    message = req.get("message", "")
    workspace = req.get("workspace")

    try:
        if "result" in req and req["result"] is not None:
            bot.report_result(session_id, str(req["result"]))

        resp = bot.handle(session_id, message, workspace)
        return resp.to_dict()
    except Exception as exc:  # noqa: BLE001
        # CRITICAL: a bug in classifying one message must never kill the
        # persistent subprocess — Java only restarts it on the *next* call
        # after noticing stdout closed, so every message sent in between
        # would silently fall back to raw pass-through (see the Java-side
        # fallback in ChatbotBridge/ChatbotService). Log the real traceback
        # to stderr for diagnosis, but always hand stdout a well-formed
        # JSON response so the loop below keeps running.
        traceback.print_exc(file=sys.stderr)
        return {
            "session_id": session_id,
            "reply": f"Something went wrong understanding that ({exc}). Could you try rephrasing?",
            "engine_input": None,
            "precision_flag": 0,
            "intent": "fallback.internal_error",
            "confidence": 0.0,
            "action": None,
        }


def main() -> None:
    # Force UTF-8 stdio regardless of the platform's default console
    # encoding (Windows in particular often defaults sys.stdin/stdout to a
    # legacy codepage like cp1252 rather than UTF-8 when not attached to an
    # interactive terminal). Java always writes/reads UTF-8
    # (ChatbotBridge/ChatbotService use StandardCharsets.UTF_8 explicitly),
    # so this keeps both sides of the pipe on the same page.
    for stream in (sys.stdin, sys.stdout):
        try:
            stream.reconfigure(encoding="utf-8", errors="replace")
        except (AttributeError, ValueError):
            pass  # Python <3.7 or a stream that doesn't support reconfigure

    bot = NLPChatbot()

    if len(sys.argv) >= 3 and sys.argv[1] == "--once":
        message = " ".join(sys.argv[2:])
        resp = bot.handle("cli", message)
        print(json.dumps(resp.to_dict(), ensure_ascii=False))
        return

    # Persistent JSONL loop — one request per line, flush after every reply
    # so the Java side's BufferedReader.readLine() never blocks waiting for
    # buffered output.
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        if line in ("__exit__", "__quit__"):
            break
        out = _handle_line(bot, line)
        sys.stdout.write(json.dumps(out, ensure_ascii=False) + "\n")
        sys.stdout.flush()


if __name__ == "__main__":
    main()
