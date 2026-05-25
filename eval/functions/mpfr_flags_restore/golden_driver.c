/*
 * golden_driver.c -- Golden master for MPFR's mpfr_flags_restore.
 * C: void mpfr_flags_restore(mpfr_flags_t flags, mpfr_flags_t mask)
 *    __gmpfr_flags = (__gmpfr_flags & (ALL ^ mask)) | (flags & mask)
 * Ref: mpfr/src/exceptions.c L133-L141.
 */
#include "common.h"
#include <assert.h>
#include <inttypes.h>

static inline void emit_case(FILE *out, const char *tag, uint64_t pre, uint64_t flags, uint64_t mask) {
    assert(pre <= MPFR_FLAGS_ALL);
    assert(flags <= MPFR_FLAGS_ALL);
    assert(mask <= MPFR_FLAGS_ALL);
    mpfr_clear_flags();
    mpfr_flags_set((mpfr_flags_t)pre);
    const uint64_t t0 = now_ns();
    mpfr_flags_restore((mpfr_flags_t)flags, (mpfr_flags_t)mask);
    const uint64_t result = (uint64_t)mpfr_flags_save();
    const uint64_t elapsed = now_ns() - t0;
    jl_begin(out, tag);
    jl_kv_u64(out, 1, "pre", pre);
    jl_kv_u64(out, 0, "flags", flags);
    jl_kv_u64(out, 0, "mask", mask);
    jl_end_inputs(out);
    jl_output_scalar_u64(out, result);
    jl_finish(out, elapsed);
}

