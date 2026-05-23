"""Prompt builder for the mpfr-ts ralph loop.

This module produces the single-string prompt handed to a sonnet L3 agent
when porting one MPFR (or GMP-substrate) function to pure TypeScript. The
prompt is fully self-contained: it embeds the Laws, the locked schema
from ``src/core.ts`` (Law 4 enforcement — the agent must not redeclare
schema types), the function-specific spec, the corresponding C source if
available, the hallucination-risk callouts from ``CLAUDE.md``, the exact
runner command, and a worked example (``mpn_add_n`` reference port).

The module is import-pure: no global state, no I/O on import. The public
entry point is :func:`build_prompt`.

References
----------
- CLAUDE.md §"The Laws" (especially Law 4 — library coherence).
- CLAUDE.md §"Hallucination-risk callouts".
- docs/PILOT_PLAN.md row 9 — Pilot Step 9 acceptance criteria.
"""

from __future__ import annotations

import json
from pathlib import Path


# --------------------------------------------------------------------------
# Path discovery
# --------------------------------------------------------------------------


def _default_repo_root() -> Path:
    """Repo root inferred from this file's location.

    ``eval/driver/prompts.py`` → ``..`` is the repo root. We avoid using
    the current working directory so the module behaves identically
    whether invoked from the repo root or any subdirectory.
    """
    return Path(__file__).resolve().parents[2]


# --------------------------------------------------------------------------
# Section renderers
# --------------------------------------------------------------------------


_IDENTITY = """\
# Identity and role

You are porting a single function from GNU MPFR (or GMP, for substrate
helpers under `src/internal/mpn/`) to pure TypeScript. Your output is a
single `.ts` file at the path given in the iteration loop below.

The library is **mpfr-ts** — an auto-ported, pure-TypeScript
reimplementation of MPFR. The published artifact has zero npm
dependencies and runs on both Bun (>= 1.3) and Node (>= 22). Your port
must therefore use **pure ESM + native BigInt only** — no `Bun.*` calls,
no `node:*` imports, no third-party packages.

The grader is brutal. Only a port that scores
`composite_correctness >= 0.95` against a libmpfr-derived golden master
is accepted; everything else is a Pilot halt. The composite is the only
acceptable signal of correctness — your own reasoning about whether the
port "looks right" is not.

Two ports of MPFR coexist in this repo:

1. **Substrate** (`src/internal/`): faithful mirrors of GMP `mpn_*` and
   MPFR helpers. Substrate ports operate on raw `bigint[]` limb arrays
   and DO NOT import from `src/core.ts`.
2. **Idiomatic surface** (`src/`): immutable TS API that returns
   `{ value, ternary }` per the locked schema below. Surface ports MUST
   import from `src/core.ts` and MUST NOT redeclare schema types.

Which one you are writing is given by the `class` field in the
function-specific contract section.
"""


_LAWS = """\
# The Laws (verbatim from CLAUDE.md §"The Laws")

These are unconditional. If a "fast path" conflicts with any, choose the
Law.

**Law 1 — Ground truth before code.** Every porting decision cites a
local source — the C function in `mpfr/src/<module>/<fn>.c`, a section
of the MPFR manual (`mpfr/doc/mpfr.texi`), the GMP `mpn` documentation,
an entry in `../auto-port-eval/HANDOFF.md`, or a test triple mined from
`mpfr/tests/<file>.c`. If the source isn't local, acquire it before
writing the code that depends on it. Cite in code comments and commit
messages in the form:

```ts
// Ref: mpfr/src/add1sp.c L142–L160 — exponent equality fast path.
//   The early return guards against the catastrophic-cancellation case
//   where p1.exp == p2.exp and signs differ; we re-enter the general
//   add1 path only after subtracting top words.
```

**Law 2 — The harness is the truth, not the agent.** A port is "done"
when `eval/state.db` records `composite_correctness >= 0.95` against a
golden master that includes happy + edge + adversarial + fuzz + mined
test cases. An agent that *claims* a port works but never ran the
grader, or ran the grader against a stub golden, has produced
nothing. The composite grade is the only acceptable signal of
correctness. Reviewer prose is not.

**Law 3 — Faithful substrate, idiomatic surface.** `src/internal/`
mirrors GMP/MPFR algorithms byte-for-byte at the I/O contract level
(faithful) — every divergence from C output is debuggable line-by-line
because every helper has a C analogue with the same signature.
`src/` is idiomatic immutable TS — `add(a, b, prec, rnd) -> {value,
ternary}`, no mutation, no `mpfr_t` rop-handles in user space. The
split is load-bearing; do not blur it.

**Law 4 — The library composes.** The end-state is a *usable*,
*coherent* TypeScript MPFR — not 600 isolated functions that each pass
a golden but speak mutually-incompatible types. Every port imports
the locked schema from `src/core.ts` (`MPFR` value type,
`RoundingMode` enum, `MPFRError` class, `{value, ternary}` result
shape). The grader rejects any port that redeclares these or returns
a structurally-incompatible value. A port that passes its own golden
but breaks composition with already-shipped ports is a regression,
not a feature.
"""


