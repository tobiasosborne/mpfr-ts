"""callgraph.py — extract the mpfr_*/mpn_* call graph from mpfr/src/*.c.

Output drives ``ralph.py --next`` (Phase A3) to pick the next pending
function in dependency order. The output schema (see
``docs/worklog/005-scale-out-handoff.md``):

    {
      "generated_at": "ISO-8601 UTC",
      "mpfr_source_root": "mpfr/src",
      "functions": {
        "mpfr_add": {
          "deps": ["mpfr_set", "mpn_add_n", ...],
          "class": "arithmetic",
          "topo_rank": 42,
          "defined_in": "add.c"
        },
        ...
      }
    }

Heuristics (acceptable for Pilot — clang AST is overkill):

  * Function definitions: lines that start with ``mpfr_<name> (`` or
    ``mpn_<name> (`` at column 0 (return type lives on the line above
    in the standard MPFR style; we don't need to parse it).
    Single-line ``int mpfr_foo(...) {`` is also recognised.
  * Callsites inside a function body: tokens matching
    ``(mpfr|mpn)_[a-z0-9_]+`` followed by ``\\s*\\(``. The body is
    delimited by the first ``{`` after the signature and the matching
    ``}`` (brace depth tracking).
  * Comments and string literals are stripped from the source before
    scanning to avoid false positives (mirrors
    ``eval/harness/ast_check.ts``'s strip logic).

Macro-expansion callsites (e.g. ``MPFR_INC_PREC(...)`` calling into
mpfr-impl headers) are accepted false-negatives. A clang AST pass is
flagged in worklog 005 as a Production-phase enhancement.

Usage
-----

    python3 eval/driver/callgraph.py \\
        --src mpfr/src \\
        --output eval/driver/callgraph.json

Defaults are ``--src mpfr/src`` and ``--output eval/driver/callgraph.json``,
both resolved relative to the current working directory (when run from
the repo root, the defaults work).

Ref: docs/worklog/005-scale-out-handoff.md §"Components to build" item 1.
Ref: CLAUDE.md §"Practical guidance" — mpfr/src/ is gitignored, must be
cloned per machine.
"""

from __future__ import annotations

import argparse
import json
import re
import sys
from dataclasses import dataclass, field
from datetime import datetime, timezone
from pathlib import Path


# ---------------------------------------------------------------------------
# Module-level patterns and tables
# ---------------------------------------------------------------------------


# Candidate function-definition pattern. We grab every ``(mpfr|mpn)_<name>``
# occurrence followed by either ``\s*\(`` (the common style) or
# ``)\s*\(`` (the paren-wrapped style MPFR uses to escape macro hijacks
# of predicates like ``mpfr_inf_p`` — see ``mpfr/src/isinf.c``: the file
# declares ``int\n(mpfr_inf_p) (mpfr_srcptr x)`` so the macro defined in
# ``mpfr.h`` doesn't intercept the definition).
#
# Two reasons we don't require column-0 anchoring on the name:
#
#   1. The synthetic test fixtures (and human-written one-liners) use
#      ``void mpfr_foo(void) { ... }`` — name NOT at column 0.
#   2. MPFR's standard style puts the return type on the previous line
#      and the name at column 0 — that case still matches a non-anchored
#      pattern.
#
# The cost of being non-anchored is that we also match call-sites with
# this regex; that's fine because ``find_function_body`` rejects any
# match whose ``)`` is immediately followed by a ``;`` (prototype) or
# is itself inside an expression that has no following ``{`` (call).
DEF_NAME_RE = re.compile(
    r"\b(?P<name>(?:mpfr|mpn)_[a-z0-9_]+)\s*\)?\s*\(",
)


# Token form: a callee identifier followed by `(`. We use a non-anchored
# pattern (no `^`) and a word boundary on the left to avoid matching
# inside-the-middle-of-a-word substrings.
CALLEE_RE = re.compile(r"\b(?P<name>(?:mpfr|mpn)_[a-z0-9_]+)\s*\(")


# A valid callee/function-name token. Lowercased ASCII only — CLAUDE.md
# Rule 13 (no Cyrillic homoglyphs); the agent-emitted port files have
# the homoglyph gate; here we just sanity-filter.
NAME_RE = re.compile(r"^(?:mpfr|mpn)_[a-z0-9_]+$")


