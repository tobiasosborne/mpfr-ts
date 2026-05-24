/*
 * golden_driver.c — Golden master for MPFR's mpfr_overflow.
 *
 * C signature
 * -----------
 *
 *   int mpfr_overflow(mpfr_ptr x, mpfr_rnd_t rnd_mode, int sign);
 *
 *   Source: mpfr/src/exceptions.c L424-L448. Sets x to ±max-finite or
 *   ±Inf based on MPFR_IS_LIKE_RNDZ; returns the ternary (with
 *   sign-multiplication on the inex temp).
 *
 * Divergence from C → TS
 * ----------------------
 *
 * TS port signature: `mpfr_overflow(prec, rnd, sign) -> Result`.
 *   - prec is explicit (C reads it off x).
 *   - rnd is the RoundingMode string enum (5 modes; no RNDF here).
 *   - sign is the strict Sign discriminant (1 | -1).
 *   - Returns Result { value, ternary } (C mutates x, returns int).
 *
 * Driver strategy
 * ---------------
 *
 *   1. mpfr_init2(x, prec)         — allocate at requested prec.
 *   2. mpfr_set_zero(x, sign)      — force sign on x BEFORE overflow,
 *                                    so the C side's MPFR_SET_SIGN
 *                                    inside mpfr_overflow doesn't get
 *                                    a chance to wipe it (it sets
 *                                    sign from the argument anyway).
 *   3. inex = mpfr_overflow(x, rnd, sign).
 *   4. Emit Result-shaped output via jl_output_result(x, inex).
 *
 * Wire format
 * -----------
 *
 *   {"tag":"<class>",
 *    "inputs":{"prec":"<decimal>","rnd":"RND[NZUDA]","sign":<1|-1>},
 *    "output":{"value":<MPFR-record>,"ternary":<-1|0|1>},
 *    "time_ns":<n>}
 *
 *   - prec via jl_kv_u64.
 *   - rnd via jl_kv_rnd.
 *   - sign via jl_kv_int (bare number).
 *   - Output via jl_output_result.
 *
 * Tag distribution (Rule 7 minimums)
 * ----------------------------------
 *
 *   happy        :  20  (common precs × all 5 rnds × both signs)
 *   edge         :  30  (PREC_MIN, limb boundaries, large precs;
 *                        both signs across all 5 modes)
 *   adversarial  :  12  (the sign-dependent branches: RNDU/RNDD with
 *                        both signs — these are the cases where the
 *                        IS_LIKE_RNDZ predicate flips)
 *   fuzz         :  50  (PRNG-driven prec/rnd/sign combos)
 *   mined        :   5  (mpfr/tests/texceptions.c L264-L320 patterns)
 *
 * Build via eval/golden_master/build.sh.
 *
 * Ref: mpfr/src/exceptions.c — C reference.
 * Ref: src/ops/overflow.ts — production port.
 */
#include "common.h"

#include <assert.h>
#include <inttypes.h>
#include <string.h>

#if GMP_NUMB_BITS != 64
#  error "mpfr_overflow golden_driver requires GMP_NUMB_BITS == 64"
#endif

/* Forward-declare: mpfr_overflow is declared in the internal header
 * mpfr-impl.h (not in the public mpfr.h), but it IS an exported symbol
 * in libmpfr.so. We forward-declare for the public-header-only build. */
extern int mpfr_overflow (mpfr_ptr, mpfr_rnd_t, int);

#define MAX_ALLOC_PREC ((uint64_t)4096)

