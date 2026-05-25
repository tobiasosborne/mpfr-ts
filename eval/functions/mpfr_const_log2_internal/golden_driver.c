/*
 * golden_driver.c -- Golden master for mpfr_const_log2_internal.
 *
 * Calls libmpfr's exported mpfr_const_log2_internal symbol directly.
 * We bypass the cache by calling the internal function rather than
 * mpfr_const_log2 (which routes through the cache).
 *
 * Wire: {"inputs":{"prec":"<dec>","rnd":"RND_"},"output":{"value":<mpfr>,"ternary":<int>}}.
 * Tag distribution (Rule 7): happy 20, edge 30, adv 12, fuzz 50, mined 5.
 *
 * Prec cap: 1024 bits (reference port uses a 2048-bit precomputed
 * constant; cap stays well below for correct-rounding edge safety).
 */
#include "common.h"
#include <assert.h>
#include <inttypes.h>

#if GMP_NUMB_BITS != 64
#  error "mpfr_const_log2_internal golden_driver requires GMP_NUMB_BITS == 64"
#endif

extern int mpfr_const_log2_internal(mpfr_ptr, mpfr_rnd_t);

#define MAX_PREC 1024

static inline void emit_case(FILE *out, const char *tag, uint64_t prec, mpfr_rnd_t rnd) {
    assert(prec >= 1 && prec <= MAX_PREC);
    mpfr_t x;
    mpfr_init2(x, (mpfr_prec_t)prec);
    const uint64_t t0 = now_ns();
    const int ternary = mpfr_const_log2_internal(x, rnd);
    const uint64_t elapsed = now_ns() - t0;
    jl_begin(out, tag);
    jl_kv_u64(out, 1, "prec", prec);
    jl_kv_rnd(out, 0, "rnd", rnd);
    jl_end_inputs(out);
    jl_output_result(out, x, ternary);
    jl_finish(out, elapsed);
    mpfr_clear(x);
}

int main(void) {
    FILE *out = stdout;
    const mpfr_rnd_t RNDS[5] = { MPFR_RNDN, MPFR_RNDZ, MPFR_RNDU, MPFR_RNDD, MPFR_RNDA };
    /* happy: 20 -- common precs across all modes. */
    for (int ri = 0; ri < 5; ++ri) emit_case(out, "happy", 53, RNDS[ri]);
    for (int ri = 0; ri < 5; ++ri) emit_case(out, "happy", 24, RNDS[ri]);
    for (int ri = 0; ri < 5; ++ri) emit_case(out, "happy", 64, RNDS[ri]);
    for (int ri = 0; ri < 5; ++ri) emit_case(out, "happy", 100, RNDS[ri]);
    /* edge: 30 -- prec=1, limb boundaries, various modes. */
    for (int ri = 0; ri < 5; ++ri) emit_case(out, "edge", 1, RNDS[ri]);
    for (int ri = 0; ri < 5; ++ri) emit_case(out, "edge", 2, RNDS[ri]);
    for (int ri = 0; ri < 5; ++ri) emit_case(out, "edge", 63, RNDS[ri]);
    for (int ri = 0; ri < 5; ++ri) emit_case(out, "edge", 65, RNDS[ri]);
    for (int ri = 0; ri < 5; ++ri) emit_case(out, "edge", 127, RNDS[ri]);
    for (int ri = 0; ri < 5; ++ri) emit_case(out, "edge", 129, RNDS[ri]);
    /* adversarial: 12 -- larger precs. */
    emit_case(out, "adversarial", 200, MPFR_RNDN);
    emit_case(out, "adversarial", 256, MPFR_RNDN);
    emit_case(out, "adversarial", 256, MPFR_RNDZ);
    emit_case(out, "adversarial", 256, MPFR_RNDU);
    emit_case(out, "adversarial", 256, MPFR_RNDD);
    emit_case(out, "adversarial", 256, MPFR_RNDA);
    emit_case(out, "adversarial", 512, MPFR_RNDN);
    emit_case(out, "adversarial", 512, MPFR_RNDZ);
    emit_case(out, "adversarial", 512, MPFR_RNDU);
    emit_case(out, "adversarial", 512, MPFR_RNDD);
    emit_case(out, "adversarial", 1024, MPFR_RNDN);
    emit_case(out, "adversarial", 1024, MPFR_RNDZ);
    /* fuzz: 50 -- random (prec, rnd). */
    {
        xs64_t rng;
        xs64_seed(&rng, 0xC0DEC0DEC0DEC0DEULL);
        for (int rep = 0; rep < 50; ++rep) {
            const uint64_t prec = 1 + xs64_below(&rng, MAX_PREC);
            const mpfr_rnd_t rnd = RNDS[xs64_below(&rng, 5)];
            emit_case(out, "fuzz", prec, rnd);
        }
    }
    /* mined: 5 */
    emit_case(out, "mined", 53, MPFR_RNDN);
    emit_case(out, "mined", 53, MPFR_RNDZ);
    emit_case(out, "mined", 100, MPFR_RNDU);
    emit_case(out, "mined", 200, MPFR_RNDD);
    emit_case(out, "mined", 500, MPFR_RNDA);
    return 0;
}
