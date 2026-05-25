/*
 * golden_driver.c -- Golden master for MPFR's mpfr_flags_clear.
 *
 * C signature: void mpfr_flags_clear(mpfr_flags_t mask)
 * C body: __gmpfr_flags &= MPFR_FLAGS_ALL ^ mask
 * Ref: mpfr/src/exceptions.c L101-L107.
 *
 * Driver flow per case:
 *   1. mpfr_clear_flags()
 *   2. mpfr_flags_set(pre)
 *   3. mpfr_flags_clear(mask)
 *   4. read mpfr_flags_save() -> emit
 *
 * Wire: {"inputs":{"pre":"<dec>","mask":"<dec>"},"output":"<dec>",...}
 * Tag minimums: happy 20, edge 30, adv 12, fuzz 50, mined 5.
 */
#include "common.h"
#include <assert.h>
#include <inttypes.h>

static inline void emit_case(FILE *out, const char *tag, uint64_t pre, uint64_t mask) {
    assert(pre <= MPFR_FLAGS_ALL);
    assert(mask <= MPFR_FLAGS_ALL);
    mpfr_clear_flags();
    mpfr_flags_set((mpfr_flags_t)pre);
    const uint64_t t0 = now_ns();
    mpfr_flags_clear((mpfr_flags_t)mask);
    const uint64_t result = (uint64_t)mpfr_flags_save();
    const uint64_t elapsed = now_ns() - t0;
    jl_begin(out, tag);
    jl_kv_u64(out, 1, "pre", pre);
    jl_kv_u64(out, 0, "mask", mask);
    jl_end_inputs(out);
    jl_output_scalar_u64(out, result);
    jl_finish(out, elapsed);
}

