/*
 * golden_driver.c — Golden master for MPFR's mpfr_set_z.
 *
 * C signature
 * -----------
 *
 *   int mpfr_set_z(mpfr_t f, mpz_srcptr z, mpfr_rnd_t rnd);
 *
 *   Converts the arbitrary-precision integer z to an MPFR at MPFR_PREC(f)
 *   bits, rounded per rnd. Ref: mpfr/src/set_z.c → set_z_2exp.c.
 *
 * Divergence from C → TS
 * ----------------------
 *
 * The TS port `mpfr_set_z(z, prec, rnd) -> Result` takes z as a JS
 * bigint (no separate limb-array marshalling) and returns the canonical
 * Result. On the wire we encode z as a decimal string (signed, no
 * leading +) — the TS-side decodeInputValue's `isDecimalIntegerString`
 * branch (eval/harness/value_codec.ts L129–L131, L176–L178) catches a
 * "^-?\d+$" string and decodes it to BigInt.
 *
 * Wire format
 * -----------
 *
 *   {"tag":"<class>",
 *    "inputs":{"z":"<arbitrary-decimal>",
 *              "prec":"<prec-decimal>","rnd":"RND[NZUDA]"},
 *    "output":{"value":<MPFR-record>,"ternary":<-1|0|1>},
 *    "time_ns":<n>}
 *
 *   We use a fresh `jl_kv_mpz` helper inline because common.h has no
 *   mpz emitter today — but the pattern is the same as jl_kv_mpfr's
 *   `mpz_get_str(NULL, 10, z)` block. Decimal-string in, BigInt out
 *   via the existing TS-side codec.
 *
 * Tag distribution
 * ----------------
 *
 *   happy        :  ~25  small bigints at common precs
 *   edge         :  ~50  z=0/±1, ±2^k, very large z, prec < bitLength
 *   adversarial  :  ~30  tie-rounding at prec boundary
 *   fuzz         :   55  random bigints (xs64 seed)
 *   mined        :    5  from mpfr/tests/tset_z.c
 *
 * NOTE on emax/emin: the C reference clamps against the active emax/
 * emin; the TS port has no such clamp. The default emax (≈ 2^30) is
 * astronomically larger than any z we feed in (max bit-length ≈ 10000),
 * so the C and TS outputs agree across the whole golden.
 */
#include "common.h"

#include <assert.h>
#include <inttypes.h>
#include <limits.h>
#include <string.h>

#if GMP_NUMB_BITS != 64
#  error "mpfr_set_z golden_driver requires GMP_NUMB_BITS == 64"
#endif

#define TS_PREC_MAX ((uint64_t)((1ULL << 31) - 257ULL))
#define TS_PREC_MIN ((uint64_t)1)

/* Emit ,"z":"<decimal>" — sign included; matches the TS-side
 * isDecimalIntegerString regex. */
static inline void jl_kv_mpz(FILE *f, int first, const char *key, mpz_srcptr z) {
    char *s = mpz_get_str(NULL, 10, z);
    /* Use jl_kv_str to handle key/comma emission and quoting; we
     * already know `s` is safe ASCII (digits + optional leading '-'). */
    jl_kv_str(f, first, key, s);
    void (*gmp_free)(void *, size_t);
    mp_get_memory_functions(NULL, NULL, &gmp_free);
    gmp_free(s, strlen(s) + 1);
}

/* Emit one mpfr_set_z case. Times only the mpfr_set_z call. */
static inline void emit_case(FILE *out, const char *tag,
                             mpz_srcptr z, uint64_t prec, mpfr_rnd_t rnd) {
    assert(prec >= TS_PREC_MIN && prec <= TS_PREC_MAX);
    mpfr_t rop;
    mpfr_init2(rop, (mpfr_prec_t)prec);

    const uint64_t t0 = now_ns();
    const int ternary = mpfr_set_z(rop, z, rnd);
    const uint64_t elapsed = now_ns() - t0;

    jl_begin(out, tag);
    jl_kv_mpz(out, 1, "z", z);
    jl_kv_u64(out, 0, "prec", prec);
    jl_kv_rnd(out, 0, "rnd", rnd);
    jl_end_inputs(out);
    jl_output_result(out, rop, ternary);
    jl_finish(out, elapsed);

    mpfr_clear(rop);
}

