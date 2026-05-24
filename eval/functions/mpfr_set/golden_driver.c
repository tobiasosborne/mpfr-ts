/*
 * golden_driver.c — Golden master for MPFR's mpfr_set.
 *
 * mpfr_set(a, b, rnd) copies b into a at a's precision, rounding per
 * rnd. The C source (mpfr/src/set.c L67-L79) is a one-line delegate
 * to mpfr_set4 with signb = SIGN(b). The cases here mirror set4's
 * coverage but always with signb implied by b — no sign-override
 * cases (those exercise abs/neg/setsign/copysign separately).
 *
 * Wire format
 * -----------
 *
 *   {"tag":"<class>",
 *    "inputs":{"b":<MPFR>,"prec":"<dec>","rnd":"RND[NZUDA]"},
 *    "output":{"value":<MPFR>,"ternary":<-1|0|1>},
 *    "time_ns":<n>}
 *
 * Tag distribution (Rule 7 minimums)
 * ----------------------------------
 *
 *   happy        :  22
 *   edge         :  32
 *   adversarial  :  10
 *   fuzz         :  55
 *   mined        :   5
 *
 * Ref: mpfr/src/set.c L67-L79 — C reference.
 * Ref: src/ops/set.ts — production port.
 */
#include "common.h"

#include <assert.h>
#include <inttypes.h>
#include <string.h>

static const mpfr_rnd_t RNDS[5] = {
    MPFR_RNDN, MPFR_RNDZ, MPFR_RNDU, MPFR_RNDD, MPFR_RNDA,
};

static void
emit_case(FILE *out, const char *tag,
          mpfr_srcptr b, mpfr_prec_t prec, mpfr_rnd_t rnd) {
    assert(prec >= 1);
    mpfr_t a;
    mpfr_init2(a, prec);
    const uint64_t t0 = now_ns();
    const int ternary = mpfr_set(a, b, rnd);
    const uint64_t elapsed = now_ns() - t0;
    jl_begin(out, tag);
    jl_kv_mpfr(out, 1, "b", b);
    jl_kv_i64(out, 0, "prec", (int64_t)prec);
    jl_kv_rnd(out, 0, "rnd", rnd);
    jl_end_inputs(out);
    jl_output_result(out, a, ternary);
    jl_finish(out, elapsed);
    mpfr_clear(a);
}

static void mk_norm_d(mpfr_ptr b, mpfr_prec_t prec, double d) {
    mpfr_init2(b, prec);
    mpfr_set_d(b, d, MPFR_RNDN);
}

static void mk_norm_si(mpfr_ptr b, mpfr_prec_t prec, long v) {
    mpfr_init2(b, prec);
    mpfr_set_si(b, v, MPFR_RNDN);
}

static void mk_norm_exact(mpfr_ptr b, mpfr_prec_t prec,
                          const char *mant_dec, mpfr_exp_t exp_ts, int sign) {
    mpz_t z;
    mpz_init(z);
    if (mpz_set_str(z, mant_dec, 10) != 0) {
        fprintf(stderr, "mk_norm_exact: bad decimal\n");
        exit(2);
    }
    if (sign < 0) mpz_neg(z, z);
    mpfr_init2(b, prec);
    mpfr_set_z_2exp(b, z, exp_ts - (mpfr_exp_t)prec, MPFR_RNDN);
    mpz_clear(z);
    assert(mpfr_regular_p(b));
}

