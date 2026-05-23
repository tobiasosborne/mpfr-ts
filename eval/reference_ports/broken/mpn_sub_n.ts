/**
 * reference_ports/broken/mpn_sub_n.ts — deliberately-buggy mpn_sub_n.
 *
 * Used to verify the golden master's discriminating power per
 * docs/PILOT_PLAN.md Step 8 (mutation-prove). A "broken" reference port
 * is the executable assertion that the goldens distinguish correct from
 * subtly-incorrect behaviour: if this file scores composite > 0.5 on the
 * mpn_sub_n golden, the golden is too weak and the function is NOT
 * Pilot-passed.
 *
 * **Deliberately broken: the inter-limb borrow is dropped.** Specifically,
 * `borrow` is reset to `0n` at the bottom of every loop iteration rather
 * than threaded into the next iteration's diff. This means:
 *
 *   - Cases where every limb-diff stays non-negative (no borrow ever
 *     produced) still pass: many small-n happy cases drawn from PRNG
 *     limbs satisfy this when `s1[i] >= s2[i]` per limb.
 *   - Every case with a real borrow ripple — `edge` (single-limb-of-0
 *     minus single-limb-of-1 at the LSB end, with all-ones to ripple
 *     through), `adversarial` (all-zero minus all-MSB-set, exercising
 *     full-length ripples), a large fraction of `fuzz` — fails because
 *     the dropped borrow leaves limbs above the underflowing position
 *     one too high, and the returned borrow disagrees with the correct
 *     value too (often 0n where the correct value would be 1n).
 *
 * This bug is structurally identical to the mpn_add_n broken variant
 * (carry dropped between limbs) — the same subtraction-side mistake an
 * agent would make by "extract the low 64 bits with mask, decide sign
 * locally" without threading the cross-limb borrow. Exercising it
 * through the goldens is a direct test of the borrow-chain coverage.
 *
 * NOT used in production. NOT imported from `src/`. Do NOT fix this
 * file — the bug is the point.
 *
 * Ref: docs/PILOT_PLAN.md Step 8 — mutation-prove acceptance.
 * Ref: CLAUDE.md PIL.3 — "perturb the reference port, confirm the
 *   composite drops below 0.95".
 */

const LIMB_BITS = 64n;
const LIMB_MASK = (1n << LIMB_BITS) - 1n;

export interface MpnSubNResult {
  readonly result: readonly bigint[];
  readonly borrow: bigint;
}

export function mpn_sub_n(
  s1: readonly bigint[],
  s2: readonly bigint[],
  n: number,
): MpnSubNResult {
  if (!Number.isInteger(n) || n < 0) {
    throw new Error(`mpn_sub_n: n must be a non-negative integer, got ${n}`);
  }
  if (s1.length < n) {
    throw new Error(
      `mpn_sub_n: s1 too short: need ${n} limbs, got ${s1.length}`,
    );
  }
  if (s2.length < n) {
    throw new Error(
      `mpn_sub_n: s2 too short: need ${n} limbs, got ${s2.length}`,
    );
  }

  const result: bigint[] = new Array<bigint>(n);
  let borrow: bigint = 0n;

  for (let i = 0; i < n; i++) {
    const a = s1[i];
    const b = s2[i];
    if (a === undefined || b === undefined) {
      throw new Error(
        `mpn_sub_n: internal invariant violated — undefined limb at index ${i}`,
      );
    }
    const diff = a - b - borrow;
    result[i] = diff & LIMB_MASK;
    // BUG: the borrow-out from this limb is computed correctly below,
    // but then immediately discarded on the next line. The next
    // iteration's `diff` therefore omits the propagated borrow.
    borrow = diff < 0n ? 1n : 0n;
    borrow = 0n; // <-- the deliberate bug: drops inter-limb borrow.
  }

  return { result: result as readonly bigint[], borrow };
}
