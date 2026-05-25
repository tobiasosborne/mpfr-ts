/*
 * golden_driver.c -- Golden master for mpfr_const_euler_bs_1.
 *
 * The C function is `static` in const_euler.c. We re-implement the
 * algorithm (golden-driver-substitute pattern per ADR 0002) using
 * native GMP mpz_t calls.
 *
 * Wire: {"inputs":{"n1","n2","N","cont"},"output":{"P","Q","T","C","D","V"}}.
 * Tag distribution (Rule 7): happy 20, edge 30, adv 12, fuzz 50, mined 5.
 *
 * Ref: mpfr/src/const_euler.c L73-L135.
 */
#include "common.h"
#include <assert.h>
#include <inttypes.h>

typedef struct {
    mpz_t P, Q, T, C, D, V;
} bs1_t;

static void bs1_init(bs1_t *s) {
    mpz_init(s->P); mpz_init(s->Q); mpz_init(s->T);
    mpz_init(s->C); mpz_init(s->D); mpz_init(s->V);
}
static void bs1_clear(bs1_t *s) {
    mpz_clear(s->P); mpz_clear(s->Q); mpz_clear(s->T);
    mpz_clear(s->C); mpz_clear(s->D); mpz_clear(s->V);
}

static void bs1(bs1_t *s, unsigned long n1, unsigned long n2, unsigned long N, int cont) {
    if (n2 - n1 == 1) {
        mpz_set_ui(s->P, N);
        mpz_mul(s->P, s->P, s->P);
        mpz_set_ui(s->Q, n1 + 1);
        mpz_mul(s->Q, s->Q, s->Q);
        mpz_set_ui(s->C, 1);
        mpz_set_ui(s->D, n1 + 1);
        mpz_set(s->T, s->P);
        mpz_set(s->V, s->P);
    } else {
        bs1_t L, R;
        mpz_t t, u, v;
        unsigned long m = (n1 + n2) / 2;
        bs1_init(&L); bs1_init(&R);
        bs1(&L, n1, m, N, 1);
        bs1(&R, m, n2, N, 1);
        mpz_init(t); mpz_init(u); mpz_init(v);

        if (cont)
            mpz_mul(s->P, L.P, R.P);
        mpz_mul(s->Q, L.Q, R.Q);
        mpz_mul(s->D, L.D, R.D);

        /* T = LP RT + RQ LT */
        mpz_mul(t, L.P, R.T);
        mpz_mul(v, R.Q, L.T);
        mpz_add(s->T, t, v);

        /* C = LC RD + RC LD */
        if (cont) {
            mpz_mul(s->C, L.C, R.D);
            mpz_addmul(s->C, R.C, L.D);
        }

        /* V = RD (RQ LV + LC LP RT) + LD LP RV */
        mpz_mul(u, L.P, R.V);
        mpz_mul(u, u, L.D);
        mpz_mul(v, R.Q, L.V);
        mpz_addmul(v, t, L.C);
        mpz_mul(v, v, R.D);
        mpz_add(s->V, u, v);

        bs1_clear(&L); bs1_clear(&R);
        mpz_clear(t); mpz_clear(u); mpz_clear(v);
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
    bs1_t s;
    bs1_init(&s);
    const uint64_t t0 = now_ns();
    bs1(&s, n1, n2, N, cont);
    const uint64_t elapsed = now_ns() - t0;
    jl_begin(out, tag);
    jl_kv_u64(out, 1, "n1", n1);
    jl_kv_u64(out, 0, "n2", n2);
    jl_kv_u64(out, 0, "N", N);
    jl_kv_int(out, 0, "cont", cont);
    jl_end_inputs(out);
    fputs(",\"output\":{", out);
    emit_mpz(out, "P", 1, s.P);
    emit_mpz(out, "Q", 0, s.Q);
    emit_mpz(out, "T", 0, s.T);
    emit_mpz(out, "C", 0, s.C);
    emit_mpz(out, "D", 0, s.D);
    emit_mpz(out, "V", 0, s.V);
    fputs("}", out);
    jl_finish(out, elapsed);
    bs1_clear(&s);
}

int main(void) {
    FILE *out = stdout;

    /* happy: 20 -- small ranges. */
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

    /* adversarial: 12 -- larger ranges to stress recursion. */
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
        xs64_seed(&rng, 0xDECAFDECAFDECAFEULL);
        for (int rep = 0; rep < 50; ++rep) {
            const uint64_t n1 = xs64_below(&rng, 50);
            const uint64_t gap = 1 + xs64_below(&rng, 50);
            const uint64_t n2 = n1 + gap;
            const uint64_t N = 1 + xs64_below(&rng, 100);
            const int cont = (int)(xs64_below(&rng, 2));
            emit_case(out, "fuzz", n1, n2, N, cont);
        }
    }

    /* mined: 5 */
    emit_case(out, "mined", 0, 1, 5, 1);
    emit_case(out, "mined", 1, 2, 5, 1);
    emit_case(out, "mined", 0, 2, 10, 0);
    emit_case(out, "mined", 0, 2, 10, 1);
    emit_case(out, "mined", 0, 8, 10, 0);

    return 0;
}
