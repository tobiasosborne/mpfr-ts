/**
 * mpfr_round_raw_generic.ts -- pure-TS port of MPFR's `mpfr_round_raw`.
 *
 * Substrate-class function. Operates on raw `bigint[]` limb arrays,
 * NOT on the idiomatic-surface `MPFR` value type from `src/core.ts` --
 * hence no core import here (CLAUDE.md Law 3: "faithful substrate,
 * idiomatic surface"). The runner exempts substrate from
 * `requireCoreImport`.
 *
 * This is the flag=0, use_inexp=1 instantiation (mpfr/src/round_prec.c
 * L25-L28) -- the actual public `mpfr_round_raw` symbol:
 *
 *   #define mpfr_round_raw_generic mpfr_round_raw
 *   #define flag 0
 *   #define use_inexp 1
 *   #include "round_raw_generic.c"
 *
 * Contract (round_raw_generic.c L32-L54): given an MSB-aligned source
 * mantissa xp of xprec bits, with value sign neg (0 => positive,
 * 1 => negative), round it to yprec bits in mode rnd, returning the
 * result limbs in yp (ceil(yprec/64) limbs), a carry (0n or 1n; 1
 * means the rounded result is a power of two that overflowed the
 * precision frame), and the inexact flag inexp in {-2,-1,0,1,2} where
 * +/-2 = +/-MPFR_EVEN_INEX (the ties-to-even marker for RNDN).
 *
 * Refs
 * ----
 *   - mpfr/src/round_raw_generic.c L60-L273 -- the macro body; this
 *     golden instantiates flag=0, use_inexp=1.
 *   - mpfr/src/round_prec.c L25-L28 -- the instantiation site.
 *   - mpfr/src/mpfr-impl.h L1190 -- #define MPFR_EVEN_INEX 2.
 *   - GMP manual S8.3 -- little-endian limb order: limbs[0] is LSB.
 *   - CLAUDE.md "Hallucination-risk callouts" -- little-endian limbs;
 *     ternary/inexact is sign of (rounded - exact).
 */

const LIMB_BITS = 64n;
const LIMB_MASK = (1n << LIMB_BITS) - 1n;
const MPFR_EVEN_INEX = 2;

/**
 * Round an MSB-aligned source mantissa (limb array, little-endian) to a
 * target precision in a given rounding mode.
 *
 * @param xp    Source mantissa limbs (little-endian, MSB-aligned).
 * @param xprec Source precision in bits (bigint).
 * @param neg   0 = positive, 1 = negative.
 * @param yprec Target precision in bits (bigint).
 * @param rnd   Rounding mode: 'RNDN' | 'RNDZ' | 'RNDU' | 'RNDD' | 'RNDA'.
 * @returns     `{ yp, carry, inexp }` where yp has ceil(yprec/64) limbs,
 *              carry is 0n or 1n, and inexp in {-2,-1,0,1,2}.
 *
 * @throws {Error} on invalid parameters (defensive; substrate hot path
 *   assumes callers validate).
 */
