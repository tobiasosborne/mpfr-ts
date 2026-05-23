/**
 * reference_ports/broken/mpfr_cmp.ts — deliberately-buggy mpfr_cmp.
 *
 * Used to mutation-prove the golden master per docs/PILOT_PLAN.md Step 8
 * and CLAUDE.md PIL.3 ("perturb the reference port, confirm the
 * composite drops below 0.95"). If this port scores composite > 0.5 on
 * the mpfr_cmp golden, the golden is too weak and the function is NOT
 * Pilot-passed.
 *
 * **Deliberately broken: ignores precision in the normal/normal mantissa
 * compare.**
 *
 * The correct port aligns both mantissas to a common width
 * `max(a.prec, b.prec)` by left-shifting the lower-prec operand before
 * comparing. This broken port compares the raw, MSB-aligned-to-OWN-prec
 * mantissas directly — without the shift. The bug:
 *
 *   - Same value at the SAME prec: agrees with correct (zero alignment
 *     shift either way).
 *   - Same value at DIFFERENT prec: DISAGREES — the lower-prec mantissa
 *     is numerically smaller as a raw bigint, so the broken port reports
 *     "a < b" (or "a > b") when the values are actually equal. This is
 *     the failure mode the edge/adversarial cases (23)–(27) and (40)
 *     exercise directly.
 *   - Different value at same prec: usually agrees with correct (no
 *     shift to skip).
 *   - Different value at different prec: occasionally agrees by
 *     coincidence (when the value comparison and the raw-mant compare
 *     happen to fall the same way).
 *
 * All non-mantissa branches (NaN throw, kind dispatch, sign / exp
 * compare) are correct — the bug is local to the bottom branch. This
 * mirrors a plausible agent error: "I forgot the prec difference
 * because I copied the structure from cmp.c which works on limbs not
 * MSB-aligned bigints."
 *
 * NOT used in production. NOT imported from `src/`. Do NOT fix this
 * file — the bug is the point.
 *
 * Ref: docs/PILOT_PLAN.md Step 8 — mutation-prove acceptance.
 * Ref: CLAUDE.md PIL.3 — composite must drop below 0.95 under mutation.
 * Ref: src/ops/cmp.ts — the correct version.
 */

import type { MPFR } from '../../../src/core.ts';
import { MPFRError, validate } from '../../../src/core.ts';

export function mpfr_cmp(a: MPFR, b: MPFR): number {
  validate(a);
  validate(b);

  if (a.kind === 'nan' || b.kind === 'nan') {
    throw new MPFRError(
      'EDOMAIN',
      `mpfr_cmp: NaN operand (a.kind=${a.kind}, b.kind=${b.kind})`,
    );
  }
  if (a.kind === 'zero' && b.kind === 'zero') return 0;
  if (a.kind === 'inf' && b.kind === 'inf') {
    if (a.sign === b.sign) return 0;
    return a.sign;
  }
  if (a.kind === 'inf') return a.sign;
  if (b.kind === 'inf') return -b.sign as 1 | -1;
  if (a.kind === 'zero') return -b.sign as 1 | -1;
  if (b.kind === 'zero') return a.sign;

  if (a.kind !== 'normal' || b.kind !== 'normal') {
    throw new MPFRError(
      'EDOMAIN',
      `mpfr_cmp(broken): non-normal pair reached normal branch`,
    );
  }
  if (a.sign !== b.sign) return a.sign;
  if (a.exp > b.exp) return a.sign;
  if (a.exp < b.exp) return -a.sign as 1 | -1;

  // BUG: compare raw mantissas WITHOUT aligning to a common width.
  // a.mant and b.mant are each MSB-aligned to their OWN prec, so a
  // same-value pair at different precs has unequal raw mant bigints
  // and this returns ±1 where the correct port returns 0.
  if (a.mant > b.mant) return a.sign;
  if (a.mant < b.mant) return -a.sign as 1 | -1;
  return 0;
}
