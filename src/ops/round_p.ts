/**
 * ops/round_p.ts — pure-TS port of MPFR's `mpfr_round_p`.
 *
 * Substrate-class helper. Tests whether a raw limb-array approximation
 * `bp` with `err0` known bits of accuracy can be rounded toward zero
 * unambiguously at precision `prec`. Used in transcendental Ziv-loop
 * tails to decide whether the current working precision is high enough.
 *
 * C signature
 * -----------
 *
 *   int mpfr_round_p(mp_limb_t *bp, mp_size_t bn, mpfr_exp_t err0,
 *                    mpfr_prec_t prec);
 *
 *   - `bp`: limb array, **little-endian** (bp[0] is least significant).
 *     The C source uses index arithmetic that walks from bp+bn-1
 *     (most significant limb) downward. Precondition:
 *     `bp[bn-1] & MPFR_LIMB_HIGHBIT` (MSB of the top limb is set).
 *   - `bn`: number of limbs.
 *   - `err0`: error bound — the value is known to be within
 *     `2^(EXP(b) - err0)` of the represented number, so the top
 *     `err0` bits of `bp` are trustworthy.
 *   - `prec`: target precision in bits.
 *
 *   Returns non-zero iff rounding-toward-zero at precision `prec` is
 *   unambiguous given the error budget. Returns 0 in the degenerate
 *   cases (err0 <= 0, err0 <= prec, prec >= bn*GMP_NUMB_BITS) where
 *   trivially insufficient information is available.
 *
 *   Body: mpfr/src/round_p.c L67-L135. Annotated walkthrough below.
 *
 * Algorithm
 * ---------
 *
 *   Let total = bn * GMP_NUMB_BITS (total bit width of the array).
 *
 *   1. Degenerate guards: if err0 <= 0 OR err0 <= prec OR prec >= total,
 *      return false. The first two mean we don't have enough error
 *      margin to round; the third means we don't have enough mantissa
 *      bits to round to.
 *
 *   2. err = min(total, err0). Now err is the actual error frontier.
 *
 *   3. Decompose the bit-frame relative to the limb array:
 *
 *        k = prec / GMP_NUMB_BITS       // limbs (from MSB) covered fully by prec
 *        s = GMP_NUMB_BITS - prec % GMP_NUMB_BITS
 *                                       // bits below the prec boundary in the
 *                                       // k-th limb-from-top (so the low s bits
 *                                       // are "below prec")
 *        n = err / GMP_NUMB_BITS - k    // number of additional limbs the error
 *                                       // band spans past the prec-containing limb
 *
 *   4. Read the "first limb past prec" (i.e. the limb at index bn-1-k
 *      counting from bp[0]). Apply a mask exposing only the s low bits:
 *      `mask = LIMB_MASK(s)` if s != GMP_NUMB_BITS (which happens when
 *      prec % GMP_NUMB_BITS == 0; in that case mask = MPFR_LIMB_MAX).
 *
 *   5. Four sub-cases depending on (n, tmp = bp[bn-1-k] & mask):
 *
 *      a. n == 0 (prec and err are in the same limb): pull the low
 *         (GMP_NUMB_BITS - err % GMP_NUMB_BITS) bits via further
 *         shifting; return true iff the resulting window is neither
 *         all-zero nor all-mask.
 *
 *      b. n > 0 AND tmp == 0: every "above the error frontier" bit is
 *         zero. Walk down the array; if any inter-limb is non-zero,
 *         return true. Otherwise check the low partial limb at the
 *         error frontier; return true iff its high bits are non-zero.
 *
 *      c. n > 0 AND tmp == mask: every "above the error frontier" bit
 *         is 1 (so we're in the all-ones-band region). Mirror image of
 *         (b): walk down, return true if any limb is not MPFR_LIMB_MAX;
 *         else check the low partial limb is not all-ones at its high.
 *
 *      d. Otherwise (n > 0, tmp is mixed): return true unconditionally.
 *         The error band already has both a 0 and a 1 in the top limb,
 *         so the rounded value can't tip either way given the budget.
 *
 * Refs
 * ----
 *
 *   - mpfr/src/round_p.c L67-L135 — the C reference.
 *   - mpfr/src/round_p.c L62-L66 — function contract comment.
 *   - mpfr/src/mpfr-impl.h §MPFR_LIMB_HIGHBIT, MPFR_LIMB_MAX,
 *     MPFR_LIMB_MASK — the limb-level masks.
 *   - GMP manual §8.3 — little-endian limb order.
 *   - src/internal/mpfr/round_raw.ts — sibling substrate.
 *   - src/core.ts §MPFRError — the input-error type.
 *   - CLAUDE.md "Hallucination-risk callouts: GMP mpn limbs are
 *     LITTLE-ENDIAN by limb index."
 */

