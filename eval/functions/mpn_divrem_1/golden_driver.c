/*
 * golden_driver.c -- Golden master for GMP's mpn_divrem_1.
 *
 * Signature
 * ---------
 *
 *   mp_limb_t mpn_divrem_1(mp_limb_t *qp,
 *                          mp_size_t qxn,
 *                          mp_limb_t *np,   (* mp_srcptr in newer GMPs *)
 *                          mp_size_t nn,
 *                          mp_limb_t d0);
 *
 * Divides the (nn + qxn)-limb non-negative integer formed by appending
 * qxn zero limbs to the LSB end of np[0..nn) by the single-limb divisor
 * d0. Writes the (nn + qxn)-limb quotient to qp; returns the remainder
 * (a single limb in [0, d0)).
 *
 * Numerator interpretation
 * ------------------------
 *
 *   dividend_int = (np_as_int << (qxn * GMP_NUMB_BITS))
 *
 * where np_as_int = sum_{i=0..nn-1} np[i] * 2^(64*i). The qxn extension
 * is "zero limbs prepended at the LSB end" -- equivalent to multiplying
 * the np-derived integer by 2^(64*qxn).
 *
 * Limb arrays are stored LITTLE-ENDIAN by limb index. GMP section 8.3.
 *
 * Wire format
 * -----------
 *
 *   {"tag":"<class>",
 *    "inputs":{"qxn":<int>,
 *              "np":["<dec>",...],
 *              "nn":<int>,
 *              "d0":"<dec>"},
 *    "output":{"q":["<dec>",...],"r":"<dec>"},
 *    "time_ns":<n>}
 *
 *   - `qxn`, `nn`: raw JS numbers (small ints, fit comfortably in 32 bits).
 *   - `np`: little-endian limb array, decimal-string per limb.
 *   - `d0`: u64 decimal string; nonzero.
 *   - `q`: little-endian quotient limb array of length nn + qxn.
 *   - `r`: u64 remainder, decimal string, always < d0.
 *
 * Tag distribution (CLAUDE.md Rule 7)
 * -----------------------------------
 *
 *   happy        :  21  (small nn in {1..5}, qxn = 0, varied d0)
 *   edge         :  32  (boundary patterns: d0 = 1, d0 = 2^63, np = 0,
 *                        nn = 0 with qxn > 0, etc.)
 *   adversarial  :  12  (max-limb numerators, max-limb divisors,
 *                        cross-limb quotient cases)
 *   fuzz         :  60  (PRNG-driven across nn in {1..16}, qxn in {0..4})
 *   mined        :   0  (mpfr/tests/ has no isolatable mpn_divrem_1 test
 *                        driver; it is exercised internally via mpfr_set_z
 *                        / mpfr_get_str. Rule 7 carve-out applies.)
 *
 * Build via eval/golden_master/build.sh.
 *
 * Ref: GMP manual section 8.3 -- mpn_divrem_1.
 * Ref: mpfr/src/mpfr-mini-gmp.c L177-L213 -- shim semantics the TS port mirrors.
 */
#include "common.h"

#include <assert.h>
#include <gmp.h>
#include <inttypes.h>

#if GMP_NUMB_BITS != 64
#  error "mpn_divrem_1 golden_driver requires GMP_NUMB_BITS == 64"
#endif

#define MAX_NN 64
#define MAX_QXN 8

/* Emit one case. Buffers are sized for the worst case (nn = MAX_NN,
 * qxn = MAX_QXN). The quotient has length nn + qxn. */
static inline void emit_case(FILE *out,
                             const char *tag,
                             int qxn,
                             const mp_limb_t *np,
                             int nn,
                             mp_limb_t d0) {
    assert(nn >= 0 && nn <= MAX_NN);
    assert(qxn >= 0 && qxn <= MAX_QXN);
    assert(d0 != 0);

    /* Mutable copy of np (GMP's mpn_divrem_1 declares np as const in
     * newer revisions, but the prototype in /usr/include/gmp.h on this
     * machine is `mp_srcptr` -- so np is read-only at the API level.
     * No mutable copy needed.) */

    mp_limb_t qp[MAX_NN + MAX_QXN] = {0};

    const uint64_t t0 = now_ns();
    const mp_limb_t r = mpn_divrem_1(qp, (mp_size_t)qxn, np, (mp_size_t)nn, d0);
    const uint64_t elapsed = now_ns() - t0;

    jl_begin(out, tag);
    jl_kv_int(out, 1, "qxn", qxn);
    jl_kv_limbs(out, 0, "np", np, (size_t)nn);
    jl_kv_int(out, 0, "nn", nn);
    jl_kv_u64(out, 0, "d0", (uint64_t)d0);
    jl_end_inputs(out);

    jl_output_begin_object(out);
    jl_kv_limbs(out, 1, "q", qp, (size_t)(nn + qxn));
    jl_kv_u64(out, 0, "r", (uint64_t)r);
    jl_output_end_object(out);

    jl_finish(out, elapsed);
}

