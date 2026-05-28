/**
 * reference_ports/correct/mpfr_round_raw.ts -- calibration reference for
 * the SUBSTRATE primitive mpfr_round_raw.
 *
 * Faithful limb-array port of the flag=0, use_inexp=1 instantiation of
 * mpfr_round_raw_generic (mpfr/src/round_raw_generic.c L60-L273;
 * instantiated mpfr/src/round_prec.c L25-L28). Operates on raw GMP limb
 * arrays in little-endian order (xp[0] is the least-significant 2^64
 * word). Substrate class: the runner's requireCoreImport AST gate is
 * exempt (eval/harness/runner.ts L1188), so this file does NOT import
 * from src/core.ts.
 *
 * This is the PREP-step calibration port -- it MUST score composite 1.0
 * against eval/functions/mpfr_round_raw/golden.jsonl. The production
 * port (written later by the porter model) will live in
 * src/internal/mpfr/ and may share machinery with round_raw.ts's
 * single-bigint roundMantissa; this reference is a direct,
 * line-for-line translation of the C limb body to be a trustworthy
 * oracle for mutation-proving.
 *
 * Contract (round_raw_generic.c L32-L54)
 * --------------------------------------
 * Given an MSB-aligned xprec-bit source mantissa xp and value sign neg
 * (0 positive, 1 negative), round to yprec bits in mode rnd. Returns:
 *   - yp:    ceil(yprec/64) result limbs (little-endian, low rw bits of
 *            yp[0] masked to 0). On carry the limbs are the wrapped
 *            value (all zero for a power-of-two overflow).
 *   - carry: 0n or 1n. 1n => the rounding overflowed past the top; the
 *            true result is a power of two and the caller bumps the
 *            exponent.
 *   - inexp: inexact flag in {-2,-1,0,1,2}. +-2 == +-MPFR_EVEN_INEX
 *            (the ties-to-even marker; sign is sign of rounded - exact).
 *
 * Refs
 * ----
 *   - mpfr/src/round_raw_generic.c L60-L273 -- the C body.
 *   - mpfr/src/round_prec.c L25-L28 -- flag=0, use_inexp=1 instantiation.
 *   - mpfr/src/mpfr-impl.h L1190 -- MPFR_EVEN_INEX == 2.
 *   - GMP manual section 8.3 -- little-endian limb order.
 *   - CLAUDE.md PIL.3 -- mutation-prove the golden against this file.
 */

const LIMB_BITS = 64n;
const LIMB_MAX = (1n << LIMB_BITS) - 1n; // 0xFFFFFFFFFFFFFFFF
const LIMB_ONE = 1n;
const MPFR_EVEN_INEX = 2;

/** Number of limbs needed to hold `prec` bits (ceil division). */
function precToLimbs(prec: bigint): number {
  return Number((prec + LIMB_BITS - 1n) / LIMB_BITS);
}

/** MPFR_LIMB_MASK(s): the low `s` bits set, for 0 <= s < 64. */
function limbMask(s: bigint): bigint {
  return (LIMB_ONE << s) - 1n;
}

/** Wrap a bigint to a single 64-bit limb. */
function wrapLimb(v: bigint): bigint {
  return v & LIMB_MAX;
}

/**
 * Add `inc` to the nw-limb little-endian array `src` (starting at the
 * top of xp via the caller's offset, but here we pass the exact nw-limb
 * window), returning {limbs, carry}. Mirrors mpn_add_1 over `nw` limbs.
 */
function mpnAdd1(window: bigint[], inc: bigint): { limbs: bigint[]; carry: bigint } {
  const out = window.slice();
  let carry = inc;
  for (let i = 0; i < out.length && carry !== 0n; i++) {
    const sum = out[i]! + carry;
    out[i] = wrapLimb(sum);
    carry = sum >> LIMB_BITS;
  }
  return { limbs: out, carry };
}