_SCHEMA_PREFACE = """\
# The locked schema

The following is `src/core.ts`. Import the types you need from this
file. **DO NOT redeclare** `MPFR`, `RoundingMode`, `Result`, `Ternary`,
`MPFRError`, `MPFRKind`, or `Sign`. The grader's AST check rejects
schema redeclarations with `composite=0` and `error="schema-violation"`.

For substrate ports (`class === 'substrate'` in spec.json), the import
requirement is **waived** — substrate operates on raw `bigint[]` limb
arrays and has no need for the MPFR value type. You must still not
redeclare the schema types, even spuriously.
"""


_HALLUCINATION_CALLOUTS = """\
# Hallucination-risk callouts (verbatim from CLAUDE.md)

Sharp pre-emptive warnings about MPFR-porting mistakes that look right
but aren't. When you catch yourself about to do one, stop and re-check
the cited reference.

- **NaN != NaN.** IEEE 754 says NaN is unequal to itself. MPFR follows
  the rule. A direct `a === b` comparison in the grader will reject
  every NaN-output golden. The runner needs a `specialCorrectness`
  branch that returns true when both are NaN. Same applies to
  function output: `cmp(NaN, x)` returns 0 in C only if MPFR's
  `erange` flag is set; the idiomatic TS port should throw a
  documented `MPFRRangeError`, never silently return 0.

- **Signed zero is real.** `+0` and `-0` are distinct MPFR values
  and the C tests check this. The TS `MPFR` value type must carry
  sign even when kind is `'zero'`. `add(+0, -0, rnd=RNDN) -> +0`;
  `add(+0, -0, rnd=RNDD) -> -0`. Get this wrong and ~30% of edge
  goldens fail silently in a way that looks like "rounding off-by-1".

- **Ternary flag is the sign of (rounded - exact), not 0/1.**
  Returns from MPFR ops are `-1` (rounded down from exact), `0`
  (exact), or `+1` (rounded up). The TS port must compute this in
  the same direction the C reference does, in the same rounding
  mode, *exactly*. Off-by-direction (returning the sign of `exact -
  rounded`) is the single most common subtle bug. The C source is
  authoritative — read it; do not infer.

- **Rounding mode count is FIVE.** `MPFR_RNDN` (nearest, ties to
  even), `MPFR_RNDZ` (toward zero), `MPFR_RNDU` (toward +inf),
  `MPFR_RNDD` (toward -inf), `MPFR_RNDA` (away from zero). Earlier
  MPFR versions had `MPFR_RNDNA` (nearest, ties away) — gone in
  current MPFR. Agents trained on older docs sometimes emit
  6-element rounding enums; correct to 5.

- **`mpfr_prec_t` is in bits, not decimal digits.** A common
  hallucination is treating `prec` as digit count. `prec=53` means
  53 bits of mantissa (~16 decimal digits), not 53 digits. Every
  spec.json must include `prec_unit: "bits"` to defuse this.

- **GMP mpn limbs are LITTLE-ENDIAN by limb index.** `limbs[0]` is
  the least-significant 64-bit word. Iteration order in `mpn_add_n`
  is `i = 0 -> n-1` with carry propagation rightward (in limb-index
  terms). Agents intuit big-endian from "first array element = most
  significant" and produce reversed limb order — every multi-limb
  op then fails. Cite GMP §8.3 in the substrate prompts.

- **`umul_ppmm` C macro output args are first.** Inherited scar
  from the FLINT eval: `umul_ppmm(hi, lo, a, b)` writes `a*b` into
  `(hi, lo)`. The first two arguments are *outputs*. Haiku misread
  this in n_flog and wrote `p*p*b`. MPFR uses `umul_ppmm`
  extensively. Prompts must call out the macro convention.

- **`mpfr_init2(rop, prec)` allocates; `mpfr_set_prec(rop, prec)`
  re-allocates if needed.** Both take prec in bits. Confusing them
  is benign in C (both work) but in the idiomatic TS port they
  fold into a single `init(prec)` factory; the agent should not
  emit two redundant constructors.

- **Correct rounding does not mean unique output.** It means: the
  output equals the unique closest representable value in the given
  rounding mode. So MPFR outputs are uniquely determined — the
  `specialCorrectness` "accept either root" branch from
  `n_sqrtmod` in auto-port-eval does NOT generalize to MPFR. The
  only places multi-valued correctness applies are NaN and signed
  zero (above). Do not over-loosen the equality check.

- **Subnormals exist in MPFR only when `mpfr_set_emin` is called.**
  By default MPFR has no subnormal range (exponent is symmetric and
  large). Tests that exercise subnormals must explicitly set emin
  / emax via the C driver, and the TS port must support the same
  range API. Don't assume IEEE 754 subnormal semantics.

- **Cyrillic homoglyphs are a real failure mode.** The auto-port-eval
  predecessor lost one full L1 grade because an agent emitted
  `0xaaaaaaaaaaaaaaaa` with a Cyrillic 'a' (U+0430). Write only ASCII
  identifiers and ASCII numeric literals.
"""


