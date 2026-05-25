/*
 * golden_driver.c -- Golden master for MPFR's mpfr_mpz_clear.
 *
 * C: void mpfr_mpz_clear(mpz_ptr z). Ref: mpfr/src/pool.c L83-L101.
 *
 * The C function is internal (declared in mpfr-impl.h, not in mpfr.h).
 * It either pushes z onto the static mpz_tab pool or calls mpz_clear.
 * The TS port is a no-op returning true; we emit true unconditionally
 * on the wire to match.
 *
 * Single happy case (no-arg accessor pattern, same as mpfr_free_pool).
 */
#include "common.h"
#include <inttypes.h>
#include <string.h>

extern void mpfr_mpz_clear(mpz_ptr);

/* Emit ,"z":"<decimal>" for the input bigint. */
static inline void jl_kv_mpz(FILE *f, int first, const char *key, mpz_srcptr z) {
    char *s = mpz_get_str(NULL, 10, z);
    jl_kv_str(f, first, key, s);
    void (*gmp_free)(void *, size_t);
    mp_get_memory_functions(NULL, NULL, &gmp_free);
    gmp_free(s, strlen(s) + 1);
}

static inline void emit_case(FILE *out, const char *tag, mpz_srcptr z_in) {
    /* Copy z_in to a fresh mpz_t we can pass to mpfr_mpz_clear. */
    mpz_t z;
    mpz_init_set(z, z_in);
    const uint64_t t0 = now_ns();
    mpfr_mpz_clear(z);
    const uint64_t elapsed = now_ns() - t0;
    /* z is now consumed (either pooled or freed); do NOT touch again. */
    jl_begin(out, tag);
    jl_kv_mpz(out, 1, "z", z_in);
    jl_end_inputs(out);
    jl_output_scalar_bool(out, 1);  /* TS contract: always true */
    jl_finish(out, elapsed);
}

static inline void emit_si(FILE *out, const char *tag, long n) {
    mpz_t z; mpz_init(z); mpz_set_si(z, n);
    emit_case(out, tag, z);
    mpz_clear(z);
}

static inline void emit_pow2(FILE *out, const char *tag, unsigned long k) {
    mpz_t z; mpz_init(z); mpz_ui_pow_ui(z, 2UL, k);
    emit_case(out, tag, z);
    mpz_clear(z);
}

int main(void) {
    FILE *out = stdout;

    /* Single happy case (vacuous-port pattern). The pool/clear path is
     * exercised below via repeated calls; all emit true on the wire. */
    emit_si(out, "happy", 0);

    /* edge: small variety of z values to exercise both pool and clear paths
     * in libmpfr (small fits the pool; large goes to mpz_clear). */
    emit_si(out, "edge", 0);
    emit_si(out, "edge", 1);
    emit_si(out, "edge", -1);
    emit_si(out, "edge", 17);
    emit_si(out, "edge", -42);
    emit_pow2(out, "edge", 0);
    emit_pow2(out, "edge", 10);
    emit_pow2(out, "edge", 100);
    emit_pow2(out, "edge", 1000);
    emit_pow2(out, "edge", 10000);  /* exceeds MPFR_POOL_MAX_SIZE -- forced clear */

    return 0;
}