import { MPFRError } from '../core.ts';

const LIMB_BITS: bigint = 64n;
const LIMB_MAX: bigint = (1n << LIMB_BITS) - 1n;
const HIGHBIT: bigint = 1n << (LIMB_BITS - 1n);

/**
 * Mask of the low `s` bits set. Equivalent to MPFR_LIMB_MASK(s).
 * For s == LIMB_BITS, returns LIMB_MAX (all bits).
 */
function limbMask(s: bigint): bigint {
  if (s >= LIMB_BITS) return LIMB_MAX;
  return (1n << s) - 1n;
}

/**
 * Test whether `bp` can be rounded toward zero at precision `prec`
 * given an err0-bit error bound.
 *
 * @mpfrName mpfr_round_p
 *
 * @param bp    Limb array, little-endian (bp[0] LSB). bp[bp.length - 1]
 *              MUST have its MSB (bit 63) set.
 * @param err0  Error bound exponent (signed).
 * @param prec  Target precision in bits (positive).
 *
 * @returns     `true` if rounding is unambiguous, `false` otherwise.
 *
 * @throws {MPFRError} `EPREC` if bp is empty, prec is non-positive,
 *                    or the MSB invariant is violated.
 *
 * @example
 *   const bp = [0x4242n | (1n << 63n)];  // single limb, MSB set
 *   mpfr_round_p(bp, 60n, 30n);  // true (plenty of margin)
 *   mpfr_round_p(bp, 0n, 30n);   // false (err0 <= 0)
 *   mpfr_round_p(bp, 30n, 30n);  // false (err0 == prec)
 */
