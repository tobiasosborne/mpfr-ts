# 016 — mutate.py carve-out: 'low-confidence-pass' for thin-surface ports

> Picks up from HANDOFF.md Priority 1 (bd `mpfr-ts-9di`). One focused
> harness patch resolves the applied-but-survived noise across the
> 7 known cases.

## TL;DR

A 4th `gate_status` value, `'low-confidence-pass'`, was added to
`eval/driver/mutate.py`. It carves out the pure-dispatch /
pure-delegation pattern that systematically defeats the existing
mutator surface. Re-grading the 7 known applied-but-survived cases
flipped 5 to the new bucket; 2 deliberately stayed `'survived'`
(the carve-out predicate is structural, not lenient).

| Function | Applied (non-init-failed) | Composites | Before | After |
|---|---:|---|---|---|
| `mpfr_sqrt1`   | 2 | 1.0000, 1.0000           | survived | low-confidence-pass |
| `mpfr_set_inf` | 2 | 1.0000, 1.0000           | survived | low-confidence-pass |
| `mpfr_get_d1`  | 1 | 1.0000                   | survived | low-confidence-pass |
| `mpn_copyi`    | 1 | 1.0000                   | survived | low-confidence-pass |
| `mpn_copyd`    | 1 | 1.0000                   | survived | low-confidence-pass |
| `mpn_zero`     | 1 | 0.9783                   | survived | **survived** (composite < 0.99 floor; real perturbation, mutators almost caught it) |
| `mpfr_sub1sp`  | 3 | 1.0000, 0.9639, 0.9944   | survived | **survived** (count > 2 threshold; 3 mutator families fired = genuine algorithmic surface) |

State.db: 138 done, 17 blocked, 4 pending (unchanged -- this is a
harness change, no ports shipped or moved).

## What changed

`eval/driver/mutate.py` gained a 4th `gate_status` value,
`'low-confidence-pass'`, gated by a strict predicate: >=1 mutation
applied, zero below-threshold, count(applied AND not module_init_failed)
<= 2, and ALL such mutations have `composite > 0.99` (strict). Two
module-level constants (`_LOW_CONFIDENCE_APPLIED_MAX`,
`_LOW_CONFIDENCE_COMPOSITE_FLOOR`) sit beside the existing
`BELOW_THRESHOLD` at L27. `_aggregate_gate` was refactored to delegate
to `_gate_status` so the two cannot drift. Net diff: 33 LOC in
mutate.py (51 added, 18 deleted); 34 LOC added to tests.

## TDD spec-hole catch

The HANDOFF's literal predicate was "all applied non-init-failed
mutations scored >= 0.95". Implementing it literally broke 2 pre-existing
tests:

- `test_gate_passes_with_one_below_threshold` (L142): outcomes
  `[(0.99, False, False), (None, False, False)]` -- 2 applied, one
  ungraded; would falsely carve.
- `test_gate_status_survived_when_mutations_applied_but_none_below`
  (L175): outcomes `[(0.99, False, False), (0.97, False, False)]` --
  2 applied with real composite movement; would falsely carve.

Refined predicate `composite > 0.99 (strict)` resolves both:
composite=0.99 fails the `> 0.99` check, stays `'survived'`. The
semantic shift: "low-confidence-pass" means "mutators essentially
didn't dent the composite" (thin surface), not "didn't kill" (which
is what `'survived'` is for).

This was caught by RED-first TDD: the first GREEN-attempt subagent
ran tests, saw the breakage, stopped and reported back rather than
pushing through. The HANDOFF spec was imprecise; the test contract
was the real spec.

## Frictions

1. **Stale DB port_paths.** For `mpfr_set_inf` and `mpfr_get_d1`,
   `state.db` recorded `port_path` as `/tmp/eval_<fn>/port.ts`
   (vanished). Workaround during Phase 1 baseline: substituted
   `src/ops/<base>.ts`. Should file a follow-up bd ticket: `state.db`
   ought to record the persistent path, not a tmpdir.

2. **bd issue title stale.** `mpfr-ts-9di` title still reads "gate
   must pass trivial-body ports (no applicable mutations)" -- that's
   the vacuous case which was shipped previously. This work is the
   applied-but-survived case (the HANDOFF reframing). Title will be
   updated at close time.

## Porter-facing guidance

A port flagged `gate_status='low-confidence-pass'` is shipped; it
does NOT need a fix. The signal means "current mutators couldn't
grip this port's surface" -- common for pure dispatchers, sign-only
flips, and trivial delegations. Do NOT add dead code to satisfy the
gate (CLAUDE.md feedback_no_mutator_bait). When future stronger
mutators land, these ports may flip to `'killed'` (good) or
`'survived'` (then they need stronger goldens). The carve-out is
structural, not a porter opt-in.

## Acceptance

- 23/23 tests/test_mutate.py pass (was 20 + 3 RED).
- 123/123 tests/ pass (was 119 + 4).
- 5/7 known applied-but-survived cases flipped to
  `'low-confidence-pass'`; 2/7 stayed `'survived'` as predicted.
- Diff is 33 net LOC in mutate.py (51 add, 18 del); 34 LOC added to
  test_mutate.py.
- No port-file edits; no `src/` touches.
- ASCII-only in additions; pre-existing em-dashes untouched.

## Pointers

- `eval/driver/mutate.py` L28-L33 (constants), L65-L75 (refactored
  `_aggregate_gate`), L77-L103 (refactored `_gate_status`)
- `eval/driver/tests/test_mutate.py` (4 new tests appended)
- HANDOFF.md "Priority 1" section (spec; superseded by this worklog's
  refined predicate)
- bd `mpfr-ts-9di` (will be closed)
