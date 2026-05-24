/*
 * golden_driver.c — Golden master for MPFR's mpfr_set_flt.
 *
 * Convert a binary32 float to MPFR. C reference is a one-line delegate
 * to mpfr_set_d (since every float is exactly representable as a double).
 *
 * On the wire we emit the source as a DOUBLE (it has been losslessly
 * promoted via `(double)f`), since the TS port has no separate binary32
 * type — the caller will pass a JS number.
 *
 * Tag distribution: happy 22, edge 30, adversarial 12, fuzz 60, mined 5.
 */
#include "common.h"

#include <assert.h>
#include <float.h>
#include <inttypes.h>
#include <math.h>
#include <string.h>

#if GMP_NUMB_BITS != 64
#  error "mpfr_set_flt golden_driver requires GMP_NUMB_BITS == 64"
#endif

#define TS_PREC_CAP ((uint64_t)4096)

static const mpfr_rnd_t RNDS[5] = {MPFR_RNDN, MPFR_RNDZ, MPFR_RNDU, MPFR_RNDD, MPFR_RNDA};

/* Construct a -0 float via bit-level memcpy. */
static inline float make_neg_zero_f(void) {
    const uint32_t bits = (uint32_t)1 << 31;
    float f;
    memcpy(&f, &bits, sizeof f);
    return f;
}

/* Emit one case (f, prec, rnd). */
static inline void emit_case(FILE *out, const char *tag,
                             float f, uint64_t prec, mpfr_rnd_t rnd) {
    mpfr_t r;
    mpfr_init2(r, (mpfr_prec_t)prec);
    const uint64_t t0 = now_ns();
    const int ternary = mpfr_set_flt(r, f, rnd);
    const uint64_t elapsed = now_ns() - t0;
    jl_begin(out, tag);
    /* Emit the SOURCE as a double — the TS port receives a JS number, which
     * is binary64; the float has been losslessly promoted. */
    jl_kv_double(out, 1, "f", (double)f);
    jl_kv_u64(out, 0, "prec", prec);
    jl_kv_rnd(out, 0, "rnd", rnd);
    jl_end_inputs(out);
    jl_output_result(out, r, ternary);
    jl_finish(out, elapsed);
    mpfr_clear(r);
}

