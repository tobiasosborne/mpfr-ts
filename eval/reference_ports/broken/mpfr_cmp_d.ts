/**
 * reference_ports/broken/mpfr_cmp_d.ts — deliberately-buggy mpfr_cmp_d.
 *
 * **Deliberately broken: returns 0 always (after the throw paths).**
 * Same mutation as broken cmp_ui — the simplest wrong answer that
 * disagrees with the correct port on every case where x != d.
 *
 * NaN x and NaN d throw paths are preserved.
 *
 * NOT used in production. Do NOT fix.
 *
 * Ref: src/ops/cmp_d.ts.
 */

import type { MPFR } from '../../../src/core.ts';
import { MPFRError, validate } from '../../../src/core.ts';

export function mpfr_cmp_d(x: MPFR, d: number): number {
  if (typeof d !== 'number') {
    throw new MPFRError('EPREC', `d must be number, got ${typeof d}`);
  }
  if (Number.isNaN(d)) {
    throw new MPFRError('EDOMAIN', `mpfr_cmp_d: NaN d (broken)`);
  }
  validate(x);
  if (x.kind === 'nan') {
    throw new MPFRError('EDOMAIN', `mpfr_cmp_d: NaN x (broken)`);
  }
  // BUG: ignore x and d entirely.
  return 0;
}
