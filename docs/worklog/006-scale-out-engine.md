# 006 — Scale-out engine: 50 → 85 in one session

> Picking up from `docs/worklog/005-scale-out-handoff.md`. Goal:
> validate the scale-out engine end-to-end and push port count to 75.
> Result: built the engine + 35 additional ports landed (50 → 85), 10
> above the 75 target. Engine throughput improved from ~1.2 to ~0.33
> orchestrator-actions per function across the session.

## TL;DR

This session built the **scale-out engine** that `005-scale-out-handoff.md`
specified, then validated it by porting 35 functions across 5 batches
(4 portable + 4 deferred for harness reasons). All 35 portable ports
scored composite=1.0 against libmpfr-derived goldens; 4 functions were
cleanly deferred (allocator hooks + abort_prec_max).

State.db tally at session end: **89 functions tracked, 85 done, 4 blocked**.

## Phases (in execution order)

**A — engine build (3 components):**
1. **A1**: `ast_check.ts` — strip imports before REDECL_PATTERNS so the
   mixed `import { type X, Y }` syntax passes. Resolves `mpfr-ts-wli`.
   18 new unit tests. (`305b5d5`)
2. **A2**: `eval/driver/callgraph.py` — extract mpfr_*/mpn_* call graph
   from mpfr/src/*.c (525 functions, 8 cycles, topo-sorted, JSON emit).
   14 pytest cases. (`aca8472`)
3. **A3**: `eval/driver/ralph.py` — new modes `--next`, `--grade`,
   `--commit-batch` (collapses ~7-10 orchestrator commands per batch
   into 3 scripted invocations). 20 new pytest cases. (`d85901d`)

**B — engine pilot (3 ports + 1 deferral):**
- mpfr_get_prec, mpfr_setmax, mpfr_setmin all one-shot composite=1.0
  via the new engine. mpfr_abort_prec_max cleanly deferred — would need
  `expected_throw` codec support which is out of scope. (`a922130`)

**C — scale-out (32 more ports across 4 batches):**
- **C1** (5): mpfr_overflow, mpfr_underflow, mpfr_powerof2_raw2,
  mpfr_print_rnd_mode, mpfr_add1sp1. Engine fix: `--next` now skips
  status='blocked' (regression test added). Allocator hooks
  mpfr_{allocate,free,reallocate}_func blocked permanently. (`7ab3b65`)
- **C2** (5): mpfr_set4, mpfr_sub1sp1n, sub1sp2, sub1sp2n, sub1sp3.
  Substantial bigint-extended-precision pattern established for
  multi-limb same-sign arithmetic. (`fe65724`)
- **`engine`**: ralph.py `--ship` mode landed mid-session (atomic
  grade+promote+commit). 31 new pytest cases (52 total in driver).
  (`829ebaa`)
- **C3** (7): mpfr_set, mpfr_sub1sp1, mpfr_powerof2_raw, plus all four
  remaining add1sp variants (1n, 2, 2n, 3). add1sp + sub1sp families
  COMPLETE for prec ≤ 192. (`7c8c0bf`)
- **C-mega** (15): mpfr_swap, init, get_exp, set_exp, check_range, dim,
  round_p, mul_2ui, div_2ui, add_si, add_ui, sub_si, sub_ui, mul_si,
  mul_ui. One opus prep, 15 parallel sonnets, all one-shot 1.0. (`ae2018b`)

## Throughput across the session

| Batch | Functions | Orchestrator actions | Actions/fn | Wall (approx) |
|---|---:|---:|---:|---:|
| Phase B (pilot) | 3 | 6 | 2.0 | ~45 min |
| C1 | 5 | 6 | 1.2 | ~45 min |
| C2 | 5 | 6 | 1.2 | ~40 min |
| C3 | 7 | 6 | 0.86 | ~50 min |
| C-mega | 15 | 5 | **0.33** | ~55 min |

The mega-batch is the breakthrough. 15 functions for 5 orchestrator
actions: `--next`-seeded (or hand-seeded) state.db rows, one opus prep
dispatch, 15 parallel sonnet dispatches in one message, `--grade`,
manual promote-with-path-rewrite + commit. The bottleneck shifted away
from orchestrator commands and toward opus prep time (~45 min for 15
functions) and human review bandwidth.

## What the engine actually looks like (post-session)

```
mpfr-ts/
├── eval/
│   ├── state.db                 # 89 rows: 85 done, 4 blocked
│   └── driver/
│       ├── callgraph.json       # 525 functions, regenerable
│       ├── callgraph.py         # 23 KB, regex extraction from mpfr/src
│       ├── prompts.py           # (unchanged from 005)
│       ├── ralph.py             # +800 LOC (was 140); 5 modes now
│       └── tests/
│           ├── test_callgraph.py  # 14 cases
│           └── test_ralph.py      # 38 cases (21 pre + 17 ship)
```

CLI:
```bash
python3 eval/driver/ralph.py --next [--batch-size N] [--filter class=X] [--include-pending-deps]
python3 eval/driver/ralph.py --grade <fn1> <fn2> ...
python3 eval/driver/ralph.py --commit-batch <msg>
python3 eval/driver/ralph.py --ship --message <msg> <fn1> <fn2> ...
python3 eval/driver/ralph.py --dry-run --function <fn>     # legacy
python3 eval/driver/ralph.py --list-pending                # legacy
```

## Scale-out learnings (the durable ones)

1. **The engine's `--next` picks internals first.** callgraph topo-sort
   puts mpn_*-style and internal helpers at rank 0-30; public API
   functions appear at rank 100+. Strict "deps satisfied" filtering
   keeps the engine in internal-helper territory unless the orchestrator
   hand-seeds a public-API batch. **Implication**: for user-facing
   scaling, hand-pick batches rather than relying on `--next`. The
   `--include-pending-deps` flag helps but doesn't reorder.

2. **Bigger prep batches dominate small ones.** A 15-function opus prep
   takes ~45 min and ~387K tokens; a 5-function prep takes ~25 min and
   ~250K tokens. The fixed-cost overhead (CLAUDE.md re-read, worked
   example load, harness investigation) amortizes. Recommended batch
   size: **15-20 functions per opus prep dispatch**.

3. **Parallel sonnet dispatch in one message works at N=15.** Worklog
   005 capped at 8 parallel agents. Pushed to 15 in this session;
   all 15 ran cleanly, all returned composite=1.0 one-shot. The harness
   coordinates correctly. **N=15+ is empirically safe**.

4. **Sonnet L3 is the right porter for routine work; opus for prep.**
   Sonnet's 100% one-shot rate at composite=1.0 across 35 ports (incl.
   the mega-batch's 15) validates Rule 8's policy. Don't manually
   invoke opus for porting unless escalation triggers.

5. **The bigint-extended-precision pattern generalizes.** sub1sp[2,2n,3]
   in Batch 2 + add1sp[2,2n,3] in Batch 3 all collapsed C's per-limb
   borrow/carry chains into a single bigint op:
   `D = (large.mant << d) ± small.mant`. **All multi-limb same-prec
   arithmetic should use this pattern**; the limb decomposition adds
   ~3× LOC with no correctness benefit in TS.

6. **Mutation-prove danger zone (0.45-0.55) needs multi-bug
   perturbations.** Single-pair swaps (e.g., flip RNDD/RNDU) land near
   the gap edge. Multi-bug or full-rotation perturbations land < 0.30
   reliably. Batch 2's set4/sub1sp1n landed at 0.45-0.48 (acceptable
   but flagged); subsequent batches used multi-bug and got 0.0-0.30.

7. **Sonnet uses absolute paths in /tmp ports; orchestrator must
   rewrite during promote.** `/tmp/eval_<fn>/port.ts` runs from /tmp
   where `/tmp/core.ts` doesn't exist, so sonnet writes
   `/home/tobias/Projects/mpfr-ts/src/core.ts`. On promote to
   `src/ops/<short>.ts` the orchestrator must rewrite to relative
   paths (`../core.ts` etc.) — otherwise the published library has
   absolute paths that break for any other consumer. Handle both
   single- and double-quote import forms in the rewrite.

8. **The engine's `--grade` resolve_port_path only checks `src/ops/`;
   substrate ports at `src/internal/mpfr/<fn>.ts` fall through.**
   Workaround: manual `runner.ts` invocation + manual run-row INSERT.
   Real fix (filed as bd) is to read state.db's class column and
   choose the right canonical path. Surfaced when promoting
   mpfr_powerof2_raw and mpfr_round_p.

9. **The `--ship` mode collapses 3 actions into 1.** `--grade +
   manual cp + --commit-batch` becomes `--ship --message ...`. Drops
   batch action count from 6 to 4. Combined with parallel sonnet
   dispatch (1 action for N agents) and bigger prep batches, the
   mega-batch achieved 5 actions for 15 functions (0.33 actions/fn).

## Open bd issues at session close

P2 — engine improvements:
- `mpfr-ts-akk`: --next pending-deps policy refinement
- `mpfr-ts-710`: --next public-API vs internal-helper priority
- `mpfr-ts-NEW1`: --ship should rewrite absolute paths during promote

P3 — harness improvements:
- `mpfr-ts-79u`: expected_throw codec (unblocks abort_prec_max,
  domain-error variants)
- `mpfr-ts-0t3`: eval/golden_master/run_all.sh wrapper
- `mpfr-ts-7em`: enforce Rule 7 tag minimums at grade time
- `mpfr-ts-i5z`: value_codec scalar-string outputs
- `mpfr-ts-2r8`: --grade substrate-path fallback
- `mpfr-ts-9li`: mpn_* substrate fns not in callgraph (sourced from GMP)
- `mpfr-ts-0dw`: state.db topo_rank vs callgraph topo_rank

P3 — port quality:
- `mpfr-ts-3ka`: runner n_throw conflation (cosmetic)
- `mpfr-ts-6ps`: state.db perf_grade NOT NULL but unused
- `mpfr-ts-upg`: worked-example-eval-leak for function #1 only

P4 — cosmetic:
- `mpfr-ts-5a3`: runner.ts is 1287 LOC
- `mpfr-ts-0pq`: allocator hooks blocked rationale documented
- `mpfr-ts-6s9`: bunfig.toml preload=[] cwd-sensitive
- `mpfr-ts-ggv`: abort_prec_max blocked-tracking

## What the next agent should do

The 85-port library is now a substantial chunk of MPFR. The remaining
~500 functions in callgraph.json split roughly:

- ~75 transcendentals (exp/log/trig/hyper/etc.) — first real test of
  MPFR's prec-extension loop pattern. No existing port exercises it.
  Expect ~5-15 functions per batch, with some failing under sonnet
  alone. May want to enable auto-escalate (sonnet → opus on failure);
  requires a phase transition per CLAUDE.md Rule 14.

- ~100 set/get/conversion variants (set_q, set_str, get_str, get_z, ...)
  — most should be straightforward. mpfr_get_str is the hardest
  (base-conversion algorithm).

- ~50 modular/remainder (fmod, modf, remainder, remquo) — moderate.

- ~250 various: more single-limb fast paths (sqr1, mul_1, etc.),
  predicates, transcendental helpers, advanced ops. Mixed difficulty.

**Recommended next session arc**:
1. Land the engine improvements still pending (P2 bds: especially
   `mpfr-ts-NEW1` --ship path rewrite). Cost: ~1hr.
2. Mega-batch (15-20) of conversion/get/set variants. ~85 → ~100.
3. Mega-batch (10-15) of modular/remainder ops. ~100 → ~115.
4. Transition to Production phase (per Rule 14: write
   `docs/worklog/NNN-phase-transition.md` first).
5. Begin transcendentals with explicit prec-extension prompt template.

## Addendum (end-of-session user directive)

The user has set the next-session target explicitly: **try a 30-function
mega-batch** as the first move. Cost-discipline is the optimization
target, not wall time.

Concrete contract for the next agent:

- **One opus prep dispatch of 30 functions**. This stress-tests the
  empirical ceiling discussed in §"Recommended next moves". Cost
  envelope: ~700K opus tokens / ~75-90 min wall (linear extrapolation
  from the 15-fn batch at 387K / 45 min). If opus hits context
  exhaustion mid-prep, that's the discovery; capture it.
- **Sonnet dispatches in WAVES of 5-10 in parallel, not all 30 at
  once.** The user's framing: spawning all 30 sonnets together risks
  ~5-10 simultaneous failures or near-miss iterations that burn tokens.
  Smaller waves let each wave's signal (one-shot rate, iteration count,
  any prompt-issue patterns) inform the next wave's dispatch.
- **Wave sequencing**: dispatch wave 1 (5-10 sonnets), await all,
  spot-check + --grade the subset. If clean, dispatch wave 2. If any
  fail, diagnose before launching wave 3. Continue until all 30 are
  graded.
- **Then commit + push**. The `--ship` mode (mpfr-ts-9vb) handles the
  atomic commit+push once all 30 are graded. Run it once with all 30
  function names after the final wave completes. If `mpfr-ts-NEW1`
  (--ship path rewrite) has been landed first, --ship handles
  promote-with-rewrite too; otherwise the orchestrator does the manual
  python rewrite as in this session's mega-batch.

**Picking the 30**: bias toward coherent themes (so the opus prep can
share insights across functions). Suggested groupings the next agent
can pick from:

- Conversion family (~15): `mpfr_set_q`, `mpfr_set_str`, `mpfr_get_str`,
  `mpfr_get_z_2exp` already in callgraph but check if not done,
  `mpfr_set_si_2exp`, `mpfr_set_ui_2exp`, `mpfr_get_si_2exp`,
  `mpfr_set_d_2exp` variants, `mpfr_set_f`, `mpfr_get_f`, etc.
- Modular/remainder (~10): `mpfr_fmod`, `mpfr_fmod_ui`, `mpfr_modf`,
  `mpfr_remainder`, `mpfr_remquo`, `mpfr_fmodquo`, etc.
- Division variants (~5): `mpfr_div_si`, `mpfr_div_ui`,
  `mpfr_si_div`, `mpfr_ui_div`, plus the single-limb fast paths
  if pickable.

Or, to maximize coherence-per-batch: pick 30 conversion-family functions
in one batch. The opus prep's "shared subtleties" advantage is highest
when the 30 are structurally similar.

**Why cost-not-time matters**: parallel waves smaller than the
theoretical max have a marginal-time penalty but a real cost upside —
failed sonnets cost full prompt+output tokens with no port produced. A
30-wide simultaneous dispatch with 2-3 failures wastes ~150K sonnet
tokens; 3-6 sequenced waves of 5-10 with a single failure waste ~30K
and the issue can be diagnosed before launching the next wave.

**Empirical headroom check still applies**: if the 30-fn opus prep
exhausts context mid-run, the next iteration is N parallel preps of
~10-15 each — the SDK-direct path worklog 005 deferred.

## File map (post-session)

```
mpfr-ts/
├── CLAUDE.md
├── PHASE.md                          "Pilot" (unchanged)
├── HANDOFF.md                        → still references worklog 005;
│                                      update to point to this file
├── docs/
│   ├── PILOT_PLAN.md
│   ├── worklog/
│   │   ├── 001-step5-harness.md
│   │   ├── 002-pilot-mpn-add-n.md
│   │   ├── 003-pilot-complete.md
│   │   ├── 004-50-ports-complete.md
│   │   ├── 005-scale-out-handoff.md
│   │   └── 006-scale-out-engine.md   ← you are here
│   └── memory/                       (refresh with latest before close)
├── src/
│   ├── core.ts                       LOCKED
│   ├── internal/
│   │   ├── mpn/{4 files}             unchanged
│   │   └── mpfr/{round_raw, cmp_raw, powerof2_raw2, powerof2_raw, round_p}.ts
│   └── ops/                          81 public ports (43→44→ ... → 81)
└── eval/
    ├── state.db                      89 rows / 85 done / 4 blocked
    ├── driver/
    │   ├── callgraph.json
    │   ├── callgraph.py              ✚ this session
    │   ├── ralph.py                  ✚ +800 LOC this session
    │   ├── prompts.py
    │   ├── schema.sql
    │   └── tests/
    │       ├── test_callgraph.py     ✚ this session
    │       └── test_ralph.py         ✚ +449 LOC this session
    ├── harness/
    │   ├── ast_check.ts              ✚ import-strip fix this session
    │   ├── ast_check.test.ts         ✚ this session
    │   └── ...
    └── functions/<fn>/               89 dirs total
```
