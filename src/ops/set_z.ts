/**
 * ops/set_z.ts — pure-TS port of MPFR's `mpfr_set_z`.
 *
 * Convert an arbitrary-precision integer (GMP `mpz_t` on the C side, a
 * JS `bigint` on this side) to an {@link MPFR} value at the caller-
 * supplied precision, rounded per `rnd`. Returns `{value, ternary}`.
 *
 * C signature
 * -----------
 *
 *   int mpfr_set_z(mpfr_t f, mpz_srcptr z, mpfr_rnd_t rnd);
 *
 *   The C body (mpfr/src/set_z.c L24–L29) is a one-liner delegate to
 *   mpfr_set_z_2exp(f, z, 0, rnd). The load-bearing implementation
 *   lives in mpfr/src/set_z_2exp.c L27–L198:
 *
 *     1. sign_z = mpz_sgn(z). If z == 0: store +0, return ternary 0.
 *     2. Otherwise: zn = limb size of |z|. count_leading_zeros on the
 *        top limb. Compute exp = zn * GMP_NUMB_BITS + e - k (where e=0
 *        for mpfr_set_z and k is the leading-zero count).
 *     3. If MPFR_PREC(f) >= bit-length-of-z: copy + lshift (exact, no
 *        rounding).
 *     4. Else: extract rounding-bit and sticky-bit from the dropped low
 *        bits, apply the rounding-mode decision, optionally addoneulp
 *        with potential carry-out bumping the exponent.
 *
 * TS divergence
 * -------------
 *
 * The TS port takes the integer as a plain JS `bigint` — that's the
 * natural representation, with no separate "limb array" type to
 * marshal. The body is structurally simpler than the C: bigint shifts
 * play the role of the limb-wise mpn_lshift + sticky-bit dance. The
 * `mpfr_set_z_2exp`'s e=0 specialisation reduces to "compute
 * srcExp = bitLength(|z|), then refit to prec".
 *
 * TS signature
 * ------------
 *
 *   mpfr_set_z(z: bigint, prec: bigint, rnd: RoundingMode): Result;
 *
 *   - `z` is `bigint` — no range restriction (unlike set_si which
 *     gates [LONG_MIN, LONG_MAX]).
 *   - `prec` is positional (no `rop` mutation).
 *   - returns {@link Result} per src/core.ts.
 *
 * Algorithm
 * ---------
 *
 *   1. Validate `prec`, `rnd`, `z`.
 *   2. If `z === 0n`: return `{value: posZero(prec), ternary: 0}` (the
 *      C reference forces sign +1 on zero — mpfr/src/set_z_2exp.c L39–L41).
 *   3. Otherwise:
 *      - `sign = z < 0n ? -1 : 1`
 *      - `absZ = z < 0n ? -z : z`
 *      - `srcPrec = bitLength(absZ)` — exact bit count of |z|
 *      - The value of z is `sign * absZ * 2^0`, so:
 *          mant = absZ (already MSB-aligned to srcPrec bits)
 *          srcExp = srcPrec (the schema's value formula is
 *                            sign * mant * 2^(exp - prec), so for an
 *                            integer the exponent in MPFR convention
 *                            equals the bit length)
 *      - If `prec >= srcPrec`: lossless. Pad with `prec - srcPrec`
 *        zeros on the right. Ternary 0.
 *      - Else: delegate to `roundMantissa(absZ, srcPrec, srcExp, prec,
 *        sign, rnd)`.
 *
 * Why `bitLength` is unbounded here
 * ---------------------------------
 *
 * Unlike `set_si` (where bitLength caps at 64), `set_z` accepts any
 * bigint — the input may have thousands of bits. The bigint shifts and
 * the naive bit-length loop both run in time proportional to the bit
 * length; for very large `z` this dominates wall time, which is why
 * `set_z` is classed `misc` (1s budget) rather than `arithmetic` (50ms).
 *
 * Bit-length implementation: linear loop is fine for the common case
 * (a few hundred bits) but degrades for thousands. We use a hybrid —
 * a chunked loop of 64 bits at a time, then refine with a 1-bit loop
 * for the residual. The bigint `>> 64n` shift is constant-time per
 * limb in V8/Bun, so the total is O(bits / 64) limb ops + ≤ 64
 * single-bit refinements. (`BigInt.prototype.toString(2).length` would
 * also work but allocates a decimal string of length proportional to
 * bits; the bit-shift loop is faster and allocation-free.)
 *
 * Refs
 * ----
 *
 *   - mpfr/src/set_z.c L24–L29 — C reference (trivial delegate).
 *   - mpfr/src/set_z_2exp.c L27–L198 — load-bearing body.
 *   - src/ops/set_si.ts — sibling for the int64-bounded case.
 *   - src/internal/mpfr/round_raw.ts — substrate.
 *   - src/core.ts — locked schema.
 *   - CLAUDE.md "Hallucination-risk callouts" — signed zero forced
 *     positive on z=0; ternary direction; rounding-mode count = FIVE.
 */

