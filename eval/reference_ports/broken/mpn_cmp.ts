/**
 * reference_ports/broken/mpn_cmp.ts — deliberately-buggy mpn_cmp.
 *
 * Used to verify the golden master's discriminating power per
 * docs/PILOT_PLAN.md Step 8 (mutation-prove). A "broken" reference port
 * is the executable assertion that the goldens distinguish correct from
 * subtly-incorrect behaviour: if this file scores composite > 0.6 on the
 * mpn_cmp golden, the golden is too weak and the function is NOT
 * Pilot-passed.
 *
 * **Deliberately broken: iterates LSB-first instead of MSB-first.**
 * The wrong-direction port returns the sign of the LOWEST differing
 * limb, not the highest. This matches a common naive-port mistake:
 * cargo-culting the iteration direction from mpn_add_n / mpn_sub_n
 * (which legitimately walk LSB-first for carry/borrow propagation)
 * without noticing that comparison has different semantics — the
 * MOST-significant differing limb is what decides.
 *
 * Discriminating behaviour
 * ------------------------
 *
 * The broken port and the correct port AGREE on:
 *
 *   - every case where the two arrays are equal (no differing limb,
 *     both ports fall through to return 0). All 8 equality cases
 *     across edge / adversarial — happy contributes ~5 more (every
 *     n=1..5 has one forced-equal case via `rep % 3 === 0`).
 *   - every case where ALL differing limbs disagree in the same
 *     direction (s1[i] > s2[i] for every differing i, or s1[i] < s2[i]
 *     for every differing i). Most random-fuzz cases fall here because
 *     the dominant-limb random distribution is biased that way; both
 *     ports return the sign of any differing limb correctly.
 *   - every case with exactly one differing limb (regardless of its
 *     index). LSB-first and MSB-first both pick the unique
 *     differing limb and return the same sign.
 *
 * The broken port DISAGREES with the correct port on:
 *
 *   - "cross-direction" cases: at least one limb has s1[i] > s2[i] AND
 *     at least one (different) limb has s1[i] < s2[i]. The correct port
 *     reports the sign at the HIGHEST differing limb; the broken port
 *     reports the sign at the LOWEST. The edge cases (11), (12), (13),
 *     (14), (16), (19), (20), (21), (22), (28), (29), (30), (31), (32)
 *     are constructed to exercise this divergence, as are adversarial
 *     (6), (7), (16), (17). A fraction of fuzz cases also disagree
 *     organically.
 *
 * Pilot brief calibration
 * -----------------------
 *
 * Per the prep brief: "aim for composite ≤ 0.6 with gap ≥ 0.4".
 * LSB-first cmp coincidentally agrees with MSB-first on a substantial
 * fraction of random-fuzz cases (any time the lowest differing limb
 * happens to be the highest differing limb, which is common when only
 * the top one or two limbs of each random operand differ — for n=1
 * EVERY case is single-differing-limb and the ports trivially agree).
 * So the broken-port composite cannot reach the ≤ 0.5 of the add_n /
 * sub_n drop-carry variant; the looser ≤ 0.6 target here reflects
 * that coincidence rate.
 *
 * NOT used in production. NOT imported from `src/`. Do NOT fix this
 * file — the bug is the point.
 *
 * Ref: docs/PILOT_PLAN.md Step 8 — mutation-prove acceptance.
 * Ref: CLAUDE.md PIL.3 — "perturb the reference port, confirm the
 *   composite drops below 0.95".
 */

export function mpn_cmp(
  s1: readonly bigint[],
  s2: readonly bigint[],
  n: number,
): number {
  if (!Number.isInteger(n) || n < 0) {
    throw new Error(`mpn_cmp: n must be a non-negative integer, got ${n}`);
  }
  if (s1.length < n) {
    throw new Error(
      `mpn_cmp: s1 too short: need ${n} limbs, got ${s1.length}`,
    );
  }
  if (s2.length < n) {
    throw new Error(
      `mpn_cmp: s2 too short: need ${n} limbs, got ${s2.length}`,
    );
  }

  // BUG: iterate LSB-first (i = 0 upward) instead of MSB-first. This
  // walks the limbs in carry-propagation order, returning the sign of
  // the LOWEST differing limb. For "cross-direction" inputs where the
  // top differing limb disagrees in sign with the bottom differing
  // limb, this returns the opposite of the correct answer.
  for (let i = 0; i < n; i++) {
    const a = s1[i];
    const b = s2[i];
    if (a === undefined || b === undefined) {
      throw new Error(
        `mpn_cmp: internal invariant violated — undefined limb at index ${i}`,
      );
    }
    if (a > b) return 1;
    if (a < b) return -1;
  }
  return 0;
}
