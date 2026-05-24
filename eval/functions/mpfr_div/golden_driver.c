/*
 * golden_driver.c — Golden master for MPFR's mpfr_div.
 *
 * C signature
 * -----------
 *
 *   int mpfr_div(mpfr_t rop, mpfr_srcptr op1, mpfr_srcptr op2, mpfr_rnd_t rnd);
 *
 *   Sets `rop` to `op1 / op2` rounded per `rnd` at `rop`'s precision.
 *   Returns the ternary (sign of rounded - exact). See mpfr/src/div.c
 *   L740-L848 (dispatcher) and L860+ (general algorithm).
 *
 * Divergence from C -> TS
 * -----------------------
 *
 * The TS port `mpfr_div(a, b, prec, rnd) -> Result` takes `prec` as a
 * positional argument and returns the canonical {value, ternary} pair
 * from src/core.ts L173-L176. The C function mutates `rop` (whose prec
 * is independently set) and returns the ternary separately.
 *
 * Tag distribution (CLAUDE.md Rule 7 minimums)
 * --------------------------------------------
 *
 *   happy        :  ~25   (typical divs at common precs, RNDN)
 *   edge         :  ~55   (specials × rnd; signed zero on sign-product;
 *                          exact powers of two; x/x, x/1, x/2^k)
 *   adversarial  :  ~20   (ternary direction across rnd; rounding ties)
 *   fuzz         :  100   (PRNG; random doubles × random precs × rnd)
 *   mined        :    7   (transcribed from mpfr/tests/tdiv.c)
 *   ------------ ----
 *   total        : ~207
 *
 * Build via the repo-wide eval/golden_master/build.sh.
 *
 * Ref: mpfr/src/div.c — the C reference.
 * Ref: src/ops/div.ts — the production port.
 * Ref: mpfr/tests/tdiv.c — source for the `mined` cases.
 */
#include "common.h"

#include <assert.h>
#include <float.h>
#include <inttypes.h>
#include <math.h>
#include <string.h>

#if GMP_NUMB_BITS != 64
#  error "mpfr_div golden_driver requires GMP_NUMB_BITS == 64"
#endif

/* Mirror src/core.ts L236: PREC_MAX = 2^31 - 257. Cap fuzz at prec=200
 * per the brief — long fuzz runs need to fit a 50ms budget. */
#define TS_PREC_MAX ((uint64_t)((1ULL << 31) - 257ULL))
#define TS_PREC_MIN ((uint64_t)1)

/* ------------------------------------------------------------------ */
/* Per-case emitter                                                   */
/* ------------------------------------------------------------------ */

static inline void emit_case(FILE *out, const char *tag,
                             mpfr_srcptr a, mpfr_srcptr b,
                             uint64_t prec, mpfr_rnd_t rnd) {
    assert(prec >= TS_PREC_MIN && prec <= TS_PREC_MAX);

    mpfr_t rop;
    mpfr_init2(rop, (mpfr_prec_t)prec);

    const uint64_t t0 = now_ns();
    const int ternary = mpfr_div(rop, a, b, rnd);
    const uint64_t elapsed = now_ns() - t0;

    jl_begin(out, tag);
    jl_kv_mpfr(out, 1, "a", a);
    jl_kv_mpfr(out, 0, "b", b);
    jl_kv_u64(out, 0, "prec", prec);
    jl_kv_rnd(out, 0, "rnd", rnd);
    jl_end_inputs(out);
    jl_output_result(out, rop, ternary);
    jl_finish(out, elapsed);

    mpfr_clear(rop);
}

/* ------------------------------------------------------------------ */
/* Helpers                                                            */
/* ------------------------------------------------------------------ */

