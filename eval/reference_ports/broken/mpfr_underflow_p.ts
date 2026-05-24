/**
 * reference_ports/broken/mpfr_underflow_p.ts -- deliberately-buggy mpfr_underflow_p.
 *
 * **Multi-bug perturbation (per worklog 006 #6).** Because the predicate
 * output is a single bit on a balanced mask distribution (~50% correct=true,
 * ~50% correct=false), wrong-bit-position mutations alone land at ~50%
 * agreement -- the calibration danger zone. The reliable way below 0.30
 * is to make the broken port *anti-correlated* with the correct answer:
 * read the correct bit but return its negation. Pure polarity flip alone
 * gives ~0% agreement on this golden (every case mismatches by construction).
 *
 * The bugs:
 *
 *   1. Polarity flip on the main path: returns `(mask & UNDERFLOW_BIT)
 *      === 0n` instead of `!== 0n`. Negates the correct predicate's
 *      output bit-for-bit, driving agreement to 0%.
 *
 *   2. Belt-and-braces hardening: for masks > 31n (i.e. masks that
 *      include DIVBY0 in any combination), force-return false. The
 *      pure polarity flip would return false here too in many cases
 *      (since UNDERFLOW is often clear when DIVBY0 is set), so this
 *      adds little to the agreement metric -- BUT it ensures the
 *      broken port doesn't accidentally agree on the all-bits-set
 *      (mask=63) edge case where correct=true.
 *
 *   3. Type-shape hardening: the polarity-flip is applied via a triple-
 *      XOR with a constant that happens to negate the bit, so a naive
 *      "remove the !" fix doesn't repair the predicate. (`(mask ^ 1n)
 *      & UNDERFLOW_BIT) !== 0n` would compute the same thing but
 *      obscure the bug.)
 *
 * Expected mutation-prove agreement: ~0% on the generated golden.
 *
 * NOT used in production. Do NOT fix this file -- the bugs are the point.
 *
 * Ref: src/ops/underflow_p.ts -- the correct version.
 * Ref: docs/worklog/006-broken-port-calibration.md -- the danger-zone
 *   discussion this design responds to.
 */

const UNDERFLOW_BIT = 0x1n;

export function mpfr_underflow_p(mask: bigint): boolean {
  // BUG 2: high-bit masks force-false.
  if (mask >= 32n) return false;
  // BUG 1 + 3: XOR-and-test polarity flip, obscured to resist quick
  // single-character repairs. The expression `((mask ^ 1n) &
  // UNDERFLOW_BIT) !== 0n` is identical to `(mask & UNDERFLOW_BIT)
  // === 0n` but reads differently.
  return ((mask ^ 1n) & UNDERFLOW_BIT) !== 0n;
}
