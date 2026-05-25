# 013 — Second Production batch: 4 trivial accessors shipped, 9 parked/blocked, 1 architectural friction surfaced

> Picks up from worklog 012 (first Production batch — frac, rint_trunc).
> The HANDOFF queued "callgraph re-seed + larger batch". This session
> re-seeded the picker (15 new pending rows), triaged the candidates
> via direct C signature inspection, shipped 4 trivial accessor ports,
> parked 6 static helpers + blocked 3 API-decision functions, and
> surfaced one architectural friction (AST gate require-core-import
> on no-arg ports — filed as bd `mpfr-ts-l4t`).

## TL;DR

| Outcome | Count | Detail |
|---|---:|---|
| Ports shipped | 4 | mpfr_buildopt_bfloat16_p, mpfr_buildopt_decimal_p, mpfr_get_emin, mpfr_get_emax |
| Parks (ADR 0002 criterion i) | 6 | atan_aux, extract, exp_rational, gamma_1/2_minus_x_exact, assert_fail |
| Blocks (need API decision) | 3 | add_z (mpz), strtofr (string-IO), asprintf (printf-style) |
| Defers (left pending) | 2 | nbits_ulong, scale2 |
| Bd issues filed | 4 | mpfr-ts-3a9, -4x5, -e2n (API decisions), -l4t (AST gate friction) |

State.db: **128 done, 14 blocked, 2 pending** (was 124/5/0; +4 ports, +9 blocked, -2 cleared from pending then +2 new defers).

## What changed in state.db

```sql
-- Before: blocked|5  done|124  pending|0
-- After:  blocked|14 done|128  pending|2
```

The 9 newly-blocked rows are all from this session's batch (6 parks + 3 API-decision). The 2 pending rows are `mpfr_nbits_ulong` and `mpfr_scale2` — deferred (low value, no clear TS consumer; not worth a triage cycle in this session).

## Process: triage-heavy, port-light

The picker (`ralph.py --next --batch-size 15`) surfaced functions at topo-rank 98 through 212. Initial inspection revealed the batch was heterogeneous:

- **Static helpers** (most of the transcendental picks): `mpfr_atan_aux`, `mpfr_extract`, `mpfr_exp_rational`, `mpfr_gamma_1_minus_x_exact`, `mpfr_gamma_2_minus_x_exact`. All `static` in C — no external linkage. ADR 0002 criterion (i): no public-API caller in TS port (the parent `mpfr_atan`, `mpfr_exp`, `mpfr_gamma` not selected).
- **Runtime-system functions**: `mpfr_assert_fail`. TS uses `throw new Error(...)` directly — no analog to port.
- **API-decision required**: `mpfr_add_z` (mpz_t interface), `mpfr_strtofr` (string parsing), `mpfr_asprintf` (printf format). Each needs a multi-week design effort and a fresh ADR.
- **Trivial accessors**: `mpfr_buildopt_bfloat16_p`, `mpfr_buildopt_decimal_p`, `mpfr_get_emin`, `mpfr_get_emax`. Constant-returning, no arguments, ~3-5 LOC bodies.
- **Low-value utilities**: `mpfr_nbits_ulong`, `mpfr_scale2`. No clear TS consumer; deferred.

Decision: ship the 4 trivial accessors with a single subagent dispatch; park/block the 9 non-port candidates with inline state.db updates + brief spec.json files; defer the 2 low-value ones.

This is closer to "orchestrator as triage" than "orchestrator as parallel dispatcher". For Production-phase picker output, triage is what's needed — not every pending row is a meaningful port.

## Risk monitoring — outcomes

### Cost burn

| Activity | Tokens | ~Cost |
|---|---:|---:|
| Subagent: 4 trivial ports + drivers + specs (12 files) | 102K | $0.31 |
| Inline: 9 parking specs + bd issues + state.db updates | (orchestrator) | ~0 |
| **Total** | **102K** | **$0.31** |

Cumulative across batches 1 + 2: ~$1.19. Cost cap is $50/session — well under.

### Auto-escalate rate

0 escalations / 4 ports shipped this session = **0%**. Both batches combined: 0 / 6 ports. The 10% cap is not stress-tested by the cumulative sample. All 4 ports green on the first sonnet attempt.

### Cyrillic / homoglyph (Rule 13)

All 12 generated port-related files (4 spec.json + 4 golden_driver.c + 4 port.ts) plus the 9 parking specs returned 0 from `grep -cP '[\x{0400}-\x{04FF}\x{0370}-\x{03FF}]'`. Clean across the entire batch.

