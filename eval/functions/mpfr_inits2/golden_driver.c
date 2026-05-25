/*
 * golden_driver.c -- Golden master for MPFR's mpfr_inits2.
 *
 * C signature: void mpfr_inits2(mpfr_prec_t p, mpfr_ptr x, ..., NULL).
 * Ref: mpfr/src/inits2.c L40-L64.
 *
 * TS port: (prec: bigint, n: bigint) -> bigint (count-passthrough).
 *
 * Wire: {"inputs":{"prec":"<dec>","n":"<dec>"},"output":"<n-as-dec>"}.
 */
#include "common.h"
#include <assert.h>
#include <inttypes.h>

#define MAX_N 12
#define TS_PREC_MAX ((uint64_t)((1ULL << 31) - 257ULL))

static inline void emit_case(FILE *out, const char *tag,
                             uint64_t prec, unsigned int n) {
    assert(prec >= 1 && prec <= TS_PREC_MAX);
    assert(n <= MAX_N);
    mpfr_t slots[MAX_N];
    const uint64_t t0 = now_ns();
    switch (n) {
        case 0:
            /* No handles to init; skip the libmpfr call entirely
             * (passing only the sentinel triggers -Wformat sentinel). */
            break;
        case 1: mpfr_inits2((mpfr_prec_t)prec, slots[0], (mpfr_ptr)0); break;
        case 2: mpfr_inits2((mpfr_prec_t)prec, slots[0], slots[1], (mpfr_ptr)0); break;
        case 3: mpfr_inits2((mpfr_prec_t)prec, slots[0], slots[1], slots[2],
                            (mpfr_ptr)0); break;
        case 4: mpfr_inits2((mpfr_prec_t)prec, slots[0], slots[1], slots[2],
                            slots[3], (mpfr_ptr)0); break;
        case 5: mpfr_inits2((mpfr_prec_t)prec, slots[0], slots[1], slots[2],
                            slots[3], slots[4], (mpfr_ptr)0); break;
        case 6: mpfr_inits2((mpfr_prec_t)prec, slots[0], slots[1], slots[2],
                            slots[3], slots[4], slots[5], (mpfr_ptr)0); break;
        case 7: mpfr_inits2((mpfr_prec_t)prec, slots[0], slots[1], slots[2],
                            slots[3], slots[4], slots[5], slots[6],
                            (mpfr_ptr)0); break;
        case 8: mpfr_inits2((mpfr_prec_t)prec, slots[0], slots[1], slots[2],
                            slots[3], slots[4], slots[5], slots[6], slots[7],
                            (mpfr_ptr)0); break;
        case 9: mpfr_inits2((mpfr_prec_t)prec, slots[0], slots[1], slots[2],
                            slots[3], slots[4], slots[5], slots[6], slots[7],
                            slots[8], (mpfr_ptr)0); break;
        case 10: mpfr_inits2((mpfr_prec_t)prec, slots[0], slots[1], slots[2],
                             slots[3], slots[4], slots[5], slots[6], slots[7],
                             slots[8], slots[9], (mpfr_ptr)0); break;
        case 11: mpfr_inits2((mpfr_prec_t)prec, slots[0], slots[1], slots[2],
                             slots[3], slots[4], slots[5], slots[6], slots[7],
                             slots[8], slots[9], slots[10], (mpfr_ptr)0); break;
        case 12: mpfr_inits2((mpfr_prec_t)prec, slots[0], slots[1], slots[2],
                             slots[3], slots[4], slots[5], slots[6], slots[7],
                             slots[8], slots[9], slots[10], slots[11],
                             (mpfr_ptr)0); break;
    }
    const uint64_t elapsed = now_ns() - t0;
    for (unsigned int i = 0; i < n; ++i) mpfr_clear(slots[i]);

    jl_begin(out, tag);
    jl_kv_u64(out, 1, "prec", prec);
    jl_kv_u64(out, 0, "n", (uint64_t)n);
    jl_end_inputs(out);
    jl_output_scalar_u64(out, (uint64_t)n);
    jl_finish(out, elapsed);
}

