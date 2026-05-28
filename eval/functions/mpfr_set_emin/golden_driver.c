/*
 * golden_driver.c -- Golden master for MPFR's mpfr_set_emin.
 *
 * C signature
 * -----------
 *
 *   int mpfr_set_emin(mpfr_exp_t exp);
 *
 *   Body (mpfr/src/exceptions.c L38-L50):
 *     if (exp >= MPFR_EMIN_MIN && exp <= MPFR_EMIN_MAX) {
 *         __gmpfr_emin = exp; return 0;
 *     } else { return 1; }
 *
 *   Returns 0 and stores the new emin when exp is in
 *   [MPFR_EMIN_MIN, MPFR_EMIN_MAX]; returns 1 and leaves the global
 *   emin UNCHANGED otherwise. Mirror of mpfr_set_emax.
 *
 *   On this platform (mpfr_exp_t == long, 64-bit):
 *     MPFR_EMIN_MIN = -(2^62 - 1) = -4611686018427387903
 *     MPFR_EMIN_MAX =  (2^62 - 1) =  4611686018427387903
 *   (see mpfr/src/mpfr-impl.h L1037,L1048-L1049; probed at build host.)
 *   Read at runtime via the public mpfr_get_emin_min()/max() so the
 *   golden tracks the host's actual bounds.
 *
 * Wire shape
 * ----------
 *
 *   {"tag":"<class>",
 *    "inputs":{"exp":"<dec int64>"},
 *    "output":{"ret":<0|1>,"emin":"<dec int64>"},
 *    "time_ns":<n>}
 *
 *   - exp: candidate emin. jl_kv_i64 (decimal string -> TS bigint).
 *   - output.ret: C int return code, 0 (accepted) or 1 (rejected).
 *     Bare JSON number (jl_kv_int) -> TS number.
 *   - output.emin: resulting global emin after the call (mpfr_get_emin()).
 *     ret==0 -> equals exp; ret==1 -> unchanged prior emin. jl_kv_i64
 *     (decimal string -> TS bigint), values can reach ~2^62.
 *
 * Driver flow per case (SAVE / RESTORE around every case):
 *   1. orig = mpfr_get_emin()
 *   2. ret  = mpfr_set_emin(exp)
 *   3. cur  = mpfr_get_emin()
 *   4. mpfr_set_emin(orig)
 *   5. emit {ret, cur}
 * main() also restores the start-of-run emin at the end.
 *
 * Tag distribution (CLAUDE.md Rule 7 minimums)
 *   happy 20, edge 30, adversarial 12, fuzz 50, mined 5.
 *
 * Compile (standalone; do NOT run build.sh):
 *   gcc -O2 -std=c11 -Wall -Wextra -Werror -I. golden_driver.c \
 *       $(pkg-config --libs mpfr) -lgmp -lm -o golden_driver
 *
 * Ref: mpfr/src/exceptions.c L38-L62 -- C reference (set/get_emin_*).
 * Ref: mpfr/src/mpfr-impl.h L1037,L1048-L1049 -- EMIN_MIN/MAX defs.
 * Ref: mpfr/tests/texceptions.c -- emin/emax set-and-restore mined pattern.
 */
#include "common.h"

#include <assert.h>
#include <inttypes.h>

/* Emit one case: try mpfr_set_emin(exp), capture ret + resulting emin,
 * then restore the prior global emin. */
static inline void emit_case(FILE *out, const char *tag, int64_t exp) {
    const mpfr_exp_t orig = mpfr_get_emin();
    const uint64_t t0 = now_ns();
    const int ret = mpfr_set_emin((mpfr_exp_t)exp);
    const uint64_t elapsed = now_ns() - t0;
    const int64_t cur = (int64_t)mpfr_get_emin();
    const int restored = mpfr_set_emin(orig);
    assert(restored == 0);  /* orig was a valid emin by construction. */
    (void)restored;

    jl_begin(out, tag);
    jl_kv_i64(out, 1, "exp", exp);
    jl_end_inputs(out);
    jl_output_begin_object(out);
    jl_kv_int(out, 1, "ret", ret);
    jl_kv_i64(out, 0, "emin", cur);
    jl_output_end_object(out);
    jl_finish(out, elapsed);
}