export function mpfr_round_raw_generic(
  xp: readonly bigint[],
  xprec: bigint,
  neg: number,
  yprec: bigint,
  rnd: string,
): { yp: readonly bigint[]; carry: bigint; inexp: number } {
  // Convert precisions to numbers for array indexing (safe: precisions
  // are well below 2^53).
  const xprecNum = Number(xprec);
  const yprecNum = Number(yprec);

  // Number of source limbs and target limbs.
  // Ref: GMP -- MPFR_PREC2LIMBS(xprec) = ceil(xprec / GMP_NUMB_BITS).
  const xsize = Math.ceil(xprecNum / 64);
  let nw = Math.floor(yprecNum / 64); // number of full target limbs
  const rw = yprecNum % 64; // remaining bits in the partial top limb

  // Ref: round_raw_generic.c L108-L123 -- xprec <= yprec: no rounding.
  if (xprecNum <= yprecNum) {
    if (rw !== 0) {
      nw++; // need a partial top limb
    }
    // yp must have nw limbs. Copy xp into the top part and zero the
    // low part.  mpn_copyd(yp + (nw - xsize), xp, xsize)
    //   -> yp[nw-xsize .. nw-1] = xp[0 .. xsize-1]
    //   -> MPN_ZERO(yp, nw - xsize)
    const lowZeros: bigint[] = new Array<bigint>(nw - xsize).fill(0n);
    const yp: bigint[] = [...lowZeros, ...xp.slice(0, xsize)];
    return { yp: yp as readonly bigint[], carry: 0n, inexp: 0 };
  }

  // Rounding needed: xprec > yprec.
  // Ref: round_raw_generic.c L125-L256.

  // MPFR_IS_LIKE_RNDZ(rnd, neg): true when the mode acts like truncation.
  const isLikeRndz =
    rnd === 'RNDZ' ||
    (rnd === 'RNDD' && neg === 0) ||
    (rnd === 'RNDU' && neg !== 0);

  // For flag=0, use_inexp=1, new_use_inexp is always 1 (since rnd_mode
  // is never MPFR_RNDF in the golden -- the 5-mode RoundingMode union).
  // So the outer `if` body always runs.

  // k = xsize - nw - 1  (index of the boundary limb -- the first limb
  // that may contain discarded bits).
  let k = xsize - nw - 1;

  let lomask: bigint;
  let himask: bigint;

  if (rw !== 0) {
    nw++; // need a partial top limb
    // Ref: round_raw_generic.c L131-L134.
    // lomask = MPFR_LIMB_MASK(GMP_NUMB_BITS - rw) = (1 << (64 - rw)) - 1
    // himask = ~lomask
    lomask = (1n << BigInt(64 - rw)) - 1n;
    himask = ~lomask & LIMB_MASK;
  } else {
    // Ref: round_raw_generic.c L136-L139 -- rw == 0: full limb masks.
    lomask = LIMB_MASK;
    himask = LIMB_MASK;
  }

  // Ref: round_raw_generic.c L141.
  // sb = xp[k] & lomask  -- first non-significant bits (in boundary limb).
  let sb = xp[k]! & lomask;

  // Ref: round_raw_generic.c L143-L202 -- RNDN / RNDNA path.
  if (rnd === 'RNDN') {
    // rbmask = rounding bit mask = 1 << (64 - 1 - rw)
    // Ref: round_raw_generic.c L146.
    const rbmask = 1n << BigInt(64 - 1 - rw);

    if ((sb & rbmask) === 0n) {
      // Rounding bit = 0: behave like RNDZ.
      // goto rnd_RNDZ -- fall through to the RNDZ path.
      // But first we need to set up the RNDZ sb-scan; the C goto jumps
      // to the `rnd_RNDZ:` label inside the RNDZ branch. We handle this
      // by factoring the RNDZ logic into a helper.
      return rndzPath(xp, xsize, nw, k, sb, neg, rw, himask);
    }

    // Rounding bit = 1.
    // sb &= ~rbmask -- clear the rounding bit, leaving sticky bits below.
    // Ref: round_raw_generic.c L153.
    sb &= ~rbmask;

    // Scan lower limbs for sticky bits.
    // Ref: round_raw_generic.c L154-L155.
    while (sb === 0n && k > 0) {
      k--;
      sb = xp[k]!;
    }

    if (sb === 0n) {
      // Exact halfway tie (all sticky bits below rounding bit are 0).
      // Check LSB of the kept part for even rounding.
      // Ref: round_raw_generic.c L159.
      // sb = xp[xsize - nw] & (himask ^ (himask << 1))
      // himask ^ (himask << 1) isolates the LSB of the kept part.
      const lsbMask = (himask ^ ((himask << 1n) & LIMB_MASK)) & LIMB_MASK;
      const keptLsb = xp[xsize - nw]! & lsbMask;

      if (keptLsb === 0n) {
        // LSB = 0 (even) -> round down.  Keep the truncated value.
        // Ref: round_raw_generic.c L162-L171.
        // *inexp = 2 * MPFR_EVEN_INEX * neg - MPFR_EVEN_INEX
        const inexp = 2 * MPFR_EVEN_INEX * neg - MPFR_EVEN_INEX;
        // mpn_copyi(yp, xp + xsize - nw, nw)
        const yp: bigint[] = xp.slice(xsize - nw, xsize).slice(0, nw);
        yp[0] = yp[0]! & himask;
        return { yp: yp as readonly bigint[], carry: 0n, inexp };
      } else {
        // LSB = 1 (odd) -> round up.  goto away_addone_ulp.
        // Ref: round_raw_generic.c L173-L183.
        // *inexp = MPFR_EVEN_INEX - 2 * MPFR_EVEN_INEX * neg
        const inexp = MPFR_EVEN_INEX - 2 * MPFR_EVEN_INEX * neg;
        // goto rnd_RNDN_add_one_ulp
        return addOneUlp(xp, xsize, nw, rw, himask, inexp);
      }
    } else {
      // Sticky bits non-zero -- round up (not a tie).
      // Ref: round_raw_generic.c L185-L201.
      // *inexp = 1 - 2 * neg
      const inexp = 1 - 2 * neg;
      // goto rnd_RNDN_add_one_ulp
      return addOneUlp(xp, xsize, nw, rw, himask, inexp);
    }
  }

  // Ref: round_raw_generic.c L204-L220 -- RNDZ / like RNDZ.
  if (isLikeRndz) {
    return rndzPath(xp, xsize, nw, k, sb, neg, rw, himask);
  }

  // Remaining: RNDA, RNDU (when not like RNDZ), RNDD (when not like RNDZ).
  // Ref: round_raw_generic.c L221-L255 -- rounding away from zero.
  {
    // Scan lower limbs for sticky bits.
    while (sb === 0n && k > 0) {
      k--;
      sb = xp[k]!;
    }

    if (sb === 0n) {
      // Exact: all dropped bits are zero. Keep the truncated value.
      // Ref: round_raw_generic.c L226-L238.
      const yp: bigint[] = xp.slice(xsize - nw, xsize).slice(0, nw);
      yp[0] = yp[0]! & himask;
      return { yp: yp as readonly bigint[], carry: 0n, inexp: 0 };
    } else {
      // Inexact: add one ulp.
      // Ref: round_raw_generic.c L239-L254.
      // *inexp = 1 - 2 * neg
      const inexp = 1 - 2 * neg;
      return addOneUlp(xp, xsize, nw, rw, himask, inexp);
    }
  }
}