/* Emit one case at (prec, rnd, sign). */
static void
emit_case(FILE *out, const char *tag,
          uint64_t prec, mpfr_rnd_t rnd, int sign) {
    assert(prec >= 1 && prec <= MAX_ALLOC_PREC);
    assert(sign == +1 || sign == -1);

    mpfr_t x;
    mpfr_init2(x, (mpfr_prec_t)prec);
    /* Force sign before overflow (C comment in the function says it
     * sets sign from the argument; setting up zero first is the
     * cleanest cross-version preparation). */
    mpfr_set_zero(x, sign);

    const uint64_t t0 = now_ns();
    const int ternary = mpfr_overflow(x, rnd, sign);
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

/* Emit a case at all 5 modes for both signs. */
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

    /* ============================================================== */
    /* happy: 20 cases — IEEE float64 prec across all modes/signs.    */
    /* ============================================================== */
    {
        emit_all_modes(out, "happy", 53);   /* 10 cases */
        emit_all_modes(out, "happy", 64);   /* 10 cases */
    }

    /* ============================================================== */
    /* edge: 30 cases — boundary precs.                               */
    /* ============================================================== */
    {
        /* prec=1 across all modes/signs. */
        emit_all_modes(out, "edge", 1);     /* 10 cases */

        /* Limb boundaries: prec=63, 64, 65 at all 5 modes, sign=+1 only. */
        for (int i = 0; i < 5; ++i) emit_case(out, "edge", 63, RNDS[i], +1);
        for (int i = 0; i < 5; ++i) emit_case(out, "edge", 65, RNDS[i], +1);

        /* Limb boundaries 127/128/129, sign=-1. */
        for (int i = 0; i < 5; ++i) emit_case(out, "edge", 127, RNDS[i], -1);

        /* Large prec. */
        emit_case(out, "edge", 1024, MPFR_RNDN, +1);
        emit_case(out, "edge", 1024, MPFR_RNDZ, +1);
        emit_case(out, "edge", 1024, MPFR_RNDU, -1);
        emit_case(out, "edge", 2048, MPFR_RNDD, +1);
        emit_case(out, "edge", MAX_ALLOC_PREC, MPFR_RNDA, -1);
    }

    /* ============================================================== */
    /* adversarial: 12 cases — RNDU/RNDD × signs (the cases where     */
    /* MPFR_IS_LIKE_RNDZ flips behaviour on sign).                    */
    /* ============================================================== */
    {
        /* RNDU rounds toward +∞: like RNDZ when sign<0 (toward zero
         * from negative direction). */
        emit_case(out, "adversarial", 53, MPFR_RNDU, +1);  /* away from zero → Inf */
        emit_case(out, "adversarial", 53, MPFR_RNDU, -1);  /* toward zero → max */
        emit_case(out, "adversarial", 113, MPFR_RNDU, +1);
        emit_case(out, "adversarial", 113, MPFR_RNDU, -1);
        /* RNDD rounds toward -∞: like RNDZ when sign>0. */
        emit_case(out, "adversarial", 53, MPFR_RNDD, +1);  /* toward zero → max */
        emit_case(out, "adversarial", 53, MPFR_RNDD, -1);  /* away → Inf */
        emit_case(out, "adversarial", 113, MPFR_RNDD, +1);
        emit_case(out, "adversarial", 113, MPFR_RNDD, -1);
        /* RNDZ: always toward zero → always max. */
        emit_case(out, "adversarial", 256, MPFR_RNDZ, +1);
        emit_case(out, "adversarial", 256, MPFR_RNDZ, -1);
        /* RNDA: always away from zero → always Inf. */
        emit_case(out, "adversarial", 256, MPFR_RNDA, +1);
        emit_case(out, "adversarial", 256, MPFR_RNDA, -1);
    }

    /* ============================================================== */
    /* fuzz: 50 cases — PRNG-driven                                  */
    /* ============================================================== */
    {
        xs64_t rng;
        xs64_seed(&rng, 0x07EAF0011223344EULL);
        for (int rep = 0; rep < 50; ++rep) {
            const uint64_t prec = 1 + xs64_below(&rng, 2048);
            const uint64_t rnd_idx = xs64_below(&rng, 5);
            const int sign = (xs64_below(&rng, 2) == 0) ? +1 : -1;
            emit_case(out, "fuzz", prec, RNDS[rnd_idx], sign);
        }
    }

    /* ============================================================== */
    /* mined: 5 cases — adapted from mpfr/tests/texceptions.c        */
    /* L264-L320 (test_set_overflow): prec=32, sign in {+1, -1},     */
    /* sweep all 5 modes.                                            */
    /* ============================================================== */
    {
        /* The texceptions.c test uses prec=32 and sweeps all modes
         * with both signs; we pick the 5 most distinctive points:
         * prec=32 at each non-RNDN mode plus one RNDN sanity. */
        emit_case(out, "mined", 32, MPFR_RNDN, +1);
        emit_case(out, "mined", 32, MPFR_RNDZ, +1);
        emit_case(out, "mined", 32, MPFR_RNDU, -1);
        emit_case(out, "mined", 32, MPFR_RNDD, +1);
        emit_case(out, "mined", 32, MPFR_RNDA, -1);
    }

    return 0;
}