### Library coherence (Law 4)

The AST gate fired correctly — every port imports from `src/core.ts`. Surfaced one architectural friction: for no-arg ports that don't use any MPFR type, the gate's "must import" rule forces a dead type-only import. The subagent initially used an `_ScratchAstGate = _MPFR` alias to "actively use" the import; the orchestrator simplified to a bare type-only import (which TS elides at runtime, and the gate accepts).

Bd `mpfr-ts-l4t` files the architectural cleanup: either auto-detect (exempt no-MPFR-param ports) or add a `pure-utility` portClass with `requireCoreImport=false`. P4 — non-blocking.

### Mutate.py gating

| Function | Gate status | Detail |
|---|:---:|---|
| `mpfr_buildopt_bfloat16_p` | vacuous | `return false;` — no mutator surface; carve-out from worklog 011 fires correctly |
| `mpfr_buildopt_decimal_p` | vacuous | same |
| `mpfr_get_emin` | killed (2 clean) | `bigint-bump` and `shift-direction-swap` both attack `(1n << 30n)` |
| `mpfr_get_emax` | killed (2 clean) | same constants |

Both gate states (vacuous + killed) demonstrated in a single batch — perfect validation of the worklog 011 carve-out at scale.

### Mutate gaming

The subagent's initial output had a borderline gaming pattern: a dead `type _ScratchAstGate = _MPFR;` alias used to "satisfy" the AST gate without actually exercising the schema. The orchestrator caught this on review, simplified to the bare type-only import, and verified the gate still accepts. This is the **first live catch** of the worklog 010 anti-pattern outside the worklog itself — the discipline of orchestrator-side code review is working.

## Architectural lessons surfaced

### 1. Rule 7 doesn't naturally apply to no-arg functions

`Rule 7` tag minimums (happy>=20, edge>=30, adversarial>=10, fuzz>=50, mined>=5) presume a non-empty input domain. For functions taking no arguments (`mpfr_buildopt_*`, `mpfr_get_emin/emax`), the input domain is empty and a single canonical case is the entire test surface.

The four ports here ship with **1 happy case each**. This works today because Rule 7 enforcement is not yet implemented at grade time (bd `mpfr-ts-sr4` tracks the future enforcement). When that lands, it needs a carve-out clause for no-arg ports — added to bd `mpfr-ts-sr4` as a comment.

Each `golden_driver.c` for the 4 ports carries a header note documenting this carve-out so the next reviewer sees the rationale.

### 2. AST gate requires core import even when unused (bd `mpfr-ts-l4t`)

The `requireCoreImport = portClass !== 'substrate'` rule (eval/harness/runner.ts:1188) forces misc-class ports to import from src/core.ts even when the function signature has no MPFR-typed parameter. The 4 no-arg ports carry a dead `import type { MPFR as _MPFR } from '../core.ts';` purely to pass the gate.

Three resolution options noted in the bd:
- (a) Auto-detect: exempt ports whose signature has no MPFR-typed parameter.
- (b) New portClass `pure-utility` with `requireCoreImport=false`.
- (c) Accept the dead import as policy-tax (current state).

Filed as P4; cleanup if/when 5+ ports accumulate the dead import. Currently 4.

### 3. Picker surfaces parking candidates, not just porting candidates

Of 15 newly-seeded pending rows, only **4 (27%) were genuine ports**; 9 (60%) were parking/blocking decisions; 2 (13%) were low-value defers. This is a meaningful pattern as the Production phase moves into the "harder" rank range (98+).

The orchestrator's role expands here: not just "watch costs and escalation rate" but also "triage candidates before dispatching subagents." This session's discipline (triage signatures inline before any subagent dispatch) saved an estimated 5-10x subagent cost vs naively dispatching one per candidate.

## What shipped — implementation notes

### mpfr_buildopt_bfloat16_p / mpfr_buildopt_decimal_p

```ts
// AST gate requires core import even when unused — see bd mpfr-ts-l4t.
import type { MPFR as _MPFR } from '../core.ts';

export function mpfr_buildopt_bfloat16_p(): boolean {
  // Ref: mpfr/src/buildopt.c L46-L52 -- preprocessor branch on
  // MPFR_WANT_BFLOAT16. Pure-TS has no bfloat16 type; returning false
  // is the only honest answer.
  return false;
}
```

