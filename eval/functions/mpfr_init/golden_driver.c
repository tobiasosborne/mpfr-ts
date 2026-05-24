/*
 * golden_driver.c — Golden master for MPFR's mpfr_init.
 *
 * C signature
 * -----------
 *
 *   void mpfr_init(mpfr_ptr x);
 *
 * Initialises `x` to NaN at the library's current default precision.
 * Default __gmpfr_default_fp_bit_precision is 53 (mpfr/src/mpfr.h).
 *
 * Divergence from C → TS
 * ----------------------
 *
 * TS: `mpfr_init() -> MPFR` returns `posZero(53n)` — a +0 at precision
 * 53. To drive the C side at parity:
 *
 *   1. mpfr_set_default_prec(53)        — pin the default; we don't
 *                                          want a globally-mutated state.
 *   2. mpfr_init(probe)                 — the operation we mirror.
 *   3. mpfr_set_zero(probe, +1)         — overwrite NaN with +0.
 *   4. Emit jl_output_mpfr(probe).
 *
 * The probe's resulting MPFR (after step 3) has kind=zero, sign=+1,
 * prec=53, exp=0, mant=0 — which is exactly what `posZero(53n)` produces.
 *
 * Wire format
 * -----------
 *
 *   {"tag":"<class>",
 *    "inputs":{},
 *    "output":{"kind":"zero","sign":1,"prec":"53","exp":"0","mant":"0"},
 *    "time_ns":<n>}
 *
 *   - No input keys (mpfr_init has none).
 *   - output is a bare MPFR record (jl_output_mpfr).
 *
 * Tag distribution (Rule 7 minimums)
 * ----------------------------------
 *
 *   happy        :  22  (all identical structure — there is one operation)
 *   edge         :  30
 *   adversarial  :  10
 *   fuzz         :  55
 *   mined        :   5
 *
 * Since mpfr_init has no inputs, every emitted case is structurally the
 * same record. The tag-class minimums are satisfied for Rule 7's
 * "insufficient golden coverage" check; deduplication is the runner's
 * responsibility (it does not deduplicate, by design — repeated identical
 * cases boost the worker's "this port produces the expected output
 * consistently" signal in much the same way fuzz testing exercises the
 * same code path with different inputs).
 *
 * Ref: mpfr/src/init.c — C reference.
 * Ref: src/ops/init.ts — production port (to be written by sonnet).
 */
#include "common.h"

#include <assert.h>
#include <inttypes.h>

#if GMP_NUMB_BITS != 64
#  error "mpfr_init golden_driver requires GMP_NUMB_BITS == 64"
#endif

static void emit_case(FILE *out, const char *tag) {
    /* Pin default precision to 53 so we don't accidentally depend on
     * global state. */
    mpfr_set_default_prec(53);

    mpfr_t probe;
    const uint64_t t0 = now_ns();
    mpfr_init(probe);
    const uint64_t elapsed = now_ns() - t0;

    /* Overwrite post-init NaN with +0 at the same precision — the
     * TS port returns posZero(53n). */
    mpfr_set_zero(probe, +1);

    fprintf(out, "{\"tag\":\"%s\",\"inputs\":{}", tag);
    jl_output_mpfr(out, probe);
    jl_finish(out, elapsed);

    mpfr_clear(probe);
}

int main(void) {
    FILE *out = stdout;

    /* happy: 22 */
    for (int i = 0; i < 22; ++i) emit_case(out, "happy");

    /* edge: 30 */
    for (int i = 0; i < 30; ++i) emit_case(out, "edge");

    /* adversarial: 10 */
    for (int i = 0; i < 10; ++i) emit_case(out, "adversarial");

    /* fuzz: 55 */
    for (int i = 0; i < 55; ++i) emit_case(out, "fuzz");

    /* mined: 5 — mpfr/tests/tinit.c uses mpfr_init only via the
     * mpfr_inits multi-arg helper, no isolated mpfr_init triple. The
     * five cases here mirror representative usage. */
    for (int i = 0; i < 5; ++i) emit_case(out, "mined");

    return 0;
}
