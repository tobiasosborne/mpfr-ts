/*
 * golden_driver.c -- Golden master for MPFR's mpfr_frac.
 *
 * C signature
 * -----------
 *
 *   int mpfr_frac(mpfr_t r, mpfr_srcptr u, mpfr_rnd_t rnd);
 *
 *   Computes the fractional part of u: r = u - trunc(u), preserving
 *   sign. The result is correctly rounded to prec(r) bits in rnd
 *   mode. The ternary flag follows the standard sign-of-(rounded-exact)
 *   convention.
 *   Ref: mpfr/src/frac.c L29-L143.
 *
 * Special-case decisions (mpfr/src/frac.c L43-L57):
 *   - NaN -> NaN, ternary 0.
 *   - Inf or integer u -> signed zero (sign(u)), ternary 0.
 *   - |u| < 1 (ue <= 0) -> mpfr_set(r, u, rnd), inheriting ternary.
 *   - Otherwise: bit-level extraction + mpfr_round_raw / mpfr_set,
 *     ternary from the rounding step.
 *
 * Divergence from C -> TS
 * -----------------------
 *
 * The TS port mpfr_frac(u, prec, rnd) -> Result takes prec
 * positionally (constructing a fresh rop at that prec) and returns
 * {value, ternary} from src/core.ts.
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
 *   happy        :  ~25
 *   edge         :  ~38  (NaN/Inf/+-0/integer/|u|<1 fast path/tie cases)
 *   adversarial  :  ~14  (ternary-direction stresses, prec(r) < prec(frac(u)),
 *                         rounding-mode disagreements)
 *   fuzz         :   60  (xorshift; biased so frac(u) is rarely tiny)
 *   mined        :   6   (from mpfr/tests/tfrac.c)
 *
 * Build via eval/golden_master/build.sh.
 *
 * Ref: mpfr/src/frac.c        -- C reference.
 * Ref: mpfr/tests/tfrac.c     -- mined source.
 * Ref: src/core.ts            -- locked Result / MPFR types.
 */
#include "common.h"

#include <assert.h>
#include <inttypes.h>
#include <string.h>

#if GMP_NUMB_BITS != 64
#  error "mpfr_frac golden_driver requires GMP_NUMB_BITS == 64"
#endif

#define TS_PREC_MAX ((uint64_t)((1ULL << 31) - 257ULL))
#define TS_PREC_MIN ((uint64_t)1)

static const mpfr_rnd_t RNDS[5] = {
    MPFR_RNDN, MPFR_RNDZ, MPFR_RNDU, MPFR_RNDD, MPFR_RNDA
};

/* Emit one mpfr_frac golden case.
 *
 *   1. mpfr_init2(rop, prec) -- fresh target with the wanted prec.
 *   2. ternary = mpfr_frac(rop, u, rnd).
 *   3. emit {tag, inputs:{u, prec, rnd}, output:{value: rop, ternary}}.
 *
 * Timing brackets only the mpfr_frac call. */
