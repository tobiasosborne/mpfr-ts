/*
 * golden_driver.c -- Golden master for MPFR's mpfr_clear_divby0.
 *
 * C signature
 * -----------
 *
 *   void mpfr_clear_divby0(void);
 *
 *   Body: `__gmpfr_flags &= MPFR_FLAGS_ALL ^ MPFR_FLAGS_DIVBY0`.
 *   Ref: mpfr/src/exceptions.c L168-L174.
 *
 * Wire shape
 * ----------
 *
 *   {"tag":"<class>",
 *    "inputs":{"mask":"<dec>"},
 *    "output":"<dec>",
 *    "time_ns":<n>}
 *
 *   - mask: pre-clear flag state in [0, 63]. uint64 emitted via jl_kv_u64.
 *   - output: post-clear flag state as decimal uint64 (jl_output_scalar_u64);
 *     mathematically equal to (mask & ~MPFR_FLAGS_DIVBY0).
 *
 * Driver flow per case:
 *   1. mpfr_clear_flags()
 *   2. mpfr_flags_set(mask)
 *   3. mpfr_clear_divby0()
 *   4. read mpfr_flags_save() -> emit as output
 *
 * Tag distribution (CLAUDE.md Rule 7 minimums)
 *
 *   happy        :  20
 *   edge         :  30
 *   adversarial  :  12
 *   fuzz         :  50
 *   mined        :   5
 *
 * Build via eval/golden_master/build.sh.
 *
 * Ref: mpfr/src/exceptions.c -- C reference.
 * Ref: src/internal/mpfr/flags.ts -- the TS-side mirror.
 */
#include "common.h"

#include <assert.h>
#include <inttypes.h>

#define BIT_TO_CLEAR ((uint64_t)MPFR_FLAGS_DIVBY0)

static inline void emit_case(FILE *out, const char *tag, uint64_t mask) {
    assert(mask <= MPFR_FLAGS_ALL);
    mpfr_clear_flags();
    mpfr_flags_set((mpfr_flags_t)mask);
    const uint64_t t0 = now_ns();
    mpfr_clear_divby0();
    const uint64_t result = (uint64_t)mpfr_flags_save();
    const uint64_t elapsed = now_ns() - t0;
    jl_begin(out, tag);
    jl_kv_u64(out, 1, "mask", mask);
    jl_end_inputs(out);
    jl_output_scalar_u64(out, result);
    jl_finish(out, elapsed);
}

