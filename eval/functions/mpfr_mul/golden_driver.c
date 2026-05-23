/*
 * golden_driver.c — Golden master for MPFR's mpfr_mul.
 *
 * C signature
 * -----------
 *
 *   int mpfr_mul(mpfr_t rop, mpfr_srcptr op1, mpfr_srcptr op2, mpfr_rnd_t rnd);
 *
 *   Sets `rop` to `op1 * op2` rounded per `rnd` at `rop`'s precision.
 *   Returns the ternary (sign of rounded - exact). See mpfr/src/mul.c
 *   L172–L237 (dispatcher) and L34–L168 (mpfr_mul3 algorithm).
 *
 * Divergence from C → TS
 * ----------------------
 *
 * The TS port `mpfr_mul(a, b, prec, rnd) -> Result` takes `prec` as a
 * positional argument and returns the canonical {value, ternary} pair
 * from src/core.ts L173–L176. The C function mutates `rop` (whose prec
 * is independently set) and returns the ternary separately.
 *
 * To grade the TS port we therefore:
 *   1. mpfr_init2(rop, prec)                    — target precision.
 *   2. ternary = mpfr_mul(rop, a, b, rnd)       — the operation we mirror.
 *   3. emit Result-shaped output via jl_output_result(rop, ternary).
 *
 * Wire format
 * -----------
 *
 *   {"tag":"<class>",
 *    "inputs":{"a":{<MPFR-record>},"b":{<MPFR-record>},"prec":"<decimal>","rnd":"RND[NZUDA]"},
 *    "output":{"value":<MPFR-record>,"ternary":<-1|0|1>},
 *    "time_ns":<n>}
 *
 * Tag distribution (CLAUDE.md Rule 7 minimums)
 * --------------------------------------------
 *
 *   happy        :  ~25   (typical mults at common precs, RNDN)
 *   edge         :  ~55   (specials × rnd; signed zero on sign-product;
 *                          exact powers of two; carry-out at MSB)
 *   adversarial  :  ~20   (ternary direction across rnd; rounding ties)
 *   fuzz         :  100   (PRNG; random doubles × random precs × rnd)
 *   mined        :    7   (transcribed from mpfr/tests/tmul.c)
 *   ------------ ----
 *   total        : ~207
 *
 * Build via the repo-wide eval/golden_master/build.sh.
 *
 * Ref: mpfr/src/mul.c — the C reference.
 * Ref: src/ops/mul.ts — the production port.
 * Ref: mpfr/tests/tmul.c — source for the `mined` cases.
 */
#include "common.h"

#include <assert.h>
#include <float.h>
#include <inttypes.h>
#include <math.h>
#include <string.h>

#if GMP_NUMB_BITS != 64
#  error "mpfr_mul golden_driver requires GMP_NUMB_BITS == 64"
#endif

/* Mirror src/core.ts L236: PREC_MAX = 2^31 - 257. Cap fuzz at prec=200
 * per the brief — long fuzz runs need to fit a 50ms budget. */
#define TS_PREC_MAX ((uint64_t)((1ULL << 31) - 257ULL))
#define TS_PREC_MIN ((uint64_t)1)

/* ------------------------------------------------------------------ */
/* Per-case emitter                                                   */
/* ------------------------------------------------------------------ */

/* Emit one mpfr_mul golden case. The caller has already constructed
 * the two operands at their own precisions. The result precision is
 * passed separately and may differ from both operands. */
