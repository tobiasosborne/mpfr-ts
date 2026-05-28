/*
 * golden_driver.c -- Golden master for MPFR's mpfr_set_default_prec.
 *
 * C signature
 * -----------
 *
 *   void mpfr_set_default_prec(mpfr_prec_t prec);
 *
 *   Body (mpfr/src/set_dfl_prec.c L28-L33):
 *     MPFR_ASSERTN(MPFR_PREC_COND(prec));
 *     __gmpfr_default_fp_bit_precision = prec;
 *
 *   where MPFR_PREC_COND(p) := (p >= MPFR_PREC_MIN && p <= MPFR_PREC_MAX)
 *   (mpfr/src/mpfr-impl.h L959). The function has NO recoverable error
 *   path: an out-of-range prec triggers MPFR_ASSERTN, which abort()s the
 *   process. So a valid call always succeeds and stores prec verbatim;
 *   mpfr_get_default_prec() then returns exactly what was set.
 *
 *   MPFR_PREC_MIN == 1; on this platform MPFR_PREC_MAX == 2^63 - 257.
 *
 * IMPORTANT: because out-of-range prec abort()s, this golden ONLY emits
 * in-range precs (every case is accepted). The error path is not
 * observable via a value and therefore not gradable; the idiomatic TS
 * port lifts the C abort() into a thrown MPFRError('EPREC', ...) on
 * prec < 1 (or prec > PREC_MAX), which the runner records as n_throw --
 * a separate concern from these value-correctness goldens.
 *
 * Wire shape
 * ----------
 *
 *   {"tag":"<class>",
 *    "inputs":{"prec":"<dec uint64>"},
 *    "output":"<dec uint64>",
 *    "time_ns":<n>}
 *
 *   - prec: requested default precision in BITS. jl_kv_u64 (decimal
 *     string -> TS bigint). Always >= 1 here.
 *   - output: the resulting mpfr_get_default_prec() after the call.
 *     Equals prec for every (in-range) case. jl_output_scalar_u64
 *     (decimal string -> TS bigint).
 *
 * Precision-cap note: goldens cap prec at 2^31 - 257 (== src/core.ts
 * PREC_MAX), which is a strict subset of the C MPFR_PREC_MAX (2^63-257)
 * and is also a valid C precision. This lets the idiomatic port reuse
 * the locked src/core.ts precision bounds without the golden ever
 * exercising a prec the TS surface would reject.
 *
 * Driver flow per case (SAVE / RESTORE around every case):
 *   1. orig = mpfr_get_default_prec()
 *   2. mpfr_set_default_prec(prec)
 *   3. cur  = mpfr_get_default_prec()
 *   4. mpfr_set_default_prec(orig)
 *   5. emit cur
 * main() restores the start-of-run default prec at the end.
 *
 * Tag distribution (CLAUDE.md Rule 7 minimums)
 *   happy 20, edge 30, adversarial 12, fuzz 50, mined 5.
 *
 * Compile (standalone; do NOT run build.sh):
 *   gcc -O2 -std=c11 -Wall -Wextra -Werror -I. golden_driver.c \
 *       $(pkg-config --libs mpfr) -lgmp -lm -o golden_driver
 *
 * Ref: mpfr/src/set_dfl_prec.c L28-L40 -- C reference (set/get default prec).
 * Ref: mpfr/src/mpfr-impl.h L959 -- MPFR_PREC_COND.
 * Ref: src/core.ts L216-L236 -- PREC_MIN / PREC_MAX in the TS surface.
 */
#include "common.h"

#include <assert.h>
#include <inttypes.h>

/* The cap used for golden inputs: src/core.ts PREC_MAX == 2^31 - 257.
 * A strict subset of the C MPFR_PREC_MAX; every value here is accepted. */
#define GOLDEN_PREC_MAX (((uint64_t)1 << 31) - 257)

/* Emit one case: set default prec, capture result, restore prior. */
static inline void emit_case(FILE *out, const char *tag, uint64_t prec) {
    assert(prec >= 1 && prec <= GOLDEN_PREC_MAX);
    const mpfr_prec_t orig = mpfr_get_default_prec();
    const uint64_t t0 = now_ns();
    mpfr_set_default_prec((mpfr_prec_t)prec);
    const uint64_t elapsed = now_ns() - t0;
    const uint64_t cur = (uint64_t)mpfr_get_default_prec();
    mpfr_set_default_prec(orig);  /* restore */

    jl_begin(out, tag);
    jl_kv_u64(out, 1, "prec", prec);
    jl_end_inputs(out);
    jl_output_scalar_u64(out, cur);
    jl_finish(out, elapsed);
}

