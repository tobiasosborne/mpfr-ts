# 008 — opus-prep automation infrastructure + validation

> Two-arc session. Arc 1 built 4 tools (gen_spec, mutators, mutate,
> calibrate) intended to reduce opus's per-function workload during
> mega-batch prep. Arc 2 validated those tools against the existing
> 115-port corpus before trying them in production. Arc 2 surfaced
> serious tool bugs (silent-wrong gen_spec output on 23% of corpus,
> module-init false-positive in mutate.py) that would have shipped
> silently without the validation step. State at session end: tools
> work and are calibrated; ralph.py integration deferred until two
> small follow-ups land.

## TL;DR

Built and validated 4 tools totaling ~1500 LOC (production + tests +
reports). Found and fixed 3 P2 gen_spec extraction bugs surfacing
silent-wrong output on 23% of the corpus. Mutator menu calibrated at
79% gate pass rate across all 4 function classes. Tools NOT yet
integrated into the live ralph.py prep flow — that's a separate arc
gated by an ADR (mpfr-ts-n8y) on spec-merge policy and two mutator
P2s (mpfr-ts-agn, mpfr-ts-omy).

## What got built

### Arc 1 — infrastructure

| Commit | Tool | Lines (src + test) | Purpose |
|---|---|---:|---|
| `55d5f0d` | `eval/driver/gen_spec.py` | 152 + 111 | Extract structural spec.json fields from C source |
| `a846a16` | `eval/driver/mutators.ts` | 101 + 99 | Regex-based AST mutator (4 mutations: op-swap, rnd-swap, ternary-negate, sign-flip) |
| `00c1935` | `eval/driver/mutate.py` | 242 + 140 | Python orchestrator: list mutations → produce mutants → grade each → aggregate |
| `99f5ddf` | `eval/driver/mutators.ts` (extended) | +75 | +3 mutations (bigint-bump, comparison-swap, shift-direction-swap) |

### Arc 2 — validation

| Commit | Artifact | Purpose |
|---|---|---|
| `1fbca41` | 8 import-shape regression tests in `test_mutate.py` | Codify `_rewrite_relative_imports` contract; all shapes pass without code change |
| `52784e5` | `eval/driver/validate_specs.py` | Structural diff of gen_spec output vs curated spec.json |
| `f654d3c` | `docs/reports/008-gen-spec-delta.md` | Findings from validate_specs across 119 functions |
| `d8d83bb` | `gen_spec.py` fix + 11 new tests | Resolved 3 P2 bugs surfaced by the report |
| `443d0f1` | `eval/driver/calibrate.py` | Drive mutate.py across stratified samples + emit per-class summary |
| `165b19c` | `docs/reports/009-mutator-calibration.md` | Findings from calibrate.py at N=4 and N=6 per class |

## How each tool fits together

```
mpfr/src/<fn>.c                          eval/functions/<fn>/spec.json
        │                                              │
        │              gen_spec.extract_spec()         │
        └─────────────────┐    │    ┌─────────────────┘
                          ▼    ▼    ▼
                    validate_specs (struct diff)
                                │
                                ▼
                      report 008 (deltas)


src/ops/<fn>.ts ──→ mutators.ts (regex perturbation) ──→ /tmp/mutant.ts
                                                              │
                                                              ▼
                          mutate.py ─→  runner.ts (grader) ─→ grade.json
                              │                                    │
                              └─── module_init_failed detection ◀──┘
                              │
                              ▼
                      ProveResult (per-mutation outcomes)
                              │
                              ▼
                      calibrate.py (aggregate across N fixtures)
                              │
                              ▼
                      report 009 (per-class, per-mutation)
```

## Validation findings

### gen_spec.py (report 008)

Initial state: 0/104 functions matched gen_spec on all 5 structural
fields. 23% of corpus had hard extraction bugs:

- **P1 (5 fns)**: parens-around-name macro override (`int (mpfr_inf_p) (...)`) → hard error.
- **P2 (3 fns)**: static decls unconditionally skipped → hard error on the 3 fns whose only decl IS static.
- **P3 (18 fns) — silent-wrong**: static decl skipped, scan continues, lands on a sibling-dispatcher call site, harvests `if (...) return mpfr_add1sp1n (...)` as the function signature. The most dangerous bug because a porter agent would consume this as input with no compile-time signal.

All three resolved in `d8d83bb`:
- Parens-name regex extension (`\b<name>\s*\)?\s*\(`)
- Accept static heads + add `_TYPE_TAIL` regex requiring the matched head to end in a return-type token (rejects control-flow tails)
- Extended `_KNOWN_TYPES` and `_classify_return` scalar mapping

After fix: extraction errors 8 → 0; `signature.returns` disagreement 52.9% → 24.1%; `signature.params` 76.9% → 67.9%.