int main(void) {
    FILE *out = stdout;

    const mpfr_exp_t start = mpfr_get_emin();

    const int64_t EMIN_MIN = (int64_t)mpfr_get_emin_min();
    const int64_t EMIN_MAX = (int64_t)mpfr_get_emin_max();
    const int64_t EMIN_DEFAULT = -(((int64_t)1 << 30) - 1);  /* MPFR_EMIN_DEFAULT */

    /* happy: 20 -- ordinary in-range exponents, accepted (ret 0). */
    emit_case(out, "happy", 0);
    emit_case(out, "happy", 1);
    emit_case(out, "happy", -1);
    emit_case(out, "happy", -2);
    emit_case(out, "happy", -10);
    emit_case(out, "happy", -100);
    emit_case(out, "happy", -1000);
    emit_case(out, "happy", -1024);
    emit_case(out, "happy", -65536);
    emit_case(out, "happy", 100);
    emit_case(out, "happy", 1000);
    emit_case(out, "happy", EMIN_DEFAULT);
    emit_case(out, "happy", -EMIN_DEFAULT);
    emit_case(out, "happy", -1073741824);  /* -2^30, just past default */
    emit_case(out, "happy", -53);
    emit_case(out, "happy", -113);
    emit_case(out, "happy", -16383);
    emit_case(out, "happy", -1000000);
    emit_case(out, "happy", 1000000);
    emit_case(out, "happy", -123456789);

    /* edge: 30 -- exact boundaries and one-off-either-side, plus interior. */
    emit_case(out, "edge", EMIN_MIN);           /* lowest accepted */
    emit_case(out, "edge", EMIN_MIN + 1);       /* accepted */
    emit_case(out, "edge", EMIN_MIN - 1);       /* rejected (too small) */
    emit_case(out, "edge", EMIN_MAX);           /* highest accepted */
    emit_case(out, "edge", EMIN_MAX - 1);       /* accepted */
    emit_case(out, "edge", EMIN_MAX + 1);       /* rejected (too large) */
    emit_case(out, "edge", EMIN_MIN + 2);
    emit_case(out, "edge", EMIN_MAX - 2);
    emit_case(out, "edge", EMIN_MIN - 2);       /* rejected */
    emit_case(out, "edge", EMIN_MAX + 2);       /* rejected */
    emit_case(out, "edge", 0);
    emit_case(out, "edge", 1);
    emit_case(out, "edge", -1);
    emit_case(out, "edge", EMIN_MAX / 2);
    emit_case(out, "edge", EMIN_MIN / 2);
    emit_case(out, "edge", EMIN_MAX - 1000);
    emit_case(out, "edge", EMIN_MIN + 1000);
    emit_case(out, "edge", INT64_MAX);          /* rejected */
    emit_case(out, "edge", INT64_MIN);          /* rejected */
    emit_case(out, "edge", INT64_MAX - 1);      /* rejected */
    emit_case(out, "edge", INT64_MIN + 1);      /* rejected */
    emit_case(out, "edge", (int64_t)1 << 31);   /* accepted */
    emit_case(out, "edge", -((int64_t)1 << 31));
    emit_case(out, "edge", (int64_t)1 << 40);
    emit_case(out, "edge", -((int64_t)1 << 40));
    emit_case(out, "edge", (int64_t)1 << 61);   /* accepted (< 2^62-1) */
    emit_case(out, "edge", -((int64_t)1 << 61));
    emit_case(out, "edge", ((int64_t)1 << 62));     /* rejected: == 2^62 > max */
    emit_case(out, "edge", -((int64_t)1 << 62));    /* rejected */
    emit_case(out, "edge", EMIN_MIN);               /* re-touch the boundary */

    /* adversarial: 12 -- on/just-over the bound, zero, just-in/just-out
     * pairs that catch a > vs >= off-by-one. */
    emit_case(out, "adversarial", EMIN_MIN);
    emit_case(out, "adversarial", EMIN_MIN - 1);
    emit_case(out, "adversarial", EMIN_MAX);
    emit_case(out, "adversarial", EMIN_MAX + 1);
    emit_case(out, "adversarial", EMIN_MIN + 1);
    emit_case(out, "adversarial", EMIN_MAX - 1);
    emit_case(out, "adversarial", 0);
    emit_case(out, "adversarial", INT64_MAX);
    emit_case(out, "adversarial", INT64_MIN);
    emit_case(out, "adversarial", EMIN_MIN);     /* repeated to check stability */
    emit_case(out, "adversarial", EMIN_MAX);
    emit_case(out, "adversarial", -(((int64_t)1 << 62) - 1));  /* == EMIN_MIN */

    /* fuzz: 50 -- PRNG-driven exponents spanning rejected and accepted. */
    {
        xs64_t rng;
        /* Hex seed, digits 0-9 A-F only. */
        xs64_seed(&rng, 0xB1CE05DDFA110ACEULL);
        for (int rep = 0; rep < 35; ++rep) {
            const int64_t v = (int64_t)xs64_next(&rng);
            emit_case(out, "fuzz", v);
        }
        for (int rep = 0; rep < 15; ++rep) {
            const uint64_t span = (uint64_t)EMIN_MAX;
            const int64_t mag = (int64_t)xs64_below(&rng, span + 1);
            const int64_t v = (xs64_next(&rng) & 1) ? mag : -mag;
            emit_case(out, "fuzz", v);
        }
    }

    /* mined: 5 -- set-emin-and-restore pattern from texceptions.c. */
    emit_case(out, "mined", EMIN_MIN);           /* set to min */
    emit_case(out, "mined", EMIN_MAX);           /* set to max */
    emit_case(out, "mined", EMIN_DEFAULT);       /* reset to default */
    emit_case(out, "mined", EMIN_MIN - 1);       /* under-bound -> rejected */
    emit_case(out, "mined", 0);                  /* harmless mid-range */

    const int ok = mpfr_set_emin(start);
    assert(ok == 0);
    (void)ok;

    return 0;
}
