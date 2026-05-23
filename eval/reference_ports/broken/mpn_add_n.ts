/**
 * reference_ports/broken/mpn_add_n.ts — deliberately-buggy mpn_add_n.
 *
 * Used to verify the golden master's discriminating power per
 * docs/PILOT_PLAN.md Step 8 (mutation-prove). A "broken" reference port
 * is the executable assertion that the goldens distinguish correct from
 * subtly-incorrect behaviour: if this file scores composite > 0.5 on the
 * mpn_add_n golden, the golden is too weak and the function is NOT
 * Pilot-passed.
 *
 * **Deliberately broken: the inter-limb carry is dropped.** Specifically,
 * `carry` is reset to `0n` at the bottom of every loop iteration rather
 * than threaded into the next iteration's sum. This means:
 *
 *   - Cases where every limb-sum stays below 2^64 (no carry ever
 *     produced) still pass: most `happy` cases bias toward this regime
 *     by design (the C driver clears the high bit of the top limb).
 *   - Every case with a real ripple — `edge` (15), `adversarial` (all),
 *     a large fraction of `fuzz` — fails because the lost carry leaves
 *     the high-limb result one short and the returned carry is wrong
 *     too (0n where the correct value would be 1n, or vice versa).
 *
 * This bug is structurally identical to one of the most common naive-
 * port mistakes: agents who "extract the lower 64 bits with mask, the
 * upper with shift" sometimes forget that the upper bits must feed the
 * NEXT iteration. Exercising it through the goldens is a direct test
 * of the carry-chain coverage.
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

export interface MpnAddNResult {
  readonly result: readonly bigint[];
  readonly carry: bigint;
}

export function mpn_add_n(
  s1: readonly bigint[],
  s2: readonly bigint[],
  n: number,
): MpnAddNResult {
  if (!Number.isInteger(n) || n < 0) {
    throw new Error(`mpn_add_n: n must be a non-negative integer, got ${n}`);
  }
  if (s1.length < n) {
    throw new Error(
      `mpn_add_n: s1 too short: need ${n} limbs, got ${s1.length}`,
    );
  }
  if (s2.length < n) {
    throw new Error(
      `mpn_add_n: s2 too short: need ${n} limbs, got ${s2.length}`,
    );
  }

  const result: bigint[] = new Array<bigint>(n);
  let carry: bigint = 0n;

  for (let i = 0; i < n; i++) {
    const a = s1[i];
    const b = s2[i];
    if (a === undefined || b === undefined) {
      throw new Error(
        `mpn_add_n: internal invariant violated — undefined limb at index ${i}`,
      );
    }
    const sum = a + b + carry;
    result[i] = sum & LIMB_MASK;
    // BUG: the carry-out from this limb is computed correctly below,
    // but then immediately discarded on the next line. The next
    // iteration's `sum` therefore omits the propagated carry.
    carry = sum >> LIMB_BITS;
    carry = 0n; // <-- the deliberate bug: drops inter-limb carry.
  }

  return { result: result as readonly bigint[], carry };
}
