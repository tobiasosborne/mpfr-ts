/*
 * golden_driver.c -- Golden master for MPFR's mpfr_set_z_2exp.
 *
 * C signature
 * -----------
 *
 *   int mpfr_set_z_2exp(mpfr_t f, mpz_srcptr z, mpfr_exp_t e, mpfr_rnd_t rnd);
 *
 *   Sets f = z * 2^e, correctly rounded to MPFR_PREC(f) bits per rnd.
 *   Ref: mpfr/src/set_z_2exp.c L27-L198.
 *
 * Divergence from C -> TS
 * -----------------------
 *
 * TS surface (per ADR 0003): mpfr_set_z_2exp(z, e, prec, rnd) -> Result,
 * with z and e as JS bigints. Wire format encodes both z and e as
 * signed decimal strings; the TS-side decodeInputValue
 * (eval/harness/value_codec.ts L176-L178) decodes "^-?\d+$" strings to
 * BigInt. e is well within int64 range across the golden so the wire
 * encoding (jl_kv_i64) is sound.
 *
 * Wire format
 * -----------
 *
 *   {"tag":"<class>",
 *    "inputs":{"z":"<arbitrary-decimal>",
 *              "e":"<int64-decimal>",
 *              "prec":"<prec-decimal>","rnd":"RND[NZUDA]"},
 *    "output":{"value":<MPFR-record>,"ternary":<-1|0|1>},
 *    "time_ns":<n>}
 *
 * Tag distribution
 * ----------------
 *
 *   happy        :  ~25  small bigints across common (e, prec) pairs
 *   edge         :  ~50  z=0 (e ignored), z=+/-1, z=+/-2^k, very large
 *                        z, prec<bitLength, e at int64 boundaries,
 *                        negative e
 *   adversarial  :  ~30  tie-rounding patterns at the prec boundary,
 *                        e shifted across that boundary
 *   fuzz         :   55  random (z, e, prec, rnd) tuples
 *   mined        :    5  from mpfr/tests/tset_z_2exp.c
 *
 * NOTE on emax/emin: the C reference clamps against the active emax/
 * emin. We keep e well within [-1000, 1000] so the C and TS outputs
 * agree across the whole golden (default emax is approximately 2^30).
 */
#include "common.h"

#include <assert.h>
#include <inttypes.h>
#include <limits.h>
#include <string.h>

#if GMP_NUMB_BITS != 64
#  error "mpfr_set_z_2exp golden_driver requires GMP_NUMB_BITS == 64"
#endif

#define TS_PREC_MAX ((uint64_t)((1ULL << 31) - 257ULL))
#define TS_PREC_MIN ((uint64_t)1)

/* Emit ,"z":"<decimal>" -- sign included; matches the TS-side
 * isDecimalIntegerString regex. Same helper shape as mpfr_set_z's
 * golden_driver.c. */
static inline void jl_kv_mpz(FILE *f, int first, const char *key, mpz_srcptr z) {
    char *s = mpz_get_str(NULL, 10, z);
    jl_kv_str(f, first, key, s);
    void (*gmp_free)(void *, size_t);
    mp_get_memory_functions(NULL, NULL, &gmp_free);
    gmp_free(s, strlen(s) + 1);
}

/* Emit one mpfr_set_z_2exp case. Times only the mpfr_set_z_2exp call. */
static inline void emit_case(FILE *out, const char *tag,
                             mpz_srcptr z, long e,
                             uint64_t prec, mpfr_rnd_t rnd) {
    assert(prec >= TS_PREC_MIN && prec <= TS_PREC_MAX);
    mpfr_t rop;
    mpfr_init2(rop, (mpfr_prec_t)prec);

    const uint64_t t0 = now_ns();
    const int ternary = mpfr_set_z_2exp(rop, z, (mpfr_exp_t)e, rnd);
    const uint64_t elapsed = now_ns() - t0;

    jl_begin(out, tag);
    jl_kv_mpz(out, 1, "z", z);
    jl_kv_i64(out, 0, "e", (int64_t)e);
    jl_kv_u64(out, 0, "prec", prec);
    jl_kv_rnd(out, 0, "rnd", rnd);
    jl_end_inputs(out);
    jl_output_result(out, rop, ternary);
    jl_finish(out, elapsed);

    mpfr_clear(rop);
}

