/*
 * golden_driver.c — Golden master for MPFR's mpfr_min.
 *
 * C signature
 * -----------
 *
 *   int mpfr_min(mpfr_t rop, mpfr_srcptr x, mpfr_srcptr y,
 *                mpfr_rnd_t rnd);
 *
 *   Returns the ternary. IEEE 754-2008 minNum semantics — NaN x non-NaN
 *   returns the non-NaN; both NaN → NaN. Signed zero orders -0 < +0.
 *   Ref: mpfr/src/minmax.c L31–L57.
 *
 * Wire format
 * -----------
 *
 *   {"tag":"<class>",
 *    "inputs":{"a":<MPFR>,"b":<MPFR>,"prec":"<dec>","rnd":"RND[NZUDA]"},
 *    "output":{"value":<MPFR>,"ternary":<-1|0|1>},
 *    "time_ns":<n>}
 *
 * Tag distribution
 * ----------------
 *
 *   happy        : ~25
 *   edge         : ~50  (all 4-kind × all 5 rnd; signed-zero pairs;
 *                        NaN-handling combos; same-magnitude opposite
 *                        signs)
 *   adversarial  : ~30  (tie cases; prec change forces rounding;
 *                        broken port swap surfaces here)
 *   fuzz         :  60
 *   mined        :   5  (from mpfr/tests/tminmax.c)
 *
 * Refs
 * ----
 *   - mpfr/src/minmax.c — the C reference (both min and max).
 *   - src/ops/min.ts — the production port.
 */
#include "common.h"

#include <assert.h>
#include <inttypes.h>
#include <math.h>
#include <string.h>

#if GMP_NUMB_BITS != 64
#  error "mpfr_min golden_driver requires GMP_NUMB_BITS == 64"
#endif

#define TS_PREC_MAX ((uint64_t)((1ULL << 31) - 257ULL))
#define TS_PREC_MIN ((uint64_t)1)