Remaining disagreements are **compatible-but-different** (filed as P3
ADR `mpfr-ts-n8y`):
- `class`: curated tracks runtime budget tier (misc=1000ms, arithmetic=200ms); gen_spec uses filename heuristic
- `signature.params`: curated uses semantic names (`a, b`); gen_spec lifts literal C decl names (`b, c`)
- `signature.returns`: curated lifts to idiomatic TS (`boolean` for `_p` predicates, `bigint` for `prec_t`); gen_spec mirrors C
- `c_signature`: whitespace + identifier choice differs (curated mirrors `mpfr.h` prototype; gen_spec mirrors definition)

### mutator calibration (report 009)

N=6 per class, 24 fixtures total:

| Class | Gate pass | Mean clean kills/fn | Mean composite |
|---|---|---:|---:|
| arithmetic | 5/6 (83%) | 1.00 | 0.78 |
| conversion | 4/6 (67%) | 0.67 | 0.78 |
| misc | 5/6 (83%) | 1.00 | 0.75 |
| substrate | 5/6 (83%) | 0.83 | 0.63 |

Per-mutation effectiveness:

| Mutation | Times applicable | Clean kill rate | Mean composite |
|---|---:|---:|---:|
| shift-direction-swap | 8 | 87.5% | 0.05 |
| bigint-bump | 18 | 50% | 0.48 |
| op-swap | 7 | 43% | 0.42 |
| ternary-negate | 22 | 14% | 0.85 |
| sign-flip | 10 | 30% | 0.65 |
| comparison-swap | 19 | 5% | 0.92 |
| rnd-swap | 16 | 0% | 1.00 |

The dominant catch is `shift-direction-swap`. `rnd-swap` is dead
weight at this sample (0 clean kills); `comparison-swap` is near-
dead. The N=1 smoke from step 4 had falsely suggested arithmetic and
conversion never trigger the gate — at N=6, every class passes.

Two new mutator regex bugs surfaced (filed as P2):
- `ternary-negate` corrupts `const { ternary: tr } = ...` destructuring → invalid TS
- `op-swap` corrupts `export function mpfr_add(...)` → invalid TS

Both are caught by `mutate.py`'s `module_init_failed` detection (no
false-pass gates), but each wastes ~1 second per run.

## Open bds (post-validation queue)

### Blocking step 6 (ralph.py integration)

- `mpfr-ts-n8y` [P3] — ADR documenting curated divergences from gen_spec (class as budget tier, identifier renaming, idiomatic-TS returns). Needed so the spec-merge logic in `_render_prep_prompt` knows which fields are curator-wins vs gen_spec-wins.

### Mutator polish (not blocking, but worth addressing before any replacement-mode trial)

- `mpfr-ts-agn` [P2] — ternary-negate must not mutate destructuring patterns
- `mpfr-ts-omy` [P2] — op-swap must not mutate function declarations
- `mpfr-ts-9di` [P3] — mutate.py gate behavior for trivial-body ports (mpfr_swap, mpfr_set_inf — no applicable mutations means gate vacuously fails)
- `mpfr-ts-18x` [P3] — comparison-swap should enumerate all sites, not first-match (would lift conversion class effectiveness)
- `mpfr-ts-6zg` [P4] — branch-replacement rnd-swap variant
- `mpfr-ts-lyr` [P2] — already-closed; new menu extension added 3 mutations

### Curated spec normalization (post-step-6 cleanup)

- `mpfr-ts-bqq` [P4] — prec_unit inconsistent on no-prec-param fns
- `mpfr-ts-00m` [P4] — c_signature space-before-paren + identifier rename drift

## What's NOT done

**Step 6 — wire gen_spec into `ralph.py`'s `_render_prep_prompt`** is
deferred. It's the first step of an *automation trial*, not part of
*validation before trial*. Pre-work needed:

1. Write the ADR (`mpfr-ts-n8y`) documenting which fields curator
   wins vs gen_spec wins.
2. Decide what the prompt does with gen_spec's output. Likely shape:
   "Here is a partial spec scaffold (function, c_signature, refs,
   class_hint, prec_unit). Fill in: doc, divergence_from_c. You MAY
   override class, signature.params, signature.returns if the
   structural inference is wrong for this function."
3. Optionally fix mpfr-ts-agn + mpfr-ts-omy first if you want the
   gate to also be used in the prep workflow for cross-checking.

Estimated step 6 effort: ~30 min ADR + ~80-120 LOC delta to
`ralph.py` + new test cases. The existing 52 ralph.py pytest cases
must stay green.

## Recommended trial procedure

When you decide to actually use the automation, the safest first move
is **shadow mode**:

1. Pick the next mega-batch's 30 functions normally.
2. Run opus prep with the **existing** workflow (no ralph.py changes
   yet).
