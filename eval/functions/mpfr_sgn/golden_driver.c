/*
 * golden_driver.c — Golden master for MPFR's mpfr_sgn.
 *
 * C signature
 * -----------
 *
 *   int mpfr_sgn(mpfr_srcptr op);
 *
 *   Returns +1 / 0 / -1 reflecting the sign of op. NaN sets the erange
 *   flag and returns 0. Ref: mpfr/src/sgn.c L24–L39. The canonical
 *   libmpfr returns -1/0/+1; this driver normalises defensively.
 *
 * Divergence from C → TS
 * ----------------------
 *
 * The TS port `mpfr_sgn(x) -> number` returns -1/0/+1 and THROWS
 * MPFRError('EDOMAIN', ...) on NaN — the documented domain-error
 * divergence per CLAUDE.md "Hallucination-risk callouts: NaN ≠ NaN",
 * matching the mpfr_cmp convention. Because a throw is graded as
 * n_throw (not a pass), this driver MUST NOT emit any NaN cases.
 *
 * Wire format
 * -----------
 *
 *   {"tag":"<class>",
 *    "inputs":{"x":<MPFR-record>},
 *    "output":<int>,
 *    "time_ns":<n>}
 *
 *   - `output` is a bare JS int (decoded by value_codec.ts as
 *     `{kind:'scalar', value:<bigint>}`). compareOutput's bigint
 *     branch coerces an integer JS number on the actual side, so the
 *     port may return plain `number` and the bare-scalar wire still
 *     grades correctly. (Same pattern as mpfr_cmp.)
 *
 * Tag distribution (CLAUDE.md Rule 7 minimums)
 * --------------------------------------------
 *
 *   happy        :  ~25  (positive + negative normals at common precs)
 *   edge         :  ~50  (zeros — both signs, multiple precs; ±Inf;
 *                         tiny normals; large normals)
 *   adversarial  :  ~20  (the broken port returns x.sign always; this
 *                         tag class is dense on zero cases to push
 *                         composite below 0.5)
 *   fuzz         :   50  (PRNG seed 0x5187ED5187ED5187ULL; finite normals)
 *   mined        :   5   (from mpfr/tests/tsgn.c)
 *
 * NaN cases are excluded — the TS port throws.
 *
 * Ref: mpfr/src/sgn.c — the C reference.
 * Ref: src/ops/sgn.ts — the production port.
 */
#include "common.h"

#include <assert.h>
#include <float.h>
#include <inttypes.h>
#include <string.h>

#if GMP_NUMB_BITS != 64
#  error "mpfr_sgn golden_driver requires GMP_NUMB_BITS == 64"
#endif

/* Normalise mpfr_sgn's return to {-1, 0, +1}. libmpfr returns -1/0/+1
 * but the C standard only promises a sign-bearing int; normalise
 * defensively so a future libmpfr can't silently invalidate goldens. */
static inline int normalise_sgn(int r) {
    return (r > 0) ? 1 : ((r < 0) ? -1 : 0);
}

/* Emit one mpfr_sgn golden case. We MUST NOT emit NaN cases — the
 * TS port throws, which the harness grades as n_throw. */
static inline void emit_case(FILE *out, const char *tag, mpfr_srcptr x) {
    assert(!mpfr_nan_p(x));

    const uint64_t t0 = now_ns();
    const int raw = mpfr_sgn(x);
    const uint64_t elapsed = now_ns() - t0;
    const int result = normalise_sgn(raw);

    jl_begin(out, tag);
    jl_kv_mpfr(out, 1, "x", x);
    jl_end_inputs(out);
    jl_output_scalar_int(out, result);
    jl_finish(out, elapsed);
}

