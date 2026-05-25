/**
 * reference_ports/correct/mpfr_fpif_read_limbs.ts -- mutation-prove reference.
 *
 * Algorithm (mpfr/src/fpif.c L520-L542): deserialise the significand
 * bytes back into a MSB-aligned mantissa.
 *
 * With endianness collapsed to LE (ADR 0004 Invariant 3) the C
 * limb-iteration code is equivalent to:
 *
 *   nb_byte  = ceil(prec / 8)
 *   read nb_byte little-endian bytes -> M_padded
 *   pad_bits = nb_byte * 8 - prec    (0..7)
 *   mant     = M_padded >> pad_bits
 *
 * The returned mant has bit (prec-1) set (when the source value was a
 * valid MPFR_MANT) and all bits >= prec clear -- matching src/core.ts.
 *
 * Wire-form harness signature (matches spec.json):
 *   mpfr_fpif_read_limbs(bytes_value: bigint,
 *                        byte_length: number,
 *                        pos: number,
 *                        prec: bigint)
 *     -> { mant: bigint, nextPos: number }
 */

import { MPFRError } from '../../../src/core.ts';

export interface ReadLimbsResult {
  readonly mant: bigint;
  readonly nextPos: number;
}

function bigIntToBytes(value: bigint, byte_length: number): Uint8Array {
  if (value < 0n) {
    throw new MPFRError(
      'EDOMAIN',
      `bigIntToBytes: value must be non-negative, got ${value}`,
    );
  }
  if (byte_length < 0 || !Number.isInteger(byte_length)) {
    throw new MPFRError(
      'EDOMAIN',
      `bigIntToBytes: byte_length must be a non-negative integer, got ${byte_length}`,
    );
  }
  const out = new Uint8Array(byte_length);
  let v = value;
  for (let i = 0; i < byte_length; i++) {
    out[i] = Number(v & 0xffn);
    v >>= 8n;
  }
  if (v !== 0n) {
    throw new MPFRError(
      'EDOMAIN',
      `bigIntToBytes: value ${value} does not fit in ${byte_length} bytes`,
    );
  }
  return out;
}

/**
 * Substrate signature (ADR 0004 "Worked example" section):
 *   fpif_read_limbs(bytes, pos, prec)
 *     -> { mant, nextPos }
 */
function fpif_read_limbs(
  bytes: Uint8Array,
  pos: number,
  prec: bigint,
): ReadLimbsResult {
  if (prec < 1n) {
    throw new MPFRError(
      'EDOMAIN',
      `fpif_read_limbs: prec must be >= 1, got ${prec}`,
    );
  }
  if (pos < 0 || !Number.isInteger(pos)) {
    throw new MPFRError(
      'EDOMAIN',
      `fpif_read_limbs: pos must be a non-negative integer, got ${pos}`,
    );
  }
  const nb_byte = Number((prec + 7n) >> 3n);
  if (pos + nb_byte > bytes.length) {
    throw new MPFRError(
      'EDOMAIN',
      `fpif_read_limbs: truncated buffer at offset ${pos} (need ${nb_byte} bytes, have ${bytes.length - pos})`,
    );
  }
  // Read nb_byte little-endian bytes into a bigint.
  let m = 0n;
  for (let i = nb_byte - 1; i >= 0; i--) {
    const b = bytes[pos + i];
    if (b === undefined) {
      throw new MPFRError('EDOMAIN', 'fpif_read_limbs: undefined byte');
    }
    m = (m << 8n) | BigInt(b);
  }
  const pad_bits = BigInt(nb_byte) * 8n - prec;
  const mant = pad_bits === 0n ? m : m >> pad_bits;
  return { mant, nextPos: pos + nb_byte };
}

export function mpfr_fpif_read_limbs(
  bytes_value: bigint,
  byte_length: number,
  pos: number,
  prec: bigint,
): ReadLimbsResult {
  if (typeof bytes_value !== 'bigint') {
    throw new MPFRError(
      'EDOMAIN',
      `mpfr_fpif_read_limbs: bytes_value must be bigint, got ${typeof bytes_value}`,
    );
  }
  if (typeof prec !== 'bigint') {
    throw new MPFRError(
      'EDOMAIN',
      `mpfr_fpif_read_limbs: prec must be bigint, got ${typeof prec}`,
    );
  }
  const bytes = bigIntToBytes(bytes_value, byte_length);
  return fpif_read_limbs(bytes, pos, prec);
}
