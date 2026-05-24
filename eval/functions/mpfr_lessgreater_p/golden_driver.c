/*
 * golden_driver.c — Golden master for MPFR's mpfr_lessgreater_p.
 *
 * C signature
 * -----------
 *
 *   int mpfr_lessgreater_p(mpfr_srcptr x, mpfr_srcptr y);
 *
 *   See mpfr/src/comparisons.c L63–L67. Truth-table row:
 *     =    <    >    unordered
 *     0    1    1    0
 *
 * Wire format
 * -----------
 *
 *   {"tag":"<class>",
 *    "inputs":{"a":{<MPFR>},"b":{<MPFR>}},
 *    "output":<bool>,
 *    "time_ns":<n>}
 *
 *   `output` is a bare JSON boolean via jl_output_scalar_bool.
 *
 * Tag distribution
 * ----------------
 *
 *   happy        :  ~25  (mix of <, >, == pairs)
 *   edge         :  ~50  (kind × kind, signed zero — equal, NaN — false,
 *                          ±Inf cross-sign, same-value-different-prec)
 *   adversarial  :  ~85  (equal pairs at different precs — broken
 *                          mutation 'returns c === 0' inverts these to
 *                          true; strict-ordered pairs at different
 *                          precs — broken returns false where correct
 *                          returns true)
 *   fuzz         :   60  (PRNG; mix of equal and strict-ordered to
 *                          give both sides of the mutation gate signal)
 *   mined        :    5  (transcribed from mpfr/tests/tcomparisons.c)
 *
 * Ref: mpfr/src/comparisons.c — the C reference.
 * Ref: src/ops/lessgreater_p.ts — the production port.
 */
#include "common.h"

#include <inttypes.h>
#include <string.h>

#if GMP_NUMB_BITS != 64
#  error "mpfr_lessgreater_p golden_driver requires GMP_NUMB_BITS == 64"
#endif

static inline void emit_case(FILE *out, const char *tag,
                             mpfr_srcptr a, mpfr_srcptr b) {
    const uint64_t t0 = now_ns();
    const int raw = mpfr_lessgreater_p(a, b);
    const uint64_t elapsed = now_ns() - t0;

    jl_begin(out, tag);
    jl_kv_mpfr(out, 1, "a", a);
    jl_kv_mpfr(out, 0, "b", b);
    jl_end_inputs(out);
    jl_output_scalar_bool(out, raw);
    jl_finish(out, elapsed);
}

static inline void init_from_double(mpfr_ptr x, double d, uint64_t prec) {
    mpfr_init2(x, (mpfr_prec_t)prec);
    mpfr_set_d(x, d, MPFR_RNDN);
}

static inline void init_from_str_binary(mpfr_ptr x, const char *s, uint64_t prec) {
    mpfr_init2(x, (mpfr_prec_t)prec);
    mpfr_set_str(x, s, 2, MPFR_RNDN);
}

static inline void init_pos_inf(mpfr_ptr x, uint64_t prec)  { mpfr_init2(x, (mpfr_prec_t)prec); mpfr_set_inf(x, 1); }
static inline void init_neg_inf(mpfr_ptr x, uint64_t prec)  { mpfr_init2(x, (mpfr_prec_t)prec); mpfr_set_inf(x, -1); }
static inline void init_pos_zero(mpfr_ptr x, uint64_t prec) { mpfr_init2(x, (mpfr_prec_t)prec); mpfr_set_zero(x, 1); }
static inline void init_neg_zero(mpfr_ptr x, uint64_t prec) { mpfr_init2(x, (mpfr_prec_t)prec); mpfr_set_zero(x, -1); }
static inline void init_nan(mpfr_ptr x, uint64_t prec)      { mpfr_init2(x, (mpfr_prec_t)prec); mpfr_set_nan(x); }

static inline void emit_dd(FILE *out, const char *tag,
                           double da, uint64_t pa, double db, uint64_t pb) {
    mpfr_t a, b;
    init_from_double(a, da, pa);
    init_from_double(b, db, pb);
    emit_case(out, tag, a, b);
    mpfr_clear(a); mpfr_clear(b);
}

