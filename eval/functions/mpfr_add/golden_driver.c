/*
 * golden_driver.c — Golden master for MPFR's mpfr_add.
 *
 * C signature
 * -----------
 *
 *   int mpfr_add(mpfr_t rop, mpfr_srcptr op1, mpfr_srcptr op2, mpfr_rnd_t rnd);
 *
 *   Sets `rop` to `op1 + op2` rounded per `rnd` at `rop`'s precision.
 *   Returns the ternary (sign of rounded - exact). See mpfr/src/add.c
 *   L24–L121.
 *
 * Divergence from C → TS
 * ----------------------
 *
 * The TS port `mpfr_add(a, b, prec, rnd) -> Result` takes `prec` as a
 * positional argument and returns the canonical {value, ternary} pair
 * from src/core.ts L173–L176. The C function mutates `rop` (whose prec
 * is independently set) and returns the ternary separately.
 *
 * To grade the TS port we therefore:
 *   1. mpfr_init2(rop, prec)                    — target precision.
 *   2. ternary = mpfr_add(rop, a, b, rnd)       — the operation we mirror.
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
 *   - `a`, `b` as MPFR records via jl_kv_mpfr.
 *   - `prec` via jl_kv_u64 — decimal bigint string.
 *   - `rnd`  via jl_kv_rnd — RoundingMode string.
 *
 * Tag distribution (CLAUDE.md Rule 7 minimums)
 * --------------------------------------------
 *
 *   happy        :  ~25   (typical adds at common precs, RNDN)
 *   edge         :  ~50   (specials × all 5 rounding modes; signed zero;
 *                          cancellation; carry into next bit)
 *   adversarial  :  ~20   (ternary direction; cross-mode disagreement)
 *   fuzz         :  100   (PRNG; random doubles × random precs × rnd)
 *   mined        :    5  (transcribed from mpfr/tests/tadd.c)
 *   ------------ ----
 *   total        : ~200
 *
 * Build via the repo-wide eval/golden_master/build.sh.
 *
 * Ref: mpfr/src/add.c — the C reference.
 * Ref: src/ops/add.ts — the production port.
 * Ref: mpfr/tests/tadd.c — source for the `mined` cases.
 */
#include "common.h"

#include <assert.h>
#include <float.h>
#include <inttypes.h>
#include <math.h>
#include <string.h>

#if GMP_NUMB_BITS != 64
#  error "mpfr_add golden_driver requires GMP_NUMB_BITS == 64"
#endif

/* Mirror src/core.ts L236: PREC_MAX = 2^31 - 257. Cap fuzz at prec=200
 * per the brief — long fuzz runs need to fit a 50ms budget. */
#define TS_PREC_MAX ((uint64_t)((1ULL << 31) - 257ULL))
#define TS_PREC_MIN ((uint64_t)1)

/* ------------------------------------------------------------------ */
/* Per-case emitter                                                   */
/* ------------------------------------------------------------------ */

/* Emit one mpfr_add golden case. The caller has already constructed
 * the two operands at their own precisions. The result precision is
 * passed separately and may differ from both operands. */