int main(void) {
    FILE *out = stdout;

    /* happy: 22 */
    {
        /* Same prec copy. */
        { mpfr_t b; mk_norm_d(b, 53, 3.14); emit_case(out, "happy", b, 53, MPFR_RNDN); mpfr_clear(b); }
        { mpfr_t b; mk_norm_d(b, 53, -3.14); emit_case(out, "happy", b, 53, MPFR_RNDN); mpfr_clear(b); }
        { mpfr_t b; mk_norm_d(b, 53, 2.71); emit_case(out, "happy", b, 53, MPFR_RNDN); mpfr_clear(b); }
        /* Lossy down-round. */
        for (int i = 0; i < 5; ++i) {
            mpfr_t b; mk_norm_d(b, 53, 1.0/3.0);
            emit_case(out, "happy", b, 24, RNDS[i]);
            mpfr_clear(b);
        }
        /* Lossless up-pad. */
        { mpfr_t b; mk_norm_d(b, 24, 1.5); emit_case(out, "happy", b, 53, MPFR_RNDN); mpfr_clear(b); }
        { mpfr_t b; mk_norm_d(b, 24, -1.5); emit_case(out, "happy", b, 53, MPFR_RNDN); mpfr_clear(b); }
        /* Zero, both signs. */
        { mpfr_t b; mpfr_init2(b, 53); mpfr_set_zero(b, 1); emit_case(out, "happy", b, 53, MPFR_RNDN); mpfr_clear(b); }
        { mpfr_t b; mpfr_init2(b, 53); mpfr_set_zero(b, -1); emit_case(out, "happy", b, 53, MPFR_RNDN); mpfr_clear(b); }
        /* Inf, both signs. */
        { mpfr_t b; mpfr_init2(b, 53); mpfr_set_inf(b, 1); emit_case(out, "happy", b, 53, MPFR_RNDN); mpfr_clear(b); }
        { mpfr_t b; mpfr_init2(b, 53); mpfr_set_inf(b, -1); emit_case(out, "happy", b, 53, MPFR_RNDN); mpfr_clear(b); }
        /* NaN. */
        { mpfr_t b; mpfr_init2(b, 53); mpfr_set_nan(b); emit_case(out, "happy", b, 53, MPFR_RNDN); mpfr_clear(b); }
        /* Various prec sizes. */
        { mpfr_t b; mk_norm_si(b, 53, 7); emit_case(out, "happy", b, 53, MPFR_RNDN); mpfr_clear(b); }
        { mpfr_t b; mk_norm_si(b, 53, -7); emit_case(out, "happy", b, 53, MPFR_RNDN); mpfr_clear(b); }
        { mpfr_t b; mk_norm_d(b, 113, 3.14); emit_case(out, "happy", b, 113, MPFR_RNDN); mpfr_clear(b); }
        { mpfr_t b; mk_norm_d(b, 100, 1.5e10); emit_case(out, "happy", b, 100, MPFR_RNDN); mpfr_clear(b); }
        { mpfr_t b; mk_norm_d(b, 80, 1e-50); emit_case(out, "happy", b, 80, MPFR_RNDN); mpfr_clear(b); }
        { mpfr_t b; mk_norm_d(b, 80, -1e-50); emit_case(out, "happy", b, 80, MPFR_RNDN); mpfr_clear(b); }
        { mpfr_t b; mk_norm_d(b, 80, 1e50); emit_case(out, "happy", b, 80, MPFR_RNDN); mpfr_clear(b); }
    }

    /* edge: 32 */
    {
        /* prec=1 boundary. */
        for (int i = 0; i < 5; ++i) {
            mpfr_t b; mk_norm_si(b, 1, 1);
            emit_case(out, "edge", b, 1, RNDS[i]);
            mpfr_clear(b);
        }
        /* prec=1 from larger source. */
        for (int i = 0; i < 5; ++i) {
            mpfr_t b; mk_norm_d(b, 53, 3.14);
            emit_case(out, "edge", b, 1, RNDS[i]);
            mpfr_clear(b);
        }
        /* Heavy rounding to small prec across all modes. */
        for (int i = 0; i < 5; ++i) {
            mpfr_t b; mk_norm_d(b, 113, 1.0/7.0);
            emit_case(out, "edge", b, 3, RNDS[i]);
            mpfr_clear(b);
        }
        /* Negative heavy-rounded. */
        for (int i = 0; i < 5; ++i) {
            mpfr_t b; mk_norm_d(b, 113, -1.0/7.0);
            emit_case(out, "edge", b, 3, RNDS[i]);
            mpfr_clear(b);
        }
        /* Zero/Inf/NaN at boundary precs. */
        { mpfr_t b; mpfr_init2(b, 1); mpfr_set_zero(b, 1); emit_case(out, "edge", b, 1, MPFR_RNDN); mpfr_clear(b); }
        { mpfr_t b; mpfr_init2(b, 1); mpfr_set_zero(b, -1); emit_case(out, "edge", b, 1, MPFR_RNDN); mpfr_clear(b); }
        { mpfr_t b; mpfr_init2(b, 1024); mpfr_set_zero(b, 1); emit_case(out, "edge", b, 53, MPFR_RNDN); mpfr_clear(b); }
        { mpfr_t b; mpfr_init2(b, 53); mpfr_set_zero(b, -1); emit_case(out, "edge", b, 1024, MPFR_RNDN); mpfr_clear(b); }
        { mpfr_t b; mpfr_init2(b, 1); mpfr_set_inf(b, 1); emit_case(out, "edge", b, 1024, MPFR_RNDN); mpfr_clear(b); }
        { mpfr_t b; mpfr_init2(b, 1024); mpfr_set_inf(b, -1); emit_case(out, "edge", b, 1, MPFR_RNDN); mpfr_clear(b); }
        { mpfr_t b; mpfr_init2(b, 1); mpfr_set_nan(b); emit_case(out, "edge", b, 1024, MPFR_RNDN); mpfr_clear(b); }
        { mpfr_t b; mpfr_init2(b, 1024); mpfr_set_nan(b); emit_case(out, "edge", b, 1, MPFR_RNDN); mpfr_clear(b); }
        /* Carry-out on rounding. */
        { mpfr_t b; mk_norm_exact(b, 8, "255", 8, 1); emit_case(out, "edge", b, 4, MPFR_RNDN); mpfr_clear(b); }
        { mpfr_t b; mk_norm_exact(b, 8, "255", 8, 1); emit_case(out, "edge", b, 4, MPFR_RNDU); mpfr_clear(b); }
        { mpfr_t b; mk_norm_exact(b, 8, "255", 8, -1); emit_case(out, "edge", b, 4, MPFR_RNDD); mpfr_clear(b); }
        /* Multi-limb roundtrip. */
        { mpfr_t b; mk_norm_d(b, 200, 1.5); emit_case(out, "edge", b, 200, MPFR_RNDN); mpfr_clear(b); }
        { mpfr_t b; mk_norm_d(b, 200, 1.5); emit_case(out, "edge", b, 100, MPFR_RNDN); mpfr_clear(b); }
    }

    /* adversarial: 10 */
    {
        /* RNDN tie-to-even at small prec. */
        { mpfr_t b; mk_norm_exact(b, 5, "20", 5, 1); emit_case(out, "adversarial", b, 3, MPFR_RNDN); mpfr_clear(b); }
        /* Negative with RNDU vs RNDD (sign-asymmetric routing). */
        { mpfr_t b; mk_norm_d(b, 53, -1.0/3.0); emit_case(out, "adversarial", b, 8, MPFR_RNDU); mpfr_clear(b); }
        { mpfr_t b; mk_norm_d(b, 53, -1.0/3.0); emit_case(out, "adversarial", b, 8, MPFR_RNDD); mpfr_clear(b); }
        /* Positive with RNDU vs RNDD. */
        { mpfr_t b; mk_norm_d(b, 53, 1.0/3.0); emit_case(out, "adversarial", b, 8, MPFR_RNDU); mpfr_clear(b); }
        { mpfr_t b; mk_norm_d(b, 53, 1.0/3.0); emit_case(out, "adversarial", b, 8, MPFR_RNDD); mpfr_clear(b); }
        /* Carry-out on add_one_ulp causing exponent bump. */
        { mpfr_t b; mk_norm_exact(b, 8, "255", 8, 1); emit_case(out, "adversarial", b, 4, MPFR_RNDA); mpfr_clear(b); }
        /* RNDA on negative: should round away (more negative). */
        { mpfr_t b; mk_norm_d(b, 53, -1.0/3.0); emit_case(out, "adversarial", b, 8, MPFR_RNDA); mpfr_clear(b); }
        /* Zero with prec change. */
        { mpfr_t b; mpfr_init2(b, 53); mpfr_set_zero(b, -1); emit_case(out, "adversarial", b, 24, MPFR_RNDN); mpfr_clear(b); }
        /* Inf with prec change. */
        { mpfr_t b; mpfr_init2(b, 53); mpfr_set_inf(b, -1); emit_case(out, "adversarial", b, 24, MPFR_RNDU); mpfr_clear(b); }
        /* NaN with prec change. */
        { mpfr_t b; mpfr_init2(b, 53); mpfr_set_nan(b); emit_case(out, "adversarial", b, 24, MPFR_RNDA); mpfr_clear(b); }
    }

    /* fuzz: 55 */
    {
        xs64_t rng;
        xs64_seed(&rng, 0x5B1F500053E70123ULL);
        for (int rep = 0; rep < 55; ++rep) {
            const uint64_t kind_sel = xs64_below(&rng, 10);
            const uint64_t srcPrec = 1 + xs64_below(&rng, 200);
            const uint64_t outPrec = 1 + xs64_below(&rng, 200);
            const uint64_t rnd_idx = xs64_below(&rng, 5);
            mpfr_t b;
            mpfr_init2(b, (mpfr_prec_t)srcPrec);
            if (kind_sel < 7) {
                const double mag = (double)(1 + xs64_below(&rng, 1000000));
                const int b_sign = (xs64_below(&rng, 2) == 0) ? 1 : -1;
                mpfr_set_d(b, b_sign * mag, MPFR_RNDN);
                if (!mpfr_regular_p(b)) {
                    mpfr_clear(b);
                    continue;
                }
            } else if (kind_sel == 7) {
                mpfr_set_zero(b, (xs64_below(&rng, 2) == 0) ? 1 : -1);
            } else if (kind_sel == 8) {
                mpfr_set_inf(b, (xs64_below(&rng, 2) == 0) ? 1 : -1);
            } else {
                mpfr_set_nan(b);
            }
            emit_case(out, "fuzz", b, (mpfr_prec_t)outPrec, RNDS[rnd_idx]);
            mpfr_clear(b);
        }
    }

    /* mined: 5 */
    {
        { mpfr_t b; mk_norm_d(b, 53, 3.14); emit_case(out, "mined", b, 53, MPFR_RNDN); mpfr_clear(b); }
        { mpfr_t b; mk_norm_d(b, 53, 1.0); emit_case(out, "mined", b, 113, MPFR_RNDN); mpfr_clear(b); }
        { mpfr_t b; mk_norm_d(b, 113, 1.0/3.0); emit_case(out, "mined", b, 24, MPFR_RNDN); mpfr_clear(b); }
        { mpfr_t b; mpfr_init2(b, 53); mpfr_set_zero(b, 1); emit_case(out, "mined", b, 24, MPFR_RNDN); mpfr_clear(b); }
        { mpfr_t b; mpfr_init2(b, 53); mpfr_set_nan(b); emit_case(out, "mined", b, 53, MPFR_RNDN); mpfr_clear(b); }
    }

    return 0;
}
