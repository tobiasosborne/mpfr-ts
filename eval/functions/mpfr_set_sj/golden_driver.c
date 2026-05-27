/*
 * golden_driver.c -- Golden master for MPFR's mpfr_set_sj.
 *
 * C: int mpfr_set_sj(mpfr_t rop, intmax_t j, mpfr_rnd_t rnd)
 *    Sets rop = j at prec=MPFR_PREC(rop), rounded per rnd.
 *    Ref: mpfr/src/set_sj.c L27-L31 (one-line delegate to set_sj_2exp
 *    with e = 0).
 *
 * Wire: {"inputs":{"j":"<signed-dec>","prec":"<dec>","rnd":"RND_"},
 *        "output":{"value":<mpfr>,"ternary":<int>}}.
 *
 * Tag distribution (Rule 7): happy 20, edge 30, adv 12, fuzz 50, mined 5.
 *
 * NOTE: Inputs cover the full int64 range including INT64_MIN (-2^63)
 * where the C body's `-(uintmax_t)j` overflow-into-2^63 trick fires
 * inside the underlying set_sj_2exp delegate.
 */
#include "common.h"
#include <assert.h>
#include <inttypes.h>
#include <limits.h>
#include <stdint.h>
#include <string.h>

#if GMP_NUMB_BITS != 64
#  error "mpfr_set_sj golden_driver requires GMP_NUMB_BITS == 64"
#endif

#define TS_PREC_MAX ((uint64_t)((1ULL << 31) - 257ULL))

extern int mpfr_set_sj(mpfr_ptr, intmax_t, mpfr_rnd_t);