static inline void emit_case(FILE *out, const char *tag,
                             mpfr_srcptr a, mpfr_srcptr b,
                             uint64_t prec, mpfr_rnd_t rnd) {
    assert(prec >= TS_PREC_MIN && prec <= TS_PREC_MAX);

    mpfr_t rop;
    mpfr_init2(rop, (mpfr_prec_t)prec);

    const uint64_t t0 = now_ns();
    const int ternary = mpfr_add(rop, a, b, rnd);
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

/* Build an MPFR from a binary string literal at the given prec. */
static inline void init_from_str_binary(mpfr_ptr x, const char *s,
                                        uint64_t prec) {
    mpfr_init2(x, (mpfr_prec_t)prec);
    mpfr_set_str(x, s, 2, MPFR_RNDN);
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

/* ------------------------------------------------------------------ */
/* main                                                               */
/* ------------------------------------------------------------------ */

int main(void) {
    FILE *out = stdout;

    const mpfr_rnd_t RNDS[5] = {MPFR_RNDN, MPFR_RNDZ, MPFR_RNDU, MPFR_RNDD, MPFR_RNDA};

    /* ============================================================== */
    /* happy: ~25 cases — typical adds at common precs, RNDN          */
    /* ============================================================== */
    {
        /* Small integer + integer at common precs. */
        emit_dd(out, "happy",  1.0,  53,  1.0,  53,  53, MPFR_RNDN); /* 2 */
        emit_dd(out, "happy",  1.0,  53,  2.0,  53,  53, MPFR_RNDN); /* 3 */
        emit_dd(out, "happy",  2.0,  53,  3.0,  53,  53, MPFR_RNDN); /* 5 */
        emit_dd(out, "happy",  10.0, 53,  20.0, 53,  53, MPFR_RNDN); /* 30 */
        emit_dd(out, "happy", 100.0, 53, 200.0, 53,  53, MPFR_RNDN); /* 300 */
        emit_dd(out, "happy",  1e9,  53,  2e9,  53,  53, MPFR_RNDN);

        /* Negative + positive (sign-preserving). */
        emit_dd(out, "happy", -1.0,  53,  2.0,  53,  53, MPFR_RNDN); /* 1 */
        emit_dd(out, "happy",  3.0,  53, -1.0,  53,  53, MPFR_RNDN); /* 2 */
        emit_dd(out, "happy", -42.0, 53, -8.0,  53,  53, MPFR_RNDN); /* -50 */

        /* Common simple fractions — exact representable. */
        emit_dd(out, "happy",  0.5,   53,  0.25,  53,  53, MPFR_RNDN); /* 0.75 */
        emit_dd(out, "happy",  0.125, 53,  0.125, 53,  53, MPFR_RNDN); /* 0.25 */
        emit_dd(out, "happy",  1.5,   53,  2.5,   53,  53, MPFR_RNDN); /* 4 */

        /* Non-dyadic — represented exactly at >=53 bits. */
        emit_dd(out, "happy",  3.14,  53,  2.86,  53,  53, MPFR_RNDN);
        emit_dd(out, "happy",  3.14,  53,  0.0,   53,  53, MPFR_RNDN); /* triv: x+0 */
        emit_dd(out, "happy",  0.0,   53,  3.14,  53,  53, MPFR_RNDN); /* triv: 0+x */
        emit_dd(out, "happy",  2.718281828459045, 53,
                              3.141592653589793, 53,  53, MPFR_RNDN);  /* e + pi */
        emit_dd(out, "happy",  1.4142135623730951, 53,
                              1.7320508075688772, 53,  53, MPFR_RNDN); /* sqrt(2) + sqrt(3) */

        /* Varying output prec. */
        emit_dd(out, "happy",  3.14, 53, 2.71, 53,  24, MPFR_RNDN);
        emit_dd(out, "happy",  3.14, 53, 2.71, 53,  64, MPFR_RNDN);
        emit_dd(out, "happy",  3.14, 53, 2.71, 53, 100, MPFR_RNDN);
        emit_dd(out, "happy",  3.14, 53, 2.71, 53, 200, MPFR_RNDN);

        /* Different input precs. */
        emit_dd(out, "happy",  1.0,  10, 1.0,  53,  53, MPFR_RNDN); /* 2 */
        emit_dd(out, "happy",  1.0,  53, 1.0, 100, 100, MPFR_RNDN);
        emit_dd(out, "happy",  0.5,  53, 0.5, 100, 200, MPFR_RNDN);

        /* Large + small (massive exponent gap; small contributes nothing
         * at narrow output prec). */
        emit_dd(out, "happy",  1.0e100, 53, 1.0e-100, 53,  53, MPFR_RNDN);

        /* Two scientific magnitudes. */
        emit_dd(out, "happy",  1.5e100, 53, 2.5e100, 53,  53, MPFR_RNDN);
    }

    /* ============================================================== */
    /* edge: ~50 cases — specials × rounding × cancellation             */
    /* ============================================================== */
    {
        /* (1-5) NaN + anything → NaN. All 5 rnd modes — none change the
         * result, but the broken port might react asymmetrically. */
        for (int i = 0; i < 5; ++i) {
            mpfr_t a, b; init_nan(a, 53); init_from_double(b, 1.0, 53);
            emit_case(out, "edge", a, b, 53, RNDS[i]);
            mpfr_clear(a); mpfr_clear(b);
        }
        /* (6-10) anything + NaN → NaN. */
        for (int i = 0; i < 5; ++i) {
            mpfr_t a, b; init_from_double(a, 1.0, 53); init_nan(b, 53);
            emit_case(out, "edge", a, b, 53, RNDS[i]);
            mpfr_clear(a); mpfr_clear(b);
        }
        /* (11) NaN + NaN → NaN. */
        {
            mpfr_t a, b; init_nan(a, 53); init_nan(b, 53);
            emit_case(out, "edge", a, b, 53, MPFR_RNDN);
            mpfr_clear(a); mpfr_clear(b);
        }

        /* (12) +Inf + +Inf → +Inf. */
        {
            mpfr_t a, b; init_pos_inf(a, 53); init_pos_inf(b, 53);
            emit_case(out, "edge", a, b, 53, MPFR_RNDN);
            mpfr_clear(a); mpfr_clear(b);
        }
        /* (13) -Inf + -Inf → -Inf. */
        {
            mpfr_t a, b; init_neg_inf(a, 53); init_neg_inf(b, 53);
            emit_case(out, "edge", a, b, 53, MPFR_RNDN);
            mpfr_clear(a); mpfr_clear(b);
        }
        /* (14) +Inf + -Inf → NaN. */
        {
            mpfr_t a, b; init_pos_inf(a, 53); init_neg_inf(b, 53);
            emit_case(out, "edge", a, b, 53, MPFR_RNDN);
            mpfr_clear(a); mpfr_clear(b);
        }
        /* (15) -Inf + +Inf → NaN. */
        {
            mpfr_t a, b; init_neg_inf(a, 53); init_pos_inf(b, 53);
            emit_case(out, "edge", a, b, 53, MPFR_RNDN);
            mpfr_clear(a); mpfr_clear(b);
        }
        /* (16) +Inf + finite → +Inf. */
        {
            mpfr_t a, b; init_pos_inf(a, 53); init_from_double(b, 3.14, 53);
            emit_case(out, "edge", a, b, 53, MPFR_RNDN);
            mpfr_clear(a); mpfr_clear(b);
        }
        /* (17) -Inf + finite → -Inf. */
        {
            mpfr_t a, b; init_neg_inf(a, 53); init_from_double(b, 3.14, 53);
            emit_case(out, "edge", a, b, 53, MPFR_RNDN);
            mpfr_clear(a); mpfr_clear(b);
        }
        /* (18) finite + +Inf → +Inf. */
        {
            mpfr_t a, b; init_from_double(a, -42.0, 53); init_pos_inf(b, 53);
            emit_case(out, "edge", a, b, 53, MPFR_RNDN);
            mpfr_clear(a); mpfr_clear(b);
        }
        /* (19) ±0 + +Inf → +Inf. */
        {
            mpfr_t a, b; init_neg_zero(a, 53); init_pos_inf(b, 53);
            emit_case(out, "edge", a, b, 53, MPFR_RNDN);
            mpfr_clear(a); mpfr_clear(b);
        }

        /* (20-24) +0 + +0 under each rnd → +0. */
        for (int i = 0; i < 5; ++i) {
            mpfr_t a, b; init_pos_zero(a, 53); init_pos_zero(b, 53);
            emit_case(out, "edge", a, b, 53, RNDS[i]);
            mpfr_clear(a); mpfr_clear(b);
        }
        /* (25-29) -0 + -0 under each rnd → -0. */
        for (int i = 0; i < 5; ++i) {
            mpfr_t a, b; init_neg_zero(a, 53); init_neg_zero(b, 53);
            emit_case(out, "edge", a, b, 53, RNDS[i]);
            mpfr_clear(a); mpfr_clear(b);
        }
        /* (30-34) +0 + -0: RNDD → -0, else +0. */
        for (int i = 0; i < 5; ++i) {
            mpfr_t a, b; init_pos_zero(a, 53); init_neg_zero(b, 53);
            emit_case(out, "edge", a, b, 53, RNDS[i]);
            mpfr_clear(a); mpfr_clear(b);
        }
        /* (35-39) -0 + +0: RNDD → -0, else +0. */
        for (int i = 0; i < 5; ++i) {
            mpfr_t a, b; init_neg_zero(a, 53); init_pos_zero(b, 53);
            emit_case(out, "edge", a, b, 53, RNDS[i]);
            mpfr_clear(a); mpfr_clear(b);
        }

        /* (40) ±0 + normal → normal at result prec. */
        {
            mpfr_t a, b; init_pos_zero(a, 53); init_from_double(b, 3.14, 53);
            emit_case(out, "edge", a, b, 53, MPFR_RNDN);
            mpfr_clear(a); mpfr_clear(b);
        }
        /* (41) -0 + normal → normal (sign from normal). */
        {
            mpfr_t a, b; init_neg_zero(a, 53); init_from_double(b, -3.14, 53);
            emit_case(out, "edge", a, b, 53, MPFR_RNDN);
            mpfr_clear(a); mpfr_clear(b);
        }
        /* (42) ±0 + normal at different prec — rounds normal to prec. */
        {
            mpfr_t a, b; init_pos_zero(a, 53); init_from_double(b, 3.14, 53);
            emit_case(out, "edge", a, b, 10, MPFR_RNDN);
            mpfr_clear(a); mpfr_clear(b);
        }

        /* (43-47) Catastrophic cancellation: 1.0 + -1.0 at each rnd → +0
         * (-0 for RNDD). Both ternary 0. */
        for (int i = 0; i < 5; ++i) {
            mpfr_t a, b;
            init_from_double(a,  1.0, 53);
            init_from_double(b, -1.0, 53);
            emit_case(out, "edge", a, b, 53, RNDS[i]);
            mpfr_clear(a); mpfr_clear(b);
        }

        /* (48) Near-cancellation: 1.0 + -(1.0 - 2^-50) at prec=53. The
         *      result is +2^-50 exactly (small value, needs renormalization).
         */
        {
            mpfr_t a, b;
            mpfr_init2(a, 53); mpfr_init2(b, 53);
            mpfr_set_d(a, 1.0, MPFR_RNDN);
            /* b = -(1 - 2^-50). Construct via mpfr_set_str. */
            mpfr_set_str(b, "-0.11111111111111111111111111111111111111111111111111", 2, MPFR_RNDN);
            emit_case(out, "edge", a, b, 53, MPFR_RNDN);
            mpfr_clear(a); mpfr_clear(b);
        }

        /* (49) Boundary carry: (1 - 2^-53) + (1 - 2^-53) = 2 - 2^-52,
         *      at prec=53 with RNDN ties-to-even rounds to 2 (carry into
         *      next bit), ternary +1. */
        {
            mpfr_t a, b;
            mpfr_init2(a, 53); mpfr_init2(b, 53);
            /* 1 - 2^-53 ≈ 0.9999...; the largest double less than 1. */
            mpfr_set_str(a, "0.11111111111111111111111111111111111111111111111111111", 2, MPFR_RNDN);
            mpfr_set_str(b, "0.11111111111111111111111111111111111111111111111111111", 2, MPFR_RNDN);
            emit_case(out, "edge", a, b, 53, MPFR_RNDU);  /* must round up to 2 */
            mpfr_clear(a); mpfr_clear(b);
        }

        /* (50) Result requires shift: 0.5 + 0.5 = 1.0 (exp grows). */
        {
            mpfr_t a, b;
            init_from_double(a, 0.5, 53);
            init_from_double(b, 0.5, 53);
            emit_case(out, "edge", a, b, 53, MPFR_RNDN);
            mpfr_clear(a); mpfr_clear(b);
        }

        /* (51) Narrow output prec from wider inputs: 1/3 + 2/3 — neither
         *      exact at any binary prec; sum at 200 bits is ≈ 1.0 exactly
         *      (after rounding) since the bit patterns are designed to
         *      cancel; we test the round to narrow. Use 0.1 + 0.2 instead
         *      (classic IEEE 754 surprise: 0.1 + 0.2 ≠ 0.3 at prec=53). */
        {
            mpfr_t a, b;
            init_from_double(a, 0.1, 53);
            init_from_double(b, 0.2, 53);
            emit_case(out, "edge", a, b, 53, MPFR_RNDN);
            mpfr_clear(a); mpfr_clear(b);
        }

        /* (52) High-magnitude same-sign add at narrow prec — exercises
         *      shift / round. */
        {
            mpfr_t a, b;
            init_from_double(a, 1.0e100, 53);
            init_from_double(b, 1.0e100, 53);
            emit_case(out, "edge", a, b, 4, MPFR_RNDN);
            mpfr_clear(a); mpfr_clear(b);
        }

        /* (53) Subnormal-region-ish small + small: 2^-1000 + 2^-1000. */
        {
            mpfr_t a, b;
            mpfr_init2(a, 53); mpfr_init2(b, 53);
            mpfr_set_str(a, "1E-1000", 2, MPFR_RNDN);
            mpfr_set_str(b, "1E-1000", 2, MPFR_RNDN);
            emit_case(out, "edge", a, b, 53, MPFR_RNDN);
            mpfr_clear(a); mpfr_clear(b);
        }

        /* (54-58) Different precs in a and b (5 different combinations). */
        emit_dd(out, "edge", 3.14,  53, 2.71,  64,  64, MPFR_RNDN);
        emit_dd(out, "edge", 3.14,  64, 2.71, 100, 100, MPFR_RNDN);
        emit_dd(out, "edge", 3.14,   2, 2.71,  53,  53, MPFR_RNDN);
        emit_dd(out, "edge", 3.14, 200, 2.71,  53, 100, MPFR_RNDN);
        emit_dd(out, "edge", 3.14, 100, 2.71, 100, 200, MPFR_RNDN);

        /* (59) Output prec=1 — extreme rounding. */
        emit_dd(out, "edge", 1.0, 53, 1.0, 53, 1, MPFR_RNDN); /* 2 */
        emit_dd(out, "edge", 1.0, 53, 0.5, 53, 1, MPFR_RNDN); /* 1.5 → 2 (ties-to-even) */

        /* (60) Output prec=1 with cancellation. */
        emit_dd(out, "edge", 1.5, 53, -0.5, 53, 1, MPFR_RNDN); /* 1 */
    }

    /* ============================================================== */
    /* adversarial: ~20 cases — ternary direction + cross-mode disagree */
    /* ============================================================== */
    {
        /* (1-5) 1.0 + 2^-100 at prec=53 — the small part rounds off but
         *      its presence affects the ternary direction.
         *        RNDN: round to 1.0, ternary -1 (rounded < exact)
         *        RNDZ: 1.0, ternary -1
         *        RNDU: next-up double, ternary +1
         *        RNDD: 1.0, ternary -1
         *        RNDA: next-up double, ternary +1
         */
        for (int i = 0; i < 5; ++i) {
            mpfr_t a, b;
            mpfr_init2(a, 53); mpfr_init2(b, 53);
            mpfr_set_d(a, 1.0, MPFR_RNDN);
            mpfr_set_str(b, "1E-100", 2, MPFR_RNDN);
            emit_case(out, "adversarial", a, b, 53, RNDS[i]);
            mpfr_clear(a); mpfr_clear(b);
        }

        /* (6-10) Negative variant: -1.0 + -2^-100. The directional
         *      rounding swaps sign sensitivity.
         */
        for (int i = 0; i < 5; ++i) {
            mpfr_t a, b;
            mpfr_init2(a, 53); mpfr_init2(b, 53);
            mpfr_set_d(a, -1.0, MPFR_RNDN);
            mpfr_set_str(b, "-1E-100", 2, MPFR_RNDN);
            emit_case(out, "adversarial", a, b, 53, RNDS[i]);
            mpfr_clear(a); mpfr_clear(b);
        }

        /* (11-12) Half-ulp ties at prec=2: 1 + 0.5 = 1.5 (binary 1.1).
         *      At prec=2 with RNDN ties-to-even → 2 (rounded up, ternary
         *      +1). At RNDZ → 1 (ternary -1).
         */
        {
            mpfr_t a, b;
            init_from_double(a, 1.0, 53); init_from_double(b, 0.5, 53);
            emit_case(out, "adversarial", a, b, 2, MPFR_RNDN);
            mpfr_clear(a); mpfr_clear(b);
        }
        {
            mpfr_t a, b;
            init_from_double(a, 1.0, 53); init_from_double(b, 0.5, 53);
            emit_case(out, "adversarial", a, b, 2, MPFR_RNDZ);
            mpfr_clear(a); mpfr_clear(b);
        }

        /* (13) Carry-out at MSB: (2^53 - 1) + 1 at prec=53. The exact
         *      sum is 2^53 (= 0b1...0), which fits in 54 bits but at
         *      prec=53 the mantissa MSB sits one place higher (exp
         *      increments). This is the "incremented == 2^outPrec"
         *      carry branch in roundMantissa.
         */
        {
            mpfr_t a, b;
            mpfr_init2(a, 53); mpfr_init2(b, 53);
            mpfr_set_str(a, "1.1111111111111111111111111111111111111111111111111111E52", 2, MPFR_RNDN);
            mpfr_set_d(b, 1.0, MPFR_RNDN);
            emit_case(out, "adversarial", a, b, 53, MPFR_RNDN);
            mpfr_clear(a); mpfr_clear(b);
        }

        /* (14) Catastrophic cancellation at high prec: 1.0 - (1.0 -
         *      2^-200). At prec=53 the result requires a 200-bit
         *      intermediate then renormalises to 2^-200. Exercises the
         *      bitLength / strip-leading-zeros path on cancellation.
         */
        {
            mpfr_t a, b;
            mpfr_init2(a, 200); mpfr_init2(b, 200);
            mpfr_set_d(a, 1.0, MPFR_RNDN);
            /* b = -(1 - 2^-200). */
            mpfr_set_d(b, 1.0, MPFR_RNDN);
            mpfr_t eps; mpfr_init2(eps, 200);
            mpfr_set_ui_2exp(eps, 1, -200, MPFR_RNDN);
            mpfr_sub(b, b, eps, MPFR_RNDN);
            mpfr_neg(b, b, MPFR_RNDN);
            mpfr_clear(eps);
            emit_case(out, "adversarial", a, b, 53, MPFR_RNDN);
            mpfr_clear(a); mpfr_clear(b);
        }

        /* (15-19) Wildly disparate exponents: 1.0 + 2^-1000 at prec=53.
         *      For each rounding mode, the small operand should affect
         *      the ternary direction but not the value. */
        for (int i = 0; i < 5; ++i) {
            mpfr_t a, b;
            mpfr_init2(a, 53); mpfr_init2(b, 53);
            mpfr_set_d(a, 1.0, MPFR_RNDN);
            mpfr_set_str(b, "1E-1000", 2, MPFR_RNDN);
            emit_case(out, "adversarial", a, b, 53, RNDS[i]);
            mpfr_clear(a); mpfr_clear(b);
        }

        /* (20) Carry-out at high prec with cancellation interaction:
         *      (1 - 2^-100) + 2^-99 at prec=100. Exercises both shift
         *      and round.
         */
        {
            mpfr_t a, b;
            mpfr_init2(a, 100); mpfr_init2(b, 100);
            mpfr_set_d(a, 1.0, MPFR_RNDN);
            mpfr_t eps; mpfr_init2(eps, 100);
            mpfr_set_ui_2exp(eps, 1, -100, MPFR_RNDN);
            mpfr_sub(a, a, eps, MPFR_RNDN);
            mpfr_set_ui_2exp(b, 1, -99, MPFR_RNDN);
            mpfr_clear(eps);
            emit_case(out, "adversarial", a, b, 100, MPFR_RNDN);
            mpfr_clear(a); mpfr_clear(b);
        }
    }

    /* ============================================================== */
    /* fuzz: 100 cases — PRNG, random doubles × random precs × rnd     */
    /* ============================================================== */
    {
        xs64_t rng;
        xs64_seed(&rng, 0xADDADDADDADDADDULL);
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

            mpfr_t a, b;
            init_from_double(a, da, pa);
            init_from_double(b, db, pb);
            emit_case(out, "fuzz", a, b, prec, rnd);
            mpfr_clear(a); mpfr_clear(b);
            emitted++;
        }
    }

    /* ============================================================== */
    /* mined: 5 cases — transcribed from mpfr/tests/tadd.c             */
    /* ============================================================== */
    {
        /* tadd.c L946: check53("1.22191250737771397120e+20",
         *   "948002822.0", MPFR_RNDN, "122191250738719408128.0"). */
        {
            mpfr_t a, b;
            mpfr_init2(a, 53); mpfr_init2(b, 53);
            mpfr_set_str(a, "1.22191250737771397120e+20", 10, MPFR_RNDN);
            mpfr_set_str(b, "948002822.0",                10, MPFR_RNDN);
            emit_case(out, "mined", a, b, 53, MPFR_RNDN);
            mpfr_clear(a); mpfr_clear(b);
        }

        /* tadd.c L948: check53("9966027674114492.0",
         *   "1780341389094537.0", MPFR_RNDN, ...). */
        {
            mpfr_t a, b;
            mpfr_init2(a, 53); mpfr_init2(b, 53);
            mpfr_set_str(a, "9966027674114492.0",  10, MPFR_RNDN);
            mpfr_set_str(b, "1780341389094537.0",  10, MPFR_RNDN);
            emit_case(out, "mined", a, b, 53, MPFR_RNDN);
            mpfr_clear(a); mpfr_clear(b);
        }

        /* tadd.c L953: check53("6.14384195492641560499e-02",
         *   "-6.14384195401037683237e-02", MPFR_RNDU, ...) — near-
         *   cancellation. */
        {
            mpfr_t a, b;
            mpfr_init2(a, 53); mpfr_init2(b, 53);
            mpfr_set_str(a, "6.14384195492641560499e-02",  10, MPFR_RNDN);
            mpfr_set_str(b, "-6.14384195401037683237e-02", 10, MPFR_RNDN);
            emit_case(out, "mined", a, b, 53, MPFR_RNDU);
            mpfr_clear(a); mpfr_clear(b);
        }

        /* tadd.c L960: check53("5.43885304644369509058e+185",
         *   "-1.87427265794105342763e-57", MPFR_RNDN, ...) — huge +
         *   tiny same-sign-ish. */
        {
            mpfr_t a, b;
            mpfr_init2(a, 53); mpfr_init2(b, 53);
            mpfr_set_str(a, "5.43885304644369509058e+185",  10, MPFR_RNDN);
            mpfr_set_str(b, "-1.87427265794105342763e-57",  10, MPFR_RNDN);
            emit_case(out, "mined", a, b, 53, MPFR_RNDN);
            mpfr_clear(a); mpfr_clear(b);
        }

        /* tadd.c L948 variant under RNDD (covers another rounding mode). */
        {
            mpfr_t a, b;
            mpfr_init2(a, 53); mpfr_init2(b, 53);
            mpfr_set_str(a, "9966027674114492.0",  10, MPFR_RNDN);
            mpfr_set_str(b, "1780341389094537.0",  10, MPFR_RNDN);
            emit_case(out, "mined", a, b, 53, MPFR_RNDD);
            mpfr_clear(a); mpfr_clear(b);
        }
    }

    return 0;
}
