/*
 * golden_driver.c -- Golden master for GMP's mpn_tdiv_qr.
 *
 * Wire format:
 *   {"tag":"<class>",
 *    "inputs":{"qxn":0,
 *              "np":["<dec>",...],
 *              "nn":<int>,
 *              "dp":["<dec>",...],
 *              "dn":<int>},
 *    "output":{"q":["<dec>",...],  // length nn-dn+1
 *              "r":["<dec>",...]}, // length dn
 *    "time_ns":<n>}
 *
 *   - All limb arrays little-endian, decimal-string per limb.
 *   - q has length nn - dn + 1 (mpn_tdiv_qr writes the high quotient
 *     limb directly into qp, unlike mpn_divrem which returns it separately).
 *
 * Tag distribution (Rule 7): happy 20, edge 30, adv 12, fuzz 50, mined 0
 *   (no isolatable test source under mpfr/tests/ for mpn_tdiv_qr).
 *
 * Ref: GMP section 8.3.
 * Ref: mpfr/src/mpfr-mini-gmp.c L246-L262 -- shim semantics.
 */
#include "common.h"
#include <assert.h>
#include <gmp.h>
#include <inttypes.h>
#include <string.h>

#if GMP_NUMB_BITS != 64
#  error "mpn_tdiv_qr golden_driver requires GMP_NUMB_BITS == 64"
#endif

#define MAX_NN 32
#define MAX_DN 16

static inline void normalize(mp_limb_t *dp, int dn) {
    assert(dn >= 1);
    dp[dn - 1] |= (mp_limb_t)1 << 63;
}

static inline void emit_case(FILE *out,
                              const char *tag,
                              const mp_limb_t *np_in,
                              int nn,
                              const mp_limb_t *dp_in,
                              int dn) {
    assert(nn >= dn && dn >= 1);
    assert(nn <= MAX_NN && dn <= MAX_DN);
    assert((dp_in[dn - 1] >> 63) & 1ULL);

    mp_limb_t np[MAX_NN];
    mp_limb_t dp[MAX_DN];
    for (int i = 0; i < nn; ++i) np[i] = np_in[i];
    for (int i = 0; i < dn; ++i) dp[i] = dp_in[i];

    mp_limb_t qp[MAX_NN] = {0};  /* needs nn - dn + 1 limbs */
    mp_limb_t rp[MAX_DN] = {0};

    const uint64_t t0 = now_ns();
    mpn_tdiv_qr(qp, rp, (mp_size_t)0, np, (mp_size_t)nn, dp, (mp_size_t)dn);
    const uint64_t elapsed = now_ns() - t0;

    jl_begin(out, tag);
    jl_kv_int(out, 1, "qxn", 0);
    jl_kv_limbs(out, 0, "np", np_in, (size_t)nn);
    jl_kv_int(out, 0, "nn", nn);
    jl_kv_limbs(out, 0, "dp", dp_in, (size_t)dn);
    jl_kv_int(out, 0, "dn", dn);
    jl_end_inputs(out);

    jl_output_begin_object(out);
    jl_kv_limbs(out, 1, "q", qp, (size_t)(nn - dn + 1));
    jl_kv_limbs(out, 0, "r", rp, (size_t)dn);
    jl_output_end_object(out);

    jl_finish(out, elapsed);
}