int main(void) {
    FILE *out = stdout;

    /* ============================================================== */
    /* happy: 21 -- small nn in {1..5}, qxn = 0, varied d0.           */
    /* ============================================================== */
    {
        xs64_t rng;
        xs64_seed(&rng, 0xDA1A0DEADBEEFULL);
        for (int nn = 1; nn <= 5; ++nn) {
            for (int rep = 0; rep < 4; ++rep) {
                mp_limb_t np[MAX_NN];
                for (int i = 0; i < nn; ++i) np[i] = xs64_next(&rng);
                mp_limb_t d0;
                do { d0 = xs64_next(&rng); } while (d0 == 0);
                emit_case(out, "happy", 0, np, nn, d0);
            }
        }
        /* One last extra: nn=5 with a tidy divisor. */
        {
            mp_limb_t np[5] = {1, 2, 3, 4, 5};
            emit_case(out, "happy", 0, np, 5, 1000ULL);
        }
    }

    /* ============================================================== */
    /* edge: 32 -- boundary patterns.                                  */
    /* ============================================================== */
    {
        const mp_limb_t M = ~(mp_limb_t)0;
        /* d0 = 1: quotient equals dividend, remainder = 0. */
        { mp_limb_t np[1] = {42}; emit_case(out, "edge", 0, np, 1, 1ULL); }
        { mp_limb_t np[2] = {0, 42}; emit_case(out, "edge", 0, np, 2, 1ULL); }
        { mp_limb_t np[3] = {1, 2, 3}; emit_case(out, "edge", 0, np, 3, 1ULL); }
        { mp_limb_t np[4] = {M, M, M, M}; emit_case(out, "edge", 0, np, 4, 1ULL); }

        /* np = 0: quotient = 0, remainder = 0. */
        { mp_limb_t np[1] = {0}; emit_case(out, "edge", 0, np, 1, 7ULL); }
        { mp_limb_t np[2] = {0, 0}; emit_case(out, "edge", 0, np, 2, 13ULL); }
        { mp_limb_t np[3] = {0, 0, 0}; emit_case(out, "edge", 0, np, 3, 99ULL); }

        /* nn = 1, np[0] < d0: quotient = 0, remainder = np[0]. */
        { mp_limb_t np[1] = {5}; emit_case(out, "edge", 0, np, 1, 17ULL); }
        { mp_limb_t np[1] = {0}; emit_case(out, "edge", 0, np, 1, 1000000ULL); }

        /* d0 = 2: simple bit shift. */
        { mp_limb_t np[1] = {7}; emit_case(out, "edge", 0, np, 1, 2ULL); }
        { mp_limb_t np[2] = {0, 1}; emit_case(out, "edge", 0, np, 2, 2ULL); }
        { mp_limb_t np[3] = {M, M, M}; emit_case(out, "edge", 0, np, 3, 2ULL); }

        /* d0 = max u64. */
        { mp_limb_t np[1] = {1}; emit_case(out, "edge", 0, np, 1, M); }
        { mp_limb_t np[2] = {1, 1}; emit_case(out, "edge", 0, np, 2, M); }
        { mp_limb_t np[3] = {M, M, M}; emit_case(out, "edge", 0, np, 3, M); }

        /* d0 with MSB set (single limb, classical "normalized" divisor). */
        { mp_limb_t np[2] = {1, M}; emit_case(out, "edge", 0, np, 2, (mp_limb_t)1 << 63); }
        { mp_limb_t np[3] = {0, 0, 1}; emit_case(out, "edge", 0, np, 3, (mp_limb_t)1 << 63); }

        /* qxn > 0 with nn > 0: dividend shifted up by qxn limbs. */
        { mp_limb_t np[1] = {5}; emit_case(out, "edge", 1, np, 1, 7ULL); }
        { mp_limb_t np[1] = {5}; emit_case(out, "edge", 2, np, 1, 7ULL); }
        { mp_limb_t np[2] = {1, 2}; emit_case(out, "edge", 1, np, 2, 17ULL); }
        { mp_limb_t np[2] = {1, 2}; emit_case(out, "edge", 3, np, 2, 17ULL); }
        { mp_limb_t np[3] = {M, M, M}; emit_case(out, "edge", 1, np, 3, M); }
        { mp_limb_t np[3] = {1, 0, 1}; emit_case(out, "edge", 4, np, 3, 3ULL); }

        /* Powers-of-2 divisor (bit shift cases). */
        { mp_limb_t np[2] = {0xDEADBEEFCAFEBABEULL, 0x123456789ABCDEF0ULL};
          emit_case(out, "edge", 0, np, 2, (mp_limb_t)1 << 4); }
        { mp_limb_t np[2] = {0xDEADBEEFCAFEBABEULL, 0x123456789ABCDEF0ULL};
          emit_case(out, "edge", 0, np, 2, (mp_limb_t)1 << 16); }
        { mp_limb_t np[2] = {0xDEADBEEFCAFEBABEULL, 0x123456789ABCDEF0ULL};
          emit_case(out, "edge", 0, np, 2, (mp_limb_t)1 << 32); }
        { mp_limb_t np[2] = {0xDEADBEEFCAFEBABEULL, 0x123456789ABCDEF0ULL};
          emit_case(out, "edge", 0, np, 2, (mp_limb_t)1 << 48); }

        /* Single-limb dividend = max u64 against various divisors. */
        { mp_limb_t np[1] = {M}; emit_case(out, "edge", 0, np, 1, 2ULL); }
        { mp_limb_t np[1] = {M}; emit_case(out, "edge", 0, np, 1, 3ULL); }
        { mp_limb_t np[1] = {M}; emit_case(out, "edge", 0, np, 1, M); }

        /* Carefully chosen: dividend = d0 exactly -> q = 1, r = 0. */
        { mp_limb_t np[1] = {12345}; emit_case(out, "edge", 0, np, 1, 12345ULL); }
        { mp_limb_t np[1] = {M}; emit_case(out, "edge", 0, np, 1, M); }
    }

    /* ============================================================== */
    /* adversarial: 12 -- cross-limb quotient stress and max divisor.  */
    /* ============================================================== */
    {
        const mp_limb_t M = ~(mp_limb_t)0;
        /* All-MAX numerator (largest n-limb integer) divided by various d0. */
        { mp_limb_t np[3] = {M, M, M}; emit_case(out, "adversarial", 0, np, 3, 2ULL); }
        { mp_limb_t np[3] = {M, M, M}; emit_case(out, "adversarial", 0, np, 3, 7ULL); }
        { mp_limb_t np[3] = {M, M, M}; emit_case(out, "adversarial", 0, np, 3, M); }
        { mp_limb_t np[5] = {M, M, M, M, M}; emit_case(out, "adversarial", 0, np, 5, M - 1ULL); }
        { mp_limb_t np[8] = {M, M, M, M, M, M, M, M}; emit_case(out, "adversarial", 0, np, 8, M); }

        /* All-MSB pattern -- forces the per-limb 2-bit-leading division. */
        { mp_limb_t np[4] = {(mp_limb_t)1 << 63, (mp_limb_t)1 << 63,
                              (mp_limb_t)1 << 63, (mp_limb_t)1 << 63};
          emit_case(out, "adversarial", 0, np, 4, (mp_limb_t)1 << 63); }

        /* Maximum qxn, single-limb np. */
        { mp_limb_t np[1] = {1}; emit_case(out, "adversarial", MAX_QXN, np, 1, 3ULL); }
        { mp_limb_t np[1] = {M}; emit_case(out, "adversarial", MAX_QXN, np, 1, 0x1000000000000001ULL); }

        /* Divisor = MSB-only single limb (classical normalized case). */
        { mp_limb_t np[6] = {0xC0DECAFEBABE1234ULL, 0xDEADBEEF12345678ULL,
                              0xFACE0FF1CEBA5E11ULL, 0xBAADF00DDEFEC8EDULL,
                              0xC001D00DC0FFEE15ULL, 0xCAFEBABE0BADBEEFULL};
          emit_case(out, "adversarial", 0, np, 6, (mp_limb_t)1 << 63); }

        /* Numerator just larger than d0: q = 1, r = small. */
        { mp_limb_t np[1] = {12346}; emit_case(out, "adversarial", 0, np, 1, 12345ULL); }
        { mp_limb_t np[2] = {1, 1}; emit_case(out, "adversarial", 0, np, 2,
                                              0x8000000000000000ULL); }
        { mp_limb_t np[2] = {0, 1}; emit_case(out, "adversarial", 0, np, 2, 2ULL); }
    }

    /* ============================================================== */
    /* fuzz: 60 -- PRNG-driven nn in [1, 16], qxn in [0, 4].          */
    /* ============================================================== */
    {
        xs64_t rng;
        xs64_seed(&rng, 0xF112BABE1F4CULL);
        for (int i = 0; i < 60; ++i) {
            int nn = (int)(xs64_below(&rng, 16) + 1);   /* nn in [1, 16] */
            int qxn = (int)xs64_below(&rng, 5);          /* qxn in [0, 4] */
            mp_limb_t np[MAX_NN];
            for (int k = 0; k < nn; ++k) np[k] = xs64_next(&rng);
            mp_limb_t d0;
            do { d0 = xs64_next(&rng); } while (d0 == 0);
            emit_case(out, "fuzz", qxn, np, nn, d0);
        }
    }

    return 0;
}
