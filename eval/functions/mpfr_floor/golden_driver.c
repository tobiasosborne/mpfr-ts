/*
 * golden_driver.c — Golden master for MPFR's mpfr_floor.
 *
 * C: int mpfr_floor(mpfr_t r, mpfr_srcptr u). Rounds toward -∞.
 *   Ref: mpfr/src/rint.c L341–L344 (wrapper) and L35–L304 (mpfr_rint
 *   engine; the RNDD branch).
 *
 * TS: mpfr_floor(x, prec) -> Result. No rnd parameter.
 *
 * Key invariants under test:
 *   - floor of negative fractional value rounds away from zero in
 *     magnitude (= toward -∞ in signed value): floor(-0.3) = -1.
 *   - floor of positive fractional truncates to +0 with sign preserved.
 *   - prec-fit for negative result rounds magnitude up (= toward -∞).
 */
#include "common.h"

#include <assert.h>
#include <inttypes.h>
#include <string.h>

#if GMP_NUMB_BITS != 64
#  error "mpfr_floor golden_driver requires GMP_NUMB_BITS == 64"
#endif

#define TS_PREC_MAX ((uint64_t)((1ULL << 31) - 257ULL))
#define TS_PREC_MIN ((uint64_t)1)

static inline void emit_case(FILE *out, const char *tag,
                             mpfr_srcptr x, uint64_t prec) {
    assert(prec >= TS_PREC_MIN && prec <= TS_PREC_MAX);
    mpfr_t rop;
    mpfr_init2(rop, (mpfr_prec_t)prec);

    const uint64_t t0 = now_ns();
    const int ternary = mpfr_floor(rop, x);
    const uint64_t elapsed = now_ns() - t0;

    jl_begin(out, tag);
    jl_kv_mpfr(out, 1, "x", x);
    jl_kv_u64(out, 0, "prec", prec);
    jl_end_inputs(out);
    jl_output_result(out, rop, ternary);
    jl_finish(out, elapsed);

    mpfr_clear(rop);
}

static inline void init_from_double(mpfr_ptr x, double d, uint64_t prec) {
    mpfr_init2(x, (mpfr_prec_t)prec); mpfr_set_d(x, d, MPFR_RNDN);
}
static inline void init_from_str_binary(mpfr_ptr x, const char *s,
                                        uint64_t prec) {
    mpfr_init2(x, (mpfr_prec_t)prec); mpfr_set_str(x, s, 2, MPFR_RNDN);
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
                          double d, uint64_t srcp, uint64_t dstp) {
    mpfr_t x; init_from_double(x, d, srcp);
    emit_case(out, tag, x, dstp);
    mpfr_clear(x);
}
static inline void emit_str(FILE *out, const char *tag,
                            const char *s, uint64_t srcp, uint64_t dstp) {
    mpfr_t x; init_from_str_binary(x, s, srcp);
    emit_case(out, tag, x, dstp);
    mpfr_clear(x);
}

