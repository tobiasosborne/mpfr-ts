/*
 * golden_driver.c — Golden master for MPFR's mpfr_add1sp1n (p == 64).
 *
 * mpfr_add1sp1n is `static` in mpfr/src/add1sp.c. We exercise it via
 * mpfr_add at prec == 64.
 *
 * Tag distribution (Rule 7 minimums)
 * ----------------------------------
 *
 *   happy        :  22
 *   edge         :  30
 *   adversarial  :  12
 *   fuzz         :  60
 *   mined        :   5
 *
 * Ref: mpfr/src/add1sp.c L256-L358 — C reference.
 * Ref: src/ops/add1sp1n.ts — production port.
 */
#include "common.h"

#include <assert.h>
#include <inttypes.h>
#include <string.h>

#if GMP_NUMB_BITS != 64
#  error "mpfr_add1sp1n golden_driver requires GMP_NUMB_BITS == 64"
#endif

static const mpfr_rnd_t RNDS[5] = {
    MPFR_RNDN, MPFR_RNDZ, MPFR_RNDU, MPFR_RNDD, MPFR_RNDA,
};

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
    const int ternary = mpfr_add(a, b, c, rnd);
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

static void mk_pair_dd(mpfr_ptr b, mpfr_ptr c, double db, double dc) {
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

static void mk_pair_ii(mpfr_ptr b, mpfr_ptr c, long magB, long magC, int sign) {
    assert(magB > 0 && magC > 0);
    mpfr_init2(b, 64);
    mpfr_init2(c, 64);
    mpfr_set_si(b, sign * magB, MPFR_RNDN);
    mpfr_set_si(c, sign * magC, MPFR_RNDN);
    assert(mpfr_regular_p(b));
    assert(mpfr_regular_p(c));
}

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
}

