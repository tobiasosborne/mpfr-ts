/**
 * mpn/tdiv_qr.ts -- pure-TS port of GMP's `mpn_tdiv_qr` (qxn == 0 path).
 *
 * Substrate-class helper. Operates on raw `bigint[]` limb arrays, NOT on
 * the idiomatic-surface `MPFR` value type from `src/core.ts` -- hence no
 * core import here (CLAUDE.md Law 3: "faithful substrate, idiomatic
 * surface"). Like `mpn_divrem`, this routine mirrors the GMP / mini-gmp
 * I/O contract byte-for-byte; many MPFR-level division paths reach
 * `mpn_tdiv_qr` directly (it is the divrem cousin that returns the
 * quotient as a length-`(nn-dn+1)` limb array rather than splitting into
 * `q[]` + scalar `qHigh`).
 *
 * C signature (GMP / mpfr-mini-gmp shim, qxn == 0):
 *
 *   void mpn_tdiv_qr(mp_limb_t       *qp,
 *                    mp_limb_t       *rp,
 *                    mp_size_t        qxn,
 *                    const mp_limb_t *np,
 *                    mp_size_t        nn,
 *                    const mp_limb_t *dp,
 *                    mp_size_t        dn);
 *
 *   - qxn must be 0 (the only path MPFR exercises; the mini-gmp shim
 *     literally `MPFR_ASSERTN (qxn == 0)`).
 *   - On return: qp[0 .. nn-dn] holds nn-dn+1 little-endian quotient
 *     limbs; rp[0 .. dn) holds dn little-endian remainder limbs
 *     (zero-padded on the MSB end if the true remainder is shorter).
 *
 * TS signature (this port):
 *
 *   mpn_tdiv_qr(qxn, np, nn, dp, dn) -> { q, r }
 *
 * Idiomatic TS returns a fresh `{q, r}` object with `q.length === nn -
 * dn + 1` and `r.length === dn`. The substrate is immutable at the
 * function boundary; callers that need in-place behaviour wrap and copy.
 *
 * Algorithm
 * ---------
 *
 * Delegates to the shipped `mpn_divrem(qn=0, np, nn, dp, dn)` substrate
 * helper, which performs the same `mpz_tdiv_qr`-equivalent BigInt
 * pack/divide/unpack as the mini-gmp shim (mpfr/src/mpfr-mini-gmp.c
 * L216-L262). `mpn_divrem` returns `{q, qHigh, r}` where `q.length ==
 * nn - dn` and `qHigh` is the (nn-dn)+1-th quotient limb; the `tdiv_qr`
 * shape concatenates them into a single length-(nn-dn+1) array:
 * `q' = [...q, qHigh]`.
 *
 * Why this is correct: the divrem shim and the tdiv_qr shim implement
 * the same identity (Q = floor(N / D); R = N - Q*D), and the only shape
 * difference between the two is "is the high quotient limb at qp[nn-dn]
 * or returned as a scalar?". Concatenation closes that gap exactly.
 *
 * Limb endianness
 * ---------------
 *
 * Little-endian limb order throughout: `limbs[0]` is the least-significant
 * 2^64 word. (CLAUDE.md "mpn limbs are LITTLE-ENDIAN by limb index.")
 *
 * Refs
 * ----
 *
 *   - GMP manual section 8.3 "Low-level Functions": mpn_tdiv_qr.
 *   - mpfr/src/mpfr-mini-gmp.c L246-L262 -- the SHIM body this port
 *     mirrors (asserts qxn == 0, computes qn := nn - dn + 1, calls
 *     mpz_tdiv_qr, copies qp / rp back).
 *   - src/internal/mpn/divrem.ts -- shipped delegate.
 *   - eval/functions/mpn_tdiv_qr/spec.json -- contract.
 *   - eval/reference_ports/correct/mpn_tdiv_qr.ts -- mutation-prove
 *     calibration baseline this production port shadows.
 *
 * Invariants
 * ----------
 *
 *   Precondition:  qxn == 0
 *                  AND nn >= dn >= 1
 *                  AND np.length >= nn, dp.length >= dn
 *                  AND every limb is a non-negative bigint < 2^64
 *                  AND combine(dp, dn) > 0.
 *
 *   Postcondition: returned `q.length === nn - dn + 1`
 *                  AND returned `r.length === dn`
 *                  AND every output limb is in [0, 2^64)
 *                  AND combine(q) * combine(dp) + combine(r) == combine(np)
 *                  AND combine(r) < combine(dp).
 */

import { mpn_divrem } from './divrem.ts';

export interface MpnTdivQrResult {
  /** Quotient limbs, little-endian. Length `nn - dn + 1`. */
  readonly q: readonly bigint[];
  /** Remainder limbs, little-endian, zero-padded. Length `dn`. */
  readonly r: readonly bigint[];
}

/**
 * Divide an `nn`-limb non-negative integer by a `dn`-limb non-negative
 * integer (both little-endian), returning the `nn-dn+1`-limb quotient
 * and `dn`-limb remainder.
 *
 * @param qxn Must be 0 (mirrors `MPFR_ASSERTN(qxn == 0)` in the
 *            mini-gmp shim). Nonzero values throw rather than silently
 *            misbehave.
 * @param np  Dividend limb array; `length >= nn`.
 * @param nn  Dividend limb count. Must satisfy `nn >= dn`.
 * @param dp  Divisor limb array; `length >= dn`.
 * @param dn  Divisor limb count. Must satisfy `dn >= 1`.
 *
 * @returns   `{q, r}` with `q.length === nn - dn + 1` (high limb at
 *            `q[nn - dn]`) and `r.length === dn` (zero-padded on the
 *            MSB end if the true remainder is shorter than `dn` limbs).
 *
 * @throws `Error` on a domain violation: `qxn != 0`, `nn < dn`,
 *         `dn < 1`, either operand array too short, or a zero divisor.
 *         Substrate is no-core-import; we use plain `Error`, not
 *         `MPFRError`.
 */
export function mpn_tdiv_qr(
  qxn: number,
  np: readonly bigint[],
  nn: number,
  dp: readonly bigint[],
  dn: number,
): MpnTdivQrResult {
  if (qxn !== 0) {
    throw new Error(
      `mpn_tdiv_qr: qxn must be 0 (MPFR shim asserts), got ${qxn}`,
    );
  }

  // Delegate to the shipped divrem substrate (it owns the pack/divide/
  // unpack and the precondition checks for nn/dn/array lengths).
  const { q: qLow, qHigh, r } = mpn_divrem(0, np, nn, dp, dn);

  // tdiv_qr's quotient shape is a single length-(nn-dn+1) array whose
  // top slot is mpn_divrem's qHigh. Construct fresh so the result is
  // independent of the divrem-returned readonly arrays.
  const q: bigint[] = new Array<bigint>(nn - dn + 1);
  for (let i = 0; i < nn - dn; i++) {
    const limb = qLow[i];
    if (limb === undefined) {
      // Structurally unreachable: mpn_divrem guarantees qLow.length ==
      // nn - dn. The check exists to satisfy noUncheckedIndexedAccess
      // and to fail loudly if a future divrem refactor weakens that
      // guarantee.
      throw new Error(
        `mpn_tdiv_qr: divrem returned short quotient at index ${i}`,
      );
    }
    q[i] = limb;
  }
  q[nn - dn] = qHigh;

  return { q: q as readonly bigint[], r };
}
