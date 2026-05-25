/**
 * reference_ports/correct/mpfr_fpif_store_precision.ts -- mutation-prove reference.
 *
 * Algorithm (mpfr/src/fpif.c L208-L240): encode a precision into the
 * fpif binary header. Reproduced verbatim using native BigInt and
 * Uint8Array, with the host-endian C abstraction collapsed to a
 * single little-endian path (ADR 0004 Invariant 3).
 *
 * Format (fpif.c L30-L43):
 *   * 1 <= p <= 248 : single byte B = p + 7.
 *   * 249 <= p     : B = size_precision - 1 (one byte 0..7), then
 *                    size_precision bytes hold (p - 249) little-endian.
 *
 * Wire-form harness signature (matches eval/functions/mpfr_fpif_store_precision/spec.json):
 *   mpfr_fpif_store_precision(precision: bigint) -> { bytes: bigint, byte_length: number }
 *
 * The actual substrate Uint8Array is converted to a (decimal-bigint,
 * byte_length) pair so the grader can use the existing scalar-bigint
 * codec path (ADR 0004 Wire encoding).
 */

import { MPFRError } from '../core.ts';

const MPFR_MAX_PRECSIZE = 7n;
const MPFR_MAX_EMBEDDED_PRECISION = 248n; // 255 - 7

export interface StorePrecisionResult {
  readonly bytes: bigint;
  readonly byte_length: number;
}

/** Convert a Uint8Array to its little-endian unsigned bigint value. */
function bytesToBigInt(bytes: Uint8Array): bigint {
  let v = 0n;
  for (let i = bytes.length - 1; i >= 0; i--) {
    const b = bytes[i];
    if (b === undefined) {
      throw new Error('mpfr_fpif_store_precision: undefined byte');
    }
    v = (v << 8n) | BigInt(b);
  }
  return v;
}

/**
 * Substrate signature (ADR 0004 "Worked example" section):
 *   fpif_store_precision(precision: bigint): Uint8Array
 *
 * Throws MPFRError('EDOMAIN') on precision < 1n.
 */
function fpif_store_precision(precision: bigint): Uint8Array {
  if (precision < 1n) {
    throw new MPFRError(
      'EDOMAIN',
      `fpif_store_precision: precision must be >= 1, got ${precision}`,
    );
  }
  if (precision <= MPFR_MAX_EMBEDDED_PRECISION) {
    const out = new Uint8Array(1);
    // p + 7 fits in [8, 255]; safe to narrow via Number().
    out[0] = Number(precision + MPFR_MAX_PRECSIZE);
    return out;
  }
  // Extended form. Compute size_precision = number of bytes needed to
  // encode (precision - 249) little-endian, by repeatedly shifting until
  // the residue is zero. Mirrors the COUNT_NB_BYTE macro semantics.
  let copy = precision - (MPFR_MAX_EMBEDDED_PRECISION + 1n);
  let size_precision = 0;
  let tmp = copy;
  do {
    tmp >>= 8n;
    size_precision++;
  } while (tmp !== 0n);
  // The C TODO at L313 caps exponents at 16 bytes; for precision the
  // header byte (size_precision - 1) is in 0..7, so size_precision is
  // bounded by 8 in the format-defined range. Allow more here only as
  // a defensive cap (callers passing > 2^64 + 248 would exceed it).
  if (size_precision > 8) {
    throw new MPFRError(
      'EDOMAIN',
      `fpif_store_precision: precision exceeds 2^64 + 248 (size_precision=${size_precision})`,
    );
  }
  const out = new Uint8Array(1 + size_precision);
  out[0] = size_precision - 1;
  for (let i = 0; i < size_precision; i++) {
    out[1 + i] = Number(copy & 0xffn);
    copy >>= 8n;
  }
  return out;
}

/**
 * Wire-form wrapper for the harness. Calls the substrate function and
 * encodes the resulting Uint8Array as (bytes, byte_length) per ADR 0004.
 */
export function mpfr_fpif_store_precision(
  precision: bigint,
): StorePrecisionResult {
  if (typeof precision !== 'bigint') {
    throw new MPFRError(
      'EDOMAIN',
      `mpfr_fpif_store_precision: precision must be bigint, got ${typeof precision}`,
    );
  }
  const buf = fpif_store_precision(precision);
  return { bytes: bytesToBigInt(buf), byte_length: buf.length };
}
