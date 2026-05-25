/**
 * ops/rint_trunc.ts -- pure-TS port of MPFR's `mpfr_rint_trunc`.
 *
 * Round `u` toward zero to a nearby integer, then correctly round that
 * integer to the requested output precision in the given rounding mode.
 *
 * `mpfr_rint_trunc` differs from `mpfr_trunc` in that the final result is
 * rounded to the *requested* `prec` using `rnd`, whereas `mpfr_trunc`
 * truncates at its own output precision (always toward zero). When `prec`
 * is wide enough to hold `trunc(u)` exactly the two agree; otherwise
 * `rint_trunc` may differ from `trunc(u)` by an ulp depending on `rnd`.
 *
 * C signature
 * -----------
 *
 *   int mpfr_rint_trunc(mpfr_ptr r, mpfr_srcptr u, mpfr_rnd_t rnd_mode);
 *
 * Ref: mpfr/src/rint.c L405-L424.
 *
 * TS signature
 * ------------
 *
 *   mpfr_rint_trunc(u: MPFR, prec: bigint, rnd: RoundingMode): Result
 *
 * Algorithm: pure delegation
 * --------------------------
 *
 * The C body is a two-branch dispatch (mpfr/src/rint.c L408-L423):
 *
 *   if (singular(u) || integer_p(u)) return mpfr_set(r, u, rnd);
 *   else {
 *     mpfr_init2(tmp, PREC(u));
 *     mpfr_trunc(tmp, u);                 // exact at u.prec
 *     return mpfr_set(r, tmp, rnd);       // final rounding to r.prec
 *   }
 *
 * `mpfr_trunc(u, u.prec)` is exact (it only zeros bits below the binary
 * point at u's own precision), so the ternary of the composite operation
 * comes entirely from the final `mpfr_set` step. The C side saves and
 * restores `__gmpfr_flags` across the trunc call; the TS surface has no
 * such global, so the save/restore is naturally absent (per the spec's
 * `divergence_from_c` note).
 *
 * Refs
 * ----
 *
 *   - mpfr/src/rint.c L405-L424 -- C reference body.
 *   - src/core.ts -- locked MPFR, Result, RoundingMode, MPFRError.
 *   - src/ops/trunc.ts -- delegate (exact at u.prec).
 *   - src/ops/set.ts -- delegate (does the final rounding to prec).
 *   - src/ops/integer_p.ts -- the integer-fast-path predicate.
 */

import type { MPFR, Result, RoundingMode } from '../core.ts';
import { MPFRError, PREC_MAX, PREC_MIN } from '../core.ts';
import { mpfr_integer_p } from './integer_p.ts';
import { mpfr_set } from './set.ts';
import { mpfr_trunc } from './trunc.ts';

const VALID_RND: ReadonlySet<RoundingMode> = new Set<RoundingMode>([
  'RNDN',
  'RNDZ',
  'RNDU',
  'RNDD',
  'RNDA',
]);

function validateArgs(prec: bigint, rnd: RoundingMode): void {
  if (typeof prec !== 'bigint') {
    throw new MPFRError('EPREC', `prec must be bigint, got ${typeof prec}`);
  }
  if (prec < PREC_MIN) {
    throw new MPFRError('EPREC', `prec must be >= ${PREC_MIN}, got ${prec}`);
  }
  if (prec > PREC_MAX) {
    throw new MPFRError('EPREC', `prec must be <= ${PREC_MAX}, got ${prec}`);
  }
  if (!VALID_RND.has(rnd)) {
    throw new MPFRError('EROUND', `unknown rounding mode: ${String(rnd)}`);
  }
}

/**
 * Round `u` toward zero to a nearby integer, then correctly round that
 * integer to `prec` bits in mode `rnd`.
 *
 * @mpfrName mpfr_rint_trunc
 *
 * @param u    operand (any kind).
 * @param prec output precision in **bits**, in `[PREC_MIN, PREC_MAX]`.
 * @param rnd  one of the five RoundingMode values.
 *
 * @returns `{value, ternary}` -- the integer-truncated, prec-rounded
 *          value and the ternary flag (sign of rounded - exact_trunc_u).
 *
 * @throws {MPFRError} `EPREC` on bad precision; `EROUND` on unknown rnd.
 *
 * Ref: mpfr/src/rint.c L405-L424 -- C reference body.
 */
export function mpfr_rint_trunc(
  u: MPFR,
  prec: bigint,
  rnd: RoundingMode,
): Result {
  validateArgs(prec, rnd);

  // Ref: mpfr/src/rint.c L408-L409 -- singular (NaN/Inf/Zero) or already
  // an integer: delegate straight to mpfr_set. mpfr_set handles NaN, Inf,
  // Zero, and prec-rerounding of integer normals correctly on its own.
  if (u.kind !== 'normal' || mpfr_integer_p(u)) {
    return mpfr_set(u, prec, rnd);
  }

  // Ref: mpfr/src/rint.c L411-L422 -- general path. u is a non-integer
  // normal value. Compute tmp = trunc(u) at u.prec (exact; the comment at
  // L417 in the C source states "trunc(u) is always representable in tmp"
  // because u.prec is wide enough to hold the integer part of u), then
  // round tmp to the requested prec in rnd via mpfr_set.
  const tmp = mpfr_trunc(u, u.prec).value;
  return mpfr_set(tmp, prec, rnd);
}
