# 007 — 30-function mega-batch: 85 → 115 ports landed

> Picking up from `docs/worklog/006-scale-out-engine.md`. Target set by
> the user-directive addendum in 006: a single 30-function mega-batch,
> cost-disciplined waves of 5–10 sonnets per dispatch off one large
> opus prep. Result: 30 ports landed at composite=1.0, almost all
> one-shot. State.db `done`: **85 → 115**.

## TL;DR

- One opus prep dispatch of 30 functions (~49 min, ~380K tokens).
- 4 sonnet waves of 6–8 in parallel; 30/30 graded clean.
- 2 engine fixes landed mid-batch: `_promote_port` rewrites absolute
  imports to relative; `compareField` uses `Object.is` for numbers in
  object-shaped outputs.
- 1 golden-driver bug fixed: `mpfr_mpn_cmp_aux` was leaking previous
  cases' `bp[]` writes into the next case's read window when `extra=1`.
- Post-batch tsc cleanup (commit `394ef02`) removed unused locals/imports
  the agents emitted under `noUnusedLocals/Parameters`.
- bd cleanup (commit `3074a98`) closed 8 obsolete pilot issues; filed
  8 new ones replacing those lost in the earlier dolt-sync gap plus
  fresh ones surfaced this session.

State.db: **115 done, 4 blocked, 119 rows total, 146 runs**.

## The 30 functions

Grouped by theme (mixed batch, not the single-theme strategy 006
recommended — chosen for breadth of user-facing API coverage):

| Theme | Count | Functions |
|---|---:|---|
| sqr family | 3 | `mpfr_sqr_1`, `mpfr_sqr_1n`, `mpfr_sqr_2` |
| mul fast paths | 3 | `mpfr_mul_1`, `mpfr_mul_1n`, `mpfr_mul_2` |
| div fast paths | 3 | `mpfr_div_1`, `mpfr_div_1n`, `mpfr_div2_approx` |
| mpn helpers | 3 | `mpfr_mpn_cmp_aux`, `mpfr_mpn_cmpzero`, `mpfr_mpn_sub_aux` |
| `_d` arithmetic | 6 | `mpfr_{add,sub,mul,div}_d`, `mpfr_d_{sub,div}` |
| `_2exp` aliases | 2 | `mpfr_mul_2exp`, `mpfr_div_2exp` |
| conversion helpers | 3 | `mpfr_get_d_2exp`, `mpfr_get_d1`, `mpfr_set_flt` |
| predicates | 4 | `mpfr_regular_p`, `mpfr_integer_p`, `mpfr_odd_p`, `mpfr_cmpabs` |
| cmp helpers | 2 | `mpfr_cmp3`, `mpfr_cmpabs_ui` |
| accessor | 1 | `mpfr_get_default_prec` |

Public API surface added: the four `_d` (double-coerced) arithmetic
ops plus the two `_d_` reverse forms — these close out a big chunk
of the "mix MPFR with native doubles" API. All five fast-path
families (`sqr_1`, `mul_1`, `div_1`) are now present, which is what
makes 53-bit single-limb prec actually fast in the public ops.

## Methodology — cost-disciplined waves

Per 006's user directive:

1. **Opus prep**: single dispatch, all 30 functions in one prompt.
   ~49 min wall, ~380K opus tokens. No context exhaustion observed —
   contradicts the linear extrapolation from 15→30 that predicted
   ~700K. Opus is sublinear in batch size for prep tasks.
2. **Sonnet waves**: 4 waves of 6–8 in parallel, await each, spot-check
   `--grade` subset, then dispatch the next. No wave required
   re-dispatch.
3. **`--ship`**: single invocation after the last wave promoted all 30
   ports atomically with `--ship --message "..." <30 fn names>`.

Observed cost/throughput vs prior batches:

| Batch | Fns | Wall (opus prep + sonnet waves) | Opus tokens | Sonnet failures |
|---|---:|---:|---:|---:|
| C-mega (006) | 15 | ~45 min | ~387K | 0 |
| 007 mega | 30 | ~49 min + 4 waves | ~380K | 0 |

The big finding: **doubling the batch size barely moved opus prep
cost or wall time**. The previous worklog's "linear extrapolation"
prediction was wrong — opus prep scales sub-linearly with function
count because the CLAUDE.md / worked-example load is fixed-cost. The
implication is that batch-size headroom is much larger than 006
guessed; 50–60 in one prep is probably tractable on cost grounds. The
real ceiling is orchestrator review bandwidth, not opus tokens.