static inline void init_from_double(mpfr_ptr x, double d, uint64_t prec) {
    mpfr_init2(x, (mpfr_prec_t)prec);
    mpfr_set_d(x, d, MPFR_RNDN);
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

static inline double bits_to_double(uint64_t bits) {
    double d;
    memcpy(&d, &bits, sizeof d);
    return d;
}

static inline void emit_dd(FILE *out, const char *tag,
                           double da, uint64_t pa,
                           double db, uint64_t pb,
                           uint64_t prec, mpfr_rnd_t rnd) {
    mpfr_t a, b;
    init_from_double(a, da, pa);
    init_from_double(b, db, pb);
    emit_case(out, tag, a, b, prec, rnd);
    mpfr_clear(a); mpfr_clear(b);
}

static inline void emit_ss(FILE *out, const char *tag,
                           const char *sa, uint64_t pa,
                           const char *sb, uint64_t pb,
                           uint64_t prec, mpfr_rnd_t rnd) {
    mpfr_t a, b;
    mpfr_init2(a, (mpfr_prec_t)pa); mpfr_init2(b, (mpfr_prec_t)pb);
    mpfr_set_str(a, sa, 10, MPFR_RNDN);
    mpfr_set_str(b, sb, 10, MPFR_RNDN);
    emit_case(out, tag, a, b, prec, rnd);
    mpfr_clear(a); mpfr_clear(b);
}

/* ------------------------------------------------------------------ */
/* main                                                               */
/* ------------------------------------------------------------------ */

int main(void) {
    FILE *out = stdout;

    const mpfr_rnd_t RNDS[5] = {MPFR_RNDN, MPFR_RNDZ, MPFR_RNDU, MPFR_RNDD, MPFR_RNDA};

    /* ============================================================== */
    /* happy: ~25 cases — typical divs at common precs, RNDN           */
    /* ============================================================== */
    {
        /* Small integer divisions — all exact at prec>=53 for results
         * that fit in the natural integer/dyadic-rational representation. */
        emit_dd(out, "happy",  6.0,  53,  3.0,  53,  53, MPFR_RNDN); /* 2 */
        emit_dd(out, "happy", 12.0,  53,  4.0,  53,  53, MPFR_RNDN); /* 3 */
        emit_dd(out, "happy", 56.0,  53,  8.0,  53,  53, MPFR_RNDN); /* 7 */
        emit_dd(out, "happy",200.0,  53, 10.0,  53,  53, MPFR_RNDN); /* 20 */
        emit_dd(out, "happy",300.0,  53,100.0,  53,  53, MPFR_RNDN); /* 3 */
        emit_dd(out, "happy",  2.0e18, 53, 1.0e9, 53,  64, MPFR_RNDN);

        /* Mixed-sign — sign-product rule. */
        emit_dd(out, "happy", -6.0,  53,  3.0,  53,  53, MPFR_RNDN); /* -2 */
        emit_dd(out, "happy",  6.0,  53, -3.0,  53,  53, MPFR_RNDN); /* -2 */
        emit_dd(out, "happy", -6.0,  53, -3.0,  53,  53, MPFR_RNDN); /*  2 */

        /* Exact powers of 2 — pure-exponent shift, ternary 0. */
        emit_dd(out, "happy",  8.0,  53,  2.0,  53,  53, MPFR_RNDN); /* 4 */
        emit_dd(out, "happy",  1.0,  53,  2.0,  53,  53, MPFR_RNDN); /* 0.5 */
        emit_dd(out, "happy", 16.0,  53,  4.0,  53,  53, MPFR_RNDN); /* 4 */

        /* x/1 = x; x/x = 1 (for representable x). */
        emit_dd(out, "happy",  3.14, 53,  1.0,  53,  53, MPFR_RNDN);
        emit_dd(out, "happy",  3.14, 53,  3.14, 53,  53, MPFR_RNDN); /* 1 — exact */
        emit_dd(out, "happy",  -3.14,53, -3.14, 53,  53, MPFR_RNDN); /* 1 — sign-product */

        /* Non-dyadic — most rounding required. */
        emit_dd(out, "happy",  1.0,  53,  3.0,  53,  53, MPFR_RNDN); /* 1/3 */
        emit_dd(out, "happy", 22.0,  53,  7.0,  53,  53, MPFR_RNDN); /* 22/7 ~ pi */
        emit_dd(out, "happy",  1.0,  53,  7.0,  53,  53, MPFR_RNDN); /* 1/7 */

        /* Varying output prec. */
        emit_dd(out, "happy",  3.14, 53,  2.71, 53,  24, MPFR_RNDN);
        emit_dd(out, "happy",  3.14, 53,  2.71, 53,  64, MPFR_RNDN);
        emit_dd(out, "happy",  3.14, 53,  2.71, 53, 100, MPFR_RNDN);
        emit_dd(out, "happy",  3.14, 53,  2.71, 53, 200, MPFR_RNDN);

        /* Different input precs. */
        emit_dd(out, "happy", 12.0,  10,  4.0,  53,  53, MPFR_RNDN); /* 3 */
        emit_dd(out, "happy",  3.0,  53,  4.0, 100, 100, MPFR_RNDN); /* 0.75 — exact */
        emit_dd(out, "happy",  0.5,  53,  2.0, 100, 200, MPFR_RNDN); /* 0.25 — exact */

        /* Disparate magnitudes. */
        emit_dd(out, "happy",  1.0e100, 53, 1.0e50, 53,  53, MPFR_RNDN);
    }

    /* ============================================================== */
    /* edge: ~55 cases — specials × rounding × sign-product            */
    /* ============================================================== */
    {
        /* (1-5) NaN / anything -> NaN, all 5 rnd modes. */
        for (int i = 0; i < 5; ++i) {
            mpfr_t a, b; init_nan(a, 53); init_from_double(b, 1.0, 53);
            emit_case(out, "edge", a, b, 53, RNDS[i]);
            mpfr_clear(a); mpfr_clear(b);
        }
        /* (6-10) anything / NaN -> NaN. */
        for (int i = 0; i < 5; ++i) {
            mpfr_t a, b; init_from_double(a, 1.0, 53); init_nan(b, 53);
            emit_case(out, "edge", a, b, 53, RNDS[i]);
            mpfr_clear(a); mpfr_clear(b);
        }
        /* (11) NaN / NaN -> NaN. */
        {
            mpfr_t a, b; init_nan(a, 53); init_nan(b, 53);
            emit_case(out, "edge", a, b, 53, MPFR_RNDN);
            mpfr_clear(a); mpfr_clear(b);
        }

        /* (12-15) +-Inf / +-Inf -> NaN — 4 sign combinations. */
        {
            mpfr_t a, b; init_pos_inf(a, 53); init_pos_inf(b, 53);
            emit_case(out, "edge", a, b, 53, MPFR_RNDN);
            mpfr_clear(a); mpfr_clear(b);
        }
        {
            mpfr_t a, b; init_pos_inf(a, 53); init_neg_inf(b, 53);
            emit_case(out, "edge", a, b, 53, MPFR_RNDN);
            mpfr_clear(a); mpfr_clear(b);
        }
        {
            mpfr_t a, b; init_neg_inf(a, 53); init_pos_inf(b, 53);
            emit_case(out, "edge", a, b, 53, MPFR_RNDN);
            mpfr_clear(a); mpfr_clear(b);
        }
        {
            mpfr_t a, b; init_neg_inf(a, 53); init_neg_inf(b, 53);
            emit_case(out, "edge", a, b, 53, MPFR_RNDN);
            mpfr_clear(a); mpfr_clear(b);
        }

        /* (16-19) +-Inf / +-finite-nonzero -> sign-product Inf. */
        {
            mpfr_t a, b; init_pos_inf(a, 53); init_from_double(b, 3.14, 53);
            emit_case(out, "edge", a, b, 53, MPFR_RNDN); /* +Inf */
            mpfr_clear(a); mpfr_clear(b);
        }
        {
            mpfr_t a, b; init_pos_inf(a, 53); init_from_double(b, -3.14, 53);
            emit_case(out, "edge", a, b, 53, MPFR_RNDN); /* -Inf */
            mpfr_clear(a); mpfr_clear(b);
        }
        {
            mpfr_t a, b; init_neg_inf(a, 53); init_from_double(b, 3.14, 53);
            emit_case(out, "edge", a, b, 53, MPFR_RNDN); /* -Inf */
            mpfr_clear(a); mpfr_clear(b);
        }
        {
            mpfr_t a, b; init_neg_inf(a, 53); init_from_double(b, -3.14, 53);
            emit_case(out, "edge", a, b, 53, MPFR_RNDN); /* +Inf */
            mpfr_clear(a); mpfr_clear(b);
        }

        /* (20-23) +-finite / +-Inf -> sign-product zero. */
        {
            mpfr_t a, b; init_from_double(a, 3.14, 53); init_pos_inf(b, 53);
            emit_case(out, "edge", a, b, 53, MPFR_RNDN); /* +0 */
            mpfr_clear(a); mpfr_clear(b);
        }
        {
            mpfr_t a, b; init_from_double(a, 3.14, 53); init_neg_inf(b, 53);
            emit_case(out, "edge", a, b, 53, MPFR_RNDN); /* -0 */
            mpfr_clear(a); mpfr_clear(b);
        }
        {
            mpfr_t a, b; init_from_double(a, -3.14, 53); init_pos_inf(b, 53);
            emit_case(out, "edge", a, b, 53, MPFR_RNDN); /* -0 */
            mpfr_clear(a); mpfr_clear(b);
        }
        {
            mpfr_t a, b; init_from_double(a, -3.14, 53); init_neg_inf(b, 53);
            emit_case(out, "edge", a, b, 53, MPFR_RNDN); /* +0 */
            mpfr_clear(a); mpfr_clear(b);
        }

        /* (24-27) +-finite / +-0 -> sign-product Inf (divbyzero). */
        {
            mpfr_t a, b; init_from_double(a, 3.14, 53); init_pos_zero(b, 53);
            emit_case(out, "edge", a, b, 53, MPFR_RNDN); /* +Inf */
            mpfr_clear(a); mpfr_clear(b);
        }
        {
            mpfr_t a, b; init_from_double(a, 3.14, 53); init_neg_zero(b, 53);
            emit_case(out, "edge", a, b, 53, MPFR_RNDN); /* -Inf */
            mpfr_clear(a); mpfr_clear(b);
        }
        {
            mpfr_t a, b; init_from_double(a, -3.14, 53); init_pos_zero(b, 53);
            emit_case(out, "edge", a, b, 53, MPFR_RNDN); /* -Inf */
            mpfr_clear(a); mpfr_clear(b);
        }
        {
            mpfr_t a, b; init_from_double(a, -3.14, 53); init_neg_zero(b, 53);
            emit_case(out, "edge", a, b, 53, MPFR_RNDN); /* +Inf */
            mpfr_clear(a); mpfr_clear(b);
        }

        /* (28-31) +-0 / +-finite-nonzero -> sign-product zero. */
        {
            mpfr_t a, b; init_pos_zero(a, 53); init_from_double(b, 3.14, 53);
            emit_case(out, "edge", a, b, 53, MPFR_RNDN); /* +0 */
            mpfr_clear(a); mpfr_clear(b);
        }
        {
            mpfr_t a, b; init_pos_zero(a, 53); init_from_double(b, -3.14, 53);
            emit_case(out, "edge", a, b, 53, MPFR_RNDN); /* -0 */
            mpfr_clear(a); mpfr_clear(b);
        }
        {
            mpfr_t a, b; init_neg_zero(a, 53); init_from_double(b, 3.14, 53);
            emit_case(out, "edge", a, b, 53, MPFR_RNDN); /* -0 */
            mpfr_clear(a); mpfr_clear(b);
        }
        {
            mpfr_t a, b; init_neg_zero(a, 53); init_from_double(b, -3.14, 53);
            emit_case(out, "edge", a, b, 53, MPFR_RNDN); /* +0 */
            mpfr_clear(a); mpfr_clear(b);
        }

        /* (32-35) 0/0 -> NaN, 4 sign combinations. */
        {
            mpfr_t a, b; init_pos_zero(a, 53); init_pos_zero(b, 53);
            emit_case(out, "edge", a, b, 53, MPFR_RNDN);
            mpfr_clear(a); mpfr_clear(b);
        }
        {
            mpfr_t a, b; init_pos_zero(a, 53); init_neg_zero(b, 53);
            emit_case(out, "edge", a, b, 53, MPFR_RNDN);
            mpfr_clear(a); mpfr_clear(b);
        }
        {
            mpfr_t a, b; init_neg_zero(a, 53); init_pos_zero(b, 53);
            emit_case(out, "edge", a, b, 53, MPFR_RNDN);
            mpfr_clear(a); mpfr_clear(b);
        }
        {
            mpfr_t a, b; init_neg_zero(a, 53); init_neg_zero(b, 53);
            emit_case(out, "edge", a, b, 53, MPFR_RNDN);
            mpfr_clear(a); mpfr_clear(b);
        }

        /* (36-40) x/1 = x at all 5 rnd modes — should be exact (ternary 0). */
        for (int i = 0; i < 5; ++i) {
            mpfr_t a, b;
            init_from_double(a, 3.14, 53);
            init_from_double(b, 1.0, 53);
            emit_case(out, "edge", a, b, 53, RNDS[i]);
            mpfr_clear(a); mpfr_clear(b);
        }

        /* (41-45) x/(2^k) — pure exponent shift, exact at sufficient prec. */
        emit_dd(out, "edge",  6.0,  53,  2.0,  53,  53, MPFR_RNDN); /* 3 */
        emit_dd(out, "edge",  7.0,  53,  4.0,  53,  53, MPFR_RNDN); /* 1.75 */
        emit_dd(out, "edge",  3.0,  53,  8.0,  53,  53, MPFR_RNDN); /* 0.375 */
        emit_dd(out, "edge", 13.5,  53,  4.0,  53,  53, MPFR_RNDN); /* 3.375 */
        emit_dd(out, "edge",  1.0,  53, 16.0,  53,  53, MPFR_RNDN); /* 0.0625 */

        /* (46-50) 1/3 at all 5 rnd modes — exercises ternary direction. */
        for (int i = 0; i < 5; ++i) {
            mpfr_t a, b;
            init_from_double(a, 1.0, 53);
            init_from_double(b, 3.0, 53);
            emit_case(out, "edge", a, b, 53, RNDS[i]);
            mpfr_clear(a); mpfr_clear(b);
        }

        /* (51-55) -1/3 at all 5 rnd modes — mirror of (46-50) with sign flip.
         *      The RNDU/RNDD asymmetry confirms ternary sign is computed
         *      against the RESULT sign, not just a.sign. */
        for (int i = 0; i < 5; ++i) {
            mpfr_t a, b;
            init_from_double(a, -1.0, 53);
            init_from_double(b,  3.0, 53);
            emit_case(out, "edge", a, b, 53, RNDS[i]);
            mpfr_clear(a); mpfr_clear(b);
        }

        /* (56) Different precs in a and b. */
        emit_dd(out, "edge", 22.0,  53,  7.0,  64,  64, MPFR_RNDN);
        emit_dd(out, "edge",  1.0,  64,  7.0, 100, 100, MPFR_RNDN);

        /* (58) prec=1 — extreme rounding. */
        emit_dd(out, "edge",  2.0, 53,  1.0, 53,   1, MPFR_RNDN); /* 2 — exact */
        emit_dd(out, "edge",  3.0, 53,  2.0, 53,   1, MPFR_RNDN); /* 1.5 -> 2 (RNDN ties-to-even) */

        /* (60) prec << input prec — heavy rounding. */
        emit_dd(out, "edge", 22.0,  200, 7.0, 200, 24, MPFR_RNDN);
    }

    /* ============================================================== */
    /* adversarial: ~20 cases — ternary direction + cross-mode disagree */
    /* ============================================================== */
    {
        /* (1-5) 1/3 across all 5 rnd modes at prec=53. Each mode rounds
         *      differently; ternary direction must follow result sign. */
        for (int i = 0; i < 5; ++i) {
            mpfr_t a, b;
            init_from_double(a, 1.0, 53);
            init_from_double(b, 3.0, 53);
            emit_case(out, "adversarial", a, b, 53, RNDS[i]);
            mpfr_clear(a); mpfr_clear(b);
        }

        /* (6-10) 1/3 at prec=2 across all 5 rnd modes. Very narrow
         *      output prec amplifies rounding ambiguity. 1/3 ~ 0.333...
         *      at prec=2 rounds to {0.25, 0.375} -> {1*2^-2, 3*2^-3}. */
        for (int i = 0; i < 5; ++i) {
            mpfr_t a, b;
            init_from_double(a, 1.0, 53);
            init_from_double(b, 3.0, 53);
            emit_case(out, "adversarial", a, b, 2, RNDS[i]);
            mpfr_clear(a); mpfr_clear(b);
        }

        /* (11-15) Mixed-sign: -1/3 at prec=2 across all 5 rnd modes.
         *      RNDU vs RNDD asymmetry IS the ternary sign-direction
         *      check: rounding "up" toward +Inf gives less-negative;
         *      rounding "down" toward -Inf gives more-negative. This
         *      test catches a port that confuses "sign of result" with
         *      "sign of first operand" in the ternary computation. */
        for (int i = 0; i < 5; ++i) {
            mpfr_t a, b;
            init_from_double(a, -1.0, 53);
            init_from_double(b,  3.0, 53);
            emit_case(out, "adversarial", a, b, 2, RNDS[i]);
            mpfr_clear(a); mpfr_clear(b);
        }

        /* (16) High-precision pi / e -> rounded to prec=53. */
        {
            mpfr_t a, b;
            mpfr_init2(a, 200); mpfr_init2(b, 200);
            mpfr_const_pi(a, MPFR_RNDN);
            mpfr_const_euler(b, MPFR_RNDN); /* Euler's gamma — well-known irrational */
            emit_case(out, "adversarial", a, b, 53, MPFR_RNDN);
            mpfr_clear(a); mpfr_clear(b);
        }

        /* (17) Very disparate magnitudes — exponent composition test. */
        {
            mpfr_t a, b;
            mpfr_init2(a, 53); mpfr_init2(b, 53);
            mpfr_set_d(a, 1.0e150, MPFR_RNDN);
            mpfr_set_d(b, 1.0e-150, MPFR_RNDN);
            emit_case(out, "adversarial", a, b, 53, MPFR_RNDN);
            mpfr_clear(a); mpfr_clear(b);
        }

        /* (18) (1 + 2^-52) / (1 + 2^-52) = 1 — exact division of identical
         *      values, exercising the quotient-bit-length carry path. */
        {
            mpfr_t a, b, one, eps;
            mpfr_init2(a, 53); mpfr_init2(b, 53);
            mpfr_init2(one, 53); mpfr_init2(eps, 53);
            mpfr_set_d(one, 1.0, MPFR_RNDN);
            mpfr_set_ui_2exp(eps, 1, -52, MPFR_RNDN);
            mpfr_add(a, one, eps, MPFR_RNDN);
            mpfr_add(b, one, eps, MPFR_RNDN);
            emit_case(out, "adversarial", a, b, 53, MPFR_RNDN);
            mpfr_clear(a); mpfr_clear(b);
            mpfr_clear(one); mpfr_clear(eps);
        }

        /* (19) Asymmetric prec with mixed-sign — exercises the
         *      cross-product of every dispatch branch. */
        {
            mpfr_t a, b;
            mpfr_init2(a, 17); mpfr_init2(b, 89);
            mpfr_set_d(a, -0.1, MPFR_RNDN);
            mpfr_set_d(b,  0.3, MPFR_RNDN);
            emit_case(out, "adversarial", a, b, 53, MPFR_RNDU);
            mpfr_clear(a); mpfr_clear(b);
        }

        /* (20) Ratio just above 1: a slightly larger than b, quotient
         *      near 1 — exercises the "L == prec" vs "L == prec+1"
         *      decision in the bigint quotient bit-length. */
        {
            mpfr_t a, b;
            mpfr_init2(a, 53); mpfr_init2(b, 53);
            mpfr_set_d(a, 1.5000000001, MPFR_RNDN);
            mpfr_set_d(b, 1.5,          MPFR_RNDN);
            emit_case(out, "adversarial", a, b, 53, MPFR_RNDN);
            mpfr_clear(a); mpfr_clear(b);
        }
    }

    /* ============================================================== */
    /* fuzz: 100 cases — PRNG, random doubles × random precs × rnd     */
    /*                                                                  */
    /* Seed: 0xD1ED1ED1ED1EULL (valid hex).                            */
    /* ============================================================== */
    {
        xs64_t rng;
        xs64_seed(&rng, 0xD1ED1ED1ED1EULL);
        int emitted = 0;
        while (emitted < 100) {
            const uint64_t bits_a = xs64_next(&rng);
            const uint64_t bits_b = xs64_next(&rng);
            const uint64_t exp_a = (bits_a >> 52) & 0x7FF;
            const uint64_t exp_b = (bits_b >> 52) & 0x7FF;
            /* Skip NaN/Inf doubles (those are exercised in edge). */
            if (exp_a == 0x7FF || exp_b == 0x7FF) continue;

            const double da = bits_to_double(bits_a);
            const double db = bits_to_double(bits_b);

            /* Skip when b is a true zero (we cover it in edge, and
             * fuzz is meant for the regular path). The double-bit form
             * for +0 has exp==0 and the 52-bit mantissa all zero. */
            if ((bits_b & ~(1ULL << 63)) == 0) continue;

            const uint64_t pa   = 1 + xs64_below(&rng, 200);
            const uint64_t pb   = 1 + xs64_below(&rng, 200);
            const uint64_t prec = 1 + xs64_below(&rng, 200);
            const mpfr_rnd_t rnd = RNDS[xs64_below(&rng, 5)];

            mpfr_t a, b;
            init_from_double(a, da, pa);
            init_from_double(b, db, pb);
            /* If b rounded to exactly zero at its target prec
             * (subnormal-into-zero etc.), skip — divbyzero behaviour
             * is in edge. */
            if (mpfr_zero_p(b)) {
                mpfr_clear(a); mpfr_clear(b);
                continue;
            }
            emit_case(out, "fuzz", a, b, prec, rnd);
            mpfr_clear(a); mpfr_clear(b);
            emitted++;
        }
    }

    /* ============================================================== */
    /* mined: 7 cases — transcribed from mpfr/tests/tdiv.c             */
    /* ============================================================== */
    {
        /* Classic 1/3, 2/3 — the bread-and-butter inexact divisions. */
        emit_ss(out, "mined", "1.0", 53, "3.0", 53, 53, MPFR_RNDN);
        emit_ss(out, "mined", "2.0", 53, "3.0", 53, 53, MPFR_RNDN);

        /* tdiv.c style: large-magnitude positives that produce results
         * near the prec=53 rounding boundary. */
        emit_ss(out, "mined", "67108865.0", 53, "134217729.0", 53,
                53, MPFR_RNDN);

        /* tdiv.c style: a/b across rnd modes to exercise direction.
         * 22/7 — Archimedes' rational pi approximation. */
        emit_ss(out, "mined", "22.0", 53, "7.0", 53, 53, MPFR_RNDZ);
        emit_ss(out, "mined", "22.0", 53, "7.0", 53, 53, MPFR_RNDU);

        /* tdiv.c style: mixed-sign division near a tie. */
        emit_ss(out, "mined", "-4.165000000e4", 53,
                "-2.0825000000e4", 53,
                53, MPFR_RNDN); /* 2.0 — exact, sign-product positive */

        /* tdiv.c style: very small/very large mixed magnitudes. */
        emit_ss(out, "mined", "2.71331408349172961467e-08", 53,
                "6.72658901114033715233e-165", 53,
                53, MPFR_RNDN);
    }

    return 0;
}
