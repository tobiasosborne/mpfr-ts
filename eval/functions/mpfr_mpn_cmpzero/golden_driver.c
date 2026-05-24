/*
 * golden_driver.c — Golden master for MPFR's mpfr_mpn_cmpzero.
 *
 * mpfr_mpn_cmpzero is `static` in mpfr/src/div.c. We mirror the trivial
 * algorithm (`return any limb != 0 ? 1 : 0`) locally and emit golden
 * cases against it.
 *
 * Tag distribution: happy 22, edge 30, adversarial 10, fuzz 55, mined 5.
 */
#include "common.h"

#include <assert.h>
#include <inttypes.h>
#include <stdint.h>

#define MAX_N 8

/* Mirror of the C source mpfr/src/div.c L648-L656. */
static inline int my_mpn_cmpzero(const uint64_t *ap, size_t an) {
    for (size_t i = an; i > 0; --i) {
        if (ap[i - 1] != 0) return 1;
    }
    return 0;
}

static inline void emit_case(FILE *out, const char *tag,
                             const uint64_t *ap, size_t an) {
    assert(an >= 1 && an <= MAX_N);
    const uint64_t t0 = now_ns();
    const int r = my_mpn_cmpzero(ap, an);
    const uint64_t elapsed = now_ns() - t0;
    jl_begin(out, tag);
    jl_kv_limbs(out, 1, "ap", ap, an);
    jl_end_inputs(out);
    jl_output_scalar_int(out, r);
    jl_finish(out, elapsed);
}