int main(void) {
    FILE *out = stdout;

    /* happy: 20 -- common save+restore patterns. */
    emit_case(out, "happy", 0, 0, MPFR_FLAGS_ALL);
    emit_case(out, "happy", MPFR_FLAGS_ALL, MPFR_FLAGS_ALL, MPFR_FLAGS_ALL);
    emit_case(out, "happy", MPFR_FLAGS_NAN, MPFR_FLAGS_DIVBY0, MPFR_FLAGS_ALL);
    emit_case(out, "happy", MPFR_FLAGS_UNDERFLOW, MPFR_FLAGS_OVERFLOW, MPFR_FLAGS_UNDERFLOW | MPFR_FLAGS_OVERFLOW);
    emit_case(out, "happy", MPFR_FLAGS_ALL, 0, MPFR_FLAGS_ALL);
    emit_case(out, "happy", 0, MPFR_FLAGS_ALL, MPFR_FLAGS_ALL);
    emit_case(out, "happy", MPFR_FLAGS_INEXACT, MPFR_FLAGS_INEXACT, 0);
    emit_case(out, "happy", MPFR_FLAGS_INEXACT, MPFR_FLAGS_ERANGE, MPFR_FLAGS_INEXACT);
    emit_case(out, "happy", MPFR_FLAGS_INEXACT, MPFR_FLAGS_ERANGE, MPFR_FLAGS_ERANGE);
    emit_case(out, "happy", MPFR_FLAGS_INEXACT, MPFR_FLAGS_ERANGE, MPFR_FLAGS_INEXACT | MPFR_FLAGS_ERANGE);
    emit_case(out, "happy", MPFR_FLAGS_NAN | MPFR_FLAGS_OVERFLOW, MPFR_FLAGS_UNDERFLOW, MPFR_FLAGS_NAN);
    emit_case(out, "happy", MPFR_FLAGS_NAN | MPFR_FLAGS_OVERFLOW, MPFR_FLAGS_UNDERFLOW, MPFR_FLAGS_UNDERFLOW);
    emit_case(out, "happy", MPFR_FLAGS_DIVBY0, MPFR_FLAGS_NAN, MPFR_FLAGS_NAN | MPFR_FLAGS_DIVBY0);
    emit_case(out, "happy", MPFR_FLAGS_ALL, MPFR_FLAGS_ALL ^ MPFR_FLAGS_NAN, MPFR_FLAGS_NAN);
    emit_case(out, "happy", MPFR_FLAGS_ALL ^ MPFR_FLAGS_DIVBY0, MPFR_FLAGS_DIVBY0, MPFR_FLAGS_DIVBY0);
    emit_case(out, "happy", MPFR_FLAGS_UNDERFLOW, MPFR_FLAGS_OVERFLOW, MPFR_FLAGS_ALL);
    emit_case(out, "happy", MPFR_FLAGS_NAN, 0, MPFR_FLAGS_NAN);
    emit_case(out, "happy", MPFR_FLAGS_NAN | MPFR_FLAGS_INEXACT, 0, MPFR_FLAGS_INEXACT);
    emit_case(out, "happy", MPFR_FLAGS_INEXACT, MPFR_FLAGS_INEXACT, MPFR_FLAGS_INEXACT);
    emit_case(out, "happy", 0, MPFR_FLAGS_ALL, 0);

    /* edge: 30 -- single bit replacement, mask zero, mask all. */
    emit_case(out, "edge", 0, MPFR_FLAGS_UNDERFLOW, MPFR_FLAGS_UNDERFLOW);
    emit_case(out, "edge", 0, MPFR_FLAGS_OVERFLOW, MPFR_FLAGS_OVERFLOW);
    emit_case(out, "edge", 0, MPFR_FLAGS_NAN, MPFR_FLAGS_NAN);
    emit_case(out, "edge", 0, MPFR_FLAGS_INEXACT, MPFR_FLAGS_INEXACT);
    emit_case(out, "edge", 0, MPFR_FLAGS_ERANGE, MPFR_FLAGS_ERANGE);
    emit_case(out, "edge", 0, MPFR_FLAGS_DIVBY0, MPFR_FLAGS_DIVBY0);
    emit_case(out, "edge", MPFR_FLAGS_ALL, 0, MPFR_FLAGS_UNDERFLOW);
    emit_case(out, "edge", MPFR_FLAGS_ALL, 0, MPFR_FLAGS_OVERFLOW);
    emit_case(out, "edge", MPFR_FLAGS_ALL, 0, MPFR_FLAGS_NAN);
    emit_case(out, "edge", MPFR_FLAGS_ALL, 0, MPFR_FLAGS_INEXACT);
    emit_case(out, "edge", MPFR_FLAGS_ALL, 0, MPFR_FLAGS_ERANGE);
    emit_case(out, "edge", MPFR_FLAGS_ALL, 0, MPFR_FLAGS_DIVBY0);
    emit_case(out, "edge", MPFR_FLAGS_UNDERFLOW, MPFR_FLAGS_OVERFLOW, 0);
    emit_case(out, "edge", MPFR_FLAGS_OVERFLOW, MPFR_FLAGS_NAN, 0);
    emit_case(out, "edge", MPFR_FLAGS_ALL, MPFR_FLAGS_ALL, 0);
    emit_case(out, "edge", 0, MPFR_FLAGS_ALL, 0);
    emit_case(out, "edge", MPFR_FLAGS_UNDERFLOW | MPFR_FLAGS_OVERFLOW, MPFR_FLAGS_NAN | MPFR_FLAGS_INEXACT, MPFR_FLAGS_OVERFLOW | MPFR_FLAGS_NAN);
    emit_case(out, "edge", MPFR_FLAGS_NAN | MPFR_FLAGS_DIVBY0, MPFR_FLAGS_UNDERFLOW | MPFR_FLAGS_ERANGE, MPFR_FLAGS_DIVBY0 | MPFR_FLAGS_UNDERFLOW);
    emit_case(out, "edge", MPFR_FLAGS_INEXACT, MPFR_FLAGS_INEXACT, MPFR_FLAGS_INEXACT);
    emit_case(out, "edge", MPFR_FLAGS_INEXACT, MPFR_FLAGS_INEXACT, MPFR_FLAGS_ERANGE);
    emit_case(out, "edge", MPFR_FLAGS_INEXACT, MPFR_FLAGS_NAN, MPFR_FLAGS_NAN | MPFR_FLAGS_INEXACT);
    emit_case(out, "edge", MPFR_FLAGS_ALL ^ MPFR_FLAGS_UNDERFLOW, MPFR_FLAGS_UNDERFLOW, MPFR_FLAGS_UNDERFLOW);
    emit_case(out, "edge", MPFR_FLAGS_ALL ^ MPFR_FLAGS_OVERFLOW, MPFR_FLAGS_OVERFLOW, MPFR_FLAGS_OVERFLOW);
    emit_case(out, "edge", MPFR_FLAGS_ALL ^ MPFR_FLAGS_NAN, MPFR_FLAGS_NAN, MPFR_FLAGS_NAN);
    emit_case(out, "edge", MPFR_FLAGS_ALL ^ MPFR_FLAGS_INEXACT, MPFR_FLAGS_INEXACT, MPFR_FLAGS_INEXACT);
    emit_case(out, "edge", MPFR_FLAGS_ALL ^ MPFR_FLAGS_ERANGE, MPFR_FLAGS_ERANGE, MPFR_FLAGS_ERANGE);
    emit_case(out, "edge", MPFR_FLAGS_ALL ^ MPFR_FLAGS_DIVBY0, MPFR_FLAGS_DIVBY0, MPFR_FLAGS_DIVBY0);
    emit_case(out, "edge", MPFR_FLAGS_UNDERFLOW | MPFR_FLAGS_OVERFLOW | MPFR_FLAGS_NAN, 0, MPFR_FLAGS_UNDERFLOW);
    emit_case(out, "edge", MPFR_FLAGS_INEXACT | MPFR_FLAGS_ERANGE | MPFR_FLAGS_DIVBY0, MPFR_FLAGS_NAN, MPFR_FLAGS_INEXACT | MPFR_FLAGS_NAN);
    emit_case(out, "edge", MPFR_FLAGS_OVERFLOW | MPFR_FLAGS_DIVBY0, MPFR_FLAGS_UNDERFLOW | MPFR_FLAGS_NAN, MPFR_FLAGS_ALL);

    /* adversarial: 12 -- save then restore-with-different-current */
    emit_case(out, "adversarial", MPFR_FLAGS_ALL, MPFR_FLAGS_ALL, MPFR_FLAGS_ALL);
    emit_case(out, "adversarial", MPFR_FLAGS_ALL, 0, MPFR_FLAGS_ALL);
    emit_case(out, "adversarial", 0, MPFR_FLAGS_ALL, MPFR_FLAGS_ALL);
    emit_case(out, "adversarial", MPFR_FLAGS_NAN, MPFR_FLAGS_DIVBY0, MPFR_FLAGS_NAN | MPFR_FLAGS_DIVBY0);
    emit_case(out, "adversarial", MPFR_FLAGS_UNDERFLOW | MPFR_FLAGS_OVERFLOW, 0, MPFR_FLAGS_NAN);
    emit_case(out, "adversarial", MPFR_FLAGS_INEXACT | MPFR_FLAGS_ERANGE, MPFR_FLAGS_DIVBY0, MPFR_FLAGS_INEXACT | MPFR_FLAGS_DIVBY0);
    emit_case(out, "adversarial", MPFR_FLAGS_DIVBY0, MPFR_FLAGS_NAN, MPFR_FLAGS_NAN);
    emit_case(out, "adversarial", MPFR_FLAGS_OVERFLOW, MPFR_FLAGS_UNDERFLOW, MPFR_FLAGS_OVERFLOW | MPFR_FLAGS_UNDERFLOW);
    emit_case(out, "adversarial", MPFR_FLAGS_ALL ^ MPFR_FLAGS_DIVBY0, MPFR_FLAGS_DIVBY0, MPFR_FLAGS_DIVBY0);
    emit_case(out, "adversarial", MPFR_FLAGS_NAN, MPFR_FLAGS_NAN, 0);
    emit_case(out, "adversarial", MPFR_FLAGS_UNDERFLOW, MPFR_FLAGS_OVERFLOW, MPFR_FLAGS_UNDERFLOW);
    emit_case(out, "adversarial", MPFR_FLAGS_INEXACT, MPFR_FLAGS_UNDERFLOW, MPFR_FLAGS_INEXACT | MPFR_FLAGS_UNDERFLOW);

    /* fuzz: 50 */
    {
        xs64_t rng;
        xs64_seed(&rng, 0xB16B00B5CAFE000ULL);
        for (int rep = 0; rep < 50; ++rep) {
            const uint64_t pre = xs64_below(&rng, (uint64_t)MPFR_FLAGS_ALL + 1);
            const uint64_t flags = xs64_below(&rng, (uint64_t)MPFR_FLAGS_ALL + 1);
            const uint64_t mask = xs64_below(&rng, (uint64_t)MPFR_FLAGS_ALL + 1);
            emit_case(out, "fuzz", pre, flags, mask);
        }
    }

    /* mined: 5 -- canonical save/restore pattern (mpfr/tests/texceptions.c) */
    emit_case(out, "mined", MPFR_FLAGS_NAN, MPFR_FLAGS_DIVBY0, MPFR_FLAGS_NAN | MPFR_FLAGS_DIVBY0);
    emit_case(out, "mined", MPFR_FLAGS_ALL, 0, MPFR_FLAGS_ALL);
    emit_case(out, "mined", 0, MPFR_FLAGS_ALL, MPFR_FLAGS_ALL);
    emit_case(out, "mined", MPFR_FLAGS_OVERFLOW, MPFR_FLAGS_UNDERFLOW, MPFR_FLAGS_UNDERFLOW | MPFR_FLAGS_OVERFLOW);
    emit_case(out, "mined", MPFR_FLAGS_INEXACT, MPFR_FLAGS_INEXACT, MPFR_FLAGS_INEXACT);

    return 0;
}