int main(void) {
    FILE *out = stdout;

    /* happy: 20 -- single-bit cases, our bit and others, plus all/none. */
    emit_case(out, "happy", 0);
    emit_case(out, "happy", BIT_TO_CLEAR);
    emit_case(out, "happy", MPFR_FLAGS_UNDERFLOW);
    emit_case(out, "happy", MPFR_FLAGS_OVERFLOW);
    emit_case(out, "happy", MPFR_FLAGS_NAN);
    emit_case(out, "happy", MPFR_FLAGS_INEXACT);
    emit_case(out, "happy", MPFR_FLAGS_ERANGE);
    emit_case(out, "happy", MPFR_FLAGS_DIVBY0);
    emit_case(out, "happy", MPFR_FLAGS_ALL);
    emit_case(out, "happy", MPFR_FLAGS_ALL ^ BIT_TO_CLEAR);
    emit_case(out, "happy", BIT_TO_CLEAR | MPFR_FLAGS_OVERFLOW);
    emit_case(out, "happy", BIT_TO_CLEAR | MPFR_FLAGS_NAN);
    emit_case(out, "happy", BIT_TO_CLEAR | MPFR_FLAGS_INEXACT);
    emit_case(out, "happy", BIT_TO_CLEAR | MPFR_FLAGS_ERANGE);
    emit_case(out, "happy", BIT_TO_CLEAR | MPFR_FLAGS_UNDERFLOW);
    emit_case(out, "happy", MPFR_FLAGS_OVERFLOW | MPFR_FLAGS_NAN);
    emit_case(out, "happy", MPFR_FLAGS_OVERFLOW | MPFR_FLAGS_INEXACT);
    emit_case(out, "happy", MPFR_FLAGS_NAN | MPFR_FLAGS_INEXACT);
    emit_case(out, "happy", MPFR_FLAGS_INEXACT | MPFR_FLAGS_ERANGE);
    emit_case(out, "happy", MPFR_FLAGS_UNDERFLOW | MPFR_FLAGS_ERANGE);

    /* edge: 30 -- exhaustive single-bit isolated and inverted, plus all C(6,3) triples. */
    emit_case(out, "edge", MPFR_FLAGS_UNDERFLOW);
    emit_case(out, "edge", MPFR_FLAGS_OVERFLOW);
    emit_case(out, "edge", MPFR_FLAGS_NAN);
    emit_case(out, "edge", MPFR_FLAGS_INEXACT);
    emit_case(out, "edge", MPFR_FLAGS_ERANGE);
    emit_case(out, "edge", MPFR_FLAGS_DIVBY0);
    emit_case(out, "edge", MPFR_FLAGS_ALL ^ MPFR_FLAGS_UNDERFLOW);
    emit_case(out, "edge", MPFR_FLAGS_ALL ^ MPFR_FLAGS_OVERFLOW);
    emit_case(out, "edge", MPFR_FLAGS_ALL ^ MPFR_FLAGS_NAN);
    emit_case(out, "edge", MPFR_FLAGS_ALL ^ MPFR_FLAGS_INEXACT);
    emit_case(out, "edge", MPFR_FLAGS_ALL ^ MPFR_FLAGS_ERANGE);
    emit_case(out, "edge", MPFR_FLAGS_ALL ^ MPFR_FLAGS_DIVBY0);
    emit_case(out, "edge", MPFR_FLAGS_UNDERFLOW | MPFR_FLAGS_OVERFLOW | MPFR_FLAGS_NAN);
    emit_case(out, "edge", MPFR_FLAGS_UNDERFLOW | MPFR_FLAGS_OVERFLOW | MPFR_FLAGS_INEXACT);
    emit_case(out, "edge", MPFR_FLAGS_UNDERFLOW | MPFR_FLAGS_OVERFLOW | MPFR_FLAGS_ERANGE);
    emit_case(out, "edge", MPFR_FLAGS_UNDERFLOW | MPFR_FLAGS_OVERFLOW | MPFR_FLAGS_DIVBY0);
    emit_case(out, "edge", MPFR_FLAGS_UNDERFLOW | MPFR_FLAGS_NAN | MPFR_FLAGS_INEXACT);
    emit_case(out, "edge", MPFR_FLAGS_UNDERFLOW | MPFR_FLAGS_NAN | MPFR_FLAGS_ERANGE);
    emit_case(out, "edge", MPFR_FLAGS_UNDERFLOW | MPFR_FLAGS_NAN | MPFR_FLAGS_DIVBY0);
    emit_case(out, "edge", MPFR_FLAGS_UNDERFLOW | MPFR_FLAGS_INEXACT | MPFR_FLAGS_ERANGE);
    emit_case(out, "edge", MPFR_FLAGS_UNDERFLOW | MPFR_FLAGS_INEXACT | MPFR_FLAGS_DIVBY0);
    emit_case(out, "edge", MPFR_FLAGS_UNDERFLOW | MPFR_FLAGS_ERANGE | MPFR_FLAGS_DIVBY0);
    emit_case(out, "edge", MPFR_FLAGS_OVERFLOW | MPFR_FLAGS_NAN | MPFR_FLAGS_INEXACT);
    emit_case(out, "edge", MPFR_FLAGS_OVERFLOW | MPFR_FLAGS_NAN | MPFR_FLAGS_ERANGE);
    emit_case(out, "edge", MPFR_FLAGS_OVERFLOW | MPFR_FLAGS_NAN | MPFR_FLAGS_DIVBY0);
    emit_case(out, "edge", MPFR_FLAGS_OVERFLOW | MPFR_FLAGS_INEXACT | MPFR_FLAGS_ERANGE);
    emit_case(out, "edge", MPFR_FLAGS_OVERFLOW | MPFR_FLAGS_INEXACT | MPFR_FLAGS_DIVBY0);
    emit_case(out, "edge", MPFR_FLAGS_OVERFLOW | MPFR_FLAGS_ERANGE | MPFR_FLAGS_DIVBY0);
    emit_case(out, "edge", MPFR_FLAGS_NAN | MPFR_FLAGS_INEXACT | MPFR_FLAGS_ERANGE);
    emit_case(out, "edge", MPFR_FLAGS_NAN | MPFR_FLAGS_INEXACT | MPFR_FLAGS_DIVBY0);

    /* adversarial: 12 -- idempotence, repeated clears, out-of-domain mask
     * (high bits stripped on entry per the mpfr_flags_set contract). */
    emit_case(out, "adversarial", BIT_TO_CLEAR);
    emit_case(out, "adversarial", BIT_TO_CLEAR | MPFR_FLAGS_ALL);
    emit_case(out, "adversarial", BIT_TO_CLEAR);  /* twice to check stability */
    emit_case(out, "adversarial", 0);             /* clear-from-empty is no-op */
    emit_case(out, "adversarial", MPFR_FLAGS_ALL);
    emit_case(out, "adversarial", MPFR_FLAGS_ALL ^ BIT_TO_CLEAR);
    emit_case(out, "adversarial", MPFR_FLAGS_NAN | BIT_TO_CLEAR);
    emit_case(out, "adversarial", MPFR_FLAGS_NAN | MPFR_FLAGS_OVERFLOW);
    emit_case(out, "adversarial", MPFR_FLAGS_INEXACT | MPFR_FLAGS_ERANGE);
    emit_case(out, "adversarial", MPFR_FLAGS_UNDERFLOW | MPFR_FLAGS_INEXACT | BIT_TO_CLEAR);
    emit_case(out, "adversarial", MPFR_FLAGS_OVERFLOW | MPFR_FLAGS_NAN | MPFR_FLAGS_INEXACT | MPFR_FLAGS_ERANGE);
    emit_case(out, "adversarial", MPFR_FLAGS_ALL ^ MPFR_FLAGS_OVERFLOW);

    /* fuzz: 50 -- PRNG-driven mask in [0, MPFR_FLAGS_ALL]. */
    {
        xs64_t rng;
        /* Hex seed using only 0-9 A-F per HANDOFF gotcha #3. */
        xs64_seed(&rng, 0xC1EA0DDBA110D0CEULL);
        for (int rep = 0; rep < 50; ++rep) {
            const uint64_t mask = xs64_below(&rng, (uint64_t)MPFR_FLAGS_ALL + 1);
            emit_case(out, "fuzz", mask);
        }
    }

    /* mined: 5 -- from mpfr/tests/texceptions.c L327-L351 clear-divby0
     * specific cases. The test sets each flag in turn and then clears
     * them. We mine the "set X then clear divby0" and "set all then
     * clear divby0" patterns. */
    emit_case(out, "mined", BIT_TO_CLEAR);                              /* clear-the-only-set-bit */
    emit_case(out, "mined", BIT_TO_CLEAR | MPFR_FLAGS_OVERFLOW);        /* preserve OVERFLOW */
    emit_case(out, "mined", BIT_TO_CLEAR | MPFR_FLAGS_NAN);             /* preserve NAN */
    emit_case(out, "mined", MPFR_FLAGS_ALL);                            /* clear from full */
    emit_case(out, "mined", 0);                                         /* clear from empty (no-op) */

    return 0;
}
