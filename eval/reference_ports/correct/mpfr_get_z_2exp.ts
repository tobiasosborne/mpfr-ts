/**
 * reference_ports/correct/mpfr_get_z_2exp.ts -- hand-written reference port.
 *
 * Extract a finite MPFR value x as a pair {z, exp} such that
 * x = z * 2^exp.
 *
 * With the schema's MSB-aligned mantissa (src/core.ts L51-L62, value
 * formula sign * mant * 2^(exp - prec)), this is a near-one-liner:
 *
 *   z   = (x.sign === -1 ? -x.mant : x.mant)
 *   exp = x.exp - x.prec
 *
 * No bit-shift adjustment is needed -- our mant is already MSB-aligned
 * to exactly prec bits, so there are no GMP-NUMB_BITS padding zeros to
 * strip. Compare with the C version (mpfr/src/get_z_2exp.c L73-L77)
 * which mpn_rshift's by sh = (-prec) mod GMP_NUMB_BITS to do the
 * de-padding.
 *
 * Singular handling (per ADR 0003, mirroring mpfr_get_z):
 *   - NaN / +/-Inf: throw MPFRError('EPREC').
 *   - +/-0:         return {z: 0n, exp: 0n}. Sign collapses on the
 *                   integer side (bigint has no -0n). The C version
 *                   returns __gmpfr_emin for the exp; the TS schema
 *                   has no equivalent flag-flagged-emin convention, so
 *                   we return 0n -- documented divergence.
 *
 * Ref: mpfr/src/get_z_2exp.c L50-L95 -- C reference.
 * Ref: eval/golden_master/common.h L410-L460 -- jl_kv_mpfr (inverse op).
 * Ref: src/ops/get_z.ts -- precedent for get-family op pattern.
 * Ref: docs/adr/0003-mpz-api.md -- API decision (pair return shape).
 * Ref: ~/.claude/projects/.../memory/mpfr_storage_traps.md -- the
 *   de-padding storage trap this port leans on.
 * Ref: CLAUDE.md PIL.3 -- mutation-prove against this file.
 */

import type { MPFR } from '../../../src/core.ts';
import { MPFRError, validate } from '../../../src/core.ts';

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
    // +/-0 -> {0n, 0n}. Sign collapses (no -0n in bigint).
    return { z: 0n, exp: 0n };
  }

  // Normal: schema's value formula is sign * mant * 2^(exp - prec),
  // and the C contract is x = z * 2^exp_c. Match: z = sign*mant,
  // exp_c = exp - prec.
  const z = x.sign === -1 ? -x.mant : x.mant;
  const exp = x.exp - x.prec;
  return { z, exp };
}
