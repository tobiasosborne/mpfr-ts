/* mpfr_clear_nanflag: __gmpfr_flags &= MPFR_FLAGS_ALL ^ MPFR_FLAGS_NAN.
 * Ref: mpfr/src/exceptions.c L176-L182. */
#include "common.h"
#include <assert.h>
#include <inttypes.h>

#define BIT_TO_CLEAR ((uint64_t)MPFR_FLAGS_NAN)

static inline void emit_case(FILE *out, const char *tag, uint64_t mask) {
    assert(mask <= MPFR_FLAGS_ALL);
    mpfr_clear_flags();
    mpfr_flags_set((mpfr_flags_t)mask);
    const uint64_t t0 = now_ns();
    mpfr_clear_nanflag();
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
    emit_case(out, "happy", BIT_TO_CLEAR | MPFR_FLAGS_UNDERFLOW);
    emit_case(out, "happy", BIT_TO_CLEAR | MPFR_FLAGS_OVERFLOW);
    emit_case(out, "happy", BIT_TO_CLEAR | MPFR_FLAGS_INEXACT);
    emit_case(out, "happy", BIT_TO_CLEAR | MPFR_FLAGS_ERANGE);
    emit_case(out, "happy", BIT_TO_CLEAR | MPFR_FLAGS_DIVBY0);
    emit_case(out, "happy", MPFR_FLAGS_UNDERFLOW | MPFR_FLAGS_OVERFLOW);
    emit_case(out, "happy", MPFR_FLAGS_OVERFLOW | MPFR_FLAGS_INEXACT);
    emit_case(out, "happy", MPFR_FLAGS_UNDERFLOW | MPFR_FLAGS_INEXACT);
    emit_case(out, "happy", MPFR_FLAGS_INEXACT | MPFR_FLAGS_ERANGE);
    emit_case(out, "happy", MPFR_FLAGS_ERANGE | MPFR_FLAGS_DIVBY0);
    /* edge: 30 */
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
    emit_case(out, "adversarial", MPFR_FLAGS_OVERFLOW | BIT_TO_CLEAR);
    emit_case(out, "adversarial", MPFR_FLAGS_OVERFLOW | MPFR_FLAGS_UNDERFLOW);
    emit_case(out, "adversarial", MPFR_FLAGS_INEXACT | MPFR_FLAGS_ERANGE);
    emit_case(out, "adversarial", MPFR_FLAGS_UNDERFLOW | MPFR_FLAGS_INEXACT | BIT_TO_CLEAR);
    emit_case(out, "adversarial", MPFR_FLAGS_OVERFLOW | MPFR_FLAGS_INEXACT | MPFR_FLAGS_ERANGE | MPFR_FLAGS_DIVBY0);
    emit_case(out, "adversarial", MPFR_FLAGS_ALL ^ MPFR_FLAGS_DIVBY0);
    /* fuzz: 50 */
    {
        xs64_t rng;
        xs64_seed(&rng, 0xBADD0FACEBADBEEFULL);
        for (int rep = 0; rep < 50; ++rep) {
            const uint64_t mask = xs64_below(&rng, (uint64_t)MPFR_FLAGS_ALL + 1);
            emit_case(out, "fuzz", mask);
        }
    }
    /* mined: 5 */
    emit_case(out, "mined", BIT_TO_CLEAR);
    emit_case(out, "mined", BIT_TO_CLEAR | MPFR_FLAGS_OVERFLOW);
    emit_case(out, "mined", BIT_TO_CLEAR | MPFR_FLAGS_UNDERFLOW);
    emit_case(out, "mined", MPFR_FLAGS_ALL);
    emit_case(out, "mined", 0);
    return 0;
}
