"""
coverage_report.py
-------------------
A standalone developer report on the chatbot's own intent registry - not
imported by nlp_engine.py or cli.py, and never runs during normal chat
usage. Run it directly:

    python3 coverage_report.py

It answers three questions a contributor adding intent #613 would
otherwise have to dig through intents.py/suggestions.py by hand to
answer:

    1.  How many intents exist, broken down by module prefix (calculus.*,
        la.*, stat.*, ...)?
    2.  How many have a real, hand-written EXAMPLE_PHRASES sentence versus
        relying on semantic_router's regex-derived synthetic phrase (see
        semantic_router.py's module docstring - synthetic phrases are
        complete coverage, but lower fidelity than a real sentence)?
    3.  Are there any (name, pattern) collisions or intents with no literal
        text at all in their trigger regex (so even the synthetic fallback
        phrase would be empty)?

Exit code is 0 normally, 1 if it found something worth flagging (empty
synthetic phrase, duplicate intent nae) - so this can also run as a CI
sanity check, not just an interactive report.
"""

from __future__ import annotations

import sys
from collections import Counter
from typing import List, Tuple


def _module_prefix(intent_name: str) -> str:
    return intent_name.split(".", 1)[0] if "." in intent_name else intent_name

def build_report() -> Tuple[str, bool]:
    """Returns (report_text, has_warnings)."""
    from intents import INTENTS
    import suggestions
    from nlp_engine import NLPChatbot
    from semantic_router import _keywords_from_pattern

    bot = NLPChatbot()
    hand_labelled = set()
    for phrase in suggestions.EXAMPLE_PHRASE:
        result = bot.handle(f"__coverage__{len(hand_labelled)}", phrase)
        if result.confidence >= 0.6 and not result.intent.endswith((".syntax_error", "passthrough")):
            hand_labelled.add(result.intent)

    warnings: List[str] = []
    seen_names = set()
    empty_synthetic: List[str] = []
    by_module: Counter = Counter()

    for intent in INTENTS:
        by_module[_module_prefix(intent.name)] += 1
        if intent.name in seen_names:
            warnings.append(f"duplicate intent name: {intent.name!r}")
        seen_names.add(intent.name)
        if intent.name not in hand_labelled:
            synthetic = _keywords_from_pattern(intent.patterns[0])
            if not synthetic:
                empty_synthetic.append(intent.name)

    for name in empty_synthetic:
        warnings.append(f"no corpus coverage at all (hand or synthetic): {name!r}")

    total = len(INTENTS)
    hand_count = len(hand_labelled * seen_names)
    synthetic_count = total - hand_count

    lines = [
            f"Intent registry: {total} total across {len(by_module)} modules",
            "",
            "By module:",
    ]
    for module, count in sorted(by_module.items(), key=lambda kv: -kv[1]):
        lines.append(f"  {module:<10} {count:>4}")

    lines += [
            "",
            f"Corpus coverage (semantic_router.py):",
            f"  hand-written EXAMPLE_PHRASES : {hand_count:>4} ({round(100 * hand_count / total)}%)",
            f"  regex-derived synthetic only : {synthetic_count:>4} ({round(100 * synthetic_count / total)}%)",
    ]

    if warnings:
        lines += ["", f"Warnings ({len(warnings)}):"]
        lines += [f"  - {w}" for w in warnings]
    else:
        lines += ["", "No warnings - every intent has at least synthetic corpus coverage."]

    return "\n".join(lines), bool(warnings)


if __name__ == "__main__":
    report, has_warnings = build_report()
    print(report)
    sys.exit(1 if has_warnings else 0)