int main(void) {
    FILE *out = stdout;

    /* happy: ~25 ----------------------------------------------------- */
    {
        /* Positive — floor = trunc. */
        emit_d(out, "happy", 2.3, 53, 53);   /* → 2 */
        emit_d(out, "happy", 2.7, 53, 53);   /* → 2 */
        emit_d(out, "happy", 3.5, 53, 53);   /* → 3 */
        emit_d(out, "happy", 4.99, 53, 53);  /* → 4 */
        emit_d(out, "happy", 10.4, 53, 53);
        emit_d(out, "happy", 100.5, 53, 53);

        /* Negative — floor goes MORE negative. */
        emit_d(out, "happy", -2.3, 53, 53);  /* → -3 (NOT -2) */
        emit_d(out, "happy", -2.7, 53, 53);  /* → -3 */
        emit_d(out, "happy", -3.5, 53, 53);  /* → -4 */
        emit_d(out, "happy", -10.4, 53, 53); /* → -11 */
        emit_d(out, "happy", -100.5, 53, 53);/* → -101 */

        /* Already integer. */
        emit_d(out, "happy", 1.0, 53, 53);
        emit_d(out, "happy", 2.0, 53, 53);
        emit_d(out, "happy", -1.0, 53, 53);
        emit_d(out, "happy", -100.0, 53, 53);

        /* Various precs. */
        emit_d(out, "happy", 3.7, 24, 24);
        emit_d(out, "happy", -3.7, 24, 24);
        emit_d(out, "happy", 3.7, 64, 64);
        emit_d(out, "happy", -3.7, 64, 64);
        emit_d(out, "happy", 3.7, 100, 100);
        emit_d(out, "happy", -3.7, 100, 100);

        emit_d(out, "happy", 1.5e15, 53, 53);
        emit_d(out, "happy", -1.5e15, 53, 53);
        emit_d(out, "happy", 6.022e23, 53, 53);
        emit_d(out, "happy", -6.022e23, 53, 53);
    }

    /* edge: ~35 ------------------------------------------------------ */
    {
        /* NaN, Inf, Zero. */
        { mpfr_t x; init_nan(x, 53); emit_case(out, "edge", x, 53); mpfr_clear(x); }
        { mpfr_t x; init_nan(x, 53); emit_case(out, "edge", x, 1); mpfr_clear(x); }
        { mpfr_t x; init_nan(x, 53); emit_case(out, "edge", x, 200); mpfr_clear(x); }
        { mpfr_t x; init_pos_inf(x, 53); emit_case(out, "edge", x, 53); mpfr_clear(x); }
        { mpfr_t x; init_pos_inf(x, 53); emit_case(out, "edge", x, 1); mpfr_clear(x); }
        { mpfr_t x; init_neg_inf(x, 53); emit_case(out, "edge", x, 53); mpfr_clear(x); }
        { mpfr_t x; init_neg_inf(x, 53); emit_case(out, "edge", x, 200); mpfr_clear(x); }
        { mpfr_t x; init_pos_zero(x, 53); emit_case(out, "edge", x, 53); mpfr_clear(x); }
        { mpfr_t x; init_neg_zero(x, 53); emit_case(out, "edge", x, 53); mpfr_clear(x); }
        { mpfr_t x; init_pos_zero(x, 53); emit_case(out, "edge", x, 200); mpfr_clear(x); }
        { mpfr_t x; init_neg_zero(x, 53); emit_case(out, "edge", x, 1); mpfr_clear(x); }

        /* |x| < 1: floor(+frac) = +0 (sign preserved!); floor(-frac) = -1. */
        emit_d(out, "edge", 0.5, 53, 53);    /* → +0 */
        emit_d(out, "edge", -0.5, 53, 53);   /* → -1 */
        emit_d(out, "edge", 0.99, 53, 53);   /* → +0 */
        emit_d(out, "edge", -0.99, 53, 53);  /* → -1 */
        emit_d(out, "edge", 0.01, 53, 53);
        emit_d(out, "edge", -0.01, 53, 53);  /* → -1 */
        emit_d(out, "edge", 1e-100, 53, 53);
        emit_d(out, "edge", -1e-100, 53, 53);/* → -1 */
        emit_d(out, "edge", 0.5, 53, 1);
        emit_d(out, "edge", -0.5, 53, 1);

        /* |x| just over 1. */
        emit_d(out, "edge", 1.0001, 53, 53);
        emit_d(out, "edge", -1.0001, 53, 53);
        emit_d(out, "edge", 1.5, 53, 53);
        emit_d(out, "edge", -1.5, 53, 53);

        /* Integer at prec=1. */
        emit_d(out, "edge", 1.0, 53, 1);
        emit_d(out, "edge", -1.0, 53, 1);

        /* Prec-fit. floor(4.7) = 4 at prec=2 = 100. */
        emit_d(out, "edge", 4.7, 53, 2);
        emit_d(out, "edge", -4.7, 53, 2);  /* floor = -5; pack at prec=2 */

        /* Prec-fit with truncation. floor(100.7) = 100; at prec=3 fits with
         * truncation (toward -∞ for negatives, toward 0 = ZERO for positives). */
        emit_d(out, "edge", 100.7, 53, 3);
        emit_d(out, "edge", -100.7, 53, 3);

        /* Integer that doesn't fit in target prec. floor(7.0) = 7; at prec=2
         * positive: drops low bit → 6. Negative input: floor(-7) = -7; at
         * prec=2 we round toward -∞ → -8 (magnitude up). */
        emit_d(out, "edge", 7.0, 53, 2);
        emit_d(out, "edge", -7.0, 53, 2);

        emit_d(out, "edge", 1e30, 53, 53);
        emit_d(out, "edge", -1e30, 53, 53);

        emit_d(out, "edge", 2.7, 53, 1);
        emit_d(out, "edge", -2.7, 53, 1);   /* floor=-3; pack at prec=1 → -4 */
        emit_d(out, "edge", 0.7, 53, 1);
        emit_d(out, "edge", -0.7, 53, 1);   /* → -1 */

        emit_d(out, "edge", 2.5, 53, 200);
        emit_d(out, "edge", -2.5, 53, 200);
    }

    /* adversarial: ~15 ---------------------------------------------- */
    {
        /* Negative x with prec-fit needing magnitude bump. */
        emit_d(out, "adversarial", -5.0, 53, 1);   /* floor=-5; -5=101; prec=1: round mag up → -110=-6 (RNDD-neg) */
        emit_d(out, "adversarial", -7.0, 53, 1);   /* floor=-7; -7=111; → -1000 = -8 */
        emit_d(out, "adversarial", -7.0, 53, 2);   /* -7 = 111 at prec=2: round mag up → 1000=8 → -8 */
        emit_d(out, "adversarial", -100.5, 53, 3); /* floor=-101; -101=1100101; drop to prec=3 → -1000000=-128 (mag up) wait need to check */

        /* Positive prec-fit boundary. */
        emit_d(out, "adversarial", 7.0, 53, 1);    /* 7=111; prec=1 → 100=4 (truncate toward zero = toward -∞) */
        emit_d(out, "adversarial", 7.0, 53, 2);    /* prec=2 → 11.0 = 110 = 6 */

        /* Floor of negative fractional, signed -1, at various precs. */
        emit_d(out, "adversarial", -1e-300, 53, 53); /* → -1 */
        emit_d(out, "adversarial", -1e-300, 53, 1);  /* → -1 */
        emit_d(out, "adversarial", -1e-300, 53, 200);

        /* Floor where x has only the MSB set in fractional bits (0.5 boundary). */
        emit_str(out, "adversarial", "1.0E-1", 53, 53);   /* +0.5 → +0 */
        emit_str(out, "adversarial", "-1.0E-1", 53, 53);  /* -0.5 → -1 */
        emit_str(out, "adversarial", "1.0E0", 53, 53);    /* +1 → +1 */
        emit_str(out, "adversarial", "-1.0E0", 53, 53);   /* -1 → -1 */

        /* x already integer at very high exponent. */
        emit_str(out, "adversarial", "1.0E100", 53, 53);
        emit_str(out, "adversarial", "-1.0E100", 53, 53);
    }

    /* fuzz: 60 — half small-magnitude fractional, half wide-range. */
    {
        xs64_t rng;
        xs64_seed(&rng, 0xF100702345ABCDEFULL);
        const uint64_t precs[6] = { 1, 2, 53, 64, 100, 200 };

        int emitted = 0;
        while (emitted < 30) {
            const uint64_t r1 = xs64_next(&rng);
            const uint64_t r2 = xs64_next(&rng);
            const double whole = (double)(r1 % 1000);
            const double frac = (double)r2 / 18446744073709551616.0;
            const int neg = (xs64_next(&rng) & 1) ? -1 : 1;
            const double d = neg * (whole + frac);

            const uint64_t srcp = precs[xs64_below(&rng, 6)];
            const uint64_t dstp = precs[xs64_below(&rng, 6)];

            mpfr_t x;
            init_from_double(x, d, srcp);
            emit_case(out, "fuzz", x, dstp);
            mpfr_clear(x);
            emitted++;
        }
        while (emitted < 60) {
            const uint64_t bits = xs64_next(&rng);
            const uint64_t exp = (bits >> 52) & 0x7FF;
            if (exp == 0x7FF) continue;

            double d;
            memcpy(&d, &bits, sizeof d);

            const uint64_t srcp = precs[xs64_below(&rng, 6)];
            const uint64_t dstp = precs[xs64_below(&rng, 6)];

            mpfr_t x;
            init_from_double(x, d, srcp);
            emit_case(out, "fuzz", x, dstp);
            mpfr_clear(x);
            emitted++;
        }
    }

    /* mined: 5 ------------------------------------------------------- */
    {
        /* basic_tests: x = 1/4 at prec=2, floor = 0 (with +sign). */
        {
            mpfr_t x; mpfr_init2(x, 16); mpfr_set_si_2exp(x, 1, -2, MPFR_RNDN);
            emit_case(out, "mined", x, 2);
            mpfr_clear(x);
        }
        /* basic_tests: x = -1/4 at prec=2, floor = -1 (since -1/4 isn't int).
         * Per C: floor of negative non-integer rounds magnitude up. */
        {
            mpfr_t x; mpfr_init2(x, 16); mpfr_set_si_2exp(x, -1, -2, MPFR_RNDN);
            emit_case(out, "mined", x, 2);
            mpfr_clear(x);
        }
        /* basic_tests: x = 5/4 (1.25), s=+1: floor = i/4 = 1. */
        {
            mpfr_t x; mpfr_init2(x, 16); mpfr_set_si_2exp(x, 5, -2, MPFR_RNDN);
            emit_case(out, "mined", x, 3);
            mpfr_clear(x);
        }
        /* basic_tests: x = -5/4 (-1.25): floor = -((5+3)/4) = -2.
         * (BASIC_TEST formula: floor expected = s>0 ? i/4 : -((i+3)/4).) */
        {
            mpfr_t x; mpfr_init2(x, 16); mpfr_set_si_2exp(x, -5, -2, MPFR_RNDN);
            emit_case(out, "mined", x, 3);
            mpfr_clear(x);
        }
        /* basic_tests: x = -17/4 (-4.25): floor = -5. */
        {
            mpfr_t x; mpfr_init2(x, 16); mpfr_set_si_2exp(x, -17, -2, MPFR_RNDN);
            emit_case(out, "mined", x, 4);
            mpfr_clear(x);
        }
    }

    return 0;
}
