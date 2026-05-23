/**
 * reference_ports/broken/mpfr_set_d.ts — deliberately-buggy mpfr_set_d.
 *
 * Used to mutation-prove the golden master per docs/PILOT_PLAN.md Step 8
 * and CLAUDE.md PIL.3 ("perturb the reference port, confirm the
 * composite drops below 0.95"). This file is the executable assertion
 * that the golden distinguishes correct from subtly-incorrect behaviour:
 * if this port scores composite > 0.5 on the mpfr_set_d golden, the
 * golden is too weak and the function is NOT Pilot-passed.
 *
 * **Deliberately broken: every input collapses to `posZero(prec)` with
 * ternary 0.**
 *
 * The bug is total — every non-zero finite input produces the wrong
 * value (a zero instead of a normal), every infinity produces the wrong
 * kind, every signed -0 produces the wrong sign. NaN still produces
 * NAN_VALUE because we route through `posZero` whose result has
 * `prec === <given>` (not 0), but the NaN cases in the golden have
 * `prec=0` on the wire — so even the NaN cases fail on the prec
 * mismatch (NaN expected → prec=0n, got → prec=<input>).
 *
 * The intent: the broken port is a "zero everything" stub, structurally
 * close to a naive first cut where the agent wires up the function
 * skeleton (signature, schema imports) but forgets to actually
 * implement the conversion. The golden must reject this.
 *
 * NOT used in production. NOT imported from `src/`. Do NOT fix this
 * file — the bug is the point.
 *
 * Ref: docs/PILOT_PLAN.md Step 8 — mutation-prove acceptance.
 * Ref: CLAUDE.md PIL.3 — composite must drop below 0.95 under mutation.
 * Ref: src/ops/set_d.ts — the correct version.
 */

import type { Result, RoundingMode } from '../../../src/core.ts';
import { posZero } from '../../../src/core.ts';

export function mpfr_set_d(d: number, prec: bigint, rnd: RoundingMode): Result {
  // BUG: every input collapses to +0 at the requested precision with
  // ternary 0. The real port would dispatch on NaN / Inf / signed zero
  // / finite normals and either pad or round the 53-bit mantissa. See
  // src/ops/set_d.ts for the correct algorithm.
  void d;
  void rnd;
  return { value: posZero(prec), ternary: 0 };
}
