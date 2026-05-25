/*
 * golden_driver.c -- Golden master for MPFR's mpfr_set_uj_2exp.
 *
 * C: int mpfr_set_uj_2exp(mpfr_t rop, uintmax_t j, intmax_t e, mpfr_rnd_t rnd)
 *    Sets rop = j * 2^e at prec=MPFR_PREC(rop), rounded per rnd.
 *    Ref: mpfr/src/set_uj.c L36-L132.
 *
 * Wire: {"inputs":{"j":"<dec>","e":"<int64-dec>","prec":"<dec>","rnd":"RND_"},
 *        "output":{"value":<mpfr>,"ternary":<int>}}.
 *
 * Tag distribution (Rule 7): happy 20, edge 30, adv 12, fuzz 50, mined 5.
 *
 * NOTE: e is kept within [-1000, 1000] so the C clamping against
 * emax/emin doesn't fire (TS port has no exponent range surface).
 */
#include "common.h"
#include <assert.h>
#include <inttypes.h>
#include <limits.h>
#include <stdint.h>
#include <string.h>

#if GMP_NUMB_BITS != 64
#  error "mpfr_set_uj_2exp golden_driver requires GMP_NUMB_BITS == 64"
#endif

#define TS_PREC_MAX ((uint64_t)((1ULL << 31) - 257ULL))

extern int mpfr_set_uj_2exp(mpfr_ptr, uintmax_t, intmax_t, mpfr_rnd_t);

/* Emit one case. */
static inline void emit_case(FILE *out, const char *tag,
                             uint64_t j, int64_t e,
                             uint64_t prec, mpfr_rnd_t rnd) {
    assert(prec >= 1 && prec <= TS_PREC_MAX);
    mpfr_t rop;
    mpfr_init2(rop, (mpfr_prec_t)prec);
    const uint64_t t0 = now_ns();
    const int ternary = mpfr_set_uj_2exp(rop, (uintmax_t)j, (intmax_t)e, rnd);
    const uint64_t elapsed = now_ns() - t0;
    jl_begin(out, tag);
    /* j as decimal string (the TS side decodes "^\d+$" to bigint). */
    char jbuf[32];
    snprintf(jbuf, sizeof(jbuf), "%" PRIu64, j);
    jl_kv_str(out, 1, "j", jbuf);
    jl_kv_i64(out, 0, "e", e);
    jl_kv_u64(out, 0, "prec", prec);
    jl_kv_rnd(out, 0, "rnd", rnd);
    jl_end_inputs(out);
    jl_output_result(out, rop, ternary);
    jl_finish(out, elapsed);
    mpfr_clear(rop);
}

