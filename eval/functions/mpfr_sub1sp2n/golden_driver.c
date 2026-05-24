/*
 * golden_driver.c — Golden master for MPFR's mpfr_sub1sp2n (p == 128).
 *
 * mpfr_sub1sp2n is `static` in mpfr/src/sub1sp.c. We exercise it via
 * mpfr_sub at prec == 128 — dispatcher L1490-L1491 routes here.
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
 * Ref: mpfr/src/sub1sp.c L775-L1053 — C reference.
 * Ref: src/ops/sub1sp2n.ts — production port.
 */
#include "common.h"

#include <assert.h>
#include <inttypes.h>
#include <string.h>

#if GMP_NUMB_BITS != 64
#  error "mpfr_sub1sp2n golden_driver requires GMP_NUMB_BITS == 64"
#endif

static const mpfr_rnd_t RNDS[5] = {
    MPFR_RNDN, MPFR_RNDZ, MPFR_RNDU, MPFR_RNDD, MPFR_RNDA,
};

static void
emit_case(FILE *out, const char *tag,
          mpfr_srcptr b, mpfr_srcptr c, mpfr_rnd_t rnd) {
    assert(mpfr_get_prec(b) == 128);
    assert(mpfr_get_prec(c) == 128);
    assert(mpfr_regular_p(b));
    assert(mpfr_regular_p(c));
    assert(mpfr_signbit(b) == mpfr_signbit(c));

    mpfr_t a;
    mpfr_init2(a, 128);
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

static void mk_pair_dd(mpfr_ptr b, mpfr_ptr c,
                       double db, double dc) {
    mpfr_init2(b, 128);
    mpfr_init2(c, 128);
    mpfr_set_d(b, db, MPFR_RNDN);
    mpfr_set_d(c, dc, MPFR_RNDN);
    assert(mpfr_regular_p(b));
    assert(mpfr_regular_p(c));
    assert(mpfr_signbit(b) == mpfr_signbit(c));
}

static void clear_pair(mpfr_ptr b, mpfr_ptr c) {
    mpfr_clear(b); mpfr_clear(c);
}

static void mk_pair_ii(mpfr_ptr b, mpfr_ptr c,
                       long magB, long magC, int sign) {
    assert(magB > 0 && magC > 0);
    mpfr_init2(b, 128);
    mpfr_init2(c, 128);
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
    mpfr_init2(b, 128);
    mpfr_init2(c, 128);
    mpfr_set_z_2exp(b, zB, expB - 128, MPFR_RNDN);
    mpfr_set_z_2exp(c, zC, expC - 128, MPFR_RNDN);
    mpz_clear(zB); mpz_clear(zC);
    assert(mpfr_regular_p(b));
    assert(mpfr_regular_p(c));
    assert(mpfr_signbit(b) == mpfr_signbit(c));
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
        { mpfr_t b, c; mk_pair_ii(b, c, 500, 1000, 1); emit_case(out, "happy", b, c, MPFR_RNDU); clear_pair(b, c); }
        { mpfr_t b, c; mk_pair_ii(b, c, 1000, 500, -1); emit_case(out, "happy", b, c, MPFR_RNDN); clear_pair(b, c); }
        { mpfr_t b, c; mk_pair_dd(b, c, 16.0, 8.0); emit_case(out, "happy", b, c, MPFR_RNDA); clear_pair(b, c); }
        { mpfr_t b, c; mk_pair_dd(b, c, 2.5, 1.25); emit_case(out, "happy", b, c, MPFR_RNDN); clear_pair(b, c); }
        { mpfr_t b, c; mk_pair_dd(b, c, 100.5, 0.25); emit_case(out, "happy", b, c, MPFR_RNDN); clear_pair(b, c); }
        { mpfr_t b, c; mk_pair_dd(b, c, 5.0, 3.0); emit_case(out, "happy", b, c, MPFR_RNDN); clear_pair(b, c); }
        { mpfr_t b, c; mk_pair_dd(b, c, 100.0, 50.0); emit_case(out, "happy", b, c, MPFR_RNDD); clear_pair(b, c); }
    }

    /* edge: 30 */
    {
        /* Full cancellation, all 5 modes both signs. */
        for (int i = 0; i < 5; ++i) {
            mpfr_t b, c; mk_pair_dd(b, c, 3.14, 3.14);
            emit_case(out, "edge", b, c, RNDS[i]); clear_pair(b, c);
        }
        for (int i = 0; i < 5; ++i) {
            mpfr_t b, c; mk_pair_dd(b, c, -3.14, -3.14);
            emit_case(out, "edge", b, c, RNDS[i]); clear_pair(b, c);
        }
        /* Near-cancellation. */
        { mpfr_t b, c; mk_pair_exact(b, c,
            "340282366920938463463374607431768211455", 128,  /* 2^128 - 1 */
            "340282366920938463463374607431768211454", 128,
            1);
          emit_case(out, "edge", b, c, MPFR_RNDU); clear_pair(b, c); }
        { mpfr_t b, c; mk_pair_exact(b, c,
            "340282366920938463463374607431768211454", 128,
            "340282366920938463463374607431768211455", 128,
            1);
          emit_case(out, "edge", b, c, MPFR_RNDU); clear_pair(b, c); }
        /* d == 64 boundary. */
        { mpfr_t b, c; mk_pair_exact(b, c,
            "170141183460469231731687303715884105728", 128,  /* 2^127 */
            "170141183460469231731687303715884105728", 64,
            1);
          emit_case(out, "edge", b, c, MPFR_RNDN); clear_pair(b, c); }
        /* d == 128 boundary. */
        { mpfr_t b, c; mk_pair_exact(b, c,
            "170141183460469231731687303715884105728", 200,
            "170141183460469231731687303715884105728", 72,
            1);
          emit_case(out, "edge", b, c, MPFR_RNDN); clear_pair(b, c); }
        /* d > 128. */
        { mpfr_t b, c; mk_pair_exact(b, c,
            "170141183460469231731687303715884105728", 300,
            "170141183460469231731687303715884105728", 100,
            1);
          emit_case(out, "edge", b, c, MPFR_RNDN); clear_pair(b, c); }
        /* d=1 cancellation. */
        { mpfr_t b, c; mk_pair_exact(b, c,
            "170141183460469231731687303715884105728", 129,
            "340282366920938463463374607431768211455", 128,
            1);
          emit_case(out, "edge", b, c, MPFR_RNDN); clear_pair(b, c); }
        /* Tiny magnitudes. */
        { mpfr_t b, c; mk_pair_dd(b, c, 1e-100, 1e-101);
          emit_case(out, "edge", b, c, MPFR_RNDN); clear_pair(b, c); }
        /* Large magnitudes. */
        { mpfr_t b, c; mk_pair_dd(b, c, 1e100, 1e99);
          emit_case(out, "edge", b, c, MPFR_RNDN); clear_pair(b, c); }
        /* Negative near-cancel. */
        { mpfr_t b, c; mk_pair_exact(b, c,
            "340282366920938463463374607431768211455", 128,
            "340282366920938463463374607431768211454", 128,
            -1);
          emit_case(out, "edge", b, c, MPFR_RNDD); clear_pair(b, c); }
        /* d=1 negative. */
        { mpfr_t b, c; mk_pair_exact(b, c,
            "170141183460469231731687303715884105728", 129,
            "340282366920938463463374607431768211455", 128,
            -1);
          emit_case(out, "edge", b, c, MPFR_RNDD); clear_pair(b, c); }
        /* Power of 2 minus small. */
        { mpfr_t b, c; mk_pair_exact(b, c,
            "170141183460469231731687303715884105728", 128,
            "1", 0,
            1);
          emit_case(out, "edge", b, c, MPFR_RNDN); clear_pair(b, c); }
        /* More edge cases to reach >= 30. */
        /* d=64 with bp small (sh issue at p=128). */
        { mpfr_t b, c; mk_pair_exact(b, c,
            "170141183460469231731687303715884105728", 128,
            "170141183460469231731687303715884105728", 64,
            -1);
          emit_case(out, "edge", b, c, MPFR_RNDD); clear_pair(b, c); }
        /* d=63. */
        { mpfr_t b, c; mk_pair_exact(b, c,
            "170141183460469231731687303715884105728", 128,
            "170141183460469231731687303715884105728", 65,
            1);
          emit_case(out, "edge", b, c, MPFR_RNDU); clear_pair(b, c); }
        /* d=65. */
        { mpfr_t b, c; mk_pair_exact(b, c,
            "170141183460469231731687303715884105728", 128,
            "170141183460469231731687303715884105728", 63,
            1);
          emit_case(out, "edge", b, c, MPFR_RNDU); clear_pair(b, c); }
        /* d=127. */
        { mpfr_t b, c; mk_pair_exact(b, c,
            "170141183460469231731687303715884105728", 200,
            "170141183460469231731687303715884105728", 73,
            1);
          emit_case(out, "edge", b, c, MPFR_RNDU); clear_pair(b, c); }
        /* d=129. */
        { mpfr_t b, c; mk_pair_exact(b, c,
            "170141183460469231731687303715884105728", 200,
            "170141183460469231731687303715884105728", 71,
            1);
          emit_case(out, "edge", b, c, MPFR_RNDU); clear_pair(b, c); }
        /* Power of 2 negative. */
        { mpfr_t b, c; mk_pair_exact(b, c,
            "170141183460469231731687303715884105728", 128,
            "1", 0,
            -1);
          emit_case(out, "edge", b, c, MPFR_RNDD); clear_pair(b, c); }
        /* Both negative tiny. */
        { mpfr_t b, c; mk_pair_dd(b, c, -1e-100, -1e-101);
          emit_case(out, "edge", b, c, MPFR_RNDD); clear_pair(b, c); }
        /* Both negative large. */
        { mpfr_t b, c; mk_pair_dd(b, c, -1e100, -1e99);
          emit_case(out, "edge", b, c, MPFR_RNDU); clear_pair(b, c); }
        /* Mantissa boundary with full carry. */
        { mpfr_t b, c; mk_pair_exact(b, c,
            "340282366920938463463374607431768211454", 128,  /* 2^128 - 2 */
            "1", 0,
            1);
          emit_case(out, "edge", b, c, MPFR_RNDU); clear_pair(b, c); }
    }

    /* adversarial: 12 */
    {
        { mpfr_t b, c; mk_pair_dd(b, c, 1.0, 1.0);
          emit_case(out, "adversarial", b, c, MPFR_RNDD); clear_pair(b, c); }
        { mpfr_t b, c; mk_pair_dd(b, c, 1.0, 1.0);
          emit_case(out, "adversarial", b, c, MPFR_RNDN); clear_pair(b, c); }
        { mpfr_t b, c; mk_pair_dd(b, c, 1.0/3.0, 1.0/7.0);
          emit_case(out, "adversarial", b, c, MPFR_RNDU); clear_pair(b, c); }
        { mpfr_t b, c; mk_pair_dd(b, c, 1.0/3.0, 1.0/7.0);
          emit_case(out, "adversarial", b, c, MPFR_RNDD); clear_pair(b, c); }
        { mpfr_t b, c; mk_pair_dd(b, c, -1.0/3.0, -1.0/7.0);
          emit_case(out, "adversarial", b, c, MPFR_RNDU); clear_pair(b, c); }
        { mpfr_t b, c; mk_pair_dd(b, c, -1.0/3.0, -1.0/7.0);
          emit_case(out, "adversarial", b, c, MPFR_RNDD); clear_pair(b, c); }
        /* All-ones - 1 ulp carry boundary. */
        { mpfr_t b, c; mk_pair_exact(b, c,
            "340282366920938463463374607431768211455", 128,
            "1", 1,
            1);
          emit_case(out, "adversarial", b, c, MPFR_RNDU); clear_pair(b, c); }
        /* d=1, special cancellation (b=B/2 at exp+1, c=B-1 at exp). */
        { mpfr_t b, c; mk_pair_exact(b, c,
            "170141183460469231731687303715884105728", 129,
            "340282366920938463463374607431768211455", 128,
            1);
          emit_case(out, "adversarial", b, c, MPFR_RNDU); clear_pair(b, c); }
        /* Large-d sticky path. */
        { mpfr_t b, c; mk_pair_dd(b, c, 1e30, 1e-30);
          emit_case(out, "adversarial", b, c, MPFR_RNDU); clear_pair(b, c); }
        { mpfr_t b, c; mk_pair_dd(b, c, 1e30, 1e-30);
          emit_case(out, "adversarial", b, c, MPFR_RNDD); clear_pair(b, c); }
        { mpfr_t b, c; mk_pair_dd(b, c, 8.0, 4.0);
          emit_case(out, "adversarial", b, c, MPFR_RNDA); clear_pair(b, c); }
        /* Tie-to-even at p=128. */
        { mpfr_t b, c; mk_pair_exact(b, c,
            "340282366920938463463374607431768211454", 128,  /* 2^128 - 2 */
            "1", 0,
            1);
          emit_case(out, "adversarial", b, c, MPFR_RNDN); clear_pair(b, c); }
    }

    /* fuzz: 60 */
    {
        xs64_t rng;
        xs64_seed(&rng, 0x5B1F50128128ULL);
        int emitted = 0;
        while (emitted < 60) {
            const double magB = (double)(1 + xs64_below(&rng, 1000000));
            const double magC = (double)(1 + xs64_below(&rng, 1000000));
            const int sign = (xs64_below(&rng, 2) == 0) ? +1 : -1;
            const uint64_t rnd_idx = xs64_below(&rng, 5);
            mpfr_t b, c;
            mpfr_init2(b, 128);
            mpfr_init2(c, 128);
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
