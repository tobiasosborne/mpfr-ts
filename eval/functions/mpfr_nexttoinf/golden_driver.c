/*
 * golden_driver.c -- Golden master for MPFR's mpfr_nexttoinf.
 *
 * C signature
 * -----------
 *
 *   void mpfr_nexttoinf(mpfr_ptr x);
 *
 * Steps x one ULP toward +/-infinity (preserving sign), at x's own
 * precision. No rounding mode, no Result {value, ternary}. The TS port
 * returns a bare MPFR.
 * Ref: mpfr/src/next.c L87-L117.
 *
 * Special cases (mpfr/src/next.c L90-L117):
 *   - NaN  -> no-op (C falls through; TS propagates NaN).
 *   - +/-Inf -> no-op (already at the destination).
 *   - +/-0   -> smallest at emin, SIGN PRESERVED (no flip).
 *   - normal -> mant += 1 ULP; carry-out -> exp += 1 / mant = MSB-only,
 *               or emax -> +/-Inf.
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
 *   mined        :   6
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
#  error "mpfr_nexttoinf golden_driver requires GMP_NUMB_BITS == 64"
#endif

/* These are exported by libmpfr but not declared in <mpfr.h> (they live
 * in mpfr-impl.h, which is not installed). The symbols exist in
 * libmpfr.so -- see `nm -D libmpfr.so | grep mpfr_(setmin|setmax|nexttoinf)`.
 * Ref: mpfr/src/next.c L87 ; mpfr/src/setmin.c L26 ; mpfr/src/setmax.c L26. */
extern void mpfr_nexttoinf(mpfr_ptr);
extern void mpfr_setmin(mpfr_ptr, mpfr_exp_t);
extern void mpfr_setmax(mpfr_ptr, mpfr_exp_t);

#define TS_PREC_MAX ((uint64_t)((1ULL << 31) - 257ULL))
#define TS_PREC_MIN ((uint64_t)1)

