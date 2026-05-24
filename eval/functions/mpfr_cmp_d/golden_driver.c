/*
 * golden_driver.c — Golden master for MPFR's mpfr_cmp_d.
 *
 * C signature
 * -----------
 *
 *   int mpfr_cmp_d(mpfr_srcptr op, double d);
 *
 *   See mpfr/src/cmp_d.c L24–L44: build a 53-bit MPFR tmp via
 *   mpfr_set_d(RNDN), delegate to mpfr_cmp.
 *
 * NaN paths excluded (TS port throws on NaN x OR NaN d).
 *
 * Wire format
 * -----------
 *
 *   {"tag":"<class>",
 *    "inputs":{"x":{<MPFR>},"d":"<quoted double>"},
 *    "output":<-1|0|1>,
 *    "time_ns":<n>}
 *
 *   `d` is emitted via jl_kv_double (quoted %.17g for finites,
 *   "+Infinity" / "-Infinity" for ±Inf; NaN excluded by tag).
 *
 * Tag distribution
 * ----------------
 *
 *   happy        :  ~25  (typical d × x pairs)
 *   edge         :  ~50  (±Inf d, ±0 d, subnormal d, same-value-
 *                         different-prec, kind boundaries on x)
 *   adversarial  :  ~100 (mantissa-alignment pairs where x and d
 *                         represent the same value at different precs)
 *   fuzz         :   60  (PRNG, seed 0xC0FFEEC0FFEEC0FFULL)
 *   mined        :    5  (transcribed from mpfr/tests/tcmp_d.c)
 *
 * Ref: mpfr/src/cmp_d.c — the C reference.
 * Ref: src/ops/cmp_d.ts — the production port.
 * Ref: mpfr/tests/tcmp_d.c — source for the `mined` cases.
 */
#include "common.h"

#include <assert.h>
#include <inttypes.h>
#include <math.h>
#include <string.h>

#if GMP_NUMB_BITS != 64
#  error "mpfr_cmp_d golden_driver requires GMP_NUMB_BITS == 64"
#endif

static inline int normalise_cmp(int r) {
    return (r > 0) ? 1 : ((r < 0) ? -1 : 0);
}

/* Emit one cmp_d case. x must NOT be NaN; d must NOT be NaN. */
static inline void emit_case(FILE *out, const char *tag,
                             mpfr_srcptr x, double d) {
    assert(!mpfr_nan_p(x));
    assert(!(d != d)); /* IEEE-754 self-inequality detects NaN without <math.h>. */

    const uint64_t t0 = now_ns();
    const int raw = mpfr_cmp_d(x, d);
    const uint64_t elapsed = now_ns() - t0;
    const int result = normalise_cmp(raw);

    jl_begin(out, tag);
    jl_kv_mpfr(out, 1, "x", x);
    jl_kv_double(out, 0, "d", d);
    jl_end_inputs(out);
    jl_output_scalar_int(out, result);
    jl_finish(out, elapsed);
}

static inline void init_from_double(mpfr_ptr x, double d, uint64_t prec) {
    mpfr_init2(x, (mpfr_prec_t)prec);
    mpfr_set_d(x, d, MPFR_RNDN);
}

static inline void init_from_si(mpfr_ptr x, long n, uint64_t prec) {
    mpfr_init2(x, (mpfr_prec_t)prec);
    mpfr_set_si(x, n, MPFR_RNDN);
}

static inline void init_from_str_binary(mpfr_ptr x, const char *s, uint64_t prec) {
    mpfr_init2(x, (mpfr_prec_t)prec);
    mpfr_set_str(x, s, 2, MPFR_RNDN);
}

