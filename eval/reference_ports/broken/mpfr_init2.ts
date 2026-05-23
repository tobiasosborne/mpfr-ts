/**
 * reference_ports/broken/mpfr_init2.ts — deliberately-buggy mpfr_init2.
 *
 * Used to mutation-prove the golden master per docs/PILOT_PLAN.md Step 8
 * and CLAUDE.md PIL.3 ("perturb the reference port, confirm the
 * composite drops below 0.95"). A "broken" reference port is the
 * executable assertion that the goldens distinguish correct from
 * subtly-incorrect behaviour: if this file scores composite > 0.5 on
 * the mpfr_init2 golden, the golden is too weak and the function is NOT
 * Pilot-passed.
 *
 * **Deliberately broken: the requested precision is discarded.**
 * Specifically, this port returns `posZero(PREC_MIN)` (i.e. `prec=1n`)
 * for every input rather than `posZero(prec)`. Cases where the requested
 * precision is exactly `1n` (`PREC_MIN`) still pass; every other case —
 * the entire `happy` block, most of `edge`, all power-of-two and prime
 * `adversarial` cases, and essentially every `fuzz` case — fails on the
 * `prec mismatch` branch of `compareMpfr` in value_codec.ts.
 *
 * This bug is structurally identical to one of the most common naive-
 * port mistakes when an agent reads `mpfr_init2`'s C source: the C
 * function spends most of its body on the allocation-and-flag-setup
 * dance, and a hasty port may capture the "construct an empty MPFR"
 * pattern without threading the `prec` argument through to the result.
 *
 * NOT used in production. NOT imported from `src/`. Do NOT fix this
 * file — the bug is the point.
 *
 * Ref: docs/PILOT_PLAN.md Step 8 — mutation-prove acceptance.
 * Ref: CLAUDE.md PIL.3 — composite must drop below 0.95 under mutation.
 * Ref: src/ops/init2.ts — the correct version.
 */

import type { MPFR } from '../../../src/core.ts';
import { posZero, PREC_MIN } from '../../../src/core.ts';

export function mpfr_init2(prec: bigint): MPFR {
  // BUG: the `prec` argument is intentionally discarded — we always
  // construct at PREC_MIN regardless of what the caller asked for.
  // A real port must thread `prec` through to `posZero`. See the
  // production version at src/ops/init2.ts.
  void prec;
  return posZero(PREC_MIN);
}