/* Emit one mpfr_nexttoinf golden case. */
static inline void emit_case(FILE *out, const char *tag, mpfr_srcptr x_in) {
    mpfr_t x;
    mpfr_init2(x, mpfr_get_prec(x_in));
    mpfr_set(x, x_in, MPFR_RNDN);

    const uint64_t t0 = now_ns();
    mpfr_nexttoinf(x);
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
static inline void init_from_str_binary(mpfr_ptr x, const char *s, uint64_t prec) {
    mpfr_init2(x, (mpfr_prec_t)prec);
    mpfr_set_str(x, s, 2, MPFR_RNDN);
}
static inline void init_from_str_decimal(mpfr_ptr x, const char *s, uint64_t prec) {
    mpfr_init2(x, (mpfr_prec_t)prec);
    mpfr_set_str(x, s, 10, MPFR_RNDN);
}
static inline void init_nan(mpfr_ptr x, uint64_t prec) {
    mpfr_init2(x, (mpfr_prec_t)prec);
    mpfr_set_nan(x);
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

/* Largest finite at emax (mpfr_setmax). One step toward Inf must
 * promote to +Inf (carry on increment, exp == emax). */
static inline void init_max_pos(mpfr_ptr x, uint64_t prec) {
    mpfr_init2(x, (mpfr_prec_t)prec);
    mpfr_setmax(x, mpfr_get_emax());
}
static inline void init_max_neg(mpfr_ptr x, uint64_t prec) {
    mpfr_init2(x, (mpfr_prec_t)prec);
    mpfr_setmax(x, mpfr_get_emax());
    mpfr_neg(x, x, MPFR_RNDN);
}

/* mpfr_setmin: smallest positive at emin (mantissa MSB-only). */
static inline void init_min_pos(mpfr_ptr x, uint64_t prec) {
    mpfr_init2(x, (mpfr_prec_t)prec);
    mpfr_setmin(x, mpfr_get_emin());
}

/* Exact power of two: mantissa = MSB-only, exp = e. nexttoinf on this
 * increments the mantissa to MSB+1 (no carry); a clean common case. */
static inline void init_pow2(mpfr_ptr x, uint64_t prec, mpfr_exp_t e) {
    mpfr_init2(x, (mpfr_prec_t)prec);
    mpfr_set_ui(x, 1, MPFR_RNDN);
    mpfr_set_exp(x, e);
}
static inline void init_pow2_neg(mpfr_ptr x, uint64_t prec, mpfr_exp_t e) {
    init_pow2(x, prec, e);
    mpfr_neg(x, x, MPFR_RNDN);
}

/* "All-bits-set mantissa at exp = e" (mpfr_setmax shape, but at any exp).
 * Triggers the carry-out branch of nexttoinf: mant = 2^prec - 1,
 * increment -> 2^prec -> renormalize to exp+1, mant = MSB-only. */
static inline void init_all_ones(mpfr_ptr x, uint64_t prec, mpfr_exp_t e) {
    mpfr_init2(x, (mpfr_prec_t)prec);
    mpfr_setmax(x, e);
}
static inline void init_all_ones_neg(mpfr_ptr x, uint64_t prec, mpfr_exp_t e) {
    init_all_ones(x, prec, e);
    mpfr_neg(x, x, MPFR_RNDN);
}

/* ----------------------- main ----------------------- */

int main(void) {
    FILE *out = stdout;

    /* ============================================================== */
    /* happy: 22 -- finite normal values across precisions/exponents. */
    /* ============================================================== */
    {
        /* Simple doubles -- mantissa increments; no carry. */
        { mpfr_t x; init_from_double(x, 3.14, 53); emit_case(out, "happy", x); mpfr_clear(x); }
        { mpfr_t x; init_from_double(x, -3.14, 53); emit_case(out, "happy", x); mpfr_clear(x); }
        { mpfr_t x; init_from_double(x, 2.71828, 53); emit_case(out, "happy", x); mpfr_clear(x); }
        { mpfr_t x; init_from_double(x, -2.71828, 53); emit_case(out, "happy", x); mpfr_clear(x); }
        { mpfr_t x; init_from_double(x, 100.5, 53); emit_case(out, "happy", x); mpfr_clear(x); }
        { mpfr_t x; init_from_double(x, -100.5, 53); emit_case(out, "happy", x); mpfr_clear(x); }
        { mpfr_t x; init_from_double(x, 1.0, 53); emit_case(out, "happy", x); mpfr_clear(x); }
        { mpfr_t x; init_from_double(x, -1.0, 53); emit_case(out, "happy", x); mpfr_clear(x); }
        { mpfr_t x; init_from_double(x, 1234567.89, 53); emit_case(out, "happy", x); mpfr_clear(x); }
        { mpfr_t x; init_from_double(x, -1234567.89, 53); emit_case(out, "happy", x); mpfr_clear(x); }
        { mpfr_t x; init_from_double(x, 1e-10, 53); emit_case(out, "happy", x); mpfr_clear(x); }
        { mpfr_t x; init_from_double(x, -1e-10, 53); emit_case(out, "happy", x); mpfr_clear(x); }

        /* Common precisions. */
        { mpfr_t x; init_from_double(x, 3.14, 24); emit_case(out, "happy", x); mpfr_clear(x); }
        { mpfr_t x; init_from_double(x, 3.14, 64); emit_case(out, "happy", x); mpfr_clear(x); }
        { mpfr_t x; init_from_double(x, 3.14, 113); emit_case(out, "happy", x); mpfr_clear(x); }
        { mpfr_t x; init_from_double(x, 3.14, 200); emit_case(out, "happy", x); mpfr_clear(x); }
        { mpfr_t x; init_from_double(x, 3.14, 500); emit_case(out, "happy", x); mpfr_clear(x); }

        /* All-ones mantissa: triggers carry-out -> exp += 1, mant = MSB-only. */
        { mpfr_t x; init_all_ones(x, 53, 5); emit_case(out, "happy", x); mpfr_clear(x); }
        { mpfr_t x; init_all_ones(x, 53, -5); emit_case(out, "happy", x); mpfr_clear(x); }
        { mpfr_t x; init_all_ones_neg(x, 53, 10); emit_case(out, "happy", x); mpfr_clear(x); }
        { mpfr_t x; init_all_ones(x, 64, 0); emit_case(out, "happy", x); mpfr_clear(x); }
        { mpfr_t x; init_all_ones(x, 128, 100); emit_case(out, "happy", x); mpfr_clear(x); }
    }

    /* ============================================================== */
    /* edge: 34 -- specials, prec=1, multi-limb, emin/emax boundaries. */
    /* ============================================================== */
    {
        /* NaN propagation. */
        { mpfr_t x; init_nan(x, 53); emit_case(out, "edge", x); mpfr_clear(x); }
        { mpfr_t x; init_nan(x, 1); emit_case(out, "edge", x); mpfr_clear(x); }
        { mpfr_t x; init_nan(x, 200); emit_case(out, "edge", x); mpfr_clear(x); }

        /* +/-Inf -> unchanged (no-op). */
        { mpfr_t x; init_pos_inf(x, 53); emit_case(out, "edge", x); mpfr_clear(x); }
        { mpfr_t x; init_pos_inf(x, 1); emit_case(out, "edge", x); mpfr_clear(x); }
        { mpfr_t x; init_pos_inf(x, 64); emit_case(out, "edge", x); mpfr_clear(x); }
        { mpfr_t x; init_pos_inf(x, 200); emit_case(out, "edge", x); mpfr_clear(x); }
        { mpfr_t x; init_neg_inf(x, 53); emit_case(out, "edge", x); mpfr_clear(x); }
        { mpfr_t x; init_neg_inf(x, 1); emit_case(out, "edge", x); mpfr_clear(x); }
        { mpfr_t x; init_neg_inf(x, 200); emit_case(out, "edge", x); mpfr_clear(x); }

        /* +/-0 -> smallest at emin, SAME sign. */
        { mpfr_t x; init_pos_zero(x, 53); emit_case(out, "edge", x); mpfr_clear(x); }
        { mpfr_t x; init_pos_zero(x, 1); emit_case(out, "edge", x); mpfr_clear(x); }
        { mpfr_t x; init_pos_zero(x, 64); emit_case(out, "edge", x); mpfr_clear(x); }
        { mpfr_t x; init_pos_zero(x, 200); emit_case(out, "edge", x); mpfr_clear(x); }
        { mpfr_t x; init_neg_zero(x, 53); emit_case(out, "edge", x); mpfr_clear(x); }
        { mpfr_t x; init_neg_zero(x, 1); emit_case(out, "edge", x); mpfr_clear(x); }
        { mpfr_t x; init_neg_zero(x, 64); emit_case(out, "edge", x); mpfr_clear(x); }

        /* prec=1 -- mantissa always = 1; nexttoinf at prec=1 always carries:
         * mant goes 1 -> 2 = 2^1, so exp += 1, mant = 1.
         * Value doubles (next power of 2). */
        { mpfr_t x; init_pow2(x, 1, 0); emit_case(out, "edge", x); mpfr_clear(x); }
        { mpfr_t x; init_pow2(x, 1, 1); emit_case(out, "edge", x); mpfr_clear(x); }
        { mpfr_t x; init_pow2(x, 1, -100); emit_case(out, "edge", x); mpfr_clear(x); }
        { mpfr_t x; init_pow2_neg(x, 1, 1); emit_case(out, "edge", x); mpfr_clear(x); }

        /* Smallest positive at emin -- nexttoinf increments mant cleanly. */
        { mpfr_t x; init_min_pos(x, 53); emit_case(out, "edge", x); mpfr_clear(x); }
        { mpfr_t x; init_min_pos(x, 1); emit_case(out, "edge", x); mpfr_clear(x); }
        { mpfr_t x; init_min_pos(x, 128); emit_case(out, "edge", x); mpfr_clear(x); }

        /* Largest finite at emax -- one step toward +Inf MUST overflow to +Inf
         * (mant = 2^prec - 1, increment carries, exp == emax -> +Inf). */
        { mpfr_t x; init_max_pos(x, 53); emit_case(out, "edge", x); mpfr_clear(x); }
        { mpfr_t x; init_max_pos(x, 1); emit_case(out, "edge", x); mpfr_clear(x); }
        { mpfr_t x; init_max_pos(x, 64); emit_case(out, "edge", x); mpfr_clear(x); }
        { mpfr_t x; init_max_pos(x, 128); emit_case(out, "edge", x); mpfr_clear(x); }
        { mpfr_t x; init_max_neg(x, 53); emit_case(out, "edge", x); mpfr_clear(x); }
        { mpfr_t x; init_max_neg(x, 1); emit_case(out, "edge", x); mpfr_clear(x); }

        /* Multi-limb mantissa: prec spanning 64- and 128-bit boundaries. */
        { mpfr_t x; init_from_double(x, 1.5, 63); emit_case(out, "edge", x); mpfr_clear(x); }
        { mpfr_t x; init_from_double(x, 1.5, 64); emit_case(out, "edge", x); mpfr_clear(x); }
        { mpfr_t x; init_from_double(x, 1.5, 65); emit_case(out, "edge", x); mpfr_clear(x); }
        { mpfr_t x; init_from_double(x, 1.5, 127); emit_case(out, "edge", x); mpfr_clear(x); }
        { mpfr_t x; init_from_double(x, 1.5, 128); emit_case(out, "edge", x); mpfr_clear(x); }
        { mpfr_t x; init_from_double(x, 1.5, 129); emit_case(out, "edge", x); mpfr_clear(x); }
    }

    /* ============================================================== */
    /* adversarial: 12 -- carry-out boundaries and emax proximity.    */
    /* ============================================================== */
    {
        /* All-ones mantissa at various exps: carry-out -> renormalize. */
        { mpfr_t x; init_all_ones(x, 24, 0); emit_case(out, "adversarial", x); mpfr_clear(x); }
        { mpfr_t x; init_all_ones(x, 24, 100); emit_case(out, "adversarial", x); mpfr_clear(x); }
        { mpfr_t x; init_all_ones(x, 24, -100); emit_case(out, "adversarial", x); mpfr_clear(x); }
        { mpfr_t x; init_all_ones(x, 64, 50); emit_case(out, "adversarial", x); mpfr_clear(x); }
        { mpfr_t x; init_all_ones(x, 128, 50); emit_case(out, "adversarial", x); mpfr_clear(x); }
        { mpfr_t x; init_all_ones(x, 200, 50); emit_case(out, "adversarial", x); mpfr_clear(x); }
        { mpfr_t x; init_all_ones_neg(x, 24, 0); emit_case(out, "adversarial", x); mpfr_clear(x); }
        { mpfr_t x; init_all_ones_neg(x, 64, 50); emit_case(out, "adversarial", x); mpfr_clear(x); }

        /* Just below emax (one ULP increment -- no overflow yet). */
        {
            mpfr_t x; mpfr_init2(x, 53);
            mpfr_setmax(x, mpfr_get_emax() - 1);
            emit_case(out, "adversarial", x);
            mpfr_clear(x);
        }
        /* Just above emin (one ULP increment -- mantissa 2^(prec-1) -> 2^(prec-1)+1). */
        {
            mpfr_t x; mpfr_init2(x, 53);
            mpfr_set_ui(x, 1, MPFR_RNDN);
            mpfr_set_exp(x, mpfr_get_emin() + 1);
            emit_case(out, "adversarial", x);
            mpfr_clear(x);
        }
        /* prec=1, exp = emax-1: mantissa 1 -> 2 carries, exp = emax, mant = 1.
         * Next step from emax-1 stays finite at emax (just below the cliff). */
        {
            mpfr_t x; mpfr_init2(x, 1);
            mpfr_set_ui(x, 1, MPFR_RNDN);
            mpfr_set_exp(x, mpfr_get_emax() - 1);
            emit_case(out, "adversarial", x);
            mpfr_clear(x);
        }
        /* prec=1, exp = emax: mantissa 1 -> 2 carries, exp == emax -> +Inf. */
        {
            mpfr_t x; mpfr_init2(x, 1);
            mpfr_set_ui(x, 1, MPFR_RNDN);
            mpfr_set_exp(x, mpfr_get_emax());
            emit_case(out, "adversarial", x);
            mpfr_clear(x);
        }
    }

    /* ============================================================== */
    /* fuzz: 55 -- xorshift-driven random normal inputs.               */
    /* ============================================================== */
    {
        xs64_t rng;
        xs64_seed(&rng, 0xDEADBEEF12345678ULL);
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
    /* mined: 6 -- derived from mpfr/tests/tnext.c patterns.           */
    /* ============================================================== */
    {
        /* tnext.c L42-L57 (special cases): NaN stays NaN under
         * nextabove/nextbelow. mpfr_nexttoinf(NaN) is a documented
         * no-op in the C source. */
        { mpfr_t x; init_nan(x, 53); emit_case(out, "mined", x); mpfr_clear(x); }

        /* tnext.c L94 -- tests array "1", "2", "3.1", "Inf" at various
         * precisions. For nexttoinf:
         *   - "1" (positive): mant = MSB, increment yields MSB+1, no carry.
         *   - "2" (positive): same pattern at exp=2.
         *   - "3.1" (positive): mant has fractional bits, increment, no carry.
         *   - "+Inf": no-op.
         */
        { mpfr_t x; init_from_str_decimal(x, "1", 53); emit_case(out, "mined", x); mpfr_clear(x); }
        { mpfr_t x; init_from_str_decimal(x, "2", 53); emit_case(out, "mined", x); mpfr_clear(x); }
        { mpfr_t x; init_from_str_decimal(x, "3.1", 53); emit_case(out, "mined", x); mpfr_clear(x); }
        { mpfr_t x; init_pos_inf(x, 53); emit_case(out, "mined", x); mpfr_clear(x); }

        /* tnext.c L60-L75 -- random normal at odd precision (prec=43,
         * matching the prec+=3 loop's offsets). */
        { mpfr_t x; init_from_double(x, 1.7320508075688772, 43); emit_case(out, "mined", x); mpfr_clear(x); }
    }

    return 0;
}
