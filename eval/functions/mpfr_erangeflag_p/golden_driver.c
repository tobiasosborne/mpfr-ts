/*
 * golden_driver.c -- Golden master for MPFR's mpfr_erangeflag_p.
 * C: int mpfr_erangeflag_p(void) -- return __gmpfr_flags & MPFR_FLAGS_ERANGE.
 * Ref: mpfr/src/exceptions.c L377-L382.
 */
#include "common.h"
#include <assert.h>
#include <inttypes.h>

#define PREDICATE_BIT MPFR_FLAGS_ERANGE

static inline void emit_case(FILE *out, const char *tag, uint64_t mask) {
    assert(mask <= MPFR_FLAGS_ALL);
    mpfr_clear_flags();
    mpfr_flags_set((mpfr_flags_t)mask);
    const uint64_t t0 = now_ns();
    const int got = mpfr_erangeflag_p();
    const uint64_t elapsed = now_ns() - t0;
    jl_begin(out, tag);
    jl_kv_u64(out, 1, "mask", mask);
    jl_end_inputs(out);
    jl_output_scalar_bool(out, got);
    jl_finish(out, elapsed);
}

int main(void) {
    FILE *out = stdout;

    /* happy: 20 */
    emit_case(out, "happy", 0);
    emit_case(out, "happy", PREDICATE_BIT);
    emit_case(out, "happy", MPFR_FLAGS_UNDERFLOW);
    emit_case(out, "happy", MPFR_FLAGS_OVERFLOW);
    emit_case(out, "happy", MPFR_FLAGS_NAN);
    emit_case(out, "happy", MPFR_FLAGS_INEXACT);
    emit_case(out, "happy", MPFR_FLAGS_ERANGE);
    emit_case(out, "happy", MPFR_FLAGS_DIVBY0);
    emit_case(out, "happy", MPFR_FLAGS_ALL);
    emit_case(out, "happy", MPFR_FLAGS_ALL ^ PREDICATE_BIT);
    emit_case(out, "happy", PREDICATE_BIT | MPFR_FLAGS_UNDERFLOW);
    emit_case(out, "happy", PREDICATE_BIT | MPFR_FLAGS_OVERFLOW);
    emit_case(out, "happy", PREDICATE_BIT | MPFR_FLAGS_NAN);
    emit_case(out, "happy", PREDICATE_BIT | MPFR_FLAGS_INEXACT);
    emit_case(out, "happy", PREDICATE_BIT | MPFR_FLAGS_DIVBY0);
    emit_case(out, "happy", MPFR_FLAGS_UNDERFLOW | MPFR_FLAGS_OVERFLOW);
    emit_case(out, "happy", MPFR_FLAGS_NAN | MPFR_FLAGS_INEXACT);
    emit_case(out, "happy", MPFR_FLAGS_UNDERFLOW | MPFR_FLAGS_NAN);
    emit_case(out, "happy", MPFR_FLAGS_OVERFLOW | MPFR_FLAGS_DIVBY0);
    emit_case(out, "happy", MPFR_FLAGS_INEXACT | MPFR_FLAGS_DIVBY0);

    /* edge: 30 -- exhaustive single + inverted + C(6,3) triples */
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

    /* adversarial: 12 -- naturally-raised flags from real ops. */
    {
        mpfr_t a, b, c;
        mpfr_init2(a, 53); mpfr_init2(b, 53); mpfr_init2(c, 53);

        mpfr_clear_flags();
        mpfr_set_inf(b, +1);
        (void)mpfr_get_si(b, MPFR_RNDN);
        uint64_t mask = (uint64_t)mpfr_flags_save();
        emit_case(out, "adversarial", mask);

        mpfr_clear_flags();
        mpfr_set_nan(b);
        (void)mpfr_get_si(b, MPFR_RNDN);
        mask = (uint64_t)mpfr_flags_save();
        emit_case(out, "adversarial", mask);

        emit_case(out, "adversarial", PREDICATE_BIT);
        emit_case(out, "adversarial", MPFR_FLAGS_ALL & ~PREDICATE_BIT);

        mpfr_clear_flags();
        mpfr_flags_set(PREDICATE_BIT);
        mpfr_flags_set(PREDICATE_BIT);
        mask = (uint64_t)mpfr_flags_save();
        emit_case(out, "adversarial", mask);

        mpfr_clear_flags();
        mpfr_flags_set(MPFR_FLAGS_ALL);
        mpfr_flags_clear(MPFR_FLAGS_ALL & ~PREDICATE_BIT);
        mask = (uint64_t)mpfr_flags_save();
        emit_case(out, "adversarial", mask);

        emit_case(out, "adversarial", MPFR_FLAGS_ALL);
        emit_case(out, "adversarial", 0);
        emit_case(out, "adversarial", PREDICATE_BIT | MPFR_FLAGS_NAN);
        emit_case(out, "adversarial", PREDICATE_BIT | MPFR_FLAGS_UNDERFLOW);
        emit_case(out, "adversarial", MPFR_FLAGS_NAN | MPFR_FLAGS_DIVBY0);
        emit_case(out, "adversarial", MPFR_FLAGS_OVERFLOW | MPFR_FLAGS_UNDERFLOW);

        mpfr_clear(a); mpfr_clear(b); mpfr_clear(c);
    }

    /* fuzz: 50 */
    {
        xs64_t rng;
        xs64_seed(&rng, 0xEBAFFADE0F1ABCDEULL);
        for (int rep = 0; rep < 50; ++rep) {
            const uint64_t mask = xs64_below(&rng, (uint64_t)MPFR_FLAGS_ALL + 1);
            emit_case(out, "fuzz", mask);
        }
    }

    /* mined: 8 -- patterns from mpfr/tests/texceptions.c */
    emit_case(out, "mined", 0);
    emit_case(out, "mined", MPFR_FLAGS_ERANGE);
    emit_case(out, "mined", MPFR_FLAGS_OVERFLOW);
    emit_case(out, "mined", MPFR_FLAGS_UNDERFLOW);
    emit_case(out, "mined", MPFR_FLAGS_DIVBY0);
    emit_case(out, "mined", MPFR_FLAGS_NAN);
    emit_case(out, "mined", MPFR_FLAGS_ALL);
    emit_case(out, "mined", MPFR_FLAGS_ERANGE | MPFR_FLAGS_NAN);

    return 0;
}
