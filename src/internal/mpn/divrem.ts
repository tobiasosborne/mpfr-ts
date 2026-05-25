/**
 * mpn/divrem.ts -- pure-TS port of GMP's `mpn_divrem` (qn == 0 path).
 *
 * Substrate-class helper. Operates on raw `bigint[]` limb arrays, NOT on
 * the idiomatic-surface `MPFR` value type from `src/core.ts` -- hence no
 * core import here (CLAUDE.md Law 3: "faithful substrate, idiomatic
 * surface"). The substrate split is load-bearing: MPFR-level division
 * (`mpfr_div`) and many mpz-mediated paths reach mpn_divrem to do the
 * actual multi-limb school division, so this routine must mirror the
 * GMP / mini-gmp I/O contract byte-for-byte.
 *
 * C signature (GMP, qn == 0 specialisation matching the mini-gmp shim):
 *
 *   mp_limb_t mpn_divrem(mp_limb_t       *qp,
 *                        mp_size_t        qn,
 *                        mp_limb_t       *np,
 *                        mp_size_t        nn,
 *                        const mp_limb_t *dp,
 *                        mp_size_t        dn);
 *
 *   - qn must be 0 (the only path MPFR exercises; the mini-gmp shim
 *     literally `MPFR_ASSERTN (qn == 0)`).
 *   - On return: qp[0 .. nn-dn) holds the low quotient limbs, the
 *     scalar return value is the (nn-dn)+1-th high quotient limb
 *     (0 or 1 in the normalised-divisor regime), and np[0 .. dn) has
 *     been overwritten with the dn-limb remainder.
 *
 * TS signature (this port):
 *
 *   mpn_divrem(qn, np, nn, dp, dn) -> { q, qHigh, r }
 *
 * The C version mutates qp and np in place; idiomatic TS returns a
 * fresh `{q, qHigh, r}` object with `q` and `r` as readonly bigint
 * arrays of fixed lengths (nn-dn and dn) and `qHigh` as a scalar
 * bigint (cleaner than C's mixed-mode "write quotient to qp and
 * return the high limb"). The substrate is immutable at the function
 * boundary -- callers that need in-place behaviour wrap this and copy.
 *
 * Algorithm
 * ---------
 *
 * Mirrors the mini-gmp shim body (mpfr/src/mpfr-mini-gmp.c L216-L251)
 * which itself wraps `mpz_tdiv_qr`:
 *
 *   1. Validate qn == 0 (MPFR_ASSERTN(qn == 0) in the shim).
 *   2. Pack np[0 .. nn) and dp[0 .. dn) into BigInt N and D.
 *   3. Q = N / D ; R = N % D     (BigInt truncated division -- both
 *                                  N and D are non-negative, so the
 *                                  TC39 semantics for `/` and `%` on
 *                                  bigints coincide with MPFR's mpz
 *                                  truncated-toward-zero contract.)
 *   4. Unpack Q into nn-dn+1 little-endian limbs. The low nn-dn limbs
 *      go into `q[]`; the top slot is `qHigh`.
 *   5. Unpack R into exactly `dn` little-endian limbs (zero-padded if
 *      the true remainder is shorter than dn limbs).
 *
 * Why qHigh is 0 or 1 in the golden domain
 * ----------------------------------------
 *
 * The golden enforces divisor normalisation (dp[dn-1] >= 2^63), so
 * the numerator N satisfies N < 2 * D * 2^(64 * (nn - dn)). Therefore
 * Q = floor(N / D) < 2 * 2^(64 * (nn - dn)), which means Q occupies
 * at most nn - dn + 1 limbs with the top limb at most 1. For
 * NON-normalised divisors (not exercised here but allowed by the
 * BigInt arithmetic) qHigh could in principle exceed 1; we do not
 * artificially cap the value -- whatever the high limb of Q is, that's
 * what we return.
 *
 * Limb endianness
 * ---------------
 *
 * GMP stores limbs in LITTLE-ENDIAN limb order: `limbs[0]` is the
 * least-significant 2^64 word, `limbs[n-1]` is the most-significant.
 * Both np and dp follow this; both q and r in the result follow this.
 * (CLAUDE.md "Hallucination-risk callouts: mpn limbs are LITTLE-ENDIAN
 * by limb index.")
 *
 * Refs
 * ----
 *
 *   - GMP manual section 8.3 "Low-level Functions": mpn_divrem
 *     multi-limb divisor variant; "The least significant limb is
 *     stored at the lowest address (i.e. limbs[0])." Divisor must
 *     be normalised (high bit of dp[dn-1] set) for the real GMP
 *     implementation; the mini-gmp shim relaxes this via mpz_tdiv_qr.
 *   - mpfr/src/mpfr-mini-gmp.c L216-L251 -- the SHIM body this port
 *     mirrors. Asserts qn == 0, computes qn := nn - dn, calls
 *     mpz_tdiv_qr, copies qp / np back, returns the high quotient
 *     limb (or 0 if the quotient occupies exactly nn-dn limbs).
 *   - eval/functions/mpn_divrem/spec.json -- full contract.
 *   - eval/reference_ports/correct/mpn_divrem.ts -- mutation-prove
 *     calibration baseline this production port shadows.
 *
 * Invariants
 * ----------
 *
 *   Precondition:  qn == 0
 *                  AND nn >= dn >= 1
 *                  AND np.length >= nn
 *                  AND dp.length >= dn
 *                  AND D = limbsToBigInt(dp, dn) > 0
 *                  AND every limb is a non-negative bigint < 2^64.
 *
 *   Postcondition: returned `q.length === nn - dn`
 *                  AND returned `r.length === dn`
 *                  AND every output limb is in [0, 2^64)
 *                  AND combine(q, qHigh) * D + combine(r) == N exactly
 *                  AND combine(r) < D.
 *
 * Performance note
 * ----------------
 *
 * Pack / divide / unpack via native BigInt. The pack+unpack pairs are
 * O(nn + dn) BigInt shifts; the divide itself is whatever V8's BigInt
 * implementation chooses (sub-quadratic for large operands). For the
 * golden's limb counts (<= ~8 per side) this is overwhelmingly fast.
 * The Optimize phase may revisit if a hot caller demonstrates an
 * allocation hotspot -- but per `memory/project_future_bigint_refactor`
 * a post-port collapse of the limb-array substrate to native BigInt
 * is the likely path, which would eliminate the pack/unpack overhead
 * entirely.
 */

