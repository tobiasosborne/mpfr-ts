/*
 * golden_driver.c -- Golden master for mpfr_const_euler_bs_2.
 *
 * The C function is `static` in const_euler.c. We re-implement the
 * algorithm (golden-driver-substitute pattern per ADR 0002) using
 * native GMP mpz_t calls -- the algorithm is verbatim from the C
 * source.
 *
 * Wire: {"inputs":{"n1":"<dec>","n2":"<dec>","N":"<dec>","cont":<int>},
 *        "output":{"P":"<dec>","Q":"<dec>","T":"<dec>"}}.
 * Tag distribution (Rule 7): happy 20, edge 30, adv 12, fuzz 50, mined 5.
 */
#include "common.h"
#include <assert.h>
#include <inttypes.h>

static void bs2(mpz_t P, mpz_t Q, mpz_t T,
                unsigned long n1, unsigned long n2, unsigned long N, int cont) {
    if (n2 - n1 == 1) {
        if (n1 == 0) {
            mpz_set_ui(P, 1);
            mpz_set_ui(Q, 4 * N);
        } else {
            mpz_set_ui(P, 2 * n1 - 1);
            mpz_pow_ui(P, P, 3);
            mpz_set_ui(Q, 32 * n1);
            mpz_mul_ui(Q, Q, N);
            mpz_mul_ui(Q, Q, N);
        }
        mpz_set(T, P);
    } else {
        mpz_t P2, Q2, T2;
        unsigned long m = (n1 + n2) / 2;
        mpz_init(P2); mpz_init(Q2); mpz_init(T2);
        bs2(P, Q, T, n1, m, N, 1);
        bs2(P2, Q2, T2, m, n2, N, 1);
        mpz_mul(T, T, Q2);
        mpz_mul(T2, T2, P);
        mpz_add(T, T, T2);
        if (cont) mpz_mul(P, P, P2);
        mpz_mul(Q, Q, Q2);
        mpz_clear(P2); mpz_clear(Q2); mpz_clear(T2);
    }
}

static inline void emit_mpz(FILE *out, const char *key, int first, const mpz_t z) {
    char *s = mpz_get_str(NULL, 10, z);
    fprintf(out, "%s\"%s\":\"%s\"", first ? "" : ",", key, s);
    void (*gmp_free)(void *, size_t);
    mp_get_memory_functions(NULL, NULL, &gmp_free);
    gmp_free(s, strlen(s) + 1);
}

static inline void emit_case(FILE *out, const char *tag,
                              uint64_t n1, uint64_t n2, uint64_t N, int cont) {
    assert(n1 < n2);
    assert(N >= 1);
    mpz_t P, Q, T;
    mpz_init(P); mpz_init(Q); mpz_init(T);
    const uint64_t t0 = now_ns();
    bs2(P, Q, T, n1, n2, N, cont);
    const uint64_t elapsed = now_ns() - t0;
    jl_begin(out, tag);
    jl_kv_u64(out, 1, "n1", n1);
    jl_kv_u64(out, 0, "n2", n2);
    jl_kv_u64(out, 0, "N", N);
    jl_kv_int(out, 0, "cont", cont);
    jl_end_inputs(out);
    fputs(",\"output\":{", out);
    emit_mpz(out, "P", 1, P);
    emit_mpz(out, "Q", 0, Q);
    emit_mpz(out, "T", 0, T);
    fputs("}", out);
    jl_finish(out, elapsed);
    mpz_clear(P); mpz_clear(Q); mpz_clear(T);
}