int main(void) {
    FILE *out = stdout;

    /* ============================================================== */
    /* happy: typical mix of <, >, ==                                  */
    /* ============================================================== */
    {
        /* Strictly less → true. */
        emit_dd(out, "happy",  1.0,  53,  2.0,  53);
        emit_dd(out, "happy",  0.0,  53,  1.0,  53);
        emit_dd(out, "happy", -1.0,  53,  1.0,  53);
        emit_dd(out, "happy", -10.0, 53, -5.0,  53);
        emit_dd(out, "happy", 1e-10, 53,  1.0,  53);
        emit_dd(out, "happy", 1e9,   53,  1e10, 53);

        /* Strictly greater → true. */
        emit_dd(out, "happy",  2.0,  53,  1.0,  53);
        emit_dd(out, "happy",  1.0,  53,  0.0,  53);
        emit_dd(out, "happy",  1.0,  53, -1.0,  53);
        emit_dd(out, "happy",  -5.0, 53, -10.0, 53);
        emit_dd(out, "happy",  1.0,  53, 1e-10, 53);
        emit_dd(out, "happy",  100.0, 53, 99.0, 53);

        /* Equal → false. */
        emit_dd(out, "happy",  1.0,  53,  1.0,  53);
        emit_dd(out, "happy",  3.14, 53,  3.14, 53);
        emit_dd(out, "happy", -1.0,  53, -1.0,  53);
        emit_dd(out, "happy",  42.0, 53,  42.0, 53);
        emit_dd(out, "happy",  1e100, 53, 1e100, 53);

        /* Equal at different prec → false. */
        emit_dd(out, "happy",  1.0,  53,  1.0,  200);
        emit_dd(out, "happy",  0.5,   2,  0.5,  53);
        emit_dd(out, "happy",  1.5,   2,  1.5,  100);
        emit_dd(out, "happy",  -2.0,  1, -2.0,  256);

        /* Larger spread. */
        emit_dd(out, "happy", -100.0, 53, 100.0, 53);
        emit_dd(out, "happy", 1e-300, 53, 1e+300, 53);
        emit_dd(out, "happy", 2.71828, 53, 3.14159, 53);
        emit_dd(out, "happy", -3.14, 53, -3.14, 200);
        emit_dd(out, "happy",  6.022e23, 53, 6.022e23, 53);
    }

    /* ============================================================== */
    /* edge: kind boundaries                                           */
    /* ============================================================== */
    {
        /* Signed zero (4) — all false (cmp does not order signed zero). */
        { mpfr_t a, b; init_pos_zero(a, 53); init_neg_zero(b, 53); emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); }
        { mpfr_t a, b; init_neg_zero(a, 53); init_pos_zero(b, 53); emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); }
        { mpfr_t a, b; init_pos_zero(a, 53); init_pos_zero(b, 53); emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); }
        { mpfr_t a, b; init_neg_zero(a, 53); init_neg_zero(b, 53); emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); }

        /* Zero vs nonzero (6) — true (ordered, unequal). */
        { mpfr_t a, b; init_pos_zero(a, 53); init_from_double(b, 1.0, 53); emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); }
        { mpfr_t a, b; init_from_double(a, 1.0, 53); init_pos_zero(b, 53); emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); }
        { mpfr_t a, b; init_neg_zero(a, 53); init_from_double(b, 1.0, 53); emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); }
        { mpfr_t a, b; init_pos_zero(a, 53); init_from_double(b, -1.0, 53); emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); }
        { mpfr_t a, b; init_neg_zero(a, 53); init_from_double(b, -1.0, 53); emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); }
        { mpfr_t a, b; init_from_double(a, -1.0, 53); init_neg_zero(b, 53); emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); }

        /* ±Inf (10). */
        { mpfr_t a, b; init_pos_inf(a, 53); init_neg_inf(b, 53); emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); }
        { mpfr_t a, b; init_neg_inf(a, 53); init_pos_inf(b, 53); emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); }
        { mpfr_t a, b; init_pos_inf(a, 53); init_pos_inf(b, 53); emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); }
        { mpfr_t a, b; init_neg_inf(a, 53); init_neg_inf(b, 53); emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); }
        { mpfr_t a, b; init_pos_inf(a, 53); init_from_double(b, 1e100, 53); emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); }
        { mpfr_t a, b; init_from_double(a, 1e100, 53); init_pos_inf(b, 53); emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); }
        { mpfr_t a, b; init_neg_inf(a, 53); init_from_double(b, -1e100, 53); emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); }
        { mpfr_t a, b; init_pos_inf(a, 53); init_pos_zero(b, 53); emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); }
        { mpfr_t a, b; init_neg_inf(a, 53); init_pos_zero(b, 53); emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); }
        { mpfr_t a, b; init_pos_inf(a, 53); init_pos_inf(b, 200); emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); }

        /* NaN — always false (8). */
        { mpfr_t a, b; init_nan(a, 53); init_from_double(b, 1.0, 53); emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); }
        { mpfr_t a, b; init_from_double(a, 1.0, 53); init_nan(b, 53); emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); }
        { mpfr_t a, b; init_nan(a, 53); init_nan(b, 53); emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); }
        { mpfr_t a, b; init_nan(a, 53); init_pos_inf(b, 53); emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); }
        { mpfr_t a, b; init_nan(a, 53); init_neg_inf(b, 53); emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); }
        { mpfr_t a, b; init_nan(a, 53); init_pos_zero(b, 53); emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); }
        { mpfr_t a, b; init_pos_zero(a, 53); init_nan(b, 53); emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); }
        { mpfr_t a, b; init_nan(a, 53); init_nan(b, 200); emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); }

        /* Same value, different prec → false (6). */
        { mpfr_t a, b; init_from_double(a, 1.0, 2); init_from_double(b, 1.0, 53); emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); }
        { mpfr_t a, b; init_from_double(a, 1.0, 53); init_from_double(b, 1.0, 2); emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); }
        { mpfr_t a, b; init_from_double(a, 0.5, 1); init_from_double(b, 0.5, 200); emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); }
        { mpfr_t a, b; init_from_double(a, -2.0, 1); init_from_double(b, -2.0, 200); emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); }
        { mpfr_t a, b; init_from_double(a, 3.14, 53); init_from_double(b, 3.14, 200); emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); }
        { mpfr_t a, b; init_from_double(a, 100.0, 53); init_from_double(b, 100.0, 256); emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); }

        /* Same MSB, extra trailing bit → true (4). */
        { mpfr_t a, b;
          init_from_double(a, 1.0, 53);
          init_from_str_binary(b, "1.000000000000000000000000000000000000000000000000000001", 54);
          emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); }
        { mpfr_t a, b;
          init_from_str_binary(a, "1.000000000000000000000000000000000000000000000000000001", 54);
          init_from_double(b, 1.0, 53);
          emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); }
        { mpfr_t a, b;
          init_from_str_binary(a, "1.0000000000000000000000000000000000000000000000000000E0", 53);
          init_from_str_binary(b, "1.0000000000000000000000000000000000000000000000000001E0", 53);
          emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); }
        { mpfr_t a, b;
          init_from_str_binary(a, "1.0000000000000000000000000000000000000000000000000001E0", 53);
          init_from_str_binary(b, "1.0000000000000000000000000000000000000000000000000000E0", 53);
          emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); }

        /* prec=1 boundary (4). */
        { mpfr_t a, b; init_from_double(a, 1.0, 1); init_from_double(b, 0.5, 1); emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); }
        { mpfr_t a, b; init_from_double(a, 0.5, 1); init_from_double(b, 1.0, 1); emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); }
        { mpfr_t a, b; init_from_double(a, 1.0, 1); init_from_double(b, 1.0, 1); emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); }
        { mpfr_t a, b; init_from_double(a, 1.0, 1); init_from_double(b, 2.0, 1); emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); }

        /* Adjacent-1-ULP at high prec → true (2). */
        { mpfr_t a, b;
          mpfr_init2(a, 256); mpfr_init2(b, 256);
          mpfr_set_ui(a, 1, MPFR_RNDN);
          mpfr_set_ui(b, 1, MPFR_RNDN);
          mpfr_t eps; mpfr_init2(eps, 256);
          mpfr_set_ui_2exp(eps, 1, -255, MPFR_RNDN);
          mpfr_add(b, b, eps, MPFR_RNDN);
          mpfr_clear(eps);
          emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); }
        { mpfr_t a, b;
          mpfr_init2(a, 256); mpfr_init2(b, 256);
          mpfr_set_ui(a, 1, MPFR_RNDN); mpfr_set_ui(b, 1, MPFR_RNDN);
          mpfr_t eps; mpfr_init2(eps, 256);
          mpfr_set_ui_2exp(eps, 1, -255, MPFR_RNDN);
          mpfr_add(a, a, eps, MPFR_RNDN); mpfr_clear(eps);
          emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); }

        /* Large- and small-exp ties → true (2). */
        { mpfr_t a, b;
          init_from_str_binary(a, "1.0E1000", 53);
          init_from_str_binary(b, "1.1E1000", 53);
          emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); }
        { mpfr_t a, b;
          init_from_str_binary(a, "1.0E-1000", 53);
          init_from_str_binary(b, "1.1E-1000", 53);
          emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); }

        /* Three-bit prec sanity → true (2). */
        { mpfr_t a, b; init_from_double(a, 1.5, 3); init_from_double(b, 1.25, 3); emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); }
        { mpfr_t a, b; init_from_double(a, 1.25, 3); init_from_double(b, 1.5, 3); emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); }

        /* Subnormal-domain pair → equal-pair-different-prec → false (1). */
        { mpfr_t a, b;
          init_from_str_binary(a, "1E-1074", 53);
          init_from_str_binary(b, "1E-1074", 200);
          emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); }

        /* Edge sign-difference at equal magnitude → true (1). */
        { mpfr_t a, b; init_from_double(a, 1.5, 53); init_from_double(b, -1.5, 53);
          emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); }
    }

    /* ============================================================== */
    /* adversarial: equal-pair cross-prec + strict-ordered cross-prec  */
    /*                                                                 */
    /* Broken `c === 0` inverts equal-pair (false→true: FAIL) and      */
    /* strict-ordered (true→false: FAIL). We sweep both directions.    */
    /* ============================================================== */
    {
        /* Equal pairs across precs — broken returns true, correct false. */
        const struct { double v; uint64_t pa; uint64_t pb; } pairs[] = {
            { 1.0,   2,   53 }, { 1.0,   2,   64 }, { 1.0,   2,   200 },
            { 1.0,  53,   64 }, { 1.0,  53,  128 }, { 1.0,  64,  256 },
            { 0.5,   2,   53 }, { 0.5,  53,  128 }, { 0.5,  64,  256 },
            { 0.25,  3,   53 }, { 0.25, 53, 200 },
            { 1.5,   2,   53 }, { 1.5,  53, 200 }, { 1.5, 100, 256 },
            { 2.0,   2,   53 }, { 2.0,  53,  64 }, { 2.0,  64, 200 },
            { 3.0,   3,   53 }, { 3.0,  53, 128 },
            { 4.0,   3,   53 }, { 4.0,  53, 200 },
            { -1.0,  2,   53 }, { -1.0, 53, 128 }, { -1.0, 100, 256 },
            { -1.5,  2,   53 }, { -1.5, 53, 128 },
            { -2.0,  2,   53 }, { -2.0, 53, 128 },
            { -3.0,  3,   53 }, { -3.0, 53, 128 },
            { 1024.0, 1,  53 }, { 1024.0, 53, 256 },
            { 65536.0, 1, 53 }, { 65536.0, 64, 256 },
            { 1.0/1024.0, 1, 200 },
            { 1.25,  3,   53 }, { 1.25, 53, 128 },
            { 1.75,  3,   53 }, { 1.75, 53, 128 },
            { 1.125, 4,   53 }, { 1.125, 64, 256 },
            { 1e100, 53,  256 },
        };
        const size_t n_eq = sizeof(pairs) / sizeof(pairs[0]);
        for (size_t i = 0; i < n_eq; ++i) {
            mpfr_t a, b;
            init_from_double(a, pairs[i].v, pairs[i].pa);
            init_from_double(b, pairs[i].v, pairs[i].pb);
            emit_case(out, "adversarial", a, b);
            mpfr_clear(a); mpfr_clear(b);
        }

        /* Equal-pair set_ui sweep. */
        const struct { unsigned long ui; uint64_t pa; uint64_t pb; } uis[] = {
            { 1,    32,  64 }, { 1,    53, 128 }, { 1,    64, 256 },
            { 3,    32,  64 }, { 3,    53, 128 }, { 3,    64, 256 },
            { 5,    53, 128 }, { 7,    53, 128 }, { 11,   53, 128 },
            { 13,   53, 128 }, { 15,   53, 128 }, { 17,   53, 128 },
            { 31,   53, 128 }, { 32,   53, 128 }, { 33,   53, 128 },
            { 63,   53, 128 }, { 64,   53, 128 }, { 65,   53, 128 },
            { 127,  53, 128 }, { 128,  53, 128 }, { 1023, 53, 128 },
            { 1024, 53, 128 }, { 65535, 53, 128 }, { 65537, 53, 128 },
            { 0xDEADBEEFUL, 53, 128 }, { 0xCAFEBABEUL, 53, 128 },
        };
        const size_t n_uis = sizeof(uis) / sizeof(uis[0]);
        for (size_t i = 0; i < n_uis; ++i) {
            mpfr_t a, b;
            mpfr_init2(a, (mpfr_prec_t)uis[i].pa);
            mpfr_init2(b, (mpfr_prec_t)uis[i].pb);
            mpfr_set_ui(a, uis[i].ui, MPFR_RNDN);
            mpfr_set_ui(b, uis[i].ui, MPFR_RNDN);
            emit_case(out, "adversarial", a, b);
            mpfr_clear(a); mpfr_clear(b);
        }

        /* Strict-ordered different-prec pairs — broken `c === 0` returns
         * false (since c != 0), correct returns true. */
        const struct { double va; uint64_t pa; double vb; uint64_t pb; } strict[] = {
            { 1.0,  53, 2.0, 53 }, { 2.0,  53, 1.0, 53 },
            { 1.0,   2, 2.0, 53 }, { 1.0,  53, 2.0,   2 },
            { 0.5,   3, 1.0, 200 }, { 1.0, 200, 0.5,   3 },
            { -1.0, 53, 1.0, 53 }, { -1.0, 200, 1.0, 53 },
            { 1e100, 53, 1e-100, 53 }, { 1e-100, 53, 1e100, 53 },
            { 1024.0, 1, 2048.0, 53 }, { 0.5, 2, 1.5, 100 },
            { 3.14, 53, 2.71, 53 }, { 2.71, 53, 3.14, 200 },
            { 0.0, 53, 1.0, 53 }, { 1.0, 53, 0.0, 53 },
        };
        const size_t n_st = sizeof(strict) / sizeof(strict[0]);
        for (size_t i = 0; i < n_st; ++i) {
            mpfr_t a, b;
            init_from_double(a, strict[i].va, strict[i].pa);
            init_from_double(b, strict[i].vb, strict[i].pb);
            emit_case(out, "adversarial", a, b);
            mpfr_clear(a); mpfr_clear(b);
        }
    }

    /* ============================================================== */
    /* fuzz: 60 cases — PRNG (mix of equal and strict-ordered)         */
    /* ============================================================== */
    {
        xs64_t rng;
        xs64_seed(&rng, 0xC0FFEEC0FFEEC0FFULL);
        const uint64_t precs[5] = { 53, 64, 100, 128, 200 };

        int emitted = 0;
        int case_idx = 0;
        while (emitted < 60) {
            /* 1/3 of cases are equal pairs (broken returns true, correct
             * returns false → FAIL); 2/3 are independent pairs (mostly
             * strict-ordered since random doubles rarely collide → broken
             * returns false where correct returns true → FAIL). Both
             * paths contribute to the broken-port gate. */
            const int equal_pair = (case_idx % 3) == 0;
            case_idx++;

            const uint64_t bits_a = xs64_next(&rng);
            const uint64_t bits_b = equal_pair ? bits_a : xs64_next(&rng);

            const uint64_t exp_a = (bits_a >> 52) & 0x7FF;
            const uint64_t exp_b = (bits_b >> 52) & 0x7FF;
            if (exp_a == 0x7FF || exp_b == 0x7FF) continue;

            double da, db;
            memcpy(&da, &bits_a, sizeof da);
            memcpy(&db, &bits_b, sizeof db);

            const uint64_t pa = precs[xs64_below(&rng, 5)];
            const uint64_t pb = precs[xs64_below(&rng, 5)];

            mpfr_t a, b;
            init_from_double(a, da, pa);
            init_from_double(b, db, pb);
            emit_case(out, "fuzz", a, b);
            mpfr_clear(a); mpfr_clear(b);
            emitted++;
        }
    }

    /* ============================================================== */
    /* mined: 5 cases — transcribed from mpfr/tests/tcomparisons.c     */
    /* ============================================================== */
    {
        /* tcomparisons.c L104–L108: equal pairs → lessgreater_p false. */
        { mpfr_t a, b;
          mpfr_init2(a, 53); mpfr_init2(b, 117);
          mpfr_set_ui(a, 12345, MPFR_RNDN);
          mpfr_set_ui(b, 12345, MPFR_RNDN);
          emit_case(out, "mined", a, b);
          mpfr_clear(a); mpfr_clear(b); }

        /* tcomparisons.c L26–L84 cmp_tests for i > 2 cmp < 0 → lessgreater_p true. */
        { mpfr_t a, b;
          init_from_double(a, -1.0, 53); init_from_double(b, 1.0, 53);
          emit_case(out, "mined", a, b); mpfr_clear(a); mpfr_clear(b); }

        /* cmp > 0 → lessgreater_p true. */
        { mpfr_t a, b;
          init_from_double(a, 2.0, 53); init_from_double(b, 1.0, 53);
          emit_case(out, "mined", a, b); mpfr_clear(a); mpfr_clear(b); }

        /* cmp_tests NaN guard (i <= 2): NaN x → lessgreater_p false. */
        { mpfr_t a, b; init_nan(a, 53); init_from_double(b, 1.0, 53);
          emit_case(out, "mined", a, b); mpfr_clear(a); mpfr_clear(b); }

        /* cmp == 0 → lessgreater_p false. */
        { mpfr_t a, b; init_from_double(a, 1.5, 53); init_from_double(b, 1.5, 53);
          emit_case(out, "mined", a, b); mpfr_clear(a); mpfr_clear(b); }
    }

    return 0;
}
