/*
 * golden_driver.c -- Golden master for MPFR's mpfr_set_uj.
 *
 * C: int mpfr_set_uj(mpfr_t rop, uintmax_t j, mpfr_rnd_t rnd)
 *    Sets rop = j (an unsigned integer) at prec=MPFR_PREC(rop), rounded
 *    per rnd. Ref: mpfr/src/set_uj.c L31-L35 (one-line delegate to
 *    mpfr_set_uj_2exp with e = 0).
 *
 * Wire: {"inputs":{"j":"<unsigned-dec>","prec":"<dec>","rnd":"RND_"},
 *        "output":{"value":<mpfr>,"ternary":<int>}}.
 *
 * Tag distribution (Rule 7): happy 20, edge 30, adv 12, fuzz 50, mined 5.
 *
 * UNSIGNED mirror of mpfr_set_sj's driver. Inputs span the full uint64
 * range [0, 2^64-1] including UINTMAX_MAX (top bit set; the 64-bit
 * mantissa needs rounding at low prec). No negatives -- the result is
 * always +0 (j=0) or a positive normal value, so no sign-flip path.
 *
 * Ref: mpfr/src/set_uj.c -- C reference.
 * Ref: eval/functions/mpfr_set_sj/golden_driver.c -- structural template.
 * Ref: mpfr/tests/tset_sj.c L46-L96 -- mined-case source (check_set_uj).
 */
#include "common.h"
#include <assert.h>
#include <inttypes.h>
#include <limits.h>
#include <stdint.h>
#include <string.h>

#if GMP_NUMB_BITS != 64
#  error "mpfr_set_uj golden_driver requires GMP_NUMB_BITS == 64"
#endif

#define TS_PREC_MAX ((uint64_t)((1ULL << 31) - 257ULL))

extern int mpfr_set_uj(mpfr_ptr, uintmax_t, mpfr_rnd_t);

