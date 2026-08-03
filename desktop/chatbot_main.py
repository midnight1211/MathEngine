#!/usr/bin/env python3
"""
chatbot_main.py — MathEngine chatbot entry point.
Spawned by ChatbotPanel.java as a subprocess.
"""
from __future__ import annotations
import sys, os, json, traceback

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "python"))

from chatbot.io import send, send_msg, send_err
from chatbot.state import STATE
from chatbot.handler import handle_message


def main() -> None:
    send_msg(
        "Hi! I'm your **MathEngine assistant** \n"
        "Ask me a math question in plain English, or type any expression directly.\n"
        "Type **help** to see what I can do."
    )
    sys.stdout.flush()

    for raw_line in sys.stdin:
        raw_line = raw_line.strip()
        if not raw_line:
            continue
        try:
            msg = json.loads(raw_line)
        except json.JSONDecodeError:
            send_err(f"Invalid JSON: {raw_line!r}")
            sys.stdout.flush()
            continue

        t = msg.get("type", "")
        if   t == "ping":     
            send({"type": "pong"})
        elif t == "auth":     
            STATE.auth_token = msg.get("token")
        elif t == "shutdown": 
            sys.exit(0)
        elif t == "message":
            text = msg.get("text", "").strip()
            try:
                # Capture return output or handle inside handle_message
                res = handle_message(text)
                if isinstance(res, str) and res:
                    send_msg(res)
                elif isinstance(res, dict) and res:
                    send(res)
            except Exception:
                send_err(f"Internal error:\n{traceback.format_exc()}")
        else:
            send_err(f"Unknown message type: {t!r}")

        # CRITICAL: Force flush stdout so JavaFX receives response instantly
        sys.stdout.flush()


if __name__ == "__main__":
    main()