/* Emit one case. */
static inline void emit_case(FILE *out, const char *tag,
                             int64_t j, uint64_t prec, mpfr_rnd_t rnd) {
    assert(prec >= 1 && prec <= TS_PREC_MAX);
    mpfr_t rop;
    mpfr_init2(rop, (mpfr_prec_t)prec);
    const uint64_t t0 = now_ns();
    const int ternary = mpfr_set_sj(rop, (intmax_t)j, rnd);
    const uint64_t elapsed = now_ns() - t0;
    jl_begin(out, tag);
    /* j as signed decimal string (TS side decodes "^-?\d+$" to bigint). */
    char jbuf[32];
    snprintf(jbuf, sizeof(jbuf), "%" PRId64, j);
    jl_kv_str(out, 1, "j", jbuf);
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

    /* happy: 20 -- small +/- ints across common (prec, rnd) cases. */
    emit_case(out, "happy", 0, 53, MPFR_RNDN);
    emit_case(out, "happy", 1, 53, MPFR_RNDN);
    emit_case(out, "happy", -1, 53, MPFR_RNDN);
    emit_case(out, "happy", 17, 53, MPFR_RNDN);
    emit_case(out, "happy", -17, 53, MPFR_RNDN);
    emit_case(out, "happy", 42, 53, MPFR_RNDN);
    emit_case(out, "happy", -42, 53, MPFR_RNDN);
    emit_case(out, "happy", 1742, 53, MPFR_RNDN);
    emit_case(out, "happy", -1742, 53, MPFR_RNDN);
    emit_case(out, "happy", 12345, 64, MPFR_RNDZ);
    emit_case(out, "happy", -12345, 64, MPFR_RNDZ);
    emit_case(out, "happy", 65535, 53, MPFR_RNDU);
    emit_case(out, "happy", -65535, 53, MPFR_RNDD);
    emit_case(out, "happy", 1LL << 32, 53, MPFR_RNDD);
    emit_case(out, "happy", -(1LL << 32), 53, MPFR_RNDU);
    emit_case(out, "happy", 1LL << 53, 53, MPFR_RNDA);
    emit_case(out, "happy", -(1LL << 53), 53, MPFR_RNDA);
    emit_case(out, "happy", 3, 24, MPFR_RNDN);
    emit_case(out, "happy", -3, 24, MPFR_RNDN);
    emit_case(out, "happy", 7, 100, MPFR_RNDN);

    /* edge: 30 -- j=0 (sign forced positive), +/-1, single-bit, prec=1,
     * INT64_MIN (where the C `-(uintmax_t)j` overflow trick fires). */
    emit_case(out, "edge", 0, 1, MPFR_RNDN);
    emit_case(out, "edge", 0, 1, MPFR_RNDZ);
    emit_case(out, "edge", 0, 1, MPFR_RNDU);
    emit_case(out, "edge", 0, 1, MPFR_RNDD);
    emit_case(out, "edge", 0, 1, MPFR_RNDA);
    emit_case(out, "edge", 1, 1, MPFR_RNDN);
    emit_case(out, "edge", -1, 1, MPFR_RNDN);
    emit_case(out, "edge", 2, 1, MPFR_RNDN);
    emit_case(out, "edge", -2, 1, MPFR_RNDN);
    emit_case(out, "edge", 3, 1, MPFR_RNDN);
    emit_case(out, "edge", -3, 1, MPFR_RNDN);
    emit_case(out, "edge", 3, 2, MPFR_RNDN);
    emit_case(out, "edge", -3, 2, MPFR_RNDN);
    emit_case(out, "edge", INT64_MIN, 1, MPFR_RNDN);
    emit_case(out, "edge", INT64_MIN, 53, MPFR_RNDN);
    emit_case(out, "edge", INT64_MIN, 63, MPFR_RNDN);
    emit_case(out, "edge", INT64_MIN, 64, MPFR_RNDN);
    emit_case(out, "edge", INT64_MIN, 65, MPFR_RNDN);
    emit_case(out, "edge", INT64_MAX, 1, MPFR_RNDN);
    emit_case(out, "edge", INT64_MAX, 53, MPFR_RNDN);
    emit_case(out, "edge", INT64_MAX, 63, MPFR_RNDN);
    emit_case(out, "edge", INT64_MAX, 64, MPFR_RNDN);
    /* Both signs across all 5 modes at prec=3 (5 = 101). */
    emit_case(out, "edge", 5, 3, MPFR_RNDN);
    emit_case(out, "edge", 5, 3, MPFR_RNDZ);
    emit_case(out, "edge", 5, 3, MPFR_RNDU);
    emit_case(out, "edge", 5, 3, MPFR_RNDD);
    emit_case(out, "edge", 5, 3, MPFR_RNDA);
    emit_case(out, "edge", -5, 3, MPFR_RNDU);
    emit_case(out, "edge", -5, 3, MPFR_RNDD);
    emit_case(out, "edge", -5, 3, MPFR_RNDA);

    /* adversarial: 12 -- tie-rounding patterns, sign-flip-and-rnd-invert path. */
    emit_case(out, "adversarial", -5, 2, MPFR_RNDN);   /* j=-5 = -101, prec 2 tie */
    emit_case(out, "adversarial", -6, 2, MPFR_RNDN);   /* j=-6 = -110, prec 2 tie */
    emit_case(out, "adversarial", -7, 2, MPFR_RNDU);   /* rnd flips to RNDD inside */
    emit_case(out, "adversarial", -7, 2, MPFR_RNDD);   /* rnd flips to RNDU inside */
    emit_case(out, "adversarial", -5, 1, MPFR_RNDU);
    emit_case(out, "adversarial", -5, 1, MPFR_RNDD);
    emit_case(out, "adversarial", -5, 1, MPFR_RNDA);
    emit_case(out, "adversarial", -5, 1, MPFR_RNDZ);
    emit_case(out, "adversarial", 5, 1, MPFR_RNDU);
    emit_case(out, "adversarial", 5, 1, MPFR_RNDD);
    emit_case(out, "adversarial", INT64_MIN, 63, MPFR_RNDU);
    emit_case(out, "adversarial", INT64_MIN, 63, MPFR_RNDD);

    /* fuzz: 50 random (j, prec, rnd) over the full int64 range. */
    {
        xs64_t rng;
        xs64_seed(&rng, 0xBADC0FFEE0DDF00DULL);
        const uint64_t precs[7] = { 1, 2, 24, 53, 64, 100, 256 };
        for (int rep = 0; rep < 50; ++rep) {
            const uint64_t raw = xs64_next(&rng);
            const int64_t j = (int64_t)raw;
            const uint64_t prec = precs[xs64_below(&rng, 7)];
            const mpfr_rnd_t rnd = RNDS[xs64_below(&rng, 5)];
            emit_case(out, "fuzz", j, prec, rnd);
        }
    }

    /* mined: 5 -- patterns from mpfr/tests/tset_sj.c. */
    emit_case(out, "mined", 1742, 53, MPFR_RNDN);  /* L150 */
    emit_case(out, "mined", -INT64_MAX, 64, MPFR_RNDN);  /* L144 */
    emit_case(out, "mined", 0, 53, MPFR_RNDN);
    emit_case(out, "mined", INT64_MIN, 64, MPFR_RNDN);
    emit_case(out, "mined", INT64_MAX, 64, MPFR_RNDN);

    return 0;
}
