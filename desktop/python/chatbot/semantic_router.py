"""
semantic_router.py
────────────────────
A dependency-free semantic router used when the regex intent registry in
intents.py finds no match (see nlp_engine.py's fallback branch).
 
Why this exists
----------------
Every Intent in intents.py is triggered by hand-written regexes, which are
precise but brittle: "deriv of x^2", "slope of x^2", and "rate of change
of x^2 wrt x" should probably all reach calculus.derivative, but adding a
new regex for every synonym/word-order the 612 intents might see is a
losing battle. Previously, the only fallback was suggestions.py, which
ranks canned example phrases against the user's text using difflib
(character-sequence similarity) - so "deriv of x^2" scores poorly against
"derivative of x^2 + 3x" even though they mean the same thing, because
difflib doesn't know "deriv" and "derivative" share a root, or that word
order shouldn't matter.
 
This module replaces that character-level comparison with a small
TF-IDF + cosine-similarity model over a bag of words, which is order-
independent and down-weights filler words ("of", "the", "is") that
appear in almost every phrase, in favor of the distinctive terms
("derivative", "eigenvalues", "gcd") that actually identify an intent.
It stays stdlib-only (re, math, collections) for the same reason
entities.py does: this is the one piece of an otherwise fully offline
C++/Java/PowerShell build, and a pip dependency (scikit-learn, spaCy,
sentence-transformers, ...) would be the odd one out.
 
What it does NOT do
--------------------
It does not attempt to auto-build an EngineCommand for the matched
intent - the 612 intents in intents.py expect wildly different argument
shapes (a single expression, an expression + bounds, a matrix literal, a
dataset, ...), and guessing that shape generically for an intent whose
own regex didn't match would be more likely to produce a wrong answer
than a helpful one. Instead, the router's job is to point the user (and
nlp_engine's fallback reply) at the *right* worked example with high
confidence, so the very next thing they type matches for real.
 
Training data
--------------
The corpus has two tiers. The primary tier is built from
suggestions.EXAMPLE_PHRASES, each labelled with whatever
nlp_engine.NLPChatbot.handle() actually resolves it to - that keeps
those labels self-consistent with the live dispatch order (smalltalk ->
knowledge -> actions -> INTENTS -> ...) instead of duplicating that
logic here, and means they never silently drift out of sync if
nlp_engine's pipeline changes.
 
But EXAMPLE_PHRASES only hand-covers a few dozen intents out of the
600+ in intents.INTENTS - every other intent had zero representation in
the corpus and could never be suggested, no matter how close a query
came. The second tier (_synthetic_corpus_entries) fills every remaining
intent with a phrase mechanically derived from its own trigger regex's
literal text, so coverage is complete (see
test_every_intent_has_corpus_coverage) without hand-authoring hundreds
more example sentences.
"""

from __future__ import annotations

import math
import re
from collections import Counter
from dataclasses import dataclass
from typing import Dict, List, Optional, Tuple

_TOKEN_RE = re.compile(r"[a-zA-Z][a-zA-Z0-9']*")

# A short list of very common words that carry little intent-distinguishing
# signal in this domain. Deliberately small - TF-IDF weighting already
# suppresses anything that shows up in most phrases, so this is just a
# cheap head start for the tiniest, most frequent connectors.
_STOPWORDS = {
        "a", "an", "the", "of", "is", "are", "and", "to", "for", "with",
        "at", "in", "on", "as", "that", "this", "it", "its", "from",
}


_NGRAM_N = 4


def _char_ngrams(word: str, n: int = _NGRAM_N) -> List[str]:
    """Sub-word features for one token, prefixed so they never collide with
    a whole-word token of the same text. These are what let "eigen values"
    (two words) still score against the corpus's "eigenvalues" (one word),
    or a small misspelling like "deviaton" still overlap heavily with
    "deviation" - problems whole-word tokens alone can't see, since they
    either match exactly or not at all."""
    if len(word) <= n:
        return [f"#{word}#"]
    return [f"#{word[i:i + n]}" for i in range(len(word) - n + 1)]


def _tokenize(text: str) -> List[str]:
    """Whole-word tokens plus each word's character n-grams. Mixing both in
    one bag-of-features means two phrases can still score similarity from
    shared roots/compounds even when they don't share a single whole word -
    at the cost of a slightly noisier signal, which IDF weighting (common
    n-grams look almost as common as common words) keeps in check."""
    words = [t.lower() for t in _TOKEN_RE.findall(text) if t.lower() not in _STOPWORDS]
    tokens = list(words)
    for w in words:
        tokens.extend(_char_ngrams(w))
    return tokens


