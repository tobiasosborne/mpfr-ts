/*
 * golden_driver.c -- Golden master for MPFR's mpfr_nextbelow.
 *
 * C signature
 * -----------
 *
 *   void mpfr_nextbelow(mpfr_ptr x);
 *
 * Steps x one ULP toward -infinity at x's own precision (IEEE 754-2008
 * nextDown restricted to one MPFR precision). No rounding mode, no
 * Result {value, ternary}. The TS port returns a bare MPFR.
 * Ref: mpfr/src/next.c L133-L147.
 *
 * Dispatch (mpfr/src/next.c L133-L147), inverted relative to nextabove:
 *   - NaN -> sets MPFR_FLAGS_NAN; x stays NaN. TS port returns NaN.
 *   - Negative x -> mpfr_nexttoinf (further from zero is "below" for x<0).
 *   - Non-negative x (incl. +0, -0 per MPFR_IS_NEG semantics) ->
 *       mpfr_nexttozero (toward zero is "below" for x>=0).
 *
 * Wire format
 * -----------
 *
 *   {"tag":"<class>",
 *    "inputs":{"x":<MPFR-wire>},
 *    "output":<MPFR-wire>,
 *    "time_ns":<n>}
 *
 *   - output is a BARE MPFR (jl_output_mpfr), not a Result.
 *
 * Tag distribution (CLAUDE.md Rule 7 minimums)
 * --------------------------------------------
 *
 *   happy        :  22
 *   edge         :  34
 *   adversarial  :  12
 *   fuzz         :  55
 *   mined        :   6  (mined from mpfr/tests/tnext.c -- direct
 *                        mpfr_nextbelow cases there.)
 *
 * Like mpfr_nextabove, mpfr_nextbelow handles NaN explicitly without an
 * assert; the driver calls it directly on NaN inputs.
 *
 * Build via eval/golden_master/build.sh.
 *
 * Ref: mpfr/src/next.c        -- C reference.
 * Ref: mpfr/tests/tnext.c     -- mined source.
 * Ref: src/core.ts            -- locked MPFR type.
 */
#include "common.h"

#include <assert.h>
#include <inttypes.h>
#include <math.h>
#include <string.h>

#if GMP_NUMB_BITS != 64
#  error "mpfr_nextbelow golden_driver requires GMP_NUMB_BITS == 64"
#endif

extern void mpfr_setmin(mpfr_ptr, mpfr_exp_t);
extern void mpfr_setmax(mpfr_ptr, mpfr_exp_t);

/* Emit one mpfr_nextbelow golden case. Safe for any kind including NaN. */
static inline void emit_case(FILE *out, const char *tag, mpfr_srcptr x_in) {
    mpfr_t x;
    mpfr_init2(x, mpfr_get_prec(x_in));
    mpfr_set(x, x_in, MPFR_RNDN);

    const uint64_t t0 = now_ns();
    mpfr_nextbelow(x);
    const uint64_t elapsed = now_ns() - t0;

    jl_begin(out, tag);
    jl_kv_mpfr(out, 1, "x", x_in);
    jl_end_inputs(out);
    jl_output_mpfr(out, x);
    jl_finish(out, elapsed);

    mpfr_clear(x);
}

/* ----------------------- builders ----------------------- */

