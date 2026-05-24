/*
 * golden_driver.c — Golden master for MPFR's mpfr_sub1sp1n (p == 64).
 *
 * mpfr_sub1sp1n is `static` in mpfr/src/sub1sp.c. We exercise it
 * indirectly via mpfr_sub with prec == 64 on both operands (and the
 * dispatcher routes prec==64 same-sign-difference here per L1481-L1484).
 * Inputs: b.prec == c.prec == 64, b.sign == c.sign, both normal.
 *
 * Wire format
 * -----------
 *
 *   {"tag":"<class>",
 *    "inputs":{"b":<MPFR>,"c":<MPFR>,"rnd":"RND[NZUDA]"},
 *    "output":{"value":<MPFR>,"ternary":<-1|0|1>},
 *    "time_ns":<n>}
 *
 * Tag distribution (Rule 7 minimums)
 * ----------------------------------
 *
 *   happy        :  22
 *   edge         :  32
 *   adversarial  :  12
 *   fuzz         :  60
 *   mined        :   5
 *
 * Ref: mpfr/src/sub1sp.c L325-L510 — C reference.
 * Ref: src/ops/sub1sp1n.ts — production port.
 */
#include "common.h"

#include <assert.h>
#include <inttypes.h>
#include <string.h>

#if GMP_NUMB_BITS != 64
#  error "mpfr_sub1sp1n golden_driver requires GMP_NUMB_BITS == 64"
#endif

static const mpfr_rnd_t RNDS[5] = {
    MPFR_RNDN, MPFR_RNDZ, MPFR_RNDU, MPFR_RNDD, MPFR_RNDA,
};

/* Emit one case at (b, c, rnd). Pre-cond: b.prec == c.prec == 64,
 * b.sign == c.sign, both kind='normal'. */
static void
emit_case(FILE *out, const char *tag,
          mpfr_srcptr b, mpfr_srcptr c, mpfr_rnd_t rnd) {
    assert(mpfr_get_prec(b) == 64);
    assert(mpfr_get_prec(c) == 64);
    assert(mpfr_regular_p(b));
    assert(mpfr_regular_p(c));
    assert(mpfr_signbit(b) == mpfr_signbit(c));

    mpfr_t a;
    mpfr_init2(a, 64);

    const uint64_t t0 = now_ns();
    const int ternary = mpfr_sub(a, b, c, rnd);
    const uint64_t elapsed = now_ns() - t0;

    jl_begin(out, tag);
    jl_kv_mpfr(out, 1, "b", b);
    jl_kv_mpfr(out, 0, "c", c);
    jl_kv_rnd(out, 0, "rnd", rnd);
    jl_end_inputs(out);
    jl_output_result(out, a, ternary);
    jl_finish(out, elapsed);

    mpfr_clear(a);
}

/* Build a same-prec-64, same-sign pair (b, c) from doubles. */
static void mk_pair_dd(mpfr_ptr b, mpfr_ptr c,
                       double db, double dc) {
    mpfr_init2(b, 64);
    mpfr_init2(c, 64);
    mpfr_set_d(b, db, MPFR_RNDN);
    mpfr_set_d(c, dc, MPFR_RNDN);
    assert(mpfr_regular_p(b));
    assert(mpfr_regular_p(c));
    assert(mpfr_signbit(b) == mpfr_signbit(c));
}

static void clear_pair(mpfr_ptr b, mpfr_ptr c) {
    mpfr_clear(b); mpfr_clear(c);
}

/* Build (b, c) from integer magnitudes at prec=64, with sign. */
static void mk_pair_ii(mpfr_ptr b, mpfr_ptr c,
                       long magB, long magC, int sign) {
    assert(magB > 0 && magC > 0);
    mpfr_init2(b, 64);
    mpfr_init2(c, 64);
    mpfr_set_si(b, sign * magB, MPFR_RNDN);
    mpfr_set_si(c, sign * magC, MPFR_RNDN);
    assert(mpfr_regular_p(b));
    assert(mpfr_regular_p(c));
}