static inline void init_from_double(mpfr_ptr x, double d, uint64_t prec) {
    mpfr_init2(x, (mpfr_prec_t)prec); mpfr_set_d(x, d, MPFR_RNDN);
}
static inline void init_from_str_binary(mpfr_ptr x, const char *s,
                                        uint64_t prec) {
    mpfr_init2(x, (mpfr_prec_t)prec); mpfr_set_str(x, s, 2, MPFR_RNDN);
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

static inline void emit_d(FILE *out, const char *tag, double d, uint64_t p) {
    mpfr_t x; init_from_double(x, d, p);
    emit_case(out, tag, x);
    mpfr_clear(x);
}

int main(void) {
    FILE *out = stdout;

    /* ============================================================== */
    /* happy: ~25 — positive + negative normals at common precs       */
    /* ============================================================== */
    {
        /* Positives (sgn = +1). */
        emit_d(out, "happy",  1.0,   53);
        emit_d(out, "happy",  2.0,   53);
        emit_d(out, "happy",  3.14,  53);
        emit_d(out, "happy",  10.0,  53);
        emit_d(out, "happy",  100.0, 53);
        emit_d(out, "happy",  1e9,   53);
        emit_d(out, "happy",  1e100, 53);
        emit_d(out, "happy",  1e-100, 53);
        emit_d(out, "happy",  2.718281828459045, 53);
        emit_d(out, "happy",  6.022e23, 53);
        emit_d(out, "happy",  1.5,    53);
        emit_d(out, "happy",  0.5,    53);

        /* Negatives (sgn = -1). */
        emit_d(out, "happy", -1.0,    53);
        emit_d(out, "happy", -2.0,    53);
        emit_d(out, "happy", -3.14,   53);
        emit_d(out, "happy", -10.0,   53);
        emit_d(out, "happy", -100.0,  53);
        emit_d(out, "happy", -1e9,    53);
        emit_d(out, "happy", -1e100,  53);
        emit_d(out, "happy", -1e-100, 53);
        emit_d(out, "happy", -1.5,    53);
        emit_d(out, "happy", -0.5,    53);

        /* Varying precs (sgn is prec-independent). */
        emit_d(out, "happy",  3.14, 1);
        emit_d(out, "happy", -3.14, 1);
        emit_d(out, "happy",  1.0, 256);
    }

    /* ============================================================== */
    /* edge: ~50 — zeros (both signs, many precs), ±Inf, tiny/large    */
    /* normals. Heavy emphasis on zeros (multiple precs) since the    */
    /* broken port returns ±1 on zero and the bug shows here.         */
    /* ============================================================== */
    {
        /* (1-10) +0 at varying precs — sgn = 0. */
        { mpfr_t x; init_pos_zero(x, 1);   emit_case(out, "edge", x); mpfr_clear(x); }
        { mpfr_t x; init_pos_zero(x, 2);   emit_case(out, "edge", x); mpfr_clear(x); }
        { mpfr_t x; init_pos_zero(x, 10);  emit_case(out, "edge", x); mpfr_clear(x); }
        { mpfr_t x; init_pos_zero(x, 24);  emit_case(out, "edge", x); mpfr_clear(x); }
        { mpfr_t x; init_pos_zero(x, 53);  emit_case(out, "edge", x); mpfr_clear(x); }
        { mpfr_t x; init_pos_zero(x, 64);  emit_case(out, "edge", x); mpfr_clear(x); }
        { mpfr_t x; init_pos_zero(x, 100); emit_case(out, "edge", x); mpfr_clear(x); }
        { mpfr_t x; init_pos_zero(x, 128); emit_case(out, "edge", x); mpfr_clear(x); }
        { mpfr_t x; init_pos_zero(x, 200); emit_case(out, "edge", x); mpfr_clear(x); }
        { mpfr_t x; init_pos_zero(x, 256); emit_case(out, "edge", x); mpfr_clear(x); }

        /* (11-20) -0 at varying precs — sgn = 0 (collapse). */
        { mpfr_t x; init_neg_zero(x, 1);   emit_case(out, "edge", x); mpfr_clear(x); }
        { mpfr_t x; init_neg_zero(x, 2);   emit_case(out, "edge", x); mpfr_clear(x); }
        { mpfr_t x; init_neg_zero(x, 10);  emit_case(out, "edge", x); mpfr_clear(x); }
        { mpfr_t x; init_neg_zero(x, 24);  emit_case(out, "edge", x); mpfr_clear(x); }
        { mpfr_t x; init_neg_zero(x, 53);  emit_case(out, "edge", x); mpfr_clear(x); }
        { mpfr_t x; init_neg_zero(x, 64);  emit_case(out, "edge", x); mpfr_clear(x); }
        { mpfr_t x; init_neg_zero(x, 100); emit_case(out, "edge", x); mpfr_clear(x); }
        { mpfr_t x; init_neg_zero(x, 128); emit_case(out, "edge", x); mpfr_clear(x); }
        { mpfr_t x; init_neg_zero(x, 200); emit_case(out, "edge", x); mpfr_clear(x); }
        { mpfr_t x; init_neg_zero(x, 256); emit_case(out, "edge", x); mpfr_clear(x); }

        /* (21-25) +Inf at varying precs — sgn = +1. */
        { mpfr_t x; init_pos_inf(x, 1);   emit_case(out, "edge", x); mpfr_clear(x); }
        { mpfr_t x; init_pos_inf(x, 53);  emit_case(out, "edge", x); mpfr_clear(x); }
        { mpfr_t x; init_pos_inf(x, 64);  emit_case(out, "edge", x); mpfr_clear(x); }
        { mpfr_t x; init_pos_inf(x, 128); emit_case(out, "edge", x); mpfr_clear(x); }
        { mpfr_t x; init_pos_inf(x, 256); emit_case(out, "edge", x); mpfr_clear(x); }

        /* (26-30) -Inf at varying precs — sgn = -1. */
        { mpfr_t x; init_neg_inf(x, 1);   emit_case(out, "edge", x); mpfr_clear(x); }
        { mpfr_t x; init_neg_inf(x, 53);  emit_case(out, "edge", x); mpfr_clear(x); }
        { mpfr_t x; init_neg_inf(x, 64);  emit_case(out, "edge", x); mpfr_clear(x); }
        { mpfr_t x; init_neg_inf(x, 128); emit_case(out, "edge", x); mpfr_clear(x); }
        { mpfr_t x; init_neg_inf(x, 256); emit_case(out, "edge", x); mpfr_clear(x); }

        /* (31-40) Smallest representable subnormal-ish values via
         * string literal — sgn = ±1 (the value is nonzero so the
         * normal branch fires). */
        { mpfr_t x; init_from_str_binary(x, "1E-1074", 53);  emit_case(out, "edge", x); mpfr_clear(x); }
        { mpfr_t x; init_from_str_binary(x, "-1E-1074", 53); emit_case(out, "edge", x); mpfr_clear(x); }
        { mpfr_t x; init_from_str_binary(x, "1E-1000000", 53);  emit_case(out, "edge", x); mpfr_clear(x); }
        { mpfr_t x; init_from_str_binary(x, "-1E-1000000", 53); emit_case(out, "edge", x); mpfr_clear(x); }
        { mpfr_t x; init_from_str_binary(x, "1E1000000", 53);   emit_case(out, "edge", x); mpfr_clear(x); }
        { mpfr_t x; init_from_str_binary(x, "-1E1000000", 53);  emit_case(out, "edge", x); mpfr_clear(x); }
        { mpfr_t x; init_from_str_binary(x, "1.111111111111E0", 53); emit_case(out, "edge", x); mpfr_clear(x); }
        { mpfr_t x; init_from_str_binary(x, "-1.111111111111E0", 53); emit_case(out, "edge", x); mpfr_clear(x); }
        emit_d(out, "edge",  1.0, 2);
        emit_d(out, "edge", -1.0, 2);

        /* (41-50) Mantissa-pattern variety at high prec — sgn = ±1. */
        { mpfr_t x; init_from_str_binary(x, "1.0000000000000000000000000000000000000000000000000001E0", 200); emit_case(out, "edge", x); mpfr_clear(x); }
        { mpfr_t x; init_from_str_binary(x, "-1.0000000000000000000000000000000000000000000000000001E0", 200); emit_case(out, "edge", x); mpfr_clear(x); }
        emit_d(out, "edge",  1.5e-300, 200);
        emit_d(out, "edge", -1.5e-300, 200);
        emit_d(out, "edge",  1.5e+300, 200);
        emit_d(out, "edge", -1.5e+300, 200);
        emit_d(out, "edge",  DBL_MIN, 200);
        emit_d(out, "edge", -DBL_MIN, 200);
        emit_d(out, "edge",  DBL_MAX, 200);
        emit_d(out, "edge", -DBL_MAX, 200);
    }

    /* ============================================================== */
    /* adversarial: ~120 — heavy zero coverage at MANY precs to push   */
    /* the broken port (which always returns x.sign on zero) well     */
    /* below composite=0.5. The composite weighting is                */
    /*   0.6*corr + 0.2*edge + 0.2*mined                              */
    /* where corr is pooled over (happy + fuzz + adversarial). With   */
    /* happy=25, fuzz=50 (all passing under broken) and adversarial   */
    /* zeros all failing, corr = 75/(75 + N_adv). For composite ≤ 0.5 */
    /* (assuming edge ≈ 0.6 and mined ≈ 0.6 contribute 0.24 jointly),  */
    /* we need 0.6*corr ≤ 0.26 → corr ≤ 0.433 → N_adv ≥ 173 − 75 ≈ 98. */
    /* We sweep at 60 precs × 2 signs = 120 adversarial cases for     */
    /* headroom against future runner re-weighting.                    */
    /* ============================================================== */
    {
        /* 60 distinct precs across the [1, PREC_MAX]-relevant range.
         * Spread densely at small precs (where most boundary bugs hide)
         * and sparsely at large precs (each large prec adds a case
         * without changing storage cost meaningfully). */
        const uint64_t zero_precs[] = {
            1, 2, 3, 4, 5, 6, 7, 8, 9, 10,
            11, 12, 13, 14, 15, 16, 17, 19, 23, 29,
            31, 32, 33, 37, 41, 47, 48, 53, 59, 61,
            63, 64, 65, 67, 71, 79, 80, 89, 97, 100,
            101, 103, 107, 109, 113, 127, 128, 131, 137, 149,
            151, 157, 163, 173, 179, 191, 199, 200, 211, 256,
        };
        const size_t n_zp = sizeof(zero_precs) / sizeof(zero_precs[0]);
        /* For each prec, emit BOTH +0 and -0 — guards against a
         * hypothetical broken port that always returns -1 (or always
         * +1) coincidentally passing a homogeneous-sign sweep. */
        for (size_t i = 0; i < n_zp; ++i) {
            {
                mpfr_t x; init_pos_zero(x, zero_precs[i]);
                emit_case(out, "adversarial", x);
                mpfr_clear(x);
            }
            {
                mpfr_t x; init_neg_zero(x, zero_precs[i]);
                emit_case(out, "adversarial", x);
                mpfr_clear(x);
            }
        }
    }

    /* ============================================================== */
    /* fuzz: 50 — PRNG-driven, finite normals only                    */
    /* ============================================================== */
    {
        xs64_t rng;
        xs64_seed(&rng, 0x5187ED5187ED5187ULL);
        const uint64_t precs[6] = { 1, 2, 53, 64, 100, 200 };

        int emitted = 0;
        while (emitted < 50) {
            const uint64_t bits = xs64_next(&rng);

            /* Skip NaN/Inf bit patterns (NaN is forbidden because the TS
             * port throws; Inf is covered in edge but we don't want fuzz
             * to overlap heavily with edge coverage). */
            const uint64_t exp = (bits >> 52) & 0x7FF;
            if (exp == 0x7FF) continue;

            double d;
            memcpy(&d, &bits, sizeof d);

            const uint64_t p = precs[xs64_below(&rng, 6)];

            mpfr_t x;
            init_from_double(x, d, p);
            emit_case(out, "fuzz", x);
            mpfr_clear(x);
            emitted++;
        }
    }

    /* ============================================================== */
    /* mined: 5 — from mpfr/tests/tsgn.c                              */
    /* ============================================================== */
    {
        /* tsgn.c — sgn(0) = 0 (both signs collapse). */
        {
            mpfr_t x; init_pos_zero(x, 53);
            emit_case(out, "mined", x);
            mpfr_clear(x);
        }
        {
            mpfr_t x; init_neg_zero(x, 53);
            emit_case(out, "mined", x);
            mpfr_clear(x);
        }
        /* tsgn.c — sgn(+Inf) = +1. */
        {
            mpfr_t x; init_pos_inf(x, 53);
            emit_case(out, "mined", x);
            mpfr_clear(x);
        }
        /* tsgn.c — sgn(positive normal) = +1. */
        {
            mpfr_t x; init_from_double(x, 1.0, 53);
            emit_case(out, "mined", x);
            mpfr_clear(x);
        }
        /* tsgn.c — sgn(negative normal) = -1. */
        {
            mpfr_t x; init_from_double(x, -1.0, 53);
            emit_case(out, "mined", x);
            mpfr_clear(x);
        }
    }

    return 0;
}