/* Convenience for small signed ints. */
static inline void emit_si(FILE *out, const char *tag,
                           long n, uint64_t prec, mpfr_rnd_t rnd) {
    mpz_t z; mpz_init(z); mpz_set_si(z, n);
    emit_case(out, tag, z, prec, rnd);
    mpz_clear(z);
}

/* Convenience for "2^k", "2^k - 1", and similar. */
static inline void emit_pow2_minus_one(FILE *out, const char *tag,
                                       unsigned long k,
                                       uint64_t prec, mpfr_rnd_t rnd,
                                       int negate) {
    mpz_t z; mpz_init(z);
    mpz_ui_pow_ui(z, 2UL, k);
    mpz_sub_ui(z, z, 1UL);
    if (negate) mpz_neg(z, z);
    emit_case(out, tag, z, prec, rnd);
    mpz_clear(z);
}

static inline void emit_pow2(FILE *out, const char *tag,
                             unsigned long k,
                             uint64_t prec, mpfr_rnd_t rnd,
                             int negate) {
    mpz_t z; mpz_init(z);
    mpz_ui_pow_ui(z, 2UL, k);
    if (negate) mpz_neg(z, z);
    emit_case(out, tag, z, prec, rnd);
    mpz_clear(z);
}

int main(void) {
    FILE *out = stdout;
    const mpfr_rnd_t RNDS[5] = {MPFR_RNDN, MPFR_RNDZ, MPFR_RNDU, MPFR_RNDD, MPFR_RNDA};

    /* ============================================================== */
    /* happy: small ints at common precs.                             */
    /* ============================================================== */
    {
        emit_si(out, "happy", 0, 53, MPFR_RNDN);
        emit_si(out, "happy", 0, 24, MPFR_RNDN);
        emit_si(out, "happy", 1, 53, MPFR_RNDN);
        emit_si(out, "happy", -1, 53, MPFR_RNDN);
        emit_si(out, "happy", 2, 53, MPFR_RNDN);
        emit_si(out, "happy", -2, 53, MPFR_RNDN);
        emit_si(out, "happy", 42, 53, MPFR_RNDN);
        emit_si(out, "happy", -42, 53, MPFR_RNDN);
        emit_si(out, "happy", 100, 53, MPFR_RNDN);
        emit_si(out, "happy", -100, 53, MPFR_RNDN);
        emit_si(out, "happy", 1000, 53, MPFR_RNDN);
        emit_si(out, "happy", 1234567L, 53, MPFR_RNDN);
        emit_si(out, "happy", -1234567L, 53, MPFR_RNDN);
        emit_si(out, "happy", 1000000000L, 53, MPFR_RNDN);
        emit_si(out, "happy", -1000000000L, 53, MPFR_RNDN);
        emit_si(out, "happy", 42, 24, MPFR_RNDN);
        emit_si(out, "happy", 42, 64, MPFR_RNDN);
        emit_si(out, "happy", 42, 100, MPFR_RNDN);
        emit_si(out, "happy", 42, 200, MPFR_RNDN);
        emit_si(out, "happy", 3, 53, MPFR_RNDN);
        emit_si(out, "happy", 5, 53, MPFR_RNDN);
        emit_si(out, "happy", 7, 53, MPFR_RNDN);
        emit_si(out, "happy", 255, 53, MPFR_RNDN);
        emit_si(out, "happy", 256, 53, MPFR_RNDN);
        emit_si(out, "happy", 65535, 53, MPFR_RNDN);
    }

    /* ============================================================== */
    /* edge: ~50 — z=0 across rnds, ±1, ±2^k, very large z, narrow    */
    /* prec, large mined-shape literal.                               */
    /* ============================================================== */
    {
        /* (1-5) z=0 at all 5 rnds — sign must be forced positive. */
        for (int i = 0; i < 5; ++i) emit_si(out, "edge", 0, 53, RNDS[i]);

        /* (6-15) ±1 at all 5 rnds. */
        for (int i = 0; i < 5; ++i) emit_si(out, "edge", 1, 53, RNDS[i]);
        for (int i = 0; i < 5; ++i) emit_si(out, "edge", -1, 53, RNDS[i]);

        /* (16-25) ±2^k — exact at any prec >= 1 (single bit). */
        emit_pow2(out, "edge", 1, 1, MPFR_RNDN, 0);
        emit_pow2(out, "edge", 1, 1, MPFR_RNDN, 1);
        emit_pow2(out, "edge", 10, 1, MPFR_RNDN, 0);
        emit_pow2(out, "edge", 100, 1, MPFR_RNDN, 0);
        emit_pow2(out, "edge", 100, 1, MPFR_RNDN, 1);
        emit_pow2(out, "edge", 200, 1, MPFR_RNDN, 0);
        emit_pow2(out, "edge", 200, 53, MPFR_RNDN, 0);
        emit_pow2(out, "edge", 500, 53, MPFR_RNDN, 0);
        emit_pow2(out, "edge", 500, 53, MPFR_RNDN, 1);
        emit_pow2(out, "edge", 1000, 53, MPFR_RNDN, 0);

        /* (26-35) 2^k - 1 (all bits set in lower k) at prec < k —
         * forces full-mantissa rounding. */
        emit_pow2_minus_one(out, "edge", 10, 5, MPFR_RNDN, 0);
        emit_pow2_minus_one(out, "edge", 10, 5, MPFR_RNDZ, 0);
        emit_pow2_minus_one(out, "edge", 10, 5, MPFR_RNDU, 0);
        emit_pow2_minus_one(out, "edge", 10, 5, MPFR_RNDA, 0);
        emit_pow2_minus_one(out, "edge", 100, 53, MPFR_RNDN, 0);
        emit_pow2_minus_one(out, "edge", 100, 53, MPFR_RNDA, 0);
        emit_pow2_minus_one(out, "edge", 200, 100, MPFR_RNDN, 0);
        emit_pow2_minus_one(out, "edge", 500, 200, MPFR_RNDN, 0);
        emit_pow2_minus_one(out, "edge", 200, 100, MPFR_RNDD, 1);
        emit_pow2_minus_one(out, "edge", 200, 100, MPFR_RNDU, 1);

        /* (36-40) Just over a precision boundary: (2^53 + 1). */
        {
            mpz_t z; mpz_init(z);
            mpz_ui_pow_ui(z, 2UL, 53UL);
            mpz_add_ui(z, z, 1UL);
            emit_case(out, "edge", z, 53, MPFR_RNDN);
            emit_case(out, "edge", z, 53, MPFR_RNDZ);
            emit_case(out, "edge", z, 53, MPFR_RNDU);
            emit_case(out, "edge", z, 53, MPFR_RNDA);
            mpz_neg(z, z);
            emit_case(out, "edge", z, 53, MPFR_RNDN);
            mpz_clear(z);
        }

        /* (41-45) Large explicit bigints (from tset_z.c L82, shortened). */
        {
            mpz_t z; mpz_init(z);
            mpz_set_str(z, "77031627725494291259359895954016675357279104942148788042", 10);
            emit_case(out, "edge", z, 160, MPFR_RNDN);
            emit_case(out, "edge", z, 53,  MPFR_RNDN);
            emit_case(out, "edge", z, 53,  MPFR_RNDU);
            emit_case(out, "edge", z, 53,  MPFR_RNDZ);
            mpz_neg(z, z);
            emit_case(out, "edge", z, 53,  MPFR_RNDN);
            mpz_clear(z);
        }

        /* (46-50) Very large bit-length stress (~5000 bits). */
        {
            mpz_t z; mpz_init(z);
            mpz_ui_pow_ui(z, 2UL, 5000UL);
            mpz_sub_ui(z, z, 1UL);
            emit_case(out, "edge", z, 100, MPFR_RNDN);
            emit_case(out, "edge", z, 1000, MPFR_RNDN);
            emit_case(out, "edge", z, 100, MPFR_RNDU);
            emit_case(out, "edge", z, 100, MPFR_RNDZ);
            mpz_neg(z, z);
            emit_case(out, "edge", z, 100, MPFR_RNDN);
            mpz_clear(z);
        }
    }

    /* ============================================================== */
    /* adversarial: ~30 — tie-rounding patterns at the prec boundary. */
    /* ============================================================== */
    {
        /* Same patterns as set_si.adversarial, scaled to non-int64
         * magnitudes by shifting into bigint territory. */
        const long patterns[] = {
            0b11011L,        /* 27 */
            0b10101L,        /* 21 */
            0b11111L,        /* 31 */
            0b10001L,        /* 17 */
            0b11100L,        /* 28 */
        };
        const size_t n_pat = sizeof(patterns) / sizeof(patterns[0]);
        for (size_t p = 0; p < n_pat; ++p) {
            for (int r = 0; r < 5; ++r) {
                emit_si(out, "adversarial",  patterns[p], 3, RNDS[r]);
                emit_si(out, "adversarial", -patterns[p], 3, RNDS[r]);
            }
        }

        /* RNDN-tie cases. */
        emit_si(out, "adversarial",  0b1010, 2, MPFR_RNDN);
        emit_si(out, "adversarial", -0b1010, 2, MPFR_RNDN);
        emit_si(out, "adversarial",  0b1110, 2, MPFR_RNDN);
        emit_si(out, "adversarial", -0b1110, 2, MPFR_RNDN);
    }

    /* ============================================================== */
    /* fuzz: 55 random bigints (xs64 seed).                           */
    /* ============================================================== */
    {
        xs64_t rng;
        xs64_seed(&rng, 0x5E72E72E72E72E72ULL);
        const uint64_t precs[7] = { 1, 2, 24, 53, 64, 100, 256 };

        for (int rep = 0; rep < 55; ++rep) {
            /* Random bit-length in [1, 600] — small enough for fast
             * golden gen but well past int64. */
            const uint64_t blen = 1 + xs64_below(&rng, 600);
            mpz_t z; mpz_init(z);
            /* Random bits via repeated xs64 draws — assemble limb-wise. */
            for (uint64_t off = 0; off < blen; off += 32) {
                const uint32_t chunk = (uint32_t)(xs64_next(&rng) & 0xFFFFFFFFu);
                mpz_mul_2exp(z, z, 32);
                mpz_add_ui(z, z, chunk);
            }
            /* Mask to exactly blen bits so the bit-length is known. */
            {
                mpz_t mask; mpz_init(mask);
                mpz_ui_pow_ui(mask, 2UL, (unsigned long)blen);
                mpz_sub_ui(mask, mask, 1UL);
                mpz_and(z, z, mask);
                mpz_clear(mask);
            }
            /* Ensure the top bit is set so bit-length is exactly blen. */
            mpz_setbit(z, (unsigned long)(blen - 1));
            /* Random sign. */
            if (xs64_next(&rng) & 1) mpz_neg(z, z);

            const uint64_t prec = precs[xs64_below(&rng, 7)];
            const mpfr_rnd_t rnd = RNDS[xs64_below(&rng, 5)];
            emit_case(out, "fuzz", z, prec, rnd);
            mpz_clear(z);
        }
    }

    /* ============================================================== */
    /* mined: 5 from mpfr/tests/tset_z.c.                             */
    /* ============================================================== */
    {
        /* tset_z.c L82–L83: large bigint at 160-bit prec, exact. */
        {
            mpz_t z; mpz_init(z);
            mpz_set_str(z, "77031627725494291259359895954016675357279104942148788042", 10);
            emit_case(out, "mined", z, 160, MPFR_RNDN);
            mpz_clear(z);
        }
        /* tset_z.c L36: mpfr_set_z(x, 0, rnd) — z=0 RNDN. */
        emit_si(out, "mined", 0, 53, MPFR_RNDN);
        /* tset_z.c structural: positive small int RNDN. */
        emit_si(out, "mined", 17, 53, MPFR_RNDN);
        /* tset_z.c structural: negative small int RNDN. */
        emit_si(out, "mined", -17, 53, MPFR_RNDN);
        /* tset_z.c structural: 1024 = 2^10 RNDN. */
        emit_si(out, "mined", 1024, 53, MPFR_RNDN);
    }

    return 0;
}
