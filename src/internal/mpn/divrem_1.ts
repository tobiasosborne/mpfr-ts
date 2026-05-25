/**
 * mpn/divrem_1.ts -- pure-TS port of GMP's `mpn_divrem_1`.
 *
 * Substrate-class helper. Operates on raw `bigint[]` limb arrays, NOT on
 * the idiomatic-surface `MPFR` value type from `src/core.ts` -- hence no
 * core import here (CLAUDE.md Law 3: "faithful substrate, idiomatic
 * surface"). The substrate split is load-bearing: mpfr_set_z,
 * mpfr_get_str, and a handful of mpz-mediated conversion paths reach
 * mpn_divrem_1 to do single-limb-divisor multi-precision division, so
 * this routine must mirror the GMP / mini-gmp I/O contract byte-for-byte.
 *
 * C signature (GMP):
 *
 *   mp_limb_t mpn_divrem_1(mp_limb_t *qp,
 *                          mp_size_t  qxn,
 *                          mp_limb_t *np,
 *                          mp_size_t  nn,
 *                          mp_limb_t  d0);
 *
 *   - Forms an (nn + qxn)-limb dividend N by prepending qxn zero limbs
 *     to the LSB end of np[0 .. nn) (i.e. shifting np left by qxn full
 *     limbs).
 *   - Writes Q = floor(N / d0) into qp[0 .. nn + qxn) in little-endian
 *     limb order.
 *   - Returns the single-limb remainder R = N mod d0, in [0, d0).
 *
 * TS signature (this port):
 *
 *   mpn_divrem_1(qxn, np, nn, d0) -> { q, r }
 *
 * The C version mutates qp in place and returns the remainder as a
 * scalar; idiomatic TS returns a fresh `{q, r}` object with `q` a
 * readonly bigint array of length `nn + qxn` and `r` a scalar bigint
 * limb. The substrate is immutable at the function boundary -- callers
 * that need in-place behaviour wrap this and copy.
 *
 * Algorithm
 * ---------
 *
 * Mirrors the mini-gmp shim body (mpfr/src/mpfr-mini-gmp.c L177-L215)
 * which itself wraps `mpz_tdiv_qr` over an mpz built from the np limbs
 * and a shift:
 *
 *   1. Pack np[0 .. nn) into a single non-negative BigInt N (little-
 *      endian limb order: np[0] contributes the low 64 bits).
 *   2. Prepend qxn zero limbs by left-shifting N by qxn * 64 bits.
 *      (Equivalent to multiplying N by 2^(qxn*64); the C shim builds
 *      an mpz of length nn+qxn with zeros below np.)
 *   3. Q = N / d0 ; R = N % d0     (BigInt truncated division -- both
 *                                    N and d0 are non-negative, so the
 *                                    TC39 semantics for `/` and `%` on
 *                                    bigints coincide with MPFR's mpz
 *                                    truncated-toward-zero contract.)
 *   4. Unpack Q into exactly nn + qxn little-endian limbs (zero-padded
 *      on the MSB end if Q is shorter -- e.g. nn=1 with d0 > np[0]
 *      yields Q=0 and a single zero quotient limb).
 *
 * Why Q always fits in nn + qxn limbs
 * -----------------------------------
 *
 * N has at most nn + qxn limbs (the high limb is at most np[nn-1] <
 * 2^64). Dividing by d0 >= 1 cannot increase the limb count, so Q
 * occupies at most nn + qxn limbs. The bigIntToLimbs helper allocates
 * exactly nn + qxn slots; the highest shift would only drop bits if
 * Q exceeded that range, which the contract forbids.
 *
 * Limb endianness
 * ---------------
 *
 * GMP stores limbs in LITTLE-ENDIAN limb order: `limbs[0]` is the
 * least-significant 2^64 word, `limbs[n-1]` is the most-significant.
 * np follows this; q in the result follows this. (CLAUDE.md
 * "Hallucination-risk callouts: mpn limbs are LITTLE-ENDIAN by limb
 * index.")
 *
 * Refs
 * ----
 *
 *   - GMP manual section 8.3 "Low-level Functions": mpn_divrem_1
 *     single-limb-divisor variant; "The least significant limb is
 *     stored at the lowest address (i.e. limbs[0])."
 *   - mpfr/src/mpfr-mini-gmp.c L177-L215 -- the SHIM body this port
 *     mirrors. Packs np into an mpz, shifts left by qxn limbs, calls
 *     mpz_tdiv_qr against a d0 mpz, copies the quotient back to qp,
 *     returns the low limb of the (single-limb) remainder.
 *   - eval/functions/mpn_divrem_1/spec.json -- full contract.
 *   - eval/reference_ports/correct/mpn_divrem_1.ts -- mutation-prove
 *     calibration baseline this production port shadows.
 *
 * Invariants
 * ----------
 *
 *   Precondition:  qxn >= 0 (integer)
 *                  AND nn >= 0 (integer)
 *                  AND np.length >= nn
 *                  AND d0 > 0n (bigint)
 *                  AND every np limb is a non-negative bigint < 2^64.
 *
 *   Postcondition: returned `q.length === nn + qxn`
 *                  AND `r` is a bigint in [0, d0)
 *                  AND every output limb is in [0, 2^64)
 *                  AND combine(q) * d0 + r == N   exactly
 *                  (where N is np << (qxn * 64)).
 *
 * Performance note
 * ----------------
 *
 * Pack / divide / unpack via native BigInt. The pack and unpack loops
 * are O(nn + qxn) BigInt shifts; the divide itself is whatever V8's
 * BigInt implementation chooses (sub-quadratic for large operands).
 * For the golden's limb counts (<= ~8) this is overwhelmingly fast.
 * The Optimize phase may revisit if a hot caller demonstrates an
 * allocation hotspot -- but per `memory/project_future_bigint_refactor`
 * a post-port collapse of the limb-array substrate to native BigInt
 * is the likely path, which would eliminate the pack/unpack overhead
 * entirely.
 */

