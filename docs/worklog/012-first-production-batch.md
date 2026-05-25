# 012 — First Production mega-batch: mpfr_frac + mpfr_rint_trunc

> Picks up from worklog 011 (Pilot -> Production transition). First
> batch run under the new phase, dispatched serially under direct
> orchestration (not via `ralph.py --phase production` autonomous
> loop) so that risk monitoring (cost burn, escalation rate, Cyrillic
> homoglyphs, dead-code gaming, library coherence) could be applied
> per-step. Two functions shipped, both green first attempt.

## TL;DR

| Function | Class | Deliverable | Composite | Mutate gate | LOC |
|---|---|---|---:|:---:|---:|
| `mpfr_frac` | misc | spec + driver + port | 1.00 | killed (op-swap 0.47) | 40 |
| `mpfr_rint_trunc` | misc | spec + driver + port | 1.00 | killed (comparison-swap 0.88) | 38 |

State.db: **124 done, 5 blocked, 0 pending** (was 122 / 5 / 2). Both
pending rows from worklog 010 / HANDOFF cleared.

Two commits (`6f64002`, `9900c27`), all artifacts shipped:

```
docs/worklog/012-first-production-batch.md
eval/functions/mpfr_frac/{spec.json, golden_driver.c}
eval/functions/mpfr_rint_trunc/{spec.json, golden_driver.c}
src/ops/{frac.ts, rint_trunc.ts}
eval/state.db                                 -- 2 runs inserted
```

## Process: serial orchestrator + per-step subagent dispatch

The batch was driven manually rather than via `ralph.py --phase
production --parallel 8`. The orchestrator (this session) ran each
function through the canonical port flow as a TaskCreate sequence,
delegating only the **code-generation** steps to subagents:

| Step | Owner | Notes |
|---|---|---|
| 0.1 Read C sources, confirm deps | orchestrator | direct reads, no subagent |
| A.1 / B.1 Spec generation + curation | orchestrator | `gen_spec.extract_spec` + inline curation per ADR 0001 |
| A.2 / B.2 Golden driver `.c` | **sonnet subagent** | per-fn dispatch; the structural model is the previous golden_driver.c |
| A.3 / B.3 Build + generate golden + verify tags + Cyrillic | orchestrator | direct bash |
| A.4 / B.4 TS port | **sonnet subagent** | per-fn dispatch |
| A.5 / B.5 Grade + verify | orchestrator | independent re-grade (Law 2) |
| A.6 / B.6 Mutation-prove | orchestrator | gate must be killed or vacuous |
| A.7 / B.7 State.db update + commit | orchestrator | direct SQL + git |

**Why serial.** The first Production batch is the place to measure
cost burn and auto-escalate rate per CLAUDE.md Production caveats.
Running serially keeps both visible in real time. After this batch
de-risks the discipline, future batches can shift to `ralph.py
--parallel 8` for throughput.

## Risk monitoring — outcomes

### Cost burn

| Subagent dispatch | Tokens | ~Cost (sonnet @ $3/MTok) |
|---|---:|---:|
| frac golden_driver | 89K | $0.27 |
| frac TS port (attempt 1 socket-dropped) | 0 | $0 |
| frac TS port (retry, green) | 57K | $0.17 |
| rint_trunc golden_driver | 92K | $0.28 |
| rint_trunc TS port | 53K | $0.16 |
| **Total subagent cost** | **291K** | **~$0.88** |

Orchestrator (this session) costs are separate but bounded by the
context window; not measured precisely here. CLAUDE.md cap is $50/
session — we are nowhere near it.

**Caveat:** n=2 is far too small to draw structural conclusions
about Production-batch cost. The per-function cost (~$0.40 for the
driver + port pair) suggests a 10-function batch would burn ~$4-5
on subagent dispatches alone — comfortably below cap. Larger batches
should re-measure since some transcendentals may need 2-3 attempts.

### Auto-escalate rate

**0 escalations / 2 functions = 0%.** Both ports were green on the
sonnet first attempt; opus was never invoked. The 10%/24h cap is
not stress-tested by this batch. Both functions were delegation-
pattern misc-class — the easiest possible Production candidates.
The next mega-batch should include at least one transcendental or
substrate-class function to genuinely exercise the escalate path.

**One transport-level retry was needed**: the frac TS port subagent
disconnected at ~98s on its first dispatch (socket close, 0 tokens
produced). A fresh dispatch with a slightly trimmed prompt succeeded
in 76s. This is NOT an auto-escalate signal — it's a network/
transport issue. Logged here so the next session knows to expect
occasional sub-2-minute drops on `general-purpose` Agent calls.

### Cyrillic homoglyph (Rule 13)

Every artifact ASCII-checked at write time:

```
grep -cP '[\x{0400}-\x{04FF}\x{0370}-\x{03FF}]' <file>
```

All 6 generated files (2 spec.json + 2 golden_driver.c + 2 port.ts)
returned 0. The drivers and ports use `--` and ASCII quotes
throughout; the specs include em-dashes (non-ASCII but not
homoglyphs) and the homoglyph-specific check correctly tolerates
them.

### Library coherence (Law 4)

Both ports import from `../core.ts`:

```ts
import type { MPFR, Result, RoundingMode } from '../core.ts';
import { MPFRError, ... } from '../core.ts';
```

No type redeclarations (`grep -E 'interface (MPFR|Result)|type
RoundingMode'` returned clean). The grader's AST gate would have
rejected with `composite=0` if either had redeclared.

### Mutate.py gaming (worklog 010 lesson)

