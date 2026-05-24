/*
 * golden_driver.c — Golden master for MPFR's mpfr_sub1sp3 (129 <= p <= 191).
 *
 * mpfr_sub1sp3 is `static` in mpfr/src/sub1sp.c. We exercise it via
 * mpfr_sub at prec in (128, 192) — the dispatcher routes here per
 * L1487-L1488.
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
 * Ref: mpfr/src/sub1sp.c L1056-L1380 — C reference.
 * Ref: src/ops/sub1sp3.ts — production port.
 */
#include "common.h"

#include <assert.h>
#include <inttypes.h>
#include <string.h>

#if GMP_NUMB_BITS != 64
#  error "mpfr_sub1sp3 golden_driver requires GMP_NUMB_BITS == 64"
#endif

static const mpfr_rnd_t RNDS[5] = {
    MPFR_RNDN, MPFR_RNDZ, MPFR_RNDU, MPFR_RNDD, MPFR_RNDA,
};

static void
emit_case(FILE *out, const char *tag,
          mpfr_srcptr b, mpfr_srcptr c, mpfr_rnd_t rnd) {
    const mpfr_prec_t p = mpfr_get_prec(b);
    assert(p == mpfr_get_prec(c));
    assert(p > 128 && p < 192);
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
    assert(prec > 128 && prec < 192);
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
    assert(prec > 128 && prec < 192);
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
    assert(prec > 128 && prec < 192);
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

    /* happy: 22 cases */
    {
        for (int i = 0; i < 5; ++i) {
            mpfr_t b, c; mk_pair_dd(b, c, 160, 3.0, 1.0);
            emit_case(out, "happy", b, c, RNDS[i]); clear_pair(b, c);
        }
        for (int i = 0; i < 5; ++i) {
            mpfr_t b, c; mk_pair_dd(b, c, 160, -3.0, -1.0);
            emit_case(out, "happy", b, c, RNDS[i]); clear_pair(b, c);
        }
        { mpfr_t b, c; mk_pair_dd(b, c, 160, 3.14, 2.71); emit_case(out, "happy", b, c, MPFR_RNDN); clear_pair(b, c); }
        { mpfr_t b, c; mk_pair_dd(b, c, 160, 100.0, 50.0); emit_case(out, "happy", b, c, MPFR_RNDN); clear_pair(b, c); }
        { mpfr_t b, c; mk_pair_dd(b, c, 129, 1e10, 1.0); emit_case(out, "happy", b, c, MPFR_RNDN); clear_pair(b, c); }
        { mpfr_t b, c; mk_pair_dd(b, c, 191, 1e10, 1.0); emit_case(out, "happy", b, c, MPFR_RNDN); clear_pair(b, c); }
        { mpfr_t b, c; mk_pair_dd(b, c, 160, 1.0, 1e-10); emit_case(out, "happy", b, c, MPFR_RNDN); clear_pair(b, c); }
        { mpfr_t b, c; mk_pair_ii(b, c, 160, 1000, 500, 1); emit_case(out, "happy", b, c, MPFR_RNDN); clear_pair(b, c); }
        { mpfr_t b, c; mk_pair_ii(b, c, 160, 500, 1000, 1); emit_case(out, "happy", b, c, MPFR_RNDU); clear_pair(b, c); }
        { mpfr_t b, c; mk_pair_dd(b, c, 160, 16.0, 8.0); emit_case(out, "happy", b, c, MPFR_RNDA); clear_pair(b, c); }
        { mpfr_t b, c; mk_pair_dd(b, c, 160, 2.5, 1.25); emit_case(out, "happy", b, c, MPFR_RNDN); clear_pair(b, c); }
        { mpfr_t b, c; mk_pair_dd(b, c, 160, 100.5, 0.25); emit_case(out, "happy", b, c, MPFR_RNDN); clear_pair(b, c); }
        { mpfr_t b, c; mk_pair_dd(b, c, 160, 5.0, 3.0); emit_case(out, "happy", b, c, MPFR_RNDN); clear_pair(b, c); }
        { mpfr_t b, c; mk_pair_dd(b, c, 160, 100.0, 50.0); emit_case(out, "happy", b, c, MPFR_RNDD); clear_pair(b, c); }
    }

    /* edge: 30 cases */
    {
        /* p=129, smallest. */
        for (int i = 0; i < 5; ++i) {
            mpfr_t b, c; mk_pair_dd(b, c, 129, 3.0, 1.0);
            emit_case(out, "edge", b, c, RNDS[i]); clear_pair(b, c);
        }
        /* p=191, largest. */
        for (int i = 0; i < 5; ++i) {
            mpfr_t b, c; mk_pair_dd(b, c, 191, 3.0, 1.0);
            emit_case(out, "edge", b, c, RNDS[i]); clear_pair(b, c);
        }
        /* Full cancellation across all 5 modes. */
        for (int i = 0; i < 5; ++i) {
            mpfr_t b, c; mk_pair_dd(b, c, 160, 3.14, 3.14);
            emit_case(out, "edge", b, c, RNDS[i]); clear_pair(b, c);
        }
        /* d > GMP_NUMB_BITS. */
        { mpfr_t b, c; mk_pair_dd(b, c, 160, 1e30, 1.0);
          emit_case(out, "edge", b, c, MPFR_RNDN); clear_pair(b, c); }
        /* d > 2*GMP_NUMB_BITS. */
        { mpfr_t b, c; mk_pair_dd(b, c, 160, 1e60, 1.0);
          emit_case(out, "edge", b, c, MPFR_RNDN); clear_pair(b, c); }
        /* d > 3*GMP_NUMB_BITS. */
        { mpfr_t b, c; mk_pair_dd(b, c, 160, 1e90, 1.0);
          emit_case(out, "edge", b, c, MPFR_RNDN); clear_pair(b, c); }
        /* d = 64 exactly. */
        { mpfr_t b, c; mk_pair_exact(b, c, 160,
            "730750818665451459101842416358141509827966271488", 160,  /* 2^159 */
            "730750818665451459101842416358141509827966271488", 96,
            1);
          emit_case(out, "edge", b, c, MPFR_RNDN); clear_pair(b, c); }
        /* d = 128 exactly. */
        { mpfr_t b, c; mk_pair_exact(b, c, 160,
            "730750818665451459101842416358141509827966271488", 160,
            "730750818665451459101842416358141509827966271488", 32,
            1);
          emit_case(out, "edge", b, c, MPFR_RNDN); clear_pair(b, c); }
        /* Near-cancel. */
        { mpfr_t b, c; mk_pair_exact(b, c, 160,
            "1461501637330902918203684832716283019655932542975", 160,  /* 2^160 - 1 */
            "1461501637330902918203684832716283019655932542974", 160,
            1);
          emit_case(out, "edge", b, c, MPFR_RNDU); clear_pair(b, c); }
        /* Tiny magnitudes. */
        { mpfr_t b, c; mk_pair_dd(b, c, 160, 1e-100, 1e-101);
          emit_case(out, "edge", b, c, MPFR_RNDN); clear_pair(b, c); }
        /* Large magnitudes. */
        { mpfr_t b, c; mk_pair_dd(b, c, 160, 1e100, 1e99);
          emit_case(out, "edge", b, c, MPFR_RNDN); clear_pair(b, c); }
        /* Negative near-cancel. */
        { mpfr_t b, c; mk_pair_exact(b, c, 160,
            "1461501637330902918203684832716283019655932542975", 160,
            "1461501637330902918203684832716283019655932542974", 160,
            -1);
          emit_case(out, "edge", b, c, MPFR_RNDD); clear_pair(b, c); }
        /* More edges. */
        /* d=63. */
        { mpfr_t b, c; mk_pair_exact(b, c, 160,
            "730750818665451459101842416358141509827966271488", 160,
            "730750818665451459101842416358141509827966271488", 97,
            1);
          emit_case(out, "edge", b, c, MPFR_RNDN); clear_pair(b, c); }
        /* d=129. */
        { mpfr_t b, c; mk_pair_exact(b, c, 160,
            "730750818665451459101842416358141509827966271488", 200,
            "730750818665451459101842416358141509827966271488", 71,
            1);
          emit_case(out, "edge", b, c, MPFR_RNDU); clear_pair(b, c); }
        /* d=192. */
        { mpfr_t b, c; mk_pair_exact(b, c, 160,
            "730750818665451459101842416358141509827966271488", 300,
            "730750818665451459101842416358141509827966271488", 108,
            1);
          emit_case(out, "edge", b, c, MPFR_RNDU); clear_pair(b, c); }
        /* All-ones-mant. */
        { mpfr_t b, c; mk_pair_exact(b, c, 160,
            "1461501637330902918203684832716283019655932542975", 160,
            "1", 1,
            1);
          emit_case(out, "edge", b, c, MPFR_RNDU); clear_pair(b, c); }
        /* Both negative cancel. */
        { mpfr_t b, c; mk_pair_dd(b, c, 160, -3.14, -3.14);
          emit_case(out, "edge", b, c, MPFR_RNDN); clear_pair(b, c); }
        /* d=1 cancel. */
        { mpfr_t b, c; mk_pair_exact(b, c, 160,
            "730750818665451459101842416358141509827966271488", 161,
            "1461501637330902918203684832716283019655932542975", 160,
            1);
          emit_case(out, "edge", b, c, MPFR_RNDN); clear_pair(b, c); }
    }

    /* adversarial: 12 cases */
    {
        { mpfr_t b, c; mk_pair_dd(b, c, 160, 1.0, 1.0);
          emit_case(out, "adversarial", b, c, MPFR_RNDD); clear_pair(b, c); }
        { mpfr_t b, c; mk_pair_dd(b, c, 160, 1.0, 1.0);
          emit_case(out, "adversarial", b, c, MPFR_RNDN); clear_pair(b, c); }
        { mpfr_t b, c; mk_pair_dd(b, c, 160, 1.0/3.0, 1.0/7.0);
          emit_case(out, "adversarial", b, c, MPFR_RNDU); clear_pair(b, c); }
        { mpfr_t b, c; mk_pair_dd(b, c, 160, 1.0/3.0, 1.0/7.0);
          emit_case(out, "adversarial", b, c, MPFR_RNDD); clear_pair(b, c); }
        { mpfr_t b, c; mk_pair_dd(b, c, 160, -1.0/3.0, -1.0/7.0);
          emit_case(out, "adversarial", b, c, MPFR_RNDU); clear_pair(b, c); }
        { mpfr_t b, c; mk_pair_dd(b, c, 160, -1.0/3.0, -1.0/7.0);
          emit_case(out, "adversarial", b, c, MPFR_RNDD); clear_pair(b, c); }
        /* Carry on add_one_ulp (all-ones mantissa). */
        { mpfr_t b, c; mk_pair_exact(b, c, 160,
            "1461501637330902918203684832716283019655932542975", 160,
            "1", 1,
            1);
          emit_case(out, "adversarial", b, c, MPFR_RNDU); clear_pair(b, c); }
        /* d=1 boundary. */
        { mpfr_t b, c; mk_pair_exact(b, c, 160,
            "730750818665451459101842416358141509827966271488", 161,
            "1461501637330902918203684832716283019655932542975", 160,
            1);
          emit_case(out, "adversarial", b, c, MPFR_RNDN); clear_pair(b, c); }
        /* Large d, multi-limb sticky path. */
        { mpfr_t b, c; mk_pair_dd(b, c, 160, 1e40, 1e-40);
          emit_case(out, "adversarial", b, c, MPFR_RNDU); clear_pair(b, c); }
        { mpfr_t b, c; mk_pair_dd(b, c, 160, 1e40, 1e-40);
          emit_case(out, "adversarial", b, c, MPFR_RNDD); clear_pair(b, c); }
        /* Cancellation tiebreak. */
        { mpfr_t b, c; mk_pair_dd(b, c, 160, 8.0, 4.0);
          emit_case(out, "adversarial", b, c, MPFR_RNDA); clear_pair(b, c); }
        /* Negative cancellation. */
        { mpfr_t b, c; mk_pair_dd(b, c, 160, -3.14, -3.14);
          emit_case(out, "adversarial", b, c, MPFR_RNDD); clear_pair(b, c); }
    }

    /* fuzz: 60 cases */
    {
        xs64_t rng;
        xs64_seed(&rng, 0x5B1F50330311ULL);
        int emitted = 0;
        while (emitted < 60) {
            const uint64_t prec = 129 + xs64_below(&rng, 63);
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
        { mpfr_t b, c; mk_pair_dd(b, c, 160, 1.0, 1.0); emit_case(out, "mined", b, c, MPFR_RNDN); clear_pair(b, c); }
        { mpfr_t b, c; mk_pair_dd(b, c, 160, 1.0, 1.0); emit_case(out, "mined", b, c, MPFR_RNDD); clear_pair(b, c); }
        { mpfr_t b, c; mk_pair_dd(b, c, 160, 2.0, 1.0); emit_case(out, "mined", b, c, MPFR_RNDN); clear_pair(b, c); }
        { mpfr_t b, c; mk_pair_dd(b, c, 160, 1.5, 0.5); emit_case(out, "mined", b, c, MPFR_RNDU); clear_pair(b, c); }
        { mpfr_t b, c; mk_pair_dd(b, c, 160, -2.0, -1.0); emit_case(out, "mined", b, c, MPFR_RNDN); clear_pair(b, c); }
    }

    return 0;
}
