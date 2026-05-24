/**
 * reference_ports/broken/mpfr_round_p.ts — deliberately-buggy
 * mpfr_round_p.
 *
 * **Multi-bug perturbation:**
 *   1. Inverts the n=0 dispatch — returns (tmp == 0n || tmp == mask)
 *      instead of (tmp != 0n && tmp != mask). Every n=0 case fails.
 *   2. Drops the degenerate-case guard (err0 <= 0 / err0 <= prec /
 *      prec >= total) — returns true unconditionally in those cases
 *      instead of false. Edge cases fail.
 *   3. Returns true uniformly when tmp is mixed (rather than handling
 *      the tmp=0 / tmp=mask sub-cases via limb walk). Multi-limb cases
 *      with all-zero or all-one intermediate limbs fail.
 *
 * NOT used in production.
 *
 * Ref: src/ops/round_p.ts — the correct version.
 */

import { MPFRError } from '../../../src/core.ts';

const LIMB_BITS: bigint = 64n;
const LIMB_MAX: bigint = (1n << LIMB_BITS) - 1n;
const HIGHBIT: bigint = 1n << (LIMB_BITS - 1n);

function limbMask(s: bigint): bigint {
  if (s >= LIMB_BITS) return LIMB_MAX;
  return (1n << s) - 1n;
}

export function mpfr_round_p(
  bp: readonly bigint[],
  err0: bigint,
  prec: bigint,
): boolean {
  const bn = BigInt(bp.length);
  if (bn < 1n) throw new MPFRError('EPREC', `empty`);
  if (typeof err0 !== 'bigint') throw new MPFRError('EPREC', `bad err0`);
  if (typeof prec !== 'bigint') throw new MPFRError('EPREC', `bad prec`);
  if (prec < 1n) throw new MPFRError('EPREC', `prec < 1`);
  const topLimb = bp[bp.length - 1];
  if (topLimb === undefined) throw new MPFRError('EPREC', `bad top`);
  if ((topLimb & HIGHBIT) === 0n) throw new MPFRError('EPREC', `MSB not set`);

  let err: bigint = bn * LIMB_BITS;
  // BUG 2: returns true instead of false on degenerate cases.
  if (err0 <= 0n || err0 <= prec || prec >= err) {
    return true;
  }
  if (err0 < err) err = err0;

  const k = prec / LIMB_BITS;
  let s = LIMB_BITS - (prec % LIMB_BITS);
  const n = err / LIMB_BITS - k;

  let idx = Number(bn - 1n - k);
  const tmpLimb = bp[idx];
  if (tmpLimb === undefined) throw new MPFRError('EPREC', `bad start`);

  let mask: bigint = s === LIMB_BITS ? LIMB_MAX : limbMask(s);
  let tmp: bigint = tmpLimb & mask;

  if (n === 0n) {
    s = LIMB_BITS - (err % LIMB_BITS);
    tmp >>= s;
    mask >>= s;
    // BUG 1: inverted dispatch.
    return tmp === 0n || tmp === mask;
  }
  // BUG 3: always returns true for n > 0, skipping the tmp=0 / tmp=mask
  // sub-case logic entirely.
  return true;
}
