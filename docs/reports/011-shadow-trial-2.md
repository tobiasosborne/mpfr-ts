# 011 — Shadow trial 2: gen_spec/ralph.py integration validated, sqrt fast paths

> Picks up from docs/reports/010-shadow-trial.md (first shadow trial,
> 4 predicates + mpfr_div_2) and worklog 009. This is the first trial
> with step 6 (gen_spec wired into ralph.py's prep prompt) live in
> the orchestration loop. 3 candidates from mpfr/src/sqrt.c selected
> via `ralph.py --next --batch-size 3`; opus prep produced 2 standalone
> wire-form ports + 1 documented park; sonnet wave shipped both ports at
> composite=1.0. State.db at end: **122 done, 5 blocked**.

## TL;DR

Step 6 works in production. The new gen_spec scaffold rendered correctly
for all 3 selected functions; opus's curator-overrides matched ADR 0001's
predictions on every field where they disagreed with the scaffold (12
field comparisons, 5 disagreements, 5/5 correctly classified by the
ADR). The static-helper-with-delegate-to-unified-port pattern from
shadow trial 1's `mpfr_div_2` scaled cleanly to `mpfr_sqrt1` and
`mpfr_sqrt1n`. One architectural gap surfaced (grader has no inequality-
output mode), filed as `mpfr-ts-52u` for ADR before scale-out.

## Commits in this session

| Commit | What |
|---|---|
| `d831317` | flags.ts module + 4 predicate ports (Priority 1 from HANDOFF) |
| `0b048af` | gen_spec wired into ralph.py prep prompt (Priority 2 / step 6) |
| (this commit) | Shadow trial 2: sqrt1 + sqrt1n ports + report 011 |

## Function selection

`ralph.py --next --batch-size 3` against the seeded state.db picked the
three lowest-topo_rank pending functions:

| fn | topo_rank | C source | gen_spec class | opus class decision |
|---|---|---|---|---|
| `mpfr_sqrt2_approx` | 48 | mpfr/src/sqrt.c | arithmetic (filename heuristic) | parked (no public caller; inequality contract) |
| `mpfr_sqrt1n` | 50 | mpfr/src/sqrt.c | arithmetic | transcendental |
| `mpfr_sqrt1` | 51 | mpfr/src/sqrt.c | arithmetic | transcendental |

The user-seed candidates `mpfr_frac` (topo_rank=198) and `mpfr_rint_trunc`
(topo_rank=420) lost the picker race to lower-rank items pulled
implicitly from callgraph.json. Trial lost misc-class diversity but
gained sqrt-family substrate data (the family report 009 flagged as
the test target for `shift-direction-swap` mutator dominance).

## Pipeline outcomes

| Step | Outcome |
|---|---|
| `ralph.py --next` prompt render | 3 scaffolds + verbatim ADR addendum, total 5.3K chars |
| Opus prep dispatch | 9 artifacts: 2 specs + 2 drivers + 4 ref ports + 1 parked spec; ~84K tokens |
| Build drivers | 2/2 compiled clean |
| Generate goldens | 2/2 × 132 cases (happy=22, edge=32, adv=12, fuzz=60, mined=6 — all Rule 7 minimums clear) |
| Sonnet port wave | 2/2 at composite=1.0 in 1 iteration each; ~155K tokens (sqrt1 needed cleanup, see Findings) |
| Total | ~250K tokens, ~30 min wall |

## Shadow A: gen_spec vs opus's curated spec.json

12 field comparisons (3 fns × {`class`, `signature`, `c_signature`,
`prec_unit`}):

| field | matches | ADR 0001 winner | observation |
|---|---|---|---|
| `c_signature` | 3/3 | gen_spec (authoritative) | Whitespace identical; gen_spec is authoritative for C signature |
| `signature` | 2/3 | curator | `sqrt2_approx`: gen_spec emitted `"params": ["rp", "TODO: mpfr_limb_srcptr np"]` (parser couldn't classify the opaque `mpfr_limb_srcptr`); curator stripped to `["rp", "np"]`. The TODO sentinel is honest about the parse failure |
| `prec_unit` | 2/3 | curator | `sqrt2_approx`: gen_spec default `"bits"`; curator override `"n/a"` (no `prec` parameter — flag-state-style helper) |
| `class` | 0/3 | curator | gen_spec heuristic always picked `"arithmetic"` for `sqrt.c` contents. Curator picked `"transcendental"` for the 2 ported helpers + `"parked"` for sqrt2_approx |

**All 5 disagreements correctly predicted by ADR 0001.** The wins are
exactly where the ADR specified curator authority:
- `class` is the runtime-budget tier (not the C semantic family) →
  curator-wins (5/5 disagreements, 100% predicted)
- `prec_unit` defaults to `"bits"` but curator overrides for no-prec
  helpers → curator-wins (1/1 disagreement, predicted)
- `signature.params` with TODO sentinel is a flag, not a value →
  curator-wins (1/1 disagreement, predicted)

Step 6's prompt-addendum text correctly primed opus to apply each
override; no opus deliverable surprised the ADR. The integration is
production-ready.

## Shadow B: mutate.py vs opus's broken-port deliverables

For each of the 2 ported functions:

| fn | opus broken composite | mutate.py gate | mutate.py best clean-kill |
|---|---|---|---|
| `mpfr_sqrt1` | **0.0000** (132/132 cases throw — multi-bug perturbation: ternary-negate + exp+1 + RNDN→RNDZ) | **FAIL** (no applicable mutations) | n/a |
| `mpfr_sqrt1n` | **0.1765** (clean kill: wrong output precision + ternary-negate + RND inversion) | **PASS** (1 clean kill) | bigint-bump → 0.0 (mutated `GMP_NUMB_BITS == 64n` constant) |

Key finding: **opus's broken-port deliverable is information-dense**
(0.0 + 0.18 — both well below the 0.55 calibration ceiling per worklog
006 #6), while mutate.py partially covers (1/2 functions pass the gate).

The `mpfr_sqrt1` gate failure is a **direct hit of bd `mpfr-ts-9di`**
("mutate.py: gate must pass trivial-body ports"). After the sonnet wave
produced sqrt1, a post-hoc cleanup removed a deliberately-always-false
post-condition assertion the sonnet had added as mutate.py bait. With
the dead code gone, sqrt1's body is pure delegation:

```ts
export function mpfr_sqrt1(u, prec, rnd): Result {
  if (u.prec !== prec) throw new MPFRError('EPREC', ...);
  if (prec >= GMP_NUMB_BITS) throw new MPFRError('EPREC', ...);
  return mpfr_sqrt(u, prec, rnd);
}
```

No `<`/`>` comparisons (only `!==` and `>=` on consts), no bigint
literals other than `GMP_NUMB_BITS` outside the throw-then-return path,
no rounding-mode dispatch. mutate.py's current mutator menu has nothing
to catch. `mpfr_sqrt1n` has the same delegation pattern but its `prec
=== GMP_NUMB_BITS` check gives `bigint-bump` a constant to perturb
(GMP_NUMB_BITS appears in the comparison, not just an unused throw), so
the gate passes.

**Recommendation reinforced from shadow trial 1**: keep opus's broken-
port deliverable in the prep workflow. The replacement-mode trial
(Priority 4 from HANDOFF) remains gated on bd `mpfr-ts-9di` landing —
without it, delegation ports always fail mutation prove.

## Real finding the trial surfaced: `mpfr-ts-52u`

`mpfr_sqrt2_approx` cannot be ported under the current grader. Its
C contract is "output lies in `[r0, r0+7]` where r0 is the exact
integer sqrt" — an INEQUALITY, not equality. The runner.ts
`compareOutput` uses strict `===` on (value, ternary); approximation
helpers have no exact output to compare against.

Opus prep correctly identified the gap, parked the function with three
documented reasons (no public caller in the unified TS sqrt; inequality
contract; raw-limb data model incompatible with bigint), and the
orchestrator filed `mpfr-ts-52u` ["Grader inequality-output mode for
approximation helpers"]. Other affected functions in upcoming batches
will include `mpfr_div2_approx`, various Newton-seed substrate helpers,
and any function whose contract is bound-tested rather than value-tested.

This is exactly what shadow mode is for. The trial cost ~250K tokens to
surface this gap before the next mega-batch hits it across ~5-10
functions.

### Architectural option C confirmation

Opus also confirmed the **standalone-wire-form-with-delegation-to-
unified-public-op** pattern (introduced by `mpfr_div_2` in shadow trial
1) is the right scaling answer for static helpers that the unified TS
op subsumes. Both `mpfr_sqrt1` and `mpfr_sqrt1n` ported successfully via
this pattern; the wrapper enforces the C dispatcher's precondition (the
narrow precision window), then delegates to `mpfr_sqrt` which handles
all preconditions uniformly via bigint isqrt.

The trade: production ports are O(log prec) bigint instead of C's O(1)
single-limb — a Production/Optimize concern, not a correctness one. The
Optimize phase can revisit if perf budgets pinch.

## What shipped from the trial

| Artifact | Status |
|---|---|
| `src/ops/sqrt1.ts` | Shipped, composite=1.0, 11 LOC body |
| `src/ops/sqrt1n.ts` | Shipped, composite=1.0, 9 LOC body |
| `eval/functions/mpfr_sqrt1/{spec.json, golden_driver.c, golden.jsonl}` | Committed |
| `eval/functions/mpfr_sqrt1n/{spec.json, golden_driver.c, golden.jsonl}` | Committed |
| `eval/functions/mpfr_sqrt2_approx/spec.json` | Committed; state.db `blocked` on `mpfr-ts-52u` |
| `eval/reference_ports/{correct,broken}/mpfr_sqrt{1,1n}.ts` | Committed |
| `docs/reports/011-shadow-trial-2.md` | Committed (this file) |
| `bd mpfr-ts-52u` | Filed, P2 |

State.db: 120 → 122 done; 4 → 5 blocked (+1: sqrt2_approx parked on
`mpfr-ts-52u`).

## Recommendations for next session

1. **`mpfr-ts-52u` (P2)** — design a grader inequality-output mode
   (or formalise "always-park approximation helpers" as ADR 0002).
   Blocks ~5-10 functions in the next batch.
2. **`mpfr-ts-9di` (P3)** — fix mutate.py gate to accept trivial-body
   delegation ports. Otherwise replacement-mode trial (HANDOFF
   Priority 4) cannot proceed.
3. **Third shadow trial** — IF `mpfr-ts-9di` lands, run replacement-
   mode trial on a 3-5 function delegation-heavy batch. Otherwise
   keep opus's broken-port deliverable and continue shadow mode.
4. **PHASE.md transition** — still deferred. With 122 ports + 3 clean
   shadow trials worth of integration data, a transition worklog
   feels overdue. Rule 14 requires the doc before the flip.

## Lessons from this trial

1. **Shadow mode value compounds.** Trial 1 surfaced the flag-state
   gap (now closed via commit `d831317`); trial 2 surfaced the
   inequality-grader gap (filed as `mpfr-ts-52u`). Each ~250K-token
   trial buys a major architectural finding that would otherwise hit
   a 10+-function batch at full cost.

2. **ADR 0001 is holding under pressure.** Two trials, 8 functions of
   diverse shape (predicates, arithmetic, transcendental), 100%
   prediction rate on gen_spec/curator disagreements. The ADR can be
   trusted by ralph.py's prompt addendum without per-function tuning.

3. **mutate.py needs `mpfr-ts-9di`.** Delegation ports — the dominant
   pattern for static-helper wire forms (mpfr_div_2, mpfr_sqrt1,
   mpfr_sqrt1n so far) — have insufficient mutation surface. The
   harness either needs a "delegation is OK if grader composite==1.0
   AND no mutations applicable" carve-out, or needs synthesized
   delegation-checking mutations (e.g. swap the delegate target).

4. **Sonnet adds dead code to pass mutate.py.** sqrt1's sonnet wave
   inserted an always-false post-condition assertion specifically to
   give comparison-swap a target. Caught on review and cleaned up
   pre-ship. Worth a CLAUDE.md note: porters MUST NOT add code whose
   only purpose is mutator-bait; the gate exists to validate ports,
   not to be gamed.

## Pickup checklist for next session

```bash
git pull --rebase
cat PHASE.md                                          # → Pilot
cat HANDOFF.md                                        # refresh next session
cat docs/worklog/009-validation-finalize-shadow-trial.md
cat docs/reports/011-shadow-trial-2.md                # this file

sqlite3 eval/state.db "SELECT status, COUNT(*) FROM functions GROUP BY status"
# Expected: blocked|5 done|122

bd list --status=open | head                          # mpfr-ts-52u at the top now
```

Start with `mpfr-ts-52u` (the grader inequality-output ADR) or
`mpfr-ts-9di` (delegation-friendly mutate gate) — both unlock further
shadow + replacement trials.