static inline void emit_case(FILE *out, const char *tag,
                             mpfr_srcptr u, uint64_t prec,
                             mpfr_rnd_t rnd) {
    assert(prec >= TS_PREC_MIN && prec <= TS_PREC_MAX);
    mpfr_t rop;
    mpfr_init2(rop, (mpfr_prec_t)prec);

    const uint64_t t0 = now_ns();
    const int ternary = mpfr_frac(rop, u, rnd);
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
    /* happy: ~25 -- regular finite |u| > 1 across precs and rnds.    */
    /* ============================================================== */
    {
        /* Simple |u| > 1 doubles, varying rnd. */
        emit_d(out, "happy", 3.14,      53, 53, MPFR_RNDN);   /* frac = 0.14 */
        emit_d(out, "happy", 2.71828,   53, 53, MPFR_RNDN);
        emit_d(out, "happy", 100.5,     53, 53, MPFR_RNDN);
        emit_d(out, "happy", 100.5,     53, 53, MPFR_RNDZ);
        emit_d(out, "happy", 100.5,     53, 53, MPFR_RNDU);
        emit_d(out, "happy", 100.5,     53, 53, MPFR_RNDD);
        emit_d(out, "happy", 100.5,     53, 53, MPFR_RNDA);

        emit_d(out, "happy", -3.14,     53, 53, MPFR_RNDN);   /* frac = -0.14 */
        emit_d(out, "happy", -2.71828,  53, 53, MPFR_RNDN);
        emit_d(out, "happy", -100.5,    53, 53, MPFR_RNDN);

        /* Sqrt(2) * 10 = 14.142..., frac = 0.142... */
        emit_d(out, "happy", 14.142135623730951,   53, 53, MPFR_RNDN);
        emit_d(out, "happy", -14.142135623730951,  53, 53, MPFR_RNDD);

        /* Common precisions. */
        emit_d(out, "happy", 3.14,  24,  24,  MPFR_RNDN);
        emit_d(out, "happy", 3.14,  64,  64,  MPFR_RNDN);
        emit_d(out, "happy", 3.14,  113, 113, MPFR_RNDN);
        emit_d(out, "happy", 3.14,  200, 200, MPFR_RNDN);

        /* Mid-range magnitudes with non-trivial frac. */
        emit_d(out, "happy", 1234.5678,    53, 53, MPFR_RNDN);
        emit_d(out, "happy", -1234.5678,   53, 53, MPFR_RNDA);
        emit_d(out, "happy", 123456.789,   53, 53, MPFR_RNDU);
        emit_d(out, "happy", -987654.321,  53, 53, MPFR_RNDD);

        /* 9.5 / 17.25 / 0.625-suffixed integers -- finite binary fracs. */
        emit_d(out, "happy", 9.5,    53, 53, MPFR_RNDN);
        emit_d(out, "happy", 17.25,  53, 53, MPFR_RNDN);
        emit_d(out, "happy", 65.625, 53, 53, MPFR_RNDN);

        /* Different src/dst precs (same family of values). */
        emit_d(out, "happy", 3.14,  100, 53, MPFR_RNDN);
        emit_d(out, "happy", 3.14,  53, 100, MPFR_RNDN);
    }

    /* ============================================================== */
    /* edge: ~38 -- specials, fast paths, ties, signed-zero            */
    /* ============================================================== */
    {
        /* NaN -> NaN, one per rnd mode. */
        for (int i = 0; i < 5; ++i) {
            mpfr_t u; init_nan(u, 53);
            emit_case(out, "edge", u, 53, RNDS[i]);
            mpfr_clear(u);
        }

        /* +Inf -> +0, -Inf -> -0. */
        { mpfr_t u; init_pos_inf(u, 53);  emit_case(out, "edge", u, 53, MPFR_RNDN); mpfr_clear(u); }
        { mpfr_t u; init_pos_inf(u, 53);  emit_case(out, "edge", u, 1,  MPFR_RNDZ); mpfr_clear(u); }
        { mpfr_t u; init_pos_inf(u, 53);  emit_case(out, "edge", u, 200, MPFR_RNDU); mpfr_clear(u); }
        { mpfr_t u; init_neg_inf(u, 53);  emit_case(out, "edge", u, 53, MPFR_RNDN); mpfr_clear(u); }
        { mpfr_t u; init_neg_inf(u, 53);  emit_case(out, "edge", u, 1,  MPFR_RNDD); mpfr_clear(u); }
        { mpfr_t u; init_neg_inf(u, 53);  emit_case(out, "edge", u, 200, MPFR_RNDA); mpfr_clear(u); }

        /* +0 -> +0, -0 -> -0 (|u| < 1 fast path via mpfr_set; ternary=0). */
        { mpfr_t u; init_pos_zero(u, 53); emit_case(out, "edge", u, 53, MPFR_RNDN); mpfr_clear(u); }
        { mpfr_t u; init_neg_zero(u, 53); emit_case(out, "edge", u, 53, MPFR_RNDN); mpfr_clear(u); }
        { mpfr_t u; init_pos_zero(u, 53); emit_case(out, "edge", u, 200, MPFR_RNDZ); mpfr_clear(u); }
        { mpfr_t u; init_neg_zero(u, 53); emit_case(out, "edge", u, 1, MPFR_RNDA); mpfr_clear(u); }

        /* Integer u: mpfr_integer_p triggers signed-zero result. */
        emit_d(out, "edge",  3.0,         53,  53, MPFR_RNDN);  /* +0 */
        emit_d(out, "edge", -7.0,         53,  53, MPFR_RNDN);  /* -0 */
        emit_d(out, "edge",  1.0,         53,  53, MPFR_RNDN);
        emit_d(out, "edge", -1.0,         53,  53, MPFR_RNDN);
        emit_d(out, "edge",  4503599627370496.0,  53, 53, MPFR_RNDN); /* 2^52 */
        emit_d(out, "edge", -4503599627370496.0,  53, 53, MPFR_RNDN);
        /* 2^100, exactly representable at prec >= 1. */
        emit_str_bin(out, "edge", "1E100",  53, 53, MPFR_RNDN);
        emit_str_bin(out, "edge", "-1E100", 53, 53, MPFR_RNDN);

        /* |u| < 1 fast path (ue <= 0). */
        emit_d(out, "edge",  0.5,    53, 53, MPFR_RNDN);   /* 2^-1 -- exp == 0 boundary */
        emit_d(out, "edge", -0.5,    53, 53, MPFR_RNDN);
        emit_d(out, "edge",  0.25,   53, 53, MPFR_RNDN);
        emit_d(out, "edge", -0.25,   53, 53, MPFR_RNDA);
        emit_d(out, "edge",  1e-10,  53, 53, MPFR_RNDN);
        emit_d(out, "edge", -1e-10,  53, 53, MPFR_RNDN);
        /* |u| < 1 with prec(r) < prec(u) -- forces rounding. */
        emit_d(out, "edge",  0.1, 53, 8, MPFR_RNDN);
        emit_d(out, "edge", -0.1, 53, 8, MPFR_RNDD);

        /* Tie cases for each rounding mode at low target prec.
         * 1.1010101...E1 (1.66..., binary tie pattern) -- the frac part is
         * 0.1010101... which at small target prec exercises every rounding
         * mode's tie-handling. */
        for (int i = 0; i < 5; ++i) {
            emit_str_bin(out, "edge", "1.10101010101010101E1", 53, 4, RNDS[i]);
        }

        /* Very-close-to-integer u: frac is tiny and near subnormal. */
        emit_str_dec(out, "edge", "100.0000000001", 100, 53, MPFR_RNDN);
        emit_str_dec(out, "edge", "100.0000000001", 100, 53, MPFR_RNDA);

        /* prec(r) very small (prec = 1) with non-integer u. */
        emit_d(out, "edge", 3.75, 53, 1, MPFR_RNDN);
        emit_d(out, "edge", -3.75, 53, 1, MPFR_RNDN);
        emit_d(out, "edge", 3.75, 53, 1, MPFR_RNDU);
        emit_d(out, "edge", 3.75, 53, 1, MPFR_RNDD);

        /* Multi-limb mantissa input (prec=128, |u|>1, real frac). */
        emit_str_dec(out, "edge", "12345.67890123456789012345678901234567890",
                     200, 200, MPFR_RNDN);
        emit_str_dec(out, "edge", "-12345.67890123456789012345678901234567890",
                     200, 200, MPFR_RNDN);
    }

    /* ============================================================== */
    /* adversarial: ~14 -- targets ternary direction + rounding-mode  */
    /* disagreement.                                                   */
    /* ============================================================== */
    {
        /* prec(r) < prec(frac(u)) forces non-zero ternary; each
         * rounding mode disagrees with the others on the same input. */
        for (int i = 0; i < 5; ++i) {
            emit_str_bin(out, "adversarial", "1.10110101111E2", 24, 4, RNDS[i]);
        }
        /* Same pattern, negative -- RNDU and RNDD swap roles relative
         * to magnitude rounding modes (RNDA / RNDZ). */
        for (int i = 0; i < 5; ++i) {
            emit_str_bin(out, "adversarial", "-1.10110101111E2", 24, 4, RNDS[i]);
        }

        /* |u| < 1 fast path with prec(r) < prec(u) -- ternary inherited
         * from mpfr_set, which rounds. Adversarial because the |u| < 1
         * branch is structurally different in the C from the main path
         * yet must produce the right ternary. */
        emit_d(out, "adversarial", 0.7, 53, 3, MPFR_RNDU);
        emit_d(out, "adversarial", 0.7, 53, 3, MPFR_RNDD);
        emit_d(out, "adversarial", -0.7, 53, 3, MPFR_RNDU);
        emit_d(out, "adversarial", -0.7, 53, 3, MPFR_RNDD);
    }

    /* ============================================================== */
    /* fuzz: 60 -- xorshift-driven random u / prec / rnd               */
    /* ============================================================== */
    {
        xs64_t rng;
        xs64_seed(&rng, 0xFEEDFACEDEADBEEFULL);
        const uint64_t precs[6] = { 1, 2, 53, 64, 100, 200 };

        int emitted = 0;
        /* 30: |u| > 1 with biased non-integer frac component. The
         * exponent-of-trunc range is small (1..16) so we exercise the
         * un, sh, count_leading_zeros logic without producing extreme
         * values that round to integers. */
        while (emitted < 30) {
            const uint64_t r1 = xs64_next(&rng);
            const uint64_t r2 = xs64_next(&rng);
            /* whole part in [1, 65536). */
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
        /* 30: wide-range random doubles -- covers all regimes, including
         * |u| < 1 (subnormal-ish), |u| > 1 huge, and the integer fast
         * path when the random double happens to be integral. */
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
    /* mined: 6 -- from mpfr/tests/tfrac.c                             */
    /* ============================================================== */
    {
        /* tfrac.c L136-L142 (special): frac(NaN) -> NaN. */
        {
            mpfr_t u; init_nan(u, 53);
            emit_case(out, "mined", u, 53, MPFR_RNDN);
            mpfr_clear(u);
        }
        /* tfrac.c L144-L154 (special): frac(0.101101E3) at prec(u)=6,
         * prec(r)=3, RNDN -> 0.101 (= 0.625). */
        {
            mpfr_t u; init_from_str_binary(u, "0.101101E3", 6);
            emit_case(out, "mined", u, 3, MPFR_RNDN);
            mpfr_clear(u);
        }
        /* tfrac.c L156-L165 (special): frac(0.101101010000010011110011001101E9)
         * at prec(u)=34, prec(r)=26, RNDN -> 0.000010011110011001101. */
        {
            mpfr_t u; init_from_str_binary(u,
                "0.101101010000010011110011001101E9", 34);
            emit_case(out, "mined", u, 26, MPFR_RNDN);
            mpfr_clear(u);
        }
        /* tfrac.c L172-L234 (bug20090918): frac(61680.352935791015625)
         * at prec(u)=32, prec(r)=13. Exercises the subnormal-shape
         * output path via set_emin shenanigans in the original; we
         * test the plain frac call without emin manipulation since
         * the TS port does not implement set_emin. */
        {
            mpfr_t u; init_from_str_decimal(u, "61680.352935791015625", 32);
            /* Cover all 5 rnd modes for this one input (matches the
             * RND_LOOP structure in tfrac.c L189). */
            for (int i = 0; i < 5; ++i) {
                emit_case(out, "mined", u, 13, RNDS[i]);
            }
            mpfr_clear(u);
        }
        /* tfrac.c L172-L234 (bug20090918) other s[]: 61680.999999. */
        {
            mpfr_t u; init_from_str_decimal(u, "61680.999999", 32);
            emit_case(out, "mined", u, 13, MPFR_RNDN);
            mpfr_clear(u);
        }
        /* tfrac.c L283-L286 (main): frac(+Inf) -> +0, ternary 0. */
        {
            mpfr_t u; init_pos_inf(u, 70);
            emit_case(out, "mined", u, 70, MPFR_RNDN);
            mpfr_clear(u);
        }
    }

    return 0;
}
