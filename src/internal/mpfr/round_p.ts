/**
 * internal/mpfr/round_p.ts — pure-TS port of MPFR's `mpfr_round_p`.
 *
 * Substrate-class helper. Operates on raw `bigint[]` limb arrays (GMP
 * little-endian convention: limbs[0] is the least-significant 64-bit word).
 * No import from `src/core.ts` per CLAUDE.md Law 3 ("faithful substrate,
 * idiomatic surface").
 *
 * C signature (mpfr/src/round_p.c L67-L135):
 *
 *   int mpfr_round_p(mp_limb_t *bp, mp_size_t bn,
 *                    mpfr_exp_t err0, mpfr_prec_t prec);
 *
 * TS signature (this port):
 *
 *   mpfr_round_p(bp: readonly bigint[], err0: bigint, prec: bigint): boolean
 *
 * Contract
 * --------
 *
 * Given a raw limb-array approximation `bp` (little-endian; bp[bn-1] has
 * MSB set), the error bound exponent `err0` (meaning "err0 bits of bp are
 * known; the error is at most 2^(exp - err0)"), and the target precision
 * `prec`, returns true iff the approximation can be rounded toward zero at
 * precision `prec` without ambiguity.
 *
 * Equivalently: the bit region between position `prec` from the MSB and
 * position `err0` from the MSB contains neither all-zero nor all-one bits
 * (since either of those patterns would leave the rounding direction
 * indeterminate given the error bound).
 *
 * Algorithm (faithful port of C body, GMP_NUMB_BITS = 64)
 * -------------------------------------------------------
 *
 * 1. Compute totalBits = bn * 64. Return false immediately if:
 *    - err0 <= 0  (error bound is non-positive — no information)
 *    - err0 <= prec  (error straddles the rounding boundary)
 *    - prec >= totalBits  (target precision spans the whole mantissa)
 *
 *    Note: the C uses `(mpfr_uexp_t) err0 <= prec` for the middle test,
 *    which treats a negative err0 as a large unsigned value. Since we
 *    already guard `err0 <= 0` first, when we reach this check err0 > 0,
 *    so the signed and unsigned tests are equivalent.
 *
 * 2. err = min(totalBits, err0)
 *
 * 3. k = floor(prec / 64)  — index from the MSB limb for the rounding boundary
 *    s = 64 - (prec % 64)  — bits in the boundary limb that are BELOW the rounding position
 *    n = floor(err / 64) - k  — number of whole-limb spans between prec and err
 *
 * 4. Inspect the boundary limb: bp[bn - 1 - k].
 *    mask = (s == 64) ? LIMB_MAX : (1n << s) - 1n   (lower s bits)
 *    tmp  = bp[bn - 1 - k] & mask
 *
 * 5. Four sub-cases:
 *    a. n == 0: prec and err are in the same limb.
 *       s2 = 64 - (err % 64)   (asserted < 64 in C; enforced by n==0 && err > prec)
 *       Check tmp >> s2  is neither 0 nor (mask >> s2).
 *    b. n > 0, tmp == 0:
 *       Scan n-1 lower limbs; if any is non-zero → return true.
 *       Then check the final error limb's upper bits.
 *    c. n > 0, tmp == mask:
 *       Scan n-1 lower limbs; if any is not LIMB_MAX → return true.
 *       Then check the final error limb's upper bits ≠ (LIMB_MAX >> s2).
 *    d. n > 0, tmp neither 0 nor mask: return true immediately.
 *
 * Limb endianness
 * ---------------
 *
 * GMP stores limbs in LITTLE-ENDIAN limb order: bp[0] is the least-significant
 * 64-bit word, bp[bn-1] is the most-significant. The C code uses pointer
 * arithmetic starting from bp[bn-1-k] and decrementing (i.e. moving toward
 * lower indices). The TS port translates this to explicit index arithmetic.
 *
 * Refs
 * ----
 *
 *   - mpfr/src/round_p.c L67-L135 — the C reference body (production; no WANT_ASSERT).
 *   - mpfr/src/round_p.c L62-L66  — contract comment.
 *   - mpfr/src/mpfr-impl.h §MPFR_LIMB_HIGHBIT, MPFR_LIMB_MAX, MPFR_LIMB_MASK.
 *   - GMP manual §8.3 — little-endian limb convention.
 *   - src/internal/mpfr/round_raw.ts — sibling substrate with same limb idiom.
 *   - CLAUDE.md "Hallucination-risk callouts: GMP mpn limbs are LITTLE-ENDIAN by limb index".
 */

// Ref: mpfr/src/mpfr-impl.h §MPFR_LIMB_MAX — 0xFFFF...FFFF (64 bits set)
const LIMB_BITS = 64n;
const LIMB_MAX = (1n << LIMB_BITS) - 1n;  // 0xFFFFFFFFFFFFFFFFn