export function mpfr_round_p(
  bp: readonly bigint[],
  err0: bigint,
  prec: bigint,
): boolean {
  // --- Input validation --------------------------------------------------
  const bn = BigInt(bp.length);
  if (bn < 1n) {
    throw new MPFRError('EPREC', 'mpfr_round_p: bp must be non-empty');
  }
  if (typeof err0 !== 'bigint') {
    throw new MPFRError('EPREC', `err0 must be bigint, got ${typeof err0}`);
  }
  if (typeof prec !== 'bigint') {
    throw new MPFRError('EPREC', `prec must be bigint, got ${typeof prec}`);
  }
  if (prec < 1n) {
    throw new MPFRError('EPREC', `prec must be >= 1, got ${prec}`);
  }

  const topLimb = bp[bp.length - 1];
  if (topLimb === undefined) {
    throw new MPFRError('EPREC', 'mpfr_round_p: bp[bn-1] undefined');
  }
  // Ref: mpfr/src/round_p.c L75 — MPFR_ASSERTD(bp[bn-1] & MPFR_LIMB_HIGHBIT).
  if ((topLimb & HIGHBIT) === 0n) {
    throw new MPFRError(
      'EPREC',
      `mpfr_round_p: bp[bn-1] must have MSB set, got ${topLimb}`,
    );
  }

  // --- Total error bound -------------------------------------------------
  // Ref: mpfr/src/round_p.c L77 — err = (mpfr_prec_t) bn * GMP_NUMB_BITS;
  let err: bigint = bn * LIMB_BITS;
  // Ref: mpfr/src/round_p.c L78-L79 — degenerate cases.
  if (err0 <= 0n || err0 <= prec || prec >= err) {
    return false;
  }
  // Ref: mpfr/src/round_p.c L80 — err = MIN(err, err0).
  // Cast err0 to uexp_t in C; we know err0 > 0 here so the bigint comparison
  // works directly.
  if (err0 < err) err = err0;

  // --- Decompose into limb / bit positions -------------------------------
  // Ref: mpfr/src/round_p.c L82-L84.
  const k = prec / LIMB_BITS;
  let s = LIMB_BITS - (prec % LIMB_BITS);
  let n = err / LIMB_BITS - k;

  // Helper: read the limb at index "bn-1-k" (k limbs down from top).
  // The C code uses pointer arithmetic `bp += bn - 1 - k; tmp = *bp--;`
  // — bp starts pointing to top-minus-k, reads it, then bp moves to the
  // limb BELOW (next iteration reads limb-1, etc.). We mirror via an
  // explicit index. Bounds-check defensively.
  const startIdx = bn - 1n - k;
  if (startIdx < 0n || startIdx >= bn) {
    // Should be unreachable: degenerate cases caught above ensure k < bn.
    throw new MPFRError(
      'EPREC',
      `mpfr_round_p: limb index ${startIdx} out of range (bn=${bn}, k=${k})`,
    );
  }
  let idx = Number(startIdx);
  const tmpLimb = bp[idx];
  if (tmpLimb === undefined) {
    throw new MPFRError('EPREC', `mpfr_round_p: bp[${idx}] undefined`);
  }
  idx--; // C: bp--

  // Ref: mpfr/src/round_p.c L92.
  // mask = s == GMP_NUMB_BITS ? MPFR_LIMB_MAX : MPFR_LIMB_MASK(s);
  let mask: bigint = s === LIMB_BITS ? LIMB_MAX : limbMask(s);
  let tmp: bigint = tmpLimb & mask;

  // --- Sub-case dispatch -------------------------------------------------
  // Ref: mpfr/src/round_p.c L95-L134.
  if (n === 0n) {
    // Ref: mpfr/src/round_p.c L95-L102. prec and err are in the same limb.
    s = LIMB_BITS - (err % LIMB_BITS);
    // C: MPFR_ASSERTD(s < GMP_NUMB_BITS). Safe because err > prec and they
    // share a limb, so err is at least one bit higher than the prec
    // boundary — s strictly less than LIMB_BITS.
    if (s >= LIMB_BITS) {
      // Defensive; should be unreachable given the degenerate-case
      // checks ensured err > prec.
      throw new MPFRError(
        'EPREC',
        `mpfr_round_p: invariant violated (s=${s} in n=0 branch)`,
      );
    }
    tmp >>= s;
    mask >>= s;
    return tmp !== 0n && tmp !== mask;
  } else if (tmp === 0n) {
    // Ref: mpfr/src/round_p.c L104-L116.
    // Check that all (n-1) intermediate limbs are 0.
    let nLocal = n;
    while ((nLocal -= 1n) > 0n) {
      if (idx < 0) {
        throw new MPFRError('EPREC', `mpfr_round_p: index out of range mid-walk`);
      }
      const limb = bp[idx];
      if (limb === undefined) {
        throw new MPFRError('EPREC', `mpfr_round_p: bp[${idx}] undefined mid-walk`);
      }
      idx--;
      if (limb !== 0n) return true;
    }
    // Check the final partial limb at the error boundary.
    s = LIMB_BITS - (err % LIMB_BITS);
    if (s === LIMB_BITS) return false;
    if (idx < 0) {
      throw new MPFRError('EPREC', `mpfr_round_p: index out of range at tail`);
    }
    const tailLimb = bp[idx];
    if (tailLimb === undefined) {
      throw new MPFRError('EPREC', `mpfr_round_p: bp[${idx}] undefined at tail`);
    }
    return (tailLimb >> s) !== 0n;
  } else if (tmp === mask) {
    // Ref: mpfr/src/round_p.c L117-L129. Mirror of the tmp==0 branch:
    // check all-ones instead of all-zeros.
    let nLocal = n;
    while ((nLocal -= 1n) > 0n) {
      if (idx < 0) {
        throw new MPFRError('EPREC', `mpfr_round_p: index out of range mid-walk (mask branch)`);
      }
      const limb = bp[idx];
      if (limb === undefined) {
        throw new MPFRError('EPREC', `mpfr_round_p: bp[${idx}] undefined mid-walk (mask)`);
      }
      idx--;
      if (limb !== LIMB_MAX) return true;
    }
    s = LIMB_BITS - (err % LIMB_BITS);
    if (s === LIMB_BITS) return false;
    if (idx < 0) {
      throw new MPFRError('EPREC', `mpfr_round_p: index out of range at tail (mask)`);
    }
    const tailLimb = bp[idx];
    if (tailLimb === undefined) {
      throw new MPFRError('EPREC', `mpfr_round_p: bp[${idx}] undefined at tail (mask)`);
    }
    return (tailLimb >> s) !== (LIMB_MAX >> s);
  } else {
    // Ref: mpfr/src/round_p.c L130-L134.
    // First limb is mixed → rounding is unambiguous.
    return true;
  }
}
