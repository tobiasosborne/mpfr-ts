/**
 * reference_ports/correct/mpfr_fpif_read_precision_from_file.ts -- mutation-prove reference.
 *
 * Algorithm (mpfr/src/fpif.c L248-L302): deserialise an fpif-encoded
 * precision from a byte buffer at a given cursor.
 *
 * Format (fpif.c L30-L43):
 *   * Read 1 byte B.
 *   * If B > 7 (MPFR_MAX_PRECSIZE): precision = B - 7, advance 1 byte.
 *   * Else: precision_size = B + 1; read precision_size bytes
 *     little-endian into copy; precision = copy + 249.
 *
 * Wire-form harness signature (matches spec.json):
 *   mpfr_fpif_read_precision_from_file(bytes_value: bigint,
 *                                       byte_length: number,
 *                                       pos: number)
 *     -> { precision: bigint, nextPos: number }
 */

import { MPFRError } from '../core.ts';

const MPFR_MAX_PRECSIZE = 7;
const MPFR_MAX_EMBEDDED_PRECISION = 248n;

export interface ReadPrecisionResult {
  readonly precision: bigint;
  readonly nextPos: number;
}

/** Reconstruct a Uint8Array of length `byte_length` from its little-endian
 *  unsigned-integer interpretation `value`. */
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
 *   fpif_read_precision(bytes, pos) -> { precision, nextPos }
 *
 * Throws MPFRError('EDOMAIN') on malformed input (truncated buffer,
 * precision encoding overflows mpfr_prec_t).
 */
function fpif_read_precision(
  bytes: Uint8Array,
  pos: number,
): ReadPrecisionResult {
  if (pos < 0 || !Number.isInteger(pos)) {
    throw new MPFRError(
      'EDOMAIN',
      `fpif_read_precision: pos must be a non-negative integer, got ${pos}`,
    );
  }
  if (pos >= bytes.length) {
    throw new MPFRError(
      'EDOMAIN',
      `fpif_read_precision: truncated buffer (pos=${pos}, len=${bytes.length})`,
    );
  }
  const first = bytes[pos];
  if (first === undefined) {
    throw new MPFRError('EDOMAIN', 'fpif_read_precision: undefined byte');
  }
  let cursor = pos + 1;
  if (first > MPFR_MAX_PRECSIZE) {
    return {
      precision: BigInt(first - MPFR_MAX_PRECSIZE),
      nextPos: cursor,
    };
  }
  const precision_size = first + 1;
  // C asserts precision_size <= MPFR_MAX_PRECSIZE + 1 (== 8).
  if (precision_size > 8) {
    throw new MPFRError(
      'EDOMAIN',
      `fpif_read_precision: invalid precision_size ${precision_size}`,
    );
  }
  if (cursor + precision_size > bytes.length) {
    throw new MPFRError(
      'EDOMAIN',
      `fpif_read_precision: truncated payload at offset ${cursor}`,
    );
  }
  let copy = 0n;
  for (let i = 0; i < precision_size; i++) {
    const b = bytes[cursor + i];
    if (b === undefined) {
      throw new MPFRError('EDOMAIN', 'fpif_read_precision: undefined byte');
    }
    copy |= BigInt(b) << BigInt(8 * i);
  }
  // C rejects encodings whose top bit of the 8th byte is set (would
  // overflow signed mpfr_prec_t).
  if (precision_size === 8) {
    const top = bytes[cursor + 7];
    if (top === undefined) {
      throw new MPFRError('EDOMAIN', 'fpif_read_precision: undefined byte');
    }
    if ((top & 0x80) !== 0) {
      throw new MPFRError(
        'EDOMAIN',
        'fpif_read_precision: precision does not fit in mpfr_prec_t',
      );
    }
  }
  cursor += precision_size;
  return {
    precision: copy + (MPFR_MAX_EMBEDDED_PRECISION + 1n),
    nextPos: cursor,
  };
}

export function mpfr_fpif_read_precision_from_file(
  bytes_value: bigint,
  byte_length: number,
  pos: number,
): ReadPrecisionResult {
  if (typeof bytes_value !== 'bigint') {
    throw new MPFRError(
      'EDOMAIN',
      `mpfr_fpif_read_precision_from_file: bytes_value must be bigint, got ${typeof bytes_value}`,
    );
  }
  const bytes = bigIntToBytes(bytes_value, byte_length);
  return fpif_read_precision(bytes, pos);
}
