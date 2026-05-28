/*
 * golden_driver.c -- Golden master for MPFR's mpfr_roundeven.
 *
 * C signature
 * -----------
 *
 *   int mpfr_roundeven(mpfr_ptr r, mpfr_srcptr u);
 *
 *   Sets r to u rounded to the nearest integer, ties to EVEN (RNDN
 *   semantics). One-line wrapper: mpfr_roundeven(r,u) returns
 *   mpfr_rint(r, u, MPFR_RNDN). Returns the ternary flag -- possibly
 *   +-2 from mpfr_rint's uflags encoding; jl_output_result normalises
 *   to {-1,0,1}.
 *   Ref: mpfr/src/rint.c L308-L312 (wrapper) and L35-L304 (engine).
 *
 *   Contrast with mpfr_round (rint.c L317-L320, RNDNA ties-AWAY):
 *     roundeven(0.5)=0, roundeven(2.5)=2, roundeven(1.5)=2;
 *     round    (0.5)=1, round    (2.5)=3, round    (1.5)=2.
 *   The ties-to-even rule is what the adversarial block exercises.
 *
 * Divergence from C -> TS
 * -----------------------
 *
 * TS port `mpfr_roundeven(x, prec) -> Result` takes prec positionally
 * (constructing a fresh rop at that prec) and returns {value, ternary}.
 * No rnd parameter -- the function name implies ties-to-even (RNDN).
 *
 * Wire format
 * -----------
 *
 *   {"tag":"<class>",
 *    "inputs":{"x":<MPFR-record>,"prec":"<decimal>"},
 *    "output":{"value":<MPFR-record>,"ternary":<-1|0|1>},
 *    "time_ns":<n>}
 *
 * Tag distribution (CLAUDE.md Rule 7 minimums)
 * --------------------------------------------
 *
 *   happy        :  25
 *   edge         :  40  (NaN/Inf/+-0/integer fast path/|x|<1/low prec)
 *   adversarial  :  15  (ties-to-even at the .5 boundary and at the
 *                        prec-fit boundary; the cases that distinguish
 *                        roundeven from round)
 *   fuzz         :  60
 *   mined        :   8  (from mpfr/tests/trint.c special() + coverage)
 *
 * Compile (do NOT use build.sh -- races with sibling drivers):
 *   gcc -O2 -std=c11 -Wall -Wextra -Werror -I. \
 *     ../functions/mpfr_roundeven/golden_driver.c \
 *     $(pkg-config --cflags --libs mpfr) -lgmp -lm \
 *     -o ../functions/mpfr_roundeven/golden_driver
 *
 * Ref: mpfr/src/rint.c L308-L312      -- C reference wrapper.
 * Ref: mpfr/tests/trint.c             -- mined source.
 * Ref: src/core.ts                    -- locked Result / MPFR types.
 * Ref: eval/functions/mpfr_round/golden_driver.c -- structural sibling.
 */
#include "common.h"

#include <assert.h>
#include <inttypes.h>
#include <string.h>

#if GMP_NUMB_BITS != 64
#  error "mpfr_roundeven golden_driver requires GMP_NUMB_BITS == 64"
#endif

#define TS_PREC_MAX ((uint64_t)((1ULL << 31) - 257ULL))
#define TS_PREC_MIN ((uint64_t)1)

/* Emit one mpfr_roundeven golden case.
 *
 *   1. mpfr_init2(rop, prec) -- target prec on a fresh probe.
 *   2. ternary = mpfr_roundeven(rop, x).
 *   3. emit {tag, inputs:{x, prec}, output:{value: rop, ternary}}.
 *
 * Timing brackets only the mpfr_roundeven call. */