# Class assignment is by filename + function-name heuristic, priority
# order: substrate > transcendental > conversion > arithmetic > misc.
#
# - Substrate: any mpn_* function, regardless of which .c file holds it.
# - Transcendental: files whose stem matches one of these names. The
#   pattern is intentionally a stem-equality match plus optional
#   suffixes (`_2`, `_aux`, ...) so `mpfr_exp_2` in `exp_2.c` still
#   buckets as transcendental.
# - Conversion: filenames of the form `get_<type>.c` / `set_<type>.c` /
#   `init_<type>.c`.
# - Arithmetic: filenames of the form `add.c` / `add_d.c` / `mul.c`
#   etc.; the basename's first identifier-block matches one of a small
#   set.

_TRANSCENDENTAL_STEMS: frozenset[str] = frozenset({
    "exp", "exp2", "exp10", "expm1",
    "log", "log2", "log10", "log1p", "log_ui",
    "sin", "cos", "tan", "sin_cos", "sincos",
    "asin", "acos", "atan", "atan2",
    "sinh", "cosh", "tanh", "sinh_cosh",
    "asinh", "acosh", "atanh",
    "pow", "pow_si", "pow_ui", "pow_z",
    "sqrt", "sqrt_ui", "cbrt", "root", "rootn_ui", "rootn_si",
    "gamma", "gamma_inc", "lngamma", "lgamma", "digamma",
    "zeta", "zeta_ui",
    "erf", "erfc",
    "ai", "li2",
    "jn", "yn", "j0", "j1", "y0", "y1",
    "hypot", "agm",
    "fma", "fms", "fmm", "fmma", "fmms",
    "beta", "bernoulli",
    "acosu", "asinu", "atanu", "atan2u",
    "cosu", "sinu", "tanu",
    "exp_2", "exp_2py",
})


_CONVERSION_STEM_RE = re.compile(
    r"^(get|set|init)_(d|si|ui|z|q|f|ld|sj|uj|ldouble|d_2exp|str|flt|float128|decimal64|decimal128)$"
)

_ARITHMETIC_STEM_RE = re.compile(
    r"^(add|sub|mul|div|cmp|neg|abs|sgn)(_[a-z0-9]+)?$"
)


# ---------------------------------------------------------------------------
# Data types
# ---------------------------------------------------------------------------


@dataclass
class FunctionDef:
    """A single ``mpfr_<name>`` or ``mpn_<name>`` function definition."""

    name: str
    deps: list[str] = field(default_factory=list)
    class_: str = "misc"
    topo_rank: int = -1
    defined_in: str = ""
    cycle: bool = False


# ---------------------------------------------------------------------------
# Source preprocessing
# ---------------------------------------------------------------------------


def strip_comments_and_strings(src: str) -> str:
    """Remove C comments and string/char literals from ``src``.

    Replacement preserves newlines (so line numbers stay aligned) and
    replaces literal contents with spaces. Mirrors the strip logic in
    ``eval/harness/ast_check.ts`` so the call-graph builder doesn't pick
    up ghost callees inside ``// mpfr_foo();`` comments or string
    literals like ``puts("mpfr_bar()")``.
    """

    out: list[str] = []
    i = 0
    n = len(src)
    while i < n:
        ch = src[i]
        nxt = src[i + 1] if i + 1 < n else ""

        # Block comment: /* ... */
        if ch == "/" and nxt == "*":
            j = src.find("*/", i + 2)
            if j == -1:
                # Unterminated comment — eat to EOF, preserve newlines.
                out.append("".join("\n" if c == "\n" else " " for c in src[i:]))
                break
            out.append("".join("\n" if c == "\n" else " " for c in src[i : j + 2]))
            i = j + 2
            continue

        # Line comment: // ... \n
        if ch == "/" and nxt == "/":
            j = src.find("\n", i + 2)
            if j == -1:
                out.append(" " * (n - i))
                break
            out.append(" " * (j - i))
            i = j
            continue

        # String literal: "..." with backslash escapes.
        if ch == '"':
            j = i + 1
            while j < n:
                if src[j] == "\\" and j + 1 < n:
                    j += 2
                    continue
                if src[j] == '"':
                    break
                j += 1
            # Replace contents (keep the opening/closing quotes as quotes
            # — they don't match either pattern). Newlines inside a
            # string get preserved.
            out.append('"')
            for c in src[i + 1 : j]:
                out.append("\n" if c == "\n" else " ")
            if j < n:
                out.append('"')
            i = j + 1
            continue

        # Char literal: '...' with backslash escapes. Same rules.
        if ch == "'":
            j = i + 1
            while j < n:
                if src[j] == "\\" and j + 1 < n:
                    j += 2
                    continue
                if src[j] == "'":
                    break
                j += 1
            out.append("'")
            for c in src[i + 1 : j]:
                out.append("\n" if c == "\n" else " ")
            if j < n:
                out.append("'")
            i = j + 1
            continue

        out.append(ch)
        i += 1

    return "".join(out)


