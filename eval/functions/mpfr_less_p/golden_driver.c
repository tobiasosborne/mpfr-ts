/*
 * golden_driver.c — Golden master for MPFR's mpfr_less_p.
 *
 * C signature
 * -----------
 *
 *   int mpfr_less_p(mpfr_srcptr x, mpfr_srcptr y);
 *
 *   Returns non-zero iff x < y, zero otherwise. NaN-vs-anything is
 *   zero (the "unordered" case). See mpfr/src/comparisons.c L52–L55:
 *
 *     int mpfr_less_p (mpfr_srcptr x, mpfr_srcptr y) {
 *       return MPFR_IS_NAN(x) || MPFR_IS_NAN(y) ? 0 : (mpfr_cmp(x, y) < 0);
 *     }
 *
 * Divergence from C → TS
 * ----------------------
 *
 * The TS port `mpfr_less_p(a, b) -> boolean` takes two immutable MPFR
 * structs and returns a TS `boolean`. UNLIKE mpfr_cmp, the predicate
 * family does NOT throw on NaN — both the C and TS sides return
 * false/0 for the "unordered" case. The golden CAN therefore include
 * NaN cases, and the TS port must return the expected `false` value.
 *
 * Wire format
 * -----------
 *
 *   {"tag":"<class>",
 *    "inputs":{"a":{<MPFR>},"b":{<MPFR>}},
 *    "output":<bool>,
 *    "time_ns":<n>}
 *
 *   `output` is a bare JSON boolean emitted by jl_output_scalar_bool.
 *
 * Tag distribution (CLAUDE.md Rule 7 minimums)
 * --------------------------------------------
 *
 *   happy        :  ~25   (typical pairs, balanced outcomes)
 *   edge         :  ~50   (kind×kind, signed zero, Inf-vs-finite,
 *                          NaN, same-value-different-prec)
 *   adversarial  :  ~85   (exponent ties, mantissa alignment)
 *   fuzz         :   60   (PRNG, seed 0xC0FFEEC0FFEEC0FFULL)
 *   mined        :    5   (transcribed from mpfr/tests/tcomparisons.c)
 *
 * Build via eval/golden_master/build.sh.
 *
 * Ref: mpfr/src/comparisons.c — the C reference.
 * Ref: src/ops/less_p.ts — the production port.
 */
#include "common.h"

#include <assert.h>
#include <inttypes.h>
#include <string.h>

#if GMP_NUMB_BITS != 64
#  error "mpfr_less_p golden_driver requires GMP_NUMB_BITS == 64"
#endif

