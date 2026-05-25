/*
 * golden_driver.c -- Golden master for GMP's mpn_divrem.
 *
 * Signature
 * ---------
 *
 *   mp_limb_t mpn_divrem(mp_limb_t *qp, mp_size_t qn,
 *                        mp_limb_t *np, mp_size_t nn,
 *                        const mp_limb_t *dp, mp_size_t dn);
 *
 * Divides the nn-limb numerator np by the dn-limb divisor dp; writes
 * the (nn-dn)-limb quotient to qp; writes the dn-limb remainder back
 * into np[0..dn); returns the high quotient limb (0 or 1 in practice).
 *
 * Per the MPFR mini-gmp shim (mpfr/src/mpfr-mini-gmp.c L216-L243):
 *   - qn must be 0 (caller-supplied "extra fractional quotient limbs"
 *     count; the shim asserts qn == 0).
 *   - qn is then internally set to nn - dn.
 *
 * Real GMP's mpn_divrem requires the divisor's high limb to have its
 * MSB set (normalized divisor). The golden_driver enforces this in
 * every case.
 *
 * Wire format
 * -----------
 *
 *   {"tag":"<class>",
 *    "inputs":{"qn":0,
 *              "np":["<dec>",...],
 *              "nn":<int>,
 *              "dp":["<dec>",...],
 *              "dn":<int>},
 *    "output":{"q":["<dec>",...],
 *              "qHigh":"<dec>",
 *              "r":["<dec>",...]},
 *    "time_ns":<n>}
 *
 *   - All limb arrays are little-endian, decimal-string per limb.
 *   - q has length nn - dn; r has length dn.
 *   - qHigh is the high quotient limb returned by mpn_divrem (0 or
 *     occasionally 1).
 *
 * Tag distribution (CLAUDE.md Rule 7)
 * -----------------------------------
 *
 *   happy        :  20  (small nn, dn in {1..4} with normalized dp)
 *   edge         :  31  (boundary patterns: nn = dn, qHigh = 1, all-max,
 *                        single-limb divisor variants)
 *   adversarial  :  12  (large quotient, max-limb numerator and divisor,
 *                        cancelling carry chains)
 *   fuzz         :  60  (PRNG-driven nn in [2, 12], dn in [1, nn], with
 *                        normalized dp[dn-1])
 *   mined        :   0  (mpfr/tests/ has no isolatable mpn_divrem test
 *                        driver; Rule 7 carve-out applies.)
 *
 * Build via eval/golden_master/build.sh.
 *
 * Ref: GMP manual section 8.3 -- mpn_divrem.
 * Ref: mpfr/src/mpfr-mini-gmp.c L216-L243 -- shim semantics.
 */
#include "common.h"

#include <assert.h>
#include <gmp.h>
#include <inttypes.h>

#if GMP_NUMB_BITS != 64
#  error "mpn_divrem golden_driver requires GMP_NUMB_BITS == 64"
#endif

#define MAX_NN 32
#define MAX_DN 16

/* Force the divisor's high limb to have MSB set (normalize). */
static inline void normalize(mp_limb_t *dp, int dn) {
    assert(dn >= 1);
    dp[dn - 1] |= (mp_limb_t)1 << 63;
}

/* Emit one case. Mutates internal copies of np / dp because mpn_divrem
 * writes the remainder back into np. */
static inline void emit_case(FILE *out,
                             const char *tag,
                             const mp_limb_t *np_in,
                             int nn,
                             const mp_limb_t *dp_in,
                             int dn) {
    assert(nn >= dn && dn >= 1);
    assert(nn <= MAX_NN && dn <= MAX_DN);
    /* Normalized divisor invariant. */
    assert((dp_in[dn - 1] >> 63) & 1ULL);

    mp_limb_t np[MAX_NN];
    mp_limb_t dp[MAX_DN];
    for (int i = 0; i < nn; ++i) np[i] = np_in[i];
    for (int i = 0; i < dn; ++i) dp[i] = dp_in[i];

    mp_limb_t qp[MAX_NN] = {0};  /* quotient buffer; needs nn-dn limbs */

    const uint64_t t0 = now_ns();
    const mp_limb_t qHigh = mpn_divrem(qp, (mp_size_t)0, np, (mp_size_t)nn, dp, (mp_size_t)dn);
    const uint64_t elapsed = now_ns() - t0;

    jl_begin(out, tag);
    jl_kv_int(out, 1, "qn", 0);
    jl_kv_limbs(out, 0, "np", np_in, (size_t)nn);
    jl_kv_int(out, 0, "nn", nn);
    jl_kv_limbs(out, 0, "dp", dp_in, (size_t)dn);
    jl_kv_int(out, 0, "dn", dn);
    jl_end_inputs(out);

    jl_output_begin_object(out);
    jl_kv_limbs(out, 1, "q", qp, (size_t)(nn - dn));
    jl_kv_u64(out, 0, "qHigh", (uint64_t)qHigh);
    /* Remainder is in np[0..dn) post-call (mpn_divrem mutated it). */
    jl_kv_limbs(out, 0, "r", np, (size_t)dn);
    jl_output_end_object(out);

    jl_finish(out, elapsed);
}

