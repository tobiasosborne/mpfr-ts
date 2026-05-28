/**
 * reference_ports/correct/mpfr_round_near_x.ts -- calibration reference
 * for the SUBSTRATE helper mpfr_round_near_x.
 *
 * Faithful port of mpfr/src/round_near_x.c L154-L235. Decides whether an
 * approximation v (with a known error bound) can be rounded to prec(y)
 * bits to yield the correctly-rounded result y. Used by the
 * transcendental functions via MPFR_FAST_COMPUTE_IF_SMALL_INPUT.
 *
 * SUBSTRATE class: an internal helper that depends on the substrate
 * primitives mpfr_round_p (round_p.ts) and the round-raw machinery
 * (round_raw.ts roundMantissa), plus mpfr_nexttozero / mpfr_nexttoinf.
 * The runner's requireCoreImport AST gate is exempt
 * (eval/harness/runner.ts L1188); the production port belongs in
 * src/internal/mpfr/. This reference imports from src/internal and
 * src/ops only as a faithful oracle for mutation-proving.
 *
 * Contract (round_near_x.c L26-L45)
 * ---------------------------------
 *   v        non-singular MPFR approximation.
 *   yprec    precision of the destination y (bits).
 *   err      mpfr_uexp_t error term: |g(x)| < 2^(EXP(v)-err).
 *   dir      0 => error toward zero (f(x) < x); 1 => away (f(x) > x).
 *   rnd      rounding mode.
 *
 *   Returns { value: null, ret: 0 } when it CANNOT round (C leaves y
 *   untouched). Otherwise { value: <rounded y>, ret: <ternary> } where
 *   ret is +-1 (never 0; an exact value cannot be detected here).
 *
 * Refs
 * ----
 *   - mpfr/src/round_near_x.c L154-L235 -- the C body.
 *   - mpfr/src/round_near_x.c L171-L177 -- the cannot-round gate.
 *   - src/internal/mpfr/round_p.ts -- mpfr_round_p dependency.
 *   - src/internal/mpfr/round_raw.ts -- roundMantissa (round-raw step).
 *   - src/ops/nexttozero.ts, src/ops/nexttoinf.ts -- the dir fixups.
 *   - CLAUDE.md PIL.3 -- mutation-prove the golden against this file.
 */

import type { MPFR, RoundingMode, Sign } from '../../../src/core.ts';
import { mpfr_round_p } from '../../../src/internal/mpfr/round_p.ts';
import { roundMantissa } from '../../../src/internal/mpfr/round_raw.ts';
import { mpfr_nexttozero } from '../../../src/ops/nexttozero.ts';
import { mpfr_nexttoinf } from '../../../src/ops/nexttoinf.ts';

const LIMB_BITS = 64n;

/** Number of limbs to hold `prec` bits (MPFR_LIMB_SIZE). */
function limbSize(prec: bigint): number {
  return Number((prec + LIMB_BITS - 1n) / LIMB_BITS);
}

/**
 * Build the GMP-style limb array MPFR keeps for a mantissa: the value
 * occupies the top `prec` bits of an `LIMB_SIZE(prec)`-limb little-endian
 * array, MSB of the top limb set, low pad bits zero. The TS schema's
 * `mant` is MSB-aligned to `prec` bits (MSB at bit prec-1); shift it up
 * so its MSB lands at bit 63 of the top limb.
 */
function mantToLimbs(mant: bigint, prec: bigint): bigint[] {
  const n = limbSize(prec);
  const totalBits = BigInt(n) * LIMB_BITS;
  // pad = bits of zero padding below the prec significant bits.
  const padded = mant << (totalBits - prec);
  const limbs: bigint[] = new Array<bigint>(n);
  let v = padded;
  const mask = (1n << LIMB_BITS) - 1n;
  for (let i = 0; i < n; i++) {
    limbs[i] = v & mask;
    v >>= LIMB_BITS;
  }
  return limbs;
}

/** MPFR_IS_LIKE_RNDZ(rnd, neg): rounds toward zero for this sign. */
function isLikeRndz(rnd: RoundingMode, neg: boolean): boolean {
  return rnd === 'RNDZ' || (rnd === 'RNDD' && !neg) || (rnd === 'RNDU' && neg);
}

