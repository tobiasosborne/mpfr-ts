/*
 * golden_driver.c -- Golden master for MPFR's mpfr_set_emax.
 *
 * C signature
 * -----------
 *
 *   int mpfr_set_emax(mpfr_exp_t exp);
 *
 *   Body (mpfr/src/exceptions.c L74-L86):
 *     if (exp >= MPFR_EMAX_MIN && exp <= MPFR_EMAX_MAX) {
 *         __gmpfr_emax = exp; return 0;
 *     } else { return 1; }
 *
 *   Returns 0 and stores the new emax when exp is in
 *   [MPFR_EMAX_MIN, MPFR_EMAX_MAX]; returns 1 and leaves the global
 *   emax UNCHANGED otherwise.
 *
 *   On this platform (mpfr_exp_t == long, 64-bit):
 *     MPFR_EMAX_MIN = -(2^62 - 1) = -4611686018427387903
 *     MPFR_EMAX_MAX =  (2^62 - 1) =  4611686018427387903
 *   (see mpfr/src/mpfr-impl.h L1037,L1050-L1051; probed at build host.)
 *   These are read at runtime via the public mpfr_get_emax_max() /
 *   mpfr_get_emax_min() so the golden tracks the host's actual bounds.
 *
 * Wire shape
 * ----------
 *
 *   {"tag":"<class>",
 *    "inputs":{"exp":"<dec int64>"},
 *    "output":{"ret":<0|1>,"emax":"<dec int64>"},
 *    "time_ns":<n>}
 *
 *   - exp: candidate emax. Signed; emitted via jl_kv_i64 (decimal string,
 *     decoded to a bigint on the TS side).
 *   - output.ret: the C int return code, 0 (accepted) or 1 (rejected).
 *     Emitted as a bare JSON number (jl_kv_int) -> TS number; the port
 *     returns a plain `number`.
 *   - output.emax: the resulting global emax after the call, read via the
 *     public mpfr_get_emax(). On ret==0 this equals exp; on ret==1 it is
 *     the unchanged prior emax. Emitted via jl_kv_i64 (decimal string ->
 *     TS bigint) since the value can reach ~2^62, beyond Number safety.
 *
 * Driver flow per case (SAVE / RESTORE around every case so the golden is
 * order-independent and reproducible):
 *   1. orig = mpfr_get_emax()        -- save the live global
 *   2. ret  = mpfr_set_emax(exp)     -- the call under test
 *   3. cur  = mpfr_get_emax()        -- capture the resulting emax
 *   4. mpfr_set_emax(orig)           -- restore the global
 *   5. emit {ret, cur}
 * main() additionally restores the start-of-run emax at the end.
 *
 * Tag distribution (CLAUDE.md Rule 7 minimums)
 *   happy        :  20
 *   edge         :  30
 *   adversarial  :  12
 *   fuzz         :  50
 *   mined        :   5
 *
 * Compile (standalone; do NOT run build.sh -- it races siblings):
 *   gcc -O2 -std=c11 -Wall -Wextra -Werror -I. golden_driver.c \
 *       $(pkg-config --libs mpfr) -lgmp -lm -o golden_driver
 *
 * Ref: mpfr/src/exceptions.c L74-L97 -- C reference (set/get_emax_*).
 * Ref: mpfr/src/mpfr-impl.h L1037,L1050-L1051 -- EMAX_MIN/MAX defs.
 * Ref: mpfr/tests/texceptions.c -- emin/emax set-and-restore mined pattern.
 */
#include "common.h"

#include <assert.h>
#include <inttypes.h>

/* Emit one case: try mpfr_set_emax(exp), capture ret + resulting emax,
 * then restore the prior global emax. */
static inline void emit_case(FILE *out, const char *tag, int64_t exp) {
    const mpfr_exp_t orig = mpfr_get_emax();
    const uint64_t t0 = now_ns();
    const int ret = mpfr_set_emax((mpfr_exp_t)exp);
    const uint64_t elapsed = now_ns() - t0;
    const int64_t cur = (int64_t)mpfr_get_emax();
    /* Restore immediately so cases never interfere. */
    const int restored = mpfr_set_emax(orig);
    assert(restored == 0);  /* orig was a valid emax by construction. */
    (void)restored;

    jl_begin(out, tag);
    jl_kv_i64(out, 1, "exp", exp);
    jl_end_inputs(out);
    jl_output_begin_object(out);
    jl_kv_int(out, 1, "ret", ret);
    jl_kv_i64(out, 0, "emax", cur);
    jl_output_end_object(out);
    jl_finish(out, elapsed);
}

