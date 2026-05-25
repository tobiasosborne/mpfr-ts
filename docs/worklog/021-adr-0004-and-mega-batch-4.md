# 021 -- ADR 0004 (binary I/O) + mega batch 4

> Two-chunk session. Chunk 1: ADR 0004 + 6 fpif statics. Chunk 2: 10-fn
> mega batch (7 ported, 3 PREP-triaged into new bd ADR-needed issues).
> Total shipped: **13 ports**. State.db: 198 -> 211 done; 27 -> 24
> blocked; 0 -> 0 pending.

## TL;DR -- what shipped

### Chunk 1: ADR 0004 + 6 fpif statics (closes mpfr-ts-4h9)

| Function | Composite | Mutate |
|---|---:|---|
| `mpfr_fpif_store_precision` | 1.0 (117/117) | killed (2 clean) |
| `mpfr_fpif_store_exponent` | 1.0 (117/117) | killed (3 clean) |
| `mpfr_fpif_store_limbs` | 1.0 (118/118) | killed (3 clean) |
| `mpfr_fpif_read_precision_from_file` | 1.0 (117/117) | killed (2 clean) |
| `mpfr_fpif_read_exponent_from_file` | 1.0 (117/117) | killed (2 clean) |
| `mpfr_fpif_read_limbs` | 1.0 (117/117) | killed (2 clean) |

Plus: `docs/adr/0004-binary-io-api.md` ratifies "`Uint8Array` codec pair
at the public surface; pure functions over `(buffer, position)` at the
substrate; no `node:fs` / `Bun.file` in `src/`." Companion harness
change: tightened `looksLikeMpfrWire` to require `prec` field presence
(an MPFR wire record always has prec; substrate outputs whose `kind`
field encodes `MPFR['kind']` -- like fpif_read_exponent's `{kind, sign,
exp, nextPos}` -- were wrongly routed to `decodeMpfr` and threw
ToBigInt errors).

### Chunk 2: 10-fn mega batch (7 shipped, 3 blocked at PREP)

**Shipped (all composite=1.0)**:
- `mpfr_set_sj_2exp` -- signed-int64 sister of `set_uj_2exp`; delegates
  with sign handling. Killed (2 clean).
- `mpfr_get_version` -- const "4.2.1" string getter. Vacuous.
- `mpfr_inexflag_p` -- flag predicate; delegates to `flags.ts`. Killed
  (0 clean -- predicate twin of `erangeflag_p`, single-bit surface).
- `mpfr_inits`, `mpfr_inits2` -- vacuous count-passthrough.
- `mpfr_modf` -- integer/fractional split returning two Result-like
  values; substantial. Killed (1 clean).
- `mpfr_mpz_clear` -- vacuous under ADR 0003 (no mpz pool exists in
  the bigint-as-mpz model).

**Blocked at PREP triage (filed new bd issues)**:
- `mpfr_get_q` -> NEW bd `mpfr-ts-8qy`: mpq API ADR for `_q` family
  conversions. C uses `mpq_t` (numerator/denominator mpz_t pair);
  needs explicit decision between `{num: bigint, den: bigint}` and
  fraction-string codec.
- `mpfr_log_begin` -> NEW bd `mpfr-ts-1ts`: logging API ADR for
  `MPFR_LOG_*` facility. C body is `static __attribute__((constructor))`
  inside `#ifdef MPFR_USE_LOGGING`; reads 7+ env vars.
- `mpfr_init_cache` -> NEW bd `mpfr-ts-2wd`: park under ADR 0002.
  C body is `#if 0`'d out; not linkable, no public TS caller.

## Process: orchestrated 2 serial PREP dispatches

| Phase | Subagent | Duration | Cost ~ |
|---|---|---:|---:|
| 1.1 Research fpif ADR options | Explore | ~5 min | $0.30 |
| 1.2 PREP 6 fpif statics + refs | general-purpose | ~24 min | $1.50 |
| Inline (build/calibrate/grade/mutate/commit) Chunk 1 | -- | ~12 min | -- |
| 2.1 PREP 10 mega-batch (with triage) | general-purpose | ~21 min | $1.20 |
| Inline (build/calibrate/grade/mutate/commit) Chunk 2 | -- | ~10 min | -- |
| Orchestrator (ADR write, planning, fixes, beads) | -- | ~25 min | -- |