/** MPFR_IS_LIKE_RNDA(rnd, neg): rounds away from zero for this sign. */
function isLikeRnda(rnd: RoundingMode, neg: boolean): boolean {
  return rnd === 'RNDA' || (rnd === 'RNDU' && !neg) || (rnd === 'RNDD' && neg);
}

export function mpfr_round_near_x(
  v: MPFR,
  yprec: bigint,
  err: bigint,
  dir: number,
  rnd: RoundingMode | 'RNDF',
): { value: MPFR | null; ret: number } {
  // round_near_x.c L161-L162: RNDF -> RNDZ.
  const rndMode: RoundingMode = rnd === 'RNDF' ? 'RNDZ' : rnd;

  // Precondition (L164): v non-singular.
  if (v.kind !== 'normal') {
    throw new Error('mpfr_round_near_x: v must be a non-singular value');
  }
  if (dir !== 0 && dir !== 1) {
    throw new Error('mpfr_round_near_x: dir must be 0 or 1');
  }

  const sign: Sign = v.sign;
  const neg = sign < 0;

  // ---- cannot-round gate (round_near_x.c L171-L177) ----
  // !(err > prec(y)+1 && (err > prec(v) || round_p(MANT(v), LIMB_SIZE(v),
  //                                                 err, prec(y)+(rnd==RNDN))))
  const canRoundFirst = err > yprec + 1n;
  let canRound = false;
  if (canRoundFirst) {
    if (err > v.prec) {
      canRound = true;
    } else {
      // round_p(MANT(v), LIMB_SIZE(v), err, prec(y) + (rnd==RNDN ? 1 : 0))
      const limbs = mantToLimbs(v.mant, v.prec);
      const rpPrec = yprec + (rndMode === 'RNDN' ? 1n : 0n);
      canRound = mpfr_round_p(limbs, err, rpPrec);
    }
  }
  if (!canRound) {
    // L176-L177: cannot round, y unmodified.
    return { value: null, ret: 0 };
  }

  // ---- round v into y (round_near_x.c L179-L193) ----
  // EXP(y)=EXP(v), SIGN(y)=SIGN(v); MPFR_RNDRAW_GEN rounds MANT(v) at
  // PREC(v) to prec(y) in mode rnd. The TRUNC handler (dir==0) sets
  // inexact=-sign and truncates; the ADDONEULP handler (dir==1) adds a
  // ulp. We mirror this with roundMantissa, handling the
  // prec(v) <= prec(y) "exact copy" case separately (roundMantissa
  // requires srcPrec > outPrec).
  let yKind: 'normal' = 'normal';
  let yMant: bigint;
  let yExp: bigint;
  let inexact: number;

  if (v.prec <= yprec) {
    // No rounding: setting v in y is exact at this stage (L108-L123 of
    // round_raw_generic, i.e. RNDRAW with sprec<=prec). Widen mant to
    // yprec bits (MSB-aligned), inexact = 0.
    yMant = v.mant << (yprec - v.prec);
    yExp = v.exp;
    inexact = 0;
  } else {
    const r = roundMantissa(v.mant, v.prec, v.exp, yprec, sign, rndMode);
    yMant = r.mant;
    yExp = r.exp;
    inexact = r.ternary;
  }

  let y: MPFR = {
    kind: yKind,
    sign,
    prec: yprec,
    exp: yExp,
    mant: yMant,
  };

  // ---- inexact==0 fixups (round_near_x.c L199-L229) ----
  if (inexact === 0) {
    if (dir === 0) {
      // L201-L214: error term negative for v positive.
      inexact = sign; // tentative
      if (isLikeRndz(rndMode, neg)) {
        // case nexttozero: step y one ulp toward zero.
        inexact = -sign;
        y = mpfr_nexttozero(y);
        // (underflow flag if y becomes zero -- not surfaced here.)
      }
    } else {
      // L215-L228: error term positive for v positive.
      inexact = -sign; // tentative
      if (isLikeRnda(rndMode, neg)) {
        // case nexttoinf: step y one ulp away from zero.
        inexact = sign;
        y = mpfr_nexttoinf(y);
        // (overflow flag if y becomes Inf -- not surfaced here.)
      }
    }
  }

  // L231-L233: inexact cannot be 0 here.
  // Normalise the ternary direction to a JS number in {-1, 1}.
  const ret = inexact > 0 ? 1 : inexact < 0 ? -1 : 0;
  return { value: y, ret };
}
