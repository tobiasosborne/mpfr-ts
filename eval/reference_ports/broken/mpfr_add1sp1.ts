/**
 * reference_ports/broken/mpfr_add1sp1.ts — deliberately-buggy mpfr_add1sp1.
 *
 * Used to mutation-prove the golden master per PIL.3.
 *
 * **Deliberately broken: drop the carry-renormalisation step.** When the
 * d < sh and sh <= d < 64 branches encounter a carry out of bit 63 (the
 * a0 > LIMB_MASK condition), the C code shifts a0 right by 1 and ORs in
 * HIGHBIT to re-establish MSB normalisation, then increments bx. This
 * broken port skips the renormalisation: it just masks a0 to 64 bits
 * (losing the carry-out signal entirely) and proceeds with rounding.
 *
 * Effect: any case where the addition of two same-prec same-sign normals
 * produces a sum that doesn't fit in p bits at the original exponent
 * (i.e. needs an exponent bump) gets the wrong mantissa and exponent.
 * That's a very large fraction of cases — essentially every case where
 * bx == cx (Case A, which always carries since the sum of two p-bit
 * MSB-set values is at least 2^(p-1) + 2^(p-1) = 2^p), plus the
 * non-trivial fraction of Case B with bx > cx where the addition still
 * carries.
 *
 * Why this bug shape:
 *   - Case A (equal exponents) always needs the bx++ adjustment AND
 *     a renormalisation (the sum-shifted-right-by-1 trick); a hurried
 *     agent might transcribe just the bx++ and forget the shift, or
 *     forget both.
 *   - The carry-handling code in Case B1/B2 is a short tightly-coupled
 *     block (3 lines including the if-test); easy to skip in a
 *     copy-paste.
 *   - This is exactly the kind of bug that "looks right" against a
 *     small same-exp case where the mantissa coincidentally lands
 *     right but breaks on every non-trivial input.
 *
 * Composite should drop to ~0 (almost all happy/fuzz cases fail).
 *
 * NOT used in production.
 *
 * Ref: docs/PILOT_PLAN.md Step 8.
 * Ref: CLAUDE.md PIL.3.
 * Ref: src/ops/add1sp1.ts — the correct version.
 */

import type { MPFR, Result, RoundingMode, Sign, Ternary } from '../../../src/core.ts';
import { MPFRError } from '../../../src/core.ts';
import { mpfr_overflow } from '../../../src/ops/overflow.ts';

const LIMB_BITS: bigint = 64n;
const LIMB_MASK: bigint = (1n << LIMB_BITS) - 1n;
const HIGHBIT: bigint = 1n << 63n;
const EMAX_DEFAULT: bigint = (1n << 30n) - 1n;
const VALID_RND: readonly RoundingMode[] = Object.freeze([
  'RNDN', 'RNDZ', 'RNDU', 'RNDD', 'RNDA',
] as const);

function isLikeRNDZ(rnd: RoundingMode, sign: Sign): boolean {
  if (rnd === 'RNDZ') return true;
  if (rnd === 'RNDU' && sign === -1) return true;
  if (rnd === 'RNDD' && sign === 1) return true;
  return false;
}

function validateArgs(b: MPFR, c: MPFR, rnd: RoundingMode): void {
  if (!VALID_RND.includes(rnd)) {
    throw new MPFRError('EROUND', `mpfr_add1sp1(broken): unknown rounding mode '${String(rnd)}'`);
  }
  if (b.kind !== 'normal' || c.kind !== 'normal') {
    throw new MPFRError('EPREC', `mpfr_add1sp1(broken): non-normal input`);
  }
  if (b.prec !== c.prec) {
    throw new MPFRError('EPREC', `mpfr_add1sp1(broken): prec mismatch`);
  }
  if (b.prec >= LIMB_BITS || b.prec < 1n) {
    throw new MPFRError('EPREC', `mpfr_add1sp1(broken): prec out of range`);
  }
  if (b.sign !== c.sign) {
    throw new MPFRError('EPREC', `mpfr_add1sp1(broken): sign mismatch`);
  }
}