int main(void) {
    FILE *out = stdout;

    /* ============================================================== */
    /* happy: 20 -- small nn, dn in {1..4} with normalized dp.        */
    /* ============================================================== */
    {
        xs64_t rng;
        xs64_seed(&rng, 0xD1DDED0FFEEDCAFEULL);
        for (int dn = 1; dn <= 4; ++dn) {
            for (int rep = 0; rep < 5; ++rep) {
                int nn = dn + (int)xs64_below(&rng, 4) + 1;  /* nn in [dn+1, dn+4] */
                if (nn > MAX_NN) nn = MAX_NN;
                mp_limb_t np[MAX_NN], dp[MAX_DN];
                for (int i = 0; i < nn; ++i) np[i] = xs64_next(&rng);
                for (int i = 0; i < dn; ++i) dp[i] = xs64_next(&rng);
                normalize(dp, dn);
                emit_case(out, "happy", np, nn, dp, dn);
            }
        }
    }

    /* ============================================================== */
    /* edge: 31 -- boundary patterns.                                 */
    /* ============================================================== */
    {
        const mp_limb_t M = ~(mp_limb_t)0;
        const mp_limb_t MSB = (mp_limb_t)1 << 63;

        /* nn = dn = 1: single-limb / single-limb. */
        { mp_limb_t np[1] = {5}; mp_limb_t dp[1] = {MSB | 7ULL};
          emit_case(out, "edge", np, 1, dp, 1); }
        { mp_limb_t np[1] = {MSB}; mp_limb_t dp[1] = {MSB};
          emit_case(out, "edge", np, 1, dp, 1); }
        { mp_limb_t np[1] = {M}; mp_limb_t dp[1] = {MSB};
          emit_case(out, "edge", np, 1, dp, 1); }

        /* np = 0 -- quotient = 0, remainder = 0. */
        { mp_limb_t np[3] = {0, 0, 0}; mp_limb_t dp[2] = {1, MSB};
          emit_case(out, "edge", np, 3, dp, 2); }
        { mp_limb_t np[4] = {0, 0, 0, 0}; mp_limb_t dp[1] = {MSB};
          emit_case(out, "edge", np, 4, dp, 1); }

        /* nn = dn (square-shape): quotient is 0 or 1 limb, qHigh = 0 or 1. */
        { mp_limb_t np[2] = {7, MSB | 1ULL}; mp_limb_t dp[2] = {1, MSB};
          emit_case(out, "edge", np, 2, dp, 2); }
        { mp_limb_t np[2] = {1, MSB}; mp_limb_t dp[2] = {7, MSB};
          emit_case(out, "edge", np, 2, dp, 2); }
        { mp_limb_t np[3] = {0, 0, MSB}; mp_limb_t dp[3] = {1, 2, MSB};
          emit_case(out, "edge", np, 3, dp, 3); }
        { mp_limb_t np[4] = {0, 0, 0, MSB}; mp_limb_t dp[4] = {0, 0, 0, MSB};
          emit_case(out, "edge", np, 4, dp, 4); }

        /* nn = dn + 1: quotient is exactly 1 limb. */
        { mp_limb_t np[2] = {0, M}; mp_limb_t dp[1] = {MSB};
          emit_case(out, "edge", np, 2, dp, 1); }
        { mp_limb_t np[3] = {1, 2, M}; mp_limb_t dp[2] = {M, MSB};
          emit_case(out, "edge", np, 3, dp, 2); }
        { mp_limb_t np[4] = {1, 2, 3, M}; mp_limb_t dp[3] = {M, M, MSB};
          emit_case(out, "edge", np, 4, dp, 3); }

        /* nn >> dn: quotient is much larger than divisor. */
        { mp_limb_t np[5] = {M, M, M, M, M}; mp_limb_t dp[1] = {MSB};
          emit_case(out, "edge", np, 5, dp, 1); }
        { mp_limb_t np[6] = {M, M, M, M, M, M}; mp_limb_t dp[2] = {M, MSB};
          emit_case(out, "edge", np, 6, dp, 2); }
        { mp_limb_t np[8] = {M, M, M, M, M, M, M, M}; mp_limb_t dp[3] = {1, 2, MSB};
          emit_case(out, "edge", np, 8, dp, 3); }

        /* Divisor = MSB-only (smallest normalized divisor). */
        { mp_limb_t np[3] = {0xDEADBEEFCAFEBABEULL, 0x123456789ABCDEF0ULL,
                              0xC0DECAFEBABE0123ULL};
          mp_limb_t dp[1] = {MSB};
          emit_case(out, "edge", np, 3, dp, 1); }
        { mp_limb_t np[5] = {0x1111ULL, 0x2222ULL, 0x3333ULL, 0x4444ULL, 0x5555ULL};
          mp_limb_t dp[2] = {0, MSB};
          emit_case(out, "edge", np, 5, dp, 2); }

        /* Carefully chosen: numerator = divisor exactly -> q = 1, r = 0. */
        { mp_limb_t np[2] = {1, MSB}; mp_limb_t dp[2] = {1, MSB};
          emit_case(out, "edge", np, 2, dp, 2); }
        { mp_limb_t np[3] = {M, M, MSB | 0x77ULL}; mp_limb_t dp[3] = {M, M, MSB | 0x77ULL};
          emit_case(out, "edge", np, 3, dp, 3); }

        /* Numerator = divisor + 1: q = 1, r = 1 (or similar small r). */
        { mp_limb_t np[2] = {2, MSB}; mp_limb_t dp[2] = {1, MSB};
          emit_case(out, "edge", np, 2, dp, 2); }

        /* High-precision divisor with mantissa zeros. */
        { mp_limb_t np[5] = {0xCAFEULL, 0xBABEULL, 0xC0DEULL, 0xFACEULL, 0xBADBULL};
          mp_limb_t dp[3] = {0, 0, MSB | 1ULL};
          emit_case(out, "edge", np, 5, dp, 3); }

        /* Wide span: nn = 16, dn = 1. */
        {
            mp_limb_t np[16];
            for (int i = 0; i < 16; ++i) np[i] = (mp_limb_t)(0x1111111100000000ULL + (uint64_t)i);
            mp_limb_t dp[1] = {MSB | 1ULL};
            emit_case(out, "edge", np, 16, dp, 1);
        }
        /* Wide span: nn = 16, dn = 8. */
        {
            mp_limb_t np[16];
            for (int i = 0; i < 16; ++i) np[i] = (mp_limb_t)(0xDEC0DECAFEBA0000ULL + (uint64_t)i);
            mp_limb_t dp[8] = {1, 2, 3, 4, 5, 6, 7, MSB};
            emit_case(out, "edge", np, 16, dp, 8);
        }

        /* dn = 1, max divisor. */
        { mp_limb_t np[4] = {0xAAAAAAAAAAAAAAAAULL, 0xBBBBBBBBBBBBBBBBULL,
                              0xCCCCCCCCCCCCCCCCULL, 0xDDDDDDDDDDDDDDDDULL};
          mp_limb_t dp[1] = {M};
          emit_case(out, "edge", np, 4, dp, 1); }

        /* Multi-limb divisor with all-MSB middle. */
        { mp_limb_t np[6] = {1, 2, 3, 4, 5, 6}; mp_limb_t dp[3] = {0, M, MSB};
          emit_case(out, "edge", np, 6, dp, 3); }

        /* Symmetric pattern: identical limbs in np and dp prefix. */
        { mp_limb_t np[4] = {0x1234567890ABCDEFULL, 0x1234567890ABCDEFULL,
                              0x1234567890ABCDEFULL, 0x1234567890ABCDEFULL};
          mp_limb_t dp[2] = {0x1234567890ABCDEFULL, MSB | 0x1234567890ABCDEFULL};
          emit_case(out, "edge", np, 4, dp, 2); }

        /* dn = 1, divisor = 0x8000000000000001. */
        { mp_limb_t np[3] = {M, M, M}; mp_limb_t dp[1] = {MSB | 1ULL};
          emit_case(out, "edge", np, 3, dp, 1); }
        { mp_limb_t np[5] = {0, 0, 0, 0, M}; mp_limb_t dp[1] = {MSB | 1ULL};
          emit_case(out, "edge", np, 5, dp, 1); }
        { mp_limb_t np[3] = {1, 0, 0}; mp_limb_t dp[2] = {0, MSB};
          emit_case(out, "edge", np, 3, dp, 2); }
        { mp_limb_t np[2] = {1, 0}; mp_limb_t dp[1] = {MSB | 0xFEDCBA9876543210ULL};
          emit_case(out, "edge", np, 2, dp, 1); }
    }

    /* ============================================================== */
    /* adversarial: 12 -- max-limb numerator and divisor, large       */
    /* quotient stress.                                                */
    /* ============================================================== */
    {
        const mp_limb_t M = ~(mp_limb_t)0;
        const mp_limb_t MSB = (mp_limb_t)1 << 63;

        /* All-MAX numerator, MSB-only divisor: quotient = nearly all bits set. */
        { mp_limb_t np[8] = {M, M, M, M, M, M, M, M};
          mp_limb_t dp[1] = {MSB};
          emit_case(out, "adversarial", np, 8, dp, 1); }
        /* All-MAX numerator, max divisor: q = 1, r = small. */
        { mp_limb_t np[8] = {M, M, M, M, M, M, M, M};
          mp_limb_t dp[4] = {M, M, M, M};
          emit_case(out, "adversarial", np, 8, dp, 4); }
        /* All-MAX numerator, all-MAX divisor (square): q = 1, r = 0. */
        { mp_limb_t np[6] = {M, M, M, M, M, M};
          mp_limb_t dp[6] = {M, M, M, M, M, M};
          emit_case(out, "adversarial", np, 6, dp, 6); }

        /* MSB-alternating numerator. */
        { mp_limb_t np[6] = {MSB, 0, MSB, 0, MSB, 0};
          mp_limb_t dp[2] = {1, MSB | 1ULL};
          emit_case(out, "adversarial", np, 6, dp, 2); }
        /* Mid-quotient-limb cancellation. */
        { mp_limb_t np[5] = {0, 0, 0, 0, M};
          mp_limb_t dp[2] = {M, MSB};
          emit_case(out, "adversarial", np, 5, dp, 2); }

        /* Long quotient stress: nn = 16, dn = 2. */
        {
            mp_limb_t np[16];
            for (int i = 0; i < 16; ++i) np[i] = M - (mp_limb_t)i;
            mp_limb_t dp[2] = {1, MSB};
            emit_case(out, "adversarial", np, 16, dp, 2);
        }
        /* nn = 12, dn = 6: quotient is ~6 limbs. */
        {
            mp_limb_t np[12];
            for (int i = 0; i < 12; ++i) np[i] = (mp_limb_t)0xDEADBEEFCAFEBABEULL + (mp_limb_t)i;
            mp_limb_t dp[6] = {1, 2, 3, 4, 5, MSB};
            emit_case(out, "adversarial", np, 12, dp, 6);
        }
        /* Pathological: every middle limb of np is 0, top limb forces a quotient. */
        { mp_limb_t np[6] = {0, 0, 0, 0, 0, M};
          mp_limb_t dp[1] = {MSB | 7ULL};
          emit_case(out, "adversarial", np, 6, dp, 1); }

        /* qHigh = 1 case: numerator just exceeds divisor << (nn-dn)*64. */
        { mp_limb_t np[3] = {1, 0, MSB | 1ULL};
          mp_limb_t dp[2] = {0, MSB};
          emit_case(out, "adversarial", np, 3, dp, 2); }

        /* Small divisor relative to numerator: produces a near-full
         * quotient with qHigh = 1 likely. */
        { mp_limb_t np[5] = {0, 0, 0, 0, MSB | 7ULL};
          mp_limb_t dp[1] = {MSB | 1ULL};
          emit_case(out, "adversarial", np, 5, dp, 1); }

        /* All-zero quotient stress: numerator < divisor (but nn == dn). */
        { mp_limb_t np[3] = {0, 0, MSB};
          mp_limb_t dp[3] = {1, 0, M};
          emit_case(out, "adversarial", np, 3, dp, 3); }
        /* Equal lengths, numerator slightly larger than divisor. */
        { mp_limb_t np[4] = {0, 0, 0, M};
          mp_limb_t dp[4] = {0, 0, 0, MSB | 1ULL};
          emit_case(out, "adversarial", np, 4, dp, 4); }
    }

    /* ============================================================== */
    /* fuzz: 60 -- PRNG-driven nn in [2, 12], dn in [1, nn], with     */
    /* normalized dp[dn-1].                                            */
    /* ============================================================== */
    {
        xs64_t rng;
        xs64_seed(&rng, 0xFEEDDEADBEEFC0DEULL);
        int emitted = 0;
        while (emitted < 60) {
            int nn = 2 + (int)xs64_below(&rng, 11);  /* nn in [2, 12] */
            int dn = 1 + (int)xs64_below(&rng, (uint64_t)nn);  /* dn in [1, nn] */
            if (dn > MAX_DN) dn = MAX_DN;
            mp_limb_t np[MAX_NN], dp[MAX_DN];
            for (int i = 0; i < nn; ++i) np[i] = xs64_next(&rng);
            for (int i = 0; i < dn; ++i) dp[i] = xs64_next(&rng);
            normalize(dp, dn);
            emit_case(out, "fuzz", np, nn, dp, dn);
            emitted++;
        }
    }

    return 0;
}
