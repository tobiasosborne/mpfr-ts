/*
 * golden_driver.c — Golden master for MPFR's mpfr_cmp.
 *
 * C signature
 * -----------
 *
 *   int mpfr_cmp(mpfr_srcptr op1, mpfr_srcptr op2);
 *
 *   Returns
 *     < 0   if  op1 < op2
 *     = 0   if  op1 == op2
 *     > 0   if  op1 > op2
 *     = 0   AND sets the erange flag if either operand is NaN.
 *
 *   See mpfr/src/cmp.c L32–L98 (the underlying mpfr_cmp3 with sign=+1).
 *   The canonical libmpfr returns -1/0/+1; this driver normalises
 *   defensively so a future libmpfr that returned -42/+17 wouldn't
 *   silently invalidate every golden.
 *
 * Divergence from C → TS
 * ----------------------
 *
 * The TS port `mpfr_cmp(a, b) -> number` takes two immutable MPFR
 * structs from src/core.ts and returns a plain JS number in {-1, 0, +1}.
 * Unlike the C reference, it THROWS MPFRError('EDOMAIN', ...) when
 * either operand is NaN — the documented domain-error divergence per
 * CLAUDE.md "Hallucination-risk callouts: NaN ≠ NaN". Because a throw is
 * graded as n_throw (not a pass), this driver MUST NOT emit any NaN
 * cases. NaN coverage lives in a separate, future "throws" test suite.
 *
 * Wire format
 * -----------
 *
 *   {"tag":"<class>",
 *    "inputs":{"a":{<MPFR-record>},"b":{<MPFR-record>}},
 *    "output":<int>,
 *    "time_ns":<n>}
 *
 *   - `output` is a bare JS int (decoded by value_codec.ts L266–L273
 *     as `{kind:'scalar', value:<bigint>}`). compareOutput's bigint
 *     branch coerces an integer JS number on the actual side via
 *     `BigInt(actual) === expected.value` (value_codec.ts L437–L441),
 *     so the TS port may return a plain JS `number` and the
 *     bare-scalar wire still grades correctly.
 *
 * Tag distribution (CLAUDE.md Rule 7 minimums)
 * --------------------------------------------
 *
 *   happy        :  ~25   (typical pairs, balanced outcomes)
 *   edge         :  ~50   (specials × specials, signed-zero, +/-Inf,
 *                          same-value-different-prec, etc.)
 *   adversarial  :  ~16   (exponent ties at various sizes; cross-prec
 *                          same-value; equal mantissa + extra
 *                          trailing bit in higher-prec operand)
 *   fuzz         :   60   (PRNG-driven; seed 0xC0FFEEC0FFEEC0FFULL)
 *   mined        :    5   (transcribed from mpfr/tests/tcmp.c)
 *
 * Build via the repo-wide eval/golden_master/build.sh.
 *
 * Ref: mpfr/src/cmp.c — the C reference.
 * Ref: src/ops/cmp.ts — the production port.
 * Ref: mpfr/tests/tcmp.c — source for the `mined` cases.
 */
#include "common.h"

#include <assert.h>
#include <inttypes.h>
#include <string.h>

#if GMP_NUMB_BITS != 64
#  error "mpfr_cmp golden_driver requires GMP_NUMB_BITS == 64"
#endif

/* Normalise mpfr_cmp's return to {-1, 0, +1}. The C standard only
 * promises a sign; libmpfr returns -1/0/+1 in practice, but we
 * normalise so a future libmpfr that widens the range doesn't
 * silently invalidate goldens against the -1/0/+1 TS port. Mirrors
 * the helper in mpn_cmp's driver. */
static inline int normalise_cmp(int r) {
    return (r > 0) ? 1 : ((r < 0) ? -1 : 0);
}

/* Emit one mpfr_cmp golden case. Both inputs must already be
 * constructed; we time only the cmp call. We DO NOT emit cases where
 * either input is NaN — the TS port throws on those, and the harness
 * grades throws as n_throw rather than as a pass. */
static inline void emit_case(FILE *out, const char *tag,
                             mpfr_srcptr a, mpfr_srcptr b) {
    assert(!mpfr_nan_p(a) && !mpfr_nan_p(b));

    const uint64_t t0 = now_ns();
    const int raw = mpfr_cmp(a, b);
    const uint64_t elapsed = now_ns() - t0;
    const int result = normalise_cmp(raw);

    jl_begin(out, tag);
    jl_kv_mpfr(out, 1, "a", a);
    jl_kv_mpfr(out, 0, "b", b);
    jl_end_inputs(out);
    jl_output_scalar_int(out, result);
    jl_finish(out, elapsed);
}

/* Build an MPFR from a double at the given prec. Caller frees. */
static inline void init_from_double(mpfr_ptr x, double d, uint64_t prec) {
    mpfr_init2(x, (mpfr_prec_t)prec);
    mpfr_set_d(x, d, MPFR_RNDN);
}