int main(void) {
    FILE *out = stdout;

    /* happy: 20 -- common (prec, n) pairs. */
    emit_case(out, "happy", 53, 3);
    emit_case(out, "happy", 53, 1);
    emit_case(out, "happy", 53, 2);
    emit_case(out, "happy", 53, 4);
    emit_case(out, "happy", 53, 0);
    emit_case(out, "happy", 24, 3);
    emit_case(out, "happy", 64, 3);
    emit_case(out, "happy", 100, 2);
    emit_case(out, "happy", 113, 2);
    emit_case(out, "happy", 200, 3);
    emit_case(out, "happy", 256, 4);
    emit_case(out, "happy", 53, 5);
    emit_case(out, "happy", 53, 8);
    emit_case(out, "happy", 53, 12);
    emit_case(out, "happy", 1, 1);
    emit_case(out, "happy", 1, 2);
    emit_case(out, "happy", 1, 12);
    emit_case(out, "happy", 1024, 1);
    emit_case(out, "happy", 1024, 6);
    emit_case(out, "happy", 53, 7);

    /* edge: 30 -- prec=1, prec near MAX, n at boundaries. */
    emit_case(out, "edge", 1, 0);
    emit_case(out, "edge", 1, 1);
    emit_case(out, "edge", 1, 12);
    emit_case(out, "edge", 2, 1);
    emit_case(out, "edge", 2, 12);
    emit_case(out, "edge", 53, 0);
    emit_case(out, "edge", 53, 12);
    emit_case(out, "edge", 64, 0);
    emit_case(out, "edge", 64, 12);
    emit_case(out, "edge", 100, 0);
    emit_case(out, "edge", 100, 12);
    emit_case(out, "edge", 1000, 0);
    emit_case(out, "edge", 1000, 12);
    emit_case(out, "edge", 1000000, 0);
    emit_case(out, "edge", 1000000, 1);
    emit_case(out, "edge", 1000000, 12);
    emit_case(out, "edge", 24, 0);
    emit_case(out, "edge", 24, 12);
    emit_case(out, "edge", 1, 0);
    emit_case(out, "edge", 1, 0);
    emit_case(out, "edge", 53, 11);
    emit_case(out, "edge", 53, 10);
    emit_case(out, "edge", 53, 9);
    emit_case(out, "edge", 53, 6);
    emit_case(out, "edge", 113, 12);
    emit_case(out, "edge", 200, 12);
    emit_case(out, "edge", 256, 12);
    emit_case(out, "edge", 24, 7);
    emit_case(out, "edge", 32, 5);
    emit_case(out, "edge", 53, 4);

    /* adversarial: 12 -- alternating (prec, n) emphasising both axes. */
    emit_case(out, "adversarial", 1, 0);
    emit_case(out, "adversarial", 1000000, 12);
    emit_case(out, "adversarial", 1, 12);
    emit_case(out, "adversarial", 1000000, 0);
    emit_case(out, "adversarial", 53, 0);
    emit_case(out, "adversarial", 53, 12);
    emit_case(out, "adversarial", 64, 6);
    emit_case(out, "adversarial", 200, 6);
    emit_case(out, "adversarial", 2, 11);
    emit_case(out, "adversarial", 100000, 11);
    emit_case(out, "adversarial", 24, 3);
    emit_case(out, "adversarial", 113, 3);

    /* fuzz: 50 random (prec, n). */
    {
        xs64_t rng;
        xs64_seed(&rng, 0xFADEBABE0CAFE0F0ULL);
        const uint64_t precs[10] = { 1, 2, 24, 32, 53, 64, 100, 113, 200, 1024 };
        for (int rep = 0; rep < 50; ++rep) {
            const uint64_t prec = precs[xs64_below(&rng, 10)];
            const unsigned int n = (unsigned int)xs64_below(&rng, 13);
            emit_case(out, "fuzz", prec, n);
        }
    }

    /* mined: 5 -- patterns from mpfr/tests/tinits.c L33
     * (mpfr_inits2(200, a, b, c, NULL) pattern). */
    emit_case(out, "mined", 200, 3);
    emit_case(out, "mined", 200, 3);
    emit_case(out, "mined", 53, 0);
    emit_case(out, "mined", 53, 1);
    emit_case(out, "mined", 1, 2);

    return 0;
}