int main(void) {
    FILE *out = stdout;

    /* happy: 20 -- common pre/mask combinations. */
    emit_case(out, "happy", 0, 0);
    emit_case(out, "happy", 0, MPFR_FLAGS_ALL);
    emit_case(out, "happy", MPFR_FLAGS_ALL, 0);
    emit_case(out, "happy", MPFR_FLAGS_ALL, MPFR_FLAGS_ALL);
    emit_case(out, "happy", MPFR_FLAGS_UNDERFLOW, MPFR_FLAGS_UNDERFLOW);
    emit_case(out, "happy", MPFR_FLAGS_OVERFLOW, MPFR_FLAGS_OVERFLOW);
    emit_case(out, "happy", MPFR_FLAGS_NAN, MPFR_FLAGS_NAN);
    emit_case(out, "happy", MPFR_FLAGS_INEXACT, MPFR_FLAGS_INEXACT);
    emit_case(out, "happy", MPFR_FLAGS_ERANGE, MPFR_FLAGS_ERANGE);
    emit_case(out, "happy", MPFR_FLAGS_DIVBY0, MPFR_FLAGS_DIVBY0);
    emit_case(out, "happy", MPFR_FLAGS_ALL, MPFR_FLAGS_UNDERFLOW);
    emit_case(out, "happy", MPFR_FLAGS_ALL, MPFR_FLAGS_OVERFLOW);
    emit_case(out, "happy", MPFR_FLAGS_ALL, MPFR_FLAGS_NAN);
    emit_case(out, "happy", MPFR_FLAGS_ALL, MPFR_FLAGS_INEXACT);
    emit_case(out, "happy", MPFR_FLAGS_ALL, MPFR_FLAGS_ERANGE);
    emit_case(out, "happy", MPFR_FLAGS_ALL, MPFR_FLAGS_DIVBY0);
    emit_case(out, "happy", MPFR_FLAGS_UNDERFLOW | MPFR_FLAGS_OVERFLOW, MPFR_FLAGS_UNDERFLOW);
    emit_case(out, "happy", MPFR_FLAGS_NAN | MPFR_FLAGS_INEXACT, MPFR_FLAGS_NAN);
    emit_case(out, "happy", MPFR_FLAGS_ERANGE | MPFR_FLAGS_DIVBY0, MPFR_FLAGS_ERANGE);
    emit_case(out, "happy", MPFR_FLAGS_UNDERFLOW | MPFR_FLAGS_NAN, MPFR_FLAGS_OVERFLOW);

    /* edge: 30 -- single-bit/inverted/disjoint patterns. */
    emit_case(out, "edge", 0, MPFR_FLAGS_UNDERFLOW);
    emit_case(out, "edge", 0, MPFR_FLAGS_OVERFLOW);
    emit_case(out, "edge", 0, MPFR_FLAGS_NAN);
    emit_case(out, "edge", 0, MPFR_FLAGS_INEXACT);
    emit_case(out, "edge", 0, MPFR_FLAGS_ERANGE);
    emit_case(out, "edge", 0, MPFR_FLAGS_DIVBY0);
    emit_case(out, "edge", MPFR_FLAGS_ALL, MPFR_FLAGS_ALL ^ MPFR_FLAGS_UNDERFLOW);
    emit_case(out, "edge", MPFR_FLAGS_ALL, MPFR_FLAGS_ALL ^ MPFR_FLAGS_OVERFLOW);
    emit_case(out, "edge", MPFR_FLAGS_ALL, MPFR_FLAGS_ALL ^ MPFR_FLAGS_NAN);
    emit_case(out, "edge", MPFR_FLAGS_ALL, MPFR_FLAGS_ALL ^ MPFR_FLAGS_INEXACT);
    emit_case(out, "edge", MPFR_FLAGS_ALL, MPFR_FLAGS_ALL ^ MPFR_FLAGS_ERANGE);
    emit_case(out, "edge", MPFR_FLAGS_ALL, MPFR_FLAGS_ALL ^ MPFR_FLAGS_DIVBY0);
    emit_case(out, "edge", MPFR_FLAGS_UNDERFLOW, MPFR_FLAGS_OVERFLOW);
    emit_case(out, "edge", MPFR_FLAGS_OVERFLOW, MPFR_FLAGS_NAN);
    emit_case(out, "edge", MPFR_FLAGS_NAN, MPFR_FLAGS_INEXACT);
    emit_case(out, "edge", MPFR_FLAGS_INEXACT, MPFR_FLAGS_ERANGE);
    emit_case(out, "edge", MPFR_FLAGS_ERANGE, MPFR_FLAGS_DIVBY0);
    emit_case(out, "edge", MPFR_FLAGS_DIVBY0, MPFR_FLAGS_UNDERFLOW);
    emit_case(out, "edge", MPFR_FLAGS_ALL, MPFR_FLAGS_UNDERFLOW | MPFR_FLAGS_OVERFLOW | MPFR_FLAGS_NAN);
    emit_case(out, "edge", MPFR_FLAGS_ALL, MPFR_FLAGS_INEXACT | MPFR_FLAGS_ERANGE | MPFR_FLAGS_DIVBY0);
    emit_case(out, "edge", MPFR_FLAGS_UNDERFLOW | MPFR_FLAGS_INEXACT, MPFR_FLAGS_UNDERFLOW);
    emit_case(out, "edge", MPFR_FLAGS_UNDERFLOW | MPFR_FLAGS_INEXACT, MPFR_FLAGS_INEXACT);
    emit_case(out, "edge", MPFR_FLAGS_UNDERFLOW | MPFR_FLAGS_INEXACT, MPFR_FLAGS_NAN);
    emit_case(out, "edge", MPFR_FLAGS_OVERFLOW | MPFR_FLAGS_DIVBY0, MPFR_FLAGS_OVERFLOW | MPFR_FLAGS_DIVBY0);
    emit_case(out, "edge", MPFR_FLAGS_ALL ^ MPFR_FLAGS_NAN, MPFR_FLAGS_NAN);
    emit_case(out, "edge", MPFR_FLAGS_ALL ^ MPFR_FLAGS_NAN, MPFR_FLAGS_ALL);
    emit_case(out, "edge", MPFR_FLAGS_UNDERFLOW | MPFR_FLAGS_OVERFLOW | MPFR_FLAGS_NAN, MPFR_FLAGS_OVERFLOW);
    emit_case(out, "edge", MPFR_FLAGS_NAN | MPFR_FLAGS_INEXACT | MPFR_FLAGS_DIVBY0, MPFR_FLAGS_INEXACT);
    emit_case(out, "edge", MPFR_FLAGS_UNDERFLOW | MPFR_FLAGS_ERANGE | MPFR_FLAGS_DIVBY0, MPFR_FLAGS_ERANGE);
    emit_case(out, "edge", MPFR_FLAGS_NAN | MPFR_FLAGS_OVERFLOW | MPFR_FLAGS_DIVBY0, MPFR_FLAGS_DIVBY0);

    /* adversarial: 12 -- idempotence, no-ops, full sweeps. */
    emit_case(out, "adversarial", MPFR_FLAGS_ALL, MPFR_FLAGS_ALL);
    emit_case(out, "adversarial", 0, MPFR_FLAGS_ALL);
    emit_case(out, "adversarial", MPFR_FLAGS_DIVBY0, MPFR_FLAGS_UNDERFLOW);
    emit_case(out, "adversarial", MPFR_FLAGS_UNDERFLOW, MPFR_FLAGS_DIVBY0);
    emit_case(out, "adversarial", MPFR_FLAGS_ALL ^ MPFR_FLAGS_DIVBY0, MPFR_FLAGS_DIVBY0);
    emit_case(out, "adversarial", MPFR_FLAGS_ALL ^ MPFR_FLAGS_UNDERFLOW, MPFR_FLAGS_UNDERFLOW);
    emit_case(out, "adversarial", MPFR_FLAGS_NAN, MPFR_FLAGS_NAN | MPFR_FLAGS_OVERFLOW);
    emit_case(out, "adversarial", MPFR_FLAGS_INEXACT | MPFR_FLAGS_ERANGE, MPFR_FLAGS_INEXACT | MPFR_FLAGS_ERANGE);
    emit_case(out, "adversarial", MPFR_FLAGS_ALL, MPFR_FLAGS_UNDERFLOW | MPFR_FLAGS_OVERFLOW | MPFR_FLAGS_NAN | MPFR_FLAGS_INEXACT);
    emit_case(out, "adversarial", MPFR_FLAGS_ALL ^ MPFR_FLAGS_INEXACT, MPFR_FLAGS_INEXACT);
    emit_case(out, "adversarial", MPFR_FLAGS_UNDERFLOW | MPFR_FLAGS_OVERFLOW | MPFR_FLAGS_DIVBY0, MPFR_FLAGS_NAN);
    emit_case(out, "adversarial", MPFR_FLAGS_NAN | MPFR_FLAGS_DIVBY0, MPFR_FLAGS_NAN | MPFR_FLAGS_DIVBY0);

    /* fuzz: 50 -- random (pre, mask). */
    {
        xs64_t rng;
        xs64_seed(&rng, 0xFEEDFACECAFEBEEFULL);
        for (int rep = 0; rep < 50; ++rep) {
            const uint64_t pre = xs64_below(&rng, (uint64_t)MPFR_FLAGS_ALL + 1);
            const uint64_t mask = xs64_below(&rng, (uint64_t)MPFR_FLAGS_ALL + 1);
            emit_case(out, "fuzz", pre, mask);
        }
    }

    /* mined: 5 -- mpfr/tests/texceptions.c patterns. */
    emit_case(out, "mined", MPFR_FLAGS_ALL, MPFR_FLAGS_ALL);
    emit_case(out, "mined", MPFR_FLAGS_OVERFLOW, MPFR_FLAGS_OVERFLOW);
    emit_case(out, "mined", MPFR_FLAGS_UNDERFLOW, MPFR_FLAGS_UNDERFLOW);
    emit_case(out, "mined", MPFR_FLAGS_DIVBY0, MPFR_FLAGS_DIVBY0);
    emit_case(out, "mined", MPFR_FLAGS_NAN | MPFR_FLAGS_INEXACT, MPFR_FLAGS_NAN);

    return 0;
}