static inline void init_from_double(mpfr_ptr x, double d, uint64_t prec) {
    mpfr_init2(x, (mpfr_prec_t)prec);
    mpfr_set_d(x, d, MPFR_RNDN);
}
static inline void init_from_str_decimal(mpfr_ptr x, const char *s, uint64_t prec) {
    mpfr_init2(x, (mpfr_prec_t)prec);
    mpfr_set_str(x, s, 10, MPFR_RNDN);
}
static inline void init_pos_inf(mpfr_ptr x, uint64_t prec) {
    mpfr_init2(x, (mpfr_prec_t)prec);
    mpfr_set_inf(x, 1);
}
static inline void init_neg_inf(mpfr_ptr x, uint64_t prec) {
    mpfr_init2(x, (mpfr_prec_t)prec);
    mpfr_set_inf(x, -1);
}
static inline void init_pos_zero(mpfr_ptr x, uint64_t prec) {
    mpfr_init2(x, (mpfr_prec_t)prec);
    mpfr_set_zero(x, 1);
}
static inline void init_neg_zero(mpfr_ptr x, uint64_t prec) {
    mpfr_init2(x, (mpfr_prec_t)prec);
    mpfr_set_zero(x, -1);
}
static inline void init_nan(mpfr_ptr x, uint64_t prec) {
    mpfr_init2(x, (mpfr_prec_t)prec);
    mpfr_set_nan(x);
}
static inline void init_min_pos(mpfr_ptr x, uint64_t prec) {
    mpfr_init2(x, (mpfr_prec_t)prec);
    mpfr_setmin(x, mpfr_get_emin());
}
static inline void init_min_neg(mpfr_ptr x, uint64_t prec) {
    mpfr_init2(x, (mpfr_prec_t)prec);
    mpfr_setmin(x, mpfr_get_emin());
    mpfr_neg(x, x, MPFR_RNDN);
}
static inline void init_max_pos(mpfr_ptr x, uint64_t prec) {
    mpfr_init2(x, (mpfr_prec_t)prec);
    mpfr_setmax(x, mpfr_get_emax());
}
static inline void init_max_neg(mpfr_ptr x, uint64_t prec) {
    mpfr_init2(x, (mpfr_prec_t)prec);
    mpfr_setmax(x, mpfr_get_emax());
    mpfr_neg(x, x, MPFR_RNDN);
}
static inline void init_pow2(mpfr_ptr x, uint64_t prec, mpfr_exp_t e) {
    mpfr_init2(x, (mpfr_prec_t)prec);
    mpfr_set_ui(x, 1, MPFR_RNDN);
    mpfr_set_exp(x, e);
}
static inline void init_pow2_neg(mpfr_ptr x, uint64_t prec, mpfr_exp_t e) {
    init_pow2(x, prec, e);
    mpfr_neg(x, x, MPFR_RNDN);
}

/* ----------------------- main ----------------------- */

