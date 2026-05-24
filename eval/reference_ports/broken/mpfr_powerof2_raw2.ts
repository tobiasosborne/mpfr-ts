/**
 * reference_ports/broken/mpfr_powerof2_raw2.ts — deliberately-buggy
 * mpfr_powerof2_raw2.
 *
 * Used to mutation-prove the golden master per docs/PILOT_PLAN.md
 * Step 8 and CLAUDE.md PIL.3. If this port scores composite > 0.55 on
 * the mpfr_powerof2_raw2 golden, the golden is too weak.
 *
 * **Deliberately broken: confuses the predicate with mpfr_powerof2_raw**
 * by checking whether `mant >= 2^(prec-1)` (i.e. accepting ANY valid
 * MSB-aligned mantissa as a power of 2). A plausible agent error: the
 * naïve "is the MSB set?" reading of the C function name. The MSB is
 * always set on a valid MSB-aligned mantissa, so this broken port
 * returns `true` on every well-formed input — flipping every false case
 * in the golden.
 *
 * Why this bug shape:
 *   - The function name "powerof2_raw2" reads as "check the raw limbs
 *     for a power of 2", which a hurried agent might implement as
 *     "test if the MSB is set" (since a power-of-2 normalised value
 *     has only the MSB set, an off-by-one-step reading is "check MSB
 *     is set").
 *   - This loses the "AND no other bits" half of the predicate.
 *   - The lost half is exactly what the C `while (xn > 0)` loop
 *     verifies — the kind of "trailing loop ignored" error LLMs make
 *     on multi-clause predicates.
 *
 * The broken port score depends heavily on the golden's true:false
 * ratio. With ~25% true cases (per the fuzz bias), this port flips ~75%
 * of cases and should land well under 0.55 composite.
 *
 * NOT used in production. NOT imported from `src/`. Do NOT fix this
 * file — the bug is the point.
 *
 * Ref: docs/PILOT_PLAN.md Step 8 — mutation-prove acceptance.
 * Ref: CLAUDE.md PIL.3 — composite must drop below 0.55 under mutation.
 * Ref: src/internal/mpfr/powerof2_raw2.ts — the correct version.
 */

export function mpfr_powerof2_raw2(mant: bigint, prec: bigint): boolean {
  // BUG: should be `mant === 1n << (prec - 1n)` (exactly equal).
  // Returns true whenever the MSB is set — i.e. on every valid
  // schema-aligned mantissa. Drops the "no other bits" requirement
  // from mpfr/src/powerof2.c L45-L48 (the while-loop).
  return mant >= 1n << (prec - 1n);
}
