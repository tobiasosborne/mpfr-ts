/**
 * reference_ports/correct/mpfr_scale2.ts -- mutation-prove reference for
 * mpfr_scale2.
 *
 * Per CLAUDE.md PIL.3 / docs/PILOT_PLAN.md Step 7, this file is the
 * calibration baseline. The production src/ops/scale2.ts does not yet
 * exist; the orchestrator will materialise it during the port-and-grade
 * flow. This file mirrors the IEEE-754 fast path from
 * mpfr/src/scale2.c L32-L58 byte-for-byte via DataView.
 *
 * Algorithm (mpfr/src/scale2.c L32-L58):
 *   if (d == 1.0) { d = 0.5; exp++; }            // now d in [0.5, 1)
 *   read biased_exp field (11 bits) from d;
 *   if (exp < -1021) {                            // subnormal target
 *     biased_exp += exp + 52;
 *     write back; d *= DBL_EPSILON;
 *   } else {                                      // normal target
 *     biased_exp += exp;
 *     write back;
 *   }
 *   return d;
 *
 * IEEE-754 binary64 layout (big-endian byte view):
 *   byte 0: sign (bit 7) | biased_exp[10..4] (bits 6..0)
 *   byte 1: biased_exp[3..0] (bits 7..4) | mant[51..48] (bits 3..0)
 *   bytes 2-7: mant[47..0]
 *
 * The TS port uses a DataView with BIG-ENDIAN byte order to make the
 * exponent field accessible via getUint16(0) >> 4 & 0x7FF, mirroring
 * the C union-based mpfr_ieee_double_extract access.
 *
 * Precondition handling: the spec.json says we validate d in [0.5, 1.0]
 * and exp in [-1073, 1025] (the C-asserted envelope). Outside that,
 * throw MPFRError 'EDOMAIN'. The golden_driver stays inside the envelope.
 *
 * Ref: mpfr/src/scale2.c -- C reference.
 * Ref: eval/functions/mpfr_scale2/spec.json -- contract.
 * Ref: src/ops/get_d.ts -- sister port using the same DataView pattern.
 */

import { MPFRError } from '../../../src/core.ts';

export function mpfr_scale2(d: number, exp: number): number {
  if (typeof d !== 'number') {
    throw new MPFRError('EPREC', `mpfr_scale2: d must be number, got ${typeof d}`);
  }
  if (typeof exp !== 'number' || !Number.isInteger(exp)) {
    throw new MPFRError('EPREC', `mpfr_scale2: exp must be an integer number, got ${exp}`);
  }
  // The C source asserts 1/2 <= d <= 1 and -1073 <= exp <= 1025; we
  // model the assertion as a domain error.
  if (!(d >= 0.5 && d <= 1.0)) {
    throw new MPFRError('EDOMAIN', `mpfr_scale2: d must be in [0.5, 1.0], got ${d}`);
  }
  if (exp < -1073 || exp > 1025) {
    throw new MPFRError('EDOMAIN', `mpfr_scale2: exp must be in [-1073, 1025], got ${exp}`);
  }

  let workD = d;
  let workExp = exp;

  // Step 1: d == 1.0 rewrite. (mpfr/src/scale2.c L36-L40.)
  if (workD === 1.0) {
    workD = 0.5;
    workExp += 1;
  }
  // Now 1/2 <= workD < 1.

  // Step 2: read biased_exp field via DataView (big-endian).
  const buf = new ArrayBuffer(8);
  const dv = new DataView(buf);
  dv.setFloat64(0, workD, false);  // false = big-endian
  // Bytes [0..1] carry: sign (1) | biased_exp (11) | top 4 mantissa bits.
  // Read those 16 bits as a single u16.
  const hi16 = dv.getUint16(0, false);
  const sign = (hi16 >> 15) & 0x1;
  const biasedExp = (hi16 >> 4) & 0x7FF;
  const mantTopNibble = hi16 & 0xF;

  // For workD in [0.5, 1), biasedExp == 1022 (unbiased = -1). We don't
  // require this in code -- we just add workExp into the exponent
  // field, matching the C union arithmetic.

  let newBiasedExp: number;
  let isSubnormalPath = false;
  if (workExp < -1021) {
    // Subnormal target. (mpfr/src/scale2.c L48-L52.)
    newBiasedExp = biasedExp + workExp + 52;
    isSubnormalPath = true;
  } else {
    // Normal target. (mpfr/src/scale2.c L53-L56.)
    newBiasedExp = biasedExp + workExp;
  }

  // Reassemble the high 16 bits with the new biased exponent.
  const newHi16 =
    ((sign & 0x1) << 15) |
    ((newBiasedExp & 0x7FF) << 4) |
    (mantTopNibble & 0xF);
  dv.setUint16(0, newHi16, false);
  let result = dv.getFloat64(0, false);

  if (isSubnormalPath) {
    // The biased_exp += exp + 52 trick produced an over-large field
    // that we now scale back via DBL_EPSILON = 2^-52. (mpfr/src/scale2.c L51.)
    result *= 2.220446049250313e-16;  // exactly 2^-52
  }
  return result;
}
