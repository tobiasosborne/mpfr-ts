/**
 * reference_ports/broken/mpn_lshift.ts — deliberately-buggy mpn_lshift.
 *
 * Used to verify the golden master's discriminating power per
 * docs/PILOT_PLAN.md Step 8 (mutation-prove). A "broken" reference port
 * is the executable assertion that the goldens distinguish correct from
 * subtly-incorrect behaviour: if this file scores composite > 0.5 on the
 * mpn_lshift golden, the golden is too weak and the function is NOT
 * Pilot-passed.
 *
 * **Deliberately broken: the `& LIMB_MASK` on `result[i]` is omitted.**
 * Bigint does NOT truncate on left-shift the way C's `mp_limb_t` does;
 * without the mask, the bits that should overflow into the next limb
 * instead remain in this limb. The result is:
 *
 *   - For count=1, s[i] with bit 63 set: result[i] retains a bit at
 *     position 64 — i.e. result[i] >= 2^64 — and the golden's
 *     decimal-string round-trip fails the strict-bigint comparison.
 *   - For larger count, the leakage is even worse: up to `count`
 *     extra high bits stay in result[i] when they should have been
 *     mask-truncated into the bottom `count` bits of result[i+1] via
 *     the `prev` register's contribution.
 *   - The `out` value remains computed correctly (it's `prev >>
 *     (64-count)`, no mask involved), so cases where `result` is all
 *     zero anyway (s = {0, 0, ..., 0}) still pass — every all-zero
 *     input is a "fall through" for both ports.
 *
 * This bug is structurally identical to one of the most common naive-
 * port mistakes: agents who translate "C truncates 64-bit shifts
 * automatically" without realising the bigint world has unbounded
 * precision and requires an explicit mask. Exercising it through the
 * goldens is a direct test of the alignment / cross-limb spill
 * coverage.
 *
 * Discriminating behaviour
 * ------------------------
 *
 * The broken port and the correct port AGREE on:
 *
 *   - every case where `s` is all zero (no bits to leak). Edges (1),
 *     (2), (10), (11), (22), (30) and one fuzz case ~per n category.
 *   - every case where the highest `count` bits of EVERY limb in `s`
 *     are zero. For random fuzz with random count this is roughly
 *     `(1 - count/64)^n` — vanishingly small for the n=8/16/32 buckets.
 *
 * The broken port DISAGREES with the correct port on:
 *
 *   - every case where ANY limb in `s[0..n-1]` has at least one of its
 *     top `count` bits set: the missing mask leaks those bits into
 *     result[i] above the 2^64 boundary. Most edge/adversarial cases
 *     and the bulk of fuzz cases fall here.
 *
 * Pilot brief calibration
 * -----------------------
 *
 * Per the prep brief: "aim for composite ≤ 0.5 with gap ≥ 0.45".
 * The missing-mask bug is structurally similar to the dropped-carry
 * bug in mpn_add_n broken (which scores well below 0.5), so the same
 * target applies here. Most goldens have at least one high bit set in
 * at least one limb, so the broken port fails the vast majority of
 * non-trivial cases.
 *
 * NOT used in production. NOT imported from `src/`. Do NOT fix this
 * file — the bug is the point.
 *
 * Ref: docs/PILOT_PLAN.md Step 8 — mutation-prove acceptance.
 * Ref: CLAUDE.md PIL.3 — "perturb the reference port, confirm the
 *   composite drops below 0.95".
 */

const LIMB_BITS = 64n;

export interface MpnLshiftResult {
  readonly result: readonly bigint[];
  readonly out: bigint;
}

export function mpn_lshift(
  s: readonly bigint[],
  n: number,
  count: number,
): MpnLshiftResult {
  if (!Number.isInteger(n) || n < 1) {
    throw new Error(`mpn_lshift: n must be a positive integer, got ${n}`);
  }
  if (!Number.isInteger(count) || count < 1 || count > 63) {
    throw new Error(
      `mpn_lshift: count must be an integer in [1, 63], got ${count}`,
    );
  }
  if (s.length < n) {
    throw new Error(
      `mpn_lshift: s too short: need ${n} limbs, got ${s.length}`,
    );
  }

  const countBig = BigInt(count);
  const complBig = LIMB_BITS - countBig;

  const result: bigint[] = new Array<bigint>(n);
  let prev: bigint = 0n;

  for (let i = 0; i < n; i++) {
    const cur = s[i];
    if (cur === undefined) {
      throw new Error(
        `mpn_lshift: internal invariant violated — undefined limb at index ${i}`,
      );
    }
    // BUG: the `& LIMB_MASK` is missing. `cur << countBig` is an
    // unbounded bigint shift; the bits that should have spilled into
    // the next limb stay glued to result[i] above the 2^64 boundary.
    result[i] = (cur << countBig) | (prev >> complBig);
    prev = cur;
  }

  const out = prev >> complBig;

  return { result: result as readonly bigint[], out };
}
