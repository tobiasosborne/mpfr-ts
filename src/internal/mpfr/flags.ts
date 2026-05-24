/**
 * internal/mpfr/flags.ts — pure-TS port of MPFR's `__gmpfr_flags` register.
 *
 * The only documented global state in MPFR. A single 6-bit register that
 * exception-raising ops OR-into and that the `_p` predicates + the
 * `flags_save`/`flags_clear`/`flags_set` family read or modify. Mirrored
 * here as a module-level `bigint` so each Bun worker (CLAUDE.md Rule 4)
 * gets a fresh, zeroed register at module init — exactly the semantics
 * the per-case worker isolation gives us "for free".
 *
 * C reference
 * -----------
 *
 *   mpfr/src/exceptions.c L37–L40 — `mpfr_flags_t __gmpfr_flags;`
 *   mpfr/src/exceptions.c L104–L107 — `mpfr_flags_clear`
 *   mpfr/src/exceptions.c L112–L115 — `mpfr_flags_set`
 *   mpfr/src/exceptions.c L128–L131 — `mpfr_flags_save`
 *   mpfr/src/exceptions.c L147–L150 — `mpfr_clear_flags`
 *   /usr/include/mpfr.h L77–L88     — MPFR_FLAGS_* bit constants
 *
 * I/O contract
 * ------------
 *
 *   getFlags()                : bigint        — read the register
 *   setFlags(bits)            : void          — `register |= bits`
 *   clearFlags(bits = ALL)    : void          — `register &= (ALL ^ bits)`
 *
 * Invariants
 * ----------
 *
 *   1. The register is always in [0n, MPFR_FLAGS_ALL]. Bits outside the
 *      defined six are masked off on entry to `setFlags` so a caller
 *      passing an out-of-range bigint cannot corrupt the register.
 *   2. The register starts at 0n at module init. Per-worker isolation
 *      (Rule 4) means each test case sees a clean register.
 *   3. No mutation occurs outside this module; the `_p` predicates and
 *      `flags_*` ops route through these primitives, never reach in.
 */

/** Bit 0 — set when an op underflows the current exponent range. */
export const MPFR_FLAGS_UNDERFLOW = 1n;
/** Bit 1 — set when an op overflows the current exponent range. */
export const MPFR_FLAGS_OVERFLOW = 2n;
/** Bit 2 — set when an op produces NaN from non-NaN inputs. */
export const MPFR_FLAGS_NAN = 4n;
/** Bit 3 — set when an op rounds (result differs from the exact value). */
export const MPFR_FLAGS_INEXACT = 8n;
/** Bit 4 — set when a conversion is out of range (e.g. `get_si(+Inf)`). */
export const MPFR_FLAGS_ERANGE = 16n;
/** Bit 5 — set on division by zero. */
export const MPFR_FLAGS_DIVBY0 = 32n;
/** Union of all six defined bits. `0b111111n = 63n`. */
export const MPFR_FLAGS_ALL =
  MPFR_FLAGS_UNDERFLOW |
  MPFR_FLAGS_OVERFLOW |
  MPFR_FLAGS_NAN |
  MPFR_FLAGS_INEXACT |
  MPFR_FLAGS_ERANGE |
  MPFR_FLAGS_DIVBY0;

let _flags: bigint = 0n;

/**
 * Read the current flag register.
 *
 * @returns the bitmask of currently-set flags, in [0n, {@link MPFR_FLAGS_ALL}].
 *
 * Ref: mpfr/src/exceptions.c L128–L131 — `mpfr_flags_save`.
 */
export function getFlags(): bigint {
  return _flags;
}

/**
 * OR `bits` into the flag register. Bits outside {@link MPFR_FLAGS_ALL}
 * are silently masked off (matches the C `mpfr_flags_t` being a typedef
 * over `unsigned int` that callers always restrict to the defined six).
 *
 * Ref: mpfr/src/exceptions.c L112–L115 — `mpfr_flags_set`.
 */
export function setFlags(bits: bigint): void {
  _flags |= bits & MPFR_FLAGS_ALL;
}

/**
 * Clear the bits named in `bits` from the register. Bits outside
 * {@link MPFR_FLAGS_ALL} are ignored. With the default argument this is
 * the equivalent of C's `mpfr_clear_flags` — wipe the whole register.
 *
 * Ref: mpfr/src/exceptions.c L104–L107 — `mpfr_flags_clear`.
 * Ref: mpfr/src/exceptions.c L147–L150 — `mpfr_clear_flags` (= `clearFlags(ALL)`).
 */
export function clearFlags(bits: bigint = MPFR_FLAGS_ALL): void {
  _flags &= MPFR_FLAGS_ALL ^ (bits & MPFR_FLAGS_ALL);
}
