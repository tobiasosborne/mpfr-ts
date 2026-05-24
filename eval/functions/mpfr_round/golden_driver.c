/*
 * golden_driver.c — Golden master for MPFR's mpfr_round.
 *
 * C signature
 * -----------
 *
 *   int mpfr_round(mpfr_t r, mpfr_srcptr u);
 *
 *   Sets r to u rounded to the nearest integer (ties away from zero,
 *   RNDNA semantics). Returns the ternary flag — possibly ±2 from
 *   mpfr_rint's uflags encoding; jl_output_result normalises to ±1.
 *   Ref: mpfr/src/rint.c L317–L320 (mpfr_round wrapper) and
 *   mpfr/src/rint.c L35–L304 (the mpfr_rint engine it defers to).
 *
 * Divergence from C → TS
 * ----------------------
 *
 * TS port `mpfr_round(x, prec) -> Result` takes prec positionally
 * (replacing the rop's prec) and returns {value, ternary}. No rnd
 * parameter — the function name implies RNDNA.
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
 *   happy        :  ~25
 *   edge         :  ~40
 *   adversarial  :  ~15  (RNDNA-tie cases that distinguish from RNDN
 *                         ties-to-even; prec-fit ties; halfway-boundary
 *                         |x|=0.5)
 *   fuzz         :   60
 *   mined        :   5   (from mpfr/tests/trint.c L112–L132 — the
 *                         coverage tests for mpfr_round)
 *
 * Build via eval/golden_master/build.sh.
 */
#include "common.h"

#include <assert.h>
#include <inttypes.h>
#include <string.h>

#if GMP_NUMB_BITS != 64
#  error "mpfr_round golden_driver requires GMP_NUMB_BITS == 64"
#endif

#define TS_PREC_MAX ((uint64_t)((1ULL << 31) - 257ULL))
#define TS_PREC_MIN ((uint64_t)1)

/* Emit one mpfr_round golden case.
 *
 *   1. mpfr_init2(rop, prec) — target prec on a fresh probe.
 *   2. ternary = mpfr_round(rop, x).
 *   3. emit {tag, inputs:{x, prec}, output:{value: rop, ternary}}.
 *
 * Timing brackets only the mpfr_round call. */
static inline void emit_case(FILE *out, const char *tag,
                             mpfr_srcptr x, uint64_t prec) {
    assert(prec >= TS_PREC_MIN && prec <= TS_PREC_MAX);
    mpfr_t rop;
    mpfr_init2(rop, (mpfr_prec_t)prec);

    const uint64_t t0 = now_ns();
    const int ternary = mpfr_round(rop, x);
    const uint64_t elapsed = now_ns() - t0;

    jl_begin(out, tag);
    jl_kv_mpfr(out, 1, "x", x);
    jl_kv_u64(out, 0, "prec", prec);
    jl_end_inputs(out);
    jl_output_result(out, rop, ternary);
    jl_finish(out, elapsed);

    mpfr_clear(rop);
}

