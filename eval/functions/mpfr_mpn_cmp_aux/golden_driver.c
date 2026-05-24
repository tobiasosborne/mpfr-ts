/*
 * golden_driver.c — Golden master for MPFR's mpfr_mpn_cmp_aux.
 *
 * Static helper; mirrored here verbatim. Returns sign of (a - (b >> extra))
 * after MSB alignment.
 *
 * Tag distribution: happy 22, edge 32, adversarial 12, fuzz 55, mined 5.
 */
#include "common.h"

#include <assert.h>
#include <inttypes.h>
#include <stdint.h>
#include <string.h>
#include <sys/types.h>

#define MAX_N 8

/* Mirror of mpfr/src/div.c L662-L718, with mp_size_t→ssize_t and
 * mp_limb_t→uint64_t. GMP_NUMB_BITS = 64. */
#define GNUMBITS 64

static int my_mpn_cmp_aux(const uint64_t *ap, ssize_t an,
                          const uint64_t *bp, ssize_t bn, int extra) {
    int cmp = 0;
    ssize_t k;
    uint64_t bb;
    if (an >= bn) {
        k = an - bn;
        while (cmp == 0 && bn > 0) {
            bn--;
            bb = extra ? ((bp[bn + 1] << (GNUMBITS - 1)) | (bp[bn] >> 1))
                       : bp[bn];
            cmp = (ap[k + bn] > bb) ? 1 : ((ap[k + bn] < bb) ? -1 : 0);
        }
        bb = extra ? (bp[0] << (GNUMBITS - 1)) : 0;
        while (cmp == 0 && k > 0) {
            k--;
            cmp = (ap[k] > bb) ? 1 : ((ap[k] < bb) ? -1 : 0);
            bb = 0;
        }
        if (cmp == 0 && bb != 0) cmp = -1;
    } else {
        k = bn - an;
        while (cmp == 0 && an > 0) {
            an--;
            bb = extra ? ((bp[k + an + 1] << (GNUMBITS - 1)) | (bp[k + an] >> 1))
                       : bp[k + an];
            if (ap[an] > bb) cmp = 1;
            else if (ap[an] < bb) cmp = -1;
        }
        while (cmp == 0 && k > 0) {
            k--;
            bb = extra ? ((bp[k + 1] << (GNUMBITS - 1)) | (bp[k] >> 1))
                       : bp[k];
            cmp = (bb != 0) ? -1 : 0;
        }
        if (cmp == 0 && extra && (bp[0] & 1)) cmp = -1;
    }
    return cmp;
}

static inline void emit_case(FILE *out, const char *tag,
                             uint64_t *ap, size_t an,
                             uint64_t *bp, size_t bn, int extra) {
    assert(an >= 1 && an <= MAX_N);
    assert(bn >= 1 && bn <= MAX_N);
    assert(extra == 0 || extra == 1);
    /* Zero the slack past the declared end of each array so my_mpn_cmp_aux's
     * extra=1 one-past-end reads (bp[bn], bp[bn+1]) are deterministic and
     * match what the TS port assumes (past-end = 0). Without this, a prior
     * case's writes to ap[bn..]/bp[bn..] leak through to this case. */
    for (size_t i = an; i < MAX_N + 2; i++) ap[i] = 0;
    for (size_t i = bn; i < MAX_N + 2; i++) bp[i] = 0;
    const uint64_t t0 = now_ns();
    const int r = my_mpn_cmp_aux(ap, (ssize_t)an, bp, (ssize_t)bn, extra);
    const uint64_t elapsed = now_ns() - t0;
    jl_begin(out, tag);
    jl_kv_limbs(out, 1, "ap", ap, an);
    jl_kv_limbs(out, 0, "bp", bp, bn);
    jl_kv_int(out, 0, "extra", extra);
    jl_end_inputs(out);
    jl_output_scalar_int(out, (r > 0) ? 1 : (r < 0) ? -1 : 0);
    jl_finish(out, elapsed);
}