# --------------------------------------------------------------------------
# Function-specific sections
# --------------------------------------------------------------------------


def _render_signature(signature: object, fn_name: str) -> str:
    """Render the spec.json ``signature`` object as a TypeScript signature.

    The spec format is an object with ``params`` (list of parameter names)
    and ``returns`` (the return type, possibly with embedded TS syntax).
    Param types are not in spec.json — they're inferred from class context
    and the C signature — so we render the param list as plain names.

    A string ``signature`` is passed through verbatim (some specs may
    eventually store the full TS signature as a single string).
    """
    if isinstance(signature, str):
        return signature
    if isinstance(signature, dict):
        params = signature.get("params", [])
        returns = signature.get("returns", "unknown")
        if not isinstance(params, list):
            raise ValueError(f"spec signature.params must be a list, got {type(params).__name__}")
        param_str = ", ".join(str(p) for p in params)
        return f"{fn_name}({param_str}) -> {returns}"
    raise ValueError(f"spec signature must be string or object, got {type(signature).__name__}")


def _find_c_source(fn_name: str, port_class: str, repo_root: Path) -> tuple[str, str]:
    """Locate the C source for ``fn_name``.

    Returns ``(citation, body)``. ``citation`` is a short string naming
    where the source came from; ``body`` is the source text (or a
    placeholder note for substrate functions whose source isn't in-tree).
    """
    if port_class == "substrate":
        # GMP substrate functions are not bundled in mpfr-ts. Point the
        # agent at the GMP manual section and any in-repo reference port.
        return (
            "GMP manual §8.3 (Low-level Functions)",
            "// C source not bundled in-tree for substrate functions.\n"
            "// Refer to the GMP manual §8.3 'Low-level Functions' for the\n"
            "// algorithmic contract, and to the worked-example section\n"
            "// below for the reference TypeScript port shape.\n",
        )

    candidate = repo_root / "mpfr" / "src" / f"{fn_name}.c"
    if candidate.exists():
        return (f"mpfr/src/{fn_name}.c", candidate.read_text(encoding="utf-8"))

    stripped = fn_name[len("mpfr_") :] if fn_name.startswith("mpfr_") else fn_name
    candidate = repo_root / "mpfr" / "src" / f"{stripped}.c"
    if candidate.exists():
        return (f"mpfr/src/{stripped}.c", candidate.read_text(encoding="utf-8"))

    return (
        "C source not located",
        f"// C source not located for {fn_name}; check mpfr/src/ for the\n"
        f"// canonical implementation before writing the port. The MPFR\n"
        f"// manual chapter on the relevant operation is also authoritative.\n",
    )


