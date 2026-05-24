/**
 * reference_ports/broken/mpfr_overflow_p.ts -- deliberately-buggy mpfr_overflow_p.
 *
 * **Multi-bug perturbation (per worklog 006 #6).** Single-bit predicate
 * outputs on a balanced mask distribution land at ~50% agreement under
 * naive perturbations -- the calibration danger zone. The reliable way
 * below 0.30 is to make the broken port *anti-correlated* with the
 * correct answer.
 *
 * The bugs:
 *
 *   1. Polarity flip on the main path: returns the negation of the
 *      correct predicate. Drives agreement to ~0%.
 *
 *   2. High-bit masks force-false: any mask >= 32 returns false.
 *
 *   3. XOR-obfuscated polarity expression to resist single-character repairs.
 *
 * Expected mutation-prove agreement: ~0.20 on the generated golden.
 *
 * NOT used in production. Do NOT fix this file.
 *
 * Ref: src/ops/overflow_p.ts -- the correct version.
 * Ref: docs/worklog/006-broken-port-calibration.md.
 */

const OVERFLOW_BIT = 0x2n;

export function mpfr_overflow_p(mask: bigint): boolean {
  // BUG 2: high-bit masks force-false.
  if (mask >= 32n) return false;
  // BUG 1 + 3: XOR-obfuscated polarity flip. Equivalent to
  // `(mask & OVERFLOW_BIT) === 0n`, the negation of correct.
  return ((mask ^ 2n) & OVERFLOW_BIT) !== 0n;
}
