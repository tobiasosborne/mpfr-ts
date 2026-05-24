/*
 * golden_driver.c — Golden master for MPFR's mpfr_set4.
 *
 * mpfr_set4(a, b, rnd, signb) copies b into a at a's precision, but with
 * the caller-supplied output sign signb (not b.sign), using signb for
 * the rounding direction. Inputs cover both same-prec (exact) and
 * prec-differing (lossy) paths, all 4 kinds (normal/zero/inf/nan), and
 * both signb values.
 *
 * C signature
 * -----------
 *
 *   int mpfr_set4(mpfr_ptr a, mpfr_srcptr b, mpfr_rnd_t rnd_mode, int signb);
 *
 * TS signature: mpfr_set4(b, prec, rnd, signb) -> Result.
 *
 * Wire format
 * -----------
 *
 *   {"tag":"<class>",
 *    "inputs":{"b":<MPFR>,"prec":"<dec>","rnd":"RND[NZUDA]","signb":<1|-1>},
 *    "output":{"value":<MPFR>,"ternary":<-1|0|1>},
 *    "time_ns":<n>}
 *
 *   - b via jl_kv_mpfr (full MPFR record).
 *   - prec via jl_kv_i64 (decimal string for bigint round-trip).
 *   - rnd via jl_kv_rnd.
 *   - signb via jl_kv_int (raw JSON int, 1 or -1).
 *   - Output via jl_output_result.
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
 * Build via eval/golden_master/build.sh.
 *
 * Ref: mpfr/src/set.c L25-L64 — C reference.
 * Ref: src/ops/set4.ts — production port.
 */
#include "common.h"

#include <assert.h>
#include <inttypes.h>
#include <string.h>

/* The 5 supported rounding modes (matches the TS port). */
static const mpfr_rnd_t RNDS[5] = {
    MPFR_RNDN, MPFR_RNDZ, MPFR_RNDU, MPFR_RNDD, MPFR_RNDA,
};

/* Emit one case at (b, prec, rnd, signb). */
static void
emit_case(FILE *out, const char *tag,
          mpfr_srcptr b, mpfr_prec_t prec, mpfr_rnd_t rnd, int signb) {
    assert(signb == 1 || signb == -1);
    assert(prec >= 1);

    mpfr_t a;
    mpfr_init2(a, prec);

    const uint64_t t0 = now_ns();
    const int ternary = mpfr_set4(a, b, rnd, signb);
    const uint64_t elapsed = now_ns() - t0;

    jl_begin(out, tag);
    jl_kv_mpfr(out, 1, "b", b);
    jl_kv_i64(out, 0, "prec", (int64_t)prec);
    jl_kv_rnd(out, 0, "rnd", rnd);
    jl_kv_int(out, 0, "signb", signb);
    jl_end_inputs(out);
    jl_output_result(out, a, ternary);
    jl_finish(out, elapsed);

    mpfr_clear(a);
}

/* Build a normal value from a double. */
static void mk_norm_d(mpfr_ptr b, mpfr_prec_t prec, double d) {
    mpfr_init2(b, prec);
    mpfr_set_d(b, d, MPFR_RNDN);
}

/* Build a normal value from a long, given sign and magnitude. */
static void mk_norm_si(mpfr_ptr b, mpfr_prec_t prec, long v) {
    mpfr_init2(b, prec);
    mpfr_set_si(b, v, MPFR_RNDN);
}

