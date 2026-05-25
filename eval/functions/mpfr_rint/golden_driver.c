/*
 * golden_driver.c -- Golden master for MPFR's mpfr_rint.
 *
 * C signature
 * -----------
 *
 *   int mpfr_rint(mpfr_t r, mpfr_srcptr u, mpfr_rnd_t rnd);
 *
 *   Round u to the nearest prec(r)-representable integer in mode rnd.
 *   Returns the ternary flag (sign of rounded - exact). The parent
 *   round-to-integer dispatcher; mpfr_trunc/floor/ceil/round are all
 *   wrappers over this (mpfr/src/rint.c L317-L344).
 *
 * The five branches:
 *   - RNDZ  : toward zero    (== mpfr_trunc(r, u)).
 *   - RNDU  : toward +inf    (== mpfr_ceil(r, u)).
 *   - RNDD  : toward -inf    (== mpfr_floor(r, u)).
 *   - RNDA  : away from zero (ceil for positives, floor for negatives).
 *   - RNDN  : nearest, ties to even (the IEEE-754 default).
 *
 * RNDN here is **ties-to-even**, distinct from mpfr_round which is
 * RNDNA (ties-away-from-zero). At u=2.5 prec=2: RNDN -> 2 (even
 * mantissa LSB), RNDNA -> 3.
 *
 * Special cases:
 *   - NaN              -> NaN, ternary 0.
 *   - +/-Inf           -> +/-Inf (sign preserved), ternary 0.
 *   - +/-0             -> +/-0 (signed-zero preserved), ternary 0.
 *   - representable integer u -> r = u, ternary 0.
 *   - |u| < 1 non-int  -> {-1, 0, +1} per rnd; RNDN at |u|=0.5 -> 0
 *                         (even) with sign preserved.
 *   - general          -> ternary = sign(rounded - exact).
 *
 * Divergence from C -> TS
 * -----------------------
 *
 * TS port `mpfr_rint(u, prec, rnd) -> Result` takes prec positionally
 * and returns {value, ternary} from src/core.ts. The C side's +-2 uflag
 * encoding for "u is an integer not representable in r" (rint.c
 * L289-L302) is collapsed to +-1 ternary on the wire via
 * jl_output_result's normalisation.
 *
 * Wire format
 * -----------
 *
 *   {"tag":"<class>",
 *    "inputs":{"u":<MPFR-record>,"prec":"<decimal>","rnd":"RND[NZUDA]"},
 *    "output":{"value":<MPFR-record>,"ternary":<-1|0|1>},
 *    "time_ns":<n>}
 *
 * Tag distribution (CLAUDE.md Rule 7 minimums)
 * --------------------------------------------
 *
 *   happy        :  ~25  (|u| >= 1 non-integer; all 5 rnds; common precs)
 *   edge         :  ~40  (specials, signed zero, integer fast path,
 *                         |u|<1, prec(r)<width(int(u)))
 *   adversarial  :  ~22  (RNDN-vs-RNDNA distinguishing cases; tie cases
 *                         at every rnd mode; halfway 2^p +/- 0.5;
 *                         carries on tie)
 *   fuzz         :   60  (xorshift; mixed magnitudes / precs / rnds)
 *   mined        :   8   (from mpfr/tests/trint.c L38-L110 explicit
 *                         mpfr_rint(...) coverage tests)
 *
 * Build via eval/golden_master/build.sh.
 *
 * Ref: mpfr/src/rint.c L35-L304       -- C reference body.
 * Ref: mpfr/tests/trint.c L38-L110    -- mined source (explicit
 *                                        mpfr_rint() coverage).
 * Ref: src/core.ts                    -- locked Result / MPFR types.
 * Ref: eval/functions/mpfr_rint_trunc/golden_driver.c -- structural
 *                                        sibling.
 */
#include "common.h"

#include <assert.h>
#include <inttypes.h>
#include <string.h>

#if GMP_NUMB_BITS != 64
#  error "mpfr_rint golden_driver requires GMP_NUMB_BITS == 64"
#endif

#define TS_PREC_MAX ((uint64_t)((1ULL << 31) - 257ULL))
#define TS_PREC_MIN ((uint64_t)1)