static inline void init_pos_inf(mpfr_ptr x, uint64_t prec)  { mpfr_init2(x, (mpfr_prec_t)prec); mpfr_set_inf(x, 1); }
static inline void init_neg_inf(mpfr_ptr x, uint64_t prec)  { mpfr_init2(x, (mpfr_prec_t)prec); mpfr_set_inf(x, -1); }
static inline void init_pos_zero(mpfr_ptr x, uint64_t prec) { mpfr_init2(x, (mpfr_prec_t)prec); mpfr_set_zero(x, 1); }
static inline void init_neg_zero(mpfr_ptr x, uint64_t prec) { mpfr_init2(x, (mpfr_prec_t)prec); mpfr_set_zero(x, -1); }

int main(void) {
    FILE *out = stdout;
    /* DBL_MIN, DBL_MAX surrogates without <float.h> dependence. */
    const double DBL_INF_POS = 1.0 / 0.0;
    const double DBL_INF_NEG = -1.0 / 0.0;

    /* ============================================================== */
    /* happy: typical x × d pairs                                      */
    /* ============================================================== */
    {
        /* Equal: x == d exactly. */
        { mpfr_t x; init_from_double(x, 1.0, 53); emit_case(out, "happy", x, 1.0); mpfr_clear(x); }
        { mpfr_t x; init_from_double(x, -1.0, 53); emit_case(out, "happy", x, -1.0); mpfr_clear(x); }
        { mpfr_t x; init_from_double(x, 3.14, 53); emit_case(out, "happy", x, 3.14); mpfr_clear(x); }
        { mpfr_t x; init_from_double(x, 0.5, 53); emit_case(out, "happy", x, 0.5); mpfr_clear(x); }
        { mpfr_t x; init_from_double(x, 1e100, 53); emit_case(out, "happy", x, 1e100); mpfr_clear(x); }

        /* x < d. */
        { mpfr_t x; init_from_double(x, 1.0, 53); emit_case(out, "happy", x, 2.0); mpfr_clear(x); }
        { mpfr_t x; init_from_double(x, 2.345, 53); emit_case(out, "happy", x, 2.4); mpfr_clear(x); }
        { mpfr_t x; init_from_double(x, -10.0, 53); emit_case(out, "happy", x, -5.0); mpfr_clear(x); }
        { mpfr_t x; init_from_double(x, 1e-10, 53); emit_case(out, "happy", x, 1.0); mpfr_clear(x); }
        { mpfr_t x; init_from_double(x, -1e10, 53); emit_case(out, "happy", x, 1.0); mpfr_clear(x); }
        { mpfr_t x; init_from_double(x, 3.0, 53); emit_case(out, "happy", x, 3.14); mpfr_clear(x); }
        { mpfr_t x; init_from_double(x, 2.34763465, 53); emit_case(out, "happy", x, 2.4); mpfr_clear(x); }

        /* x > d. */
        { mpfr_t x; init_from_double(x, 2.0, 53); emit_case(out, "happy", x, 1.0); mpfr_clear(x); }
        { mpfr_t x; init_from_double(x, 2.345, 53); emit_case(out, "happy", x, 2.3); mpfr_clear(x); }
        { mpfr_t x; init_from_double(x, -5.0, 53); emit_case(out, "happy", x, -10.0); mpfr_clear(x); }
        { mpfr_t x; init_from_double(x, 1.0, 53); emit_case(out, "happy", x, 1e-10); mpfr_clear(x); }
        { mpfr_t x; init_from_double(x, 100.0, 53); emit_case(out, "happy", x, 99.999); mpfr_clear(x); }
        { mpfr_t x; init_from_double(x, 3.14, 53); emit_case(out, "happy", x, 3.0); mpfr_clear(x); }

        /* Magnitudes. */
        { mpfr_t x; init_from_double(x, 1e300, 53); emit_case(out, "happy", x, 1e-300); mpfr_clear(x); }
        { mpfr_t x; init_from_double(x, 1e-300, 53); emit_case(out, "happy", x, 1e300); mpfr_clear(x); }
        { mpfr_t x; init_from_double(x, -1.5e100, 53); emit_case(out, "happy", x, 1.5e100); mpfr_clear(x); }
        { mpfr_t x; init_from_double(x, 0.1, 53); emit_case(out, "happy", x, 0.2); mpfr_clear(x); }
        { mpfr_t x; init_from_double(x, 0.25, 53); emit_case(out, "happy", x, 0.5); mpfr_clear(x); }
        { mpfr_t x; init_from_double(x, 2.71828, 53); emit_case(out, "happy", x, 3.14159); mpfr_clear(x); }
        { mpfr_t x; init_from_si(x, 42, 53); emit_case(out, "happy", x, 42.0); mpfr_clear(x); }
        { mpfr_t x; init_from_si(x, 42, 53); emit_case(out, "happy", x, 42.5); mpfr_clear(x); }
    }

    /* ============================================================== */
    /* edge: kind × d boundaries                                       */
    /* ============================================================== */
    {
        /* ±Inf d (8). */
        { mpfr_t x; init_from_double(x, 1.0, 53); emit_case(out, "edge", x, DBL_INF_POS); mpfr_clear(x); }
        { mpfr_t x; init_from_double(x, 1e300, 53); emit_case(out, "edge", x, DBL_INF_POS); mpfr_clear(x); }
        { mpfr_t x; init_from_double(x, -1.0, 53); emit_case(out, "edge", x, DBL_INF_POS); mpfr_clear(x); }
        { mpfr_t x; init_pos_inf(x, 53); emit_case(out, "edge", x, DBL_INF_POS); mpfr_clear(x); }
        { mpfr_t x; init_from_double(x, 1.0, 53); emit_case(out, "edge", x, DBL_INF_NEG); mpfr_clear(x); }
        { mpfr_t x; init_from_double(x, -1.0, 53); emit_case(out, "edge", x, DBL_INF_NEG); mpfr_clear(x); }
        { mpfr_t x; init_from_double(x, -1e300, 53); emit_case(out, "edge", x, DBL_INF_NEG); mpfr_clear(x); }
        { mpfr_t x; init_neg_inf(x, 53); emit_case(out, "edge", x, DBL_INF_NEG); mpfr_clear(x); }

        /* ±Inf x (6). */
        { mpfr_t x; init_pos_inf(x, 53); emit_case(out, "edge", x, 0.0); mpfr_clear(x); }
        { mpfr_t x; init_pos_inf(x, 53); emit_case(out, "edge", x, 1.0); mpfr_clear(x); }
        { mpfr_t x; init_pos_inf(x, 53); emit_case(out, "edge", x, -1e300); mpfr_clear(x); }
        { mpfr_t x; init_neg_inf(x, 53); emit_case(out, "edge", x, 0.0); mpfr_clear(x); }
        { mpfr_t x; init_neg_inf(x, 53); emit_case(out, "edge", x, -1.0); mpfr_clear(x); }
        { mpfr_t x; init_neg_inf(x, 53); emit_case(out, "edge", x, 1e300); mpfr_clear(x); }

        /* +Inf x vs -Inf d (and vice versa). */
        { mpfr_t x; init_pos_inf(x, 53); emit_case(out, "edge", x, DBL_INF_NEG); mpfr_clear(x); }
        { mpfr_t x; init_neg_inf(x, 53); emit_case(out, "edge", x, DBL_INF_POS); mpfr_clear(x); }

        /* ±0 x × d (8). cmp does NOT order signed zero. */
        { mpfr_t x; init_pos_zero(x, 53); emit_case(out, "edge", x, 0.0); mpfr_clear(x); }
        { mpfr_t x; init_pos_zero(x, 53); emit_case(out, "edge", x, -0.0); mpfr_clear(x); }
        { mpfr_t x; init_neg_zero(x, 53); emit_case(out, "edge", x, 0.0); mpfr_clear(x); }
        { mpfr_t x; init_neg_zero(x, 53); emit_case(out, "edge", x, -0.0); mpfr_clear(x); }
        { mpfr_t x; init_pos_zero(x, 53); emit_case(out, "edge", x, 1.0); mpfr_clear(x); }
        { mpfr_t x; init_pos_zero(x, 53); emit_case(out, "edge", x, -1.0); mpfr_clear(x); }
        { mpfr_t x; init_neg_zero(x, 53); emit_case(out, "edge", x, 1.0); mpfr_clear(x); }
        { mpfr_t x; init_neg_zero(x, 53); emit_case(out, "edge", x, -1.0); mpfr_clear(x); }

        /* x normal vs d == ±0. */
        { mpfr_t x; init_from_double(x, 1.0, 53); emit_case(out, "edge", x, 0.0); mpfr_clear(x); }
        { mpfr_t x; init_from_double(x, -1.0, 53); emit_case(out, "edge", x, 0.0); mpfr_clear(x); }
        { mpfr_t x; init_from_double(x, 1.0, 53); emit_case(out, "edge", x, -0.0); mpfr_clear(x); }
        { mpfr_t x; init_from_double(x, -1.0, 53); emit_case(out, "edge", x, -0.0); mpfr_clear(x); }
        { mpfr_t x; init_from_double(x, 1e-300, 53); emit_case(out, "edge", x, 0.0); mpfr_clear(x); }
        { mpfr_t x; init_from_double(x, -1e-300, 53); emit_case(out, "edge", x, 0.0); mpfr_clear(x); }

        /* Subnormal d (5). 2^-1074 = MIN_VALUE; 2^-1050 mid-subnormal. */
        { mpfr_t x; init_from_double(x, 0.0, 53); emit_case(out, "edge", x, 5e-324); mpfr_clear(x); }
        { mpfr_t x; init_from_double(x, 5e-324, 100); emit_case(out, "edge", x, 5e-324); mpfr_clear(x); }
        { mpfr_t x; init_from_double(x, 1e-310, 100); emit_case(out, "edge", x, 1e-310); mpfr_clear(x); }
        { mpfr_t x; init_from_double(x, 0.0, 53); emit_case(out, "edge", x, -5e-324); mpfr_clear(x); }
        { mpfr_t x; init_from_double(x, 1e-300, 53); emit_case(out, "edge", x, 5e-324); mpfr_clear(x); }

        /* Same value, different x prec — KEY for alignment shift. */
        { mpfr_t x; init_from_double(x, 1.0, 2); emit_case(out, "edge", x, 1.0); mpfr_clear(x); }
        { mpfr_t x; init_from_double(x, 1.0, 53); emit_case(out, "edge", x, 1.0); mpfr_clear(x); }
        { mpfr_t x; init_from_double(x, 1.0, 200); emit_case(out, "edge", x, 1.0); mpfr_clear(x); }
        { mpfr_t x; init_from_double(x, 1.5, 2); emit_case(out, "edge", x, 1.5); mpfr_clear(x); }
        { mpfr_t x; init_from_double(x, 1.5, 53); emit_case(out, "edge", x, 1.5); mpfr_clear(x); }
        { mpfr_t x; init_from_double(x, 1.5, 200); emit_case(out, "edge", x, 1.5); mpfr_clear(x); }
        { mpfr_t x; init_from_double(x, -2.0, 2); emit_case(out, "edge", x, -2.0); mpfr_clear(x); }
        { mpfr_t x; init_from_double(x, -2.0, 200); emit_case(out, "edge", x, -2.0); mpfr_clear(x); }

        /* x at higher prec than 53 — extra trailing bits.
         *   x = 1 + 2^-100 (prec 200), d = 1.0 → x > d. */
        { mpfr_t x; mpfr_init2(x, 200); mpfr_set_ui(x, 1, MPFR_RNDN);
          mpfr_t eps; mpfr_init2(eps, 200); mpfr_set_ui_2exp(eps, 1, -100, MPFR_RNDN);
          mpfr_add(x, x, eps, MPFR_RNDN); mpfr_clear(eps);
          emit_case(out, "edge", x, 1.0); mpfr_clear(x); }
        { mpfr_t x; mpfr_init2(x, 200); mpfr_set_ui(x, 1, MPFR_RNDN);
          mpfr_t eps; mpfr_init2(eps, 200); mpfr_set_ui_2exp(eps, 1, -100, MPFR_RNDN);
          mpfr_sub(x, x, eps, MPFR_RNDN); mpfr_clear(eps);
          emit_case(out, "edge", x, 1.0); mpfr_clear(x); }

        /* prec=1 boundary. */
        { mpfr_t x; init_from_double(x, 1.0, 1); emit_case(out, "edge", x, 1.0); mpfr_clear(x); }
        { mpfr_t x; init_from_double(x, 0.5, 1); emit_case(out, "edge", x, 0.5); mpfr_clear(x); }
        { mpfr_t x; init_from_double(x, 1.0, 1); emit_case(out, "edge", x, 0.5); mpfr_clear(x); }
        { mpfr_t x; init_from_double(x, 0.5, 1); emit_case(out, "edge", x, 1.0); mpfr_clear(x); }
    }

    /* ============================================================== */
    /* adversarial: same-value cross-prec + ULP perturbation           */
    /* ============================================================== */
    {
        /* Same value at many precs paired with the same d. */
        const struct { double v; uint64_t prec; } eq_pairs[] = {
            { 1.0, 1 }, { 1.0, 2 }, { 1.0, 10 }, { 1.0, 53 }, { 1.0, 64 },
            { 1.0, 100 }, { 1.0, 128 }, { 1.0, 200 }, { 1.0, 256 },
            { 0.5, 1 }, { 0.5, 53 }, { 0.5, 128 }, { 0.5, 256 },
            { 0.25, 2 }, { 0.25, 53 }, { 0.25, 256 },
            { 0.125, 3 }, { 0.125, 53 }, { 0.125, 256 },
            { 1.5, 2 }, { 1.5, 53 }, { 1.5, 256 },
            { 2.0, 1 }, { 2.0, 53 }, { 2.0, 256 },
            { 3.0, 2 }, { 3.0, 53 }, { 3.0, 256 },
            { 4.0, 2 }, { 4.0, 53 }, { 4.0, 256 },
            { 8.0, 3 }, { 8.0, 53 }, { 8.0, 256 },
            { 16.0, 4 }, { 16.0, 53 }, { 16.0, 256 },
            { 1024.0, 1 }, { 1024.0, 53 }, { 1024.0, 256 },
            { 65536.0, 1 }, { 65536.0, 53 }, { 65536.0, 256 },
            { 1.0/1024.0, 1 }, { 1.0/1024.0, 53 }, { 1.0/1024.0, 256 },
            { 1.0/65536.0, 1 }, { 1.0/65536.0, 53 }, { 1.0/65536.0, 256 },
            { -1.0, 1 }, { -1.0, 53 }, { -1.0, 256 },
            { -0.5, 2 }, { -0.5, 53 }, { -0.5, 256 },
            { -1.5, 2 }, { -1.5, 53 }, { -1.5, 256 },
            { -2.0, 1 }, { -2.0, 53 }, { -2.0, 256 },
            { -1024.0, 1 }, { -1024.0, 53 }, { -1024.0, 256 },
            { 3.14, 53 }, { 3.14, 128 }, { 3.14, 256 },
            { -3.14, 53 }, { -3.14, 128 }, { -3.14, 256 },
            { 1e10, 53 }, { 1e10, 256 },
            { 1e20, 67 }, { 1e20, 80 }, { 1e20, 256 },
            { 1e100, 53 }, { 1e100, 256 },
            { 1e-100, 53 }, { 1e-100, 256 },
            { 1e300, 53 }, { 1e300, 256 },
            { 1e-300, 53 }, { 1e-300, 256 },
        };
        const size_t n_eq = sizeof(eq_pairs) / sizeof(eq_pairs[0]);
        for (size_t i = 0; i < n_eq; ++i) {
            mpfr_t x;
            init_from_double(x, eq_pairs[i].v, eq_pairs[i].prec);
            emit_case(out, "adversarial", x, eq_pairs[i].v);
            mpfr_clear(x);
        }

        /* x = d + ULP at high prec → x > d. */
        {
            const double ds[] = { 1.0, 3.14, 1e10, 1e-10, 1e100, -1.0, -1e10 };
            for (size_t i = 0; i < sizeof(ds)/sizeof(ds[0]); ++i) {
                mpfr_t x; mpfr_init2(x, 200);
                mpfr_set_d(x, ds[i], MPFR_RNDN);
                mpfr_t eps; mpfr_init2(eps, 200);
                /* Choose an offset small enough to not perturb the leading
                 * 53 bits at any reasonable magnitude. 2^-150 is below
                 * d's ULP for d < ~2^97. */
                mpfr_set_ui_2exp(eps, 1, -150, MPFR_RNDN);
                if (ds[i] < 0) {
                    mpfr_sub(x, x, eps, MPFR_RNDN); /* more negative: x < d */
                } else {
                    mpfr_add(x, x, eps, MPFR_RNDN); /* x > d */
                }
                mpfr_clear(eps);
                emit_case(out, "adversarial", x, ds[i]);
                mpfr_clear(x);
            }
            /* And the opposite direction. */
            for (size_t i = 0; i < sizeof(ds)/sizeof(ds[0]); ++i) {
                mpfr_t x; mpfr_init2(x, 200);
                mpfr_set_d(x, ds[i], MPFR_RNDN);
                mpfr_t eps; mpfr_init2(eps, 200);
                mpfr_set_ui_2exp(eps, 1, -150, MPFR_RNDN);
                if (ds[i] < 0) {
                    mpfr_add(x, x, eps, MPFR_RNDN); /* less negative: x > d */
                } else {
                    mpfr_sub(x, x, eps, MPFR_RNDN); /* x < d */
                }
                mpfr_clear(eps);
                emit_case(out, "adversarial", x, ds[i]);
                mpfr_clear(x);
            }
        }

        /* Equal-mantissa-with-extra-bit at very high prec.
         *   x = 1 + 2^-200 (prec 250), d = 1.0 → x > d. */
        { mpfr_t x; mpfr_init2(x, 250); mpfr_set_ui(x, 1, MPFR_RNDN);
          mpfr_t eps; mpfr_init2(eps, 250); mpfr_set_ui_2exp(eps, 1, -200, MPFR_RNDN);
          mpfr_add(x, x, eps, MPFR_RNDN); mpfr_clear(eps);
          emit_case(out, "adversarial", x, 1.0); mpfr_clear(x); }
        { mpfr_t x; mpfr_init2(x, 250); mpfr_set_ui(x, 1, MPFR_RNDN);
          mpfr_t eps; mpfr_init2(eps, 250); mpfr_set_ui_2exp(eps, 1, -200, MPFR_RNDN);
          mpfr_sub(x, x, eps, MPFR_RNDN); mpfr_clear(eps);
          emit_case(out, "adversarial", x, 1.0); mpfr_clear(x); }

        /* x vs ±Inf d at various x precs. */
        { mpfr_t x; init_from_double(x, 1e300, 53); emit_case(out, "adversarial", x, DBL_INF_POS); mpfr_clear(x); }
        { mpfr_t x; init_from_double(x, 1e300, 200); emit_case(out, "adversarial", x, DBL_INF_POS); mpfr_clear(x); }
        { mpfr_t x; init_from_double(x, -1e300, 200); emit_case(out, "adversarial", x, DBL_INF_NEG); mpfr_clear(x); }
        { mpfr_t x; init_pos_inf(x, 200); emit_case(out, "adversarial", x, 1.0); mpfr_clear(x); }
        { mpfr_t x; init_neg_inf(x, 200); emit_case(out, "adversarial", x, -1.0); mpfr_clear(x); }

        /* Same-sign exponent-equal mantissa difference at moderate prec
         * with d picked to compare strictly less / equal / greater. */
        { mpfr_t x; init_from_str_binary(x, "1.00000000000000000000000000000000000000000000000001E1000", 53);
          emit_case(out, "adversarial", x, 1e10); mpfr_clear(x); }
        { mpfr_t x; init_from_str_binary(x, "1.10000000000000000000000000000000000000000000000000E0", 53);
          emit_case(out, "adversarial", x, 1.5); mpfr_clear(x); }
    }

    /* ============================================================== */
    /* fuzz: 60 cases — PRNG-driven                                   */
    /* ============================================================== */
    {
        xs64_t rng;
        xs64_seed(&rng, 0xC0FFEEC0FFEEC0FFULL);
        const uint64_t precs[5] = { 53, 64, 100, 128, 200 };

        int emitted = 0;
        while (emitted < 60) {
            const uint64_t bits_a = xs64_next(&rng);
            const uint64_t bits_b = xs64_next(&rng);

            /* Skip NaN double-bit patterns (both x and d). */
            const uint64_t exp_a = (bits_a >> 52) & 0x7FF;
            const uint64_t exp_b = (bits_b >> 52) & 0x7FF;
            if (exp_a == 0x7FF || exp_b == 0x7FF) continue;

            /* Skip explicit NaN bit patterns: only an exponent of 0x7FF
             * with non-zero mantissa is NaN; we already excluded all
             * 0x7FF (which covers both NaN and Inf via mantissa, but
             * we do include Inf bit patterns by leaving them in via
             * the cast). Recheck: exp == 0x7FF AND mantissa == 0 is
             * Inf; we excluded both. We'll re-fold Inf into edge. */

            double da, db;
            memcpy(&da, &bits_a, sizeof da);
            memcpy(&db, &bits_b, sizeof db);

            const uint64_t pa = precs[xs64_below(&rng, 5)];

            mpfr_t x;
            init_from_double(x, da, pa);
            emit_case(out, "fuzz", x, db);
            mpfr_clear(x);
            emitted++;
        }
    }

    /* ============================================================== */
    /* mined: 5 cases — transcribed from mpfr/tests/tcmp_d.c           */
    /* ============================================================== */
    {
        /* tcmp_d.c L36–L42: mpfr_set_d(x, 2.34763465); cmp_d(x, 2.34763465) == 0. */
        { mpfr_t x; init_from_double(x, 2.34763465, 53); emit_case(out, "mined", x, 2.34763465); mpfr_clear(x); }

        /* tcmp_d.c L43–L48: cmp_d(x=2.34763465, 2.345) > 0. */
        { mpfr_t x; init_from_double(x, 2.34763465, 53); emit_case(out, "mined", x, 2.345); mpfr_clear(x); }

        /* tcmp_d.c L49–L54: cmp_d(x=2.34763465, 2.4) < 0. */
        { mpfr_t x; init_from_double(x, 2.34763465, 53); emit_case(out, "mined", x, 2.4); mpfr_clear(x); }

        /* tcmp_d.c L56–L63: x = -0; cmp_d(x, 0.0) == 0. */
        { mpfr_t x; init_neg_zero(x, 53); emit_case(out, "mined", x, 0.0); mpfr_clear(x); }

        /* tcmp_d.c L65–L71: x = +Inf (via 1/0); cmp_d(x, 0.0) != 0 (= +1). */
        { mpfr_t x; init_pos_inf(x, 53); emit_case(out, "mined", x, 0.0); mpfr_clear(x); }
    }

    return 0;
}