int main(void) {
    FILE *out = stdout;
    uint64_t a[MAX_N];

    /* happy: 22 — mix of zero and non-zero. */
    a[0] = 0; emit_case(out, "happy", a, 1);
    a[0] = 1; emit_case(out, "happy", a, 1);
    a[0] = UINT64_MAX; emit_case(out, "happy", a, 1);
    a[0] = 0; a[1] = 0; emit_case(out, "happy", a, 2);
    a[0] = 0; a[1] = 1; emit_case(out, "happy", a, 2);
    a[0] = 1; a[1] = 0; emit_case(out, "happy", a, 2);
    a[0] = 1; a[1] = 1; emit_case(out, "happy", a, 2);
    for (int n = 1; n <= MAX_N; n++) {
        for (int i = 0; i < n; i++) a[i] = 0;
        emit_case(out, "happy", a, n);
    }
    for (int n = 1; n <= MAX_N; n++) {
        for (int i = 0; i < n; i++) a[i] = (uint64_t)(i + 1);
        emit_case(out, "happy", a, n);
    }
    /* fill with one mid-limb non-zero. */
    {
        a[0] = 0; a[1] = 0; a[2] = 0xDEAD; a[3] = 0;
        emit_case(out, "happy", a, 4);
    }
    {
        a[0] = 0; a[1] = 0xDEAD; a[2] = 0;
        emit_case(out, "happy", a, 3);
    }

    /* edge: 30 — boundaries on n and limb values. */
    /* n=1 across values. */
    for (uint64_t i = 0; i < 5; ++i) { a[0] = i; emit_case(out, "edge", a, 1); }
    /* n=MAX_N all zero. */
    for (int i = 0; i < MAX_N; i++) a[i] = 0;
    emit_case(out, "edge", a, MAX_N);
    /* n=MAX_N high limb only. */
    a[MAX_N - 1] = 1;
    emit_case(out, "edge", a, MAX_N);
    /* n=MAX_N low limb only. */
    for (int i = 0; i < MAX_N; i++) a[i] = 0;
    a[0] = 1;
    emit_case(out, "edge", a, MAX_N);
    /* n=2, limbs UINT64_MAX. */
    a[0] = UINT64_MAX; a[1] = UINT64_MAX;
    emit_case(out, "edge", a, 2);
    /* n=8 all UINT64_MAX. */
    for (int i = 0; i < 8; i++) a[i] = UINT64_MAX;
    emit_case(out, "edge", a, 8);
    /* alternating zero/non-zero. */
    for (int n = 2; n <= MAX_N; n++) {
        for (int i = 0; i < n; i++) a[i] = (i % 2 == 0) ? 0 : 1;
        emit_case(out, "edge", a, n);
    }
    /* MSB-only set. */
    {
        a[0] = 0; a[1] = (uint64_t)1 << 63;
        emit_case(out, "edge", a, 2);
    }
    /* LSB-only set. */
    {
        a[0] = 1; for (int i = 1; i < 4; i++) a[i] = 0;
        emit_case(out, "edge", a, 4);
    }
    /* Just-above-zero. */
    {
        a[0] = 1; a[1] = 0; a[2] = 0; a[3] = 0; a[4] = 0;
        emit_case(out, "edge", a, 5);
    }
    /* Single non-zero in middle. */
    {
        for (int i = 0; i < 5; i++) a[i] = 0;
        a[2] = 42;
        emit_case(out, "edge", a, 5);
    }
    /* Long zero array. */
    for (int i = 0; i < MAX_N; i++) a[i] = 0;
    emit_case(out, "edge", a, MAX_N);
    /* Long non-zero array. */
    for (int i = 0; i < MAX_N; i++) a[i] = 1234567 + (uint64_t)i;
    emit_case(out, "edge", a, MAX_N);
    /* Mix: top half non-zero. */
    for (int i = 0; i < 4; i++) a[i] = 0;
    for (int i = 4; i < 8; i++) a[i] = 1;
    emit_case(out, "edge", a, 8);

    /* adversarial: 10 */
    /* Zero in high pos but non-zero in low. */
    a[0] = 0xCAFE; a[1] = 0; a[2] = 0;
    emit_case(out, "adversarial", a, 3);
    a[0] = 0; a[1] = 0xCAFE; a[2] = 0;
    emit_case(out, "adversarial", a, 3);
    a[0] = 0; a[1] = 0; a[2] = 0xCAFE;
    emit_case(out, "adversarial", a, 3);
    /* Very small non-zero — bit 0 only. */
    a[0] = 1; emit_case(out, "adversarial", a, 1);
    /* Very large limb. */
    a[0] = UINT64_MAX - 1; emit_case(out, "adversarial", a, 1);
    /* All-but-one zero. */
    for (int i = 0; i < 8; i++) { a[i] = 0; }
    a[5] = 1;
    emit_case(out, "adversarial", a, 8);
    /* Two non-zero. */
    for (int i = 0; i < 8; i++) { a[i] = 0; }
    a[1] = 1; a[7] = 1;
    emit_case(out, "adversarial", a, 8);
    /* n=2 with high limb just 1. */
    a[0] = 0; a[1] = 1; emit_case(out, "adversarial", a, 2);
    /* n=2 with low limb just 1. */
    a[0] = 1; a[1] = 0; emit_case(out, "adversarial", a, 2);
    /* n=3 with middle non-zero only. */
    a[0] = 0; a[1] = 99; a[2] = 0; emit_case(out, "adversarial", a, 3);

    /* fuzz: 55 */
    {
        xs64_t rng;
        xs64_seed(&rng, 0xC0DE0C0DE0C0DE0CULL);
        for (int rep = 0; rep < 55; ++rep) {
            const size_t n = 1 + (size_t)(xs64_below(&rng, MAX_N));
            for (size_t i = 0; i < n; ++i) {
                /* 50% chance of zero, 50% random. */
                a[i] = (xs64_below(&rng, 2) == 0) ? 0 : xs64_next(&rng);
            }
            emit_case(out, "fuzz", a, n);
        }
    }

    /* mined: 5 */
    a[0] = 0; emit_case(out, "mined", a, 1);
    a[0] = 1; emit_case(out, "mined", a, 1);
    a[0] = 0; a[1] = 1; emit_case(out, "mined", a, 2);
    a[0] = 1; a[1] = 0; emit_case(out, "mined", a, 2);
    for (int i = 0; i < 4; i++) { a[i] = 0; }
    emit_case(out, "mined", a, 4);

    return 0;
}