static const mpfr_rnd_t RNDS[5] = {
    MPFR_RNDN, MPFR_RNDZ, MPFR_RNDU, MPFR_RNDD, MPFR_RNDA
};

/* Emit one mpfr_rint golden case.
 *
 *   1. mpfr_init2(rop, prec).
 *   2. ternary = mpfr_rint(rop, u, rnd).
 *   3. emit {tag, inputs:{u, prec, rnd}, output:{value: rop, ternary}}.
 *
 * Timing brackets only the mpfr_rint call. */
static inline void emit_case(FILE *out, const char *tag,
                             mpfr_srcptr u, uint64_t prec,
                             mpfr_rnd_t rnd) {
    assert(prec >= TS_PREC_MIN && prec <= TS_PREC_MAX);
    mpfr_t rop;
    mpfr_init2(rop, (mpfr_prec_t)prec);

    const uint64_t t0 = now_ns();
    const int ternary = mpfr_rint(rop, u, rnd);
    const uint64_t elapsed = now_ns() - t0;

    jl_begin(out, tag);
    jl_kv_mpfr(out, 1, "u", u);
    jl_kv_u64(out, 0, "prec", prec);
    jl_kv_rnd(out, 0, "rnd", rnd);
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
static inline void init_from_str_decimal(mpfr_ptr x, const char *s,
                                         uint64_t prec) {
    mpfr_init2(x, (mpfr_prec_t)prec); mpfr_set_str(x, s, 10, MPFR_RNDN);
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
                          double d, uint64_t srcp, uint64_t dstp,
                          mpfr_rnd_t rnd) {
    mpfr_t u; init_from_double(u, d, srcp);
    emit_case(out, tag, u, dstp, rnd);
    mpfr_clear(u);
}

static inline void emit_str_bin(FILE *out, const char *tag,
                                const char *s, uint64_t srcp, uint64_t dstp,
                                mpfr_rnd_t rnd) {
    mpfr_t u; init_from_str_binary(u, s, srcp);
    emit_case(out, tag, u, dstp, rnd);
    mpfr_clear(u);
}

static inline void emit_str_dec(FILE *out, const char *tag,
                                const char *s, uint64_t srcp, uint64_t dstp,
                                mpfr_rnd_t rnd) {
    mpfr_t u; init_from_str_decimal(u, s, srcp);
    emit_case(out, tag, u, dstp, rnd);
    mpfr_clear(u);
}

/* ----------------------- main ----------------------- */

int main(void) {
    FILE *out = stdout;

    /* ============================================================== */
    /* happy: ~25 -- |u| >= 1 non-integers; all 5 rnds; common precs.  */
    /* ============================================================== */
    {
        /* Positive non-integers across all rnds. */
        emit_d(out, "happy", 3.14,    53, 53, MPFR_RNDN);
        emit_d(out, "happy", 3.14,    53, 53, MPFR_RNDZ);
        emit_d(out, "happy", 3.14,    53, 53, MPFR_RNDU);
        emit_d(out, "happy", 3.14,    53, 53, MPFR_RNDD);
        emit_d(out, "happy", 3.14,    53, 53, MPFR_RNDA);

        /* Negative non-integers. */
        emit_d(out, "happy", -3.14,   53, 53, MPFR_RNDN);
        emit_d(out, "happy", -3.14,   53, 53, MPFR_RNDU);
        emit_d(out, "happy", -3.14,   53, 53, MPFR_RNDD);
        emit_d(out, "happy", -3.14,   53, 53, MPFR_RNDA);

        /* RNDN on .7 frac -- not a tie, rounds up. */
        emit_d(out, "happy", 2.7,     53, 53, MPFR_RNDN);
        emit_d(out, "happy", 10.4,    53, 53, MPFR_RNDN);
        emit_d(out, "happy", 100.9,   53, 53, MPFR_RNDN);
        emit_d(out, "happy", -2.7,    53, 53, MPFR_RNDN);

        /* Common precisions (24/53/64/113/200). */
        emit_d(out, "happy", 3.7,     24,  24,  MPFR_RNDN);
        emit_d(out, "happy", 3.7,     64,  64,  MPFR_RNDN);
        emit_d(out, "happy", 3.7,     113, 113, MPFR_RNDN);
        emit_d(out, "happy", 3.7,     200, 200, MPFR_RNDN);

        /* Larger magnitudes -- the integer fits in 53 bits exactly. */
        emit_d(out, "happy", 1.5e15,  53, 53, MPFR_RNDN);
        emit_d(out, "happy", -1.5e15, 53, 53, MPFR_RNDA);

        /* Different src/dst precs. */
        emit_d(out, "happy", 3.14,    100, 53,  MPFR_RNDN);
        emit_d(out, "happy", 3.14,    53,  100, MPFR_RNDU);

        /* Exact binary fractional parts. */
        emit_d(out, "happy", 9.5,     53, 53, MPFR_RNDN);  /* tie 9/10 -> 10 (even) */
        emit_d(out, "happy", 8.5,     53, 53, MPFR_RNDN);  /* tie 8/9 -> 8 (even) */
        emit_d(out, "happy", 17.25,   53, 53, MPFR_RNDN);  /* nearest 17 */
        emit_d(out, "happy", 17.75,   53, 53, MPFR_RNDN);  /* nearest 18 */
    }

    /* ============================================================== */
    /* edge: ~40 -- specials, signed zero, integer fast path, |u|<1   */
    /* ============================================================== */
    {
        /* NaN -> NaN, one per rnd. */
        for (int i = 0; i < 5; ++i) {
            mpfr_t u; init_nan(u, 53);
            emit_case(out, "edge", u, 53, RNDS[i]);
            mpfr_clear(u);
        }

        /* +Inf -> +Inf, various prec(r) and rnd. */
        { mpfr_t u; init_pos_inf(u, 53);  emit_case(out, "edge", u, 53,  MPFR_RNDN); mpfr_clear(u); }
        { mpfr_t u; init_pos_inf(u, 53);  emit_case(out, "edge", u, 1,   MPFR_RNDZ); mpfr_clear(u); }
        { mpfr_t u; init_pos_inf(u, 53);  emit_case(out, "edge", u, 200, MPFR_RNDU); mpfr_clear(u); }
        /* -Inf -> -Inf. */
        { mpfr_t u; init_neg_inf(u, 53);  emit_case(out, "edge", u, 53,  MPFR_RNDN); mpfr_clear(u); }
        { mpfr_t u; init_neg_inf(u, 53);  emit_case(out, "edge", u, 1,   MPFR_RNDD); mpfr_clear(u); }
        { mpfr_t u; init_neg_inf(u, 53);  emit_case(out, "edge", u, 200, MPFR_RNDA); mpfr_clear(u); }

        /* +0 -> +0, -0 -> -0 across rnds. */
        { mpfr_t u; init_pos_zero(u, 53); emit_case(out, "edge", u, 53,  MPFR_RNDN); mpfr_clear(u); }
        { mpfr_t u; init_neg_zero(u, 53); emit_case(out, "edge", u, 53,  MPFR_RNDN); mpfr_clear(u); }
        { mpfr_t u; init_pos_zero(u, 53); emit_case(out, "edge", u, 200, MPFR_RNDZ); mpfr_clear(u); }
        { mpfr_t u; init_neg_zero(u, 53); emit_case(out, "edge", u, 1,   MPFR_RNDA); mpfr_clear(u); }
        { mpfr_t u; init_pos_zero(u, 53); emit_case(out, "edge", u, 53,  MPFR_RNDD); mpfr_clear(u); }
        { mpfr_t u; init_neg_zero(u, 53); emit_case(out, "edge", u, 53,  MPFR_RNDU); mpfr_clear(u); }

        /* Already-integer u -- representable in prec(r). All rnds same. */
        emit_d(out, "edge",  3.0,     53,  53, MPFR_RNDN);
        emit_d(out, "edge", -7.0,     53,  53, MPFR_RNDN);
        emit_d(out, "edge",  1.0,     53,  53, MPFR_RNDN);
        emit_d(out, "edge", -1.0,     53,  53, MPFR_RNDN);
        emit_d(out, "edge",  4503599627370496.0,  53, 53, MPFR_RNDN); /* 2^52 */
        emit_d(out, "edge", -4503599627370496.0,  53, 53, MPFR_RNDA);
        emit_str_bin(out, "edge",  "1E100",  53, 53, MPFR_RNDN);      /* 2^100 */
        emit_str_bin(out, "edge", "-1E100",  53, 53, MPFR_RNDD);

        /* Integer u, all 5 rnds -- all same result. */
        for (int i = 0; i < 5; ++i) {
            emit_d(out, "edge", 3.0, 53, 53, RNDS[i]);
        }

        /* |u| < 1 non-integer: forced to {-1, 0, +1} by rnd. */
        emit_d(out, "edge",  0.5,    53, 53, MPFR_RNDN);   /* tie 0 (even) */
        emit_d(out, "edge", -0.5,    53, 53, MPFR_RNDN);   /* tie -0 (even) */
        emit_d(out, "edge",  0.5,    53, 53, MPFR_RNDA);   /* +1 (away) */
        emit_d(out, "edge", -0.5,    53, 53, MPFR_RNDA);   /* -1 (away) */
        emit_d(out, "edge",  0.5,    53, 53, MPFR_RNDU);   /* +1 */
        emit_d(out, "edge", -0.5,    53, 53, MPFR_RNDU);   /* -0 */
        emit_d(out, "edge",  0.5,    53, 53, MPFR_RNDD);   /* +0 */
        emit_d(out, "edge", -0.5,    53, 53, MPFR_RNDD);   /* -1 */
        emit_d(out, "edge",  0.5,    53, 53, MPFR_RNDZ);   /* +0 */
        emit_d(out, "edge", -0.5,    53, 53, MPFR_RNDZ);   /* -0 */

        /* |u| < 0.5 -- RNDN always 0; RNDA away to +-1. */
        emit_d(out, "edge",  0.25,   53, 53, MPFR_RNDN);
        emit_d(out, "edge", -0.25,   53, 53, MPFR_RNDN);
        emit_d(out, "edge",  0.25,   53, 53, MPFR_RNDA);
        emit_d(out, "edge", -0.25,   53, 53, MPFR_RNDA);

        /* |u| > 0.5 -- RNDN goes to +-1. */
        emit_d(out, "edge",  0.7,    53, 53, MPFR_RNDN);
        emit_d(out, "edge", -0.7,    53, 53, MPFR_RNDN);
        emit_d(out, "edge",  1e-10,  53, 53, MPFR_RNDU);
        emit_d(out, "edge", -1e-10,  53, 53, MPFR_RNDD);

        /* prec(r) < width(int(u)) at non-tie. */
        emit_d(out, "edge",  100.7,  53, 3, MPFR_RNDN);  /* int=101->96 (=11000000) */
        emit_d(out, "edge", -100.7,  53, 3, MPFR_RNDN);
        emit_d(out, "edge",  4.7,    53, 2, MPFR_RNDN);  /* int=5 -> nearest@p2 of {4,6}; tie even=4 */
        emit_d(out, "edge",  4.4,    53, 2, MPFR_RNDN);  /* int=4 -> 4 (no round) */
    }

    /* ============================================================== */
    /* adversarial: ~22 -- RNDN-vs-RNDNA distinguishing; tie cases     */
    /* ============================================================== */
    {
        /* The big one: 2.5 -- distinguishes RNDN ties-to-even from
         * mpfr_round's RNDNA ties-away. At prec=53, integer rep is
         * fine: RNDN -> 2 (even), RNDNA -> 3.
         * Also covered: 0.5, 1.5, 3.5 at full prec. */
        emit_str_bin(out, "adversarial",  "1.01E1",  53, 53, MPFR_RNDN); /* 2.5 -> 2 */
        emit_str_bin(out, "adversarial",  "1.1E1",   53, 53, MPFR_RNDN); /* 3 (integer) */
        emit_str_bin(out, "adversarial",  "1.1E0",   53, 53, MPFR_RNDN); /* 1.5 -> 2 */
        emit_str_bin(out, "adversarial",  "1.11E1",  53, 53, MPFR_RNDN); /* 3.5 -> 4 */
        emit_str_bin(out, "adversarial",  "1.001E2", 53, 53, MPFR_RNDN); /* 4.5 -> 4 */
        emit_str_bin(out, "adversarial",  "1.011E2", 53, 53, MPFR_RNDN); /* 5.5 -> 6 */
        emit_str_bin(out, "adversarial",  "1.101E2", 53, 53, MPFR_RNDN); /* 6.5 -> 6 */
        emit_str_bin(out, "adversarial",  "1.111E2", 53, 53, MPFR_RNDN); /* 7.5 -> 8 */
        /* Negatives -- same magnitude rule, sign preserved. */
        emit_str_bin(out, "adversarial", "-1.01E1",  53, 53, MPFR_RNDN); /* -2.5 -> -2 */
        emit_str_bin(out, "adversarial", "-1.11E1",  53, 53, MPFR_RNDN); /* -3.5 -> -4 */

        /* The other big one: prec(r)=2 forces the tie at the prec
         * boundary -- u=5, prec=2: dropped LSB is '1', truncated mant
         * '10' is even -> stay even (4). u=7, prec=2: dropped '1',
         * truncated '11' odd -> increment '100', carry to '10'@exp=3 = 4
         * actually wait: increment 11 to 100 overflows 2-bit mant; the
         * carry-out renormalises to mant=10 exp=3 -> value=4*2=8. */
        emit_d(out, "adversarial",  5.0,  53, 2, MPFR_RNDN);  /* -> 4 (tie even) */
        emit_d(out, "adversarial",  7.0,  53, 2, MPFR_RNDN);  /* -> 8 (tie, odd LSB up) */
        emit_d(out, "adversarial", -5.0,  53, 2, MPFR_RNDN);
        emit_d(out, "adversarial", -7.0,  53, 2, MPFR_RNDN);

        /* Adversarial across rnds for same value -- 5.0 at prec=2:
         * RNDZ -> 4 (toward 0); RNDU -> 6; RNDD -> 4; RNDA -> 6;
         * RNDN -> 4 (tie even). */
        emit_d(out, "adversarial", 5.0, 53, 2, MPFR_RNDZ);
        emit_d(out, "adversarial", 5.0, 53, 2, MPFR_RNDU);
        emit_d(out, "adversarial", 5.0, 53, 2, MPFR_RNDD);
        emit_d(out, "adversarial", 5.0, 53, 2, MPFR_RNDA);

        /* prec(r)=1 super-low rounding. u=3 at prec=1: trunc top bit=1
         * exp=2 -> 2; dropped '1' is tie (rest 0), truncated mant '1'
         * odd LSB -> increment to '10', carry to '1' exp=3 -> 4. */
        emit_d(out, "adversarial",  3.0, 53, 1, MPFR_RNDN);  /* -> 4 */
        emit_d(out, "adversarial", -3.0, 53, 1, MPFR_RNDN);

        /* Mined-style adversarial: from trint.c L67-L74 -- the prec=3 x
         * "1.11E0" = 1.75 at prec(r)=2 RNDU. Result expected: 1.0E1 = 2
         * (but with set_emax(1) in C this overflows; we don't set emax
         * so result is just 2 exactly). */
        emit_str_bin(out, "adversarial", "1.11E0", 3, 2, MPFR_RNDU); /* -> 2 */
    }

    /* ============================================================== */
    /* fuzz: 60 -- xorshift-driven random u / prec / rnd               */
    /* ============================================================== */
    {
        xs64_t rng;
        xs64_seed(&rng, 0x5EED1234ABCDEF01ULL);
        const uint64_t precs[6] = { 1, 2, 53, 64, 100, 200 };

        int emitted = 0;
        /* 30: |u| > 1 with non-integer fractional bias. Whole part in
         * [1, 65536) so the integer trunc has 1..17 significant bits;
         * combined with prec(r) in {1,2,53,...} this hits both exact
         * (prec(r) >= width) and rounding regimes. */
        while (emitted < 30) {
            const uint64_t r1 = xs64_next(&rng);
            const uint64_t r2 = xs64_next(&rng);
            const double whole = (double)(1 + (r1 % 65535));
            const double frac = ((double)(r2 | 1) / 18446744073709551616.0);
            const int neg = (xs64_next(&rng) & 1) ? -1 : 1;
            const double d = neg * (whole + frac);

            const uint64_t srcp = precs[xs64_below(&rng, 6)];
            const uint64_t dstp = precs[xs64_below(&rng, 6)];
            const mpfr_rnd_t rnd = RNDS[xs64_below(&rng, 5)];

            mpfr_t u;
            init_from_double(u, d, srcp);
            emit_case(out, "fuzz", u, dstp, rnd);
            mpfr_clear(u);
            emitted++;
        }
        /* 30: wide-range random doubles -- includes |u|<1, large
         * magnitudes, and the integer fast path. Skip NaN/Inf-encoded
         * bit patterns; explicit edge coverage handles those. */
        while (emitted < 60) {
            const uint64_t bits = xs64_next(&rng);
            const uint64_t exp_bits = (bits >> 52) & 0x7FF;
            if (exp_bits == 0x7FF) continue;

            double d;
            memcpy(&d, &bits, sizeof d);

            const uint64_t srcp = precs[xs64_below(&rng, 6)];
            const uint64_t dstp = precs[xs64_below(&rng, 6)];
            const mpfr_rnd_t rnd = RNDS[xs64_below(&rng, 5)];

            mpfr_t u;
            init_from_double(u, d, srcp);
            emit_case(out, "fuzz", u, dstp, rnd);
            mpfr_clear(u);
            emitted++;
        }
    }

    /* ============================================================== */
    /* mined: 8 -- from mpfr/tests/trint.c (explicit mpfr_rint calls)  */
    /* ============================================================== */
    {
        /* trint.c L38-L40: rint(NaN, RNDN) -> NaN. */
        {
            mpfr_t u; init_nan(u, 53);
            emit_case(out, "mined", u, 53, MPFR_RNDN);
            mpfr_clear(u);
        }
        /* trint.c L42-L44: rint(+Inf, RNDN) -> +Inf. */
        {
            mpfr_t u; init_pos_inf(u, 53);
            emit_case(out, "mined", u, 53, MPFR_RNDN);
            mpfr_clear(u);
        }
        /* trint.c L46-L48: rint(-Inf, RNDN) -> -Inf. */
        {
            mpfr_t u; init_neg_inf(u, 53);
            emit_case(out, "mined", u, 53, MPFR_RNDN);
            mpfr_clear(u);
        }
        /* trint.c L50-L52: rint(+0, RNDN) -> +0 (positive zero). */
        {
            mpfr_t u; init_pos_zero(u, 53);
            emit_case(out, "mined", u, 53, MPFR_RNDN);
            mpfr_clear(u);
        }
        /* trint.c L54-L57: rint(-0, RNDN) -> -0 (negative zero). */
        {
            mpfr_t u; init_neg_zero(u, 53);
            emit_case(out, "mined", u, 53, MPFR_RNDN);
            mpfr_clear(u);
        }
        /* trint.c L59-L64 (coverage test): prec(x)=2, x = 2^GMP_NUMB_BITS,
         * rint(y, x, RNDN) at prec(y)=default. We use prec(y)=53 to
         * mirror common practice. x = 2^64 is an exact integer at all
         * relevant precisions. */
        {
            mpfr_t u; mpfr_init2(u, 2);
            mpfr_set_ui(u, 1, MPFR_RNDN);
            mpfr_mul_2ui(u, u, 64, MPFR_RNDN);
            emit_case(out, "mined", u, 53, MPFR_RNDN);
            mpfr_clear(u);
        }
        /* trint.c L83-L89: prec(x)=53, x = "0.10101100000000101001
         * 010101111111000000011111010000010E-1" (a small fraction),
         * rint(y, x, RNDU) -> 1 and rint(y, x, RNDD) -> +0. We emit
         * the RNDU case; the RNDD case is structurally identical and
         * covered by adjacent edge cases. */
        emit_str_bin(out, "mined",
            "0.10101100000000101001010101111111000000011111010000010E-1",
            53, 53, MPFR_RNDU);
        /* trint.c L98-L103: prec(x)=39, prec(y)=29, x = "-0.100010110
         * 100011010001111001001001100111E39" (large signed integer at
         * a high exponent), rint(y, x, RNDN) -> y with the bottom bits
         * rounded. */
        emit_str_bin(out, "mined",
            "-0.100010110100011010001111001001001100111E39",
            39, 29, MPFR_RNDN);
    }

    return 0;
}
