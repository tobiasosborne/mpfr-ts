/**
 * reference_ports/broken/mpfr_cmp_ui.ts — deliberately-buggy mpfr_cmp_ui.
 *
 * **Deliberately broken: returns 0 always (after the throw paths).**
 * The simplest "wrong" mutation: ignore both operands and report
 * "equal". Disagrees with the correct port on every case where x != n
 * (which is the vast majority), giving an extremely high gap.
 *
 * NaN x and out-of-range n throw paths are preserved (same rationale
 * as broken cmp_si — keep throw branches correct so the gap is local
 * to the cmp result).
 *
 * NOT used in production. Do NOT fix.
 *
 * Ref: src/ops/cmp_ui.ts.
 */

import type { MPFR } from '../../../src/core.ts';
import { MPFRError, validate } from '../../../src/core.ts';

const ULONG_MAX_VAL: bigint = (1n << 64n) - 1n;

export function mpfr_cmp_ui(x: MPFR, n: bigint): number {
  if (typeof n !== 'bigint') {
    throw new MPFRError('EPREC', `n must be bigint, got ${typeof n}`);
  }
  if (n < 0n || n > ULONG_MAX_VAL) {
    throw new MPFRError(
      'EPREC',
      `n out of uint64 range [0, ${ULONG_MAX_VAL}], got ${n}`,
    );
  }
  validate(x);
  if (x.kind === 'nan') {
    throw new MPFRError(
      'EDOMAIN',
      `mpfr_cmp_ui: NaN x (broken)`,
    );
  }
  // BUG: ignore x and n entirely, report "equal".
  return 0;
}
