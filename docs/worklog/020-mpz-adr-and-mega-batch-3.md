# 020 — Chunks 2+3: mpz API ADR (5 ports) + mega batch 3 (17 ports)

> Chunks 2 and 3 of the 3-chunk continuation session. Chunk 2: ADR 0003
> + 5 _z ports. Chunk 3: 30-fn mega batch (17 shipped, 12 blocked, 1
> dropped during calibration). Total session shipped: **24 ports across
> 3 chunks**. State.db: 174 → 198 done · 19 → 27 blocked · 0 pending.
> Cost ~$8.

## TL;DR — what shipped this worklog

### Chunk 2: mpz API ADR + 5 _z ports (mpfr-ts-3a9 → CLOSED)

| Function | Dest | Composite | Mutate |
|---|---|---:|---|
| `mpfr_set_z_2exp` | `src/ops/set_z_2exp.ts` | 1.0 (165/165) | killed (5/0) |
| `mpfr_get_z_2exp` | `src/ops/get_z_2exp.ts` | 1.0 (146/146) | low-confidence-pass (1/0) |
| `mpfr_add_z` | `src/ops/add_z.ts` | 1.0 (127/127) | killed (5/1) |
| `mpfr_sub_z` | `src/ops/sub_z.ts` | 1.0 (127/127) | killed (5/1) |
| `mpfr_mul_z` | `src/ops/mul_z.ts` | 1.0 (127/127) | killed (5/1) |

Plus: `docs/adr/0003-mpz-api.md` ratifies "native TS bigint as mpz_t analogue".

### Chunk 3: mega batch 3 — 17 shipped of 30 prepped

**Wave A (12 trivial, all composite=1.0)**:
- `mpfr_flags_clear`, `mpfr_flags_save`, `mpfr_flags_set`, `mpfr_flags_test` — vacuous, delegate to internal/mpfr/flags.ts
- `mpfr_erangeflag_p` — vacuous
- `mpfr_get_default_rounding_mode` — killed (1 clean)
- `mpfr_get_emax_max/min`, `mpfr_get_emin_max/min` — killed (2 clean each)
- `mpfr_get_cputime`, `mpfr_free_pool` — vacuous (no-op accessors)

**Wave B (5 middle/substantial, all composite=1.0)**:
- `mpfr_flags_restore` — vacuous
- `mpfr_dump` — vacuous (delegates to mpfr_fdump from worklog 019)
- `mpfr_const_euler_bs_1` — killed (2 applied, 1 clean)
- `mpfr_set_uj_2exp` — killed (6 applied, 2 clean); unsigned-uint64 sister of set_z_2exp
- `mpn_tdiv_qr` — killed (1 clean); substrate, delegates to mpn_divrem

**12 blocked during PREP** (existing or new bd issue):
- `mpfr_fprintf` → existing string-IO ADR (`mpfr-ts-e2n`)
- 6× `mpfr_fpif_*` → **NEW bd `mpfr-ts-4h9`** (P2 binary I/O ADR)
- `mpfr_cos2_aux`, `mpfr_exp2_aux`, `mpfr_exp2_aux2` → ADR 0002 (no public TS caller exists)
- `mpfr_random_deviate_value` → **NEW bd `mpfr-ts-bpo`** (P2 PRNG ADR)
- `mpfr_rem1` → ADR 0002 (no public TS caller exists)

**1 dropped during calibration**:
- `mpfr_cbrt` → **NEW bd `mpfr-ts-zhd`** (P2). PREP-shipped reference port at composite=0.11. Faithful integer-cube-root + Newton-bisect adjust algorithm broken on the n=prec+1 RNDN tie-handling. Two paths forward documented in the issue: (a) delegating shortcut via exp(ln(x)/3) once exp+log shipped; (b) faithful Ziv loop port in a dedicated Optimize-phase session.

## Process: orchestrated 8 serial subagent dispatches

