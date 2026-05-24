/**
 * ops/print_rnd_mode.ts — pure-TS port of MPFR's `mpfr_print_rnd_mode`.
 *
 * Surface-class (misc) function. Converts a locked-schema `RoundingMode`
 * string enum value to its canonical C-side name, e.g. `'RNDN'` → `"MPFR_RNDN"`.
 *
 * C signature:
 *   const char *mpfr_print_rnd_mode(mpfr_rnd_t rnd_mode)
 *
 * TS signature (this port):
 *   mpfr_print_rnd_mode(rnd: RoundingMode) -> { name: string }
 *
 * The object-wrapper return shape (rather than a bare string) is required by
 * the harness's value-codec: `decodeExpectedOutput` cannot parse opaque scalar
 * strings, so wrapping in `{name: ...}` routes comparison through the 'object'
 * branch where field-level string equality works.
 *
 * Ref: mpfr/src/print_rnd_mode.c L27–L50 — 5-way (plus RNDF) switch;
 *   returns NULL for unrecognised values.
 * Ref: src/core.ts L137–L151 — locked RoundingMode enum (5 modes; RNDF
 *   deliberately omitted as unsupported in this port).
 * Ref: src/core.ts L178–L208 — MPFRError class; 'EROUND' for unknown modes.
 * Ref: eval/harness/value_codec.ts L274 — object-wrapper workaround.
 *
 * Divergence from C: C returns NULL on unknown input; TS throws
 * MPFRError('EROUND', ...) because the locked-schema RoundingMode type
 * covers exactly 5 modes and any other string is malformed input.
 */

import type { RoundingMode } from "../core.ts";
import { MPFRError } from "../core.ts";

/**
 * Convert a `RoundingMode` to its canonical MPFR C-side name string.
 *
 * @mpfrName mpfr_print_rnd_mode
 *
 * @param rnd - One of the five locked-schema rounding modes.
 * @returns `{ name: "MPFR_RNDN" | "MPFR_RNDZ" | "MPFR_RNDU" | "MPFR_RNDD" | "MPFR_RNDA" }`
 * @throws {MPFRError} `EROUND` if `rnd` is not a recognised `RoundingMode`.
 *
 * Ref: mpfr/src/print_rnd_mode.c L27–L50 — the complete C body is a
 *   switch over `mpfr_rnd_t` with one case per mode.
 */
export function mpfr_print_rnd_mode(rnd: RoundingMode): { name: string } {
  // Ref: mpfr/src/print_rnd_mode.c L27–L50 — direct 5-way dispatch.
  // MPFR_RNDF is deliberately excluded: the locked-schema RoundingMode type
  // does not include 'RNDF' (src/core.ts L137–L151), so any non-RoundingMode
  // input is malformed and must throw rather than return a name.
  switch (rnd) {
    case 'RNDN':
      return { name: 'MPFR_RNDN' };
    case 'RNDZ':
      return { name: 'MPFR_RNDZ' };
    case 'RNDU':
      return { name: 'MPFR_RNDU' };
    case 'RNDD':
      return { name: 'MPFR_RNDD' };
    case 'RNDA':
      return { name: 'MPFR_RNDA' };
    default:
      // C returns NULL for unknown rnd_mode; TS throws because unknown input
      // is a schema violation — the locked RoundingMode type has exactly 5
      // valid values and any other string is a caller error.
      throw new MPFRError(
        'EROUND',
        `mpfr_print_rnd_mode: unknown rounding mode: ${String(rnd)}`,
      );
  }
}