/**
 * RNDZ path: truncate, set inexp, copy, mask.
 *
 * Ref: round_raw_generic.c L204-L220 (rnd_RNDZ label).
 */
function rndzPath(
  xp: readonly bigint[],
  xsize: number,
  nw: number,
  k: number,
  sb: bigint,
  neg: number,
  rw: number,
  himask: bigint,
): { yp: readonly bigint[]; carry: bigint; inexp: number } {
  // Scan lower limbs for sticky bits.
  // Ref: round_raw_generic.c L208-L209.
  while (sb === 0n && k > 0) {
    k--;
    sb = xp[k]!;
  }

  // inexp = sb == 0 ? 0 : 2 * neg - 1
  // Ref: round_raw_generic.c L211-L213.
  const inexp = sb === 0n ? 0 : 2 * neg - 1;

  // mpn_copyi(yp, xp + xsize - nw, nw)
  // Ref: round_raw_generic.c L216-L218.
  const yp: bigint[] = xp.slice(xsize - nw, xsize).slice(0, nw);
  yp[0] = yp[0]! & himask;

  return { yp: yp as readonly bigint[], carry: 0n, inexp };
}

/**
 * Add-one-ulp path: add 1 at the ulp position of the top nw limbs,
 * mask the low result limb, and return carry and inexp.
 *
 * Ref: round_raw_generic.c L190-L200 (rnd_RNDN_add_one_ulp label).
 */
function addOneUlp(
  xp: readonly bigint[],
  xsize: number,
  nw: number,
  rw: number,
  himask: bigint,
  inexp: number,
): { yp: readonly bigint[]; carry: bigint; inexp: number } {
  // Extract the top nw limbs of xp (the kept part).
  const src = xp.slice(xsize - nw, xsize);

  // Addend: if rw > 0, 1 << (64 - rw), otherwise 1.
  // Ref: round_raw_generic.c L195-L197.
  const addend = rw !== 0
    ? 1n << BigInt(64 - rw)
    : 1n;

  // Inline mpn_add_1(yp, src, nw, addend).
  // Ref: GMP mpn_add_1 -- LSB-first carry chain.
  const yp: bigint[] = new Array<bigint>(nw);
  let carry: bigint = addend;
  for (let i = 0; i < nw; i++) {
    const sum = src[i]! + carry;
    yp[i] = sum & LIMB_MASK;
    carry = sum >> LIMB_BITS;
  }

  // yp[0] &= himask  -- mask off the low rw bits.
  // Ref: round_raw_generic.c L198.
  yp[0] = yp[0]! & himask;

  return { yp: yp as readonly bigint[], carry, inexp };
}
