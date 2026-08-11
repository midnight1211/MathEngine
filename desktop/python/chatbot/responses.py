"""
responses.py
-------------
Small talk and framing text isn't a math computation itself:
greetings, help/capabilities, and templates for wrapping engine results
or errors in a conversational reply.
"""

import re

GREETING_RE = re.compile(r"^\s*(hi|hello|hey|yo|good (morning|afternoon|evening))\b[!.]*\s*$", re.IGNORECASE)
THANKS_RE = re.compile(r"^\s*(thanks|thank you|thx|ty)\b", re.IGNORECASE)
HELP_RE = re.compile(r"^\b(help|what can you do|capabilities|examples?)\b", re.IGNORECASE)
BYE_RE = re.compile(r"^\s*(bye|goodbye|see ya|exit|quit)\b", re.IGNORECASE)
STATS_RE = re.compile(
    r"^\s*(?:usage )?stats\b|^\s*show (?:me )?(?:the )?stats\b|"
    r"^\s*(?:chat|session) stats\b|^\s*how am i doing\??$",
    re.IGNORECASE,
)
HISTORY_RE = re.compile(
    r"^\s*(?:show (?:me )?(?:my |the )?)?history\b|"
    r"^\s*what (?:have|did) (?:i|we) (?:ask|say|do)(?:ed)?\??$|"
    r"^\s*recap\b",
    re.IGNORECASE,
)

GREETING_REPLY = (
        "Hi! I'm the MathEngine assistant. Ask me things like "
        "\"derivative of x^2 + 3x\", \"determinant of [[1,2],[3,4]]\", "
        "\"mean of [4,8,15,16,23,42]\", or \"gcd of 48 and 18\" - "
        "or just type a plain expression like \"2^8 + sqrt(16)\"."
)

THANKS_REPLY = "You're welcome - send another expression whenevery you're ready."

BYE_REPLY = "Goodbye!"

HELP_REPLY = (
        "I can turn natural-language math requests into MathEngine computations "
        "across calculus, linear algebra, statistics, number theory, discrete "
        "math, geometry, complex analysis, numerical analysis, differential "
        "equations, and abstract algebra. A few examples:\n"
        "  \u2022 \"derivative of sin(x) * x^2\"\n"
        "  \u2022 \"integrate x^2 from 0 to 3\"\n"
        "  \u2022 \"limit of (1+1/x)^x as x approaches infinity\"\n"
        "  \u2022 \"inverse of [[2,1],[1,1]]\"\n"
        "  \u2022 \"mean and standard deviation of [4,8,15,16,23,42]\"\n"
        "  \u2022 \"is 97 prime\"\n"
        "  \u2022 \"10 choose 3\"\n"
        "  \u2022 \"distance between (0,0) and (3,4)\"\n"
        "  \u2022 \"solve dy/dt = -2y with y(0) = 5 to t = 4\"\n"
        "Plain expressions like \"2^8 + sqrt(16)\" work too, and I remember "
        "your last expression, so \"now integrate that\" works as a follow-up."
)

NO_MATCH_REPLY = (
        "I couldn't match that to a specific operation, so I passed it to the "
        "engine as a plain expression. If that's not what you meant, try "
        "phrasing it like \"derivative of ...\", \"determinant of [[...]]\", "
        "or ask me for \"help\" to see more examples."
)


def format_result_reply(framing: str, result: str) -> str:
    return f"{framing}\n\n{result}"


def format_error_reply(framing: str, error: str) -> str:
    return f"{framing} ran into a problem: {error}"
