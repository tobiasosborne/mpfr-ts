/*
 * golden_driver.c -- Golden master for MPFR's mpfr_flags_save.
 * C: mpfr_flags_t mpfr_flags_save(void) -- return __gmpfr_flags.
 * Ref: mpfr/src/exceptions.c L125-L131.
 */
#include "common.h"
#include <assert.h>
#include <inttypes.h>

static inline void emit_case(FILE *out, const char *tag, uint64_t pre) {
    assert(pre <= MPFR_FLAGS_ALL);
    mpfr_clear_flags();
    mpfr_flags_set((mpfr_flags_t)pre);
    const uint64_t t0 = now_ns();
    const uint64_t result = (uint64_t)mpfr_flags_save();
    const uint64_t elapsed = now_ns() - t0;
    jl_begin(out, tag);
    jl_kv_u64(out, 1, "pre", pre);
    jl_end_inputs(out);
    jl_output_scalar_u64(out, result);
    jl_finish(out, elapsed);
}

int main(void) {
    FILE *out = stdout;

    /* happy: 20 -- enumerate common single/multi-bit states. */
    emit_case(out, "happy", 0);
    emit_case(out, "happy", MPFR_FLAGS_UNDERFLOW);
    emit_case(out, "happy", MPFR_FLAGS_OVERFLOW);
    emit_case(out, "happy", MPFR_FLAGS_NAN);
    emit_case(out, "happy", MPFR_FLAGS_INEXACT);
    emit_case(out, "happy", MPFR_FLAGS_ERANGE);
    emit_case(out, "happy", MPFR_FLAGS_DIVBY0);
    emit_case(out, "happy", MPFR_FLAGS_ALL);
    emit_case(out, "happy", MPFR_FLAGS_UNDERFLOW | MPFR_FLAGS_OVERFLOW);
    emit_case(out, "happy", MPFR_FLAGS_NAN | MPFR_FLAGS_INEXACT);
    emit_case(out, "happy", MPFR_FLAGS_ERANGE | MPFR_FLAGS_DIVBY0);
    emit_case(out, "happy", MPFR_FLAGS_UNDERFLOW | MPFR_FLAGS_NAN);
    emit_case(out, "happy", MPFR_FLAGS_OVERFLOW | MPFR_FLAGS_INEXACT);
    emit_case(out, "happy", MPFR_FLAGS_ALL ^ MPFR_FLAGS_NAN);
    emit_case(out, "happy", MPFR_FLAGS_ALL ^ MPFR_FLAGS_DIVBY0);
    emit_case(out, "happy", MPFR_FLAGS_ALL ^ MPFR_FLAGS_UNDERFLOW);
    emit_case(out, "happy", MPFR_FLAGS_UNDERFLOW | MPFR_FLAGS_DIVBY0);
    emit_case(out, "happy", MPFR_FLAGS_OVERFLOW | MPFR_FLAGS_ERANGE);
    emit_case(out, "happy", MPFR_FLAGS_NAN | MPFR_FLAGS_DIVBY0);
    emit_case(out, "happy", MPFR_FLAGS_INEXACT | MPFR_FLAGS_ERANGE);

    /* edge: 30 -- all 6 single, all 6 inverted, plus all C(6,3) triples. */
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
    emit_case(out, "adversarial", 0);
    emit_case(out, "adversarial", MPFR_FLAGS_ALL);
    emit_case(out, "adversarial", MPFR_FLAGS_NAN);
    emit_case(out, "adversarial", MPFR_FLAGS_DIVBY0);
    emit_case(out, "adversarial", MPFR_FLAGS_UNDERFLOW);
    emit_case(out, "adversarial", MPFR_FLAGS_OVERFLOW);
    emit_case(out, "adversarial", MPFR_FLAGS_INEXACT);
    emit_case(out, "adversarial", MPFR_FLAGS_ERANGE);
    emit_case(out, "adversarial", MPFR_FLAGS_NAN | MPFR_FLAGS_INEXACT | MPFR_FLAGS_ERANGE);
    emit_case(out, "adversarial", MPFR_FLAGS_ALL ^ MPFR_FLAGS_NAN);
    emit_case(out, "adversarial", MPFR_FLAGS_UNDERFLOW | MPFR_FLAGS_DIVBY0);
    emit_case(out, "adversarial", MPFR_FLAGS_OVERFLOW | MPFR_FLAGS_NAN);

    /* fuzz: 50 */
    {
        xs64_t rng;
        xs64_seed(&rng, 0xBEEFD00DDABBADDAULL);
        for (int rep = 0; rep < 50; ++rep) {
            const uint64_t pre = xs64_below(&rng, (uint64_t)MPFR_FLAGS_ALL + 1);
            emit_case(out, "fuzz", pre);
        }
    }

    /* mined: 5 */
    emit_case(out, "mined", MPFR_FLAGS_OVERFLOW);
    emit_case(out, "mined", MPFR_FLAGS_UNDERFLOW);
    emit_case(out, "mined", MPFR_FLAGS_DIVBY0);
    emit_case(out, "mined", MPFR_FLAGS_NAN);
    emit_case(out, "mined", MPFR_FLAGS_ALL);

    return 0;
}