export function mpfr_add1sp1(
  b: MPFR,
  c: MPFR,
  rnd: RoundingMode,
): Result {
  validateArgs(b, c, rnd);
  const p = b.prec;
  const sh = LIMB_BITS - p;
  const mask = (1n << sh) - 1n;
  const sign: Sign = b.sign;

  let bx = b.exp;
  let cx = c.exp;
  let bp0 = (b.mant << sh) & LIMB_MASK;
  let cp0 = (c.mant << sh) & LIMB_MASK;
  let a0: bigint;
  let rb: bigint;
  let sb: bigint;

  if (bx === cx) {
    // BUG: skip the (>> 1) renormalisation and the bx++ adjustment.
    // Just sum directly (will mostly overflow 64 bits) and mask.
    a0 = (bp0 + cp0) & LIMB_MASK;
    rb = a0 & (1n << (sh - 1n));
    a0 = a0 ^ rb;
    sb = 0n;
  } else {
    if (bx < cx) {
      const t1 = bx; bx = cx; cx = t1;
      const t2 = bp0; bp0 = cp0; cp0 = t2;
    }
    const d = bx - cx;
    if (d < sh) {
      a0 = (bp0 + (cp0 >> d)) & LIMB_MASK;
      // BUG: missing carry renormalisation.
      rb = a0 & (1n << (sh - 1n));
      sb = (a0 & mask) ^ rb;
      a0 = a0 & ~mask;
    } else if (d < LIMB_BITS) {
      sb = (cp0 << (LIMB_BITS - d)) & LIMB_MASK;
      a0 = (bp0 + (cp0 >> d)) & LIMB_MASK;
      // BUG: missing carry renormalisation.
      rb = a0 & (1n << (sh - 1n));
      sb = sb | ((a0 & mask) ^ rb);
      a0 = a0 & ~mask;
    } else {
      a0 = bp0;
      rb = 0n;
      sb = 1n;
    }
  }

  if (bx > EMAX_DEFAULT) {
    return mpfr_overflow(p, rnd, sign);
  }

  let ternary: Ternary = 0;
  let addOneUlp = false;
  if (rb === 0n && sb === 0n) {
    ternary = 0;
  } else if (rnd === 'RNDN') {
    if (rb === 0n || (sb === 0n && (a0 & (1n << sh)) === 0n)) {
      ternary = (-sign) as Ternary;
    } else {
      addOneUlp = true;
    }
  } else if (isLikeRNDZ(rnd, sign)) {
    ternary = (-sign) as Ternary;
  } else {
    addOneUlp = true;
  }
  if (addOneUlp) {
    a0 = a0 + (1n << sh);
    if (a0 > LIMB_MASK) {
      a0 = HIGHBIT;
      if (bx + 1n > EMAX_DEFAULT) {
        return mpfr_overflow(p, rnd, sign);
      }
      bx = bx + 1n;
    }
    ternary = sign;
  }
  const ts_mant = a0 >> sh;

  // The broken port may produce a value with the MSB unset (the carry
  // skip can give mant < 2^(p-1)). In that case the schema-validate
  // gate in the harness will reject the result. We surface as a
  // diagnostic — the broken port is allowed to fail loudly.
  if ((ts_mant & (1n << (p - 1n))) === 0n) {
    // Fudge the MSB so we don't throw schema-violation — let the
    // grader compare against the golden and find the mismatch.
    // (Without this fudge, the broken port would fail with EPREC
    // on every case, which counts as n_throw rather than n_pass.
    // We want it to compare wrong, so it scores low against the
    // golden value-mismatch, not on the throw path.)
    const fudged = ts_mant | (1n << (p - 1n));
    const value: MPFR = { kind: 'normal', sign, prec: p, exp: bx, mant: fudged };
    return { value, ternary };
  }

  const value: MPFR = {
    kind: 'normal',
    sign,
    prec: p,
    exp: bx,
    mant: ts_mant,
  };
  return { value, ternary };
}
