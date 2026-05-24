/**
 * reference_ports/broken/mpfr_cmp_si.ts — deliberately-buggy mpfr_cmp_si.
 *
 * Used to mutation-prove the golden master per CLAUDE.md PIL.3.
 *
 * **Deliberately broken: IGNORES THE SIGN OF n.** The port treats n as
 * its absolute value `|n|`, so for any negative n, the comparison flips:
 *
 *   - Correct: cmp_si(x=5, n=-3) → +1  (5 > -3)
 *   - Broken:  cmp_si(x=5, n=-3) → cmp(x=5, +3) = +1  (coincidentally
 *              right when |n| < x)
 *
 *   - Correct: cmp_si(x=-5, n=-3) → -1 (-5 < -3)
 *   - Broken:  cmp_si(x=-5, n=-3) → cmp(-5, +3) = -1 (coincidentally
 *              right when sign(x) is negative since both branches drop x)
 *
 *   - Correct: cmp_si(x=2, n=-3) → +1 (2 > -3)
 *   - Broken:  cmp_si(x=2, n=-3) → cmp(2, +3) = -1 (FAIL — sign of n
 *              matters because |n| > x)
 *
 *   - Correct: cmp_si(x=-2, n=3) → -1 (-2 < 3)
 *   - Broken:  cmp_si(x=-2, n=3) → cmp(-2, +3) = -1 (PASS — sign of n
 *              matches in this case since n is positive anyway)
 *
 * The mutation is "drop the sign of n" — equivalent to using
 * `Math.abs(n)`. Many cases coincidentally pass; the adversarial sweep
 * piles on positive-x-with-negative-n pairs where coincidence breaks.
 *
 * NaN x and out-of-range n branches are preserved (the broken port
 * still throws on those — failing them with throws-not-passes makes the
 * gap signal LESS clear, so we keep the throw paths correct).
 *
 * NOT used in production. Do NOT fix this file.
 *
 * Ref: src/ops/cmp_si.ts — the correct version.
 */

import type { MPFR, Sign } from '../../../src/core.ts';
import { MPFRError } from '../../../src/core.ts';
import { compareMPFR } from '../../../src/internal/mpfr/cmp_raw.ts';

const LONG_MIN_VAL: bigint = -(1n << 63n);
const LONG_MAX_VAL: bigint = (1n << 63n) - 1n;

function bitLength(n: bigint): bigint {
  let bits = 0n;
  let probe = n;
  while (probe > 0n) {
    bits++;
    probe >>= 1n;
  }
  return bits;
}

export function mpfr_cmp_si(x: MPFR, n: bigint): number {
  if (typeof n !== 'bigint') {
    throw new MPFRError('EPREC', `n must be bigint, got ${typeof n}`);
  }
  if (n < LONG_MIN_VAL || n > LONG_MAX_VAL) {
    throw new MPFRError(
      'EPREC',
      `n out of int64 range [${LONG_MIN_VAL}, ${LONG_MAX_VAL}], got ${n}`,
    );
  }
  if (x.kind === 'nan') {
    throw new MPFRError(
      'EDOMAIN',
      `mpfr_cmp_si: NaN x (broken)`,
    );
  }

  let temp: MPFR;
  if (n === 0n) {
    temp = { kind: 'zero', sign: 1, prec: 1n, exp: 0n, mant: 0n };
  } else {
    // BUG: sign always +1 — drops the sign of n entirely.
    const sign: Sign = 1;
    const absN: bigint = n < 0n ? -n : n;
    const bits = bitLength(absN);
    temp = {
      kind: 'normal',
      sign,
      prec: bits,
      exp: bits,
      mant: absN,
    };
  }

  const r = compareMPFR(x, temp);
  if (r === null) {
    throw new MPFRError('EDOMAIN', 'unexpected null');
  }
  return r;
}
