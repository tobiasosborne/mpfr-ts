/**
 * reference_ports/correct/mpfr_fpif_store_limbs.ts -- mutation-prove reference.
 *
 * Algorithm (mpfr/src/fpif.c L483-L511): encode the mantissa of a
 * regular MPFR value into the fpif binary format. The C implementation
 * iterates over MPFR's internal limb array; we mirror the byte-level
 * output via the equivalent direct bigint formulation:
 *
 *   nb_byte  = ceil(prec / 8)
 *   pad_bits = nb_byte * 8 - prec    (0..7)
 *   M        = mant << pad_bits      (MSB-aligned padded mantissa)
 *   bytes    = M as nb_byte little-endian unsigned-int bytes
 *
 * This produces identical bytes to the C limb-iteration path once host
 * endianness is fixed to LE (ADR 0004 Invariant 3). See the
 * golden_driver.c top docstring for the equivalence derivation.
 *
 * Wire-form harness signature (matches eval/functions/mpfr_fpif_store_limbs/spec.json):
 *   mpfr_fpif_store_limbs(x: MPFR) -> { bytes: bigint, byte_length: number }
 */

import type { MPFR } from '../../../src/core.ts';
import { MPFRError } from '../../../src/core.ts';

export interface StoreLimbsResult {
  readonly bytes: bigint;
  readonly byte_length: number;
}

function bytesToBigInt(bytes: Uint8Array): bigint {
  let v = 0n;
  for (let i = bytes.length - 1; i >= 0; i--) {
    const b = bytes[i];
    if (b === undefined) {
      throw new Error('mpfr_fpif_store_limbs: undefined byte');
    }
    v = (v << 8n) | BigInt(b);
  }
  return v;
}

/**
 * Substrate signature (ADR 0004 "Worked example" section):
 *   fpif_store_limbs(x: MPFR): Uint8Array
 */
function fpif_store_limbs(x: MPFR): Uint8Array {
  if (x.kind !== 'normal') {
    throw new MPFRError(
      'EDOMAIN',
      `fpif_store_limbs: x.kind must be 'normal', got '${x.kind}'`,
    );
  }
  if (x.prec < 1n) {
    throw new MPFRError(
      'EDOMAIN',
      `fpif_store_limbs: prec must be >= 1, got ${x.prec}`,
    );
  }
  const nb_byte = (x.prec + 7n) >> 3n;
  const pad_bits = nb_byte * 8n - x.prec;
  // x.mant has bit (prec-1) set, all bits >= prec clear. Shifting left
  // by pad_bits aligns the mantissa to fill exactly nb_byte * 8 bits
  // (so the new bit (nb_byte*8 - 1) is set; the bottom pad_bits bits
  // are zero).
  let M = x.mant << pad_bits;
  const n = Number(nb_byte);
  const out = new Uint8Array(n);
  for (let i = 0; i < n; i++) {
    out[i] = Number(M & 0xffn);
    M >>= 8n;
  }
  return out;
}

export function mpfr_fpif_store_limbs(x: MPFR): StoreLimbsResult {
  if (x === null || typeof x !== 'object') {
    throw new MPFRError(
      'EDOMAIN',
      `mpfr_fpif_store_limbs: x must be an MPFR object`,
    );
  }
  const buf = fpif_store_limbs(x);
  return { bytes: bytesToBigInt(buf), byte_length: buf.length };
}