int main(void) {
    FILE *out = stdout;
    const mpfr_rnd_t RNDS[5] = { MPFR_RNDN, MPFR_RNDZ, MPFR_RNDU, MPFR_RNDD, MPFR_RNDA };

    /* happy: 20 -- small j across common (e, prec, rnd) cases. */
    emit_case(out, "happy", 0, 0, 53, MPFR_RNDN);
    emit_case(out, "happy", 1, 0, 53, MPFR_RNDN);
    emit_case(out, "happy", 7, 0, 53, MPFR_RNDN);
    emit_case(out, "happy", 17, 10, 53, MPFR_RNDN);
    emit_case(out, "happy", 100, -5, 53, MPFR_RNDN);
    emit_case(out, "happy", 12345, 0, 64, MPFR_RNDZ);
    emit_case(out, "happy", 65535, 16, 53, MPFR_RNDU);
    emit_case(out, "happy", 1ULL << 32, 0, 53, MPFR_RNDD);
    emit_case(out, "happy", 1ULL << 53, 0, 53, MPFR_RNDA);
    emit_case(out, "happy", 1ULL << 60, 0, 64, MPFR_RNDN);
    emit_case(out, "happy", 3, 0, 24, MPFR_RNDN);
    emit_case(out, "happy", 3, 0, 53, MPFR_RNDN);
    emit_case(out, "happy", 3, 0, 100, MPFR_RNDN);
    emit_case(out, "happy", 5, -3, 53, MPFR_RNDN);
    emit_case(out, "happy", 1, 100, 53, MPFR_RNDN);
    emit_case(out, "happy", 1, -100, 53, MPFR_RNDN);
    emit_case(out, "happy", 99, 7, 53, MPFR_RNDN);
    emit_case(out, "happy", 12345, 0, 32, MPFR_RNDN);
    emit_case(out, "happy", 256, -8, 53, MPFR_RNDN);
    emit_case(out, "happy", 1024, 5, 53, MPFR_RNDN);

    /* edge: 30 -- j=0, j=1, single-bit, prec=1, e at limits. */
    emit_case(out, "edge", 0, 0, 1, MPFR_RNDN);
    emit_case(out, "edge", 0, 100, 53, MPFR_RNDN);
    emit_case(out, "edge", 0, -100, 53, MPFR_RNDN);
    emit_case(out, "edge", 0, 0, 256, MPFR_RNDN);
    emit_case(out, "edge", 1, 0, 1, MPFR_RNDN);
    emit_case(out, "edge", 1, 0, 53, MPFR_RNDN);
    emit_case(out, "edge", 1, 100, 1, MPFR_RNDN);
    emit_case(out, "edge", 1, -100, 1, MPFR_RNDN);
    emit_case(out, "edge", 2, 0, 1, MPFR_RNDN);
    emit_case(out, "edge", 3, 0, 1, MPFR_RNDN);
    emit_case(out, "edge", 3, 0, 2, MPFR_RNDN);
    emit_case(out, "edge", 1ULL << 63, 0, 1, MPFR_RNDN);
    emit_case(out, "edge", 1ULL << 63, 0, 53, MPFR_RNDN);
    emit_case(out, "edge", 1ULL << 63, 0, 64, MPFR_RNDN);
    emit_case(out, "edge", 1ULL << 63, 0, 65, MPFR_RNDN);
    emit_case(out, "edge", ~(uint64_t)0, 0, 64, MPFR_RNDN);
    emit_case(out, "edge", ~(uint64_t)0, 0, 65, MPFR_RNDN);
    emit_case(out, "edge", ~(uint64_t)0, 0, 53, MPFR_RNDZ);
    emit_case(out, "edge", ~(uint64_t)0, 0, 53, MPFR_RNDU);
    emit_case(out, "edge", ~(uint64_t)0, 0, 53, MPFR_RNDD);
    emit_case(out, "edge", ~(uint64_t)0, 0, 53, MPFR_RNDA);
    emit_case(out, "edge", 5, 0, 3, MPFR_RNDN);
    emit_case(out, "edge", 5, 0, 3, MPFR_RNDZ);
    emit_case(out, "edge", 5, 0, 3, MPFR_RNDU);
    emit_case(out, "edge", 5, 0, 3, MPFR_RNDD);
    emit_case(out, "edge", 5, 0, 3, MPFR_RNDA);
    emit_case(out, "edge", 1ULL << 30, 30, 53, MPFR_RNDN);
    emit_case(out, "edge", 1ULL << 30, -30, 53, MPFR_RNDN);
    emit_case(out, "edge", 1234567890123ULL, 0, 53, MPFR_RNDN);
    emit_case(out, "edge", 1234567890123ULL, 100, 53, MPFR_RNDN);

    /* adversarial: 12 -- tie-rounding patterns at prec boundary. */
    emit_case(out, "adversarial", 5, 0, 2, MPFR_RNDN);   /* 5 = 101, prec 2 -> tie */
    emit_case(out, "adversarial", 6, 0, 2, MPFR_RNDN);   /* 6 = 110, prec 2 -> tie */
    emit_case(out, "adversarial", 0xAAAAAAAAAAAAAAABULL, 0, 53, MPFR_RNDN);
    emit_case(out, "adversarial", 0xAAAAAAAAAAAAAAABULL, 0, 53, MPFR_RNDU);
    emit_case(out, "adversarial", 0xAAAAAAAAAAAAAAABULL, 0, 53, MPFR_RNDD);
    emit_case(out, "adversarial", 0xCCCCCCCCCCCCCCCCULL, 0, 64, MPFR_RNDN);
    emit_case(out, "adversarial", (1ULL << 63) | 1ULL, 0, 1, MPFR_RNDN);
    emit_case(out, "adversarial", (1ULL << 63) | 1ULL, 0, 2, MPFR_RNDN);
    emit_case(out, "adversarial", (1ULL << 63) | (1ULL << 62), 0, 1, MPFR_RNDN);
    emit_case(out, "adversarial", 0xFFFFFFFFFFFFFFFFULL, 1000, 53, MPFR_RNDN);
    emit_case(out, "adversarial", 0xFFFFFFFFFFFFFFFFULL, -1000, 53, MPFR_RNDN);
    emit_case(out, "adversarial", (1ULL << 53) + 1, 0, 53, MPFR_RNDN);

    /* fuzz: 50 */
    {
        xs64_t rng;
        xs64_seed(&rng, 0xABBA1234ABBA5678ULL);
        for (int rep = 0; rep < 50; ++rep) {
            const uint64_t j = xs64_next(&rng);
            const int64_t e = (int64_t)(xs64_below(&rng, 2001)) - 1000;  /* [-1000, 1000] */
            const uint64_t prec = 1 + xs64_below(&rng, 200);
            const mpfr_rnd_t rnd = RNDS[xs64_below(&rng, 5)];
            emit_case(out, "fuzz", j, e, prec, rnd);
        }
    }

    /* mined: 5 -- canonical test patterns from tset_uj.c. */
    emit_case(out, "mined", 0, 0, 53, MPFR_RNDN);
    emit_case(out, "mined", 1, 0, 53, MPFR_RNDN);
    emit_case(out, "mined", 1ULL << 32, 0, 53, MPFR_RNDN);
    emit_case(out, "mined", ~(uint64_t)0, 0, 64, MPFR_RNDN);
    emit_case(out, "mined", 1, 1, 53, MPFR_RNDN);

    return 0;
}
