/*
 * golden_driver.c — Golden master for MPFR's mpfr_mpn_sub_aux.
 *
 * Static helper; mirrored verbatim. Computes ap[] -= (bp[] >> extra) + cy
 * in place, returns borrow-out.
 *
 * Tag distribution: happy 22, edge 30, adversarial 12, fuzz 55, mined 5.
 */
#include "common.h"

#include <assert.h>
#include <inttypes.h>
#include <stdint.h>
#include <sys/types.h>

#define MAX_N 8

/* Mirror of mpfr/src/div.c L723-L744. */
static uint64_t my_mpn_sub_aux(uint64_t *ap, const uint64_t *bp, ssize_t n,
                               uint64_t cy, int extra) {
    uint64_t bb, rp;
    ssize_t i = 0;
    while (n-- > 0) {
        /* extra=1: bb = (bp[i+1] << 63) | (bp[i] >> 1); extra=0: bb = bp[i]. */
        bb = extra ? (((bp[i + 1]) << 63) | (bp[i] >> 1)) : bp[i];
        rp = ap[i] - bb - cy;
        uint64_t new_cy = ((ap[i] < bb) || (cy && rp == UINT64_MAX)) ? 1 : 0;
        cy = new_cy;
        ap[i] = rp;
        i++;
    }
    return cy;
}

static inline void emit_case(FILE *out, const char *tag,
                             const uint64_t *ap_in, const uint64_t *bp_full,
                             size_t n, uint64_t cy, int extra) {
    assert(n >= 1 && n <= MAX_N);
    assert(extra == 0 || extra == 1);
    /* When extra=1, the algorithm reads bp[i+1], so bp must have length n+1.
     * Caller provides bp_full with at least n+1 elements when extra=1. */
    uint64_t ap[MAX_N];
    for (size_t i = 0; i < n; ++i) ap[i] = ap_in[i];
    const uint64_t t0 = now_ns();
    const uint64_t borrow = my_mpn_sub_aux(ap, bp_full, (ssize_t)n, cy, extra);
    const uint64_t elapsed = now_ns() - t0;
    jl_begin(out, tag);
    jl_kv_limbs(out, 1, "ap", ap_in, n);
    /* Emit bp at length n (the algorithm only reads bp[0..n], or bp[0..n+1)
     * when extra=1; for the TS port we need both forms — pass length n+extra
     * so the TS reassembles bp correctly. */
    const size_t bp_emit_len = n + (size_t)extra;
    jl_kv_limbs(out, 0, "bp", bp_full, bp_emit_len);
    jl_kv_u64(out, 0, "cy", cy);
    jl_kv_int(out, 0, "extra", extra);
    jl_end_inputs(out);
    jl_output_begin_object(out);
    jl_kv_limbs(out, 1, "result", ap, n);
    jl_kv_u64(out, 0, "borrow", borrow);
    jl_output_end_object(out);
    jl_finish(out, elapsed);
}