# ---------------------------------------------------------------------------
# Function-body delimiter
# ---------------------------------------------------------------------------


def find_function_body(src: str, start_idx: int) -> tuple[int, int] | None:
    """Locate the body ``{ ... }`` of a function whose name starts at
    ``start_idx`` in ``src`` (already comment/string-stripped).

    Returns ``(open_brace_idx, close_brace_idx)`` for the matched braces,
    or ``None`` if no body is found (prototype, not a definition).

    The opening brace is the first ``{`` after the closing ``)`` of the
    signature's argument list. We find that ``)`` by scanning past the
    first ``(`` from ``start_idx`` and tracking paren depth. If the
    next non-whitespace character after the matched ``)`` is ``;``, this
    is a prototype, not a definition (return ``None``). Otherwise the
    body must begin with ``{``; we scan to the matching ``}``.
    """

    n = len(src)
    # Find the opening `(` of the signature.
    paren_open = src.find("(", start_idx)
    if paren_open == -1:
        return None
    # Scan to matching `)`.
    depth = 1
    j = paren_open + 1
    while j < n and depth > 0:
        c = src[j]
        if c == "(":
            depth += 1
        elif c == ")":
            depth -= 1
        j += 1
    if depth != 0:
        return None  # malformed
    # j now points just past the matching `)`. Skip whitespace; the next
    # non-whitespace char tells us what kind of construct this is:
    #
    #   * `{` — function definition (the common case).
    #   * alphabetic / underscore — K&R-style parameter declarations
    #     follow (rare but legal); we scan forward to the next `{`.
    #   * anything else (``;``, `)`, `,`, `+`, `&&`, `||`, …) — this is
    #     a call-site or a prototype, not a definition.
    while j < n and src[j].isspace():
        j += 1
    if j >= n:
        return None
    if src[j] == "{":
        open_brace = j
    elif src[j].isalpha() or src[j] == "_":
        # K&R declarations: scan forward to the first `{` we hit before
        # any obvious bail-out token. The MPFR codebase is ANSI/C99 so
        # we don't actually expect this path to fire on real upstream
        # files; included for defensive completeness.
        open_brace = src.find("{", j)
        if open_brace == -1:
            return None
    else:
        return None  # call-site, prototype, or other non-definition
    # Match the close brace.
    depth = 1
    k = open_brace + 1
    while k < n and depth > 0:
        c = src[k]
        if c == "{":
            depth += 1
        elif c == "}":
            depth -= 1
        k += 1
    if depth != 0:
        return None  # malformed
    return (open_brace, k - 1)


# ---------------------------------------------------------------------------
# Per-source extraction
# ---------------------------------------------------------------------------


def find_function_defs(source: str, defined_in: str) -> list[FunctionDef]:
    """Extract every ``mpfr_*``/``mpn_*`` function *definition* in
    ``source``. Returns one ``FunctionDef`` per definition, with ``deps``
    populated from the function body. ``defined_in`` is recorded
    verbatim (caller chooses absolute vs relative).

    ``source`` is expected to be the raw file contents; this function
    strips comments and strings internally.
    """

    cleaned = strip_comments_and_strings(source)
    defs: list[FunctionDef] = []

    for m in DEF_NAME_RE.finditer(cleaned):
        name = m.group("name")
        if not NAME_RE.match(name):
            continue
        body = find_function_body(cleaned, m.start())
        if body is None:
            continue  # prototype, not a definition
        open_b, close_b = body
        body_text = cleaned[open_b + 1 : close_b]
        deps = sorted(find_callees(body_text) - {name})
        defs.append(
            FunctionDef(
                name=name,
                deps=deps,
                class_="",  # filled in by classify() at walk time
                defined_in=defined_in,
            )
        )

    return defs


def find_callees(body: str) -> set[str]:
    """Return the set of ``mpfr_*``/``mpn_*`` callee names appearing as
    function calls in ``body``. ``body`` should already be comment- and
    string-stripped.
    """

    return {
        m.group("name")
        for m in CALLEE_RE.finditer(body)
        if NAME_RE.match(m.group("name"))
    }


