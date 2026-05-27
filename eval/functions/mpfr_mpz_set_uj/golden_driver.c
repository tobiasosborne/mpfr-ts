/*
 * golden_driver.c -- Golden master for MPFR's mpfr_mpz_set_uj.
 *
 * C: static void mpfr_mpz_set_uj(mpz_t z, uintmax_t n).
 *    Ref: mpfr/src/pow_uj.c L40-L57.
 *
 * Since the C symbol is static (no public linkage), this driver
 * REPLICATES the C algorithm verbatim (mirror of the same decomposition)
 * per ADR 0002 -- the golden-driver-substitute pattern. The faithful
 * C reimplementation here produces the same wire output the real
 * static helper would.
 *
 * Under ADR 0003, the TS port reduces to identity for the uint64
 * domain (since bigint has no need for the unsigned-long decomposition).
 * Both the substitute and the TS port produce z = n, which round-trip
 * through the decimal-string mpz codec to bigint n.
 *
 * Wire: {"inputs":{"n":"<uint64-dec>"},"output":"<n-as-dec>"}.
 *
 * Tag distribution (Rule 7): happy 20, edge 30, adv 12, fuzz 50, mined 5.
 */
#include "common.h"
#include <assert.h>
#include <inttypes.h>
#include <limits.h>
#include <string.h>

#define ULONG_BITS (sizeof(unsigned long) * CHAR_BIT)

/* Portable substitute: literal mirror of mpfr/src/pow_uj.c L40-L57. */
static void substitute_mpz_set_uj(mpz_t z, uintmax_t n) {
    if ((unsigned long) n == n) {
        mpz_set_ui(z, (unsigned long) n);
    } else {
        uintmax_t h = (n >> (ULONG_BITS - 1)) >> 1;
        /* equivalent to n >> ULONG_BITS, per the C comment about GCC bug 4210 */
        mpz_set_ui(z, (unsigned long) h);
        mpz_mul_2exp(z, z, ULONG_BITS);
        mpz_add_ui(z, z, (unsigned long) n);
    }
}

/* Emit ,"key":"<decimal>" for an mpz_t. */
static inline void jl_kv_mpz(FILE *f, int first, const char *key, mpz_srcptr z) {
    char *s = mpz_get_str(NULL, 10, z);
    jl_kv_str(f, first, key, s);
    void (*gmp_free)(void *, size_t);
    mp_get_memory_functions(NULL, NULL, &gmp_free);
    gmp_free(s, strlen(s) + 1);
}

/* Output as decimal-string-shaped scalar (round-trips to bigint). */
static inline void jl_output_mpz(FILE *f, mpz_srcptr z) {
    char *s = mpz_get_str(NULL, 10, z);
    fputs(",\"output\":\"", f);
    fputs(s, f);
    fputc('"', f);
    void (*gmp_free)(void *, size_t);
    mp_get_memory_functions(NULL, NULL, &gmp_free);
    gmp_free(s, strlen(s) + 1);
}

static inline void emit_case(FILE *out, const char *tag, uint64_t n) {
    mpz_t z;
    mpz_init(z);
    const uint64_t t0 = now_ns();
    substitute_mpz_set_uj(z, (uintmax_t)n);
    const uint64_t elapsed = now_ns() - t0;

    jl_begin(out, tag);
    jl_kv_u64(out, 1, "n", n);
    jl_end_inputs(out);
    jl_output_mpz(out, z);
    jl_finish(out, elapsed);
    mpz_clear(z);
}

int main(void) {
    FILE *out = stdout;

    /* happy: 20 -- small values, both in-ULONG and over-ULONG ranges. */
    emit_case(out, "happy", 0);
    emit_case(out, "happy", 1);
    emit_case(out, "happy", 2);
    emit_case(out, "happy", 17);
    emit_case(out, "happy", 42);
    emit_case(out, "happy", 100);
    emit_case(out, "happy", 1742);
    emit_case(out, "happy", 65535);
    emit_case(out, "happy", 1000000);
    emit_case(out, "happy", 1234567890ULL);
    emit_case(out, "happy", 0xFFFFFFFFULL);                  /* 2^32 - 1 */
    emit_case(out, "happy", 0x100000000ULL);                 /* 2^32 */
    emit_case(out, "happy", 0xFFFFFFFFFFFFULL);              /* 2^48 - 1 */
    emit_case(out, "happy", 0x1000000000000ULL);             /* 2^48 */
    emit_case(out, "happy", 0x7FFFFFFFFFFFFFFFULL);          /* 2^63 - 1 */
    emit_case(out, "happy", 0x8000000000000000ULL);          /* 2^63 */
    emit_case(out, "happy", 0xFFFFFFFFFFFFFFFFULL);          /* UINT64_MAX */
    emit_case(out, "happy", 12345);
    emit_case(out, "happy", 67890);
    emit_case(out, "happy", 13);

    /* edge: 30 -- boundaries and ULONG_MAX / 2^32 transitions. */
    emit_case(out, "edge", 0);
    emit_case(out, "edge", 1);
    emit_case(out, "edge", ULONG_MAX);              /* boundary: max in-ULONG */
    emit_case(out, "edge", (uint64_t)ULONG_MAX);
    if (sizeof(unsigned long) < sizeof(uint64_t)) {
        /* LP32 path (won't fire on LP64 Linux): force the (h != 0) branch */
        emit_case(out, "edge", (uint64_t)ULONG_MAX + 1ULL);
    } else {
        emit_case(out, "edge", 1ULL << 31);
    }
    /* Each power of two from 0 to 63 -- spans the in/out-of-ULONG boundary. */
    for (int k = 0; k < 27; ++k) {
        emit_case(out, "edge", (uint64_t)1ULL << k);
    }

    /* adversarial: 12 -- specific decomposition boundaries that exercise
     * the (n >> (ULONG_BITS-1)) >> 1 trick. */
    emit_case(out, "adversarial", 0xFFFFFFFFULL);
    emit_case(out, "adversarial", 0x100000000ULL);
    emit_case(out, "adversarial", 0xFFFFFFFFFFFFFFFFULL);
    emit_case(out, "adversarial", 0x8000000000000000ULL);
    emit_case(out, "adversarial", 0x8000000000000001ULL);
    emit_case(out, "adversarial", 0x7FFFFFFFFFFFFFFFULL);
    emit_case(out, "adversarial", 0xFFFFFFFFFFFFFFFEULL);
    emit_case(out, "adversarial", 0xAAAAAAAAAAAAAAAAULL);
    emit_case(out, "adversarial", 0x5555555555555555ULL);
    emit_case(out, "adversarial", 0xDEADBEEFDEADBEEFULL);
    emit_case(out, "adversarial", 0xCAFEBABECAFEBABEULL);
    emit_case(out, "adversarial", 0x123456789ABCDEF0ULL);

    /* fuzz: 50 random values across uint64 range. */
    {
        xs64_t rng;
        xs64_seed(&rng, 0xABCDEF0123456789ULL);
        for (int rep = 0; rep < 50; ++rep) {
            const uint64_t n = xs64_next(&rng);
            emit_case(out, "fuzz", n);
        }
    }

    /* mined: 5 -- patterns relevant to mpfr_pow_uj's main caller. */
    emit_case(out, "mined", 0);
    emit_case(out, "mined", 1);
    emit_case(out, "mined", 2);
    emit_case(out, "mined", (uint64_t)ULONG_MAX);
    emit_case(out, "mined", 0xFFFFFFFFFFFFFFFFULL);

    return 0;
}
