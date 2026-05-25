# 011 — Pilot → Production phase transition + ADR 0002 + mutate.py vacuous carve-out

> Picks up from worklog 010 / HANDOFF.md. Closes the three priorities
> queued for the next session: (1) ADR 0002 resolving the inequality-
> grader framing surfaced by shadow trial 2; (2) mutate.py gains a
> vacuous-pass carve-out for trivial-body ports (bd `mpfr-ts-9di`
> option (a)); (3) PHASE.md flips from `Pilot` to `Production` per
> Rule 14 — Pilot exit criterion (PIL.5) was exceeded 4+ sessions ago.

## TL;DR

| Deliverable | Path | Δ |
|---|---|---:|
| ADR 0002 — approximation-helper grading policy | `docs/adr/0002-approximation-helper-grading.md` | +180 |
| `mpfr_sqrt2_approx` spec rewrite (cites ADR 0002) | `eval/functions/mpfr_sqrt2_approx/spec.json` | refactor |
| mutate.py vacuous-pass carve-out | `eval/driver/mutate.py` | +30 |
| New mutate.py tests (5 cases) | `eval/driver/tests/test_mutate.py` | +60 |
| Phase transition worklog (this file) | `docs/worklog/011-phase-transition.md` | new |
| PHASE.md flipped | `PHASE.md` | `Pilot` → `Production` |
| bd `mpfr-ts-52u` closed (ADR 0002) | beads | — |
| bd `mpfr-ts-9di` notes (option (a) shipped, partial closure) | beads | — |

Live evidence backing each change:

- **ADR 0002**: `mpfr_div2_approx` already at composite=1.0, 129/129
  cases under strict-equality grading. The inequality-grader framing
  from HANDOFF was a misread; the architecture already had a cleaner
  answer (golden-driver-substitute pattern) shipping in production.
- **mutate.py carve-out**: `mpfr_swap` (pure-delegation port) now
  reports `gate_passed=True (vacuous)` — previously was `False`.
  119/119 driver pytest pass (was 114 + 5 new).
- **Phase transition**: state.db at session start was 122 done /
  5 blocked / 2 pending; Pilot exit criterion (PIL.5: 10 functions
  composite≥0.95 in a clean run, mutation-proven goldens) exceeded
  12× over. ADR 0001 held 17/17 across 2 shadow trials. Continuing
  to hold `Pilot` was bookkeeping drift.

## Priority 1 — ADR 0002 (`mpfr-ts-52u` → closed)

### The framing gap

HANDOFF queued P1 as "extend the grader to accept inequality outputs,
either via wire-format `output_range: [lo, hi]` (option a), or by
parking all approximation helpers via ADR (option b), or by
always-delegating (option c)". The framing assumed `mpfr_sqrt2_approx`
was parked because its C contract is an inequality (`{rp,2}-4 ≤
floor(sqrt(...)) ≤ {rp,2}+26`), and the grader uses strict equality.

Investigation of the already-shipped `mpfr_div2_approx`
(composite=1.0, 129 cases, `r2026-05-24T12:40:42Z`) revealed the
architecture's actual answer.

### The golden-driver-substitute pattern (already in production)

`mpfr/src/div.c` `mpfr_div2_approx` calls
`__gmpfr_invert_limb_approx` — a non-portable LUT-backed inverse.
`eval/functions/mpfr_div2_approx/golden_driver.c` *does not* link
libmpfr's `mpfr_div2_approx`. Instead it re-derives the algorithm
verbatim using a portable substitute for `invert_limb_approx`:

```c
/* num = (B-1)*B - v;  return (uint64_t)(num / v); */
```

The golden is emitted from this portable algorithm. The TS port
(`src/internal/mpfr/div2_approx.ts`) mirrors the substitute
step-by-step. Both the golden and the TS port derive from the same
portable algorithm — strict equality holds by construction, and the
inequality contract is an *invariant of the algorithm*, not a runtime
assertion.

This generalizes: for any C helper blocked on a non-portable
primitive (LUT, ASM, intrinsic), `golden_driver.c` substitutes a
portable equivalent; the TS port mirrors it; grading stays equality-
based.

### `mpfr_sqrt2_approx` parking refresh

The parked spec listed three reasons. ADR 0002 retires reason (2)
(inequality contract); (1) and (3) jointly justify parking:

1. **No public-API caller.** `src/ops/sqrt.ts` uses bigint `isqrt`
   across all precisions; `mpfr_sqrt2` is not selected for porting.
   Porting `sqrt2_approx` standalone produces dead code.
3. **Wire-form misalignment.** Raw 2-limb `mpfr_limb_ptr` I/O
   contract; the substitute pattern could express this, but only
   carries weight once (1) resolves.

ADR 0002 §Decision formalizes:

> Park if and only if (i) no public-API caller, OR (ii) wire-form
> intractability after substrate carve-out. Inequality contract
> alone is NOT a parking reason — the golden-driver-substitute
> pattern handles it.

### What did NOT ship

