/*
 * golden_driver.c -- Golden master for MPFR's mpfr_clear_underflow.
 *
 * C: `__gmpfr_flags &= MPFR_FLAGS_ALL ^ MPFR_FLAGS_UNDERFLOW`.
 * Ref: mpfr/src/exceptions.c L152-L158.
 *
 * Wire: {"inputs":{"mask":"<dec>"},"output":"<dec>"}.
 * Driver flow per case: clear, set(mask), clear_underflow, save.
 *
 * Tag distribution (Rule 7): happy 20, edge 30, adv 12, fuzz 50, mined 5.
 */
#include "common.h"

#include <assert.h>
#include <inttypes.h>

#define BIT_TO_CLEAR ((uint64_t)MPFR_FLAGS_UNDERFLOW)

static inline void emit_case(FILE *out, const char *tag, uint64_t mask) {
    assert(mask <= MPFR_FLAGS_ALL);
    mpfr_clear_flags();
    mpfr_flags_set((mpfr_flags_t)mask);
    const uint64_t t0 = now_ns();
    mpfr_clear_underflow();
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

    /* happy: 20 */
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
    emit_case(out, "happy", BIT_TO_CLEAR | MPFR_FLAGS_DIVBY0);
    emit_case(out, "happy", MPFR_FLAGS_OVERFLOW | MPFR_FLAGS_NAN);
    emit_case(out, "happy", MPFR_FLAGS_OVERFLOW | MPFR_FLAGS_INEXACT);
    emit_case(out, "happy", MPFR_FLAGS_NAN | MPFR_FLAGS_INEXACT);
    emit_case(out, "happy", MPFR_FLAGS_INEXACT | MPFR_FLAGS_ERANGE);
    emit_case(out, "happy", MPFR_FLAGS_ERANGE | MPFR_FLAGS_DIVBY0);

    /* edge: 30 -- single, inverted, all C(6,3) triples. */
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

    /* adversarial: 12 */
    emit_case(out, "adversarial", BIT_TO_CLEAR);
    emit_case(out, "adversarial", BIT_TO_CLEAR | MPFR_FLAGS_ALL);
    emit_case(out, "adversarial", BIT_TO_CLEAR);
    emit_case(out, "adversarial", 0);
    emit_case(out, "adversarial", MPFR_FLAGS_ALL);
    emit_case(out, "adversarial", MPFR_FLAGS_ALL ^ BIT_TO_CLEAR);
    emit_case(out, "adversarial", MPFR_FLAGS_NAN | BIT_TO_CLEAR);
    emit_case(out, "adversarial", MPFR_FLAGS_NAN | MPFR_FLAGS_OVERFLOW);
    emit_case(out, "adversarial", MPFR_FLAGS_INEXACT | MPFR_FLAGS_ERANGE);
    emit_case(out, "adversarial", MPFR_FLAGS_UNDERFLOW | MPFR_FLAGS_INEXACT | BIT_TO_CLEAR);
    emit_case(out, "adversarial", MPFR_FLAGS_OVERFLOW | MPFR_FLAGS_NAN | MPFR_FLAGS_INEXACT | MPFR_FLAGS_ERANGE);
    emit_case(out, "adversarial", MPFR_FLAGS_ALL ^ MPFR_FLAGS_DIVBY0);

    /* fuzz: 50 -- PRNG seed includes hex literal w/ only 0-9 A-F. */
    {
        xs64_t rng;
        xs64_seed(&rng, 0xFEEDFACEBABEC0DEULL);
        for (int rep = 0; rep < 50; ++rep) {
            const uint64_t mask = xs64_below(&rng, (uint64_t)MPFR_FLAGS_ALL + 1);
            emit_case(out, "fuzz", mask);
        }
    }

    /* mined: 5 */
    emit_case(out, "mined", BIT_TO_CLEAR);
    emit_case(out, "mined", BIT_TO_CLEAR | MPFR_FLAGS_OVERFLOW);
    emit_case(out, "mined", BIT_TO_CLEAR | MPFR_FLAGS_NAN);
    emit_case(out, "mined", MPFR_FLAGS_ALL);
    emit_case(out, "mined", 0);

    return 0;
}
