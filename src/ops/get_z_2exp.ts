/**
 * ops/get_z_2exp.ts -- pure-TS port of MPFR's `mpfr_get_z_2exp`.
 *
 * Extract a finite MPFR value `x` as a pair `{z, exp}` such that
 * `x = z * 2^exp` exactly.
 *
 * C signature
 * -----------
 *
 *   mpfr_exp_t mpfr_get_z_2exp(mpz_ptr rop, mpfr_srcptr op);
 *
 *   Body (mpfr/src/get_z_2exp.c L50-L95): copy the mantissa limb array
 *   into `rop`, mpn_rshift by `sh = (-prec) mod GMP_NUMB_BITS` to strip
 *   the trailing GMP-padding zeros, then return `MPFR_GET_EXP(op) - prec`
 *   as the exponent.
 *
 * TS divergence
 * -------------
 *
 * Per ADR 0003 invariant 2: returns a frozen pair `{z, exp}` instead of
 * mutating an out-parameter and returning only the exponent. No
 * de-padding shift needed -- the locked schema (src/core.ts L51-L62)
 * stores `mant` MSB-aligned to exactly `prec` bits, so `z = sign * mant`
 * is already the integer mantissa and `exp_c = exp - prec` is the
 * binary exponent in `x = z * 2^exp_c` form.
 *
 * Singular handling (per ADR 0003, mirroring `mpfr_get_z`):
 *   - NaN / +/-Inf: throw `MPFRError('EPREC', ...)`. C returns 0 and
 *     sets the ERANGE flag, but the TS schema has no flag surface;
 *     silent saturation hides bugs at the trust boundary.
 *   - +/-0: return `{z: 0n, exp: 0n}`. Sign collapses on the bigint
 *     side (no `-0n`). The C version returns `__gmpfr_emin` for the
 *     exponent, but the TS schema has no flag-flagged-emin convention,
 *     so we return `0n` -- documented divergence.
 *
 * Refs
 * ----
 *
 *   - mpfr/src/get_z_2exp.c L50-L95 -- C reference body.
 *   - src/ops/get_z.ts -- precedent for get-family op surface.
 *   - docs/adr/0003-mpz-api.md -- API decision (pair return shape).
 *   - src/core.ts -- locked schema (mant is already MSB-aligned).
 *   - ~/.claude/projects/.../memory/mpfr_storage_traps.md -- the
 *     de-padding storage trap this port leans on.
 */

import type { MPFR } from '../core.ts';
import { MPFRError, validate } from '../core.ts';

/**
 * Extract `x = z * 2^exp` as a `{z, exp}` pair.
 *
 * @mpfrName mpfr_get_z_2exp
 *
 * @param x  a finite MPFR value. Must pass {@link validate}.
 *
 * @returns  `{z, exp}` such that `x = z * 2^exp` exactly. For `x = +/-0`,
 *           returns `{z: 0n, exp: 0n}`.
 *
 * @throws {MPFRError} `EPREC` if `x` is NaN or +/-Inf. (C returns 0
 *                    with the ERANGE flag; the TS surface throws.)
 *
 * @example
 *   const y = mpfr_set_z_2exp(17n, 10n, 53n, 'RNDN').value;  // 17 * 2^10
 *   mpfr_get_z_2exp(y);  // {z, exp} with y === z * 2n^exp (exactly).
 */
export function mpfr_get_z_2exp(x: MPFR): { z: bigint; exp: bigint } {
  validate(x);

  if (x.kind === 'nan') {
    throw new MPFRError(
      'EPREC',
      'mpfr_get_z_2exp: NaN cannot be decomposed into (z, exp)',
    );
  }
  if (x.kind === 'inf') {
    throw new MPFRError(
      'EPREC',
      `mpfr_get_z_2exp: ${x.sign === 1 ? '+Inf' : '-Inf'} cannot be decomposed into (z, exp)`,
    );
  }
  if (x.kind === 'zero') {
    return { z: 0n, exp: 0n };
  }

  // Normal: schema's value formula sign * mant * 2^(exp - prec) maps
  // directly to z = sign*mant, exp_c = exp - prec.
  const z = x.sign === -1 ? -x.mant : x.mant;
  const exp = x.exp - x.prec;
  return { z, exp };
}