/* Convenience for small signed ints. */
static inline void emit_si(FILE *out, const char *tag,
                           long n, long e, uint64_t prec, mpfr_rnd_t rnd) {
    mpz_t z; mpz_init(z); mpz_set_si(z, n);
    emit_case(out, tag, z, e, prec, rnd);
    mpz_clear(z);
}

/* Convenience for 2^k. */
static inline void emit_pow2(FILE *out, const char *tag,
                             unsigned long k, long e,
                             uint64_t prec, mpfr_rnd_t rnd, int negate) {
    mpz_t z; mpz_init(z);
    mpz_ui_pow_ui(z, 2UL, k);
    if (negate) mpz_neg(z, z);
    emit_case(out, tag, z, e, prec, rnd);
    mpz_clear(z);
}

/* Convenience for 2^k - 1 (all lower-k bits set). */
static inline void emit_pow2_minus_one(FILE *out, const char *tag,
                                       unsigned long k, long e,
                                       uint64_t prec, mpfr_rnd_t rnd,
                                       int negate) {
    mpz_t z; mpz_init(z);
    mpz_ui_pow_ui(z, 2UL, k);
    mpz_sub_ui(z, z, 1UL);
    if (negate) mpz_neg(z, z);
    emit_case(out, tag, z, e, prec, rnd);
    mpz_clear(z);
}

