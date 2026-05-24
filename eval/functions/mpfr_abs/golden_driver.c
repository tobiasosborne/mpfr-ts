/*
 * golden_driver.c — Golden master for MPFR's mpfr_abs.
 *
 * C signature
 * -----------
 *
 *   int mpfr_abs(mpfr_t rop, mpfr_srcptr op, mpfr_rnd_t rnd);
 *
 *   Sets rop to |op|, rounded per rnd to rop's precision; returns the
 *   ternary flag. Ref: mpfr/src/set.c L83–L96 (the out-of-line a==b
 *   case) and mpfr/src/mpfr.h L970 (the macro form, which expands to
 *   mpfr_set4(a, b, r, 1)).
 *
 * Divergence from C → TS
 * ----------------------
 *
 * Same as mpfr_neg's driver — see that file for the rationale. We drive
 * the C side at the SAME prec as the TS port and emit Result-shaped
 * output via jl_output_result.
 *
 * Wire format
 * -----------
 *
 *   {"tag":"<class>",
 *    "inputs":{"x":<MPFR-record>,"prec":"<decimal>","rnd":"RND[NZUDA]"},
 *    "output":{"value":<MPFR-record>,"ternary":<-1|0|1>},
 *    "time_ns":<n>}
 *
 * Tag distribution (CLAUDE.md Rule 7 minimums)
 * --------------------------------------------
 *
 *   happy        :  ~25
 *   edge         :  ~50  (all kinds × all 5 rnd modes + signed-zero
 *                         collapse + ±Inf → +Inf)
 *   adversarial  :  ~30  (prec change + RNDN tie + sign collapse with
 *                         carry-out at MSB)
 *   fuzz         :   60  (PRNG seed 0xAB501DAB501DULL — hex-pun visible;
 *                         xs64_seed normalises if 0)
 *   mined        :   5   (from mpfr/tests/tabs.c)
 *
 * Ref: mpfr/src/set.c — C reference (mpfr_set4 + the abs alias handler).
 * Ref: src/ops/abs.ts — the production port.
 * Ref: src/internal/mpfr/round_raw.ts — shared rounding substrate.
 */
#include "common.h"

#include <assert.h>
#include <float.h>
#include <inttypes.h>
#include <math.h>
#include <string.h>

#if GMP_NUMB_BITS != 64
#  error "mpfr_abs golden_driver requires GMP_NUMB_BITS == 64"
#endif

#define TS_PREC_MAX ((uint64_t)((1ULL << 31) - 257ULL))
#define TS_PREC_MIN ((uint64_t)1)

static inline void emit_case(FILE *out, const char *tag,
                             mpfr_srcptr x, uint64_t prec, mpfr_rnd_t rnd) {
    assert(prec >= TS_PREC_MIN && prec <= TS_PREC_MAX);
    mpfr_t rop;
    mpfr_init2(rop, (mpfr_prec_t)prec);

    const uint64_t t0 = now_ns();
    const int ternary = mpfr_abs(rop, x, rnd);
    const uint64_t elapsed = now_ns() - t0;

    jl_begin(out, tag);
    jl_kv_mpfr(out, 1, "x", x);
    jl_kv_u64(out, 0, "prec", prec);
    jl_kv_rnd(out, 0, "rnd", rnd);
    jl_end_inputs(out);
    jl_output_result(out, rop, ternary);
    jl_finish(out, elapsed);

    mpfr_clear(rop);
}

static inline void init_from_double(mpfr_ptr x, double d, uint64_t prec) {
    mpfr_init2(x, (mpfr_prec_t)prec); mpfr_set_d(x, d, MPFR_RNDN);
}
static inline void init_nan(mpfr_ptr x, uint64_t prec) {
    mpfr_init2(x, (mpfr_prec_t)prec); mpfr_set_nan(x);
}
static inline void init_pos_inf(mpfr_ptr x, uint64_t prec) {
    mpfr_init2(x, (mpfr_prec_t)prec); mpfr_set_inf(x, 1);
}
static inline void init_neg_inf(mpfr_ptr x, uint64_t prec) {
    mpfr_init2(x, (mpfr_prec_t)prec); mpfr_set_inf(x, -1);
}
static inline void init_pos_zero(mpfr_ptr x, uint64_t prec) {
    mpfr_init2(x, (mpfr_prec_t)prec); mpfr_set_zero(x, 1);
}
static inline void init_neg_zero(mpfr_ptr x, uint64_t prec) {
    mpfr_init2(x, (mpfr_prec_t)prec); mpfr_set_zero(x, -1);
}

