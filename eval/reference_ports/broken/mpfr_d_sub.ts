/**
 * reference_ports/broken/mpfr_d_sub.ts — deliberately-buggy mpfr_d_sub.
 *
 * Multi-bug: (1) reverses argument order — computes c - b instead of
 * b - c (note: d_sub computes DOUBLE b minus MPFR c), (2) negates ternary.
 *
 * NOT used in production.
 */

import type { MPFR, Result, RoundingMode, Ternary } from '../../../src/core.ts';
import { mpfr_sub } from '../../../src/ops/sub.ts';
import { mpfr_set_d } from '../../../src/ops/set_d.ts';

export function mpfr_d_sub(
  b: number,
  c: MPFR,
  prec: bigint,
  rnd: RoundingMode,
): Result {
  const bMpfr = mpfr_set_d(b, 53n, rnd).value;
  // BUG 1: argument order swap — computes c - bMpfr instead of bMpfr - c.
  const r = mpfr_sub(c, bMpfr, prec, rnd);
  // BUG 2: negate ternary.
  return { value: r.value, ternary: (-r.ternary) as Ternary };
}