Both ports are naturally compact delegations. Neither needed dead-
code padding to satisfy mutate.py:

- `mpfr_frac`: gate killed (op-swap clean kill at composite=0.4676,
  weak kill at comparison-swap=0.9136). One real mutation broke the
  golden by a 50%+ margin; one weak break gave additional signal.
- `mpfr_rint_trunc`: gate killed (comparison-swap=0.8787). One weak
  kill, clean_kills=0. The vacuous carve-out from worklog 011 was
  not needed because comparison-swap found a guard-path edge case
  that broke the golden below 0.95.

In both cases the gate's outcome reflected the actual code surface
without intervention.

## Algorithmic notes — what shipped

### mpfr_frac

Delegation via `sub + trunc`:

```ts
// Ref: mpfr/src/frac.c L29-L143.
if (u.kind === 'nan')                       return { value: NAN_VALUE, ternary: 0 };
if (u.kind === 'inf' || mpfr_integer_p(u))  return { value: signedZero(u.sign, prec), ternary: 0 };
if (u.exp <= 0n)                            return mpfr_set(u, prec, rnd);          // |u| < 1 fast path
const truncU = mpfr_trunc(u, u.prec).value; // exact at u.prec
return mpfr_sub(u, truncU, prec, rnd);      // ternary by construction
```

Ternary correctness: `trunc(u, u.prec)` is exact, so `sub(u,
trunc_u, prec, rnd)` has ternary == sign(rounded - (u - trunc_u))
== sign(rounded - exact_frac). Matches the C contract. 153/153.

### mpfr_rint_trunc

Pure delegation, ~30 LOC body:

```ts
// Ref: mpfr/src/rint.c L405-L424.
if (u.kind !== 'normal' || mpfr_integer_p(u))  return mpfr_set(u, prec, rnd);
const tmp = mpfr_trunc(u, u.prec).value;       // exact
return mpfr_set(tmp, prec, rnd);               // does the final rounding
```

The C body's `MPFR_SAVE_EXPO_DECL` / flag-restore dance is
naturally absent in TS (no `__gmpfr_flags` global on the public
surface). 153/153.

## State.db at end of session

```sql
SELECT status, COUNT(*) FROM functions GROUP BY status;
-- blocked|5
-- done|124
```

**0 pending rows remain.** The next batch needs either:

1. Callgraph extension to seed more functions into state.db
   (currently 129 rows out of ~600 total MPFR functions; the topo-
   ranker has more candidates available in the callgraph dump but
   hasn't been re-run for the lower-priority tail).
2. Pick from the 5 blocked rows — but 4 of them are runtime-system
   functions (`mpfr_abort_prec_max`, `mpfr_allocate_func`,
   `mpfr_free_func`, `mpfr_reallocate_func`) that are conceptually
   no-ops in the immutable bigint TS surface and won't be ported;
   the 5th (`mpfr_sqrt2_approx`) is ADR-0002-parked.

The natural next session task is option 1: extend the callgraph to
seed more pending rows.

## bd at end of session

No new bd issues filed (no architectural gaps surfaced in this
batch). Pre-existing open issues unchanged.

## Pointers

- `docs/worklog/011-phase-transition.md` -- the phase transition
  worklog this batch validates.
- `docs/adr/0002-approximation-helper-grading.md` -- the ADR
  shipped last session, unchanged.
- `eval/functions/mpfr_frac/`, `eval/functions/mpfr_rint_trunc/` --
  artifacts.
- `src/ops/frac.ts`, `src/ops/rint_trunc.ts` -- the ports.
- `eval/driver/mutate.py` -- the vacuous-pass carve-out from
  worklog 011 was not needed in this batch (both gates killed
  legitimately).

## Lessons / process notes

1. **Serial orchestration was the right choice for n=2**. Each
   subagent dispatch was visible in real time; the one transport
   drop was caught and retried in <30 seconds of wall time. Auto-
   pilot via `ralph.py` would have been faster but would have
   obscured the cost-burn and escalation measurements that
   Production phase asks for.

2. **Subagent prompts should be tighter than CLAUDE.md's full
   text**. The first dispatch (for frac TS port, the one that
   dropped) had a very long prompt with extensive context. The
   retry trimmed it to essentials + read-order list and the
   subagent completed in 76s vs the dropped 98s. Future dispatches
   should aim for ~50-80 lines of prompt, not 200+.

3. **The delegation pattern wins on misc-class ports.** Both
   ports compose 2-3 already-shipped ops. The TS port is shorter,
   the verification is easier (the delegates are already proven),
   and the ternary correctness falls out of the algebra. The only
   cost is the runtime indirection cost — which is a
   Phase-Optimize concern, not Phase-Production.

4. **State.db's pending queue runs out quickly under steady
   Production execution.** Two functions cleared the queue. The
   callgraph needs a re-run before the next mega-batch can be
   topo-picked. Filing this as the next-session focus.

## Next session

Likely two parts:

- **Re-run callgraph to seed more pending rows.** `python3
  eval/driver/callgraph.py` to ingest the next tier of functions
  from `mpfr/src/` into state.db. Expected: 20-50 new pending
  rows, mostly misc / arithmetic / substrate.
- **Then dispatch the next batch.** Whether to do it serially
  (like this batch, for continued instrumentation) or via
  `ralph.py --parallel 8` (for throughput) is the session-opener
  decision. My recommendation: one more serial batch (of 5-8
  functions) to triangulate cost burn at moderate scale, then
  switch to parallel for the bulk of Production.