static inline void emit_case(FILE *out, const char *tag,
                             mpfr_srcptr x, uint64_t prec) {
    assert(prec >= TS_PREC_MIN && prec <= TS_PREC_MAX);
    mpfr_t rop;
    mpfr_init2(rop, (mpfr_prec_t)prec);

    const uint64_t t0 = now_ns();
    const int ternary = mpfr_roundeven(rop, x);
    const uint64_t elapsed = now_ns() - t0;

    jl_begin(out, tag);
    jl_kv_mpfr(out, 1, "x", x);
    jl_kv_u64(out, 0, "prec", prec);
    jl_end_inputs(out);
    jl_output_result(out, rop, ternary);
    jl_finish(out, elapsed);

    mpfr_clear(rop);
}

/* ----------------------- helpers ----------------------- */

static inline void init_from_double(mpfr_ptr x, double d, uint64_t prec) {
    mpfr_init2(x, (mpfr_prec_t)prec); mpfr_set_d(x, d, MPFR_RNDN);
}
static inline void init_from_str_binary(mpfr_ptr x, const char *s,
                                        uint64_t prec) {
    mpfr_init2(x, (mpfr_prec_t)prec); mpfr_set_str(x, s, 2, MPFR_RNDN);
}
static inline void init_from_si(mpfr_ptr x, long v, uint64_t prec) {
    mpfr_init2(x, (mpfr_prec_t)prec); mpfr_set_si(x, v, MPFR_RNDN);
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

static inline void emit_d(FILE *out, const char *tag,
                          double d, uint64_t srcp, uint64_t dstp) {
    mpfr_t x; init_from_double(x, d, srcp);
    emit_case(out, tag, x, dstp);
    mpfr_clear(x);
}
static inline void emit_str(FILE *out, const char *tag,
                            const char *s, uint64_t srcp, uint64_t dstp) {
    mpfr_t x; init_from_str_binary(x, s, srcp);
    emit_case(out, tag, x, dstp);
    mpfr_clear(x);
}

/* ----------------------- main ----------------------- */

int main(void) {
    FILE *out = stdout;

    /* ============================================================== */
    /* happy: 25 -- finite |x| >= 1 and simple |x| < 1 rounding.       */
    /* ============================================================== */
    {
        /* Simple non-integers: nearest integer, ties-to-even. */
        emit_d(out, "happy", 3.14,    53, 53);   /* -> 3 */
        emit_d(out, "happy", 3.7,     53, 53);   /* -> 4 */
        emit_d(out, "happy", -3.14,   53, 53);   /* -> -3 */
        emit_d(out, "happy", -3.7,    53, 53);   /* -> -4 */
        emit_d(out, "happy", 2.3,     53, 53);   /* -> 2 */
        emit_d(out, "happy", 10.4,    53, 53);   /* -> 10 */
        emit_d(out, "happy", 10.6,    53, 53);   /* -> 11 */
        emit_d(out, "happy", 100.49,  53, 53);   /* -> 100 */
        emit_d(out, "happy", 100.51,  53, 53);   /* -> 101 */
        emit_d(out, "happy", 1234.5678, 53, 53); /* -> 1235 */
        emit_d(out, "happy", -1234.4321, 53, 53);/* -> -1234 */

        /* Common precisions, value with a clear nearest integer. */
        emit_d(out, "happy", 3.7,     24,  24);
        emit_d(out, "happy", 3.7,     64,  64);
        emit_d(out, "happy", 3.7,     113, 113);
        emit_d(out, "happy", 3.7,     200, 200);

        /* Larger magnitudes (still exact integers at this prec). */
        emit_d(out, "happy", 1.5e15,  53, 53);
        emit_d(out, "happy", -1.5e15, 53, 53);
        emit_d(out, "happy", 6.022e23, 53, 53);
        emit_d(out, "happy", -6.022e23, 53, 53);

        /* Different src/dst precs. */
        emit_d(out, "happy", 3.14,    100, 53);
        emit_d(out, "happy", 3.14,    53,  100);

        /* Exact-binary fractional parts: nearest integer unambiguous. */
        emit_d(out, "happy", 9.25,    53, 53);   /* -> 9 */
        emit_d(out, "happy", 17.75,   53, 53);   /* -> 18 */
        emit_d(out, "happy", 65.875,  53, 53);   /* -> 66 */
        emit_d(out, "happy", -65.125, 53, 53);   /* -> -65 */
    }

    /* ============================================================== */
    /* edge: 40 -- specials, signed zero, integer fast path, low prec  */
    /* ============================================================== */
    {
        /* NaN -> NaN. */
        { mpfr_t x; init_nan(x, 53);      emit_case(out, "edge", x, 53);  mpfr_clear(x); }
        { mpfr_t x; init_nan(x, 53);      emit_case(out, "edge", x, 1);   mpfr_clear(x); }
        { mpfr_t x; init_nan(x, 200);     emit_case(out, "edge", x, 200); mpfr_clear(x); }

        /* +-Inf -> +-Inf (sign preserved, ternary 0). */
        { mpfr_t x; init_pos_inf(x, 53);  emit_case(out, "edge", x, 53);  mpfr_clear(x); }
        { mpfr_t x; init_pos_inf(x, 53);  emit_case(out, "edge", x, 1);   mpfr_clear(x); }
        { mpfr_t x; init_pos_inf(x, 53);  emit_case(out, "edge", x, 200); mpfr_clear(x); }
        { mpfr_t x; init_neg_inf(x, 53);  emit_case(out, "edge", x, 53);  mpfr_clear(x); }
        { mpfr_t x; init_neg_inf(x, 53);  emit_case(out, "edge", x, 1);   mpfr_clear(x); }
        { mpfr_t x; init_neg_inf(x, 53);  emit_case(out, "edge", x, 200); mpfr_clear(x); }

        /* +-0 -> +-0 (signed zero preserved). */
        { mpfr_t x; init_pos_zero(x, 53); emit_case(out, "edge", x, 53);  mpfr_clear(x); }
        { mpfr_t x; init_neg_zero(x, 53); emit_case(out, "edge", x, 53);  mpfr_clear(x); }
        { mpfr_t x; init_pos_zero(x, 53); emit_case(out, "edge", x, 200); mpfr_clear(x); }
        { mpfr_t x; init_neg_zero(x, 53); emit_case(out, "edge", x, 1);   mpfr_clear(x); }

        /* Already-integer x: integer fast path, ternary 0 when fits. */
        emit_d(out, "edge",  3.0,         53, 53);
        emit_d(out, "edge", -7.0,         53, 53);
        emit_d(out, "edge",  1.0,         53, 53);
        emit_d(out, "edge", -1.0,         53, 53);
        emit_d(out, "edge",  2.0,         53, 53);
        emit_d(out, "edge",  4503599627370496.0,  53, 53); /* 2^52 */
        emit_d(out, "edge", -4503599627370496.0,  53, 53);
        emit_str(out, "edge",  "1E100",  53, 53);          /* 2^100 */
        emit_str(out, "edge", "-1E100",  53, 53);

        /* |x| < 1 non-integer, NOT exactly 0.5: nearest is 0 or +-1.
         * 0.25 -> 0; 0.75 -> 1; 0.7 -> 1; 0.3 -> 0. */
        emit_d(out, "edge",  0.25,    53, 53);   /* -> +0, ternary -2->-1 */
        emit_d(out, "edge", -0.25,    53, 53);   /* -> -0 */
        emit_d(out, "edge",  0.75,    53, 53);   /* -> +1 */
        emit_d(out, "edge", -0.75,    53, 53);   /* -> -1 */
        emit_d(out, "edge",  0.7,     53, 53);   /* -> +1 */
        emit_d(out, "edge", -0.3,     53, 53);   /* -> -0 */
        emit_d(out, "edge",  1e-10,   53, 53);   /* -> +0 */
        emit_d(out, "edge", -1e-10,   53, 53);   /* -> -0 */

        /* Exactly +-0.5: ties-to-even rounds to 0 (the RNDN 0.5 rule). */
        emit_d(out, "edge",  0.5,     53, 53);   /* -> +0 */
        emit_d(out, "edge", -0.5,     53, 53);   /* -> -0 */
        emit_str(out, "edge", "0.1E0", 53, 1);   /* 0.5 -> 0 */
        emit_str(out, "edge", "-0.1E0",53, 1);   /* -0.5 -> -0 */

        /* Very large x (exp > 1000) -- integer; set may round at lower prec. */
        emit_str(out, "edge",  "1.0101010101E1100", 53, 53);
        emit_str(out, "edge", "-1.0101010101E1100", 53, 53);

        /* Output prec smaller than nearest-int bit width: forces a
         * second rounding by the prec-fit step. */
        emit_d(out, "edge",  100.7,   53, 3);    /* nearest=101 -> 3 bits */
        emit_d(out, "edge", -100.7,   53, 3);
        emit_d(out, "edge",  4.7,     53, 2);    /* nearest=5 -> 2 bits */
        emit_d(out, "edge",  3.4,     53, 1);    /* nearest=3 -> 1 bit */
        emit_d(out, "edge", -3.4,     53, 1);
    }

    /* ============================================================== */
    /* adversarial: 15 -- ties-to-even cases (distinguish from round)  */
    /* ============================================================== */
    {
        /* Half-integer values: ties to EVEN. roundeven differs from
         * round (ties-away) on exactly these. Build via binary strings
         * so the .5 is exact. */
        emit_str(out, "adversarial", "1.1E0",  53, 53);  /* 1.5 -> 2 (even) */
        emit_str(out, "adversarial", "1.01E1", 53, 53);  /* 2.5 -> 2 (even) */
        emit_str(out, "adversarial", "1.11E1", 53, 53);  /* 3.5 -> 4 (even) */
        emit_str(out, "adversarial", "1.001E2",53, 53);  /* 4.5 -> 4 (even) */
        emit_str(out, "adversarial", "1.011E2",53, 53);  /* 5.5 -> 6 (even) */
        emit_str(out, "adversarial", "-1.1E0", 53, 53);  /* -1.5 -> -2 */
        emit_str(out, "adversarial", "-1.01E1",53, 53);  /* -2.5 -> -2 */
        emit_str(out, "adversarial", "-1.11E1",53, 53);  /* -3.5 -> -4 */

        /* prec-fit boundary ties: nearest integer then a halfway drop at
         * smaller prec, again ties-to-even.
         * x=5.7 -> nearest int 6 = 110_2; at prec=2 drop 1 bit, value 6
         * is exact in 2 bits? 6=110 needs 3 sig bits -> at prec 2 it is
         * 11_2 * 2^1 = 6 exactly? No: 110 has 2 trailing-after-MSB so
         * prec=2 keeps 11, exp=3 -> 6 exact. Use 5.7->6 then prec=2 keeps
         * exact; instead use a value whose nearest int has a true tie at
         * prec. nearest=5 (101_2): prec=2 drops low bit '1' = tie ->
         * even: 10_2 -> 4. */
        emit_d(out, "adversarial", 5.4,  53, 2);  /* nearest=5=101 -> tie -> 4 (even) */
        emit_d(out, "adversarial", -5.4, 53, 2);  /* nearest=-5 -> -4 */
        emit_d(out, "adversarial", 7.4,  53, 2);  /* nearest=7=111 -> tie -> 8 (even, carry) */
        emit_d(out, "adversarial", -7.4, 53, 2);  /* nearest=-7 -> -8 */
        emit_d(out, "adversarial", 3.4,  53, 1);  /* nearest=3=11 -> tie -> 4 (even, carry) */

        /* Larger prec-fit reduction on a binary-exact integer: the
         * nearest integer is large and gets re-rounded ties-to-even at
         * prec=4. Exercises the multi-limb-ish integer extraction path. */
        emit_str(out, "adversarial", "1.10101E20", 24, 4);
        emit_str(out, "adversarial", "-1.10101E20", 24, 4);
    }

    /* ============================================================== */
    /* fuzz: 60 -- xorshift-driven random x / prec.                    */
    /* ============================================================== */
    {
        xs64_t rng;
        xs64_seed(&rng, 0xEEDCBA9876543210ULL);  /* unique hex seed */
        const uint64_t precs[6] = { 1, 2, 53, 64, 100, 200 };

        int emitted = 0;
        /* 30: |x| > 1 with a biased fractional part, hitting both the
         * nearest-int and the prec-fit rounding regimes. Whole part in
         * [1, 65536). */
        while (emitted < 30) {
            const uint64_t r1 = xs64_next(&rng);
            const uint64_t r2 = xs64_next(&rng);
            const double whole = (double)(1 + (r1 % 65535));
            const double frac = ((double)(r2 | 1) / 18446744073709551616.0);
            const int neg = (xs64_next(&rng) & 1) ? -1 : 1;
            const double d = neg * (whole + frac);

            const uint64_t srcp = precs[xs64_below(&rng, 6)];
            const uint64_t dstp = precs[xs64_below(&rng, 6)];

            mpfr_t x; init_from_double(x, d, srcp);
            emit_case(out, "fuzz", x, dstp);
            mpfr_clear(x);
            emitted++;
        }
        /* 30: wide-range random doubles (incl. |x| < 1, huge integers,
         * occasional integral values). Skip NaN/Inf encodings. */
        while (emitted < 60) {
            const uint64_t bits = xs64_next(&rng);
            const uint64_t exp_bits = (bits >> 52) & 0x7FF;
            if (exp_bits == 0x7FF) continue;

            double d;
            memcpy(&d, &bits, sizeof d);

            const uint64_t srcp = precs[xs64_below(&rng, 6)];
            const uint64_t dstp = precs[xs64_below(&rng, 6)];

            mpfr_t x; init_from_double(x, d, srcp);
            emit_case(out, "fuzz", x, dstp);
            mpfr_clear(x);
            emitted++;
        }
    }

    /* ============================================================== */
    /* mined: 8 -- from mpfr/tests/trint.c special() + coverage.       */
    /* ============================================================== */
    {
        /* trint.c L38-L40: rint(NaN, RNDN) -> NaN. */
        { mpfr_t x; init_nan(x, 53);      emit_case(out, "mined", x, 53); mpfr_clear(x); }
        /* trint.c L42-L44: rint(+Inf, RNDN) -> +Inf. */
        { mpfr_t x; init_pos_inf(x, 53);  emit_case(out, "mined", x, 53); mpfr_clear(x); }
        /* trint.c L46-L48: rint(-Inf, RNDN) -> -Inf. */
        { mpfr_t x; init_neg_inf(x, 53);  emit_case(out, "mined", x, 53); mpfr_clear(x); }
        /* trint.c L50-L52: rint(+0, RNDN) -> +0. */
        { mpfr_t x; init_pos_zero(x, 53); emit_case(out, "mined", x, 53); mpfr_clear(x); }
        /* trint.c L54-L57: rint(-0, RNDN) -> -0. */
        { mpfr_t x; init_neg_zero(x, 53); emit_case(out, "mined", x, 53); mpfr_clear(x); }
        /* trint.c L60-L63: coverage -- x = 2^limb_bits at prec 2, RNDN
         * leaves x unchanged (already a representable integer). */
        {
            mpfr_t x; init_from_si(x, 1, 2);
            mpfr_mul_2ui(x, x, (unsigned long)mp_bits_per_limb, MPFR_RNDN);
            emit_case(out, "mined", x, 53);
            mpfr_clear(x);
        }
        /* trint.c L92-L99: prec(x)=36, prec(y)=2, x=-11000110...0000100,
         * RNDN -> -11E27. A genuine prec-fit ties-to-even reduction. */
        {
            mpfr_t x; init_from_str_binary(x, "-11000110101010111111110111001.0000100", 36);
            emit_case(out, "mined", x, 2);
            mpfr_clear(x);
        }
        /* trint.c L100-L103: prec(x)=39, prec(y)=29, RNDN. */
        {
            mpfr_t x; init_from_str_binary(x, "-0.100010110100011010001111001001001100111E39", 39);
            emit_case(out, "mined", x, 29);
            mpfr_clear(x);
        }
    }

    return 0;
}
