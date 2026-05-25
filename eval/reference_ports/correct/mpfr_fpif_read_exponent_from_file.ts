/**
 * reference_ports/correct/mpfr_fpif_read_exponent_from_file.ts -- mutation-prove reference.
 *
 * Algorithm (mpfr/src/fpif.c L399-L473): deserialise the
 * sign+kind+exponent byte field from a buffer at a given cursor.
 *
 * Wire-form harness signature (matches spec.json):
 *   mpfr_fpif_read_exponent_from_file(bytes_value: bigint,
 *                                      byte_length: number,
 *                                      pos: number,
 *                                      prec: bigint)
 *     -> { kind: MPFR['kind']; sign: 1 | -1; exp: bigint; nextPos: number }
 */

import type { MPFR } from '../../../src/core.ts';
import { MPFRError } from '../../../src/core.ts';

const MPFR_KIND_ZERO = 119;
const MPFR_KIND_INF = 120;
const MPFR_KIND_NAN = 121;
const MPFR_MAX_EMBEDDED_EXPONENT = 47n;
const MPFR_EXTERNAL_EXPONENT = 94;

// Default emin/emax (mpfr/src/mpfr.h L231-L232: MPFR_EMIN_DEFAULT /
// MPFR_EMAX_DEFAULT = +/-(2^30 - 1)). Used for the EXP_IN_RANGE check.
const DEFAULT_EMIN_MIN = -((1n << 30n) - 1n);
const DEFAULT_EMAX_MAX = (1n << 30n) - 1n;

export interface ReadExponentResult {
  readonly kind: MPFR['kind'];
  readonly sign: 1 | -1;
  readonly exp: bigint;
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
 *   fpif_read_exponent(bytes, pos, prec)
 *     -> { kind, sign, exp, nextPos }
 */
function fpif_read_exponent(
  bytes: Uint8Array,
  pos: number,
  _prec: bigint,
): ReadExponentResult {
  if (pos < 0 || !Number.isInteger(pos)) {
    throw new MPFRError(
      'EDOMAIN',
      `fpif_read_exponent: pos must be a non-negative integer, got ${pos}`,
    );
  }
  if (pos >= bytes.length) {
    throw new MPFRError(
      'EDOMAIN',
      `fpif_read_exponent: truncated buffer at offset ${pos}`,
    );
  }
  const b0 = bytes[pos];
  if (b0 === undefined) {
    throw new MPFRError('EDOMAIN', 'fpif_read_exponent: undefined byte');
  }
  let cursor = pos + 1;
  const sign: 1 | -1 = (b0 & 0x80) !== 0 ? -1 : 1;
  const e = b0 & 0x7f;

  if (e > MPFR_EXTERNAL_EXPONENT && e < MPFR_KIND_ZERO) {
    // Extended exponent path: next (e - 94) bytes hold |exp| - 47 with
    // the top bit of the last byte indicating sign.
    const exponent_size = e - MPFR_EXTERNAL_EXPONENT;
    if (exponent_size > 16) {
      throw new MPFRError(
        'EDOMAIN',
        `fpif_read_exponent: exponent_size ${exponent_size} > 16`,
      );
    }
    if (exponent_size > 8) {
      // C also checks exponent_size > sizeof(mpfr_exp_t). On the locked
      // schema mpfr_exp_t is treated as 64-bit, so > 8 is impossible to
      // represent and is rejected.
      throw new MPFRError(
        'EDOMAIN',
        `fpif_read_exponent: exponent_size ${exponent_size} > sizeof(mpfr_exp_t)`,
      );
    }
    if (cursor + exponent_size > bytes.length) {
      throw new MPFRError(
        'EDOMAIN',
        `fpif_read_exponent: truncated exponent payload at offset ${cursor}`,
      );
    }
    let uexp = 0n;
    for (let i = 0; i < exponent_size; i++) {
      const b = bytes[cursor + i];
      if (b === undefined) {
        throw new MPFRError('EDOMAIN', 'fpif_read_exponent: undefined byte');
      }
      uexp |= BigInt(b) << BigInt(8 * i);
    }
    const exp_sign_bit_pos = 1n << BigInt(8 * exponent_size - 1);
    const exp_sign_bit = uexp & exp_sign_bit_pos;
    uexp &= ~exp_sign_bit_pos;
    uexp += MPFR_MAX_EMBEDDED_EXPONENT;
    const exponent = exp_sign_bit !== 0n ? -uexp : uexp;
    if (exponent < DEFAULT_EMIN_MIN || exponent > DEFAULT_EMAX_MAX) {
      throw new MPFRError(
        'EDOMAIN',
        `fpif_read_exponent: exponent ${exponent} out of [${DEFAULT_EMIN_MIN}, ${DEFAULT_EMAX_MAX}]`,
      );
    }
    return {
      kind: 'normal',
      sign,
      exp: exponent,
      nextPos: cursor + exponent_size,
    };
  }
  if (e === MPFR_KIND_ZERO) {
    return { kind: 'zero', sign, exp: 0n, nextPos: cursor };
  }
  if (e === MPFR_KIND_INF) {
    return { kind: 'inf', sign, exp: 0n, nextPos: cursor };
  }
  if (e === MPFR_KIND_NAN) {
    return { kind: 'nan', sign, exp: 0n, nextPos: cursor };
  }
  if (e <= MPFR_EXTERNAL_EXPONENT) {
    // Embedded path. exp = E - 47, in [-47, 47].
    const exponent = BigInt(e) - MPFR_MAX_EMBEDDED_EXPONENT;
    if (exponent < DEFAULT_EMIN_MIN || exponent > DEFAULT_EMAX_MAX) {
      throw new MPFRError(
        'EDOMAIN',
        `fpif_read_exponent: embedded exponent ${exponent} out of range`,
      );
    }
    return { kind: 'normal', sign, exp: exponent, nextPos: cursor };
  }
  // The C source returns 1 (error) on the gap 122..127.
  throw new MPFRError(
    'EDOMAIN',
    `fpif_read_exponent: invalid exponent byte 0x${e.toString(16)}`,
  );
}

export function mpfr_fpif_read_exponent_from_file(
  bytes_value: bigint,
  byte_length: number,
  pos: number,
  prec: bigint,
): ReadExponentResult {
  if (typeof bytes_value !== 'bigint') {
    throw new MPFRError(
      'EDOMAIN',
      `mpfr_fpif_read_exponent_from_file: bytes_value must be bigint, got ${typeof bytes_value}`,
    );
  }
  if (typeof prec !== 'bigint') {
    throw new MPFRError(
      'EDOMAIN',
      `mpfr_fpif_read_exponent_from_file: prec must be bigint, got ${typeof prec}`,
    );
  }
  const bytes = bigIntToBytes(bytes_value, byte_length);
  return fpif_read_exponent(bytes, pos, prec);
}
