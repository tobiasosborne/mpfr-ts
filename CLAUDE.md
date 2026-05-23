# CLAUDE.md — mpfr-ts

If you are an agent (Claude Code, an SDK harness, a downstream tool)
landing in this repo, read this **top to bottom every session**. After a
context compression, re-read. The rules drift out of working memory
faster than you think; that's why they're numbered.

---

## What this is

**mpfr-ts is an auto-ported pure-TypeScript reimplementation of [GNU
MPFR](https://www.mpfr.org/), produced by a ralph loop driving sonnet L3
across the library root-to-leaves of the call graph.** Two systems
coexist in this repo:

1. **The port itself** — `src/` (idiomatic immutable TS API) and
   `src/internal/` (faithful substrate that mirrors GMP `mpn_*` and
   MPFR helpers).
2. **The eval harness that produced the port** — `eval/` (per-function
   `golden_driver.c`, mined `mpfr/tests/*.c` cases, worker-isolated
   `runner.ts`, SQLite state DB, ralph orchestration scripts).

The artifact is *both*. The published library is `src/` + `package.json`;
the eval is what makes it trustworthy.

This is a **warmup** project. The protocol developed here is meant to
scale to FLINT next, then larger C/C++ libraries. Decisions that look
over-engineered for "just MPFR" are intentional — they're the
reusable substrate of the next eval.

## Read order

For any task, in this order:

1. This file (`CLAUDE.md`).
2. `PHASE.md` — one line, current phase (Pilot | Production | Optimize).
3. `../auto-port-eval/RESULTS.md` and `../auto-port-eval/HANDOFF.md` —
   the predecessor project's hard-won lessons. Especially HANDOFF
   §"Things I learned the hard way".
4. The relevant `eval/functions/<fn>/spec.json` for the function you
   are touching.
5. The corresponding C source under `mpfr/src/` for the function
   (clone fresh if absent — see "Practical guidance").

If you have not read this file and `PHASE.md`, you must refuse to add
code. (Gate stated by named files, not ordinals, to prevent
count-drift.)

## Project phase awareness

The project is a **three-phase plan**, and the rules differ across
phases. **Know which phase you are in before doing anything.**

- **Pilot.** First 10 functions (`mpn_add_n`, `mpn_sub_n`, `mpn_cmp`,
  `mpn_lshift`, `mpfr_init2`, `mpfr_set_d`, `mpfr_get_d`, `mpfr_cmp`,
  `mpfr_add`, `mpfr_mul`). Ralph loop runs in **halt-on-failure
  mode** — every failure stops the loop for human review. The
  deliverable is *the harness, proven sound*, not the 10 ports
  themselves. A working pilot with a flaky harness is a Pilot
  failure; a clean harness with 10 verified ports is a Pilot success.
- **Production.** Remaining ~590 functions, ralph loop runs in
  **auto-escalate mode** (sonnet L3 → opus L3 → park). The
  deliverable is the full port + a `parked.md` describing what
  needs human attention.
- **Optimize.** Re-attempt every function tagged `correct-but-slow`
  in the state DB, with prompts that focus the agent on perf. The
  deliverable is moving the perf-grade distribution rightward without
  regressing correctness.

The current phase is recorded in `PHASE.md`. At session start,
`cat PHASE.md` and abort if your task does not match.

---

## The Laws

Three laws. Unconditional. If a "fast path" conflicts with any, choose
the Law.

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
`src/` is idiomatic immutable TS — `add(a, b, prec, rnd) → {value,
ternary}`, no mutation, no `mpfr_t` rop-handles in user space. The
split is load-bearing; do not blur it. (See
`~/.claude/projects/-home-tobiasosborne-Projects-mpfr-ts/memory/decision_api_shape.md`
and `decision_substrate.md`.)

**Law 4 — The library composes.** The end-state is a *usable*,
*coherent* TypeScript MPFR — not 600 isolated functions that each pass
a golden but speak mutually-incompatible types. Every port imports
the locked schema from `src/core.ts` (`MPFR` value type,
`RoundingMode` enum, `MPFRError` class, `{value, ternary}` result
shape). The grader rejects any port that redeclares these or returns
a structurally-incompatible value. A cross-function integration
suite under `eval/integration/` exercises chains
(`set_d(3.14) → mul(x, y, prec, rnd) → add(...) → get_d(...)`) and
is part of the Production exit criteria. **A port that passes its
own golden but breaks composition with already-shipped ports is a
regression, not a feature.** See "Library coherence" below.

---

## The Rules

Numbered, non-negotiable. Re-read after compaction.

0. **Laws 1, 2, 3 apply.** Always.

1. **Fail fast, fail loud.** Throw on invariant violations
   (`prec < 1`, mantissa MSB not set in a normal number, `kind ===
   'nan'` reaching an arithmetic op without the NaN-propagation
   guard). Crashes with context beat silently-wrong ternary flags or
   silently-broken rounding. The grader cannot diagnose what the port
   hid.

2. **All bugs are deep.** No bandaids, no "looks right for this test."
   A wrong ternary flag in `mpfr_add` that "passes" because the golden
   case happens to have ternary=0 is exactly the kind of bug the
   brutal harness exists to surface. Investigate root causes. A fix
   that mutates one branch to make one case green often breaks a
   rounding invariant elsewhere — verify by re-running the *full*
   golden set, not just the failing case.

3. **Skepticism.** Verify subagent output, previous-session claims,
   and your own memory against the current state of the repo and the
   state DB. `eval/state.db` is authoritative; conversation context
   and agent self-reports are not. Especially: be skeptical of
   "composite=1.0" reports from L3 agents — re-run `node
   eval/harness/runner.ts --function X --port src/... --golden ...
   --output /tmp/verify.json` yourself before trusting.

4. **The harness gets per-test worker isolation from day one.** Each
   test case runs in a Bun `Worker` (`new Worker(new URL("./worker.ts",
   import.meta.url))`) with a class-tier hard wall (50ms / 200ms /
   1s). On timeout, `worker.terminate()`. This is non-negotiable
   because transcendental ports *will* infinite-loop, and the
   auto-port-eval predecessor's "no per-test timeout" scar is the
   single largest known failure mode (see
   `../auto-port-eval/HANDOFF.md` §2). Do not regress to
   `subprocess.run(timeout=...)` or single-process `import('./port.ts')`.

5. **Two TDD shapes — both valid.**
   - **Spec-from-scratch:** classic RED → GREEN → refactor. For new
     harness components, new helpers, new analysis scripts.
   - **Port-and-verify:** port the C algorithm faithfully, capture
     invariants in the golden master, **mutation-prove** the goldens
     catch regressions (perturb the port, confirm the composite
     drops, restore), cross-validate against the live `libmpfr`
     output. Mutation-proving replaces literal "RED first" because
     the golden is generated *from libmpfr* — the port can't fail
     before it exists.

   The discipline is "the golden has caught a real regression," not
   "the golden was committed before the port."

6. **Tiered workflow.** Scale agent effort to change size:
   - **Trivial** (<5 LOC; typo / comment / a missed `BigInt` cast):
     direct edit, no subagents.
   - **Small** (one function, <30 LOC; e.g. fixing one ported helper):
     direct edit; one `Explore` subagent if MPFR/GMP source surface
     is unfamiliar; re-run the relevant golden.
   - **Core** (new harness component, new ralph-loop policy, new
     spec.json schema field, cross-function refactor): TaskCreate
     checklist; for contested design choices, spawn 2–3 research
     subagents independently before implementing (the 3+1 pattern).

7. **"Composite ≥ 0.95 on a sparse golden" is not a passing test.**
   Every golden master must include all five tag classes (happy,
   edge, adversarial, fuzz, mined) with the minimum counts:
   `happy >= 20`, `edge >= 30`, `adversarial >= 10`, `fuzz >= 50`,
   `mined >= 5` (or all available from `mpfr/tests/<fn>.c`, if fewer
   than 5 exist). A golden with only happy cases is broken and
   blocks the function. The grader rejects such goldens with
   `error: "insufficient golden coverage"`.

8. **Sonnet L3 is the default porter.** Opus L3 is reserved for
   auto-escalation after a sonnet L3 failure. Do not manually invoke
   opus for "I have a feeling this one's hard" — the evidence
   (`../auto-port-eval/RESULTS.md`) shows opus is Pareto-dominated 6×
   for routine ports. Trust the policy.

9. **State DB is the only persistent tracker.** `eval/state.db`
   (SQLite) records function status, run history, per-case detail,
   and parked-function diagnostics. No `TodoWrite`, no markdown TODO
   lists for cross-session work. `TaskCreate` is permitted for
   **in-session** sub-step tracking only (the harness-visible
   checklist of "what I'm doing right now in this conversation").
   If the work would be filed as an issue in another repo's beads,
   it belongs in `eval/state.db`; if it's a step *inside* a session,
   it can live in `TaskCreate`.

10. **Literate programming for the substrate, sparse comments for
    ports.** `src/internal/` files are exposition: top-of-file
    docstring explains which GMP/MPFR algorithm is being mirrored,
    which paper or manual section the algorithm comes from, what the
    I/O contract is, what invariants hold on entry and exit. Public
    `src/` files are tighter: signature, one paragraph of intent,
    then the code. Both must cite the C source they mirror (Law 1).

11. **No GitHub CI, no automated remote runs.** Quality gates run
    locally: `node eval/harness/runner.ts --function X --port ...`,
    full-suite `python3 eval/driver/grade_all.py`, the SQLite
    dashboard query. The user has explicitly rejected automated CI
    across all their projects; failure-email noise is worse than
    zero signal. Do NOT create `.github/workflows/`.

12. **No npm dependencies in the port; port runs on Bun OR Node.**
    `src/` and `src/internal/` use **pure ESM + native `BigInt`
    only** — no `Bun.*` calls, no `node:*` imports, no third-party
    packages. The runtime distinction is invisible to library
    consumers, who may install `mpfr-ts` on either Bun or Node and
    have it work identically. The harness under `eval/` is
    Bun-native (uses `Bun.spawn`, `Bun.file`, Bun `Worker`, `bun
    test`); Python driver scripts (`ralph.py`, `dashboard.py`)
    don't care which TS runtime the harness uses. The published
    `package.json` lists zero runtime deps and declares engine
    compatibility for both Bun ≥1.3 and Node ≥22.

13. **Cyrillic homoglyph check on every agent-generated file.** The
    auto-port-eval predecessor lost one full L1 grade because haiku
    emitted `0xaaaaaaaaaaaaaaaаn` with a Cyrillic 'а' (U+0430). The
    `finalize.py` equivalent in `eval/driver/` MUST grep
    agent-emitted files for non-ASCII and reject before grading.
    Composite=0 from a Cyrillic typo is a wasted run.

14. **Phase changes go through `PHASE.md` + a worklog entry.** Do
    not silently flip from Pilot to Production by changing the ralph
    loop's failure policy. Update `PHASE.md`, write a short
    `docs/worklog/NNN-phase-transition.md` describing what the
    Pilot proved and what auto-escalate caveats remain, then change
    the policy. Phase transitions are commitments.

15. **Repeat rules.** Re-read this file at session start, after
    `/clear`, after any context compression. The agent that re-reads
    catches drift; the agent that doesn't ships it.

---

## Pilot gating

These rules apply *only during the Pilot phase* and override nothing
in §1–§15; they are an additional layer.

PIL.1. **Halt-on-failure.** The ralph loop stops on any port with
`composite_correctness < 0.95`. Manual investigation before resume.
This is the trade-off for Pilot: throughput is irrelevant, harness
trust is everything.

PIL.2. **Hand-port the first function as a reference (`mpn_add_n`).**
Before the first ralph-loop tick, write `src/internal/mpn/add_n.ts`
by hand. This serves as (a) red-green sanity for the harness
(reference port should score 1.0; a deliberately-broken copy in
`eval/reference_ports/broken/` should score < 0.5), and (b) the
worked example agents read in the prompt.

PIL.3. **Mutation-prove the golden for every pilot function.**
Before declaring a function "passed Pilot", perturb the reference
port (flip one branch, increment one constant by 1) and confirm the
composite drops below 0.95. If perturbing doesn't break the score,
the golden is insufficient — strengthen it before continuing.

PIL.4. **Pilot does not auto-escalate.** Opus L3 is not invoked in
Pilot. Every failure is human-reviewed and either (a) the prompt is
fixed, (b) the harness is fixed, or (c) the spec is fixed. After a
fix, the whole Pilot is re-run from `mpn_add_n` to verify no
regression.

PIL.5. **Transition to Production requires:** 10/10 pilot functions
with composite ≥ 0.95 in a single clean ralph-loop run (no human
intervention mid-run), each function's golden mutation-proven, the
state DB populated with full per-case detail for all 10. Anything
less is not a Pilot pass.

---

## Hallucination-risk callouts

Sharp pre-emptive warnings about MPFR-porting mistakes that look right
but aren't. When you catch yourself about to do one, stop and re-check
the cited reference.

- **NaN ≠ NaN.** IEEE 754 says NaN is unequal to itself. MPFR follows
  the rule. A direct `a === b` comparison in the grader will reject
  every NaN-output golden. The runner needs a `specialCorrectness`
  branch that returns true when both are NaN. Same applies to
  function output: `cmp(NaN, x)` returns 0 in C only if MPFR's
  `erange` flag is set; the idiomatic TS port should throw a
  documented `MPFRRangeError`, never silently return 0.

- **Signed zero is real.** `+0` and `-0` are distinct MPFR values
  and the C tests check this. The TS `MPFR` value type must carry
  sign even when kind is `'zero'`. `add(+0, -0, rnd=RNDN) → +0`;
  `add(+0, -0, rnd=RNDD) → -0`. Get this wrong and ~30% of edge
  goldens fail silently in a way that looks like "rounding off-by-1".

- **Ternary flag is the sign of (rounded - exact), not 0/1.**
  Returns from MPFR ops are `-1` (rounded down from exact), `0`
  (exact), or `+1` (rounded up). The TS port must compute this in
  the same direction the C reference does, in the same rounding
  mode, *exactly*. Off-by-direction (returning the sign of `exact -
  rounded`) is the single most common subtle bug. The C source is
  authoritative — read it; do not infer.

- **Rounding mode count is FIVE.** `MPFR_RNDN` (nearest, ties to
  even), `MPFR_RNDZ` (toward zero), `MPFR_RNDU` (toward +∞),
  `MPFR_RNDD` (toward -∞), `MPFR_RNDA` (away from zero). Earlier
  MPFR versions had `MPFR_RNDNA` (nearest, ties away) — gone in
  current MPFR. Agents trained on older docs sometimes emit
  6-element rounding enums; correct to 5.

- **`mpfr_prec_t` is in bits, not decimal digits.** A common
  hallucination is treating `prec` as digit count. `prec=53` means
  53 bits of mantissa (≈ 16 decimal digits), not 53 digits. Every
  spec.json must include `prec_unit: "bits"` to defuse this.

- **GMP mpn limbs are LITTLE-ENDIAN by limb index.** `limbs[0]` is
  the least-significant 64-bit word. Iteration order in `mpn_add_n`
  is `i = 0 → n-1` with carry propagation rightward (in limb-index
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

- **Cost-estimate token splits are heuristic ±2×.** When reading
  `eval/state.db` `usd_est` column, treat absolute dollars as
  ±2×; relative ordering between runs is sound. Same caveat as the
  predecessor (`../auto-port-eval/RESULTS.md` §"Caveats").

---

## Library coherence

The locked schema lives in `src/core.ts`. It is **versioned and
frozen**; changes go through an ADR (`docs/adr/NNNN-core-schema-*.md`)
and trigger a full library re-grade. Every public function imports
from this file; no port redeclares the types.

```ts
// src/core.ts — LOCKED. Bump only via ADR.

/** A finite or special MPFR value. Immutable. */
export interface MPFR {
  readonly kind: 'normal' | 'zero' | 'inf' | 'nan';
  readonly sign: 1 | -1;          // meaningful for zero/inf/normal; arbitrary for NaN (use 1)
  readonly prec: bigint;          // bits, >= 1; ignored for nan
  readonly exp: bigint;           // base-2 exponent of the value (when kind='normal')
  readonly mant: bigint;          // mantissa, MSB-aligned to prec bits (when kind='normal')
}

/** Five rounding modes per current MPFR (no RNDNA). String enum for
 *  prompt-clarity; never use raw ints. */
export type RoundingMode = 'RNDN' | 'RNDZ' | 'RNDU' | 'RNDD' | 'RNDA';

/** Ternary flag: sign of (rounded - exact). */
export type Ternary = -1 | 0 | 1;

/** Standard return shape for every public op. */
export interface Result {
  readonly value: MPFR;
  readonly ternary: Ternary;
}

/** Thrown only for malformed *input* (prec < 1, unknown rounding mode,
 *  NaN reaching a function that documents it as a domain error). Never
 *  thrown for routine MPFR behaviour (overflow, underflow, NaN output). */
export class MPFRError extends Error {
  constructor(public code: 'EPREC' | 'EROUND' | 'EDOMAIN', msg: string) {
    super(msg);
  }
}
```

**Enforcement (load-bearing, not optional):**

1. **Prompts include `src/core.ts` verbatim** in every port-task
   prompt. The agent is told "import these types; do not redeclare;
   your function must return `Result`."
2. **The grader does an AST check on every port** before running test
   cases: must contain `import { MPFR, RoundingMode, Result } from
   "../core.ts"` (or appropriate relative path); must not contain
   `interface MPFR` or `type RoundingMode` redeclarations. Failure
   → composite=0, error="schema-violation".
3. **The runner validates returned values structurally** against the
   `Result`/`MPFR` shape — kind in the enum, sign in {1, -1}, prec a
   bigint ≥ 1, mant a bigint, ternary in {-1, 0, 1}. A value that
   passes equality on numeric content but has the wrong shape (e.g.
   `kind: 'normal'` with `mant: 0n`) is wrong.
4. **Integration tests** in `eval/integration/<chain>.ts` import
   from `src/index.ts` and run multi-function pipelines. Sample:
   ```ts
   import { setD, mul, add, getD } from '../../src/index.ts';
   const a = setD(3.14, 53n, 'RNDN').value;
   const b = setD(2.71, 53n, 'RNDN').value;
   const c = mul(a, b, 53n, 'RNDN').value;
   const d = add(c, a, 53n, 'RNDN').value;
   const x = getD(d, 'RNDN').value;
   assertClose(x, 3.14 * 2.71 + 3.14, eps=1e-12);
   ```
   These are not per-function goldens — they verify the *library*
   works as a library. Production cannot exit until the integration
   suite passes 100%.

**Naming convention.** Public API uses camelCase (`setD`, `getD`,
`mpfrCmp` becomes `cmp`), strips the `mpfr_` prefix, and reads like
TypeScript. The C-original name is preserved in the JSDoc tag
`@mpfrName mpfr_set_d` so cross-referencing the upstream is trivial.

## State DB schema

`eval/state.db` is SQLite, single file, committed to the repo (small
enough). It's the cross-session truth — agents in future sessions
should `sqlite3 eval/state.db` rather than re-derive state from file
timestamps.

```sql
CREATE TABLE functions (
  name TEXT PRIMARY KEY,         -- mpfr_add
  class TEXT,                    -- arithmetic|conversion|transcendental|misc|substrate
  signature TEXT,                -- TS signature
  deps TEXT,                     -- JSON array
  status TEXT,                   -- pending|in_flight|done|slow|parked|blocked
  best_run_id TEXT,
  best_correctness REAL,
  best_perf_grade REAL,
  attempts INTEGER DEFAULT 0,
  escalated INTEGER DEFAULT 0,
  topo_rank INTEGER              -- sort key for ralph loop's "next" picker
);

CREATE TABLE runs (
  run_id TEXT PRIMARY KEY,
  fn_name TEXT, model TEXT, effort TEXT, seed INTEGER,
  started_at REAL, ended_at REAL,
  composite_correctness REAL, perf_grade REAL,
  n_cases INTEGER, n_pass INTEGER, n_throw INTEGER, n_timegate INTEGER, n_infloop INTEGER,
  first_error TEXT, raw_path TEXT, port_path TEXT, grade_path TEXT,
  usd_est REAL
);

CREATE TABLE cases (
  run_id TEXT, case_idx INTEGER, tag TEXT,
  passed INTEGER, threw INTEGER, timed_out INTEGER,
  ms_actual REAL, ms_budget REAL,
  PRIMARY KEY (run_id, case_idx)
);
```

Common queries (keep in your toolbox):

```sql
-- What should the ralph loop pick next?
SELECT name FROM functions
WHERE status='pending'
  AND NOT EXISTS (
    SELECT 1 FROM json_each(deps) d
    WHERE (SELECT status FROM functions WHERE name=d.value)
            NOT IN ('done', 'slow'))
ORDER BY topo_rank LIMIT 1;

-- Dashboard: how are we doing?
SELECT status, COUNT(*) FROM functions GROUP BY status;

-- Slow but correct — candidates for the Optimize phase
SELECT name, best_perf_grade FROM functions
WHERE status='slow' ORDER BY best_perf_grade ASC;

-- Parked: what blocked us?
SELECT f.name, r.first_error FROM functions f
JOIN runs r ON f.best_run_id = r.run_id
WHERE f.status='parked';
```

---

## Build & test

Bun runs `.ts` files directly (no tsc, no esbuild, no `--experimental`
flag). Until the harness is initialized this section is partly
aspirational.

```bash
# One-time setup (Pilot session zero):
sudo apt install libmpfr-dev libgmp-dev      # system deps
curl -fsSL https://bun.sh/install | bash     # if bun not installed
git clone --depth 1 https://gitlab.inria.fr/mpfr/mpfr.git ./mpfr
python3 -m venv .venv && .venv/bin/pip install pandas matplotlib
sqlite3 eval/state.db < eval/driver/schema.sql
bun install                                  # no-op if package.json has zero deps; sets up bun lockfile

# Build all golden drivers (re-run only on driver source change):
bash eval/golden_master/build.sh

# Generate all goldens (re-run only on driver change or libmpfr upgrade):
bash eval/golden_master/run_all.sh

# Grade a single port:
bun eval/harness/runner.ts \
  --function mpfr_add \
  --port src/internal/mpfr/add.ts \
  --golden eval/functions/mpfr_add/golden.jsonl \
  --output /tmp/grade.json

# Run the integration suite (Production exit criterion, Law 4):
bun test eval/integration/

# Run the ralph loop (Production phase):
python3 eval/driver/ralph.py --phase production --parallel 8

# Run the ralph loop (Pilot phase):
python3 eval/driver/ralph.py --phase pilot --halt-on-failure

# Mutation-prove a single golden:
python3 eval/driver/mutation_prove.py --function mpfr_add

# Dashboard:
python3 eval/driver/dashboard.py

# Sanity-check the published port works on Node too (Rule 12):
node --experimental-strip-types -e "import('./src/index.ts').then(m => console.log(m.add))"
```

---

## Practical guidance

- **Runtime is Bun ≥1.3** for the harness and dev loop. Native TS,
  no bundler, no `--experimental` flags. The published `src/` runs
  on Bun OR Node ≥22 (Rule 12) — keep it that way by never importing
  `Bun.*` or `node:*` from `src/`.
- **MPFR upstream lives under `./mpfr/`, gitignored.** Clone fresh
  per machine. Pin version in `MPFR_VERSION` constant in
  `eval/driver/manifest.py` so all goldens are reproducible.
- **libmpfr is already system-installed** on Ubuntu — `apt show
  libmpfr-dev`. Link drivers with `-lmpfr -lgmp -lm`.
- **Ralph loop dispatches up to 8 parallel `Agent` calls per
  message.** Proven-safe in the auto-port-eval predecessor at 10×;
  we drop to 8 to leave headroom for the worker-isolated grader.
- **Memory** lives under
  `~/.claude/projects/-home-tobiasosborne-Projects-mpfr-ts/memory/`.
  The decisions log (API shape, substrate strategy, failure policy,
  perf gate) is there and load-bearing.
- **The auto-port-eval rig is the reference implementation.** When
  designing an mpfr-ts harness component, check first whether
  `../auto-port-eval/eval/` has the analog. Fork the pattern; don't
  reinvent. (Note: that rig's `prompts.py` hardcodes `/home/tobias/...`
  not `/home/tobiasosborne/...` — do not propagate that bug here.)

---

## Stop conditions (escalate to user)

Don't push through any of these.

- The reference (hand-written) port of a Pilot function scores
  `composite < 1.0`. Either the spec is wrong, the golden is wrong,
  or the runner is wrong — none of which the agent should fix
  autonomously.
- A function's deps cycle in the call graph (suggests the
  extraction is wrong, since MPFR is acyclic by construction).
- Auto-escalate (sonnet → opus) fails on >10% of Production
  functions in a 24h window. Suggests systemic harness problem,
  not per-function difficulty.
- Cost burn exceeds **$50** in a single ralph-loop session without
  a corresponding rise in completed-function count. Suggests an
  agent stuck in a tool-use loop or the prompt is broken.
- A change you are about to make would mutate `../auto-port-eval/`.
  That repo is the reference, not a dependency we patch.

When escalating, attach: the failing function name, the last
`grade.json`, the relevant C source path, and the `state.db` query
that surfaces the pattern.

---

## File map (proposed; create as needed)

```
mpfr-ts/
├── CLAUDE.md                this file
├── PHASE.md                 one line: "Pilot" | "Production" | "Optimize"
├── README.md                public-facing (later)
├── package.json             zero runtime deps
├── mpfr/                    cloned upstream, gitignored
├── src/                     idiomatic TS public API
│   ├── index.ts
│   ├── core.ts              MPFR value type, RoundingMode, constants
│   └── ops/                 add.ts, mul.ts, sqrt.ts, exp.ts, ...
├── src/internal/            faithful substrate
│   ├── mpn/                 add_n.ts, sub_n.ts, mul_basecase.ts, ...
│   └── mpfr/                round_raw.ts, check_range.ts, ...
├── eval/
│   ├── state.db             SQLite — single source of truth
│   ├── functions/<fn>/
│   │   ├── spec.json
│   │   ├── golden_driver.c
│   │   ├── golden.jsonl
│   │   └── mined_tests.jsonl
│   ├── harness/
│   │   ├── runner.ts        worker-pool grader (Rule 4)
│   │   ├── worker.ts        per-test isolation
│   │   └── value_codec.ts
│   ├── golden_master/
│   │   ├── common.h         xorshift, JSONL emit, MPFR (de)serialize
│   │   ├── build.sh
│   │   └── run_all.sh
│   ├── driver/
│   │   ├── schema.sql
│   │   ├── prompts.py       L3-only (sonnet + opus escalation)
│   │   ├── callgraph.py     clang AST → deps + topo rank
│   │   ├── ralph.py         the loop
│   │   ├── mutation_prove.py
│   │   └── dashboard.py
│   ├── reference_ports/     hand-written sanity ports (correct/, broken/)
│   └── reports/             generated dashboards, perf histograms
└── docs/
    ├── worklog/             sharded session log (NNN-*.md)
    └── adr/                 design choices (NNNN-title.md)
```

---

## Session close

When the session is winding down:

1. Update `eval/state.db` with any in_flight runs (mark done /
   parked / slow). Stale `in_flight` rows confuse the next session.
2. If a meaningful chunk closed, add a `docs/worklog/NNN-*.md`
   shard (Context → What changed → Why these choices → Frictions
   surfaced → Acceptance → Pointers).
3. If a non-obvious lesson surfaced, save to memory and update
   `~/.claude/projects/.../memory/MEMORY.md`.
4. `git add eval/state.db` if it changed — the DB is checked in.
5. If this session transitioned a phase, update `PHASE.md` and
   write `docs/worklog/NNN-phase-transition.md` per Rule 14.

---

## Tool of last resort

If the Laws conflict with a fast path: choose the Laws. "Just ship and
fix later" is not a working mode here. The cost of a Production run
that quietly diverges from libmpfr — wrong ternary flag in one in a
hundred ports, never noticed because the goldens weren't
mutation-proven — is months of unwinding when downstream users hit it,
plus loss of confidence in every port. The cost of stopping to read
`mpfr/src/<fn>.c` or re-run the mutation prover is minutes.

When in doubt: re-read this file, re-read the relevant C source,
query the state DB, then ask the user.


<!-- BEGIN BEADS INTEGRATION v:1 profile:minimal hash:ca08a54f -->
## Beads Issue Tracker

This project uses **bd (beads)** for issue tracking. Run `bd prime` to see full workflow context and commands.

### Quick Reference

```bash
bd ready              # Find available work
bd show <id>          # View issue details
bd update <id> --claim  # Claim work
bd close <id>         # Complete work
```

### Rules

- Use `bd` for ALL task tracking — do NOT use TodoWrite, TaskCreate, or markdown TODO lists
- Run `bd prime` for detailed command reference and session close protocol
- Use `bd remember` for persistent knowledge — do NOT use MEMORY.md files

## Session Completion

**When ending a work session**, you MUST complete ALL steps below. Work is NOT complete until `git push` succeeds.

**MANDATORY WORKFLOW:**

1. **File issues for remaining work** - Create issues for anything that needs follow-up
2. **Run quality gates** (if code changed) - Tests, linters, builds
3. **Update issue status** - Close finished work, update in-progress items
4. **PUSH TO REMOTE** - This is MANDATORY:
   ```bash
   git pull --rebase
   bd dolt push
   git push
   git status  # MUST show "up to date with origin"
   ```
5. **Clean up** - Clear stashes, prune remote branches
6. **Verify** - All changes committed AND pushed
7. **Hand off** - Provide context for next session

**CRITICAL RULES:**
- Work is NOT complete until `git push` succeeds
- NEVER stop before pushing - that leaves work stranded locally
- NEVER say "ready to push when you are" - YOU must push
- If push fails, resolve and retry until it succeeds
<!-- END BEADS INTEGRATION -->
