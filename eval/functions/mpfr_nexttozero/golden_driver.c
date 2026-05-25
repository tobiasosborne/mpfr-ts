/*
 * golden_driver.c -- Golden master for MPFR's mpfr_nexttozero.
 *
 * C signature
 * -----------
 *
 *   void mpfr_nexttozero(mpfr_ptr x);
 *
 * Steps x one ULP toward zero at x's own precision; no rounding mode,
 * no Result {value, ternary}. The TS port returns a bare MPFR.
 * Ref: mpfr/src/next.c L45-L85.
 *
 * Special cases (mpfr/src/next.c L48-L85):
 *   - NaN -> the C asserts; the TS port defensively propagates NaN.
 *     The driver emits NaN cases anyway so the TS-side propagation is
 *     test-covered.
 *   - +/-Inf -> largest finite at emax (sign preserved).
 *   - +/-0   -> smallest at emin, SIGN FLIPPED.
 *   - normal -> mant -= 1 ULP; renormalize if MSB dropped (exact-power-of-2 case).
 *
 * Wire format
 * -----------
 *
 *   {"tag":"<class>",
 *    "inputs":{"x":<MPFR-wire>},
 *    "output":<MPFR-wire>,
 *    "time_ns":<n>}
 *
 *   - output is a BARE MPFR (jl_output_mpfr), not a Result, because the
 *     port returns MPFR not Result.
 *
 * Tag distribution (CLAUDE.md Rule 7 minimums)
 * --------------------------------------------
 *
 *   happy        :  22
 *   edge         :  34
 *   adversarial  :  12
 *   fuzz         :  55
 *   mined        :   6   (derived from tnext.c patterns; see L361 below)
 *
 * The mined count is derived from mpfr/tests/tnext.c, which exercises
 * mpfr_nexttozero only indirectly through nextabove/nextbelow. We hand-
 * convert the tnext.c patterns into direct nexttozero calls below.
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
#  error "mpfr_nexttozero golden_driver requires GMP_NUMB_BITS == 64"
#endif

/* These are exported by libmpfr but not declared in <mpfr.h> (they live
 * in mpfr-impl.h, which is not installed). The symbols exist in
 * libmpfr.so -- see `nm -D libmpfr.so | grep mpfr_(setmin|setmax|nexttozero)`.
 * Ref: mpfr/src/next.c L45 ; mpfr/src/setmin.c L26 ; mpfr/src/setmax.c L26. */
extern void mpfr_nexttozero(mpfr_ptr);
extern void mpfr_setmin(mpfr_ptr, mpfr_exp_t);
extern void mpfr_setmax(mpfr_ptr, mpfr_exp_t);

#define TS_PREC_MAX ((uint64_t)((1ULL << 31) - 257ULL))
#define TS_PREC_MIN ((uint64_t)1)

/* Emit one mpfr_nexttozero golden case.
 *
 *   1. Clone the input value (mpfr_nexttozero mutates).
 *   2. Time the mpfr_nexttozero call.
 *   3. Emit {tag, inputs:{x: <pre-call x>}, output: <post-call x>}.
 *
 * Timing brackets only the mpfr_nexttozero call.
 *
 * NaN inputs are forbidden here -- the C asserts (mpfr/src/next.c L56)
 * because the function is private to NaN-checking callers. Use
 * emit_nan_case() instead for NaN coverage; the TS port defensively
 * propagates NaN. */
static inline void emit_case(FILE *out, const char *tag, mpfr_srcptr x_in) {
    assert(!mpfr_nan_p(x_in));
    mpfr_t x;
    mpfr_init2(x, mpfr_get_prec(x_in));
    mpfr_set(x, x_in, MPFR_RNDN);  /* exact since same prec */

    const uint64_t t0 = now_ns();
    mpfr_nexttozero(x);
    const uint64_t elapsed = now_ns() - t0;

    jl_begin(out, tag);
    jl_kv_mpfr(out, 1, "x", x_in);
    jl_end_inputs(out);
    jl_output_mpfr(out, x);
    jl_finish(out, elapsed);

    mpfr_clear(x);
}