int main(void) {
    FILE *out = stdout;

    /* happy: 22 */
    emit_case(out, "happy", 1.0f, 53, MPFR_RNDN);
    emit_case(out, "happy", 2.0f, 53, MPFR_RNDN);
    emit_case(out, "happy", 3.14f, 53, MPFR_RNDN);
    emit_case(out, "happy", -3.14f, 53, MPFR_RNDN);
    emit_case(out, "happy", 0.5f, 53, MPFR_RNDN);
    emit_case(out, "happy", 0.25f, 53, MPFR_RNDN);
    emit_case(out, "happy", -1.0f, 53, MPFR_RNDN);
    emit_case(out, "happy", 1.5f, 53, MPFR_RNDN);
    emit_case(out, "happy", 100.0f, 53, MPFR_RNDN);
    emit_case(out, "happy", 100.5f, 53, MPFR_RNDN);
    emit_case(out, "happy", 1e10f, 53, MPFR_RNDN);
    emit_case(out, "happy", 1e-10f, 53, MPFR_RNDN);
    emit_case(out, "happy", 1.0f, 24, MPFR_RNDN);
    emit_case(out, "happy", 1.0f, 100, MPFR_RNDN);
    emit_case(out, "happy", 0.1f, 24, MPFR_RNDN);
    emit_case(out, "happy", 0.1f, 53, MPFR_RNDN);
    emit_case(out, "happy", -0.5f, 53, MPFR_RNDN);
    emit_case(out, "happy", -100.5f, 53, MPFR_RNDA);
    emit_case(out, "happy", 1.5e30f, 53, MPFR_RNDN);
    emit_case(out, "happy", 1.5e-30f, 53, MPFR_RNDN);
    emit_case(out, "happy", 2.71828f, 53, MPFR_RNDN);
    emit_case(out, "happy", 3.14159f, 100, MPFR_RNDN);

    /* edge: 30 */
    /* NaN under each rnd. */
    for (int i = 0; i < 5; ++i) emit_case(out, "edge", NAN, 53, RNDS[i]);
    /* +Inf under each rnd. */
    for (int i = 0; i < 5; ++i) emit_case(out, "edge", INFINITY, 53, RNDS[i]);
    /* -Inf under each rnd. */
    for (int i = 0; i < 5; ++i) emit_case(out, "edge", -INFINITY, 53, RNDS[i]);
    /* +0 under each rnd. */
    for (int i = 0; i < 5; ++i) emit_case(out, "edge", 0.0f, 53, RNDS[i]);
    /* -0 under each rnd. */
    for (int i = 0; i < 5; ++i) emit_case(out, "edge", make_neg_zero_f(), 53, RNDS[i]);
    /* prec=1, single bit. */
    emit_case(out, "edge", 1.0f, 1, MPFR_RNDN);
    emit_case(out, "edge", -1.0f, 1, MPFR_RNDU);
    /* High prec — exact even though source is float. */
    emit_case(out, "edge", 1.0f, TS_PREC_CAP, MPFR_RNDN);
    emit_case(out, "edge", 3.14f, TS_PREC_CAP, MPFR_RNDN);
    /* Float MAX/MIN. */
    emit_case(out, "edge", FLT_MAX, 53, MPFR_RNDN);

    /* adversarial: 12 — rounding-sensitive precs. */
    emit_case(out, "adversarial", 1.0f/3.0f, 2, MPFR_RNDN);
    emit_case(out, "adversarial", 1.0f/3.0f, 2, MPFR_RNDU);
    emit_case(out, "adversarial", 1.0f/3.0f, 2, MPFR_RNDD);
    emit_case(out, "adversarial", -1.0f/3.0f, 2, MPFR_RNDD);
    emit_case(out, "adversarial", 0.1f, 1, MPFR_RNDN);
    emit_case(out, "adversarial", 0.1f, 1, MPFR_RNDU);
    emit_case(out, "adversarial", FLT_MIN, 53, MPFR_RNDN);
    emit_case(out, "adversarial", FLT_MAX, 53, MPFR_RNDN);
    emit_case(out, "adversarial", FLT_EPSILON, 53, MPFR_RNDN);
    emit_case(out, "adversarial", 16777215.0f, 24, MPFR_RNDN);  /* 2^24-1 */
    emit_case(out, "adversarial", 16777216.0f, 24, MPFR_RNDN);  /* 2^24 */
    emit_case(out, "adversarial", 16777217.0f, 24, MPFR_RNDN);  /* 2^24+1 (not float-exact) */

    /* fuzz: 60 */
    {
        xs64_t rng;
        xs64_seed(&rng, 0xF1F1F1F1F1F1F1F1ULL);
        for (int rep = 0; rep < 60; ++rep) {
            const uint64_t prec = 1 + xs64_below(&rng, 256);
            const uint64_t r1 = xs64_next(&rng);
            float fd = ((float)(r1 % 200000ULL) - 100000.0f) / 100.0f;
            const uint64_t rnd_idx = xs64_below(&rng, 5);
            emit_case(out, "fuzz", fd, prec, RNDS[rnd_idx]);
        }
    }

    /* mined: 5 — derived from tget_d.c shapes (no dedicated tset_flt test). */
    emit_case(out, "mined", 1.0f, 53, MPFR_RNDN);
    emit_case(out, "mined", 3.14f, 53, MPFR_RNDN);
    emit_case(out, "mined", 0.5f, 24, MPFR_RNDN);
    emit_case(out, "mined", -1.0f, 53, MPFR_RNDN);
    emit_case(out, "mined", 0.0f, 53, MPFR_RNDN);

    return 0;
}
