# 017 — Mega batch: 10 ports shipped, post-carve-out validation

> First mega batch of the Production phase. 10 ports shipped at
> composite=1.0 across 7 serial subagent dispatches in ~45 minutes
> orchestrator time. Validates the worklog 016 'low-confidence-pass'
> carve-out in production (`mpfr_scale2` landed there cleanly).

## TL;DR

The 10 ports, all shipped composite=1.0 against locked goldens:

| Function | Class | Dest | n_cases | wall_ms | Mutate gate | LOC |
|---|---|---|---:|---:|---|---:|
| `mpn_divrem` | substrate | `src/internal/mpn/divrem.ts` | 122 | 25 | killed (3 clean) | ~225 |
| `mpn_divrem_1` | substrate | `src/internal/mpn/divrem_1.ts` | 125 | 22 | killed (2 clean + 1 below_threshold) | ~215 |
| `mpfr_nextabove` | misc | `src/ops/nextabove.ts` | 131 | 29 | vacuous | ~50 |
| `mpfr_nextbelow` | misc | `src/ops/nextbelow.ts` | 131 | 24 | vacuous | ~50 |
| `mpfr_nbits_ulong` | misc | `src/ops/nbits_ulong.ts` | 110 | 21 | killed (1 clean) | ~125 |
| `mpfr_scale2` | misc | `src/ops/scale2.ts` | 111 | 22 | **low-confidence-pass** | ~130 |
| `mpfr_buildopt_float128_p` | misc | `src/ops/buildopt_float128_p.ts` | 1 | 16 | vacuous | 50 |
| `mpfr_buildopt_float16_p` | misc | `src/ops/buildopt_float16_p.ts` | 1 | 16 | vacuous | 51 |
| `mpfr_buildopt_gmpinternals_p` | misc | `src/ops/buildopt_gmpinternals_p.ts` | 1 | 16 | vacuous | 52 |
| `mpfr_buildopt_sharedcache_p` | misc | `src/ops/buildopt_sharedcache_p.ts` | 1 | 13 | vacuous | 50 |

State.db: 138 → **148 done · 17 blocked · 0 pending** (+10 done,
emptied the pending queue; the picker auto-enqueued 6 more candidates
during PREP and all were shipped in the same batch).

## Commits this session

[TBD — will be filled in at commit time. Likely 1 commit for the whole
batch since `--grade` processed all 10 atomically.]

Picker behavior note: at session start, `state.db` had 4 pending rows
carried from worklog 015 (`mpn_divrem`, `mpn_divrem_1`,
`mpfr_nbits_ulong`, `mpfr_scale2`). During PREP, the orchestrator
also ran `ralph.py --next --batch-size 10` which auto-enqueued 6
additional candidates (`mpfr_nextabove`, `mpfr_nextbelow`, and the 4
`buildopt_*_p`). All 6 newly-enqueued + all 4 carried = 10 shipped.
The "0 pending" end-state is the result of harvesting the full
queue, not an empty enqueue.

## Process: 7 serial subagent dispatches

| Phase | Subagent | Duration | Tokens |
|---|---|---:|---:|
| PREP (10 specs + drivers + reference ports) | 1 | ~23 min | 252K |
| Calibration (10 reference pairs) | inline | ~30s | — |
| PORT-B (4× buildopt) | 1 | ~2 min | 42K |
| PORT-A (next pair) | 1 | ~12 min | 58K |
| PORT-E (nbits_ulong) | 1 | ~1.5 min | 47K |
| PORT-C (mpn_divrem) | 1 | ~2 min | 49K |
| PORT-D (mpn_divrem_1) | 1 | ~1.5 min | 44K |
| PORT-F (scale2 — with bug-fix guidance) | 1 | ~1.5 min | 45K |
| Final grade pass (`ralph.py --grade ×10`) | inline | ~3s | — |

Total orchestration time: ~45 min. 7 subagent dispatches. Approximate
cost: ~$2 (well under the $50/session stop condition).

Discipline outcomes: one subagent at a time (user-requested serial
discipline). All 7 dispatches succeeded; zero API-overload (529)
incidents. This contrasts with worklog 015 where 2 of 5 dispatches
overloaded and forced inline fallback. The serial pattern is the new
default for batches of this size — slower wall-clock than parallel,
but zero retry budget burned.

