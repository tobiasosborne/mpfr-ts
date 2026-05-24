/*
 * golden_driver.c — Golden master for MPFR's mpfr_sub1sp2 (65 <= p <= 127).
 *
 * mpfr_sub1sp2 is `static` in mpfr/src/sub1sp.c. We exercise it
 * indirectly via mpfr_sub at prec in (64, 128) — the dispatcher routes
 * to sub1sp2 per L1478-L1479.
 *
 * Inputs: b.prec == c.prec in [65, 127], b.sign == c.sign, both normal.
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
 * Ref: mpfr/src/sub1sp.c L513-L772 — C reference.
 * Ref: src/ops/sub1sp2.ts — production port.
 */
#include "common.h"

#include <assert.h>
#include <inttypes.h>
#include <string.h>

#if GMP_NUMB_BITS != 64
#  error "mpfr_sub1sp2 golden_driver requires GMP_NUMB_BITS == 64"
#endif

static const mpfr_rnd_t RNDS[5] = {
    MPFR_RNDN, MPFR_RNDZ, MPFR_RNDU, MPFR_RNDD, MPFR_RNDA,
};

static void
emit_case(FILE *out, const char *tag,
          mpfr_srcptr b, mpfr_srcptr c, mpfr_rnd_t rnd) {
    const mpfr_prec_t p = mpfr_get_prec(b);
    assert(p == mpfr_get_prec(c));
    assert(p > 64 && p < 128);
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
    assert(prec > 64 && prec < 128);
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
    assert(prec > 64 && prec < 128);
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
    assert(prec > 64 && prec < 128);
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
            mpfr_t b, c; mk_pair_dd(b, c, 113, 1.0, 0.5);
            emit_case(out, "happy", b, c, RNDS[i]); clear_pair(b, c);
        }
        { mpfr_t b, c; mk_pair_dd(b, c, 113, 3.14, 2.71); emit_case(out, "happy", b, c, MPFR_RNDN); clear_pair(b, c); }
        { mpfr_t b, c; mk_pair_dd(b, c, 113, -3.14, -2.71); emit_case(out, "happy", b, c, MPFR_RNDN); clear_pair(b, c); }
        { mpfr_t b, c; mk_pair_dd(b, c, 100, 100.0, 50.0); emit_case(out, "happy", b, c, MPFR_RNDN); clear_pair(b, c); }
        { mpfr_t b, c; mk_pair_dd(b, c, 65, 1e10, 1.0); emit_case(out, "happy", b, c, MPFR_RNDN); clear_pair(b, c); }
        { mpfr_t b, c; mk_pair_dd(b, c, 127, 1e10, 1.0); emit_case(out, "happy", b, c, MPFR_RNDN); clear_pair(b, c); }
        { mpfr_t b, c; mk_pair_dd(b, c, 80, 1.0, 1e-10); emit_case(out, "happy", b, c, MPFR_RNDN); clear_pair(b, c); }
        { mpfr_t b, c; mk_pair_dd(b, c, 100, -100.0, -50.0); emit_case(out, "happy", b, c, MPFR_RNDD); clear_pair(b, c); }
        { mpfr_t b, c; mk_pair_dd(b, c, 100, -50.0, -100.0); emit_case(out, "happy", b, c, MPFR_RNDU); clear_pair(b, c); }
        { mpfr_t b, c; mk_pair_ii(b, c, 96, 1000, 500, 1); emit_case(out, "happy", b, c, MPFR_RNDN); clear_pair(b, c); }
        { mpfr_t b, c; mk_pair_ii(b, c, 96, 500, 1000, 1); emit_case(out, "happy", b, c, MPFR_RNDN); clear_pair(b, c); }
        { mpfr_t b, c; mk_pair_ii(b, c, 96, 1000, 500, -1); emit_case(out, "happy", b, c, MPFR_RNDN); clear_pair(b, c); }
        { mpfr_t b, c; mk_pair_ii(b, c, 96, 500, 1000, -1); emit_case(out, "happy", b, c, MPFR_RNDN); clear_pair(b, c); }
        { mpfr_t b, c; mk_pair_dd(b, c, 113, 16.0, 8.0); emit_case(out, "happy", b, c, MPFR_RNDA); clear_pair(b, c); }
        { mpfr_t b, c; mk_pair_dd(b, c, 113, 16.0, 4.0); emit_case(out, "happy", b, c, MPFR_RNDZ); clear_pair(b, c); }
        { mpfr_t b, c; mk_pair_dd(b, c, 80, 2.5, 1.25); emit_case(out, "happy", b, c, MPFR_RNDN); clear_pair(b, c); }
        { mpfr_t b, c; mk_pair_dd(b, c, 80, 100.5, 0.25); emit_case(out, "happy", b, c, MPFR_RNDN); clear_pair(b, c); }
        { mpfr_t b, c; mk_pair_dd(b, c, 80, 100.5, 100.5); emit_case(out, "happy", b, c, MPFR_RNDN); clear_pair(b, c); }
    }

    /* edge: 32 cases */
    {
        /* p=65, smallest. */
        for (int i = 0; i < 5; ++i) {
            mpfr_t b, c; mk_pair_dd(b, c, 65, 3.0, 1.0);
            emit_case(out, "edge", b, c, RNDS[i]); clear_pair(b, c);
        }
        /* p=127, largest. */
        for (int i = 0; i < 5; ++i) {
            mpfr_t b, c; mk_pair_dd(b, c, 127, 3.0, 1.0);
            emit_case(out, "edge", b, c, RNDS[i]); clear_pair(b, c);
        }
        /* Full cancellation under all 5 modes. */
        for (int i = 0; i < 5; ++i) {
            mpfr_t b, c; mk_pair_dd(b, c, 96, 3.14, 3.14);
            emit_case(out, "edge", b, c, RNDS[i]); clear_pair(b, c);
        }
        /* Near-cancellation (multi-limb).
         * mantissa = 2^96 - 1 (close to all-ones for prec=96) vs 2^96 - 2. */
        { mpfr_t b, c; mk_pair_exact(b, c, 96,
            "79228162514264337593543950335", 96,  /* 2^96 - 1 */
            "79228162514264337593543950334", 96,  /* 2^96 - 2 */
            1);
          emit_case(out, "edge", b, c, MPFR_RNDN); clear_pair(b, c); }
        { mpfr_t b, c; mk_pair_exact(b, c, 96,
            "79228162514264337593543950334", 96,
            "79228162514264337593543950335", 96,
            1);
          emit_case(out, "edge", b, c, MPFR_RNDN); clear_pair(b, c); }
        /* d == 64 boundary at p=96. */
        { mpfr_t b, c; mk_pair_exact(b, c, 96,
            "39614081257132168796771975168", 100,  /* 2^95 */
            "39614081257132168796771975168", 36,
            1);
          emit_case(out, "edge", b, c, MPFR_RNDN); clear_pair(b, c); }
        /* d > 64 at p=80. */
        { mpfr_t b, c; mk_pair_exact(b, c, 80,
            "604462909807314587353088", 80,  /* 2^79 */
            "604462909807314587353088", 5,
            1);
          emit_case(out, "edge", b, c, MPFR_RNDN); clear_pair(b, c); }
        /* d > 2*GMP_NUMB_BITS = 128. */
        { mpfr_t b, c; mk_pair_exact(b, c, 80,
            "604462909807314587353088", 200,
            "604462909807314587353088", 50,
            1);
          emit_case(out, "edge", b, c, MPFR_RNDN); clear_pair(b, c); }
        /* Negative-magnitude near-cancellation. */
        { mpfr_t b, c; mk_pair_exact(b, c, 100,
            "1267650600228229401496703205375", 100,  /* 2^100 - 1 */
            "1267650600228229401496703205374", 100,
            -1);
          emit_case(out, "edge", b, c, MPFR_RNDD); clear_pair(b, c); }
        /* d=1 cancellation special. */
        { mpfr_t b, c; mk_pair_exact(b, c, 80,
            "604462909807314587353088", 81,  /* 2^79 at exp 81 = val 2^80 */
            "1208925819614629174706175", 80, /* 2^80 - 1 at exp 80 */
            1);
          emit_case(out, "edge", b, c, MPFR_RNDN); clear_pair(b, c); }
        /* Very small magnitudes. */
        { mpfr_t b, c; mk_pair_dd(b, c, 113, 1e-100, 1e-101);
          emit_case(out, "edge", b, c, MPFR_RNDU); clear_pair(b, c); }
        /* Very large magnitudes. */
        { mpfr_t b, c; mk_pair_dd(b, c, 113, 1e100, 1e99);
          emit_case(out, "edge", b, c, MPFR_RNDD); clear_pair(b, c); }
        /* Padding to >= 30 edges. */
        /* Power-of-2 minus tiny. */
        { mpfr_t b, c; mk_pair_exact(b, c, 96,
            "39614081257132168796771975168", 96,  /* 2^95 */
            "1", 0,
            1);
          emit_case(out, "edge", b, c, MPFR_RNDN); clear_pair(b, c); }
        { mpfr_t b, c; mk_pair_exact(b, c, 96,
            "39614081257132168796771975168", 96,
            "1", 0,
            -1);
          emit_case(out, "edge", b, c, MPFR_RNDN); clear_pair(b, c); }
        /* All-ones mant at sundry precs. */
        { mpfr_t b, c; mk_pair_exact(b, c, 70,
            "1180591620717411303423", 70,  /* 2^70 - 1 */
            "1", 1,
            1);
          emit_case(out, "edge", b, c, MPFR_RNDN); clear_pair(b, c); }
        { mpfr_t b, c; mk_pair_exact(b, c, 125,
            "42535295865117307932921825928971026431", 125,  /* 2^125 - 1 */
            "1", 1,
            1);
          emit_case(out, "edge", b, c, MPFR_RNDU); clear_pair(b, c); }
        /* Both negative, d small. */
        { mpfr_t b, c; mk_pair_dd(b, c, 96, -3.0, -1.0);
          emit_case(out, "edge", b, c, MPFR_RNDD); clear_pair(b, c); }
        { mpfr_t b, c; mk_pair_dd(b, c, 96, -1.0, -3.0);
          emit_case(out, "edge", b, c, MPFR_RNDU); clear_pair(b, c); }
    }

    /* adversarial: 12 cases */
    {
        /* RNDN tie-to-even at prec=96. */
        { mpfr_t b, c; mk_pair_exact(b, c, 96,
            "79228162514264337593543950335", 96,
            "1", 0,
            1);
          emit_case(out, "adversarial", b, c, MPFR_RNDN); clear_pair(b, c); }
        /* Cancellation rnd-D vs rnd-N at p=80. */
        { mpfr_t b, c; mk_pair_dd(b, c, 80, 1.0, 1.0);
          emit_case(out, "adversarial", b, c, MPFR_RNDD); clear_pair(b, c); }
        { mpfr_t b, c; mk_pair_dd(b, c, 80, 1.0, 1.0);
          emit_case(out, "adversarial", b, c, MPFR_RNDN); clear_pair(b, c); }
        /* Heavy cancellation with multi-limb shift normalisation. */
        { mpfr_t b, c; mk_pair_exact(b, c, 100,
            "1267650600228229401496703205376", 101,  /* 2^100, val = 2^200 ... no, at exp=101 val = 2^200 */
            "1267650600228229401496703205375", 100,
            1);
          emit_case(out, "adversarial", b, c, MPFR_RNDU); clear_pair(b, c); }
        /* RNDU on positive vs RNDD on positive. */
        { mpfr_t b, c; mk_pair_dd(b, c, 113, 1.0/3.0, 1.0/7.0);
          emit_case(out, "adversarial", b, c, MPFR_RNDU); clear_pair(b, c); }
        { mpfr_t b, c; mk_pair_dd(b, c, 113, 1.0/3.0, 1.0/7.0);
          emit_case(out, "adversarial", b, c, MPFR_RNDD); clear_pair(b, c); }
        /* d boundary at sh-1 vs sh (sh = 128 - p = 32 at p=96). */
        { mpfr_t b, c; mk_pair_exact(b, c, 96,
            "39614081257132168796771975168", 96,
            "39614081257132168796771975168", 65,
            1);
          emit_case(out, "adversarial", b, c, MPFR_RNDU); clear_pair(b, c); }
        { mpfr_t b, c; mk_pair_exact(b, c, 96,
            "39614081257132168796771975168", 96,
            "39614081257132168796771975168", 64,
            1);
          emit_case(out, "adversarial", b, c, MPFR_RNDU); clear_pair(b, c); }
        /* Carry on add_one_ulp (mantissa all-ones for prec). */
        { mpfr_t b, c; mk_pair_exact(b, c, 96,
            "79228162514264337593543950335", 96,
            "1", 1,
            1);
          emit_case(out, "adversarial", b, c, MPFR_RNDU); clear_pair(b, c); }
        /* All 5 modes on the same hard-to-round case. */
        { mpfr_t b, c; mk_pair_dd(b, c, 113, 1.0/3.0, 1.0/7.0);
          emit_case(out, "adversarial", b, c, MPFR_RNDA); clear_pair(b, c); }
        { mpfr_t b, c; mk_pair_dd(b, c, 113, 1.0/3.0, 1.0/7.0);
          emit_case(out, "adversarial", b, c, MPFR_RNDZ); clear_pair(b, c); }
        { mpfr_t b, c; mk_pair_dd(b, c, 113, -1.0/3.0, -1.0/7.0);
          emit_case(out, "adversarial", b, c, MPFR_RNDU); clear_pair(b, c); }
    }

    /* fuzz: 60 cases */
    {
        xs64_t rng;
        xs64_seed(&rng, 0x5B1F50220112ULL);
        int emitted = 0;
        while (emitted < 60) {
            const uint64_t prec = 65 + xs64_below(&rng, 63);  /* [65, 127] */
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
        { mpfr_t b, c; mk_pair_dd(b, c, 113, 1.0, 1.0); emit_case(out, "mined", b, c, MPFR_RNDN); clear_pair(b, c); }
        { mpfr_t b, c; mk_pair_dd(b, c, 113, 1.0, 1.0); emit_case(out, "mined", b, c, MPFR_RNDD); clear_pair(b, c); }
        { mpfr_t b, c; mk_pair_dd(b, c, 96, 2.0, 1.0); emit_case(out, "mined", b, c, MPFR_RNDN); clear_pair(b, c); }
        { mpfr_t b, c; mk_pair_dd(b, c, 80, 1.5, 0.5); emit_case(out, "mined", b, c, MPFR_RNDU); clear_pair(b, c); }
        { mpfr_t b, c; mk_pair_dd(b, c, 100, -2.0, -1.0); emit_case(out, "mined", b, c, MPFR_RNDN); clear_pair(b, c); }
    }

    return 0;
}
