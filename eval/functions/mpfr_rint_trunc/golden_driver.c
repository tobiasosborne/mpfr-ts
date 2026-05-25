/*
 * golden_driver.c -- Golden master for MPFR's mpfr_rint_trunc.
 *
 * C signature
 * -----------
 *
 *   int mpfr_rint_trunc(mpfr_t r, mpfr_srcptr u, mpfr_rnd_t rnd);
 *
 *   Round u toward zero to a nearby integer, then round THAT integer
 *   to prec(r) bits in mode rnd. Returns the ternary flag (sign of
 *   rounded - exact_trunc_u). The C body (mpfr/src/rint.c L405-L424)
 *   is a pure delegation:
 *
 *     if (singular(u) || integer_p(u))
 *         return mpfr_set(r, u, rnd);
 *     else {
 *         tmp = mpfr_init2(prec(u));
 *         mpfr_trunc(tmp, u);              // exact at prec(u)
 *         return mpfr_set(r, tmp, rnd);    // may round if prec(r) < width(tmp)
 *     }
 *
 * Two-step trunc-then-round distinguishes this from mpfr_trunc: the
 * latter only ever rounds toward zero at r's own prec, whereas
 * mpfr_rint_trunc applies a correctly-rounded final mpfr_set so when
 * prec(r) < bit-width(trunc(u)) the result may differ from trunc(u) by
 * one ulp in the rnd direction.
 *
 * Special cases:
 *   - NaN     -> NaN, ternary 0 (inherited from mpfr_set).
 *   - +/-Inf  -> +/-Inf at prec(r), ternary 0 (mpfr_set preserves sign).
 *   - +/-0    -> +/-0 at prec(r), ternary 0 (mpfr_set preserves signed zero).
 *   - integer u (any prec) -> mpfr_set(r, u, rnd); ternary from that set.
 *   - |u| < 1 non-integer -> trunc(u) = +/-0 (sign of u); mpfr_set ->
 *     signed zero, ternary 0.
 *   - General: ternary = sign(rounded - trunc(u)).
 *
 * Divergence from C -> TS
 * -----------------------
 *
 * TS port `mpfr_rint_trunc(u, prec, rnd) -> Result` takes prec
 * positionally (constructing a fresh rop at that prec) and returns
 * {value, ternary} from src/core.ts. Flag side effects from the
 * intermediate mpfr_trunc are not surfaced (they are restored in the
 * C source and invisible to callers either way).
 *
 * Wire format
 * -----------
 *
 *   {"tag":"<class>",
 *    "inputs":{"u":<MPFR-record>,"prec":"<decimal>","rnd":"RND[NZUDA]"},
 *    "output":{"value":<MPFR-record>,"ternary":<-1|0|1>},
 *    "time_ns":<n>}
 *
 * Tag distribution (CLAUDE.md Rule 7 minimums)
 * --------------------------------------------
 *
 *   happy        :  ~25  (|u| >= 1 finite, all 5 rnds, common precs)
 *   edge         :  ~40  (NaN/Inf/+-0/integer/|u|<1/small-prec output)
 *   adversarial  :  ~14  (prec(r) < width(trunc(u)) tie cases, all 5 rnds)
 *   fuzz         :   60  (xorshift; mixed magnitudes)
 *   mined        :   8   (from mpfr/tests/trint.c basic_tests + special)
 *
 * Build via eval/golden_master/build.sh.
 *
 * Ref: mpfr/src/rint.c L405-L424      -- C reference body.
 * Ref: mpfr/tests/trint.c             -- mined source (basic_tests +
 *                                        special()).
 * Ref: src/core.ts                    -- locked Result / MPFR types.
 * Ref: eval/functions/mpfr_frac/golden_driver.c -- structural sibling.
 * Ref: eval/functions/mpfr_round/golden_driver.c -- single-arg sibling.
 */
#include "common.h"

#include <assert.h>
#include <inttypes.h>
#include <string.h>

#if GMP_NUMB_BITS != 64
#  error "mpfr_rint_trunc golden_driver requires GMP_NUMB_BITS == 64"
#endif