export function mpfr_round_raw_generic(
  xp: readonly bigint[],
  xprec: bigint,
  neg: number,
  yprec: bigint,
  rnd: string,
): { yp: readonly bigint[]; carry: bigint; inexp: number } {
  // round_raw_generic.c L82: MPFR_ASSERTD(neg == 0 || neg == 1).
  const isNeg = neg !== 0;

  // round_raw_generic.c L88-L102: RNDF maps to RNDZ with inexp forced 0.
  let rndMode = rnd;
  let inexpForcedZero = false;
  if (rndMode === 'RNDF') {
    rndMode = 'RNDZ';
    inexpForcedZero = true;
  }

  // MPFR_IS_LIKE_RNDZ(rnd, neg): RNDZ always; RNDD when value >= 0
  // (neg==0); RNDU when value < 0 (neg==1).
  const likeRndz =
    rndMode === 'RNDZ' ||
    (rndMode === 'RNDD' && !isNeg) ||
    (rndMode === 'RNDU' && isNeg);

  // round_raw_generic.c L104-L106.
  const xsize = precToLimbs(xprec);
  let nw = Number(yprec / LIMB_BITS); // whole limbs of yprec
  const rw = Number(yprec & (LIMB_BITS - 1n)); // remainder bits

  // round_raw_generic.c L108-L123: xprec <= yprec, no rounding.
  if (xprec <= yprec) {
    if (rw) nw++;
    // yp has nw limbs; copy xp into the TOP xsize limbs, zero-pad below.
    const yp: bigint[] = new Array<bigint>(nw).fill(0n);
    for (let i = 0; i < xsize; i++) {
      yp[nw - xsize + i] = xp[i] ?? 0n;
    }
    return { yp, carry: 0n, inexp: inexpForcedZero ? 0 : 0 };
  }

  // From here xprec > yprec: rounding is required.
  // round_raw_generic.c L125: the inexp/non-RNDZ branch vs the
  // RNDZ/no-inexp branch. Since use_inexp=1 (new_use_inexp truthy unless
  // RNDF forced it 0), we take the inexp branch whenever NOT RNDF, OR
  // when not like-RNDZ.
  const useInexp = !inexpForcedZero;

  if (useInexp || !likeRndz) {
    // round_raw_generic.c L127-L139.
    let k = xsize - nw - 1;
    let himask: bigint;
    let lomask: bigint;
    if (rw) {
      nw++;
      lomask = limbMask(LIMB_BITS - BigInt(rw));
      himask = LIMB_MAX & ~lomask;
    } else {
      lomask = LIMB_MAX;
      himask = LIMB_MAX;
    }

    // sb = xp[k] & lomask; first non-significant bits. (L141)
    let sb = (xp[k] ?? 0n) & lomask;

    // The kept window: the top nw limbs of xp (xp[xsize-nw .. xsize-1]).
    const windowStart = xsize - nw;
    const buildYpTrunc = (): bigint[] => {
      // mpn_copyi(yp, xp + xsize - nw, nw); yp[0] &= himask.
      const yp: bigint[] = new Array<bigint>(nw);
      for (let i = 0; i < nw; i++) yp[i] = xp[windowStart + i] ?? 0n;
      yp[0] = yp[0]! & himask;
      return yp;
    };
    const buildYpAddOne = (): { yp: bigint[]; carry: bigint } => {
      // carry = mpn_add_1(yp, xp+xsize-nw, nw, rw ? 1<<(64-rw) : 1);
      // yp[0] &= himask.
      const window: bigint[] = new Array<bigint>(nw);
      for (let i = 0; i < nw; i++) window[i] = xp[windowStart + i] ?? 0n;
      const inc = rw ? LIMB_ONE << (LIMB_BITS - BigInt(rw)) : LIMB_ONE;
      const { limbs, carry } = mpnAdd1(window, inc);
      limbs[0] = limbs[0]! & himask;
      return { yp: limbs, carry };
    };

    if (rndMode === 'RNDN' || rndMode === 'RNDNA') {
      // round_raw_generic.c L143-L202: rounding to nearest.
      const rbmask = LIMB_ONE << (LIMB_BITS - 1n - BigInt(rw));
      if ((sb & rbmask) === 0n) {
        // rounding bit 0 -> behave like RNDZ. (L148-L149, goto rnd_RNDZ)
        return rndRndz(xp, k, sb, windowStart, nw, himask, isNeg, buildYpTrunc);
      }
      if (rndMode === 'RNDNA') {
        // L151-L152: like rounding away from zero.
        const { yp, carry } = buildYpAddOne();
        const inexp = isNeg ? -1 : 1;
        return { yp, carry, inexp };
      }
      // L153-L155: first bits after the rounding bit.
      sb &= ~rbmask;
      while (sb === 0n && k > 0) {
        k--;
        sb = xp[k] ?? 0n;
      }
      if (sb === 0n) {
        // L156-L184: even rounding. Test the LSB of the kept value.
        const keptLsb = (xp[windowStart] ?? 0n) & (himask ^ (himask << 1n) & LIMB_MAX);
        if (keptLsb === 0n) {
          // L161-L172: kept LSB even -> round down (to even).
          // inexp = 2*EVEN*neg - EVEN  (neg in {0,1}) => -2 or +2.
          const inexp = 2 * MPFR_EVEN_INEX * (isNeg ? 1 : 0) - MPFR_EVEN_INEX;
          const yp = buildYpTrunc();
          return { yp, carry: 0n, inexp };
        }
        // L173-L184: kept LSB odd -> round up (to even), add one ulp.
        const inexp = MPFR_EVEN_INEX - 2 * MPFR_EVEN_INEX * (isNeg ? 1 : 0);
        const { yp, carry } = buildYpAddOne();
        return { yp, carry, inexp };
      }
      // L185-L201: sb != 0 -> ordinary round up.
      const inexp = 1 - 2 * (isNeg ? 1 : 0); // neg==0 ? 1 : -1
      const { yp, carry } = buildYpAddOne();
      return { yp, carry, inexp };
    }

    if (likeRndz) {
      // round_raw_generic.c L204-L220: rounding toward zero.
      return rndRndz(xp, k, sb, windowStart, nw, himask, isNeg, buildYpTrunc);
    }

    // round_raw_generic.c L221-L255: rounding away from zero.
    while (sb === 0n && k > 0) {
      k--;
      sb = xp[k] ?? 0n;
    }
    if (sb === 0n) {
      // L226-L238: exact -> truncate, inexp 0.
      const yp = buildYpTrunc();
      return { yp, carry: 0n, inexp: 0 };
    }
    // L239-L254: sb != 0 -> add one ulp.
    const inexp = 1 - 2 * (isNeg ? 1 : 0);
    const { yp, carry } = buildYpAddOne();
    return { yp, carry, inexp };
  }

  // round_raw_generic.c L257-L272: rounding toward zero, no inexact flag
  // (only reachable when inexpForcedZero (RNDF) and likeRndz). flag=0 so
  // we still build yp by truncation.
  let himask: bigint;
  if (rw) {
    nw++;
    himask = LIMB_MAX & ~limbMask(LIMB_BITS - BigInt(rw));
  } else {
    himask = LIMB_MAX;
  }
  const windowStart = xsize - nw;
  const yp: bigint[] = new Array<bigint>(nw);
  for (let i = 0; i < nw; i++) yp[i] = xp[windowStart + i] ?? 0n;
  yp[0] = yp[0]! & himask;
  return { yp, carry: 0n, inexp: 0 };
}

/**
 * The rnd_RNDZ label body (round_raw_generic.c L207-L219): scan sticky
 * bits, set inexp (0 if exact, else +-1 by sign), build yp by
 * truncation. Shared by the explicit-RNDZ path and the RNDN
 * rounding-bit-0 fall-through.
 */
function rndRndz(
  xp: readonly bigint[],
  k: number,
  sb0: bigint,
  _windowStart: number,
  _nw: number,
  _himask: bigint,
  isNeg: boolean,
  buildYpTrunc: () => bigint[],
): { yp: readonly bigint[]; carry: bigint; inexp: number } {
  let sb = sb0;
  let kk = k;
  // L208-L209: while (sb == 0 && k > 0) sb = xp[--k];
  while (sb === 0n && kk > 0) {
    kk--;
    sb = xp[kk] ?? 0n;
  }
  // L213: inexp = sb==0 ? 0 : (2*neg - 1).
  const inexp = sb === 0n ? 0 : 2 * (isNeg ? 1 : 0) - 1;
  const yp = buildYpTrunc();
  return { yp, carry: 0n, inexp };
}
