/**
 * reference_ports/broken/mpfr_divby0_p.ts -- deliberately-buggy mpfr_divby0_p.
 *
 * **Multi-bug perturbation (per worklog 006 #6).** Single-bit predicate
 * outputs on a balanced mask distribution land at ~50% agreement under
 * naive perturbations -- the calibration danger zone. The reliable way
 * below 0.30 is to make the broken port *anti-correlated* with the
 * correct answer.
 *
 * The bugs:
 *
 *   1. Polarity flip: returns the negation of the correct predicate.
 *      Drives agreement to ~0%.
 *
 *   2. Low-bit masks force-true: any mask < 16 returns true. Combined
 *      with the polarity flip, this targets the half of cases where
 *      DIVBY0 is NOT set (correct=false), and over-flips them.
 *
 *   3. XOR-obfuscated polarity expression on the high-bit path.
 *
 * Note: DIVBY0's bit (0x20) is the highest in MPFR_FLAGS_*, so the
 * complement of the predicate's true-set is mostly low-bit masks.
 * The "low-bit force-true" override exploits that asymmetry.
 *
 * Expected mutation-prove agreement: ~0.20 on the generated golden.
 *
 * NOT used in production. Do NOT fix this file.
 *
 * Ref: src/ops/divby0_p.ts -- the correct version.
 * Ref: docs/worklog/006-broken-port-calibration.md.
 */

const DIVBY0_BIT = 0x20n;

export function mpfr_divby0_p(mask: bigint): boolean {
  // BUG 2: low-bit masks force-true.
  if (mask < 16n) return true;
  // BUG 1 + 3: XOR-obfuscated polarity flip on the high-bit path.
  // Equivalent to `(mask & DIVBY0_BIT) === 0n`, the negation of correct.
  return ((mask ^ 32n) & DIVBY0_BIT) !== 0n;
}