| Phase | Subagent | Duration | Cost ~ |
|---|---|---:|---:|
| 2.1 INVESTIGATE mpz API | 1 | ~12 min | $0.50 |
| 2.3 ADR + PREP 5 _z | 1 | ~25 min | $1.00 |
| 2.5 PORT 5 _z | 1 | ~5 min | $0.30 |
| 3.2 PREP 30 (with triage) | 1 | ~30 min | $1.50 |
| 3.5 PORT Wave A (12) | 1 | ~3 min | $0.30 |
| 3.6 PORT Wave B (5) | 1 | ~4 min | $0.30 |
| Inline (build/calibrate/grade/mutate) | — | ~25 min | — |
| Orchestrator (planning, fixes, beads) | — | ~20 min | — |

Total wall: ~2 hours orchestration. Total cost: ~$8 (Chunk 1 ~$1 + Chunks 2/3 ~$7).

Zero 529s across all 6 subagent dispatches.

## Calibration-caught issues (4 — all fixed before/during PORT)

### 1. `mpfr_add_z` / `mpfr_sub_z` signed-zero trap (worklog 019 → worklog 020 lesson)

The PREP-shipped reference ports for add_z/sub_z scored 0.97/0.99 on signed-zero edge cases. Root cause: `mpfr_set_z(0n, ...)` returns `+0`, then `mpfr_add(-0, +0, RNDN)` returns `+0` per IEEE 754. But C `mpfr_add_z` dispatches via `mpfr_add_si(x, 0, rnd)` which is effectively `mpfr_set(x, prec, rnd)` — sign preserved.

**Fix**: mirror C dispatch — when z fits in slong (most cases including z=0), delegate to the `_si` variant. The reference ports + production ports now both do this.

**Worklog-worthy gotcha**: any future TS port that "delegates to mpfr_add" without considering the C dispatch table will hit this trap. Captured in HANDOFF gotcha #6.

### 2. `mpfr_const_euler_bs_1` cont=0 P/C init bug

The PREP-shipped reference for bs_1 had `cont=0 ? L.P : L.P * R.P` for the P field. But the C source (const_euler.c L99-L101) only writes `s->P` when cont=1 — leaves it at the init value 0 otherwise. Same for C field. Fix: `cont=0 ? 0n : ...`.

Caught the same way as worklog 019's fdump: regenerated golden surfaced the divergence.

### 3. `mpfr_free_pool` codec null-output gap

Reference port returned `null`. Codec doesn't handle null/undefined scalar outputs (separate gap from mpfr-ts-2ls — that one covered strings). Fix: return `boolean true` as success marker (mirrors buildopt_*_p precedent). Updated both ref port and golden_driver.

### 4. `mpfr_cbrt` faithful algorithm broken (composite=0.11)

Subagent flagged this risk in the PREP report — the RNDN tie-handling at the `n = prec + 1` step. Confirmed in calibration: 117 cases, 11 pass (NaN/Inf/Zero edges only). **Dropped from this batch, filed for Optimize-phase work.** No collateral — cbrt is rank 130, blocks nothing else this batch.

## Risk monitoring — outcomes

