/*
 * golden_driver.c -- Golden master for MPFR's mpfr_sub_z.
 *
 * C signature: int mpfr_sub_z(mpfr_t y, mpfr_srcptr x, mpz_srcptr z,
 *                             mpfr_rnd_t r);
 *
 * Computes y = x - z at MPFR_PREC(y) bits per rnd.
 * Ref: mpfr/src/gmp_op.c L114-L121.
 *
 * Structurally mirrors the mpfr_add_z driver; same wire format and
 * tag distribution shape, just with mpfr_sub_z called instead.
 * Per ADR 0003 the TS port delegates to mpfr_set_z + mpfr_sub.
 */
#include "common.h"

#include <assert.h>
#include <inttypes.h>
#include <limits.h>
#include <string.h>

#if GMP_NUMB_BITS != 64
#  error "mpfr_sub_z golden_driver requires GMP_NUMB_BITS == 64"
#endif

#define TS_PREC_MAX ((uint64_t)((1ULL << 31) - 257ULL))
#define TS_PREC_MIN ((uint64_t)1)

static inline void jl_kv_mpz(FILE *f, int first, const char *key, mpz_srcptr z) {
    char *s = mpz_get_str(NULL, 10, z);
    jl_kv_str(f, first, key, s);
    void (*gmp_free)(void *, size_t);
    mp_get_memory_functions(NULL, NULL, &gmp_free);
    gmp_free(s, strlen(s) + 1);
}

static inline void emit_case(FILE *out, const char *tag,
                             mpfr_srcptr x, mpz_srcptr z,
                             uint64_t prec, mpfr_rnd_t rnd) {
    assert(prec >= TS_PREC_MIN && prec <= TS_PREC_MAX);
    mpfr_t y;
    mpfr_init2(y, (mpfr_prec_t)prec);

    const uint64_t t0 = now_ns();
    const int ternary = mpfr_sub_z(y, x, z, rnd);
    const uint64_t elapsed = now_ns() - t0;

    jl_begin(out, tag);
    jl_kv_mpfr(out, 1, "x", x);
    jl_kv_mpz(out, 0, "z", z);
    jl_kv_u64(out, 0, "prec", prec);
    jl_kv_rnd(out, 0, "rnd", rnd);
    jl_end_inputs(out);
    jl_output_result(out, y, ternary);
    jl_finish(out, elapsed);

    mpfr_clear(y);
}

static inline void emit_d_si(FILE *out, const char *tag,
                             double xd, uint64_t xprec,
                             long zn,
                             uint64_t prec, mpfr_rnd_t rnd) {
    mpfr_t x; mpfr_init2(x, (mpfr_prec_t)xprec); mpfr_set_d(x, xd, MPFR_RNDN);
    mpz_t z; mpz_init(z); mpz_set_si(z, zn);
    emit_case(out, tag, x, z, prec, rnd);
    mpz_clear(z);
    mpfr_clear(x);
}

static inline void emit_d_z(FILE *out, const char *tag,
                            double xd, uint64_t xprec,
                            mpz_srcptr z,
                            uint64_t prec, mpfr_rnd_t rnd) {
    mpfr_t x; mpfr_init2(x, (mpfr_prec_t)xprec); mpfr_set_d(x, xd, MPFR_RNDN);
    emit_case(out, tag, x, z, prec, rnd);
    mpfr_clear(x);
}

