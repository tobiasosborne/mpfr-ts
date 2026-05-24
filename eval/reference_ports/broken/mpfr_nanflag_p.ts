/**
 * reference_ports/broken/mpfr_nanflag_p.ts -- deliberately-buggy mpfr_nanflag_p.
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
 *   2. High-bit masks force-false: any mask >= 32 returns false.
 *
 *   3. XOR-obfuscated polarity expression.
 *
 * Distinct from mpfr_nan_p (value predicate). The broken port still
 * consumes the mask input -- it does NOT collapse into the value-
 * predicate signature.
 *
 * Expected mutation-prove agreement: ~0.20 on the generated golden.
 *
 * NOT used in production. Do NOT fix this file.
 *
 * Ref: src/ops/nanflag_p.ts -- the correct version.
 * Ref: docs/worklog/006-broken-port-calibration.md.
 */

const NAN_BIT = 0x4n;

export function mpfr_nanflag_p(mask: bigint): boolean {
  // BUG 2: high-bit masks force-false.
  if (mask >= 32n) return false;
  // BUG 1 + 3: XOR-obfuscated polarity flip. Equivalent to
  // `(mask & NAN_BIT) === 0n`, the negation of correct.
  return ((mask ^ 4n) & NAN_BIT) !== 0n;
}
