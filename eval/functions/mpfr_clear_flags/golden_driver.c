/* mpfr_clear_flags: __gmpfr_flags = 0 (wipes the entire flag register).
 * Ref: mpfr/src/exceptions.c L144-L150. */
#include "common.h"
#include <assert.h>
#include <inttypes.h>

static inline void emit_case(FILE *out, const char *tag, uint64_t mask) {
    assert(mask <= MPFR_FLAGS_ALL);
    mpfr_clear_flags();
    mpfr_flags_set((mpfr_flags_t)mask);
    const uint64_t t0 = now_ns();
    mpfr_clear_flags();
    const uint64_t result = (uint64_t)mpfr_flags_save();
    const uint64_t elapsed = now_ns() - t0;
    jl_begin(out, tag);
    jl_kv_u64(out, 1, "mask", mask);
    jl_end_inputs(out);
    jl_output_scalar_u64(out, result);  /* always 0 */
    jl_finish(out, elapsed);
}

int main(void) {
    FILE *out = stdout;
    /* happy: 20 -- all 6 single bits, all 2-flag combos, plus full and empty. */
    emit_case(out, "happy", 0);
    emit_case(out, "happy", MPFR_FLAGS_UNDERFLOW);
    emit_case(out, "happy", MPFR_FLAGS_OVERFLOW);
    emit_case(out, "happy", MPFR_FLAGS_NAN);
    emit_case(out, "happy", MPFR_FLAGS_INEXACT);
    emit_case(out, "happy", MPFR_FLAGS_ERANGE);
    emit_case(out, "happy", MPFR_FLAGS_DIVBY0);
    emit_case(out, "happy", MPFR_FLAGS_ALL);
    emit_case(out, "happy", MPFR_FLAGS_UNDERFLOW | MPFR_FLAGS_OVERFLOW);
    emit_case(out, "happy", MPFR_FLAGS_UNDERFLOW | MPFR_FLAGS_NAN);
    emit_case(out, "happy", MPFR_FLAGS_UNDERFLOW | MPFR_FLAGS_INEXACT);
    emit_case(out, "happy", MPFR_FLAGS_UNDERFLOW | MPFR_FLAGS_ERANGE);
    emit_case(out, "happy", MPFR_FLAGS_UNDERFLOW | MPFR_FLAGS_DIVBY0);
    emit_case(out, "happy", MPFR_FLAGS_OVERFLOW | MPFR_FLAGS_NAN);
    emit_case(out, "happy", MPFR_FLAGS_OVERFLOW | MPFR_FLAGS_INEXACT);
    emit_case(out, "happy", MPFR_FLAGS_OVERFLOW | MPFR_FLAGS_ERANGE);
    emit_case(out, "happy", MPFR_FLAGS_OVERFLOW | MPFR_FLAGS_DIVBY0);
    emit_case(out, "happy", MPFR_FLAGS_NAN | MPFR_FLAGS_INEXACT);
    emit_case(out, "happy", MPFR_FLAGS_NAN | MPFR_FLAGS_ERANGE);
    emit_case(out, "happy", MPFR_FLAGS_NAN | MPFR_FLAGS_DIVBY0);
    /* edge: 30 -- single inverted + all C(6,3) triples + a few extras. */
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
    emit_case(out, "edge", MPFR_FLAGS_NAN | MPFR_FLAGS_ERANGE | MPFR_FLAGS_DIVBY0);
    emit_case(out, "edge", MPFR_FLAGS_INEXACT | MPFR_FLAGS_ERANGE | MPFR_FLAGS_DIVBY0);
    emit_case(out, "edge", MPFR_FLAGS_INEXACT | MPFR_FLAGS_ERANGE);
    emit_case(out, "edge", MPFR_FLAGS_ERANGE | MPFR_FLAGS_DIVBY0);
    emit_case(out, "edge", MPFR_FLAGS_INEXACT | MPFR_FLAGS_DIVBY0);
    emit_case(out, "edge", MPFR_FLAGS_UNDERFLOW | MPFR_FLAGS_DIVBY0 | MPFR_FLAGS_NAN);
    /* adversarial: 12 -- idempotence, repeated clears, various masks. */
    emit_case(out, "adversarial", MPFR_FLAGS_ALL);
    emit_case(out, "adversarial", 0);
    emit_case(out, "adversarial", MPFR_FLAGS_NAN);
    emit_case(out, "adversarial", MPFR_FLAGS_INEXACT | MPFR_FLAGS_ERANGE);
    emit_case(out, "adversarial", MPFR_FLAGS_ALL);
    emit_case(out, "adversarial", MPFR_FLAGS_UNDERFLOW | MPFR_FLAGS_OVERFLOW);
    emit_case(out, "adversarial", MPFR_FLAGS_DIVBY0 | MPFR_FLAGS_INEXACT);
    emit_case(out, "adversarial", MPFR_FLAGS_NAN | MPFR_FLAGS_UNDERFLOW);
    emit_case(out, "adversarial", MPFR_FLAGS_OVERFLOW | MPFR_FLAGS_NAN | MPFR_FLAGS_DIVBY0);
    emit_case(out, "adversarial", MPFR_FLAGS_INEXACT);
    emit_case(out, "adversarial", MPFR_FLAGS_ERANGE);
    emit_case(out, "adversarial", MPFR_FLAGS_UNDERFLOW);
    /* fuzz: 50 */
    {
        xs64_t rng;
        xs64_seed(&rng, 0x12345678ABCDEF00ULL);
        for (int rep = 0; rep < 50; ++rep) {
            const uint64_t mask = xs64_below(&rng, (uint64_t)MPFR_FLAGS_ALL + 1);
            emit_case(out, "fuzz", mask);
        }
    }
    /* mined: 5 -- from texceptions.c L327-L351 (which calls mpfr_clear_flags as the test scaffolding). */
    emit_case(out, "mined", MPFR_FLAGS_OVERFLOW);
    emit_case(out, "mined", MPFR_FLAGS_UNDERFLOW);
    emit_case(out, "mined", MPFR_FLAGS_NAN);
    emit_case(out, "mined", MPFR_FLAGS_DIVBY0);
    emit_case(out, "mined", MPFR_FLAGS_ALL);
    return 0;
}