@dataclass
class RouteMatch:
    intent: str         # the intent name this phrase resolved to (e.g. "calculus.derivative")
    phrase: str         # the canonical example phrase, verbatim - safe to show the user
    score: float        # cosine similarity in [0,1]


class SemanticRouter:
    """TF-IDF + cosine-similarity router over a small labelled phrase corpus.

    Usage:
        router = SemanticRouter(labelled_phrases)       # [(intent_name, phrase), ...]
        router.route("deriv of x^2")
        -> [RouteMatch(intent="calculus.derivative", phrase="derivative of x^2 + 3x", score=0.71), ...]
    """

    def __init__(self, lavelled_phrases: List[Tuple[str, str]]):
        # Keep only entries with an actual label and phrase; a phrase that
        # resolved to nothing usable (e.g. a low-confidence fallback) isn't
        # a trustworthy training example and would only teach the router
        # to route people into another dead end.
        self._entries: List[Tuple[str, str]] = [
                (intent, phrase) for intent, phrase in labelled_phrases if intent and phrase
        ]
        self._doc_tokens: List[List[str]] = [_tokenize(phrase) for _, phrase in self._entries]
        self._idf: Dict[str, float] = self._compute_idf(self._doc_tokens)
        self._doc_vectors: List[Dict[str, float]] = [
                self._vectorize(tokens) for tokens in self._doc_tokens
        ]

    @staticmethod
    def _compute_idf(documents: List[List[str]]) -> Dict[str, float]:
        n_docs = len(documents)
        doc_freq: Counter = Counter()
        for tokens in documents:
            doc_freq.update(set(tokens))
        # Smoothed IDF (add-one on both numerator and denominator) so a term
        # that appears in every single document still gets a small positive
        # weight instead of exactly zero.
        return {
                term: math.log((1 + n_docs) / (1 + df)) + 1.0
                for term, df in doc_freq.items()
        }

    def _vectorize(self, tokens: List[str]) -> Dict[str, float]:
        if not tokens:
            return {}
        tf = Counter(tokens)
        vec = {term: count * self._idf.get(term, 0.0) for term, count in tf.items()}
        norm = math.sqrt(sum(w * w for w in vec.values()))
        if norm > 0:
            vec = {term: w / norm for term, w in vec.items()}
        return vec

    def _query_vector(self, text: str) -> Dict[str, float]:
        # Re-use the corpus's DIF table for the query so both sides are
        # weighted on the same scale; any query-only temr (idf=0, since it
        # never appeared in the corpus) simply contributes nothing, which
        # is the right behavior - it's a term the router has no evidence
        # about, not a term that should be penalized or crash the lookup.
        return self._vectorize(_tokenize(text))

    @staticmethod
    def _cosine(a: Dict[str, float], b: Dict[str, float]) -> float:
        if not a or not b:
            return 0.0
        # Vectors are already L2-normalized in _vectorize, so cosine
        # similarity is just the dot product over the smaller vector's keys.
        if len(a) > len(b):
            a, b = b, a
        return sum(w * b.get(term, 0.0) for term, w in a.items())

    def route(self, text: str, top_k: int = 3) -> List[RouteMatch]:
        """Returns up to `top_k` best-matching (intent, phrase, score)
        triples, sorted by score descending, with at most one result per
        intent (its single best-scoring phrase) so callers don't show the
        same suggested operation twice."""
        query_vec = self._query_vector(text)
        if not query_vec:
            return []

        best_per_intent: Dict[str, RouteMatch] = {}
        for (intent, phrase), doc_vec in zip(self._entries, self._doc_vectors):
            score = self._cosine(query_vec, doc_vec)
            if score <= 0:
                continue
            current = best_per_intent.get(intent)
            if current is None or score > current.score:
                best_per_intent[intent] = RouteMatch(intent, phrase, score)

        ranked = sorted(best_per_intent.values(), key=lambda m: m.score, reverse=True)
        return ranked[:top_k]


# --------------------------------------------------------------------------------------------
# Corpus construction and a module-level singleton, built lazily so this
# module never has to import nlp_engine at load time (nlp_engine imports
# this module, so a top-level circular import would break both).
# --------------------------------------------------------------------------------------------

_router_singleton: Optional[SemanticRouter] = None