/* Emit one mpfr_less_p case. */
static inline void emit_case(FILE *out, const char *tag,
                             mpfr_srcptr a, mpfr_srcptr b) {
    const uint64_t t0 = now_ns();
    const int raw = mpfr_less_p(a, b);
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
    /* happy: ~25 cases — typical pairs                                */
    /* ============================================================== */
    {
        /* Strictly less / greater / equal at same prec. */
        emit_dd(out, "happy",  1.0,  53,  2.0,  53);   /* true */
        emit_dd(out, "happy",  2.0,  53,  1.0,  53);   /* false */
        emit_dd(out, "happy",  1.0,  53,  1.0,  53);   /* false (equal) */
        emit_dd(out, "happy",  3.0,  53, 10.0,  53);
        emit_dd(out, "happy", 10.0,  53,  3.0,  53);
        emit_dd(out, "happy",  1.5e100, 53, 1.5e101, 53);
        emit_dd(out, "happy",  1.5e101, 53, 1.5e100, 53);
        emit_dd(out, "happy",  1.5e-100, 53, 1.5e-101, 53);

        /* Cross-sign — sign settles it. */
        emit_dd(out, "happy",  1.0,   53, -1.0,  53);
        emit_dd(out, "happy", -1.0,   53,  1.0,  53);
        emit_dd(out, "happy",  2.5,   53, -7.0,  53);
        emit_dd(out, "happy", -100.0, 53,  0.01, 53);

        /* Various exponent magnitudes. */
        emit_dd(out, "happy",  0.5,   53,  0.25, 53);
        emit_dd(out, "happy",  0.25,  53,  0.5,  53);
        emit_dd(out, "happy",  1e9,   53,  1e10, 53);
        emit_dd(out, "happy",  1e-9,  53,  1e-10, 53);
        emit_dd(out, "happy",  6.022e23, 53, 6.022e23, 53);
        emit_dd(out, "happy",  2.718281828459045, 53,
                              3.141592653589793, 53);

        /* Larger-difference pairs (negative). */
        emit_dd(out, "happy", -1.5,   53, -2.0,  53);
        emit_dd(out, "happy", -2.0,   53, -1.5,  53);
        emit_dd(out, "happy",  42.0,  53,  41.0, 53);
        emit_dd(out, "happy",  41.0,  53,  42.0, 53);
        emit_dd(out, "happy",  3.14,  53,  3.14, 53);   /* false (equal) */
        emit_dd(out, "happy", -3.14,  53, -3.14, 53);   /* false (equal) */
        emit_dd(out, "happy",  0.0,   53,  0.0,  53);   /* false (equal +0/+0) */
    }

    /* ============================================================== */
    /* edge: ~50 cases — kind × kind, signed zero, NaN, etc.          */
    /* ============================================================== */
    {
        /* Signed-zero pairs (all return false: 0/0 is equal). */
        { mpfr_t a, b; init_pos_zero(a, 53); init_neg_zero(b, 53); emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); }
        { mpfr_t a, b; init_neg_zero(a, 53); init_pos_zero(b, 53); emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); }
        { mpfr_t a, b; init_pos_zero(a, 53); init_pos_zero(b, 53); emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); }
        { mpfr_t a, b; init_neg_zero(a, 53); init_neg_zero(b, 53); emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); }
        /* Zero vs normal. */
        { mpfr_t a, b; init_pos_zero(a, 53); init_from_double(b, 1.0, 53); emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); }
        { mpfr_t a, b; init_from_double(a, 1.0, 53); init_pos_zero(b, 53); emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); }
        { mpfr_t a, b; init_neg_zero(a, 53); init_from_double(b, 1.0, 53); emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); }
        { mpfr_t a, b; init_pos_zero(a, 53); init_from_double(b, -1.0, 53); emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); }
        { mpfr_t a, b; init_neg_zero(a, 53); init_from_double(b, -1.0, 53); emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); }
        { mpfr_t a, b; init_from_double(a, -1.0, 53); init_neg_zero(b, 53); emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); }

        /* Inf pairs. */
        { mpfr_t a, b; init_pos_inf(a, 53); init_neg_inf(b, 53); emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); }
        { mpfr_t a, b; init_neg_inf(a, 53); init_pos_inf(b, 53); emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); }
        { mpfr_t a, b; init_pos_inf(a, 53); init_pos_inf(b, 53); emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); }
        { mpfr_t a, b; init_neg_inf(a, 53); init_neg_inf(b, 53); emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); }
        { mpfr_t a, b; init_pos_inf(a, 53); init_from_double(b, 1.0e100, 53); emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); }
        { mpfr_t a, b; init_from_double(a, 1.0e100, 53); init_pos_inf(b, 53); emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); }
        { mpfr_t a, b; init_neg_inf(a, 53); init_from_double(b, 1.0e-100, 53); emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); }
        { mpfr_t a, b; init_neg_inf(a, 53); init_from_double(b, -1.0e100, 53); emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); }
        { mpfr_t a, b; init_pos_inf(a, 53); init_pos_zero(b, 53); emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); }
        { mpfr_t a, b; init_pos_inf(a, 53); init_neg_zero(b, 53); emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); }
        { mpfr_t a, b; init_neg_inf(a, 53); init_pos_zero(b, 53); emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); }
        { mpfr_t a, b; init_neg_inf(a, 53); init_neg_zero(b, 53); emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); }

        /* NaN cases — predicate returns false for ALL. KEY: predicate
         * golden DOES include NaN (unlike mpfr_cmp). */
        { mpfr_t a, b; init_nan(a, 53); init_from_double(b, 1.0, 53); emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); }
        { mpfr_t a, b; init_from_double(a, 1.0, 53); init_nan(b, 53); emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); }
        { mpfr_t a, b; init_nan(a, 53); init_nan(b, 53); emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); }
        { mpfr_t a, b; init_nan(a, 53); init_pos_inf(b, 53); emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); }
        { mpfr_t a, b; init_nan(a, 53); init_neg_inf(b, 53); emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); }
        { mpfr_t a, b; init_nan(a, 53); init_pos_zero(b, 53); emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); }
        { mpfr_t a, b; init_pos_zero(a, 53); init_nan(b, 53); emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); }
        { mpfr_t a, b; init_nan(a, 53); init_from_double(b, -1.0, 53); emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); }

        /* Same value, different prec. */
        { mpfr_t a, b; init_from_double(a, 1.0, 2);  init_from_double(b, 1.0, 53);  emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); }
        { mpfr_t a, b; init_from_double(a, 1.0, 53); init_from_double(b, 1.0, 2);   emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); }
        { mpfr_t a, b; init_from_double(a, 1.5, 2);  init_from_double(b, 1.5, 53);  emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); }
        { mpfr_t a, b; init_from_double(a, 0.5, 1);  init_from_double(b, 0.5, 200); emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); }
        { mpfr_t a, b; init_from_double(a, -2.0, 1); init_from_double(b, -2.0, 200); emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); }

        /* Boundary: equal MSB-aligned mantissa value, higher-prec has extra bit. */
        { mpfr_t a, b;
          init_from_double(a, 1.0, 53);
          init_from_str_binary(b, "1.000000000000000000000000000000000000000000000000000001", 54);
          emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); }
        { mpfr_t a, b;
          init_from_str_binary(a, "1.000000000000000000000000000000000000000000000000000001", 54);
          init_from_double(b, 1.0, 53);
          emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); }

        /* prec=1 boundary. */
        { mpfr_t a, b; init_from_double(a, 1.0, 1); init_from_double(b, 0.5, 1); emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); }
        { mpfr_t a, b; init_from_double(a, 0.5, 1); init_from_double(b, 1.0, 1); emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); }
        { mpfr_t a, b; init_from_double(a, 1.0, 1); init_from_double(b, 1.0, 1); emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); }

        /* Adjacent exponents and 1-ULP differences. */
        { mpfr_t a, b; init_from_double(a, 1024.0, 53); init_from_double(b, 2048.0, 53); emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); }
        { mpfr_t a, b;
          init_from_str_binary(a, "1.0000000000000000000000000000000000000000000000000000E0", 53);
          init_from_str_binary(b, "1.0000000000000000000000000000000000000000000000000001E0", 53);
          emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); }
        { mpfr_t a, b;
          init_from_str_binary(a, "1.0000000000000000000000000000000000000000000000000001E0", 53);
          init_from_str_binary(b, "1.0000000000000000000000000000000000000000000000000000E0", 53);
          emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); }

        /* Large-exponent ties. */
        { mpfr_t a, b;
          init_from_str_binary(a, "1.0E1000", 53);
          init_from_str_binary(b, "1.1E1000", 53);
          emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); }
        { mpfr_t a, b;
          init_from_str_binary(a, "1.0E-1000", 53);
          init_from_str_binary(b, "1.1E-1000", 53);
          emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); }

        /* Inf at different precs. */
        { mpfr_t a, b; init_pos_inf(a, 53); init_pos_inf(b, 200); emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); }
        { mpfr_t a, b; init_pos_zero(a, 53); init_neg_zero(b, 200); emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); }

        /* Three-bit prec sanity. */
        { mpfr_t a, b; init_from_double(a, 1.5, 3);  init_from_double(b, 1.25, 3); emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); }
        { mpfr_t a, b; init_from_double(a, 1.25, 3); init_from_double(b, 1.5, 3); emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); }

        /* Same value across larger precs. */
        { mpfr_t a, b; init_from_double(a, 3.14, 53); init_from_double(b, 3.14, 200); emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); }
        { mpfr_t a, b; init_from_double(a, 4.0, 53); init_from_double(b, 8.0, 53); emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); }
        { mpfr_t a, b; init_from_double(a, -4.0, 53); init_from_double(b, -8.0, 53); emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); }
        { mpfr_t a, b; init_from_double(a, 1e-300, 53); init_from_double(b, 1e+300, 53); emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); }
        { mpfr_t a, b; init_from_double(a, -1e-300, 53); init_from_double(b, -1e+300, 53); emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); }
        { mpfr_t a, b; init_neg_zero(a, 53); init_pos_inf(b, 53); emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); }
    }

    /* ============================================================== */
    /* adversarial: exponent ties + mantissa alignment + same-value-  */
    /* different-prec sweep. The same-value-different-prec mass is     */
    /* there to make the "drops the alignment shift" mutation visible — */
    /* though for less_p the broken port we ship is `!actual`, which   */
    /* fails on every case (much stronger).                            */
    /* ============================================================== */
    {
        /* Same value via set_d at different prec — equal pair, less_p false. */
        const struct { double v; uint64_t pa; uint64_t pb; } pairs[] = {
            { 1.0,   2,   53 }, { 1.0,   2,   64 }, { 1.0,   2,   128 },
            { 1.0,  53,  100 }, { 1.0,  53,  128 }, { 1.0,  64,  256 },
            { 0.5,   2,   53 }, { 0.5,  53,  100 }, { 0.5,  64,  256 },
            { 0.25,  3,   53 }, { 0.25, 53, 200 },  { 0.25, 64, 256 },
            { 1.5,   2,   53 }, { 1.5,  53, 200 },  { 1.5, 100, 256 },
            { 2.0,   2,   53 }, { 2.0,  53,  64 },  { 2.0,  64, 200 },
            { 3.0,   3,   53 }, { 3.0,  53,  64 },  { 3.0,  64, 256 },
            { 4.0,   3,   53 }, { 4.0,  53, 200 },  { 4.0,  64, 256 },
            { -1.0,  2,   53 }, { -1.0, 53, 128 },  { -1.0, 100, 256 },
            { -1.5,  2,   53 }, { -1.5, 53, 128 },  { -1.5,  64, 256 },
            { -2.0,  2,   53 }, { -2.0, 53, 128 },  { -2.0,  64, 256 },
            { -3.0,  3,   53 }, { -3.0, 53, 128 },  { -3.0,  64, 256 },
            { 1024.0,    1,  53 }, { 1024.0,  53, 256 },
            { 65536.0,   1,  53 }, { 65536.0, 64, 256 },
            { 1048576.0, 1,  53 }, { 1.0/1024.0, 1, 200 },
            { 1.25,  3,   53 }, { 1.25, 53, 128 },  { 1.25,  64, 256 },
            { 1.75,  3,   53 }, { 1.75, 53, 128 },  { 1.75,  64, 256 },
        };
        const size_t n_pairs = sizeof(pairs) / sizeof(pairs[0]);
        for (size_t i = 0; i < n_pairs; ++i) {
            mpfr_t a, b;
            init_from_double(a, pairs[i].v, pairs[i].pa);
            init_from_double(b, pairs[i].v, pairs[i].pb);
            emit_case(out, "adversarial", a, b);
            mpfr_clear(a); mpfr_clear(b);
        }

        /* set_ui sweep at different precs — also equal pairs. */
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

        /* Adjacent-by-1-ULP at very high prec. */
        { mpfr_t a, b;
          mpfr_init2(a, 256); mpfr_init2(b, 256);
          mpfr_set_ui(a, 1, MPFR_RNDN);
          mpfr_set_ui(b, 1, MPFR_RNDN);
          mpfr_t eps; mpfr_init2(eps, 256);
          mpfr_set_ui_2exp(eps, 1, -255, MPFR_RNDN);
          mpfr_add(b, b, eps, MPFR_RNDN);
          mpfr_clear(eps);
          emit_case(out, "adversarial", a, b);
          mpfr_clear(a); mpfr_clear(b); }
        { mpfr_t a, b;
          mpfr_init2(a, 256); mpfr_init2(b, 256);
          mpfr_set_ui(a, 1, MPFR_RNDN);
          mpfr_set_ui(b, 1, MPFR_RNDN);
          mpfr_t eps; mpfr_init2(eps, 256);
          mpfr_set_ui_2exp(eps, 1, -255, MPFR_RNDN);
          mpfr_add(a, a, eps, MPFR_RNDN);
          mpfr_clear(eps);
          emit_case(out, "adversarial", a, b);
          mpfr_clear(a); mpfr_clear(b); }

        /* Very large / very small exponent ties. */
        { mpfr_t a, b;
          init_from_str_binary(a, "1.0E1000000", 64);
          init_from_str_binary(b, "1.1E1000000", 64);
          emit_case(out, "adversarial", a, b);
          mpfr_clear(a); mpfr_clear(b); }
        { mpfr_t a, b;
          init_from_str_binary(a, "1.0E-1000000", 64);
          init_from_str_binary(b, "1.1E-1000000", 64);
          emit_case(out, "adversarial", a, b);
          mpfr_clear(a); mpfr_clear(b); }

        /* Same magnitude, opposite sign. */
        { mpfr_t a, b;
          init_from_str_binary(a,  "1.0000000000000000000000000000000000000000000000000001E50", 200);
          init_from_str_binary(b, "-1.0000000000000000000000000000000000000000000000000001E50", 200);
          emit_case(out, "adversarial", a, b);
          mpfr_clear(a); mpfr_clear(b); }

        /* Tiny exponent difference. */
        { mpfr_t a, b;
          init_from_str_binary(a, "1.1111111111111111111111111111111111111111111111111111E10", 53);
          init_from_str_binary(b, "1.0000000000000000000000000000000000000000000000000000E11", 53);
          emit_case(out, "adversarial", a, b);
          mpfr_clear(a); mpfr_clear(b); }
    }

    /* ============================================================== */
    /* fuzz: 60 cases — PRNG-driven                                   */
    /*                                                                 */
    /* Mix is deliberate: roughly half the cases are TWO independent  */
    /* random doubles (testing strict-order outcomes), the other half */
    /* are EQUAL PAIRS (same source value at potentially different    */
    /* precs — testing the equality / NaN / etc. outcomes). The mix   */
    /* is what makes the broken-port mutation gate work for           */
    /* lessequal_p / greaterequal_p / equal_p: those mutations only   */
    /* differ from the correct port on equal pairs, so a fuzz stream  */
    /* with no equal pairs would let the broken port pass every fuzz */
    /* case by coincidence (random doubles essentially never collide). */
    /* With ~30 equal-pair cases in fuzz, the broken `equal_p` port   */
    /* (returns false unconditionally) fails them all, contributing   */
    /* ~30 corr-class misses; combined with the adversarial-class     */
    /* same-value-different-prec sweep, composite drops below 0.5.    */
    /*                                                                 */
    /* This is also closer to how the mpfr/tests/tcomparisons.c       */
    /* eq_tests routine exercises predicates: it generates the same   */
    /* mpfr_t at two different precs and asserts every predicate's    */
    /* equality-aware response is correct. We replicate that pattern  */
    /* here with a much higher case count.                             */
    /* ============================================================== */
    {
        xs64_t rng;
        xs64_seed(&rng, 0xC0FFEEC0FFEEC0FFULL);
        const uint64_t precs[5] = { 53, 64, 100, 128, 200 };

        int emitted = 0;
        int case_idx = 0;
        while (emitted < 60) {
            /* Mix: case_idx % 3 == 0 → independent random pair (testing
             * strict-order responses), case_idx % 3 != 0 → equal pair
             * (testing equality / NaN responses). This 2:1 equal-pair
             * skew calibrates the broken-port mutation gate for the
             * lessequal_p / greaterequal_p / equal_p brokens, which only
             * differ from the correct port on equal-pair cases. With
             * ~40 equal-pair fuzz cases, the broken equality predicates
             * fail enough of the corr-class to push composite below
             * the 0.5 mutation-gate ceiling.
             *
             * The 1/3 of independent pairs is enough to exercise the
             * sign-and-exponent dispatch branches the predicates
             * inherit from mpfr_cmp — those are well-covered in
             * adversarial too, so the slimmer fuzz independent-pair
             * count is fine. */
            const int equal_pair = (case_idx % 3) != 0;
            case_idx++;

            const uint64_t bits_a = xs64_next(&rng);
            const uint64_t bits_b = equal_pair ? bits_a : xs64_next(&rng);

            /* Skip NaN/Inf double-bit patterns. */
            const uint64_t exp_a = (bits_a >> 52) & 0x7FF;
            const uint64_t exp_b = (bits_b >> 52) & 0x7FF;
            if (exp_a == 0x7FF || exp_b == 0x7FF) continue;

            double da, db;
            memcpy(&da, &bits_a, sizeof da);
            memcpy(&db, &bits_b, sizeof db);

            const uint64_t pa = precs[xs64_below(&rng, 5)];
            const uint64_t pb = precs[xs64_below(&rng, 5)];
            /* Note: a double's 53 significant bits round-trip exactly
             * through any prec >= 53 (set_d is exact). All precs[] entries
             * are >= 53, so for equal-pair cases the (da, pa) and (db, pb)
             * MPFR values represent the same number regardless of
             * pa-vs-pb ordering — they just happen to be MSB-aligned to
             * different prec widths. */

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
        /* tcomparisons.c L87–L117 eq_tests: same value at different
         * precs must satisfy less_p == false. */
        { mpfr_t a, b;
          mpfr_init2(a, 53); mpfr_init2(b, 117);  /* 53 + 64 */
          mpfr_set_ui(a, 12345, MPFR_RNDN);
          mpfr_set_ui(b, 12345, MPFR_RNDN);
          emit_case(out, "mined", a, b);
          mpfr_clear(a); mpfr_clear(b); }

        /* tcomparisons.c L26–L84 cmp_tests: random pairs, NaN guard
         * (i <= 2). NaN vs anything → false. */
        { mpfr_t a, b;
          init_nan(a, 53);
          init_nan(b, 53);
          emit_case(out, "mined", a, b);
          mpfr_clear(a); mpfr_clear(b); }

        /* cmp_tests: NaN vs finite → false. */
        { mpfr_t a, b;
          init_nan(a, 53);
          init_from_double(b, 1.5, 53);
          emit_case(out, "mined", a, b);
          mpfr_clear(a); mpfr_clear(b); }

        /* cmp_tests: finite vs NaN → false. */
        { mpfr_t a, b;
          init_from_double(a, 1.5, 53);
          init_nan(b, 53);
          emit_case(out, "mined", a, b);
          mpfr_clear(a); mpfr_clear(b); }

        /* cmp_tests: cmp < 0 ⇒ less_p true. */
        { mpfr_t a, b;
          init_from_double(a, -1.0, 53);
          init_from_double(b,  1.0, 53);
          emit_case(out, "mined", a, b);
          mpfr_clear(a); mpfr_clear(b); }
    }

    return 0;
}