def _render_function_contract(
    fn_name: str,
    spec: dict[str, object],
    repo_root: Path,
) -> str:
    """Render the function-specific contract section.

    Includes the function name, class, TypeScript signature, the
    C signature (if present in the spec), supplementary notes from
    ``doc``/``refs``, and the C source body when found in-tree.
    """
    port_class = str(spec.get("class", "arithmetic"))
    ts_sig = _render_signature(spec.get("signature", {}), fn_name)
    c_sig = spec.get("c_signature")
    doc = spec.get("doc")
    refs = spec.get("refs")

    citation, c_body = _find_c_source(fn_name, port_class, repo_root)

    out: list[str] = []
    out.append("# Function-specific contract")
    out.append("")
    out.append(f"- **Function**: `{fn_name}`")
    out.append(f"- **Class**: `{port_class}`")
    out.append(f"- **TypeScript signature**: `{ts_sig}`")
    if isinstance(c_sig, str):
        out.append(f"- **C signature**: `{c_sig}`")
    if isinstance(doc, str):
        out.append("")
        out.append("**Notes from spec.json**:")
        out.append("")
        out.append(doc)
    if isinstance(refs, list) and refs:
        out.append("")
        out.append("**Spec references**:")
        for ref in refs:
            out.append(f"- {ref}")

    out.append("")
    out.append(f"**C source ({citation})**:")
    out.append("")
    out.append("```c")
    # Avoid embedding stray triple-backticks from the source by escaping
    # them; in practice MPFR sources contain none, but be defensive.
    out.append(c_body.replace("```", "``​`"))
    out.append("```")
    return "\n".join(out)


# --------------------------------------------------------------------------
# Iteration loop and worked example
# --------------------------------------------------------------------------


def _render_iteration_loop(
    fn_name: str,
    port_class: str,
    repo_root: Path,
) -> str:
    """Render the iteration-loop instructions.

    Specifies the exact paths the agent must write to, the runner
    invocation, and the stop conditions. Paths are absolute — agents
    routinely drift cwd, so relative paths are a footgun.
    """
    runner_path = repo_root / "eval" / "harness" / "runner.ts"
    golden_path = repo_root / "eval" / "functions" / fn_name / "golden.jsonl"
    core_path = repo_root / "src" / "core.ts"
    port_path = f"/tmp/eval_{fn_name}/port.ts"
    grade_path = f"/tmp/eval_{fn_name}/grade.json"

    # Public (non-substrate) ports must import from src/core.ts. Because the
    # port lives under /tmp/ during grading, the relative path "../core.ts"
    # resolves to /tmp/core.ts (which does not exist). Use the absolute path
    # below in your port's import statement. When the port is promoted to its
    # canonical location (src/ops/...), the orchestrator rewrites this to a
    # relative path.
    import_guidance = (
        ""
        if port_class == "substrate"
        else f"""\

   **Import path**: your port runs from `/tmp/eval_{fn_name}/port.ts`, so a
   relative `../core.ts` import will fail (`/tmp/core.ts` does not exist).
   Use the absolute path:

   ```ts
   import type {{ MPFR /*, RoundingMode, Result, Ternary*/ }} from "{core_path}";
   import {{ /* posZero, MPFRError, PREC_MIN, ... as needed */ }} from "{core_path}";
   ```

   Two separate `import` statements: one `import type {{ ... }}` for type-only
   symbols, one `import {{ ... }}` for runtime values. The ast_check gate
   currently flags `import {{ type MPFR }}` mixed syntax as a false-positive
   redeclaration (bd `mpfr-ts-wli`). The orchestrator will rewrite paths to
   relative when promoting your port to `src/ops/`.
"""
    )

    return f"""\
# Iteration loop

You drive a fixed-budget iteration loop. At each step you write the
port, run the grader, read the JSON output, and decide whether to stop
or refine.

1. Create the working directory and write your port to:

   ```
   {port_path}
   ```
{import_guidance}

2. Run the grader:

   ```
   bun {runner_path} \\
     --function {fn_name} \\
     --port {port_path} \\
     --golden {golden_path} \\
     --output {grade_path} \\
     --class {port_class}
   ```

3. Read `{grade_path}` as JSON. The field
   `composite_correctness` is in `[0.0, 1.0]`. If it is
   `>= 0.95`, you are done — stop and report the final score.

4. Otherwise: read `first_error` (and inspect the per-case detail in
   the JSON), identify the root cause, fix the port, and re-grade.

5. Hard cap of **6 iterations**. If you have not reached `>= 0.95`
   after six grading runs, stop and return your best port even
   though it has not passed.

Hard rules:

- **Do not invent test cases.** The golden master is the contract;
  inventing your own cases hides regressions.
- **Do not modify the goldens.** `{golden_path}` and every other
  file under `eval/functions/{fn_name}/` is read-only for you.
- **Do not modify the harness.** `{runner_path}` and the files it
  imports are read-only for you. If the harness rejects your port
  with `schema-violation`, fix the port (don't disable the check).
- **Cite the C source** in code comments using the `Ref:` form
  documented in Law 1.
- **Pure ESM + native BigInt only.** No `Bun.*`, no `node:*`, no
  npm packages.
"""