# --- Synthetic corpus entries, derived directly from each Intent's own trigger regex --------
# suggestions.EXAMPLE_PHRASES only hand-curates ~40 realistic sentences,
# covering a small fraction of the 400+ entries in intents.INTENTS - every
# other intent had zero semantic_router coverage, so a synonym/misspelled
# query for e.g. "romberg integration" or "kl divergence" could never be
# suggested no matter how close it was, simply because nothing in the
# corpus represented that intent at all.
#
# Rather than hand-write the 500+ more example sentences (unmaintainable, and
# guaranteed to drift from the actual patterns over time), this extracts
# the literal keyword text already embedded in each intent's own regex -
# which is, after all, exactly the vocabulary a real trigger phrase would
# use - and treats that as a lower-fidelity but zero-maintenance phrase.
# A hand-curated EXAMPLE_PHRASES entry always wins where one exists: this
# only fills in intents nothing else has already labelled.
_REGEX_GROUP_OPEN_RE = re.compile(r"\(\?P<\w+>|\(\?:")
_REGEX_ESCAPE_CLASS_RE = re.compile(r"\\[bBdDsSwWAZ]")
_REGEX_OTHER_ESCAPE_RE = re.compile(r"\\.")
_REGEX_JUNK_CHARS_RE = re.compile(r"[.^$*+{}\[\]()|?]")
_WORD_RE = re.compile(r"[a-zA-Z][a-zA-Z']*")


def _keywords_from_pattern(patterns: "re.Patterm") -> str:
    """Strips regex syntax out of one Intent pattern's source, leaving just
    the literal words it was built from - e.g. the pattern for "romberg
    integration" style triggers becomes the phrase "romberg integration
    from to" once (?P<...>...) capture groups and regex metacharacters are
    removed. Consecutive duplicate words (common after an alternation like
    (?:derivative|deriv) collapses to nothing extractable) are collapsed."""
    src = pattern.pattern
    src = _REGEX_GROUP_OPEN_RE.sub(" ", src)
    src = _REGEX_ESCAPE_CLASS_RE.sub(" ", src)
    src = _REGEX_OTHER_ESCAPE_RE.sub(" ", src)
    src = _REGEX_JUNK_CHARS_RE.sub(" ", src)
    words = [w.strip("'") for w in _WORD_RE.findall(src)]
    words = [w for w in words if len(w) > 1]
    deduped: List[str] = []
    for w in words:
        if not deduped or deduped[-1].lower() != w.lower():
            deduped.append(w)
    return " ".join(deduped)


def _synthetic_corpus_entries(already_labelled_intents: set) -> List[Tuple[str, str]]:
    from intents import INTENTS # deferred: see module docstring

    entries: List[Tuple[str, str]] = []
    for intent in INTENTS:
        if intent.name in already_labelled_intents:
            continue # a real example phrase already covers this intent
        phrase = _keywords_from_pattern(intent.patterns[0])
        if phrase:
            entries.append((intent.name, phrase))
    return entries


def _build_labelled_corpus() -> List[Tuple[str, str]]:
    # Deferred import - see module docstring / comment above
    from nlp_engine import NLPChatbot
    import suggestions

    bot = NLPChatbot()
    labelled: List[Tuple[str, str]] = []
    for phrase in suggestions.EXAMPLE_PHRASES:
        # A throwaway session per phrase: these are one-shot canonical
        # examples, not a real conversation, so there's no reason for one
        # phrase's session state (last expression, etc.) to leak into the
        # next phrase's labelling
        result = bot.handle(f"__router_corpus__{len(labelled)}", phrase)
        # Only trust labels the pipeline itself was confident about -
        # a phrase that only reached fallback.passthrough or an
        # arithmetic/syntax-error branch teaches the router nothing useful
        # about "which intent" it represents.
        if result.confidence >= 0.6 and not result.intent.endswith((".syntax_error", "passthrough")):
            labelled.append((result.intent, phrase))
    
    # Fill in every intent EXAMPLE_PHRASES doesn't reach with a synthetic,
    # regex-derived phrase - see _synthetic_corpus_entries() above. These
    # are lower-fidelity than a real hand-written sentence (no natural
    # word order, occasional leftover noise word), but a rough phrase is
    # still far better than the total silence those 500+ intents has
    # before: without this, the router could never suggest them at all,
    # no matter how close a real query came.
    already_labelled = {intent for intent, _ in labelled}
    labelled.extend(_synthetic_corpus_entries(already_labelled))
    return labelled


def get_router() -> SemanticRouter:
    """Returns the shared, lazily-built router. Building it re-runs every
    EXAMPLE_PHRASES entry through the real chatbot pipeline once (a few
    dozen calls, all local regex work - no network, no heavy model load),
    then caches the result for the lifetime of the process."""
    global _router_singleton
    if _router_singleton is None:
        _router_singleton = SemanticRouter(_build_labelled_corpus())
    return _router_singleton


def route(text: str, top_k: int = 3) -> List[RouteMatch]:
    """Convenience wrapper: route `text` using the shared singleton router."""
    return get_router().route(text, top_k=top_k)