static inline void emit_case(FILE *out, const char *tag,
                             mpfr_srcptr a, mpfr_srcptr b,
                             uint64_t prec, mpfr_rnd_t rnd) {
    assert(prec >= TS_PREC_MIN && prec <= TS_PREC_MAX);
    mpfr_t rop;
    mpfr_init2(rop, (mpfr_prec_t)prec);

    const uint64_t t0 = now_ns();
    const int ternary = mpfr_min(rop, a, b, rnd);
    const uint64_t elapsed = now_ns() - t0;

    jl_begin(out, tag);
    jl_kv_mpfr(out, 1, "a", a);
    jl_kv_mpfr(out, 0, "b", b);
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

static inline void emit_dd(FILE *out, const char *tag,
                           double ad, double bd,
                           uint64_t ap, uint64_t bp, uint64_t dp,
                           mpfr_rnd_t rnd) {
    mpfr_t a, b;
    init_from_double(a, ad, ap);
    init_from_double(b, bd, bp);
    emit_case(out, tag, a, b, dp, rnd);
    mpfr_clear(a); mpfr_clear(b);
}

int main(void) {
    FILE *out = stdout;
    const mpfr_rnd_t RNDS[5] = {MPFR_RNDN, MPFR_RNDZ, MPFR_RNDU, MPFR_RNDD, MPFR_RNDA};

    /* ============================================================== */
    /* happy: ~25                                                     */
    /* ============================================================== */
    {
        emit_dd(out, "happy", 1.0, 2.0, 53, 53, 53, MPFR_RNDN);
        emit_dd(out, "happy", 2.0, 1.0, 53, 53, 53, MPFR_RNDN);
        emit_dd(out, "happy", -1.0, 2.0, 53, 53, 53, MPFR_RNDN);
        emit_dd(out, "happy", 1.0, -2.0, 53, 53, 53, MPFR_RNDN);
        emit_dd(out, "happy", -1.0, -2.0, 53, 53, 53, MPFR_RNDN);
        emit_dd(out, "happy", 0.5, 1.5, 53, 53, 53, MPFR_RNDN);
        emit_dd(out, "happy", 10.0, 100.0, 53, 53, 53, MPFR_RNDN);
        emit_dd(out, "happy", 100.0, 10.0, 53, 53, 53, MPFR_RNDN);
        emit_dd(out, "happy", 1.5e10, 2.5e10, 53, 53, 53, MPFR_RNDN);
        emit_dd(out, "happy", 3.14, 2.71, 53, 53, 53, MPFR_RNDN);
        emit_dd(out, "happy", 2.71, 3.14, 53, 53, 53, MPFR_RNDN);

        /* Equal values — result is `a` per the C reference on tie. */
        emit_dd(out, "happy", 1.0, 1.0, 53, 53, 53, MPFR_RNDN);
        emit_dd(out, "happy", -1.0, -1.0, 53, 53, 53, MPFR_RNDN);

        /* Common precs. */
        emit_dd(out, "happy", 3.14, 2.71, 24, 24, 24, MPFR_RNDN);
        emit_dd(out, "happy", 3.14, 2.71, 64, 64, 64, MPFR_RNDN);
        emit_dd(out, "happy", 3.14, 2.71, 100, 100, 100, MPFR_RNDN);
        emit_dd(out, "happy", 3.14, 2.71, 200, 200, 200, MPFR_RNDN);

        /* Negatives. */
        emit_dd(out, "happy", -3.14, -2.71, 53, 53, 53, MPFR_RNDN);
        emit_dd(out, "happy", -100.0, 0.001, 53, 53, 53, MPFR_RNDN);
        emit_dd(out, "happy", 1.5e100, 1.5e-100, 53, 53, 53, MPFR_RNDN);
        emit_dd(out, "happy", -1.5e100, -1.5e-100, 53, 53, 53, MPFR_RNDN);

        /* Signed-zero shouldn't appear in happy (it's an edge case). */
        emit_dd(out, "happy", 6.022e23, -6.022e23, 53, 53, 53, MPFR_RNDN);
        emit_dd(out, "happy", 1.0e-300, 1.0e-200, 53, 53, 53, MPFR_RNDN);
        emit_dd(out, "happy", -1.0e-300, -1.0e-200, 53, 53, 53, MPFR_RNDN);
        emit_dd(out, "happy", 42.0, 42.0001, 53, 53, 53, MPFR_RNDN);
    }

    /* ============================================================== */
    /* edge: ~50                                                      */
    /* ============================================================== */
    {
        /* (1-5) Both NaN × 5 rnd modes → NaN. */
        for (int i = 0; i < 5; ++i) {
            mpfr_t a, b;
            init_nan(a, 53); init_nan(b, 53);
            emit_case(out, "edge", a, b, 53, RNDS[i]);
            mpfr_clear(a); mpfr_clear(b);
        }

        /* (6-10) NaN, 0 — returns 0 (non-NaN wins). */
        for (int i = 0; i < 5; ++i) {
            mpfr_t a, b;
            init_nan(a, 53); init_pos_zero(b, 53);
            emit_case(out, "edge", a, b, 53, RNDS[i]);
            mpfr_clear(a); mpfr_clear(b);
        }

        /* (11-15) 0, NaN — symmetric. */
        for (int i = 0; i < 5; ++i) {
            mpfr_t a, b;
            init_pos_zero(a, 53); init_nan(b, 53);
            emit_case(out, "edge", a, b, 53, RNDS[i]);
            mpfr_clear(a); mpfr_clear(b);
        }

        /* (16-19) Signed-zero ordering: min(+0, -0) = -0, min(-0, +0) = -0,
         * min(+0, +0) = +0, min(-0, -0) = -0. RNDN. */
        {
            mpfr_t a, b;
            init_pos_zero(a, 53); init_neg_zero(b, 53);
            emit_case(out, "edge", a, b, 53, MPFR_RNDN);
            mpfr_clear(a); mpfr_clear(b);
        }
        {
            mpfr_t a, b;
            init_neg_zero(a, 53); init_pos_zero(b, 53);
            emit_case(out, "edge", a, b, 53, MPFR_RNDN);
            mpfr_clear(a); mpfr_clear(b);
        }
        {
            mpfr_t a, b;
            init_pos_zero(a, 53); init_pos_zero(b, 53);
            emit_case(out, "edge", a, b, 53, MPFR_RNDN);
            mpfr_clear(a); mpfr_clear(b);
        }
        {
            mpfr_t a, b;
            init_neg_zero(a, 53); init_neg_zero(b, 53);
            emit_case(out, "edge", a, b, 53, MPFR_RNDN);
            mpfr_clear(a); mpfr_clear(b);
        }

        /* (20-21) +Inf vs finite. */
        {
            mpfr_t a, b;
            init_pos_inf(a, 53); init_from_double(b, -12.0, 53);
            emit_case(out, "edge", a, b, 53, MPFR_RNDN);
            mpfr_clear(a); mpfr_clear(b);
        }
        {
            mpfr_t a, b;
            init_from_double(a, -12.0, 53); init_pos_inf(b, 53);
            emit_case(out, "edge", a, b, 53, MPFR_RNDN);
            mpfr_clear(a); mpfr_clear(b);
        }
        /* (22-23) -Inf vs finite — -Inf wins. */
        {
            mpfr_t a, b;
            init_neg_inf(a, 53); init_from_double(b, 12.0, 53);
            emit_case(out, "edge", a, b, 53, MPFR_RNDN);
            mpfr_clear(a); mpfr_clear(b);
        }
        {
            mpfr_t a, b;
            init_from_double(a, 12.0, 53); init_neg_inf(b, 53);
            emit_case(out, "edge", a, b, 53, MPFR_RNDN);
            mpfr_clear(a); mpfr_clear(b);
        }
        /* (24-25) +Inf vs -Inf. */
        {
            mpfr_t a, b;
            init_pos_inf(a, 53); init_neg_inf(b, 53);
            emit_case(out, "edge", a, b, 53, MPFR_RNDN);
            mpfr_clear(a); mpfr_clear(b);
        }
        {
            mpfr_t a, b;
            init_neg_inf(a, 53); init_pos_inf(b, 53);
            emit_case(out, "edge", a, b, 53, MPFR_RNDN);
            mpfr_clear(a); mpfr_clear(b);
        }
        /* (26) ±Inf same. */
        {
            mpfr_t a, b;
            init_pos_inf(a, 53); init_pos_inf(b, 53);
            emit_case(out, "edge", a, b, 53, MPFR_RNDN);
            mpfr_clear(a); mpfr_clear(b);
        }
        /* (27) 0 vs positive. */
        {
            mpfr_t a, b;
            init_pos_zero(a, 53); init_from_double(b, 1.0, 53);
            emit_case(out, "edge", a, b, 53, MPFR_RNDN);
            mpfr_clear(a); mpfr_clear(b);
        }
        /* (28) 0 vs negative. */
        {
            mpfr_t a, b;
            init_pos_zero(a, 53); init_from_double(b, -1.0, 53);
            emit_case(out, "edge", a, b, 53, MPFR_RNDN);
            mpfr_clear(a); mpfr_clear(b);
        }

        /* (29-33) Same-magnitude opposite-sign × 5 rnd modes. min picks
         * the negative one. */
        for (int i = 0; i < 5; ++i) {
            mpfr_t a, b;
            init_from_double(a, 3.14, 53); init_from_double(b, -3.14, 53);
            emit_case(out, "edge", a, b, 53, RNDS[i]);
            mpfr_clear(a); mpfr_clear(b);
        }

        /* (34-38) Normal × 5 rnd modes — same-prec normal pair. */
        for (int i = 0; i < 5; ++i) {
            mpfr_t a, b;
            init_from_double(a, 1.5, 53); init_from_double(b, 2.5, 53);
            emit_case(out, "edge", a, b, 53, RNDS[i]);
            mpfr_clear(a); mpfr_clear(b);
        }

        /* (39-43) Prec change forcing rounding on the winner. */
        emit_dd(out, "edge", 3.14, 2.71, 53, 53, 4, MPFR_RNDN);
        emit_dd(out, "edge", 3.14, 2.71, 53, 53, 4, MPFR_RNDU);
        emit_dd(out, "edge", 3.14, 2.71, 53, 53, 4, MPFR_RNDD);
        emit_dd(out, "edge", 3.14, 2.71, 53, 53, 4, MPFR_RNDZ);
        emit_dd(out, "edge", 3.14, 2.71, 53, 53, 4, MPFR_RNDA);

        /* (44-46) prec=1 boundary. */
        emit_dd(out, "edge", 1.0, 2.0, 1, 1, 1, MPFR_RNDN);
        emit_dd(out, "edge", -1.0, -2.0, 1, 1, 1, MPFR_RNDN);
        emit_dd(out, "edge", 1.0, -1.0, 1, 1, 1, MPFR_RNDN);

        /* (47-50) Mixed prec lossless pad. */
        emit_dd(out, "edge", 1.0, 2.0,  10,  53,  53, MPFR_RNDN);
        emit_dd(out, "edge", 1.5, 0.5,  53, 100, 100, MPFR_RNDN);
        emit_dd(out, "edge", -3.0, 3.0,  3, 200, 200, MPFR_RNDD);
        emit_dd(out, "edge", 1.0, 2.0, 200,  10, 200, MPFR_RNDN);
    }

    /* ============================================================== */
    /* adversarial: ~30                                                */
    /* ============================================================== */
    {
        /* Patterns at prec=53 → prec=3 (force rounding). a < b so min
         * picks a; broken port (swap) picks b => sign/value mismatch. */
        const char *patterns[] = {
            "1.1011E0",
            "1.0101E0",
            "1.1111E0",
            "1.0001E10",
            "1.1100E-50",
        };
        const size_t n_pat = sizeof(patterns) / sizeof(patterns[0]);

        for (size_t p = 0; p < n_pat; ++p) {
            for (int r = 0; r < 5; ++r) {
                mpfr_t a, b;
                /* a = -pattern; b = +pattern. a < b, min picks a. */
                char buf[200];
                buf[0] = '-';
                size_t n = strlen(patterns[p]);
                memcpy(buf + 1, patterns[p], n + 1);
                mpfr_init2(a, 53); mpfr_set_str(a, buf, 2, MPFR_RNDN);
                mpfr_init2(b, 53); mpfr_set_str(b, patterns[p], 2, MPFR_RNDN);
                emit_case(out, "adversarial", a, b, 3, RNDS[r]);
                mpfr_clear(a); mpfr_clear(b);
            }
        }

        /* RNDN tie + prec refit on the winner. */
        {
            mpfr_t a, b;
            mpfr_init2(a, 4); mpfr_set_str(a, "-1.110E0", 2, MPFR_RNDN);
            mpfr_init2(b, 4); mpfr_set_str(b, "1.110E0", 2, MPFR_RNDN);
            emit_case(out, "adversarial", a, b, 2, MPFR_RNDN);
            mpfr_clear(a); mpfr_clear(b);
        }

        /* Carry-out at MSB on the chosen operand. */
        {
            mpfr_t a, b;
            mpfr_init2(a, 4); mpfr_set_str(a, "-1.111E0", 2, MPFR_RNDN);
            mpfr_init2(b, 4); mpfr_set_str(b, "1.000E0", 2, MPFR_RNDN);
            emit_case(out, "adversarial", a, b, 3, MPFR_RNDD);
            mpfr_clear(a); mpfr_clear(b);
        }

        /* Same-value tie at different precs — min picks `a` per the C
         * reference. The lower-prec lossless-pad branch is exercised. */
        {
            mpfr_t a, b;
            init_from_double(a, 1.5, 4); init_from_double(b, 1.5, 53);
            emit_case(out, "adversarial", a, b, 53, MPFR_RNDN);
            mpfr_clear(a); mpfr_clear(b);
        }
        /* And the reverse. */
        {
            mpfr_t a, b;
            init_from_double(a, 1.5, 53); init_from_double(b, 1.5, 4);
            emit_case(out, "adversarial", a, b, 53, MPFR_RNDN);
            mpfr_clear(a); mpfr_clear(b);
        }
    }

    /* ============================================================== */
    /* fuzz: 60                                                       */
    /* ============================================================== */
    {
        xs64_t rng;
        xs64_seed(&rng, 0xAA1A11A1771A11ULL); /* min-ish hex pun */
        const uint64_t precs[6] = { 1, 2, 53, 64, 100, 200 };

        int emitted = 0;
        while (emitted < 60) {
            const uint64_t abits = xs64_next(&rng);
            const uint64_t bbits = xs64_next(&rng);
            const uint64_t aexp = (abits >> 52) & 0x7FF;
            const uint64_t bexp = (bbits >> 52) & 0x7FF;
            if (aexp == 0x7FF || bexp == 0x7FF) continue;

            double ad, bd;
            memcpy(&ad, &abits, sizeof ad);
            memcpy(&bd, &bbits, sizeof bd);

            const uint64_t ap = precs[xs64_below(&rng, 6)];
            const uint64_t bp = precs[xs64_below(&rng, 6)];
            const uint64_t dp = precs[xs64_below(&rng, 6)];
            const mpfr_rnd_t rnd = RNDS[xs64_below(&rng, 5)];

            mpfr_t a, b;
            init_from_double(a, ad, ap);
            init_from_double(b, bd, bp);
            emit_case(out, "fuzz", a, b, dp, rnd);
            mpfr_clear(a); mpfr_clear(b);
            emitted++;
        }
    }

    /* ============================================================== */
    /* mined: 5 — from mpfr/tests/tminmax.c                            */
    /* ============================================================== */
    {
        /* tminmax.c L36–L43: min(NaN, NaN) = NaN. */
        {
            mpfr_t a, b;
            init_nan(a, 53); init_nan(b, 53);
            emit_case(out, "mined", a, b, 53, MPFR_RNDN);
            mpfr_clear(a); mpfr_clear(b);
        }
        /* tminmax.c L51–L58: min(NaN, 0) = 0. */
        {
            mpfr_t a, b;
            init_nan(a, 53); init_pos_zero(b, 53);
            emit_case(out, "mined", a, b, 53, MPFR_RNDN);
            mpfr_clear(a); mpfr_clear(b);
        }
        /* tminmax.c L86–L91: min(+0, -0) = -0. */
        {
            mpfr_t a, b;
            init_pos_zero(a, 53); init_neg_zero(b, 53);
            emit_case(out, "mined", a, b, 53, MPFR_RNDN);
            mpfr_clear(a); mpfr_clear(b);
        }
        /* tminmax.c L123–L137: min(-Inf, 12) = -Inf. */
        {
            mpfr_t a, b;
            init_neg_inf(a, 53); init_from_double(b, 12.0, 53);
            emit_case(out, "mined", a, b, 53, MPFR_RNDN);
            mpfr_clear(a); mpfr_clear(b);
        }
        /* tminmax.c L154–L158: min(42, 17) = 17. */
        {
            mpfr_t a, b;
            init_from_double(a, 42.0, 53); init_from_double(b, 17.0, 53);
            emit_case(out, "mined", a, b, 53, MPFR_RNDN);
            mpfr_clear(a); mpfr_clear(b);
        }
    }

    return 0;
}