const LIMB_BITS = 64n;
const LIMB_MASK = (1n << LIMB_BITS) - 1n;

export interface MpnDivremResult {
  readonly q: readonly bigint[];
  readonly qHigh: bigint;
  readonly r: readonly bigint[];
}

/**
 * Pack the low `n` little-endian limbs of `limbs` into a single
 * non-negative BigInt. The MSB-end limb is `limbs[n-1]`.
 */
function limbsToBigInt(limbs: readonly bigint[], n: number): bigint {
  let v = 0n;
  for (let i = n - 1; i >= 0; i--) {
    // noUncheckedIndexedAccess: `limbs[i]` is `bigint | undefined`.
    // The caller has length-checked `limbs.length >= n`, so this is
    // structurally impossible -- but the compiler can't see that, and
    // we prefer an explicit narrow over `!` so future refactors that
    // loosen the precondition surface here.
    const limb = limbs[i];
    if (limb === undefined) {
      throw new Error(
        `mpn_divrem: internal invariant violated -- undefined limb at index ${i}`,
      );
    }
    v = (v << LIMB_BITS) | limb;
  }
  return v;
}

/**
 * Decompose a non-negative BigInt `v` into exactly `n` little-endian
 * 64-bit limbs (zero-padded if `v` is shorter than n limbs). If `v`
 * occupies more than `n` limbs the surplus high bits are silently
 * dropped -- callers must size `n` to cover the actual range.
 */
function bigIntToLimbs(v: bigint, n: number): bigint[] {
  const out: bigint[] = new Array<bigint>(n);
  let remaining = v;
  for (let i = 0; i < n; i++) {
    out[i] = remaining & LIMB_MASK;
    remaining >>= LIMB_BITS;
  }
  return out;
}