static inline void emit_d(FILE *out, const char *tag,
                          double d, uint64_t srcp, uint64_t dstp,
                          mpfr_rnd_t rnd) {
    mpfr_t x; init_from_double(x, d, srcp);
    emit_case(out, tag, x, dstp, rnd);
    mpfr_clear(x);
}

int main(void) {
    FILE *out = stdout;
    const mpfr_rnd_t RNDS[5] = {MPFR_RNDN, MPFR_RNDZ, MPFR_RNDU, MPFR_RNDD, MPFR_RNDA};

    /* ============================================================== */
    /* happy: ~25                                                     */
    /* ============================================================== */
    {
        /* Already-positive (abs is identity on value). */
        emit_d(out, "happy",  1.0,   53,  53, MPFR_RNDN);
        emit_d(out, "happy",  2.0,   53,  53, MPFR_RNDN);
        emit_d(out, "happy",  3.14,  53,  53, MPFR_RNDN);
        emit_d(out, "happy",  10.0,  53,  53, MPFR_RNDN);
        emit_d(out, "happy",  100.0, 53,  53, MPFR_RNDN);
        emit_d(out, "happy",  1e9,   53,  53, MPFR_RNDN);
        emit_d(out, "happy",  1.0,   53,  53, MPFR_RNDN);

        /* Negatives (the actual transformation). */
        emit_d(out, "happy", -1.0,   53,  53, MPFR_RNDN);
        emit_d(out, "happy", -2.0,   53,  53, MPFR_RNDN);
        emit_d(out, "happy", -3.14,  53,  53, MPFR_RNDN);
        emit_d(out, "happy", -10.0,  53,  53, MPFR_RNDN);
        emit_d(out, "happy", -100.0, 53,  53, MPFR_RNDN);
        emit_d(out, "happy", -1e9,   53,  53, MPFR_RNDN);

        /* Common precs. */
        emit_d(out, "happy",  3.14,  24,  24, MPFR_RNDN);
        emit_d(out, "happy",  3.14,  64,  64, MPFR_RNDN);
        emit_d(out, "happy",  3.14, 100, 100, MPFR_RNDN);
        emit_d(out, "happy",  3.14, 200, 200, MPFR_RNDN);
        emit_d(out, "happy", -3.14,  24,  24, MPFR_RNDN);
        emit_d(out, "happy", -3.14,  64,  64, MPFR_RNDN);
        emit_d(out, "happy", -3.14, 100, 100, MPFR_RNDN);
        emit_d(out, "happy", -3.14, 200, 200, MPFR_RNDN);

        /* Magnitudes. */
        emit_d(out, "happy", -1.5e100, 53, 53, MPFR_RNDN);
        emit_d(out, "happy", -1.5e-100, 53, 53, MPFR_RNDN);
        emit_d(out, "happy",  6.022e23, 53, 53, MPFR_RNDN);
        emit_d(out, "happy", -6.022e23, 53, 53, MPFR_RNDN);
    }

    /* ============================================================== */
    /* edge: ~50                                                      */
    /* ============================================================== */
    {
        /* (1-5) NaN × 5 rnd modes — NaN. */
        for (int i = 0; i < 5; ++i) {
            mpfr_t x; init_nan(x, 53);
            emit_case(out, "edge", x, 53, RNDS[i]);
            mpfr_clear(x);
        }
        /* (6-10) +Inf × 5 — +Inf (identity). */
        for (int i = 0; i < 5; ++i) {
            mpfr_t x; init_pos_inf(x, 53);
            emit_case(out, "edge", x, 53, RNDS[i]);
            mpfr_clear(x);
        }
        /* (11-15) -Inf × 5 — +Inf (sign collapse). */
        for (int i = 0; i < 5; ++i) {
            mpfr_t x; init_neg_inf(x, 53);
            emit_case(out, "edge", x, 53, RNDS[i]);
            mpfr_clear(x);
        }
        /* (16-20) +0 × 5 — +0 (identity). */
        for (int i = 0; i < 5; ++i) {
            mpfr_t x; init_pos_zero(x, 53);
            emit_case(out, "edge", x, 53, RNDS[i]);
            mpfr_clear(x);
        }
        /* (21-25) -0 × 5 — +0 (signed-zero collapse). */
        for (int i = 0; i < 5; ++i) {
            mpfr_t x; init_neg_zero(x, 53);
            emit_case(out, "edge", x, 53, RNDS[i]);
            mpfr_clear(x);
        }

        /* (26-30) positive normal × 5 — identity, same prec. */
        for (int i = 0; i < 5; ++i) {
            mpfr_t x; init_from_double(x, 3.14, 53);
            emit_case(out, "edge", x, 53, RNDS[i]);
            mpfr_clear(x);
        }
        /* (31-35) negative normal × 5 — sign collapse, same prec. */
        for (int i = 0; i < 5; ++i) {
            mpfr_t x; init_from_double(x, -3.14, 53);
            emit_case(out, "edge", x, 53, RNDS[i]);
            mpfr_clear(x);
        }

        /* (36) Inf with prec mismatch. */
        {
            mpfr_t x; init_neg_inf(x, 64);
            emit_case(out, "edge", x, 200, MPFR_RNDN);
            mpfr_clear(x);
        }
        /* (37) -0 with prec mismatch — still +0. */
        {
            mpfr_t x; init_neg_zero(x, 64);
            emit_case(out, "edge", x, 200, MPFR_RNDD);
            mpfr_clear(x);
        }
        /* (38) +0 with prec mismatch. */
        {
            mpfr_t x; init_pos_zero(x, 64);
            emit_case(out, "edge", x, 200, MPFR_RNDU);
            mpfr_clear(x);
        }

        /* (39-43) Same-prec at prec=1 (boundary). Both signs. */
        for (int i = 0; i < 5; ++i) {
            mpfr_t x; init_from_double(x, -1.0, 1);
            emit_case(out, "edge", x, 1, RNDS[i]);
            mpfr_clear(x);
        }

        /* (44) Same-prec at very large prec. */
        {
            mpfr_t x; init_from_double(x, -1.5, 200);
            emit_case(out, "edge", x, 200, MPFR_RNDN);
            mpfr_clear(x);
        }
        /* (45) Same-prec at prec=2. */
        {
            mpfr_t x; init_from_double(x, -1.5, 2);
            emit_case(out, "edge", x, 2, MPFR_RNDN);
            mpfr_clear(x);
        }

        /* (46-50) Mixed prec — small-to-large lossless pad. */
        emit_d(out, "edge", -1.0,  10,  53, MPFR_RNDN);
        emit_d(out, "edge",  1.0,  10,  53, MPFR_RNDN);
        emit_d(out, "edge", -1.5,  53, 100, MPFR_RNDN);
        emit_d(out, "edge",  1.5,  53, 100, MPFR_RNDN);
        emit_d(out, "edge", -3.0,   3, 200, MPFR_RNDD);
    }

    /* ============================================================== */
    /* adversarial: ~30 — prec change forcing rounding; sign collapse */
    /* combined with rounding direction; carry-out at MSB.            */
    /* ============================================================== */
    {
        const char *patterns[] = {
            "1.1011E0",
            "1.0101E0",
            "1.1111E0",
            "1.0001E10",
            "1.1100E-50",
            "1.0000000000000000000000000000000000000000000000000001E0",
        };
        const size_t n_pat = sizeof(patterns) / sizeof(patterns[0]);

        /* For each pattern: NEGATIVE source at prec=53, target prec=3,
         * all 5 rnd modes. The negation forces the broken port (which
         * uses sign=-1 for rounding direction) to misroute RNDU/RNDD;
         * the correct port uses sign=+1 since the result is positive. */
        for (size_t p = 0; p < n_pat; ++p) {
            for (int r = 0; r < 5; ++r) {
                mpfr_t x;
                mpfr_init2(x, 53);
                /* Prepend "-" to make negative. */
                char buf[200];
                buf[0] = '-';
                size_t n = strlen(patterns[p]);
                memcpy(buf + 1, patterns[p], n + 1);
                mpfr_set_str(x, buf, 2, MPFR_RNDN);
                emit_case(out, "adversarial", x, 3, RNDS[r]);
                mpfr_clear(x);
            }
        }

        /* RNDN tie on positive source. */
        {
            mpfr_t x; mpfr_init2(x, 4); mpfr_set_str(x, "1.110E0", 2, MPFR_RNDN);
            emit_case(out, "adversarial", x, 2, MPFR_RNDN);
            mpfr_clear(x);
        }
        /* RNDN tie on negative source — collapses to positive. */
        {
            mpfr_t x; mpfr_init2(x, 4); mpfr_set_str(x, "-1.110E0", 2, MPFR_RNDN);
            emit_case(out, "adversarial", x, 2, MPFR_RNDN);
            mpfr_clear(x);
        }

        /* Carry-out at MSB on negative source rounded under RNDU. */
        {
            mpfr_t x; mpfr_init2(x, 4); mpfr_set_str(x, "-1.111E0", 2, MPFR_RNDN);
            emit_case(out, "adversarial", x, 3, MPFR_RNDU);
            mpfr_clear(x);
        }
        /* Same on positive source. */
        {
            mpfr_t x; mpfr_init2(x, 4); mpfr_set_str(x, "1.111E0", 2, MPFR_RNDN);
            emit_case(out, "adversarial", x, 3, MPFR_RNDU);
            mpfr_clear(x);
        }
    }

    /* ============================================================== */
    /* fuzz: 60                                                       */
    /* ============================================================== */
    {
        xs64_t rng;
        xs64_seed(&rng, 0xAB501DAB501DULL);
        const uint64_t precs[6] = { 1, 2, 53, 64, 100, 200 };

        int emitted = 0;
        while (emitted < 60) {
            const uint64_t bits = xs64_next(&rng);
            const uint64_t exp = (bits >> 52) & 0x7FF;
            if (exp == 0x7FF) continue;

            double d;
            memcpy(&d, &bits, sizeof d);

            const uint64_t srcp = precs[xs64_below(&rng, 6)];
            const uint64_t dstp = precs[xs64_below(&rng, 6)];
            const mpfr_rnd_t rnd = RNDS[xs64_below(&rng, 5)];

            mpfr_t x;
            init_from_double(x, d, srcp);
            emit_case(out, "fuzz", x, dstp, rnd);
            mpfr_clear(x);
            emitted++;
        }
    }

    /* ============================================================== */
    /* mined: 5 — from mpfr/tests/tabs.c                              */
    /* ============================================================== */
    {
        /* tabs.c — abs(NaN) = NaN. */
        {
            mpfr_t x; init_nan(x, 53);
            emit_case(out, "mined", x, 53, MPFR_RNDN);
            mpfr_clear(x);
        }
        /* tabs.c — abs(-0) = +0. */
        {
            mpfr_t x; init_neg_zero(x, 53);
            emit_case(out, "mined", x, 53, MPFR_RNDN);
            mpfr_clear(x);
        }
        /* tabs.c — abs(-Inf) = +Inf. */
        {
            mpfr_t x; init_neg_inf(x, 53);
            emit_case(out, "mined", x, 53, MPFR_RNDN);
            mpfr_clear(x);
        }
        /* tabs.c — abs(-1.0) = +1.0. */
        {
            mpfr_t x; init_from_double(x, -1.0, 53);
            emit_case(out, "mined", x, 53, MPFR_RNDN);
            mpfr_clear(x);
        }
        /* tabs.c — abs with prec change rounding. */
        {
            mpfr_t x; mpfr_init2(x, 53); mpfr_set_d(x, -5.0/3.0, MPFR_RNDN);
            emit_case(out, "mined", x, 4, MPFR_RNDN);
            mpfr_clear(x);
        }
    }

    return 0;
}
