/*
 * golden_driver.c -- Golden master for MPFR's mpfr_inits.
 *
 * C signature is varargs: mpfr_inits(x, y, z, ..., (mpfr_ptr)0).
 * Returns void. Ref: mpfr/src/inits.c L37-L60.
 *
 * The TS port takes a count n: bigint and returns n unchanged (a
 * count-passthrough success marker; see spec.json divergence_from_c).
 * The C driver still calls mpfr_inits with the right number of args
 * (to exercise the libmpfr code path) but emits the count on the wire.
 *
 * Wire: {"inputs":{"n":"<dec>"},"output":"<n-as-dec>"}.
 */
#include "common.h"
#include <assert.h>
#include <inttypes.h>

/* Emit one case: call mpfr_inits with n initialised handles, then emit
 * n on the wire. Stack-allocated handles up to a static cap. */
#define MAX_N 12

static inline void emit_case(FILE *out, const char *tag, unsigned int n) {
    assert(n <= MAX_N);
    mpfr_t slots[MAX_N];
    const uint64_t t0 = now_ns();
    /* Hand-roll the varargs call for each small n; libmpfr's mpfr_inits
     * walks the va_list until it sees the NULL sentinel. For n=0 we
     * skip the call entirely (passing only the sentinel triggers
     * -Wformat sentinel warnings; the contract is "no work"). */
    switch (n) {
        case 0:
            /* No real handles to init; nothing to do. The TS port
             * mirrors this: mpfr_inits(0n) returns 0n. */
            break;
        case 1: mpfr_inits(slots[0], (mpfr_ptr)0); break;
        case 2: mpfr_inits(slots[0], slots[1], (mpfr_ptr)0); break;
        case 3: mpfr_inits(slots[0], slots[1], slots[2], (mpfr_ptr)0); break;
        case 4: mpfr_inits(slots[0], slots[1], slots[2], slots[3], (mpfr_ptr)0); break;
        case 5: mpfr_inits(slots[0], slots[1], slots[2], slots[3], slots[4],
                           (mpfr_ptr)0); break;
        case 6: mpfr_inits(slots[0], slots[1], slots[2], slots[3], slots[4],
                           slots[5], (mpfr_ptr)0); break;
        case 7: mpfr_inits(slots[0], slots[1], slots[2], slots[3], slots[4],
                           slots[5], slots[6], (mpfr_ptr)0); break;
        case 8: mpfr_inits(slots[0], slots[1], slots[2], slots[3], slots[4],
                           slots[5], slots[6], slots[7], (mpfr_ptr)0); break;
        case 9: mpfr_inits(slots[0], slots[1], slots[2], slots[3], slots[4],
                           slots[5], slots[6], slots[7], slots[8],
                           (mpfr_ptr)0); break;
        case 10: mpfr_inits(slots[0], slots[1], slots[2], slots[3], slots[4],
                            slots[5], slots[6], slots[7], slots[8], slots[9],
                            (mpfr_ptr)0); break;
        case 11: mpfr_inits(slots[0], slots[1], slots[2], slots[3], slots[4],
                            slots[5], slots[6], slots[7], slots[8], slots[9],
                            slots[10], (mpfr_ptr)0); break;
        case 12: mpfr_inits(slots[0], slots[1], slots[2], slots[3], slots[4],
                            slots[5], slots[6], slots[7], slots[8], slots[9],
                            slots[10], slots[11], (mpfr_ptr)0); break;
    }
    const uint64_t elapsed = now_ns() - t0;
    /* Clear the inited handles so libmpfr doesn't leak. */
    for (unsigned int i = 0; i < n; ++i) mpfr_clear(slots[i]);

    jl_begin(out, tag);
    jl_kv_u64(out, 1, "n", (uint64_t)n);
    jl_end_inputs(out);
    jl_output_scalar_u64(out, (uint64_t)n);  /* count-passthrough */
    jl_finish(out, elapsed);
}

int main(void) {
    FILE *out = stdout;

    /* happy: 20 -- small n in [0, 12]. */
    emit_case(out, "happy", 0);
    emit_case(out, "happy", 1);
    emit_case(out, "happy", 2);
    emit_case(out, "happy", 3);
    emit_case(out, "happy", 4);
    emit_case(out, "happy", 5);
    emit_case(out, "happy", 6);
    emit_case(out, "happy", 7);
    emit_case(out, "happy", 8);
    emit_case(out, "happy", 9);
    emit_case(out, "happy", 10);
    emit_case(out, "happy", 11);
    emit_case(out, "happy", 12);
    /* Repeat a few to fill out to 20. */
    emit_case(out, "happy", 0);
    emit_case(out, "happy", 1);
    emit_case(out, "happy", 2);
    emit_case(out, "happy", 3);
    emit_case(out, "happy", 4);
    emit_case(out, "happy", 5);
    emit_case(out, "happy", 6);

    /* edge: 30 -- boundary cases (0, 1) and full coverage of 0..MAX_N. */
    for (unsigned int n = 0; n <= 12; ++n) emit_case(out, "edge", n);
    for (unsigned int n = 0; n <= 11; ++n) emit_case(out, "edge", n);
    emit_case(out, "edge", 0);
    emit_case(out, "edge", 12);
    emit_case(out, "edge", 1);
    emit_case(out, "edge", 0);
    emit_case(out, "edge", 12);

    /* adversarial: 12 -- repeated zero, repeated max, identity check. */
    emit_case(out, "adversarial", 0);
    emit_case(out, "adversarial", 12);
    emit_case(out, "adversarial", 1);
    emit_case(out, "adversarial", 11);
    emit_case(out, "adversarial", 2);
    emit_case(out, "adversarial", 10);
    emit_case(out, "adversarial", 3);
    emit_case(out, "adversarial", 9);
    emit_case(out, "adversarial", 4);
    emit_case(out, "adversarial", 8);
    emit_case(out, "adversarial", 5);
    emit_case(out, "adversarial", 7);

    /* fuzz: 50 random n in [0, 12]. */
    {
        xs64_t rng;
        xs64_seed(&rng, 0xCAFEFADEBABE0BADULL);
        for (int rep = 0; rep < 50; ++rep) {
            const unsigned int n = (unsigned int)xs64_below(&rng, 13);
            emit_case(out, "fuzz", n);
        }
    }

    /* mined: 5 -- patterns from mpfr/tests/tinits.c L31-L34
     * (a, b, c three-element init pattern; tinits.c uses n=3 twice). */
    emit_case(out, "mined", 3);
    emit_case(out, "mined", 3);
    emit_case(out, "mined", 0);
    emit_case(out, "mined", 1);
    emit_case(out, "mined", 2);

    return 0;
}