int main(void) {
    FILE *out = stdout;
    const mpfr_rnd_t RNDS[5] = {MPFR_RNDN, MPFR_RNDZ, MPFR_RNDU, MPFR_RNDD, MPFR_RNDA};

    /* ============================================================== */
    /* happy: 25 -- typical x - z.                                    */
    /* ============================================================== */
    {
        emit_d_si(out, "happy", 1.0, 53, 0, 53, MPFR_RNDN);
        emit_d_si(out, "happy", 1.0, 53, 1, 53, MPFR_RNDN);
        emit_d_si(out, "happy", 1.0, 53, -1, 53, MPFR_RNDN);
        emit_d_si(out, "happy", 100.0, 53, 100, 53, MPFR_RNDN);   /* -> 0 */
        emit_d_si(out, "happy", -100.0, 53, -100, 53, MPFR_RNDN); /* -> 0 */
        emit_d_si(out, "happy", 3.14, 53, 3, 53, MPFR_RNDN);
        emit_d_si(out, "happy", 3.14, 53, -3, 53, MPFR_RNDN);
        emit_d_si(out, "happy", -3.14, 53, 3, 53, MPFR_RNDN);
        emit_d_si(out, "happy", -3.14, 53, -3, 53, MPFR_RNDN);
        emit_d_si(out, "happy", 5.0, 53, 5, 53, MPFR_RNDN);       /* exact -> +0 */
        emit_d_si(out, "happy", 5.0, 53, 5, 53, MPFR_RNDD);       /* RNDD -> -0 */
        emit_d_si(out, "happy", 1.5, 53, 1000, 53, MPFR_RNDN);
        emit_d_si(out, "happy", 1.5, 53, -1000, 53, MPFR_RNDN);
        emit_d_si(out, "happy", 1.0/3.0, 53, 1, 24, MPFR_RNDN);
        emit_d_si(out, "happy", 1.0/3.0, 53, -1, 24, MPFR_RNDN);
        emit_d_si(out, "happy", -1.0/7.0, 53, 5, 24, MPFR_RNDN);
        emit_d_si(out, "happy", 100.0, 53, 1234567L, 53, MPFR_RNDN);
        emit_d_si(out, "happy", 0.0, 53, 17, 53, MPFR_RNDN);
        emit_d_si(out, "happy", 0.0, 53, -17, 53, MPFR_RNDN);
        emit_d_si(out, "happy", 17.0, 53, 0, 53, MPFR_RNDN);
        /* Singulars in x. */
        { mpfr_t x; mpfr_init2(x, 53); mpfr_set_nan(x);
          mpz_t z; mpz_init(z); mpz_set_si(z, 5);
          emit_case(out, "happy", x, z, 53, MPFR_RNDN);
          mpz_clear(z); mpfr_clear(x); }
        { mpfr_t x; mpfr_init2(x, 53); mpfr_set_inf(x, +1);
          mpz_t z; mpz_init(z); mpz_set_si(z, 5);
          emit_case(out, "happy", x, z, 53, MPFR_RNDN);
          mpz_clear(z); mpfr_clear(x); }
        { mpfr_t x; mpfr_init2(x, 53); mpfr_set_inf(x, -1);
          mpz_t z; mpz_init(z); mpz_set_si(z, 5);
          emit_case(out, "happy", x, z, 53, MPFR_RNDN);
          mpz_clear(z); mpfr_clear(x); }
        { mpfr_t x; mpfr_init2(x, 53); mpfr_set_zero(x, +1);
          mpz_t z; mpz_init(z); mpz_set_si(z, 5);
          emit_case(out, "happy", x, z, 53, MPFR_RNDN);
          mpz_clear(z); mpfr_clear(x); }
        { mpfr_t x; mpfr_init2(x, 53); mpfr_set_zero(x, -1);
          mpz_t z; mpz_init(z); mpz_set_si(z, 0);
          emit_case(out, "happy", x, z, 53, MPFR_RNDN);
          mpz_clear(z); mpfr_clear(x); }
    }

    /* ============================================================== */
    /* edge: 32 -- z=0 across rnds, +/-0-0, x=NaN/Inf, very large z. */
    /* ============================================================== */
    {
        /* (1-5) x=1.5, z=0 across all 5 rnds. */
        for (int i = 0; i < 5; ++i) emit_d_si(out, "edge", 1.5, 53, 0, 53, RNDS[i]);
        /* (6-10) +0 - 0 across all 5 rnds. */
        for (int i = 0; i < 5; ++i) {
            mpfr_t x; mpfr_init2(x, 53); mpfr_set_zero(x, +1);
            mpz_t z; mpz_init(z); mpz_set_si(z, 0);
            emit_case(out, "edge", x, z, 53, RNDS[i]);
            mpz_clear(z); mpfr_clear(x);
        }
        /* (11-15) -0 - 0 across all 5 rnds. */
        for (int i = 0; i < 5; ++i) {
            mpfr_t x; mpfr_init2(x, 53); mpfr_set_zero(x, -1);
            mpz_t z; mpz_init(z); mpz_set_si(z, 0);
            emit_case(out, "edge", x, z, 53, RNDS[i]);
            mpz_clear(z); mpfr_clear(x);
        }
        /* (16-20) x=NaN - various z. */
        for (int i = 0; i < 5; ++i) {
            mpfr_t x; mpfr_init2(x, 53); mpfr_set_nan(x);
            mpz_t z; mpz_init(z); mpz_set_si(z, (long)(i - 2));
            emit_case(out, "edge", x, z, 53, RNDS[i]);
            mpz_clear(z); mpfr_clear(x);
        }
        /* (21-25) x=+/-Inf - various z. */
        { mpfr_t x; mpfr_init2(x, 53); mpfr_set_inf(x, +1);
          mpz_t z; mpz_init(z); mpz_set_si(z, 100);
          emit_case(out, "edge", x, z, 53, MPFR_RNDN);
          mpz_clear(z); mpfr_clear(x); }
        { mpfr_t x; mpfr_init2(x, 53); mpfr_set_inf(x, +1);
          mpz_t z; mpz_init(z); mpz_set_si(z, -100);
          emit_case(out, "edge", x, z, 53, MPFR_RNDN);
          mpz_clear(z); mpfr_clear(x); }
        { mpfr_t x; mpfr_init2(x, 53); mpfr_set_inf(x, -1);
          mpz_t z; mpz_init(z); mpz_set_si(z, 100);
          emit_case(out, "edge", x, z, 53, MPFR_RNDN);
          mpz_clear(z); mpfr_clear(x); }
        { mpfr_t x; mpfr_init2(x, 53); mpfr_set_inf(x, -1);
          mpz_t z; mpz_init(z); mpz_set_si(z, -100);
          emit_case(out, "edge", x, z, 53, MPFR_RNDN);
          mpz_clear(z); mpfr_clear(x); }
        emit_d_si(out, "edge", 1.5, 1, 1, 1, MPFR_RNDN);
        /* (26-30) Very large z. */
        {
            mpz_t z; mpz_init(z);
            mpz_ui_pow_ui(z, 2UL, 1024UL);
            mpz_sub_ui(z, z, 1UL);
            emit_d_z(out, "edge", 1.0, 53, z, 53, MPFR_RNDN);
            emit_d_z(out, "edge", 1.0, 53, z, 1024, MPFR_RNDN);
            emit_d_z(out, "edge", 1.0, 53, z, 53, MPFR_RNDU);
            emit_d_z(out, "edge", 1.0, 53, z, 53, MPFR_RNDD);
            mpz_neg(z, z);
            emit_d_z(out, "edge", 1.0, 53, z, 53, MPFR_RNDN);
            mpz_clear(z);
        }
        /* (31-32) Catastrophic cancellation. */
        emit_d_si(out, "edge", 100.0, 53, 100, 3, MPFR_RNDN);   /* -> 0 */
        emit_d_si(out, "edge", 100.5, 53, 100, 3, MPFR_RNDU);
    }

    /* ============================================================== */
    /* adversarial: 10                                                */
    /* ============================================================== */
    {
        emit_d_si(out, "adversarial", 1.0, 53, 1, 53, MPFR_RNDN);   /* exact -> +0 */
        emit_d_si(out, "adversarial", 1.0, 53, 1, 53, MPFR_RNDD);   /* exact -> -0 */
        emit_d_si(out, "adversarial", 3.14, 53, 1, 24, MPFR_RNDU);
        emit_d_si(out, "adversarial", -3.14, 53, -1, 24, MPFR_RNDD);
        emit_d_si(out, "adversarial", 0.9999999999999999, 53, 1, 24, MPFR_RNDN);
        emit_d_si(out, "adversarial", 1e15, 53, 1, 53, MPFR_RNDN);
        emit_d_si(out, "adversarial", -1e15, 53, -1, 53, MPFR_RNDU);
        emit_d_si(out, "adversarial", 0.5, 53, 1, 1, MPFR_RNDU);
        emit_d_si(out, "adversarial", 1e-300, 53, -1, 53, MPFR_RNDN);
        /* tie. */
        emit_d_si(out, "adversarial", 1.5, 53, -2, 1, MPFR_RNDN);
    }

    /* ============================================================== */
    /* fuzz: 55                                                       */
    /* ============================================================== */
    {
        xs64_t rng;
        xs64_seed(&rng, 0xDEFEC8EDABBAABBAULL);
        const uint64_t precs[5] = { 24, 53, 64, 100, 200 };

        for (int rep = 0; rep < 55; ++rep) {
            const uint64_t u1 = xs64_next(&rng);
            double xd;
            memcpy(&xd, &u1, sizeof xd);
            if (xd != xd || xd == 0.0) xd = 1.0;
            if (xd > 1e100 || xd < -1e100) xd = xd / 1e100;
            if (xd < 1e-100 && xd > -1e-100) xd = xd * 1e100;

            const uint64_t xprec = precs[xs64_below(&rng, 5)];
            mpfr_t x; mpfr_init2(x, (mpfr_prec_t)xprec);
            mpfr_set_d(x, xd, MPFR_RNDN);

            const uint64_t blen = 1 + xs64_below(&rng, 200);
            mpz_t z; mpz_init(z);
            for (uint64_t off = 0; off < blen; off += 32) {
                const uint32_t chunk = (uint32_t)(xs64_next(&rng) & 0xFFFFFFFFu);
                mpz_mul_2exp(z, z, 32);
                mpz_add_ui(z, z, chunk);
            }
            {
                mpz_t mask; mpz_init(mask);
                mpz_ui_pow_ui(mask, 2UL, (unsigned long)blen);
                mpz_sub_ui(mask, mask, 1UL);
                mpz_and(z, z, mask);
                mpz_clear(mask);
            }
            if (xs64_next(&rng) & 1) mpz_neg(z, z);

            const uint64_t prec = precs[xs64_below(&rng, 5)];
            const mpfr_rnd_t rnd = RNDS[xs64_below(&rng, 5)];
            emit_case(out, "fuzz", x, z, prec, rnd);
            mpz_clear(z);
            mpfr_clear(x);
        }
    }

    /* ============================================================== */
    /* mined: 5 from mpfr/tests/tgmpop.c.                             */
    /* ============================================================== */
    {
        /* check_for_zero L203-L210: +/-0 - 0 across rnds. */
        { mpfr_t x; mpfr_init2(x, 53); mpfr_set_zero(x, +1);
          mpz_t z; mpz_init(z); mpz_set_si(z, 0);
          emit_case(out, "mined", x, z, 53, MPFR_RNDN);
          mpz_clear(z); mpfr_clear(x); }
        { mpfr_t x; mpfr_init2(x, 53); mpfr_set_zero(x, -1);
          mpz_t z; mpz_init(z); mpz_set_si(z, 0);
          emit_case(out, "mined", x, z, 53, MPFR_RNDD);
          mpz_clear(z); mpfr_clear(x); }
        /* reduced_expo_range L1128-L1135: x=20, z=3, expect y=17. */
        { mpfr_t x; mpfr_init2(x, 32); mpfr_set_ui(x, 20, MPFR_RNDN);
          mpz_t z; mpz_init(z); mpz_set_ui(z, 3);
          emit_case(out, "mined", x, z, 32, MPFR_RNDN);
          mpz_clear(z); mpfr_clear(x); }
        emit_d_si(out, "mined", 25.0, 53, 17, 53, MPFR_RNDN);
        emit_d_si(out, "mined", -25.0, 53, 17, 53, MPFR_RNDN);
    }

    return 0;
}
