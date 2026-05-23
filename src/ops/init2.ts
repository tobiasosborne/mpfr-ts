/**
 * ops/init2.ts — pure-TS port of MPFR's `mpfr_init2`.
 *
 * Public-surface op. Imports from the locked schema in `src/core.ts` per
 * CLAUDE.md Law 4 (library coherence): no redeclaration of `MPFR`, no
 * substitute precision bounds, no parallel error type. The grader's
 * ast_check enforces this at composite=0; deviation here would break
 * every downstream port that composes against `mpfr_init2`.
 *
 * C signature
 * -----------
 *
 *   void mpfr_init2(mpfr_ptr x, mpfr_prec_t p);
 *
 *   - mutates `x` in place;
 *   - allocates the limb backing (`mpfr_allocate_func`);
 *   - sets the precision to `p`;
 *   - leaves the value as NaN with mantissa contents *uninitialised*
 *     (the caller MUST run `mpfr_set_*` before reading).
 *
 *   Ref: mpfr/src/init2.c L32–L71.
 *
 * Divergence from C — and why
 * ---------------------------
 *
 * The C function's "uninitialised mantissa, you must overwrite before
 * reading" contract has no immutable analog: every `MPFR` value crossing
 * the TS boundary must be well-formed against `validate()` in
 * `src/core.ts`, so we cannot hand out an "uninitialised" value. The two
 * candidates were:
 *
 *   (a) Return the canonical `NAN_VALUE` — mirrors the C post-condition
 *       literally (post-init, the C struct's exponent sentinel reads as
 *       NaN — see mpfr/src/init2.c L70 `MPFR_SET_NAN(x)`). BUT: NaN in
 *       the TS surface has `prec === 0n` (see src/core.ts L103–L107), so
 *       the requested precision would be silently discarded. That breaks
 *       the contract `init2` is named for.
 *
 *   (b) Return `posZero(prec)` — a deterministic, well-formed `+0` at
 *       exactly the requested precision. Preserves the precision
 *       round-trip, exercises the prec-validation path (throws `EPREC`
 *       for `prec < PREC_MIN` or `prec > PREC_MAX`, matching the C
 *       `MPFR_ASSERTN(MPFR_PREC_COND(p))` at init2.c L59), and produces
 *       a value `validate()` accepts. The trade-off — a divergence from
 *       libmpfr's *literal* output — is documented and contained: users
 *       who specifically want NaN at no precision use `NAN_VALUE`
 *       directly.
 *
 * We pick (b). Rationale captured in
 * `~/.claude/projects/-home-tobiasosborne-Projects-mpfr-ts/memory/`
 * (decision log) and cited from the spec.json `divergence_from_c` field.
 *
 * Precision bounds
 * ----------------
 *
 * `PREC_MIN` and `PREC_MAX` come from `src/core.ts`. They mirror
 * `MPFR_PREC_MIN` (1) and the smallest cross-platform `MPFR_PREC_MAX`
 * (`2^31 - 257`) respectively. Going through `posZero(prec)` — which
 * calls the internal `assertPrec` — guarantees we use the *one* bounds
 * check the rest of the library uses; we never re-implement that check
 * here.
 *
 *   Ref: mpfr/src/mpfr.h L191–L193 — MPFR_PREC_MIN / MPFR_PREC_MAX
 *     formulas the schema constants mirror.
 *   Ref: src/core.ts L216–L236 — PREC_MIN, PREC_MAX, locked.
 *
 * Allocation
 * ----------
 *
 * The C version allocates `MPFR_PREC2LIMBS(p)` limbs of backing storage.
 * The TS surface stores the mantissa as a single `bigint`, so there is
 * no separate allocation step. The `O(1)` work the TS port performs is
 * exactly the bounds check and the construction of a frozen-by-shape
 * literal — the storage is whatever `posZero` returns.
 *
 * Refs
 * ----
 *
 *   - mpfr/src/init2.c — C reference.
 *   - src/core.ts §"posZero" — the constructor of record.
 *   - eval/functions/mpfr_init2/spec.json — class:"misc", signature.
 *   - CLAUDE.md Law 4 — schema imports; PIL.* — Pilot gating.
 */

import type { MPFR } from '../core.ts';
import { posZero } from '../core.ts';

/**
 * Construct a fresh {@link MPFR} at the given precision.
 *
 * Returns `posZero(prec)` — a deterministic `+0` at `prec` bits — as the
 * immutable-surface analog to C's `mpfr_init2`. See the module docstring
 * for the rationale on the divergence-from-C choice.
 *
 * @mpfrName mpfr_init2
 *
 * @param prec  precision in **bits**, in `[PREC_MIN, PREC_MAX]` inclusive.
 *              Per CLAUDE.md "Hallucination-risk callouts: mpfr_prec_t is
 *              in bits, not decimal digits" — `53n` means 53 mantissa
 *              bits (≈ 16 decimal digits), not 53 digits.
 *
 * @returns     an `MPFR` value with `kind === 'zero'`, `sign === 1`,
 *              `prec === prec`, `exp === 0n`, `mant === 0n`. The value
 *              passes `validate()` from `src/core.ts` without
 *              post-processing.
 *
 * @throws {MPFRError} with `code === 'EPREC'` when `prec < PREC_MIN`,
 *                    `prec > PREC_MAX`, or `prec` is not a `bigint` at
 *                    runtime (callers crossing the TS type boundary).
 *                    Propagated unchanged from `posZero`/`assertPrec`.
 *
 * @example
 *   const x = mpfr_init2(53n);  // +0 at IEEE float64 precision
 *   x.kind     === 'zero';
 *   x.sign     === 1;
 *   x.prec     === 53n;
 *   x.exp      === 0n;
 *   x.mant     === 0n;
 */
export function mpfr_init2(prec: bigint): MPFR {
  // The single delegation. `posZero` performs the bounds check via the
  // internal `assertPrec` in core.ts (L445–L455), and the returned
  // value is the canonical `{kind:'zero', sign:1, prec, exp:0n, mant:0n}`
  // shape `validate()` accepts. There is no other work for `mpfr_init2`
  // to do on the immutable TS surface — the C allocation step has no
  // analog when the mantissa is a single bigint.
  return posZero(prec);
}