int main(void) {
    FILE *out = stdout;

    /* ============================================================== */
    /* happy: 22 -- finite normals; both signs across precisions.     */
    /* nextbelow(+x) = +x - 1 ULP toward 0; nextbelow(-x) = -x - 1 ULP.*/
    /* ============================================================== */
    {
        { mpfr_t x; init_from_double(x, 3.14, 53); emit_case(out, "happy", x); mpfr_clear(x); }
        { mpfr_t x; init_from_double(x, -3.14, 53); emit_case(out, "happy", x); mpfr_clear(x); }
        { mpfr_t x; init_from_double(x, 2.71828, 53); emit_case(out, "happy", x); mpfr_clear(x); }
        { mpfr_t x; init_from_double(x, -2.71828, 53); emit_case(out, "happy", x); mpfr_clear(x); }
        { mpfr_t x; init_from_double(x, 100.5, 53); emit_case(out, "happy", x); mpfr_clear(x); }
        { mpfr_t x; init_from_double(x, -100.5, 53); emit_case(out, "happy", x); mpfr_clear(x); }
        { mpfr_t x; init_from_double(x, 1.0, 53); emit_case(out, "happy", x); mpfr_clear(x); }   /* +pow2 -> nexttozero renormalize */
        { mpfr_t x; init_from_double(x, -1.0, 53); emit_case(out, "happy", x); mpfr_clear(x); }
        { mpfr_t x; init_from_double(x, 1234567.89, 53); emit_case(out, "happy", x); mpfr_clear(x); }
        { mpfr_t x; init_from_double(x, -1234567.89, 53); emit_case(out, "happy", x); mpfr_clear(x); }
        { mpfr_t x; init_from_double(x, 1e-10, 53); emit_case(out, "happy", x); mpfr_clear(x); }
        { mpfr_t x; init_from_double(x, -1e-10, 53); emit_case(out, "happy", x); mpfr_clear(x); }

        /* Various common precisions. */
        { mpfr_t x; init_from_double(x, 3.14, 24); emit_case(out, "happy", x); mpfr_clear(x); }
        { mpfr_t x; init_from_double(x, 3.14, 64); emit_case(out, "happy", x); mpfr_clear(x); }
        { mpfr_t x; init_from_double(x, 3.14, 113); emit_case(out, "happy", x); mpfr_clear(x); }
        { mpfr_t x; init_from_double(x, 3.14, 200); emit_case(out, "happy", x); mpfr_clear(x); }
        { mpfr_t x; init_from_double(x, 3.14, 500); emit_case(out, "happy", x); mpfr_clear(x); }

        /* Powers of two: positive -> nexttozero renormalize (exp -= 1,
         * mant all-1s). Negative -> nexttoinf simple increment of |mant|. */
        { mpfr_t x; init_pow2(x, 53, 5); emit_case(out, "happy", x); mpfr_clear(x); }
        { mpfr_t x; init_pow2_neg(x, 53, 5); emit_case(out, "happy", x); mpfr_clear(x); }
        { mpfr_t x; init_pow2(x, 64, 0); emit_case(out, "happy", x); mpfr_clear(x); }
        { mpfr_t x; init_pow2_neg(x, 64, 0); emit_case(out, "happy", x); mpfr_clear(x); }
        { mpfr_t x; init_pow2(x, 128, 100); emit_case(out, "happy", x); mpfr_clear(x); }
    }

    /* ============================================================== */
    /* edge: 34 -- NaN, +/-Inf, +/-0, emin/emax boundaries, prec=1.   */
    /* ============================================================== */
    {
        /* NaN: stays NaN. */
        { mpfr_t x; init_nan(x, 53); emit_case(out, "edge", x); mpfr_clear(x); }
        { mpfr_t x; init_nan(x, 1); emit_case(out, "edge", x); mpfr_clear(x); }
        { mpfr_t x; init_nan(x, 200); emit_case(out, "edge", x); mpfr_clear(x); }

        /* +Inf -> nexttozero -> mpfr_setmax (largest finite). */
        { mpfr_t x; init_pos_inf(x, 53); emit_case(out, "edge", x); mpfr_clear(x); }
        { mpfr_t x; init_pos_inf(x, 1); emit_case(out, "edge", x); mpfr_clear(x); }
        { mpfr_t x; init_pos_inf(x, 64); emit_case(out, "edge", x); mpfr_clear(x); }
        { mpfr_t x; init_pos_inf(x, 200); emit_case(out, "edge", x); mpfr_clear(x); }
        /* -Inf -> nexttoinf -> no-op (already at -Inf). */
        { mpfr_t x; init_neg_inf(x, 53); emit_case(out, "edge", x); mpfr_clear(x); }
        { mpfr_t x; init_neg_inf(x, 1); emit_case(out, "edge", x); mpfr_clear(x); }
        { mpfr_t x; init_neg_inf(x, 200); emit_case(out, "edge", x); mpfr_clear(x); }

        /* +0 -> nexttozero -> -smallest (sign flip). */
        { mpfr_t x; init_pos_zero(x, 53); emit_case(out, "edge", x); mpfr_clear(x); }
        { mpfr_t x; init_pos_zero(x, 1); emit_case(out, "edge", x); mpfr_clear(x); }
        { mpfr_t x; init_pos_zero(x, 64); emit_case(out, "edge", x); mpfr_clear(x); }
        { mpfr_t x; init_pos_zero(x, 200); emit_case(out, "edge", x); mpfr_clear(x); }
        /* -0 -> nexttoinf -> -smallest (sign preserved). Combined with +0 case,
         * both yield -smallest (IEEE nextDown). */
        { mpfr_t x; init_neg_zero(x, 53); emit_case(out, "edge", x); mpfr_clear(x); }
        { mpfr_t x; init_neg_zero(x, 1); emit_case(out, "edge", x); mpfr_clear(x); }
        { mpfr_t x; init_neg_zero(x, 64); emit_case(out, "edge", x); mpfr_clear(x); }

        /* prec=1 -- mantissa is MSB-only. */
        { mpfr_t x; init_pow2(x, 1, 0); emit_case(out, "edge", x); mpfr_clear(x); }
        { mpfr_t x; init_pow2(x, 1, 1); emit_case(out, "edge", x); mpfr_clear(x); }
        { mpfr_t x; init_pow2(x, 1, 100); emit_case(out, "edge", x); mpfr_clear(x); }
        { mpfr_t x; init_pow2_neg(x, 1, 1); emit_case(out, "edge", x); mpfr_clear(x); }

        /* +smallest -> nexttozero -> +0. */
        { mpfr_t x; init_min_pos(x, 53); emit_case(out, "edge", x); mpfr_clear(x); }
        { mpfr_t x; init_min_pos(x, 1); emit_case(out, "edge", x); mpfr_clear(x); }
        { mpfr_t x; init_min_pos(x, 128); emit_case(out, "edge", x); mpfr_clear(x); }
        /* -smallest -> nexttoinf -> 2 * -smallest (simple increment of |mant|). */
        { mpfr_t x; init_min_neg(x, 53); emit_case(out, "edge", x); mpfr_clear(x); }
        { mpfr_t x; init_min_neg(x, 1); emit_case(out, "edge", x); mpfr_clear(x); }

        /* +max -> nexttozero -> mant decrement. */
        { mpfr_t x; init_max_pos(x, 53); emit_case(out, "edge", x); mpfr_clear(x); }
        { mpfr_t x; init_max_pos(x, 1); emit_case(out, "edge", x); mpfr_clear(x); }
        { mpfr_t x; init_max_pos(x, 128); emit_case(out, "edge", x); mpfr_clear(x); }
        /* -max -> nexttoinf -> -Inf (overflow at emax). */
        { mpfr_t x; init_max_neg(x, 53); emit_case(out, "edge", x); mpfr_clear(x); }
        { mpfr_t x; init_max_neg(x, 128); emit_case(out, "edge", x); mpfr_clear(x); }

        /* Multi-limb mantissa: prec across 64- and 128-bit boundaries. */
        { mpfr_t x; init_from_double(x, 1.5, 63); emit_case(out, "edge", x); mpfr_clear(x); }
        { mpfr_t x; init_from_double(x, 1.5, 64); emit_case(out, "edge", x); mpfr_clear(x); }
        { mpfr_t x; init_from_double(x, 1.5, 65); emit_case(out, "edge", x); mpfr_clear(x); }
        { mpfr_t x; init_from_double(x, 1.5, 127); emit_case(out, "edge", x); mpfr_clear(x); }
        { mpfr_t x; init_from_double(x, 1.5, 128); emit_case(out, "edge", x); mpfr_clear(x); }
    }

    /* ============================================================== */
    /* adversarial: 12 -- pow-of-2 boundaries; exp extremes; max-mant. */
    /* ============================================================== */
    {
        /* +pow2 -> nexttozero renormalize. */
        { mpfr_t x; init_pow2(x, 24, 0); emit_case(out, "adversarial", x); mpfr_clear(x); }
        { mpfr_t x; init_pow2(x, 24, 100); emit_case(out, "adversarial", x); mpfr_clear(x); }
        { mpfr_t x; init_pow2(x, 64, 50); emit_case(out, "adversarial", x); mpfr_clear(x); }
        /* -pow2 -> nexttoinf simple increment. */
        { mpfr_t x; init_pow2_neg(x, 24, 0); emit_case(out, "adversarial", x); mpfr_clear(x); }
        { mpfr_t x; init_pow2_neg(x, 64, 50); emit_case(out, "adversarial", x); mpfr_clear(x); }
        { mpfr_t x; init_pow2_neg(x, 200, 50); emit_case(out, "adversarial", x); mpfr_clear(x); }

        /* All-ones mantissa, negative (1 - 1 ULP of next binade) at moderate
         * exponent -- nexttoinf overflow: exp += 1, mant = MSB-only. */
        {
            mpfr_t x; mpfr_init2(x, 53);
            mpfr_setmax(x, 10);
            mpfr_neg(x, x, MPFR_RNDN);
            emit_case(out, "adversarial", x);
            mpfr_clear(x);
        }
        /* Same positive -- nexttozero simple mant decrement. */
        {
            mpfr_t x; mpfr_init2(x, 53);
            mpfr_setmax(x, 10);
            emit_case(out, "adversarial", x);
            mpfr_clear(x);
        }

        /* Just above emin (negative side): nexttoinf simple increment of |mant|. */
        {
            mpfr_t x; mpfr_init2(x, 53);
            mpfr_set_ui(x, 1, MPFR_RNDN);
            mpfr_set_exp(x, mpfr_get_emin() + 1);
            mpfr_neg(x, x, MPFR_RNDN);
            emit_case(out, "adversarial", x);
            mpfr_clear(x);
        }
        /* Just below emax (negative side): nexttoinf simple increment of |mant|. */
        {
            mpfr_t x; mpfr_init2(x, 53);
            mpfr_set_ui(x, 1, MPFR_RNDN);
            mpfr_set_exp(x, mpfr_get_emax() - 1);
            mpfr_neg(x, x, MPFR_RNDN);
            emit_case(out, "adversarial", x);
            mpfr_clear(x);
        }

        /* MSB+1 (smallest above-power-of-2), positive -- nexttozero plain decrement. */
        {
            mpfr_t x; mpfr_init2(x, 53);
            mpfr_set_str(x, "1.0000000000000000000000000000000000000000000000000001", 2, MPFR_RNDN);
            emit_case(out, "adversarial", x);
            mpfr_clear(x);
        }
        /* Same negative. */
        {
            mpfr_t x; mpfr_init2(x, 53);
            mpfr_set_str(x, "-1.0000000000000000000000000000000000000000000000000001", 2, MPFR_RNDN);
            emit_case(out, "adversarial", x);
            mpfr_clear(x);
        }
    }

    /* ============================================================== */
    /* fuzz: 55 -- xorshift-driven random normal inputs.              */
    /* Seed differs from nextabove to broaden cross-fn coverage.      */
    /* ============================================================== */
    {
        xs64_t rng;
        xs64_seed(&rng, 0xBEEFDEAD12345678ULL);
        const uint64_t precs[7] = { 1, 2, 24, 53, 64, 100, 200 };

        int emitted = 0;
        while (emitted < 55) {
            const uint64_t prec = precs[xs64_below(&rng, 7)];
            const uint64_t r1 = xs64_next(&rng);
            const uint64_t r2 = xs64_next(&rng);
            const int64_t exp_bias = (int64_t)(r1 % 401) - 200;
            const double base = ((double)((r2 | 1)) / 18446744073709551616.0);
            const double d = base * ldexp(1.0, (int)exp_bias);
            const int neg = (xs64_below(&rng, 2) == 0) ? +1 : -1;
            mpfr_t x;
            mpfr_init2(x, (mpfr_prec_t)prec);
            mpfr_set_d(x, neg * d, MPFR_RNDN);
            if (!mpfr_regular_p(x)) {
                mpfr_clear(x);
                continue;
            }
            emit_case(out, "fuzz", x);
            mpfr_clear(x);
            emitted++;
        }
    }

    /* ============================================================== */
    /* mined: 6 -- direct mpfr_nextbelow cases from mpfr/tests/tnext.c. */
    /* ============================================================== */
    {
        { mpfr_t x; init_nan(x, 53); emit_case(out, "mined", x); mpfr_clear(x); }
        { mpfr_t x; init_pos_inf(x, 53); emit_case(out, "mined", x); mpfr_clear(x); }
        { mpfr_t x; init_neg_inf(x, 53); emit_case(out, "mined", x); mpfr_clear(x); }
        { mpfr_t x; init_from_str_decimal(x, "1", 53); emit_case(out, "mined", x); mpfr_clear(x); }
        { mpfr_t x; init_from_str_decimal(x, "2", 53); emit_case(out, "mined", x); mpfr_clear(x); }
        { mpfr_t x; init_from_str_decimal(x, "3.1", 53); emit_case(out, "mined", x); mpfr_clear(x); }
    }

    return 0;
}
