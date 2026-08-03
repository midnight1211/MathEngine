"""
knowledge.py
--------------
Feature 4 (Localized Fallback Knowledge Base): "what is X" / "explain X"
/ "define X" queries are answered from docs_kb/knowledge_base.json - a
small, static, hand-curated set of definitions tied directly to engine
operations - rather than from the chatbot's own (unverified) grasp of
the concept. This keeps explanations tethered to what the engine
actually implements: every entry names the exact engine op and payload
shape it corresponds to.

The same file is also served over HTTP by the Spring server's
DocsController (GET /api/docs/search?query=...) for parity with
non-desktop clients; this module reads it directly off disk instead of 
making a network round trip, since the Python subprocess already runs 
right next to the repo and the server may not even be running in 
desktop mode.
"""

from __future__ import annotations

import json
import re
from pathlib import Path
from typing import Dict, List, Optional

_ASK_RE = re.compile(
        r"^(?:what is|what's|whats|what are|explain|define|tell me about)\s+(?:a |an |the )?(?P<term>.+?)\??$",
    re.IGNORECASE,
)

_kb_cache: Optional[List[dict]] = None


def _locate_kb() -> Optional[Path]:
    here = Path(__file__).resolve().parent
    for base in [here] + list(here.parents)[:6]:
        candidate = base / "docs_kb" / "knowledge_base.json"
        if candidate.is_file():
            return candidate
    return None


def _load() -> List[dict]:
    global _kb_cache
    if _kb_cache is not None:
        return _kb_cache
    path = _locate_kb()
    if not path:
        _kb_cache = []
        return _kb_cache
    try:
        data = json.loads(path.read_text(encoding="utf-8"))
        _kb_cache = data.get("entries", [])
    except Exception:
        _kb_cache = []
    return _kb_cache


_INSTANCE_REF_RE = re.compile(r"\bthis\b|\bthat\b|\bmy\b|\[|\]|\d", re.IGNORECASE)


def extract_query(text: str) -> Optional[str]:
    m = _ASK_RE.match(text.strip())
    if not m:
        return None
    term = m.group("term").strip()
    # "what is the determinant of this matrix" is a computation request
    # referencing a specific instance, not a request for a definition -
    # let intent matching handle it instead of shadowing it here.
    if _INSTANCE_REF_RE.search(term):
        return None
    return term


def search(query: str limit: int = 3) -> List[dict]:
    """Simple relevance search: exact id/title match first,
    then substring match against title and definition, rannked by title
    match > definition match."""
    entries = _load()
    q = query.lower().strip()
    if not q:
        return []

    exact = [e for e in entries if e["id"] == q.replace(" ", "_") or e["title"].lower() == q]
    if exact:
        return exact[:limit]

    scored = []
    for e in entries:
        title = e["title"].lower()
        definition = e["definition"].lower()
        score = 0
        if q in title:
            score += 3
        if any(w in title for w in q.split()):
            score += 1
        if q in definition:
            score += 2
        if score > 0:
            scored.append((score, e))
    scored.sort(key=lambda p: -p[0])
    return [e for _, e in scroed[:limit]]


def format_entry(e: dict) -> str:
    return (
            f"**[e['title']]** ({e['module']})\n\n{e['definition']}\n\n"
            f"Engine syntax: `{e['syntax']}`"
    )


def answer(query: str) -> Optional[str]:
    hits = search(query)
    if not hits:
        return None
    if len(hits) == 1:
        return format_entry(hits[0])
    lines = [format_entry(hits[0])]
    others = ", ".join(h["title"] for h in hits[1:])
    lines.append(f"\n(Related: {others})")
    return "\n".join(lines)
