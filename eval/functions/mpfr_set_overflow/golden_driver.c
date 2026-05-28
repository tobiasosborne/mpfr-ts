/*
 * golden_driver.c -- Golden master for MPFR's mpfr_set_overflow.
 *
 * C signature
 * -----------
 *
 *   void mpfr_set_overflow(void);
 *
 *   Body: `__gmpfr_flags |= MPFR_FLAGS_OVERFLOW`.
 *   Ref: mpfr/src/exceptions.c L208-L214.
 *
 * This is the OR-SET mirror of mpfr_clear_divby0 (which AND-clears).
 *
 * Wire shape
 * ----------
 *
 *   {"tag":"<class>",
 *    "inputs":{"mask":"<dec>"},
 *    "output":"<dec>",
 *    "time_ns":<n>}
 *
 *   - mask: pre-set flag state in [0, 63]. uint64 emitted via jl_kv_u64.
 *   - output: post-set flag state as decimal uint64 (jl_output_scalar_u64);
 *     mathematically equal to (mask | MPFR_FLAGS_OVERFLOW).
 *
 * Driver flow per case:
 *   1. mpfr_clear_flags()
 *   2. mpfr_flags_set(mask)
 *   3. mpfr_set_overflow()
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
 * Ref: mpfr/src/exceptions.c L208-L214 -- C reference.
 * Ref: src/internal/mpfr/flags.ts -- the TS-side mirror.
 */
#include "common.h"

#include <assert.h>
#include <inttypes.h>

#define BIT_TO_SET ((uint64_t)MPFR_FLAGS_OVERFLOW)

static inline void emit_case(FILE *out, const char *tag, uint64_t mask) {
    assert(mask <= MPFR_FLAGS_ALL);
    mpfr_clear_flags();
    mpfr_flags_set((mpfr_flags_t)mask);
    const uint64_t t0 = now_ns();
    mpfr_set_overflow();
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
    emit_case(out, "happy", BIT_TO_SET);
    emit_case(out, "happy", MPFR_FLAGS_UNDERFLOW);
    emit_case(out, "happy", MPFR_FLAGS_OVERFLOW);
    emit_case(out, "happy", MPFR_FLAGS_NAN);
    emit_case(out, "happy", MPFR_FLAGS_INEXACT);
    emit_case(out, "happy", MPFR_FLAGS_ERANGE);
    emit_case(out, "happy", MPFR_FLAGS_DIVBY0);
    emit_case(out, "happy", MPFR_FLAGS_ALL);
    emit_case(out, "happy", MPFR_FLAGS_ALL ^ BIT_TO_SET);
    emit_case(out, "happy", BIT_TO_SET | MPFR_FLAGS_OVERFLOW);
    emit_case(out, "happy", BIT_TO_SET | MPFR_FLAGS_NAN);
    emit_case(out, "happy", BIT_TO_SET | MPFR_FLAGS_INEXACT);
    emit_case(out, "happy", BIT_TO_SET | MPFR_FLAGS_ERANGE);
    emit_case(out, "happy", BIT_TO_SET | MPFR_FLAGS_UNDERFLOW);
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

    /* adversarial: 12 -- idempotence, repeated sets, our-bit-already-set,
     * full register (set is a no-op there). */
    emit_case(out, "adversarial", BIT_TO_SET);
    emit_case(out, "adversarial", BIT_TO_SET | MPFR_FLAGS_ALL);
    emit_case(out, "adversarial", BIT_TO_SET);  /* twice to check stability */
    emit_case(out, "adversarial", 0);             /* set-into-empty */
    emit_case(out, "adversarial", MPFR_FLAGS_ALL);
    emit_case(out, "adversarial", MPFR_FLAGS_ALL ^ BIT_TO_SET);
    emit_case(out, "adversarial", MPFR_FLAGS_NAN | BIT_TO_SET);
    emit_case(out, "adversarial", MPFR_FLAGS_NAN | MPFR_FLAGS_OVERFLOW);
    emit_case(out, "adversarial", MPFR_FLAGS_INEXACT | MPFR_FLAGS_ERANGE);
    emit_case(out, "adversarial", MPFR_FLAGS_UNDERFLOW | MPFR_FLAGS_INEXACT | BIT_TO_SET);
    emit_case(out, "adversarial", MPFR_FLAGS_OVERFLOW | MPFR_FLAGS_NAN | MPFR_FLAGS_INEXACT | MPFR_FLAGS_ERANGE);
    emit_case(out, "adversarial", MPFR_FLAGS_ALL ^ MPFR_FLAGS_OVERFLOW);

    /* fuzz: 50 -- PRNG-driven mask in [0, MPFR_FLAGS_ALL]. */
    {
        xs64_t rng;
        /* Hex seed using only 0-9 A-F per HANDOFF gotcha #3. */
        xs64_seed(&rng, 0x0FED1CBA98765432ULL);
        for (int rep = 0; rep < 50; ++rep) {
            const uint64_t mask = xs64_below(&rng, (uint64_t)MPFR_FLAGS_ALL + 1);
            emit_case(out, "fuzz", mask);
        }
    }

    /* mined: 5 -- from mpfr/tests/texceptions.c L327-L351, which sets each
     * flag via the set_* family and verifies the corresponding _p predicate.
     * We mine the "set X from empty", "set X with neighbours present", and
     * "set X from full" patterns. */
    emit_case(out, "mined", 0);                                 /* set-from-empty */
    emit_case(out, "mined", BIT_TO_SET);                        /* set-already-set (no-op) */
    emit_case(out, "mined", MPFR_FLAGS_OVERFLOW);               /* set with OVERFLOW present */
    emit_case(out, "mined", MPFR_FLAGS_NAN | MPFR_FLAGS_INEXACT);/* set with two neighbours */
    emit_case(out, "mined", MPFR_FLAGS_ALL);                    /* set from full (no-op) */

    return 0;
}
