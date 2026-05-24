/*
 * golden_driver.c — Golden master for MPFR's mpfr_get_default_prec.
 *
 * The function takes no arguments and returns the current default
 * precision in bits, as an mpfr_prec_t (signed long). The default at
 * library start is IEEE_DBL_MANT_DIG (= 53).
 *
 * Since the TS port has no thread-locals, the default is stored in a
 * module-level variable, initialised to 53n. The grader compares against
 * what libmpfr reports BEFORE any set_default_prec call (i.e. the
 * unmutated library default).
 *
 * Each emitted case probes the default after setting it to a known
 * value via mpfr_set_default_prec — this gives the grader a range of
 * "what should get_default_prec return after set_default_prec(X)" tests.
 * The TS port needs the sister mpfr_set_default_prec function to make
 * this composition work; if it's not yet ported, the golden's only
 * useful cases are the no-set baseline (53n).
 *
 * Wire format
 * -----------
 *
 *   {"tag":"<class>",
 *    "inputs":{"prev_set":"<dec>"},
 *    "output":"<dec>",
 *    "time_ns":<n>}
 *
 *   - `prev_set` is the value passed to mpfr_set_default_prec before
 *     calling mpfr_get_default_prec, allowing the test to be stateless:
 *     the TS-side port should also call its own set_default_prec(prev_set)
 *     before get_default_prec(). For tests where no set is desired,
 *     prev_set is "53" (the library default).
 *   - output is the int64 returned by get_default_prec, emitted as a
 *     decimal string (so the TS port's bigint return round-trips).
 *
 * Tag distribution: happy 20, edge 30, adversarial 10, fuzz 50, mined 5.
 *
 * NOTE: this driver MUTATES the libmpfr global default-prec across
 * cases. That's fine because the C process runs to completion in one
 * invocation. The TS-side port reads its own module-level var; the
 * runner's per-test worker isolation means each TS test starts from
 * the module's initial value (53n) unless the test sets it first.
 */
#include "common.h"

#include <assert.h>
#include <inttypes.h>
#include <string.h>

#if GMP_NUMB_BITS != 64
#  error "mpfr_get_default_prec golden_driver requires GMP_NUMB_BITS == 64"
#endif

#define TS_PREC_CAP ((uint64_t)4096)

/* Emit one case: set default prec to `p`, call get_default_prec, emit. */
static inline void emit_case(FILE *out, const char *tag, uint64_t prev_set) {
    /* Mutate libmpfr's global default. */
    mpfr_set_default_prec((mpfr_prec_t)prev_set);
    const uint64_t t0 = now_ns();
    const mpfr_prec_t got = mpfr_get_default_prec();
    const uint64_t elapsed = now_ns() - t0;
    jl_begin(out, tag);
    jl_kv_u64(out, 1, "prev_set", prev_set);
    jl_end_inputs(out);
    jl_output_scalar_i64(out, (int64_t)got);
    jl_finish(out, elapsed);
}

int main(void) {
    FILE *out = stdout;

    /* happy: 20 — common precisions. */
    emit_case(out, "happy", 53);    /* default */
    emit_case(out, "happy", 1);
    emit_case(out, "happy", 2);
    emit_case(out, "happy", 8);
    emit_case(out, "happy", 16);
    emit_case(out, "happy", 24);    /* float32 */
    emit_case(out, "happy", 32);
    emit_case(out, "happy", 53);    /* float64 */
    emit_case(out, "happy", 64);
    emit_case(out, "happy", 96);
    emit_case(out, "happy", 100);
    emit_case(out, "happy", 113);   /* float128 */
    emit_case(out, "happy", 128);
    emit_case(out, "happy", 200);
    emit_case(out, "happy", 256);
    emit_case(out, "happy", 300);
    emit_case(out, "happy", 500);
    emit_case(out, "happy", 1000);
    emit_case(out, "happy", 2000);
    emit_case(out, "happy", TS_PREC_CAP);  /* 4096 */

    /* edge: 30 — boundary precs. */
    emit_case(out, "edge", 1);
    emit_case(out, "edge", 2);
    emit_case(out, "edge", 3);
    emit_case(out, "edge", 4);
    emit_case(out, "edge", 5);
    emit_case(out, "edge", 7);
    emit_case(out, "edge", 11);
    emit_case(out, "edge", 13);
    emit_case(out, "edge", 17);
    emit_case(out, "edge", 19);
    emit_case(out, "edge", 23);
    emit_case(out, "edge", 29);
    emit_case(out, "edge", 31);
    emit_case(out, "edge", 37);
    emit_case(out, "edge", 41);
    emit_case(out, "edge", 43);
    emit_case(out, "edge", 47);
    emit_case(out, "edge", 51);
    emit_case(out, "edge", 52);
    emit_case(out, "edge", 53);
    emit_case(out, "edge", 54);
    emit_case(out, "edge", 60);
    emit_case(out, "edge", 63);
    emit_case(out, "edge", 64);
    emit_case(out, "edge", 65);
    emit_case(out, "edge", 127);
    emit_case(out, "edge", 128);
    emit_case(out, "edge", 129);
    emit_case(out, "edge", 192);
    emit_case(out, "edge", 193);

    /* adversarial: 10 — round-trip via set→get→set→get sequences. */
    emit_case(out, "adversarial", 1);
    emit_case(out, "adversarial", TS_PREC_CAP);
    emit_case(out, "adversarial", 1);
    emit_case(out, "adversarial", 53);
    emit_case(out, "adversarial", 100);
    emit_case(out, "adversarial", 1);
    emit_case(out, "adversarial", TS_PREC_CAP / 2);
    emit_case(out, "adversarial", 53);
    emit_case(out, "adversarial", 2);
    emit_case(out, "adversarial", 2048);

    /* fuzz: 50 */
    {
        xs64_t rng;
        xs64_seed(&rng, 0xDEFA1A1A1A1A1A1AULL);
        for (int rep = 0; rep < 50; ++rep) {
            const uint64_t p = 1 + xs64_below(&rng, TS_PREC_CAP);
            emit_case(out, "fuzz", p);
        }
    }

    /* mined: 5 — common test patterns. */
    emit_case(out, "mined", 53);
    emit_case(out, "mined", 100);
    emit_case(out, "mined", 1);
    emit_case(out, "mined", 64);
    emit_case(out, "mined", 1024);

    return 0;
}
