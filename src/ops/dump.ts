/**
 * ops/dump.ts -- pure-TS port of MPFR's `mpfr_dump`.
 *
 * Convenience wrapper around `mpfr_fdump` that dumps to stdout. The C
 * body (mpfr/src/dump.c L127-L131) is literally
 *
 *   void mpfr_dump(mpfr_srcptr u) { mpfr_fdump(stdout, u); }
 *
 * The TS port likewise delegates to the shipped `mpfr_fdump`. See
 * `src/ops/fdump.ts` for the divergence rationale (no FILE* analogue in
 * TS; the dump is returned as a string). `mpfr_dump` therefore returns
 * the same string that `mpfr_fdump` would have written to `stdout`; the
 * caller can `console.log` it, write to a file, or pipe it as they wish.
 *
 * Refs
 * ----
 *
 *   - mpfr/src/dump.c L127-L131 -- C reference body.
 *   - src/ops/fdump.ts -- the shipped delegate.
 *   - eval/reference_ports/correct/mpfr_dump.ts -- mutation-prove ref.
 *   - eval/functions/mpfr_dump/spec.json -- contract.
 */

import type { MPFR } from '../core.ts';
import { mpfr_fdump } from './fdump.ts';

/**
 * Render an MPFR value as the canonical MPFR debug-dump string. Equivalent
 * to `mpfr_fdump(x)`; see that function for the output format.
 *
 * @mpfrName mpfr_dump
 *
 * @divergence The C signature is `void mpfr_dump(mpfr_srcptr u)`; it
 *   writes to `stdout` and returns nothing. The TS port returns the
 *   dump string instead (same as `mpfr_fdump`), so callers can decide
 *   where to send it without coupling this op to a runtime-specific
 *   stream API.
 *
 * @param x The value to dump.
 * @returns The dump string, terminated by `'\n'`.
 */
export function mpfr_dump(x: MPFR): string {
  return mpfr_fdump(x);
}