int main(void) {
    FILE *out = stdout;
    const mpfr_rnd_t RNDS[5] = {MPFR_RNDN, MPFR_RNDZ, MPFR_RNDU, MPFR_RNDD, MPFR_RNDA};

    /* ============================================================== */
    /* happy: ~25 -- small ints across common (e, prec) pairs.        */
    /* ============================================================== */
    {
        emit_si(out, "happy", 0, 0, 53, MPFR_RNDN);
        emit_si(out, "happy", 0, 10, 53, MPFR_RNDN);
        emit_si(out, "happy", 1, 0, 53, MPFR_RNDN);
        emit_si(out, "happy", 1, 1, 53, MPFR_RNDN);
        emit_si(out, "happy", 1, 10, 53, MPFR_RNDN);
        emit_si(out, "happy", -1, 10, 53, MPFR_RNDN);
        emit_si(out, "happy", 1, -1, 53, MPFR_RNDN);
        emit_si(out, "happy", 1, -10, 53, MPFR_RNDN);
        emit_si(out, "happy", 17, 5, 53, MPFR_RNDN);
        emit_si(out, "happy", -17, 5, 53, MPFR_RNDN);
        emit_si(out, "happy", 42, 0, 53, MPFR_RNDN);
        emit_si(out, "happy", 42, 100, 53, MPFR_RNDN);
        emit_si(out, "happy", 42, -100, 53, MPFR_RNDN);
        emit_si(out, "happy", 100, 0, 53, MPFR_RNDN);
        emit_si(out, "happy", -100, 0, 53, MPFR_RNDN);
        emit_si(out, "happy", 1234567L, 20, 53, MPFR_RNDN);
        emit_si(out, "happy", -1234567L, 20, 53, MPFR_RNDN);
        emit_si(out, "happy", 1000000000L, 0, 53, MPFR_RNDN);
        emit_si(out, "happy", 1L << 32, 16, 64, MPFR_RNDN);
        emit_si(out, "happy", 3, 0, 53, MPFR_RNDN);
        emit_si(out, "happy", 5, 0, 53, MPFR_RNDN);
        emit_si(out, "happy", 7, 0, 53, MPFR_RNDN);
        emit_si(out, "happy", 17, 0, 24, MPFR_RNDN);
        emit_si(out, "happy", 17, 0, 64, MPFR_RNDN);
        emit_si(out, "happy", 17, 0, 200, MPFR_RNDN);
    }

    /* ============================================================== */
    /* edge: ~50 -- z=0 across rnds (e ignored), +/-1 across rnds,    */
    /* +/-2^k at narrow prec, very large z, e at int64-ish boundaries,*/
    /* very negative e.                                               */
    /* ============================================================== */
    {
        /* (1-10) z=0 across all 5 rnds with two different e values --
         * confirms e is ignored and sign is forced positive. */
        for (int i = 0; i < 5; ++i) emit_si(out, "edge", 0, 1000, 53, RNDS[i]);
        for (int i = 0; i < 5; ++i) emit_si(out, "edge", 0, -1000, 53, RNDS[i]);

        /* (11-20) +/-1 across all 5 rnds with e=0. */
        for (int i = 0; i < 5; ++i) emit_si(out, "edge", 1, 0, 53, RNDS[i]);
        for (int i = 0; i < 5; ++i) emit_si(out, "edge", -1, 0, 53, RNDS[i]);

        /* (21-30) +/-2^k for k in {1, 10, 50, 200, 500} -- exact at any
         * prec >= 1 since z is a single bit. e shifts the exponent. */
        emit_pow2(out, "edge", 1,    0, 1, MPFR_RNDN, 0);
        emit_pow2(out, "edge", 1,   10, 1, MPFR_RNDN, 0);
        emit_pow2(out, "edge", 10,   5, 1, MPFR_RNDN, 0);
        emit_pow2(out, "edge", 50,  -5, 1, MPFR_RNDN, 0);
        emit_pow2(out, "edge", 200,  0, 1, MPFR_RNDN, 0);
        emit_pow2(out, "edge", 500,  0, 53, MPFR_RNDN, 0);
        emit_pow2(out, "edge", 500,  0, 53, MPFR_RNDN, 1);
        emit_pow2(out, "edge", 50,   3, 1, MPFR_RNDN, 1);
        emit_pow2(out, "edge", 10, -10, 53, MPFR_RNDN, 0);
        emit_pow2(out, "edge", 10, -10, 53, MPFR_RNDN, 1);

        /* (31-40) 2^k - 1 forcing full-mantissa rounding at prec < k. */
        emit_pow2_minus_one(out, "edge", 10, 0, 5, MPFR_RNDN, 0);
        emit_pow2_minus_one(out, "edge", 10, 0, 5, MPFR_RNDZ, 0);
        emit_pow2_minus_one(out, "edge", 10, 0, 5, MPFR_RNDU, 0);
        emit_pow2_minus_one(out, "edge", 10, 0, 5, MPFR_RNDA, 0);
        emit_pow2_minus_one(out, "edge", 100, 5, 53, MPFR_RNDN, 0);
        emit_pow2_minus_one(out, "edge", 100, -5, 53, MPFR_RNDA, 0);
        emit_pow2_minus_one(out, "edge", 200, 0, 100, MPFR_RNDN, 0);
        emit_pow2_minus_one(out, "edge", 200, 0, 100, MPFR_RNDD, 1);
        emit_pow2_minus_one(out, "edge", 200, 0, 100, MPFR_RNDU, 1);
        emit_pow2_minus_one(out, "edge", 500, 10, 200, MPFR_RNDN, 0);

        /* (41-45) e at int64-ish but safe-for-default-emax range. */
        emit_si(out, "edge", 1, 1000, 53, MPFR_RNDN);
        emit_si(out, "edge", 1, -1000, 53, MPFR_RNDN);
        emit_si(out, "edge", 17, 1000, 53, MPFR_RNDN);
        emit_si(out, "edge", 17, -1000, 53, MPFR_RNDN);
        emit_si(out, "edge", -17, 1000, 53, MPFR_RNDN);

        /* (46-50) Very large bit-length stress (~3000 bits) with non-
         * zero e. */
        {
            mpz_t z; mpz_init(z);
            mpz_ui_pow_ui(z, 2UL, 3000UL);
            mpz_sub_ui(z, z, 1UL);
            emit_case(out, "edge", z, 100, 100, MPFR_RNDN);
            emit_case(out, "edge", z, -100, 1000, MPFR_RNDN);
            emit_case(out, "edge", z, 0, 100, MPFR_RNDU);
            emit_case(out, "edge", z, 0, 100, MPFR_RNDZ);
            mpz_neg(z, z);
            emit_case(out, "edge", z, 50, 100, MPFR_RNDN);
            mpz_clear(z);
        }
    }

    /* ============================================================== */
    /* adversarial: ~30 -- tie-rounding patterns at the prec boundary */
    /* with various e shifts.                                         */
    /* ============================================================== */
    {
        const long patterns[] = {
            0xB,   /* 1011 = 11 */
            0xD,   /* 1101 = 13 */
            0xF,   /* 1111 = 15 */
            0x11,  /* 10001 = 17 */
            0x1F,  /* 11111 = 31 */
        };
        const size_t n_pat = sizeof(patterns) / sizeof(patterns[0]);
        const long es[] = { 0, 5, -5 };
        const size_t n_e = sizeof(es) / sizeof(es[0]);
        for (size_t p = 0; p < n_pat; ++p) {
            for (size_t ei = 0; ei < n_e; ++ei) {
                for (int r = 0; r < 5; ++r) {
                    /* Use prec=3 so the tie cases truly bite. */
                    if (p * n_e * 5 + ei * 5 + r < 30) {
                        emit_si(out, "adversarial", patterns[p], es[ei], 3, RNDS[r]);
                    }
                }
            }
        }
    }

    /* ============================================================== */
    /* fuzz: 55 random (z, e, prec, rnd) tuples.                      */
    /* ============================================================== */
    {
        xs64_t rng;
        xs64_seed(&rng, 0x5E72E72E5E72E72EULL);
        const uint64_t precs[7] = { 1, 2, 24, 53, 64, 100, 256 };

        for (int rep = 0; rep < 55; ++rep) {
            const uint64_t blen = 1 + xs64_below(&rng, 400);
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
            mpz_setbit(z, (unsigned long)(blen - 1));
            if (xs64_next(&rng) & 1) mpz_neg(z, z);

            /* e in [-500, 500] so we stay safely inside the default
             * emax of approximately 2^30. */
            const long e = (long)(xs64_below(&rng, 1001)) - 500L;
            const uint64_t prec = precs[xs64_below(&rng, 7)];
            const mpfr_rnd_t rnd = RNDS[xs64_below(&rng, 5)];
            emit_case(out, "fuzz", z, e, prec, rnd);
            mpz_clear(z);
        }
    }

    /* ============================================================== */
    /* mined: 5 from mpfr/tests/tset_z_2exp.c.                        */
    /* ============================================================== */
    {
        /* tset_z_2exp.c check0() L66-L83: z=0 at random e across rnds.
         * We capture one per rnd. */
        emit_si(out, "mined", 0, 100, 53, MPFR_RNDN);
        emit_si(out, "mined", 0, -100, 53, MPFR_RNDZ);
        /* tset_z_2exp.c check() L100-L130: small i with random e,
         * precision is sizeof(long)*8 so the result is exact. We pin
         * to a representative (i, e). */
        emit_si(out, "mined", 17, 5, 64, MPFR_RNDN);
        emit_si(out, "mined", -42, 10, 64, MPFR_RNDN);
        /* tset_z_2exp.c check_huge() L210-L215: 17 << 0x7ffffff0, then
         * mpfr_set_z_2exp with e = -0x7ffffff0 -- recovers 17. We use
         * a smaller representative (17 << 100, e = -100). */
        {
            mpz_t z; mpz_init(z);
            mpz_set_ui(z, 17UL);
            mpz_mul_2exp(z, z, 100);
            mpz_add_ui(z, z, 1UL);
            emit_case(out, "mined", z, -100, 53, MPFR_RNDN);
            mpz_clear(z);
        }
    }

    return 0;
}
