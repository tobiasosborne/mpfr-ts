/**
 * reference_ports/broken/mpfr_fpif_read_exponent_from_file.ts -- deliberately-buggy.
 *
 * **BUG: collapses the entire decode to a constant
 * { kind: 'normal', sign: 1, exp: 0n, nextPos: pos + 1 } regardless of
 * input.** Per HANDOFF gotcha #10, narrow perturbations (off-by-one
 * MPFR_EXTERNAL_EXPONENT, wrong sign-bit mask) leave many cases passing
 * (embedded exponents in the [-47, 47] range are common, and exp=0 is
 * the most-common embedded value); collapsing to a constant guarantees
 * every case fails the grader.
 */

import type { MPFR } from '../../../src/core.ts';
import { MPFRError } from '../../../src/core.ts';

export interface ReadExponentResult {
  readonly kind: MPFR['kind'];
  readonly sign: 1 | -1;
  readonly exp: bigint;
  readonly nextPos: number;
}

export function mpfr_fpif_read_exponent_from_file(
  bytes_value: bigint,
  byte_length: number,
  pos: number,
  prec: bigint,
): ReadExponentResult {
  if (typeof bytes_value !== 'bigint' || typeof prec !== 'bigint') {
    throw new MPFRError('EDOMAIN', 'bad input');
  }
  if (byte_length < 0 || !Number.isInteger(byte_length)) {
    throw new MPFRError('EDOMAIN', 'bad byte_length');
  }
  if (pos < 0 || !Number.isInteger(pos)) {
    throw new MPFRError('EDOMAIN', 'bad pos');
  }
  // BUG: constant output.
  return { kind: 'normal', sign: 1, exp: 0n, nextPos: pos + 1 };
}