/**
 * Divide an `nn`-limb non-negative integer (little-endian limb order)
 * by a `dn`-limb non-negative integer (little-endian limb order),
 * returning the quotient (split as a low `nn-dn` limb array plus a
 * scalar `qHigh` high limb) and the `dn`-limb remainder.
 *
 * @param qn  Must be 0 (mirrors `MPFR_ASSERTN(qn == 0)` in the
 *            mini-gmp shim). The TS port throws if nonzero so callers
 *            that drift from the documented contract fail loudly
 *            rather than silently producing garbage.
 * @param np  Dividend limb array; must have `length >= nn`.
 * @param nn  Dividend limb count. Must satisfy `nn >= dn`.
 * @param dp  Divisor limb array; must have `length >= dn`.
 * @param dn  Divisor limb count. Must satisfy `dn >= 1`. The golden
 *            additionally enforces dp[dn-1] >= 2^63 (high-bit set),
 *            matching GMP's normalisation requirement; this port
 *            does not re-validate normalisation because the BigInt
 *            arithmetic produces the correct result regardless.
 * @returns   `{q, qHigh, r}` where:
 *              - `q.length === nn - dn` (the low quotient limbs);
 *              - `qHigh` is the (nn-dn)+1-th quotient limb -- in the
 *                normalised-divisor domain this is 0n or 1n;
 *              - `r.length === dn` (the dn-limb remainder, zero-padded
 *                on the MSB end if the true remainder is shorter).
 *
 * @throws `Error` on a domain violation: `qn != 0`, `nn` not a
 *         non-negative integer, `dn < 1`, `nn < dn`, either operand
 *         array too short, or a zero divisor. (Substrate is
 *         no-core-import; we use plain `Error`, not `MPFRError`. The
 *         grader records the message verbatim.)
 */
export function mpn_divrem(
  qn: number,
  np: readonly bigint[],
  nn: number,
  dp: readonly bigint[],
  dn: number,
): MpnDivremResult {
  // Step 1: validate qn == 0 (MPFR_ASSERTN(qn == 0) per the mini-gmp shim).
  if (qn !== 0) {
    throw new Error(
      `mpn_divrem: qn must be 0 (MPFR shim asserts), got ${qn}`,
    );
  }
  if (!Number.isInteger(nn) || nn < 0) {
    throw new Error(
      `mpn_divrem: nn must be a non-negative integer, got ${nn}`,
    );
  }
  if (!Number.isInteger(dn) || dn < 1) {
    throw new Error(`mpn_divrem: dn must be >= 1, got ${dn}`);
  }
  if (nn < dn) {
    throw new Error(
      `mpn_divrem: nn must be >= dn, got nn=${nn}, dn=${dn}`,
    );
  }
  if (np.length < nn) {
    throw new Error(
      `mpn_divrem: np too short: need ${nn} limbs, got ${np.length}`,
    );
  }
  if (dp.length < dn) {
    throw new Error(
      `mpn_divrem: dp too short: need ${dn} limbs, got ${dp.length}`,
    );
  }

  // Step 2: combine to BigInt.
  const N = limbsToBigInt(np, nn);
  const D = limbsToBigInt(dp, dn);
  if (D === 0n) {
    throw new Error(`mpn_divrem: divisor is zero`);
  }

  // Step 3: BigInt truncated division. Both operands are non-negative,
  // so `/` and `%` on bigints match mpz_tdiv_qr semantics exactly.
  const Q = N / D;
  const R = N % D;

  // Step 4: decompose Q into nn-dn+1 little-endian limbs. The low
  // nn-dn limbs become `q`; the top slot becomes `qHigh`.
  const totalQLimbs = nn - dn + 1;
  const qAll = bigIntToLimbs(Q, totalQLimbs);
  const q = qAll.slice(0, nn - dn);
  // qAll[nn - dn] is structurally guaranteed to exist (we allocated
  // totalQLimbs = nn - dn + 1 entries), but the noUncheckedIndexedAccess
  // type narrows it to `bigint | undefined`. Fall back to 0n on the
  // (unreachable) undefined branch rather than throw -- there's no
  // soundness issue, just a type assertion.
  const qHighOpt = qAll[nn - dn];
  const qHigh: bigint = qHighOpt === undefined ? 0n : qHighOpt;

  // Step 5: decompose R into exactly `dn` little-endian limbs
  // (zero-padded on the MSB end if R is shorter than dn limbs).
  const r = bigIntToLimbs(R, dn);

  return {
    q: q as readonly bigint[],
    qHigh,
    r: r as readonly bigint[],
  };
}