## Engine fixes (resolves prior bds)

### `_promote_port` inline path rewrite — resolves `mpfr-ts-NEW1`

Previously: sonnet writes `/tmp/eval_<fn>/port.ts` with absolute
imports (`/home/tobias/Projects/mpfr-ts/src/core.ts`) because the
file runs from `/tmp` where relative paths to `src/` don't resolve.
The orchestrator was manually running a Python rewrite script after
each promote.

Now: `_promote_port` in `ralph.py` rewrites all absolute imports of
`/home/tobias/Projects/mpfr-ts/...` (single- and double-quoted) to
the correct relative path based on the destination directory under
`src/`. A single `--ship` invocation now handles grade → promote →
commit end-to-end without orchestrator intervention.

This was 006's #1 P2 engine improvement. It works.

### `--grade` substrate fallback — resolves `mpfr-ts-2r8`

Previously: `--grade <fn>` resolved port paths only against
`src/ops/<short>.ts`. Substrate-class ports landing under
`src/internal/mpfr/<short>.ts` or `src/internal/mpn/<short>.ts`
fell through to "port not found".

Now: `--grade` reads `state.db`'s `class` column for the function and
picks the canonical destination (`src/internal/mpfr/` for substrate
non-mpn, `src/internal/mpn/` for substrate mpn, `src/ops/` for
everything else). Grading substrate functions no longer needs a
manual `runner.ts` invocation.

### `compareField` uses `Object.is` for object-shaped number fields

Bug: `mpfr_get_d_2exp` returns `{value: number, exp: bigint}`. When
`value` is NaN, the runner's per-field comparator was using `===`
on the number field (NaN !== NaN, fails by design). The scalar-output
branch already used `Object.is`; the object-field branch did not.

Fix: object-field number comparisons now use `Object.is`, matching
the scalar branch. NaN-output goldens for object-returning functions
now pass.

### `mpfr_mpn_cmp_aux` golden driver fix

Bug: the C helper reads `bp[bn]` and `bp[bn+1]` when `extra=1`. The
driver's per-case scratch buffer was reused without zeroing the slack
limbs, so the previous case's writes were polluting the next case's
read window. Composite was 0.86 until fixed; after zeroing the slack,
composite=1.0.

## Lessons (durable)

1. **Opus prep is sublinear in batch size for cost.** 006 predicted
   ~700K tokens for a 30-fn prep based on a 15-fn baseline of ~387K.
   Actual was ~380K — essentially flat. The fixed-cost overhead
   (CLAUDE.md read, worked example, harness investigation) dominates.
   **Implication**: increase mega-batch size to 50–60 functions per
   opus prep before the next saturation experiment.

2. **Wave dispatch of 6–8 sonnets is robust at scale.** 4 waves × 6–8
   parallel sonnets across 30 functions = 0 failures, 0 retries.
   Confirms 006's #3 finding that N=15+ parallel is safe; this session
   stayed at N=6–8 per wave for cost discipline (user directive), not
   because of harness limits.

3. **Mixed-theme batches still amortize well.** 006 recommended
   single-theme batches for "shared subtlety" leverage. This session
   used 10 themes across 30 functions and still got 100% one-shot.
   The opus prep's per-function spec generation is robust to theme
   heterogeneity. **Implication**: don't force theme coherence at the
   cost of leaving high-value functions out of a batch.

4. **TSC noUnusedLocals/Parameters catches roughly one issue per 10
   sonnet ports.** Post-batch cleanup (`394ef02`) removed 6 small
   issues (unused imports, dead helpers, redundant locals) across
   the 30 ports. Worth either (a) tightening the agent prompt to
   strip unused emit, or (b) keeping the post-batch tsc-cleanup pass
   as a routine step. Currently doing (b).

5. **Substrate-class promote needs ref_port path fix in opus
   templates.** Five mega-batch ports were substrate-class but opus
   wrote their ref_port stubs assuming `src/ops/...`. Post-batch
   cleanup moved them to `src/internal/mpfr/...`. Not yet automated;
   the opus template should consult state.db `class` when generating
   the ref_port path. Filed implicitly as a future improvement (not
   yet a bd issue — low recurrence rate makes it borderline).

## bd churn

Closed (commit `3074a98`):
- 8 obsolete pilot-era issues (`mpfr-ts-637`, `mpfr-ts-odi`, and
  variants `odi.5-10`) — superseded by 115 ports landed since the
  pilot completed in worklog 003.