/* Build (b, c) with exact bit-pattern mantissas at prec=64. */
static void mk_pair_exact(mpfr_ptr b, mpfr_ptr c,
                          const char *mantB_dec, mpfr_exp_t expB,
                          const char *mantC_dec, mpfr_exp_t expC,
                          int sign) {
    mpz_t zB, zC;
    mpz_init(zB); mpz_init(zC);
    if (mpz_set_str(zB, mantB_dec, 10) != 0
        || mpz_set_str(zC, mantC_dec, 10) != 0) {
        fprintf(stderr, "mk_pair_exact: bad decimal\n");
        exit(2);
    }
    if (sign < 0) { mpz_neg(zB, zB); mpz_neg(zC, zC); }
    mpfr_init2(b, 64);
    mpfr_init2(c, 64);
    mpfr_set_z_2exp(b, zB, expB - 64, MPFR_RNDN);
    mpfr_set_z_2exp(c, zC, expC - 64, MPFR_RNDN);
    mpz_clear(zB); mpz_clear(zC);
    assert(mpfr_regular_p(b));
    assert(mpfr_regular_p(c));
    assert(mpfr_signbit(b) == mpfr_signbit(c));
}

int main(void) {
    FILE *out = stdout;

    /* ============================================================== */
    /* happy: 22 cases                                                */
    /* ============================================================== */
    {
        { mpfr_t b, c; mk_pair_dd(b, c, 5.0, 3.0); emit_case(out, "happy", b, c, MPFR_RNDN); clear_pair(b, c); }
        { mpfr_t b, c; mk_pair_dd(b, c, 3.0, 5.0); emit_case(out, "happy", b, c, MPFR_RNDN); clear_pair(b, c); }
        { mpfr_t b, c; mk_pair_dd(b, c, 7.0, 2.5); emit_case(out, "happy", b, c, MPFR_RNDN); clear_pair(b, c); }
        { mpfr_t b, c; mk_pair_dd(b, c, -7.0, -2.5); emit_case(out, "happy", b, c, MPFR_RNDN); clear_pair(b, c); }
        { mpfr_t b, c; mk_pair_dd(b, c, 1e10, 1.0); emit_case(out, "happy", b, c, MPFR_RNDN); clear_pair(b, c); }
        { mpfr_t b, c; mk_pair_dd(b, c, 1.0, 1e10); emit_case(out, "happy", b, c, MPFR_RNDN); clear_pair(b, c); }
        { mpfr_t b, c; mk_pair_dd(b, c, 3.14, 2.71); emit_case(out, "happy", b, c, MPFR_RNDZ); clear_pair(b, c); }
        { mpfr_t b, c; mk_pair_dd(b, c, -3.14, -2.71); emit_case(out, "happy", b, c, MPFR_RNDD); clear_pair(b, c); }
        { mpfr_t b, c; mk_pair_ii(b, c, 100, 50, 1); emit_case(out, "happy", b, c, MPFR_RNDN); clear_pair(b, c); }
        { mpfr_t b, c; mk_pair_ii(b, c, 50, 100, 1); emit_case(out, "happy", b, c, MPFR_RNDU); clear_pair(b, c); }
        { mpfr_t b, c; mk_pair_ii(b, c, 100, 50, -1); emit_case(out, "happy", b, c, MPFR_RNDD); clear_pair(b, c); }
        /* All 5 modes on same input. */
        for (int i = 0; i < 5; ++i) {
            mpfr_t b, c; mk_pair_dd(b, c, 8.0, 3.0);
            emit_case(out, "happy", b, c, RNDS[i]); clear_pair(b, c);
        }
        { mpfr_t b, c; mk_pair_dd(b, c, 1.0, 0.5); emit_case(out, "happy", b, c, MPFR_RNDN); clear_pair(b, c); }
        { mpfr_t b, c; mk_pair_dd(b, c, 4.0, 1.0); emit_case(out, "happy", b, c, MPFR_RNDN); clear_pair(b, c); }
        { mpfr_t b, c; mk_pair_dd(b, c, 16.0, 1.0); emit_case(out, "happy", b, c, MPFR_RNDA); clear_pair(b, c); }
        { mpfr_t b, c; mk_pair_dd(b, c, 100.5, 100.25); emit_case(out, "happy", b, c, MPFR_RNDN); clear_pair(b, c); }
        { mpfr_t b, c; mk_pair_dd(b, c, -100.5, -100.25); emit_case(out, "happy", b, c, MPFR_RNDN); clear_pair(b, c); }
        { mpfr_t b, c; mk_pair_dd(b, c, 0.5, 0.5); emit_case(out, "happy", b, c, MPFR_RNDN); clear_pair(b, c); }
    }

    /* ============================================================== */
    /* edge: 32 cases — cancellation, exponent boundaries             */
    /* ============================================================== */
    {
        /* Full cancellation (a == b): expect zero per rnd mode. */
        for (int i = 0; i < 5; ++i) {
            mpfr_t b, c; mk_pair_dd(b, c, 3.14, 3.14);
            emit_case(out, "edge", b, c, RNDS[i]); clear_pair(b, c);
        }
        /* Full cancellation, negative inputs (all 5 modes). */
        for (int i = 0; i < 5; ++i) {
            mpfr_t b, c; mk_pair_dd(b, c, -3.14, -3.14);
            emit_case(out, "edge", b, c, RNDS[i]); clear_pair(b, c);
        }
        /* Near-cancellation, d=0, |b| slightly larger. */
        { mpfr_t b, c; mk_pair_exact(b, c,
            "9223372036854775808", 64,  /* 2^63 */
            "9223372036854775807", 64,  /* 2^63 - 1 */
            1);
          emit_case(out, "edge", b, c, MPFR_RNDN); clear_pair(b, c); }
        /* Near-cancellation, |c| slightly larger. */
        { mpfr_t b, c; mk_pair_exact(b, c,
            "9223372036854775807", 64,
            "9223372036854775808", 64,
            1);
          emit_case(out, "edge", b, c, MPFR_RNDN); clear_pair(b, c); }
        /* d = 1, special b=B/2, c=B-1 cancellation case. */
        { mpfr_t b, c; mk_pair_exact(b, c,
            "9223372036854775808", 65,  /* 2^63 at exp=65, val = 2^64 */
            "18446744073709551615", 64, /* 2^64 - 1 at exp=64 */
            1);
          emit_case(out, "edge", b, c, MPFR_RNDN); clear_pair(b, c); }
        /* d = 64 exactly (B2 boundary): b at HIGHBIT, c = HIGHBIT. */
        { mpfr_t b, c; mk_pair_exact(b, c,
            "9223372036854775808", 100,
            "9223372036854775808", 36,
            1);
          emit_case(out, "edge", b, c, MPFR_RNDN); clear_pair(b, c); }
        /* d = 64, bp0 = HIGHBIT, cp0 > HIGHBIT (case b path). */
        { mpfr_t b, c; mk_pair_exact(b, c,
            "9223372036854775808", 100,
            "18446744073709551614", 36,  /* 2^64 - 2 */
            1);
          emit_case(out, "edge", b, c, MPFR_RNDN); clear_pair(b, c); }
        /* d = 65 (case c/d). */
        { mpfr_t b, c; mk_pair_exact(b, c,
            "9223372036854775808", 100,
            "9223372036854775808", 35,
            1);
          emit_case(out, "edge", b, c, MPFR_RNDN); clear_pair(b, c); }
        { mpfr_t b, c; mk_pair_exact(b, c,
            "9223372036854775808", 100,
            "18446744073709551614", 35,
            1);
          emit_case(out, "edge", b, c, MPFR_RNDN); clear_pair(b, c); }
        /* d = 70 (case e: rb=sb=1). */
        { mpfr_t b, c; mk_pair_exact(b, c,
            "9223372036854775808", 100,
            "18446744073709551614", 30,
            1);
          emit_case(out, "edge", b, c, MPFR_RNDN); clear_pair(b, c); }
        /* Negative sign carry. */
        { mpfr_t b, c; mk_pair_dd(b, c, -8.0, -3.0);
          emit_case(out, "edge", b, c, MPFR_RNDN); clear_pair(b, c); }
        /* All-ones mantissa minus 1-ulp. */
        { mpfr_t b, c; mk_pair_exact(b, c,
            "18446744073709551615", 64,
            "1", 1,
            1);
          emit_case(out, "edge", b, c, MPFR_RNDN); clear_pair(b, c); }
        /* Both very small (subnormal-like, but no emin in default). */
        { mpfr_t b, c; mk_pair_dd(b, c, 1e-100, 1e-101);
          emit_case(out, "edge", b, c, MPFR_RNDN); clear_pair(b, c); }
        /* Both very large. */
        { mpfr_t b, c; mk_pair_dd(b, c, 1e100, 1e99);
          emit_case(out, "edge", b, c, MPFR_RNDN); clear_pair(b, c); }
        /* d=1, neg-sign exact subtraction. */
        { mpfr_t b, c; mk_pair_exact(b, c,
            "18446744073709551614", 64,  /* 2^64 - 2 */
            "9223372036854775808", 63,   /* 2^63 at exp=63 → val 2^62 */
            -1);
          emit_case(out, "edge", b, c, MPFR_RNDN); clear_pair(b, c); }
        /* d=63 just below 64. */
        { mpfr_t b, c; mk_pair_exact(b, c,
            "9223372036854775808", 70,
            "9223372036854775808", 7,
            1);
          emit_case(out, "edge", b, c, MPFR_RNDU); clear_pair(b, c); }
        { mpfr_t b, c; mk_pair_exact(b, c,
            "9223372036854775808", 70,
            "9223372036854775808", 7,
            -1);
          emit_case(out, "edge", b, c, MPFR_RNDD); clear_pair(b, c); }
        /* Additional edge cases to reach >= 30. */
        /* d = 100, rb=sb=1 case (e). */
        { mpfr_t b, c; mk_pair_exact(b, c,
            "9223372036854775808", 200,
            "18446744073709551614", 100,
            1);
          emit_case(out, "edge", b, c, MPFR_RNDZ); clear_pair(b, c); }
        /* d = 66, case (c). */
        { mpfr_t b, c; mk_pair_exact(b, c,
            "9223372036854775808", 100,
            "9223372036854775808", 34,
            1);
          emit_case(out, "edge", b, c, MPFR_RNDU); clear_pair(b, c); }
        /* d = 66, case (d). */
        { mpfr_t b, c; mk_pair_exact(b, c,
            "9223372036854775808", 100,
            "18446744073709551614", 34,
            1);
          emit_case(out, "edge", b, c, MPFR_RNDU); clear_pair(b, c); }
        /* bp0 > HIGHBIT, d = 64. */
        { mpfr_t b, c; mk_pair_exact(b, c,
            "13835058055282163712", 100,  /* 0xC000... */
            "9223372036854775808", 36,
            1);
          emit_case(out, "edge", b, c, MPFR_RNDN); clear_pair(b, c); }
        /* bp0 > HIGHBIT, d > 64. */
        { mpfr_t b, c; mk_pair_exact(b, c,
            "13835058055282163712", 100,
            "9223372036854775808", 25,
            1);
          emit_case(out, "edge", b, c, MPFR_RNDN); clear_pair(b, c); }
        /* Two identical-magnitude differently-shifted, near edge of B1. */
        { mpfr_t b, c; mk_pair_exact(b, c,
            "12297829382473034410", 64,  /* 0xAAAA... pattern */
            "12297829382473034410", 60,
            1);
          emit_case(out, "edge", b, c, MPFR_RNDN); clear_pair(b, c); }
    }

    /* ============================================================== */
    /* adversarial: 12 cases                                          */
    /* ============================================================== */
    {
        /* Tie-break to even at p=64 (LSB after subtract is the round-bit). */
        { mpfr_t b, c; mk_pair_exact(b, c,
            "18446744073709551614", 64,  /* 2^64 - 2 */
            "9223372036854775808", 1,    /* tiny */
            1);
          emit_case(out, "adversarial", b, c, MPFR_RNDN); clear_pair(b, c); }
        /* Negative tie-break. */
        { mpfr_t b, c; mk_pair_exact(b, c,
            "18446744073709551614", 64,
            "9223372036854775808", 1,
            -1);
          emit_case(out, "adversarial", b, c, MPFR_RNDN); clear_pair(b, c); }
        /* d=64, cp0 = HIGHBIT exactly (case (a) within bp0 = HIGHBIT branch). */
        { mpfr_t b, c; mk_pair_exact(b, c,
            "9223372036854775808", 100,
            "9223372036854775808", 36,
            1);
          emit_case(out, "adversarial", b, c, MPFR_RNDU); clear_pair(b, c); }
        { mpfr_t b, c; mk_pair_exact(b, c,
            "9223372036854775808", 100,
            "9223372036854775808", 36,
            1);
          emit_case(out, "adversarial", b, c, MPFR_RNDD); clear_pair(b, c); }
        /* Cancellation with rnd-mode-sensitive zero sign. */
        { mpfr_t b, c; mk_pair_dd(b, c, 1.0, 1.0);
          emit_case(out, "adversarial", b, c, MPFR_RNDD); clear_pair(b, c); }
        { mpfr_t b, c; mk_pair_dd(b, c, 1.0, 1.0);
          emit_case(out, "adversarial", b, c, MPFR_RNDN); clear_pair(b, c); }
        /* RNDU on positive: rb=1, sb=0 → add_one_ulp. */
        { mpfr_t b, c; mk_pair_exact(b, c,
            "18446744073709551615", 64,  /* 2^64 - 1, all ones */
            "9223372036854775808", 1,    /* val = 1.0 (HIGHBIT at exp 1 = 2^0) */
            1);
          emit_case(out, "adversarial", b, c, MPFR_RNDU); clear_pair(b, c); }
        /* RNDD on negative. */
        { mpfr_t b, c; mk_pair_exact(b, c,
            "18446744073709551615", 64,
            "9223372036854775808", 1,
            -1);
          emit_case(out, "adversarial", b, c, MPFR_RNDD); clear_pair(b, c); }
        /* d=2, both far apart, RNDA. */
        { mpfr_t b, c; mk_pair_dd(b, c, 4.0, 0.5);
          emit_case(out, "adversarial", b, c, MPFR_RNDA); clear_pair(b, c); }
        /* The d=1 special case (a0 == 0 trigger). */
        { mpfr_t b, c; mk_pair_exact(b, c,
            "9223372036854775808", 65,  /* 2^63 at exp 65, val = 2^64 */
            "18446744073709551615", 64, /* 2^64-1 at exp 64 */
            1);
          emit_case(out, "adversarial", b, c, MPFR_RNDU); clear_pair(b, c); }
        { mpfr_t b, c; mk_pair_exact(b, c,
            "9223372036854775808", 65,
            "18446744073709551615", 64,
            -1);
          emit_case(out, "adversarial", b, c, MPFR_RNDD); clear_pair(b, c); }
        /* d very large (> 70). */
        { mpfr_t b, c; mk_pair_exact(b, c,
            "9223372036854775808", 200,
            "9223372036854775808", 50,
            1);
          emit_case(out, "adversarial", b, c, MPFR_RNDU); clear_pair(b, c); }
    }

    /* ============================================================== */
    /* fuzz: 60 cases                                                 */
    /* ============================================================== */
    {
        xs64_t rng;
        xs64_seed(&rng, 0x5B1F50111CADD1F1ULL);
        int emitted = 0;
        while (emitted < 60) {
            const double magB = (double)(1 + xs64_below(&rng, 1000000));
            const double magC = (double)(1 + xs64_below(&rng, 1000000));
            const int sign = (xs64_below(&rng, 2) == 0) ? +1 : -1;
            const uint64_t rnd_idx = xs64_below(&rng, 5);
            mpfr_t b, c;
            mpfr_init2(b, 64);
            mpfr_init2(c, 64);
            mpfr_set_d(b, sign * magB, MPFR_RNDN);
            mpfr_set_d(c, sign * magC, MPFR_RNDN);
            if (!mpfr_regular_p(b) || !mpfr_regular_p(c)
                || mpfr_signbit(b) != mpfr_signbit(c)) {
                mpfr_clear(b); mpfr_clear(c);
                continue;
            }
            emit_case(out, "fuzz", b, c, RNDS[rnd_idx]);
            mpfr_clear(b); mpfr_clear(c);
            emitted++;
        }
    }

    /* ============================================================== */
    /* mined: 5 cases                                                 */
    /* ============================================================== */
    {
        { mpfr_t b, c; mk_pair_dd(b, c, 1.0, 1.0); emit_case(out, "mined", b, c, MPFR_RNDN); clear_pair(b, c); }
        { mpfr_t b, c; mk_pair_dd(b, c, 1.0, 1.0); emit_case(out, "mined", b, c, MPFR_RNDD); clear_pair(b, c); }
        { mpfr_t b, c; mk_pair_dd(b, c, 1.0, 0.5); emit_case(out, "mined", b, c, MPFR_RNDN); clear_pair(b, c); }
        { mpfr_t b, c; mk_pair_dd(b, c, 2.0, 1.0); emit_case(out, "mined", b, c, MPFR_RNDN); clear_pair(b, c); }
        { mpfr_t b, c; mk_pair_dd(b, c, -2.0, -1.0); emit_case(out, "mined", b, c, MPFR_RNDN); clear_pair(b, c); }
    }

    return 0;
}
