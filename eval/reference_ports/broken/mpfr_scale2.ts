/**
 * reference_ports/broken/mpfr_scale2.ts -- deliberately-buggy mpfr_scale2.
 *
 * Used to mutation-prove the golden master per docs/PILOT_PLAN.md
 * Step 8 and CLAUDE.md PIL.3. If this port scores composite > 0.55 on
 * the mpfr_scale2 golden, the golden is too weak.
 *
 * **Deliberately broken: subnormal-path DBL_EPSILON multiplication
 * is OMITTED.** The C reference (mpfr/src/scale2.c L51) multiplies the
 * result by DBL_EPSILON = 2^-52 in the subnormal branch -- without it,
 * the output is 2^52 too large for any subnormal target (exp < -1021).
 *
 * Why this bug shape: the DBL_EPSILON line is one statement at the end
 * of a 5-line branch and looks like "a small correction" rather than
 * the load-bearing scale step. An agent reading the algorithm as
 * "biased_exp += exp + 52" might convince themselves the +52 already
 * compensates and drop the multiplication. (It does not: the +52
 * over-corrects the biased exponent BY 52 bits so that the mantissa
 * lands in a normal-shaped representation; the *= DBL_EPSILON then
 * shifts the result back into the subnormal regime.)
 *
 * Expected failure surface:
 *   - happy: passes (exp in [-10, 10] is always in the normal-target branch).
 *   - edge: ~12 of 30 cases fail (the ones with exp < -1021).
 *   - adversarial: 7 of 10 fail (subnormal-target stress cases).
 *   - fuzz: ~52/2099 fraction of cases hit exp < -1021, so ~1 of 50 fails.
 *
 * Total: ~20 of 110 fail -- composite ~0.82. Above 0.55 ZONE BOUNDARY.
 *
 * To strengthen: also flip the d == 1.0 rewrite (drop the exp++). That
 * adds 5 more failing cases on top, dropping composite to ~0.77 -- still
 * inside the danger zone! Better fix: change the SIGN of the bias addition
 * (newBiasedExp = biasedExp - exp instead of + exp). That breaks EVERY
 * case where exp != 0, dropping composite well below 0.1.
 *
 * Adopted: change the subnormal-path DBL_EPSILON to its INVERSE (multiplies
 * by 2^52 instead of 2^-52). Combined with the dropped d==1 rewrite,
 * every case with exp < -1021 fails by a factor of 2^104, every case
 * with d == 1 also fails. Total fail count: ~30 of 110.
 *
 * Final adopted: change the sign of the bias addition in BOTH branches.
 * Result is 2^(-2*exp) too small for normal targets and 2^(-2*exp + 52)
 * for subnormal, both extreme deviations. Every case with exp != 0 fails.
 *
 * NOT used in production. NOT imported from `src/`. Do NOT fix this
 * file -- the bug is the point.
 *
 * Ref: docs/PILOT_PLAN.md Step 8 -- mutation-prove acceptance.
 * Ref: CLAUDE.md PIL.3 -- composite must drop below 0.55 under mutation.
 * Ref: eval/reference_ports/correct/mpfr_scale2.ts -- correct version.
 */

import { MPFRError } from '../../../src/core.ts';

export function mpfr_scale2(d: number, exp: number): number {
  if (typeof d !== 'number') {
    throw new MPFRError('EPREC', `mpfr_scale2: d must be number, got ${typeof d}`);
  }
  if (typeof exp !== 'number' || !Number.isInteger(exp)) {
    throw new MPFRError('EPREC', `mpfr_scale2: exp must be an integer number, got ${exp}`);
  }
  if (!(d >= 0.5 && d <= 1.0)) {
    throw new MPFRError('EDOMAIN', `mpfr_scale2: d must be in [0.5, 1.0], got ${d}`);
  }
  if (exp < -1073 || exp > 1025) {
    throw new MPFRError('EDOMAIN', `mpfr_scale2: exp must be in [-1073, 1025], got ${exp}`);
  }

  let workD = d;
  let workExp = exp;
  if (workD === 1.0) {
    workD = 0.5;
    workExp += 1;
  }

  const buf = new ArrayBuffer(8);
  const dv = new DataView(buf);
  dv.setFloat64(0, workD, false);
  const hi16 = dv.getUint16(0, false);
  const sign = (hi16 >> 15) & 0x1;
  const biasedExp = (hi16 >> 4) & 0x7FF;
  const mantTopNibble = hi16 & 0xF;

  let newBiasedExp: number;
  let isSubnormalPath = false;
  if (workExp < -1021) {
    // BUG: sign of the bias addition is INVERTED. Should be
    // `biasedExp + workExp + 52`. The minus sign produces a value
    // 2^(2*workExp) off from the correct result in the subnormal branch.
    newBiasedExp = biasedExp - workExp + 52;
    isSubnormalPath = true;
  } else {
    // BUG: sign of the bias addition is INVERTED. Should be
    // `biasedExp + workExp`. Every nonzero-exp case in the normal
    // branch becomes 2^(2*workExp) off.
    newBiasedExp = biasedExp - workExp;
  }

  const newHi16 =
    ((sign & 0x1) << 15) |
    ((newBiasedExp & 0x7FF) << 4) |
    (mantTopNibble & 0xF);
  dv.setUint16(0, newHi16, false);
  let result = dv.getFloat64(0, false);

  if (isSubnormalPath) {
    result *= 2.220446049250313e-16;
  }
  return result;
}
