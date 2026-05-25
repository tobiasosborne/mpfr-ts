/*
 * golden_driver.c -- Golden master for GMP's mpn_zero.
 *
 * Signature
 * ---------
 *
 *   void mpn_zero(mp_limb_t *rp, mp_size_t n);
 *
 * Sets rp[0..n) to zero. The output is fully determined by n alone --
 * there is no input data dependency. Ref: GMP manual §8.3.
 *
 * Wire format
 * -----------
 *
 *   {"tag":"<class>","inputs":{"n":<int>},
 *    "output":{"result":["0",...,"0"]},"time_ns":<n>}
 *
 * Tag distribution -- given that mpn_zero is a pure n -> n-zeros map,
 * the "interesting" surface is the size sweep. Rule 7 minimums are met
 * by parameterizing over n:
 *
 *   happy        :  20  (small n in {1..20})
 *   edge         :  30  (n in {0, 1, 2, 3, ..., 25, 32, 48, 64, ...})
 *   adversarial  :  10  (boundary n values: 0, 1, 63, 64, 100, 256, ...)
 *   fuzz         :  50  (random n in [0, 64])
 *
 * mined absent -- substrate carve-out.
 */
#include "common.h"

#include <assert.h>

#if GMP_NUMB_BITS != 64
#  error "mpn_zero golden_driver requires GMP_NUMB_BITS == 64"
#endif

#define MAX_N 256

static inline void emit_case(FILE *out, const char *tag, size_t n) {
    assert(n <= MAX_N);

    mp_limb_t result[MAX_N + 1];
    /* Pre-fill with non-zero sentinel so we'd detect a partial zero. */
    for (size_t i = 0; i <= MAX_N; ++i) result[i] = 0xDEADBEEFCAFEBABEULL;
    const uint64_t t0 = now_ns();
    mpn_zero(result, (mp_size_t)n);
    const uint64_t elapsed = now_ns() - t0;

    jl_begin(out, tag);
    jl_kv_int(out, 1, "n", (int)n);
    jl_end_inputs(out);

    jl_output_begin_object(out);
    jl_kv_limbs(out, 1, "result", result, n);
    jl_output_end_object(out);

    jl_finish(out, elapsed);
}

int main(void) {
    FILE *out = stdout;

    /* happy: 20 -- n in {1..20} */
    for (size_t n = 1; n <= 20; ++n) emit_case(out, "happy", n);

    /* edge: 30 -- specific boundary sizes */
    {
        const size_t edge_sizes[] = {
            0, 1, 2, 3, 4, 5, 6, 7, 8, 9,
            10, 12, 15, 16, 17, 20, 24, 25, 30, 31,
            32, 33, 48, 49, 50, 60, 63, 64, 80, 100
        };
        for (size_t i = 0; i < 30; ++i) {
            emit_case(out, "edge", edge_sizes[i]);
        }
    }

    /* adversarial: 10 -- boundary + large sizes */
    {
        const size_t adv_sizes[] = {
            0, 1, 63, 64, 65, 128, 129, 200, 255, 256
        };
        for (size_t i = 0; i < 10; ++i) {
            emit_case(out, "adversarial", adv_sizes[i]);
        }
    }

    /* fuzz: 50 -- random n in [0, 64] */
    {
        xs64_t rng;
        xs64_seed(&rng, 0xF022000DEEDULL);
        for (int rep = 0; rep < 50; ++rep) {
            const size_t n = (size_t)xs64_below(&rng, 65);
            emit_case(out, "fuzz", n);
        }
    }

    return 0;
}