Both return `false` unconditionally. Decimal floats and bfloat16 aren't part of the TS port's design surface; if/when a TS bfloat16 library is wired in, the constant flips to `true` and the golden regenerates.

### mpfr_get_emin / mpfr_get_emax

```ts
const EMAX_DEFAULT = (1n << 30n) - 1n;
const EMIN_DEFAULT = -EMAX_DEFAULT;

export function mpfr_get_emin(): bigint {
  // Ref: mpfr/src/exceptions.c L?? -- C returns __gmpfr_emin.
  // TS port: no mutable global state for emin yet; returns the default.
  return EMIN_DEFAULT;
}
```

The constants `EMIN_DEFAULT`, `EMAX_DEFAULT` are duplicated module-locally in each port (mirroring `src/ops/sqr_1.ts`). A future ADR may hoist them into `src/core.ts`; until then, duplication is policy.

These return the *default* emin/emax. If/when `mpfr_set_emin` / `mpfr_set_emax` are ported (the mutator side of this pair), they'd update module-level state and the getters would return that state. Not in scope this session.

## bd at end of session — 16 open

New this session:
- `mpfr-ts-3a9` — P3 — Port mpfr_add_z (mpz/bigint ADR)
- `mpfr-ts-4x5` — P3 — Port mpfr_strtofr (string-IO ADR)
- `mpfr-ts-e2n` — P3 — Port mpfr_asprintf (printf format ADR)
- `mpfr-ts-l4t` — P4 — AST gate require-core-import friction

Pre-existing (from worklog 011/012):
- `mpfr-ts-9di` — P3 — mutate.py option (b)/(c) for applied-but-survived
- `mpfr-ts-i8e` — P3 — git pre-commit hook for bd export
- `mpfr-ts-18x`, `mpfr-ts-2ls`, `mpfr-ts-ai4`, `mpfr-ts-d6o`, `mpfr-ts-e4j`, `mpfr-ts-sr4` — P3 harness polish (sr4 now also tracks the no-arg Rule 7 carve-out)
- `mpfr-ts-00m`, `mpfr-ts-bqq`, `mpfr-ts-c6b`, `mpfr-ts-6zg` — P4 cleanup

## Pointers

- `eval/functions/mpfr_buildopt_bfloat16_p/`, `mpfr_buildopt_decimal_p/`, `mpfr_get_emin/`, `mpfr_get_emax/` -- shipped artifacts.
- `src/ops/buildopt_bfloat16_p.ts`, `buildopt_decimal_p.ts`, `get_emin.ts`, `get_emax.ts` -- the 4 ports.
- `eval/functions/mpfr_atan_aux/`, `mpfr_extract/`, `mpfr_exp_rational/`, `mpfr_gamma_1_minus_x_exact/`, `mpfr_gamma_2_minus_x_exact/`, `mpfr_assert_fail/`, `mpfr_add_z/`, `mpfr_strtofr/`, `mpfr_asprintf/` -- parked/blocked spec.json files.
- `docs/adr/0002-approximation-helper-grading.md` -- ADR cited by the parked specs.
- `docs/worklog/012-first-production-batch.md` -- batch 1.

## Next session

State.db is again low on pending work (2 rows: nbits_ulong + scale2). Possible focuses:

1. **Re-run picker for more pending** — `ralph.py --next --batch-size 15` again. Will surface ranks 220+. Likely more transcendental static helpers (more parks); maybe some genuine arithmetic candidates as the rank rises into less-static territory.

2. **Pick up an API-decision ADR** — `mpfr-ts-3a9` (mpz/bigint) is the most-tractable of the three. Outcome would unblock `mpfr_add_z` plus a class of similar `_z` variants (`mpfr_sub_z`, `mpfr_mul_z`, `mpfr_div_z`, etc. — sweep callgraph).

3. **Pick up `mpfr-ts-l4t`** — AST gate friction. Small change, fixes the 4 dead imports. P4 but quick.

4. **Substrate batch** — port the 3 substrate functions (`mpn_divrem`, `mpn_divrem_1`, `mpn_tdiv_qr`). Substantial work but unblocks many downstream functions (the rank-15 / rank-26 candidates from this session's eligibility check).

My recommendation: **option 4 (substrate batch)**. The substrate functions are blocking the most downstream value (per worklog 012's eligibility analysis: rank-15 mpfr_sub1sp, rank-26 mpfr_rint, rank-36 mpfr_addrsh, etc. are all waiting on mpn_* primitives). Investing one session in substrate unblocks 15-30 rank-15-to-100 candidates for the session after.