# ---------------------------------------------------------------------------
# Classification
# ---------------------------------------------------------------------------


def classify(name: str, path: str) -> str:
    """Return the class bucket for a function named ``name`` defined in
    file ``path``.

    Priority order (first match wins):

      1. ``mpn_*`` -> ``substrate``.
      2. file stem matches a known transcendental -> ``transcendental``.
      3. file stem matches ``(get|set|init)_<type>`` -> ``conversion``.
      4. file stem matches ``(add|sub|mul|div|cmp|neg|abs|sgn)[_..]?`` -> ``arithmetic``.
      5. Default -> ``misc``.
    """

    if name.startswith("mpn_"):
        return "substrate"

    stem = Path(path).stem  # filename without `.c`

    if stem in _TRANSCENDENTAL_STEMS:
        return "transcendental"

    if _CONVERSION_STEM_RE.match(stem):
        return "conversion"

    if _ARITHMETIC_STEM_RE.match(stem):
        return "arithmetic"

    return "misc"


# ---------------------------------------------------------------------------
# Walker
# ---------------------------------------------------------------------------


def walk_mpfr_src(root: Path) -> dict[str, FunctionDef]:
    """Walk every ``.c`` file under ``root`` (non-recursive: just the
    top level, matching ``mpfr/src/*.c``) and build the full call
    graph. Returns a dict keyed by function name.

    If two files define the same function name (shouldn't happen in
    real MPFR — the build picks one — but synthetic test fixtures might),
    the second occurrence wins and we keep the last-seen ``defined_in``;
    the dep set is recomputed from that file's body.

    Side effect: every returned ``FunctionDef`` has its ``topo_rank``
    populated and ``cycle`` flag set per ``topo_sort``.
    """

    root = root.resolve()
    graph: dict[str, FunctionDef] = {}

    for c_path in sorted(root.glob("*.c")):
        try:
            text = c_path.read_text(encoding="utf-8", errors="replace")
        except OSError as e:  # pragma: no cover — defensive
            print(f"[callgraph] WARN: skip {c_path}: {e}", file=sys.stderr)
            continue

        # Record the path *relative to ``root``* — that's what the JSON
        # schema expects ("add.c" not "/abs/.../add.c"). Falls back to
        # the bare name when the resolution would be silly.
        try:
            rel = str(c_path.relative_to(root))
        except ValueError:  # pragma: no cover
            rel = c_path.name

        for fd in find_function_defs(text, rel):
            fd.class_ = classify(fd.name, rel)
            graph[fd.name] = fd

    # Now compute topological order across the union of all functions.
    order, cycle_nodes = topo_sort(graph)
    for idx, name in enumerate(order):
        graph[name].topo_rank = idx
    for name in cycle_nodes:
        graph[name].cycle = True

    return graph


# ---------------------------------------------------------------------------
# Topological sort
# ---------------------------------------------------------------------------


