/*
 * golden_driver.c — Golden master for MPFR's mpfr_underflow.
 *
 * C signature
 * -----------
 *
 *   int mpfr_underflow(mpfr_ptr x, mpfr_rnd_t rnd_mode, int sign);
 *
 *   Source: mpfr/src/exceptions.c L396-L420. Sets x to ±0 or ±min-finite
 *   based on MPFR_IS_LIKE_RNDZ; returns the ternary with sign-mul.
 *
 * Divergence from C → TS
 * ----------------------
 *
 * TS port signature: `mpfr_underflow(prec, rnd, sign) -> Result`.
 *   - Mirror image of mpfr_overflow (same call shape, ±0 / ±min instead
 *     of ±max / ±Inf).
 *
 * Driver strategy
 * ---------------
 *
 *   1. mpfr_init2(x, prec).
 *   2. mpfr_set_zero(x, sign)  — force sign before underflow.
 *   3. inex = mpfr_underflow(x, rnd, sign).
 *   4. Emit Result-shaped output.
 *
 * Wire format identical to mpfr_overflow's.
 *
 * Tag distribution (Rule 7 minimums)
 * ----------------------------------
 *
 *   happy        :  20
 *   edge         :  30
 *   adversarial  :  12
 *   fuzz         :  50
 *   mined        :   5  (mpfr/tests/texceptions.c L204-L262 patterns)
 *
 * Build via eval/golden_master/build.sh.
 *
 * Ref: mpfr/src/exceptions.c — C reference.
 * Ref: src/ops/underflow.ts — production port.
 */
#include "common.h"

#include <assert.h>
#include <inttypes.h>
#include <string.h>

#if GMP_NUMB_BITS != 64
#  error "mpfr_underflow golden_driver requires GMP_NUMB_BITS == 64"
#endif

/* Forward-declare: mpfr_underflow is declared in the internal header
 * mpfr-impl.h (not in the public mpfr.h) but is exported from
 * libmpfr.so. */
extern int mpfr_underflow (mpfr_ptr, mpfr_rnd_t, int);

#define MAX_ALLOC_PREC ((uint64_t)4096)

static void
emit_case(FILE *out, const char *tag,
          uint64_t prec, mpfr_rnd_t rnd, int sign) {
    assert(prec >= 1 && prec <= MAX_ALLOC_PREC);
    assert(sign == +1 || sign == -1);

    mpfr_t x;
    mpfr_init2(x, (mpfr_prec_t)prec);
    mpfr_set_zero(x, sign);

    const uint64_t t0 = now_ns();
    const int ternary = mpfr_underflow(x, rnd, sign);
    const uint64_t elapsed = now_ns() - t0;

    jl_begin(out, tag);
    jl_kv_u64(out, 1, "prec", prec);
    jl_kv_rnd(out, 0, "rnd", rnd);
    jl_kv_int(out, 0, "sign", sign);
    jl_end_inputs(out);
    jl_output_result(out, x, ternary);
    jl_finish(out, elapsed);

    mpfr_clear(x);
}

static void emit_all_modes(FILE *out, const char *tag, uint64_t prec) {
    const mpfr_rnd_t RNDS[5] = {
        MPFR_RNDN, MPFR_RNDZ, MPFR_RNDU, MPFR_RNDD, MPFR_RNDA,
    };
    for (int i = 0; i < 5; ++i) {
        emit_case(out, tag, prec, RNDS[i], +1);
        emit_case(out, tag, prec, RNDS[i], -1);
    }
}

int main(void) {
    FILE *out = stdout;
    const mpfr_rnd_t RNDS[5] = {
        MPFR_RNDN, MPFR_RNDZ, MPFR_RNDU, MPFR_RNDD, MPFR_RNDA,
    };

    /* happy: 20. */
    {
        emit_all_modes(out, "happy", 53);
        emit_all_modes(out, "happy", 64);
    }

    /* edge: 30. */
    {
        emit_all_modes(out, "edge", 1);
        for (int i = 0; i < 5; ++i) emit_case(out, "edge", 63, RNDS[i], +1);
        for (int i = 0; i < 5; ++i) emit_case(out, "edge", 65, RNDS[i], +1);
        for (int i = 0; i < 5; ++i) emit_case(out, "edge", 127, RNDS[i], -1);
        emit_case(out, "edge", 1024, MPFR_RNDN, +1);
        emit_case(out, "edge", 1024, MPFR_RNDZ, +1);
        emit_case(out, "edge", 1024, MPFR_RNDU, -1);
        emit_case(out, "edge", 2048, MPFR_RNDD, +1);
        emit_case(out, "edge", MAX_ALLOC_PREC, MPFR_RNDA, -1);
    }

    /* adversarial: 12 — sign-dependent branches. */
    {
        emit_case(out, "adversarial", 53, MPFR_RNDU, +1);
        emit_case(out, "adversarial", 53, MPFR_RNDU, -1);
        emit_case(out, "adversarial", 113, MPFR_RNDU, +1);
        emit_case(out, "adversarial", 113, MPFR_RNDU, -1);
        emit_case(out, "adversarial", 53, MPFR_RNDD, +1);
        emit_case(out, "adversarial", 53, MPFR_RNDD, -1);
        emit_case(out, "adversarial", 113, MPFR_RNDD, +1);
        emit_case(out, "adversarial", 113, MPFR_RNDD, -1);
        emit_case(out, "adversarial", 256, MPFR_RNDZ, +1);
        emit_case(out, "adversarial", 256, MPFR_RNDZ, -1);
        emit_case(out, "adversarial", 256, MPFR_RNDA, +1);
        emit_case(out, "adversarial", 256, MPFR_RNDA, -1);
    }

    /* fuzz: 50. */
    {
        xs64_t rng;
        xs64_seed(&rng, 0x09DEF0011223344EULL);
        for (int rep = 0; rep < 50; ++rep) {
            const uint64_t prec = 1 + xs64_below(&rng, 2048);
            const uint64_t rnd_idx = xs64_below(&rng, 5);
            const int sign = (xs64_below(&rng, 2) == 0) ? +1 : -1;
            emit_case(out, "fuzz", prec, RNDS[rnd_idx], sign);
        }
    }

    /* mined: 5 — texceptions.c L204-L262 patterns. */
    {
        emit_case(out, "mined", 32, MPFR_RNDN, +1);
        emit_case(out, "mined", 32, MPFR_RNDZ, +1);
        emit_case(out, "mined", 32, MPFR_RNDU, -1);
        emit_case(out, "mined", 32, MPFR_RNDD, +1);
        emit_case(out, "mined", 32, MPFR_RNDA, -1);
    }

    return 0;
}