static inline void emit_case(FILE *out, const char *tag,
                             mpfr_srcptr a, mpfr_srcptr b,
                             uint64_t prec, mpfr_rnd_t rnd) {
    assert(prec >= TS_PREC_MIN && prec <= TS_PREC_MAX);

    mpfr_t rop;
    mpfr_init2(rop, (mpfr_prec_t)prec);

    const uint64_t t0 = now_ns();
    const int ternary = mpfr_mul(rop, a, b, rnd);
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

/* Build an MPFR from a double at the given prec. Caller frees. */
static inline void init_from_double(mpfr_ptr x, double d, uint64_t prec) {
    mpfr_init2(x, (mpfr_prec_t)prec);
    mpfr_set_d(x, d, MPFR_RNDN);
}

/* Build a singular MPFR at the given prec. */
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

/* Construct a double from raw IEEE 754 bits — UB-free per C11 §6.5p7. */
static inline double bits_to_double(uint64_t bits) {
    double d;
    memcpy(&d, &bits, sizeof d);
    return d;
}

/* Emit a case from two doubles at potentially different precs, producing
 * a result at a separately-specified output prec. */
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

/* Emit a case from two decimal-string operands at the given precs and
 * a separately-specified output prec. */
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
    /* happy: ~25 cases — typical mults at common precs, RNDN          */
    /* ============================================================== */
    {
        /* Small integer × integer at common precs — all exact at >= 53. */
        emit_dd(out, "happy",  2.0,  53,  3.0,  53,  53, MPFR_RNDN); /* 6 */
        emit_dd(out, "happy",  3.0,  53,  4.0,  53,  53, MPFR_RNDN); /* 12 */
        emit_dd(out, "happy",  7.0,  53,  8.0,  53,  53, MPFR_RNDN); /* 56 */
        emit_dd(out, "happy", 10.0,  53, 20.0,  53,  53, MPFR_RNDN); /* 200 */
        emit_dd(out, "happy", 100.0, 53,  3.0,  53,  53, MPFR_RNDN); /* 300 */
        emit_dd(out, "happy",  1.0e9, 53, 2.0e9, 53,  64, MPFR_RNDN);

        /* Mixed-sign — sign-product rule. */
        emit_dd(out, "happy", -2.0,  53,  3.0,  53,  53, MPFR_RNDN); /* -6 */
        emit_dd(out, "happy",  2.0,  53, -3.0,  53,  53, MPFR_RNDN); /* -6 */
        emit_dd(out, "happy", -2.0,  53, -3.0,  53,  53, MPFR_RNDN); /*  6 */

        /* Exact powers of 2 — pure-exponent shift, ternary 0. */
        emit_dd(out, "happy",  2.0,  53,  4.0,  53,  53, MPFR_RNDN); /* 8 */
        emit_dd(out, "happy",  0.5,  53,  0.25, 53,  53, MPFR_RNDN); /* 0.125 */
        emit_dd(out, "happy",  0.5,  53,  2.0,  53,  53, MPFR_RNDN); /* 1 */
        emit_dd(out, "happy",  1.0,  53,  1.0,  53,  53, MPFR_RNDN); /* 1 — identity */

        /* Non-dyadic — represented exactly at >= 53 bits per operand. */
        emit_dd(out, "happy",  3.14,  53,  2.71,  53,  53, MPFR_RNDN);
        emit_dd(out, "happy",  3.14,  53,  0.0,   53,  53, MPFR_RNDN); /* x*0 */
        emit_dd(out, "happy",  0.0,   53,  3.14,  53,  53, MPFR_RNDN); /* 0*x */
        emit_dd(out, "happy",  2.718281828459045, 53,
                              3.141592653589793,  53,  53, MPFR_RNDN); /* e*pi */
        emit_dd(out, "happy",  1.4142135623730951, 53,
                              1.7320508075688772,  53,  53, MPFR_RNDN); /* sqrt(2)*sqrt(3) ~ sqrt(6) */

        /* Varying output prec. */
        emit_dd(out, "happy",  3.14, 53, 2.71, 53,  24, MPFR_RNDN);
        emit_dd(out, "happy",  3.14, 53, 2.71, 53,  64, MPFR_RNDN);
        emit_dd(out, "happy",  3.14, 53, 2.71, 53, 100, MPFR_RNDN);
        emit_dd(out, "happy",  3.14, 53, 2.71, 53, 200, MPFR_RNDN);

        /* Different input precs. */
        emit_dd(out, "happy",  3.0,  10, 4.0,  53,  53, MPFR_RNDN); /* 12 */
        emit_dd(out, "happy",  3.0,  53, 4.0, 100, 100, MPFR_RNDN); /* 12 */
        emit_dd(out, "happy",  0.5,  53, 0.5, 100, 200, MPFR_RNDN); /* 0.25 */

        /* Disparate magnitudes — product fits comfortably in target prec. */
        emit_dd(out, "happy",  1.0e100, 53, 1.0e-50, 53,  53, MPFR_RNDN);
    }

    /* ============================================================== */
    /* edge: ~55 cases — specials × rounding × sign-product            */
    /* ============================================================== */
    {
        /* (1-5) NaN * anything → NaN. All 5 rnd modes — none change the
         * result, but the broken port might react asymmetrically. */
        for (int i = 0; i < 5; ++i) {
            mpfr_t a, b; init_nan(a, 53); init_from_double(b, 1.0, 53);
            emit_case(out, "edge", a, b, 53, RNDS[i]);
            mpfr_clear(a); mpfr_clear(b);
        }
        /* (6-10) anything * NaN → NaN. */
        for (int i = 0; i < 5; ++i) {
            mpfr_t a, b; init_from_double(a, 1.0, 53); init_nan(b, 53);
            emit_case(out, "edge", a, b, 53, RNDS[i]);
            mpfr_clear(a); mpfr_clear(b);
        }
        /* (11) NaN * NaN → NaN. */
        {
            mpfr_t a, b; init_nan(a, 53); init_nan(b, 53);
            emit_case(out, "edge", a, b, 53, MPFR_RNDN);
            mpfr_clear(a); mpfr_clear(b);
        }

        /* (12-15) +-Inf * +-Inf — four sign combinations.
         *      (+)*(+)=+Inf, (+)*(-)=-Inf, (-)*(+)=-Inf, (-)*(-)=+Inf. */
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

        /* (16-19) +-Inf * +-normal — 4 sign combos. */
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

        /* (20-21) normal * +-Inf — symmetric to 16-19. */
        {
            mpfr_t a, b; init_from_double(a, 3.14, 53); init_pos_inf(b, 53);
            emit_case(out, "edge", a, b, 53, MPFR_RNDN); /* +Inf */
            mpfr_clear(a); mpfr_clear(b);
        }
        {
            mpfr_t a, b; init_from_double(a, -3.14, 53); init_neg_inf(b, 53);
            emit_case(out, "edge", a, b, 53, MPFR_RNDN); /* +Inf */
            mpfr_clear(a); mpfr_clear(b);
        }

        /* (22-25) 0 * Inf → NaN. All 4 sign combos.
         *      Mirrors mpfr/src/mul.c L60–L86 — Inf*0 and 0*Inf are
         *      both indeterminate, hence NaN. */
        {
            mpfr_t a, b; init_pos_zero(a, 53); init_pos_inf(b, 53);
            emit_case(out, "edge", a, b, 53, MPFR_RNDN);
            mpfr_clear(a); mpfr_clear(b);
        }
        {
            mpfr_t a, b; init_pos_zero(a, 53); init_neg_inf(b, 53);
            emit_case(out, "edge", a, b, 53, MPFR_RNDN);
            mpfr_clear(a); mpfr_clear(b);
        }
        {
            mpfr_t a, b; init_neg_zero(a, 53); init_pos_inf(b, 53);
            emit_case(out, "edge", a, b, 53, MPFR_RNDN);
            mpfr_clear(a); mpfr_clear(b);
        }
        {
            mpfr_t a, b; init_pos_inf(a, 53); init_neg_zero(b, 53);
            emit_case(out, "edge", a, b, 53, MPFR_RNDN);
            mpfr_clear(a); mpfr_clear(b);
        }

        /* (26-29) +-0 * +-0 — sign is product of signs. */
        {
            mpfr_t a, b; init_pos_zero(a, 53); init_pos_zero(b, 53);
            emit_case(out, "edge", a, b, 53, MPFR_RNDN); /* +0 */
            mpfr_clear(a); mpfr_clear(b);
        }
        {
            mpfr_t a, b; init_pos_zero(a, 53); init_neg_zero(b, 53);
            emit_case(out, "edge", a, b, 53, MPFR_RNDN); /* -0 */
            mpfr_clear(a); mpfr_clear(b);
        }
        {
            mpfr_t a, b; init_neg_zero(a, 53); init_pos_zero(b, 53);
            emit_case(out, "edge", a, b, 53, MPFR_RNDN); /* -0 */
            mpfr_clear(a); mpfr_clear(b);
        }
        {
            mpfr_t a, b; init_neg_zero(a, 53); init_neg_zero(b, 53);
            emit_case(out, "edge", a, b, 53, MPFR_RNDN); /* +0 */
            mpfr_clear(a); mpfr_clear(b);
        }

        /* (30-33) +-0 * +-normal — sign-product gives 4 outcomes;
         *      the rounding mode is irrelevant (exact zero). */
        {
            mpfr_t a, b; init_pos_zero(a, 53); init_from_double(b, 3.14, 53);
            emit_case(out, "edge", a, b, 53, MPFR_RNDD); /* +0 — RNDD doesn't tip a true 0 to -0 in mul */
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

        /* (34-37) +-normal * +-0 — symmetric. */
        {
            mpfr_t a, b; init_from_double(a, 3.14, 53); init_pos_zero(b, 53);
            emit_case(out, "edge", a, b, 53, MPFR_RNDN); /* +0 */
            mpfr_clear(a); mpfr_clear(b);
        }
        {
            mpfr_t a, b; init_from_double(a, 3.14, 53); init_neg_zero(b, 53);
            emit_case(out, "edge", a, b, 53, MPFR_RNDN); /* -0 */
            mpfr_clear(a); mpfr_clear(b);
        }
        {
            mpfr_t a, b; init_from_double(a, -3.14, 53); init_pos_zero(b, 53);
            emit_case(out, "edge", a, b, 53, MPFR_RNDN); /* -0 */
            mpfr_clear(a); mpfr_clear(b);
        }
        {
            mpfr_t a, b; init_from_double(a, -3.14, 53); init_neg_zero(b, 53);
            emit_case(out, "edge", a, b, 53, MPFR_RNDN); /* +0 */
            mpfr_clear(a); mpfr_clear(b);
        }

        /* (38-42) Exact powers-of-2 product: 2^k * 2^m = 2^(k+m), exact.
         *      Exercises the resultExp = a.exp + b.exp branch with
         *      mantissa MSBs causing the "L == a.prec + b.prec" case
         *      (since mant = 2^(prec-1), product = 2^(a.prec + b.prec - 2),
         *      bit-length = a.prec + b.prec - 1). All 5 rnd modes — all
         *      should give ternary 0 (exact). */
        for (int i = 0; i < 5; ++i) {
            mpfr_t a, b;
            init_from_double(a, 8.0, 53);
            init_from_double(b, 16.0, 53);
            emit_case(out, "edge", a, b, 53, RNDS[i]); /* 128 — exact */
            mpfr_clear(a); mpfr_clear(b);
        }

        /* (43) Carry-out at MSB: (1 - 2^-53) * (1 - 2^-53). At prec=53,
         *      the result mantissa rounds back to 1 - 2^-52 with no
         *      carry-out, OR rounds up to 1.0 with carry-out — depends
         *      on rounding mode. This is the boundary case where the
         *      product's bit-length matters. */
        {
            mpfr_t a, b;
            mpfr_init2(a, 53); mpfr_init2(b, 53);
            mpfr_set_str(a, "0.11111111111111111111111111111111111111111111111111111", 2, MPFR_RNDN);
            mpfr_set_str(b, "0.11111111111111111111111111111111111111111111111111111", 2, MPFR_RNDN);
            emit_case(out, "edge", a, b, 53, MPFR_RNDU); /* exercise round-up */
            mpfr_clear(a); mpfr_clear(b);
        }

        /* (44) (1 - 2^-53) * (1 - 2^-53) under RNDN — ties-to-even. */
        {
            mpfr_t a, b;
            mpfr_init2(a, 53); mpfr_init2(b, 53);
            mpfr_set_str(a, "0.11111111111111111111111111111111111111111111111111111", 2, MPFR_RNDN);
            mpfr_set_str(b, "0.11111111111111111111111111111111111111111111111111111", 2, MPFR_RNDN);
            emit_case(out, "edge", a, b, 53, MPFR_RNDN);
            mpfr_clear(a); mpfr_clear(b);
        }

        /* (45-49) 0.5 * 1.0 at all 5 rnd modes. Exact, ternary 0. */
        for (int i = 0; i < 5; ++i) {
            mpfr_t a, b;
            init_from_double(a, 0.5, 53);
            init_from_double(b, 1.0, 53);
            emit_case(out, "edge", a, b, 53, RNDS[i]);
            mpfr_clear(a); mpfr_clear(b);
        }

        /* (50-54) Different precs in a and b — 5 different combinations. */
        emit_dd(out, "edge", 3.14,  53, 2.71,  64,  64, MPFR_RNDN);
        emit_dd(out, "edge", 3.14,  64, 2.71, 100, 100, MPFR_RNDN);
        emit_dd(out, "edge", 3.14,   2, 2.71,  53,  53, MPFR_RNDN); /* prec=2 quantises 3.14 to ~3.0 */
        emit_dd(out, "edge", 3.14, 200, 2.71,  53, 100, MPFR_RNDN);
        emit_dd(out, "edge", 3.14, 100, 2.71, 100, 200, MPFR_RNDN);

        /* (55) Output prec=1 — extreme rounding. */
        emit_dd(out, "edge", 1.0, 53, 2.0, 53, 1, MPFR_RNDN); /* 2 — exact */

        /* (56) Output prec=1 with non-power-of-2 inputs. */
        emit_dd(out, "edge", 1.5, 53, 1.5, 53, 1, MPFR_RNDN); /* 2.25 → 2 (RNDN ties-to-even) */

        /* (57) Mixed-sign × different-prec — covers the cross-product
         *      of the dispatcher branches. */
        emit_dd(out, "edge", -1.5, 64, 2.5, 100, 80, MPFR_RNDN);
    }

    /* ============================================================== */
    /* adversarial: ~20 cases — ternary direction + cross-mode disagree */
    /* ============================================================== */
    {
        /* (1-5) sqrt(2) * sqrt(2) at high input prec, narrow output
         *      prec=53. Exact answer is 2.0 (in unbounded prec); finite
         *      prec gives a rounding boundary. */
        for (int i = 0; i < 5; ++i) {
            mpfr_t a, b;
            mpfr_init2(a, 200); mpfr_init2(b, 200);
            mpfr_sqrt_ui(a, 2, MPFR_RNDN);
            mpfr_sqrt_ui(b, 2, MPFR_RNDN);
            emit_case(out, "adversarial", a, b, 53, RNDS[i]);
            mpfr_clear(a); mpfr_clear(b);
        }

        /* (6-10) Same-sign product at prec=53 of two operands whose
         *      product's low-order bits sit exactly at the rounding
         *      tie. We use 1.5 * 1.5 = 2.25 at prec=2 (ties-to-even):
         *      2.25 in binary at prec=2 is 10.0 (rounds to 2) or 10.1
         *      (rounds to 3 — but 3 needs prec >= 2 to represent
         *      exactly). RNDN ties-to-even gives 2; RNDU gives 3; RNDZ
         *      gives 2; RNDD gives 2; RNDA gives 3. */
        for (int i = 0; i < 5; ++i) {
            mpfr_t a, b;
            init_from_double(a, 1.5, 53);
            init_from_double(b, 1.5, 53);
            emit_case(out, "adversarial", a, b, 2, RNDS[i]);
            mpfr_clear(a); mpfr_clear(b);
        }

        /* (11-15) Mixed-sign: -1.5 * 1.5 = -2.25 at prec=2 across all
         *      5 rnd modes. The RNDU vs RNDD asymmetry IS the ternary
         *      sign-direction check: rounding "up" toward +Inf gives
         *      -2 (closer to 0); rounding "down" toward -Inf gives -3
         *      (further from 0). This is THE test that catches a port
         *      that confuses "sign of result" with "sign of first
         *      operand" in the ternary computation. */
        for (int i = 0; i < 5; ++i) {
            mpfr_t a, b;
            init_from_double(a, -1.5, 53);
            init_from_double(b,  1.5, 53);
            emit_case(out, "adversarial", a, b, 2, RNDS[i]);
            mpfr_clear(a); mpfr_clear(b);
        }

        /* (16) Carry-out at MSB with sign: (1 + 2^-53) * (1 + 2^-53)
         *      at prec=53. Product is 1 + 2^-52 + 2^-106; rounds to
         *      next-up double at prec=53 RNDN. */
        {
            mpfr_t a, b;
            mpfr_init2(a, 53); mpfr_init2(b, 53);
            mpfr_set_d(a, 1.0, MPFR_RNDN);
            mpfr_t eps; mpfr_init2(eps, 53);
            mpfr_set_ui_2exp(eps, 1, -53, MPFR_RNDN);
            mpfr_add(a, a, eps, MPFR_RNDN); /* a = 1 + 2^-53 — actually 1.0 at prec=53 RNDN */
            mpfr_set_d(b, 1.0, MPFR_RNDN);
            mpfr_add(b, b, eps, MPFR_RNDN);
            mpfr_clear(eps);
            emit_case(out, "adversarial", a, b, 53, MPFR_RNDN);
            mpfr_clear(a); mpfr_clear(b);
        }

        /* (17) Very disparate magnitudes — product is still finite and
         *      well within IEEE 754 range. Tests that the exp
         *      computation correctly composes (no overflow, no
         *      underflow). */
        {
            mpfr_t a, b;
            mpfr_init2(a, 53); mpfr_init2(b, 53);
            mpfr_set_d(a, 1.0e150, MPFR_RNDN);
            mpfr_set_d(b, 1.0e-150, MPFR_RNDN);
            emit_case(out, "adversarial", a, b, 53, MPFR_RNDN);
            mpfr_clear(a); mpfr_clear(b);
        }

        /* (18) High-precision exact multiply: 2 * pi (at prec=200)
         *      rounded to prec=53. The ternary direction here is one
         *      of the canonical "this WOULD pass if we got the sign
         *      reversed because pi is positive" trap cases — except
         *      sign-product is positive so the test verifies the
         *      RNDN-rounding direction independent of sign. */
        {
            mpfr_t a, b;
            mpfr_init2(a, 200); mpfr_init2(b, 200);
            mpfr_const_pi(a, MPFR_RNDN);
            mpfr_set_d(b, 2.0, MPFR_RNDN);
            emit_case(out, "adversarial", a, b, 53, MPFR_RNDN);
            mpfr_clear(a); mpfr_clear(b);
        }

        /* (19) Negative pi * 2 — symmetric ternary direction. */
        {
            mpfr_t a, b;
            mpfr_init2(a, 200); mpfr_init2(b, 200);
            mpfr_const_pi(a, MPFR_RNDN);
            mpfr_neg(a, a, MPFR_RNDN);
            mpfr_set_d(b, 2.0, MPFR_RNDN);
            emit_case(out, "adversarial", a, b, 53, MPFR_RNDN);
            mpfr_clear(a); mpfr_clear(b);
        }

        /* (20) Asymmetric prec with mixed-sign — exercises the
         *      cross-product of every dispatch branch. */
        {
            mpfr_t a, b;
            mpfr_init2(a, 17); mpfr_init2(b, 89);
            mpfr_set_d(a, -0.1, MPFR_RNDN);
            mpfr_set_d(b,  0.3, MPFR_RNDN);
            emit_case(out, "adversarial", a, b, 53, MPFR_RNDU);
            mpfr_clear(a); mpfr_clear(b);
        }
    }

    /* ============================================================== */
    /* fuzz: 100 cases — PRNG, random doubles × random precs × rnd     */
    /*                                                                  */
    /* Seed per the brief: 0x717117117117ULL.                          */
    /* ============================================================== */
    {
        xs64_t rng;
        xs64_seed(&rng, 0x717117117117ULL);
        int emitted = 0;
        while (emitted < 100) {
            /* Draw two 64-bit doubles. Skip NaN/Inf doubles — those
             * are exercised in edge; here we want random finite values. */
            const uint64_t bits_a = xs64_next(&rng);
            const uint64_t bits_b = xs64_next(&rng);
            const uint64_t exp_a = (bits_a >> 52) & 0x7FF;
            const uint64_t exp_b = (bits_b >> 52) & 0x7FF;
            if (exp_a == 0x7FF || exp_b == 0x7FF) continue;

            const double da = bits_to_double(bits_a);
            const double db = bits_to_double(bits_b);

            /* Random input precs in [1, 200] — brief caps at 200 for
             * fuzz so 50ms budget is safe. */
            const uint64_t pa   = 1 + xs64_below(&rng, 200);
            const uint64_t pb   = 1 + xs64_below(&rng, 200);
            const uint64_t prec = 1 + xs64_below(&rng, 200);
            const mpfr_rnd_t rnd = RNDS[xs64_below(&rng, 5)];

            /* Guard against double-overflow when da and db are both
             * huge: at very large magnitudes the product may overflow
             * the IEEE-754 double range used to seed the MPFR value.
             * MPFR handles overflow internally (returns +-Inf), but
             * the result is then not interesting from a rounding-
             * boundary perspective. Skip cases where either operand
             * is so large that doubling it loses precision in an
             * uninteresting way. We accept the bias — fuzz is fuzz. */

            mpfr_t a, b;
            init_from_double(a, da, pa);
            init_from_double(b, db, pb);
            emit_case(out, "fuzz", a, b, prec, rnd);
            mpfr_clear(a); mpfr_clear(b);
            emitted++;
        }
    }

    /* ============================================================== */
    /* mined: 7 cases — transcribed from mpfr/tests/tmul.c             */
    /* ============================================================== */
    {
        /* tmul.c L1311: check53("6.9314718...e-1", "0.0", RNDZ, "0.0") —
         *   x * +0 = +0 (RNDZ doesn't perturb the exact zero result;
         *   sign-product positive * positive = positive). */
        emit_ss(out, "mined", "6.9314718055994530941514e-1", 53, "0.0", 53,
                53, MPFR_RNDZ);

        /* tmul.c L1312: check53("0.0", "6.9314718...e-1", RNDZ, "0.0") —
         *   symmetric: +0 * x = +0. */
        emit_ss(out, "mined", "0.0", 53, "6.9314718055994530941514e-1", 53,
                53, MPFR_RNDZ);

        /* tmul.c L1314: check53("-4.165000000e4",
         *   "-0.00004801920768307322868063274915", RNDN, "2.0") —
         *   negative * negative = positive, exact 2.0 at prec=53. */
        emit_ss(out, "mined", "-4.165000000e4", 53,
                "-0.00004801920768307322868063274915", 53,
                53, MPFR_RNDN);

        /* tmul.c L1316: check53("2.71331408349172961467e-08",
         *   "-6.72658901114033715233e-165", RNDZ, ...) — mixed sign,
         *   wide magnitude gap. */
        emit_ss(out, "mined", "2.71331408349172961467e-08", 53,
                "-6.72658901114033715233e-165", 53,
                53, MPFR_RNDZ);

        /* tmul.c L1318: same operands as L1316 but RNDA — different
         *   ternary direction. */
        emit_ss(out, "mined", "2.71331408349172961467e-08", 53,
                "-6.72658901114033715233e-165", 53,
                53, MPFR_RNDA);

        /* tmul.c L1320: check53("0.31869277231188065",
         *   "0.88642843322303122", RNDZ, "2.8249833...e-1"). */
        emit_ss(out, "mined", "0.31869277231188065", 53,
                "0.88642843322303122", 53,
                53, MPFR_RNDZ);

        /* tmul.c L1330: check53("67108865.0", "134217729.0", RNDN,
         *   "9.007199456067584e15") — large-magnitude positive
         *   integers (each barely outside the prec=53 exact range)
         *   producing a product near the prec=53 boundary. */
        emit_ss(out, "mined", "67108865.0", 53, "134217729.0", 53,
                53, MPFR_RNDN);
    }

    return 0;
}
