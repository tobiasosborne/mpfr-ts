/**
 * reference_ports/correct/mpfr_fpif_store_exponent.ts -- mutation-prove reference.
 *
 * Algorithm (mpfr/src/fpif.c L317-L387): encode the sign+exponent (or
 * kind sentinel) byte field of an MPFR value into the fpif binary
 * format. Reproduced verbatim using native BigInt and Uint8Array, with
 * the host-endian C abstraction collapsed to a single little-endian path
 * (ADR 0004 Invariant 3).
 *
 * Format (fpif.c L44-L57):
 *   Byte A = [seeeeeee] where s = sign bit (0x80), E = [eeeeeee] (7 bits).
 *   * For PURE_FP (kind='normal'):
 *       - If -47 <= e <= 47: E = e + 47, exponent_size = 0 (single byte).
 *       - Else: E = MPFR_EXTERNAL_EXPONENT + exponent_size,
 *               then exponent_size bytes hold (|e| - 47) shifted by 1
 *               with the top bit set if e < 0.
 *   * Singular values:
 *       - kind='zero': E = MPFR_KIND_ZERO (119).
 *       - kind='inf':  E = MPFR_KIND_INF  (120).
 *       - kind='nan':  E = MPFR_KIND_NAN  (121).
 *   * Sign bit OR'd into byte[0] high bit for all kinds (including NaN).
 *
 * Wire-form harness signature (matches eval/functions/mpfr_fpif_store_exponent/spec.json):
 *   mpfr_fpif_store_exponent(x: MPFR) -> { bytes: bigint, byte_length: number }
 */

import type { MPFR } from '../../../src/core.ts';
import { MPFRError } from '../../../src/core.ts';

const MPFR_KIND_ZERO = 119;
const MPFR_KIND_INF = 120;
const MPFR_KIND_NAN = 121;
const MPFR_MAX_EMBEDDED_EXPONENT = 47n;
const MPFR_EXTERNAL_EXPONENT = 94;

export interface StoreExponentResult {
  readonly bytes: bigint;
  readonly byte_length: number;
}

function bytesToBigInt(bytes: Uint8Array): bigint {
  let v = 0n;
  for (let i = bytes.length - 1; i >= 0; i--) {
    const b = bytes[i];
    if (b === undefined) {
      throw new Error('mpfr_fpif_store_exponent: undefined byte');
    }
    v = (v << 8n) | BigInt(b);
  }
  return v;
}

/**
 * Substrate signature (ADR 0004 "Worked example" section):
 *   fpif_store_exponent(x: MPFR): Uint8Array
 */
function fpif_store_exponent(x: MPFR): Uint8Array {
  let header: number;
  let payload: number[] = [];

  if (x.kind === 'normal') {
    const e = x.exp;
    if (
      e > MPFR_MAX_EMBEDDED_EXPONENT ||
      e < -MPFR_MAX_EMBEDDED_EXPONENT
    ) {
      // Extended path.
      const absE = e < 0n ? -e : e;
      let uexp = absE - MPFR_MAX_EMBEDDED_EXPONENT;
      const copy_exponent = uexp << 1n;
      // C MPFR_ASSERTD (copy_exponent > uexp): the shift must not overflow.
      // For bigint there is no fixed-width overflow but we assert sanity.
      if (copy_exponent <= uexp) {
        throw new MPFRError(
          'EDOMAIN',
          `fpif_store_exponent: exponent ${e} causes shift overflow`,
        );
      }
      // COUNT_NB_BYTE: shift copy_exponent right 8 until zero, counting.
      let exponent_size = 0;
      let tmp = copy_exponent;
      do {
        tmp >>= 8n;
        exponent_size++;
      } while (tmp !== 0n);
      if (exponent_size > 16) {
        throw new MPFRError(
          'EDOMAIN',
          `fpif_store_exponent: exponent ${e} exceeds 16-byte encoding`,
        );
      }
      const exp_sign_bit = 1n << BigInt(8 * exponent_size - 1);
      // C MPFR_ASSERTD(uexp < exp_sign_bit).
      if (uexp >= exp_sign_bit) {
        throw new MPFRError(
          'EDOMAIN',
          `fpif_store_exponent: exponent ${e} fills the sign-bit slot`,
        );
      }
      if (e < 0n) uexp |= exp_sign_bit;
      header = MPFR_EXTERNAL_EXPONENT + exponent_size;
      let rem = uexp;
      for (let i = 0; i < exponent_size; i++) {
        payload.push(Number(rem & 0xffn));
        rem >>= 8n;
      }
    } else {
      // Embedded: uexp = e + 47 fits in 0..94 (single byte).
      header = Number(e + MPFR_MAX_EMBEDDED_EXPONENT);
    }
  } else if (x.kind === 'zero') {
    header = MPFR_KIND_ZERO;
  } else if (x.kind === 'inf') {
    header = MPFR_KIND_INF;
  } else if (x.kind === 'nan') {
    header = MPFR_KIND_NAN;
  } else {
    throw new MPFRError(
      'EDOMAIN',
      `fpif_store_exponent: unknown kind ${String((x as { kind?: string }).kind)}`,
    );
  }

  // OR the sign bit into byte[0]'s high bit. NaN sign is preserved
  // (C does this; src/core.ts canonicalises NaN to sign=+1, so this
  // is a no-op for NaN inputs from the locked schema).
  if (x.sign < 0) header |= 0x80;

  const out = new Uint8Array(1 + payload.length);
  out[0] = header;
  for (let i = 0; i < payload.length; i++) {
    const b = payload[i];
    if (b === undefined) {
      throw new Error('mpfr_fpif_store_exponent: undefined payload byte');
    }
    out[1 + i] = b;
  }
  return out;
}

export function mpfr_fpif_store_exponent(x: MPFR): StoreExponentResult {
  if (x === null || typeof x !== 'object') {
    throw new MPFRError(
      'EDOMAIN',
      `mpfr_fpif_store_exponent: x must be an MPFR object`,
    );
  }
  const buf = fpif_store_exponent(x);
  return { bytes: bytesToBigInt(buf), byte_length: buf.length };
}