int main(void) {
    FILE *out = stdout;

    /* happy: 22 */
    {
        for (int i = 0; i < 5; ++i) {
            mpfr_t b, c; mk_pair_dd(b, c, 3.0, 1.0);
            emit_case(out, "happy", b, c, RNDS[i]); clear_pair(b, c);
        }
        for (int i = 0; i < 5; ++i) {
            mpfr_t b, c; mk_pair_dd(b, c, -3.0, -1.0);
            emit_case(out, "happy", b, c, RNDS[i]); clear_pair(b, c);
        }
        { mpfr_t b, c; mk_pair_dd(b, c, 3.14, 2.71); emit_case(out, "happy", b, c, MPFR_RNDN); clear_pair(b, c); }
        { mpfr_t b, c; mk_pair_dd(b, c, 100.0, 50.0); emit_case(out, "happy", b, c, MPFR_RNDN); clear_pair(b, c); }
        { mpfr_t b, c; mk_pair_dd(b, c, 1e10, 1.0); emit_case(out, "happy", b, c, MPFR_RNDN); clear_pair(b, c); }
        { mpfr_t b, c; mk_pair_dd(b, c, 1.0, 1e-10); emit_case(out, "happy", b, c, MPFR_RNDN); clear_pair(b, c); }
        { mpfr_t b, c; mk_pair_ii(b, c, 1000, 500, 1); emit_case(out, "happy", b, c, MPFR_RNDN); clear_pair(b, c); }
        { mpfr_t b, c; mk_pair_ii(b, c, 1000, 500, -1); emit_case(out, "happy", b, c, MPFR_RNDN); clear_pair(b, c); }
        { mpfr_t b, c; mk_pair_dd(b, c, 16.0, 8.0); emit_case(out, "happy", b, c, MPFR_RNDA); clear_pair(b, c); }
        { mpfr_t b, c; mk_pair_dd(b, c, 2.5, 1.25); emit_case(out, "happy", b, c, MPFR_RNDN); clear_pair(b, c); }
        { mpfr_t b, c; mk_pair_dd(b, c, 100.5, 0.25); emit_case(out, "happy", b, c, MPFR_RNDN); clear_pair(b, c); }
        { mpfr_t b, c; mk_pair_dd(b, c, 5.0, 3.0); emit_case(out, "happy", b, c, MPFR_RNDN); clear_pair(b, c); }
    }

    /* edge: 30 cases */
    {
        /* Equal exp (Case A) with all-ones, all-ones+1 mantissas — exercise overflow carry. */
        { mpfr_t b, c; mk_pair_exact(b, c,
            "18446744073709551615", 64,  /* 2^64 - 1 */
            "18446744073709551615", 64,
            1);
          emit_case(out, "edge", b, c, MPFR_RNDN); clear_pair(b, c); }
        { mpfr_t b, c; mk_pair_exact(b, c,
            "18446744073709551614", 64,  /* 2^64 - 2 */
            "1", 0,
            1);
          emit_case(out, "edge", b, c, MPFR_RNDN); clear_pair(b, c); }
        /* d=1 boundary. */
        { mpfr_t b, c; mk_pair_exact(b, c,
            "9223372036854775808", 64,  /* 2^63 = MSB-only */
            "9223372036854775808", 63,  /* 2^63 at exp 63 = same val / 2 */
            1);
          emit_case(out, "edge", b, c, MPFR_RNDN); clear_pair(b, c); }
        /* d=63 boundary. */
        { mpfr_t b, c; mk_pair_exact(b, c,
            "9223372036854775808", 100,
            "9223372036854775808", 37,
            1);
          emit_case(out, "edge", b, c, MPFR_RNDN); clear_pair(b, c); }
        /* d=64 boundary. */
        { mpfr_t b, c; mk_pair_exact(b, c,
            "9223372036854775808", 100,
            "9223372036854775808", 36,
            1);
          emit_case(out, "edge", b, c, MPFR_RNDN); clear_pair(b, c); }
        /* d=65 (d > 64). */
        { mpfr_t b, c; mk_pair_exact(b, c,
            "9223372036854775808", 100,
            "9223372036854775808", 35,
            1);
          emit_case(out, "edge", b, c, MPFR_RNDN); clear_pair(b, c); }
        /* d=64, cp=HIGHBIT (rb=1, sb=0 path). */
        { mpfr_t b, c; mk_pair_exact(b, c,
            "12345678901234567890", 200,  /* arbitrary positive < 2^64 with MSB set */
            "9223372036854775808", 136,
            1);
          emit_case(out, "edge", b, c, MPFR_RNDN); clear_pair(b, c); }
        /* RNDN tie-to-even at d=1. */
        { mpfr_t b, c; mk_pair_exact(b, c,
            "9223372036854775808", 65,  /* 2^63 at exp 65 = val 2^64 */
            "9223372036854775808", 64,  /* 2^63 at exp 64 = val 2^63 */
            1);
          emit_case(out, "edge", b, c, MPFR_RNDN); clear_pair(b, c); }
        /* Large positive */
        { mpfr_t b, c; mk_pair_dd(b, c, 1e100, 1e99); emit_case(out, "edge", b, c, MPFR_RNDU); clear_pair(b, c); }
        { mpfr_t b, c; mk_pair_dd(b, c, 1e100, 1e99); emit_case(out, "edge", b, c, MPFR_RNDD); clear_pair(b, c); }
        /* Tiny */
        { mpfr_t b, c; mk_pair_dd(b, c, 1e-100, 1e-101); emit_case(out, "edge", b, c, MPFR_RNDU); clear_pair(b, c); }
        /* Negative variants */
        { mpfr_t b, c; mk_pair_dd(b, c, -1e10, -1e9); emit_case(out, "edge", b, c, MPFR_RNDD); clear_pair(b, c); }
        { mpfr_t b, c; mk_pair_dd(b, c, -1e10, -1e9); emit_case(out, "edge", b, c, MPFR_RNDU); clear_pair(b, c); }
        /* d=2 small case. */
        { mpfr_t b, c; mk_pair_exact(b, c,
            "18446744073709551615", 64,  /* 2^64 - 1 */
            "9223372036854775808", 62,
            1);
          emit_case(out, "edge", b, c, MPFR_RNDU); clear_pair(b, c); }
        /* All-ones plus 1 = exact power of 2 (carry to bit 64). */
        { mpfr_t b, c; mk_pair_exact(b, c,
            "18446744073709551615", 64,
            "1", 0,
            1);
          emit_case(out, "edge", b, c, MPFR_RNDN); clear_pair(b, c); }
        /* RNDA on negative pair. */
        { mpfr_t b, c; mk_pair_dd(b, c, -2.5, -1.5); emit_case(out, "edge", b, c, MPFR_RNDA); clear_pair(b, c); }
        /* RNDZ on negative pair. */
        { mpfr_t b, c; mk_pair_dd(b, c, -2.5, -1.5); emit_case(out, "edge", b, c, MPFR_RNDZ); clear_pair(b, c); }
        /* Power-of-2 + 1 boundary. */
        { mpfr_t b, c; mk_pair_exact(b, c,
            "9223372036854775808", 64,
            "9223372036854775808", 1,  /* small */
            1);
          emit_case(out, "edge", b, c, MPFR_RNDN); clear_pair(b, c); }
        /* All-modes negative. */
        for (int i = 0; i < 5; ++i) {
            mpfr_t b, c; mk_pair_dd(b, c, -100.0, -50.0);
            emit_case(out, "edge", b, c, RNDS[i]); clear_pair(b, c);
        }
        /* Same-sign positive overflow boundary near emax */
        { mpfr_t b, c; mk_pair_dd(b, c, 1e10, 1e10); emit_case(out, "edge", b, c, MPFR_RNDN); clear_pair(b, c); }
        { mpfr_t b, c; mk_pair_dd(b, c, 1e10, 1e10); emit_case(out, "edge", b, c, MPFR_RNDU); clear_pair(b, c); }
        /* Equal exp, large negative. */
        { mpfr_t b, c; mk_pair_dd(b, c, -1e30, -1e30); emit_case(out, "edge", b, c, MPFR_RNDD); clear_pair(b, c); }
        /* d=1 special. */
        { mpfr_t b, c; mk_pair_exact(b, c,
            "9223372036854775808", 100,  /* 2^63 at exp 100 */
            "18446744073709551615", 99,  /* 2^64 - 1 at exp 99 */
            1);
          emit_case(out, "edge", b, c, MPFR_RNDN); clear_pair(b, c); }
        /* Tie-break at p=64 (all-ones - 1 + 1 = exact). */
        { mpfr_t b, c; mk_pair_exact(b, c,
            "18446744073709551614", 64,  /* 2^64 - 2 */
            "1", 1,                       /* 2 */
            1);
          emit_case(out, "edge", b, c, MPFR_RNDN); clear_pair(b, c); }
        /* Carry-out then more carry. */
        { mpfr_t b, c; mk_pair_exact(b, c,
            "18446744073709551615", 64,
            "18446744073709551615", 64,
            -1);
          emit_case(out, "edge", b, c, MPFR_RNDU); clear_pair(b, c); }
        /* d=63 small case. */
        { mpfr_t b, c; mk_pair_exact(b, c,
            "9223372036854775808", 64,
            "9223372036854775808", 1,  /* val = 1 */
            1);
          emit_case(out, "edge", b, c, MPFR_RNDD); clear_pair(b, c); }
    }

    /* adversarial: 12 */
    {
        /* Tie-break at p=64. */
        { mpfr_t b, c; mk_pair_exact(b, c,
            "18446744073709551614", 64,  /* 2^64 - 2 */
            "1", 0,
            1);
          emit_case(out, "adversarial", b, c, MPFR_RNDN); clear_pair(b, c); }
        { mpfr_t b, c; mk_pair_dd(b, c, 1.0, 1.0); emit_case(out, "adversarial", b, c, MPFR_RNDN); clear_pair(b, c); }
        { mpfr_t b, c; mk_pair_dd(b, c, 1.0, 1.0); emit_case(out, "adversarial", b, c, MPFR_RNDD); clear_pair(b, c); }
        { mpfr_t b, c; mk_pair_dd(b, c, 1.0/3.0, 1.0/7.0); emit_case(out, "adversarial", b, c, MPFR_RNDU); clear_pair(b, c); }
        { mpfr_t b, c; mk_pair_dd(b, c, 1.0/3.0, 1.0/7.0); emit_case(out, "adversarial", b, c, MPFR_RNDD); clear_pair(b, c); }
        { mpfr_t b, c; mk_pair_dd(b, c, -1.0/3.0, -1.0/7.0); emit_case(out, "adversarial", b, c, MPFR_RNDU); clear_pair(b, c); }
        { mpfr_t b, c; mk_pair_dd(b, c, -1.0/3.0, -1.0/7.0); emit_case(out, "adversarial", b, c, MPFR_RNDD); clear_pair(b, c); }
        /* Carry-out chain. */
        { mpfr_t b, c; mk_pair_exact(b, c,
            "18446744073709551615", 64,  /* 2^64 - 1 */
            "1", 1,                      /* 2 */
            1);
          emit_case(out, "adversarial", b, c, MPFR_RNDU); clear_pair(b, c); }
        /* d = 64 exactly with cp != HIGHBIT (rb=1, sb=1). */
        { mpfr_t b, c; mk_pair_exact(b, c,
            "18446744073709551615", 200,  /* 2^64 - 1 */
            "12345678901234567890", 136,  /* arbitrary > HIGHBIT */
            1);
          emit_case(out, "adversarial", b, c, MPFR_RNDN); clear_pair(b, c); }
        /* Sticky-path large d. */
        { mpfr_t b, c; mk_pair_dd(b, c, 1e30, 1e-30); emit_case(out, "adversarial", b, c, MPFR_RNDU); clear_pair(b, c); }
        { mpfr_t b, c; mk_pair_dd(b, c, 1e30, 1e-30); emit_case(out, "adversarial", b, c, MPFR_RNDD); clear_pair(b, c); }
        { mpfr_t b, c; mk_pair_dd(b, c, 8.0, 4.0); emit_case(out, "adversarial", b, c, MPFR_RNDA); clear_pair(b, c); }
    }

    /* fuzz: 60 */
    {
        xs64_t rng;
        xs64_seed(&rng, 0x5B1F5064016401AAULL);
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

    /* mined: 5 */
    {
        { mpfr_t b, c; mk_pair_dd(b, c, 1.0, 1.0); emit_case(out, "mined", b, c, MPFR_RNDN); clear_pair(b, c); }
        { mpfr_t b, c; mk_pair_dd(b, c, 1.0, 1.0); emit_case(out, "mined", b, c, MPFR_RNDD); clear_pair(b, c); }
        { mpfr_t b, c; mk_pair_dd(b, c, 2.0, 1.0); emit_case(out, "mined", b, c, MPFR_RNDN); clear_pair(b, c); }
        { mpfr_t b, c; mk_pair_dd(b, c, 1.5, 0.5); emit_case(out, "mined", b, c, MPFR_RNDU); clear_pair(b, c); }
        { mpfr_t b, c; mk_pair_dd(b, c, -2.0, -1.0); emit_case(out, "mined", b, c, MPFR_RNDN); clear_pair(b, c); }
    }

    return 0;
}