#define TS_PREC_MAX ((uint64_t)((1ULL << 31) - 257ULL))
#define TS_PREC_MIN ((uint64_t)1)

static const mpfr_rnd_t RNDS[5] = {
    MPFR_RNDN, MPFR_RNDZ, MPFR_RNDU, MPFR_RNDD, MPFR_RNDA
};

/* Emit one mpfr_rint_trunc golden case.
 *
 *   1. mpfr_init2(rop, prec) -- fresh target with the wanted prec.
 *   2. ternary = mpfr_rint_trunc(rop, u, rnd).
 *   3. emit {tag, inputs:{u, prec, rnd}, output:{value: rop, ternary}}.
 *
 * Timing brackets only the mpfr_rint_trunc call. */
static inline void emit_case(FILE *out, const char *tag,
                             mpfr_srcptr u, uint64_t prec,
                             mpfr_rnd_t rnd) {
    assert(prec >= TS_PREC_MIN && prec <= TS_PREC_MAX);
    mpfr_t rop;
    mpfr_init2(rop, (mpfr_prec_t)prec);

    const uint64_t t0 = now_ns();
    const int ternary = mpfr_rint_trunc(rop, u, rnd);
    const uint64_t elapsed = now_ns() - t0;

    jl_begin(out, tag);
    jl_kv_mpfr(out, 1, "u", u);
    jl_kv_u64(out, 0, "prec", prec);
    jl_kv_rnd(out, 0, "rnd", rnd);
    jl_end_inputs(out);
    jl_output_result(out, rop, ternary);
    jl_finish(out, elapsed);

    mpfr_clear(rop);
}

/* ----------------------- helpers ----------------------- */

static inline void init_from_double(mpfr_ptr x, double d, uint64_t prec) {
    mpfr_init2(x, (mpfr_prec_t)prec); mpfr_set_d(x, d, MPFR_RNDN);
}
static inline void init_from_str_binary(mpfr_ptr x, const char *s,
                                        uint64_t prec) {
    mpfr_init2(x, (mpfr_prec_t)prec); mpfr_set_str(x, s, 2, MPFR_RNDN);
}
static inline void init_from_str_decimal(mpfr_ptr x, const char *s,
                                         uint64_t prec) {
    mpfr_init2(x, (mpfr_prec_t)prec); mpfr_set_str(x, s, 10, MPFR_RNDN);
}
static inline void init_from_si(mpfr_ptr x, long v, uint64_t prec) {
    mpfr_init2(x, (mpfr_prec_t)prec); mpfr_set_si(x, v, MPFR_RNDN);
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
    mpfr_t u; init_from_double(u, d, srcp);
    emit_case(out, tag, u, dstp, rnd);
    mpfr_clear(u);
}

static inline void emit_str_bin(FILE *out, const char *tag,
                                const char *s, uint64_t srcp, uint64_t dstp,
                                mpfr_rnd_t rnd) {
    mpfr_t u; init_from_str_binary(u, s, srcp);
    emit_case(out, tag, u, dstp, rnd);
    mpfr_clear(u);
}

static inline void emit_str_dec(FILE *out, const char *tag,
                                const char *s, uint64_t srcp, uint64_t dstp,
                                mpfr_rnd_t rnd) {
    mpfr_t u; init_from_str_decimal(u, s, srcp);
    emit_case(out, tag, u, dstp, rnd);
    mpfr_clear(u);
}

/* ----------------------- main ----------------------- */

