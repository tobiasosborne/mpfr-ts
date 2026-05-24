/*
 * golden_driver.c — Golden master for MPFR's mpfr_trunc.
 *
 * C: int mpfr_trunc(mpfr_t r, mpfr_srcptr u). Truncates toward zero.
 *   Ref: mpfr/src/rint.c L325–L328 (wrapper) and L35–L304 (mpfr_rint
 *   engine; the RNDZ branch).
 *
 * TS: mpfr_trunc(x: MPFR, prec: bigint) -> Result. No rnd parameter.
 *
 * Wire: inputs {x, prec}; output {value, ternary}. Same format as
 * mpfr_round's driver, minus the rnd field.
 *
 * Tag distribution: happy ~25 / edge ~35 / adversarial ~15 / fuzz 60 /
 * mined 5 (mpfr/tests/ttrunc.c).
 */
#include "common.h"

#include <assert.h>
#include <inttypes.h>
#include <string.h>

#if GMP_NUMB_BITS != 64
#  error "mpfr_trunc golden_driver requires GMP_NUMB_BITS == 64"
#endif

#define TS_PREC_MAX ((uint64_t)((1ULL << 31) - 257ULL))
#define TS_PREC_MIN ((uint64_t)1)

static inline void emit_case(FILE *out, const char *tag,
                             mpfr_srcptr x, uint64_t prec) {
    assert(prec >= TS_PREC_MIN && prec <= TS_PREC_MAX);
    mpfr_t rop;
    mpfr_init2(rop, (mpfr_prec_t)prec);

    const uint64_t t0 = now_ns();
    const int ternary = mpfr_trunc(rop, x);
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
        emit_d(out, "happy", 2.3, 53, 53);   /* → 2 */
        emit_d(out, "happy", 2.7, 53, 53);   /* → 2 (truncate) */
        emit_d(out, "happy", 3.5, 53, 53);   /* → 3 */
        emit_d(out, "happy", 4.99, 53, 53);  /* → 4 */
        emit_d(out, "happy", 10.4, 53, 53);
        emit_d(out, "happy", 10.6, 53, 53);
        emit_d(out, "happy", 100.5, 53, 53);
        emit_d(out, "happy", -2.3, 53, 53);  /* → -2 (truncate toward 0) */
        emit_d(out, "happy", -2.7, 53, 53);  /* → -2 */
        emit_d(out, "happy", -3.5, 53, 53);  /* → -3 */
        emit_d(out, "happy", -10.4, 53, 53); /* → -10 */
        emit_d(out, "happy", -100.5, 53, 53);

        /* Already integer. */
        emit_d(out, "happy", 1.0, 53, 53);
        emit_d(out, "happy", 2.0, 53, 53);
        emit_d(out, "happy", -1.0, 53, 53);
        emit_d(out, "happy", -100.0, 53, 53);
        emit_d(out, "happy", 0.0, 53, 53);

        /* Various precs. */
        emit_d(out, "happy", 3.7, 24, 24);
        emit_d(out, "happy", 3.7, 64, 64);
        emit_d(out, "happy", 3.7, 100, 100);
        emit_d(out, "happy", 3.7, 200, 200);

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

        /* |x| < 1: trunc magnitude → 0; sign preserved (signed zero). */
        emit_d(out, "edge", 0.5, 53, 53);    /* → +0 (NOT 0!) */
        emit_d(out, "edge", -0.5, 53, 53);   /* → -0 */
        emit_d(out, "edge", 0.99, 53, 53);   /* → +0 */
        emit_d(out, "edge", -0.99, 53, 53);  /* → -0 */
        emit_d(out, "edge", 0.01, 53, 53);
        emit_d(out, "edge", -0.01, 53, 53);
        emit_d(out, "edge", 1e-100, 53, 53);
        emit_d(out, "edge", -1e-100, 53, 53);

        /* |x| just over 1. */
        emit_d(out, "edge", 1.0001, 53, 53);
        emit_d(out, "edge", -1.0001, 53, 53);
        emit_d(out, "edge", 1.5, 53, 53);
        emit_d(out, "edge", -1.5, 53, 53);

        /* Integer at prec=1. */
        emit_d(out, "edge", 1.0, 53, 1);
        emit_d(out, "edge", -1.0, 53, 1);

        /* Prec-fit: trunc(4.7) = 4 at prec=2 = 100. */
        emit_d(out, "edge", 4.7, 53, 2);
        emit_d(out, "edge", -4.7, 53, 2);

        /* Prec-fit with truncation. trunc(100.7) = 100 = 1100100; at prec=3
         * → drop 4 bits, trunc to 110 = 6, exp=7 → 6*2^(7-3) = 96. */
        emit_d(out, "edge", 100.7, 53, 3);
        emit_d(out, "edge", -100.7, 53, 3);

        /* Already-integer with prec change. */
        emit_d(out, "edge", 7.0, 53, 2);  /* trunc=7, prec=2 → 6 (drop 1) */
        emit_d(out, "edge", -7.0, 53, 2);

        /* Very large integer. */
        emit_d(out, "edge", 1e30, 53, 53);
        emit_d(out, "edge", -1e30, 53, 53);

        /* Tiny prec extremes. */
        emit_d(out, "edge", 2.7, 53, 1);  /* trunc=2; mant=1 exp=2 → 2 */
        emit_d(out, "edge", -2.7, 53, 1);
        emit_d(out, "edge", 0.7, 53, 1);   /* trunc → +0 */
        emit_d(out, "edge", -0.7, 53, 1);

        /* Large prec output, small prec input. */
        emit_d(out, "edge", 2.5, 53, 200);
        emit_d(out, "edge", -2.5, 53, 200);
    }

    /* adversarial: ~15 ---------------------------------------------- */
    {
        /* x with x.exp >= x.prec (already an integer at source prec). */
        emit_str(out, "adversarial", "1.0E5", 3, 3);  /* 1.0 * 2^5 = 32 */
        emit_str(out, "adversarial", "-1.0E5", 3, 3);
        emit_str(out, "adversarial", "1.0E5", 3, 2);  /* prec=2 forces fit */

        /* x with low bits of fractional part being all 1s (max-magnitude
         * fractional). trunc still drops them. */
        emit_str(out, "adversarial", "1.111111111111111111111111111111E0", 53, 53);
        emit_str(out, "adversarial", "-1.111111111111111111111111111111E0", 53, 53);

        /* x just barely above 1, with very low prec target. */
        emit_str(out, "adversarial", "1.0000000000000000000000000000001E0", 53, 1);

        /* x = exactly an integer at high prec, target prec=1.
         * trunc=N exactly, then pack N at prec=1: bl(N) bits, drop bl-1. */
        emit_d(out, "adversarial", 5.0, 53, 1);    /* 5 = 101; prec=1 → 100 (4) */
        emit_d(out, "adversarial", 7.0, 53, 1);    /* 7 = 111; prec=1 → 100 (4) */
        emit_d(out, "adversarial", -5.0, 53, 1);
        emit_d(out, "adversarial", -7.0, 53, 1);

        /* trunc result is exactly 0 with a NEGATIVE input — must
         * preserve -0. */
        emit_str(out, "adversarial", "-1.111E-1", 53, 53);  /* -0.9375 → -0 */
        emit_str(out, "adversarial", "-1.0001E-2", 53, 53); /* tiny negative */

        /* x = 2^prec - 1 (max integer representable at high prec).
         * trunc is the same value; refit to lower prec drops the LSB
         * pattern. */
        emit_str(out, "adversarial",
            "1.111111111111111111111111111111111111111111111111111E52", 53, 3);

        /* x.exp just enough above x.prec that the integer has many trailing
         * zero bits. */
        emit_str(out, "adversarial", "1.0E100", 53, 53);
        emit_str(out, "adversarial", "1.0E100", 53, 3);
    }

    /* fuzz: 60 — half small-magnitude fractional (to distinguish from
     * round/ceil/floor in the mutation-prove pass), half wide-range. */
    {
        xs64_t rng;
        xs64_seed(&rng, 0xDEADBEEF12345678ULL);
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

    /* mined: 5 — from mpfr/tests/ttrunc.c (and trint.c basic_tests) - */
    {
        /* basic_tests: x = 1/4 (i=1 at prec=2): trunc → +0 with ternary -1. */
        {
            mpfr_t x; mpfr_init2(x, 16); mpfr_set_si_2exp(x, 1, -2, MPFR_RNDN);
            emit_case(out, "mined", x, 2);
            mpfr_clear(x);
        }
        /* basic_tests: x = -1/4: trunc → -0 with ternary +1. */
        {
            mpfr_t x; mpfr_init2(x, 16); mpfr_set_si_2exp(x, -1, -2, MPFR_RNDN);
            emit_case(out, "mined", x, 2);
            mpfr_clear(x);
        }
        /* basic_tests: x = 5/4 (i=5, s=+1, prec=3): trunc(1.25) = 1 = i/4. */
        {
            mpfr_t x; mpfr_init2(x, 16); mpfr_set_si_2exp(x, 5, -2, MPFR_RNDN);
            emit_case(out, "mined", x, 3);
            mpfr_clear(x);
        }
        /* basic_tests: x = -5/4: trunc(-1.25) = -1 (toward zero). */
        {
            mpfr_t x; mpfr_init2(x, 16); mpfr_set_si_2exp(x, -5, -2, MPFR_RNDN);
            emit_case(out, "mined", x, 3);
            mpfr_clear(x);
        }
        /* basic_tests-style: x = 17/4 = 4.25 at prec=4. trunc = 4. */
        {
            mpfr_t x; mpfr_init2(x, 16); mpfr_set_si_2exp(x, 17, -2, MPFR_RNDN);
            emit_case(out, "mined", x, 4);
            mpfr_clear(x);
        }
    }

    return 0;
}