def _render_worked_example(repo_root: Path) -> str:
    """Embed the ``mpn_add_n`` reference port as a worked example."""
    ref = repo_root / "src" / "internal" / "mpn" / "add_n.ts"
    body = ref.read_text(encoding="utf-8") if ref.exists() else (
        "// reference port not found — see src/internal/mpn/add_n.ts in the repo.\n"
    )
    return f"""\
# Worked example — `mpn_add_n` reference port

Here is the reference port of `mpn_add_n`, a worked example of what
"good" looks like for a **substrate** function. Note: top-of-file
docstring with C-source citation, named constants for limb widths,
narrow typing (`readonly bigint[]`), a single allocation, an immutable
return shape, and a structural-precondition guard that throws rather
than producing silent wrong output.

The same shape applies to `mpfr_*` **surface** functions — substitute
`Result` (from `src/core.ts`) for the substrate return type, import the
schema types instead of declaring a local interface, and route errors
through `MPFRError` rather than `Error`.

```ts
{body}```
"""


_OUTPUT_CONTRACT = """\
# Output contract

Write your final port to the path given in the iteration loop section
(an absolute path under `/tmp/`). Your reply text should NOT contain
the port itself — the runner reads the port from disk. Instead, your
final reply should contain:

1. The final `composite_correctness` from `grade.json`.
2. A one-sentence summary of any unresolved failures (or "all
   passing" if `>= 0.95`).
3. The number of iterations you used (1..6).

If you cannot reach `>= 0.95` in 6 iterations, stop anyway and
return the best port plus the score — the orchestrator will decide
whether to escalate or park.
"""


# --------------------------------------------------------------------------
# Public entry point
# --------------------------------------------------------------------------


def build_prompt(function_name: str, *, repo_root: Path | None = None) -> str:
    """Build the full prompt for porting ``function_name``.

    Parameters
    ----------
    function_name:
        Stable C name of the function — e.g. ``"mpn_add_n"``,
        ``"mpfr_add"``. Must match a directory under
        ``eval/functions/`` containing a ``spec.json``.
    repo_root:
        Repo root override. Defaults to the directory two levels above
        this file (``eval/driver/prompts.py`` -> repo root). Pass an
        explicit path to render prompts against a worktree or test
        fixture without changing cwd.

    Returns
    -------
    str
        The full prompt as a single string. Sections are separated by
        blank lines; section headers are Markdown ``#`` headings so the
        agent's renderer (Markdown-aware) groups them visually.

    Raises
    ------
    FileNotFoundError
        If ``spec.json`` or ``src/core.ts`` is missing.
    ValueError
        If the spec is structurally malformed (e.g. ``signature`` is
        the wrong shape).
    """
    root = repo_root if repo_root is not None else _default_repo_root()

    spec_path = root / "eval" / "functions" / function_name / "spec.json"
    if not spec_path.exists():
        raise FileNotFoundError(f"spec.json not found at {spec_path}")
    spec_text = spec_path.read_text(encoding="utf-8")
    spec = json.loads(spec_text)
    if not isinstance(spec, dict):
        raise ValueError(f"spec.json at {spec_path} must be a JSON object")

    core_path = root / "src" / "core.ts"
    if not core_path.exists():
        raise FileNotFoundError(f"src/core.ts not found at {core_path}")
    core_body = core_path.read_text(encoding="utf-8")

    port_class = str(spec.get("class", "arithmetic"))

    sections: list[str] = [
        _IDENTITY,
        _LAWS,
        _SCHEMA_PREFACE,
        "```ts\n" + core_body + "```",
        _render_function_contract(function_name, spec, root),
        _HALLUCINATION_CALLOUTS,
        _render_iteration_loop(function_name, port_class, root),
        _render_worked_example(root),
        _OUTPUT_CONTRACT,
    ]
    # Single blank line between sections; trailing newline at end.
    return "\n\n".join(s.rstrip() for s in sections) + "\n"