int main(void) {
    FILE *out = stdout;

    /* ============================================================== */
    /* happy: ~25 -- |u| >= 1 finite values; all 5 rnds; common precs. */
    /* ============================================================== */
    {
        /* Simple positive non-integers -- trunc toward zero, then set. */
        emit_d(out, "happy", 3.14,    53, 53, MPFR_RNDN);   /* trunc=3 */
        emit_d(out, "happy", 3.14,    53, 53, MPFR_RNDZ);
        emit_d(out, "happy", 3.14,    53, 53, MPFR_RNDU);
        emit_d(out, "happy", 3.14,    53, 53, MPFR_RNDD);
        emit_d(out, "happy", 3.14,    53, 53, MPFR_RNDA);

        /* Negatives -- trunc toward zero gives -3. */
        emit_d(out, "happy", -3.14,   53, 53, MPFR_RNDN);   /* trunc=-3 */
        emit_d(out, "happy", -2.71828, 53, 53, MPFR_RNDN);
        emit_d(out, "happy", -100.5,  53, 53, MPFR_RNDN);

        /* More positives at RNDN. */
        emit_d(out, "happy", 2.7,     53, 53, MPFR_RNDN);   /* trunc=2 */
        emit_d(out, "happy", 10.4,    53, 53, MPFR_RNDN);   /* trunc=10 */
        emit_d(out, "happy", 100.5,   53, 53, MPFR_RNDN);   /* trunc=100 */
        emit_d(out, "happy", 1234.5678, 53, 53, MPFR_RNDN); /* trunc=1234 */

        /* Common precisions (24/53/113/200). */
        emit_d(out, "happy", 3.7,     24,  24,  MPFR_RNDN);
        emit_d(out, "happy", 3.7,     64,  64,  MPFR_RNDN);
        emit_d(out, "happy", 3.7,     113, 113, MPFR_RNDN);
        emit_d(out, "happy", 3.7,     200, 200, MPFR_RNDN);

        /* Larger magnitudes -- trunc still exact at prec(u). */
        emit_d(out, "happy", 1.5e15,  53, 53, MPFR_RNDN);
        emit_d(out, "happy", -1.5e15, 53, 53, MPFR_RNDN);
        emit_d(out, "happy", 6.022e23, 53, 53, MPFR_RNDN);
        emit_d(out, "happy", -6.022e23, 53, 53, MPFR_RNDN);

        /* Different src/dst precs (same family of values). */
        emit_d(out, "happy", 3.14,    100, 53,  MPFR_RNDN);
        emit_d(out, "happy", 3.14,    53,  100, MPFR_RNDN);

        /* 9.5 / 17.25 / 0.625-suffixed: exact binary fractional parts. */
        emit_d(out, "happy", 9.5,     53, 53, MPFR_RNDN);   /* trunc=9 */
        emit_d(out, "happy", 17.25,   53, 53, MPFR_RNDN);   /* trunc=17 */
        emit_d(out, "happy", 65.625,  53, 53, MPFR_RNDN);   /* trunc=65 */
    }

    /* ============================================================== */
    /* edge: ~40 -- specials, signed zero, integer fast path, low prec */
    /* ============================================================== */
    {
        /* NaN -> NaN, one per rnd mode. */
        for (int i = 0; i < 5; ++i) {
            mpfr_t u; init_nan(u, 53);
            emit_case(out, "edge", u, 53, RNDS[i]);
            mpfr_clear(u);
        }

        /* +Inf -> +Inf, various prec(r) and rnd. */
        { mpfr_t u; init_pos_inf(u, 53);  emit_case(out, "edge", u, 53,  MPFR_RNDN); mpfr_clear(u); }
        { mpfr_t u; init_pos_inf(u, 53);  emit_case(out, "edge", u, 1,   MPFR_RNDZ); mpfr_clear(u); }
        { mpfr_t u; init_pos_inf(u, 53);  emit_case(out, "edge", u, 200, MPFR_RNDU); mpfr_clear(u); }
        /* -Inf -> -Inf. */
        { mpfr_t u; init_neg_inf(u, 53);  emit_case(out, "edge", u, 53,  MPFR_RNDN); mpfr_clear(u); }
        { mpfr_t u; init_neg_inf(u, 53);  emit_case(out, "edge", u, 1,   MPFR_RNDD); mpfr_clear(u); }
        { mpfr_t u; init_neg_inf(u, 53);  emit_case(out, "edge", u, 200, MPFR_RNDA); mpfr_clear(u); }

        /* +0 -> +0, -0 -> -0 (singular fast path via mpfr_set). */
        { mpfr_t u; init_pos_zero(u, 53); emit_case(out, "edge", u, 53,  MPFR_RNDN); mpfr_clear(u); }
        { mpfr_t u; init_neg_zero(u, 53); emit_case(out, "edge", u, 53,  MPFR_RNDN); mpfr_clear(u); }
        { mpfr_t u; init_pos_zero(u, 53); emit_case(out, "edge", u, 200, MPFR_RNDZ); mpfr_clear(u); }
        { mpfr_t u; init_neg_zero(u, 53); emit_case(out, "edge", u, 1,   MPFR_RNDA); mpfr_clear(u); }

        /* Already-integer u: integer_p fast path, mpfr_set(r, u, rnd).
         * For these, all 5 rnd modes yield the same value (u itself
         * when it fits in prec(r); rounded otherwise) and ternary 0
         * when prec(r) >= bit-width(u). */
        emit_d(out, "edge",  3.0,         53,  53, MPFR_RNDN);
        emit_d(out, "edge", -7.0,         53,  53, MPFR_RNDN);
        emit_d(out, "edge",  1.0,         53,  53, MPFR_RNDN);
        emit_d(out, "edge", -1.0,         53,  53, MPFR_RNDN);
        emit_d(out, "edge",  4503599627370496.0,  53, 53, MPFR_RNDN); /* 2^52 */
        emit_d(out, "edge", -4503599627370496.0,  53, 53, MPFR_RNDN);
        emit_str_bin(out, "edge",  "1E100",  53, 53, MPFR_RNDN);      /* 2^100 */
        emit_str_bin(out, "edge", "-1E100",  53, 53, MPFR_RNDN);

        /* Integer u over all 5 rnd modes -- all give the same result. */
        for (int i = 0; i < 5; ++i) {
            emit_d(out, "edge", 3.0, 53, 53, RNDS[i]);
        }

        /* |u| < 1 non-integer: trunc(u) = signed zero; result is +0/-0
         * at prec(r) with ternary 0. */
        emit_d(out, "edge",  0.5,     53, 53, MPFR_RNDN);   /* trunc=+0 */
        emit_d(out, "edge", -0.5,     53, 53, MPFR_RNDN);   /* trunc=-0 */
        emit_d(out, "edge",  0.7,     53, 53, MPFR_RNDN);
        emit_d(out, "edge", -0.7,     53, 53, MPFR_RNDN);
        emit_d(out, "edge",  0.25,    53, 53, MPFR_RNDA);
        emit_d(out, "edge", -0.25,    53, 53, MPFR_RNDD);
        emit_d(out, "edge",  1e-10,   53, 53, MPFR_RNDN);
        emit_d(out, "edge", -1e-10,   53, 53, MPFR_RNDU);

        /* Very large u (exp > 1000) -- trunc(u) = u exactly (integer),
         * then set may round at smaller prec. */
        emit_str_bin(out, "edge", "1.0101010101E1100", 53, 53, MPFR_RNDN);
        emit_str_bin(out, "edge", "-1.0101010101E1100", 53, 53, MPFR_RNDN);

        /* Output prec smaller than trunc(u) bit width -- forces a
         * non-zero ternary from the final mpfr_set. */
        emit_d(out, "edge",  100.7,   53, 3, MPFR_RNDN);    /* trunc=100=1100100 (7 bits) */
        emit_d(out, "edge", -100.7,   53, 3, MPFR_RNDN);
        emit_d(out, "edge",  4.7,     53, 2, MPFR_RNDN);    /* trunc=4=100 -> 2 bits */
        emit_d(out, "edge", -4.7,     53, 2, MPFR_RNDN);

        /* prec(r)=1 forces aggressive rounding on the integer trunc. */
        emit_d(out, "edge",  3.75,    53, 1, MPFR_RNDN);    /* trunc=3 -> 1 bit */
        emit_d(out, "edge", -3.75,    53, 1, MPFR_RNDN);
        emit_d(out, "edge",  5.99,    53, 1, MPFR_RNDU);
        emit_d(out, "edge", -5.99,    53, 1, MPFR_RNDD);
    }

    /* ============================================================== */
    /* adversarial: ~14 -- prec(r) < width(trunc(u)) ties + rnd disagree */
    /* ============================================================== */
    {
        /* trunc(u) = 5 = 101_2 at prec(r)=2: drop 1 bit, halfway tie.
         * RNDN ties-to-even: trunc=10=2 -> result 4 (exp=3); RNDA: 6;
         * RNDU: 6; RNDD: 4; RNDZ: 4. Different per rnd. */
        emit_d(out, "adversarial", 5.7,  53, 2, MPFR_RNDN);  /* trunc=5 -> 4 (tie even) */
        emit_d(out, "adversarial", 5.7,  53, 2, MPFR_RNDZ);  /* -> 4 */
        emit_d(out, "adversarial", 5.7,  53, 2, MPFR_RNDU);  /* -> 6 */
        emit_d(out, "adversarial", 5.7,  53, 2, MPFR_RNDD);  /* -> 4 */
        emit_d(out, "adversarial", 5.7,  53, 2, MPFR_RNDA);  /* -> 6 */
        /* Negative counterparts -- RNDU/RNDD swap roles w.r.t. magnitude. */
        emit_d(out, "adversarial", -5.7, 53, 2, MPFR_RNDN);  /* trunc=-5 -> -4 (tie even) */
        emit_d(out, "adversarial", -5.7, 53, 2, MPFR_RNDU);  /* -> -4 (toward +inf) */
        emit_d(out, "adversarial", -5.7, 53, 2, MPFR_RNDD);  /* -> -6 (toward -inf) */

        /* trunc(u) = 7 = 111_2 at prec(r)=2: drop 1, tie, carry.
         * RNDN: round-to-even, but truncated=11=3 has odd LSB so round up
         * to 100, carry -> mant=10 exp=3 -> value=8. */
        emit_d(out, "adversarial",  7.3, 53, 2, MPFR_RNDN);  /* -> 8 */
        emit_d(out, "adversarial", -7.3, 53, 2, MPFR_RNDN);

        /* trunc(u) = 3 = 11_2 at prec(r)=1: drop 1, tie, carry to 10 = 2,
         * exp=2 -> value=4. */
        emit_d(out, "adversarial",  3.9, 53, 1, MPFR_RNDN);
        emit_d(out, "adversarial", -3.9, 53, 1, MPFR_RNDN);

        /* Large integer trunc rounded at low prec -- RNDU vs RNDD on
         * a non-tie. */
        emit_str_bin(out, "adversarial",
                     "1.10110101111E20", 24, 4, MPFR_RNDU);
        emit_str_bin(out, "adversarial",
                     "-1.10110101111E20", 24, 4, MPFR_RNDD);
    }

    /* ============================================================== */
    /* fuzz: 60 -- xorshift-driven random u / prec / rnd               */
    /* ============================================================== */
    {
        xs64_t rng;
        xs64_seed(&rng, 0xA1B2C3D4E5F60718ULL);
        const uint64_t precs[6] = { 1, 2, 53, 64, 100, 200 };

        int emitted = 0;
        /* 30: |u| > 1 with biased non-integer frac so trunc-then-set
         * exercises the rounding step distinctively. Whole part in
         * [1, 65536) so the integer trunc has 1..17 significant bits;
         * combined with prec(r) in {1,2,53,...} this hits both the
         * exact (prec(r) >= width) and rounding (prec(r) < width)
         * regimes. */
        while (emitted < 30) {
            const uint64_t r1 = xs64_next(&rng);
            const uint64_t r2 = xs64_next(&rng);
            const double whole = (double)(1 + (r1 % 65535));
            /* frac in (0, 1) -- exclude 0 so u is non-integer. */
            const double frac = ((double)(r2 | 1) / 18446744073709551616.0);
            const int neg = (xs64_next(&rng) & 1) ? -1 : 1;
            const double d = neg * (whole + frac);

            const uint64_t srcp = precs[xs64_below(&rng, 6)];
            const uint64_t dstp = precs[xs64_below(&rng, 6)];
            const mpfr_rnd_t rnd = RNDS[xs64_below(&rng, 5)];

            mpfr_t u;
            init_from_double(u, d, srcp);
            emit_case(out, "fuzz", u, dstp, rnd);
            mpfr_clear(u);
            emitted++;
        }
        /* 30: wide-range random doubles -- covers all regimes including
         * |u| < 1 (trunc to signed zero), |u| > 1 huge (trunc exact and
         * possibly rounded by set), and the integer fast path when the
         * random double happens to be integral. */
        while (emitted < 60) {
            const uint64_t bits = xs64_next(&rng);
            const uint64_t exp_bits = (bits >> 52) & 0x7FF;
            /* Skip NaN/Inf-encoded bit patterns -- we have explicit
             * edge coverage for those. */
            if (exp_bits == 0x7FF) continue;

            double d;
            memcpy(&d, &bits, sizeof d);

            const uint64_t srcp = precs[xs64_below(&rng, 6)];
            const uint64_t dstp = precs[xs64_below(&rng, 6)];
            const mpfr_rnd_t rnd = RNDS[xs64_below(&rng, 5)];

            mpfr_t u;
            init_from_double(u, d, srcp);
            emit_case(out, "fuzz", u, dstp, rnd);
            mpfr_clear(u);
            emitted++;
        }
    }

    /* ============================================================== */
    /* mined: 8 -- from mpfr/tests/trint.c                             */
    /* ============================================================== */
    {
        /* trint.c L38-L40 (special): rint(NaN, RNDN) -> NaN.
         * Applies identically to rint_trunc since both singular paths
         * delegate to mpfr_set. */
        {
            mpfr_t u; init_nan(u, 53);
            emit_case(out, "mined", u, 53, MPFR_RNDN);
            mpfr_clear(u);
        }
        /* trint.c L42-L44 (special): rint(+Inf, RNDN) -> +Inf. */
        {
            mpfr_t u; init_pos_inf(u, 53);
            emit_case(out, "mined", u, 53, MPFR_RNDN);
            mpfr_clear(u);
        }
        /* trint.c L46-L48 (special): rint(-Inf, RNDN) -> -Inf. */
        {
            mpfr_t u; init_neg_inf(u, 53);
            emit_case(out, "mined", u, 53, MPFR_RNDN);
            mpfr_clear(u);
        }
        /* trint.c L50-L52 (special): rint(+0) -> +0. */
        {
            mpfr_t u; init_pos_zero(u, 53);
            emit_case(out, "mined", u, 53, MPFR_RNDN);
            mpfr_clear(u);
        }
        /* trint.c L54-L57 (special): rint(-0) -> -0. */
        {
            mpfr_t u; init_neg_zero(u, 53);
            emit_case(out, "mined", u, 53, MPFR_RNDN);
            mpfr_clear(u);
        }
        /* trint.c L289 BASIC_TEST(trunc, s*(i/4)) with x = s*i/4 at
         * prec=16. Concrete instances (i=1, 3, 4, 5, 7 over s=+1,-1)
         * provide tight expected-trunc invariants that the C test
         * cross-validates:
         *   i=1, s=+1: x= 1/4=0.25;  trunc=0.
         *   i=3, s=+1: x= 3/4=0.75;  trunc=0.
         *   i=4, s=+1: x= 4/4=1.0;   trunc=1 (integer fast path).
         *   i=5, s=+1: x= 5/4=1.25;  trunc=1.
         *   i=7, s=+1: x= 7/4=1.75;  trunc=1.
         *   i=5, s=-1: x=-5/4=-1.25; trunc=-1. */
        {
            mpfr_t u; init_from_str_binary(u, "1.01E0", 16); /* 1.25 */
            emit_case(out, "mined", u, 2, MPFR_RNDN);
            mpfr_clear(u);
        }
        {
            mpfr_t u; init_from_str_binary(u, "1.11E0", 16); /* 1.75 */
            emit_case(out, "mined", u, 2, MPFR_RNDN);
            mpfr_clear(u);
        }
        /* trint.c L563 (main loop): mpfr_rint_trunc(y, x, MPFR_RNDZ)
         * with x set via mpfr_set_z(z) and z in [2^(s-1), 2^s). Cover
         * the case s=4, z=11 (x=11.0, an integer -- exercises the
         * integer_p fast path at RNDZ). The C test asserts the result
         * equals mpfr_trunc(y, x) and inexact matches. */
        {
            mpfr_t u; init_from_si(u, 11, 4);
            emit_case(out, "mined", u, 4, MPFR_RNDZ);
            mpfr_clear(u);
        }
    }

    return 0;
}
