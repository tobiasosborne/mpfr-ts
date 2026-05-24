/*
 * golden_driver.c — Golden master for MPFR's mpfr_sub.
 *
 * C signature
 * -----------
 *
 *   int mpfr_sub(mpfr_t rop, mpfr_srcptr op1, mpfr_srcptr op2, mpfr_rnd_t rnd);
 *
 *   Sets rop to op1 - op2 rounded per rnd at rop's precision. Returns
 *   the ternary (sign of rounded - exact). See mpfr/src/sub.c.
 *
 * Divergence from C → TS
 * ----------------------
 *
 * TS port `mpfr_sub(a, b, prec, rnd) -> Result` takes prec as a
 * positional argument and returns {value, ternary}. The C function
 * mutates rop. To grade we run libmpfr's mpfr_sub at the target prec
 * and emit a Result-shaped record via jl_output_result.
 *
 * Wire format mirrors mpfr_add exactly — same shape, different
 * operation.
 *
 *   {"tag":"<class>",
 *    "inputs":{"a":<MPFR>,"b":<MPFR>,"prec":"<decimal>","rnd":"RND[NZUDA]"},
 *    "output":{"value":<MPFR>,"ternary":<-1|0|1>},
 *    "time_ns":<n>}
 *
 * Tag distribution (Rule 7 minimums)
 * ----------------------------------
 *
 *   happy        :  ~25   (typical subs at common precs, RNDN)
 *   edge         :  ~60   (specials × all 5 rnd modes; signed-zero rules;
 *                          a - a → ±0 cancellation; +Inf - +Inf → NaN)
 *   adversarial  :  ~25   (ternary direction; massive cancellation;
 *                          a - b where b is tiny; rnd inversion edge)
 *   fuzz         :  100   (PRNG; random doubles × random precs × rnd)
 *   mined        :    6   (from mpfr/tests/tsub.c)
 *   ------------ ----
 *   total        : ~216
 *
 * Sub-specific edges we exercise that mpfr_add's golden doesn't:
 *   - a - a → ±0 (cancellation by structural identity)
 *   - a - 0 → a (identity)
 *   - 0 - a → -a (sign flip)
 *   - +Inf - +Inf → NaN
 *   - +Inf - -Inf → +Inf
 *   - (+0) - (+0) under RNDD → -0 (rnd-aware zero sign)
 *   - (+0) - (-0) under RNDD → +0
 *
 * Build via eval/golden_master/build.sh.
 *
 * Ref: mpfr/src/sub.c — the C reference.
 * Ref: src/ops/sub.ts — the production port (composes over mpfr_add).
 * Ref: mpfr/tests/tsub.c — source for the mined cases.
 */
#include "common.h"

#include <assert.h>
#include <float.h>
#include <inttypes.h>
#include <math.h>
#include <string.h>

#if GMP_NUMB_BITS != 64
#  error "mpfr_sub golden_driver requires GMP_NUMB_BITS == 64"
#endif

#define TS_PREC_MAX ((uint64_t)((1ULL << 31) - 257ULL))
#define TS_PREC_MIN ((uint64_t)1)