int main(void) {
    FILE *out = stdout;
    /* my_mpn_cmp_aux reads bp[bn] and bp[bn+1] when extra=1 (mirrors the C
     * source's intentional one-past-end peek). The arrays must therefore
     * carry zeroed slack — declare MAX_N+2 and zero each pass to make
     * those reads deterministic. The wire output still uses only bn limbs
     * (jl_kv_limbs writes ap[0..an), bp[0..bn)), so the slack is invisible
     * to the TS port. */
    uint64_t a[MAX_N + 2], b[MAX_N + 2];
    memset(a, 0, sizeof a);
    memset(b, 0, sizeof b);

    /* happy: 22 — equal-length cases, extra=0, basic compares. */
    a[0] = 1; b[0] = 1; emit_case(out, "happy", a, 1, b, 1, 0);
    a[0] = 1; b[0] = 2; emit_case(out, "happy", a, 1, b, 1, 0);
    a[0] = 2; b[0] = 1; emit_case(out, "happy", a, 1, b, 1, 0);
    a[0] = 0; b[0] = 0; emit_case(out, "happy", a, 1, b, 1, 0);
    a[0] = 0; b[0] = 1; emit_case(out, "happy", a, 1, b, 1, 0);
    a[0] = UINT64_MAX; b[0] = UINT64_MAX; emit_case(out, "happy", a, 1, b, 1, 0);
    a[0] = UINT64_MAX; b[0] = 1; emit_case(out, "happy", a, 1, b, 1, 0);
    a[0] = 1; a[1] = 1; b[0] = 1; b[1] = 1; emit_case(out, "happy", a, 2, b, 2, 0);
    a[0] = 0; a[1] = 1; b[0] = 0; b[1] = 1; emit_case(out, "happy", a, 2, b, 2, 0);
    a[0] = 1; a[1] = 2; b[0] = 2; b[1] = 1; emit_case(out, "happy", a, 2, b, 2, 0);
    a[0] = 1; a[1] = 2; b[0] = 1; b[1] = 1; emit_case(out, "happy", a, 2, b, 2, 0);
    a[0] = 1; a[1] = 1; b[0] = 1; b[1] = 2; emit_case(out, "happy", a, 2, b, 2, 0);
    /* extra=1 cases. */
    a[0] = 4; b[0] = 8; emit_case(out, "happy", a, 1, b, 1, 1);   /* 4 vs 8>>1=4 → equal-ish */
    a[0] = 4; b[0] = 9; emit_case(out, "happy", a, 1, b, 1, 1);   /* 4 vs 9>>1=4, but bit 0 of b → cmp=-1 */
    a[0] = 5; b[0] = 8; emit_case(out, "happy", a, 1, b, 1, 1);
    a[0] = 3; b[0] = 8; emit_case(out, "happy", a, 1, b, 1, 1);
    a[0] = 1; b[0] = 1; emit_case(out, "happy", a, 1, b, 1, 1);   /* 1 vs 1>>1=0, bit 0 → -1 */
    /* Different lengths. */
    a[0] = 1; a[1] = 1; b[0] = 1; emit_case(out, "happy", a, 2, b, 1, 0);
    a[0] = 1; b[0] = 1; b[1] = 1; emit_case(out, "happy", a, 1, b, 2, 0);
    a[0] = 0; a[1] = 1; b[0] = 1; emit_case(out, "happy", a, 2, b, 1, 0);   /* a high = 1, b high = 1 → equal high; a low = 0, "b low" = 0 (k=1, k-- only loops to bp[0] zero phase) */
    a[0] = 1; b[0] = 0; b[1] = 1; emit_case(out, "happy", a, 1, b, 2, 0);
    a[0] = 1; a[1] = 2; a[2] = 3; b[0] = 1; b[1] = 2; b[2] = 3; emit_case(out, "happy", a, 3, b, 3, 0);

    /* edge: 32 — extra=1 with various widths. */
    /* extra=1 with high-bit b. */
    b[0] = (uint64_t)1 << 63; a[0] = 0;
    emit_case(out, "edge", a, 1, b, 1, 1);   /* a=0, b>>1 = 1<<62 → -1 */
    a[0] = (uint64_t)1 << 62;
    emit_case(out, "edge", a, 1, b, 1, 1);   /* a=1<<62, b>>1=1<<62 → 0 */
    a[0] = ((uint64_t)1 << 62) + 1;
    emit_case(out, "edge", a, 1, b, 1, 1);   /* a > b>>1 → +1 */
    /* Multi-limb extra=1. */
    a[0] = 0; a[1] = 0; b[0] = 0; b[1] = (uint64_t)1 << 63;
    emit_case(out, "edge", a, 2, b, 2, 1);  /* a=0, b>>1 ≠ 0 → -1 */
    /* k>0 path with extra=1. */
    a[0] = 0; a[1] = 0; a[2] = 1; b[0] = 1;
    emit_case(out, "edge", a, 3, b, 1, 1);
    /* an > bn, extra=0. */
    a[0] = 1; a[1] = 1; a[2] = 0; b[0] = 1; b[1] = 1;
    emit_case(out, "edge", a, 3, b, 2, 0);
    a[0] = 0; a[1] = 1; a[2] = 0; b[0] = 1; b[1] = 1;
    emit_case(out, "edge", a, 3, b, 2, 0);
    /* an < bn, extra=0. */
    a[0] = 1; a[1] = 1; b[0] = 1; b[1] = 1; b[2] = 0;
    emit_case(out, "edge", a, 2, b, 3, 0);
    a[0] = 1; a[1] = 1; b[0] = 1; b[1] = 1; b[2] = 1;
    emit_case(out, "edge", a, 2, b, 3, 0);
    /* all-max vs all-max. */
    for (int i = 0; i < MAX_N; i++) a[i] = b[i] = UINT64_MAX;
    emit_case(out, "edge", a, MAX_N, b, MAX_N, 0);
    emit_case(out, "edge", a, MAX_N, b, MAX_N, 1);
    /* MAX_N=8, extra=0. */
    for (int i = 0; i < 8; i++) { a[i] = (uint64_t)i; b[i] = (uint64_t)(i + 1); }
    emit_case(out, "edge", a, 8, b, 8, 0);
    /* alternating. */
    for (int i = 0; i < 4; i++) { a[i] = i; b[i] = i; }
    emit_case(out, "edge", a, 4, b, 4, 1);
    /* Various small. */
    a[0] = 5; b[0] = 10; emit_case(out, "edge", a, 1, b, 1, 1);  /* 5 vs 5 → 0; bit 0 of b=0 → 0 */
    a[0] = 5; b[0] = 11; emit_case(out, "edge", a, 1, b, 1, 1);  /* 5 vs 5; bit 0 of b=1 → -1 */
    a[0] = 6; b[0] = 11; emit_case(out, "edge", a, 1, b, 1, 1);
    a[0] = 4; b[0] = 11; emit_case(out, "edge", a, 1, b, 1, 1);
    /* MSB cases. */
    a[0] = (uint64_t)1 << 63; b[0] = (uint64_t)1 << 63;
    emit_case(out, "edge", a, 1, b, 1, 0);
    emit_case(out, "edge", a, 1, b, 1, 1);
    /* an=bn=1, b=0. */
    a[0] = 0; b[0] = 0; emit_case(out, "edge", a, 1, b, 1, 1);
    a[0] = 1; b[0] = 0; emit_case(out, "edge", a, 1, b, 1, 1);
    /* an < bn with all high b nonzero. */
    a[0] = 1; b[0] = 0; b[1] = 1; b[2] = 1;
    emit_case(out, "edge", a, 1, b, 3, 0);
    /* an > bn with all high a nonzero. */
    a[0] = 0; a[1] = 1; a[2] = 1; b[0] = 1;
    emit_case(out, "edge", a, 3, b, 1, 0);
    /* Various lengths. */
    for (int i = 0; i < 6; i++) { a[i] = i + 1; b[i] = i + 1; }
    emit_case(out, "edge", a, 6, b, 6, 0);
    emit_case(out, "edge", a, 6, b, 6, 1);
    /* extra=1, bp[0] odd → -1 nudge. */
    a[0] = 0; a[1] = 0; b[0] = 1; b[1] = 0;
    emit_case(out, "edge", a, 2, b, 2, 1);
    a[0] = 0; a[1] = 0; b[0] = 3; b[1] = 0;
    emit_case(out, "edge", a, 2, b, 2, 1);
    /* an=1, bn=8, extra=1. */
    for (int i = 0; i < 8; i++) { b[i] = 1; }
    a[0] = 0;
    emit_case(out, "edge", a, 1, b, 8, 1);

    /* adversarial: 12 */
    /* Just-under, just-over, equal at high precision. */
    for (int i = 0; i < 4; i++) { a[i] = UINT64_MAX; b[i] = UINT64_MAX; }
    a[0] = UINT64_MAX - 1;
    emit_case(out, "adversarial", a, 4, b, 4, 0);
    for (int i = 0; i < 4; i++) { a[i] = UINT64_MAX; b[i] = UINT64_MAX; }
    b[0] = UINT64_MAX - 1;
    emit_case(out, "adversarial", a, 4, b, 4, 0);
    /* extra=1 right at the bit-0-pickup edge. */
    for (int i = 0; i < 4; i++) { a[i] = 0; b[i] = 0; }
    a[3] = (uint64_t)1 << 62; b[3] = (uint64_t)1 << 63;
    emit_case(out, "adversarial", a, 4, b, 4, 1);
    /* an=2, bn=2, extra=1, deep tie. */
    a[0] = (uint64_t)1 << 63; a[1] = 0;
    b[0] = 0; b[1] = 1;
    emit_case(out, "adversarial", a, 2, b, 2, 1);   /* a high = 0, b high>>1 = 0 with carry from b[1]; a low = 1<<63, "b low" = 0 with bp[0] high bit (shifted from b[1]) = 0 → cmp = +1 */
    /* Wide tie + low-bit pickup. */
    a[0] = 0; a[1] = 0; a[2] = (uint64_t)1 << 63;
    b[0] = 1; b[1] = 0; b[2] = ((uint64_t)1 << 63) << 0;  /* b same high */
    emit_case(out, "adversarial", a, 3, b, 3, 1);
    /* extra=0, bn > an, b mantissa partially zero. */
    a[0] = 1; b[0] = 0; b[1] = 0; b[2] = 1;
    emit_case(out, "adversarial", a, 1, b, 3, 0);
    /* extra=1, bn > an. */
    a[0] = 0; b[0] = 1; b[1] = 0; b[2] = 1;
    emit_case(out, "adversarial", a, 1, b, 3, 1);
    /* an=3, bn=1, extra=1. */
    a[0] = 0; a[1] = 0; a[2] = (uint64_t)1 << 62; b[0] = (uint64_t)1 << 63;
    emit_case(out, "adversarial", a, 3, b, 1, 1);
    /* All-zero a, b non-zero. */
    a[0] = 0; a[1] = 0; b[0] = 0; b[1] = 1;
    emit_case(out, "adversarial", a, 2, b, 2, 0);
    /* All-zero b, a non-zero. */
    a[0] = 0; a[1] = 1; b[0] = 0; b[1] = 0;
    emit_case(out, "adversarial", a, 2, b, 2, 0);
    /* Long arrays equal except for one bit. */
    for (int i = 0; i < 8; i++) { a[i] = 0x5555555555555555ULL; b[i] = 0x5555555555555555ULL; }
    a[4] = 0x5555555555555556ULL;
    emit_case(out, "adversarial", a, 8, b, 8, 0);
    /* Long arrays equal except for highest. */
    for (int i = 0; i < 8; i++) { a[i] = 0x5555555555555555ULL; b[i] = 0x5555555555555555ULL; }
    b[7] = 0x5555555555555556ULL;
    emit_case(out, "adversarial", a, 8, b, 8, 0);

    /* fuzz: 55 */
    {
        xs64_t rng;
        xs64_seed(&rng, 0xCAFEBABE12345678ULL);
        for (int rep = 0; rep < 55; ++rep) {
            const size_t an = 1 + (size_t)(xs64_below(&rng, MAX_N));
            const size_t bn = 1 + (size_t)(xs64_below(&rng, MAX_N));
            const int extra = (int)xs64_below(&rng, 2);
            for (size_t i = 0; i < an; ++i) a[i] = xs64_next(&rng);
            for (size_t i = 0; i < bn; ++i) b[i] = xs64_next(&rng);
            emit_case(out, "fuzz", a, an, b, bn, extra);
        }
    }

    /* mined: 5 */
    a[0] = 1; b[0] = 1; emit_case(out, "mined", a, 1, b, 1, 0);
    a[0] = 1; b[0] = 2; emit_case(out, "mined", a, 1, b, 1, 0);
    a[0] = 1; b[0] = 2; emit_case(out, "mined", a, 1, b, 1, 1);
    a[0] = 0; a[1] = 1; b[0] = 0; b[1] = 1; emit_case(out, "mined", a, 2, b, 2, 0);
    a[0] = 0; a[1] = 1; b[0] = 0; b[1] = 2; emit_case(out, "mined", a, 2, b, 2, 1);

    return 0;
}