int main(void) {
    FILE *out = stdout;
    /* happy: 20 -- small ranges and moderate N. */
    emit_case(out, "happy", 0, 1, 5, 1);
    emit_case(out, "happy", 0, 1, 5, 0);
    emit_case(out, "happy", 0, 2, 5, 1);
    emit_case(out, "happy", 0, 2, 5, 0);
    emit_case(out, "happy", 1, 2, 5, 1);
    emit_case(out, "happy", 0, 4, 5, 1);
    emit_case(out, "happy", 0, 4, 5, 0);
    emit_case(out, "happy", 1, 4, 5, 1);
    emit_case(out, "happy", 2, 4, 5, 1);
    emit_case(out, "happy", 0, 8, 10, 1);
    emit_case(out, "happy", 0, 8, 10, 0);
    emit_case(out, "happy", 4, 8, 10, 1);
    emit_case(out, "happy", 0, 10, 20, 1);
    emit_case(out, "happy", 0, 10, 20, 0);
    emit_case(out, "happy", 5, 10, 20, 1);
    emit_case(out, "happy", 0, 16, 50, 1);
    emit_case(out, "happy", 0, 16, 50, 0);
    emit_case(out, "happy", 8, 16, 50, 1);
    emit_case(out, "happy", 1, 3, 100, 1);
    emit_case(out, "happy", 2, 5, 100, 1);
    /* edge: 30 -- base cases, n1=0, large gaps. */
    emit_case(out, "edge", 0, 1, 1, 1);
    emit_case(out, "edge", 0, 1, 2, 1);
    emit_case(out, "edge", 0, 1, 10, 1);
    emit_case(out, "edge", 0, 1, 100, 1);
    emit_case(out, "edge", 1, 2, 1, 1);
    emit_case(out, "edge", 1, 2, 2, 1);
    emit_case(out, "edge", 1, 2, 10, 1);
    emit_case(out, "edge", 1, 2, 100, 1);
    emit_case(out, "edge", 99, 100, 1, 1);
    emit_case(out, "edge", 99, 100, 50, 1);
    emit_case(out, "edge", 0, 2, 1, 1);
    emit_case(out, "edge", 0, 2, 1, 0);
    emit_case(out, "edge", 0, 4, 1, 0);
    emit_case(out, "edge", 0, 8, 1, 0);
    emit_case(out, "edge", 0, 16, 1, 0);
    emit_case(out, "edge", 0, 32, 5, 0);
    emit_case(out, "edge", 0, 32, 5, 1);
    emit_case(out, "edge", 0, 64, 5, 0);
    emit_case(out, "edge", 0, 64, 5, 1);
    emit_case(out, "edge", 0, 32, 100, 0);
    emit_case(out, "edge", 0, 32, 100, 1);
    emit_case(out, "edge", 0, 5, 100, 0);
    emit_case(out, "edge", 0, 5, 100, 1);
    emit_case(out, "edge", 5, 10, 100, 0);
    emit_case(out, "edge", 5, 10, 100, 1);
    emit_case(out, "edge", 10, 20, 50, 0);
    emit_case(out, "edge", 10, 20, 50, 1);
    emit_case(out, "edge", 0, 3, 100, 1);
    emit_case(out, "edge", 0, 6, 100, 0);
    emit_case(out, "edge", 0, 7, 100, 1);
    /* adversarial: 12 -- larger ranges to stress the recursion depth. */
    emit_case(out, "adversarial", 0, 50, 50, 0);
    emit_case(out, "adversarial", 0, 100, 100, 0);
    emit_case(out, "adversarial", 0, 200, 200, 0);
    emit_case(out, "adversarial", 0, 100, 100, 1);
    emit_case(out, "adversarial", 50, 100, 100, 1);
    emit_case(out, "adversarial", 0, 64, 1000, 0);
    emit_case(out, "adversarial", 0, 128, 1000, 0);
    emit_case(out, "adversarial", 100, 200, 50, 1);
    emit_case(out, "adversarial", 1, 50, 10, 0);
    emit_case(out, "adversarial", 1, 100, 10, 0);
    emit_case(out, "adversarial", 25, 75, 25, 1);
    emit_case(out, "adversarial", 0, 32, 1, 1);
    /* fuzz: 50 */
    {
        xs64_t rng;
        xs64_seed(&rng, 0xABCDABCDABCDABCDULL);
        for (int rep = 0; rep < 50; ++rep) {
            const uint64_t n1 = xs64_below(&rng, 50);
            const uint64_t gap = 1 + xs64_below(&rng, 50);
            const uint64_t n2 = n1 + gap;
            const uint64_t N = 1 + xs64_below(&rng, 100);
            const int cont = (int)(xs64_below(&rng, 2));
            emit_case(out, "fuzz", n1, n2, N, cont);
        }
    }
    /* mined: 5 -- minimal recursion + medium recursion examples. */
    emit_case(out, "mined", 0, 1, 5, 1);     /* base case n1=0 */
    emit_case(out, "mined", 1, 2, 5, 1);     /* base case n1!=0 */
    emit_case(out, "mined", 0, 2, 10, 0);    /* one-level recursion, cont=0 */
    emit_case(out, "mined", 0, 2, 10, 1);    /* one-level recursion, cont=1 */
    emit_case(out, "mined", 0, 8, 10, 0);    /* 3-level recursion */
    return 0;
}
