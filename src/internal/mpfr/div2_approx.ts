/**
 * mpfr/div2_approx.ts — pure-TS port of MPFR's `mpfr_div2_approx`.
 *
 * Substrate-class helper. Operates on raw uint64 limb values as bigints,
 * NOT on the idiomatic-surface `MPFR` value type from `src/core.ts` —
 * hence no core import here (CLAUDE.md Law 3: "faithful substrate, idiomatic
 * surface").
 *
 * C signature (MPFR internal, static):
 *
 *   static void mpfr_div2_approx(mpfr_limb_ptr Q1, mpfr_limb_ptr Q0,
 *                                mp_limb_t u1, mp_limb_t u0,
 *                                mp_limb_t v1, mp_limb_t v0)
 *
 * TS signature (this port):
 *
 *   mpfr_div2_approx(u1, u0, v1, v0) -> { Q1: bigint, Q0: bigint }
 *
 * Algorithm
 * ---------
 *
 * Given u = u1*B + u0 < v = v1*B + v0 with v normalised (high bit of v1 set),
 * computes Q = Q1*B + Q0 such that Q <= floor(u*B^2 / v) <= Q + 21.
 *
 * The C implementation uses __gmpfr_invert_limb_approx which is backed by a
 * 256-entry x86-ASM LUT. The golden driver replicates the algorithm but
 * substitutes a portable approximate inverse via __uint128_t arithmetic.
 * This TS port mirrors the golden driver's algorithm exactly for bit-exact
 * output.
 *
 * Ref: mpfr/src/div.c L47-L103 — the C reference body.
 * Ref: eval/functions/mpfr_div2_approx/golden_driver.c L28-L126 — portable
 *   mirror of the algorithm used by the golden, including the portable
 *   invert_limb_approx via __uint128_t arithmetic. This TS port mirrors
 *   the driver's algorithm step-by-step for bit-exact output.
 * Ref: mpfr/src/invert_limb.h — __gmpfr_invert_limb_approx (LUT-based
 *   inverse, not portably replicable; golden uses portable substitute).
 * Ref: CLAUDE.md "Hallucination-risk callouts: umul_ppmm output args are
 *   first" — umul_ppmm(hi, lo, a, b) writes a*b into (hi, lo); the first
 *   two args are OUTPUTS. Reproduced here as separate hi/lo captures.
 * Ref: CLAUDE.md "Hallucination-risk callouts: GMP mpn limbs are
 *   LITTLE-ENDIAN by limb index".
 *
 * Key implementation note on carry detection
 * -------------------------------------------
 *
 * C uint64_t addition wraps on overflow; carry detection is `new >= old`
 * being false (wrapping detected). BigInt never overflows, so we must detect
 * carry differently: carry = (mathematical_sum >= 2^64).
 *
 * Invariants
 * ----------
 *
 *   Preconditions: v1 has high bit set (normalised); u1*B+u0 < v1*B+v0.
 *   All input bigints are in [0, 2^64).
 *
 *   Postcondition: Q1, Q0 in [0, 2^64); Q = Q1*B+Q0 is a lower approximation
 *   of floor(u*B^2/v) within 21 of the true quotient.
 */

const B = 1n << 64n;       // 2^64 — the limb base
const MASK64 = B - 1n;     // mask for low 64 bits
const UINT64_MAX = MASK64; // 0xFFFFFFFFFFFFFFFF

/**
 * Compute the approximate inverse of v (a normalised 64-bit limb, i.e.
 * high bit set). Mirrors the golden_driver.c invert_limb_approx function:
 *
 *   num = (B - 1) * B       = B^2 - B
 *   num -= v                = B^2 - B - v
 *   return floor(num / v)
 *
 * This is a portable substitute for __gmpfr_invert_limb_approx (LUT-based).
 *
 * Ref: eval/functions/mpfr_div2_approx/golden_driver.c L34-L42.
 */
function invert_limb_approx(v: bigint): bigint {
  // (B-1)*B = B^2 - B
  let num = (B - 1n) * B;
  // B^2 - B - v
  num -= v;
  // floor(num / v), then cast to uint64_t (truncate to 64 bits).
  // The C driver does: return (uint64_t)((num / v)); which truncates.
  // The quotient can exceed 2^64 for small v (e.g. v near 2^63 gives ~2^65),
  // so we MUST mask to 64 bits to match the C uint64_t cast.
  // Ref: golden_driver.c L40-L41 — `return (uint64_t)((num / v))`.
  return (num / v) & MASK64;
}

/**
 * Unsigned 64-bit addition with carry out. Returns {lo, cy} where lo is
 * (a + b) mod 2^64 and cy is 1 if (a + b) >= 2^64, else 0.
 *
 * In C: uint64_t new_v = a + b; cy = (new_v < a) ? 1 : 0; (overflow detection)
 * In BigInt: no overflow, so we detect carry as mathematical_sum >= B.
 */
function addCarry(a: bigint, b: bigint): { lo: bigint; cy: bigint } {
  const sum = a + b;
  if (sum >= B) {
    return { lo: sum - B, cy: 1n };
  }
  return { lo: sum, cy: 0n };
}