const LIMB_BITS = 64n;
const LIMB_MASK = (1n << LIMB_BITS) - 1n;

export interface MpnDivrem1Result {
  readonly q: readonly bigint[];
  readonly r: bigint;
}

/**
 * Divide an (nn + qxn)-limb non-negative integer (little-endian limb
 * order, formed by prepending qxn zero limbs to the LSB end of np) by
 * a single nonzero limb d0, returning the (nn + qxn)-limb quotient and
 * the single-limb remainder.
 *
 * @param qxn  Number of zero limbs prepended to np to form the
 *             dividend. Must be a non-negative integer. The common
 *             case is qxn == 0 (no fractional extension); qxn > 0
 *             corresponds to GMP's "fractional quotient" mode used
 *             when extracting low bits beyond the input precision.
 * @param np   Dividend limb array in little-endian limb order; must
 *             have `length >= nn`. Each limb is in [0, 2^64).
 * @param nn   Number of np limbs to consume. Must be a non-negative
 *             integer. nn == 0 with qxn == 0 is degenerate (empty
 *             dividend) and returns `{q: [], r: 0n}`.
 * @param d0   Single-limb divisor in (0, 2^64). Must be a positive
 *             bigint.
 * @returns    `{q, r}` where:
 *               - `q.length === nn + qxn` (zero-padded on the MSB end
 *                 if the true quotient is shorter);
 *               - `r` is a bigint in [0, d0).
 *
 * @throws `Error` on a domain violation: qxn or nn not a non-negative
 *         integer, np too short, d0 not a positive bigint. (Substrate
 *         is no-core-import; we use plain `Error`, not `MPFRError`.
 *         The grader records the message verbatim.)
 */
export function mpn_divrem_1(
  qxn: number,
  np: readonly bigint[],
  nn: number,
  d0: bigint,
): MpnDivrem1Result {
  // Validation: fail fast and loud on invariant breaches (CLAUDE.md Rule 1).
  if (!Number.isInteger(qxn) || qxn < 0) {
    throw new Error(
      `mpn_divrem_1: qxn must be a non-negative integer, got ${qxn}`,
    );
  }
  if (!Number.isInteger(nn) || nn < 0) {
    throw new Error(
      `mpn_divrem_1: nn must be a non-negative integer, got ${nn}`,
    );
  }
  if (np.length < nn) {
    throw new Error(
      `mpn_divrem_1: np too short: need ${nn} limbs, got ${np.length}`,
    );
  }
  if (typeof d0 !== 'bigint') {
    throw new Error(
      `mpn_divrem_1: d0 must be bigint, got ${typeof d0}`,
    );
  }
  if (d0 <= 0n) {
    throw new Error(`mpn_divrem_1: d0 must be > 0, got ${d0}`);
  }

  // Step 1: pack np[0 .. nn) into a single non-negative BigInt. Walk
  // MSB-first so each shift-left brings the next limb into the low
  // 64 bits. np is little-endian -- np[nn-1] is the MSB-end limb.
  let dividend = 0n;
  for (let i = nn - 1; i >= 0; i--) {
    // noUncheckedIndexedAccess: `np[i]` is `bigint | undefined`. We
    // length-checked np.length >= nn above, so this is structurally
    // impossible -- but the compiler can't see that, and we prefer an
    // explicit narrow over `!` so future refactors that loosen the
    // precondition surface here.
    const limb = np[i];
    if (limb === undefined) {
      throw new Error(
        `mpn_divrem_1: internal invariant violated -- undefined limb at index ${i}`,
      );
    }
    dividend = (dividend << LIMB_BITS) | limb;
  }

  // Step 2: prepend qxn zero limbs at the LSB end by left-shifting
  // the packed dividend by (qxn * 64) bits. Equivalent to building an
  // (nn + qxn)-limb mpz with zeros below np in the C shim.
  dividend <<= BigInt(qxn) * LIMB_BITS;

  // Step 3: BigInt truncated division. Both operands are non-negative,
  // so `/` and `%` on bigints match mpz_tdiv_qr semantics exactly.
  const qBig = dividend / d0;
  const r = dividend % d0;

  // Step 4: decompose the quotient into exactly (nn + qxn) little-endian
  // limbs (zero-padded on the MSB end when the true quotient is shorter,
  // e.g. nn=1 with d0 > np[0] yields qBig=0 and a single zero limb).
  const qLen = nn + qxn;
  const q: bigint[] = new Array<bigint>(qLen);
  let remaining = qBig;
  for (let i = 0; i < qLen; i++) {
    q[i] = remaining & LIMB_MASK;
    remaining >>= LIMB_BITS;
  }
  // If `remaining` is nonzero here the BigInt quotient exceeded
  // (nn + qxn) limbs -- but the C contract guarantees this cannot
  // happen for valid inputs (Q = floor(N / d0) with d0 >= 1 has at
  // most as many limbs as N, which has at most nn + qxn). No check;
  // the loop above already truncated, which would be the bug surface
  // if the contract were violated.

  return { q: q as readonly bigint[], r };
}