Filed (commit `3074a98`):
- `mpfr-ts-i8e` [P2] — git pre-commit hook to run `bd export -o
  .beads/issues.jsonl` (replaces the dolt-sync-gap class of bug
  flagged in the old HANDOFF).
- `mpfr-ts-ai4` [P3] — runner `n_throw` conflates exceptions with
  value mismatches.
- `mpfr-ts-e4j` [P3] — `expected_throw` codec for goldens with
  domain errors (unblocks `mpfr_abort_prec_max` and similar).
- `mpfr-ts-lq8` [P3] — `eval/golden_master/run_all.sh` wrapper.
- `mpfr-ts-sr4` [P3] — enforce Rule 7 tag minimums at grade time.
- `mpfr-ts-2ls` [P3] — value_codec scalar-string outputs.
- `mpfr-ts-d6o` [P3] — callgraph.py misses mpn_* substrate fns
  (sourced from GMP, not in `mpfr/src/`).
- `mpfr-ts-c6b` [P4] — state.db topo_rank diverges from callgraph
  topo_rank.

Resolved (not re-filed): `mpfr-ts-NEW1` (promote path rewrite) and
`mpfr-ts-2r8` (--grade substrate fallback) — both fixed this session.

## State of the library

```
src/
├── core.ts                       LOCKED (unchanged)
├── internal/
│   ├── mpn/                      4 files
│   └── mpfr/                     14 files (was 9; +5 substrate this batch)
└── ops/                          ~96 public ports

eval/
├── state.db                      119 rows: 115 done, 4 blocked, 146 runs
└── functions/                    119 spec.json + golden_driver.c sets
```

The 4 blocked functions (unchanged from 006): `mpfr_abort_prec_max`
(needs `expected_throw` codec), `mpfr_{allocate,free,reallocate}_func`
(allocator hooks — likely permanently blocked; no clean TS analogue).

## Open questions / flags for the next session

1. **`PHASE.md` still says `Pilot`.** Per CLAUDE.md, Pilot is "the
   first 10 functions". We're at 115. The transition was deferred
   across multiple sessions; Rule 14 requires a worklog phase-
   transition document before flipping the file. This should be the
   first action of whichever session takes on the next major batch.
   The auto-escalate (sonnet → opus) policy implied by Production is
   not yet active; the orchestrator is implicitly running a "Pilot+"
   workflow (halt-on-failure, no opus escalation) at 115 ports. Decide
   explicitly whether to formalize Production or to keep Pilot rules
   indefinitely.

2. **Mega-batch ceiling untested above 30.** Opus prep was nearly
   flat from 15→30. Next saturation experiment should try **50–60
   functions per opus prep** to find where context or coherence
   actually breaks. Cost projection: ~400–450K opus tokens (sublinear
   extrapolation); waves of 8–10 sonnets across 6–7 waves.

3. **TSC-cleanup post-pass should be automated or prompt-fixed.** Six
   minor issues across 30 ports is a low rate but a fixed cost. Either
   (a) add a one-line `bun x tsc --noEmit` gate to `--ship` that
   fails if there are unused-symbol warnings, or (b) extend the agent
   prompt to strip unused emit. Cheap to do; defer until next mega-
   batch makes it actually annoying.

## What the next agent should do

Mirroring 006's closing recommendation, updated for current state:

1. **Decide on PHASE.md.** Either write `docs/worklog/008-phase-
   transition.md` and flip to Production (auto-escalate enabled), or
   document why Pilot continues. ~30 min.
2. **One engine bd if it bothers you**: `mpfr-ts-i8e` is the highest
   recurrence-rate one (every manual commit risks the dolt-sync gap).
   ~15 min to write a pre-commit hook.
3. **50–60-function mega-batch** to test the empirical ceiling.
   Conversion family is still the highest-coherence pool. Cost
   envelope: ~450K opus + ~6–7 waves of 8–10 sonnets each.
4. Or: pivot to **transcendentals**, which exercise the prec-extension
   loop pattern not yet present in any port. Smaller batches (5–10)
   advisable until the pattern is validated.

## Pickup checklist

```bash
git pull --rebase
cat PHASE.md                       # → Pilot
cat HANDOFF.md                     # → this worklog's summary
sqlite3 eval/state.db "SELECT status, COUNT(*) FROM functions GROUP BY status"
# Expected: blocked|4, done|115

bun x tsc --noEmit                 # must be clean
bun eval/acceptance/step5/run.ts   # 5/5 expected
/home/tobias/.local/bin/pytest eval/driver/tests/  # 52/52 expected

bd ready                           # 8 issues, all P2-P4 polish
```