/* Emit a NaN-input case directly, bypassing the C function (which would
 * MPFR_ASSERTN-fail). The expected output is NaN -- the TS port's
 * defensive NaN propagation. We construct an MPFR NaN value for the
 * "input" field (which serialises via jl_kv_mpfr to the canonical NaN
 * wire shape) and emit NaN again for the output via the same helper.
 * time_ns is set to 0 -- the C-side timer is not exercised.
 *
 * Ref: mpfr/src/next.c L56 -- MPFR_ASSERTN(MPFR_IS_ZERO(x)) is the
 *   assertion that the singular-but-not-Inf branch is hit only for zero;
 *   NaN reaches it because MPFR_IS_SINGULAR includes NaN, and the
 *   assertion is what blocks us from calling mpfr_nexttozero on NaN. */
static inline void emit_nan_case(FILE *out, const char *tag, uint64_t prec) {
    mpfr_t x;
    mpfr_init2(x, (mpfr_prec_t)prec);
    mpfr_set_nan(x);

    jl_begin(out, tag);
    jl_kv_mpfr(out, 1, "x", x);
    jl_end_inputs(out);
    /* Output is also NaN. We emit the same value via jl_output_mpfr,
     * which serialises to {"kind":"nan","sign":1,"prec":"0","exp":"0",
     * "mant":"0"} -- matching the TS-side NAN_VALUE exactly. */
    jl_output_mpfr(out, x);
    jl_finish(out, 0);

    mpfr_clear(x);
}

/* ----------------------- builders ----------------------- */