3. After opus prep finishes, for each of the 30 functions:
   a. Run `gen_spec.extract_spec` and compare its output to opus's
      spec.json — see how often opus's choices differ from
      gen_spec's structural extraction.
   b. After sonnet ports the function, run `mutate.mutation_prove`
      against the port + golden — see if the menu's gate agrees with
      opus's broken-port mutation-prove result.

Cost: ~5 min orchestrator time per function for the shadow comparison.
Reveals what the integration would have actually done across a real
batch, without committing to the integration yet.

If shadow mode passes (gen_spec produces useful scaffolds; mutate.py
gates agree with opus's mutation-prove on >90% of functions), proceed
with **replacement mode** in the next mega-batch:

1. Land step 6 (ralph.py wiring).
2. Modify the opus prep prompt to skip the broken-port deliverable
   when mutate.py's gate passes against the eventual sonnet port.
3. Save ~30 LOC of opus output per function plus ~3-5 min of opus
   time per function.

## Failure modes the validation caught (would have shipped silently)

1. **gen_spec returning call-site garbage on 18 functions** — porter
   prompts would have included `if (p == GMP_NUMB_BITS) return mpfr_add1sp1n (...)` as a function signature. Caught in report 008 §P3.

2. **mutate.py reporting gate_passed on every mutant** — the import-
   rewrite bug meant every mutant failed at module-init, scored
   composite=0, and the gate vacuously passed on a zero-mutation-
   ran result. Caught during step 3 of the prior arc when the
   orchestrator noticed every score was exactly 0.0 (Rule 3
   skepticism).

3. **Initial mutator menu too weak** — 1/6 clean kills across
   calibration. Caught by the calibration test design (required
   ≥4/6); led to the menu extension in `99f5ddf`.

4. **`ternary-negate` regex matching `const ternary: Ternary = ...`**
   — initial regex broke transpile on `sqr_2.ts`. Caught by the
   round-trip transpile gate in mutators.test.ts.

The pattern in each: a checking layer designed for skepticism (Rule
3, round-trip transpile gate, calibration target, validation-before-
trial) caught a silent failure mode that would have undermined the
tool's purpose. Each catch was orthogonal to its own tool's tests —
the failures only surfaced when the tool was used against real data
at scale.

## Pickup checklist

```bash
git pull --rebase
cat PHASE.md                                          # → Pilot
cat HANDOFF.md                                        # last refreshed for 115-port milestone
cat docs/worklog/008-automation-infra.md              # this file
cat docs/reports/008-gen-spec-delta.md                # gen_spec findings
cat docs/reports/009-mutator-calibration.md           # mutator calibration

# Verify tools still pass
cd eval/driver && /home/tobiasosborne/.local/bin/pytest tests/ -v
# Expected: 47 passing (gen_spec 19 + mutate 14 + validate_specs 7 +
# calibrate 5 + 2 misc; double-check the count drifted)

cd eval/driver && bun test mutators.test.ts
# Expected: 22 passing

bun x tsc --noEmit                                    # must be clean

bd ready                                              # open issues to triage
```

## Lessons (durable)

1. **Validate before you integrate.** Both gen_spec and mutate.py
   shipped initial calibration tests that passed. Both had silent
   failure modes only the *broader* validation surfaced — gen_spec's
   P3 call-site harvesting, mutate.py's module-init false positive.
   The investment in `validate_specs.py` and `calibrate.py` was
   roughly 2 hours of subagent time + 1 hour of orchestrator
   analysis. The cost of NOT building them would have been a
   poisoned mega-batch where every port had a wrong spec.

2. **Real fixtures > synthetic fixtures by orders of magnitude.** The
   gen_spec test suite at the end of step 1 (Arc 1) had 8 pytest
   cases all using synthetic C source strings. Real C source surfaced
   3 entirely-new bug classes (parens-name, static-skip, type-table
   gaps). Build synthetic tests for the algorithm; calibrate against
   real data for the integration boundary.

3. **A "skepticism check" pays for itself.** The orchestrator noticing
   every mutate.py composite was exactly 0.0 (Rule 3) caught the
   import-rewrite bug that all five calibration tests had passed.
   Without that pause, the false-positive gate would have shipped.

4. **Regex-based AST manipulation needs constant guardrails.** Three
   separate regex bugs in this arc (ternary-negate on declarations,
   ternary-negate on destructuring, op-swap on function decls). The
   round-trip transpile gate in mutators.test.ts caught one; the
   module_init_failed detection in mutate.py caught the other two.
   Without those layers, each regex bug would have polluted real
   gates with mutants that failed for the wrong reason.

5. **Per-class behavior is real but smaller than initial signal
   suggests.** The N=1 smoke "only substrate catches mutations"
   collapsed at N=6 to "all classes catch but substrate catches
   hardest." Always sample big enough to escape variance.