/**
 * Approximate 2-limb quotient approximation.
 *
 * Given u = u1*B + u0 and v = v1*B + v0, with v normalised (v1 has MSB set)
 * and u < v, computes Q = {Q1, Q0} (in [0, B)) such that
 *   Q <= floor(u * B^2 / v) <= Q + 21.
 *
 * @param u1 High limb of numerator (bigint, [0, 2^64))
 * @param u0 Low limb of numerator (bigint, [0, 2^64))
 * @param v1 High limb of denominator (bigint, [0, 2^64), MSB set)
 * @param v0 Low limb of denominator (bigint, [0, 2^64))
 * @returns { Q1, Q0 } — 2-limb lower approximation of floor(u*B^2/v)
 *
 * Ref: eval/functions/mpfr_div2_approx/golden_driver.c L48-L126.
 * Ref: mpfr/src/div.c L47-L103 — C original (uses LUT-based inverse).
 */
export function mpfr_div2_approx(
  u1: bigint,
  u0: bigint,
  v1: bigint,
  v0: bigint,
): { Q1: bigint; Q0: bigint } {
  // Compute the approximate inverse of v1 (or v1+1 if v1 is not UINT64_MAX).
  // Ref: golden_driver.c L53-L54.
  let inv: bigint;
  if (v1 === UINT64_MAX) {
    inv = 0n;
  } else {
    inv = invert_limb_approx(v1 + 1n);
  }

  // q1:q0 = u1 * inv  (umul_ppmm — hi, lo outputs)
  // Ref: golden_driver.c L56-L61 — __uint128_t p = u1 * inv; q1 = p>>64; q0 = p&MASK.
  let q1: bigint;
  let q0: bigint;
  {
    const p = u1 * inv;
    q1 = p >> 64n;
    q0 = p & MASK64;
  }
  // Ref: golden_driver.c L62 — q1 += u1.
  // Note: q1 may overflow 64 bits here in the C code (it's all fine since
  // the C uses uint64_t which wraps). We apply MASK64 to keep 64-bit range.
  q1 = (q1 + u1) & MASK64;

  // r1:r0 = q1 * v1  (umul_ppmm)
  // Ref: golden_driver.c L64-L68.
  let r1: bigint;
  let r0: bigint;
  {
    const p = q1 * v1;
    r1 = p >> 64n;
    r0 = p & MASK64;
  }

  // xx:yy = q1 * v0  (umul_ppmm)
  // Ref: golden_driver.c L69-L74.
  let xx: bigint;
  let yy: bigint;
  {
    const p = q1 * v0;
    xx = p >> 64n;
    yy = p & MASK64;
  }

  // ADD_LIMB(r0, xx, cy): r0 += xx; cy = carry-out; r1 += cy.
  // In C: uint64 addition wraps and carry is detected by new < old.
  // In BigInt: detect carry as sum >= 2^64.
  // Ref: golden_driver.c L76-L83.
  {
    const res = addCarry(r0, xx);
    r0 = res.lo;
    r1 = (r1 + res.cy) & MASK64;
  }

  // Increment r0 if yy != 0; propagate carry to r1.
  // Ref: golden_driver.c L85-L90.
  if (yy !== 0n) {
    const res = addCarry(r0, 1n);
    if (res.cy === 1n) {
      r1 = (r1 + 1n) & MASK64;
    }
    r0 = res.lo;
  }

  // r0 = u0 - r0; r1 = u1 - r1 - borrow(r0 > u0).
  // In C: unsigned subtraction; borrow = (r0 > u0) BEFORE subtracting r0 from u0.
  // Ref: golden_driver.c L91-L97.
  {
    const borrow: bigint = r0 > u0 ? 1n : 0n;
    r0 = (u0 - r0) & MASK64;
    r1 = (u1 - r1 - borrow) & MASK64;
  }

  // q0 = r0; q1 += r1.
  // Ref: golden_driver.c L99-L100.
  q0 = r0;
  q1 = (q1 + r1) & MASK64;

  // xx:yy = r0 * inv (umul_ppmm); yy is ignored (void cast in C).
  // Ref: golden_driver.c L101-L106.
  {
    const p = r0 * inv;
    xx = p >> 64n;
    // yy discarded — golden_driver.c L105: (void)yy
  }

  // q0 += xx; carry to q1.
  // Ref: golden_driver.c L107-L112.
  {
    const res = addCarry(q0, xx);
    q0 = res.lo;
    q1 = (q1 + res.cy) & MASK64;
  }

  // Up to a few corrections via inv addition while r1 > 0.
  // The C comment says "up to 4 corrections". At this point r1 is the
  // number of remaining correction steps needed (small: 0-4).
  // IMPORTANT: r1 here is the CURRENT r1 value (post-subtraction), NOT
  // the original r1. Since u < v and v is normalised, this is bounded.
  // Ref: golden_driver.c L113-L121.
  while (r1 > 0n) {
    const res = addCarry(q0, inv);
    q0 = res.lo;
    q1 = (q1 + res.cy) & MASK64;
    r1--;
  }

  return { Q1: q1, Q0: q0 };
}
