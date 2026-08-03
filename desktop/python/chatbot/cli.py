#!/usr/bin/env python3
"""
cli.py
-------
Entry point run by Java (ChatbotBridge.java) via ProcessBuilder, and by
the Spring server (ChatController.java) the same way. Stdlib only -
no pip install step, matching the rest of this project's zero-setup-
beyond-a-compiler build story.

Protocol: one JSON object per line on stdin, one JSON object per line
on stdout (line-delimited JSON, i.e. JSONL) so a single subprocess can
stay alive for a whole app session instead of paying process-startup
cost per message.

Request:
    {"session_id": "abc123", "message": "derivative of x^2"}

    Optional fields:
        "result": "<string>"  - the engine's result for the *previous*
                                turn, if the caller wants it remembered
                                (see NLPChatbot.report_result).

Response:
    {
        "session_id":     "abc123",
        "reply":          "Differentiating x^2 with respect to x.",
        "engine_input":   "diff[x^2,x]",
        "precision_flag": 0
        "intent":         "calculus.derivative",
        "confidence":     0.9
    }

    On malformed input:
    {"error": "<message>"}

Also supports one-shot mode for quick manual testing:
    python cli.py --once "derivative of x^2"
"""

import json
import sys

from nlp_engine import NLPChatbot


def _handle_line(bot: NLPChatbot, line: str) -> dict:
    try:
        req = json.loads(line)
    except json.JSONDecodeError as exc:
        return {"error": f"invalid JSON: {exc}"}

    session_id = str(req.get("session_id") or "default")
    message = req.get("message", "")

    if "result" in req and req["result"] is not None:
        bot.report_result(session_id, str(req["result"]))

    resp = bot.handle(session_id, message)
    return resp.to_dict()


def main() -> None:
    bot = NLPChatbot()

    if len(sys.argv) >= 3 and sys.argv[1] == "--once":
        message = " ".join(sys.argv[2:])
        resp = bot.handle("cli", message)
        print(json.dumps(resp.to_dict(), ensure_ascii=False))
        return

    # Persistent JSONL loop - one request per line, flush after every reply
    # on the Java side's BufferedReader.readLine() never blocks waiting for
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
