/*
 * golden_driver.c -- Golden master for MPFR's mpfr_mpz_init2.
 *
 * C: void mpfr_mpz_init2(mpz_ptr z, mp_bitcnt_t n). Ref: mpfr/src/pool.c L55-L80.
 *
 * The C function is internal (declared in mpfr-impl.h). Either pool-reclaims
 * (if n <= MPFR_POOL_MAX_SIZE * GMP_NUMB_BITS = 32 * 64 = 2048) or calls
 * the real mpz_init2 to pre-allocate >= n bits. Output value is always +0.
 *
 * The TS port is a vacuous factory taking n (hint) and returning 0n.
 *
 * Wire: {"inputs":{"n":"<dec>"},"output":"0"}.
 *
 * Tag distribution (Rule 7): happy 20, edge 30, adv 12, fuzz 50, mined 5.
 * We vary n across the pool / non-pool C branches (n <= 2048 vs > 2048)
 * to verify the TS port handles both equivalently (i.e. returns 0n).
 */
#include "common.h"
#include <assert.h>
#include <inttypes.h>
#include <string.h>

extern void mpfr_mpz_init2(mpz_ptr, mp_bitcnt_t);

static inline void emit_case(FILE *out, const char *tag, uint64_t n) {
    mpz_t z;
    const uint64_t t0 = now_ns();
    mpfr_mpz_init2(z, (mp_bitcnt_t)n);
    const uint64_t elapsed = now_ns() - t0;
    mpz_clear(z);

    jl_begin(out, tag);
    jl_kv_u64(out, 1, "n", n);
    jl_end_inputs(out);
    jl_output_scalar_i64(out, 0);
    jl_finish(out, elapsed);
}

int main(void) {
    FILE *out = stdout;

    /* happy: 20 -- small n values, both pool and non-pool C branches. */
    emit_case(out, "happy", 0);
    emit_case(out, "happy", 1);
    emit_case(out, "happy", 32);
    emit_case(out, "happy", 53);
    emit_case(out, "happy", 64);
    emit_case(out, "happy", 100);
    emit_case(out, "happy", 128);
    emit_case(out, "happy", 200);
    emit_case(out, "happy", 256);
    emit_case(out, "happy", 512);
    emit_case(out, "happy", 1000);
    emit_case(out, "happy", 1024);
    emit_case(out, "happy", 1500);
    emit_case(out, "happy", 2000);
    emit_case(out, "happy", 2048);  /* pool boundary */
    emit_case(out, "happy", 2049);  /* just past pool */
    emit_case(out, "happy", 3000);
    emit_case(out, "happy", 4096);  /* TS PREC_MAX-like ceiling */
    emit_case(out, "happy", 16);
    emit_case(out, "happy", 8);

    /* edge: 30 -- boundaries (0, 1, 2048 pool cap, very large). */
    emit_case(out, "edge", 0);
    emit_case(out, "edge", 1);
    emit_case(out, "edge", 2);
    emit_case(out, "edge", 3);
    emit_case(out, "edge", 63);
    emit_case(out, "edge", 64);
    emit_case(out, "edge", 65);
    emit_case(out, "edge", 127);
    emit_case(out, "edge", 128);
    emit_case(out, "edge", 129);
    emit_case(out, "edge", 2047);
    emit_case(out, "edge", 2048);
    emit_case(out, "edge", 2049);
    emit_case(out, "edge", 2050);
    emit_case(out, "edge", 4095);
    emit_case(out, "edge", 4096);
    emit_case(out, "edge", 100);
    emit_case(out, "edge", 200);
    emit_case(out, "edge", 300);
    emit_case(out, "edge", 400);
    emit_case(out, "edge", 500);
    emit_case(out, "edge", 600);
    emit_case(out, "edge", 700);
    emit_case(out, "edge", 800);
    emit_case(out, "edge", 900);
    emit_case(out, "edge", 1000);
    emit_case(out, "edge", 1100);
    emit_case(out, "edge", 1200);
    emit_case(out, "edge", 1300);
    emit_case(out, "edge", 1400);

    /* adversarial: 12 -- exact pool-cap and far-outside-pool boundary. */
    emit_case(out, "adversarial", 0);
    emit_case(out, "adversarial", 2048);
    emit_case(out, "adversarial", 2047);
    emit_case(out, "adversarial", 2049);
    emit_case(out, "adversarial", 1);
    emit_case(out, "adversarial", 4096);
    emit_case(out, "adversarial", 2);
    emit_case(out, "adversarial", 100);
    emit_case(out, "adversarial", 1000);
    emit_case(out, "adversarial", 1024);
    emit_case(out, "adversarial", 50);
    emit_case(out, "adversarial", 750);

    /* fuzz: 50 random n in [0, 4096]. */
    {
        xs64_t rng;
        xs64_seed(&rng, 0xFADEC0FFEEBABE05ULL);
        for (int rep = 0; rep < 50; ++rep) {
            const uint64_t n = xs64_below(&rng, 4097);
            emit_case(out, "fuzz", n);
        }
    }

    /* mined: 5 -- common precision hints likely used by MPFR internals. */
    emit_case(out, "mined", 53);   /* IEEE double */
    emit_case(out, "mined", 24);   /* IEEE single */
    emit_case(out, "mined", 64);   /* extended */
    emit_case(out, "mined", 128);  /* quad */
    emit_case(out, "mined", 0);    /* min */

    return 0;
}