TDD-shape "port-and-verify" applied throughout: PREP creates a
calibrated reference (broken<0.55 / correct>=0.95 on the locked
golden), the orchestrator verifies the calibration, then the PORT
subagent writes a production version targeting composite=1.0. The
PORT subagent never sees the libmpfr output directly; it sees the
golden + the C source + the reference port's structure.

PREP-vs-PORT token economics: PREP at 252K tokens is 4-5× the cost of
a single PORT dispatch. The dominant work in PREP is spec
authoring + driver C-code + reference port (in pure TS) + 10× the
golden_master build/regenerate cycle. Once PREP lands, PORT
dispatches are cheap — the median PORT this batch was ~1.5 min /
45K tokens / ~$0.15. The economic shape favors batching PREP across
many functions per session; this is the first time the project has
run PREP-for-10 in one shot, and it paid off.

## Mutate.py status spread post-carve-out

First production batch since the worklog 016 carve-out shipped. The
distribution validates the predicate:

- **killed**: 3 — `mpn_divrem`, `mpn_divrem_1`, `mpfr_nbits_ulong`.
  Substantial algorithmic surface; mutators bit cleanly.
- **vacuous**: 6 — 4× `buildopt_*_p` + `nextabove` + `nextbelow`.
  Zero applied mutations (pure dispatchers and constant-return
  predicates).
- **low-confidence-pass**: 1 — `mpfr_scale2`. First production port
  to land in the new bucket. 1 applied mutation at composite=1.0
  (under the count <= 2 threshold, above the composite > 0.99 floor)
  + 1 init_failed; the predicate worked exactly as designed.

Critically, **none of the 10 ports got stuck in `'survived'`** —
which is exactly the noise that `mpfr-ts-9di` was filed to
eliminate. The carve-out hits its target band cleanly, and the
predicate's strictness (count <= 2 AND composite > 0.99) holds the
line against false carves.