import type { MPFR, Result, RoundingMode, Sign } from '../core.ts';
import {
  MPFRError,
  PREC_MAX,
  PREC_MIN,
  posZero,
} from '../core.ts';
import { roundMantissa } from '../internal/mpfr/round_raw.ts';

/**
 * Compute the bit length of a non-negative bigint. Chunked: peel off
 * 64-bit limbs first, then refine within the top limb.
 *
 * Returns 0 for `0n`. For an n-bit value the loop runs in `ceil(n/64)`
 * limb shifts + at most 64 single-bit shifts — O(n) overall, which is
 * the best a bigint operation can do without a built-in bit-length.
 */
function bitLength(n: bigint): bigint {
  if (n === 0n) return 0n;
  // Peel 64-bit chunks. After this loop `probe` fits in a 64-bit
  // window and `bits` counts whole limbs consumed.
  let bits = 0n;
  let probe = n;
  while (probe >= 0x10000000000000000n /* 2^64 */) {
    bits += 64n;
    probe >>= 64n;
  }
  // Refine within the top limb (at most 64 iterations).
  while (probe > 0n) {
    bits++;
    probe >>= 1n;
  }
  return bits;
}

function validateArgs(z: bigint, prec: bigint, rnd: RoundingMode): void {
  if (typeof z !== 'bigint') {
    throw new MPFRError('EPREC', `z must be bigint, got ${typeof z}`);
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
 * Convert an arbitrary-precision integer to an MPFR at `prec` bits per
 * `rnd`.
 *
 * @mpfrName mpfr_set_z
 *
 * @param z     the integer, as a `bigint` of any magnitude.
 * @param prec  precision in bits, in `[PREC_MIN, PREC_MAX]`.
 * @param rnd   one of the five rounding modes.
 *
 * @returns     `{value, ternary}` per src/core.ts.
 *
 * @throws {MPFRError} `EPREC` on non-bigint `z` or bad precision;
 *                    `EROUND` on bad rounding mode.
 *
 * @example
 *   mpfr_set_z(0n, 53n, 'RNDN');           // posZero(53n), ternary 0 (sign forced +1)
 *   mpfr_set_z(1n, 53n, 'RNDN');           // +1.0 at prec 53
 *   mpfr_set_z(-1n, 53n, 'RNDN');          // -1.0 at prec 53
 *   mpfr_set_z(1n << 200n, 53n, 'RNDN');   // 2^200 at prec 53 (exact: only one set bit)
 *   mpfr_set_z((1n << 100n) + 1n, 53n, 'RNDN');  // rounded; ternary depends on rnd
 */
export function mpfr_set_z(
  z: bigint,
  prec: bigint,
  rnd: RoundingMode,
): Result {
  validateArgs(z, prec, rnd);

  // --- Zero shortcut --------------------------------------------------------
  // mpfr/src/set_z_2exp.c L39–L41: store +0, return ternary 0. Sign is
  // forced positive on zero (the C side calls MPFR_SET_POS); a bigint
  // input can't carry a sign for zero (no -0n in JS), so this matches
  // the C reference structurally.
  if (z === 0n) {
    return { value: posZero(prec), ternary: 0 };
  }

  // --- Sign / magnitude split ----------------------------------------------
  const sign: Sign = z < 0n ? -1 : 1;
  const absZ: bigint = z < 0n ? -z : z;

  // bitLength(absZ) >= 1 since we've eliminated z=0. The mantissa absZ
  // is already MSB-aligned to exactly bitLength bits — its top bit is
  // at position bitLength - 1.
  const srcPrec = bitLength(absZ);
  const srcMant = absZ;
  // For an integer the value is `sign * absZ * 2^0`. The schema's
  // formula is `sign * mant * 2^(exp - prec)`, so with mant = absZ and
  // prec = srcPrec we have exp = srcPrec. Same convention set_si uses.
  const srcExp = srcPrec;

  // --- Lossless padding when prec >= srcPrec --------------------------------
  if (prec >= srcPrec) {
    const padShift = prec - srcPrec;
    const value: MPFR = {
      kind: 'normal',
      sign,
      prec,
      exp: srcExp,
      mant: srcMant << padShift,
    };
    return { value, ternary: 0 };
  }

  // --- Lossy rounding when prec < srcPrec ----------------------------------
  // Delegate to the substrate. roundMantissa handles carry-out by
  // renormalising the mantissa to 2^(prec-1) and bumping the exponent
  // (e.g. (2^53 - 1) at prec 52, RNDA → 2^52, exp += 1).
  const { mant, exp, ternary } = roundMantissa(
    srcMant,
    srcPrec,
    srcExp,
    prec,
    sign,
    rnd,
  );
  const value: MPFR = {
    kind: 'normal',
    sign,
    prec,
    exp,
    mant,
  };
  return { value, ternary };
}