The wire-format extension (option (a) from HANDOFF) was *not* built.
YAGNI applies: zero divergent-algorithm ports exist today, the
substrate discipline pushes new approximation helpers toward
substitute-pattern faithful ports, and CLAUDE.md system rules
("don't design for hypothetical future requirements") + Law 4
("library composes") both reinforce the smaller architecture.
ADR 0002 §Revisit documents the exact conditions that would
re-open the wire-format question.

### Acceptance

- `docs/adr/0002-approximation-helper-grading.md` shipped.
- `eval/functions/mpfr_sqrt2_approx/spec.json` rewritten: reason (2)
  deleted; ADR 0002 cited; refs updated to point at the live
  golden-driver-substitute reference (`mpfr_div2_approx`).
- bd `mpfr-ts-52u` closed with full rationale string.
- No code changes in `eval/harness/runner.ts`, `value_codec.ts`,
  `ast_check.ts`, `mutate.py`, or `gen_spec.py` — the ADR is the
  deliverable.

## Priority 2 — mutate.py vacuous-pass carve-out (`mpfr-ts-9di` → partial)

### What changed

`eval/driver/mutate.py`:

- `MutationOutcome` gains `applied: bool = True`. Set to `False` when
  `mutators.ts apply` returns exit 3 (not applicable).
- `ProveResult` gains `gate_status: str = 'killed'` — one of
  `'killed'` / `'vacuous'` / `'survived'`.
- `_aggregate_gate` returns `True` when either (a) some applied
  mutation drove composite ≤ 0.95 — the load-bearing PIL.3 case;
  or (b) no mutation was applicable to this port at all — the
  vacuous carve-out.
- New `_gate_status` returns the finer-grained classification.
- `main()` prints `gate_passed: True (vacuous)` / `(killed)` /
  `(survived)` for human readers.

The carve-out is conservative: vacuous fires *only* when zero
mutations were applicable (`mutators.ts list` returned empty, OR
every listed mutation returned exit 3). Mutations that applied but
init-failed do *not* trigger vacuous — they're a harness bug, not
a port property; status='survived'. Mutations that applied and
timed out also don't trigger vacuous — same reasoning.

### Live verification

| Port | Mutations applied | Pre-fix gate | Post-fix gate | Status |
|---|---:|:---:|:---:|---|
| `mpfr_swap` | 0 | ✗ FAIL | ✓ PASS | vacuous (new!) |
| `mpfr_sqrt1` | 2 | ✗ FAIL | ✗ FAIL | survived |
| `mpfr_sqrt1n` | 2 | ✓ PASS | ✓ PASS | killed |
| `mpfr_set_inf` | 2 | ✗ FAIL | ✗ FAIL | survived |
| `mpfr_get_d1` | 1 | ✗ FAIL | ✗ FAIL | survived |
| `mpfr_add_d` | 5 | ✓ PASS | ✓ PASS | killed |
| `mpfr_sqr_2` | 6 | ✓ PASS | ✓ PASS | killed |

`mpfr_swap` is the canonical example: body is `return { a: b, b: a }`,
zero mutator surface, correct-by-construction. Previously the gate
falsely flagged it as a port failure; now it passes vacuously.

### Why mpfr-ts-9di is only partially closed

The bd description listed three patterns: zero-applicable, 2-
applicable-all-survived, 1-applicable-survived. HANDOFF noted
"Option (a) is the simplest. Option (b) provides better signal."
Option (a) — what shipped — addresses *only* the zero-applicable
case.

Applied-but-survived cases (sqrt1, set_inf, get_d1 above) remain
`gate_passed=False, gate_status='survived'`. This is the *correct*
classification — these ports have algorithmic surface (guards,
delegation calls, dispatch logic), but the golden's coverage is on
the happy path and mutations of guard-paths don't break it. The
'survived' status surfaces "golden insufficient on this port",
which is actionable info for the curator.

bd `mpfr-ts-9di` notes record this partial closure; the issue stays
open for a future option (b) (complexity floor) or (c) (spec-level
exempt flag), to be picked up if live impact justifies. None of the
three survived-status ports are blocking Production scale-out.

### Tests

- 14 existing test cases preserved (backward compat via field
  defaults on `applied` and `gate_status`).
- 5 new test cases:
  - `test_gate_vacuous_when_no_outcomes` — empty outcomes list →
    True, status='vacuous'.
  - `test_gate_vacuous_when_all_mutations_inapplicable` — every
    mutation `applied=False` → vacuous.
  - `test_gate_status_killed` — happy path.
  - `test_gate_status_survived_when_mutations_applied_but_none_below`
    — sqrt1-shape case.
  - `test_gate_status_survived_when_only_init_failures` — verifies
    init-fail does NOT trigger vacuous (it's a harness bug, gate
    fails).

Driver pytest: 119 passed (was 114). 1.98s.

### Acceptance

- Live `mpfr_swap` mutation_prove now exits 0.
- Live `mpfr_sqrt1n` still passes (killed); `mpfr_sqrt1`,
  `mpfr_set_inf`, `mpfr_get_d1` still fail (survived).