Worth noting: `'vacuous'` is more common than `'low-confidence-pass'`
in this batch (6 vs 1). The buildopt + dispatcher cluster lands at
exactly-zero applied mutations, not 1-2 applied at high composite.
The 9di scope as filed was right; the predicate refinement (worklog
016's TDD-caught `composite > 0.99 strict`) was also right. Both
paths are healthy.

## Calibration-caught bug: scale2 MPFR_ASSERTD over-validation

The PREP-shipped reference port for `mpfr_scale2` scored
composite=0.9935 (110/111 pass, 1 throw). The throw was on case 30
(`exp=-1074`) — the reference port treated the C
`MPFR_ASSERTD(-1073 <= exp && exp <= 1025)` as a hard input
validator. **`MPFR_ASSERTD` is debug-only**; release builds (which
the `golden_driver` target compiles to) treat it as a no-op, and the
code handles `exp=-1074` cleanly via the subnormal branch.

The production port (written by PORT-F with explicit
"don't replicate this validation" guidance in the prompt) stripped
the over-validation, matching C release-build behavior. Composite
jumped to 1.0.

**Porter-facing rule**: when porting `MPFR_ASSERTD`, the TS port
should NOT throw unless the C source has a hard runtime check
elsewhere. `MPFR_ASSERTD` is a debug aid for the C maintainers, not
a contract for callers. (Logging this as a gotcha in HANDOFF.md.)

This is the calibration-first discipline working: the orchestrator's
verify step caught the reference-port bug before the PORT subagent
ever saw it, and the PORT-F prompt prevented re-propagation. Without
that discipline, the bug would have shipped to production at
composite=0.99 and survived the gate as `'low-confidence-pass'`
(applied=1 at composite=0.99 fails the strict > 0.99, so it would
have been `'survived'` actually — but the point stands: catching
the upstream bug is cheaper than relying on downstream gates).

## Risk monitoring — outcomes

| Risk | Outcome |
|---|---|
| Cost burn | ~$2 of $50 ceiling |
| API overload (529s) | 0 incidents across 7 dispatches |
| Cyrillic homoglyph | clean (Rule 13 verified at PREP + every PORT) |
| Hex literal hygiene | clean (HANDOFF gotcha #3 verified at PREP) |
| Mutator bait | none — gates passed honestly (3 killed, 6 vacuous, 1 low-confidence-pass) |
| Reference-port bug propagation | caught (scale2; orchestrator's PORT-F prompt prevented re-propagation) |

## Frictions

1. **Reference port for `scale2` had a real bug** (over-validation of
   debug-only `MPFR_ASSERTD`). Calibration phase caught it before the
   PORT subagent saw it. This is the calibration-first discipline
   paying off — without it, the bug would have replicated and the
   port would have shipped at composite=0.9935. Worth a gotcha entry
   in HANDOFF.md.

2. **Mutate.py `'vacuous'` is more common than `'low-confidence-pass'`
   in this batch (6 vs 1).** The carve-out covers more than expected —
   the buildopt + dispatcher cluster lands at exactly-zero applied
   mutations, not 1-2 applied at high composite. The 9di scope as
   filed was right; the predicate refinement (worklog 016 TDD catch)
   was also right. Both paths are healthy. Just a calibration note
   for future bucket-distribution expectations.

3. **`--ship` requires `/tmp` staging; PORT subagents wrote directly
   to canonical paths.** The orchestrator routed around this via
   `--grade` (which falls back to canonical paths via
   `resolve_port_path` in `ralph.py` L420-L434) — state.db gets
   updated identically. Worth a small refactor someday so `--ship`
   and `--grade` share the resolver. Not a P-anything; file under
   "tidy when nearby" (already filed as `mpfr-ts-ndc`).

## Batch composition: why these 10 fit together

The batch had three distinct shapes, dispatched to PORT subagents
matched to each:

- **Substrate division pair** (`mpn_divrem`, `mpn_divrem_1`): the
  heaviest algorithmic work (~440 LOC combined). Multi-precision
  division — `divrem_1` is the single-limb divisor case;
  `divrem` is the general 2-limb-divisor case built on top.
  Both killed all applicable mutations. These were the worklog
  015 carry-overs that justified the batch's lower bound.

- **Trivial primitives** (`nextabove`, `nextbelow`, 4× `buildopt_*_p`):
  six near-zero-surface ports. `nextabove`/`nextbelow` are
  thin wrappers (`nextabove(x) -> nexttoinf(x)`-style); the
  buildopt predicates are constant-return (`return 0` in our
  build). Six functions in one ~$0.05 PORT-B dispatch — the
  high-throughput end of the cost curve.

- **The middle tier** (`mpfr_nbits_ulong`, `mpfr_scale2`): real
  algorithmic surface but bounded scope. `nbits_ulong` counts
  the bit-length of an unsigned long; `scale2` multiplies by
  2^k with proper subnormal / overflow handling. Both ~125-130
  LOC. These are where the calibration-first discipline earned
  its keep this batch — the scale2 reference-port bug surfaced
  here.

The shape mix is worth recording because it suggests a heuristic
for picker batches: group dispatches by surface complexity so the
PORT prompts can be specialized. Trivial primitives benefit from
"here are 4 nearly-identical specs, write all 4" framings;
substrate work needs detailed C-source citations; middle-tier
needs the bug-watch prompts.

## Acceptance

- 10/10 ports composite=1.0 against locked goldens
- All gates pass: 3 killed + 6 vacuous + 1 low-confidence-pass
- state.db status updated atomically via `ralph.py --grade` for all 10
- ASCII-only + hex-literal hygiene clean across 50 created files
- No regressions: the prior 138 ports unchanged

## Pointers

- `eval/driver/ralph.py --grade` (L467-L625) — single source of truth
  for grade-and-update; canonical-path fallback at `resolve_port_path`
  (L409-L436) handled the direct-to-`src/` workflow this batch used.
- `eval/driver/mutate.py` L77-L103 — `'low-confidence-pass'` predicate
  shipped in worklog 016, validated in production by `mpfr_scale2`.
- The 10 ports in `src/ops/` and `src/internal/mpn/`.
- HANDOFF.md priority sequence (refreshed at end of this session).