int main(void) {
    FILE *out = stdout;

    const mpfr_exp_t start = mpfr_get_emax();

    /* Host bounds, read via the public API so the golden matches the
     * exact platform the libmpfr was built for. */
    const int64_t EMAX_MIN = (int64_t)mpfr_get_emax_min();
    const int64_t EMAX_MAX = (int64_t)mpfr_get_emax_max();
    const int64_t EMAX_DEFAULT = ((int64_t)1 << 30) - 1;  /* MPFR_EMAX_DEFAULT */

    /* happy: 20 -- ordinary in-range exponents that are accepted (ret 0). */
    emit_case(out, "happy", 0);
    emit_case(out, "happy", 1);
    emit_case(out, "happy", -1);
    emit_case(out, "happy", 2);
    emit_case(out, "happy", 10);
    emit_case(out, "happy", 100);
    emit_case(out, "happy", 1000);
    emit_case(out, "happy", 1024);
    emit_case(out, "happy", 65536);
    emit_case(out, "happy", -100);
    emit_case(out, "happy", -1000);
    emit_case(out, "happy", EMAX_DEFAULT);
    emit_case(out, "happy", -EMAX_DEFAULT);
    emit_case(out, "happy", 1073741824);   /* 2^30, just past default */
    emit_case(out, "happy", 53);
    emit_case(out, "happy", 113);
    emit_case(out, "happy", 16383);
    emit_case(out, "happy", 1000000);
    emit_case(out, "happy", -1000000);
    emit_case(out, "happy", 123456789);

    /* edge: 30 -- exact boundaries and one-off-either-side, plus a few
     * structurally interesting interior points. */
    emit_case(out, "edge", EMAX_MIN);           /* lowest accepted */
    emit_case(out, "edge", EMAX_MIN + 1);       /* accepted */
    emit_case(out, "edge", EMAX_MIN - 1);       /* rejected (too small) */
    emit_case(out, "edge", EMAX_MAX);           /* highest accepted */
    emit_case(out, "edge", EMAX_MAX - 1);       /* accepted */
    emit_case(out, "edge", EMAX_MAX + 1);       /* rejected (too large) */
    emit_case(out, "edge", EMAX_MIN + 2);
    emit_case(out, "edge", EMAX_MAX - 2);
    emit_case(out, "edge", EMAX_MIN - 2);       /* rejected */
    emit_case(out, "edge", EMAX_MAX + 2);       /* rejected */
    emit_case(out, "edge", 0);
    emit_case(out, "edge", 1);
    emit_case(out, "edge", -1);
    emit_case(out, "edge", EMAX_MAX / 2);
    emit_case(out, "edge", EMAX_MIN / 2);
    emit_case(out, "edge", EMAX_MAX - 1000);
    emit_case(out, "edge", EMAX_MIN + 1000);
    emit_case(out, "edge", INT64_MAX);          /* rejected: far above max */
    emit_case(out, "edge", INT64_MIN);          /* rejected: far below min */
    emit_case(out, "edge", INT64_MAX - 1);      /* rejected */
    emit_case(out, "edge", INT64_MIN + 1);      /* rejected */
    emit_case(out, "edge", (int64_t)1 << 31);   /* accepted (within max) */
    emit_case(out, "edge", -((int64_t)1 << 31));
    emit_case(out, "edge", (int64_t)1 << 40);
    emit_case(out, "edge", -((int64_t)1 << 40));
    emit_case(out, "edge", (int64_t)1 << 61);   /* accepted (< 2^62-1) */
    emit_case(out, "edge", -((int64_t)1 << 61));
    emit_case(out, "edge", ((int64_t)1 << 62));     /* rejected: == 2^62 > max */
    emit_case(out, "edge", -((int64_t)1 << 62));    /* rejected */
    emit_case(out, "edge", EMAX_MAX);               /* re-touch the boundary */

    /* adversarial: 12 -- exactly-on/just-over the bound, zero, and the
     * just-in/just-out pairs that catch a > vs >= off-by-one in the port. */
    emit_case(out, "adversarial", EMAX_MIN);
    emit_case(out, "adversarial", EMAX_MIN - 1);
    emit_case(out, "adversarial", EMAX_MAX);
    emit_case(out, "adversarial", EMAX_MAX + 1);
    emit_case(out, "adversarial", EMAX_MIN + 1);
    emit_case(out, "adversarial", EMAX_MAX - 1);
    emit_case(out, "adversarial", 0);
    emit_case(out, "adversarial", INT64_MAX);
    emit_case(out, "adversarial", INT64_MIN);
    emit_case(out, "adversarial", EMAX_MAX);     /* repeated to check stability */
    emit_case(out, "adversarial", EMAX_MIN);
    emit_case(out, "adversarial", ((int64_t)1 << 62) - 1);  /* == EMAX_MAX */

    /* fuzz: 50 -- PRNG-driven exponents spanning rejected and accepted
     * ranges. We draw a signed value with full int64 spread (so a good
     * fraction land out of range) plus a band of guaranteed-in-range. */
    {
        xs64_t rng;
        /* Hex seed, digits 0-9 A-F only (HANDOFF gotcha #3). */
        xs64_seed(&rng, 0xE3AA0F1DCB05E3AAULL);
        for (int rep = 0; rep < 35; ++rep) {
            /* Full-width signed draw: many will be out of range. */
            const int64_t v = (int64_t)xs64_next(&rng);
            emit_case(out, "fuzz", v);
        }
        for (int rep = 0; rep < 15; ++rep) {
            /* In-range band: |v| <= EMAX_MAX, accepted (ret 0). */
            const uint64_t span = (uint64_t)EMAX_MAX;  /* [0, EMAX_MAX] */
            const int64_t mag = (int64_t)xs64_below(&rng, span + 1);
            const int64_t v = (xs64_next(&rng) & 1) ? mag : -mag;
            emit_case(out, "fuzz", v);
        }
    }

    /* mined: 5 -- the set-emax-and-restore pattern from
     * mpfr/tests/texceptions.c (boundary probing + valid resets). */
    emit_case(out, "mined", EMAX_MAX);           /* set to max */
    emit_case(out, "mined", EMAX_MIN);           /* set to min */
    emit_case(out, "mined", EMAX_DEFAULT);       /* reset to default */
    emit_case(out, "mined", EMAX_MAX + 1);       /* over-bound -> rejected */
    emit_case(out, "mined", 0);                  /* harmless mid-range */

    /* Restore the run's starting emax (defensive; every case already
     * restores, but main() leaves the global pristine). */
    const int ok = mpfr_set_emax(start);
    assert(ok == 0);
    (void)ok;

    return 0;
}