static inline void init_from_double(mpfr_ptr x, double d, uint64_t prec) {
    mpfr_init2(x, (mpfr_prec_t)prec);
    mpfr_set_d(x, d, MPFR_RNDN);
}
static inline void init_from_str_binary(mpfr_ptr x, const char *s, uint64_t prec) {
    mpfr_init2(x, (mpfr_prec_t)prec);
    mpfr_set_str(x, s, 2, MPFR_RNDN);
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

/* Build a value at exp == EMIN_DEFAULT, MSB-only mantissa (mpfr_setmin
 * pattern). Useful for testing the underflow-to-zero branch of
 * nexttozero: this is the smallest representable positive value, and
 * one step toward zero must collapse it to +0. */
static inline void init_min_pos(mpfr_ptr x, uint64_t prec) {
    mpfr_init2(x, (mpfr_prec_t)prec);
    mpfr_setmin(x, mpfr_get_emin());
    /* mpfr_setmin keeps the current sign; freshly-init'd values are
     * positive zero, which mpfr_setmin promotes to the smallest positive
     * normal. */
}
static inline void init_min_neg(mpfr_ptr x, uint64_t prec) {
    mpfr_init2(x, (mpfr_prec_t)prec);
    mpfr_setmin(x, mpfr_get_emin());
    mpfr_neg(x, x, MPFR_RNDN);
}

/* Build a value at exp == EMAX_DEFAULT, all-bits-set mantissa
 * (mpfr_setmax pattern). The largest finite positive value at the given
 * precision. */
static inline void init_max_pos(mpfr_ptr x, uint64_t prec) {
    mpfr_init2(x, (mpfr_prec_t)prec);
    mpfr_setmax(x, mpfr_get_emax());
}

/* Build an exact power of two: mantissa = MSB-only, exp = e.
 * One step toward zero must drop to the next-lower binade (exp -= 1,
 * mantissa all ones). */
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
    /* happy: 22 -- finite normal values across precisions/exponents. */
    /* ============================================================== */
    {
        /* Simple doubles. nexttozero(3.14): mantissa decrements; MSB stays set. */
        { mpfr_t x; init_from_double(x, 3.14, 53); emit_case(out, "happy", x); mpfr_clear(x); }
        { mpfr_t x; init_from_double(x, -3.14, 53); emit_case(out, "happy", x); mpfr_clear(x); }
        { mpfr_t x; init_from_double(x, 2.71828, 53); emit_case(out, "happy", x); mpfr_clear(x); }
        { mpfr_t x; init_from_double(x, -2.71828, 53); emit_case(out, "happy", x); mpfr_clear(x); }
        { mpfr_t x; init_from_double(x, 100.5, 53); emit_case(out, "happy", x); mpfr_clear(x); }
        { mpfr_t x; init_from_double(x, -100.5, 53); emit_case(out, "happy", x); mpfr_clear(x); }
        { mpfr_t x; init_from_double(x, 1.0, 53); emit_case(out, "happy", x); mpfr_clear(x); }   /* exact 2^0; renormalize */
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

        /* Powers of two -- trigger the renormalize branch (exp -= 1, mant all-1s). */
        { mpfr_t x; init_pow2(x, 53, 5); emit_case(out, "happy", x); mpfr_clear(x); }
        { mpfr_t x; init_pow2(x, 53, -5); emit_case(out, "happy", x); mpfr_clear(x); }
        { mpfr_t x; init_pow2_neg(x, 53, 10); emit_case(out, "happy", x); mpfr_clear(x); }
        { mpfr_t x; init_pow2(x, 64, 0); emit_case(out, "happy", x); mpfr_clear(x); }
        { mpfr_t x; init_pow2(x, 128, 100); emit_case(out, "happy", x); mpfr_clear(x); }
    }

    /* ============================================================== */
    /* edge: 34 -- specials, prec=1, multi-limb, emin/emax boundaries. */
    /* ============================================================== */
    {
        /* NaN propagation. C asserts (mpfr/src/next.c L56) so we cannot
         * call mpfr_nexttozero on NaN; emit the expected NaN output
         * directly via emit_nan_case. The TS port defensively returns NaN. */
        emit_nan_case(out, "edge", 53);
        emit_nan_case(out, "edge", 1);
        emit_nan_case(out, "edge", 200);

        /* +/-Inf -> largest finite at emax (sign preserved). */
        { mpfr_t x; init_pos_inf(x, 53); emit_case(out, "edge", x); mpfr_clear(x); }
        { mpfr_t x; init_pos_inf(x, 1); emit_case(out, "edge", x); mpfr_clear(x); }
        { mpfr_t x; init_pos_inf(x, 64); emit_case(out, "edge", x); mpfr_clear(x); }
        { mpfr_t x; init_pos_inf(x, 200); emit_case(out, "edge", x); mpfr_clear(x); }
        { mpfr_t x; init_neg_inf(x, 53); emit_case(out, "edge", x); mpfr_clear(x); }
        { mpfr_t x; init_neg_inf(x, 1); emit_case(out, "edge", x); mpfr_clear(x); }
        { mpfr_t x; init_neg_inf(x, 200); emit_case(out, "edge", x); mpfr_clear(x); }

        /* +/-0 -> smallest at emin, SIGN FLIPPED. +0 -> -smallest, -0 -> +smallest. */
        { mpfr_t x; init_pos_zero(x, 53); emit_case(out, "edge", x); mpfr_clear(x); }
        { mpfr_t x; init_pos_zero(x, 1); emit_case(out, "edge", x); mpfr_clear(x); }
        { mpfr_t x; init_pos_zero(x, 64); emit_case(out, "edge", x); mpfr_clear(x); }
        { mpfr_t x; init_pos_zero(x, 200); emit_case(out, "edge", x); mpfr_clear(x); }
        { mpfr_t x; init_neg_zero(x, 53); emit_case(out, "edge", x); mpfr_clear(x); }
        { mpfr_t x; init_neg_zero(x, 1); emit_case(out, "edge", x); mpfr_clear(x); }
        { mpfr_t x; init_neg_zero(x, 64); emit_case(out, "edge", x); mpfr_clear(x); }

        /* prec=1 -- mantissa is always MSB-only (=1 in TS bigint). After
         * nexttozero, exp -= 1, mant = 1 (same, since 2^prec - 1 = 1 at prec=1). */
        { mpfr_t x; init_pow2(x, 1, 0); emit_case(out, "edge", x); mpfr_clear(x); }
        { mpfr_t x; init_pow2(x, 1, 1); emit_case(out, "edge", x); mpfr_clear(x); }
        { mpfr_t x; init_pow2(x, 1, 100); emit_case(out, "edge", x); mpfr_clear(x); }
        { mpfr_t x; init_pow2_neg(x, 1, 1); emit_case(out, "edge", x); mpfr_clear(x); }

        /* Smallest positive at emin -- one step toward zero must yield +0. */
        { mpfr_t x; init_min_pos(x, 53); emit_case(out, "edge", x); mpfr_clear(x); }
        { mpfr_t x; init_min_pos(x, 1); emit_case(out, "edge", x); mpfr_clear(x); }
        { mpfr_t x; init_min_pos(x, 128); emit_case(out, "edge", x); mpfr_clear(x); }
        { mpfr_t x; init_min_neg(x, 53); emit_case(out, "edge", x); mpfr_clear(x); }
        { mpfr_t x; init_min_neg(x, 1); emit_case(out, "edge", x); mpfr_clear(x); }

        /* Largest finite at emax -- one step toward zero, no exponent shift
         * (mant decrements; MSB stays set). Use init_max_pos for full prec coverage. */
        { mpfr_t x; init_max_pos(x, 53); emit_case(out, "edge", x); mpfr_clear(x); }
        { mpfr_t x; init_max_pos(x, 1); emit_case(out, "edge", x); mpfr_clear(x); }
        { mpfr_t x; init_max_pos(x, 128); emit_case(out, "edge", x); mpfr_clear(x); }

        /* Multi-limb mantissa: prec spanning the 64- and 128-bit boundaries. */
        { mpfr_t x; init_from_double(x, 1.5, 63); emit_case(out, "edge", x); mpfr_clear(x); }
        { mpfr_t x; init_from_double(x, 1.5, 64); emit_case(out, "edge", x); mpfr_clear(x); }
        { mpfr_t x; init_from_double(x, 1.5, 65); emit_case(out, "edge", x); mpfr_clear(x); }
        { mpfr_t x; init_from_double(x, 1.5, 127); emit_case(out, "edge", x); mpfr_clear(x); }
        { mpfr_t x; init_from_double(x, 1.5, 128); emit_case(out, "edge", x); mpfr_clear(x); }
        { mpfr_t x; init_from_double(x, 1.5, 129); emit_case(out, "edge", x); mpfr_clear(x); }
    }

    /* ============================================================== */
    /* adversarial: 12 -- exact-power-of-2 boundaries at multiple precs */
    /* and emax/emin proximity.                                        */
    /* ============================================================== */
    {
        /* Exact powers of two: trigger renormalize branch with crisp
         * post-state (exp -= 1, mant = 2^prec - 1). */
        { mpfr_t x; init_pow2(x, 24, 0); emit_case(out, "adversarial", x); mpfr_clear(x); }
        { mpfr_t x; init_pow2(x, 24, 100); emit_case(out, "adversarial", x); mpfr_clear(x); }
        { mpfr_t x; init_pow2(x, 24, -100); emit_case(out, "adversarial", x); mpfr_clear(x); }
        { mpfr_t x; init_pow2(x, 64, 50); emit_case(out, "adversarial", x); mpfr_clear(x); }
        { mpfr_t x; init_pow2(x, 128, 50); emit_case(out, "adversarial", x); mpfr_clear(x); }
        { mpfr_t x; init_pow2(x, 200, 50); emit_case(out, "adversarial", x); mpfr_clear(x); }
        { mpfr_t x; init_pow2_neg(x, 24, 0); emit_case(out, "adversarial", x); mpfr_clear(x); }
        { mpfr_t x; init_pow2_neg(x, 64, 50); emit_case(out, "adversarial", x); mpfr_clear(x); }

        /* Just above emin (no underflow yet -- mant decrement leaves a
         * regular normal). */
        {
            mpfr_t x; mpfr_init2(x, 53);
            mpfr_set_ui(x, 1, MPFR_RNDN);
            mpfr_set_exp(x, mpfr_get_emin() + 1);  /* exp = emin+1, mant = MSB */
            emit_case(out, "adversarial", x);
            mpfr_clear(x);
        }
        /* Just below emax (mant decrement). */
        {
            mpfr_t x; mpfr_init2(x, 53);
            mpfr_set_ui(x, 1, MPFR_RNDN);
            mpfr_set_exp(x, mpfr_get_emax() - 1);
            emit_case(out, "adversarial", x);
            mpfr_clear(x);
        }
        /* mantissa = MSB+1 (smallest above-power-of-2): decrement should
         * leave a clean MSB-only mantissa, no renormalize. */
        {
            mpfr_t x; mpfr_init2(x, 53);
            /* Build 2^52 + 1 stored at exp=1 (i.e. value = 1 + 2^-52, a
             * MSB-only mantissa plus a single trailing bit). */
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
    /* fuzz: 55 -- xorshift-driven random normal inputs.               */
    /* ============================================================== */
    {
        xs64_t rng;
        xs64_seed(&rng, 0xCAFEBABE12345678ULL);
        const uint64_t precs[7] = { 1, 2, 24, 53, 64, 100, 200 };

        int emitted = 0;
        while (emitted < 55) {
            const uint64_t prec = precs[xs64_below(&rng, 7)];
            const uint64_t r1 = xs64_next(&rng);
            const uint64_t r2 = xs64_next(&rng);
            /* Magnitude across a wide exponent range: pick exp in
             * [-200, 200] for a varied spread. */
            const int64_t exp_bias = (int64_t)(r1 % 401) - 200;
            const double base = ((double)((r2 | 1)) / 18446744073709551616.0);
            /* base is in (0,1); scale to a number with exp_bias-ish magnitude. */
            const double d = base * ldexp(1.0, (int)exp_bias);
            const int neg = (xs64_below(&rng, 2) == 0) ? +1 : -1;
            mpfr_t x;
            mpfr_init2(x, (mpfr_prec_t)prec);
            mpfr_set_d(x, neg * d, MPFR_RNDN);
            if (!mpfr_regular_p(x)) {
                /* Skip subnormals-that-rounded-to-zero or huge-that-became-inf. */
                mpfr_clear(x);
                continue;
            }
            emit_case(out, "fuzz", x);
            mpfr_clear(x);
            emitted++;
        }
    }

    /* ============================================================== */
    /* mined: 6 -- derived from mpfr/tests/tnext.c patterns.           */
    /* ============================================================== */
    {
        /* tnext.c L42-L57 (special cases via mpfr_nextabove/below):
         *   - NaN stays NaN (mpfr_nan_p still holds after).
         *     For mpfr_nexttozero (TS port): NaN -> NaN. We emit via the
         *     NaN-bypass helper because the C function asserts on NaN.
         */
        emit_nan_case(out, "mined", 53);

        /* tnext.c L94 -- tests array "1", "2", "3.1", "Inf" at various
         * precisions. nexttozero on positive of these:
         *   - "1" positive: pow2 case (renormalize branch).
         *   - "2" positive: pow2 case at exp=2.
         *   - "3.1" positive: regular normal (mant decrement, MSB stays set).
         *   - "+Inf": clamps to largest finite at emax.
         * The negative variants are equivalent under sign flip; we cover
         * one to keep the mined count tight.
         */
        { mpfr_t x; init_from_str_decimal(x, "1", 53); emit_case(out, "mined", x); mpfr_clear(x); }
        { mpfr_t x; init_from_str_decimal(x, "2", 53); emit_case(out, "mined", x); mpfr_clear(x); }
        { mpfr_t x; init_from_str_decimal(x, "3.1", 53); emit_case(out, "mined", x); mpfr_clear(x); }
        { mpfr_t x; init_pos_inf(x, 53); emit_case(out, "mined", x); mpfr_clear(x); }

        /* tnext.c L60-L75 -- random pattern: nextabove/below should
         * leave a finite value, the resulting value should differ from
         * input by exactly 1 ULP. We test the underlying mechanic
         * directly: an arbitrary mid-range normal at prec=43 (an odd
         * non-power-of-two precision tnext.c covers via prec+=3 loop). */
        { mpfr_t x; init_from_double(x, 1.7320508075688772, 43); emit_case(out, "mined", x); mpfr_clear(x); }
    }

    return 0;
}