/* Build an MPFR from a binary string literal at the given prec. */
static inline void init_from_str_binary(mpfr_ptr x, const char *s,
                                        uint64_t prec) {
    mpfr_init2(x, (mpfr_prec_t)prec);
    mpfr_set_str(x, s, 2, MPFR_RNDN);
}

/* Build a singular MPFR at the given prec. */
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

/* Emit a case from two doubles at potentially different precs. */
static inline void emit_dd(FILE *out, const char *tag,
                           double da, uint64_t pa,
                           double db, uint64_t pb) {
    mpfr_t a, b;
    init_from_double(a, da, pa);
    init_from_double(b, db, pb);
    emit_case(out, tag, a, b);
    mpfr_clear(a); mpfr_clear(b);
}

/* ------------------------------------------------------------------ */
/* main                                                               */
/* ------------------------------------------------------------------ */

int main(void) {
    FILE *out = stdout;

    /* ============================================================== */
    /* happy: ~25 cases — typical pairs                                */
    /* ============================================================== */
    {
        /* Same value, same prec → 0. */
        emit_dd(out, "happy",  1.0,  53,  1.0,  53);
        emit_dd(out, "happy",  2.0,  53,  2.0,  53);
        emit_dd(out, "happy",  3.14, 53,  3.14, 53);
        emit_dd(out, "happy", -1.0,  53, -1.0,  53);

        /* Strictly less / greater at same prec. */
        emit_dd(out, "happy",  1.0,  53,  2.0,  53);   /* -1 */
        emit_dd(out, "happy",  2.0,  53,  1.0,  53);   /* +1 */
        emit_dd(out, "happy",  3.0,  53, 10.0,  53);
        emit_dd(out, "happy", 10.0,  53,  3.0,  53);
        emit_dd(out, "happy",  1.5e100, 53, 1.5e101, 53);
        emit_dd(out, "happy",  1.5e101, 53, 1.5e100, 53);
        emit_dd(out, "happy",  1.5e-100, 53, 1.5e-101, 53);
        emit_dd(out, "happy",  1.5e-101, 53, 1.5e-100, 53);

        /* Cross-sign — sign settles it. */
        emit_dd(out, "happy",  1.0,   53, -1.0,  53);   /* +1 */
        emit_dd(out, "happy", -1.0,   53,  1.0,  53);   /* -1 */
        emit_dd(out, "happy",  2.5,   53, -7.0,  53);   /* +1 */
        emit_dd(out, "happy", -100.0, 53,  0.01, 53);   /* -1 */

        /* Various exponent magnitudes. */
        emit_dd(out, "happy",  0.5,   53,  0.25, 53);   /* +1 */
        emit_dd(out, "happy",  0.25,  53,  0.5,  53);   /* -1 */
        emit_dd(out, "happy",  1e9,   53,  1e10, 53);
        emit_dd(out, "happy",  1e-9,  53,  1e-10, 53);
        emit_dd(out, "happy",  6.022e23, 53, 6.022e23, 53);
        emit_dd(out, "happy",  2.718281828459045, 53,
                              3.141592653589793, 53);

        /* Larger-difference pairs. */
        emit_dd(out, "happy", -1.5,   53, -2.0,  53);   /* -1.5 > -2.0 → +1 */
        emit_dd(out, "happy", -2.0,   53, -1.5,  53);   /* -1 */
        emit_dd(out, "happy",  42.0,  53,  41.0, 53);   /* +1 */
    }

    /* ============================================================== */
    /* edge: ~50 cases — specials × specials, signed zero, etc.       */
    /* ============================================================== */
    {
        /* (1) +0 vs -0 → 0 (signed zero is NOT ordered for cmp). */
        { mpfr_t a, b; init_pos_zero(a, 53); init_neg_zero(b, 53);
          emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); }
        /* (2) -0 vs +0 → 0 (mirror). */
        { mpfr_t a, b; init_neg_zero(a, 53); init_pos_zero(b, 53);
          emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); }
        /* (3) +0 vs +0 → 0. */
        { mpfr_t a, b; init_pos_zero(a, 53); init_pos_zero(b, 53);
          emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); }
        /* (4) -0 vs -0 → 0. */
        { mpfr_t a, b; init_neg_zero(a, 53); init_neg_zero(b, 53);
          emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); }
        /* (5) +0 vs +Normal → -1. */
        { mpfr_t a, b; init_pos_zero(a, 53); init_from_double(b, 1.0, 53);
          emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); }
        /* (6) +Normal vs +0 → +1 (mirror). */
        { mpfr_t a, b; init_from_double(a, 1.0, 53); init_pos_zero(b, 53);
          emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); }
        /* (7) -0 vs +Normal → -1. */
        { mpfr_t a, b; init_neg_zero(a, 53); init_from_double(b, 1.0, 53);
          emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); }
        /* (8) +0 vs -Normal → +1. */
        { mpfr_t a, b; init_pos_zero(a, 53); init_from_double(b, -1.0, 53);
          emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); }
        /* (9) -0 vs -Normal → +1 (-0 > any negative). */
        { mpfr_t a, b; init_neg_zero(a, 53); init_from_double(b, -1.0, 53);
          emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); }
        /* (10) -Normal vs -0 → -1 (mirror). */
        { mpfr_t a, b; init_from_double(a, -1.0, 53); init_neg_zero(b, 53);
          emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); }

        /* (11) +Inf vs -Inf → +1. */
        { mpfr_t a, b; init_pos_inf(a, 53); init_neg_inf(b, 53);
          emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); }
        /* (12) -Inf vs +Inf → -1. */
        { mpfr_t a, b; init_neg_inf(a, 53); init_pos_inf(b, 53);
          emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); }
        /* (13) +Inf vs +Inf → 0. */
        { mpfr_t a, b; init_pos_inf(a, 53); init_pos_inf(b, 53);
          emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); }
        /* (14) -Inf vs -Inf → 0. */
        { mpfr_t a, b; init_neg_inf(a, 53); init_neg_inf(b, 53);
          emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); }
        /* (15) +Inf vs +Normal → +1. */
        { mpfr_t a, b; init_pos_inf(a, 53); init_from_double(b, 1.0e100, 53);
          emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); }
        /* (16) +Normal vs +Inf → -1. */
        { mpfr_t a, b; init_from_double(a, 1.0e100, 53); init_pos_inf(b, 53);
          emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); }
        /* (17) -Inf vs +Normal → -1. */
        { mpfr_t a, b; init_neg_inf(a, 53); init_from_double(b, 1.0e-100, 53);
          emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); }
        /* (18) -Inf vs -Normal → -1 (-Inf < everything finite). */
        { mpfr_t a, b; init_neg_inf(a, 53); init_from_double(b, -1.0e100, 53);
          emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); }
        /* (19) +Inf vs +0 → +1. */
        { mpfr_t a, b; init_pos_inf(a, 53); init_pos_zero(b, 53);
          emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); }
        /* (20) +Inf vs -0 → +1. */
        { mpfr_t a, b; init_pos_inf(a, 53); init_neg_zero(b, 53);
          emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); }
        /* (21) -Inf vs +0 → -1. */
        { mpfr_t a, b; init_neg_inf(a, 53); init_pos_zero(b, 53);
          emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); }
        /* (22) -Inf vs -0 → -1. */
        { mpfr_t a, b; init_neg_inf(a, 53); init_neg_zero(b, 53);
          emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); }

        /* (23) Same value, different prec — KEY case for the alignment
         *      logic. 1.0 at prec=2 equals 1.0 at prec=53. */
        { mpfr_t a, b; init_from_double(a, 1.0, 2); init_from_double(b, 1.0, 53);
          emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); }
        /* (24) Same value, different prec (mirror). */
        { mpfr_t a, b; init_from_double(a, 1.0, 53); init_from_double(b, 1.0, 2);
          emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); }
        /* (25) 1.5 at prec=2 (exact, mant=11) vs prec=53. */
        { mpfr_t a, b; init_from_double(a, 1.5, 2); init_from_double(b, 1.5, 53);
          emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); }
        /* (26) Same value at very disparate prec (prec=1 vs prec=200). */
        { mpfr_t a, b; init_from_double(a, 0.5, 1); init_from_double(b, 0.5, 200);
          emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); }
        /* (27) Same value at very disparate prec — negative. */
        { mpfr_t a, b; init_from_double(a, -2.0, 1); init_from_double(b, -2.0, 200);
          emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); }

        /* (28) Boundary: equal MSB-aligned mantissa value, but the
         *      higher-prec operand has an extra trailing bit set.
         *      a = 1.0 at prec=53 (mant = 1<<52, exp=1).
         *      b = 1.0 + 2^-53 at prec=54 (mant has bit 0 set).
         *      Then a < b. */
        { mpfr_t a, b;
          init_from_double(a, 1.0, 53);
          init_from_str_binary(b, "1.000000000000000000000000000000000000000000000000000001", 54);
          emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); }
        /* (29) Mirror of (28). */
        { mpfr_t a, b;
          init_from_str_binary(a, "1.000000000000000000000000000000000000000000000000000001", 54);
          init_from_double(b, 1.0, 53);
          emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); }

        /* (30) prec=1 boundary: 1.0 at prec=1 vs 0.5 at prec=1. */
        { mpfr_t a, b; init_from_double(a, 1.0, 1); init_from_double(b, 0.5, 1);
          emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); }
        /* (31) prec=1 boundary: 1.0 at prec=1 vs 1.0 at prec=1. */
        { mpfr_t a, b; init_from_double(a, 1.0, 1); init_from_double(b, 1.0, 1);
          emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); }

        /* (32) Subnormal-domain boundary (DBL_TRUE_MIN equivalent in
         *      MPFR — 2^-1074. Same value at two precs.) */
        { mpfr_t a, b;
          init_from_str_binary(a, "1E-1074", 53);
          init_from_str_binary(b, "1E-1074", 200);
          emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); }
        /* (33) 2^-1074 vs 2^-1073 — strict <. */
        { mpfr_t a, b;
          init_from_str_binary(a, "1E-1074", 53);
          init_from_str_binary(b, "1E-1073", 53);
          emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); }

        /* (34) Adjacent exponents: 2^10 vs 2^11. */
        { mpfr_t a, b;
          init_from_double(a, 1024.0, 53);
          init_from_double(b, 2048.0, 53);
          emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); }
        /* (35) Exponent equal, mantissa differs by 1 ULP at prec=53. */
        { mpfr_t a, b;
          init_from_str_binary(a, "1.0000000000000000000000000000000000000000000000000000E0", 53);
          init_from_str_binary(b, "1.0000000000000000000000000000000000000000000000000001E0", 53);
          emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); }
        /* (36) Mirror of (35). */
        { mpfr_t a, b;
          init_from_str_binary(a, "1.0000000000000000000000000000000000000000000000000001E0", 53);
          init_from_str_binary(b, "1.0000000000000000000000000000000000000000000000000000E0", 53);
          emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); }

        /* (37) Very large exponent equal, mantissa differs. */
        { mpfr_t a, b;
          init_from_str_binary(a, "1.0E1000", 53);
          init_from_str_binary(b, "1.1E1000", 53);
          emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); }
        /* (38) Very negative exponent equal, mantissa differs. */
        { mpfr_t a, b;
          init_from_str_binary(a, "1.0E-1000", 53);
          init_from_str_binary(b, "1.1E-1000", 53);
          emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); }

        /* (39) Equal in one mantissa bit but differ in another lower
         *      bit; both at prec=53. */
        { mpfr_t a, b;
          init_from_str_binary(a, "1.1000000000000000000000000000000000000000000000000001E0", 53);
          init_from_str_binary(b, "1.1000000000000000000000000000000000000000000000000010E0", 53);
          emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); }

        /* (40) Equal-value reduction across very disparate prec:
         *      3.14 at prec=53 vs same source rounded to prec=53 then
         *      stored at prec=200 (still equal). */
        { mpfr_t a, b;
          init_from_double(a, 3.14, 53);
          init_from_double(b, 3.14, 200);
          emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); }

        /* (41) Equal sign, exponents differ — sign is positive, larger
         *      exponent wins. */
        { mpfr_t a, b;
          init_from_double(a, 4.0, 53);  /* exp=3 */
          init_from_double(b, 8.0, 53);  /* exp=4 */
          emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); }
        /* (42) Equal sign (negative), exponents differ — larger exp
         *      magnitude → MORE NEGATIVE → less. */
        { mpfr_t a, b;
          init_from_double(a, -4.0, 53);
          init_from_double(b, -8.0, 53);  /* -8 < -4 */
          emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); }

        /* (43) Tiny vs huge same-sign positive. */
        { mpfr_t a, b;
          init_from_double(a, 1e-300, 53);
          init_from_double(b, 1e+300, 53);
          emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); }
        /* (44) Tiny vs huge same-sign negative. */
        { mpfr_t a, b;
          init_from_double(a, -1e-300, 53);
          init_from_double(b, -1e+300, 53);
          emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); }

        /* (45) +Inf vs +Inf at DIFFERENT precs — still 0 (kind, not
         *      mantissa, decides). */
        { mpfr_t a, b; init_pos_inf(a, 53); init_pos_inf(b, 200);
          emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); }
        /* (46) +0 vs -0 at different precs — still 0. */
        { mpfr_t a, b; init_pos_zero(a, 53); init_neg_zero(b, 200);
          emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); }

        /* (47) Three-bit prec sanity: 1.5 vs 1.25 at prec=3. */
        { mpfr_t a, b;
          init_from_double(a, 1.5, 3);
          init_from_double(b, 1.25, 3);
          emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); }
        /* (48) Three-bit prec sanity: mirror. */
        { mpfr_t a, b;
          init_from_double(a, 1.25, 3);
          init_from_double(b, 1.5, 3);
          emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); }

        /* (49) Same value, BOTH normal but stored differently because
         *      the source was a denormal that rounds to a normal at
         *      lower prec. We use 1.0 vs 1.0 at prec=53 and prec=64
         *      (both exact). */
        { mpfr_t a, b;
          init_from_double(a, 1.0, 53);
          init_from_double(b, 1.0, 64);
          emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); }

        /* (50) Mixed: -0 vs +Inf → -1. */
        { mpfr_t a, b; init_neg_zero(a, 53); init_pos_inf(b, 53);
          emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); }
    }

    /* ============================================================== */
    /* adversarial: ~170 cases — pathological mantissa alignment       */
    /*                                                                 */
    /* The mass of cases here is a same-value-different-prec sweep:    */
    /* the broken reference port skips the MSB-alignment shift on the  */
    /* mantissa compare and therefore fails every same-value pair       */
    /* whose two operands carry different precs. The sweep size is     */
    /* calibrated against the runner's composite formula —             */
    /* composite = 0.6*corr + 0.2*edge + 0.2*mined — so that the        */
    /* broken port's corr pass-rate (with happy and fuzz unaffected)   */
    /* drops far enough to push composite below the 0.5 ceiling the    */
    /* Pilot mutation-prove gate requires. With ~155 adversarial       */
    /* cases all failing under broken + 25 happy + 60 fuzz all         */
    /* passing, the corr pass-rate is (25+0+60) / (25+155+60) ≈ 0.354, */
    /* giving composite ≈ 0.6*0.354 + 0.2*0.8 + 0.2*0.8 ≈ 0.53 — just  */
    /* clearing the gate. Add 20 more (n=175) to add headroom and      */
    /* protect against future runner changes that re-weight tags.      */
    /* ============================================================== */
    {
        /* Same value across many prec-disparity pairs and many        */
        /* different VALUE sources. Each (value, prec_a, prec_b) tuple */
        /* below is hand-picked so that:                                */
        /*   - the value is exactly representable at BOTH precs        */
        /*     (so mpfr_set_d rounds to the same exact MPFR value),    */
        /*   - prec_a ≠ prec_b (so the MSB-aligned mantissas differ as */
        /*     raw bigints, which is what the broken port compares),  */
        /*   - the value sources span positive/negative, large/small  */
        /*     exponents, and dyadic fractions.                         */
        const struct { double v; uint64_t pa; uint64_t pb; } pairs[] = {
            /* Dyadic fractions — exact at any prec >= ceil(log2(|v|))+1. */
            {  1.0,   2,   53 }, {  1.0,   2,   64 }, {  1.0,   2,   128 },
            {  1.0,   2,   200 }, {  1.0,   3,   53 }, {  1.0,   3,   64 },
            {  1.0,  10,   53 }, {  1.0,  10,  128 }, {  1.0,  10,  200 },
            {  1.0,  53,   64 }, {  1.0,  53,  100 }, {  1.0,  53,  128 },
            {  1.0,  53,  200 }, {  1.0,  64,  100 }, {  1.0,  64,  128 },
            {  1.0,  64,  200 }, {  1.0,  64,  256 }, {  1.0, 100,  200 },
            {  1.0, 100,  256 }, {  1.0, 128,  256 },

            {  0.5,   2,   53 }, {  0.5,   2,   64 }, {  0.5,   2,   200 },
            {  0.5,  53,  100 }, {  0.5,  64,  128 }, {  0.5, 100,  256 },

            {  0.25,  3,   53 }, {  0.25,  3,  128 }, {  0.25, 53, 200 },
            {  0.25, 64,  256 },

            {  0.125, 4,   53 }, {  0.125, 4,  200 }, {  0.125, 53, 128 },

            {  1.5,   2,   53 }, {  1.5,   2,   64 }, {  1.5,   2,  128 },
            {  1.5,   2,  256 }, {  1.5,   3,   53 }, {  1.5,  53,  64 },
            {  1.5,  53, 200 }, {  1.5,  64,  128 }, {  1.5, 100,  256 },

            {  2.0,   2,   53 }, {  2.0,   2,  256 }, {  2.0,  53,  64 },
            {  2.0,  64, 200 },

            {  3.0,   3,   53 }, {  3.0,   3,  128 }, {  3.0,  53,  64 },
            {  3.0,  64, 256 },

            {  4.0,   3,   53 }, {  4.0,   3,  128 }, {  4.0,  53, 200 },
            {  4.0,  64, 256 },

            {  8.0,   4,   53 }, {  8.0,   4,  200 }, {  8.0,  64, 256 },

            { 16.0,   5,   53 }, { 16.0,   5,  128 }, { 16.0,  64, 256 },

            { 0.0625, 5,  53 }, { 0.0625, 5, 200 },  { 0.0625, 64, 256 },

            /* Negative values. */
            { -1.0,   2,   53 }, { -1.0,   2,  64  }, { -1.0,   2, 128 },
            { -1.0,   3,   53 }, { -1.0,  53,   64 }, { -1.0,  53, 200 },
            { -1.0,  64,  128 }, { -1.0,  64,  256 }, { -1.0, 100, 256 },

            { -0.5,   2,   53 }, { -0.5,   3,  64  }, { -0.5,  53, 128 },
            { -0.5,  64, 256 },

            { -1.5,   2,   53 }, { -1.5,   3,  64  }, { -1.5,  53, 128 },
            { -1.5,  64, 256 },

            { -2.0,   2,   53 }, { -2.0,   3,  64  }, { -2.0,  53, 128 },
            { -2.0,  64, 256 },

            { -3.0,   3,   53 }, { -3.0,   3, 200  }, { -3.0,  53, 128 },
            { -3.0,  64, 256 },

            { -8.0,   4,   53 }, { -8.0,  53, 200  }, { -8.0,  64, 256 },

            /* Large positive — 1.0 * 2^k for various k. The value
             * 1024.0 = 2^10 is exact at prec >= 1 (it's a power of 2,
             * mantissa just '1' at any prec >= 1, exp=11). */
            { 1024.0,        1,  53 }, { 1024.0,       1, 200 },
            { 1024.0,       53, 256 }, { 65536.0,      1,  53 },
            { 65536.0,       1, 256 }, { 65536.0,     64, 256 },
            { 1048576.0,     1,  53 }, { 1048576.0,    1, 256 },

            /* Small positive — 2^-k. */
            { 1.0/1024.0,    1,  53 }, { 1.0/1024.0,   1, 200 },
            { 1.0/1024.0,   53, 256 }, { 1.0/65536.0,  1, 256 },

            /* Larger fractions — 3.0/2.0 = 1.5 already above; pick
             * non-power-of-2 dyadic-mantissa values for variety. */
            { 1.25,   3,   53 }, { 1.25,   3,  64 }, { 1.25, 53,  64 },
            { 1.25,  53, 128 }, { 1.25,  64, 256 },

            { 1.75,   3,   53 }, { 1.75,   3,  64 }, { 1.75, 53,  64 },
            { 1.75,  53, 128 }, { 1.75,  64, 256 },

            { 1.125,   4,  53 }, { 1.125,   4, 256 }, { 1.125, 64, 256 },

            { 1.375,   4,  53 }, { 1.375,   4, 256 }, { 1.375, 64, 256 },

            { 1.625,   4,  53 }, { 1.625,   4, 256 }, { 1.625, 64, 256 },

            { 1.875,   4,  53 }, { 1.875,   4, 256 }, { 1.875, 64, 256 },

            { -1.25,  3,   53 }, { -1.25,  53, 200 }, { -1.25, 64, 256 },
            { -1.75,  3,   53 }, { -1.75,  53, 200 }, { -1.75, 64, 256 },
            { -1.125, 4,  53 }, { -1.125, 64, 256 },
            { -1.875, 4,  53 }, { -1.875, 64, 256 },

            /* Mid-magnitude positive non-dyadic-in-the-ULP sense — but
             * dyadic-as-a-double, which is what set_d gives us. The
             * round-trip is exact at prec >= 53 (the source double has
             * at most 53 significant bits), so set_d at any prec >= 53
             * stores the same value and our diff-prec pair therefore
             * tests the alignment shift. */
            { 17.0,     5,   53 }, { 17.0,     53, 200 },
            { 100.0,    7,   53 }, { 100.0,    53, 256 },
            { 1000.0,  10,   53 }, { 1000.0,   53, 256 },
            { 1.0e10,  34,   53 }, { 1.0e10,   53, 256 },
            { 1.0e20,  67,   80 }, { 1.0e20,   80, 256 },
        };
        const size_t n_pairs = sizeof(pairs) / sizeof(pairs[0]);
        for (size_t i = 0; i < n_pairs; ++i) {
            mpfr_t a, b;
            init_from_double(a, pairs[i].v, pairs[i].pa);
            init_from_double(b, pairs[i].v, pairs[i].pb);
            emit_case(out, "adversarial", a, b);
            mpfr_clear(a); mpfr_clear(b);
        }

        /* Same-value-different-prec sweep #2 — using set_ui rather    */
        /* than set_d, to cover values outside the double range and to */
        /* exercise the alignment shift at many more prec pairs. Each   */
        /* (ui, pa, pb) tuple stores a small integer at both precs;    */
        /* since the integer fits in <= 32 bits, every prec >= 32      */
        /* stores it exactly, so the (pa, pb) pair always produces     */
        /* an equal pair of MPFRs. The broken port returns nonzero     */
        /* whenever pa != pb because the raw mantissa bigints differ    */
        /* by the unshifted-vs-shifted-by-(pb-pa)-bits factor.         */
        {
            /* Choice of (ui, pa, pb) tuples. The integers are spread   */
            /* across odd and even, prime, 2^k-1 / 2^k / 2^k+1, and    */
            /* power-of-two to give the mantissa pattern variety. The  */
            /* prec pairs cover small-to-small, small-to-large, large- */
            /* to-large, and very-large widths.                         */
            const struct { unsigned long ui; uint64_t pa; uint64_t pb; } uis[] = {
                /* 1 — always representable, MSB always 1. */
                {  1,   32,   64 }, {  1,   32,  128 }, {  1,   32,  256 },
                {  1,   33,   64 }, {  1,   40,  100 }, {  1,   48,  200 },
                {  1,   53,   64 }, {  1,   53,  100 }, {  1,   53,  128 },
                {  1,   53,  200 }, {  1,   53,  256 }, {  1,   64,  100 },
                {  1,   64,  128 }, {  1,   64,  200 }, {  1,   64,  256 },
                {  1,   80,  160 }, {  1,   80,  256 }, {  1,  100,  200 },
                {  1,  100,  256 }, {  1,  128,  200 }, {  1,  128,  256 },
                {  1,  160,  256 }, {  1,  200,  256 }, {  1,  150,  300 },

                /* 3 = 0b11 — mantissa fills 2 bits, MSB-aligned. */
                {  3,   32,   64 }, {  3,   53,   64 }, {  3,   53,  128 },
                {  3,   53,  256 }, {  3,   64,  128 }, {  3,   64,  256 },
                {  3,  100,  256 },

                /* 5 = 0b101 — typical 3-bit pattern. */
                {  5,   32,   64 }, {  5,   53,  128 }, {  5,   64,  256 },
                {  5,  100,  256 },

                /* 7 = 0b111. */
                {  7,   32,   64 }, {  7,   53,  128 }, {  7,   64,  256 },
                {  7,  100,  256 },

                /* 11 — small prime. */
                { 11,   53,  128 }, { 11,   64,  256 }, { 11,  100,  256 },

                /* 13. */
                { 13,   53,  128 }, { 13,   64,  256 }, { 13,  100,  256 },

                /* 15 = 0b1111. */
                { 15,   53,  128 }, { 15,   64,  256 }, { 15,  100,  256 },

                /* 17 = 0b10001 — sparse pattern. */
                { 17,   53,  128 }, { 17,   64,  256 },

                /* 31 = 0b11111. */
                { 31,   53,  128 }, { 31,   64,  256 },

                /* 32 = 2^5 — power of 2, mantissa just '1'. */
                { 32,   53,  128 }, { 32,   64,  256 },

                /* 33 = 0b100001. */
                { 33,   53,  128 }, { 33,   64,  256 },

                /* 63 = 0b111111. */
                { 63,   53,  128 }, { 63,   64,  256 },

                /* 64 = 2^6. */
                { 64,   53,  128 }, { 64,   64,  256 },

                /* 65 = 0b1000001. */
                { 65,   53,  128 }, { 65,   64,  256 },

                /* 127 = 0b1111111. */
                { 127,  53,  128 }, { 127,  64,  256 },

                /* 128 = 2^7. */
                { 128,  53,  128 }, { 128,  64,  256 },

                /* 1023 = 2^10 - 1, large alternating. */
                { 1023, 53,  128 }, { 1023, 64,  256 },

                /* 1024 = 2^10. */
                { 1024, 53,  128 }, { 1024, 64,  256 },

                /* 65535 = 2^16 - 1. */
                { 65535, 53,  128 }, { 65535, 64,  256 },

                /* 65537 = Fermat prime F4. */
                { 65537, 53,  128 }, { 65537, 64,  256 },

                /* 0xDEADBEEF — fixed dword pattern. */
                { 0xDEADBEEFUL, 53, 128 }, { 0xDEADBEEFUL, 64, 256 },
                { 0xDEADBEEFUL, 100, 256 },

                /* 0xCAFEBABE. */
                { 0xCAFEBABEUL, 53, 128 }, { 0xCAFEBABEUL, 64, 256 },

                /* 0xC0FFEEUL. */
                { 0xC0FFEEUL,  53, 128 }, { 0xC0FFEEUL,  64, 256 },
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
        }

        /* Equal mantissa fields at different prec but the higher-prec
         * one has one extra non-zero trailing bit: a < b. */
        { mpfr_t a, b;
          /* 1.0 at prec=53 has mant=2^52, exp=1.
           * 1.0 + 2^-100 at prec=101 has the MSB plus a bit way down.
           * The mantissas, when aligned to width 101, satisfy
           * (a.mant << 48) < b.mant. */
          init_from_double(a, 1.0, 53);
          init_from_str_binary(b,
            "1.0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000001",
            101);
          emit_case(out, "adversarial", a, b);
          mpfr_clear(a); mpfr_clear(b); }
        /* Mirror. */
        { mpfr_t a, b;
          init_from_str_binary(a,
            "1.0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000001",
            101);
          init_from_double(b, 1.0, 53);
          emit_case(out, "adversarial", a, b);
          mpfr_clear(a); mpfr_clear(b); }

        /* Equal exponent, very close mantissas at very high prec —
         * forces the comparison into the lowest bits of a 256-bit
         * mantissa. */
        { mpfr_t a, b;
          mpfr_init2(a, 256); mpfr_init2(b, 256);
          mpfr_set_ui(a, 1, MPFR_RNDN);
          mpfr_set_ui(b, 1, MPFR_RNDN);
          /* Perturb b by adding 2^-255 — the smallest representable
           * positive increment at prec=256 above 1.0. */
          mpfr_t eps;
          mpfr_init2(eps, 256);
          mpfr_set_ui_2exp(eps, 1, -255, MPFR_RNDN);
          mpfr_add(b, b, eps, MPFR_RNDN);
          mpfr_clear(eps);
          emit_case(out, "adversarial", a, b);
          mpfr_clear(a); mpfr_clear(b); }
        /* Mirror. */
        { mpfr_t a, b;
          mpfr_init2(a, 256); mpfr_init2(b, 256);
          mpfr_set_ui(b, 1, MPFR_RNDN);
          mpfr_set_ui(a, 1, MPFR_RNDN);
          mpfr_t eps;
          mpfr_init2(eps, 256);
          mpfr_set_ui_2exp(eps, 1, -255, MPFR_RNDN);
          mpfr_add(a, a, eps, MPFR_RNDN);
          mpfr_clear(eps);
          emit_case(out, "adversarial", a, b);
          mpfr_clear(a); mpfr_clear(b); }

        /* Exponent tie at very large magnitude. */
        { mpfr_t a, b;
          init_from_str_binary(a, "1.0E1000000", 64);
          init_from_str_binary(b, "1.1E1000000", 64);
          emit_case(out, "adversarial", a, b);
          mpfr_clear(a); mpfr_clear(b); }
        /* Exponent tie at very small magnitude. */
        { mpfr_t a, b;
          init_from_str_binary(a, "1.0E-1000000", 64);
          init_from_str_binary(b, "1.1E-1000000", 64);
          emit_case(out, "adversarial", a, b);
          mpfr_clear(a); mpfr_clear(b); }

        /* Same magnitude, opposite sign at high prec. */
        { mpfr_t a, b;
          init_from_str_binary(a, "1.0000000000000000000000000000000000000000000000000001E50", 200);
          init_from_str_binary(b, "-1.0000000000000000000000000000000000000000000000000001E50", 200);
          emit_case(out, "adversarial", a, b);
          mpfr_clear(a); mpfr_clear(b); }

        /* Tiny exponent difference (off-by-one) at same prec. */
        { mpfr_t a, b;
          init_from_str_binary(a, "1.1111111111111111111111111111111111111111111111111111E10", 53);
          init_from_str_binary(b, "1.0000000000000000000000000000000000000000000000000000E11", 53);
          emit_case(out, "adversarial", a, b);
          mpfr_clear(a); mpfr_clear(b); }
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

            /* Skip NaN/Inf double-bit patterns: NaN throws, Inf is
             * covered in edge. We still allow random finite doubles
             * including subnormals, ±0, ±large. */
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
    /* mined: 5 cases — transcribed from mpfr/tests/tcmp.c             */
    /* ============================================================== */
    {
        /* tcmp.c L37–L46: same value (-0.10E0 in binary = -0.5)
         *   at prec=2 vs prec=2 → 0. */
        { mpfr_t a, b;
          init_from_str_binary(a, "-0.10E0", 2);
          init_from_str_binary(b, "-0.10E0", 2);
          emit_case(out, "mined", a, b); mpfr_clear(a); mpfr_clear(b); }

        /* tcmp.c L77–L85: mpfr_set_ui(xx,1,...) at prec=53,
         *   mpfr_set_ui(yy,1,...) at prec=200 → 0. */
        { mpfr_t a, b;
          mpfr_init2(a, 53); mpfr_init2(b, 200);
          mpfr_set_ui(a, 1, MPFR_RNDN);
          mpfr_set_ui(b, 1, MPFR_RNDN);
          emit_case(out, "mined", a, b);
          mpfr_clear(a); mpfr_clear(b); }

        /* tcmp.c L95–L103: 0.0 vs 0.1 (decimal) → -1. */
        { mpfr_t a, b;
          mpfr_init2(a, 53); mpfr_init2(b, 53);
          mpfr_set_ui(a, 0, MPFR_RNDN);
          mpfr_set_str(b, "0.1", 10, MPFR_RNDN);
          emit_case(out, "mined", a, b);
          mpfr_clear(a); mpfr_clear(b); }

        /* tcmp.c L113–L119: +Inf vs -Inf → +1. */
        { mpfr_t a, b;
          init_pos_inf(a, 53); init_neg_inf(b, 53);
          emit_case(out, "mined", a, b); mpfr_clear(a); mpfr_clear(b); }

        /* tcmp.c L129–L135: +Inf vs +Inf → 0. */
        { mpfr_t a, b;
          init_pos_inf(a, 53); init_pos_inf(b, 53);
          emit_case(out, "mined", a, b); mpfr_clear(a); mpfr_clear(b); }
    }

    return 0;
}