Estimated total: ~$3-4 in agent calls (well under the $50 ceiling).
Zero 529s across all 3 subagent dispatches.

## Calibration-caught issues (2 -- both fixed inline)

### 1. `looksLikeMpfrWire` collision with fpif_read_exponent

The PREP-shipped reference for `mpfr_fpif_read_exponent_from_file`
graded composite=0 with `pass=0/117, throw=117`. Root cause: the
`{kind: MPFR['kind'], sign, exp, nextPos}` output shape from
`fpif_read_exponent` (per ADR 0004 worked example) has a `kind` field
whose value is one of `{normal, zero, inf, nan}`. The harness's
`decodeExpectedOutput` saw the `kind` field, matched
`looksLikeMpfrWire` (which only checked `kind`'s enum membership),
routed to `decodeMpfr`, and threw "Invalid argument type in ToBigInt
operation" on `BigInt(undefined)` for the missing `prec` field.

**Fix**: tightened `looksLikeMpfrWire` to require `typeof r['prec']
=== 'string'`. MPFR wire records always carry `prec` as a
decimal-integer string per `jl_kv_mpfr` in common.h. The other 5 fpif
references and all 198 prior ports continued to grade composite=1.0
post-fix.

**Worklog-worthy gotcha**: any future TS port whose output is an
object with a `kind` field in the MPFR-kind enum but without a `prec`
field will now correctly fall through to generic-struct decoding.
Captured in HANDOFF gotcha #10.

### 2. Import path rewrite missed `src/ops/` cross-imports

Promote step (cp reference port to src/ops/) used
`sed 's|../../../src/core.ts|../core.ts|g'` -- catches the core import
but missed `../../../src/ops/<x>.ts` cross-imports in `set_sj_2exp`
(imports `set_uj_2exp`) and `modf` (imports `set`, `rint_trunc`,
`frac`).

**Fix**: added `sed 's|../../../src/ops/|./|g'` to the promote loop
inline. Both files re-graded composite=1.0 immediately.

**Worklog-worthy**: every future cross-import in a port file that
references another `src/ops/` sister function needs the second sed
rule. Promote-script polish would help; filed as orchestrator-side
work, not a port issue.

## ADR 0004 (load-bearing artifact this session)

`docs/adr/0004-binary-io-api.md` ratifies `Uint8Array` as the canonical
binary I/O type for the fpif family. Five invariants are stated and
enforced structurally:

1. `Uint8Array` only -- no `Buffer`, no `ArrayBuffer`,
   no `Uint8ClampedArray`.
2. Pure functions over `(buffer, position)` -- no callback IO
   interface, no stateful reader/writer handles.
3. Little-endian everywhere -- `HAVE_BIG_ENDIAN` C dual-path collapsed
   to single LE implementation.
4. No filesystem in `src/` -- FILE* C variants are explicitly not
   ported; users compose with `node:fs` or `Bun.file` at their own
   boundary.
5. Errors via `MPFRError('EDOMAIN')` throw -- C's 0/1 return becomes
   TS exception.

Alternatives rejected: callback IO interface (B), single-bigint
encoding (C), defer-and-park (D) -- each documented in the ADR with
rationale.

The grader supports the wire format trivially: byte buffers travel
as decimal-bigint scalars (LE-uint interpretation), and the existing
codec paths (`isDecimalIntegerString` + `compareField` bigint
branch + object-output `'object'` branch) handle them with no
schema change.

## Risk monitoring -- outcomes

| Risk | Outcome |
|---|---|
| Cost burn | ~$3-4 of $50 ceiling |
| API overload (529s) | 0 incidents across 3 dispatches |
| Cyrillic homoglyph | clean (Rule 13 verified at every PREP + PORT) |
| ADR scope creep | clean (4h9 closed by ADR 0004 + 6 ports; new ADRs filed properly as separate issues) |
| Mutator bait | none -- gates passed honestly (6 killed + 1 vacuous + 1 trivial-killed in Chunk 1; 5 killed + 2 vacuous in Chunk 2) |
| Reference-port bug propagation | caught (codec collision; fixed via harness tightening, not per-port workaround) |
| Carve-out drift | 0 survived; predicate continues to fire correctly across all 13 mutate gates |
| Out-of-scope subagent edits | 0 incidents |

## Mutate gate distribution this session (13 fns gated)

- **killed**: 9 -- store_precision, store_exponent, store_limbs,
  read_precision, read_exponent, read_limbs, set_sj_2exp, inexflag_p,
  modf, inits, inits2 (clean kill counts 0-3 across).
- **vacuous**: 2 -- get_version, mpz_clear.
- **survived**: **0** across all 13 gated functions.

## Open bd issues at session end (21 total -- 3 new, 1 closed)

P1: (cleared)

P2:
- **NEW** `mpfr-ts-8qy` -- mpq API ADR (unblocks `mpfr_get_q` + downstream `_q` family)
- `mpfr-ts-bpo` -- PRNG ADR for random_deviate (1 blocked, more accumulating)
- `mpfr-ts-zhd` -- cbrt Optimize phase
- `mpfr-ts-ra3` -- cbrt (duplicate of zhd?)

P3:
- **NEW**: `mpfr-ts-1ts` -- logging API ADR
- `mpfr-ts-4x5`, `mpfr-ts-e2n` -- string-IO and printf API ADRs
- `mpfr-ts-i8e` -- git pre-commit hook
- `mpfr-ts-ndc` -- state.db port_path tmpdirs
- `mpfr-ts-18x`, `mpfr-ts-d6o`, `mpfr-ts-sr4` -- harness polish

P4:
- **NEW**: `mpfr-ts-2wd` -- park `mpfr_init_cache`

**Closed this session**: `mpfr-ts-4h9` (fpif binary I/O ADR).

## Acceptance

- 13/13 ports shipped composite=1.0 against locked goldens.
- 3 blocked correctly during PREP triage (filed new bd issues).
- 0 dropped during calibration.
- All gates pass: 9 killed + 2 vacuous + 0 survived.
- state.db status updated atomically via `ralph.py --grade` for all 13.
- ASCII-only verified across ~80 created files.
- 2 calibration-caught issues fixed inline (codec tightening + sed
  rule expansion).
- 0 regressions: 198 prior ports unchanged.
- 3 new bd issues filed; 1 closed.

## Pointers

- `docs/adr/0004-binary-io-api.md` (the ADR; load-bearing for fpif and
  any future byte-buffer ports)
- `eval/harness/value_codec.ts` -- `looksLikeMpfrWire` tightening
  (load-bearing for any future port whose output object has a `kind`
  field with `MPFR['kind']` value)
- The 13 ports in `src/ops/`
- bd issues: 3 new (mpq, logging, init_cache), 1 closed (mpfr-ts-4h9)

## One final thing

Library is now **211 / 525 = 40% complete**. Cumulative cost across all
sessions to date: ~$18-19. The PREP-PORT cost shape continues to hold;
the calibration discipline catches what the goldens would have shipped
as composite=0 noise (1 codec collision this session; 4 last session).
The mutation-proven golden discipline catches what brokens-only
calibration cannot (0 survived across all 13 gates).

**Two ADRs filed this session** (mpq, logging) are 2-4-hour units;
both are P2-P3 and don't gate the next batch. The next P1 is now
neither -- the picker should yield another 25-30 portable fns when
`ralph.py --next` runs next session.

Next-session priorities (refreshed HANDOFF):
- **P1: next mega batch** (~25-30 ports; queue empty -> rerun --next)
- **P2: bd new (mpq ADR)** (unblocks `mpfr_get_q` + future _q family)
- **P2: bd `mpfr-ts-bpo`** (PRNG ADR for random_deviate)
- **P3: bd `mpfr-ts-1ts`** (logging API ADR)
- **P4: bd `mpfr-ts-2wd`** (park init_cache)
- **P4: bd `mpfr-ts-zhd`** (cbrt Optimize)
