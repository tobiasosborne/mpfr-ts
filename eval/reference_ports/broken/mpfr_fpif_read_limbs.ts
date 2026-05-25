/**
 * reference_ports/broken/mpfr_fpif_read_limbs.ts -- deliberately-buggy.
 *
 * **BUG: collapses the entire decode to a constant
 * { mant: 1n, nextPos: pos + 1 } regardless of input.** Per HANDOFF
 * gotcha #10, narrow perturbations (off-by-one pad_bits direction,
 * wrong byte order) leave too many cases passing because many small
 * mantissas / powers of two have simple bit patterns; collapsing to a
 * constant guarantees every case fails the grader.
 */

import { MPFRError } from '../../../src/core.ts';

export interface ReadLimbsResult {
  readonly mant: bigint;
  readonly nextPos: number;
}

export function mpfr_fpif_read_limbs(
  bytes_value: bigint,
  byte_length: number,
  pos: number,
  prec: bigint,
): ReadLimbsResult {
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
  return { mant: 1n, nextPos: pos + 1 };
}