int main(void) {
    FILE *out = stdout;

    /* happy: 20 */
    {
        xs64_t rng;
        xs64_seed(&rng, 0xD1DDEDDABBADD00DULL);
        for (int dn = 1; dn <= 4; ++dn) {
            for (int rep = 0; rep < 5; ++rep) {
                int nn = dn + (int)xs64_below(&rng, 4) + 1;
                if (nn > MAX_NN) nn = MAX_NN;
                mp_limb_t np[MAX_NN], dp[MAX_DN];
                for (int i = 0; i < nn; ++i) np[i] = xs64_next(&rng);
                for (int i = 0; i < dn; ++i) dp[i] = xs64_next(&rng);
                normalize(dp, dn);
                emit_case(out, "happy", np, nn, dp, dn);
            }
        }
    }

    /* edge: 30 -- boundary patterns. */
    {
        const mp_limb_t M = ~(mp_limb_t)0;
        const mp_limb_t MSB = (mp_limb_t)1 << 63;

        { mp_limb_t np[1] = {5}; mp_limb_t dp[1] = {MSB | 7ULL};
          emit_case(out, "edge", np, 1, dp, 1); }
        { mp_limb_t np[1] = {MSB}; mp_limb_t dp[1] = {MSB};
          emit_case(out, "edge", np, 1, dp, 1); }
        { mp_limb_t np[1] = {M}; mp_limb_t dp[1] = {MSB};
          emit_case(out, "edge", np, 1, dp, 1); }
        { mp_limb_t np[3] = {0, 0, 0}; mp_limb_t dp[2] = {1, MSB};
          emit_case(out, "edge", np, 3, dp, 2); }
        { mp_limb_t np[4] = {0, 0, 0, 0}; mp_limb_t dp[1] = {MSB};
          emit_case(out, "edge", np, 4, dp, 1); }
        { mp_limb_t np[2] = {7, MSB | 1ULL}; mp_limb_t dp[2] = {1, MSB};
          emit_case(out, "edge", np, 2, dp, 2); }
        { mp_limb_t np[2] = {1, MSB}; mp_limb_t dp[2] = {7, MSB};
          emit_case(out, "edge", np, 2, dp, 2); }
        { mp_limb_t np[3] = {0, 0, MSB}; mp_limb_t dp[3] = {1, 2, MSB};
          emit_case(out, "edge", np, 3, dp, 3); }
        { mp_limb_t np[4] = {0, 0, 0, MSB}; mp_limb_t dp[4] = {0, 0, 0, MSB};
          emit_case(out, "edge", np, 4, dp, 4); }
        { mp_limb_t np[2] = {0, M}; mp_limb_t dp[1] = {MSB};
          emit_case(out, "edge", np, 2, dp, 1); }
        { mp_limb_t np[3] = {1, 2, M}; mp_limb_t dp[2] = {M, MSB};
          emit_case(out, "edge", np, 3, dp, 2); }
        { mp_limb_t np[4] = {1, 2, 3, M}; mp_limb_t dp[3] = {M, M, MSB};
          emit_case(out, "edge", np, 4, dp, 3); }
        { mp_limb_t np[5] = {M, M, M, M, M}; mp_limb_t dp[1] = {MSB};
          emit_case(out, "edge", np, 5, dp, 1); }
        { mp_limb_t np[6] = {M, M, M, M, M, M}; mp_limb_t dp[2] = {M, MSB};
          emit_case(out, "edge", np, 6, dp, 2); }
        { mp_limb_t np[8] = {M, M, M, M, M, M, M, M}; mp_limb_t dp[3] = {1, 2, MSB};
          emit_case(out, "edge", np, 8, dp, 3); }
        { mp_limb_t np[2] = {1, 0}; mp_limb_t dp[1] = {MSB};
          emit_case(out, "edge", np, 2, dp, 1); }
        { mp_limb_t np[3] = {1, 0, 0}; mp_limb_t dp[2] = {1, MSB};
          emit_case(out, "edge", np, 3, dp, 2); }
        { mp_limb_t np[4] = {M, M, M, 0}; mp_limb_t dp[2] = {1, MSB};
          emit_case(out, "edge", np, 4, dp, 2); }
        { mp_limb_t np[5] = {0, 0, 0, 0, MSB}; mp_limb_t dp[2] = {1, MSB};
          emit_case(out, "edge", np, 5, dp, 2); }
        { mp_limb_t np[3] = {M, M, MSB}; mp_limb_t dp[1] = {MSB};
          emit_case(out, "edge", np, 3, dp, 1); }
        { mp_limb_t np[3] = {1, 1, 1 | MSB}; mp_limb_t dp[2] = {1, MSB};
          emit_case(out, "edge", np, 3, dp, 2); }
        { mp_limb_t np[4] = {7, 11, 13, 17 | MSB}; mp_limb_t dp[2] = {3, MSB | 5ULL};
          emit_case(out, "edge", np, 4, dp, 2); }
        { mp_limb_t np[5] = {0, 0, 0, 0, 1 | MSB}; mp_limb_t dp[3] = {0, 0, MSB};
          emit_case(out, "edge", np, 5, dp, 3); }
        { mp_limb_t np[6] = {1, 2, 3, 4, 5, 6 | MSB}; mp_limb_t dp[3] = {7, 8, 9 | MSB};
          emit_case(out, "edge", np, 6, dp, 3); }
        { mp_limb_t np[2] = {M, MSB - 1}; mp_limb_t dp[1] = {MSB};
          emit_case(out, "edge", np, 2, dp, 1); }
        { mp_limb_t np[3] = {0, M, 0 | MSB}; mp_limb_t dp[1] = {MSB | 1ULL};
          emit_case(out, "edge", np, 3, dp, 1); }
        { mp_limb_t np[4] = {0, 0, M, 0 | MSB}; mp_limb_t dp[2] = {1, MSB};
          emit_case(out, "edge", np, 4, dp, 2); }
        { mp_limb_t np[5] = {0, 0, 0, M, 0 | MSB}; mp_limb_t dp[3] = {1, 2, MSB};
          emit_case(out, "edge", np, 5, dp, 3); }
        { mp_limb_t np[8] = {1, 2, 3, 4, 5, 6, 7, 8 | MSB}; mp_limb_t dp[4] = {9, 10, 11, MSB};
          emit_case(out, "edge", np, 8, dp, 4); }
        { mp_limb_t np[6] = {0, 0, 1, 0, 0, MSB}; mp_limb_t dp[3] = {1, 0, MSB};
          emit_case(out, "edge", np, 6, dp, 3); }
    }

    /* adversarial: 12 -- large quotients, max-limb cases. */
    {
        const mp_limb_t M = ~(mp_limb_t)0;
        const mp_limb_t MSB = (mp_limb_t)1 << 63;

        { mp_limb_t np[8] = {M, M, M, M, M, M, M, M};
          mp_limb_t dp[1] = {MSB};
          emit_case(out, "adversarial", np, 8, dp, 1); }
        { mp_limb_t np[8] = {M, M, M, M, M, M, M, M};
          mp_limb_t dp[2] = {M, MSB};
          emit_case(out, "adversarial", np, 8, dp, 2); }
        { mp_limb_t np[10] = {M, M, M, M, M, M, M, M, M, M};
          mp_limb_t dp[3] = {1, 1, MSB};
          emit_case(out, "adversarial", np, 10, dp, 3); }
        { mp_limb_t np[12] = {M, M, M, M, M, M, M, M, M, M, M, M};
          mp_limb_t dp[4] = {1, 2, 3, MSB};
          emit_case(out, "adversarial", np, 12, dp, 4); }
        { mp_limb_t np[16] = {M, M, M, M, M, M, M, M, M, M, M, M, M, M, M, M};
          mp_limb_t dp[1] = {MSB | 1};
          emit_case(out, "adversarial", np, 16, dp, 1); }
        { mp_limb_t np[16] = {M, M, M, M, M, M, M, M, M, M, M, M, M, M, M, M};
          mp_limb_t dp[8] = {M, M, M, M, M, M, M, MSB};
          emit_case(out, "adversarial", np, 16, dp, 8); }
        { mp_limb_t np[8] = {0, 0, 0, 0, 0, 0, 0, M};
          mp_limb_t dp[4] = {0, 0, 0, MSB};
          emit_case(out, "adversarial", np, 8, dp, 4); }
        { mp_limb_t np[6] = {M, 0, M, 0, M, 0 | MSB};
          mp_limb_t dp[3] = {M, 0, MSB};
          emit_case(out, "adversarial", np, 6, dp, 3); }
        { mp_limb_t np[12] = {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, MSB};
          mp_limb_t dp[6] = {0, 0, 0, 0, 0, MSB};
          emit_case(out, "adversarial", np, 12, dp, 6); }
        { mp_limb_t np[4] = {M, M, M, M};
          mp_limb_t dp[2] = {1, MSB};
          emit_case(out, "adversarial", np, 4, dp, 2); }
        { mp_limb_t np[16] = {M, 0, M, 0, M, 0, M, 0, M, 0, M, 0, M, 0, M, 0 | MSB};
          mp_limb_t dp[4] = {1, 2, 3, MSB};
          emit_case(out, "adversarial", np, 16, dp, 4); }
        { mp_limb_t np[2] = {0, MSB};
          mp_limb_t dp[2] = {1, MSB};
          emit_case(out, "adversarial", np, 2, dp, 2); }
    }

    /* fuzz: 50 */
    {
        xs64_t rng;
        xs64_seed(&rng, 0xFADEBEADBEEFFADEULL);
        for (int rep = 0; rep < 50; ++rep) {
            int dn = 1 + (int)xs64_below(&rng, MAX_DN);
            int nn = dn + (int)xs64_below(&rng, MAX_NN - dn);
            if (nn < dn) nn = dn;
            if (nn > MAX_NN) nn = MAX_NN;
            mp_limb_t np[MAX_NN], dp[MAX_DN];
            for (int i = 0; i < nn; ++i) np[i] = xs64_next(&rng);
            for (int i = 0; i < dn; ++i) dp[i] = xs64_next(&rng);
            normalize(dp, dn);
            emit_case(out, "fuzz", np, nn, dp, dn);
        }
    }

    /* mined: 0 -- no isolatable mpn_tdiv_qr test driver in mpfr/tests/. */

    return 0;
}