- 119/119 pytest pass.
- bd `mpfr-ts-9di` notes updated.

## Priority 3 — Pilot → Production transition (Rule 14)

### What Pilot proved (and what was overdue)

Pilot's exit criterion (CLAUDE.md PIL.5): **10 pilot functions with
composite≥0.95 in a single clean ralph-loop run, each golden
mutation-proven, state.db populated.** Achieved several sessions ago;
the project has been operating at ~120-function scale with the same
discipline ever since.

Specifically the Pilot proved out:

1. **Worker-isolated grader (Rule 4)** is stable at scale. Two shadow
   trials (8 functions) and ~120 production ports have shipped with
   per-test workers; no timeout-related corruption, no inter-test
   leak.
2. **Sonnet L3 is the right default (Rule 8)**. Opus L3 escalation
   has fired sparingly and resolved cleanly in the cases it has.
   Cost discipline holds.
3. **State DB is sufficient (Rule 9)**. SQLite-as-truth has not
   surfaced cross-session confusion or stale state.
4. **Golden-driver substitute pattern works** (ADR 0002, above) —
   even for the apparently-hardest case (inequality contracts),
   the discipline of "TS port mirrors a portable C substitute"
   gives bit-exact equality grading.
5. **ADR 0001 holds**. 17/17 prediction rate across 2 shadow
   trials. Validated under live load.
6. **Shadow trials surface architectural gaps cheaply**. Each trial
   cost ~250K tokens and surfaced a major finding that would
   otherwise hit a 10+ function batch at full cost.

### Auto-escalate caveats remaining

Production exit criteria (CLAUDE.md §Phase awareness) call out:

- Cost burn cap: $50/session. Honored by shadow-trial cost
  signature (~250K tokens ≈ $5).
- Auto-escalate failure rate cap: 10%/24h. Has not been measured at
  Production scale; first ~5 production mega-batches should
  instrument this directly.

Known limitations carried into Production:

- **`mutate.py` applied-but-survived cases** (bd `mpfr-ts-9di`
  partial closure above): when a port has guard-path mutations but
  no happy-path mutations, gate reports `survived`. Curator must
  visually verify; this is *signal*, not noise, but it requires
  human review. Affects <5 ports out of ~600.
- **Approximation helpers may park** (ADR 0002 §Decision criterion):
  helpers without a public-API caller in the unified TS surface
  will park rather than ship as dead code. State.db captures this
  cleanly; the dashboard query `WHERE status='blocked'` surfaces
  them.

### Phase transition

- `PHASE.md`: `Pilot` → `Production`.
- This worklog (`docs/worklog/011-phase-transition.md`) is the
  required Rule 14 deliverable.
- Ralph loop dispatches will now run with `--phase production
  --parallel 8 --auto-escalate` rather than `--phase pilot
  --halt-on-failure`. The first Production mega-batch is the
  natural next session.

### Acceptance

- `cat PHASE.md` → `Production`.
- `docs/worklog/011-phase-transition.md` exists.
- No other code or schema changes — the transition is a single-
  word file edit + this worklog.

## State DB at end of session

```sql
SELECT status, COUNT(*) FROM functions GROUP BY status;
-- blocked|5
-- done|122
-- pending|2
```

Unchanged. (P1+P2 were architectural/harness work, not new ports.)

## bd at end of session

- `mpfr-ts-52u` — closed (ADR 0002 supersedes).
- `mpfr-ts-9di` — open with notes (option (a) shipped, applied-but-
  survived case deferred).
- `mpfr-ts-i8e` (git pre-commit auto-export) — open, unrelated to
  this session.
- All other P3/P4 from HANDOFF — open, unrelated to this session.

## Pointers

- `docs/adr/0001-spec-merge-policy.md` — sibling ADR, same shape.
- `docs/adr/0002-approximation-helper-grading.md` — the ADR shipped
  this session.
- `eval/functions/mpfr_div2_approx/golden_driver.c` — live reference
  for the golden-driver-substitute pattern.
- `src/internal/mpfr/div2_approx.ts` — live reference for the TS
  port style.
- `eval/driver/mutate.py` — the vacuous carve-out.
- `eval/driver/tests/test_mutate.py` — 5 new test cases.
- CLAUDE.md Rule 14 — phase transition discipline.
- CLAUDE.md PIL.5 — Pilot exit criterion.

## Next session

HANDOFF after this session should re-rank priorities given:

- ADR 0002 closes the inequality framing entirely; do not revive
  unless a concrete divergent-algorithm port lands.
- `mpfr-ts-9di` is partially closed; defer the remainder unless
  applied-but-survived gate failures start blocking Production
  ships.
- **Production scale-out is the natural next focus**. State.db has
  2 pending functions (`mpfr_frac` rank 198, `mpfr_rint_trunc` rank
  420) sitting from HANDOFF Priority 3; the first Production
  mega-batch can pick up from there and instrument the auto-
  escalate-rate caveat above.