int main(void) {
    FILE *out = stdout;

    const mpfr_prec_t start = mpfr_get_default_prec();

    /* happy: 20 -- ordinary precisions in common use. */
    emit_case(out, "happy", 53);     /* IEEE double */
    emit_case(out, "happy", 24);     /* IEEE single */
    emit_case(out, "happy", 113);    /* IEEE quad */
    emit_case(out, "happy", 64);     /* x87 extended */
    emit_case(out, "happy", 11);     /* IEEE half */
    emit_case(out, "happy", 2);
    emit_case(out, "happy", 4);
    emit_case(out, "happy", 8);
    emit_case(out, "happy", 16);
    emit_case(out, "happy", 32);
    emit_case(out, "happy", 100);
    emit_case(out, "happy", 128);
    emit_case(out, "happy", 200);
    emit_case(out, "happy", 256);
    emit_case(out, "happy", 512);
    emit_case(out, "happy", 1000);
    emit_case(out, "happy", 1024);
    emit_case(out, "happy", 10000);
    emit_case(out, "happy", 65536);
    emit_case(out, "happy", 12345);

    /* edge: 30 -- boundaries (MIN==1), small ints, large in-range,
     * powers of two, and the golden cap. */
    emit_case(out, "edge", 1);                    /* MPFR_PREC_MIN */
    emit_case(out, "edge", 2);
    emit_case(out, "edge", 3);
    emit_case(out, "edge", GOLDEN_PREC_MAX);      /* highest emitted */
    emit_case(out, "edge", GOLDEN_PREC_MAX - 1);
    emit_case(out, "edge", GOLDEN_PREC_MAX - 2);
    emit_case(out, "edge", (uint64_t)1 << 30);
    emit_case(out, "edge", ((uint64_t)1 << 30) + 1);
    emit_case(out, "edge", ((uint64_t)1 << 30) - 1);
    emit_case(out, "edge", (uint64_t)1 << 20);
    emit_case(out, "edge", (uint64_t)1 << 16);
    emit_case(out, "edge", (uint64_t)1 << 10);
    emit_case(out, "edge", (uint64_t)1 << 8);
    emit_case(out, "edge", (uint64_t)1 << 4);
    emit_case(out, "edge", (uint64_t)1 << 1);
    emit_case(out, "edge", 63);
    emit_case(out, "edge", 65);
    emit_case(out, "edge", 127);
    emit_case(out, "edge", 129);
    emit_case(out, "edge", 255);
    emit_case(out, "edge", 257);
    emit_case(out, "edge", 511);
    emit_case(out, "edge", 513);
    emit_case(out, "edge", 1023);
    emit_case(out, "edge", 1025);
    emit_case(out, "edge", 999999);
    emit_case(out, "edge", 1000001);
    emit_case(out, "edge", GOLDEN_PREC_MAX / 2);
    emit_case(out, "edge", GOLDEN_PREC_MAX - 100);
    emit_case(out, "edge", 7);

    /* adversarial: 12 -- the absolute minimum, the golden cap, the just-
     * inside-cap values, and idempotence/stability repeats. */
    emit_case(out, "adversarial", 1);                    /* PREC_MIN */
    emit_case(out, "adversarial", GOLDEN_PREC_MAX);      /* cap */
    emit_case(out, "adversarial", GOLDEN_PREC_MAX - 1);
    emit_case(out, "adversarial", 1);                    /* repeat -> stable */
    emit_case(out, "adversarial", GOLDEN_PREC_MAX);      /* repeat -> stable */
    emit_case(out, "adversarial", 2);
    emit_case(out, "adversarial", 53);
    emit_case(out, "adversarial", (uint64_t)1 << 30);
    emit_case(out, "adversarial", ((uint64_t)1 << 31) - 258);
    emit_case(out, "adversarial", ((uint64_t)1 << 31) - 257);  /* == cap */
    emit_case(out, "adversarial", 64);
    emit_case(out, "adversarial", 100000);

    /* fuzz: 50 -- PRNG-driven precs uniform in [1, GOLDEN_PREC_MAX]. */
    {
        xs64_t rng;
        /* Hex seed, digits 0-9 A-F only. */
        xs64_seed(&rng, 0xD0FFEE5C0FFEE0ADULL);
        for (int rep = 0; rep < 50; ++rep) {
            /* xs64_below(span) is in [0, span); +1 -> [1, GOLDEN_PREC_MAX]. */
            const uint64_t prec = 1 + xs64_below(&rng, GOLDEN_PREC_MAX);
            emit_case(out, "fuzz", prec);
        }
    }

    /* mined: 5 -- the set-default-prec / restore pattern used across
     * mpfr/tests (tset_*, tests.c set_default_prec(53) reset). */
    emit_case(out, "mined", 53);     /* reset to library default */
    emit_case(out, "mined", 1);      /* minimum */
    emit_case(out, "mined", 113);    /* quad */
    emit_case(out, "mined", 2);      /* tiny */
    emit_case(out, "mined", 1000);   /* big-ish */

    mpfr_set_default_prec(start);  /* restore run start */

    return 0;
}