int main(void) {
    FILE *out = stdout;
    uint64_t a[MAX_N], b[MAX_N + 1];

    /* happy: 22 */
    a[0] = 10; b[0] = 5; b[1] = 0;
    emit_case(out, "happy", a, b, 1, 0, 0);
    emit_case(out, "happy", a, b, 1, 1, 0);   /* with carry */
    a[0] = 5; b[0] = 10; b[1] = 0;
    emit_case(out, "happy", a, b, 1, 0, 0);   /* borrow */
    a[0] = 5; b[0] = 5; b[1] = 0;
    emit_case(out, "happy", a, b, 1, 0, 0);
    a[0] = UINT64_MAX; b[0] = 1; b[1] = 0;
    emit_case(out, "happy", a, b, 1, 0, 0);
    a[0] = 0; b[0] = 0; b[1] = 0;
    emit_case(out, "happy", a, b, 1, 0, 0);
    a[0] = 0; b[0] = 0; b[1] = 0;
    emit_case(out, "happy", a, b, 1, 1, 0);   /* 0 - 0 - 1 → -1, borrow */
    /* extra=1 cases. */
    a[0] = 10; b[0] = 8; b[1] = 0;
    emit_case(out, "happy", a, b, 1, 0, 1);   /* a - (b>>1) - 0 = 10 - 4 = 6 */
    a[0] = 4; b[0] = 8; b[1] = 0;
    emit_case(out, "happy", a, b, 1, 0, 1);   /* 4 - 4 = 0 */
    a[0] = 0; b[0] = 0; b[1] = 1;
    emit_case(out, "happy", a, b, 1, 0, 1);   /* b>>1 with b[1]=1 → high bit shifts in */
    /* Multi-limb. */
    a[0] = 10; a[1] = 20; b[0] = 5; b[1] = 5; b[2] = 0;
    emit_case(out, "happy", a, b, 2, 0, 0);
    a[0] = 0; a[1] = 0; b[0] = 1; b[1] = 0; b[2] = 0;
    emit_case(out, "happy", a, b, 2, 0, 0);
    a[0] = UINT64_MAX; a[1] = 0; b[0] = 1; b[1] = 0; b[2] = 0;
    emit_case(out, "happy", a, b, 2, 0, 0);
    a[0] = 0; a[1] = 1; b[0] = 1; b[1] = 0; b[2] = 0;
    emit_case(out, "happy", a, b, 2, 0, 0);
    /* 3-limb. */
    for (int i = 0; i < 3; i++) { a[i] = 100; b[i] = 10; } b[3] = 0;
    emit_case(out, "happy", a, b, 3, 0, 0);
    emit_case(out, "happy", a, b, 3, 1, 0);
    /* extra=1, multi-limb. */
    a[0] = 8; a[1] = 0; b[0] = 16; b[1] = 0; b[2] = 0;
    emit_case(out, "happy", a, b, 2, 0, 1);
    /* All zeros. */
    for (int i = 0; i < 4; i++) { a[i] = 0; b[i] = 0; } b[4] = 0;
    emit_case(out, "happy", a, b, 4, 0, 0);
    emit_case(out, "happy", a, b, 4, 0, 1);
    emit_case(out, "happy", a, b, 4, 1, 0);
    /* a all max. */
    for (int i = 0; i < 4; i++) { a[i] = UINT64_MAX; b[i] = 1; } b[4] = 0;
    emit_case(out, "happy", a, b, 4, 0, 0);
    emit_case(out, "happy", a, b, 4, 0, 1);

    /* edge: 30 */
    /* Single-limb borrow chain. */
    a[0] = 1; a[1] = 1; b[0] = 2; b[1] = 0; b[2] = 0;
    emit_case(out, "edge", a, b, 2, 0, 0);
    /* Carry propagation across limbs. */
    a[0] = 0; a[1] = UINT64_MAX; b[0] = 1; b[1] = 0; b[2] = 0;
    emit_case(out, "edge", a, b, 2, 0, 0);
    /* All max - all max + cy. */
    for (int i = 0; i < 4; i++) { a[i] = UINT64_MAX; b[i] = UINT64_MAX; } b[4] = 0;
    emit_case(out, "edge", a, b, 4, 0, 0);
    emit_case(out, "edge", a, b, 4, 1, 0);
    emit_case(out, "edge", a, b, 4, 0, 1);
    emit_case(out, "edge", a, b, 4, 1, 1);
    /* n=8. */
    for (int i = 0; i < 8; i++) { a[i] = (uint64_t)(i + 1); b[i] = 1; } b[8] = 0;
    emit_case(out, "edge", a, b, 8, 0, 0);
    emit_case(out, "edge", a, b, 8, 0, 1);
    /* Borrow propagation through all limbs. */
    for (int i = 0; i < 4; i++) { a[i] = 0; b[i] = 0; } b[0] = 1; b[4] = 0;
    emit_case(out, "edge", a, b, 4, 0, 0);
    /* extra=1 with high-bit b. */
    a[0] = 0; b[0] = 0; b[1] = (uint64_t)1 << 63; b[2] = 0;
    emit_case(out, "edge", a, b, 2, 0, 1);
    /* MSB-set a, MSB-set b. */
    a[0] = (uint64_t)1 << 63; a[1] = 0; b[0] = (uint64_t)1 << 63; b[1] = 0; b[2] = 0;
    emit_case(out, "edge", a, b, 2, 0, 0);
    /* n=1 only. */
    a[0] = 42; b[0] = 13; b[1] = 0;
    emit_case(out, "edge", a, b, 1, 0, 0);
    emit_case(out, "edge", a, b, 1, 1, 0);
    a[0] = 42; b[0] = 13; b[1] = 1;
    emit_case(out, "edge", a, b, 1, 0, 1);
    /* a < (b>>1) + cy. */
    a[0] = 3; b[0] = 8; b[1] = 0;
    emit_case(out, "edge", a, b, 1, 0, 1);   /* 3 - 4 = -1, borrow */
    a[0] = 4; b[0] = 8; b[1] = 0;
    emit_case(out, "edge", a, b, 1, 1, 1);   /* 4 - 4 - 1 = -1, borrow */
    /* Multi-limb with extra=1. */
    a[0] = 0; a[1] = 0; a[2] = 0; b[0] = 1; b[1] = 0; b[2] = 0; b[3] = 0;
    emit_case(out, "edge", a, b, 3, 0, 1);   /* effectively a -= 0 + extra-shifted small */
    /* a = max - 1, b = 1. */
    a[0] = UINT64_MAX - 1; b[0] = 1; b[1] = 0;
    emit_case(out, "edge", a, b, 1, 0, 0);
    emit_case(out, "edge", a, b, 1, 1, 0);
    /* a = 1, b = UINT64_MAX. */
    a[0] = 1; b[0] = UINT64_MAX; b[1] = 0;
    emit_case(out, "edge", a, b, 1, 0, 0);
    /* a = UINT64_MAX, b = UINT64_MAX, cy = 1, extra = 1. */
    a[0] = UINT64_MAX; b[0] = UINT64_MAX; b[1] = 0;
    emit_case(out, "edge", a, b, 1, 1, 1);
    /* Various small. */
    a[0] = 100; b[0] = 50; b[1] = 0;
    emit_case(out, "edge", a, b, 1, 1, 0);
    a[0] = 100; b[0] = 100; b[1] = 0;
    emit_case(out, "edge", a, b, 1, 1, 0);
    /* All-max, n=2. */
    a[0] = UINT64_MAX; a[1] = UINT64_MAX; b[0] = UINT64_MAX; b[1] = UINT64_MAX; b[2] = 0;
    emit_case(out, "edge", a, b, 2, 1, 0);
    /* Mixed signs of carry. */
    a[0] = 0; a[1] = 0; b[0] = 0; b[1] = 0; b[2] = 0;
    emit_case(out, "edge", a, b, 2, 1, 0);
    a[0] = UINT64_MAX; a[1] = 0; b[0] = 0; b[1] = 0; b[2] = 0;
    emit_case(out, "edge", a, b, 2, 1, 1);
    /* Big difference. */
    a[0] = 0; a[1] = 0; a[2] = 1; b[0] = 1; b[1] = 0; b[2] = 0; b[3] = 0;
    emit_case(out, "edge", a, b, 3, 0, 0);

    /* adversarial: 12 */
    /* cy=1 and rp == UINT64_MAX triggers the secondary borrow condition. */
    a[0] = 0; b[0] = UINT64_MAX; b[1] = 0;
    emit_case(out, "adversarial", a, b, 1, 1, 0);   /* 0 - (UINT64_MAX) - 1; underflow + secondary check */
    a[0] = 1; b[0] = UINT64_MAX; b[1] = 0;
    emit_case(out, "adversarial", a, b, 1, 1, 0);
    /* extra=1 with cy=1 across multi-limb. */
    a[0] = 0; a[1] = 0; b[0] = 1; b[1] = 0; b[2] = 0;
    emit_case(out, "adversarial", a, b, 2, 1, 1);
    /* a[i] - bb where bb wraps. */
    a[0] = 0; a[1] = 0; a[2] = 0; a[3] = 1;
    b[0] = 0; b[1] = 0; b[2] = 0; b[3] = 0; b[4] = 0;
    emit_case(out, "adversarial", a, b, 4, 1, 0);
    /* extra=1 with successive carries. */
    for (int i = 0; i < 4; i++) { a[i] = 0; b[i] = UINT64_MAX; } b[4] = 0;
    emit_case(out, "adversarial", a, b, 4, 0, 1);
    /* n=8 all max with all carries. */
    for (int i = 0; i < 8; i++) { a[i] = 0; b[i] = UINT64_MAX; } b[8] = 0;
    emit_case(out, "adversarial", a, b, 8, 1, 0);
    /* Random-ish. */
    a[0] = 0x123; a[1] = 0xABC; b[0] = 0x456; b[1] = 0xDEF; b[2] = 0;
    emit_case(out, "adversarial", a, b, 2, 0, 0);
    a[0] = 0x123; a[1] = 0xABC; b[0] = 0x456; b[1] = 0xDEF; b[2] = 0;
    emit_case(out, "adversarial", a, b, 2, 1, 0);
    a[0] = 0x123; a[1] = 0xABC; b[0] = 0x456; b[1] = 0xDEF; b[2] = 1;
    emit_case(out, "adversarial", a, b, 2, 0, 1);
    /* Half-overflow at limb boundary. */
    a[0] = ((uint64_t)1 << 63); a[1] = 0; b[0] = ((uint64_t)1 << 63); b[1] = 0; b[2] = 0;
    emit_case(out, "adversarial", a, b, 2, 0, 0);
    /* Boundary of secondary cy condition. */
    a[0] = 0; b[0] = 0; b[1] = 0;
    emit_case(out, "adversarial", a, b, 1, 1, 0);
    /* Long sequence with carry propagation. */
    for (int i = 0; i < 8; i++) { a[i] = 0; b[i] = 0; } b[8] = 0;
    a[7] = 1;
    emit_case(out, "adversarial", a, b, 8, 1, 0);

    /* fuzz: 55 */
    {
        xs64_t rng;
        xs64_seed(&rng, 0xBEEFCAFEABCDEF01ULL);
        for (int rep = 0; rep < 55; ++rep) {
            const size_t n = 1 + (size_t)(xs64_below(&rng, MAX_N));
            const uint64_t cy = xs64_below(&rng, 2);
            const int extra = (int)xs64_below(&rng, 2);
            for (size_t i = 0; i < n; ++i) a[i] = xs64_next(&rng);
            for (size_t i = 0; i < n + 1; ++i) b[i] = xs64_next(&rng);
            emit_case(out, "fuzz", a, b, n, cy, extra);
        }
    }

    /* mined: 5 */
    a[0] = 10; b[0] = 5; b[1] = 0;
    emit_case(out, "mined", a, b, 1, 0, 0);
    a[0] = 5; b[0] = 10; b[1] = 0;
    emit_case(out, "mined", a, b, 1, 0, 0);
    a[0] = 10; b[0] = 8; b[1] = 0;
    emit_case(out, "mined", a, b, 1, 0, 1);
    a[0] = 0; a[1] = 1; b[0] = 1; b[1] = 0; b[2] = 0;
    emit_case(out, "mined", a, b, 2, 0, 0);
    a[0] = 0; a[1] = 0; b[0] = UINT64_MAX; b[1] = UINT64_MAX; b[2] = 0;
    emit_case(out, "mined", a, b, 2, 0, 0);

    return 0;
}
