/**
 * ops/get_default_rounding_mode.ts -- pure-TS port of MPFR's
 * `mpfr_get_default_rounding_mode`.
 *
 * Returns the library-wide default rounding mode. The C body is
 * `return __gmpfr_default_rounding_mode`, where the global is initialised
 * to `MPFR_RNDN` at module load (mpfr/src/set_rnd.c L25).
 *
 * No setter is in this batch (`mpfr_set_default_rounding_mode` is a
 * separate port), so the TS port returns the constant `'RNDN'` -- the
 * locked-schema string form of the default.
 *
 * Algorithm (mpfr/src/set_rnd.c L34-L38):
 *
 *   return __gmpfr_default_rounding_mode
 *
 * Ref: mpfr/src/set_rnd.c L34-L38 -- C reference body.
 * Ref: mpfr/src/set_rnd.c L25     -- MPFR_THREAD_VAR default = MPFR_RNDN.
 * Ref: src/core.ts                -- RoundingMode enum (5 modes, no RNDNA).
 *
 * @divergence Return type: C `mpfr_rnd_t` (enum int) -> TS `RoundingMode`
 *   string per the locked schema. Module-level state semantics are
 *   preserved (each Bun worker gets a fresh init per CLAUDE.md Rule 4),
 *   but without a setter in scope the observable behaviour is the
 *   default-only constant.
 */

import type { RoundingMode } from '../core.ts';

/**
 * Read the library default rounding mode.
 *
 * @mpfrName mpfr_get_default_rounding_mode
 *
 * @returns Always `'RNDN'` in this batch (default; setter not in scope).
 */
export function mpfr_get_default_rounding_mode(): RoundingMode {
  return 'RNDN';
}
