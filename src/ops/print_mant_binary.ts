/**
 * mpfr_print_mant_binary -- pure-TS port of MPFR's debug mantissa printer.
 *
 * Returns the formatted string instead of printing to stdout. Mirrors the
 * C output layout exactly, including '[' after the r-th bit and matching ']'.
 *
 * Ref: mpfr/src/print_raw.c L26-L47 -- C reference body.
 *   The C function prints limb bits MSB-first, inserts '[' after count == r,
 *   and ']' after the final limb if count >= r.
 *
 * Ref: src/core.ts -- MPFR shape; mant is MSB-aligned to prec bits.
 *   We reconstruct the C limb layout by left-shifting mant by (n*64 - r)
 *   so the significand occupies the top bits of the limb array.
 */

import type { MPFR } from "../core.ts";
import { MPFRError } from "../core.ts";

const MASK_64 = (1n << 64n) - 1n;

/**
 * Format the internal binary representation of a significand.
 *
 * @param str Prefix string (printed followed by a space before the bits).
 * @param x   MPFR value (must be 'normal'; singular values throw EDOMAIN).
 * @returns   Formatted string matching mpfr_print_mant_binary output.
 *
 * @throws {MPFRError} EDOMAIN if x is not a normal finite value.
 */
export function mpfr_print_mant_binary(str: string, x: MPFR): string {
  if (typeof str !== 'string') {
    throw new MPFRError('EDOMAIN',
      `mpfr_print_mant_binary: str must be string, got ${typeof str}`);
  }
  if (x === null || typeof x !== 'object') {
    throw new MPFRError('EPREC', 'mpfr_print_mant_binary: x must be MPFR');
  }
  if (x.kind !== 'normal') {
    throw new MPFRError('EDOMAIN',
      `mpfr_print_mant_binary: only defined on normal values, got kind=${x.kind}`);
  }

  // MPFR_PREC2LIMBS: n = ceil(r / GMP_NUMB_BITS) where GMP_NUMB_BITS = 64.
  const r = x.prec;
  const n = (r + 63n) / 64n;
  const totalBits = n * 64n;
  const padShift = totalBits - r; // bits to left-shift mant to fill top of limb array
  const padded = x.mant << padShift;

  let out = `${str} `;
  let count = 0n;

  // Walk limbs from most-significant (n-1) down to least-significant (0).
  for (let li = n - 1n; li >= 0n; li--) {
    const limb = (padded >> (li * 64n)) & MASK_64;

    // Print bits MSB-first within this limb.
    for (let i = 63n; i >= 0n; i--) {
      out += (limb >> i) & 1n ? '1' : '0';
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
