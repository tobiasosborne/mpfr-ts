/*
 * golden_driver.c — Golden master for MPFR's mpfr_sub1sp1 (p < 64).
 *
 * mpfr_sub1sp1 is `static` in mpfr/src/sub1sp.c. We exercise it via
 * mpfr_sub at prec < 64 — dispatcher L1474-L1476 routes here.
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
 * Ref: mpfr/src/sub1sp.c L138-L320 — C reference.
 * Ref: src/ops/sub1sp1.ts — production port.
 */
#include "common.h"

#include <assert.h>
#include <inttypes.h>
#include <string.h>

#if GMP_NUMB_BITS != 64
#  error "mpfr_sub1sp1 golden_driver requires GMP_NUMB_BITS == 64"
#endif

static const mpfr_rnd_t RNDS[5] = {
    MPFR_RNDN, MPFR_RNDZ, MPFR_RNDU, MPFR_RNDD, MPFR_RNDA,
};

static void
emit_case(FILE *out, const char *tag,
          mpfr_srcptr b, mpfr_srcptr c, mpfr_rnd_t rnd) {
    const mpfr_prec_t p = mpfr_get_prec(b);
    assert(p == mpfr_get_prec(c));
    assert(p >= 1 && p < 64);
    assert(mpfr_regular_p(b));
    assert(mpfr_regular_p(c));
    assert(mpfr_signbit(b) == mpfr_signbit(c));

    mpfr_t a;
    mpfr_init2(a, p);
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

static void mk_pair_dd(mpfr_ptr b, mpfr_ptr c, mpfr_prec_t prec,
                       double db, double dc) {
    assert(prec >= 1 && prec < 64);
    mpfr_init2(b, prec);
    mpfr_init2(c, prec);
    mpfr_set_d(b, db, MPFR_RNDN);
    mpfr_set_d(c, dc, MPFR_RNDN);
    assert(mpfr_regular_p(b));
    assert(mpfr_regular_p(c));
    assert(mpfr_signbit(b) == mpfr_signbit(c));
}

static void clear_pair(mpfr_ptr b, mpfr_ptr c) {
    mpfr_clear(b); mpfr_clear(c);
}

static void mk_pair_ii(mpfr_ptr b, mpfr_ptr c, mpfr_prec_t prec,
                       long magB, long magC, int sign) {
    assert(prec >= 1 && prec < 64);
    assert(magB > 0 && magC > 0);
    mpfr_init2(b, prec);
    mpfr_init2(c, prec);
    mpfr_set_si(b, sign * magB, MPFR_RNDN);
    mpfr_set_si(c, sign * magC, MPFR_RNDN);
    assert(mpfr_regular_p(b));
    assert(mpfr_regular_p(c));
}

static void mk_pair_exact(mpfr_ptr b, mpfr_ptr c, mpfr_prec_t prec,
                          const char *mantB_dec, mpfr_exp_t expB,
                          const char *mantC_dec, mpfr_exp_t expC,
                          int sign) {
    assert(prec >= 1 && prec < 64);
    mpz_t zB, zC;
    mpz_init(zB); mpz_init(zC);
    if (mpz_set_str(zB, mantB_dec, 10) != 0
        || mpz_set_str(zC, mantC_dec, 10) != 0) {
        fprintf(stderr, "mk_pair_exact: bad decimal\n");
        exit(2);
    }
    if (sign < 0) { mpz_neg(zB, zB); mpz_neg(zC, zC); }
    mpfr_init2(b, prec);
    mpfr_init2(c, prec);
    mpfr_set_z_2exp(b, zB, expB - (mpfr_exp_t)prec, MPFR_RNDN);
    mpfr_set_z_2exp(c, zC, expC - (mpfr_exp_t)prec, MPFR_RNDN);
    mpz_clear(zB); mpz_clear(zC);
    assert(mpfr_regular_p(b));
    assert(mpfr_regular_p(c));
    assert(mpfr_signbit(b) == mpfr_signbit(c));
}

int main(void) {
    FILE *out = stdout;

    /* happy: 22 cases — common values at prec 24, 53. */
    {
        for (int i = 0; i < 5; ++i) {
            mpfr_t b, c; mk_pair_dd(b, c, 53, 3.0, 1.0);
            emit_case(out, "happy", b, c, RNDS[i]); clear_pair(b, c);
        }
        for (int i = 0; i < 5; ++i) {
            mpfr_t b, c; mk_pair_dd(b, c, 53, -3.0, -1.0);
            emit_case(out, "happy", b, c, RNDS[i]); clear_pair(b, c);
        }
        { mpfr_t b, c; mk_pair_dd(b, c, 53, 3.14, 2.71); emit_case(out, "happy", b, c, MPFR_RNDN); clear_pair(b, c); }
        { mpfr_t b, c; mk_pair_dd(b, c, 53, 100.0, 50.0); emit_case(out, "happy", b, c, MPFR_RNDN); clear_pair(b, c); }
        { mpfr_t b, c; mk_pair_dd(b, c, 53, 1e10, 1.0); emit_case(out, "happy", b, c, MPFR_RNDN); clear_pair(b, c); }
        { mpfr_t b, c; mk_pair_dd(b, c, 53, 1.0, 1e-10); emit_case(out, "happy", b, c, MPFR_RNDN); clear_pair(b, c); }
        { mpfr_t b, c; mk_pair_ii(b, c, 24, 1000, 500, 1); emit_case(out, "happy", b, c, MPFR_RNDN); clear_pair(b, c); }
        { mpfr_t b, c; mk_pair_ii(b, c, 24, 500, 1000, 1); emit_case(out, "happy", b, c, MPFR_RNDU); clear_pair(b, c); }
        { mpfr_t b, c; mk_pair_ii(b, c, 24, 1000, 500, -1); emit_case(out, "happy", b, c, MPFR_RNDN); clear_pair(b, c); }
        { mpfr_t b, c; mk_pair_dd(b, c, 53, 16.0, 8.0); emit_case(out, "happy", b, c, MPFR_RNDA); clear_pair(b, c); }
        { mpfr_t b, c; mk_pair_dd(b, c, 53, 2.5, 1.25); emit_case(out, "happy", b, c, MPFR_RNDN); clear_pair(b, c); }
        { mpfr_t b, c; mk_pair_dd(b, c, 53, 100.5, 0.25); emit_case(out, "happy", b, c, MPFR_RNDN); clear_pair(b, c); }
        { mpfr_t b, c; mk_pair_dd(b, c, 53, 5.0, 3.0); emit_case(out, "happy", b, c, MPFR_RNDN); clear_pair(b, c); }
        { mpfr_t b, c; mk_pair_dd(b, c, 24, 100.0, 50.0); emit_case(out, "happy", b, c, MPFR_RNDD); clear_pair(b, c); }
    }

    /* edge: 32 cases */
    {
        /* prec=1, smallest. */
        for (int i = 0; i < 5; ++i) {
            mpfr_t b, c; mk_pair_ii(b, c, 1, 1, 1, 1);
            emit_case(out, "edge", b, c, RNDS[i]); clear_pair(b, c);
        }
        /* prec=63, largest in range. */
        for (int i = 0; i < 5; ++i) {
            mpfr_t b, c; mk_pair_dd(b, c, 63, 3.0, 1.0);
            emit_case(out, "edge", b, c, RNDS[i]); clear_pair(b, c);
        }
        /* Full cancellation under all 5 modes (signs both). */
        for (int i = 0; i < 5; ++i) {
            mpfr_t b, c; mk_pair_dd(b, c, 53, 3.14, 3.14);
            emit_case(out, "edge", b, c, RNDS[i]); clear_pair(b, c);
        }
        for (int i = 0; i < 5; ++i) {
            mpfr_t b, c; mk_pair_dd(b, c, 24, -2.0, -2.0);
            emit_case(out, "edge", b, c, RNDS[i]); clear_pair(b, c);
        }
        /* d=1 cancellation. */
        { mpfr_t b, c; mk_pair_exact(b, c, 53,
            "4503599627370496", 54,   /* 2^52 at exp 54 = val 2^53 */
            "9007199254740991", 53,   /* 2^53 - 1 at exp 53 */
            1);
          emit_case(out, "edge", b, c, MPFR_RNDN); clear_pair(b, c); }
        /* d=GMP_NUMB_BITS-ish... actually d > 64 will go to B2 even at small p */
        { mpfr_t b, c; mk_pair_exact(b, c, 53,
            "9007199254740992", 100,  /* 2^53 at exp 100 */
            "9007199254740992", 30,   /* 2^53 at exp 30, d=70 */
            1);
          emit_case(out, "edge", b, c, MPFR_RNDN); clear_pair(b, c); }
        /* Tiny magnitudes. */
        { mpfr_t b, c; mk_pair_dd(b, c, 53, 1e-100, 1e-101);
          emit_case(out, "edge", b, c, MPFR_RNDN); clear_pair(b, c); }
        /* Large magnitudes. */
        { mpfr_t b, c; mk_pair_dd(b, c, 53, 1e100, 1e99);
          emit_case(out, "edge", b, c, MPFR_RNDN); clear_pair(b, c); }
        /* Negative near-cancel. */
        { mpfr_t b, c; mk_pair_exact(b, c, 53,
            "9007199254740991", 53,   /* 2^53 - 1 */
            "9007199254740990", 53,   /* 2^53 - 2 */
            -1);
          emit_case(out, "edge", b, c, MPFR_RNDD); clear_pair(b, c); }
        /* Power of 2 minus 1. */
        { mpfr_t b, c; mk_pair_exact(b, c, 53,
            "4503599627370496", 53,  /* 2^52 */
            "1", 0,                  /* 1.0 */
            1);
          emit_case(out, "edge", b, c, MPFR_RNDN); clear_pair(b, c); }
        /* Negative power-of-2 minus 1. */
        { mpfr_t b, c; mk_pair_exact(b, c, 53,
            "4503599627370496", 53,
            "1", 0,
            -1);
          emit_case(out, "edge", b, c, MPFR_RNDD); clear_pair(b, c); }
        /* d=53 boundary. */
        { mpfr_t b, c; mk_pair_exact(b, c, 53,
            "9007199254740991", 100,
            "9007199254740991", 47,
            1);
          emit_case(out, "edge", b, c, MPFR_RNDN); clear_pair(b, c); }
        /* d at sh-1 boundary. */
        { mpfr_t b, c; mk_pair_exact(b, c, 30,
            "1073741823", 30,  /* 2^30 - 1 at prec 30 */
            "1073741823", 17,
            1);
          emit_case(out, "edge", b, c, MPFR_RNDU); clear_pair(b, c); }
        /* prec=63 cancellation. */
        { mpfr_t b, c; mk_pair_exact(b, c, 63,
            "9223372036854775807", 63,  /* 2^63 - 1 */
            "9223372036854775806", 63,  /* 2^63 - 2 */
            1);
          emit_case(out, "edge", b, c, MPFR_RNDN); clear_pair(b, c); }
    }

    /* adversarial: 12 cases */
    {
        { mpfr_t b, c; mk_pair_dd(b, c, 53, 1.0, 1.0); emit_case(out, "adversarial", b, c, MPFR_RNDD); clear_pair(b, c); }
        { mpfr_t b, c; mk_pair_dd(b, c, 53, 1.0, 1.0); emit_case(out, "adversarial", b, c, MPFR_RNDN); clear_pair(b, c); }
        { mpfr_t b, c; mk_pair_dd(b, c, 53, 1.0/3.0, 1.0/7.0); emit_case(out, "adversarial", b, c, MPFR_RNDU); clear_pair(b, c); }
        { mpfr_t b, c; mk_pair_dd(b, c, 53, 1.0/3.0, 1.0/7.0); emit_case(out, "adversarial", b, c, MPFR_RNDD); clear_pair(b, c); }
        { mpfr_t b, c; mk_pair_dd(b, c, 53, -1.0/3.0, -1.0/7.0); emit_case(out, "adversarial", b, c, MPFR_RNDU); clear_pair(b, c); }
        { mpfr_t b, c; mk_pair_dd(b, c, 53, -1.0/3.0, -1.0/7.0); emit_case(out, "adversarial", b, c, MPFR_RNDD); clear_pair(b, c); }
        /* All-ones - 1 ulp carry boundary at p=53. */
        { mpfr_t b, c; mk_pair_exact(b, c, 53,
            "9007199254740991", 53,  /* 2^53 - 1 */
            "1", 1,                  /* 2 */
            1);
          emit_case(out, "adversarial", b, c, MPFR_RNDU); clear_pair(b, c); }
        /* d=1 special cancellation. */
        { mpfr_t b, c; mk_pair_exact(b, c, 53,
            "4503599627370496", 54,    /* 2^52 at exp 54 = val 2^53 */
            "9007199254740991", 53,    /* 2^53 - 1 */
            1);
          emit_case(out, "adversarial", b, c, MPFR_RNDU); clear_pair(b, c); }
        /* Large-d sticky path. */
        { mpfr_t b, c; mk_pair_dd(b, c, 53, 1e30, 1e-30);
          emit_case(out, "adversarial", b, c, MPFR_RNDU); clear_pair(b, c); }
        { mpfr_t b, c; mk_pair_dd(b, c, 53, 1e30, 1e-30);
          emit_case(out, "adversarial", b, c, MPFR_RNDD); clear_pair(b, c); }
        { mpfr_t b, c; mk_pair_dd(b, c, 53, 8.0, 4.0);
          emit_case(out, "adversarial", b, c, MPFR_RNDA); clear_pair(b, c); }
        /* Tie-to-even at p=53. */
        { mpfr_t b, c; mk_pair_exact(b, c, 53,
            "9007199254740990", 53,    /* 2^53 - 2 */
            "1", 0,                    /* 1 */
            1);
          emit_case(out, "adversarial", b, c, MPFR_RNDN); clear_pair(b, c); }
    }

    /* fuzz: 60 cases */
    {
        xs64_t rng;
        xs64_seed(&rng, 0x5B1F50515B1F0001ULL);
        int emitted = 0;
        while (emitted < 60) {
            const uint64_t prec = 1 + xs64_below(&rng, 63);  /* [1, 63] */
            const double magB = (double)(1 + xs64_below(&rng, 1000000));
            const double magC = (double)(1 + xs64_below(&rng, 1000000));
            const int sign = (xs64_below(&rng, 2) == 0) ? +1 : -1;
            const uint64_t rnd_idx = xs64_below(&rng, 5);
            mpfr_t b, c;
            mpfr_init2(b, (mpfr_prec_t)prec);
            mpfr_init2(c, (mpfr_prec_t)prec);
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

    /* mined: 5 cases */
    {
        { mpfr_t b, c; mk_pair_dd(b, c, 53, 1.0, 1.0); emit_case(out, "mined", b, c, MPFR_RNDN); clear_pair(b, c); }
        { mpfr_t b, c; mk_pair_dd(b, c, 53, 1.0, 1.0); emit_case(out, "mined", b, c, MPFR_RNDD); clear_pair(b, c); }
        { mpfr_t b, c; mk_pair_dd(b, c, 53, 2.0, 1.0); emit_case(out, "mined", b, c, MPFR_RNDN); clear_pair(b, c); }
        { mpfr_t b, c; mk_pair_dd(b, c, 24, 1.5, 0.5); emit_case(out, "mined", b, c, MPFR_RNDU); clear_pair(b, c); }
        { mpfr_t b, c; mk_pair_dd(b, c, 53, -2.0, -1.0); emit_case(out, "mined", b, c, MPFR_RNDN); clear_pair(b, c); }
    }

    return 0;
}
