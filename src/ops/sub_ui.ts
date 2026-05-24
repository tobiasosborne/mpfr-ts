/**
 * port.ts — eval port of MPFR's `mpfr_sub_ui` for grading.
 *
 * Subtract a non-negative integer `c` from an {@link MPFR} value `b`.
 * Structural mirror of {@link mpfr_add_ui} — same dispatch, same
 * cMPFR construction, just `mpfr_sub` instead of `mpfr_add` at the tail.
 *
 * C signature
 * -----------
 *
 *   int mpfr_sub_ui(mpfr_ptr y, mpfr_srcptr x, unsigned long u,
 *                   mpfr_rnd_t rnd_mode);
 *
 *   Body (mpfr/src/sub_ui.c L26-L94). Structure:
 *
 *     - u == 0: return mpfr_set(y, x, rnd_mode).
 *     - x is NaN: return NaN.
 *     - x is ±Inf: return ±Inf (sign preserved), ternary 0.
 *     - x is ±0 (with u != 0): fall through to general path (the C
 *       comment notes "we can't use mpfr_set_si as the opposite of u
 *       does not necessarily fit in a long"). The result is -c.
 *     - else (normal): build temp uu via mpfr_set_ui, then mpfr_sub.
 *
 *   Ref: mpfr/src/sub_ui.c L26-L94.
 *
 * TS divergences
 * --------------
 *
 *   1. check_range tail omitted (schema has no exp range).
 *   2. Internal mpfr_t at GMP_NUMB_BITS replaced by mpfr_set_ui at
 *      bitLength(c) — exact conversion.
 *
 * Refs
 * ----
 *
 *   - mpfr/src/sub_ui.c L26-L94 — C reference.
 *   - /home/tobias/Projects/mpfr-ts/src/ops/sub.ts — load-bearing delegate.
 *   - /home/tobias/Projects/mpfr-ts/src/ops/set_ui.ts — integer-to-MPFR exact conversion.
 *   - /home/tobias/Projects/mpfr-ts/src/ops/set.ts — c == 0 fast path.
 *   - /home/tobias/Projects/mpfr-ts/src/ops/add_ui.ts — structural mirror.
 *   - /home/tobias/Projects/mpfr-ts/src/core.ts — locked schema.
 */

import type { MPFR, Result, RoundingMode } from "../core.ts";
import {
  MPFRError,
  NAN_VALUE,
  PREC_MAX,
  PREC_MIN,
  negInf,
  posInf,
} from "../core.ts";
import { mpfr_set } from "./set.ts";
import { mpfr_set_ui } from "./set_ui.ts";
import { mpfr_sub } from "./sub.ts";

/** Largest unsigned 64-bit integer. */
const ULONG_MAX_VAL: bigint = (1n << 64n) - 1n;

/**
 * Bit length of a non-negative bigint (position of topmost set bit,
 * 1-indexed). Returns 0n for 0n.
 */
function bitLength(n: bigint): bigint {
  if (n === 0n) return 0n;
  let bits = 0n;
  let probe = n;
  while (probe > 0n) {
    bits++;
    probe >>= 1n;
  }
  return bits;
}

function validateArgs(
  c: bigint,
  prec: bigint,
  rnd: RoundingMode,
): void {
  if (typeof c !== 'bigint') {
    throw new MPFRError('EPREC', `c must be bigint, got ${typeof c}`);
  }
  if (c < 0n || c > ULONG_MAX_VAL) {
    throw new MPFRError(
      'EPREC',
      `c out of uint64 range [0, ${ULONG_MAX_VAL}], got ${c}`,
    );
  }
  if (typeof prec !== 'bigint') {
    throw new MPFRError('EPREC', `prec must be bigint, got ${typeof prec}`);
  }
  if (prec < PREC_MIN) {
    throw new MPFRError('EPREC', `prec must be >= ${PREC_MIN}, got ${prec}`);
  }
  if (prec > PREC_MAX) {
    throw new MPFRError('EPREC', `prec must be <= ${PREC_MAX}, got ${prec}`);
  }
  if (
    rnd !== 'RNDN' &&
    rnd !== 'RNDZ' &&
    rnd !== 'RNDU' &&
    rnd !== 'RNDD' &&
    rnd !== 'RNDA'
  ) {
    throw new MPFRError('EROUND', `unknown rounding mode: ${String(rnd)}`);
  }
}

/**
 * Compute `b - c` at precision `prec` per `rnd`.
 *
 * @mpfrName mpfr_sub_ui
 *
 * @param b     the MPFR minuend. Any kind ('normal', 'zero', 'inf', 'nan').
 * @param c     the integer subtrahend, in `[0, 2^64 - 1]`.
 * @param prec  output precision in bits.
 * @param rnd   one of the five rounding modes.
 *
 * @returns     `{value, ternary}`.
 *
 * @throws {MPFRError} `EPREC` on bad `c` / `prec`; `EROUND` on bad rnd.
 */
export function mpfr_sub_ui(
  b: MPFR,
  c: bigint,
  prec: bigint,
  rnd: RoundingMode,
): Result {
  validateArgs(c, prec, rnd);

  // Ref: mpfr/src/sub_ui.c L35-L36 — u == 0 fast path: mpfr_set(y, x, rnd).
  if (c === 0n) {
    return mpfr_set(b, prec, rnd);
  }

  // Ref: mpfr/src/sub_ui.c L39-L54 — singular b dispatch.
  // NOTE: the C side does NOT short-circuit b == ±0 here (because the
  // result would be -u which may exceed LONG_MAX; mpfr_set_si is unsafe).
  // It falls through to the general path. We mirror that.
  switch (b.kind) {
    case 'nan':
      // Ref: mpfr/src/sub_ui.c L41-L44 — NaN x → NaN.
      return { value: NAN_VALUE, ternary: 0 };
    case 'inf':
      // Ref: mpfr/src/sub_ui.c L46-L50 — ±Inf preserves sign, ternary 0.
      // ±Inf - finite == ±Inf (same sign as the infinity).
      return {
        value: b.sign === 1 ? posInf(prec) : negInf(prec),
        ternary: 0,
      };
    case 'zero':
    case 'normal':
      break;
  }

  // --- General path: build cMPFR exactly, delegate to mpfr_sub -----------
  // Ref: mpfr/src/sub_ui.c L65-L88 — internal mpfr_t populated from u,
  // then mpfr_sub(y, x, uu, rnd_mode). We use mpfr_set_ui at bitLength(c)
  // precision for an exact conversion (ternary 0 guaranteed when prec of
  // the temp equals bitLength(c) which is exactly the value's bit width).
  // The ±0 case falls through here and produces b - c = 0 - c = -c.
  const cPrec = bitLength(c);  // >= 1 since c > 0n here
  const cMPFR = mpfr_set_ui(c, cPrec, 'RNDN').value;
  return mpfr_sub(b, cMPFR, prec, rnd);
}
