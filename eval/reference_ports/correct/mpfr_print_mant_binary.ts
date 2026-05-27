/**
 * reference_ports/correct/mpfr_print_mant_binary.ts -- mutation-prove reference.
 *
 * Format the internal binary representation of a significand as a string,
 * mirroring the C output format of mpfr_print_mant_binary (which prints
 * to stdout) but as a pure function returning the string.
 *
 * Algorithm (mpfr/src/print_raw.c L26-L47):
 *   n = ceil(r / 64) -- number of 64-bit limbs.
 *   Output starts with `str + ' '` (prefix + space).
 *   For each limb index from n-1 down to 0:
 *     For each bit position from 63 down to 0:
 *       Print '0' or '1' depending on the bit.
 *       After printing r bits total, print '['.
 *     Print '.' after each limb.
 *   If count >= r at the end, print ']'.
 *   Trailing newline.
 *
 * Limb reconstruction from MPFR shape:
 *   The C side reads from a raw mp_limb_t array p where p[0] is the
 *   least-significant 64-bit chunk, and the mantissa is right-padded
 *   with zeros (the low `n*64 - r` bits of p[0] are zero per the schema).
 *   The TS port reproduces this by computing
 *     padded = x.mant << (n*64 - r)
 *   then splitting padded into 64-bit chunks: limb[i] = (padded >> (64*i)) & 0xFFFFFFFFFFFFFFFFn.
 *
 * Ref: mpfr/src/print_raw.c L26-L47 -- C reference.
 * Ref: src/core.ts -- MPFR shape; mant is MSB-aligned to prec bits.
 */

import type { MPFR } from '../../../src/core.ts';
import { MPFRError } from '../../../src/core.ts';

const MASK_64 = (1n << 64n) - 1n;

export function mpfr_print_mant_binary(str: string, x: MPFR): string {
  if (typeof str !== 'string') {
    throw new MPFRError('EDOMAIN', `mpfr_print_mant_binary: str must be string, got ${typeof str}`);
  }
  if (x === null || typeof x !== 'object') {
    throw new MPFRError('EPREC', 'mpfr_print_mant_binary: x must be MPFR');
  }
  if (x.kind !== 'normal') {
    throw new MPFRError(
      'EDOMAIN',
      `mpfr_print_mant_binary: only defined on normal values, got kind=${x.kind}`,
    );
  }

  const r = x.prec; // bigint
  // MPFR_PREC2LIMBS: n = ceil(r / 64) = (r + 63) >> 6.
  const n = (r + 63n) / 64n;
  const totalBits = n * 64n;
  const padShift = totalBits - r; // 0n..63n
  const padded = x.mant << padShift;

  let out = `${str} `;
  let count = 0n;
  // Walk limbs from n-1 down to 0.
  for (let li = n - 1n; li >= 0n; li--) {
    const limb = (padded >> (li * 64n)) & MASK_64;
    // Print bits MSB-first within this limb.
    for (let i = 63n; i >= 0n; i--) {
      const bit = (limb >> i) & 1n;
      out += bit === 1n ? '1' : '0';
      count++;
      if (count === r) {
        out += '[';
      }
    }
    out += '.';
  }
  if (count >= r) {
    out += ']';
  }
  out += '\n';
  return out;
}