static inline void init_from_double(mpfr_ptr x, double d, uint64_t prec) {
    mpfr_init2(x, (mpfr_prec_t)prec); mpfr_set_d(x, d, MPFR_RNDN);
}
static inline void init_from_str_binary(mpfr_ptr x, const char *s,
                                        uint64_t prec) {
    mpfr_init2(x, (mpfr_prec_t)prec); mpfr_set_str(x, s, 2, MPFR_RNDN);
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

int main(void) {
    FILE *out = stdout;

    /* ============================================================== */
    /* happy: ~25 cases — typical inputs at common precs.             */
    /* ============================================================== */
    {
        /* Simple positive fractions — round to nearest. */
        emit_d(out, "happy", 2.3, 53, 53);   /* → 2 */
        emit_d(out, "happy", 2.7, 53, 53);   /* → 3 */
        emit_d(out, "happy", 3.5, 53, 53);   /* → 4 (RNDNA ties away) */
        emit_d(out, "happy", 4.5, 53, 53);   /* → 5 (RNDNA ties away) */
        emit_d(out, "happy", 10.4, 53, 53);  /* → 10 */
        emit_d(out, "happy", 10.6, 53, 53);  /* → 11 */
        emit_d(out, "happy", 100.5, 53, 53); /* → 101 */

        /* Negatives — RNDNA still rounds magnitude away. */
        emit_d(out, "happy", -2.3, 53, 53);  /* → -2 */
        emit_d(out, "happy", -2.7, 53, 53);  /* → -3 */
        emit_d(out, "happy", -3.5, 53, 53);  /* → -4 */
        emit_d(out, "happy", -10.4, 53, 53); /* → -10 */
        emit_d(out, "happy", -100.5, 53, 53);/* → -101 */

        /* Already-integer inputs — ternary 0. */
        emit_d(out, "happy", 1.0, 53, 53);
        emit_d(out, "happy", 2.0, 53, 53);
        emit_d(out, "happy", -1.0, 53, 53);
        emit_d(out, "happy", -100.0, 53, 53);
        emit_d(out, "happy", 0.0, 53, 53);  /* +0 */

        /* Common precs — same fractional input. */
        emit_d(out, "happy", 3.7, 24, 24);
        emit_d(out, "happy", 3.7, 64, 64);
        emit_d(out, "happy", 3.7, 100, 100);
        emit_d(out, "happy", 3.7, 200, 200);

        /* Large magnitudes. */
        emit_d(out, "happy", 1.5e15, 53, 53);
        emit_d(out, "happy", -1.5e15, 53, 53);
        emit_d(out, "happy", 6.022e23, 53, 53);
        emit_d(out, "happy", -6.022e23, 53, 53);
    }

    /* ============================================================== */
    /* edge: ~40 cases — kinds, signed zero, boundary values.         */
    /* ============================================================== */
    {
        /* NaN, ±Inf — propagate. */
        { mpfr_t x; init_nan(x, 53); emit_case(out, "edge", x, 53); mpfr_clear(x); }
        { mpfr_t x; init_nan(x, 53); emit_case(out, "edge", x, 1);  mpfr_clear(x); }
        { mpfr_t x; init_nan(x, 53); emit_case(out, "edge", x, 200); mpfr_clear(x); }
        { mpfr_t x; init_pos_inf(x, 53); emit_case(out, "edge", x, 53); mpfr_clear(x); }
        { mpfr_t x; init_pos_inf(x, 53); emit_case(out, "edge", x, 1);  mpfr_clear(x); }
        { mpfr_t x; init_neg_inf(x, 53); emit_case(out, "edge", x, 53); mpfr_clear(x); }
        { mpfr_t x; init_neg_inf(x, 53); emit_case(out, "edge", x, 200); mpfr_clear(x); }

        /* ±0 — signed zero preserved through round. */
        { mpfr_t x; init_pos_zero(x, 53); emit_case(out, "edge", x, 53); mpfr_clear(x); }
        { mpfr_t x; init_neg_zero(x, 53); emit_case(out, "edge", x, 53); mpfr_clear(x); }
        { mpfr_t x; init_pos_zero(x, 53); emit_case(out, "edge", x, 200); mpfr_clear(x); }
        { mpfr_t x; init_neg_zero(x, 53); emit_case(out, "edge", x, 1); mpfr_clear(x); }

        /* |x| < 1 boundary: 0.5 (exact tie at |x|=0.5).
         * RNDNA: round(+0.5) = +1, round(-0.5) = -1. */
        emit_str(out, "edge", "1.0E-1",  53, 53);  /* +0.5 = 2^-1 */
        emit_str(out, "edge", "-1.0E-1", 53, 53);  /* -0.5 */
        emit_str(out, "edge", "1.0E-1",  53, 1);   /* result fits in prec=1 */
        emit_str(out, "edge", "-1.0E-1", 53, 1);
        emit_str(out, "edge", "1.0E-1",  53, 100);

        /* |x| just below 0.5 — rounds to 0 (with sign preserved). */
        emit_str(out, "edge", "1.1E-2",  53, 53);  /* 0.011 = 0.375 */
        emit_str(out, "edge", "-1.1E-2", 53, 53);  /* -0.375 */

        /* |x| just above 0.5 — rounds to ±1. */
        emit_str(out, "edge", "1.1E-1", 53, 53);   /* 0.11 = 0.75 */
        emit_str(out, "edge", "-1.1E-1", 53, 53);

        /* |x| < 1 with very small magnitudes — rounds to ±0. */
        emit_d(out, "edge",  0.01, 53, 53);
        emit_d(out, "edge", -0.01, 53, 53);
        emit_d(out, "edge",  1e-100, 53, 53);
        emit_d(out, "edge", -1e-100, 53, 53);

        /* Already-integer at prec=1 (the smallest prec). */
        emit_d(out, "edge",  1.0, 53, 1);
        emit_d(out, "edge", -1.0, 53, 1);

        /* Negative result at low prec where the integer needs to fit
         * in fewer bits than its natural representation. */
        emit_d(out, "edge", 4.7, 53, 2);   /* round(4.7)=5; 5 at prec=2 */
        emit_d(out, "edge", -4.7, 53, 2);

        /* Tie at 1.5 — RNDNA → 2. */
        emit_d(out, "edge",  1.5, 53, 53);  /* → 2 */
        emit_d(out, "edge", -1.5, 53, 53);  /* → -2 */
        emit_d(out, "edge",  1.5, 53, 1);
        emit_d(out, "edge", -1.5, 53, 1);

        /* Tie at 0.5 with target prec stresses the integer-zero
         * branch — result is exactly ±1 which fits at any prec >= 1. */
        emit_d(out, "edge",  0.5, 53, 53);  /* → 1 */
        emit_d(out, "edge", -0.5, 53, 53);  /* → -1 */
        emit_d(out, "edge",  0.5, 53, 1);
        emit_d(out, "edge", -0.5, 53, 1);

        /* x.exp == 1 boundary: |x| in [1, 2). round(1.4)=1, round(1.6)=2,
         * round(1.5)=2 (tie). */
        emit_d(out, "edge", 1.4, 53, 53);
        emit_d(out, "edge", 1.6, 53, 53);

        /* Very large prec with an integer input — exact, ternary 0. */
        emit_d(out, "edge", 1.0, 200, 200);
        emit_d(out, "edge", -1.0, 200, 200);

        /* Small-prec output forcing the prec-fit step. */
        emit_d(out, "edge", 100.7, 53, 3);  /* round=101; prec-fit to 3 bits */
        emit_d(out, "edge", -100.7, 53, 3);
    }

    /* ============================================================== */
    /* adversarial: ~15 cases — RNDNA-vs-RNDN distinguishing.         */
    /* ============================================================== */
    {
        /* Prec-fit tie: integer = 5, prec=2.
         *   5 = 101 binary. drop 1 bit. trunc=10=2, dropped=1, half=1.
         *   RNDNA: increment → 11=3, exp+1 → value = 3*2^(3-2) = 6.
         *   RNDN ties-to-even: LSB of 2 is 0 → don't increment → 4.
         *   These differ; tests that the port uses RNDNA. */
        emit_d(out, "adversarial", 4.5, 53, 2);  /* round=5 at prec=2 → 6 */
        emit_d(out, "adversarial", -4.5, 53, 2);

        /* Integer = 7, prec = 2. 7 = 111; drop 1 bit. trunc=11=3, dropped=1.
         * Tie. RNDNA: incr → 100 carries → mant=10 exp=3 → 4*2 = 8. */
        emit_d(out, "adversarial", 6.5, 53, 2);  /* round(6.5)=7 at prec=2 → 8 */
        emit_d(out, "adversarial", -6.5, 53, 2);

        /* Integer = 3, prec = 1: 3 = 11; drop 1 bit. trunc=1, dropped=1, half=1.
         * RNDNA: incr → 10 carries → mant=1 exp=2, value=2*2 = 4. */
        emit_d(out, "adversarial", 2.5, 53, 1);

        /* RNDNA tie at |x|=0.5 exact: round(0.5)=1, ternary +1. */
        emit_str(out, "adversarial", "1.0E-1", 53, 53);
        emit_str(out, "adversarial", "-1.0E-1", 53, 53);

        /* RNDNA tie at |x|=0.5 with extra bits — NOT a tie anymore. */
        emit_str(out, "adversarial", "1.000001E-1", 53, 53); /* 0.5 + 2^-7 */
        emit_str(out, "adversarial", "1.111111E-2", 53, 53); /* < 0.5; just below */

        /* Halfway between integers at higher magnitudes. */
        emit_str(out, "adversarial", "1.011E2", 53, 53);  /* 1.011E2 = 5.5 → 6 */
        emit_str(out, "adversarial", "-1.011E2", 53, 53); /* → -6 */
        emit_str(out, "adversarial", "1.111E3", 53, 53);  /* 1111 = 15 — already int */

        /* Tie + prec-fit double rounding: input is a tie that rounds
         * to N, where N also ties at the target prec. Tests the
         * single-pass behaviour. */
        emit_d(out, "adversarial", 10.5, 53, 3);  /* round=11; prec=3 fits */
        emit_d(out, "adversarial", 9.5, 53, 3);   /* round=10; 1010; prec=3 → 1010 drop 1 = 101=5 exp=4 = 10? Check. */

        /* Large fractional below boundary with many bits set. */
        emit_str(out, "adversarial", "1.0111111111111111111111111E0", 53, 53);
        emit_str(out, "adversarial", "-1.0111111111111111111111111E0", 53, 53);
    }

    /* ============================================================== */
    /* fuzz: 60 cases — PRNG-driven, biased to small-magnitude         */
    /* fractional values so trunc/round/ceil/floor genuinely diverge   */
    /* across the fuzz set (mutation-prove leverage).                  */
    /* ============================================================== */
    {
        xs64_t rng;
        xs64_seed(&rng, 0xC0FFEEBABE5CADEEULL);
        const uint64_t precs[6] = { 1, 2, 53, 64, 100, 200 };

        int emitted = 0;
        /* 30 small-magnitude fractional values with frac biased toward
         * the half-ulp boundary [0.5, 1.0) — this is where round and
         * trunc diverge (round bumps, trunc doesn't), so the broken
         * port's gap from correct is maximised here. */
        while (emitted < 30) {
            const uint64_t r1 = xs64_next(&rng);
            const uint64_t r2 = xs64_next(&rng);
            const double whole = (double)(r1 % 1000);
            /* frac in [0.5, 1) — biased to the round-up half. */
            const double frac = 0.5 + 0.5 * ((double)r2 / 18446744073709551616.0);
            const int neg = (xs64_next(&rng) & 1) ? -1 : 1;
            const double d = neg * (whole + frac);

            const uint64_t srcp = precs[xs64_below(&rng, 6)];
            const uint64_t dstp = precs[xs64_below(&rng, 6)];

            mpfr_t x;
            init_from_double(x, d, srcp);
            emit_case(out, "fuzz", x, dstp);
            mpfr_clear(x);
            emitted++;
        }
        /* 30 wide-range random doubles — covers extreme magnitudes
         * where rounding is largely no-op but exercises the regime A
         * (xExp >= prec) path. */
        while (emitted < 60) {
            const uint64_t bits = xs64_next(&rng);
            const uint64_t exp = (bits >> 52) & 0x7FF;
            if (exp == 0x7FF) continue;

            double d;
            memcpy(&d, &bits, sizeof d);

            const uint64_t srcp = precs[xs64_below(&rng, 6)];
            const uint64_t dstp = precs[xs64_below(&rng, 6)];

            mpfr_t x;
            init_from_double(x, d, srcp);
            emit_case(out, "fuzz", x, dstp);
            mpfr_clear(x);
            emitted++;
        }
    }

    /* ============================================================== */
    /* mined: 5 cases — from mpfr/tests/trint.c L112–L145             */
    /* ============================================================== */
    {
        /* trint.c L113–L122: x=2.5 at prec=3, round to prec=2 → 3
         * (RNDNA ties away, NOT RNDN's ties-to-even). */
        {
            mpfr_t x; init_from_str_binary(x, "1.01E1", 3); /* 2.5 */
            emit_case(out, "mined", x, 2);
            mpfr_clear(x);
        }
        /* trint.c L124–L132: x=6.875 (110.111) at prec=6, round to prec=3.
         * round(6.875)=7 at prec=3 = 111. */
        {
            mpfr_t x; init_from_str_binary(x, "110.111", 6);
            emit_case(out, "mined", x, 3);
            mpfr_clear(x);
        }
        /* trint.c L134–L146: bug-found-by-Watkins case. Long binary
         * fraction; round to nearest integer at prec=84. */
        {
            mpfr_t x; init_from_str_binary(x,
                "0.110011010010001000000111101101001111111100101110010000000000000000000000000000000000E32",
                84);
            emit_case(out, "mined", x, 84);
            mpfr_clear(x);
        }
        /* trint.c-style: round(2.5) at higher prec. */
        {
            mpfr_t x; init_from_str_binary(x, "1.01E1", 53);
            emit_case(out, "mined", x, 53);
            mpfr_clear(x);
        }
        /* trint.c basic_tests-style: round at prec=1 with i=3 → s*((i+2)/4) = ±1.
         * x = 3/4 = 0.75 at prec=4. round(0.75) = 1. */
        {
            mpfr_t x; init_from_str_binary(x, "0.11", 4); /* 0.75 */
            emit_case(out, "mined", x, 1);
            mpfr_clear(x);
        }
    }

    return 0;
}