/* Emit one mpfr_sub golden case. */
static inline void emit_case(FILE *out, const char *tag,
                             mpfr_srcptr a, mpfr_srcptr b,
                             uint64_t prec, mpfr_rnd_t rnd) {
    assert(prec >= TS_PREC_MIN && prec <= TS_PREC_MAX);
    mpfr_t rop;
    mpfr_init2(rop, (mpfr_prec_t)prec);

    const uint64_t t0 = now_ns();
    const int ternary = mpfr_sub(rop, a, b, rnd);
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

/* Helpers. */
static inline void init_from_double(mpfr_ptr x, double d, uint64_t prec) {
    mpfr_init2(x, (mpfr_prec_t)prec); mpfr_set_d(x, d, MPFR_RNDN);
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

int main(void) {
    FILE *out = stdout;
    const mpfr_rnd_t RNDS[5] = {MPFR_RNDN, MPFR_RNDZ, MPFR_RNDU, MPFR_RNDD, MPFR_RNDA};

    /* happy: 25 cases — typical subs. */
    {
        emit_dd(out, "happy",  3.0,  53,  1.0,  53,  53, MPFR_RNDN); /* 2 */
        emit_dd(out, "happy",  5.0,  53,  2.0,  53,  53, MPFR_RNDN); /* 3 */
        emit_dd(out, "happy", 10.0,  53,  3.0,  53,  53, MPFR_RNDN); /* 7 */
        emit_dd(out, "happy", 100.0, 53,  1.0,  53,  53, MPFR_RNDN); /* 99 */
        emit_dd(out, "happy",  1e9,  53,  2e9,  53,  53, MPFR_RNDN); /* -1e9 */
        emit_dd(out, "happy", -1.0,  53,  2.0,  53,  53, MPFR_RNDN); /* -3 */
        emit_dd(out, "happy",  1.0,  53, -2.0,  53,  53, MPFR_RNDN); /* 3 */
        emit_dd(out, "happy", -42.0, 53, -8.0,  53,  53, MPFR_RNDN); /* -34 */
        emit_dd(out, "happy",  0.5,   53,  0.25,  53,  53, MPFR_RNDN); /* 0.25 */
        emit_dd(out, "happy",  3.14,  53,  2.86,  53,  53, MPFR_RNDN);
        emit_dd(out, "happy",  3.14,  53,  0.0,   53,  53, MPFR_RNDN); /* x - 0 = x */
        emit_dd(out, "happy",  0.0,   53,  3.14,  53,  53, MPFR_RNDN); /* 0 - x = -x */
        emit_dd(out, "happy",  3.141592653589793, 53,
                              2.718281828459045, 53,  53, MPFR_RNDN); /* pi - e */
        emit_dd(out, "happy",  1.7320508075688772, 53,
                              1.4142135623730951, 53,  53, MPFR_RNDN); /* sqrt(3) - sqrt(2) */
        emit_dd(out, "happy",  3.14, 53, 2.71, 53,  24, MPFR_RNDN);
        emit_dd(out, "happy",  3.14, 53, 2.71, 53,  64, MPFR_RNDN);
        emit_dd(out, "happy",  3.14, 53, 2.71, 53, 100, MPFR_RNDN);
        emit_dd(out, "happy",  3.14, 53, 2.71, 53, 200, MPFR_RNDN);
        emit_dd(out, "happy",  1.0,  10, 1.0,  53,  53, MPFR_RNDN); /* 0 — cancellation */
        emit_dd(out, "happy",  2.0,  53, 1.0, 100, 100, MPFR_RNDN);
        emit_dd(out, "happy",  0.5,  53, 0.5, 100, 200, MPFR_RNDN); /* 0 */
        emit_dd(out, "happy",  1.0e100, 53, 1.0e-100, 53,  53, MPFR_RNDN);
        emit_dd(out, "happy",  2.5e100, 53, 1.5e100,  53,  53, MPFR_RNDN);
        emit_dd(out, "happy",  6.022e23, 53, 1e20,   53,  53, MPFR_RNDN);
        emit_dd(out, "happy",  3.0,  53,  3.0,  53,  53, MPFR_RNDN); /* 0 */
    }

    /* edge: 60 cases — specials × rnd × cancellation × sub-specific. */
    {
        /* (1-5) NaN - anything → NaN; all 5 rnd. */
        for (int i = 0; i < 5; ++i) {
            mpfr_t a, b; init_nan(a, 53); init_from_double(b, 1.0, 53);
            emit_case(out, "edge", a, b, 53, RNDS[i]);
            mpfr_clear(a); mpfr_clear(b);
        }
        /* (6-10) anything - NaN → NaN. */
        for (int i = 0; i < 5; ++i) {
            mpfr_t a, b; init_from_double(a, 1.0, 53); init_nan(b, 53);
            emit_case(out, "edge", a, b, 53, RNDS[i]);
            mpfr_clear(a); mpfr_clear(b);
        }
        /* (11) NaN - NaN → NaN. */
        {
            mpfr_t a, b; init_nan(a, 53); init_nan(b, 53);
            emit_case(out, "edge", a, b, 53, MPFR_RNDN);
            mpfr_clear(a); mpfr_clear(b);
        }

        /* (12) +Inf - +Inf → NaN. (Sub-specific: differs from add.) */
        {
            mpfr_t a, b; init_pos_inf(a, 53); init_pos_inf(b, 53);
            emit_case(out, "edge", a, b, 53, MPFR_RNDN);
            mpfr_clear(a); mpfr_clear(b);
        }
        /* (13) -Inf - -Inf → NaN. */
        {
            mpfr_t a, b; init_neg_inf(a, 53); init_neg_inf(b, 53);
            emit_case(out, "edge", a, b, 53, MPFR_RNDN);
            mpfr_clear(a); mpfr_clear(b);
        }
        /* (14) +Inf - -Inf → +Inf. */
        {
            mpfr_t a, b; init_pos_inf(a, 53); init_neg_inf(b, 53);
            emit_case(out, "edge", a, b, 53, MPFR_RNDN);
            mpfr_clear(a); mpfr_clear(b);
        }
        /* (15) -Inf - +Inf → -Inf. */
        {
            mpfr_t a, b; init_neg_inf(a, 53); init_pos_inf(b, 53);
            emit_case(out, "edge", a, b, 53, MPFR_RNDN);
            mpfr_clear(a); mpfr_clear(b);
        }
        /* (16) +Inf - finite → +Inf. */
        {
            mpfr_t a, b; init_pos_inf(a, 53); init_from_double(b, 3.14, 53);
            emit_case(out, "edge", a, b, 53, MPFR_RNDN);
            mpfr_clear(a); mpfr_clear(b);
        }
        /* (17) -Inf - finite → -Inf. */
        {
            mpfr_t a, b; init_neg_inf(a, 53); init_from_double(b, 3.14, 53);
            emit_case(out, "edge", a, b, 53, MPFR_RNDN);
            mpfr_clear(a); mpfr_clear(b);
        }
        /* (18) finite - +Inf → -Inf. */
        {
            mpfr_t a, b; init_from_double(a, -42.0, 53); init_pos_inf(b, 53);
            emit_case(out, "edge", a, b, 53, MPFR_RNDN);
            mpfr_clear(a); mpfr_clear(b);
        }
        /* (19) finite - -Inf → +Inf. */
        {
            mpfr_t a, b; init_from_double(a, -42.0, 53); init_neg_inf(b, 53);
            emit_case(out, "edge", a, b, 53, MPFR_RNDN);
            mpfr_clear(a); mpfr_clear(b);
        }

        /* (20-24) +0 - +0 under each rnd. RNDD → -0; else +0.
         *
         * From mpfr/src/sub.c L60–L69:
         *   rnd_mode != RNDD: sign = (b.neg && c.pos) ? -1 : 1
         *   rnd_mode == RNDD: sign = (b.pos && c.neg) ? 1 : -1
         * For (+0) - (+0): b=+0, c=+0.
         *   non-RNDD: b.neg=0, c.pos=1 → cond=false → sign=+1 → +0.
         *   RNDD:      b.pos=1, c.neg=0 → cond=false → sign=-1 → -0.
         */
        for (int i = 0; i < 5; ++i) {
            mpfr_t a, b; init_pos_zero(a, 53); init_pos_zero(b, 53);
            emit_case(out, "edge", a, b, 53, RNDS[i]);
            mpfr_clear(a); mpfr_clear(b);
        }
        /* (25-29) -0 - -0 under each rnd. Symmetric — same as (+0)-(+0). */
        for (int i = 0; i < 5; ++i) {
            mpfr_t a, b; init_neg_zero(a, 53); init_neg_zero(b, 53);
            emit_case(out, "edge", a, b, 53, RNDS[i]);
            mpfr_clear(a); mpfr_clear(b);
        }
        /* (30-34) +0 - -0 under each rnd.
         * For (+0) - (-0): b=+0, c=-0.
         *   non-RNDD: b.neg=0, c.pos=0 → cond=false → sign=+1 → +0.
         *   RNDD:      b.pos=1, c.neg=1 → cond=true  → sign=+1 → +0.
         * So always +0 (sub.c special rule). */
        for (int i = 0; i < 5; ++i) {
            mpfr_t a, b; init_pos_zero(a, 53); init_neg_zero(b, 53);
            emit_case(out, "edge", a, b, 53, RNDS[i]);
            mpfr_clear(a); mpfr_clear(b);
        }
        /* (35-39) -0 - +0 under each rnd.
         * For (-0) - (+0): b=-0, c=+0.
         *   non-RNDD: b.neg=1, c.pos=1 → cond=true  → sign=-1 → -0.
         *   RNDD:      b.pos=0, c.neg=0 → cond=false → sign=-1 → -0.
         * So always -0. */
        for (int i = 0; i < 5; ++i) {
            mpfr_t a, b; init_neg_zero(a, 53); init_pos_zero(b, 53);
            emit_case(out, "edge", a, b, 53, RNDS[i]);
            mpfr_clear(a); mpfr_clear(b);
        }

        /* (40) ±0 - normal → -normal. */
        {
            mpfr_t a, b; init_pos_zero(a, 53); init_from_double(b, 3.14, 53);
            emit_case(out, "edge", a, b, 53, MPFR_RNDN);
            mpfr_clear(a); mpfr_clear(b);
        }
        /* (41) normal - ±0 → normal. */
        {
            mpfr_t a, b; init_from_double(a, 3.14, 53); init_pos_zero(b, 53);
            emit_case(out, "edge", a, b, 53, MPFR_RNDN);
            mpfr_clear(a); mpfr_clear(b);
        }
        /* (42) normal - -0 → normal. */
        {
            mpfr_t a, b; init_from_double(a, 3.14, 53); init_neg_zero(b, 53);
            emit_case(out, "edge", a, b, 53, MPFR_RNDN);
            mpfr_clear(a); mpfr_clear(b);
        }
        /* (43) ±0 - normal at different prec — rounds normal. */
        {
            mpfr_t a, b; init_pos_zero(a, 53); init_from_double(b, 3.14, 53);
            emit_case(out, "edge", a, b, 10, MPFR_RNDN);
            mpfr_clear(a); mpfr_clear(b);
        }

        /* (44-48) Catastrophic cancellation: 1.0 - 1.0 at each rnd → ±0. */
        for (int i = 0; i < 5; ++i) {
            mpfr_t a, b;
            init_from_double(a, 1.0, 53);
            init_from_double(b, 1.0, 53);
            emit_case(out, "edge", a, b, 53, RNDS[i]);
            mpfr_clear(a); mpfr_clear(b);
        }

        /* (49) Near-cancellation: 1.0 - (1.0 - 2^-50) = 2^-50 exactly. */
        {
            mpfr_t a, b;
            mpfr_init2(a, 53); mpfr_init2(b, 53);
            mpfr_set_d(a, 1.0, MPFR_RNDN);
            mpfr_set_str(b, "0.11111111111111111111111111111111111111111111111111", 2, MPFR_RNDN);
            emit_case(out, "edge", a, b, 53, MPFR_RNDN);
            mpfr_clear(a); mpfr_clear(b);
        }

        /* (50) Boundary carry: (1 + 2^-53) - (-1 + 2^-53) — but
         *      no carry on sub; use 0.5 - (-0.5) = 1.0 instead. */
        {
            mpfr_t a, b;
            init_from_double(a, 0.5, 53);
            init_from_double(b, -0.5, 53);
            emit_case(out, "edge", a, b, 53, MPFR_RNDN);
            mpfr_clear(a); mpfr_clear(b);
        }

        /* (51) Different precs in a and b. */
        emit_dd(out, "edge", 3.14,  53, 2.71,  64,  64, MPFR_RNDN);
        emit_dd(out, "edge", 3.14,  64, 2.71, 100, 100, MPFR_RNDN);
        emit_dd(out, "edge", 3.14,   2, 2.71,  53,  53, MPFR_RNDN);
        emit_dd(out, "edge", 3.14, 200, 2.71,  53, 100, MPFR_RNDN);
        emit_dd(out, "edge", 3.14, 100, 2.71, 100, 200, MPFR_RNDN);

        /* (56-58) Output prec=1 — extreme rounding. */
        emit_dd(out, "edge", 1.0, 53, 0.0, 53, 1, MPFR_RNDN); /* 1 */
        emit_dd(out, "edge", 2.0, 53, 1.0, 53, 1, MPFR_RNDN); /* 1 */
        emit_dd(out, "edge", 1.5, 53, 0.5, 53, 1, MPFR_RNDN); /* 1 */

        /* (59) Subnormal-region small - small: 2^-1000 - 2^-1000 = 0. */
        {
            mpfr_t a, b;
            mpfr_init2(a, 53); mpfr_init2(b, 53);
            mpfr_set_str(a, "1E-1000", 2, MPFR_RNDN);
            mpfr_set_str(b, "1E-1000", 2, MPFR_RNDN);
            emit_case(out, "edge", a, b, 53, MPFR_RNDN);
            mpfr_clear(a); mpfr_clear(b);
        }

        /* (60) a - a where a is a fresh constructed value (aliasing-
         *      structural-identity test). */
        {
            mpfr_t a, b;
            init_from_double(a, 3.14, 53);
            init_from_double(b, 3.14, 53);
            emit_case(out, "edge", a, b, 53, MPFR_RNDD); /* RNDD → -0 */
            mpfr_clear(a); mpfr_clear(b);
        }
    }

    /* adversarial: 25 cases. */
    {
        /* (1-5) 1.0 - 2^-100 at prec=53 — small subtrahend rounds off
         *       but its presence affects ternary direction.
         *       At RNDN/RNDZ/RNDD: round to nextDown(1.0) — wait, sub
         *       moves toward zero. 1.0 - epsilon where epsilon is below
         *       the LSB: depending on mode rounds to 1.0 or nextDown(1.0).
         */
        for (int i = 0; i < 5; ++i) {
            mpfr_t a, b;
            mpfr_init2(a, 53); mpfr_init2(b, 53);
            mpfr_set_d(a, 1.0, MPFR_RNDN);
            mpfr_set_str(b, "1E-100", 2, MPFR_RNDN);
            emit_case(out, "adversarial", a, b, 53, RNDS[i]);
            mpfr_clear(a); mpfr_clear(b);
        }

        /* (6-10) -1.0 - 2^-100 — directional sensitivity inverts. */
        for (int i = 0; i < 5; ++i) {
            mpfr_t a, b;
            mpfr_init2(a, 53); mpfr_init2(b, 53);
            mpfr_set_d(a, -1.0, MPFR_RNDN);
            mpfr_set_str(b, "1E-100", 2, MPFR_RNDN);
            emit_case(out, "adversarial", a, b, 53, RNDS[i]);
            mpfr_clear(a); mpfr_clear(b);
        }

        /* (11-12) Half-ulp tie at prec=2: 1 - (-0.5) = 1.5. RNDN ties-
         *         to-even → 2 (LSB 0). RNDZ → 1 (toward zero). */
        {
            mpfr_t a, b;
            init_from_double(a, 1.0, 53); init_from_double(b, -0.5, 53);
            emit_case(out, "adversarial", a, b, 2, MPFR_RNDN);
            mpfr_clear(a); mpfr_clear(b);
        }
        {
            mpfr_t a, b;
            init_from_double(a, 1.0, 53); init_from_double(b, -0.5, 53);
            emit_case(out, "adversarial", a, b, 2, MPFR_RNDZ);
            mpfr_clear(a); mpfr_clear(b);
        }

        /* (13) Catastrophic cancellation at high prec: 1.0 - (1.0 - 2^-200).
         *      Result is 2^-200 exactly. Exercises bitLength / strip-leading-
         *      zeros path. */
        {
            mpfr_t a, b;
            mpfr_init2(a, 200); mpfr_init2(b, 200);
            mpfr_set_d(a, 1.0, MPFR_RNDN);
            mpfr_set_d(b, 1.0, MPFR_RNDN);
            mpfr_t eps; mpfr_init2(eps, 200);
            mpfr_set_ui_2exp(eps, 1, -200, MPFR_RNDN);
            mpfr_sub(b, b, eps, MPFR_RNDN);
            mpfr_clear(eps);
            emit_case(out, "adversarial", a, b, 53, MPFR_RNDN);
            mpfr_clear(a); mpfr_clear(b);
        }

        /* (14-18) Wildly disparate exponents: 1.0 - 2^-1000 at prec=53. */
        for (int i = 0; i < 5; ++i) {
            mpfr_t a, b;
            mpfr_init2(a, 53); mpfr_init2(b, 53);
            mpfr_set_d(a, 1.0, MPFR_RNDN);
            mpfr_set_str(b, "1E-1000", 2, MPFR_RNDN);
            emit_case(out, "adversarial", a, b, 53, RNDS[i]);
            mpfr_clear(a); mpfr_clear(b);
        }

        /* (19) Sign-flip via 0 - x at prec mismatch. */
        {
            mpfr_t a, b;
            init_pos_zero(a, 53);
            init_from_double(b, 3.14, 53);
            emit_case(out, "adversarial", a, b, 10, MPFR_RNDU);
            mpfr_clear(a); mpfr_clear(b);
        }

        /* (20) Carry-out at MSB on cancellation: a where a is just-
         *      below-2 minus a tiny positive, getting close to 2 from
         *      below; RNDU should push to 2 exactly. */
        {
            mpfr_t a, b;
            mpfr_init2(a, 53); mpfr_init2(b, 53);
            mpfr_set_str(a, "1.1111111111111111111111111111111111111111111111111111E0", 2, MPFR_RNDN);
            mpfr_set_str(b, "-1E-100", 2, MPFR_RNDN);
            emit_case(out, "adversarial", a, b, 53, MPFR_RNDU);
            mpfr_clear(a); mpfr_clear(b);
        }

        /* (21-22) RNDU/RNDD asymmetry on positive minus small positive.
         *         x = 1.0, y = 2^-60. Result is just-below-1.0 by an
         *         amount that rounds at prec=53. */
        {
            mpfr_t a, b;
            mpfr_init2(a, 53); mpfr_init2(b, 53);
            mpfr_set_d(a, 1.0, MPFR_RNDN);
            mpfr_set_str(b, "1E-60", 2, MPFR_RNDN);
            emit_case(out, "adversarial", a, b, 53, MPFR_RNDU);
            mpfr_clear(a); mpfr_clear(b);
        }
        {
            mpfr_t a, b;
            mpfr_init2(a, 53); mpfr_init2(b, 53);
            mpfr_set_d(a, 1.0, MPFR_RNDN);
            mpfr_set_str(b, "1E-60", 2, MPFR_RNDN);
            emit_case(out, "adversarial", a, b, 53, MPFR_RNDD);
            mpfr_clear(a); mpfr_clear(b);
        }

        /* (23) Massive cancellation: (1 + 2^-100) - 1 = 2^-100. */
        {
            mpfr_t a, b;
            mpfr_init2(a, 200); mpfr_init2(b, 200);
            mpfr_set_d(a, 1.0, MPFR_RNDN);
            mpfr_t eps; mpfr_init2(eps, 200);
            mpfr_set_ui_2exp(eps, 1, -100, MPFR_RNDN);
            mpfr_add(a, a, eps, MPFR_RNDN);
            mpfr_set_d(b, 1.0, MPFR_RNDN);
            mpfr_clear(eps);
            emit_case(out, "adversarial", a, b, 200, MPFR_RNDN);
            mpfr_clear(a); mpfr_clear(b);
        }

        /* (24) sign-symmetric: (-a) - (-b) should equal -(a-b) when
         *      well-defined. Test the symmetry holds. */
        {
            mpfr_t a, b;
            init_from_double(a, -5.5, 53);
            init_from_double(b, -2.5, 53);
            emit_case(out, "adversarial", a, b, 53, MPFR_RNDN); /* -3 */
            mpfr_clear(a); mpfr_clear(b);
        }

        /* (25) Tiny - tiny opposite-sign — effective addition at small
         *      magnitudes. */
        {
            mpfr_t a, b;
            mpfr_init2(a, 53); mpfr_init2(b, 53);
            mpfr_set_str(a, "1E-100", 2, MPFR_RNDN);
            mpfr_set_str(b, "-1E-100", 2, MPFR_RNDN);
            emit_case(out, "adversarial", a, b, 53, MPFR_RNDN);
            mpfr_clear(a); mpfr_clear(b);
        }
    }

    /* fuzz: 100 cases — PRNG. Seed picked for hex-clean visibility. */
    {
        xs64_t rng;
        xs64_seed(&rng, 0x5B7AC7107E5705A0ULL);
        int emitted = 0;
        while (emitted < 100) {
            const uint64_t bits_a = xs64_next(&rng);
            const uint64_t bits_b = xs64_next(&rng);
            const uint64_t exp_a = (bits_a >> 52) & 0x7FF;
            const uint64_t exp_b = (bits_b >> 52) & 0x7FF;
            if (exp_a == 0x7FF || exp_b == 0x7FF) continue;

            const double da = bits_to_double(bits_a);
            const double db = bits_to_double(bits_b);
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

    /* mined: 6 cases — from mpfr/tests/tsub.c. The shapes that exercise
     * the sub-specific paths (cancellation, sign-flip, rnd-aware zero). */
    {
        /* tsub.c L48-ish: simple integer subtraction. */
        {
            mpfr_t a, b;
            mpfr_init2(a, 53); mpfr_init2(b, 53);
            mpfr_set_str(a, "9966027674114492.0",  10, MPFR_RNDN);
            mpfr_set_str(b, "1780341389094537.0",  10, MPFR_RNDN);
            emit_case(out, "mined", a, b, 53, MPFR_RNDN);
            mpfr_clear(a); mpfr_clear(b);
        }
        /* tsub.c cancellation pattern: x - x → ±0. */
        {
            mpfr_t a, b;
            init_from_double(a, 1.234e10, 53);
            init_from_double(b, 1.234e10, 53);
            emit_case(out, "mined", a, b, 53, MPFR_RNDD); /* -0 */
            mpfr_clear(a); mpfr_clear(b);
        }
        /* tsub.c sign-flip: 0 - x → -x. */
        {
            mpfr_t a, b;
            init_pos_zero(a, 53);
            init_from_double(b, 3.14, 53);
            emit_case(out, "mined", a, b, 53, MPFR_RNDN);
            mpfr_clear(a); mpfr_clear(b);
        }
        /* tsub.c Inf cases: +Inf - +Inf = NaN. */
        {
            mpfr_t a, b; init_pos_inf(a, 53); init_pos_inf(b, 53);
            emit_case(out, "mined", a, b, 53, MPFR_RNDN);
            mpfr_clear(a); mpfr_clear(b);
        }
        /* tsub.c: -1 - 1 = -2 at narrow prec exercises sign. */
        {
            mpfr_t a, b;
            init_from_double(a, -1.0, 53);
            init_from_double(b,  1.0, 53);
            emit_case(out, "mined", a, b, 53, MPFR_RNDN);
            mpfr_clear(a); mpfr_clear(b);
        }
        /* tsub.c precision-mismatch path. */
        {
            mpfr_t a, b;
            init_from_double(a, 3.14, 100);
            init_from_double(b, 2.71,  53);
            emit_case(out, "mined", a, b, 24, MPFR_RNDN);
            mpfr_clear(a); mpfr_clear(b);
        }
    }

    return 0;
}
