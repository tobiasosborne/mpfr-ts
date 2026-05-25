/**
 * ops/get_cputime.ts -- pure-TS port of MPFR's `mpfr_get_cputime`.
 *
 * The C function (mpfr/src/logging.c L114-L132) returns user-mode CPU
 * milliseconds via getrusage (POSIX) or clock() (ANSI). It exists for
 * MPFR_USE_LOGGING-conditional logging instrumentation only -- no other
 * MPFR function depends on it.
 *
 * Algorithm (mpfr/src/logging.c L114-L132): getrusage then return
 *   `ru.ru_utime.tv_sec*1000 + ru.ru_utime.tv_usec/1000`.
 *
 * Ref: mpfr/src/logging.c L114-L132 -- C reference body.
 * Ref: mpfr/src/logging.c L99-L112  -- MPFR_USE_LOGGING-conditional note.
 *
 * @divergence Return value: C returns elapsed user-mode CPU ms; TS returns
 *   `0` unconditionally. CPU time is not portably observable from pure-TS
 *   without `node:process` or a Bun-specific API, both forbidden in `src/`
 *   per CLAUDE.md Rule 12. The function is for libmpfr logging only -- no
 *   downstream port depends on the value -- so returning `0` is safe and
 *   matches the golden's hardcoded contract.
 */

import type { MPFR as _MPFR } from '../core.ts';

/**
 * User-mode CPU milliseconds since process start.
 *
 * @mpfrName mpfr_get_cputime
 *
 * @returns Always `0` in the pure-TS port. See `@divergence` above.
 */
export function mpfr_get_cputime(): number {
  return 0;
}