/* Emit one case. */
static inline void emit_case(FILE *out, const char *tag,
                             uint64_t j, uint64_t prec, mpfr_rnd_t rnd) {
    assert(prec >= 1 && prec <= TS_PREC_MAX);
    mpfr_t rop;
    mpfr_init2(rop, (mpfr_prec_t)prec);
    const uint64_t t0 = now_ns();
    const int ternary = mpfr_set_uj(rop, (uintmax_t)j, rnd);
    const uint64_t elapsed = now_ns() - t0;
    jl_begin(out, tag);
    /* j as unsigned decimal string (TS side decodes "^\d+$" to bigint). */
    char jbuf[24];
    snprintf(jbuf, sizeof(jbuf), "%" PRIu64, j);
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

    /* happy: 20 -- small unsigned ints across common (prec, rnd) cases. */
    emit_case(out, "happy", 0, 53, MPFR_RNDN);
    emit_case(out, "happy", 1, 53, MPFR_RNDN);
    emit_case(out, "happy", 2, 53, MPFR_RNDN);
    emit_case(out, "happy", 17, 53, MPFR_RNDN);
    emit_case(out, "happy", 42, 53, MPFR_RNDN);
    emit_case(out, "happy", 100, 53, MPFR_RNDN);
    emit_case(out, "happy", 255, 53, MPFR_RNDN);
    emit_case(out, "happy", 1742, 53, MPFR_RNDN);
    emit_case(out, "happy", 12345, 64, MPFR_RNDZ);
    emit_case(out, "happy", 65535, 53, MPFR_RNDU);
    emit_case(out, "happy", 65536, 53, MPFR_RNDD);
    emit_case(out, "happy", 1ULL << 32, 53, MPFR_RNDD);
    emit_case(out, "happy", 1ULL << 53, 53, MPFR_RNDU);
    emit_case(out, "happy", 1ULL << 53, 53, MPFR_RNDA);
    emit_case(out, "happy", 3, 24, MPFR_RNDN);
    emit_case(out, "happy", 7, 100, MPFR_RNDN);
    emit_case(out, "happy", 1000000, 53, MPFR_RNDN);
    emit_case(out, "happy", 1000000000ULL, 53, MPFR_RNDN);
    emit_case(out, "happy", 0xDEADBEEFULL, 64, MPFR_RNDN);
    emit_case(out, "happy", 0xCAFEBABEULL, 53, MPFR_RNDZ);

    /* edge: 30 -- j=0 (sign forced +), j=1, single-bit, prec=1, and
     * UINT64_MAX (top bit set; lossless at prec>=64, rounds below). */
    emit_case(out, "edge", 0, 1, MPFR_RNDN);
    emit_case(out, "edge", 0, 1, MPFR_RNDZ);
    emit_case(out, "edge", 0, 1, MPFR_RNDU);
    emit_case(out, "edge", 0, 1, MPFR_RNDD);
    emit_case(out, "edge", 0, 1, MPFR_RNDA);
    emit_case(out, "edge", 1, 1, MPFR_RNDN);
    emit_case(out, "edge", 2, 1, MPFR_RNDN);
    emit_case(out, "edge", 3, 1, MPFR_RNDN);
    emit_case(out, "edge", 3, 2, MPFR_RNDN);
    emit_case(out, "edge", 1ULL << 63, 1, MPFR_RNDN);   /* single top bit */
    emit_case(out, "edge", 1ULL << 63, 64, MPFR_RNDN);
    emit_case(out, "edge", UINT64_MAX, 1, MPFR_RNDN);
    emit_case(out, "edge", UINT64_MAX, 53, MPFR_RNDN);
    emit_case(out, "edge", UINT64_MAX, 63, MPFR_RNDN);
    emit_case(out, "edge", UINT64_MAX, 64, MPFR_RNDN);   /* exact: prec=64 */
    emit_case(out, "edge", UINT64_MAX, 65, MPFR_RNDN);   /* lossless pad */
    emit_case(out, "edge", UINT64_MAX, 100, MPFR_RNDN);
    /* j=5 (=101) across all 5 modes at prec=3 (exact, no rounding). */
    emit_case(out, "edge", 5, 3, MPFR_RNDN);
    emit_case(out, "edge", 5, 3, MPFR_RNDZ);
    emit_case(out, "edge", 5, 3, MPFR_RNDU);
    emit_case(out, "edge", 5, 3, MPFR_RNDD);
    emit_case(out, "edge", 5, 3, MPFR_RNDA);
    /* j=5 (=101) at prec=2: drops the trailing 1-bit -> rounding fires. */
    emit_case(out, "edge", 5, 2, MPFR_RNDN);
    emit_case(out, "edge", 5, 2, MPFR_RNDZ);
    emit_case(out, "edge", 5, 2, MPFR_RNDU);
    emit_case(out, "edge", 5, 2, MPFR_RNDD);
    emit_case(out, "edge", 5, 2, MPFR_RNDA);
    /* INT64_MAX as unsigned (2^63-1) and 2^63 (just above int64). */
    emit_case(out, "edge", (uint64_t)INT64_MAX, 53, MPFR_RNDN);
    emit_case(out, "edge", (1ULL << 63), 53, MPFR_RNDN);
    emit_case(out, "edge", (1ULL << 63) + 1ULL, 53, MPFR_RNDN);

    /* adversarial: 12 -- tie-rounding patterns at the prec boundary. */
    emit_case(out, "adversarial", 6, 2, MPFR_RNDN);   /* 110, prec 2 tie -> even */
    emit_case(out, "adversarial", 7, 2, MPFR_RNDN);   /* 111, prec 2 */
    emit_case(out, "adversarial", 7, 2, MPFR_RNDU);
    emit_case(out, "adversarial", 7, 2, MPFR_RNDD);
    emit_case(out, "adversarial", 5, 1, MPFR_RNDU);
    emit_case(out, "adversarial", 5, 1, MPFR_RNDD);
    emit_case(out, "adversarial", 5, 1, MPFR_RNDA);
    emit_case(out, "adversarial", 5, 1, MPFR_RNDZ);
    /* UINT64_MAX at prec 63: drops exactly one bit -> round up to 2^64. */
    emit_case(out, "adversarial", UINT64_MAX, 63, MPFR_RNDU);
    emit_case(out, "adversarial", UINT64_MAX, 63, MPFR_RNDD);
    emit_case(out, "adversarial", UINT64_MAX, 62, MPFR_RNDN);
    emit_case(out, "adversarial", (3ULL << 62), 1, MPFR_RNDN); /* 11 at top */

    /* fuzz: 50 random (j, prec, rnd) over the full uint64 range.
     * Seed shares the auto-port-eval xorshift stream; unique 64-bit hex. */
    {
        xs64_t rng;
        xs64_seed(&rng, 0xABCDEF0123456789ULL);
        const uint64_t precs[7] = { 1, 2, 24, 53, 64, 100, 256 };
        for (int rep = 0; rep < 50; ++rep) {
            const uint64_t j = xs64_next(&rng);
            const uint64_t prec = precs[xs64_below(&rng, 7)];
            const mpfr_rnd_t rnd = RNDS[xs64_below(&rng, 5)];
            emit_case(out, "fuzz", j, prec, rnd);
        }
    }

    /* mined: 5 -- patterns from mpfr/tests/tset_sj.c (check_set_uj). */
    emit_case(out, "mined", UINT64_MAX, 64, MPFR_RNDN);  /* L84: UINTMAX_MAX */
    emit_case(out, "mined", 0, 64, MPFR_RNDN);           /* L91: j=0 */
    emit_case(out, "mined", 1742, 53, MPFR_RNDN);        /* small limb, prec sweep */
    emit_case(out, "mined", 12345, 128, MPFR_RNDN);      /* prec=128 lossless pad */
    emit_case(out, "mined", UINT64_MAX, 2, MPFR_RNDN);   /* pmin=2 from L328 sweep */

    return 0;
}