/* Build a normal value with an exact mantissa pattern. */
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

    /* ============================================================== */
    /* happy: 22 cases — common values across all kinds.              */
    /* ============================================================== */
    {
        /* Normal at prec=53, same prec, +signb. */
        {
            mpfr_t b;
            mk_norm_d(b, 53, 3.14);
            emit_case(out, "happy", b, 53, MPFR_RNDN, 1);
            mpfr_clear(b);
        }
        {
            mpfr_t b;
            mk_norm_d(b, 53, 3.14);
            emit_case(out, "happy", b, 53, MPFR_RNDN, -1);
            mpfr_clear(b);
        }
        {
            mpfr_t b;
            mk_norm_d(b, 53, -2.71);
            emit_case(out, "happy", b, 53, MPFR_RNDN, 1);
            mpfr_clear(b);
        }
        {
            mpfr_t b;
            mk_norm_d(b, 53, -2.71);
            emit_case(out, "happy", b, 53, MPFR_RNDN, -1);
            mpfr_clear(b);
        }
        /* Normal at prec=53, smaller output prec (lossy). */
        {
            mpfr_t b;
            mk_norm_d(b, 53, 3.14);
            emit_case(out, "happy", b, 24, MPFR_RNDN, 1);
            mpfr_clear(b);
        }
        {
            mpfr_t b;
            mk_norm_d(b, 53, 3.14);
            emit_case(out, "happy", b, 24, MPFR_RNDN, -1);
            mpfr_clear(b);
        }
        /* Normal, larger output prec (lossless pad). */
        {
            mpfr_t b;
            mk_norm_d(b, 24, 1.5);
            emit_case(out, "happy", b, 53, MPFR_RNDN, 1);
            mpfr_clear(b);
        }
        {
            mpfr_t b;
            mk_norm_d(b, 24, 1.5);
            emit_case(out, "happy", b, 53, MPFR_RNDN, -1);
            mpfr_clear(b);
        }
        /* Zero, both signs of b, both signb. */
        {
            mpfr_t b;
            mpfr_init2(b, 53);
            mpfr_set_zero(b, 1);
            emit_case(out, "happy", b, 53, MPFR_RNDN, 1);
            mpfr_clear(b);
        }
        {
            mpfr_t b;
            mpfr_init2(b, 53);
            mpfr_set_zero(b, 1);
            emit_case(out, "happy", b, 53, MPFR_RNDN, -1);
            mpfr_clear(b);
        }
        {
            mpfr_t b;
            mpfr_init2(b, 53);
            mpfr_set_zero(b, -1);
            emit_case(out, "happy", b, 53, MPFR_RNDN, 1);
            mpfr_clear(b);
        }
        /* Inf, both signs. */
        {
            mpfr_t b;
            mpfr_init2(b, 53);
            mpfr_set_inf(b, 1);
            emit_case(out, "happy", b, 53, MPFR_RNDN, 1);
            mpfr_clear(b);
        }
        {
            mpfr_t b;
            mpfr_init2(b, 53);
            mpfr_set_inf(b, 1);
            emit_case(out, "happy", b, 53, MPFR_RNDN, -1);
            mpfr_clear(b);
        }
        {
            mpfr_t b;
            mpfr_init2(b, 53);
            mpfr_set_inf(b, -1);
            emit_case(out, "happy", b, 53, MPFR_RNDN, 1);
            mpfr_clear(b);
        }
        /* NaN. */
        {
            mpfr_t b;
            mpfr_init2(b, 53);
            mpfr_set_nan(b);
            emit_case(out, "happy", b, 53, MPFR_RNDN, 1);
            mpfr_clear(b);
        }
        {
            mpfr_t b;
            mpfr_init2(b, 53);
            mpfr_set_nan(b);
            emit_case(out, "happy", b, 53, MPFR_RNDN, -1);
            mpfr_clear(b);
        }
        /* All 5 rounding modes at prec=24 on a value that needs rounding. */
        for (int i = 0; i < 5; ++i) {
            mpfr_t b;
            mk_norm_d(b, 53, 1.0 / 3.0);
            emit_case(out, "happy", b, 24, RNDS[i], 1);
            mpfr_clear(b);
        }
        /* Same value, -signb so rounding direction flips for asymmetric modes. */
        {
            mpfr_t b;
            mk_norm_d(b, 53, 1.0 / 3.0);
            emit_case(out, "happy", b, 24, MPFR_RNDU, -1);
            mpfr_clear(b);
        }
    }

    /* ============================================================== */
    /* edge: 32 cases — boundary precs, special exponents, signb flips */
    /* across the rounding modes that depend on sign.                  */
    /* ============================================================== */
    {
        /* prec=1 boundary. */
        for (int i = 0; i < 5; ++i) {
            mpfr_t b;
            mk_norm_si(b, 1, 1);
            emit_case(out, "edge", b, 1, RNDS[i], 1);
            mpfr_clear(b);
        }
        for (int i = 0; i < 5; ++i) {
            mpfr_t b;
            mk_norm_si(b, 1, 1);
            emit_case(out, "edge", b, 1, RNDS[i], -1);
            mpfr_clear(b);
        }
        /* Large prec → small prec (heavy rounding). */
        for (int i = 0; i < 5; ++i) {
            mpfr_t b;
            mk_norm_d(b, 113, 1.0 / 7.0);
            emit_case(out, "edge", b, 3, RNDS[i], 1);
            mpfr_clear(b);
        }
        for (int i = 0; i < 5; ++i) {
            mpfr_t b;
            mk_norm_d(b, 113, 1.0 / 7.0);
            emit_case(out, "edge", b, 3, RNDS[i], -1);
            mpfr_clear(b);
        }
        /* Same prec; signb same as b.sign → equivalent to mpfr_set. */
        {
            mpfr_t b;
            mk_norm_si(b, 53, 7);
            emit_case(out, "edge", b, 53, MPFR_RNDN, 1);
            mpfr_clear(b);
        }
        {
            mpfr_t b;
            mk_norm_si(b, 53, -7);
            emit_case(out, "edge", b, 53, MPFR_RNDN, -1);
            mpfr_clear(b);
        }
        /* Same prec; signb opposite of b.sign → equivalent to mpfr_neg. */
        {
            mpfr_t b;
            mk_norm_si(b, 53, 7);
            emit_case(out, "edge", b, 53, MPFR_RNDN, -1);
            mpfr_clear(b);
        }
        {
            mpfr_t b;
            mk_norm_si(b, 53, -7);
            emit_case(out, "edge", b, 53, MPFR_RNDN, 1);
            mpfr_clear(b);
        }
        /* Exact carry-out: round small prec where the carry bit triggers. */
        {
            mpfr_t b;
            mk_norm_exact(b, 8, "255", 8, 1);
            emit_case(out, "edge", b, 4, MPFR_RNDN, 1);
            mpfr_clear(b);
        }
        {
            mpfr_t b;
            mk_norm_exact(b, 8, "255", 8, 1);
            emit_case(out, "edge", b, 4, MPFR_RNDU, 1);
            mpfr_clear(b);
        }
        {
            mpfr_t b;
            mk_norm_exact(b, 8, "255", 8, 1);
            emit_case(out, "edge", b, 4, MPFR_RNDU, -1);
            mpfr_clear(b);
        }
        /* Carry-out across signb. */
        {
            mpfr_t b;
            mk_norm_exact(b, 8, "255", 8, -1);
            emit_case(out, "edge", b, 4, MPFR_RNDA, 1);
            mpfr_clear(b);
        }
        /* Powers of two at the prec boundary (mantissa = MSB only). */
        {
            mpfr_t b;
            mk_norm_si(b, 53, 1024);
            emit_case(out, "edge", b, 10, MPFR_RNDN, 1);
            mpfr_clear(b);
        }
        {
            mpfr_t b;
            mk_norm_si(b, 53, 1024);
            emit_case(out, "edge", b, 10, MPFR_RNDN, -1);
            mpfr_clear(b);
        }
    }

    /* ============================================================== */
    /* adversarial: 12 cases — rounding-boundary cases where signb     */
    /* matters and where the C path divergence (RNDU vs RNDD branches  */
    /* by signb) would silently break a port that misuses b.sign       */
    /* for rounding direction.                                         */
    /* ============================================================== */
    {
        /* Halfway-cancel: tie-break to even at lossy prec, both signs.
         * The RNDN tie-break must use the result sign (signb) since
         * the parity rule depends on the bit pattern after rounding. */
        {
            mpfr_t b;
            /* prec=5: mantissa = 0b11000 (24), exp = 5 → val = 24.
             * round to prec=3: 24 = 0b11000, msb 3 = 0b110, dropped 0b00.
             * exact. ternary = 0. */
            mk_norm_exact(b, 5, "24", 5, 1);
            emit_case(out, "adversarial", b, 3, MPFR_RNDN, 1);
            mpfr_clear(b);
        }
        /* RNDU with positive signb vs negative signb on same b: opposite
         * behaviour expected. */
        {
            mpfr_t b;
            mk_norm_d(b, 53, 1.0 / 7.0);
            emit_case(out, "adversarial", b, 4, MPFR_RNDU, 1);
            mpfr_clear(b);
        }
        {
            mpfr_t b;
            mk_norm_d(b, 53, 1.0 / 7.0);
            emit_case(out, "adversarial", b, 4, MPFR_RNDU, -1);
            mpfr_clear(b);
        }
        /* RNDD with positive vs negative signb. */
        {
            mpfr_t b;
            mk_norm_d(b, 53, 1.0 / 7.0);
            emit_case(out, "adversarial", b, 4, MPFR_RNDD, 1);
            mpfr_clear(b);
        }
        {
            mpfr_t b;
            mk_norm_d(b, 53, 1.0 / 7.0);
            emit_case(out, "adversarial", b, 4, MPFR_RNDD, -1);
            mpfr_clear(b);
        }
        /* Round-bit boundary at carry. */
        {
            mpfr_t b;
            /* prec=8 mant=255 (=2^8-1) → 4-bit rounds to 16, exp bumps. */
            mk_norm_exact(b, 8, "255", 8, 1);
            emit_case(out, "adversarial", b, 4, MPFR_RNDU, 1);
            mpfr_clear(b);
        }
        {
            mpfr_t b;
            mk_norm_exact(b, 8, "255", 8, 1);
            emit_case(out, "adversarial", b, 4, MPFR_RNDD, -1);
            mpfr_clear(b);
        }
        /* Half-bit tie with even LSB after truncation (round to even → truncate). */
        {
            mpfr_t b;
            /* prec=5 mant=20 (=0b10100, MSB-aligned) — bit 4 set, bit 2 set.
             * rounded to prec=3: 0b101 (= 5); dropped bits = 0b00 → exact. */
            mk_norm_exact(b, 5, "20", 5, 1);
            emit_case(out, "adversarial", b, 3, MPFR_RNDN, 1);
            mpfr_clear(b);
        }
        /* Inf with non-matching signb (signb flip). */
        {
            mpfr_t b;
            mpfr_init2(b, 53);
            mpfr_set_inf(b, 1);
            emit_case(out, "adversarial", b, 24, MPFR_RNDN, -1);
            mpfr_clear(b);
        }
        /* Zero with non-matching signb. */
        {
            mpfr_t b;
            mpfr_init2(b, 53);
            mpfr_set_zero(b, 1);
            emit_case(out, "adversarial", b, 24, MPFR_RNDN, -1);
            mpfr_clear(b);
        }
        /* NaN with signb=-1: schema collapses to NAN_VALUE. */
        {
            mpfr_t b;
            mpfr_init2(b, 53);
            mpfr_set_nan(b);
            emit_case(out, "adversarial", b, 24, MPFR_RNDA, -1);
            mpfr_clear(b);
        }
        /* Smallest negative normal then RNDU with +signb: rounded up
         * means less negative, ternary direction depends on signb. */
        {
            mpfr_t b;
            mk_norm_d(b, 53, -1.0 / 3.0);
            emit_case(out, "adversarial", b, 8, MPFR_RNDU, 1);
            mpfr_clear(b);
        }
    }

    /* ============================================================== */
    /* fuzz: 60 cases — PRNG-driven                                   */
    /* ============================================================== */
    {
        xs64_t rng;
        xs64_seed(&rng, 0x5E74C0DEF1F4ULL);

        for (int rep = 0; rep < 60; ++rep) {
            /* Random kind selector (0=normal, 1=zero, 2=inf, 3=nan, with
             * higher weighting on normal for coverage). */
            const uint64_t kind_sel = xs64_below(&rng, 10);
            /* prec for input b in [1, 200]. */
            const uint64_t srcPrec = 1 + xs64_below(&rng, 200);
            /* output prec in [1, 200]. */
            const uint64_t outPrec = 1 + xs64_below(&rng, 200);
            const int signb = (xs64_below(&rng, 2) == 0) ? 1 : -1;
            const uint64_t rnd_idx = xs64_below(&rng, 5);

            mpfr_t b;
            if (kind_sel < 7) {
                /* normal */
                mpfr_init2(b, (mpfr_prec_t)srcPrec);
                const double mag = (double)(1 + xs64_below(&rng, 1000000));
                const int b_sign = (xs64_below(&rng, 2) == 0) ? 1 : -1;
                mpfr_set_d(b, b_sign * mag, MPFR_RNDN);
                if (!mpfr_regular_p(b)) {
                    /* tiny prec might round to zero; skip */
                    mpfr_clear(b);
                    continue;
                }
            } else if (kind_sel == 7) {
                mpfr_init2(b, (mpfr_prec_t)srcPrec);
                mpfr_set_zero(b, (xs64_below(&rng, 2) == 0) ? 1 : -1);
            } else if (kind_sel == 8) {
                mpfr_init2(b, (mpfr_prec_t)srcPrec);
                mpfr_set_inf(b, (xs64_below(&rng, 2) == 0) ? 1 : -1);
            } else {
                mpfr_init2(b, (mpfr_prec_t)srcPrec);
                mpfr_set_nan(b);
            }
            emit_case(out, "fuzz", b, (mpfr_prec_t)outPrec, RNDS[rnd_idx], signb);
            mpfr_clear(b);
        }
    }

    /* ============================================================== */
    /* mined: 5 cases — representative shapes from mpfr/tests/tset.c   */
    /* and the abs/neg/setsign delegators (which are the typical entry */
    /* paths to mpfr_set4 in real workloads).                          */
    /* ============================================================== */
    {
        /* mpfr_set: signb = SIGN(b). */
        {
            mpfr_t b;
            mk_norm_d(b, 53, 3.14);
            emit_case(out, "mined", b, 53, MPFR_RNDN, 1);
            mpfr_clear(b);
        }
        /* mpfr_abs: signb = +1 regardless of b. */
        {
            mpfr_t b;
            mk_norm_d(b, 53, -3.14);
            emit_case(out, "mined", b, 53, MPFR_RNDN, 1);
            mpfr_clear(b);
        }
        /* mpfr_neg: signb = -SIGN(b). */
        {
            mpfr_t b;
            mk_norm_d(b, 53, 2.5);
            emit_case(out, "mined", b, 53, MPFR_RNDN, -1);
            mpfr_clear(b);
        }
        /* mpfr_setsign with caller-chosen sign. */
        {
            mpfr_t b;
            mk_norm_d(b, 53, 5.0);
            emit_case(out, "mined", b, 53, MPFR_RNDN, -1);
            mpfr_clear(b);
        }
        /* Round small at a non-matching signb. */
        {
            mpfr_t b;
            mk_norm_d(b, 53, 1.0 / 3.0);
            emit_case(out, "mined", b, 8, MPFR_RNDU, -1);
            mpfr_clear(b);
        }
    }

    return 0;
}