/**
 * Returns true iff the bn-limb approximation `bp` (little-endian, MSB of
 * top limb set) with error bound `err0` known bits can be rounded toward
 * zero at precision `prec` bits.
 *
 * Faithful port of `mpfr_round_p` from mpfr/src/round_p.c L67-L135.
 * Returns a TS boolean (not C int 0/non-zero) per the predicate convention.
 *
 * @param bp    Limb array, little-endian (bp[0] is least-significant).
 *              Must have MSB of bp[bp.length-1] set (i.e. bp[bn-1] & (1n<<63n) != 0n).
 * @param err0  Number of known significant bits from the MSB (signed; <= 0 → false).
 * @param prec  Target precision in bits (>= 1).
 * @returns     true iff the approximation can be unambiguously rounded.
 *
 * @mpfrName mpfr_round_p
 */
export function mpfr_round_p(
  bp: readonly bigint[],
  err0: bigint,
  prec: bigint,
): boolean {
  // Ref: mpfr/src/round_p.c L75 — MPFR_ASSERTD(bp[bn-1] & MPFR_LIMB_HIGHBIT)
  // We mirror the assert as a thrown error for malformed input, consistent
  // with the spec's divergence_from_c note.
  const bn = bp.length;
  if (bn === 0) {
    throw new Error('mpfr_round_p: bp must be non-empty');
  }
  const topLimb = bp[bn - 1];
  if (topLimb === undefined || (topLimb & (1n << 63n)) === 0n) {
    throw new Error('mpfr_round_p: MSB of top limb must be set');
  }

  // Ref: mpfr/src/round_p.c L77-L79 — early-exit checks.
  //
  // totalBits = bn * GMP_NUMB_BITS = bn * 64
  const totalBits = BigInt(bn) * LIMB_BITS;

  // Check 1: err0 <= 0 → can't round
  // Check 2: (mpfr_uexp_t) err0 <= prec → can't round
  //   In C this is an unsigned comparison, but since we've already guarded
  //   err0 <= 0, at this point err0 > 0, so signed == unsigned.
  // Check 3: prec >= totalBits → can't round
  if (err0 <= 0n || err0 <= prec || prec >= totalBits) {
    return false;
  }

  // Ref: mpfr/src/round_p.c L80 — err = MIN(err, err0)
  // err0 > 0 here, totalBits > 0, and we know err0 > prec >= 1.
  const err: bigint = err0 < totalBits ? err0 : totalBits;

  // Ref: mpfr/src/round_p.c L82-L84
  // k = prec / GMP_NUMB_BITS  (index from MSB limb containing rounding boundary)
  // s = GMP_NUMB_BITS - prec % GMP_NUMB_BITS
  // n = err / GMP_NUMB_BITS - k
  const k: bigint = prec / LIMB_BITS;         // floor division
  const s: bigint = LIMB_BITS - (prec % LIMB_BITS);  // bits below rounding boundary in that limb
  const n: bigint = (err / LIMB_BITS) - k;     // floor division

  // Ref: mpfr/src/round_p.c L86-L87 — MPFR_ASSERTD(n >= 0) and MPFR_ASSERTD(bn > k)
  // Both are guaranteed by the early-exit conditions above:
  //   n >= 0: err/64 >= prec/64 because err > prec (err >= err0 > prec ... wait)
  //   Actually err > prec because err0 > prec and err = min(totalBits, err0).
  //   So floor(err/64) >= floor(prec/64) = k? Not necessarily if err and prec
  //   share the same limb. But n = floor(err/64) - k.
  //   We need floor(err/64) >= k = floor(prec/64). This holds because err > prec,
  //   so floor(err/64) >= floor(prec/64) (floor is monotone). So n >= 0. ✓
  //   bn > k: k = floor(prec/64). Since prec < totalBits = bn*64, we have
  //   floor(prec/64) < bn, so k <= bn-1, so bn > k. ✓

  // Ref: mpfr/src/round_p.c L90 — bp += bn - 1 - k; tmp = *bp--;
  // In little-endian array terms:
  //   The limb at index (bn-1-k) is the limb containing the rounding boundary.
  //   C decrements the pointer after reading, so the next limb read is bp[bn-2-k].
  // We use an explicit index `idx` starting at bn-1-k and decrementing.
  const startIdx: number = Number(BigInt(bn) - 1n - k);

  // Ref: mpfr/src/round_p.c L92 — mask = s == GMP_NUMB_BITS ? MPFR_LIMB_MAX : MPFR_LIMB_MASK(s)
  // MPFR_LIMB_MASK(s) = (1 << s) - 1 for s < 64; for s == 64 use LIMB_MAX.
  const mask: bigint = s === LIMB_BITS ? LIMB_MAX : (1n << s) - 1n;

  // Ref: mpfr/src/round_p.c L93 — tmp &= mask
  const startLimb = bp[startIdx];
  if (startLimb === undefined) {
    throw new Error(`mpfr_round_p: internal error: startIdx ${startIdx} out of range`);
  }
  let tmp: bigint = startLimb & mask;

  // Ref: mpfr/src/round_p.c L95-L135 — four sub-cases.

  if (n === 0n) {
    // Ref: mpfr/src/round_p.c L95-L103 — n == 0: prec and err in the same limb.
    //
    // s2 = GMP_NUMB_BITS - err % GMP_NUMB_BITS
    // MPFR_ASSERTD(s2 < GMP_NUMB_BITS)  (since n==0 and err > prec > 0, err%64 > 0)
    // tmp >>= s2; mask >>= s2;
    // return tmp != 0 && tmp != mask;
    const errMod: bigint = err % LIMB_BITS;
    // n==0 and err > prec: if err%64 == 0, then floor(err/64) == err/64 and
    // k = floor(prec/64). n=0 means floor(err/64) == k = floor(prec/64).
    // If err%64 == 0, then err == k*64, so err == prec/64 * 64 <= prec < err,
    // contradiction. So errMod > 0, i.e. s2 < 64. ✓
    const s2: bigint = LIMB_BITS - errMod;
    tmp >>= s2;
    const shiftedMask: bigint = mask >> s2;
    return tmp !== 0n && tmp !== shiftedMask;
  } else if (tmp === 0n) {
    // Ref: mpfr/src/round_p.c L104-L115 — tmp == 0 case.
    //
    // Check if all (n-1) next limbs are 0.
    // C iterates: while (--n) if (*bp-- != 0) return 1;
    // In TS: limbs at indices (startIdx-1), (startIdx-2), ..., (startIdx-(n-1))
    // Then check the final error limb.
    let remaining = n;
    let idx = startIdx - 1;
    while (remaining > 1n) {
      const limb = bp[idx];
      if (limb === undefined || limb !== 0n) {
        return true;
      }
      idx--;
      remaining--;
    }
    // Ref: mpfr/src/round_p.c L111-L115 — check final error limb.
    // s = GMP_NUMB_BITS - err % GMP_NUMB_BITS
    // if (s == GMP_NUMB_BITS) return 0;
    // tmp = *bp >> s;
    // return tmp != 0;
    const errMod: bigint = err % LIMB_BITS;
    if (errMod === 0n) {
      // s2 == 64 means err is exactly limb-aligned; the error limb's bits
      // below the error boundary are in a separate (lower) limb — but those
      // bits are below err0, so they're unknowable. Return false.
      return false;
    }
    const s2: bigint = LIMB_BITS - errMod;
    const finalLimb = bp[idx];
    if (finalLimb === undefined) {
      return false;
    }
    const finalTmp: bigint = finalLimb >> s2;
    return finalTmp !== 0n;
  } else if (tmp === mask) {
    // Ref: mpfr/src/round_p.c L117-L128 — tmp == mask case (all ones).
    //
    // Check if all (n-1) next limbs are LIMB_MAX.
    // C iterates: while (--n) if (*bp-- != MPFR_LIMB_MAX) return 1;
    // Then check the final error limb's significant bits != (MPFR_LIMB_MAX >> s2).
    let remaining = n;
    let idx = startIdx - 1;
    while (remaining > 1n) {
      const limb = bp[idx];
      if (limb === undefined || limb !== LIMB_MAX) {
        return true;
      }
      idx--;
      remaining--;
    }
    // Ref: mpfr/src/round_p.c L124-L128 — check final error limb.
    // s = GMP_NUMB_BITS - err % GMP_NUMB_BITS
    // if (s == GMP_NUMB_BITS) return 0;
    // tmp = *bp >> s;
    // return tmp != (MPFR_LIMB_MAX >> s);
    const errMod: bigint = err % LIMB_BITS;
    if (errMod === 0n) {
      // All bits in the error band are 0xFFFF..., and err is limb-aligned:
      // the lower (unknowable) bits are separate. Return false.
      return false;
    }
    const s2: bigint = LIMB_BITS - errMod;
    const finalLimb = bp[idx];
    if (finalLimb === undefined) {
      return false;
    }
    const finalTmp: bigint = finalLimb >> s2;
    return finalTmp !== (LIMB_MAX >> s2);
  } else {
    // Ref: mpfr/src/round_p.c L130-L134 — first limb differs from all-0 or all-1.
    // Unambiguous rounding: return 1.
    return true;
  }
}
