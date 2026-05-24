/**
 * ops/cmp_ui.ts — pure-TS port of MPFR's `mpfr_cmp_ui`.
 *
 * Compare an {@link MPFR} value against an unsigned machine integer.
 * Returns `-1` if `x < n`, `0` if `x == n`, `+1` if `x > n`.
 *
 * C signature
 * -----------
 *
 *   int mpfr_cmp_ui(mpfr_srcptr b, unsigned long int i);
 *
 *   The C entry at mpfr/src/cmp_ui.c L115–L118 wraps
 *   `mpfr_cmp_ui_2exp(b, i, 0)`. The body (L33–L92) dispatches:
 *
 *     1. NaN b: set erange flag, return 0.
 *     2. ±Inf b: return MPFR_INT_SIGN(b).
 *     3. ±0 b: return `i != 0 ? -1 : 0` (i is unsigned, so always
 *        non-negative; a non-zero i compares strictly above ±0).
 *     4. b negative: return -1 (any negative MPFR < any non-negative ui).
 *     5. b positive, i == 0: return 1.
 *     6. Otherwise: exponent + limb compare on positive operands.
 *
 * TS signature
 * ------------
 *
 *   mpfr_cmp_ui(x: MPFR, n: bigint): number;
 *
 *   - `n` is a non-negative `bigint` in `[0, ULONG_MAX]` = `[0, 2^64 - 1]`.
 *     Matches `mpfr_set_ui`'s argument convention.
 *   - Returns a plain JS `number` in `{-1, 0, +1}`.
 *
 * Divergence from C → TS
 * ----------------------
 *
 * Same divergence as `mpfr_cmp` and `mpfr_cmp_si`: NaN `x` THROWS
 * `MPFRError('EDOMAIN', ...)` rather than silently returning 0.
 * Out-of-range `n` (negative or above ULONG_MAX) throws
 * `MPFRError('EPREC', ...)`.
 *
 * Algorithm
 * ---------
 *
 * Same strategy as `mpfr_cmp_si`: build a temp MPFR exact-representation
 * of `n` and delegate to {@link compareMPFR}. The only differences from
 * cmp_si:
 *
 *   - `n`'s sign is implicitly `+1` (it's unsigned).
 *   - The range is `[0, 2^64 - 1]` instead of `[-(2^63), 2^63 - 1]`.
 *
 * The temp MPFR has prec = bitLength(n) (up to 64 bits), exp = bitLength(n),
 * mant = n. For n === 0n, we build a +0 MPFR; compareMPFR's zero-vs-anything
 * dispatch matches the C reference's `b > 0 && i == 0 → 1` and
 * `b == 0 && i != 0 → -1` branches exactly.
 *
 * Refs
 * ----
 *
 *   - mpfr/src/cmp_ui.c L33–L92 — the C reference (mpfr_cmp_ui_2exp
 *     with f=0).
 *   - mpfr/src/cmp_ui.c L113–L118 — the public entry.
 *   - src/internal/mpfr/cmp_raw.ts — the shared comparison core.
 *   - src/ops/cmp_si.ts — signed sibling; this port is a direct
 *     specialisation for non-negative `n`.
 *   - src/ops/set_ui.ts — the n-to-MPFR exact conversion this port
 *     mirrors structurally.
 *   - CLAUDE.md "Hallucination-risk callouts": NaN ≠ NaN (throw).
 */

import type { MPFR } from '../core.ts';
import { MPFRError } from '../core.ts';
import { compareMPFR } from '../internal/mpfr/cmp_raw.ts';

/**
 * Largest unsigned 64-bit integer: `2^64 - 1`. Matches `ULONG_MAX`.
 */
const ULONG_MAX_VAL: bigint = (1n << 64n) - 1n;

/**
 * Bit length of a non-negative bigint (position of topmost set bit,
 * 1-indexed). Returns 0 for `0n`. Bounded by 64 iterations.
 */
function bitLength(n: bigint): bigint {
  let bits = 0n;
  let probe = n;
  while (probe > 0n) {
    bits++;
    probe >>= 1n;
  }
  return bits;
}

/**
 * Compare an {@link MPFR} value against an unsigned integer.
 *
 * @param x  MPFR value. Must pass {@link import('../core.ts').validate}.
 * @param n  non-negative bigint in `[0, ULONG_MAX]`.
 * @returns `-1` if `x < n`, `0` if `x == n`, `+1` if `x > n`.
 *
 * @throws {MPFRError} `EDOMAIN` if `x.kind === 'nan'`.
 * @throws {MPFRError} `EPREC` if `n` is not a bigint or lies outside
 *   `[0, ULONG_MAX]`.
 *
 * @mpfrName mpfr_cmp_ui
 */
export function mpfr_cmp_ui(x: MPFR, n: bigint): number {
  // --- Boundary validation ---------------------------------------------------
  if (typeof n !== 'bigint') {
    throw new MPFRError('EPREC', `n must be bigint, got ${typeof n}`);
  }
  if (n < 0n || n > ULONG_MAX_VAL) {
    throw new MPFRError(
      'EPREC',
      `n out of uint64 range [0, ${ULONG_MAX_VAL}], got ${n}`,
    );
  }

  // --- NaN x → throw ---------------------------------------------------------
  if (x.kind === 'nan') {
    throw new MPFRError(
      'EDOMAIN',
      `mpfr_cmp_ui: NaN x (cmp_ui requires a non-NaN MPFR operand)`,
    );
  }

  // --- Build a temp MPFR exact-representation of n ---------------------------
  // `n` is non-negative; sign is always +1. See cmp_si.ts for the
  // schema-formula derivation that justifies the (prec, exp, mant)
  // = (bitLength, bitLength, n) choice.
  let temp: MPFR;
  if (n === 0n) {
    temp = { kind: 'zero', sign: 1, prec: 1n, exp: 0n, mant: 0n };
  } else {
    const bits = bitLength(n);
    temp = {
      kind: 'normal',
      sign: 1,
      prec: bits,
      exp: bits,
      mant: n,
    };
  }

  // --- Delegate to compareMPFR -----------------------------------------------
  const r = compareMPFR(x, temp);
  if (r === null) {
    // Unreachable defensively (same rationale as cmp_si).
    throw new MPFRError(
      'EDOMAIN',
      `mpfr_cmp_ui: compareMPFR returned null unexpectedly (x.kind=${x.kind})`,
    );
  }
  return r;
}