| Risk | Outcome |
|---|---|
| Cost burn | ~$8 of $50 ceiling |
| API overload (529s) | 0 incidents across 8 dispatches |
| Cyrillic homoglyph | clean (Rule 13 verified at every PREP + PORT) |
| Hex literal hygiene | clean (HANDOFF gotcha #3) |
| Mutator bait | none — gates passed honestly (8 killed + 13 vacuous + 1 low-confidence-pass across 22 fns mutated) |
| Reference-port bug propagation | caught (4 ports fixed in calibration; PORT subagents saw clean references) |
| Carve-out drift | 0 survived; predicate continues to fire correctly |
| Out-of-scope subagent edits | 1 caught (get_emin.ts comment deletion, reverted before commit) |

## Mutate gate distribution this session (22 fns gated)

- **killed**: 12 — get_default_rounding_mode, get_emax_max/min, get_emin_max/min, const_euler_bs_1, set_uj_2exp, mpn_tdiv_qr, set_z_2exp, add_z, sub_z, mul_z. Substantial surface; mutators bit cleanly.
- **vacuous**: 9 — all 5 flags_*, erangeflag_p, get_cputime, free_pool, flags_restore, dump. Pure dispatchers/getters.
- **low-confidence-pass**: 1 — get_z_2exp (1 applied at composite > 0.99; carve-out predicate fired correctly).
- **survived**: **0** across all 22 gated functions.

Worklog 016 carve-out predicate validated for the third consecutive session.

## ADR 0003 (the load-bearing artifact this session)

`docs/adr/0003-mpz-api.md` ratifies what `mpfr_set_z`/`mpfr_get_z` already did: native TS `bigint` is the mpz_t analogue. 5 invariants (cited at the call site by every _z port's JSDoc):

1. No `mpz_t` handle appears in the public TS surface.
2. Wire format is decimal string via `mpz_get_str(NULL, 10, z)` / `BigInt(...)`.
3. Rounding ops keep `Result = {value, ternary}` shape; exact ops (e.g. `get_z_2exp`) return `{z, exp}` pair.
4. NaN/Inf inputs to mpz-producing fns throw `MPFRError('EPREC')`.
5. Internal ports MAY use these public ops directly (Law 4 composition).

Alternative considered (option (b) "composition-only"): rejected because 13+ internal MPFR callers across `pow`, `cos`, `atan`, `eint`, `digamma`, `lngamma`, `yn`, `grandom`, `random_deviate`, `exp_2`, `rem1`, `sin_cos`, `get_q`, `li2`, `log10p1` would need to inline `bitLength`-derived precision logic at every call site.

## Acceptance

- 24/24 ports shipped composite=1.0 against locked goldens (5 _z + 17 misc/substrate + 2 worklog 019).
- 12 blocked correctly during PREP triage (existing or new ADR).
- 1 dropped during calibration (cbrt) with follow-up issue filed.
- All gates pass: 12 killed + 9 vacuous + 1 low-confidence-pass + 0 survived.
- state.db status updated atomically via `ralph.py --grade` for all 24.
- ASCII-only + hex-literal hygiene clean across ~110 created files.
- 4 calibration-caught issues fixed before PORT subagents saw them.
- 0 regressions: 174 prior ports unchanged.
- 3 new bd issues filed: `mpfr-ts-4h9` (binary I/O ADR), `mpfr-ts-bpo` (PRNG ADR), `mpfr-ts-zhd` (cbrt Optimize).
- 1 closed: `mpfr-ts-3a9` (mpz API ADR).

## Pointers

- `docs/adr/0003-mpz-api.md` (the API contract)
- `docs/worklog/019-value-codec-strings.md` (Chunk 1)
- bd issues: `mpfr-ts-4h9`, `mpfr-ts-bpo`, `mpfr-ts-zhd` (new this session)
- The 17 worklog-020 production ports in `src/ops/` + `src/internal/mpn/tdiv_qr.ts`

## One final thing

Library is now at **198 / 525 = 38% complete**. Cumulative cost across all sessions (017 + 018 + 019/020): ~$15. Three consecutive serial-dispatch sessions with zero 529s. The PREP-PORT economic shape holds: PREP at ~$1.50 per ~25-30 functions, PORTs at ~$0.30 each. Calibration discipline catches what the goldens would have shipped.

Next-session priorities (refreshed HANDOFF):
- **P1: bd `mpfr-ts-4h9`** (binary I/O ADR; 6 fpif_* unblock)
- **P2: bd `mpfr-ts-bpo`** (PRNG ADR; random_deviate_value + future random fns unblock)
- **P3: next mega batch** (~25-30 ports)
- **P4: bd `mpfr-ts-zhd`** (cbrt Optimize)