def topo_sort(graph: dict[str, FunctionDef]) -> tuple[list[str], set[str]]:
    """Topologically sort ``graph``. Returns ``(order, cycle_nodes)``:

      * ``order`` is a list of every function name; functions with no
        in-graph deps come first. Ties broken alphabetically for
        determinism (CLAUDE.md Rule 12 — idempotent outputs).
      * ``cycle_nodes`` is the set of names participating in any cycle
        (detected via iterative DFS).

    Cycle-participating nodes are still given a ``topo_rank``: we use
    Tarjan-style SCC decomposition, output the SCCs in reverse-finish
    order, and within each SCC sort alphabetically. Non-cycle nodes
    are emitted as single-element SCCs.

    The classic Kahn algorithm would refuse to emit cycle nodes; we
    instead use Tarjan + condensed-DAG-topo to keep the output
    well-defined.
    """

    # Restrict the dep edges to nodes present in the graph. Deps that
    # point outside the graph (e.g. a callee whose definition is not in
    # mpfr/src/ because it's a GMP function) are dropped for ordering
    # purposes — they don't constrain topo order. The JSON output still
    # records them as deps, just not as ordering-relevant edges.
    nodes = list(graph.keys())
    in_graph_deps = {
        name: [d for d in graph[name].deps if d in graph] for name in nodes
    }

    # Tarjan's SCC algorithm (iterative).
    index_counter = [0]
    stack: list[str] = []
    on_stack: set[str] = set()
    indices: dict[str, int] = {}
    lowlinks: dict[str, int] = {}
    sccs: list[list[str]] = []

    def strongconnect(start: str) -> None:
        # Iterative DFS using a work stack of (node, iterator).
        work: list[tuple[str, list[str]]] = [(start, list(in_graph_deps[start]))]
        indices[start] = index_counter[0]
        lowlinks[start] = index_counter[0]
        index_counter[0] += 1
        stack.append(start)
        on_stack.add(start)

        while work:
            v, succs = work[-1]
            if succs:
                w = succs.pop()
                if w not in indices:
                    indices[w] = index_counter[0]
                    lowlinks[w] = index_counter[0]
                    index_counter[0] += 1
                    stack.append(w)
                    on_stack.add(w)
                    work.append((w, list(in_graph_deps[w])))
                elif w in on_stack:
                    lowlinks[v] = min(lowlinks[v], indices[w])
            else:
                # All successors processed.
                if lowlinks[v] == indices[v]:
                    scc: list[str] = []
                    while True:
                        w = stack.pop()
                        on_stack.discard(w)
                        scc.append(w)
                        if w == v:
                            break
                    sccs.append(scc)
                work.pop()
                if work:
                    parent = work[-1][0]
                    lowlinks[parent] = min(lowlinks[parent], lowlinks[v])

    # Iterate in sorted order for determinism.
    for n in sorted(nodes):
        if n not in indices:
            strongconnect(n)

    # Tarjan emits SCCs in reverse topological order of the condensation,
    # i.e. *sinks* (leaves of the dep graph) first. That is exactly the
    # porting order we want: a function should come AFTER all the things
    # it depends on. So we keep Tarjan's natural order — leaves get
    # ``topo_rank=0``, top-level callers get the largest ranks.

    # Within each SCC, sort alphabetically for determinism.
    order: list[str] = []
    cycle_nodes: set[str] = set()
    for scc in sccs:
        scc.sort()
        if len(scc) > 1:
            cycle_nodes.update(scc)
        else:
            # An SCC of size 1 only counts as a cycle if the node has a
            # self-loop. We've filtered self-edges out at extraction
            # time, but be defensive.
            (only,) = scc
            if only in in_graph_deps[only]:
                cycle_nodes.add(only)
        order.extend(scc)

    return order, cycle_nodes


# ---------------------------------------------------------------------------
# JSON emission
# ---------------------------------------------------------------------------


def emit_json(graph: dict[str, FunctionDef], *, src_root: Path) -> str:
    """Serialise ``graph`` to a JSON string per the documented schema.

    Output keys are sorted; dep lists are sorted; the document is
    indented for human review. Idempotent except for ``generated_at``.
    """

    functions_out: dict[str, dict[str, object]] = {}
    for name in sorted(graph.keys()):
        fd = graph[name]
        entry: dict[str, object] = {
            "deps": sorted(fd.deps),
            "class": fd.class_,
            "topo_rank": fd.topo_rank,
            "defined_in": fd.defined_in,
        }
        if fd.cycle:
            entry["cycle"] = True
        functions_out[name] = entry

    doc = {
        "generated_at": datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ"),
        "mpfr_source_root": str(src_root),
        "functions": functions_out,
    }
    return json.dumps(doc, indent=2, sort_keys=True) + "\n"


# ---------------------------------------------------------------------------
# CLI
# ---------------------------------------------------------------------------


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(
        description="Build the mpfr_*/mpn_* call graph from mpfr/src/*.c.",
    )
    parser.add_argument(
        "--src",
        type=Path,
        default=Path("mpfr/src"),
        help="Directory containing MPFR .c sources (default: mpfr/src).",
    )
    parser.add_argument(
        "--output",
        type=Path,
        default=Path("eval/driver/callgraph.json"),
        help="JSON output path (default: eval/driver/callgraph.json).",
    )
    args = parser.parse_args(argv)

    src_root: Path = args.src
    if not src_root.is_dir():
        print(f"error: {src_root} is not a directory", file=sys.stderr)
        return 2

    graph = walk_mpfr_src(src_root)
    payload = emit_json(graph, src_root=src_root)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(payload, encoding="utf-8")

    n_cycle = sum(1 for fd in graph.values() if fd.cycle)
    print(
        f"[callgraph] wrote {args.output} "
        f"({len(graph)} functions, {n_cycle} in cycles)",
        file=sys.stderr,
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
